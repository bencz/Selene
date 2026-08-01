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
#include <cstdarg>
#include <cstring>
#include <algorithm>

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

   // Every public symbol name funnels through here. The apostrophes of ELENA
   // reference names, the tag colon and the '#' of compiler-generated names
   // (#roleN; the method-id separator) are not digestible by the GNU
   // assembler in unquoted names -- '#' even starts a comment -- and the C
   // runtime has to spell these symbols in __asm__ labels compiled by gcc.
   // All three become dots. ELENA identifiers never contain '.', which keeps
   // the mapping collision-free in practice:
   //   $package'posix'writelit -> selene.native.$package.posix.writelit
   std::string sanitizeSymbol(const std::string& name)
   {
      std::string out = name;
      for (size_t i = 0 ; i < out.size() ; i++) {
         if (out[i] == '\'' || out[i] == ':' || out[i] == '#')
            out[i] = '.';
      }
      return out;
   }

   struct Impl
   {
      llvm::LLVMContext                    context;
      std::unique_ptr<llvm::Module>        module;
      std::unique_ptr<llvm::TargetMachine> machine;

      std::string triple;
      std::string lastError;

      const TargetInfo* target = nullptr;

      // --- module-local name resolution ---
      //
      // Installed by the driver before each module's procedures translate.
      // Byte code carries module-local indices; every emitted symbol carries
      // the RESOLVED name, which is what lets objects from different modules
      // link at all.
      SeleneResolveName     resolveNameFn     = nullptr;
      SeleneResolveSpelling resolveSpellingFn = nullptr;
      SeleneResolveMessage  resolveMessageFn  = nullptr;
      void*                 resolverContext   = nullptr;

      const char* resolveName(unsigned int naked)
      {
         return resolveNameFn ? resolveNameFn(resolverContext, naked) : nullptr;
      }

      const char* resolveSpelling(unsigned int naked)
      {
         return resolveSpellingFn ? resolveSpellingFn(resolverContext, naked) : nullptr;
      }

      unsigned int resolveMessage(unsigned int messageRef)
      {
         // Predefined messages ($new and friends) are global already.
         if (messageRef & 0x80000000u)
            return messageRef;

         return resolveMessageFn ? resolveMessageFn(resolverContext, messageRef)
                                 : messageRef;
      }

      // Classes that give value constants their VMT ([compiler] section),
      // and the nil symbol that seeds static memoisation cells.
      std::string literalClass;
      std::string integerClass;
      std::string realClass;
      std::string nilSymbol;

      // Value constants are interned by content: the same literal reached
      // from two modules is one object. Key is kind + spelling.
      std::map<std::string, std::string> internedValues;
      unsigned int                       internCounter = 0;

      // --- Selene's calling convention ---
      //
      // Every method returns {value, ok}: failure is Selene's primary
      // conditional -- #if and #loop are built on it -- so it is a branch on
      // a flag, never an unwind. PHYSICALLY the pair is ONE target word:
      // objects are slot-aligned, so bit 0 carries the flag and the value
      // lives in the bits above it. A one-word result returns in a register
      // under every C ABI -- a two-field struct did not: the Microsoft x64
      // convention returns it through a hidden pointer, which silently broke
      // every call between C and generated code on Windows.
      //
      // Must match selene_result in runtime/selene.h exactly.
      //   docs/plan/23-failure-abi.md
      llvm::Type* resultType = nullptr;

      void defineResultType()
      {
         resultType = llvm::Type::getIntNTy(context, target->slotBytes() * 8);
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

void LLVMGenerator :: setResolver(SeleneResolveName names,
                                  SeleneResolveSpelling spellings,
                                  SeleneResolveMessage messages, void* context)
{
   Impl* impl = (Impl*)_impl;

   impl->resolveNameFn     = names;
   impl->resolveSpellingFn = spellings;
   impl->resolveMessageFn  = messages;
   impl->resolverContext   = context;
}

void LLVMGenerator :: setValueClasses(const char* literal, const char* integer,
                                      const char* real, const char* nil)
{
   Impl* impl = (Impl*)_impl;

   impl->literalClass = literal ? literal : "";
   impl->integerClass = integer ? integer : "";
   impl->realClass    = real ? real : "";
   impl->nilSymbol    = nil ? nil : "";
}

bool LLVMGenerator :: emitVMT(const char* rawName, const char* rawParentName,
                              unsigned int flags, const unsigned int* messages,
                              const char* const* methodNames, unsigned int count,
                              const char** errorMessage)
{
   Impl* impl = (Impl*)_impl;

   std::string nameStorage = sanitizeSymbol(rawName);
   const char* name = nameStorage.c_str();

   std::string parentStorage =
      rawParentName ? sanitizeSymbol(rawParentName) : std::string();
   const char* parentName = rawParentName ? parentStorage.c_str() : NULL;

   llvm::Type* i32   = llvm::Type::getInt32Ty(impl->context);
   llvm::Type* ptr   = llvm::PointerType::get(impl->context, 0);
   llvm::Type* word  = impl->module->getDataLayout().getIntPtrType(impl->context);
   llvm::StructType* entry =
      llvm::StructType::get(impl->context, { i32, ptr });

   llvm::Type* method =
      llvm::FunctionType::get(impl->resultType, { ptr, ptr }, false);

   // The caller hands entries in CODE order (it sorted by offset to compute
   // method extents); the runtime's binary search needs SIGNED message
   // order. Sorting is this function's job -- the one place every table
   // passes through.
   std::vector<std::pair<int, llvm::Constant*>> sorted;
   for (unsigned int i = 0 ; i < count ; i++) {
      llvm::Constant* fn = llvm::cast<llvm::Constant>(
         impl->module->getOrInsertFunction(
            sanitizeSymbol(methodNames[i]),
            (llvm::FunctionType*)method).getCallee());

      sorted.push_back({ (int)messages[i], fn });
   }
   std::sort(sorted.begin(), sorted.end(),
             [](const std::pair<int, llvm::Constant*>& a,
                const std::pair<int, llvm::Constant*>& b) {
                return a.first < b.first;
             });

   std::vector<llvm::Constant*> entries;
   for (const std::pair<int, llvm::Constant*>& e : sorted) {
      entries.push_back(llvm::ConstantStruct::get(
         entry, { llvm::ConstantInt::get(i32, (unsigned int)e.first), e.second }));
   }

   // Terminator. The table is ended rather than counted, which is what lets a
   // VMT be walked without carrying its length.
   entries.push_back(llvm::ConstantStruct::get(
      entry, { llvm::ConstantInt::get(i32, 0x7FFFFFFFu),
               llvm::ConstantPointerNull::get((llvm::PointerType*)ptr) }));

   llvm::ArrayType* table = llvm::ArrayType::get(entry, entries.size());

   // The image is [role][flags][parent][entries], with the public symbol
   // pointing at the ENTRIES -- the runtime addresses the header BELOW the
   // pointer (vmt[-3], vmt[-2], vmt[-1], selene.h). Emitting only the table,
   // as this first did, made parent_of() read whatever global the linker
   // placed before it: dispatch then walked garbage the moment a message had
   // to be resolved through the parent chain.
   llvm::StructType* image =
      llvm::StructType::get(impl->context, { ptr, word, ptr, table });

   llvm::Constant* parent =
      llvm::ConstantPointerNull::get((llvm::PointerType*)ptr);
   if (parentName && parentName[0]) {
      llvm::GlobalValue* known =
         (llvm::GlobalValue*)impl->module->getNamedValue(parentName);
      if (!known) {
         known = new llvm::GlobalVariable(*impl->module, ptr, true,
                                          llvm::GlobalValue::ExternalLinkage,
                                          nullptr, parentName);
      }
      parent = known;
   }

   std::string storage = std::string(name) + ".image";

   // Same as emitConstantObject: the name may already exist as a DECLARATION
   // created when a send first referenced it, and that has to be replaced
   // rather than treated as "already done".
   llvm::GlobalValue* existing =
      (llvm::GlobalValue*)impl->module->getNamedValue(name);

   if (existing && !existing->isDeclaration())
      return true;

   if (existing)
      existing->setName(std::string(name) + ".decl");

   llvm::Constant* init = llvm::ConstantStruct::get(image, {
      llvm::ConstantPointerNull::get((llvm::PointerType*)ptr),   // role table
      llvm::ConstantInt::get(word, flags),
      parent,
      llvm::ConstantArray::get(table, entries)
   });

   llvm::GlobalVariable* global =
      new llvm::GlobalVariable(*impl->module, image, true,
                               llvm::GlobalValue::ExternalLinkage, init,
                               storage.c_str());

   // Three header slots: pointer, word, pointer -- exactly 3 slots on every
   // target, so the entries sit at a fixed slot offset.
   llvm::Constant* entriesAt = llvm::ConstantExpr::getGetElementPtr(
      llvm::Type::getInt8Ty(impl->context), global,
      llvm::ConstantInt::get(word, 3 * (unsigned long long)impl->target->slotBytes()));

   llvm::GlobalAlias* alias =
      llvm::GlobalAlias::create(llvm::Type::getInt8Ty(impl->context), 0,
                                llvm::GlobalValue::ExternalLinkage, name,
                                entriesAt, impl->module.get());

   if (existing) {
      existing->replaceAllUsesWith(alias);
      existing->eraseFromParent();
   }

   (void)errorMessage;
   return true;
}

bool LLVMGenerator :: emitConstantObject(const char* rawName, const char* rawVmtName,
                                         unsigned int fieldCount, const char** errorMessage)
{
   Impl* impl = (Impl*)_impl;

   std::string nameStorage = sanitizeSymbol(rawName);
   const char* name = nameStorage.c_str();

   std::string vmtStorage = rawVmtName ? sanitizeSymbol(rawVmtName) : std::string();
   const char* vmtName = rawVmtName ? vmtStorage.c_str() : NULL;

   std::string storage = std::string(name) + ".image";

   // The translator will already have created this name as an external
   // DECLARATION the first time a send referenced it. Returning early on
   // "the name exists" therefore left it forever undefined -- the declaration
   // has to be replaced by the definition, not skipped.
   llvm::GlobalValue* existing =
      (llvm::GlobalValue*)impl->module->getNamedValue(name);

   if (existing && !existing->isDeclaration())
      return true;                                  // genuinely already defined

   if (existing)
      existing->setName(std::string(name) + ".decl");

   llvm::Type* ptr  = llvm::PointerType::get(impl->context, 0);
   llvm::Type* word = impl->module->getDataLayout().getIntPtrType(impl->context);

   const unsigned int slotBytes = impl->target->slotBytes();
   llvm::ArrayType* fields = llvm::ArrayType::get(ptr, fieldCount);

   // [header][vmt][fields] -- matching selene_header() and selene_vmt_of()
   // in runtime/selene.h. Those two must never drift apart.
   llvm::StructType* image =
      llvm::StructType::get(impl->context, { word, ptr, fields });

   llvm::GlobalValue* vmt =
      (llvm::GlobalValue*)impl->module->getNamedValue(vmtName);
   if (!vmt && vmtName && vmtName[0]) {
      vmt = new llvm::GlobalVariable(*impl->module, ptr, true,
                                     llvm::GlobalValue::ExternalLinkage,
                                     nullptr, vmtName);
   }
   llvm::Constant* vmtValue = vmt
      ? (llvm::Constant*)vmt
      : llvm::ConstantPointerNull::get((llvm::PointerType*)ptr);

   std::vector<llvm::Constant*> nulls(fieldCount,
      llvm::ConstantPointerNull::get((llvm::PointerType*)ptr));

   llvm::Constant* init = llvm::ConstantStruct::get(image, {
      llvm::ConstantInt::get(word, fieldCount),      // header: size in slots
      vmtValue,
      llvm::ConstantArray::get(fields, nulls)
   });

   // WRITABLE, not read-only: #shift rewrites a live object's VMT pointer, and
   // it may be applied to a constant. In .rodata that would fault instead of
   // behaving as the language defines.
   llvm::GlobalVariable* object =
      new llvm::GlobalVariable(*impl->module, image, false,
                               llvm::GlobalValue::ExternalLinkage, init,
                               storage.c_str());

   // The public symbol points past the header, at the fields.
   llvm::Constant* fieldsAt = llvm::ConstantExpr::getGetElementPtr(
      llvm::Type::getInt8Ty(impl->context), object,
      llvm::ConstantInt::get(word, 2 * slotBytes));

   llvm::GlobalAlias* alias =
      llvm::GlobalAlias::create(llvm::Type::getInt8Ty(impl->context), 0,
                                llvm::GlobalValue::ExternalLinkage, name,
                                fieldsAt, impl->module.get());

   if (existing) {
      existing->replaceAllUsesWith(alias);
      existing->eraseFromParent();
   }

   (void)errorMessage;
   return true;
}

bool LLVMGenerator :: emitData(const char* rawName, const unsigned char* bytes,
                               size_t length, const char** errorMessage)
{
   Impl* impl = (Impl*)_impl;

   std::string nameStorage = sanitizeSymbol(rawName);
   const char* name = nameStorage.c_str();

   // Same as emitConstantObject: the name may already exist as a DECLARATION
   // created when a send first referenced it, and that has to be replaced
   // rather than treated as "already done".
   llvm::GlobalValue* existing =
      (llvm::GlobalValue*)impl->module->getNamedValue(name);

   if (existing && !existing->isDeclaration())
      return true;

   if (existing)
      existing->setName(std::string(name) + ".decl");

   std::vector<llvm::Constant*> values;
   llvm::Type* i8 = llvm::Type::getInt8Ty(impl->context);
   for (size_t i = 0 ; i < length ; i++)
      values.push_back(llvm::ConstantInt::get(i8, bytes[i]));

   llvm::ArrayType* array = llvm::ArrayType::get(i8, length);

   llvm::GlobalVariable* global =
      new llvm::GlobalVariable(*impl->module, array, true,
                               llvm::GlobalValue::ExternalLinkage,
                               llvm::ConstantArray::get(array, values), name);

   if (existing) {
      existing->replaceAllUsesWith(global);
      existing->eraseFromParent();
   }

   (void)errorMessage;
   return true;
}

bool LLVMGenerator :: emitEntry(const char* symbolName, const char** errorMessage)
{
   Impl* impl = (Impl*)_impl;

   llvm::Type* ptr = llvm::PointerType::get(impl->context, 0);
   llvm::FunctionType* type =
      llvm::FunctionType::get(impl->resultType, { ptr, ptr }, false);

   // A thunk rather than an alias: an alias must point at a definition, and
   // the program symbol may live in an object linked later.
   llvm::FunctionCallee target =
      impl->module->getOrInsertFunction(sanitizeSymbol(symbolName), type);

   llvm::Function* entry = llvm::Function::Create(
      type, llvm::GlobalValue::ExternalLinkage, "selene.program",
      impl->module.get());

   llvm::IRBuilder<> builder(
      llvm::BasicBlock::Create(impl->context, "entry", entry));

   llvm::Value* r = builder.CreateCall(
      target, { entry->getArg(0), entry->getArg(1) });
   builder.CreateRet(r);

   (void)errorMessage;
   return true;
}

bool LLVMGenerator :: emitStubs(const char* path, const char* const* provided,
                                unsigned int providedCount,
                                const char** errorMessage)
{
   Impl* impl = (Impl*)_impl;

   std::set<std::string> known;
   for (unsigned int i = 0 ; i < providedCount ; i++)
      known.insert(provided[i]);

   llvm::Module stubs("selene.stubs", impl->context);
   stubs.setTargetTriple(llvm::Triple(impl->triple));
   stubs.setDataLayout(impl->machine->createDataLayout());

   llvm::Type* ptr = llvm::PointerType::get(impl->context, 0);
   llvm::FunctionType* procType =
      llvm::FunctionType::get(impl->resultType, { ptr, ptr }, false);

   llvm::FunctionCallee report = stubs.getOrInsertFunction(
      "selene_unimplemented",
      llvm::FunctionType::get(llvm::Type::getVoidTy(impl->context),
                              { ptr }, false));

   for (llvm::Function& fn : *impl->module) {
      if (!fn.isDeclaration())
         continue;
      if (!fn.getName().starts_with("selene."))
         continue;                     // runtime C entry points are selene_*
      if (known.count(fn.getName().str()))
         continue;                     // the runtime archive has the real one
      if (fn.getFunctionType() != procType)
         continue;

      llvm::Function* stub = llvm::Function::Create(
         procType, llvm::GlobalValue::ExternalLinkage, fn.getName(), &stubs);

      llvm::IRBuilder<> builder(
         llvm::BasicBlock::Create(impl->context, "entry", stub));

      builder.CreateCall(report,
         { builder.CreateGlobalString(fn.getName(), ".name", 0, &stubs) });
      builder.CreateRet(llvm::ConstantInt::get(impl->resultType, 0));
   }

   // Undefined DATA -- the VMT or constant of a class no linked module
   // defines (the library still carries a few 2009-era orphan references).
   // The stand-in is shaped like an empty dispatch table: three null header
   // slots, then the terminator entry, public name at the entries. Sending
   // anything to it fails the message; nothing crashes.
   llvm::Type* word  = impl->module->getDataLayout().getIntPtrType(impl->context);
   llvm::Type* i32   = llvm::Type::getInt32Ty(impl->context);
   llvm::StructType* entry = llvm::StructType::get(impl->context, { i32, ptr });
   llvm::StructType* image =
      llvm::StructType::get(impl->context, { ptr, word, ptr, entry });

   for (llvm::GlobalVariable& data : impl->module->globals()) {
      if (!data.isDeclaration())
         continue;
      if (!data.getName().starts_with("selene."))
         continue;
      if (known.count(data.getName().str()))
         continue;

      llvm::Constant* init = llvm::ConstantStruct::get(image, {
         llvm::ConstantPointerNull::get((llvm::PointerType*)ptr),
         llvm::ConstantInt::get(word, 0),
         llvm::ConstantPointerNull::get((llvm::PointerType*)ptr),
         llvm::ConstantStruct::get(entry, {
            llvm::ConstantInt::get(i32, 0x7FFFFFFFu),
            llvm::ConstantPointerNull::get((llvm::PointerType*)ptr)
         })
      });

      llvm::GlobalVariable* storage = new llvm::GlobalVariable(
         stubs, image, false, llvm::GlobalValue::ExternalLinkage, init,
         data.getName() + ".missing");

      llvm::Constant* at = llvm::ConstantExpr::getGetElementPtr(
         llvm::Type::getInt8Ty(impl->context), storage,
         llvm::ConstantInt::get(word,
            3ull * (unsigned long long)impl->target->slotBytes()));

      llvm::GlobalAlias::create(llvm::Type::getInt8Ty(impl->context), 0,
                                llvm::GlobalValue::ExternalLinkage,
                                data.getName(), at, &stubs);
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

   passes.run(stubs);
   out.flush();

   return true;
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
// Three facts about the byte code, each learned by translating it wrongly
// first, shape everything below:
//
//  * Frame slots and evaluation-stack slots are the SAME storage. A local is
//    created by pushing it (newLocal: ipush 0), the message parameter is
//    pushed by the sprepparam prologue as local #1, and ifpush n addresses
//    [ebp - n*4] -- the n-th pushed slot. One array of slot allocas serves
//    both; frame slot n is slots[n-1].
//
//  * Branch and failure targets are RELATIVE to the end of their own
//    instruction (fixJumps, bccompiler.cpp), and a loop's back edge is a
//    negative offset. Reading them as absolute attached failure edges to
//    garbage and made every loop unreachable.
//
//  * The stack depth at a merge point cannot be derived by a linear walk.
//    It is computed by a worklist pass over the control flow graph before
//    any IR is emitted, seeded by what the stream itself records: ifset
//    carries the absolute unwind level of its scope, and the freestack
//    after rcallemb carries the slot count an embedded blob consumes. A
//    depth the analysis cannot prove is a hard error, never a guess.
//
//   docs/plan/17-llvm-backend-and-targets.md section 4.2
//---------------------------------------------------------------------------

namespace
{
   // Maximum stack depth modelled -- locals included, since locals are stack
   // slots. Exceeding it is reported, never silently truncated.
   const size_t MAX_STACK = 64;

   // One decoded instruction. The byte code is decoded ONCE into a vector of
   // these; block discovery, depth analysis and translation all walk the same
   // decode. Independent decoders drifting apart is how a VMT reference ends
   // up read as an opcode.
   struct Instruction
   {
      size_t        offset  = 0;    // where the opcode byte sits
      size_t        next    = 0;    // just past the instruction, extras included
      unsigned char op      = 0;
      unsigned int  a1      = 0;
      unsigned int  a2      = 0;
      unsigned int  extra   = 0;    // ircall's trailing VMT reference
      int           target  = -1;   // absolute branch/failure target, or -1
      int           depthIn = -1;   // stack depth on entry; -1 = unreachable
   };

   struct Translator
   {
      Impl&                impl;
      llvm::IRBuilder<>    builder;
      llvm::Function*      function = nullptr;

      std::vector<Instruction> instructions;
      std::map<size_t, size_t> index;          // byte offset -> instruction

      // one alloca per slot -- locals and evaluation stack alike
      std::vector<llvm::AllocaInst*> slots;
      int                            depth = 0;

      // byte offset -> block, for every branch and failure target
      std::map<size_t, llvm::BasicBlock*> blocks;

      // Which prologue opened the procedure decides what the caller's frame
      // holds. Under prep (a symbol) ifpush -1 reads the placeholder slot the
      // caller pushed, which arrives as the argument; under sprep and friends
      // (a method) it is the receiver slot -- self as the caller saw it.
      enum Prologue { plNone, plSymbol, plMethod };
      Prologue prologue = plNone;

      std::string error;
      size_t      here = 0;         // offset of the instruction being handled

      Translator(Impl& i) : impl(i), builder(i.context) {}

      llvm::Type* ptrTy()  { return llvm::PointerType::get(impl.context, 0); }
      llvm::Type* i64Ty()  { return llvm::Type::getInt64Ty(impl.context); }

      // Bytes per object slot, from the TARGET -- never sizeof(void*) of the
      // host. This is the value that used to be a literal 4 everywhere.
      unsigned int slotBytes() { return impl.target->slotBytes(); }

      llvm::Value* nullPtr()
      {
         return llvm::ConstantPointerNull::get((llvm::PointerType*)ptrTy());
      }

      // Every problem is reported with the offset it was found at; the first
      // one wins. Returning false lets call sites `return fail(...)`.
      bool fail(const char* fmt, ...)
      {
         if (error.empty()) {
            char text[160];
            va_list args;
            va_start(args, fmt);
            vsnprintf(text, sizeof(text), fmt, args);
            va_end(args);

            char message[192];
            snprintf(message, sizeof(message), "at +%u: %s",
                     (unsigned int)here, text);
            error = message;
         }
         return false;
      }

      // --- frame addressing ---
      //
      // Frame slot n and stack slot n-1 are the SAME slot, addressed two
      // ways: ifpush n reads [ebp - n*4], and the n-th push wrote it. A
      // separate locals array here meant every local read returned an
      // uninitialised alloca -- the invalid receiver selene_send kept seeing.
      //
      // Negative offsets reach the caller's frame; only -1 is ever emitted
      // (otVSelf / otSymbolParam, bccompiler.cpp:363).
      llvm::Value* frameSlot(int offset)
      {
         if (offset < 0) {
            if (offset != -1) {
               fail("caller-frame slot %d", offset);
               return nullPtr();
            }
            return (prologue == plSymbol) ? function->getArg(1)
                                          : function->getArg(0);
         }

         if (offset == 0) {
            fail("frame slot 0 is the saved frame pointer");
            return nullPtr();
         }

         llvm::AllocaInst* s = slotAt(offset - 1);
         return s ? builder.CreateLoad(ptrTy(), s) : nullPtr();
      }

      void frameStore(int offset, llvm::Value* value)
      {
         if (offset < 1) {
            fail("store to frame slot %d", offset);
            return;
         }

         llvm::AllocaInst* s = slotAt(offset - 1);
         if (s) builder.CreateStore(value, s);
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

      // A callable reference. The mask says what kind of procedure it is;
      // the resolver turns the module-local index into the full name, so the
      // same symbol links from every module that mentions it.
      llvm::FunctionCallee procedure(unsigned int ref)
      {
         unsigned int mask  = ref & 0xFF000000u;
         unsigned int naked = ref & 0x00FFFFFFu;

         const char* resolved = impl.resolveName(naked);

         std::string name;
         if (!resolved) {
            fail("procedure reference %08X does not resolve to a name", ref);
            name = "selene.unresolved";
         }
         else if (mask == (unsigned int)mskSymbolRef) {
            name = std::string("selene.sym:") + resolved;
         }
         else if (mask == (unsigned int)mskNativeCodeRef) {
            name = std::string("selene.native:") + resolved;
         }
         else {
            fail("call through unexpected reference kind %08X ('%s')",
                 ref, resolved);
            name = "selene.unresolved";
         }

         llvm::Type* params[] = { ptrTy(), ptrTy() };

         return impl.module->getOrInsertFunction(
            sanitizeSymbol(name),
            llvm::FunctionType::get(impl.resultType, params, false));
      }
      llvm::Type* i32Ty()  { return llvm::Type::getInt32Ty(impl.context); }
      llvm::Type* i1Ty()   { return llvm::Type::getInt1Ty(impl.context); }

      // Every stack access goes through these. The depth they index with is
      // assigned per instruction by the flow analysis before any IR exists,
      // so going out of range is not "imprecise" -- it is a bug in the model,
      // and it is reported as one.
      llvm::AllocaInst* slotAt(int at)
      {
         if (at < 0 || at >= (int)slots.size()) {
            fail("slot %d is outside the modelled stack", at);
            return nullptr;
         }
         return slots[at];
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
         return s ? builder.CreateLoad(ptrTy(), s) : nullPtr();
      }

      llvm::Value* peek(int fromTop)
      {
         llvm::AllocaInst* s = slotAt(depth - fromTop);
         return s ? builder.CreateLoad(ptrTy(), s) : nullPtr();
      }

      void poke(int fromTop, llvm::Value* value)
      {
         llvm::AllocaInst* s = slotAt(depth - fromTop);
         if (s) builder.CreateStore(value, s);
      }

      // An address the final link resolves: a named external unless this
      // module already defined it.
      llvm::Value* dataGlobal(const std::string& rawName)
      {
         std::string name = sanitizeSymbol(rawName);

         llvm::GlobalValue* g =
            (llvm::GlobalValue*)impl.module->getNamedValue(name);
         if (!g) {
            g = new llvm::GlobalVariable(*impl.module, ptrTy(), true,
                                         llvm::GlobalValue::ExternalLinkage,
                                         nullptr, name);
         }
         return g;
      }

      // A static symbol's memoisation cell: one writable slot, initially NIL
      // -- rreturnif compares against the nil INSTANCE, and a cell holding 0
      // would read as "already computed: the value is 0" and return failure
      // forever. linkonce so every module referencing the same symbol shares
      // the cell instead of colliding on the definition.
      llvm::Value* staticCell(const std::string& rawName)
      {
         std::string name = sanitizeSymbol(rawName);

         llvm::GlobalVariable* g = impl.module->getGlobalVariable(name, true);
         if (!g) {
            llvm::Constant* nil = impl.nilSymbol.empty()
               ? (llvm::Constant*)llvm::ConstantPointerNull::get(
                    (llvm::PointerType*)ptrTy())
               : (llvm::Constant*)dataGlobal("selene.const:" + impl.nilSymbol);

            g = new llvm::GlobalVariable(
                   *impl.module, ptrTy(), false,
                   llvm::GlobalValue::LinkOnceODRLinkage, nil, name);
         }
         return g;
      }

      // Literal, integer and real constants materialise as static objects the
      // first time something references them, interned by CONTENT: the same
      // spelling reached from two modules is one object. The image matches
      // the runtime layout -- [size|BINARY][vmt][payload] with the public
      // symbol at the payload -- and the VMT comes from the configured value
      // classes ([compiler] literalclass= and friends).
      llvm::Value* valueConstant(unsigned int mask, unsigned int naked)
      {
         const char* spelling = impl.resolveSpelling(naked);
         if (!spelling) {
            fail("constant %08X has no spelling", mask | naked);
            return nullPtr();
         }

         const char* kind; const std::string* cls;
         if (mask == (unsigned int)mskLiteralRef) {
            kind = "lit";  cls = &impl.literalClass;
         }
         else if (mask == (unsigned int)mskInt32Ref) {
            kind = "int";  cls = &impl.integerClass;
         }
         else {
            kind = "real"; cls = &impl.realClass;
         }

         std::string key = std::string(kind) + ":" + spelling;
         auto known = impl.internedValues.find(key);
         if (known != impl.internedValues.end())
            return dataGlobal(known->second);

         // payload, in the layout the 2009 loader used
         std::vector<unsigned char> payload;
         if (mask == (unsigned int)mskLiteralRef) {
            size_t length = strlen(spelling);
            payload.push_back((unsigned char)(length & 0xFF));
            payload.push_back((unsigned char)((length >> 8) & 0xFF));
            payload.push_back((unsigned char)((length >> 16) & 0xFF));
            payload.push_back((unsigned char)((length >> 24) & 0xFF));
            for (size_t i = 0 ; i <= length ; i++)          // bytes + NUL
               payload.push_back((unsigned char)spelling[i]);
         }
         else if (mask == (unsigned int)mskInt32Ref) {
            long value = strtol(spelling, nullptr, 10);
            for (int i = 0 ; i < 4 ; i++)
               payload.push_back((unsigned char)((value >> (i * 8)) & 0xFF));
         }
         else {
            double value = strtod(spelling, nullptr);
            unsigned char raw[8];
            memcpy(raw, &value, 8);
            for (int i = 0 ; i < 8 ; i++)
               payload.push_back(raw[i]);
         }

         char publicName[32];
         snprintf(publicName, sizeof(publicName), "selene.%s.%u",
                  kind, impl.internCounter++);

         llvm::Type* word =
            impl.module->getDataLayout().getIntPtrType(impl.context);
         llvm::ArrayType* bytes =
            llvm::ArrayType::get(llvm::Type::getInt8Ty(impl.context),
                                 payload.size());
         llvm::StructType* image =
            llvm::StructType::get(impl.context, { word, ptrTy(), bytes });

         llvm::Constant* vmt = cls->empty()
            ? (llvm::Constant*)llvm::ConstantPointerNull::get(
                 (llvm::PointerType*)ptrTy())
            : (llvm::Constant*)dataGlobal("selene.vmt:" + *cls);

         // header: binary flag (top bit of the target word) + payload size
         llvm::APInt header(impl.target->slotBytes() * 8,
                            (unsigned long long)payload.size());
         header.setBit(impl.target->slotBytes() * 8 - 1);

         std::vector<llvm::Constant*> content;
         for (unsigned char b : payload)
            content.push_back(llvm::ConstantInt::get(
               llvm::Type::getInt8Ty(impl.context), b));

         llvm::Constant* init = llvm::ConstantStruct::get(image, {
            llvm::ConstantInt::get(impl.context, header),
            vmt,
            llvm::ConstantArray::get(bytes, content)
         });

         llvm::GlobalVariable* storage =
            new llvm::GlobalVariable(*impl.module, image, false,
                                     llvm::GlobalValue::ExternalLinkage, init,
                                     std::string(publicName) + ".image");

         llvm::Constant* at = llvm::ConstantExpr::getGetElementPtr(
            llvm::Type::getInt8Ty(impl.context), storage,
            llvm::ConstantInt::get(word, 2ull * impl.target->slotBytes()));

         llvm::GlobalAlias* alias =
            llvm::GlobalAlias::create(llvm::Type::getInt8Ty(impl.context), 0,
                                      llvm::GlobalValue::ExternalLinkage,
                                      publicName, at, impl.module.get());
         (void)alias;

         impl.internedValues[key] = publicName;
         return dataGlobal(publicName);
      }

      // The address a data reference denotes. The mask picks the artifact
      // kind; the resolver supplies the full name.
      llvm::Value* reference(unsigned int ref)
      {
         unsigned int mask  = ref & 0xFF000000u;
         unsigned int naked = ref & 0x00FFFFFFu;

         switch (mask) {
            case (unsigned int)mskVMTRef:
            case (unsigned int)mskConstantRef:
            case (unsigned int)mskDataRef:
            case (unsigned int)mskNativeDataRef:        // a role table
            case (unsigned int)mskStaticConstRef:
            {
               const char* resolved = impl.resolveName(naked);
               if (!resolved) {
                  fail("reference %08X does not resolve to a name", ref);
                  return nullPtr();
               }

               if (mask == (unsigned int)mskStaticConstRef)
                  return staticCell(std::string("selene.static:") + resolved);

               const char* tag =
                  (mask == (unsigned int)mskVMTRef)        ? "selene.vmt:"
                : (mask == (unsigned int)mskConstantRef)   ? "selene.const:"
                : (mask == (unsigned int)mskNativeDataRef) ? "selene.roles:"
                                                           : "selene.data:";
               return dataGlobal(tag + std::string(resolved));
            }

            case (unsigned int)mskLiteralRef:
            case (unsigned int)mskInt32Ref:
            case (unsigned int)mskRealRef:
               return valueConstant(mask, naked);

            default:
               fail("reference %08X carries an unexpected mask", ref);
               return nullPtr();
         }
      }

      // The packed result word: value bits above, ok flag in bit 0 -- see
      // defineResultType. These four are the ONLY places that know the
      // packing.
      llvm::Value* makeResult(llvm::Value* value, bool ok)
      {
         llvm::Value* raw = builder.CreatePtrToInt(value, impl.resultType);
         if (ok)
            raw = builder.CreateOr(raw, llvm::ConstantInt::get(impl.resultType, 1));

         return raw;
      }

      // 0 is not a valid object reference anywhere in the system, and a
      // returned 0 IS the failure signal: the shared failure epilogue is
      // literally `ipush 0; sreturn`. The 2009 protocol carried the fact in
      // EAX and branched on test eax,eax; here the same fact sets the flag
      // bit. A translation that returned ok=true unconditionally made every
      // failure epilogue report success.
      llvm::Value* makeResultChecked(llvm::Value* value)
      {
         llvm::Value* raw = builder.CreatePtrToInt(value, impl.resultType);
         llvm::Value* ok  = builder.CreateICmpNE(
            raw, llvm::ConstantInt::get(impl.resultType, 0));

         return builder.CreateOr(
            raw, builder.CreateZExt(ok, impl.resultType));
      }

      llvm::Value* resultValue(llvm::Value* raw)
      {
         llvm::Value* bits = builder.CreateAnd(
            raw, llvm::ConstantInt::get(impl.resultType, ~1ull));

         return builder.CreateIntToPtr(bits, ptrTy());
      }

      llvm::Value* resultOk(llvm::Value* raw)
      {
         llvm::Value* bit = builder.CreateAnd(
            raw, llvm::ConstantInt::get(impl.resultType, 1));

         return builder.CreateICmpNE(
            bit, llvm::ConstantInt::get(impl.resultType, 0));
      }

      static unsigned int read32(const unsigned char* p, size_t at)
      {
         return (unsigned int)p[at] | ((unsigned int)p[at + 1] << 8)
              | ((unsigned int)p[at + 2] << 16) | ((unsigned int)p[at + 3] << 24);
      }

      // Family 2 is push; ocreate also pushes what it allocated.
      static bool isPush(unsigned char op)
      {
         return (op >> 4) == 0x2 || op == bcOCreate;
      }

      // Family 3 (return) and family 7 (exit) end the procedure.
      static bool isTerminator(unsigned char op)
      {
         return (op >> 4) == 0x3 || (op >> 4) == 0x7;
      }

      // The slot count an embedded blob consumes is recorded ONLY by the
      // freestack that directly follows rcallemb (callEmbedded,
      // bccompiler.cpp:468). The freestack after other calls restates an
      // effect modelled below and is ignored.
      int embParamCount(size_t at) const
      {
         if (at + 1 < instructions.size()
            && instructions[at + 1].op == (unsigned char)bcFreeStack)
         {
            return (int)instructions[at + 1].a1;
         }
         return 0;
      }

      // Net stack effect, for the depth analysis. Every case here has a
      // matching implementation in the translation switch; the two moving
      // together is what the analysis verifies.
      int stackDelta(size_t at) const
      {
         const Instruction& ins = instructions[at];

         if (isPush(ins.op))
            return 1;

         switch (ins.op) {
            case bcSPrepParam:               // the parameter becomes local #1
            case bcPrepRedir:
               return 1;
            case bcIOCall0:                  // pop param and receiver, push
            case bcIOCall1:                  // the result
            case bcIRCall0:
            case bcIRCall1:
               return -1;
            case bcRCallEmb:
               return -embParamCount(at);
            default:
               if ((ins.op >> 4) == 0x5)     // pop
                  return -1;
               return 0;                     // moves, meta, rcall, redirect
         }
      }

      // Decode the whole procedure up front. Targets are relative to the END
      // of their instruction -- fixJumps patches `label - placeholder - 4`
      // (8 with ircall's extra dword) and loops jump backwards.
      bool decode(const unsigned char* code, size_t length)
      {
         size_t i = 0;
         while (i < length) {
            Instruction ins;
            ins.offset = i;
            ins.op     = code[i];
            here       = i;

            int args = getByteCodeArgCount(ins.op);
            if (i + 1 + (size_t)args * 4 > length)
               return fail("truncated instruction");

            if (args >= 1) ins.a1 = read32(code, i + 1);
            if (args >= 2) ins.a2 = read32(code, i + 5);
            i += 1 + (size_t)args * 4;

            // Static dispatch takes THREE operands -- message, failure target
            // and the VMT to search -- but the encoding carries only two. Any
            // decoder that stops at two is four bytes out of step from here
            // on; that is what made a VMT reference look like an opcode.
            if (ins.op == bcIRCall0 || ins.op == bcIRCall1) {
               if (i + 4 > length)
                  return fail("ircall is missing its VMT operand");

               ins.extra = read32(code, i);
               i += 4;
            }
            ins.next = i;

            bool hasTarget = true;
            long target = 0;
            switch (ins.op) {
               case bcRCall: case bcRCallExt: case bcRCallEmb:
               case bcIOCall0: case bcIOCall1:
               case bcIRCall0: case bcIRCall1:
                  target = (long)ins.next + (long)(int)ins.a2;
                  break;
               case bcIJump:
                  target = (long)ins.next + (long)(int)ins.a1;
                  break;
               default:
                  hasTarget = false;
            }
            if (hasTarget) {
               if (target < 0 || (size_t)target >= length)
                  return fail("branch target %+ld is outside the procedure",
                              target - (long)ins.next);
               ins.target = (int)target;
            }

            index[ins.offset] = instructions.size();
            instructions.push_back(ins);
         }

         // A target inside another instruction means this decode and the
         // writer have drifted apart -- the same class of defect as an
         // unknown opcode, and worth the same loud failure.
         for (const Instruction& ins : instructions) {
            if (ins.target >= 0 && index.find((size_t)ins.target) == index.end()) {
               here = ins.offset;
               return fail("branch target +%d is inside another instruction",
                           ins.target);
            }
         }
         return true;
      }

      // Can this code run correctly at ANY entry depth?
      //
      // The byte code genuinely merges paths at different depths and keeps
      // going: the shared failure epilogue (ipush 0; sreturn), and whole
      // method tails after a branch, where the deeper path simply carries a
      // dead leftover slot. On x86 this is free -- everything is esp-relative.
      // For the SSA translation it is safe exactly when the code from here
      // never USES a value from a slot it did not itself write: reads must
      // stay inside the suffix of slots produced after the merge, with pop's
      // discarded read exempt. Frame reads are always fine -- locals sit at
      // fixed absolute slots, the same for every path.
      //
      // `valid` tracks that suffix. A use deeper than it means the merged
      // translation would read a slot whose value depends on which path ran,
      // and THAT is a hard error.
      bool toleratesAnyDepth(size_t at) const
      {
         int valid = 0;              // top slots this code wrote itself
         for (int budget = 2048 ; budget > 0 && at < instructions.size() ; budget--) {
            const Instruction& ins = instructions[at];
            unsigned char op = ins.op;
            int family = op >> 4;
            at++;

            // metadata
            if (op == bcNop || family == 0x0 || op == bcDebug)
               continue;

            // the unwinder: depth is absolute again from here on
            if (op == bcIFSet)
               return true;

            switch (op) {
               case bcIJump:                              // follow the jump
               {
                  auto it = index.find((size_t)ins.target);
                  if (it == index.end())
                     return false;
                  at = it->second;
                  continue;
               }

               case bcIOPush:                             // dup [sp + n*slot]
                  if (valid < (int)ins.a1 + 1)
                     return false;
                  valid++;
                  continue;

               case bcIOMove:                             // S[top-n] := S[top]
                  if (valid < (int)ins.a1 + 1)
                     return false;
                  continue;

               case bcOMovePtr:                           // uses S[top-n]
                  if (valid < (int)ins.a1 + 1)
                     return false;
                  continue;

               case bcIFMove: case bcRMovePtr:
               case bcISMove: case bcIOSet:               // use the top
               case bcRReturnIf:
               case bcRedirect: case bcRRedirect:
                  if (valid < 1)
                     return false;
                  continue;

               case bcIOCall0: case bcIOCall1:            // pop 2, push result
               case bcIRCall0: case bcIRCall1:
                  if (valid < 2)
                     return false;
                  valid--;
                  continue;

               case bcRCall:                              // replace the top
                  if (valid < 1)
                     return false;
                  continue;

               case bcRCallExt:                           // reads the top two
                  if (valid < 2)
                     return false;
                  continue;

               case bcRCallEmb:
               {
                  int params = embParamCount(at - 1);
                  if (valid < params + 1)
                     return false;
                  valid -= params;
                  continue;
               }

               case bcShift: case bcUnShift: case bcIOSwap:
                  continue;

               case bcSExit: case bcExitRedir:
                  return true;
            }

            if (isPush(op)) {                             // plain pushes
               valid++;
               continue;
            }
            if (family == 0x5) {                          // pop discards its
               if (valid > 0) valid--;                    // read
               continue;
            }
            if (family == 0x3)                            // return uses the top
               return valid >= 1;
            if (family == 0x7)
               return true;

            return false;                                 // anything else:
         }                                                // require exactness
         return false;
      }

      // Assign every reachable instruction its entry depth by propagating
      // over the flow graph -- fall-through, jumps, and the failure edge
      // every call carries. ifset is the absolute reset the byte code was
      // built around: after a failed branch the stack is at an arbitrary
      // depth, and ifset snaps it to its scope's level.
      bool analyze()
      {
         if (instructions.empty())
            return true;

         std::vector<std::pair<size_t, int>> work;
         work.push_back({ 0, 0 });

         while (!work.empty()) {
            size_t at = work.back().first;
            int    d  = work.back().second;
            work.pop_back();

            Instruction& ins = instructions[at];
            here = ins.offset;

            if (ins.depthIn >= 0) {
               if (ins.depthIn == d)
                  continue;
               if (toleratesAnyDepth(at))
                  continue;
               return fail("reached at depth %d and at depth %d",
                           ins.depthIn, d);
            }
            ins.depthIn = d;

            int out = (ins.op == bcIFSet) ? (int)ins.a1 : d + stackDelta(at);
            if (out < 0)
               return fail("stack underflow");
            if (out > (int)MAX_STACK)
               return fail("deeper than the %u modelled slots",
                           (unsigned int)MAX_STACK);

            if (ins.target >= 0)
               work.push_back({ index.at((size_t)ins.target), out });

            if (ins.op != bcIJump && !isTerminator(ins.op)
               && at + 1 < instructions.size())
            {
               work.push_back({ at + 1, out });
            }
         }
         return true;
      }

      // Every branch and failure target opens a block. Targets come from the
      // decoded vector, so this cannot disagree with the translation loop.
      void findBlocks()
      {
         for (const Instruction& ins : instructions) {
            if (ins.target < 0)
               continue;

            size_t at = (size_t)ins.target;
            if (blocks.find(at) == blocks.end()) {
               char name[32];
               snprintf(name, sizeof(name), "L%u", (unsigned int)at);
               blocks[at] = llvm::BasicBlock::Create(impl.context, name, function);
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
            case bcIOOperand:   return peek((int)a1 + 1);    // [sp + n*slot]
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

      // Store a value where an operand denotes. Every x86 move template
      // reads [esp] WITHOUT popping, so callers pass peek(1) and the stack
      // depth is untouched.
      bool operandStore(int form, unsigned int a1, llvm::Value* value)
      {
         switch (form) {
            case bcIOOperand:   poke((int)a1 + 1, value); return true;
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
      void emitFailureEdge(llvm::Value* ok, int target)
      {
         llvm::BasicBlock* cont =
            llvm::BasicBlock::Create(impl.context, "cont", function);

         auto it = blocks.find((size_t)target);
         if (it != blocks.end()) {
            builder.CreateCondBr(ok, cont, it->second);
         }
         else {
            // findBlocks opened a block for every decoded target, so this is
            // an internal inconsistency, not bad input.
            fail("failure edge to +%d has no block", target);
            builder.CreateBr(cont);
         }

         builder.SetInsertPoint(cont);
      }

      bool translate(const char* fname, const unsigned char* code, size_t length);
   };

   bool Translator::translate(const char* fname, const unsigned char* code, size_t length)
   {
      if (!decode(code, length))
         return false;

      if (!analyze())
         return false;

      // Selene's convention: (self, argument) -> { ptr, i1 }.
      //   docs/plan/23-failure-abi.md
      llvm::Type* params[] = { ptrTy(), ptrTy() };
      llvm::FunctionType* type =
         llvm::FunctionType::get(impl.resultType, params, false);

      // A module translated earlier may already have referenced this name,
      // leaving a DECLARATION behind; the definition has to fill that very
      // function. Creating a fresh one made LLVM rename it (name.5) and the
      // original stayed forever undefined.
      llvm::FunctionCallee callee =
         impl.module->getOrInsertFunction(sanitizeSymbol(fname), type);
      function = llvm::dyn_cast<llvm::Function>(callee.getCallee());
      if (!function) {
         here = 0;
         return fail("the name '%s' is already bound to something that is "
                     "not a procedure", fname);
      }
      if (!function->isDeclaration()) {
         here = 0;
         function = nullptr;                 // do not let the error path
         return fail("'%s' is defined twice", fname);   // erase the original
      }

      llvm::BasicBlock* entry =
         llvm::BasicBlock::Create(impl.context, "entry", function);
      builder.SetInsertPoint(entry);

      for (size_t i = 0 ; i < MAX_STACK ; i++)
         slots.push_back(builder.CreateAlloca(ptrTy(), nullptr, "slot"));

      findBlocks();

      for (size_t at = 0 ; at < instructions.size() ; at++) {
         const Instruction& ins = instructions[at];

         // start a new block if something branches here
         auto blockHere = blocks.find(ins.offset);
         if (blockHere != blocks.end()) {
            if (!builder.GetInsertBlock()->getTerminator())
               builder.CreateBr(blockHere->second);

            builder.SetInsertPoint(blockHere->second);
         }

         // Only code the depth analysis reached is translated. What it did
         // not reach cannot execute, and translating it against a made-up
         // depth would leak wrong values into blocks that can.
         if (ins.depthIn < 0)
            continue;

         if (builder.GetInsertBlock()->getTerminator()) {
            here = ins.offset;
            return fail("reachable code directly after a terminator");
         }

         here  = ins.offset;
         depth = ins.depthIn;

         unsigned char op = ins.op;
         unsigned int  a1 = ins.a1, a2 = ins.a2;

         switch (op) {
            case bcNop:
            case (unsigned char)bcAllocStack:      // stack metadata; the depth
            case (unsigned char)bcFreeStack:       // analysis already used it
            case bcDebug:
               break;

            // --- prologues ---
            //
            // Frame setup is implicit in IR; what a prologue decides is how
            // the caller's frame is addressed (frameSlot) and whether the
            // message parameter is materialised as local #1.
            case bcPrep:
               prologue = plSymbol;
               break;

            case bcSPrep:
               prologue = plMethod;
               break;

            case bcSPrepParam:
            case bcPrepRedir:
               prologue = plMethod;
               push(function->getArg(1));          // the parameter, local #1
               break;

            // --- pushes ---

            case bcRPush:
               push(reference(a1));
               break;

            case bcIPush:
               push(builder.CreateIntToPtr(
                       llvm::ConstantInt::get(i32Ty(), a1), ptrTy()));
               break;

            case bcSPush:
               push(function->getArg(0));          // self
               break;

            case bcIFPush:
               push(frameSlot((int)a1));
               break;

            // --- moves: every x86 template reads [esp] without popping ---

            case bcIFMove:
               frameStore((int)a1, peek(1));
               break;

            case bcIFSet:
               // The scope unwinder: the depth analysis consumed its absolute
               // level; at run time it moves no data.
               break;

            // --- object fields ---

            case bcISPush:
               push(builder.CreateLoad(ptrTy(),
                       fieldAddress(function->getArg(0), (int)a1)));
               break;

            case bcISMove:
               // Assigning a reference into a field is a write barrier, not a
               // plain store: the collector has to learn about the cross
               // generation edge.
               builder.CreateCall(
                  runtime("selene_barrier", llvm::Type::getVoidTy(impl.context),
                          { ptrTy(), ptrTy() }),
                  { fieldAddress(function->getArg(0), (int)a1), peek(1) });
               break;

            case bcOMovePtr:
            {
               // Field of the object argument1 slots down := top. argument2
               // is a BYTE offset in the 32-bit layout -- the front end
               // shifted it, unlike every other field operand -- so it is
               // rescaled to the target's slot size here.
               llvm::Value* value  = peek(1);
               llvm::Value* object = peek((int)a1 + 1);

               llvm::Value* slot = builder.CreateGEP(
                  llvm::Type::getInt8Ty(impl.context), object,
                  llvm::ConstantInt::get(i64Ty(), (long long)(a2 / 4) * slotBytes()));

               builder.CreateCall(
                  runtime("selene_barrier", llvm::Type::getVoidTy(impl.context),
                          { ptrTy(), ptrTy() }),
                  { slot, value });
               break;
            }

            case bcIOSet:
            {
               // Stamp a class VMT into a field of the object on top -- how a
               // $typeinstance learns its class. Byte offset, same rescale as
               // omoveptr; the VMT mask is applied by the reader, exactly as
               // the x86 JIT did (scope.argument2 |= mskVMTRef).
               llvm::Value* object = peek(1);

               llvm::Value* slot = builder.CreateGEP(
                  llvm::Type::getInt8Ty(impl.context), object,
                  llvm::ConstantInt::get(i64Ty(), (long long)(a1 / 4) * slotBytes()));

               builder.CreateStore(reference(a2 | (unsigned int)mskVMTRef), slot);
               break;
            }

            // --- stack-relative ---

            case bcIOPush:
               push(peek((int)a1 + 1));            // push [sp + n*slot]
               break;

            case bcIOMove:
               poke((int)a1 + 1, peek(1));         // S[top-n] := S[top]
               break;

            case bcIOSwap:
               // Faithfully a no-op: the 2009 template reads two slots and
               // writes each back where it came from. See doc 03 sec. 2.5.39.
               break;

            // --- references ---

            case bcRPushPtr:
               push(builder.CreateLoad(ptrTy(), reference(a1)));
               break;

            case bcRMovePtr:
               builder.CreateStore(peek(1), reference(a1));
               break;

            // --- object creation ---

            case bcOCreate:
            {
               // Size and VMT go to the runtime, which owns the object header
               // layout. Inlining that layout here would freeze it into every
               // generated program.
               // The VMT operand arrives WITHOUT its mask: the x86 JIT applies
               // mskVMTRef itself (scope.argument2 |= mskVMTRef). A back end
               // that takes the operand at face value references a symbol
               // nothing defines.
               llvm::Value* object = builder.CreateCall(
                  runtime("selene_create", ptrTy(), { i32Ty(), ptrTy() }),
                  { llvm::ConstantInt::get(i32Ty(), a1),
                    reference(a2 | (unsigned int)mskVMTRef) });

               push(object);
               break;
            }

            case bcPop:
               pop();
               break;

            // --- returns ---
            //
            // 0 is the failure signal: the shared failure epilogue is
            // `ipush 0; sreturn`. The success flag is therefore COMPUTED from
            // the value, exactly as the 2009 caller's test eax,eax did.
            case bcReturn:
            case bcSReturn:
               builder.CreateRet(makeResultChecked(pop()));
               break;

            case bcSExit:
               // Epilogue without an explicit result: the receiver, which is
               // never 0 -- so this is the SUCCESS epilogue, not a failure.
               builder.CreateRet(makeResult(function->getArg(0), true));
               break;

            case bcExitRedir:
               // Redirect-method epilogue: reached when no redirect matched,
               // and always a failure.
               builder.CreateRet(makeResult(nullPtr(), false));
               break;

            // --- sends ---

            case bcIOCall0:
            case bcIOCall1:
            {
               // Pop the parameter and the receiver; the result takes the
               // receiver's slot. The parameter TRAVELS WITH THE SEND --
               // dropping it here is how every method received null.
               llvm::Value* param    = pop();
               llvm::Value* receiver = pop();

               llvm::Value* r = builder.CreateCall(
                  runtime("selene_send", impl.resultType,
                          { ptrTy(), ptrTy(), i32Ty() }),
                  { receiver, param,
                    llvm::ConstantInt::get(i32Ty(), impl.resolveMessage(a1)) });

               push(resultValue(r));
               emitFailureEdge(resultOk(r), ins.target);
               break;
            }

            case bcIRCall0:
            case bcIRCall1:
            {
               llvm::Value* param    = pop();
               llvm::Value* receiver = pop();

               llvm::Value* r = builder.CreateCall(
                  runtime("selene_send_static", impl.resultType,
                          { ptrTy(), ptrTy(), i32Ty(), ptrTy() }),
                  { receiver, param,
                    llvm::ConstantInt::get(i32Ty(), impl.resolveMessage(a1)),
                    reference(ins.extra) });

               push(resultValue(r));
               emitFailureEdge(resultOk(r), ins.target);
               break;
            }

            // --- control flow ---

            case bcIJump:
            {
               auto target = blocks.find((size_t)ins.target);
               if (target == blocks.end()) {
                  here = ins.offset;
                  return fail("jump to +%d has no block", ins.target);
               }
               builder.CreateBr(target->second);
               break;
            }

            case bcRReturnIf:
            {
               // Return the top of stack when it is NO LONGER the given
               // reference; fall through, top intact, when it still is.
               // Static-symbol memoisation: "the cache is not nil any more,
               // return it". (The 2009 opcode table describes the reverse;
               // the asm template elena'10 is the authority.)
               llvm::Value* value = peek(1);
               llvm::Value* same  = builder.CreateICmpEQ(value, reference(a1));

               llvm::BasicBlock* ret  = llvm::BasicBlock::Create(impl.context, "retif", function);
               llvm::BasicBlock* cont = llvm::BasicBlock::Create(impl.context, "cont", function);
               builder.CreateCondBr(same, cont, ret);

               builder.SetInsertPoint(ret);
               builder.CreateRet(makeResultChecked(value));

               builder.SetInsertPoint(cont);
               break;
            }

            // --- procedure calls ---

            case bcRCall:
            {
               // The caller pushed a placeholder (pushSymbol: rpush nil), or
               // feeds the current top to a property symbol. Either way the
               // callee reads that slot through ifpush -1 and the result
               // REPLACES it -- the symbol epilogue writes the caller's top
               // slot: net stack effect zero, unlike a send.
               llvm::Value* argument = peek(1);

               llvm::Value* r = builder.CreateCall(procedure(a1),
                                                   { nullPtr(), argument });

               poke(1, resultValue(r));
               emitFailureEdge(resultOk(r), ins.target);
               break;
            }

            case bcRCallExt:
            {
               // #external f(a, b): the arguments were PUSHED (a below, b on
               // top), the native leaves the stack alone -- its result is the
               // success flag, and out-parameters are written through the
               // argument objects. The byte code pops the arguments
               // afterwards. Until the typed FFI lands, a native receives the
               // top two slots: (first-pushed, last-pushed).
               llvm::Value* second = peek(1);
               llvm::Value* first  = depth >= 2 ? peek(2) : nullPtr();

               llvm::Value* r = builder.CreateCall(procedure(a1),
                                                   { first, second });

               emitFailureEdge(resultOk(r), ins.target);
               break;
            }

            case bcRCallEmb:
            {
               // Inline versus call is a PERFORMANCE decision, not a semantic
               // one. The 2009 compiler pasted these blobs unconditionally
               // because it had no optimizer; emitting a call and letting
               // LTO's inliner decide is strictly better. The blobs have
               // names (standard'i32add and the rest); their C bodies are
               // what remain to be written, not the translation.
               //
               // The blob consumes `params` slots -- the freestack after it
               // is the only record of that -- and its result replaces the
               // slot below them.
               int params = embParamCount(at);

               llvm::Value* receiver = peek(params + 1);
               llvm::Value* argument = (params >= 1) ? peek(params) : nullPtr();
               // params > 1 drops all but the first argument at this ABI;
               // the named typed intrinsics give blobs real signatures.

               llvm::Value* r = builder.CreateCall(procedure(a1),
                                                   { receiver, argument });

               depth -= params;
               poke(1, resultValue(r));
               emitFailureEdge(resultOk(r), ins.target);
               break;
            }

            // --- redirection ---

            case bcRedirect:
            case bcRRedirect:
            {
               // Re-send the in-flight message to the object on top. On
               // success the enclosing METHOD returns with that result; on
               // failure control falls through with the stack untouched.
               llvm::Value* target = peek(1);

               llvm::Value* r = (op == bcRedirect)
                  ? builder.CreateCall(
                       runtime("selene_redirect", impl.resultType,
                               { ptrTy(), ptrTy() }),
                       { target, function->getArg(1) })
                  : builder.CreateCall(
                       runtime("selene_redirect_super", impl.resultType,
                               { ptrTy(), ptrTy(), ptrTy() }),
                       { target, function->getArg(1), reference(a1) });

               llvm::BasicBlock* done =
                  llvm::BasicBlock::Create(impl.context, "redirected", function);
               llvm::BasicBlock* cont =
                  llvm::BasicBlock::Create(impl.context, "cont", function);

               builder.CreateCondBr(resultOk(r), done, cont);

               builder.SetInsertPoint(done);
               builder.CreateRet(r);

               builder.SetInsertPoint(cont);
               break;
            }

            // --- roles ("shift" technology) ---
            //
            // Rewrites SELF's VMT pointer in place: role `argument1` from the
            // class's role table goes in, unshift puts the owner class back.
            // This is why the VMT load at a send site must never be marked
            // invariant or hoisted out of a loop: an object's class genuinely
            // changes at run time.
            case bcShift:
               builder.CreateCall(
                  runtime("selene_shift", llvm::Type::getVoidTy(impl.context),
                          { ptrTy(), i32Ty() }),
                  { function->getArg(0), llvm::ConstantInt::get(i32Ty(), a1) });
               break;

            case bcUnShift:
               builder.CreateCall(
                  runtime("selene_unshift", llvm::Type::getVoidTy(impl.context),
                          { ptrTy() }),
                  { function->getArg(0) });
               break;

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
                     prologue = plMethod;
                     break;

                  case 0x2:                        // push
                  {
                     llvm::Value* v = operandValue(form, a1, known);
                     if (!known) goto unsupported;
                     push(v);
                     break;
                  }

                  case 0x3:                        // return; ok = value != 0
                  {
                     llvm::Value* v = operandValue(form, a1, known);
                     builder.CreateRet(makeResultChecked(known ? v : pop()));
                     break;
                  }

                  case 0x5:                        // pop
                     pop();
                     break;

                  case 0x6:                        // move
                  case 0x9:                        // set
                  {
                     // reads the top WITHOUT popping, like every x86 move
                     if (!operandStore(form, a1, peek(1))) goto unsupported;
                     break;
                  }

                  case 0xB:                        // return UNLESS it matches
                  {
                     llvm::Value* expected = operandValue(form, a1, known);
                     if (!known) goto unsupported;

                     llvm::Value* value = peek(1);
                     llvm::Value* same  = builder.CreateICmpEQ(value, expected);

                     llvm::BasicBlock* ret =
                        llvm::BasicBlock::Create(impl.context, "retif", function);
                     llvm::BasicBlock* cont =
                        llvm::BasicBlock::Create(impl.context, "cont", function);

                     builder.CreateCondBr(same, cont, ret);
                     builder.SetInsertPoint(ret);
                     builder.CreateRet(makeResultChecked(value));
                     builder.SetInsertPoint(cont);
                     break;
                  }

                  case 0x7:                        // exit
                     builder.CreateRet(makeResult(nullPtr(), false));
                     break;

                  default:
                     goto unsupported;
               }
               break;

            unsupported:
               {
                  const char* n = getByteCodeName(op);
                  char msg[128];
                  // Show the surrounding bytes: an unknown opcode is far more
                  // often a decode that drifted than a genuinely new command,
                  // and the context is what distinguishes the two.
                  char context[220];
                  size_t from = (here >= 40) ? here - 40 : 0;
                  size_t p = 0;
                  for (size_t b = from ; b < length && b < here + 12 && p < sizeof(context) - 8 ; b++) {
                     p += snprintf(context + p, sizeof(context) - p,
                                   (b == here) ? "[%02X]" : "%02X ", code[b]);
                  }

                  snprintf(msg, sizeof(msg),
                           "'%s' (%02X: cmd %X, opr %X) at +%u/%u  ... %s",
                           n ? n : "???", op, op >> 4, op & 0x0F,
                           (unsigned int)here, (unsigned int)length, context);
                  error = msg;

                  return false;
               }
            }
         }

         // slotAt and friends report by setting `error`; stop at the first.
         if (!error.empty())
            return false;
      }

      // Unreachable branch targets and byte code that falls off the end both
      // leave open blocks; a Selene method that reaches one fails the message.
      for (llvm::BasicBlock& b : *function) {
         if (!b.getTerminator()) {
            builder.SetInsertPoint(&b);
            builder.CreateRet(makeResult(nullPtr(), false));
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
      // Drop the half-built body: a module carrying one would fail
      // verification for reasons unrelated to the opcode that was missing.
      // The function itself stays as a declaration -- earlier call sites may
      // already point at it, and an undefined symbol is the honest outcome.
      if (translator.function)
         translator.function->deleteBody();

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
