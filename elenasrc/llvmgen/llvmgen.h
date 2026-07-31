//---------------------------------------------------------------------------
//      S E L E N E   P r o j e c t:  LLVM code generation backend
//
//      Turns a compiled module's byte code into native object code, in process.
//
//      LLVM is LINKED INTO the compiler rather than driven as a subprocess.
//      There is no intermediate textual IR and no external compiler
//      invocation: elc reads byte code and writes a .o. That is what allows the
//      hand-written PE linker and the asm2binx assembler to be deleted rather
//      than replaced by a shell pipeline.
//
//      This header deliberately exposes NO LLVM types. Everything the rest of
//      the compiler sees is plain C++, so only this one translation unit needs
//      LLVM's headers and only this one file has to be rebuilt when LLVM moves.
//
//      See docs/plan/17-llvm-backend-and-targets.md
//---------------------------------------------------------------------------

#ifndef llvmgenH
#define llvmgenH 1

namespace _ELENA_
{

struct TargetInfo;

// --- LLVMGenerator ---
class LLVMGenerator
{
   void* _impl;         // opaque; defined in llvmgen.cpp

public:
   // Prepares a code generator for the given target. Returns false when the
   // target's back end is not compiled into this build of LLVM, with the reason
   // in errorMessage.
   bool init(const TargetInfo* target, const char** errorMessage);

   // Name of the LLVM target actually selected, for diagnostics.
   const char* targetName() const;

   // Translates one compiled procedure's byte code into an LLVM function.
   //
   // `code` is the byte code as it appears in a .sem section, WITHOUT the
   // leading length word. An unsupported opcode is reported by name rather
   // than skipped, so partial support can never silently emit wrong code.
   bool translateProcedure(const char* name, const unsigned char* code,
                           size_t length, const char** errorMessage);

   // Runs LLVM's standard optimization pipeline. Level 0..3.
   //
   // This is where the per-slot allocas the translator emits become SSA
   // registers, so it is not optional polish -- without it the generated code
   // carries the whole evaluation stack in memory.
   bool optimize(int level, const char** errorMessage);

   // Verifies the module built so far. Reports the first problem LLVM finds.
   bool verify(const char** errorMessage);

   // Emits the current module as an object file.
   bool emitObject(const char* path, const char** errorMessage);

   // Emits the current module as textual IR -- for inspecting what the
   // translator produced, not part of the compilation path.
   bool emitIR(const char* path, const char** errorMessage);

   LLVMGenerator();
   ~LLVMGenerator();
};

// True when this build has the LLVM back end compiled in.
bool isLLVMAvailable();

} // _ELENA_

#endif // llvmgenH
