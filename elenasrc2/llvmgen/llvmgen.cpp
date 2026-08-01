//---------------------------------------------------------------------------
//      E L E N A   P r o j e c t:  LLVM code generation backend
//
//      The ONLY translation unit that includes LLVM headers.
//---------------------------------------------------------------------------

#include "elena.h"
// --------------------------------------------------------------------------
#include "llvmgen.h"

// common/tools.h defines min/max as macros; they poison LLVM's templates
#undef min
#undef max

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/TargetParser/Triple.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace _ELENA_;

// --- target registration ---------------------------------------------------
//
// Explicit per-target initialization rather than InitializeAllTargets(): the
// enabled set lives in source, matching ELENA_LLVM_TARGETS in CMakeLists.

#define ELENA_INIT_TARGET(NAME)          \
   LLVMInitialize##NAME##TargetInfo();   \
   LLVMInitialize##NAME##Target();       \
   LLVMInitialize##NAME##TargetMC();     \
   LLVMInitialize##NAME##AsmPrinter();   \
   LLVMInitialize##NAME##AsmParser();

static void initTargets()
{
   static bool done = false;
   if (done)
      return;

   ELENA_INIT_TARGET(X86)       // x86, x86-64
   ELENA_INIT_TARGET(AArch64)   // arm64
   ELENA_INIT_TARGET(PowerPC)   // ppc32, ppc64, ppc64le
   ELENA_INIT_TARGET(SystemZ)   // s390x

   done = true;
}

// --- implementation --------------------------------------------------------

namespace
{

struct Impl
{
   llvm::LLVMContext                    context;
   std::unique_ptr<llvm::Module>        module;
   std::unique_ptr<llvm::TargetMachine> machine;
   const TargetInfo*                    target;

   _ELENA_::TranslateCallbacks          callbacks;

   llvm::IntegerType* wordType()
   {
      return llvm::Type::getIntNTy(context, target->pointerBits);
   }

   Impl()
      : target(NULL)
   {
      callbacks.context = NULL;
      callbacks.referenceName = NULL;
      callbacks.internMessage = NULL;
   }
};

}

inline static Impl* impl(void* p) { return (Impl*)p; }

LLVMGenerator :: LLVMGenerator()
{
   _impl = new Impl();
}

LLVMGenerator :: ~LLVMGenerator()
{
   delete impl(_impl);
}

bool LLVMGenerator :: init(const TargetInfo* target, GenError& error)
{
   initTargets();

   Impl* self = impl(_impl);
   self->target = target;

   std::string message;
   const llvm::Target* selected = llvm::TargetRegistry::lookupTarget(target->triple, message);
   if (!selected) {
      error.copy(message.c_str());
      return false;
   }

   llvm::TargetOptions options;
   self->machine.reset(selected->createTargetMachine(
      llvm::Triple(target->triple), "generic", "", options, llvm::Reloc::PIC_));
   if (!self->machine) {
      error.copy("cannot create the target machine");
      return false;
   }

   self->module.reset(new llvm::Module("elena", self->context));
   self->module->setTargetTriple(llvm::Triple(target->triple));
   self->module->setDataLayout(self->machine->createDataLayout());

   return true;
}

void LLVMGenerator :: setCallbacks(const TranslateCallbacks& callbacks)
{
   impl(_impl)->callbacks = callbacks;
}

// --- e-code translation ----------------------------------------------------
//
// Ground facts, each taken from the serializer rather than guessed:
//
//  * opcodes <= 0x8F carry no operand; 0x90..0xEF one LE dword; 0xF0..0xFF
//    two (ByteCommand::save, engine/bytecode.h)
//  * the two-dword CONDITIONALS are serialized [comparison operand][jump
//    displacement] -- the reverse of the in-memory tape order
//    (bcwriter.cpp writeProcedure, "reverse order" note in bytecode.h:223)
//  * a jump displacement is relative to the END of its own dword; labels
//    are materialized as a bcNop byte at the target offset
//  * frame cells and evaluation-stack cells are the same storage: `open`
//    pushes the saved frame pointer as an ordinary cell, FI i addresses the
//    i-th cell pushed after `open`, SI i addresses the i-th cell below the
//    top (the x86 JIT's [ebp - i*4] / [esp + i*4])
//  * at entry the x86 stack held a return address; the model keeps a
//    sentinel cell for it so SI indexes line up: SI reaching below cell 0
//    addresses the caller-provided argument frame

namespace
{

const size_t MAX_CELLS = 96;

struct Instruction
{
   unsigned char opcode;
   size_t        offset;      // of the opcode byte
   size_t        next;        // first byte after the instruction
   long long     arg;         // first operand
   long long     extra;       // comparison operand of two-dword conditionals
   bool          jump;
   size_t        target;      // absolute target offset when 'jump'
};

// one-dword opcodes whose operand is a jump displacement
inline bool isJumpOp(unsigned char op)
{
   switch (op) {
      case 0x96:  // ifheap
      case 0xA0:  // jump
      case 0xA6:  // hook
      case 0xA7:  // address
      case 0xA9:  // less
      case 0xAA:  // notless
      case 0xAB:  // ifb
      case 0xAC:  // elseb
      case 0xAD:  // if
      case 0xAE:  // else
      case 0xAF:  // next
         return true;
      default:
         return false;
   }
}

// two-dword opcodes serialized [operand][jump displacement]
inline bool isCondJumpOp(unsigned char op)
{
   return op >= 0xF7 && op <= 0xFD;   // lessn ifm elsem ifr elser ifn elsen
}

struct BlockState
{
   int  depth;      // number of live cells
   int  fpBase;     // cell index of the saved-frame sentinel, -1 if no frame
   long long staticE; // last statically known E value, -1 if unknown
   bool known;

   BlockState() { depth = 0; fpBase = -1; staticE = -1; known = false; }
};

struct Translator
{
   Impl&                     impl;
   _ELENA_::GenError&        error;
   const unsigned char*      code;
   size_t                    length;
   unsigned int              entryMessage;

   std::vector<Instruction>  stream;
   std::map<size_t, size_t>  byOffset;          // offset -> stream index
   std::map<size_t, BlockState> blocks;         // block entry offset -> state

   llvm::Function*           function;
   llvm::IRBuilder<>*        builder;
   llvm::IntegerType*        wordTy;
   llvm::PointerType*        ptrTy;
   llvm::Value*              regA;
   llvm::Value*              regB;
   llvm::Value*              regD;
   llvm::Value*              regE;
   llvm::Value*              cells;              // [MAX_CELLS x word]
   llvm::Value*              argFrame;           // ptr parameter
   std::map<size_t, llvm::BasicBlock*> blockFor; // block offset -> BB

   Translator(Impl& impl_, _ELENA_::GenError& error_)
      : impl(impl_), error(error_), code(NULL), length(0), entryMessage(0),
        function(NULL), builder(NULL), wordTy(NULL), ptrTy(NULL),
        regA(NULL), regB(NULL), regD(NULL), regE(NULL),
        cells(NULL), argFrame(NULL)
   {
   }

   bool fail(const Instruction& at, const char* reason)
   {
      char buffer[200];
      snprintf(buffer, sizeof(buffer), "opcode %02X at +%04X: %s",
         at.opcode, (unsigned int)at.offset, reason);
      error.copy(buffer);
      return false;
   }

   // -- decoding ----------------------------------------------------------

   bool decode()
   {
      size_t offset = 0;
      while (offset < length) {
         Instruction ins;
         ins.opcode = code[offset];
         ins.offset = offset;
         ins.arg = 0;
         ins.extra = 0;
         ins.jump = false;
         ins.target = 0;

         size_t position = offset + 1;
         if (ins.opcode > 0xEF) {
            if (position + 8 > length) { error.copy("truncated stream"); return false; }
            int first, second;
            memcpy(&first, code + position, 4);
            memcpy(&second, code + position + 4, 4);
            position += 8;
            if (isCondJumpOp(ins.opcode)) {
               ins.extra = first;
               ins.jump = true;
               ins.target = position + second;
            }
            else {
               ins.arg = first;
               ins.extra = second;
            }
         }
         else if (ins.opcode > 0x8F) {
            if (position + 4 > length) { error.copy("truncated stream"); return false; }
            int value;
            memcpy(&value, code + position, 4);
            position += 4;
            if (isJumpOp(ins.opcode)) {
               ins.jump = true;
               ins.target = position + value;
            }
            else ins.arg = value;
         }
         ins.next = position;

         byOffset[ins.offset] = stream.size();
         stream.push_back(ins);
         offset = position;
      }
      return true;
   }

   // -- static stack model -------------------------------------------------

   // net cell effect of one instruction; conservative and explicit.
   // returns false when the effect cannot be statically known
   bool cellDelta(const Instruction& ins, const BlockState& in, int& delta, const Instruction** callsite)
   {
      delta = 0;
      switch (ins.opcode) {
         case 0x02: case 0x05: case 0x0A: case 0x22:            // pushb pushe pusha pushd
         case 0xB0: case 0xB2: case 0xB4: case 0xB6:            // pushn pushr pushai pushfi
         case 0xBA: case 0xBD:                                  // pushsi pushf
            delta = 1; return true;
         case 0x03: case 0x0B: case 0x0D: case 0x14: case 0x23: // pop popa pope popb popd
            delta = -1; return true;
         case 0xD0:                                             // popi n
            delta = -(int)ins.arg; return true;
         case 0x98:                                             // open n
            delta = 1; return true;                             // the fp sentinel
         case 0x24:                                             // dreserve (unmanaged, D cells)
            return false;
         case 0xBF: case 0x92:                                  // reserve / restore (unmanaged)
            delta = 0; return true;
         case 0xA2: case 0x39: {                                // acallvi / acallvd
            int floor_ = (in.fpBase >= 0) ? in.fpBase + 1 : 1;
            if (in.staticE < 0 || (int)(in.staticE & 0x0F) >= 0x0C) {
               // a dispatcher-style send: the message is dynamic (or an
               // open list), so the callee runs against the CALLER's
               // argument cells. That is provable exactly when this
               // procedure has nothing of its own above the floor.
               if (in.depth == floor_) {
                  delta = 0;
                  return true;
               }
               return false;
            }
            delta = -sendConsumption(in, (int)(in.staticE & 0x0F));
            return true;
         }
         case 0xFE: {                                           // xcallrm r, m
            int count = (int)(ins.extra & 0x0F);
            if (count >= 0x0C) {
               int floor_ = (in.fpBase >= 0) ? in.fpBase + 1 : 1;
               if (in.depth == floor_) {
                  delta = 0;
                  return true;
               }
               return false;
            }
            delta = -sendConsumption(in, count);
            return true;
         }
         case 0xA3:                                             // callr (api procedure)
            // arity is the callee's business in e-code; until the coreapi
            // C surface fixes arities, assume balance and let the driver
            // count how often this assumption was taken
            delta = 0; return true;
         case 0xA5:                                             // callextr
            delta = 0; return true;
         default:
            return true;
      }
   }

   // how many of OUR cells a send consumes. A regular send pops its
   // self+parameters (pushed just before). A constructor-redirect send at
   // method entry, however, runs against the CALLER's argument cells and
   // leaves them intact -- the code after it still reads them through
   // negative frame indexes. The model therefore never lets a send eat
   // through the sentinel/frame floor.
   int sendConsumption(const BlockState& in, int count)
   {
      int want = count + 1;                     // self travels with the args
      int floor_ = (in.fpBase >= 0) ? in.fpBase + 1 : 1;
      int available = in.depth - floor_;
      if (available < 0)
         available = 0;
      return want < available ? want : available;
   }

   bool analyze()
   {
      // block discovery: entry, every jump target, every fallthrough after
      // a conditional; a hook target is also a block
      std::vector<size_t> worklist;
      BlockState entry;
      entry.depth = 1;        // the return-address sentinel
      entry.fpBase = -1;
      // at entry E holds the message the procedure was invoked with
      entry.staticE = entryMessage;
      entry.known = true;
      blocks[0] = entry;
      worklist.push_back(0);

      while (!worklist.empty()) {
         size_t at = worklist.back();
         worklist.pop_back();

         BlockState state = blocks[at];
         size_t index = byOffset.count(at) ? byOffset[at] : (size_t)-1;
         if (index == (size_t)-1) { error.copy("jump into the middle of an instruction"); return false; }

         for ( ; index < stream.size() ; index++) {
            const Instruction& ins = stream[index];

            // E tracking
            if (ins.opcode == 0x9F) {                          // copym
               state.staticE = ins.arg;
            }
            else if (ins.opcode == 0x91) {                     // ecopy n
               state.staticE = ins.arg;
            }
            else if (writesE(ins.opcode)) state.staticE = -1;

            int delta = 0;
            const Instruction* unused = NULL;
            if (!cellDelta(ins, state, delta, &unused))
               return fail(ins, "unprovable stack effect");

            if (ins.opcode == 0x98) {                          // open
               state.fpBase = state.depth;                     // the sentinel cell
            }
            else if (ins.opcode == 0x15) {                     // close
               if (state.fpBase < 0) return fail(ins, "close without open");
               state.depth = state.fpBase;
               state.fpBase = -1;
               delta = 0;
            }
            else if (ins.opcode == 0xD2) {                     // scopyf level
               if (state.fpBase < 0) return fail(ins, "scopyf without frame");
               state.depth = state.fpBase + 1 + (int)ins.arg;
               delta = 0;
            }

            state.depth += delta;
            if (state.depth < 0 || state.depth > (int)MAX_CELLS)
               return fail(ins, "stack depth out of range");

            if (ins.opcode == 0xA6) {
               // hook: the handler edge carries this state; execution
               // continues linearly
               if (!propagate(ins.target, state, worklist)) return false;
            }
            else if (ins.jump && ins.opcode != 0xA7) {         // address only records
               if (!propagate(ins.target, state, worklist)) return false;
               if (ins.opcode == 0xA0)                         // unconditional
                  break;
               // a conditional's fall-through path is a block of its own --
               // the emitter needs a basic block to branch to
               if (!propagate(ins.next, state, worklist)) return false;
               break;
            }
            if (isTerminator(ins.opcode))
               break;
            if (index + 1 >= stream.size())
               break;
         }
      }
      return true;
   }

   bool writesE(unsigned char op)
   {
      switch (op) {
         case 0x0D:            // pope
         case 0x20:            // ecopyd
         case 0x2C:            // eswap
         case 0x11: case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: // len xlen blen wlen flag nlen
         case 0xB1: case 0xBC: // eloadfi eloadsi
         case 0xC6:            // eswapsi
         case 0xD3: case 0xD4: // setverb setsubj -- partial, drop tracking
         case 0xD8:            // eaddn
            return true;
         default:
            return false;
      }
   }

   bool isTerminator(unsigned char op)
   {
      switch (op) {
         case 0x17:            // quit
         case 0x1B:            // equit
         case 0x99:            // quitn
         case 0x07:            // throw
         case 0xF5:            // xjumprm
         case 0xA1:            // ajumpvi
            return true;
         default:
            return false;
      }
   }

   // true when the block starting at 'target' reaches a terminator or an
   // unconditional jump touching only cell-free operations -- registers,
   // globals, payloads, plain jumps. Such a tail cannot observe a depth
   // difference, so merging paths of different depths into it is sound in
   // this model (cells are function-local storage, returns ignore them).
   bool cellFreeTail(size_t target)
   {
      std::map<size_t, size_t>::iterator at = byOffset.find(target);
      if (at == byOffset.end())
         return false;
      for (size_t index = at->second ; index < stream.size() ; index++) {
         unsigned char op = stream[index].opcode;
         if (isTerminator(op) || op == 0xA0)
            return true;
         switch (op) {
            case 0x00: case 0x01: case 0x04: case 0x26:         // nops
            case 0x0C: case 0x12: case 0x20: case 0x21:         // register moves
            case 0x90: case 0x91: case 0x9E: case 0x9A:         // dcopy ecopy acopyr bcopyr
            case 0x93: case 0xCC:                               // aloadr asaver
            case 0xCD: case 0xCE:                               // aloadai aloadbi
            case 0x36:                                          // class
            case 0x28:                                          // freelock
               continue;
            default:
               return false;                                    // anything cell-touching
         }
      }
      return true;
   }

   bool propagate(size_t target, const BlockState& state, std::vector<size_t>& worklist)
   {
      std::map<size_t, BlockState>::iterator existing = blocks.find(target);
      if (existing == blocks.end()) {
         BlockState next = state;
         blocks[target] = next;
         worklist.push_back(target);
         return true;
      }
      BlockState& present = existing->second;
      if (present.depth != state.depth || present.fpBase != state.fpBase) {
         if (present.fpBase == state.fpBase && cellFreeTail(target)) {
            // keep the smaller depth; the tail cannot tell the difference
            if (state.depth < present.depth) {
               present.depth = state.depth;
               worklist.push_back(target);
            }
            if (present.staticE != state.staticE)
               present.staticE = -1;
            return true;
         }
         char buffer[120];
         snprintf(buffer, sizeof(buffer),
            "conflicting depth at +%04X (%d/%d vs %d/%d)",
            (unsigned int)target, present.depth, present.fpBase,
            state.depth, state.fpBase);
         error.copy(buffer);
         return false;
      }
      if (present.staticE != state.staticE)
         present.staticE = -1;
      return true;
   }

   // -- IR emission --------------------------------------------------------
   //
   // Registers and cells are memory (allocas); mem2reg rebuilds SSA, so
   // merges need no PHIs -- the depth analysis already guaranteed the cell
   // layout agrees on every edge. The cell array mirrors the x86 stack by
   // growing DOWNWARD in memory: the address of a deeper cell is higher,
   // so a callee's argument frame pointer walks the caller's cells with a
   // positive stride, exactly like [esp + i*4] did.

   llvm::ArrayType*     cellsTy;
   llvm::FunctionCallee sendVi, resend, allocNew, allocBin, setField,
                        throwFn, hookPush, unhookFn, currentEx, setjmpFn,
                        lenFn, validateFn, trylockFn, freelockFn, externStub;

   llvm::Value* cellAddress(int c)
   {
      if (c >= 0)
         return builder->CreateConstInBoundsGEP2_64(cellsTy, cells, 0, MAX_CELLS - 1 - c);
      return builder->CreateConstInBoundsGEP1_64(wordTy, argFrame, -c);
   }

   llvm::Value* loadCell(int c)  { return builder->CreateLoad(wordTy, cellAddress(c)); }
   void storeCell(int c, llvm::Value* v) { builder->CreateStore(v, cellAddress(c)); }

   llvm::Value* loadA() { return builder->CreateLoad(ptrTy, regA); }
   llvm::Value* loadB() { return builder->CreateLoad(ptrTy, regB); }
   llvm::Value* loadD() { return builder->CreateLoad(wordTy, regD); }
   llvm::Value* loadE() { return builder->CreateLoad(wordTy, regE); }
   void storeA(llvm::Value* v) { builder->CreateStore(v, regA); }
   void storeB(llvm::Value* v) { builder->CreateStore(v, regB); }
   void storeD(llvm::Value* v) { builder->CreateStore(v, regD); }
   void storeE(llvm::Value* v) { builder->CreateStore(v, regE); }

   llvm::Value* toWord(llvm::Value* p) { return builder->CreatePtrToInt(p, wordTy); }
   llvm::Value* toPtr(llvm::Value* w)  { return builder->CreateIntToPtr(w, ptrTy); }
   llvm::Constant* wordC(long long v)  { return llvm::ConstantInt::get(wordTy, v, true); }

   // materializes a constant object: [size/flags][vmt][payload], with the
   // public symbol pointing AT the payload so the header sits below the
   // object pointer, exactly as the runtime expects
   llvm::Value* constantObject(unsigned int mask, unsigned int id)
   {
      char keyBuffer[24];
      snprintf(keyBuffer, sizeof(keyBuffer), "elena.k.%02X.%06X", mask >> 24, id);
      if (llvm::GlobalValue* known = impl.module->getNamedValue(keyBuffer))
         return known;

      const char* spelling = impl.callbacks.constantValue
         ? impl.callbacks.constantValue(impl.callbacks.context, id) : NULL;
      if (!spelling)
         spelling = "";

      const char* vmtName;
      llvm::Constant* payload;
      long long sizeField;
      switch (mask) {
         case 0x03000000: {                                     // int32 (hex spelling)
            vmtName = "elena.vmt.system.IntNumber";
            long long v = strtoll(spelling, NULL, 16);
            payload = llvm::ConstantInt::get(llvm::Type::getInt32Ty(impl.context), v);
            sizeField = -4;
            break;
         }
         case 0x04000000: {                                     // int64 (marker + decimal)
            vmtName = "elena.vmt.system.LongNumber";
            long long v = strtoll(spelling + (spelling[0] ? 1 : 0), NULL, 10);
            payload = llvm::ConstantInt::get(llvm::Type::getInt64Ty(impl.context), v);
            sizeField = -8;
            break;
         }
         case 0x05000000: {                                     // real
            vmtName = "elena.vmt.system.RealNumber";
            payload = llvm::ConstantFP::get(llvm::Type::getDoubleTy(impl.context),
               strtod(spelling, NULL));
            sizeField = -8;
            break;
         }
         case 0x07000000:                                       // char (utf-8 payload)
         case 0x02000000: {                                     // literal
            vmtName = (mask == 0x02000000)
               ? "elena.vmt.system.LiteralValue" : "elena.vmt.system.CharValue";
            payload = llvm::ConstantDataArray::getString(impl.context, spelling, true);
            sizeField = -((long long)strlen(spelling) + 1);
            break;
         }
         default:
            return NULL;
      }

      llvm::GlobalVariable* vmt = impl.module->getGlobalVariable(vmtName, true);
      if (!vmt)
         vmt = new llvm::GlobalVariable(*impl.module, wordTy, false,
            llvm::GlobalValue::ExternalLinkage, nullptr, vmtName);

      llvm::StructType* image = llvm::StructType::get(impl.context,
         { wordTy, ptrTy, payload->getType() });
      llvm::GlobalVariable* body = new llvm::GlobalVariable(*impl.module, image,
         false /* #shift can rewrite a live object's class */,
         llvm::GlobalValue::PrivateLinkage,
         llvm::ConstantStruct::get(image, { wordC(sizeField), vmt, payload }),
         std::string(keyBuffer) + ".image");

      llvm::Constant* indexes[3] = {
         llvm::ConstantInt::get(llvm::Type::getInt32Ty(impl.context), 0),
         llvm::ConstantInt::get(llvm::Type::getInt32Ty(impl.context), 2),
         NULL
      };
      llvm::Constant* payloadAddr = llvm::ConstantExpr::getInBoundsGetElementPtr(
         image, body, llvm::ArrayRef<llvm::Constant*>(indexes, 2));

      return llvm::GlobalAlias::create(payload->getType(), 0,
         llvm::GlobalValue::InternalLinkage, keyBuffer, payloadAddr, impl.module.get());
   }

   // resolves a reference operand (refId | mask) to a named global address
   llvm::Value* referenceValue(long long operand)
   {
      unsigned int mask = (unsigned int)operand & 0xFF000000;
      unsigned int id = (unsigned int)operand & 0x00FFFFFF;

      // value constants own their data; everything else is a named cell
      switch (mask) {
         case 0x02000000: case 0x03000000: case 0x04000000:
         case 0x05000000: case 0x07000000: {
            llvm::Value* object = constantObject(mask, id);
            if (object)
               return object;
            break;
         }
      }

      const char* name = NULL;
      if (impl.callbacks.referenceName)
         name = impl.callbacks.referenceName(impl.callbacks.context, id);

      std::string symbol("elena.");
      switch (mask) {
         case 0x41000000: symbol += "vmt."; break;       // mskVMTRef
         case 0x02000000: symbol += "lit."; break;       // mskLiteralRef
         case 0x03000000: symbol += "int."; break;       // mskInt32Ref
         case 0x04000000: symbol += "long."; break;      // mskInt64Ref
         case 0x05000000: symbol += "real."; break;      // mskRealRef
         case 0x07000000: symbol += "char."; break;      // mskCharRef
         case 0x01000000: symbol += "const."; break;     // mskConstantRef
         case 0x06000000: symbol += "mssg."; break;      // mskMessage
         case 0x80000000: case 0x82000000:
                          symbol += "stat."; break;      // static variable
         case 0xA0000000: symbol += "data."; break;      // mskDataRef
         default:         symbol += "ref."; break;
      }
      if (name)
         symbol += sanitized(name);
      else {
         char buffer[16];
         snprintf(buffer, sizeof(buffer), "%08llX", operand);
         symbol += buffer;
      }

      if (llvm::GlobalValue* known = impl.module->getNamedValue(symbol))
         return known;
      return new llvm::GlobalVariable(*impl.module, wordTy, false,
         llvm::GlobalValue::ExternalLinkage, nullptr, symbol);
   }

   static std::string sanitized(const char* name)
   {
      std::string out(name);
      for (size_t i = 0 ; i < out.size() ; i++) {
         if (out[i] == '\'' || out[i] == ':' || out[i] == '#')
            out[i] = '.';
      }
      return out;
   }

   llvm::FunctionType* methodType()
   {
      llvm::Type* params[3] = { ptrTy, wordTy, ptrTy };
      return llvm::FunctionType::get(ptrTy, params, false);
   }

   void declareRuntime()
   {
      llvm::Module& m = *impl.module;
      llvm::Type* i32 = llvm::Type::getInt32Ty(impl.context);

      sendVi = m.getOrInsertFunction("elena_send_vi",
         llvm::FunctionType::get(ptrTy, { ptrTy, wordTy, ptrTy, wordTy }, false));
      resend = m.getOrInsertFunction("elena_bsredirect",
         llvm::FunctionType::get(ptrTy, { ptrTy, wordTy, ptrTy, ptrTy }, false));
      allocNew = m.getOrInsertFunction("elena_new",
         llvm::FunctionType::get(ptrTy, { ptrTy, wordTy }, false));
      allocBin = m.getOrInsertFunction("elena_newbinary",
         llvm::FunctionType::get(ptrTy, { ptrTy, wordTy }, false));
      setField = m.getOrInsertFunction("elena_setfield",
         llvm::FunctionType::get(llvm::Type::getVoidTy(impl.context),
            { ptrTy, wordTy, ptrTy }, false));
      throwFn = m.getOrInsertFunction("elena_throw",
         llvm::FunctionType::get(llvm::Type::getVoidTy(impl.context), { ptrTy }, false));
      hookPush = m.getOrInsertFunction("elena_hook_push",
         llvm::FunctionType::get(llvm::Type::getVoidTy(impl.context), { ptrTy }, false));
      unhookFn = m.getOrInsertFunction("elena_unhook",
         llvm::FunctionType::get(llvm::Type::getVoidTy(impl.context), {}, false));
      currentEx = m.getOrInsertFunction("elena_current_exception",
         llvm::FunctionType::get(ptrTy, {}, false));
      setjmpFn = m.getOrInsertFunction("_setjmp",
         llvm::FunctionType::get(i32, { ptrTy }, false));
      if (llvm::Function* f = llvm::dyn_cast<llvm::Function>(setjmpFn.getCallee()))
         f->addFnAttr(llvm::Attribute::ReturnsTwice);
      lenFn = m.getOrInsertFunction("elena_length",
         llvm::FunctionType::get(wordTy, { ptrTy, wordTy }, false));
      validateFn = m.getOrInsertFunction("elena_validate",
         llvm::FunctionType::get(llvm::Type::getVoidTy(impl.context), { ptrTy }, false));
      trylockFn = m.getOrInsertFunction("elena_trylock",
         llvm::FunctionType::get(wordTy, { ptrTy }, false));
      freelockFn = m.getOrInsertFunction("elena_freelock",
         llvm::FunctionType::get(llvm::Type::getVoidTy(impl.context), { ptrTy }, false));
      externStub = m.getOrInsertFunction("elena_external_stub",
         llvm::FunctionType::get(wordTy, {}, false));
   }

   // 32-bit payload of a value object (int/char): first word of the body
   llvm::Value* payload32(llvm::Value* object)
   {
      return builder->CreateLoad(llvm::Type::getInt32Ty(impl.context), object);
   }
   void storePayload32(llvm::Value* object, llvm::Value* v)
   {
      builder->CreateStore(v, object);
   }
   llvm::Value* payload64(llvm::Value* object)
   {
      return builder->CreateLoad(llvm::Type::getInt64Ty(impl.context), object);
   }
   void storePayload64(llvm::Value* object, llvm::Value* v)
   {
      builder->CreateStore(v, object);
   }
   llvm::Value* payloadF64(llvm::Value* object)
   {
      return builder->CreateLoad(llvm::Type::getDoubleTy(impl.context), object);
   }
   void storePayloadF64(llvm::Value* object, llvm::Value* v)
   {
      builder->CreateStore(v, object);
   }

   // object header: [-2] size/flags, [-1] vmt  (two words below the pointer)
   llvm::Value* vmtOf(llvm::Value* object)
   {
      llvm::Value* slot = builder->CreateConstInBoundsGEP1_64(ptrTy, object, -1);
      return builder->CreateLoad(ptrTy, slot);
   }

   llvm::BasicBlock* blockAt(size_t offset)
   {
      std::map<size_t, llvm::BasicBlock*>::iterator it = blockFor.find(offset);
      return it == blockFor.end() ? NULL : it->second;
   }

   llvm::Value* condFromCompare(unsigned char op, llvm::Value* condition)
   {
      // ifX branches when equal/true; elseX when not
      return condition;
   }

   void branchOn(llvm::Value* condition, size_t target, size_t fallthrough)
   {
      builder->CreateCondBr(condition, blockAt(target), blockAt(fallthrough));
   }

   llvm::Value* regF;   // the x87-style floating register (rload/rsave/dcopyr)

   // messages cross module boundaries: every message OPERAND is re-encoded
   // into the global id space before it reaches emitted code
   long long interned(long long message)
   {
      if (impl.callbacks.internMessage)
         return impl.callbacks.internMessage(impl.callbacks.context, (unsigned int)message);
      return message;
   }

   llvm::Value* libmUnary(const char* name, llvm::Value* v)
   {
      llvm::Type* d = llvm::Type::getDoubleTy(impl.context);
      llvm::FunctionCallee fn = impl.module->getOrInsertFunction(name,
         llvm::FunctionType::get(d, { d }, false));
      return builder->CreateCall(fn, { v });
   }

   // emits the whole function; the walk mirrors analyze() so the static
   // depth is re-derived identically
   bool emitFunction(const char* symbolName)
   {
      wordTy = impl.wordType();
      ptrTy = llvm::PointerType::get(impl.context, 0);
      cellsTy = llvm::ArrayType::get(wordTy, MAX_CELLS);
      declareRuntime();

      llvm::Type* i8 = llvm::Type::getInt8Ty(impl.context);
      llvm::Type* i32 = llvm::Type::getInt32Ty(impl.context);
      llvm::Type* f64 = llvm::Type::getDoubleTy(impl.context);

      std::string symbol = "elena." + sanitized(symbolName);
      llvm::Function* existing = impl.module->getFunction(symbol);
      if (existing && !existing->isDeclaration())
         return true;   // an identical body was already emitted

      function = existing ? existing
         : llvm::Function::Create(methodType(), llvm::Function::ExternalLinkage,
              symbol, impl.module.get());

      llvm::IRBuilder<> b(impl.context);
      builder = &b;

      llvm::BasicBlock* entryBB = llvm::BasicBlock::Create(impl.context, "entry", function);
      b.SetInsertPoint(entryBB);

      regA = b.CreateAlloca(ptrTy, nullptr, "A");
      regB = b.CreateAlloca(ptrTy, nullptr, "B");
      regD = b.CreateAlloca(wordTy, nullptr, "D");
      regE = b.CreateAlloca(wordTy, nullptr, "E");
      regF = b.CreateAlloca(f64, nullptr, "F");
      cells = b.CreateAlloca(cellsTy, nullptr, "cells");
      llvm::Value* foundFlag = b.CreateAlloca(i8, nullptr, "found");

      // one hook frame per hook site, materialized at entry
      std::map<size_t, llvm::Value*> hookFrames;
      for (size_t i = 0 ; i < stream.size() ; i++) {
         if (stream[i].opcode == 0xA6)
            hookFrames[stream[i].offset] =
               b.CreateAlloca(llvm::ArrayType::get(i8, 576), nullptr, "hook");
      }

      llvm::Function::arg_iterator args = function->arg_begin();
      llvm::Value* selfArg = &*args++;
      llvm::Value* messageArg = &*args++;
      argFrame = &*args;

      b.CreateStore(selfArg, regA);
      b.CreateStore(llvm::Constant::getNullValue(ptrTy), regB);
      b.CreateStore(wordC(0), regD);
      b.CreateStore(messageArg, regE);
      storeCell(0, wordC(0));   // the return-address sentinel

      for (std::map<size_t, BlockState>::iterator it = blocks.begin() ; it != blocks.end() ; it++)
         blockFor[it->first] = llvm::BasicBlock::Create(impl.context, "", function);

      b.CreateBr(blockAt(0));

      for (std::map<size_t, BlockState>::iterator bit = blocks.begin() ; bit != blocks.end() ; bit++) {
         size_t at = bit->first;
         BlockState state = bit->second;
         b.SetInsertPoint(blockAt(at));

         size_t index = byOffset[at];
         bool closed = false;
         for ( ; index < stream.size() && !closed ; index++) {
            const Instruction& ins = stream[index];

            // entering another block: fall through explicitly
            if (ins.offset != at && blockFor.count(ins.offset)) {
               b.CreateBr(blockAt(ins.offset));
               closed = true;
               break;
            }

            int depth = state.depth;
            int fpBase = state.fpBase;

            // pre-compute the state transition exactly as analyze() did
            if (ins.opcode == 0x9F || ins.opcode == 0x91) state.staticE = ins.arg;
            else if (writesE(ins.opcode)) state.staticE = -1;

            int delta = 0;
            const Instruction* unused = NULL;
            BlockState pre = state;
            pre.depth = depth; pre.fpBase = fpBase;
            cellDelta(ins, pre, delta, &unused);
            if (ins.opcode == 0x98) state.fpBase = depth;
            else if (ins.opcode == 0x15) { state.depth = fpBase; state.fpBase = -1; delta = 0; }
            else if (ins.opcode == 0xD2) { state.depth = fpBase + 1 + (int)ins.arg; delta = 0; }
            if (ins.opcode != 0x15 && ins.opcode != 0xD2)
               state.depth = depth + delta;

            if (!emitInstruction(ins, depth, fpBase, pre, hookFrames, foundFlag, closed))
               return false;
         }

         if (!closed && b.GetInsertBlock()->getTerminator() == NULL)
            b.CreateRet(loadA());
      }

      // defensive: every block must be terminated
      for (llvm::Function::iterator fit = function->begin() ; fit != function->end() ; fit++) {
         if (fit->getTerminator() == NULL) {
            llvm::IRBuilder<> tail(&*fit);
            tail.CreateRet(llvm::Constant::getNullValue(ptrTy));
         }
      }

      builder = NULL;
      return true;
   }

   bool emitInstruction(const Instruction& ins, int depth, int fpBase,
      const BlockState& pre, std::map<size_t, llvm::Value*>& hookFrames,
      llvm::Value* foundFlag, bool& closed)
   {
      llvm::IRBuilder<>& b = *builder;
      llvm::Type* i8 = llvm::Type::getInt8Ty(impl.context);
      llvm::Type* i16 = llvm::Type::getInt16Ty(impl.context);
      llvm::Type* i32 = llvm::Type::getInt32Ty(impl.context);
      llvm::Type* f64 = llvm::Type::getDoubleTy(impl.context);

      int top = depth - 1;
      switch (ins.opcode) {
         // -- no-ops and bookkeeping
         case 0x00: case 0x01: case 0x04: case 0x26:            // nop breakpoint snop exclude
         case 0x92: case 0xBF:                                  // restore reserve (unmanaged)
         case 0xD2:                                             // scopyf: static effect only
         case 0x15:                                             // close: static effect only
            break;
         case 0x98:                                             // open: the fp sentinel cell
            storeCell(depth, wordC(0));
            break;

         // -- pushes
         case 0x0A: storeCell(depth, toWord(loadA())); break;   // pusha
         case 0x02: storeCell(depth, toWord(loadB())); break;   // pushb
         case 0x22: storeCell(depth, loadD()); break;           // pushd
         case 0x05: storeCell(depth, loadE()); break;           // pushe
         case 0xB0: storeCell(depth, wordC(ins.arg)); break;    // pushn
         case 0xB2: storeCell(depth, toWord(referenceValue(ins.arg))); break; // pushr
         case 0xB4: {                                           // pushai i
            llvm::Value* slot = b.CreateConstInBoundsGEP1_64(wordTy, loadA(), ins.arg);
            storeCell(depth, b.CreateLoad(wordTy, slot));
            break;
         }
         case 0xB6: storeCell(depth, loadCell(fpBase + (int)ins.arg)); break; // pushfi
         case 0xBA: storeCell(depth, loadCell(top - (int)ins.arg)); break;    // pushsi
         case 0xBD: storeCell(depth, toWord(cellAddress(fpBase + (int)ins.arg))); break; // pushf

         // -- pops
         case 0x03: break;                                      // pop (discard)
         case 0x0B: storeA(toPtr(loadCell(top))); break;        // popa
         case 0x14: storeB(toPtr(loadCell(top))); break;        // popb
         case 0x23: storeD(loadCell(top)); break;               // popd
         case 0x0D: storeE(loadCell(top)); break;               // pope
         case 0xD0: break;                                      // popi n

         // -- register moves
         case 0x0C: storeA(loadB()); break;                     // acopyb
         case 0x12: storeB(loadA()); break;                     // bcopya
         case 0x21: storeD(loadE()); break;                     // dcopye
         case 0x20: storeE(loadD()); break;                     // ecopyd
         case 0x90: storeD(wordC(ins.arg)); break;              // dcopy
         case 0x91: storeE(wordC(ins.arg)); break;              // ecopy
         case 0x9E: storeA(referenceValue(ins.arg)); break;     // acopyr
         case 0x9A: storeB(referenceValue(ins.arg)); break;     // bcopyr
         case 0x9C: storeA(cellAddress(fpBase + (int)ins.arg)); break;  // acopyf
         case 0x9D: storeA(cellAddress(top - (int)ins.arg + 0)); break; // acopys
         case 0x9B: storeB(cellAddress(fpBase + (int)ins.arg)); break;  // bcopyf
         case 0x97: storeB(cellAddress(top - (int)ins.arg + 0)); break; // bcopys

         // -- frame / stack loads and stores
         case 0x94: storeA(toPtr(loadCell(fpBase + (int)ins.arg))); break; // aloadfi
         case 0x95: storeA(toPtr(loadCell(top - (int)ins.arg))); break;    // aloadsi
         case 0xC4: storeCell(fpBase + (int)ins.arg, toWord(loadA())); break; // asavefi
         case 0xC3: storeCell(top - (int)ins.arg, toWord(loadA())); break; // asavesi
         case 0xC8: storeB(toPtr(loadCell(fpBase + (int)ins.arg))); break; // bloadfi
         case 0xC9: storeB(toPtr(loadCell(top - (int)ins.arg))); break;   // bloadsi
         case 0xB7: storeD(loadCell(fpBase + (int)ins.arg)); break;       // dloadfi
         case 0xB8: storeD(loadCell(top - (int)ins.arg)); break;          // dloadsi
         case 0xB9: storeCell(fpBase + (int)ins.arg, loadD()); break;     // dsavefi
         case 0xBB: storeCell(top - (int)ins.arg, loadD()); break;        // dsavesi
         case 0xB1: storeE(loadCell(fpBase + (int)ins.arg)); break;       // eloadfi
         case 0xBC: storeE(loadCell(top - (int)ins.arg)); break;          // eloadsi
         case 0xB5: storeCell(fpBase + (int)ins.arg, loadE()); break;     // esavefi
         case 0xBE: storeCell(top - (int)ins.arg, loadE()); break;        // esavesi

         // -- global cells
         case 0x93: storeA(toPtr(b.CreateLoad(wordTy, referenceValue(ins.arg)))); break; // aloadr
         case 0xCC: b.CreateStore(toWord(loadA()), referenceValue(ins.arg)); break;      // asaver

         // -- object fields
         case 0xCD: {                                           // aloadai i
            llvm::Value* slot = b.CreateConstInBoundsGEP1_64(wordTy, loadA(), ins.arg);
            storeA(toPtr(b.CreateLoad(wordTy, slot)));
            break;
         }
         case 0xCE: {                                           // aloadbi i
            llvm::Value* slot = b.CreateConstInBoundsGEP1_64(wordTy, loadB(), ins.arg);
            storeA(toPtr(b.CreateLoad(wordTy, slot)));
            break;
         }
         case 0xC0:                                             // asavebi (write barrier)
            b.CreateCall(setField, { loadB(), wordC(ins.arg), loadA() });
            break;
         case 0xCF: {                                           // axsavebi (direct)
            llvm::Value* slot = b.CreateConstInBoundsGEP1_64(wordTy, loadB(), ins.arg);
            b.CreateStore(toWord(loadA()), slot);
            break;
         }
         case 0x18: {                                           // get: A = B[D]
            llvm::Value* slot = b.CreateInBoundsGEP(wordTy, loadB(), loadD());
            storeA(toPtr(b.CreateLoad(wordTy, slot)));
            break;
         }
         case 0x19:                                             // set (write barrier)
            b.CreateCall(setField, { loadB(), loadD(), loadA() });
            break;
         case 0x2F: {                                           // xset (direct)
            llvm::Value* slot = b.CreateInBoundsGEP(wordTy, loadB(), loadD());
            b.CreateStore(toWord(loadA()), slot);
            break;
         }

         // -- swaps
         case 0x2D: { llvm::Value* t = loadCell(top); storeCell(top, toWord(loadB())); storeB(toPtr(t)); break; } // bswap
         case 0x2C: { llvm::Value* t = loadCell(top); storeCell(top, loadE()); storeE(t); break; }                // eswap
         case 0xC2: { int c = top - (int)ins.arg; llvm::Value* t = loadCell(c); storeCell(c, toWord(loadA())); storeA(toPtr(t)); break; } // aswapsi
         case 0xC5: { int c = top - (int)ins.arg; llvm::Value* t = loadCell(c); storeCell(c, toWord(loadB())); storeB(toPtr(t)); break; } // bswapsi
         case 0xC6: { int c = top - (int)ins.arg; llvm::Value* t = loadCell(c); storeCell(c, loadE()); storeE(t); break; }               // eswapsi
         case 0xC7: { int c = top - (int)ins.arg; llvm::Value* t = loadCell(c); storeCell(c, loadD()); storeD(t); break; }               // dswapsi

         // -- D/E arithmetic
         case 0x09: storeD(b.CreateOr(loadD(), loadE())); break;           // or
         case 0x1E: storeD(b.CreateAdd(loadD(), loadE())); break;          // add
         case 0x16: storeD(b.CreateSub(loadD(), loadE())); break;          // sub
         case 0x1A: storeD(b.CreateAdd(loadD(), wordC(1))); break;         // inc
         case 0x13: storeD(b.CreateSub(loadD(), wordC(1))); break;         // dec
         case 0x10: storeD(b.CreateZExt(b.CreateICmpEQ(loadD(), wordC(0)), wordTy)); break; // not
         case 0xD6: storeD(b.CreateAdd(loadD(), wordC(ins.arg))); break;   // addn
         case 0xD5: storeD(b.CreateAnd(loadD(), wordC(ins.arg))); break;   // andn
         case 0xD7: storeD(b.CreateOr(loadD(), wordC(ins.arg))); break;    // orn
         case 0xDA: storeD(b.CreateMul(loadD(), wordC(ins.arg))); break;   // muln
         case 0xD9:                                                        // shiftn
            storeD(ins.arg >= 0 ? b.CreateAShr(loadD(), wordC(ins.arg))
                                : b.CreateShl(loadD(), wordC(-ins.arg)));
            break;
         case 0xD8: storeE(b.CreateAdd(loadE(), wordC(ins.arg))); break;   // eaddn
         case 0x06: storeD(b.CreateAnd(loadE(), wordC(0xFF000000u))); break; // dcopyverb
         case 0x0F: storeD(b.CreateAnd(loadE(), wordC(0x80FFFFF0u))); break; // dcopysubj
         case 0x08: storeD(b.CreateAnd(loadE(), wordC(0x0F))); break;        // dcopycount
         case 0xD3: storeE(b.CreateOr(b.CreateAnd(loadE(), wordC(~0x7F000000ll)), wordC(ins.arg))); break; // setverb
         case 0xD4: storeE(b.CreateOr(b.CreateAnd(loadE(), wordC(~0x00FFFFF0ll)), wordC(ins.arg))); break; // setsubj

         // -- control
         case 0xA0: b.CreateBr(blockAt(ins.target)); closed = true; break; // jump
         case 0xAD: branchOn(b.CreateICmpEQ(loadD(), loadE()), ins.target, ins.next); closed = true; break; // if
         case 0xAE: branchOn(b.CreateICmpNE(loadD(), loadE()), ins.target, ins.next); closed = true; break; // else
         case 0xA9: branchOn(b.CreateICmpSLT(loadD(), loadE()), ins.target, ins.next); closed = true; break; // less
         case 0xAA: branchOn(b.CreateICmpSGE(loadD(), loadE()), ins.target, ins.next); closed = true; break; // notless
         case 0xAB: branchOn(b.CreateICmpEQ(loadA(), loadB()), ins.target, ins.next); closed = true; break;  // ifb
         case 0xAC: branchOn(b.CreateICmpNE(loadA(), loadB()), ins.target, ins.next); closed = true; break;  // elseb
         case 0xFC: branchOn(b.CreateICmpEQ(loadD(), wordC(ins.extra)), ins.target, ins.next); closed = true; break; // ifn
         case 0xFD: branchOn(b.CreateICmpNE(loadD(), wordC(ins.extra)), ins.target, ins.next); closed = true; break; // elsen
         case 0xF7: branchOn(b.CreateICmpSLT(loadD(), wordC(ins.extra)), ins.target, ins.next); closed = true; break; // lessn
         case 0xF8: branchOn(b.CreateICmpEQ(loadE(), wordC(interned(ins.extra))), ins.target, ins.next); closed = true; break;  // ifm
         case 0xF9: branchOn(b.CreateICmpNE(loadE(), wordC(interned(ins.extra))), ins.target, ins.next); closed = true; break;  // elsem
         case 0xFA: case 0xFB: {                                // ifr / elser
            llvm::Value* rhs = (ins.extra == 0)
               ? llvm::Constant::getNullValue(ptrTy) : referenceValue(ins.extra);
            llvm::Value* c = (ins.opcode == 0xFA)
               ? b.CreateICmpEQ(loadA(), rhs) : b.CreateICmpNE(loadA(), rhs);
            branchOn(c, ins.target, ins.next);
            closed = true;
            break;
         }
         case 0xAF: {                                           // next: D++; if D<E jump
            storeD(b.CreateAdd(loadD(), wordC(1)));
            branchOn(b.CreateICmpSLT(loadD(), loadE()), ins.target, ins.next);
            closed = true;
            break;
         }

         // -- returns
         case 0x17: case 0x99: case 0x1B:                       // quit quitn equit
            b.CreateRet(loadA());
            closed = true;
            break;

         // -- allocation
         case 0x1F: storeA(b.CreateCall(allocNew, { loadA(), loadD() })); break;                    // create
         case 0x4F: storeA(b.CreateCall(allocBin, { loadA(), b.CreateMul(loadD(), wordC(4)) })); break; // ncreate
         case 0x5F: storeA(b.CreateCall(allocBin, { loadA(), b.CreateMul(loadD(), wordC(2)) })); break; // wcreate
         case 0x6F: storeA(b.CreateCall(allocBin, { loadA(), loadD() })); break;                    // bcreate
         case 0xF0: storeA(b.CreateCall(allocNew, { referenceValue(ins.arg), wordC(ins.extra) })); break; // new
         case 0xF1: storeA(b.CreateCall(allocBin, { referenceValue(ins.arg), wordC(ins.extra) })); break; // newn

         // -- introspection
         case 0x36: storeA(vmtOf(loadA())); break;              // class
         case 0x33: storeE(b.CreateCall(lenFn, { loadB(), wordC(0x33) })); break;  // flag
         case 0x11: storeE(b.CreateCall(lenFn, { loadB(), wordC(0x11) })); break;  // len
         case 0x30: storeE(b.CreateCall(lenFn, { loadB(), wordC(0x30) })); break;  // xlen
         case 0x31: storeE(b.CreateCall(lenFn, { loadB(), wordC(0x31) })); break;  // blen
         case 0x32: storeE(b.CreateCall(lenFn, { loadB(), wordC(0x32) })); break;  // wlen
         case 0x34: storeE(b.CreateCall(lenFn, { loadB(), wordC(0x34) })); break;  // nlen
         case 0x3A: b.CreateCall(validateFn, { loadA() }); break;                  // validate

         // -- exceptions
         case 0x07:                                             // throw
            b.CreateCall(throwFn, { loadA() });
            b.CreateUnreachable();
            closed = true;
            break;
         case 0x1D: b.CreateCall(unhookFn, {}); break;          // unhook
         case 0xA6: {                                           // hook label
            llvm::Value* frame = hookFrames[ins.offset];
            b.CreateCall(hookPush, { frame });
            llvm::Value* buf = b.CreateConstInBoundsGEP1_64(i8, frame, 16);
            llvm::Value* sj = b.CreateCall(setjmpFn, { buf });
            llvm::BasicBlock* handler = llvm::BasicBlock::Create(impl.context, "", function);
            llvm::BasicBlock* cont = llvm::BasicBlock::Create(impl.context, "", function);
            b.CreateCondBr(b.CreateICmpNE(sj, llvm::ConstantInt::get(i32, 0)), handler, cont);
            b.SetInsertPoint(handler);
            storeA(b.CreateCall(currentEx, {}));
            b.CreateBr(blockAt(ins.target));
            b.SetInsertPoint(cont);
            break;
         }

         // -- sends and calls
         case 0xA2: case 0x39: {                                // acallvi / acallvd
            llvm::Value* frame = cellAddress(depth);
            llvm::Value* index = (ins.opcode == 0xA2) ? (llvm::Value*)wordC(ins.arg) : loadD();
            storeA(b.CreateCall(sendVi, { loadA(), loadE(), frame, index }));
            break;
         }
         case 0xA1: {                                           // ajumpvi (tail)
            llvm::Value* frame = cellAddress(depth);
            b.CreateRet(b.CreateCall(sendVi, { loadA(), loadE(), frame, wordC(ins.arg) }));
            closed = true;
            break;
         }
         case 0xFE: case 0xF5: {                                // xcallrm / xjumprm
            unsigned int message = ins.extra;
            if (impl.callbacks.internMessage)
               message = impl.callbacks.internMessage(impl.callbacks.context, (unsigned int)ins.extra);
            const char* className = impl.callbacks.referenceName
               ? impl.callbacks.referenceName(impl.callbacks.context, (unsigned int)ins.arg & 0x00FFFFFF)
               : NULL;
            char buffer[16];
            snprintf(buffer, sizeof(buffer), ".%X", message);
            std::string target = "elena.m.";
            target += className ? sanitized(className) : "unknown";
            target += buffer;
            llvm::FunctionCallee callee = impl.module->getOrInsertFunction(target, methodType());
            llvm::Value* frame = cellAddress(depth);
            llvm::Value* r = b.CreateCall(callee, { loadA(), wordC(interned(ins.extra)), frame });
            if (ins.opcode == 0xF5) {
               b.CreateRet(r);
               closed = true;
            }
            else storeA(r);
            break;
         }
         case 0xA3: {                                           // callr (procedure)
            const char* procName = impl.callbacks.referenceName
               ? impl.callbacks.referenceName(impl.callbacks.context, (unsigned int)ins.arg & 0x00FFFFFF)
               : NULL;
            unsigned int mask = (unsigned int)ins.arg & 0xFF000000;
            bool isSymbol = (mask == 0x12000000 || mask == 0x32000000);
            std::string target = isSymbol ? "elena.sym." : "elena.p.";
            target += procName ? sanitized(procName) : "unknown";
            llvm::FunctionCallee callee = impl.module->getOrInsertFunction(target, methodType());
            storeA(b.CreateCall(callee, { loadA(), loadE(), cellAddress(depth) }));
            break;
         }
         case 0x38: {                                           // call [A]
            llvm::Value* code = b.CreateLoad(ptrTy, loadA());
            storeA(b.CreateCall(methodType(), code,
               { loadA(), loadE(), cellAddress(depth) }));
            break;
         }
         case 0xA5: {                                           // callextr
            // typed FFI (plan 18 / P4) emits the real per-signature call;
            // until then the runtime reports which external was reached
            const char* importName = impl.callbacks.referenceName
               ? impl.callbacks.referenceName(impl.callbacks.context,
                    (unsigned int)ins.arg & 0x00FFFFFF)
               : NULL;
            llvm::Value* name = b.CreateGlobalString(importName ? importName : "?");
            llvm::FunctionCallee stub = impl.module->getOrInsertFunction(
               "elena_external_stub",
               llvm::FunctionType::get(wordTy, { ptrTy }, false));
            storeD(b.CreateCall(stub, { name }));
            break;
         }
         case 0x0E: {                                           // bsredirect
            llvm::Value* r = b.CreateCall(resend,
               { loadA(), loadE(), cellAddress(depth), foundFlag });
            llvm::Value* found = b.CreateLoad(i8, foundFlag);
            llvm::BasicBlock* done = llvm::BasicBlock::Create(impl.context, "", function);
            llvm::BasicBlock* cont = llvm::BasicBlock::Create(impl.context, "", function);
            b.CreateCondBr(b.CreateICmpNE(found, llvm::ConstantInt::get(i8, 0)), done, cont);
            b.SetInsertPoint(done);
            b.CreateRet(r);
            b.SetInsertPoint(cont);
            break;
         }
         case 0x37:                                             // mindex
            storeD(b.CreateCall(impl.module->getOrInsertFunction("elena_mindex",
               llvm::FunctionType::get(wordTy, { ptrTy, wordTy }, false)),
               { loadA(), loadE() }));
            break;
         case 0xF4:                                             // xindexrm
            storeD(b.CreateCall(impl.module->getOrInsertFunction("elena_mindex",
               llvm::FunctionType::get(wordTy, { ptrTy, wordTy }, false)),
               { referenceValue(ins.arg), wordC(ins.extra) }));
            break;

         // -- selection
         case 0xF6: storeA(toPtr(b.CreateSelect(b.CreateICmpEQ(loadD(), wordC(0)),
               toWord(referenceValue(ins.arg)), toWord(referenceValue(ins.extra))))); break; // selectr
         case 0xF3: storeA(toPtr(b.CreateSelect(
               b.CreateICmpEQ(loadA(), llvm::Constant::getNullValue(ptrTy)),
               toWord(referenceValue(ins.arg)), toWord(referenceValue(ins.extra))))); break; // xselectr

         // -- 32-bit payload primitives
         case 0x48: storeD(b.CreateSExt(payload32(loadA()), wordTy)); break;       // nload
         case 0x47: storePayload32(loadB(), b.CreateTrunc(loadD(), i32)); break;   // nsave
         case 0xCA: { llvm::Value* p = b.CreateConstInBoundsGEP1_64(i32, loadA(), ins.arg);
                      storeD(b.CreateSExt(b.CreateLoad(i32, p), wordTy)); break; } // nloadi
         case 0xCB: { llvm::Value* p = b.CreateConstInBoundsGEP1_64(i32, loadB(), ins.arg);
                      b.CreateStore(b.CreateTrunc(loadD(), i32), p); break; }      // nsavei
         case 0x42: storePayload32(loadB(), payload32(loadA())); break;            // ncopy
         case 0x50: storePayload32(loadA(), payload32(loadB())); break;            // ncopyb
         case 0x43: storePayload32(loadB(), b.CreateAdd(payload32(loadB()), payload32(loadA()))); break; // nadd
         case 0x44: storePayload32(loadB(), b.CreateSub(payload32(loadB()), payload32(loadA()))); break; // nsub
         case 0x45: storePayload32(loadB(), b.CreateMul(payload32(loadB()), payload32(loadA()))); break; // nmul
         case 0x46: storePayload32(loadB(), b.CreateSDiv(payload32(loadB()), payload32(loadA()))); break;// ndiv
         case 0x4A: storePayload32(loadB(), b.CreateAnd(payload32(loadB()), payload32(loadA()))); break; // nand
         case 0x4B: storePayload32(loadB(), b.CreateOr(payload32(loadB()), payload32(loadA()))); break;  // nor
         case 0x4C: storePayload32(loadB(), b.CreateXor(payload32(loadB()), payload32(loadA()))); break; // nxor
         case 0x4E: storePayload32(loadB(), b.CreateNot(payload32(loadA()))); break;                     // nnot
         case 0x4D: {                                           // nshift: by D, sign selects direction
            llvm::Value* v = payload32(loadA());
            llvm::Value* d32 = b.CreateTrunc(loadD(), i32);
            llvm::Value* neg = b.CreateICmpSLT(d32, llvm::ConstantInt::get(i32, 0));
            llvm::Value* left = b.CreateShl(v, b.CreateNeg(d32));
            llvm::Value* right = b.CreateLShr(v, d32);
            storePayload32(loadB(), b.CreateSelect(neg, left, right));
            break;
         }
         case 0x40: storeD(b.CreateZExt(b.CreateICmpEQ(payload32(loadA()), payload32(loadB())), wordTy)); break; // nequal
         case 0x41: storeD(b.CreateZExt(b.CreateICmpSLT(payload32(loadA()), payload32(loadB())), wordTy)); break; // nless

         // -- 64-bit payload primitives
         case 0x70: storePayload64(loadB(), payload64(loadA())); break;    // lcopy
         case 0x51: storePayload64(loadA(), payload64(loadB())); break;    // lcopyb
         case 0x74: storePayload64(loadB(), b.CreateAdd(payload64(loadB()), payload64(loadA()))); break; // ladd
         case 0x75: storePayload64(loadB(), b.CreateSub(payload64(loadB()), payload64(loadA()))); break; // lsub
         case 0x76: storePayload64(loadB(), b.CreateMul(payload64(loadB()), payload64(loadA()))); break; // lmul
         case 0x77: storePayload64(loadB(), b.CreateSDiv(payload64(loadB()), payload64(loadA()))); break;// ldiv
         case 0x78: storePayload64(loadB(), b.CreateAnd(payload64(loadB()), payload64(loadA()))); break; // land
         case 0x79: storePayload64(loadB(), b.CreateOr(payload64(loadB()), payload64(loadA()))); break;  // lor
         case 0x7A: storePayload64(loadB(), b.CreateXor(payload64(loadB()), payload64(loadA()))); break; // lxor
         case 0x7C: storePayload64(loadB(), b.CreateNot(payload64(loadA()))); break;                     // lnot
         case 0x7B: storePayload64(loadB(), b.CreateShl(payload64(loadB()), loadD())); break;            // lshift
         case 0x72: storeD(b.CreateZExt(b.CreateICmpEQ(payload64(loadA()), payload64(loadB())), wordTy)); break; // lequal
         case 0x73: storeD(b.CreateZExt(b.CreateICmpSLT(payload64(loadA()), payload64(loadB())), wordTy)); break; // lless

         // -- real primitives (the F register mirrors the x87 accumulator)
         case 0x80: b.CreateStore(b.CreateSIToFP(loadD(), f64), regF); break;   // rcopy
         case 0x8F: b.CreateStore(payloadF64(loadA()), regF); break;            // rload
         case 0x82: storePayloadF64(loadB(), b.CreateLoad(f64, regF)); break;   // rsave
         case 0x49: storeD(b.CreateFPToSI(b.CreateLoad(f64, regF), wordTy)); break; // dcopyr
         case 0x85: storePayloadF64(loadB(), b.CreateFAdd(payloadF64(loadB()), payloadF64(loadA()))); break; // radd
         case 0x86: storePayloadF64(loadB(), b.CreateFSub(payloadF64(loadB()), payloadF64(loadA()))); break; // rsub
         case 0x87: storePayloadF64(loadB(), b.CreateFMul(payloadF64(loadB()), payloadF64(loadA()))); break; // rmul
         case 0x88: storePayloadF64(loadB(), b.CreateFDiv(payloadF64(loadB()), payloadF64(loadA()))); break; // rdiv
         case 0x83: storeD(b.CreateZExt(b.CreateFCmpOEQ(payloadF64(loadA()), payloadF64(loadB())), wordTy)); break; // requal
         case 0x84: storeD(b.CreateZExt(b.CreateFCmpOLT(payloadF64(loadA()), payloadF64(loadB())), wordTy)); break; // rless
         case 0x8A: storePayloadF64(loadB(), libmUnary("exp", payloadF64(loadA()))); break;   // rexp
         case 0x8B: storePayloadF64(loadB(), libmUnary("log", payloadF64(loadA()))); break;   // rln
         case 0x8C: storePayloadF64(loadB(), libmUnary("fabs", payloadF64(loadA()))); break;  // rabs
         case 0x8D: storePayloadF64(loadB(), libmUnary("round", payloadF64(loadA()))); break; // rround
         case 0x8E: storePayloadF64(loadB(), libmUnary("trunc", payloadF64(loadA()))); break; // rint
         case 0x66: storePayloadF64(loadB(), libmUnary("sin", payloadF64(loadA()))); break;   // rsin
         case 0x67: storePayloadF64(loadB(), libmUnary("cos", payloadF64(loadA()))); break;   // rcos
         case 0x68: storePayloadF64(loadB(), libmUnary("atan", payloadF64(loadA()))); break;  // rarctan

         // -- byte/short/int array elements
         case 0x65: { llvm::Value* p = b.CreateInBoundsGEP(i8, loadA(), loadD());
                      storeE(b.CreateZExt(b.CreateLoad(i8, p), wordTy)); break; }    // breadb
         case 0x60: { llvm::Value* p = b.CreateInBoundsGEP(i8, loadA(), loadD());
                      storeE(b.CreateZExt(b.CreateLoad(i16, p), wordTy)); break; }   // breadw
         case 0x61: { llvm::Value* p = b.CreateInBoundsGEP(i8, loadA(), loadD());
                      storeE(b.CreateZExt(b.CreateLoad(i32, p), wordTy)); break; }   // bread
         case 0x6C: { llvm::Value* p = b.CreateInBoundsGEP(i8, loadB(), loadD());
                      b.CreateStore(b.CreateTrunc(loadE(), i8), p); break; }         // bwriteb
         case 0x6D: { llvm::Value* p = b.CreateInBoundsGEP(i8, loadB(), loadD());
                      b.CreateStore(b.CreateTrunc(loadE(), i16), p); break; }        // bwritew
         case 0x69: { llvm::Value* p = b.CreateInBoundsGEP(i8, loadB(), loadD());
                      b.CreateStore(b.CreateTrunc(loadE(), i32), p); break; }        // bwrite
         case 0x59: { llvm::Value* p = b.CreateInBoundsGEP(i16, loadA(), loadD());
                      storeE(b.CreateZExt(b.CreateLoad(i16, p), wordTy)); break; }   // wread
         case 0x5A: { llvm::Value* p = b.CreateInBoundsGEP(i16, loadB(), loadD());
                      b.CreateStore(b.CreateTrunc(loadE(), i16), p); break; }        // wwrite
         case 0x5B: { llvm::Value* p = b.CreateInBoundsGEP(i32, loadA(), loadD());
                      storeE(b.CreateZExt(b.CreateLoad(i32, p), wordTy)); break; }   // nread
         case 0x5C: { llvm::Value* p = b.CreateInBoundsGEP(i32, loadB(), loadD());
                      b.CreateStore(b.CreateTrunc(loadE(), i32), p); break; }        // nwrite

         // -- block copies and locks (runtime)
         case 0x2E: b.CreateCall(impl.module->getOrInsertFunction("elena_copy",
               llvm::FunctionType::get(llvm::Type::getVoidTy(impl.context), { ptrTy, ptrTy }, false)),
               { loadB(), loadA() }); break;                    // copy
         case 0x52: b.CreateCall(impl.module->getOrInsertFunction("elena_copy",
               llvm::FunctionType::get(llvm::Type::getVoidTy(impl.context), { ptrTy, ptrTy }, false)),
               { loadA(), loadB() }); break;                    // copyb
         case 0x27: storeD(b.CreateCall(trylockFn, { loadA() })); break;  // trylock
         case 0x28: b.CreateCall(freelockFn, { loadA() }); break;         // freelock
         case 0x9F: storeE(wordC(interned(ins.arg))); break;              // copym
         case 0x96: {                                           // ifheap
            llvm::Value* c = b.CreateCall(impl.module->getOrInsertFunction("elena_isheap",
               llvm::FunctionType::get(wordTy, { ptrTy }, false)), { loadA() });
            branchOn(b.CreateICmpNE(c, wordC(0)), ins.target, ins.next);
            closed = true;
            break;
         }
         case 0xA7:                                             // address label: E = code address
            storeE(wordC(0));                                   // placeholder until needed
            break;

         default:
            return fail(ins, "no emission rule");
      }
      return true;
   }
};

}

bool LLVMGenerator :: translateProcedure(const char* name, const unsigned char* code,
   size_t length, unsigned int entryMessage, TranslateStats& stats, GenError& error)
{
   Impl* self = impl(_impl);
   if (!self->machine) {
      error.copy("generator is not initialized");
      return false;
   }

   stats.procedures++;

   Translator translator(*self, error);
   translator.code = code;
   translator.length = length;
   translator.entryMessage = entryMessage;

   if (!translator.decode() || !translator.analyze()) {
      stats.failed++;
      // attribute the failure to the opcode named in the error message
      unsigned int opcode = 0;
      if (sscanf((const char*)error, "opcode %02X", &opcode) == 1 && opcode < 256)
         stats.failedOpcode[opcode]++;

      if (getenv("ELENA_LLVM_TRACE")) {
         printf("  -- %s (entry message %08X)\n", name, entryMessage);
         for (size_t i = 0 ; i < translator.stream.size() ; i++) {
            const Instruction& ins = translator.stream[i];
            if (ins.jump) {
               printf("     +%04X  %02X  -> +%04X (%lld)\n", (unsigned int)ins.offset,
                  ins.opcode, (unsigned int)ins.target, ins.extra);
            }
            else printf("     +%04X  %02X  %lld, %lld\n", (unsigned int)ins.offset,
               ins.opcode, ins.arg, ins.extra);
         }
      }
      return false;
   }

   std::string symbol(entryMessage != 0 ? "m." : "sym.");
   symbol += name;
   if (!translator.emitFunction(symbol.c_str())) {
      stats.failed++;
      unsigned int opcode = 0;
      if (sscanf((const char*)error, "opcode %02X", &opcode) == 1 && opcode < 256)
         stats.failedOpcode[opcode]++;
      // discard the partial body, keep the declaration
      if (translator.function && !translator.function->isDeclaration())
         translator.function->deleteBody();
      return false;
   }

   stats.translated++;

   return true;
}

static std::string sanitizedName(const char* name)
{
   std::string out(name);
   for (size_t i = 0 ; i < out.size() ; i++)
      if (out[i] == '\'' || out[i] == ':' || out[i] == '#') out[i] = '.';
   return out;
}

bool LLVMGenerator :: emitVMT(const char* className, const char* classClassName,
   unsigned long long flags, unsigned int count,
   const unsigned int* messages, const char** functions, GenError& error)
{
   Impl* self = impl(_impl);
   if (!self->machine) { error.copy("generator is not initialized"); return false; }

   llvm::LLVMContext& ctx = self->context;
   llvm::IntegerType* wordTy = self->wordType();
   llvm::PointerType* ptrTy = llvm::PointerType::get(ctx, 0);
   llvm::FunctionType* methodTy =
      llvm::FunctionType::get(ptrTy, { ptrTy, wordTy, ptrTy }, false);
   llvm::StructType* entryTy = llvm::StructType::get(ctx, { wordTy, ptrTy });

   std::string symbol = "elena.vmt." + sanitizedName(className);
   if (llvm::GlobalValue* present = self->module->getNamedValue(symbol)) {
      if (!present->isDeclaration())
         return true;
   }

   llvm::Constant* classClass = llvm::Constant::getNullValue(ptrTy);
   if (classClassName && classClassName[0]) {
      std::string ccSymbol = "elena.vmt." + sanitizedName(classClassName);
      llvm::GlobalValue* cc = self->module->getNamedValue(ccSymbol);
      if (!cc)
         cc = new llvm::GlobalVariable(*self->module, wordTy, false,
            llvm::GlobalValue::ExternalLinkage, nullptr, ccSymbol);
      classClass = cc;
   }

   std::string ownPrefix = "elena.m." + sanitizedName(className) + ".";

   std::vector<llvm::Constant*> entries;
   for (unsigned int i = 0 ; i < count ; i++) {
      llvm::FunctionCallee fn = self->module->getOrInsertFunction(functions[i], methodTy);
      entries.push_back(llvm::ConstantStruct::get(entryTy,
         { llvm::ConstantInt::get(wordTy, messages[i]),
           llvm::cast<llvm::Constant>(fn.getCallee()) }));

      // an inherited entry is also reachable by a DIRECT resolved call
      // against THIS class (xcallrm) -- provide the name as a tail-call
      // thunk to the defining ancestor's body
      char hex[12];
      snprintf(hex, sizeof(hex), "%X", messages[i]);
      std::string direct = ownPrefix + hex;
      if (direct != functions[i] && !self->module->getFunction(direct)) {
         llvm::Function* thunk = llvm::Function::Create(methodTy,
            llvm::Function::ExternalLinkage, direct, self->module.get());
         llvm::IRBuilder<> b(llvm::BasicBlock::Create(ctx, "entry", thunk));
         llvm::Function::arg_iterator args = thunk->arg_begin();
         llvm::Value* a0 = &*args++;
         llvm::Value* a1 = &*args++;
         llvm::Value* a2 = &*args;
         llvm::CallInst* call = b.CreateCall(fn, { a0, a1, a2 });
         call->setTailCall(true);
         b.CreateRet(call);
      }
   }
   entries.push_back(llvm::ConstantStruct::get(entryTy,
      { llvm::Constant::getAllOnesValue(wordTy), llvm::Constant::getNullValue(ptrTy) }));

   llvm::ArrayType* tableTy = llvm::ArrayType::get(entryTy, entries.size());
   llvm::StructType* imageTy = llvm::StructType::get(ctx,
      { wordTy, wordTy, ptrTy, tableTy });
   llvm::GlobalVariable* image = new llvm::GlobalVariable(*self->module, imageTy,
      false, llvm::GlobalValue::PrivateLinkage,
      llvm::ConstantStruct::get(imageTy, {
         llvm::ConstantInt::get(wordTy, count),
         llvm::ConstantInt::get(wordTy, flags),
         classClass,
         llvm::ConstantArray::get(tableTy, entries) }),
      symbol + ".image");

   llvm::Constant* indexes[2] = {
      llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0),
      llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 3) };
   llvm::Constant* at = llvm::ConstantExpr::getInBoundsGetElementPtr(
      imageTy, image, llvm::ArrayRef<llvm::Constant*>(indexes, 2));

   if (llvm::GlobalValue* placeholder = self->module->getNamedValue(symbol)) {
      llvm::GlobalAlias* alias = llvm::GlobalAlias::create(tableTy, 0,
         llvm::GlobalValue::ExternalLinkage, symbol + ".table", at, self->module.get());
      placeholder->replaceAllUsesWith(alias);
      placeholder->eraseFromParent();
      alias->setName(symbol);
   }
   else llvm::GlobalAlias::create(tableTy, 0,
      llvm::GlobalValue::ExternalLinkage, symbol, at, self->module.get());

   return true;
}

bool LLVMGenerator :: emitClassConstant(const char* className, GenError& error)
{
   Impl* self = impl(_impl);
   if (!self->machine) { error.copy("generator is not initialized"); return false; }

   llvm::LLVMContext& ctx = self->context;
   llvm::IntegerType* wordTy = self->wordType();
   llvm::PointerType* ptrTy = llvm::PointerType::get(ctx, 0);

   std::string symbol = "elena.const." + sanitizedName(className);
   if (llvm::GlobalValue* present = self->module->getNamedValue(symbol)) {
      if (!present->isDeclaration())
         return true;
   }

   std::string vmtSymbol = "elena.vmt." + sanitizedName(className);
   llvm::GlobalValue* vmt = self->module->getNamedValue(vmtSymbol);
   if (!vmt)
      vmt = new llvm::GlobalVariable(*self->module, wordTy, false,
         llvm::GlobalValue::ExternalLinkage, nullptr, vmtSymbol);

   llvm::StructType* imageTy = llvm::StructType::get(ctx, { wordTy, ptrTy });
   llvm::GlobalVariable* image = new llvm::GlobalVariable(*self->module, imageTy,
      false, llvm::GlobalValue::PrivateLinkage,
      llvm::ConstantStruct::get(imageTy,
         { llvm::ConstantInt::get(wordTy, 0), vmt }),
      symbol + ".image");

   llvm::Constant* offset = llvm::ConstantInt::get(wordTy, 2 * (self->target->pointerBits / 8));
   llvm::Constant* at = llvm::ConstantExpr::getGetElementPtr(
      llvm::Type::getInt8Ty(ctx), image, offset);

   if (llvm::GlobalValue* placeholder = self->module->getNamedValue(symbol)) {
      llvm::GlobalAlias* alias = llvm::GlobalAlias::create(imageTy, 0,
         llvm::GlobalValue::ExternalLinkage, symbol + ".obj", at, self->module.get());
      placeholder->replaceAllUsesWith(alias);
      placeholder->eraseFromParent();
      alias->setName(symbol);
   }
   else llvm::GlobalAlias::create(imageTy, 0,
      llvm::GlobalValue::ExternalLinkage, symbol, at, self->module.get());

   return true;
}

bool LLVMGenerator :: emitEntry(const char* symbolName, GenError& error)
{
   Impl* self = impl(_impl);
   if (!self->machine) {
      error.copy("generator is not initialized");
      return false;
   }

   llvm::LLVMContext& ctx = self->context;
   llvm::IntegerType* wordTy = self->wordType();
   llvm::PointerType* ptrTy = llvm::PointerType::get(ctx, 0);
   llvm::FunctionType* methodTy =
      llvm::FunctionType::get(ptrTy, { ptrTy, wordTy, ptrTy }, false);

   std::string target = "elena.sym.";
   {
      std::string s(symbolName);
      for (size_t i = 0 ; i < s.size() ; i++)
         if (s[i] == '\'' || s[i] == ':' || s[i] == '#') s[i] = '.';
      target += s;
   }

   llvm::FunctionCallee symbol = self->module->getOrInsertFunction(target, methodTy);
   llvm::Function* entry = llvm::Function::Create(
      llvm::FunctionType::get(ptrTy, false),
      llvm::Function::ExternalLinkage, "elena_program", self->module.get());

   llvm::IRBuilder<> b(llvm::BasicBlock::Create(ctx, "entry", entry));
   llvm::Value* r = b.CreateCall(symbol, {
      llvm::Constant::getNullValue(ptrTy),
      llvm::ConstantInt::get(wordTy, 0),
      llvm::Constant::getNullValue(ptrTy) });
   b.CreateRet(r);

   return true;
}

void LLVMGenerator :: emitStubs()
{
   Impl* self = impl(_impl);
   llvm::LLVMContext& ctx = self->context;
   llvm::IntegerType* wordTy = self->wordType();
   llvm::PointerType* ptrTy = llvm::PointerType::get(ctx, 0);

   llvm::FunctionCallee report = self->module->getOrInsertFunction(
      "elena_unimplemented",
      llvm::FunctionType::get(llvm::Type::getVoidTy(ctx),
         { llvm::PointerType::get(ctx, 0) }, false));

   // undefined elena.* data becomes a zeroed cell wide enough to be read
   // as a (degenerate, empty) dispatch table
   for (llvm::Module::global_iterator it = self->module->global_begin() ;
        it != self->module->global_end() ; it++)
   {
      if (it->isDeclaration() && it->getName().starts_with("elena.")) {
         it->setInitializer(llvm::Constant::getNullValue(it->getValueType()));
         it->setLinkage(llvm::GlobalValue::WeakAnyLinkage);
      }
   }

   // undefined elena.* code reports itself and fails
   std::vector<llvm::Function*> pending;
   for (llvm::Module::iterator it = self->module->begin() ;
        it != self->module->end() ; it++)
   {
      if (it->isDeclaration() && it->getName().starts_with("elena."))
         pending.push_back(&*it);
   }
   for (size_t i = 0 ; i < pending.size() ; i++) {
      llvm::Function* fn = pending[i];
      llvm::IRBuilder<> b(llvm::BasicBlock::Create(ctx, "entry", fn));
      b.CreateCall(report, { b.CreateGlobalString(fn->getName()) });
      if (fn->getReturnType()->isVoidTy())
         b.CreateRetVoid();
      else if (fn->getReturnType()->isPointerTy())
         b.CreateRet(llvm::Constant::getNullValue(fn->getReturnType()));
      else b.CreateRet(llvm::Constant::getNullValue(fn->getReturnType()));
      fn->setLinkage(llvm::GlobalValue::WeakAnyLinkage);
   }
   (void)wordTy;
}

bool LLVMGenerator :: verify(GenError& error)
{
   Impl* self = impl(_impl);

   std::string message;
   llvm::raw_string_ostream stream(message);
   if (llvm::verifyModule(*self->module, &stream)) {
      stream.flush();
      if (message.size() > 500)
         message.resize(500);
      error.copy(message.c_str());
      return false;
   }
   return true;
}

bool LLVMGenerator :: optimize(int level, GenError& error)
{
   Impl* self = impl(_impl);
   if (!self->machine) {
      error.copy("generator is not initialized");
      return false;
   }

   llvm::PassBuilder builder(self->machine.get());

   llvm::LoopAnalysisManager     lam;
   llvm::FunctionAnalysisManager fam;
   llvm::CGSCCAnalysisManager    cam;
   llvm::ModuleAnalysisManager   mam;

   builder.registerModuleAnalyses(mam);
   builder.registerCGSCCAnalyses(cam);
   builder.registerFunctionAnalyses(fam);
   builder.registerLoopAnalyses(lam);
   builder.crossRegisterProxies(lam, fam, cam, mam);

   llvm::ModulePassManager passes = (level <= 0)
      ? builder.buildO0DefaultPipeline(llvm::OptimizationLevel::O0)
      : builder.buildPerModuleDefaultPipeline(
           level == 1 ? llvm::OptimizationLevel::O1 : llvm::OptimizationLevel::O2);

   passes.run(*self->module, mam);

   return true;
}

static bool emitCodeFile(Impl* self, const char* path,
   llvm::CodeGenFileType kind, GenError& error)
{
   if (!self->machine) {
      error.copy("generator is not initialized");
      return false;
   }

   std::error_code code;
   llvm::raw_fd_ostream out(path, code, llvm::sys::fs::OF_None);
   if (code) {
      error.copy(code.message().c_str());
      return false;
   }

   llvm::legacy::PassManager passes;
   if (self->machine->addPassesToEmitFile(passes, out, nullptr, kind)) {
      error.copy("the target cannot emit this file kind");
      return false;
   }

   passes.run(*self->module);
   out.flush();

   return true;
}

bool LLVMGenerator :: emitObject(const char* path, GenError& error)
{
   return emitCodeFile(impl(_impl), path, llvm::CodeGenFileType::ObjectFile, error);
}

bool LLVMGenerator :: emitAssembly(const char* path, GenError& error)
{
   return emitCodeFile(impl(_impl), path, llvm::CodeGenFileType::AssemblyFile, error);
}

bool LLVMGenerator :: emitIR(const char* path, GenError& error)
{
   Impl* self = impl(_impl);

   std::error_code code;
   llvm::raw_fd_ostream out(path, code, llvm::sys::fs::OF_None);
   if (code) {
      error.copy(code.message().c_str());
      return false;
   }

   self->module->print(out, nullptr);
   out.flush();

   return true;
}

// --- self test -------------------------------------------------------------

int _ELENA_::llvmSelfTest(const char* scratchDir)
{
   size_t count = 0;
   const TargetInfo* targets = getTargetList(count);

   int failures = 0;
   for (size_t i = 0 ; i < count ; i++) {
      LLVMGenerator generator;
      GenError error;

      if (!generator.init(&targets[i], error)) {
         printf("%-10s FAILED: %s\n", targets[i].name, (const char*)error);
         failures++;
         continue;
      }

      String<char, 512> path(scratchDir);
      path.append("/selftest-");
      path.append(targets[i].name);
      path.append(".o");

      if (!generator.emitObject(path, error)) {
         printf("%-10s FAILED: %s\n", targets[i].name, (const char*)error);
         failures++;
         continue;
      }

      printf("%-10s ok (%s)\n", targets[i].name, targets[i].triple);
   }

   return failures;
}
