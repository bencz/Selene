//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Compiler Engine
//
//		This file contains ELENA JIT compiler class implementation.
//
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#include "elena.h"
// --------------------------------------------------------------------------
#include "jitcompiler.h"

using namespace _ELENA_;

inline void insertVMTEntry(VMTEntry* entries, int count, int index)
{
   for (int i = count ; i >= index ; i--) {
      entries[i+1] = entries[i];
   }
}

// --- _JITCompiler ---

void _JITCompiler :: compileSymbol(_ReferenceHelper& helper, StreamReader& reader, SectionWriter& codeWriter)
{
   compileMethod(helper, reader, codeWriter);
}

void _JITCompiler :: compilePseudoVMT(SectionWriter& vmtWriter, void* vaddress)
{
   // VMT dummy header
   vmtWriter.writeDWord(0);
   vmtWriter.writeDWord(elVMTAnyHandler);						
   vmtWriter.writeDWord(0);

   // VMT Any handler
   // there should be two similar entries for any handler (due to iocall(n) implementation)
   vmtWriter.writeDWord(0);
   vmtWriter.writeRef((ref_t)vaddress, 0);
   vmtWriter.writeDWord(0);
   vmtWriter.writeRef((ref_t)vaddress, 0);
}

int _JITCompiler :: copyParentVMT(void* parentVMT, VMTEntry* entries)
{
   if (parentVMT != NULL) {
      // get the parent entry array
      VMTEntry* parentEntries = (VMTEntry*)((ref_t)parentVMT + elVMTOffset);      

      // copy parent VMT
      size_t parentEntryNumber = 0;
      while (parentEntries[parentEntryNumber].messageID != TERMINAL_MESSAGE_ID) {
         entries[parentEntryNumber] = parentEntries[parentEntryNumber];

         parentEntryNumber++;
      }

      // copy parent Any handler entry
      int parentFlag = *(int*)((size_t)parentVMT + 4);
      if (test(parentFlag, elVMTAnyHandler)) {
         parentEntryNumber++;

         // locate any handler entry (at the bottom of VMT), skipping header
         VMTEntry* anyHandlerEntry = (VMTEntry*)((size_t)parentEntries + (parentEntryNumber << 3));

         entries[parentEntryNumber - 1] = *anyHandlerEntry;
         // replace message id to garantee any handler method is the last in VMT
         entries[parentEntryNumber - 1].messageID = TERMINAL_MESSAGE_ID;
      }
      return parentEntryNumber;
   }
   else return 0;
}

void _JITCompiler :: addVMTEntry(_ReferenceHelper& helper, int messageID, size_t codePosition, VMTEntry* entries, size_t& entryCount)
{
   size_t index = 0;

   // if any handler method
   if (messageID == 0) {
      // replace message id to garantee any handler method is the last in VMT
      messageID = TERMINAL_MESSAGE_ID;
   }      
   // if not predefined message define message id
   else if (!test(messageID, PREDEFINED_REF)) {
      messageID = helper.resolveMessageID(messageID);
   }

   // find the message entry
   while (index < entryCount && (entries[index].messageID < messageID))
      index++;

   if(index < entryCount) {
      if (entries[index].messageID != messageID) {
         insertVMTEntry(entries, entryCount, index);
         entryCount++;
      }
   }
   else entryCount++;

   entries[index].messageID = messageID;
   entries[index].address = codePosition;
}

void _JITCompiler :: compileVMT(void* vaddress, SectionWriter& vmtWriter, ClassHeader& header, int count, bool virtualMode)
{
   ref_t position = vmtWriter.Position();

   Section* image = vmtWriter.getSection();
   VMTEntry* entries = (VMTEntry*)image->get(position + elVMTOffset);

   // create VMT header
   if (test(header.flags, elVMTWithRoles)) {
      vmtWriter.writeRef(header.roleRef, 0);             // role table
   }
   else vmtWriter.writeDWord(0);

   vmtWriter.writeDWord(header.flags);						    // vmt flags

   // if any hanlder entry - save reference to any handler entry, 0 otherwise
   if (test(header.flags, elVMTAnyHandler)) {
      // any handler should be located right after terminator entry
      ref_t anyHandlerPos = position + elVMTOffset + (count << 3);

      vmtWriter.writeRef((ref_t)vaddress, anyHandlerPos - vmtWriter.Position() + 8);

      // replace back any handler message id to zero
      entries[count - 1].messageID = 0;

      // there should be two similar entries for any handler (due to iocall(n) implementation)
      entries[count] = entries[count - 1];      

      // reserve place for VMT terminator entry
      insertVMTEntry(entries, count, count - 1);

      // if in a virtual mode mark method address as reference
      if (virtualMode) {
         // there are two similar entries for any handler (due to iocall(n) implementation)
         image->addReference(mskCodeRef, anyHandlerPos + 4);
         image->addReference(mskCodeRef, anyHandlerPos + 12);
      }

      // exclude any handler from VMT processing
      count--;
   }
   // role vmt should refer to the owner class vmt
   else if (test(header.flags, elRoleVMT)) {
      vmtWriter.writeRef(header.roleRef, elVMTOffset);             
   }
   // for normal vmt, parent is not specified
   else vmtWriter.writeDWord(0);

   // if in virtual mode mark method addresses as reference
   if (virtualMode) {
      ref_t entryPosition = vmtWriter.Position();
      for (int i = 0 ; i < count ; i++) {
         image->addReference(mskCodeRef, entryPosition + 4);
      
         entryPosition += 8;
      }
   }

   // add VMT terminator entry
   vmtWriter.seek(vmtWriter.Position() + (count << 3));
   vmtWriter.writeDWord(TERMINAL_MESSAGE_ID);
   vmtWriter.writeDWord(0);
}
