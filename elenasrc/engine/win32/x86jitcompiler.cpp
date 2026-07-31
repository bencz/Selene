//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Compiler Engine
//
//		This file contains ELENA JIT linker class.
//		Supported platforms: x86
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#include "elena.h"
// --------------------------------------------------------------------------
#include "x86jitcompiler.h"
#include "bytecode.h"

using namespace _ELENA_;

// --- x86JITCompiler ---

inline void _ELENA_::copySection(x86JITScope& scope, x86JITInlineCode& code, _Module* module)
{
   // check if the code could contain references
   if (code.section != NULL) {
      size_t position = scope.code->Position();

      scope.code->write(code.code, code.length);

      // resolve section references
      _ELENA_::RelocationMap::Iterator it = code.section->References();
      while (!it.Eof()) {
         scope.code->seek(*it + position);

         // if argument1
         switch (it.key()) {
            case -1: // argument1 as a number
               scope.code->writeDWord(scope.argument1);
               break;
            case -2: // argument2 as a number
               scope.code->writeDWord(scope.argument2);
               break;
            case -3: // argument1 as a object reference
               scope.helper->writeReference(*scope.code, scope.argument1, elEmptyObject);
               break;
            case -4: // argument2 as a object reference
               scope.helper->writeReference(*scope.code, scope.argument2, elEmptyObject);
               break;
            case -5: // argument1 as a class reference
               scope.helper->writeReference(*scope.code, scope.argument1, elVMTOffset);
               break;
            case -6: // argument2 as a class reference
               scope.helper->writeReference(*scope.code, scope.argument2, elVMTOffset);
               break;
            case -7: // argument1 as a reference
               scope.helper->writeReference(*scope.code, scope.argument1, 0);
               break;
            case -8: // argument2 as a reference
               scope.helper->writeReference(*scope.code, scope.argument2, 0);
               break;
            default: 
               // if we embed the code try to resolve the reference
               if (module != NULL) {
                  scope.helper->writeReference(*scope.code, module, it.key(), (*code.section)[*it]);
               }
               else {
                  // otherwise references used in code should be already preloaded
                  if (test(it.key(), mskRelativeRef)) {
                     scope.helper->writeReference(*scope.code, 
                        scope.compiler->_preloaded.get(it.key() & ~mskRelativeRef), true, (*code.section)[*it]);
                  }
                  else scope.helper->writeReference(*scope.code, 
                     scope.compiler->_preloaded.get(it.key()), false, (*code.section)[*it]);
               }
               break;
         }
         it++;
      }
      scope.code->seekEOF();
   }
   // otherwise simply copy it
   else scope.code->write(code.code, code.length);
}

void compileJumpIfNot(x86JITScope& scope, int label, bool forwardJump, bool shortJump)
{
   // test eax, eax
   // jz   lbEnding
   scope.code->writeWord(0xC085);
   if (!forwardJump) {
      scope.lh.writeJxxBack(x86Helper::JUMP_TYPE_JZ, label);
   }
   else {
      // if it is forward jump, try to predict if it is short
      if (shortJump) {
         scope.lh.writeShortJxxForward(label, x86Helper::JUMP_TYPE_JZ);
      }
      else scope.lh.writeJxxForward(label, x86Helper::JUMP_TYPE_JZ);
   }
}

void compileJump(x86JITScope& scope, int label, bool forwardJump, bool shortJump)
{
   // jMP   lbEnding
   if (!forwardJump) {
      scope.lh.writeJmpBack(label);
   }
   else {
      // if it is forward jump, try to predict if it is short
      if (shortJump) {
         scope.lh.writeShortJmpForward(label);
      }
      else scope.lh.writeJmpForward(label);
   }
}

void _ELENA_::compileNop(int opcode, x86JITScope& scope)
{
   // nop command is used to indicate possible label
   // fix the label if it exists
   if (scope.lh.checkLabel(scope.tape->Position() - 1)) {
      scope.lh.fixLabel(scope.tape->Position() - 1);
   }
   // or add the label
   else scope.lh.setLabel(scope.tape->Position() - 1);
}

void _ELENA_::compilePrep(int opcode, x86JITScope& scope)
{
   if (opcode == bcSOperand) {
      copySection(scope, scope.compiler->_inlines[x86JITCompiler::cmdSPrepare], NULL);

      scope.prevFSPOffs = 8;
   }
   else if (opcode == bcSPrmOperand) {
      // oprepare
      // push ebx
      copySection(scope, scope.compiler->_inlines[x86JITCompiler::cmdSPrepare], NULL);
      scope.code->writeByte(0x53);

      scope.prevFSPOffs = 8;
   }
   else if (opcode == bcRedirOperand) {
      copySection(scope, scope.compiler->_inlines[x86JITCompiler::cmdPrepRedir], NULL);
      scope.code->writeByte(0x53);

      scope.prevFSPOffs = 0xC;
   }
   else if (opcode == 0) {
      copySection(scope, scope.compiler->_inlines[x86JITCompiler::cmdPrepare], NULL);

      scope.prevFSPOffs = 4;
   }
}

void _ELENA_::compilePush(int opcode, x86JITScope& scope)
{
   if(opcode == bcROperand) {
      // push reference      
      scope.code->writeByte(0x68);
      scope.helper->writeReference(*scope.code, scope.argument1, elEmptyObject);
   }
   else if (opcode == bcIOperand) {
      // push constant
      scope.code->writeByte(0x68);
      scope.code->writeDWord(scope.argument1);
   }
   else if (opcode == bcSOperand) {
      // push edi
      scope.code->writeByte(0x57);
   }
   else if (opcode == bcRPtrOperand) {
      // mov edx, reference
      // push [edx]
      scope.code->writeByte(0xBA);
      scope.helper->writeReference(*scope.code, scope.argument1, 0);
      scope.code->writeWord(0x32FF);
   }
   else if (opcode == bcIFOperand) {      
      scope.code->writeWord(0xB5FF);
      if (scope.argument1 < 0) {
         // push [ebp + prev_bsp - level * 4]         
         scope.code->writeDWord(scope.prevFSPOffs - (scope.argument1 << 2));
      }
      else {
         // push [ebp-level*4]
         scope.code->writeDWord(-(scope.argument1 << 2));
      }
   }
   else if (opcode == bcIOOperand) {
      // push [esp-offset]
      scope.code->writeWord(0xB4FF);
      scope.code->writeByte(0x24);
      scope.code->writeDWord(-(scope.argument1 << 2));
   }
   else if (opcode == bcISOperand) {
      // push [edi + (offset - 1) * 4]
      scope.code->writeWord(0xB7FF);
      scope.code->writeDWord((scope.argument1 - 1) << 2);
   }
}

void _ELENA_::compileReturn(int opcode, x86JITScope& scope)
{
   if (opcode == bcSOperand) {
      copySection(scope, scope.compiler->_inlines[x86JITCompiler::cmdSReturn], NULL);
   }
   else if (opcode == 0) {
      copySection(scope, scope.compiler->_inlines[x86JITCompiler::cmdReturn], NULL);
   }
}

inline void embed(SectionInfo info, x86JITScope& scope)
{
   x86JITInlineCode codeScope;

   codeScope.section = info.section;
   codeScope.code = info.section->getArray();
   codeScope.length = info.section->Length();

   copySection(scope, codeScope, info.module);
}

void _ELENA_::compileCall(int opcode, x86JITScope& scope)
{
   int jumpOffset = scope.argument2;
   if (opcode==bcREOperand) {
      // call symbol
      scope.code->writeByte(0xE8);
      scope.helper->writeReference(*scope.code, scope.argument1 | mskRelativeRef, 0);
   }
   else if (opcode==bcExtOperand) {
      scope.argument1 |= mskRelativeRef;

      // rcallext
      copySection(scope, scope.compiler->_inlines[x86JITCompiler::cmdCallExt], NULL);
   }
   else if (opcode==bcEmbOperand) {
      embed(scope.helper->getSection(scope.argument1), scope);
   }
   // compile alternative jump offset
   // try to use short jump if offset small (< 0x10?)
   compileJumpIfNot(scope, scope.tape->Position() + jumpOffset, true, (jumpOffset < 0x10)); 
}

void _ELENA_::compilePop(int opcode, x86JITScope& scope)
{
   // pop edx
   scope.code->writeByte(0x5A);
}

void _ELENA_::compileMove(int opcode, x86JITScope& scope)
{
   if (opcode == bcRPtrOperand) {
      // mov edx, [esp]
      // mov [ref], edx
      scope.code->writeWord(0x148B);
      scope.code->writeByte(0x24);
      scope.code->writeWord(0x1589);
      scope.helper->writeReference(*scope.code, scope.argument1, 0);
   }
   else if (opcode == bcIFOperand) {
      // mov edx, [esp]
      scope.code->writeWord(0x148B);
      scope.code->writeByte(0x24);

      if (scope.argument1 < 0) {
         // mov [ebp + prev_bsp - level * 4], edx
         x86Helper::movMR32disp(scope.code, x86Helper::otEBP, x86Helper::otEDX, scope.prevFSPOffs - (scope.argument1 << 2));
      }
      else {
         // mov [ebp-level*4], edx
         x86Helper::movMR32disp(scope.code, x86Helper::otEBP, x86Helper::otEDX, -(scope.argument1 << 2));
      }      
   }
   else if (opcode == bcIOOperand) {
      // mov edx, [esp]
      // mov [esp+level*4], edx

      scope.code->writeWord(0x148B);
      scope.code->writeByte(0x24);
      x86Helper::movMR32disp(scope.code, x86Helper::otESP, x86Helper::otEDX, scope.argument1 << 2);
   }
   else if (opcode == bcOPtrOperand) {
      // mov edx, [esp]
      // mov ebx, [esp+level*4]
      // mov [ebx+offset], edx

      scope.code->writeWord(0x148B);
      scope.code->writeByte(0x24);
      scope.code->writeWord(0x9C8B);
      scope.code->writeByte(0x24);
      scope.code->writeDWord(scope.argument1 << 2);
      scope.code->writeWord(0x9389);
      scope.code->writeDWord(scope.argument2);
   }
   else if (opcode == bcISOperand) {
      // mov edx, [esp]
      // mov [edi+(offset - 1) * 4], edx
      scope.code->writeWord(0x148B);
      scope.code->writeByte(0x24);
      scope.code->writeWord(0x9789);
      scope.code->writeDWord((scope.argument1 - 1) << 2);
   }
}

void _ELENA_::compileExit(int opcode, x86JITScope& scope)
{
   if (opcode == bcSOperand) {
      copySection(scope, scope.compiler->_inlines[x86JITCompiler::cmdSExit], NULL);
   }
   else if (opcode == bcRedirOperand) {
      copySection(scope, scope.compiler->_inlines[x86JITCompiler::cmdExitRedir], NULL);
   }
//   //else scope.code->write(scope.inlines[x86Exit].address, _entries[x86SReturn].size);
}

void _ELENA_::compileRedirect(int opcode, x86JITScope& scope)
{
   if (opcode == bcROperand) {
      copySection(scope, scope.compiler->_inlines[x86JITCompiler::cmdRRedirect], NULL);
   }
   else if (opcode == 0) {
      copySection(scope, scope.compiler->_inlines[x86JITCompiler::cmdRedirect], NULL);
   }
}

void _ELENA_::compileSet(int opcode, x86JITScope& scope)
{
   if (opcode == bcREOperand) {
      scope.argument2 |= mskVMTRef;

      copySection(scope, scope.compiler->_inlines[x86JITCompiler::cmdIOSet], NULL);
   }
   if (opcode == bcIFOperand) {
      // lea esp, [ebp - level * 4]

      x86Helper::leaRM32disp(                     // lea esp, [ebp-level*4]
         scope.code, x86Helper::otESP, x86Helper::otEBP, -(scope.argument1 << 2));
   }
}

void _ELENA_::compileReturnIf(int opcode, x86JITScope& scope)
{
   if (opcode == bcROperand) {
      copySection(scope, scope.compiler->_inlines[x86JITCompiler::cmdRReturnIf], NULL);
   }
}

void _ELENA_::compileIOCallN(int opcode, x86JITScope& scope)
{
   int jumpOffset = scope.argument2;
   if (opcode==bcION0) {
      // iocall(0)
      // specify initial vmt offset
      scope.argument2 = 0;
      copySection(scope, scope.compiler->_inlines[x86JITCompiler::cmdIOCallN], NULL);
   }
   else if (opcode==bcION1) {
      // iocall(8)
      // if the message is not predefined it should be resolved
      if (!test(scope.argument1, PREDEFINED_REF)) {
         scope.argument1 = scope.helper->resolveMessageID(scope.argument1);
      }
      // specify initial vmt offset
      scope.argument2 = 8;
      copySection(scope, scope.compiler->_inlines[x86JITCompiler::cmdIOCallN], NULL);
   }
   else if (opcode == bcIRN0 || opcode == bcIRN1) {
      // parent vmt reference is following the main command
      // mov eax, reference
      // ircall(0 | 8)
      scope.code->writeByte(0xB8);
      scope.helper->writeReference(*scope.code, scope.tape->getDWord(), elVMTOffset);

      // if the message is not predefined it should be resolved
      if (!test(scope.argument1, PREDEFINED_REF)) {
         scope.argument1 = scope.helper->resolveMessageID(scope.argument1);
      }
      // specify initial vmt offset
      scope.argument2 = (opcode == bcIRN0) ? 0 : 8;
      copySection(scope, scope.compiler->_inlines[x86JITCompiler::cmdIRCallN], NULL);
   }

   // iocall should include a virtual step, to cope with possible branching
   compileOthers(0x0C, scope);

   // compile alternative jump offset
   // try to use short jump if offset small (< 0x10?)
   compileJumpIfNot(scope, scope.tape->Position() + jumpOffset, true, (jumpOffset < 0x10)); 
}

void _ELENA_::compileCreate(x86JITScope& scope)
{
   // creating the object
   int fieldCount = scope.argument1;
   int size = fieldCount << 2;
   // if binary object (field count is negative in that case and contains the raw size)
   if (fieldCount < 0) {
      size = fieldCount & ~gcBinary;
      fieldCount = gcBinary | ((size + 3) >> 2);
   }
   scope.argument1 = fieldCount;
   scope.argument2 |= mskVMTRef;

   // mov  ecx, #gc_page + (size - 1)
   // ocreate

   scope.code->writeByte(0xB9);
   scope.code->writeDWord(align(size + elEmptyObject, gcPageSize));

   if (fieldCount > 0) {
      // try to use optimized code for small objects (<= 6 fields)
      if (fieldCount > 2) {
         if (fieldCount > 4) {
            if (fieldCount > 6) {
               copySection(scope, scope.compiler->_inlines[x86JITCompiler::cmdOCreate], NULL);
            }
            else copySection(scope, scope.compiler->_inlines[x86JITCompiler::cmdOCreate6], NULL);
         }
         else copySection(scope, scope.compiler->_inlines[x86JITCompiler::cmdOCreate4], NULL);
      }
      else copySection(scope, scope.compiler->_inlines[x86JITCompiler::cmdOCreate2], NULL);
   }
   else copySection(scope, scope.compiler->_inlines[x86JITCompiler::cmdOCreate0], NULL);
}

void _ELENA_::compileOthers(int opcode, x86JITScope& scope)
{
   // unshift
   if (opcode == 0) {
      copySection(scope, scope.compiler->_inlines[x86JITCompiler::cmdUnShift], NULL);
   }
   // if ijump
   else if (opcode == 2) {
      compileJump(scope, scope.tape->Position() + scope.argument1, (scope.argument1 > 0), (scope.argument1 < 0x10)); 
   }
   // if ocreate
   else if (opcode == 3) {
      compileCreate(scope);
   }
   else if (opcode == bcSwapOperand) {
      scope.argument1 = scope.argument1 << 2;

      copySection(scope, scope.compiler->_inlines[x86JITCompiler::cmdIOSWAP], NULL);
   }
   // breakpoint
   else if (opcode == 0x0C) {
      scope.helper->addBreakpoint(scope.code->Position());
   }
   // shift
   else if (opcode == bcRoleOperand) {
      scope.argument1 = scope.argument1 << 2;

      copySection(scope, scope.compiler->_inlines[x86JITCompiler::cmdShift], NULL);
   }
}

inline void loadInlineCode(x86JITInlineCode& inlineCode, const TCHAR* reference, _Module* binary, bool clearSection)
{
   inlineCode.section = binary->mapSection(binary->mapReference(reference, true) | mskNativeCodeRef, true);
   inlineCode.code = inlineCode.section->getArray();
   inlineCode.length = inlineCode.section->Length();

   // clear section to stop copySection from trying to check for the possible references
   if (clearSection)
      inlineCode.section = NULL;
}

x86JITCompiler :: x86JITCompiler(_Module* binary)
{
   // load inline sections used in compilation
   loadInlineCode(_inlines[cmdPrepare], PREP_FUNCTION, binary, true);
   loadInlineCode(_inlines[cmdSPrepare], SPREP_FUNCTION, binary, true);
   loadInlineCode(_inlines[cmdReturn], RETURN_FUNCTION, binary, true);
   loadInlineCode(_inlines[cmdIOCallN], IOCALLN_FUNCTION, binary, false);
   loadInlineCode(_inlines[cmdSExit], SEXIT_FUNCTION, binary, true);
   loadInlineCode(_inlines[cmdSReturn], SRETURN_FUNCTION, binary, true);
   loadInlineCode(_inlines[cmdRReturnIf], RRETURNIF_FUNCTION, binary, false);
   loadInlineCode(_inlines[cmdOCreate], OCREATE_FUNCTION, binary, false);
   loadInlineCode(_inlines[cmdOCreate2], OCREATE2_FUNCTION, binary, false);
   loadInlineCode(_inlines[cmdOCreate4], OCREATE4_FUNCTION, binary, false);
   loadInlineCode(_inlines[cmdOCreate6], OCREATE6_FUNCTION, binary, false);
   loadInlineCode(_inlines[cmdOCreate0], OCREATE0_FUNCTION, binary, false);
   loadInlineCode(_inlines[cmdCallExt], CALLEXT_FUNCTION, binary, false);
   loadInlineCode(_inlines[cmdPrepRedir], PREPREDIR_FUNCTION, binary, true);
   loadInlineCode(_inlines[cmdExitRedir], EXITREDIR_FUNCTION, binary, true);
   loadInlineCode(_inlines[cmdRedirect], REDIRECT_FUNCTION, binary, true);
   loadInlineCode(_inlines[cmdRRedirect], RREDIRECT_FUNCTION, binary, false);
   loadInlineCode(_inlines[cmdIOSWAP], IOSWAP_FUNCTION, binary, false);
   loadInlineCode(_inlines[cmdIOSet], IOSET_FUNCTION, binary, false);
   loadInlineCode(_inlines[cmdShift], SHIFT_FUNCTION, binary, false);
   loadInlineCode(_inlines[cmdUnShift], UNSHIFT_FUNCTION, binary, false);
   loadInlineCode(_inlines[cmdIRCallN], IRCALL_FUNCTION, binary, false);

   // load commands
   _commands[0x00] = &compileNop;
   _commands[0x01] = &compilePrep;
   _commands[0x02] = &compilePush;
   _commands[0x03] = &compileReturn;
   _commands[0x04] = &compileCall;
   _commands[0x05] = &compilePop;
   _commands[0x06] = &compileMove;
   _commands[0x07] = &compileExit;
   _commands[0x08] = &compileRedirect;
   _commands[0x09] = &compileSet;
   _commands[0x0A] = &compileNop;
   _commands[0x0B] = &compileReturnIf;
   _commands[0x0C] = &compileIOCallN;
   _commands[0x0D] = &compileNop;
   _commands[0x0E] = &compileNop;
   _commands[0x0F] = &compileOthers;
}

void x86JITCompiler :: addPreloadedReference(ref_t reference, void* address)
{
   _preloaded.add(reference, address);
}

void x86JITCompiler :: alignCode(SectionWriter* writer, int alignment, bool code)
{
   writer->align(VA_ALIGNMENT, code ? 0x90 : 0x00);
}

void x86JITCompiler :: compileMethod(_ReferenceHelper& helper, StreamReader& tapeReader, SectionWriter& codeWriter)
{
   x86JITScope scope(&tapeReader, &codeWriter, &helper, this);

   size_t codeSize = tapeReader.getDWord();
   size_t endPos = tapeReader.Position() + codeSize;

   unsigned char code = 0;
   while(tapeReader.Position() < endPos) {
      // read bytecode + arguments
      code = tapeReader.getByte();
      // if command has an argument
      if ((code & 0x3) != 0) {         
         scope.argument1 = tapeReader.getDWord();
         // if command has second argument
         if (test(code, 0x3)) {
            scope.argument2 = tapeReader.getDWord();
         }
      }
   
      _commands[code >> 0x4](code & ~bcCommand, scope);
   }
}
