//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Compiler
//
//		This header contains ELENA Executive Linker class declaration
//		Supported platforms: Win32
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef linkerH
#define linkerH 1

#include "project.h"
#include "jitlinker.h"

namespace _ELENA_
{

// --- Linker ---

class Linker : public _LoaderHelper
{
   typedef Map<const char*, Section*, false> ImageSectionMap;
   typedef Map<const TCHAR*, ReferenceMap*>  ImportTable;

   Project*         _project;
   ReferenceLoader  _loader;
   ImageSectionMap  _sections;

   // Reference mappings
   ReferenceMap     _nativeReferences; // native references
   ReferenceMap     _symbolReferences; // symbol references
   ReferenceMap     _constReferences;  // constant symbol references
   ReferenceMap     _messages;         // messages
   ReferenceMap     _numbers;
   ReferenceMap     _literals;

   // Import table
   size_t           _importTableSize;
   ImportTable      _importTable;
   RelocationFixMap _importFixes;

   // Linker target image properties
   int _headerSize, _imageSize;
   int _codeBase, _dataBase, _bssBase, _importBase;
   int _entryPoint; 

   void* _symbolEntryPoint;

   // Debug info
   bool      _withDebugInfo;
   Section*  _debugInfo;

   Section* getSection(const char* name);

   int getSectionSize(const char* name)
   {
      if (_sections.exist(name)) {
         return _sections.get(name)->Length();
      }
      else return 0;
   }

   ref_t resolveExternal(const TCHAR* reference);

   void createImportTable(size_t count);

   void createImage(const TCHAR* entry);
   void mapImage();
   void fixImage();

   void writeDOSStub(FileWriter* file);
   void writeHeader(FileWriter* file, short characteristics);
   void writeNTHeader(FileWriter* file);
   void writeSections(FileWriter* file);

   bool createExecutable(const TCHAR* exePath);
   bool createDebugFile(const TCHAR* debugFilePath);

   void* resolveNativeReference(const TCHAR* reference, size_t mask);
   void* resolveSymbolReference(const TCHAR* reference, size_t mask);
   ref_t resolveMessage(const TCHAR* reference);

   void mapNativeReference(const TCHAR* reference, void* vaddress);
   void mapSymbolReference(const TCHAR* reference, void* vaddress, size_t mask);

public:
   // _LoaderHelper interface 
   virtual const TCHAR* retrieveReference(_Module* module, ref_t reference, ref_t mask);

   virtual void* resolveReference(const TCHAR* reference, size_t mask);
   virtual void mapReference(const TCHAR* reference, void* vaddress, size_t mask);

   virtual SectionInfo getSectionInfo(const TCHAR* reference, size_t mask);
   virtual ClassSectionInfo getClassSectionInfo(const TCHAR* reference, size_t codeMask, size_t vmtMask);

   virtual Section* getTargetSection(size_t mask);
   virtual Section* getTargetDebugSection();

   virtual const TCHAR* getLiteralClass();
   virtual const TCHAR* getIntegerClass();
   virtual const TCHAR* getRealClass();

   virtual size_t getLinkerConstant(int id);

   void run();

   Linker(Project* project, _JITCompiler* compiler)
      : _loader(this, compiler, true, (void*)mskCodeRef, project->BoolSetting(opWithDebugInfo)), _sections(NULL, freeobj), _importTable(NULL, freeobj),
        _nativeReferences((size_t)-1), _symbolReferences((size_t)-1), _constReferences((size_t)-1), _messages(0), _literals((size_t)-1), 
        _numbers((size_t)-1), _importFixes((size_t)-1)
   {
      _project = project;

      _symbolEntryPoint = NULL;
      _entryPoint = 0;
      _headerSize = _imageSize = 0;
      _codeBase = _dataBase = _bssBase = _importBase = 0;
      _importTableSize = 0;
      _debugInfo = NULL;
      _withDebugInfo = project->BoolSetting(opWithDebugInfo);
   }

   ~Linker() { freeobj(_debugInfo); }
};

} // _ELENA_

#endif // linkerH
