//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Compiler Engine
//
//		This file contains ELENA JIT linker class.
//		Supported platforms: x86
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef x86jitcompilerH
#define x86jitcompilerH 1

#include "jitcompiler.h"
#include "x86helper.h"

namespace _ELENA_
{

class x86JITCompiler;

// --- x86JITScope ---

struct x86JITScope
{
   // byte code command arguments
   int            argument1;
   int            argument2;

   size_t         prevFSPOffs; // offset to the previous stack frame

   x86LabelHelper lh;

   _ReferenceHelper* helper;
   StreamReader*     tape;
   SectionWriter*    code;
   x86JITCompiler*   compiler;

   x86JITScope(StreamReader* tape, SectionWriter* code, _ReferenceHelper* helper, x86JITCompiler* compiler)
      : lh(code)
   {
      this->tape = tape;
      this->code = code;
      this->prevFSPOffs = 0;
      this->helper = helper;
      this->compiler = compiler;
   }
};

// --- x86JITInlineCode ---

struct x86JITInlineCode
{
   Section* section;
   void*    code;
   int      length;
};

// --- x86JITCompiler ---

class x86JITCompiler : public _JITCompiler
{
protected:
   enum Command
   {
      cmdPrepare     = 0,
      cmdSPrepare    = 1,
      cmdReturn      = 2,
      cmdIOCallN     = 3,
      cmdSExit       = 4,
      cmdSReturn     = 5,
      cmdRReturnIf   = 6,
      cmdOCreate     = 7,
      cmdOCreate2    = 8,
      cmdOCreate4    = 9,
      cmdOCreate6    = 10,
      cmdOCreate0    = 11,
      cmdCallExt     = 12,
      cmdPrepRedir   = 13,
      cmdExitRedir   = 14,
      cmdRedirect    = 15,
      cmdRRedirect   = 16,
      cmdIOSWAP      = 17,
      cmdIOSet       = 18,
      cmdShift       = 19,
      cmdUnShift     = 20,
      cmdIRCallN     = 21
   };

   friend inline void copySection(x86JITScope& scope, x86JITInlineCode& code, _Module* module);

   // commands
   friend void compileNop(int opcode, x86JITScope& scope);
   friend void compilePrep(int opcode, x86JITScope& scope);
   friend void compilePush(int opcode, x86JITScope& scope);
   friend void compileReturn(int opcode, x86JITScope& scope);
   friend void compileCall(int opcode, x86JITScope& scope);
   friend void compilePop(int opcode, x86JITScope& scope);
   friend void compileMove(int opcode, x86JITScope& scope);
   friend void compileExit(int opcode, x86JITScope& scope);
   friend void compileRedirect(int opcode, x86JITScope& scope);
   friend void compileSet(int opcode, x86JITScope& scope);
   friend void compileReturnIf(int opcode, x86JITScope& scope);
   friend void compileIOCallN(int opcode, x86JITScope& scope);
   friend void compileOthers(int opcode, x86JITScope& scope);
   friend void compileCreate(x86JITScope& scope);

   // fields
   void (*_commands[0x10])(int opcode, x86JITScope& scope);

   x86JITInlineCode _inlines[22];
   
   // preloaded GC references
   Cache<ref_t, void*, 0x10> _preloaded;

public:
   virtual void addPreloadedReference(ref_t reference, void* address);

   virtual void compileMethod(_ReferenceHelper& helper, StreamReader& reader, SectionWriter& codeWriter);

   virtual void alignCode(SectionWriter* writer, int alignment, bool code);

   x86JITCompiler(_Module* binary);
};

} // _ELENA_

#endif // x86jitcompilerH
