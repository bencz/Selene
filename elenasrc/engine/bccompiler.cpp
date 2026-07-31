//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Compiler Engine
//
//		This file contains ELENA byte code compiler class implementation.
//
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#include "elena.h"
// --------------------------------------------------------------------------
#include "bccompiler.h"

using namespace _ELENA_;

// --- Auxiliary structures ---

struct JumpInfo
{
   int    level;
   size_t position;
   bool   withExtraParam;

   JumpInfo()
   {
      level = -1;
   }
   JumpInfo(int level, size_t position)
   {
      this->level = level;
      this->position = position;
      this->withExtraParam = false;
   }
   JumpInfo(int level, size_t position, bool withExtraParam)
   {
      this->level = level;
      this->position = position;
      this->withExtraParam = withExtraParam;
   }
};

// --- Auxiliary functions ---

void fixJumps(Section* code, int labelPosition, Stack<JumpInfo>& stack, int level)
{
   if (stack.Count() > 0) {
      while (stack.peek().level >= level) {
         JumpInfo jump = stack.pop();
         if (jump.level == level) {
            // position starts after the offset (and an extra parameter if any)
            code->patchU32LE(jump.position,
               (unsigned int)(labelPosition - jump.position - (jump.withExtraParam ? 8 : 4)));
         }
      }
   }
}

// --- ByteCodeCompiler ---

void ByteCodeCompiler :: openClassDebugInfo(_Module* debugModule, DumpWriter* debug, DumpWriter* debugStrings, const TCHAR* className, int flags)
{
   // put place holder if debug section is empty
   if (debug->Position() == 0)
   {
      debug->writeDWord(0);
   }

   LocalString<IDENTIFIER_LEN + 1> bookmark(className);
   debugModule->mapPredefinedReference(bookmark, debug->Position());

   ref_t position = debugStrings->Position();

   debugStrings->writeLiteral(className);

   DebugLineInfo symbolInfo(dsClass, 0, 0, 0);
   symbolInfo.addresses.symbol.nameRef = position;
   symbolInfo.addresses.symbol.flags = flags;

   debug->write((void*)&symbolInfo, sizeof(DebugLineInfo));
}

void ByteCodeCompiler :: openSymbolDebugInfo(_Module* debugModule, DumpWriter* debug, DumpWriter* debugStrings, const TCHAR* symbolName)
{
   // put place holder if debug section is empty
   if (debug->Position() == 0)
   {
      debug->writeDWord(0);
   }

   // map symbol debug info, starting the symbol with # to distinsuish from class
   LocalString<IDENTIFIER_LEN + 1> bookmark(_T("#"), symbolName);
   debugModule->mapPredefinedReference(bookmark, debug->Position());

   ref_t position = debugStrings->Position();

   debugStrings->writeLiteral(symbolName);

   DebugLineInfo symbolInfo(dsSymbol, 0, 0, 0);
   symbolInfo.addresses.symbol.nameRef = position;

   debug->write((void*)&symbolInfo, sizeof(DebugLineInfo));
}

void ByteCodeCompiler :: openProcedureDebugInfo(DumpWriter* debug)
{
   DebugLineInfo symbolInfo(dsProcedure, 0, 0, 0);

   debug->write((void*)&symbolInfo, sizeof(DebugLineInfo));
}

void ByteCodeCompiler :: writeFieldDebugInfo(ClassInfo::FieldMap& fields, DumpWriter* writer, DumpWriter* debugStrings)
{
   ClassInfo::FieldMap::Iterator it = fields.start();
   while (!it.Eof()) {
      DebugLineInfo symbolInfo(dsField, 0, 0, 0);

      symbolInfo.addresses.symbol.nameRef = debugStrings->Position();
      debugStrings->writeLiteral(it.key());

      writer->write((void*)&symbolInfo, sizeof(DebugLineInfo));

      it++;
   }
}

void ByteCodeCompiler :: writeLocal(ByteCodeIterator& it, DumpWriter* debug, DumpWriter* debugStrings)
{
   DebugLineInfo info;

   const TCHAR* localName = (TCHAR*)(*it).argument;
   // if self variable
   if (compstr(localName, SELF_VAR)) {
      info.symbol = dsBase;
      info.addresses.local.level = (*it).Hint();
   }
   else {
      info.symbol = dsLocal;
      info.addresses.local.nameRef = debugStrings->Position();
      info.addresses.local.level = (*it).Hint();

      debugStrings->writeLiteral(localName);
   }
   debug->write((char*)&info, sizeof(DebugLineInfo));
}

void ByteCodeCompiler :: writeBreakpoint(ByteCodeIterator& it, DumpWriter* debug)
{
   // reading breakpoint coordinate
   DebugLineInfo info;

   info.row = (*it).argument - 1;
   info.col = 0;
   info.length = 0;
   info.symbol = (DebugSymbol)(*it).Hint();

   it++;
   // it should be followed by coordinates
   if(*it == bdBreakCoord) {
      info.col = (*it).argument;
      info.length = (*it).Hint();
   }
   else it--;

   // saving breakpoint
   debug->write((char*)&info, sizeof(DebugLineInfo));
}

void ByteCodeCompiler :: endDebugInfo(DumpWriter* debug)
{
   DebugLineInfo symbolInfo(dsEnd, 0, 0, 0);

   debug->write((void*)&symbolInfo, sizeof(DebugLineInfo));
}

void ByteCodeCompiler :: newBreakpoint(CommandTape& tape, int row, int disp, int length, int stepType)
{
   tape.write(bcDebug);
   tape.write(bdBreakpoint, row, stepType);
   tape.write(bdBreakCoord, disp, length);
}

void ByteCodeCompiler :: newDummyBreakpoint(CommandTape& tape, int stepType)
{
   tape.write(bdBreakpoint, 0, stepType);
}

void ByteCodeCompiler :: newLocalInfo(CommandTape& tape, const TCHAR* localName, int level)
{
   tape.write(bdLocal, (ref_t)localName, level);
}

void ByteCodeCompiler :: newStaticSymbol(CommandTape& tape, ref_t staticReference, ref_t nilReference)
{
   // symbol-begin:
   //   rpushptr static
   //   rreturnif nil
   //   pop
   //   prep

   tape.write(blBegin, 0, bltSymbol);
   tape.write(bcRPushPtr, staticReference | mskStaticConstRef);
   tape.write(bcRReturnIf, nilReference | mskConstantRef);
   tape.write(bcPop);
   tape.write(bcPrep);
}

void ByteCodeCompiler :: newSymbol(CommandTape& tape)
{
   // symbol-begin:
   //   prep
   tape.write(blBegin, 0, bltSymbol);
   tape.write(bcPrep);
}

void ByteCodeCompiler :: newClass(CommandTape& tape)
{
   // class-begin:
   tape.write(blBegin, 0, bltClass);
}

void ByteCodeCompiler :: newRoleTable(CommandTape& tape, ref_t reference)
{
   tape.write(blBegin, reference, bltRole);
}

void ByteCodeCompiler :: newRole(CommandTape& tape, ref_t reference)
{
   tape.write(blDeclare, reference, bltRole);
}

void ByteCodeCompiler :: newMethod(CommandTape& tape, ref_t messageRef, bool withParam)
{
   // method-begin:
   //   oprepparam | oprep
   tape.write(blBegin, messageRef, bltMethod);
   tape.write(withParam ? bcSPrepParam : bcSPrep);
   if (withParam)
      tape.write(bcAllocStack, 1);
}

void ByteCodeCompiler :: idleMethod(CommandTape& tape, ref_t messageRef)
{
   // !! temporal, method should not contains any code
   tape.write(blBegin, messageRef, bltMethod);
   tape.write(bcSPrep);
   tape.write(bcSExit);
   tape.write(blEnd, 0, bltProc);
}

void ByteCodeCompiler :: newRedirectMethod(CommandTape& tape, ref_t messageRef)
{
   // method-begin:
   //   prepredir
   tape.write(blBegin, messageRef, bltMethod);
   tape.write(bcPrepRedir);
}

void ByteCodeCompiler :: newLocal(CommandTape& tape)
{
   // ipush 0
   tape.write(bcIPush, 0);
   tape.write(bcAllocStack, 1);
}

void ByteCodeCompiler :: newBranchStatement(CommandTape& tape)
{
   // branch-begin:
   //   iopush 0

   tape.write(blBegin, 0, bltBranch);
   tape.write(bcIOPush);
   tape.write(bcAllocStack, 1);
}

void ByteCodeCompiler :: newBranch(CommandTape& tape)
{
   // branch-begin:
   //   iopush 0

   tape.write(blBegin, 0, bltBranch);
   tape.write(bcIOPush);
   tape.write(bcAllocStack, 1);
}

void ByteCodeCompiler :: newAlternativeBranch(CommandTape& tape)
{
  //    iomove 1
  //    ifset  branch-level
   //   jump branch-end
   // branch-failing:
   //   ifset  branch-level
   //   iopush 0
   tape.write(bcIOMove, 1);
   tape.write(bcIFSet, 0, bltBranch);
   tape.write(bcIJump, 0, bltBranch);
   tape.write(blFailure, 0, bltBranch);
   tape.write(bcIFSet, 0, bltBranch);
   tape.write(bcIOPush);
   tape.write(bcAllocStack, 1);
}

void ByteCodeCompiler :: newAlternativeBranchStatement(CommandTape& tape)
{
   //   jump branch-end
   // branch-failing:
   //   ifset  branch-level
   //   iopush 0
   tape.write(bcIJump, 0, bltBranch);
   tape.write(blFailure, 0, bltBranch);
   tape.write(bcIFSet, 0, bltBranch);
   tape.write(bcIOPush);
   tape.write(bcAllocStack, 1);
}

void ByteCodeCompiler :: newLoop(CommandTape& tape)
{
   tape.write(blBegin, 0, bltLoop);
}

void ByteCodeCompiler :: newSubCode(CommandTape& tape)
{
   //   ifset  branch-level
   tape.write(bcIFSet, 0, bltBranch);
}

void ByteCodeCompiler :: pushNewObject(CommandTape& tape, int size, ref_t reference)
{
   // ocreate size, reference
   tape.write(bcOCreate, size, reference);
   tape.write(bcAllocStack, 1);
}

void ByteCodeCompiler :: pushNewObject(CommandTape& tape, int size, ref_t reference, ref_t constantRef)
{
   // ocreate size, reference
   tape.write(bcOCreate, size, reference);
   tape.write(bcIOSet, 0, constantRef);
   tape.write(bcAllocStack, 1);
}

void ByteCodeCompiler :: pushObject(CommandTape& tape, ObjectInfo object)
{
   switch (object.type) {
      case otConstant:
         // rpush reference
         tape.write(bcRPush, object.reference | mskConstantRef);
         break;
      case otLiteral:
         // rpush reference
         tape.write(bcRPush, object.reference | mskLiteralRef);
         break;
      case otInteger:
         // rpush reference
         tape.write(bcRPush, object.reference | mskInt32Ref);
         break;
      case otReal:
         // rpush reference
         tape.write(bcRPush, object.reference | mskRealRef);
         break;
      case otLocal:
         // ifpush offset
         tape.write(bcIFPush, object.reference);
         break;
      case otVSelf:
         // ifpush -1  ; -1 means the vself variable at the top of previous stack frame
         tape.write(bcIFPush, -1);
         break;
	  case otSymbolParam:
         // ifpush -1  ; -1 means the variable at the top of previous stack frame
         tape.write(bcIFPush, -1);
         break;
      case otSelf:
      case otSuper:
         // opush
         tape.write(bcSPush);
         break;
      case otField:
      case otOuter:
         // iopush offset
         tape.write(bcISPush, object.reference);
         break;
   }
   tape.write(bcAllocStack, 1);
}

void ByteCodeCompiler :: pushSymbol(CommandTape& tape, ref_t symbolReference, ref_t nilReference, bool branchMode)
{
   // rpush nil
   // rcall symbol, <ending>
   tape.write(bcRPush, nilReference | mskConstantRef);
   tape.write(bcRCall, symbolReference | mskSymbolRef, branchMode ? bltBranch : bltProc);
   tape.write(bcAllocStack, 1);
}

void ByteCodeCompiler :: pushProperty(CommandTape& tape, ref_t symbolReference, bool branchMode)
{
   // rcall symbol, <ending>
   tape.write(bcRCall, symbolReference | mskSymbolRef, branchMode ? bltBranch : bltProc);
}

void ByteCodeCompiler :: pushCurrent(CommandTape& tape)
{
	tape.write(bcIOPush);
   tape.write(bcAllocStack, 1);
}

void ByteCodeCompiler :: swap(CommandTape& tape, int offset)
{
   tape.write(bcIOSwap, offset);
}

void ByteCodeCompiler :: shift(CommandTape& tape, int offset)
{
   tape.write(bcShift, offset);
}

void ByteCodeCompiler :: unshift(CommandTape& tape)
{
   tape.write(bcUnShift);
}

void ByteCodeCompiler :: sendMessage(CommandTape& tape, ref_t messageRef, bool branchMode)
{
   if (test(messageRef, PREDEFINED_REF)) {
      // if message is predefined, where n = 0, for new message and n = 1 for others
      // rocall(n) message, <procedure-ending> | <ending>
      switch(messageRef) {
         case NEW_MESSAGE_ID:
            tape.write(bcIOCall0, messageRef, branchMode ? bltBranch : bltProc);
            break;
         default:
            tape.write(bcIOCall1, messageRef, branchMode ? bltBranch : bltProc);
      }
   }
   else {
      // iocall(1) message, <procedure-ending> | <ending>
      tape.write(bcIOCall1, messageRef, branchMode ? bltBranch : bltProc);
   }
   tape.write(bcFreeStack, 1);
}

void ByteCodeCompiler :: sendMessage(CommandTape& tape, ref_t messageRef, ref_t classRef, bool branchMode)
{
   if (test(messageRef, PREDEFINED_REF)) {
      // if message is predefined, where n = 0, for new message and n = 1 for others
      // rocall(n) message, <procedure-ending> | <ending>
      switch(messageRef) {
         case NEW_MESSAGE_ID:
            tape.write(bcIRCall0, messageRef, branchMode ? bltBranch : bltProc);
            break;
         default:
            tape.write(bcIRCall1, messageRef, branchMode ? bltBranch : bltProc);
      }
   }
   else {
      // iocall(1) message, <procedure-ending> | <ending>
      tape.write(bcIRCall1, messageRef, branchMode ? bltBranch : bltProc);
   }
   tape.write(bcExtraParam, classRef | mskVMTRef);
   tape.write(bcFreeStack, 1);
}

void ByteCodeCompiler :: callExternal(CommandTape& tape, ref_t functionReference, bool branchMode)
{
   // rcall symbol, <ending>
   tape.write(bcRCallExt, functionReference | mskNativeCodeRef, branchMode ? bltBranch : bltProc);
}

void ByteCodeCompiler :: callEmbedded(CommandTape& tape, ref_t functionReference, bool branchMode, int paramCount)
{
   // rcall symbol, <ending>
   tape.write(bcRCallEmb, functionReference | mskNativeCodeRef, branchMode ? bltBranch : bltProc);
   if (paramCount > 0)
      tape.write(bcFreeStack, paramCount);
}

void ByteCodeCompiler :: redirectMessage(CommandTape& tape)
{
   // redirect
   tape.write(bcRedirect, 0, bltProc);
}

void ByteCodeCompiler :: redirectMessageToParent(CommandTape& tape, ref_t parentRef)
{
   // rredirect ref
   tape.write(bcRRedirect, parentRef | mskVMTRef, bltProc);
}

void ByteCodeCompiler :: moveObject(CommandTape& tape, ObjectInfo object)
{
   switch (object.type) {
      case otLocal:
         tape.write(bcIFMove, object.reference);
         break;
      case otField:
         tape.write(bcISMove, object.reference);
         break;
   }
}

void ByteCodeCompiler :: moveToObjectPtr(CommandTape& tape, ObjectInfo object, int offset)
{
   if (object.type == otLocal) {
      tape.write(bcOMovePtr, object.reference, offset);
   }
}

void ByteCodeCompiler :: returnObject(CommandTape& tape)
{
   // oreturn
   tape. write(bcSReturn);
}

void ByteCodeCompiler :: endBranch(CommandTape& tape, bool branchMode)
{
  //  iomove 1
  //  ifset  branch-level
  //  jump branch-end
  // branch-failing:
  //  jump proc-failing | branch-failing
  // branch-end:  

   tape.write(bcIOMove, 1);
   tape.write(bcIFSet, 0, bltBranch);
   tape.write(bcIJump, 0, bltBranch);
   tape.write(blFailure, 0, bltBranch);
   tape.write(bcIJump, 0, branchMode ? bltBranch : bltProc);
   tape.write(blEnd, 0, bltBranch);
}

void ByteCodeCompiler :: endBranchStatement(CommandTape& tape, bool branchMode)
{
  // branch-failing:
  //  ifset  branch-level
  // branch-end:  

   tape.write(blFailure, 0, bltBranch);
   tape.write(bcIFSet, 0, bltBranch);
   tape.write(blEnd, 0, bltBranch);
}

void ByteCodeCompiler :: endLoop(CommandTape& tape)
{
   tape.write(bcIFSet, 0, bltLoop);
   tape.write(bcIJump, 0, bltLoop);
   tape.write(blFailure, 0, bltBranch);
   tape.write(bcIFSet, 0, bltLoop);
   tape.write(blEnd, 0, bltLoop);
}

void ByteCodeCompiler :: endStatement(CommandTape& tape)
{
   // pop
   tape.write(bcPop);
   tape.write(bcFreeStack, 1);
}

void ByteCodeCompiler :: endMethod(CommandTape& tape)
{
   //   sexit
   // proc-failure:
   //   ipush 0
   //   sreturn
   // end 

   tape.write(bcSExit);
   tape.write(blFailure, 0, bltProc);
   tape.write(bcIPush, 0);
   tape.write(bcSReturn);
   tape.write(blEnd, 0, bltProc);
}

void ByteCodeCompiler :: endRedirectMethod(CommandTape& tape)
{
   //   exitredir
   // end 

   tape.write(bcExitRedir);
   tape.write(blEnd, 0, bltProc);
}

void ByteCodeCompiler :: endSymbol(CommandTape& tape)
{
   //   return
   // proc-failing:
   //   ipush 0
   //   returnredir
   // symbol-end:
   tape.write(bcReturn);
   tape.write(blFailure, 0, bltProc);
   tape.write(bcIPush, 0);
   tape.write(bcReturn);
   tape.write(blEnd, 0, bltSymbol);
}

void ByteCodeCompiler :: endStaticSymbol(CommandTape& tape, ref_t staticReference)
{
   //   rmoveptr static
   //   return
   // proc-failing:
   //   ipush 0
   //   return
   // symbol-end:

   tape.write(bcRMovePtr, staticReference | mskStaticConstRef);
   tape.write(bcReturn);
   tape.write(blFailure, 0, bltProc);
   tape.write(bcIPush, 0);
   tape.write(bcReturn);
   tape.write(blEnd, 0, bltSymbol);
}

void ByteCodeCompiler :: endRoleTable(CommandTape& tape)
{
   tape.write(blEnd, 0, bltRole);
}

void ByteCodeCompiler :: endClass(CommandTape& tape)
{
   // end:
   tape.write(blEnd, 0, bltClass);
}

void ByteCodeCompiler :: saveProcedure(ByteCodeIterator& it, SectionWriter* code, DumpWriter* debug, DumpWriter* debugStrings)
{
   if (debug)
      openProcedureDebugInfo(debug);

   code->writeU32LE(0);                                // write size place holder
   size_t procPosition = code->Position();   

   Stack<int>      stackLevels;                       // scope stack levels
   Stack<JumpInfo> jumpsToFail(JumpInfo(-1, 0));      // !! maybe better to use memorystack?
   Stack<JumpInfo> jumpsToProcFail(JumpInfo(-1, 0));  // !! maybe better to use memorystack?
   Stack<JumpInfo> jumpsToEnd(JumpInfo(-1, 0));       // !! maybe better to use memorystack?
   Map<int, int>   jumpsToLoop;                       // !! maybe better to use memorystack?

   int level = 1;
   int stackLevel = 0;
   while (!it.Eof() && level > 0) {
      // save command
      switch (*it) {
         case bcAllocStack:
            stackLevel += (*it).argument;
            break;
         case bcFreeStack:
            stackLevel -= (*it).argument;
            break;
         case blBegin:
            stackLevels.push(stackLevel);
            level++;
            if ((*it).Hint() == bltLoop) {
               jumpsToLoop.add(level, code->Position());
               code->writeByte(bcNop);
            }
            break;
         case blFailure:
            // resolve label offsets
            if ((*it).Hint() == bltBranch) {
               fixJumps(code->getSection(), code->Position(), jumpsToFail, level);
            }
            else if ((*it).Hint() == bltProc) {
               fixJumps(code->getSection(), code->Position(), jumpsToProcFail, 1);
            }
            // JIT compiler interprets nop command as a label mark
            code->writeByte(bcNop);
            break;
         case blEnd:
            stackLevels.pop();
            if ((*it).Hint() == bltBranch) {
               // resolve label offsets
               fixJumps(code->getSection(), code->Position(), jumpsToEnd, level);
               // JIT compiler interprets nop command as a label mark
               code->writeByte(bcNop);
            }
            else if ((*it).Hint() == bltLoop) {
               jumpsToLoop.erase(level);
            }
            level--;
            break;
         case bdLocal:
            if (debug)
               writeLocal(it, debug, debugStrings);
            break;
         case bdBreakpoint:
            if (debug)
               writeBreakpoint(it, debug);
            break;
         case bcDebug:
            // generate debug exception only if debug info enabled
            if (debug)
               (*it).save(code);
            break;
         case bcIFSet:
            (*it).save(code, true);
            if ((*it).hint.type == bltBranch || (*it).hint.type == bltLoop) {
               stackLevel = stackLevels.peek();               
            }
            else stackLevel = (*it).argument;

            code->writeU32LE((unsigned int)stackLevel);
            break;
         case bcExtraParam:
            code->writeU32LE((unsigned int)(*it).argument);
            break;
         case bcRCall:
         case bcRCallExt:
         case bcRCallEmb:
         case bcIOCall0:
         case bcIOCall1:
         case bcIRCall0:
         case bcIRCall1:
            (*it).save(code);
            // save alternative jump offset
            if ((*it).hint.type == bltProc) {
               jumpsToProcFail.push(JumpInfo(1, code->Position(), (*it == bcIRCall0 || *it == bcIRCall1)));
            }
            else if ((*it).hint.type == bltBranch) {
               jumpsToFail.push(JumpInfo(level, code->Position()));
            }
            // put jump offset place holder
            code->writeU32LE(0);
            break;
         case bcIJump:
            (*it).save(code, true);
            // write offset to the label if it is back jump 
            if ((*it).Hint() == bltLoop) {
               code->writeU32LE((unsigned int)(jumpsToLoop.get(level) - code->Position() - 4));
            }
            // otherwise put place holder
            else {
               if ((*it).Hint() == bltBranch) {
                  jumpsToEnd.push(JumpInfo(level, code->Position()));
               }
               else {
                  jumpsToProcFail.push(JumpInfo(1, code->Position()));
               }
               // put jump offset place holder
               code->writeU32LE(0);
            }
            break;
         case bcOMovePtr:
         case bcOCreate:
         case bcIOSet:
            (*it).save(code);
            code->writeU32LE((unsigned int)(*it).Hint());
            break;
         default:
            (*it).save(code);
            break;
      }
      if (level == 0)
         break;
      it++;
   }
   // save the real procedure size
   code->getSection()->patchU32LE(procPosition - 4,
      (unsigned int)(code->Position() - procPosition));

   // add debug end line info
   if (debug)
      endDebugInfo(debug);
}

void ByteCodeCompiler :: saveSymbol(ref_t reference, ByteCodeIterator& it, _Module* module, _Module* debugModule)
{
   // initialize bytecode writer
   SectionWriter codeWriter(module->mapSection(reference | mskSymbolRef, false));

   // create debug info if debugModule available
   if (debugModule) {
      // initialize debug info writer
      DumpWriter debugWriter(debugModule->mapSection(DEBUG_LINEINFO_ID, false));
      DumpWriter debugStringWriter(debugModule->mapSection(DEBUG_STRINGS_ID, false));

      // save symbol debug line info
      openSymbolDebugInfo(debugModule, &debugWriter, &debugStringWriter, module->resolveReference(reference & ~mskAnyRef));

      saveProcedure(it, &codeWriter, &debugWriter, &debugStringWriter);

      endDebugInfo(&debugWriter);
   }
   else saveProcedure(it, &codeWriter);
}

void ByteCodeCompiler :: saveRoleTable(ByteCodeIterator& it, ref_t reference, _Module* module)
{
   // initialize role table writer
   SectionWriter writer(module->mapSection(reference | mskNativeDataRef, false));

   // create table
   while (*it != blEnd) {
      if (*it == blDeclare && (*it).Hint()==bltRole) 
         writer.writeRef((*it).argument | mskVMTRef, elVMTOffset);

      it++;
   }
}

void ByteCodeCompiler :: saveVMT(size_t classPosition, SectionWriter* vmtWriter, SectionWriter* codeWriter, ByteCodeIterator& it, DumpWriter* debug, DumpWriter* debugStrings)
{
   while (!it.Eof() && (*it) != blEnd) {
      switch (*it)
      {
         case blBegin:
            // create VMT entry
            if ((*it).Hint() == bltMethod) {
               vmtWriter->writeU32LE((unsigned int)(*it).argument);      // Message ID
               vmtWriter->writeU32LE((unsigned int)codeWriter->Position()); // Method Address

               saveProcedure(++it, codeWriter, debug, debugStrings);
            }
            break;
      };
      it++;
   }
   // save the real procedure size
   vmtWriter->getSection()->patchU32LE(classPosition - 4,
         (unsigned int)(vmtWriter->Position() - classPosition));
}

void ByteCodeCompiler :: saveClass(ref_t reference, ByteCodeIterator& it, _Module* module, _Module* debugModule)
{
   // initialize bytecode writer
   SectionWriter codeWriter(module->mapSection(reference | mskClassRef, false));

   // initialize vmt section writers
   SectionWriter vmtWriter(module->mapSection(reference | mskVMTRef, false));

   vmtWriter.writeU32LE(0);                              // save size place holder
   size_t classPosition = vmtWriter.Position();   

   // copy class meta data header + vmt size
   DumpReader reader(module->mapSection(reference | mskMetaDataRef, true));
   ClassInfo info;
   info.load(&reader, false);

   vmtWriter.write((void*)&info.header, sizeof(ClassHeader));
   vmtWriter.writeU32LE((unsigned int)info.classSize);

   // create debug info if debugModule available
   if (debugModule) {
      DumpWriter debugWriter(debugModule->mapSection(DEBUG_LINEINFO_ID, false));
      DumpWriter debugStringWriter(debugModule->mapSection(DEBUG_STRINGS_ID, false));

     // save class debug info
      openClassDebugInfo(debugModule, &debugWriter, &debugStringWriter, module->resolveReference(reference & ~mskAnyRef), info.header.flags);
      writeFieldDebugInfo(info.fields, &debugWriter, &debugStringWriter);

     // save class role table
      if (*it == blBegin && (*it).Hint() == bltRole) {
         saveRoleTable(it, info.header.roleRef, module);
         it++;
      }

      saveVMT(classPosition, &vmtWriter, &codeWriter, it, &debugWriter, &debugStringWriter);

      endDebugInfo(&debugWriter);
   }
   else saveVMT(classPosition, &vmtWriter, &codeWriter, it, NULL, NULL);
}

void ByteCodeCompiler :: save(ref_t reference, CommandTape& tape, _Module* module, _Module* debugModule)
{
   ByteCodeIterator it = tape.start();
   while (!it.Eof()) {
      if (*it == blBegin) {
         if ((*it).Hint() == bltClass) {
            saveClass(reference, ++it, module, debugModule);
         }
         else if ((*it).Hint() == bltSymbol) {
            saveSymbol(reference, ++it, module, debugModule);
         }
      }
      it++;
   }
}
