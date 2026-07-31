//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Compiler Engine
//
//		This file contains ELENA JIT linker class.
//
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef jitlinkerH
#define jitlinkerH 1

#include "jitcompiler.h"

namespace _ELENA_
{

// -- Loader basic constant --
#define LOADER_NOTLOADED (void*)-1

// --- JITUnresolvedException ---
struct JITUnresolvedException
{
   const TCHAR* reference;

   JITUnresolvedException(const TCHAR* reference)
   {
      this->reference = reference;
   }
};

// --- LoaderHelper ---
class _LoaderHelper
{
public:
   virtual const TCHAR* retrieveReference(_Module* module, ref_t reference, ref_t mask) = 0;

   virtual void* resolveReference(const TCHAR* reference, size_t mask) = 0;

   virtual void mapReference(const TCHAR* reference, void* vaddress, size_t mask) = 0;

   virtual SectionInfo getSectionInfo(const TCHAR* reference, size_t mask) = 0;

   virtual ClassSectionInfo getClassSectionInfo(const TCHAR* reference, size_t codeMask, size_t vmtMask) = 0;

   virtual Section* getTargetSection(size_t mask) = 0;

   virtual Section* getTargetDebugSection() = 0;

   virtual const TCHAR* getLiteralClass() = 0;
   virtual const TCHAR* getIntegerClass() = 0;
   virtual const TCHAR* getRealClass() = 0;

   virtual size_t getLinkerConstant(int id) = 0;
};

// --- ReferenceLoader class ---
class ReferenceLoader
{
   struct RefInfo
   {
      ref_t    reference;
      _Module* module;

      RefInfo()
      {
         reference = 0;
      }
      RefInfo(ref_t reference, _Module* module)
      {
         this->reference = reference;
         this->module = module;
      }
   };

   typedef CachedMemoryMap<ref_t, RefInfo, 10> References;

   // --- ReferenceHelper ---
   class ReferenceHelper : public _ReferenceHelper
   {
      References*      _references;

      ReferenceLoader* _owner;
      Section*         _debug;
      _Module*         _module;

   public:
      virtual SectionInfo getSection(ref_t reference);

      virtual ref_t resolveMessageID(ref_t reference);

      virtual void addBreakpoint(size_t position);

//      virtual void writeMethodReference(SectionWriter& writer, size_t tapeDisp);
      virtual void writeReference(SectionWriter& writer, _Module* module, ref_t reference, size_t disp);
      virtual void writeReference(SectionWriter& writer, ref_t reference, size_t disp);
      virtual void writeReference(SectionWriter& writer, void* vaddress, bool relative, size_t disp);

      ReferenceHelper(ReferenceLoader* owner, _Module* module, References* references)
      {
         _references = references;
         _owner = owner;
         _module = module;
         _debug = NULL;
      }
   };

   friend class ReferenceHelper;

   _LoaderHelper* _helper;
   _JITCompiler*  _compiler; 
   bool           _virtualMode;
   bool           _withDebugInfo;
   void*          _codeBase;

   void* getVMTReference(_Module* module, ref_t reference);

   void createNativeSymbolDebugInfo(const TCHAR* reference);
   void createNativeClassDebugInfo(const TCHAR* reference, void* vaddress);

   void* calculateVAddress(SectionWriter* writer, int mask);

   void fixReferences(References& relocations, Section* image);

   size_t loadMethod(ReferenceHelper& refHelper, DumpReader& reader, SectionWriter& writer);

   ref_t resolveMessageID(_Module* module, ref_t reference);

   void* loadNativeSection(const TCHAR*  reference, int mask, SectionInfo sectionInfo);
   void* loadBytecodeSection(const TCHAR*  reference, int mask, SectionInfo sectionInfo);
   void* loadBytecodeVMTSection(const TCHAR*  reference, int mask, ClassSectionInfo sectionInfo);
   void* loadConstant(const TCHAR*  reference, int mask);
   void* loadStaticVariable(const TCHAR* reference, int mask);

   void* loadNativeData(const TCHAR* reference, int size);
   void* loadPseudoVMT(const TCHAR* reference, void* codeRef);

public:
   void preloadCoreCode(_Module* coreBinary);

   void* load(const TCHAR* reference, int mask, bool silentMode);

   ReferenceLoader(_LoaderHelper* helper, _JITCompiler* compiler, bool virtualMode, void* codeBase, bool withDebugInfo)
   {
      _helper = helper;
      _compiler = compiler;
      _virtualMode = virtualMode;
      _withDebugInfo = withDebugInfo;
      _codeBase = codeBase;
   }
};

} // _ELENA_

#endif // jitlinkerH
