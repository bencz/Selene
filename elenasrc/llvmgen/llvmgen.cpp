//---------------------------------------------------------------------------
//      S E L E N E   P r o j e c t:  LLVM code generation backend
//
//      The ONLY translation unit that includes LLVM headers. Everything it
//      exposes is plain C++ (see llvmgen.h), so LLVM's API churn is contained
//      here rather than spread through the compiler.
//---------------------------------------------------------------------------

#include "elena.h"
// --------------------------------------------------------------------------
#include "targetinfo.h"
#include "bytecode.h"
#include "llvmgen.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/IR/Constants.h>

#include <string>
#include <memory>
#include <map>
#include <vector>
#include <set>

using namespace _ELENA_;

namespace
{
   // Registers the back ends compiled into this build. Idempotent.
   void initializeTargets()
   {
      static bool done = false;
      if (done)
         return;

      // Only the back ends this build links, named one by one.
      //
      // InitializeAll*() would reference every target compiled into libLLVM --
      // AMDGPU, WebAssembly, XCore and a dozen others we neither link nor want.
      // Listing them here keeps the enabled set in the source, matching
      // ELENA_LLVM_TARGETS in CMakeLists.txt.
      #define SELENE_INIT_TARGET(NAME)      \
         LLVMInitialize##NAME##TargetInfo();  \
         LLVMInitialize##NAME##Target();      \
         LLVMInitialize##NAME##TargetMC();    \
         LLVMInitialize##NAME##AsmPrinter();  \
         LLVMInitialize##NAME##AsmParser();

      SELENE_INIT_TARGET(X86)        // x86, x86-64
      SELENE_INIT_TARGET(AArch64)    // arm64
      SELENE_INIT_TARGET(PowerPC)    // ppc32, ppc64, ppc64le
      SELENE_INIT_TARGET(SystemZ)    // s390x

      #undef SELENE_INIT_TARGET

      done = true;
   }

   struct Impl
   {
      llvm::LLVMContext                    context;
      std::unique_ptr<llvm::Module>        module;
      std::unique_ptr<llvm::TargetMachine> machine;

      std::string triple;
      std::string lastError;

      const TargetInfo* target = nullptr;

      // --- Selene's calling convention ---
      //
      // Every method returns { ptr, i1 }: the value and whether the message
      // succeeded. Failure is Selene's primary conditional -- #if and #loop are
      // built on it -- so it is a branch on a flag, never an unwind.
      //
      //   docs/plan/23-failure-abi.md
      llvm::StructType* resultType = nullptr;

      void defineResultType()
      {
         resultType = llvm::StructType::create(
            context,
            { llvm::PointerType::get(context, 0), llvm::Type::getInt1Ty(context) },
            "selene.result");
      }
   };
}

// --- LLVMGenerator ---

LLVMGenerator :: LLVMGenerator()
{
   _impl = new Impl();
}

LLVMGenerator :: ~LLVMGenerator()
{
   delete (Impl*)_impl;
}

bool LLVMGenerator :: init(const TargetInfo* target, const char** errorMessage)
{
   Impl* impl = (Impl*)_impl;

   initializeTargets();

   impl->triple = target->triple;
   impl->target = target;

   std::string error;
   const llvm::Target* selected =
      llvm::TargetRegistry::lookupTarget(impl->triple, error);

   if (!selected) {
      impl->lastError = error;
      if (errorMessage) *errorMessage = impl->lastError.c_str();

      return false;
   }

   llvm::TargetOptions options;

   // PIC by default. A kernel image will want Static instead, which is why this
   // is a target property rather than a constant.
   //   docs/plan/20-os-development.md section 8
   impl->machine.reset(selected->createTargetMachine(
      llvm::Triple(impl->triple), "generic", "", options, llvm::Reloc::PIC_));

   if (!impl->machine) {
      impl->lastError = "could not create a target machine for " + impl->triple;
      if (errorMessage) *errorMessage = impl->lastError.c_str();

      return false;
   }

   impl->module = std::make_unique<llvm::Module>("selene", impl->context);
   impl->module->setTargetTriple(llvm::Triple(impl->triple));
   impl->module->setDataLayout(impl->machine->createDataLayout());

   impl->defineResultType();

   return true;
}

const char* LLVMGenerator :: targetName() const
{
   Impl* impl = (Impl*)_impl;

   return impl->triple.c_str();
}

bool LLVMGenerator :: optimize(int level, const char** errorMessage)
{
   Impl* impl = (Impl*)_impl;

   if (!impl->module) {
      impl->lastError = "the generator was not initialised";
      if (errorMessage) *errorMessage = impl->lastError.c_str();

      return false;
   }

   // The evaluation stack is emitted as one alloca per slot precisely so this
   // pass can promote them to SSA registers. Hand-building SSA would mean
   // constructing phi nodes at every branch; mem2reg does it correctly and for
   // free, and it is what makes the stack traffic the 2009 assembly avoided by
   // hand disappear entirely rather than merely be reduced.
   //   docs/plan/17-llvm-backend-and-targets.md section 4.2
   llvm::PassBuilder builder(impl->machine.get());

   llvm::LoopAnalysisManager     loops;
   llvm::FunctionAnalysisManager functions;
   llvm::CGSCCAnalysisManager    cgscc;
   llvm::ModuleAnalysisManager   modules;

   builder.registerModuleAnalyses(modules);
   builder.registerCGSCCAnalyses(cgscc);
   builder.registerFunctionAnalyses(functions);
   builder.registerLoopAnalyses(loops);
   builder.crossRegisterProxies(loops, functions, cgscc, modules);

   llvm::OptimizationLevel opt;
   switch (level) {
      case 0:  opt = llvm::OptimizationLevel::O0; break;
      case 1:  opt = llvm::OptimizationLevel::O1; break;
      case 3:  opt = llvm::OptimizationLevel::O3; break;
      default: opt = llvm::OptimizationLevel::O2; break;
   }

   llvm::ModulePassManager passes =
      (opt == llvm::OptimizationLevel::O0)
         ? builder.buildO0DefaultPipeline(opt)
         : builder.buildPerModuleDefaultPipeline(opt);

   passes.run(*impl->module, modules);

   return true;
}

bool LLVMGenerator :: emitObject(const char* path, const char** errorMessage)
{
   Impl* impl = (Impl*)_impl;

   if (!impl->machine || !impl->module) {
      impl->lastError = "the generator was not initialised";
      if (errorMessage) *errorMessage = impl->lastError.c_str();

      return false;
   }

   std::error_code code;
   llvm::raw_fd_ostream out(path, code, llvm::sys::fs::OF_None);
   if (code) {
      impl->lastError = code.message();
      if (errorMessage) *errorMessage = impl->lastError.c_str();

      return false;
   }

   llvm::legacy::PassManager passes;
   if (impl->machine->addPassesToEmitFile(passes, out, nullptr,
                                          llvm::CodeGenFileType::ObjectFile))
   {
      impl->lastError = "this target cannot emit object files";
      if (errorMessage) *errorMessage = impl->lastError.c_str();

      return false;
   }

   passes.run(*impl->module);
   out.flush();

   return true;
}


//---------------------------------------------------------------------------
// Byte code -> LLVM IR
//
// The evaluation stack becomes a fixed set of allocas, one per slot, which
// mem2reg then promotes to SSA registers. Building SSA by hand would mean
// constructing phi nodes at every branch; mem2reg does it correctly and is
// free.
//
// This is also why the byte code's allocstack/freestack metadata matters: it
// carries the depth the compiler computed, so the slot count is known without
// re-deriving it. The x86 back end discards that information because it uses
// the machine stack directly.
//   docs/plan/17-llvm-backend-and-targets.md section 4.2
//---------------------------------------------------------------------------

namespace
{
   // Maximum evaluation stack depth modelled. Exceeding it is reported, never
   // silently truncated.
   const size_t MAX_STACK = 64;

   struct Translator
   {
      Impl&                impl;
      llvm::IRBuilder<>    builder;
      llvm::Function*      function = nullptr;

      // one alloca per evaluation stack slot
      std::vector<llvm::AllocaInst*> slots;
      int                            depth = 0;

      // byte offset -> block, for the failure edges every call carries
      std::map<size_t, llvm::BasicBlock*> blocks;

      std::string error;

      Translator(Impl& i) : impl(i), builder(i.context) {}

      llvm::Type* ptrTy()  { return llvm::PointerType::get(impl.context, 0); }
      llvm::Type* i64Ty()  { return llvm::Type::getInt64Ty(impl.context); }

      // Bytes per object slot, from the TARGET -- never sizeof(void*) of the
      // host. This is the value that used to be a literal 4 everywhere.
      unsigned int slotBytes() { return impl.target->slotBytes(); }

      // --- frame ---
      //
      // Positive offsets address locals in the current frame; negative ones
      // reach into the caller's frame, which is how the single argument is
      // passed. Both become allocas so mem2reg can promote them.
      std::vector<llvm::AllocaInst*> frame;

      llvm::Value* frameSlot(int offset)
      {
         if (offset < 0)
            return function->getArg(1);              // the argument

         size_t index = (size_t)offset;
         if (index >= frame.size()) {
            imprecise = true;
            return llvm::ConstantPointerNull::get((llvm::PointerType*)ptrTy());
         }
         return builder.CreateLoad(ptrTy(), frame[index]);
      }

      void frameStore(int offset, llvm::Value* value)
      {
         if (offset < 0)
            return;                                   // caller's frame is read-only here

         size_t index = (size_t)offset;
         if (index < frame.size()) {
            builder.CreateStore(value, frame[index]);
         }
         else imprecise = true;
      }

      // --- object fields ---
      //
      // Field indices are 1-based in the byte code and are INDICES, not byte
      // offsets -- see docs/plan/17 section 3.1. The scaling happens here,
      // where the target slot size is known.
      llvm::Value* fieldAddress(llvm::Value* object, int index)
      {
         llvm::Value* offset =
            llvm::ConstantInt::get(i64Ty(), (long long)(index - 1) * slotBytes());

         return builder.CreateGEP(llvm::Type::getInt8Ty(impl.context), object, offset);
      }

      // --- runtime entry points ---
      //
      // Named symbols, never numbers. A missing one is a link error.
      //   docs/plan/19-runtime-in-c.md section 3.1
      llvm::FunctionCallee runtime(const char* name, llvm::Type* ret,
                                   std::initializer_list<llvm::Type*> args)
      {
         return impl.module->getOrInsertFunction(
            name, llvm::FunctionType::get(ret, std::vector<llvm::Type*>(args), false));
      }

      // A callable reference resolved by the linker.
      llvm::FunctionCallee procedure(unsigned int ref)
      {
         char name[32];
         snprintf(name, sizeof(name), "selene.proc.%08X", ref);

         llvm::Type* params[] = { ptrTy(), ptrTy() };

         return impl.module->getOrInsertFunction(
            name, llvm::FunctionType::get(impl.resultType, params, false));
      }
      llvm::Type* i32Ty()  { return llvm::Type::getInt32Ty(impl.context); }
      llvm::Type* i1Ty()   { return llvm::Type::getInt1Ty(impl.context); }

      // Every evaluation stack access goes through these.
      //
      // The depth counter is derived by walking the byte code, and that walk is
      // approximate for methods with several branch targets: a block can be
      // reached at a depth other than the one the linear scan predicted. Rather
      // than index out of range, an out-of-range slot yields null and is
      // recorded, so the translation is reported as imprecise instead of
      // crashing or silently emitting wrong code.
      //
      // Making it exact needs a real stack-depth analysis over the control flow
      // graph, seeded by the allocstack/freestack metadata the byte code
      // already carries.
      bool imprecise = false;

      llvm::AllocaInst* slotAt(int index)
      {
         if (index < 0 || index >= (int)slots.size()) {
            imprecise = true;
            return nullptr;
         }
         return slots[index];
      }

      void push(llvm::Value* v)
      {
         llvm::AllocaInst* s = slotAt(depth);
         if (s) builder.CreateStore(v, s);

         depth++;
      }

      llvm::Value* pop()
      {
         depth--;

         llvm::AllocaInst* s = slotAt(depth);
         if (s) return builder.CreateLoad(ptrTy(), s);

         return llvm::ConstantPointerNull::get((llvm::PointerType*)ptrTy());
      }

      llvm::Value* peek(int fromTop)
      {
         llvm::AllocaInst* s = slotAt(depth - fromTop);
         if (s) return builder.CreateLoad(ptrTy(), s);

         return llvm::ConstantPointerNull::get((llvm::PointerType*)ptrTy());
      }

      void poke(int fromTop, llvm::Value* value)
      {
         llvm::AllocaInst* s = slotAt(depth - fromTop);
         if (s) builder.CreateStore(value, s);
      }

      // A reference the linker resolves. Emitted as an external global so the
      // symbol is a real relocation rather than a baked-in address.
      llvm::Value* reference(unsigned int ref)
      {
         char name[32];
         snprintf(name, sizeof(name), "selene.ref.%08X", ref);

         llvm::GlobalVariable* g = impl.module->getGlobalVariable(name);
         if (!g) {
            g = new llvm::GlobalVariable(*impl.module, ptrTy(), true,
                                         llvm::GlobalValue::ExternalLinkage,
                                         nullptr, name);
         }
         return g;
      }

      // The runtime's dispatcher. Named, not numbered -- see
      // docs/plan/19-runtime-in-c.md section 3.1.
      llvm::FunctionCallee sendFunction()
      {
         llvm::Type* args[] = { ptrTy(), i32Ty() };

         return impl.module->getOrInsertFunction(
            "selene_send",
            llvm::FunctionType::get(impl.resultType, args, false));
      }

      llvm::Value* makeResult(llvm::Value* value, bool ok)
      {
         llvm::Value* r = llvm::UndefValue::get(impl.resultType);
         r = builder.CreateInsertValue(r, value, 0);
         r = builder.CreateInsertValue(
                r, llvm::ConstantInt::get(i1Ty(), ok ? 1 : 0), 1);

         return r;
      }

      // First pass: every call's second argument is a failure target, and those
      // are the only branch destinations in the byte code.
      void findBlocks(const unsigned char* code, size_t length)
      {
         std::set<size_t> targets;
         size_t i = 0;
         while (i < length) {
            unsigned char op = code[i];
            int args = getByteCodeArgCount(op);
            size_t argPos = i + 1;

            // A truncated final instruction must not be read past the end of
            // the section.
            if (argPos + (size_t)args * 4 > length)
               break;

            if (args == 2) {
               unsigned int t = (unsigned int)code[argPos+4] | ((unsigned int)code[argPos+5] << 8)
                              | ((unsigned int)code[argPos+6] << 16) | ((unsigned int)code[argPos+7] << 24);
               targets.insert(t);
            }
            i += 1 + args * 4;
         }
         for (size_t t : targets) {
            if (t < length) {
               char name[32];
               snprintf(name, sizeof(name), "L%u", (unsigned int)t);
               blocks[t] = llvm::BasicBlock::Create(impl.context, name, function);
            }
         }
      }


      //---------------------------------------------------------------------
      // Operand forms
      //
      // A byte code byte is COMMAND (high nibble) + OPERAND FORM (low nibble),
      // which is how the x86 JIT dispatches: _commands[code >> 4] receives
      // code & 0xF. Switching on the whole byte only covers the combinations
      // that happen to have a name in the enum, and every other valid pairing
      // looks like an unknown opcode.
      //---------------------------------------------------------------------

      llvm::Value* intConst(unsigned int v)
      {
         return builder.CreateIntToPtr(llvm::ConstantInt::get(i32Ty(), v), ptrTy());
      }

      // The value an operand denotes.
      llvm::Value* operandValue(int form, unsigned int a1, bool& known)
      {
         known = true;
         switch (form) {
            case 0x0: return peek(1);                        // top of stack
            case bcROperand:    return reference(a1);        // a reference
            case bcIOperand:    return intConst(a1);         // an integer
            case bcIOOperand:   return peek((int)a1);        // [sp:offset]
            case bcSOperand:    return function->getArg(0);  // self
            case bcRPtrOperand: return builder.CreateLoad(ptrTy(), reference(a1));
            case bcIFOperand:   return frameSlot((int)a1);   // frame slot
            case bcSPrmOperand: return function->getArg(1);  // the argument
            case bcISOperand:                                // self's field
               return builder.CreateLoad(ptrTy(),
                         fieldAddress(function->getArg(0), (int)a1));
            default:
               known = false;
               return llvm::ConstantPointerNull::get((llvm::PointerType*)ptrTy());
         }
      }

      // Store a value where an operand denotes.
      bool operandStore(int form, unsigned int a1, llvm::Value* value)
      {
         switch (form) {
            case bcIOOperand:   poke((int)a1, value); return true;
            case bcIFOperand:   frameStore((int)a1, value); return true;
            case bcRPtrOperand: builder.CreateStore(value, reference(a1)); return true;
            case bcISOperand:
               // Assigning a reference into a field is a write barrier, not a
               // plain store: the collector has to learn about the edge.
               builder.CreateCall(
                  runtime("selene_barrier", llvm::Type::getVoidTy(impl.context),
                          { ptrTy(), ptrTy() }),
                  { fieldAddress(function->getArg(0), (int)a1), value });
               return true;
            default:
               return false;
         }
      }

      // The failure edge every call carries. A branch on a flag, never an
      // unwind: failure is Selene's primary conditional.
      //   docs/plan/23-failure-abi.md
      void emitFailureEdge(llvm::Value* ok, unsigned int target)
      {
         llvm::BasicBlock* cont =
            llvm::BasicBlock::Create(impl.context, "cont", function);

         auto it = blocks.find(target);
         if (it != blocks.end()) {
            builder.CreateCondBr(ok, cont, it->second);
         }
         else builder.CreateCondBr(ok, cont, cont);

         builder.SetInsertPoint(cont);
      }

      bool translate(const char* fname, const unsigned char* code, size_t length);
   };

   bool Translator::translate(const char* fname, const unsigned char* code, size_t length)
   {
      // Selene's convention: (self, argument) -> { ptr, i1 }.
      //   docs/plan/23-failure-abi.md
      llvm::Type* params[] = { ptrTy(), ptrTy() };
      llvm::FunctionType* type =
         llvm::FunctionType::get(impl.resultType, params, false);

      function = llvm::Function::Create(type, llvm::GlobalValue::ExternalLinkage,
                                        fname, impl.module.get());

      llvm::BasicBlock* entry =
         llvm::BasicBlock::Create(impl.context, "entry", function);
      builder.SetInsertPoint(entry);

      for (size_t i = 0 ; i < MAX_STACK ; i++)
         slots.push_back(builder.CreateAlloca(ptrTy(), nullptr, "slot"));
      for (size_t i = 0 ; i < MAX_STACK ; i++)
         frame.push_back(builder.CreateAlloca(ptrTy(), nullptr, "local"));

      findBlocks(code, length);

      size_t i = 0;
      while (i < length) {
         size_t here = i;

         // start a new block if something branches here
         auto it = blocks.find(here);
         if (it != blocks.end()) {
            if (!builder.GetInsertBlock()->getTerminator())
               builder.CreateBr(it->second);

            builder.SetInsertPoint(it->second);
         }

         // Byte code keeps going after a return; IR cannot. Anything between a
         // terminator and the next branch target is unreachable, so it goes
         // into a fresh block that LLVM will drop.
         if (builder.GetInsertBlock()->getTerminator()) {
            builder.SetInsertPoint(
               llvm::BasicBlock::Create(impl.context, "unreachable", function));
         }

         unsigned char op = code[i];
         int args = getByteCodeArgCount(op);

         if (i + 1 + (size_t)args * 4 > length)
            break;                                  // truncated tail

         unsigned int a1 = 0, a2 = 0;
         if (args >= 1)
            a1 = (unsigned int)code[i+1] | ((unsigned int)code[i+2] << 8)
               | ((unsigned int)code[i+3] << 16) | ((unsigned int)code[i+4] << 24);
         if (args >= 2)
            a2 = (unsigned int)code[i+5] | ((unsigned int)code[i+6] << 8)
               | ((unsigned int)code[i+7] << 16) | ((unsigned int)code[i+8] << 24);
         i += 1 + args * 4;

         switch (op) {
            case bcNop:
               break;

            // Stack depth metadata -- carries what the compiler computed.
            case (unsigned char)bcAllocStack:
               depth += (int)a1;
               break;
            case (unsigned char)bcFreeStack:
               break;

            case bcPrep:
            case bcSPrep:
               break;                                  // frame setup is implicit in IR

            case bcRPush:
               push(reference(a1));
               break;

            case bcIPush:
               push(builder.CreateIntToPtr(
                       llvm::ConstantInt::get(i32Ty(), a1), ptrTy()));
               break;

            case bcSPush:
               push(function->getArg(0));              // self
               break;

            case bcIFPush:
               push(frameSlot((int)a1));
               break;

            case bcIFMove:
            case bcIFSet:
               frameStore((int)a1, pop());
               break;

            // --- object fields ---

            case bcISPush:
               push(builder.CreateLoad(ptrTy(),
                       fieldAddress(function->getArg(0), (int)a1)));
               break;

            case bcISMove:
            {
               llvm::Value* value = pop();
               // Assigning a reference into a field is a write barrier, not a
               // plain store: the collector has to learn about the cross
               // generation edge.
               builder.CreateCall(
                  runtime("selene_barrier", llvm::Type::getVoidTy(impl.context),
                          { ptrTy(), ptrTy() }),
                  { fieldAddress(function->getArg(0), (int)a1), value });
               break;
            }

            case bcOMovePtr:
            {
               // Store into a field of an object sitting on the evaluation
               // stack: argument1 selects the slot, argument2 the field index.
               llvm::Value* value  = pop();
               llvm::Value* object = peek((int)a1);

               builder.CreateCall(
                  runtime("selene_barrier", llvm::Type::getVoidTy(impl.context),
                          { ptrTy(), ptrTy() }),
                  { fieldAddress(object, (int)a2), value });
               break;
            }

            // --- stack-relative ---

            case bcIOPush:
               push(peek((int)a1));
               break;

            case bcIOMove:
            case bcIOSet:
               poke((int)a1, pop());
               break;

            // --- references ---

            case bcRPushPtr:
               push(builder.CreateLoad(ptrTy(), reference(a1)));
               break;

            case bcRMovePtr:
               builder.CreateStore(pop(), reference(a1));
               break;

            // --- object creation ---

            case bcOCreate:
            {
               // Size and VMT go to the runtime, which owns the object header
               // layout. Inlining that layout here would freeze it into every
               // generated program.
               llvm::Value* object = builder.CreateCall(
                  runtime("selene_create", ptrTy(), { i32Ty(), ptrTy() }),
                  { llvm::ConstantInt::get(i32Ty(), a1), reference(a2) });

               push(object);
               break;
            }

            case bcPop:
               pop();
               break;

            case bcReturn:
            case bcSReturn:
               builder.CreateRet(makeResult(pop(), true));
               break;

            case bcSExit:
               builder.CreateRet(makeResult(
                  llvm::ConstantPointerNull::get((llvm::PointerType*)ptrTy()), false));
               break;

            case bcIOCall0:
            case bcIOCall1:
            {
               llvm::Value* param    = pop();
               llvm::Value* receiver = pop();
               (void)param;

               llvm::Value* r = builder.CreateCall(
                  sendFunction(),
                  { receiver, llvm::ConstantInt::get(i32Ty(), a1) });

               llvm::Value* value = builder.CreateExtractValue(r, 0);
               llvm::Value* ok    = builder.CreateExtractValue(r, 1);

               push(value);
               emitFailureEdge(ok, a2);
               break;
            }

            // Debug markers carry no code. The .sdm debug module is emitted
            // separately, so nothing is lost by skipping them here.
            case bcDebug:
               break;

            // --- control flow ---

            case bcIJump:
            {
               auto target = blocks.find(a1);
               if (target != blocks.end()) {
                  builder.CreateBr(target->second);
               }
               else builder.CreateUnreachable();
               break;
            }

            case bcRReturnIf:
            {
               // Return the top of stack when it is the given reference.
               llvm::Value* value = pop();
               llvm::Value* same  = builder.CreateICmpEQ(value, reference(a1));

               llvm::BasicBlock* ret  = llvm::BasicBlock::Create(impl.context, "retif", function);
               llvm::BasicBlock* cont = llvm::BasicBlock::Create(impl.context, "cont", function);
               builder.CreateCondBr(same, ret, cont);

               builder.SetInsertPoint(ret);
               builder.CreateRet(makeResult(value, true));

               builder.SetInsertPoint(cont);
               push(value);
               break;
            }

            // --- calls ---

            // Inline versus call is a PERFORMANCE decision, not a semantic one.
            //
            // The 2009 compiler pasted these blobs unconditionally because it
            // had no optimizer: rcallemb was the only way to avoid call
            // overhead. Emitting a call and letting LTO's inliner decide is
            // strictly better -- it inlines under a cost model instead of
            // always, and it works on every target instead of x86.
            //
            // Translating it needs nothing special. The blobs now have names
            // (standard'i32add and the rest), so each becomes an ordinary
            // runtime function; the C implementations are what remain to be
            // written, not the translation.
            case bcRCallEmb:
            case bcRCall:
            case bcRCallExt:
            {
               llvm::Value* param    = pop();
               llvm::Value* receiver = pop();

               llvm::Value* r = builder.CreateCall(procedure(a1), { receiver, param });
               llvm::Value* value = builder.CreateExtractValue(r, 0);
               llvm::Value* ok    = builder.CreateExtractValue(r, 1);

               push(value);
               emitFailureEdge(ok, a2);
               break;
            }

            case bcIRCall0:
            case bcIRCall1:
            {
               // Static dispatch: the receiver's class is known, so the message
               // search starts from a named VMT rather than the object's own.
               llvm::Value* param    = pop();
               llvm::Value* receiver = pop();
               (void)param;

               llvm::Value* r = builder.CreateCall(
                  runtime("selene_send_static", impl.resultType,
                          { ptrTy(), i32Ty(), ptrTy() }),
                  { receiver, llvm::ConstantInt::get(i32Ty(), a1), reference(a2) });

               push(builder.CreateExtractValue(r, 0));
               emitFailureEdge(builder.CreateExtractValue(r, 1), 0);
               break;
            }

            // --- redirection ---

            case bcPrepRedir:
            case bcExitRedir:
            case bcRedirect:
            case bcRRedirect:
            {
               llvm::Value* receiver = pop();
               llvm::Value* r = builder.CreateCall(
                  runtime("selene_redirect", impl.resultType, { ptrTy() }),
                  { receiver });

               push(builder.CreateExtractValue(r, 0));
               break;
            }

            // --- roles ("shift" technology) ---
            //
            // Rewrites the live object's VMT pointer. This is why the VMT load
            // at a send site must never be marked invariant or hoisted out of a
            // loop: an object's class genuinely changes at run time.
            case bcShift:
            case bcUnShift:
            {
               llvm::Value* object = pop();
               builder.CreateCall(
                  runtime("selene_shift", llvm::Type::getVoidTy(impl.context),
                          { ptrTy(), ptrTy() }),
                  { object, (op == bcShift) ? reference(a1)
                     : llvm::ConstantPointerNull::get((llvm::PointerType*)ptrTy()) });

               push(object);
               break;
            }

            case bcSPrepParam:
               break;                                  // frame setup is implicit in IR

            default:
            {
               // Not a named combination: decompose into command family and
               // operand form, which is how the byte is actually encoded.
               int family = op >> 4;
               int form   = op & 0x0F;
               bool known = false;

               switch (family) {
                  case 0x0:                        // meta -- no code effect
                  case 0x0A:
                  case 0x0D:
                  case 0x0E:
                     break;

                  case 0x1:                        // frame prologue variants
                     break;

                  case 0x2:                        // push
                  {
                     llvm::Value* v = operandValue(form, a1, known);
                     if (!known) goto unsupported;
                     push(v);
                     break;
                  }

                  case 0x3:                        // return
                  {
                     llvm::Value* v = operandValue(form, a1, known);
                     builder.CreateRet(makeResult(known ? v : pop(), true));
                     break;
                  }

                  case 0x5:                        // pop
                     pop();
                     break;

                  case 0x6:                        // move
                  case 0x9:                        // set
                  {
                     llvm::Value* v = pop();
                     if (!operandStore(form, a1, v)) goto unsupported;
                     break;
                  }

                  case 0xB:                        // return if the operand matches
                  {
                     llvm::Value* expected = operandValue(form, a1, known);
                     if (!known) goto unsupported;

                     llvm::Value* value = peek(1);
                     llvm::Value* same  = builder.CreateICmpEQ(value, expected);

                     llvm::BasicBlock* ret =
                        llvm::BasicBlock::Create(impl.context, "retif", function);
                     llvm::BasicBlock* cont =
                        llvm::BasicBlock::Create(impl.context, "cont", function);

                     builder.CreateCondBr(same, ret, cont);
                     builder.SetInsertPoint(ret);
                     builder.CreateRet(makeResult(value, true));
                     builder.SetInsertPoint(cont);
                     break;
                  }

                  case 0x7:                        // exit
                     builder.CreateRet(makeResult(
                        llvm::ConstantPointerNull::get((llvm::PointerType*)ptrTy()),
                        false));
                     break;

                  default:
                     goto unsupported;
               }
               break;

            unsupported:
               {
                  const char* n = getByteCodeName(op);
                  char msg[128];
                  snprintf(msg, sizeof(msg),
                           "byte code '%s' (%02X: command %X, operand %X) is not translated yet",
                           n ? n : "???", op, op >> 4, op & 0x0F);
                  error = msg;

                  return false;
               }
            }
         }
      }

      // Byte code may fall off the end; Selene methods that reach it fail.
      for (llvm::BasicBlock& b : *function) {
         if (!b.getTerminator()) {
            builder.SetInsertPoint(&b);
            builder.CreateRet(makeResult(
               llvm::ConstantPointerNull::get((llvm::PointerType*)ptrTy()), false));
         }
      }

      return true;
   }
}

bool LLVMGenerator :: translateProcedure(const char* name, const unsigned char* code,
                                         size_t length, const char** errorMessage)
{
   Impl* impl = (Impl*)_impl;

   Translator translator(*impl);
   if (!translator.translate(name, code, length)) {
      // Remove the half-built function: a module carrying one would fail
      // verification for reasons unrelated to the opcode that was missing.
      if (translator.function)
         translator.function->eraseFromParent();

      impl->lastError = translator.error;
      if (errorMessage) *errorMessage = impl->lastError.c_str();

      return false;
   }
   return true;
}

bool LLVMGenerator :: verify(const char** errorMessage)
{
   Impl* impl = (Impl*)_impl;

   std::string message;
   llvm::raw_string_ostream stream(message);

   if (llvm::verifyModule(*impl->module, &stream)) {
      impl->lastError = message;
      if (errorMessage) *errorMessage = impl->lastError.c_str();

      return false;
   }
   return true;
}

bool LLVMGenerator :: emitIR(const char* path, const char** errorMessage)
{
   Impl* impl = (Impl*)_impl;

   std::error_code code;
   llvm::raw_fd_ostream out(path, code, llvm::sys::fs::OF_None);
   if (code) {
      impl->lastError = code.message();
      if (errorMessage) *errorMessage = impl->lastError.c_str();

      return false;
   }
   impl->module->print(out, nullptr);

   return true;
}

bool _ELENA_::isLLVMAvailable()
{
   return true;
}
