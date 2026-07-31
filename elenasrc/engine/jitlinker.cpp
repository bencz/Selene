//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Compiler Engine
//
//		This file contains ELENA JIT linker class implementation.
//
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#include "elena.h"
// --------------------------------------------------------------------------
#include "jitlinker.h"

using namespace _ELENA_;

// --- resolveReference ---

inline void resolveReference(Section* image, size_t position, ref_t vaddress, size_t mask, bool virtualMode)
{
   if (!virtualMode) {
      ref_t address = reallocateReference(vaddress);
      if (test(mask, mskRelativeRef)) {
         (*image)[position] = address - ((size_t)image->getArray()) - position - 4;
      }
      else (*image)[position] += address;
   }
   // in virtual mode
   else {
      if (test(mask, mskRelativeRef))
         vaddress = vaddress | mskRelativeRef;

      image->addReference(vaddress, position);
   }
}

// --- ReferenceLoader::ReferenceHelper ---

SectionInfo ReferenceLoader::ReferenceHelper :: getSection(ref_t reference)
{
   const TCHAR* referenceName = _module->resolveReference(reference & ~mskAnyRef);

   return _owner->_helper->getSectionInfo(referenceName, mskNativeCodeRef);
}

ref_t ReferenceLoader::ReferenceHelper :: resolveMessageID(ref_t reference)
{
   return _owner->resolveMessageID(_module, reference);
}

void ReferenceLoader::ReferenceHelper :: addBreakpoint(size_t position)
{
   if (_owner->_withDebugInfo) {
      if (!_debug)
         _debug = _owner->_helper->getTargetDebugSection();

      SectionWriter writer(_debug);

      if (!_owner->_virtualMode) {
         ref_t address = reallocateReference((size_t)_owner->_codeBase);

         writer.writeDWord(address + position);
      }
      else writer.writeRef((ref_t)_owner->_codeBase, position);
   }
}

//void ReferenceLoader::ReferenceHelper :: writeMethodReference(SectionWriter& writer, size_t tapeDisp)
//{
//   _relocations->add(tapeDisp, writer.Position());
//   writer.writeRef(mskCodeRef, 0);
//}

void ReferenceLoader::ReferenceHelper :: writeReference(SectionWriter& writer, _Module* module, ref_t reference, size_t disp)
{
   ref_t position = writer.Position();
   writer.writeDWord(disp);

   ref_t mask = reference & mskAnyRef;
   ref_t refID = reference & ~mskAnyRef;
   
   // try to resolve immediately
   void* vaddress = _owner->_helper->resolveReference(
      _owner->_helper->retrieveReference(module, refID, mask & ~mskRelativeRef), mask & ~mskRelativeRef);

   if (vaddress != LOADER_NOTLOADED) {
      resolveReference(writer.getSection(), position, (ref_t)vaddress, mask, _owner->_virtualMode);
   }
   // or resolve later
   else _references->add(position, RefInfo(reference, module));
}

void ReferenceLoader::ReferenceHelper :: writeReference(SectionWriter& writer, ref_t reference, size_t disp)
{
   writeReference(writer, _module, reference, disp);
}

void ReferenceLoader::ReferenceHelper :: writeReference(SectionWriter& writer, void* vaddress, bool relative, size_t disp)
{
   if (!_owner->_virtualMode) {
      ref_t address = reallocateReference((size_t)vaddress);

      writer.writeDWord(address + disp);
   }
   else if (relative) {
      writer.writeRef((ref_t)vaddress | mskRelativeRef, disp);
   }
   else writer.writeRef((ref_t)vaddress, disp);
}

// --- ReferenceLoader ---

ref_t ReferenceLoader :: resolveMessageID(_Module* module, ref_t reference)
{
   if (!test(reference, PREDEFINED_REF)) { 
      const TCHAR* message = module->resolveMessage(reference);

      return (ref_t)_helper->resolveReference(message, 0);
   }
   else return reference;
}

void* ReferenceLoader :: calculateVAddress(SectionWriter* writer, int mask)
{
   // align the section
   _compiler->alignCode(writer, VA_ALIGNMENT, test(mask, mskCodeRef));

   // virtual address - real address in the memory of nonvirtual mode, or section relative address
   size_t va = _virtualMode ? writer->Position() : (size_t)writer->Address();

   // returning vaddress = mask + address divided by section alignment
   return (void*)(mask | va >> VA_ALIGNMENT_POWER);
}

void ReferenceLoader :: fixReferences(References& references, Section* image)
{
   // fix not loaded references
   ref_t currentMask = 0;
   ref_t currentRef = 0;
   References::Iterator it = references.start();
   while (!it.Eof()) {
      RefInfo current = *it;

      currentMask = current.reference & mskAnyRef;
      currentRef = current.reference & ~mskAnyRef;

      void* refVAddress = load(_helper->retrieveReference(current.module, currentRef, currentMask), currentMask & ~mskRelativeRef, false);

      resolveReference(image, it.key(), (ref_t)refVAddress, currentMask, _virtualMode);

      it++;
   }
}

void* ReferenceLoader :: getVMTReference(_Module* module, ref_t reference)
{
   if (reference != 0) {
      ref_t mask = mskVMTRef;

      void* vaddress = load(_helper->retrieveReference(module, reference, mask), mask, false);
      if (_virtualMode) {
         Section* image = _helper->getTargetSection(mask);

         return image->get(reallocateReference((ref_t)vaddress));
      }
      else return vaddress;
   }
   else return NULL;
}

size_t ReferenceLoader :: loadMethod(ReferenceHelper& refHelper, DumpReader& reader, SectionWriter& writer)
{
   size_t position = writer.Position();

   // method just in time compilation
   _compiler->compileMethod(refHelper, reader, writer);

   return position;
}

void* ReferenceLoader :: loadNativeSection(const TCHAR*  reference, int mask, SectionInfo sectionInfo)
{
   if (sectionInfo.section == NULL)
      return LOADER_NOTLOADED;

   // get target image & resolve virtual address
   Section* image = _helper->getTargetSection(mask);
   SectionWriter writer(image);

   void* vaddress = calculateVAddress(&writer, mask & mskImageMask);
   size_t position = writer.Position();

   _helper->mapReference(reference, vaddress, mask);

   // load section into target image
   DumpReader reader(sectionInfo.section);
   writer.read(&reader, sectionInfo.section->Length());

   // resolve section references
   _ELENA_::RelocationMap::Iterator it = sectionInfo.section->References();
   ref_t currentMask = 0;
   ref_t currentRef = 0;
   while (!it.Eof()) {
      currentMask = it.key() & mskAnyRef;
      currentRef = it.key() & ~mskAnyRef;

      if (currentMask == mskLinkerConstant) {
         (*image)[*it + position] = _helper->getLinkerConstant(currentRef);
      }
      else if (currentMask == 0) {
         (*image)[*it + position] = resolveMessageID(sectionInfo.module, currentRef);
      }
      else {
         void* refVAddress = load(_helper->retrieveReference(sectionInfo.module, currentRef, currentMask), currentMask & ~mskRelativeRef, false);

         resolveReference(image, *it + position, (ref_t)refVAddress, currentMask, _virtualMode);
      }
      it++;
   }
   return vaddress;
}

void* ReferenceLoader :: loadBytecodeSection(const TCHAR*  reference, int mask, SectionInfo sectionInfo)
{
   if (sectionInfo.section == NULL)
      return LOADER_NOTLOADED;

   // get target image & resolve virtual address
   Section* image = _helper->getTargetSection(mask);
   SectionWriter writer(image);

   void* vaddress = calculateVAddress(&writer, mask & mskImageMask);
   size_t position = writer.Position();

   _helper->mapReference(reference, vaddress, mask);

   // create native debug info header if debug info enabled
   if (_withDebugInfo)
      createNativeSymbolDebugInfo(reference);

   // symbol just in time compilation
   References references(RefInfo(0, NULL));
   ReferenceHelper refHelper(this, sectionInfo.module, &references);
   DumpReader reader(sectionInfo.section);
   _compiler->compileSymbol(refHelper, reader, writer);

   // fix not loaded references
   fixReferences(references, image);

   return vaddress;
}

void* ReferenceLoader :: loadBytecodeVMTSection(const TCHAR*  reference, int mask, ClassSectionInfo sectionInfo)
{
   if (sectionInfo.codeSection == NULL || sectionInfo.vmtSection == NULL)
      return LOADER_NOTLOADED;

   // get target image & resolve virtual address
   Section* vmtImage = _helper->getTargetSection(mask);
   Section* codeImage = _helper->getTargetSection(mskClassRef);

   SectionWriter vmtWriter(vmtImage);   

   void* vaddress = calculateVAddress(&vmtWriter, mask & mskImageMask);

   _helper->mapReference(reference, vaddress, mask);

   // VMT just in time compilation
   DumpReader vmtReader(sectionInfo.vmtSection);
   // read tape record size
   size_t size = vmtReader.getDWord();

   // read VMT header
   ClassHeader header;
   vmtReader.read((void*)&header, sizeof(ClassHeader));

   bool withAnyHandler = test(header.flags, elVMTAnyHandler);

   // read VMT size + tailing terminator entry
   int vmtSize = vmtReader.getDWord() + sizeof(VMTEntry);
   // make sure it is enough place to store Any Handler VMT as well
   if (withAnyHandler)
      vmtSize += elAnyHandlerSize;

   // put VMT place holder
   size_t position = vmtWriter.Position();
   vmtWriter.writeBytes(0, vmtSize);
   size_t newpos = vmtWriter.Position();
   vmtWriter.seek(position);

   // load parent class
   void* parentVMT = getVMTReference(sectionInfo.module, header.parentRef);
   size_t count = _compiler->copyParentVMT(parentVMT, (VMTEntry*)vmtImage->get(position + elVMTOffset));

   // load role table / role owner class
   if (header.roleRef != 0) {
      // if it is role vmt, load role parent
	  if (test(header.flags, elRoleVMT)) {
         header.roleRef = (ref_t)load(_helper->retrieveReference(sectionInfo.module, header.roleRef, mskVMTRef), mskVMTRef, false);
      }
	  // else load role table
	  else header.roleRef = (ref_t)load(_helper->retrieveReference(sectionInfo.module, header.roleRef, mskNativeDataRef), mskNativeDataRef, false);
   }

   // create native debug info header if debug info enabled
   if (_withDebugInfo)
      createNativeClassDebugInfo(reference, vaddress);

   // read and compile VMT entries
   SectionWriter   codeWriter(codeImage);
   DumpReader      codeReader(sectionInfo.codeSection);

   References      references(RefInfo(0, NULL));
   ReferenceHelper refHelper(this, sectionInfo.module, &references);
   int             number = (size - sizeof(ClassHeader) - 4) >> 3;
   size_t          methodPosition;
   VMTEntry        entry;
   for (int i = 0 ; i < number ; i++) {
      vmtReader.read((void*)&entry, sizeof(VMTEntry));

      codeReader.seek(entry.address);
      methodPosition = loadMethod(refHelper, codeReader, codeWriter);

      _compiler->addVMTEntry(refHelper, entry.messageID, methodPosition, (VMTEntry*)vmtImage->get(position + elVMTOffset), count);
   }

   // arrange VMT
   _compiler->compileVMT(vaddress, vmtWriter, header, count, _virtualMode);

   // fix not loaded references
   fixReferences(references, codeImage);

   return vaddress;
}

void* ReferenceLoader :: loadConstant(const TCHAR* reference, int mask)
{
   // get target image & resolve virtual address
   Section* image = _helper->getTargetSection(mskDataRef);
   SectionWriter writer(image);

   bool valueConstant = false;
   void* vaddress = calculateVAddress(&writer, mskDataRef);

   _helper->mapReference(reference, vaddress, mask);

   // create constant object place holder
   size_t position = writer.Position() + 4;
   writer.writeBytes(0, elEmptyObject);
   (*image)[position] = elVMTOffset;

   if (mask == mskLiteralRef) {
      writer.writeDWord(getlength(reference));
      writer.writeLiteral(reference, getlength(reference) + 1);
      writer.align(4, 0);

      reference = _helper->getLiteralClass();

      valueConstant = true;
   }
   else if (mask == mskInt32Ref) {
      int integer = _ttoi(reference);      

      writer.writeDWord(integer);
      writer.align(4, 0);

      reference = _helper->getIntegerClass();

      valueConstant = true;
   }
   else if (mask == mskRealRef) {
      double number = _tcstod(reference, NULL);

      writer.write((char*)&number, 8);
      writer.align(4, 0);

      reference = _helper->getRealClass();

      valueConstant = true;
   }

   // get constant VMT reference
   void* vmtVAddress = load(reference, mskVMTRef, false);

   // check if the class could be constant one
   if (!valueConstant) {
      // read VMT flags
      Section* image = _helper->getTargetSection(mskVMTRef);
      int flags = (*image)[reallocateReference((ref_t)vmtVAddress) + 4];

      if (!test(flags, elStateless)) 
         throw JITUnresolvedException(reference);
   }
   
   // fix object VMT reference
   resolveReference(image, position, (ref_t)vmtVAddress, mskVMTRef, _virtualMode);

   return vaddress;
}

void* ReferenceLoader :: loadStaticVariable(const TCHAR*  reference, int mask)
{
   // get target image & resolve virtual address
   Section* image = _helper->getTargetSection(mskStaticRef);
   SectionWriter writer(image);

   size_t vaddress = (_virtualMode ? writer.Position() : (size_t)writer.Address()) | mskStaticRef;

   _helper->mapReference(reference, (void*)vaddress, mskNativeStaticRef);
   writer.writeDWord(0);

   return (void*)vaddress;
}

void ReferenceLoader :: createNativeSymbolDebugInfo(const TCHAR* reference)
{
   Section* debug = _helper->getTargetDebugSection();

   DumpWriter writer(debug);
   // start with # to distinguish the symbol debug info from the class one
   writer.writeChar(_T('#'));
   writer.writeLiteral(reference);
}

void ReferenceLoader :: createNativeClassDebugInfo(const TCHAR* reference, void* vaddress)
{
   Section* debug = _helper->getTargetDebugSection();

   SectionWriter writer(debug);
   writer.writeLiteral(reference);

   // save VMT address
   if (!_virtualMode) {
      ref_t address = reallocateReference((size_t)vaddress);

      writer.writeDWord(address + elVMTOffset);
   }
   else writer.writeRef((ref_t)vaddress, elVMTOffset);
}

void* ReferenceLoader :: load(const TCHAR* reference, int mask, bool silentMode)
{
   void* vaddress = _helper->resolveReference(reference, mask);
   if (vaddress==LOADER_NOTLOADED) {
      if (mask == mskNativeCodeRef || mask == mskNativeDataRef) {
         vaddress = loadNativeSection(reference, mask, _helper->getSectionInfo(reference, mask));
      }
      else if (mask == mskSymbolRef) {
         vaddress = loadBytecodeSection(reference, mask, _helper->getSectionInfo(reference, mask));         
      }
      else if (mask == mskVMTRef) {
         vaddress = loadBytecodeVMTSection(reference, mask, _helper->getClassSectionInfo(reference, mskClassRef, mskVMTRef));
      }
      else if (mask == mskConstantRef || mask == mskLiteralRef || mask == mskInt32Ref || mask == mskRealRef) {
         vaddress = loadConstant(reference, mask);
      }
      else if (mask == mskStaticConstRef) {
         vaddress = loadStaticVariable(reference, mask);
      }
   }
   if (!silentMode && vaddress == LOADER_NOTLOADED)
      throw JITUnresolvedException(reference);

   return vaddress;
}

void* ReferenceLoader :: loadNativeData(const TCHAR* reference, int size)
{
   // get target image & resolve virtual address
   Section* image = _helper->getTargetSection(mskNativeDataRef);
   SectionWriter writer(image);

   void* vaddress = calculateVAddress(&writer, mskDataRef);

   _helper->mapReference(reference, vaddress, mskNativeDataRef);
   writer.writeBytes(0, size);

   return vaddress;
}

void* ReferenceLoader :: loadPseudoVMT(const TCHAR* reference, void* codeAddress)
{
   // get target image & resolve virtual address
   Section* image = _helper->getTargetSection(mskVMTRef);
   SectionWriter writer(image);

   void* vaddress = calculateVAddress(&writer, mskDataRef);

   _helper->mapReference(reference, vaddress, mskVMTRef);

   _compiler->compilePseudoVMT(writer, codeAddress);

   return vaddress;
}

void ReferenceLoader :: preloadCoreCode(_Module* coreBinary)
{
    // load GC table
    //@current_stack_frame : +x00
    //@heap_pointer        : +x04
    //@yg_heap_pointer     : +x08
    //@mg_heap_pointer     : +x0C
    //@og_heap_pointer     : +x10
    //@static_size         : +x14
    //@gc_heap_end         : +x18

    //@mg_ygptr            : +x1C
    //@mg_ygptr_end        : +x20
    //@og_ygptr            : +x24
    //@og_ygptr_end        : +x28
    //@gc_flag             : +x2C
   void* gcTable = loadNativeData(GC_TABLE, 0x30);
   _compiler->addPreloadedReference(coreBinary->mapReference(GC_TABLE, true) | mskNativeDataRef, gcTable);

   // load GC stack root
   void* gcRoot =loadStaticVariable(GC_ROOT, mskStaticConstRef);
   _compiler->addPreloadedReference(coreBinary->mapReference(GC_ROOT, true) | mskNativeDataRef, gcRoot);

   // load GC allocation function
   void* allocFun = load(ALLOC_FUNCTION, mskNativeCodeRef, false);
   _compiler->addPreloadedReference(coreBinary->mapReference(ALLOC_FUNCTION, true) | mskNativeCodeRef, allocFun);

   void* nilRef = load(NIL_CLASS, mskConstantRef, false);
   _compiler->addPreloadedReference(coreBinary->mapReference(NIL_CLASS, nilRef) | mskConstantRef, nilRef);

   // load $elena'$group pseudo VMT
   void* groupRef = load(GROUP_FUNCTION, mskNativeCodeRef, false);

   loadPseudoVMT(GROUP_CLASS, groupRef);

   // load $elena'$cast pseudo VMT
   void* castRef = load(CAST_FUNCTION, mskNativeCodeRef, false);

   loadPseudoVMT(CAST_CLASS, castRef);
}
