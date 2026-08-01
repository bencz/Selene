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

// How generated symbols are named.
//
// Byte code references are MODULE-LOCAL indices; naming globals after them
// ("selene.ref.%08X") made every module invent its own vocabulary and no two
// objects could link. The generator therefore asks the caller to resolve
// each index to its full reference name ("$elena'type") and derives ONE
// symbol per artifact kind from it:
//
//   selene.sym:<name>      a symbol procedure
//   selene.native:<name>   an embedded/native procedure (C body in runtime/)
//   selene.vmt:<name>      a class dispatch table (entries; header below)
//   selene.const:<name>    the materialised constant instance of a symbol
//   selene.static:<name>   a static symbol's memoisation cell
//   selene.data:<name>     a raw data section
//   selene.lit.N/int.N/real.N   interned value constants (content-keyed)
//
// In the EMITTED symbol every apostrophe and the tag colon become dots --
// the GNU assembler cannot digest either in an unquoted name, and the C
// runtime spells these symbols in __asm__ labels compiled by gcc. ELENA
// identifiers never contain '.', so the mapping is collision-free:
//   $package'posix'writelit  ->  selene.native.$package.posix.writelit
//
// Forwards ('program'output) are the caller's business: its resolver applies
// the forward table before returning the name.
typedef const char*  (*SeleneResolveName)(void* context, unsigned int reference);
typedef const char*  (*SeleneResolveSpelling)(void* context, unsigned int reference);
typedef unsigned int (*SeleneResolveMessage)(void* context, unsigned int messageRef);

// --- LLVMGenerator ---
class LLVMGenerator
{
   void* _impl;         // opaque; defined in llvmgen.cpp

public:
   // Installs the resolvers for the module about to be translated. Must be
   // called before translateProcedure for each module: names, constant
   // spellings and message ids are all module-local.
   void setResolver(SeleneResolveName names, SeleneResolveSpelling spellings,
                    SeleneResolveMessage messages, void* context);

   // Classes that give literal/integer/real constants their VMT, from the
   // project configuration ([compiler] literalclass= and friends), plus the
   // nil symbol whose constant instance initialises static memoisation cells
   // -- a cell holding nil means "not computed yet", and 0 is not nil.
   void setValueClasses(const char* literal, const char* integer,
                        const char* real, const char* nil);
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

   // Emits a VMT as a global: the message dispatch table a class points at.
   //
   // The image is [role][flags][parent][entries] with the emitted symbol
   // pointing at the ENTRIES, because the runtime addresses the header below
   // the table pointer (vmt[-3..-1], runtime/selene.h). Entries are
   // {i32 message, ptr method}, sorted by SIGNED GLOBAL message id here,
   // ended by a terminator -- the layout the runtime's binary search expects.
   // parentName is the full public name of the parent's VMT, or NULL.
   bool emitVMT(const char* name, const char* parentName,
                unsigned int flags, const unsigned int* messages,
                const char* const* methodNames, unsigned int count,
                const char** errorMessage);

   // Materialises a constant symbol as a statically initialised object.
   //
   // The image is [header][vmt][fields] and the emitted symbol points at the
   // FIELDS, because that is where a Selene object pointer points -- the header
   // sits below it so field access needs no offset.
   bool emitConstantObject(const char* name, const char* vmtName,
                           unsigned int fieldCount, const char** errorMessage);

   // Emits a raw data section as a global.
   bool emitData(const char* name, const unsigned char* bytes,
                 size_t length, const char** errorMessage);

   // Emits `selene.program` -- the fixed entry the runtime startup calls --
   // as a thunk to the given symbol procedure. The 'program forward decides
   // the target; the runtime never needs to know a program's name.
   bool emitEntry(const char* symbolName, const char** errorMessage);

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
