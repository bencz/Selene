//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Compiler Engine
//
//		This file contains ELENA byte code compiler class.
//
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef bccompilerH
#define bccompilerH 1

#include "bytecode.h"

namespace _ELENA_
{

// --- ObjectType ---
enum ObjectType
{
   otUnknown,
   otProperty,
   otSymbol,
   otConstant,
   otVSelf,
   otSelf,
   otSuper,
   otField,
   otLocal,
   otLiteral,
   otInteger,
   otReal,
   otSymbolParam,
   otOuter,
   otRole,
   otExpression
};

// --- ObjectInfo ---
struct ObjectInfo
{
   ObjectType type;
   ref_t      reference;

   ObjectInfo()
   {
      type = otUnknown;
      reference = 0;
   }
   ObjectInfo(ObjectType type, ref_t reference)
   {
      this->type = type;
      this->reference = reference;
   }
};

// --- ByteCodeCompiler class ---
class ByteCodeCompiler
{
   void openClassDebugInfo(_Module* debugModule, DumpWriter* debug, DumpWriter* debugStrings, const TCHAR* className, int flags);
   void openSymbolDebugInfo(_Module* debugModule, DumpWriter* debug, DumpWriter* debugStrings, const TCHAR* symbolName);
   void openProcedureDebugInfo(DumpWriter* writer);

   void writeFieldDebugInfo(ClassInfo::FieldMap& fields, DumpWriter* writer, DumpWriter* debugStrings);
   void writeLocal(ByteCodeIterator& it, DumpWriter* debug, DumpWriter* debugStrings);
   void writeBreakpoint(ByteCodeIterator& it, DumpWriter* debug);

   void endDebugInfo(DumpWriter* debug);

public:
   bool checkIfBegan(CommandTape& tape)
   {
      if (tape.tape.Count() > 0) {
         return ((*tape.tape.start()).code != blBegin);
      }
      else return false;
   }

   // to deal with variable tab size, instead of column we use offset from the row beginning (tab has a one character length in that case)
   void newBreakpoint(CommandTape& tape, int row, int disp, int length, int stepType);
   void newDummyBreakpoint(CommandTape& tape, int stepType);
   void newLocalInfo(CommandTape& tape, const TCHAR* localName, int level);

   void idleMethod(CommandTape& tape, ref_t messageRef);

   void newStaticSymbol(CommandTape& tape, ref_t symbolReference, ref_t nilReference);
   void newSymbol(CommandTape& tape);
   void newClass(CommandTape& tape);
   void newRoleTable(CommandTape& tape, ref_t reference);
   void newMethod(CommandTape& tape, ref_t messageRef, bool withParam);
   void newRedirectMethod(CommandTape& tape, ref_t messageRef);
   void newLocal(CommandTape& tape);
   void newBranchStatement(CommandTape& tape);
   void newBranch(CommandTape& tape);
   void newAlternativeBranch(CommandTape& tape);
   void newAlternativeBranchStatement(CommandTape& tape);
   void newLoop(CommandTape& tape);
   void newSubCode(CommandTape& tape);
   void newRole(CommandTape& tape, ref_t reference);

   void pushNewObject(CommandTape& tape, int size, ref_t reference);
   void pushNewObject(CommandTape& tape, int size, ref_t reference, ref_t constantRef);
   void pushObject(CommandTape& tape, ObjectInfo object);
   void pushSymbol(CommandTape& tape, ref_t symbolReference, ref_t nilReference, bool branchMode);
   void pushProperty(CommandTape& tape, ref_t symbolReference, bool branchMode);
   void pushCurrent(CommandTape& tape);
   void sendMessage(CommandTape& tape, ref_t messageRef, bool branchMode);
   void sendMessage(CommandTape& tape, ref_t messageRef, ref_t classRef, bool branchMode);
   void callExternal(CommandTape& tape, ref_t reference, bool branchMode);
   void callEmbedded(CommandTape& tape, ref_t reference, bool branchMode, int paramCount);
   void redirectMessage(CommandTape& tape);
   void redirectMessageToParent(CommandTape& tape, ref_t parentRef);
   void moveObject(CommandTape& tape, ObjectInfo object);
   void moveToObjectPtr(CommandTape& tape, ObjectInfo object, int offset);
   void returnObject(CommandTape& tape);
   void swap(CommandTape& tape, int offset);
   void shift(CommandTape& tape, int offset);
   void unshift(CommandTape& tape);

   void endStatement(CommandTape& tape);
   void endBranchStatement(CommandTape& tape, bool branchMode);
   void endBranch(CommandTape& tape, bool branchMode);
   void endLoop(CommandTape& tape);
   void endMethod(CommandTape& tape);
   void endRedirectMethod(CommandTape& tape);
   void endSymbol(CommandTape& tape);
   void endStaticSymbol(CommandTape& tape, ref_t staticReference);
   void endRoleTable(CommandTape& tape);
   void endClass(CommandTape& tape);

   void saveProcedure(ByteCodeIterator& it, SectionWriter* code, DumpWriter* debug, DumpWriter* debugStrings);
   void saveProcedure(ByteCodeIterator& it, SectionWriter* code)
   {
      saveProcedure(it, code, NULL, NULL);
   }

   void saveRoleTable(ByteCodeIterator& it, ref_t reference, _Module* module);
   void saveVMT(size_t classPosition, SectionWriter* vmt, SectionWriter* code, ByteCodeIterator& it, DumpWriter* debug, DumpWriter* debugStrings);

   void saveSymbol(ref_t reference, ByteCodeIterator& it, _Module* module, _Module* debugModule);
   void saveClass(ref_t reference, ByteCodeIterator& it, _Module* module, _Module* debugModule);
   void save(ref_t reference, CommandTape& tape, _Module* module, _Module* debugModule);
};

} // _ELENA_

#endif // bccompilerH
