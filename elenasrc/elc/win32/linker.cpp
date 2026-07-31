//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Compiler
//
//		This file contains ELENA Executive Linker class implementation
//		Supported platforms: Win32
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#include "elena.h"
// --------------------------------------------------------------------------
#include "linker.h"
#include "errors.h"

#include <windows.h>

#include <time.h>

#define MAJOR_OS           0x04
#define MINOR_OS           0x00

#define FILE_ALIGNMENT     0x200
#define SECTION_ALIGNMENT  0x1000
#define IMAGE_BASE         0x00400000

#define TEXT_SECTION       ".text"
#define DATA_SECTION       ".data"
#define BSS_SECTION        ".bss"
#define IMPORT_SECTION     ".import"
#define DEBUG_SECTION      ".debug"

#ifndef IMAGE_SIZEOF_NT_OPTIONAL_HEADER
#define IMAGE_SIZEOF_NT_OPTIONAL_HEADER 224
#endif

using namespace _ELENA_;

// --- Linker ---

Section* Linker :: getSection(const char* name)
{
   if (compstr(name, DEBUG_SECTION)) {
      if (_debugInfo==NULL)
         _debugInfo = new Section();

      return _debugInfo;
   }
   else {
      Section* section = _sections.get(name);
      if (section == NULL) {
         section = new Section();

         if (compstr(name, DATA_SECTION)) {
            SectionWriter writer(section);

            writer.writeLiteral(ELENA_SIGNITURE, strlen(ELENA_SIGNITURE));
            writer.align(4, 0);
         }
         _sections.add(name, section);
      }
      return section;
   }
}

inline void* retrieveMappedReference(ReferenceMap& map, const TCHAR* reference, int mask)
{
   ReferenceMap::Iterator it = map.getIt(reference);
   if (!it.Eof()) {
      // check if we get the correct reference
      if (((*it) & mskImageMask)  == mask) {
         return (void*)*it;
      }
      // otherwise try to find it
      else while(!it.Eof() && compstr(it.key(), reference)) {
         if (((*it) & mskImageMask) == mask) {
            return (void*)*it;
         }
         it++;
      }
   }
   return LOADER_NOTLOADED;
}

void* Linker :: resolveNativeReference(const TCHAR* reference, size_t mask)
{
   return retrieveMappedReference(_nativeReferences, reference, mask & mskImageMask);
}

void* Linker :: resolveSymbolReference(const TCHAR* reference, size_t mask)
{
   if (mask == mskSymbolRef || mask == mskVMTRef) {
      return retrieveMappedReference(_symbolReferences, reference, mask & mskImageMask);
   }
   else if (mask == mskConstantRef) {
      return (void*)_constReferences.get(reference);
   }
   // to distinguish symbol code from class code pseudo mask is used for class code
   else return LOADER_NOTLOADED;
}

ref_t Linker :: resolveMessage(const TCHAR* reference)
{
   return mapKey(_messages, reference, _messages.Count() + 1);
}

void Linker :: mapNativeReference(const TCHAR* reference, void* vaddress)
{
   _nativeReferences.add(reference, (ref_t)vaddress);
}

void Linker :: mapSymbolReference(const TCHAR* reference, void* vaddress, size_t mask)
{
   if (mask == mskSymbolRef || mask == mskVMTRef) {
      _symbolReferences.add(reference, (ref_t)vaddress);
   }
   else if (mask == mskConstantRef) {
      _constReferences.add(reference, (ref_t)vaddress);
   }
}

void* Linker :: resolveReference(const TCHAR* reference, size_t mask)
{
   if (mask == mskExternalRef) {
      return (void*)resolveExternal(reference);
   }
   else if (mask == mskConstantRef) {
      return resolveSymbolReference(reference, mask);
   }
   else if (mask == mskInt32Ref || mask == mskRealRef) {
      return (void*)_numbers.get(reference);
   }
   else if (mask == mskLiteralRef) {
      return (void*)_literals.get(reference);
   }
   else if (test(mask, mskNativeMask)) {
      return resolveNativeReference(reference, mask);
   }
   else if (mask==0) {
      return (void*)resolveMessage(reference);
   }
   else if (mask == mskStaticConstRef) {
      return resolveNativeReference(reference, mskNativeStaticRef);
   }
   else return resolveSymbolReference(reference, mask);
}

void Linker :: mapReference(const TCHAR* reference, void* vaddress, size_t mask)
{
   if (mask == mskConstantRef) {
      mapSymbolReference(reference, vaddress, mask);
   }
   else if (mask == mskInt32Ref || mask == mskRealRef) {
      mapKey(_numbers, reference, (size_t)vaddress);
   }
   else if (mask == mskLiteralRef) {
      mapKey(_literals, reference, (size_t)vaddress);
   }
   else if (test(mask, mskNativeMask)) {
      mapNativeReference(reference, vaddress);
   }
   else mapSymbolReference(reference, vaddress, mask);
}

SectionInfo Linker :: getSectionInfo(const TCHAR* reference, size_t mask)
{
   SectionInfo sectionInfo;

   ref_t referenceID = 0;
   sectionInfo.module = _project->resolveModule(reference, referenceID);
   if (sectionInfo.module == NULL || referenceID == 0) {
      _project->raiseError(errUnresovableLink, reference);
   }
   else sectionInfo.section = sectionInfo.module->mapSection(referenceID | mask, true);

   if (sectionInfo.section == NULL) {
      _project->raiseError(errUnresovableLink, reference);
   }

   return sectionInfo;
}

ClassSectionInfo Linker :: getClassSectionInfo(const TCHAR* reference, size_t codeMask, size_t vmtMask)
{
   ClassSectionInfo sectionInfo;

   ref_t referenceID = 0;
   sectionInfo.module = _project->resolveModule(reference, referenceID);
   if (sectionInfo.module == NULL || referenceID == 0)
      _project->raiseError(errUnresovableLink, reference);
   else {
      sectionInfo.codeSection = sectionInfo.module->mapSection(referenceID | codeMask, true);
      sectionInfo.vmtSection = sectionInfo.module->mapSection(referenceID | vmtMask, true);
   }
   return sectionInfo;
}

Section* Linker :: getTargetSection(size_t mask)
{
   mask = mask & mskImageMask;
   if (mask == mskCodeRef) {
      return getSection(TEXT_SECTION);
   }
   else if (mask == mskDataRef) {
      return getSection(DATA_SECTION);
   }
   else if (mask == mskStaticRef) {
      return getSection(BSS_SECTION);
   }
   else return NULL;
}

Section* Linker :: getTargetDebugSection()
{
   return getSection(DEBUG_SECTION);
}

const TCHAR* Linker :: retrieveReference(_Module* module, ref_t reference, ref_t mask)
{
   if (mask == mskLiteralRef || mask == mskInt32Ref || mask == mskRealRef) {
      return module->resolveConstant(reference);
   }
   // if it is a message
   else if (mask == 0) {
      return module->resolveMessage(reference);
   }
   else {
      const TCHAR* referenceName = module->resolveReference(reference);
      while (isWeakReference(referenceName)) {
         const TCHAR* resolvedName = _project->resolveForward(referenceName);

         if (!emptystr(resolvedName))  {
            referenceName = resolvedName;
         }
         else _project->raiseError(errUnresovableLink, referenceName);
      }
      return referenceName;
   }
}

const TCHAR* Linker :: getLiteralClass()
{
   const TCHAR* className = _project->StrSetting(opLiteralClass);

   return emptystr(className) ? SUPER_CLASS : className;
}

const TCHAR* Linker :: getIntegerClass()
{
   const TCHAR* className = _project->StrSetting(opIntegerClass);

   return emptystr(className) ? SUPER_CLASS : className;
}

const TCHAR* Linker :: getRealClass()
{
   const TCHAR* className = _project->StrSetting(opRealClass);

   return emptystr(className) ? SUPER_CLASS : className;
}

size_t Linker :: getLinkerConstant(int id)
{
   switch (id) {
      case lnGCSize:
         return _project->IntSetting(opGCHeapSize);
      default:
         return 0;
   }
}

ref_t Linker :: resolveExternal(const TCHAR* external)
{
   const TCHAR* function = _tcsrchr(external, '.') + 1;
   LocalString<MAX_PATH> dll(external + getlength(DLL_NAMESPACE) + 1,
                               function - (external + getlength(DLL_NAMESPACE)) - 2);

   ReferenceMap* functions = _importTable.get(dll);
   if (functions==NULL) {
      functions = new ReferenceMap(0);

      _importTable.add(dll, functions);
   }
   ref_t reference = functions->get(function);
   if (!reference) {
      _importTableSize++;
      reference = _importTableSize | mskExternalRef;

      functions->add(function, reference);
   }
   return reference;
}

void Linker :: createImage(const TCHAR* entry)
{
   try
   {
      // text section should be created first
      Section* code = getSection(TEXT_SECTION);

      // initialize Garbage Collection routine for JIT compiler
      _loader.preloadCoreCode(_project->resolvePrimitive(CORE_BINARY_MODULE, false));

      // load starting symbol      
      _symbolEntryPoint = _loader.load(entry, mskNativeCodeRef, true);
      if(_symbolEntryPoint == LOADER_NOTLOADED)
         _project->raiseError(errUnresovableLink, entry);

      _entryPoint = reallocateReference((ref_t)_symbolEntryPoint);
   }
   catch(JITUnresolvedException& ex)
   {
      _project->raiseError(errUnresovableLink, ex.reference);
   }
}

void Linker :: mapImage()
{
   int alignment = _project->IntSetting(opSectionAlignment, SECTION_ALIGNMENT);

   _codeBase = 0x1000;               // !! code section should always be first?
   _dataBase = align(_codeBase + getSectionSize(TEXT_SECTION), alignment);
   _bssBase = align(_dataBase + getSectionSize(DATA_SECTION), alignment);
   _importBase = align(_bssBase + getSectionSize(BSS_SECTION), alignment);
   _imageSize = align(_importBase + getSectionSize(IMPORT_SECTION), alignment);
}

void Linker :: createImportTable(size_t count)
{
   Section* import = getSection(IMPORT_SECTION);

   SectionWriter writer(import);

   // reference to the import section
   ref_t importRef = (count + 1) | mskExternalRef;
   _importFixes.add(importRef, 0);

   SectionWriter tableWriter(import);
   writer.writeBytes(0, (count + 1) * 20);               // fill import table
   SectionWriter fwdWriter(import);
   writer.writeBytes(0, (_importTable.Count()+count)*4);  // fill forward table
   SectionWriter lstWriter(import);
   writer.writeBytes(0, (_importTable.Count()+count)*4);  // fill import list

   ImportTable::Iterator dll = _importTable.start();
   while (!dll.Eof()) {
      tableWriter.writeRef(importRef, lstWriter.Position());              // OriginalFirstThunk
      tableWriter.writeDWord(time(NULL));                                 // TimeDateStamp
      tableWriter.writeDWord(-1);                                         // ForwarderChain
      tableWriter.writeRef(importRef, import->Length());                  // Name
      const TCHAR* dllname = dll.key();
      if (!Path::checkExtension(dllname, _T("dll"))) {
         writer.writeAsciiLiteral(dllname, getlength(dllname));
         writer.writeAsciiLiteral(_T(".dll"));
      }
      else writer.writeAsciiLiteral(dllname);
      tableWriter.writeRef(importRef, fwdWriter.Position());              // ForwarderChain

      // fill OriginalFirstThunk & ForwarderChain
      ReferenceMap::Iterator fun = (*dll)->start();
      while (!fun.Eof()) {
         _importFixes.add(*fun, fwdWriter.Position());

         fwdWriter.writeRef(importRef, import->Length());
         lstWriter.writeRef(importRef, import->Length());

         writer.writeWord(1);                                             // Hint (not used)
         writer.writeAsciiLiteral(fun.key());

         fun++;
      }
      lstWriter.writeDWord(0);                                            // mark end of chains
      fwdWriter.writeDWord(0);

      dll++;
   }
}

void Linker :: fixImage()
{
   Section* text = _sections.get(TEXT_SECTION);
   Section* data = _sections.get(DATA_SECTION);
   Section* import = _sections.get(IMPORT_SECTION);

  // fix up import table
   import->fixupReferences(_importFixes, _importBase, false);

   int imageBase = _project->IntSetting(opImageBase, IMAGE_BASE);

  // fix up static table size
   void* table = resolveNativeReference(GC_TABLE, mskNativeDataRef);
   ref_t offset = reallocateReference((size_t)table);
   (*data)[offset + 0x14] = (getSectionSize(BSS_SECTION) >> 2);

  // fix up code references
   text->fixupReferences(_codeBase + imageBase, mskCodeRef, reallocateReference);
   data->fixupReferences(_codeBase + imageBase, mskCodeRef, reallocateReference);

  // fix up data references
   text->fixupReferences(_dataBase + imageBase, mskDataRef, reallocateReference);
   data->fixupReferences(_dataBase + imageBase, mskDataRef, reallocateReference);

  // fix up static data references
   text->fixupReferences(_bssBase + imageBase, mskStaticRef, returnReference);

  // fix up import
   text->fixupReferences(_importFixes, _importBase + imageBase, false);

  // fix up debug info if enabled
   if (_withDebugInfo) {
      _debugInfo->fixupReferences(_codeBase + imageBase, mskCodeRef, reallocateReference);
      _debugInfo->fixupReferences(_dataBase + imageBase, mskDataRef, reallocateReference);
   }
}

void Linker :: writeDOSStub(FileWriter* file)
{
   LocalPath stubPath(_project->StrSetting(opAppPath), _T("winstub.ex_"));
   FileReader stub(stubPath, _T("rb"), feRaw);

   if (stub.isOpened()) {
      file->read(&stub, stub.Length());
   }
   else _project->raiseError(errInvalidFile, stubPath.asString());
}

void Linker :: writeHeader(FileWriter* file, short characteristics)
{
   IMAGE_FILE_HEADER   header;

   header.Machine = IMAGE_FILE_MACHINE_I386; // !! machine type may be different;
   header.NumberOfSections = (short)_sections.Count();
   header.TimeDateStamp = time(NULL);
   header.SizeOfOptionalHeader = IMAGE_SIZEOF_NT_OPTIONAL_HEADER;
   header.Characteristics = characteristics;
   header.Characteristics |= IMAGE_FILE_32BIT_MACHINE;
   header.Characteristics |= IMAGE_FILE_LOCAL_SYMS_STRIPPED;
   header.Characteristics |= IMAGE_FILE_LINE_NUMS_STRIPPED;
   header.PointerToSymbolTable = 0;
   header.NumberOfSymbols = 0;

   file->write((char*)&header, IMAGE_SIZEOF_FILE_HEADER);
}

void Linker :: writeNTHeader(FileWriter* file)
{
   IMAGE_OPTIONAL_HEADER   header;

   header.Magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
   header.MajorLinkerVersion = 1;                                              // not used
   header.MinorLinkerVersion = 0;
   header.SizeOfCode = getSectionSize(TEXT_SECTION);
   header.SizeOfInitializedData = getSectionSize(DATA_SECTION) + getSectionSize(IMPORT_SECTION);
   header.SizeOfUninitializedData = getSectionSize(BSS_SECTION);         // 0x400

   header.AddressOfEntryPoint = _codeBase + _entryPoint;
   header.BaseOfCode = _codeBase;
   header.BaseOfData = _dataBase;

   header.ImageBase = _project->IntSetting(opImageBase, IMAGE_BASE);
   header.SectionAlignment = _project->IntSetting(opSectionAlignment, SECTION_ALIGNMENT);
   header.FileAlignment = _project->IntSetting(opFileAlignment, FILE_ALIGNMENT);

   header.MajorOperatingSystemVersion = MAJOR_OS;
   header.MinorOperatingSystemVersion = MINOR_OS;
   header.MajorImageVersion = 0;                                               // not used
   header.MinorImageVersion = 0;
   header.MajorSubsystemVersion = MAJOR_OS;                                    // set for Win 4.0
   header.MinorSubsystemVersion = MINOR_OS;
   #ifndef mingw49
   header.Win32VersionValue = 0;                                               // ??
   #endif

   header.SizeOfImage = _imageSize;
   header.SizeOfHeaders = _headerSize;
   header.CheckSum = 0;                                                        // For EXE file
   switch (_project->IntSetting(opSystemType, ptConsole))
   {
      case ptGUI:
         header.Subsystem = IMAGE_SUBSYSTEM_WINDOWS_GUI;
         break;
      case ptConsole:
      default:
         header.Subsystem = IMAGE_SUBSYSTEM_WINDOWS_CUI;
         break;
   }

   header.DllCharacteristics = 0;                                              // For EXE file
   header.LoaderFlags = 0;                                                     // not used

   header.SizeOfStackReserve = _project->IntSetting(opSizeOfStackReserv, 0x100000); // !! explicit constant name
   header.SizeOfStackCommit = _project->IntSetting(opSizeOfStackCommit, 0x1000);    // !! explicit constant name
   header.SizeOfHeapReserve = _project->IntSetting(opSizeOfHeapReserv, 0x100000);   // !! explicit constant name
   header.SizeOfHeapCommit = _project->IntSetting(opSizeOfHeapCommit, 0x10000);     // !! explicit constant name

   header.NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;
   for (unsigned long i = 0 ; i < header.NumberOfRvaAndSizes ; i++) {
      header.DataDirectory[i].VirtualAddress = 0;
      header.DataDirectory[i].Size = 0;
   }
   if (_sections.exist(IMPORT_SECTION)) {
      header.DataDirectory[1].VirtualAddress = _importBase;
      header.DataDirectory[1].Size = getSectionSize(IMPORT_SECTION);
   }
   file->write((char*)&header, IMAGE_SIZEOF_NT_OPTIONAL_HEADER);
}

void Linker :: writeSections(FileWriter* file)
{
   IMAGE_SECTION_HEADER header;

   int alignment = _project->IntSetting(opFileAlignment, FILE_ALIGNMENT);
   int sectionAlignment = _project->IntSetting(opSectionAlignment, SECTION_ALIGNMENT);
   int tblOffset = _headerSize;

   ImageSectionMap::Iterator it = _sections.start();
   while (!it.Eof()) {
      strncpy((char*)header.Name, it.key(), 8);
      header.Misc.VirtualSize = align((*it)->Length(), sectionAlignment);
      header.SizeOfRawData = align((*it)->Length(), alignment);
      header.PointerToRawData = tblOffset;

      header.PointerToRelocations = 0;
      header.PointerToLinenumbers = 0;
      header.NumberOfRelocations = 0;
      header.NumberOfLinenumbers = 0;

      if(compstr(it.key(), TEXT_SECTION)){
         header.VirtualAddress = _codeBase;
         header.Characteristics = IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE
                                      | IMAGE_SCN_MEM_READ;
      }
      else if (compstr(it.key(), DATA_SECTION)) {
         header.VirtualAddress = _dataBase;
         header.Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ
                                      | IMAGE_SCN_MEM_WRITE;
      }
      else if (compstr(it.key(), BSS_SECTION)) {
         header.VirtualAddress = _bssBase;
         header.Characteristics = IMAGE_SCN_CNT_UNINITIALIZED_DATA | IMAGE_SCN_MEM_READ
                                      | IMAGE_SCN_MEM_WRITE;
         header.SizeOfRawData = 0;
         header.PointerToRawData = 0;
      }
      else if (compstr(it.key(), IMPORT_SECTION)) {
         header.VirtualAddress = _importBase;
         header.Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_SHARED
                                      | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;
      }
      file->write((char*)&header, IMAGE_SIZEOF_SECTION_HEADER);

      tblOffset += header.SizeOfRawData;
      it++;
   }
   file->align(alignment);

   it = _sections.start();
   while (!it.Eof()) {
      if (!compstr(it.key(), BSS_SECTION)) {
         DumpReader reader(*it);
         file->read(&reader, (*it)->Length());
         file->align(alignment);
      }
      it++;
   }
}

bool Linker :: createExecutable(const TCHAR* exePath)
{
   FileWriter executable(exePath, feRaw);

   if (!executable.isOpened())
      return false;

   writeDOSStub(&executable);

   executable.writeDWord((int)IMAGE_NT_SIGNATURE);

   _headerSize = executable.Length();
   _headerSize += IMAGE_SIZEOF_FILE_HEADER + IMAGE_SIZEOF_NT_OPTIONAL_HEADER;
   _headerSize += IMAGE_SIZEOF_SECTION_HEADER * _sections.Count();
   _headerSize = align(_headerSize, _project->IntSetting(opFileAlignment, FILE_ALIGNMENT));

   writeHeader(&executable, IMAGE_FILE_EXECUTABLE_IMAGE);
   writeNTHeader(&executable);
   writeSections(&executable);

   return true;
}

bool Linker :: createDebugFile(const TCHAR* debugFilePath)
{
   FileWriter	debugFile(debugFilePath, feRaw);

   if (!debugFile.isOpened())
      return false;

   Section*	debugInfo = getSection(DEBUG_SECTION);

   // signature
   debugFile.write(DEBUG_MODULE_SIGNATURE, strlen(DEBUG_MODULE_SIGNATURE));

   // save entry point
   const TCHAR* starter = STARTUP_CLASS;
   while (isWeakReference(starter)) {
      starter = _project->resolveForward(starter);
   }

   ref_t imageBase = _project->IntSetting(opImageBase, IMAGE_BASE);
   ref_t entryPoint = _codeBase + imageBase + ((ref_t)resolveReference(starter, mskSymbolRef) << VA_ALIGNMENT_POWER);

   debugFile.writeDWord(entryPoint);

   // save DebugInfo
   DumpReader reader(_debugInfo);
   debugFile.read(&reader, _debugInfo->Length());

   return true;
}

void Linker :: run()
{
   createImage(_project->StrSetting(opEntry));
   createImportTable(_importTableSize);
   mapImage();
   fixImage();

   const TCHAR* path = _project->StrSetting(opTarget);
   if (!createExecutable(path))
      _project->raiseError(errCannotCreate, path);

   if (_withDebugInfo) {
      LocalPath debugPath(path);
      debugPath.changeExtension(DEBUG_FILE_EXTENSION);

      if (!createDebugFile(debugPath))
         _project->raiseError(errCannotCreate, debugPath.asString());
   }
}
