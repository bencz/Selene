//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Compiler Engine
//
//		This file contains ELENA JIT compiler class.
//
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef jitcompilerH
#define jitcompilerH 1

namespace _ELENA_
{

//#define DUPLICATE_ENTRY (size_t)-2

// --- ReferenceHelper ---

class _ReferenceHelper
{
public:
   virtual SectionInfo getSection(ref_t reference) = 0;

   virtual ref_t resolveMessageID(ref_t reference) = 0;

   virtual void writeReference(SectionWriter& writer, _Module* module, ref_t reference, size_t disp) = 0;
   virtual void writeReference(SectionWriter& writer, ref_t reference, size_t disp) = 0;
   virtual void writeReference(SectionWriter& writer, void* vaddress, bool relative, size_t disp) = 0;
   //virtual void writeMethodReference(SectionWriter& writer, size_t tapeDisp) = 0;

   virtual void addBreakpoint(size_t position) = 0;
};

// --- JITCompiler class ---
class _JITCompiler
{
public:
   virtual void addPreloadedReference(ref_t reference, void* address) = 0;

   virtual void alignCode(SectionWriter* writer, int alignment, bool code) = 0;

   virtual void compileSymbol(_ReferenceHelper& helper, StreamReader& reader, SectionWriter& codeWriter);

   virtual void compilePseudoVMT(SectionWriter& vmtWriter, void* address);

   virtual void compileMethod(_ReferenceHelper& helper, StreamReader& reader, SectionWriter& codeWriter) = 0;

   virtual int copyParentVMT(void* parentVMT, VMTEntry* entries);

   virtual void addVMTEntry(_ReferenceHelper& helper, int messageID, size_t codePosition, VMTEntry* entries, size_t& count);

   virtual void compileVMT(void* vaddress, SectionWriter& vmtWriter, ClassHeader& header, int count, bool virtualMode);
};

} // _ELENA_

#endif // jitcompilerH
