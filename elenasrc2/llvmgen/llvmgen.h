//---------------------------------------------------------------------------
//      E L E N A   P r o j e c t:  LLVM code generation backend
//
//      LLVM is LINKED INTO the compiler rather than driven as a subprocess.
//      There is no intermediate textual IR and no external compiler
//      invocation in the compile path: elc reads e-code and writes a .o for
//      the system linker. emitIR() exists as a debugging aid only.
//
//      This header deliberately exposes NO LLVM types: LLVMGenerator holds an
//      opaque implementation pointer, so exactly one translation unit
//      (llvmgen.cpp) includes LLVM headers and only one file rebuilds when
//      LLVM's API moves.
//
//      NAME-BASED LINKING
//      ------------------
//      Everything the generator emits is a named global in one LLVM module:
//
//         elena.<tag>.<resolved reference name>
//
//      where the ELENA apostrophe, the tag colon and '#' become dots -- the
//      GNU assembler cannot digest them unquoted ('#' even starts a comment),
//      and the C runtime spells these symbols in __asm__ labels. ELENA
//      identifiers never contain '.', so the mapping is collision-free.
//      (Round 1 spelled the prefix "selene."; the scheme is identical --
//      see experimental_version/elenasrc/llvmgen/llvmgen.h.)
//
//      Forwards ('name) are the CALLER's business: whoever asks for a symbol
//      resolves the forward first; the generator only ever sees strong names.
//---------------------------------------------------------------------------

#ifndef llvmgenH
#define llvmgenH 1

#include "targetinfo.h"

namespace _ELENA_
{

// One error buffer type for the whole generator: LLVM's diagnostics arrive
// as strings, and the driver only ever prints them.
typedef String<char, 512> GenError;

// The generator never sees a _Module: byte-code operands are module-local
// ids, and the caller resolves them to names / global message ids through
// these callbacks (forwards are resolved by the caller too).
struct TranslateCallbacks
{
   void* context;

   // module-local reference id (mask stripped) -> fully qualified name
   const char* (*referenceName)(void* context, unsigned int reference);

   // module-local constant id -> the constant's spelling (literal text,
   // number text...); constants live in their own module table
   const char* (*constantValue)(void* context, unsigned int reference);

   // module-local encoded message -> encoded message in the global id space
   unsigned int (*internMessage)(void* context, unsigned int message);
};

// Running tally over a translation batch; the driver prints it. Opcode
// coverage is measured against the REAL library corpus, not guessed.
struct TranslateStats
{
   unsigned int procedures;      // procedures attempted
   unsigned int translated;      // fully lowered
   unsigned int failed;          // aborted (unsupported opcode / unprovable stack)
   unsigned int failedOpcode[256];  // per-opcode abort counter

   TranslateStats()
   {
      procedures = translated = failed = 0;
      for (int i = 0 ; i < 256 ; i++)
         failedOpcode[i] = 0;
   }
};

class LLVMGenerator
{
   void* _impl;

public:
   // Builds the LLVM module and TargetMachine for the given target.
   // Fails (returning false) when the requested back end is not compiled in.
   bool init(const TargetInfo* target, GenError& error);

   void setCallbacks(const TranslateCallbacks& callbacks);

   // Translates one e-code procedure (serialized form, after the length
   // prefix) into an LLVM function definition named 'name' (already
   // sanitized). On failure 'error' says which opcode/offset broke and the
   // failing opcode is recorded in 'stats'.
   // 'entryMessage' is the encoded message the procedure is invoked with
   // (the VMT entry's message; 0 for symbols) -- it seeds the E register,
   // whose parameter count drives the stack effect of dispatching sends.
   bool translateProcedure(const char* name, const unsigned char* code,
      size_t length, unsigned int entryMessage, TranslateStats& stats, GenError& error);

   // Defines elena_program: the fixed entry thunk the runtime's main()
   // calls; it evaluates the given (already resolved) program symbol.
   bool emitEntry(const char* symbolName, GenError& error);

   // Turns every still-undeclared elena.* global into a loud stub: data
   // becomes a zeroed cell, functions call elena_unimplemented. The link
   // always succeeds; a missing capability reports itself at run time.
   void emitStubs();

   // Runs llvm::verifyModule; a broken module is a generator bug, never an
   // input error.
   bool verify(GenError& error);

   // New-pass-manager per-module pipeline. Not optional polish: the
   // translator emits one alloca per slot/register and relies on mem2reg.
   bool optimize(int level, GenError& error);

   // Writes the object file for the configured target.
   bool emitObject(const char* path, GenError& error);

   // Writes the target assembly. Debugging aid, same machinery as
   // emitObject with a textual output.
   bool emitAssembly(const char* path, GenError& error);

   // Writes textual IR. Debugging aid only -- never part of the compile path.
   bool emitIR(const char* path, GenError& error);

   LLVMGenerator();
   ~LLVMGenerator();
};

// Brings up a TargetMachine for every row of the target table and emits an
// empty object file, so a broken LLVM integration is reported here rather
// than in the middle of a compilation. Returns the number of failures.
int llvmSelfTest(const char* scratchDir);

} // _ELENA_

#endif // llvmgenH
