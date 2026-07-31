//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Compiler
//
//		This file contains ELENA compiler class implementation.
//
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#include "elena.h"
// --------------------------------------------------------------------------
#include "compiler.h"
#include "errors.h"
#include <errno.h>

using namespace _ELENA_;

// --- Mode constants ---
#define CTRL_BRANCHING        0x0001
#define CTRL_PROPERTY         0x0002
#define CTRL_ROOT             0x0004
#define CTRL_ACTION           0x0008
#define CTRL_INHERITED        0x0010
#define CTRL_SINGLE           0x0020

#define VSELF_PTR_OFFSET      -1

// --- Auxiliary routines ---

inline bool isCollection(DNode node)
{
   return (node == nsExpression && node.nextNode()==nsExpression);
}

inline ref_t importMessage(_Module* exporter, ref_t exportRef, _Module* importer)
{
   if (!test(exportRef, PREDEFINED_REF)) {
      const TCHAR* message = exporter->resolveMessage(exportRef);

      return importer->mapMessage(message);
   }
   else return exportRef;
}

inline ref_t importReference(_Module* exporter, ref_t exportRef, _Module* importer)
{
   const TCHAR* reference = exporter->resolveReference(exportRef);

   return importer->mapReference(reference);
}

inline void findUninqueName(_Module* module, LocalReferenceName& name)
{
   size_t pos = getlength(name);
   int   index = 0;
   ref_t ref = 0;
   do {
      name[pos] = 0;
      name.appendHex(index++);

      ref = module->mapReference(name, true);
   } while (ref != 0);
}

// skip the hints and return the first hint node or none
inline DNode skipHints(DNode& node)
{
   DNode hints;
   if (node == nsHint)
      hints = node;

   while (node == nsHint)
      node = node.nextNode();

   return hints;
}

inline int countSymbols(DNode node)
{
   int counter = 0;
   while (node != nsNone) {
      counter++;

      node = node.nextNode();
   }
   return counter;
}

inline bool findSymbol(DNode node, Symbol symbol)
{
   while (node != nsNone) {
      if (node==symbol)
         return true;

      node = node.nextNode();
   }
   return false;
}

// --- Compiler::ModuleScope ---

Compiler::ModuleScope :: ModuleScope(Project* project, _Module* module, _Module* debugModule, const TCHAR* sourcePath)
   : symbolHints(otUnknown), forwardsUnresolved(Unresolved(), NULL)
{
   this->project = project;
   this->module = module;
   this->debugModule = debugModule;
   this->sourcePath = sourcePath;
   
   warnOnUnresolved = project->BoolSetting(opWarnOnUnresolved);
   warnOnWeakUnresolved = project->BoolSetting(opWarnOnWeakUnresolved);

   // predefined symbols
   // nil
   ref_t nilRef = module->mapReference(NIL_CLASS);
   defineConstant(nilRef);
   forwards.add(_T("nil"), nilRef);

   // type
   forwards.add(_T("type"), module->mapReference(TYPE_CLASS));
}

ObjectInfo Compiler::ModuleScope :: mapObject(TerminalInfo identifier)
{
   if (identifier==tsReference) {
      return mapReference(identifier, false);
   }
   else if (identifier==tsIdentifier || identifier==tsPrivate) {
      return getObjectInfo(mapSymbol(identifier, true));
   }
   else return ObjectInfo();
}

const TCHAR* Compiler::ModuleScope :: matchMask(const TCHAR* reference)
{
   LocalNamespace ns(reference);

   NamespaceMaskMap::Iterator it = masks.start();
   while (!it.Eof()) {
      if (ns.compare(it.key())) {
         return *it;
      }
      else it++;
   }
   return NULL;
}

ObjectInfo Compiler::ModuleScope :: mapReference(const TCHAR* reference, bool existing)
{
   ref_t referenceID = 0;

   if (!isWeakReference(reference)) {
      // check if there is a namespace mask
      const TCHAR* mask = matchMask(reference);
      if (!emptystr(mask)) {
         LocalNamespace  ns(mask);
         LocalIdentifier name(reference);
         LocalReferenceName fullName(ns, name);

         referenceID = module->mapReference(fullName, existing);
      }
      else referenceID = module->mapReference(reference, existing);
   }
   else referenceID = module->mapReference(reference, existing);

   return getObjectInfo(referenceID);
}

void Compiler::ModuleScope :: validateReference(TerminalInfo terminal, ref_t reference)
{   
   // check if the reference may be resolved
   bool found = false;

   if (warnOnUnresolved && (warnOnWeakUnresolved || !isWeakReference(terminal))) {
      int   mask = reference & mskAnyRef;
      reference &= ~mskAnyRef;
      
      _Module* refModule = project->resolveModule(module->resolveReference(reference), reference, true);

      if (refModule != NULL) {
         if (refModule->mapSection(reference | mask, true)!=NULL)
            found = true;
      }
      if (!found) {
         if (module == refModule) {
            forwardsUnresolved.add(Unresolved(terminal, reference | mask));
         }
         else raiseWarning(wrnUnresovableLink, terminal);
      }
   }
}

void Compiler::ModuleScope :: validateForwards()
{
   for (List<Unresolved>::Iterator it = forwardsUnresolved.start() ; !it.Eof() ; it++) {
      if (module->mapSection((*it).reference, true)==NULL)
         raiseWarning(wrnUnresovableLink, (*it).terminal);
   }
}

void Compiler::ModuleScope :: raiseError(const TCHAR* message, TerminalInfo terminal)
{
   project->raiseError(message, sourcePath, terminal.Row(), terminal.Col(), terminal.value);
}

void Compiler::ModuleScope :: raiseWarning(const TCHAR* message, TerminalInfo terminal)
{
   project->printInfo(message, sourcePath, terminal.Row(), terminal.Col(), terminal.value);
   project->indicateWarning();
}

void Compiler::ModuleScope :: compileForwardHints(DNode hints, const TCHAR* reference)
{
   while (hints == nsHint) {
      if (compstr(hints.Terminal(), HINT_CONSTANT)) {
         defineConstant(mapReference(reference).reference);
      }
      else raiseWarning(wrnUnknownHint, hints.Terminal());

      hints = hints.nextNode();
   }
}

// --- Compiler::SourceScope ---

Compiler::SourceScope :: SourceScope(Scope* parent)
   : Scope(parent)
{
   this->reference = 0;
}

Compiler::SourceScope :: SourceScope(ModuleScope* moduleScope, ref_t reference)
   : Scope(moduleScope)
{
   this->reference = reference;
}

// --- Compiler::SymbolScope ---

Compiler::SymbolScope :: SymbolScope(ModuleScope* parent, ref_t reference)
   : SourceScope(parent, reference)
{
   this->parameter = NULL;
}

void Compiler::SymbolScope :: compileHints(DNode hints)
{
   while (hints == nsHint) {
      raiseWarning(wrnUnknownHint, hints.Terminal());

      hints = hints.nextNode();
   }
}

ObjectInfo Compiler::SymbolScope :: mapObject(TerminalInfo identifier)
{
   if (compstr(parameter, identifier.value)) {
      return ObjectInfo(otSymbolParam, 0);
   }
   else return Scope::mapObject(identifier);
}

// --- Compiler::ClassScope ---

Compiler::ClassScope :: ClassScope(ModuleScope* parent, ref_t reference)
   : SourceScope(parent, reference)
{
   info.header.parentRef = moduleScope->module->mapReference(SUPER_CLASS);
   info.header.flags = elStandartVMT;
   this->info.header.roleRef = 0;
   info.classSize = elVMTOffset;
   info.size = 0;
}

ObjectInfo Compiler::ClassScope :: mapObject(TerminalInfo identifier)
{
   if (compstr(identifier, SELF_VAR)) {
      return ObjectInfo(otVSelf, 0);
   }
   else if (compstr(identifier, SUPER_VAR)) {
      return ObjectInfo(otSuper, info.header.parentRef);
   }
   else {
      int reference = info.fields.get(identifier);
      if (reference != -1) {
         return ObjectInfo(otField, reference);
      }
      else return Scope::mapObject(identifier);
   }
}

ObjectInfo Compiler::ClassScope :: mapRole(TerminalInfo identifier)
{
   // return role parent
   if (!info.roles.exist(identifier.value))
      raiseError(errUnknownRole, identifier);

   ObjectInfo roleInfo(otRole, 0);

   ClassInfo::FieldMap::Iterator it = info.roles.start();
   while (!it.Eof()) {
      if (compstr(it.key(), identifier.value))
         break;

      roleInfo.reference++;
      it++;
   }
   return roleInfo;
}

void Compiler::ClassScope :: compileHints(DNode hints)
{
   while (hints == nsHint) {
      TerminalInfo terminal = hints.Terminal();

      if (compstr(terminal, HINT_DEBUG)) {
         DNode value = hints.select(nsHintValue);

         if (value==nsNone) {
            raiseWarning(wrnInvalidHint, terminal);
         }
         else setDebugWatchHints(value);
      }
      else raiseWarning(wrnUnknownHint, terminal);

      hints = hints.nextNode();
   }
}

void Compiler::ClassScope :: setDebugWatchHints(DNode hintValue)
{
   TerminalInfo terminal = hintValue.Terminal();

   if (compstr(terminal, HINT_DEBUG_INT)) {
      info.header.flags |= elDebugDWORD;
   }
   else if (compstr(terminal, HINT_DEBUG_LONG)) {
      info.header.flags |= elDebugQWORD;
   }
   else if (compstr(terminal, HINT_DEBUG_REAL)) {
      info.header.flags |= elDebugReal64;
   }
   else if (compstr(terminal, HINT_DEBUG_LITERAL)) {
      info.header.flags |= elDebugLiteral;
   }
   else if (compstr(terminal, HINT_DEBUG_ARRAY)) {
      info.header.flags |= elDebugArray;
   }
   //else if (compstr(debugHint, HINT_DEBUG_DUMP)) {
   //   scope.metaData.header.flags |= elDebugDump;
   //}
   else raiseWarning(wrnUnknownHintValue, terminal);
}

// --- Compiler::RoleScope ---

Compiler::RoleScope :: RoleScope(ClassScope* parent, ref_t reference)
  : ClassScope(parent->moduleScope, reference)
{
   this->info.header.flags |= elRoleVMT;
   this->info.header.roleRef = parent->reference;
   this->info.header.parentRef = 0;
   this->parent = parent;
}

ObjectInfo Compiler::RoleScope :: mapObject(TerminalInfo identifier)
{
   return parent->mapObject(identifier);
}

// --- Compiler::MetodScope ---

Compiler::MethodScope :: MethodScope(ClassScope* parent, ref_t messageRef)
   : Scope(parent)
{
   this->messageRef = messageRef;
   this->param = NULL;
   this->isDefaultMethod = false;
}

void Compiler::MethodScope :: compileHints(DNode hints)
{
   while (hints == nsHint) {
      if (compstr(hints.Terminal(), HINT_DEFAULT)) {
         isDefaultMethod = true;
      }
      else raiseWarning(wrnUnknownHint, hints.Terminal());

      hints = hints.nextNode();
   }
}

ObjectInfo Compiler::MethodScope :: mapObject(TerminalInfo identifier)
{
   if (compstr(identifier, THIS_VAR)) {
      return ObjectInfo(otSelf, 0);
   }
   else return Scope::mapObject(identifier);
}

// --- Compiler::CodeScope ---

Compiler::CodeScope :: CodeScope(SourceScope* parent)
   : Scope(parent)
{
   this->tape = &parent->tape;
   this->level = 0;
   this->methodRef = 0;
}

Compiler::CodeScope :: CodeScope(MethodScope* parent)
   : Scope(parent)
{
   this->tape = &((ClassScope*)parent->getScope(slClass))->tape;
   this->level = 0;
   this->methodRef = parent->messageRef;
}

Compiler::CodeScope :: CodeScope(CodeScope* parent)
   : Scope(parent)
{
   this->tape = parent->tape;
   this->level = parent->level + parent->locals.Count();;
   this->methodRef = parent->methodRef;
}

ObjectInfo Compiler::CodeScope :: mapObject(TerminalInfo identifier)
{
   ref_t reference = locals.get(identifier);
   if (reference) {
      return ObjectInfo(otLocal, reference);
   }
   else return Scope::mapObject(identifier);
}

// --- Compiler::InlineClassScope ---

Compiler::InlineClassScope :: InlineClassScope(CodeScope* owner, ref_t reference)
   : ClassScope(owner->moduleScope, reference), outers(Outer())
{
   this->parent = owner;
   info.header.flags |= elInlineClass;
}

ObjectInfo Compiler::InlineClassScope :: mapObject(TerminalInfo identifier)
{
   if (compstr(identifier, THIS_VAR)) {
      return ObjectInfo(otSelf, 0);
   }
   else {
      Outer outer = outers.get(identifier);

	  // if object already mapped
      if (outer.reference!=-1) {
         if (outer.outerObject.type == otSuper) {
            return ObjectInfo(otSuper, outer.reference);
         }
         else return ObjectInfo(otOuter, outer.reference);
      }
      else {
         outer.outerObject = parent->mapObject(identifier);
         // map if the object is outer one
         if (outer.outerObject.type==otLocal || outer.outerObject.type==otField || outer.outerObject.type==otSymbolParam
            || outer.outerObject.type==otOuter || outer.outerObject.type==otVSelf || outer.outerObject.type==otSuper)
         {
            outer.reference = info.fields.Count() + 1;

            outers.add(identifier, outer);
            mapKey(info.fields, identifier.value, outer.reference);

            return ObjectInfo(otOuter, outer.reference);
         }
         // if inline symbol declared in symbol it treats self variable in a special way
         else if (compstr(identifier, SELF_VAR)) {
            return ObjectInfo(otVSelf, 0);
         }
         else return outer.outerObject;
      }
   }
}

// --- Compiler ---

Compiler :: Compiler(StreamReader* syntax)
   : _parser(syntax), _predefined(0)
{
   loadPredefinedMessages();
}

void Compiler :: loadPredefinedMessages()
{
   // load predefined
   _predefined.add(FAIL_MESSAGE, FAIL_MESSAGE_ID);
   _predefined.add(NEW_MESSAGE, NEW_MESSAGE_ID);
   _predefined.add(PROCEED_MESSAGE, PROCEED_MESSAGE_ID);
   _predefined.add(OPROCEED_MESSAGE, PROCEED_MESSAGE_ID);
   _predefined.add(COPY_MESSAGE, COPY_MESSAGE_ID);
   _predefined.add(COPYTO_MESSAGE, COPYTO_MESSAGE_ID);

   _predefined.add(NOTNIL_MESSAGE, NOTNIL_MESSAGE_ID);
   _predefined.add(OF_MESSAGE, OF_MESSAGE_ID);
   _predefined.add(IF_MESSAGE, IF_MESSAGE_ID);
   _predefined.add(IFNOT_MESSAGE, IFNOT_MESSAGE_ID);
   _predefined.add(ADD_MESSAGE, ADD_MESSAGE_ID);
   _predefined.add(SUB_MESSAGE, SUB_MESSAGE_ID);
   _predefined.add(MUL_MESSAGE, MUL_MESSAGE_ID);
   _predefined.add(DIV_MESSAGE, DIV_MESSAGE_ID);
   _predefined.add(BIGGEREQ_MESSAGE, BIGGEREQ_MESSAGE_ID);
   _predefined.add(SMALLEREQ_MESSAGE, SMALLEREQ_MESSAGE_ID);
   _predefined.add(BIGGER_MESSAGE, BIGGER_MESSAGE_ID);
   _predefined.add(SMALLER_MESSAGE, SMALLER_MESSAGE_ID);
   _predefined.add(EQUAL_MESSAGE, EQUAL_MESSAGE_ID);
   _predefined.add(NOTEQUAL_MESSAGE, NOTEQUAL_MESSAGE_ID);
   _predefined.add(ADD2_MESSAGE, ADD2_MESSAGE_ID);
   _predefined.add(SUB2_MESSAGE, SUB2_MESSAGE_ID);
   _predefined.add(BACK_MESSAGE, BACK_MESSAGE_ID);
   _predefined.add(RUN_MESSAGE, RUN_MESSAGE_ID);
   _predefined.add(SAME_MESSAGE, SAME_MESSAGE_ID);
}

ref_t Compiler :: mapProtectedMessage(const TCHAR* message, ModuleScope* moduleScope)
{
   const TCHAR* mask = moduleScope->matchMask(message);
   if (!emptystr(mask)) {
      LocalNamespace  ns(mask);
      LocalIdentifier name(message);
      LocalPrivateMessage fullName(ns, name);

      return moduleScope->module->mapMessage(fullName);
   }
   else return moduleScope->module->mapMessage(message);
}

ref_t Compiler :: mapDefaultMessage(const TCHAR* message, ModuleScope* moduleScope)
{
   LocalPrivateMessage fullName(STANDARD_MODULE, message);

   return moduleScope->module->mapMessage(fullName);
}

ref_t Compiler :: mapMessage(TerminalInfo terminal, ModuleScope* moduleScope)
{
   if (terminal==tsPrivate) {
      LocalPrivateMessage name(moduleScope->module->Name(), terminal);

      return moduleScope->module->mapMessage(name);
   }
   else if (terminal==tsProtected) {
      return mapProtectedMessage(terminal, moduleScope);
   }
   else {
      int index = _predefined.get(terminal);
      if (index != 0) {
         return index;
      }
      else return moduleScope->module->mapMessage(terminal);
   }
}

ref_t Compiler :: mapSymbolExpression(CodeScope& scope, int mode)
{
   ModuleScope* moduleScope = scope.moduleScope;

   // if it is a root inline expression we could try to name it after the symbol
   if (test(mode, CTRL_ROOT) && test(mode, CTRL_SINGLE)) {
      SymbolScope* symbol = (SymbolScope*)scope.getScope(Scope::slSymbol);
      if (symbol != NULL)
         return symbol->reference;
   }
   // otherwise auto generate the name
   LocalReferenceName name(moduleScope->module->Name(), INLINE_POSTFIX);

   findUninqueName(moduleScope->module, name);

   return moduleScope->module->mapReference(name);
}

void Compiler :: inheritRoles(ClassScope& scope, RoleMap& roles)
{
   ClassInfo::FieldMap::Iterator roleIt = scope.info.roles.start();
   while (!roleIt.Eof()) {
      LocalReferenceName referenceName(scope.moduleScope->module->Name(), ROLE_POSTFIX);
      findUninqueName(scope.moduleScope->module, referenceName);

      ref_t reference = scope.moduleScope->module->mapReference(referenceName);

      RoleScope* roleScope = new RoleScope(&scope, reference);

      // cannot have roles, so dummy list used
      RoleMap dummy;
      inheritClass(*roleScope, *roleIt, dummy);

      _coder.newClass(roleScope->tape);
      _coder.endClass(roleScope->tape);

      *roleIt = reference;

      roles.add(reference, roleScope);

      roleIt++;
   }
}

Compiler::InheritResult Compiler :: inheritClass(ClassScope& scope, ref_t parentRef, RoleMap& roles)
{
   ModuleScope* moduleScope = scope.moduleScope;

   size_t flagCopy = scope.info.header.flags;

   // get module reference
   ref_t moduleRef = 0;
   _Module* module = moduleScope->project->resolveModule(moduleScope->module->resolveReference(parentRef), moduleRef);

   if (module == NULL || moduleRef == 0)
      return irUnsuccessfull;

   // load parent meta data
   Section* metaData = module->mapSection(moduleRef | mskMetaDataRef, true);
   if (metaData != NULL) {
      DumpReader reader(metaData);
      // import references if we inheriting class from another module
      if (moduleScope->module != module) {
         ClassInfo copy;
         copy.load(&reader, false);

         scope.info.header = copy.header;
         scope.info.classSize = copy.classSize;
         scope.info.size = copy.size;
         // import method references and mark them as inherited
         ClassInfo::MethodMap::Iterator it = copy.methods.start();
         while (!it.Eof()) {
            scope.info.methods.add(importMessage(module, it.key(), moduleScope->module), false);

            it++;
         }

         // import role table
         if (copy.header.roleRef != 0)
            scope.info.header.roleRef = importReference(module, copy.header.roleRef, moduleScope->module);

         // import role references
         ClassInfo::FieldMap::Iterator roleIt = copy.roles.start();
         while (!roleIt.Eof()) {
            scope.info.roles.add(roleIt.key(), importReference(module, *roleIt, moduleScope->module));

            roleIt++;
         }

         // copy fields
         scope.info.fields.add(copy.fields);
      }
      else {
         scope.info.load(&reader, false);

         // mark all methods as inherited
         ClassInfo::MethodMap::Iterator it = scope.info.methods.start();
         while (!it.Eof()) {
            *it = false;

            it++;
         }
      }
      // inherit roles
      if (scope.info.roles.Count() > 0) {
         inheritRoles(scope, roles);
      }

      // restore parent and flags
      scope.info.header.parentRef = parentRef;
      scope.info.header.flags |= flagCopy;

      return irSuccessfull;
   }
   else return irUnsuccessfull;
}

void Compiler :: compileMessage(DNode node, CodeScope& scope, ObjectInfo object, int mode)
{
   DNode operand = node.firstChild();
   // message operand
   if (operand != nsNone) {
      compileExpression(operand, scope, mode);
   }
   // if no parameter provided place 'nil object
   else _coder.pushObject(*scope.tape, scope.moduleScope->mapReference(NIL_CLASS));

   recordStep(scope, node.Terminal(), dsProcedureStep);

   ref_t messageRef = 0;
   // if it is redirect message use the current method id
   if (compstr(node.Terminal(), REDIRECT_MESSAGE)) {
      messageRef = scope.methodRef;
      if (messageRef==0)
         scope.raiseError(errInvalidRedirectMessage, node.Terminal());
   }
   else messageRef = mapMessage(node.Terminal(), scope.moduleScope);

   // we need to place dummy breakpoint because sendMessage requires virtual end breakpoint (to cope with possible branching)
   _coder.newDummyBreakpoint(*scope.tape, dsVirtualEnd);
   if (object.type == otSuper) {
      _coder.sendMessage(*scope.tape, messageRef, object.reference, test(mode, CTRL_BRANCHING));
   }
   else _coder.sendMessage(*scope.tape, messageRef, test(mode, CTRL_BRANCHING));
}

void Compiler :: compileOperations(DNode node, CodeScope& scope, ObjectInfo object, int mode)
{
   ObjectInfo currentObject = object;

   bool branching = findSymbol(node, nsAlternative);
   if (branching)
      _coder.newBranch(*scope.tape);

   while (node != nsNone) {
      if (node==nsL1Operation || node==nsL2Operation || node==nsL3Operation || node==nsL4Operation) {
         if (branching) {
            compileMessage(node, scope, currentObject, mode | CTRL_BRANCHING);
         }
         else compileMessage(node, scope, currentObject, mode);

         // only the first message in the chain could be sent to the class parent
         currentObject.type = otExpression;
      }
      else if (node==nsAlternative) {
         _coder.newBreakpoint(*scope.tape, 0, 0, 0, dsVirtualEnd);
         _coder.newAlternativeBranch(*scope.tape);

         currentObject = object;
      }
      node = node.nextNode();
   }

   if (branching)
      _coder.endBranch(*scope.tape, test(mode, CTRL_BRANCHING));
}

void Compiler :: compileControlChain(DNode node, CodeScope& scope, ObjectInfo object, int mode)
{
   ObjectInfo currentObject = object;

   _coder.newBranchStatement(*scope.tape);
   while (node != nsNone) {
      if (node==nsL1Operation || node==nsL2Operation || node==nsL3Operation || node==nsL4Operation) {
         compileMessage(node, scope, object, mode | CTRL_BRANCHING);

         // only the first message in the chain could be sent to the class parent
         currentObject.type = otExpression;
      }
      else if (node==nsAlternative) {
         _coder.newBreakpoint(*scope.tape, 0, 0, 0, dsVirtualEnd);

         _coder.newAlternativeBranchStatement(*scope.tape);
         currentObject = object;
      }
      else if (node == nsSubCode) {
         _coder.newSubCode(*scope.tape);

         CodeScope subcode(&scope);
         compileCode(node, subcode);
      }
      node = node.nextNode();
   }
   _coder.endBranchStatement(*scope.tape, false);
}

ObjectInfo Compiler :: compileTerminal(TerminalInfo terminal, CodeScope& scope, int mode)
{
   bool isProperty = test(mode, CTRL_PROPERTY);

   if (!isProperty)
      recordStep(scope, terminal, dsStep);

   ObjectInfo object;
   if (terminal==tsLiteral) {
      object = ObjectInfo(otLiteral, scope.moduleScope->module->mapConstant(terminal));
   }
   else if (terminal == tsInteger) {
      int integer = _ttoi(terminal.value);
      if (errno == ERANGE)
         scope.raiseError(errInvalidIntNumber, terminal);

      object = ObjectInfo(otInteger, scope.moduleScope->module->mapConstant(terminal));
   }
   else if (terminal==tsHexInteger) {
      LocalString<20> s(terminal.value, getlength(terminal.value) - 1);

      long integer = _tcstoul(s, NULL, 16);
      if (errno == ERANGE)
         scope.raiseError(errInvalidIntNumber, terminal);

      // convert back to string as a decimal integer
      s.clear();
      s.appendLong(integer);

      object = ObjectInfo(otInteger, scope.moduleScope->module->mapConstant(s));
   }
   else if (terminal == tsReal) {
      LocalString<30> s(terminal.value, getlength(terminal.value) - 1);
      double number = _tcstod(s, NULL);
      if (errno == ERANGE)
         scope.raiseError(errInvalidIntNumber, terminal);

      object = ObjectInfo(otReal, scope.moduleScope->module->mapConstant(s));
   }
   else if (!emptystr(terminal)) {
      object = scope.mapObject(terminal);
   }

   if (object.type != otUnknown) {
      // symbol which has initializing part
      if (isProperty) {
         if (object.type != otSymbol)
            scope.raiseError(errInvalidProperty, terminal);

         object.type = otProperty;
         _coder.pushProperty(*scope.tape, object.reference, test(mode, CTRL_BRANCHING));

         scope.moduleScope->validateReference(terminal, object.reference | mskSymbolRef);
      }
      // normal symbol
      else if (object.type == otSymbol) {
         _coder.pushSymbol(*scope.tape, object.reference, scope.moduleScope->module->mapReference(NIL_CLASS), test(mode, CTRL_BRANCHING));

         scope.moduleScope->validateReference(terminal, object.reference | mskSymbolRef);
      }
      else _coder.pushObject(*scope.tape, object);
   }
   else scope.raiseError(errUnknownObject, terminal);

   return object;
}

void Compiler :: compileSymbolExpression(DNode node, CodeScope& ownerScope, InlineClassScope& scope, int mode)
{
   bool assigned = test(mode, CTRL_INHERITED);
   bool isProperty = test(mode, CTRL_PROPERTY);
   bool needNew = false;
  
   // check if newClass is already called
   if (!_coder.checkIfBegan(scope.tape))
      _coder.newClass(scope.tape);

   // if action expression
   if (test(mode, CTRL_ACTION)) {
      compileVMT(node, scope);
   }
   // if symbol expression
   else compileVMT(node.firstChild(), scope);

   int field_count = scope.info.fields.Count();
   int role_count = scope.info.roles.Count();

   if (field_count==0 && role_count == 0 && !test(scope.info.header.flags, elStructureRole)) {
      // stateless inline class
      scope.info.header.flags |= elStateless;

      scope.moduleScope->defineConstant(scope.reference);

      _coder.pushObject(*ownerScope.tape, ObjectInfo(otConstant, scope.reference));

      needNew = isProperty;
   }
   else {
      scope.info.header.flags &= ~elStateless;

      _coder.pushNewObject(*ownerScope.tape, field_count, scope.reference);

      Map<const TCHAR*, InlineClassScope::Outer>::Iterator outer_it = scope.outers.start();
      while(!outer_it.Eof()) {
         ObjectInfo info = (*outer_it).outerObject;

         _coder.pushObject(*ownerScope.tape, info);
         // Field INDEX, not a byte offset: the backend multiplies by the target
      // slot size. See doc/todo.txt:334 -- the original author identified this
      // in 2009 as the thing that makes byte code processor-specific.
      _coder.moveToObjectPtr(*ownerScope.tape, ObjectInfo(otLocal, 1), ((*outer_it).reference - 1));
         _coder.endStatement(*ownerScope.tape);

         outer_it++;
      }
      needNew = assigned | isProperty;
   }

   if (needNew) {
      // we need to place dummy breakpoint because sendMessage requires virtual end breakpoint (due to current implementation)
      _coder.newDummyBreakpoint(*ownerScope.tape, dsVirtualEnd);
      if (!isProperty) {
         _coder.pushObject(*ownerScope.tape, scope.moduleScope->mapReference(NIL_CLASS));
         _coder.sendMessage(*ownerScope.tape, NEW_MESSAGE_ID, false);
      }
      else {
         _coder.swap(*ownerScope.tape, 1);
         _coder.sendMessage(*ownerScope.tape, NEW_MESSAGE_ID, false);
      }
   }

   _coder.endClass(scope.tape);

   // save class meta data
   SectionWriter metaWriter(scope.moduleScope->module->mapSection(scope.reference | mskMetaDataRef, false));
   scope.info.save(&metaWriter);

   // create byte code sections
   _coder.save(scope.reference, scope.tape, scope.moduleScope->module, scope.moduleScope->debugModule);
}

void Compiler :: compileSymbolExpression(DNode node, CodeScope& ownerScope, int mode)
{
   if (test(mode, CTRL_PROPERTY))
      ownerScope.raiseError(errInvalidProperty, node.Terminal());

   recordStep(ownerScope, node.Terminal(), dsStep);

   InlineClassScope scope(&ownerScope, mapSymbolExpression(ownerScope, mode));

   // if it is an action symbol
   if (test(mode, CTRL_ACTION)) {
      // cannot have roles, so dummy list used
      RoleMap dummy;
      compileParentDeclaration(DNode(), scope, dummy);

      compileSymbolExpression(node, ownerScope, scope, mode);
   }
   // if it is inherited symbol expression
   else if (node.Terminal() != nsNone) {
      RoleMap roles(NULL, freeobj);

	  // inherit parent and create role table if necessary
      compileParentDeclaration(node, scope, roles);
      compileRoleDeclarations(scope, roles);

      compileSymbolExpression(node.firstChild(), ownerScope, scope, mode | CTRL_INHERITED);
   }
   // if it is normal symbol expression
   else {
      // cannot have roles, so dummy list used
      RoleMap dummy;
      compileParentDeclaration(DNode(), scope, dummy);

      compileSymbolExpression(node.firstChild(), ownerScope, scope, mode);
   }
}

void Compiler :: compileExternalFunction(DNode node, CodeScope& scope, int mode)
{
   DNode argument = node.firstChild();
   int count = 0;
   // load function parameters into stack
   while (argument != nsNone) {
      compileExpression(argument, scope, mode);
      count++;

      argument = argument.nextNode();
   }
   recordStep(scope, node.Terminal(), dsAtomicStep);

   if (count > 127)
      scope.raiseError(errExtTooManyParameters, node.Terminal());

   _coder.callExternal(*scope.tape, scope.moduleScope->module->mapReference(ReferenceName(PACKAGE_MODULE, node.Terminal())),
      test(mode, CTRL_BRANCHING));

   // clear the stack from function parameters
   // !! replace sequence of pops with a single one
   while (count > 1) {
      _coder.endStatement(*scope.tape);

      count--;
   }
}

void Compiler :: compileEmbeddedExpression(DNode node, CodeScope& scope, int mode)
{
   int paramCount = 0;

   DNode argument = node.firstChild();
   // load function parameters into stack
   while (argument != nsNone) {
      compileExpression(argument, scope, mode);

      argument = argument.nextNode();
      paramCount++;
   }

   recordStep(scope, node.Terminal(), dsAtomicStep);

   // embed code
   _coder.callEmbedded(*scope.tape, scope.moduleScope->module->mapReference(ReferenceName(PACKAGE_MODULE, node.Terminal())),
      test(mode, CTRL_BRANCHING), paramCount - 1);

   // embedded code should clear stack itself
}

void Compiler :: compileCollection(DNode objectNode, CodeScope& scope, int mode)
{
   compileCollection(objectNode, scope, mode, scope.moduleScope->module->mapReference(scope.moduleScope->project->StrSetting(opArrayClass)));
}

void Compiler :: compileCollection(DNode node, CodeScope& scope, int mode, ref_t vmtReference)
{
   int counter = countSymbols(node);

   _coder.pushNewObject(*scope.tape, counter, vmtReference);

   int index = 0;
   while (node != nsNone) {
      compileExpression(node, scope, mode);
      // Field index, not a byte offset -- see the note above.
      _coder.moveToObjectPtr(*scope.tape, ObjectInfo(otLocal, 1), index);
      _coder.endStatement(*scope.tape);

      node = node.nextNode();
      index++;
   }
}

void Compiler :: compileGroup(DNode objectNode, CodeScope& scope, int mode, bool isBroadcasting)
{
   compileCollection(objectNode.firstChild(), scope, mode, 
      isBroadcasting ? scope.moduleScope->module->mapReference(CAST_CLASS) 
      : scope.moduleScope->module->mapReference(GROUP_CLASS));
}

void Compiler :: compileExtend(DNode objectNode, CodeScope& ownerScope, int mode)
{
   if (test(mode, CTRL_PROPERTY))
      ownerScope.raiseError(errInvalidProperty, objectNode.Terminal());

   recordStep(ownerScope, objectNode.firstChild().Terminal(), dsAtomicStep);

   InlineClassScope scope(&ownerScope, mapSymbolExpression(ownerScope, mode));

   _coder.newClass(scope.tape);

   // cannot have roles, so dummy list used
   RoleMap dummy;
   compileParentDeclaration(DNode(), scope, dummy);

   // create any handler
   scope.info.header.flags |= elVMTAnyHandler;

   // for any handler message id is 0
   MethodScope methodScope(&scope, 0);
   compileRedirectMethod(objectNode, methodScope, false);

   compileSymbolExpression(objectNode.nextNode(), ownerScope, scope, mode);
}

void Compiler :: compileType(DNode objectNode, CodeScope& scope, int mode)
{
   ObjectInfo type;

   recordStep(scope, objectNode.Terminal(), dsStep);

   TerminalInfo terminal = objectNode.Terminal();
   if (compstr(terminal, CAST_SYMBOL)) {
      type.reference = scope.moduleScope->module->mapReference(CAST_CLASS);
   }
   else if (compstr(terminal, GROUP_SYMBOL)) {
      type.reference = scope.moduleScope->module->mapReference(GROUP_CLASS);
   }
   else if (terminal.symbol == tsReference) {
      // get class reference
      type = scope.moduleScope->mapReference(terminal);
   }
   else scope.raiseError(errInvalidOperation, terminal);

   _coder.pushNewObject(*scope.tape, 4 | gcBinary, scope.moduleScope->module->mapReference(TYPEINSTANCE_CLASS), type.reference);
}

void Compiler :: compileShift(DNode node, CodeScope& scope)
{
   recordStep(scope, node.Terminal(), dsAtomicStep);

   ObjectInfo role = ((ClassScope*)scope.getScope(Scope::slClass))->mapRole(node.Terminal());
   _coder.shift(*scope.tape, role.reference);
}

void Compiler :: compileUnShift(DNode node, CodeScope& scope)
{
   recordStep(scope, node.Terminal(), dsAtomicStep);

   if (!test(((ClassScope*)scope.getScope(Scope::slClass))->info.header.flags, elRoleVMT)) {
      scope.raiseError(errInvalidShift, node.Terminal());
   }
   _coder.unshift(*scope.tape);
}

ObjectInfo Compiler :: compileObject(DNode objectNode, CodeScope& scope, int mode)
{
   ObjectInfo objectInfo(otExpression, 0);

   DNode member = objectNode.firstChild();
   switch (member)
   {
      case nsInlineSymbol:
         compileSymbolExpression(objectNode, scope, mode);
         break;
      case nsActionExpression:
         compileSymbolExpression(objectNode, scope, mode | CTRL_ACTION);
         break;
      case nsGroup:
         compileGroup(member, scope, mode, false);
         break;
      case nsCast:
         compileGroup(member, scope, mode, true);
         break;
      case nsExtend:
         compileExtend(member, scope, mode);
         break;
      case nsType:
         compileType(member, scope, mode);
         break;
      case nsExpression:
         if (isCollection(member)) {
            compileCollection(member, scope, mode);
         }
         else compileExpression(member, scope, mode);
         break;
      default:
         objectInfo = compileTerminal(objectNode.Terminal(), scope, mode);
   }
   return objectInfo;
}

void Compiler :: compileControlExpression(DNode node, CodeScope& scope, int mode)
{
   DNode objectNode = node.firstChild();

   ObjectInfo objectInfo = compileObject(objectNode, scope, mode);

   compileControlChain(objectNode.nextNode(), scope, objectInfo, mode & ~CTRL_ROOT);
}

void Compiler :: compileLoop(DNode node, CodeScope& scope, int mode)
{
   _coder.newLoop(*scope.tape);

   compileExpression(node, scope, mode | CTRL_BRANCHING);
   DNode code = node.select(nsSubCode);
   compileCode(code, scope);

   _coder.newBreakpoint(*scope.tape, 0, 0, 0, dsVirtualEnd);
   _coder.endLoop(*scope.tape);
}

void Compiler :: compileExpression(DNode node, CodeScope& scope, int mode)
{
   DNode member = node.firstChild();

   // mark if the expression is a single one (contains no operations)
   if (member.nextNode() == nsNone)
      mode |= CTRL_SINGLE;

   ObjectInfo objectInfo(otExpression, 0); 
   if (member==nsObject) {
      if(member.nextNode() == nsL0Operation) {
         recordStep(scope, member.Terminal(), dsVirtualStep);

         compileExpression(member.nextNode(), scope, mode & ~CTRL_ROOT);
         compileObject(member, scope, mode | CTRL_PROPERTY);
      }
      else objectInfo = compileObject(member, scope, mode);
   }
   else if (member == nsExternalExpression) {
      compileExternalFunction(member, scope, mode);
   }
   else if (member == nsEmbeddedExpression) {
      compileEmbeddedExpression(member, scope, mode);
   }
   else compileExpression(member, scope, mode);

   if (member.nextNode() != nsNone) {
      compileOperations(member.nextNode(), scope, objectInfo, mode & ~CTRL_ROOT);
   }
}

void Compiler :: compileAssignment(DNode node, CodeScope& scope, TerminalInfo variable)
{
   ObjectInfo object = scope.mapObject(variable);

   compileExpression(node.firstChild(), scope, 0);
   if (object.type == otLocal) {
      _coder.moveObject(*scope.tape, object);
   }
   else if (object.type == otField) {
      _coder.moveObject(*scope.tape, object);
      // if the object is temporal recreate it, and check if the yg object refers to mg one
      _coder.callEmbedded(*scope.tape, scope.moduleScope->module->mapReference(ASSIGN_FUNCTION), false, -1);
   }
   else if (object.type == otUnknown) {
      scope.raiseError(errUnknownObject, variable);
   }
   else scope.raiseError(errInvalidOperation, variable);
}

void Compiler :: compileVariable(DNode node, CodeScope& scope)
{
   if (!scope.locals.exist(node.Terminal())) {
      int level = scope.newLocal(node.Terminal());

      _coder.newLocal(*scope.tape);
      _coder.newLocalInfo(*scope.tape, node.Terminal(), level);

      compileAssignment(node.firstChild(), scope, node.Terminal());
   }
   else scope.raiseError(errDuplicatedLocal, node.Terminal());
}

void Compiler :: compileCode(DNode node, CodeScope& scope)
{
   DNode statement = node.firstChild();
   while (statement != nsNone) {
      switch(statement) {
         case nsExpression:
            if (statement.select(nsAssigning)!=nsNone) {
               compileAssignment(statement.select(nsAssigning), scope, statement.firstChild().Terminal());
            }
            else compileExpression(statement, scope, CTRL_ROOT);

            _coder.endStatement(*scope.tape);
            break;
         case nsControl:
            compileControlExpression(statement, scope, CTRL_ROOT);

            _coder.endStatement(*scope.tape);
            break;
         case nsLoop:
            compileLoop(statement, scope, CTRL_ROOT);
            break;
         case nsRetStatement:
            compileExpression(statement.firstChild(), scope, CTRL_ROOT);
            _coder.newBreakpoint(*scope.tape, 0, 0, 0, dsVirtualEnd);
            _coder.returnObject(*scope.tape);
            break;
         case nsVariable:
            compileVariable(statement, scope);

            _coder.endStatement(*scope.tape);
            break;
         case nsShift:
            if (statement.firstChild()==nsShiftParam) {
               compileShift(statement.firstChild(), scope);
            }
            else compileUnShift(statement, scope);
            break;
         case nsCodeEnd:
            recordStep(scope, statement.Terminal(), dsEOP);
            break;
      }
      statement = statement.nextNode();
   }
}

void Compiler :: compileIdleMethod(ClassScope& scope)
{
   if (!scope.info.methods.exist(DUMMY_MESSAGE_ID)) {
      scope.info.classSize += sizeof(VMTEntry);
      scope.info.methods.add(DUMMY_MESSAGE_ID, true);

      _coder.idleMethod(scope.tape, DUMMY_MESSAGE_ID);
   }
}

void Compiler :: compileMethod(DNode node, MethodScope& scope)
{
   ClassScope* ownerScope = (ClassScope*)scope.getScope(Scope::slClass);

   // check if the method is inhreited and update vmt size accordingly
   ClassInfo::MethodMap::Iterator it = ownerScope->info.methods.getIt(scope.messageRef);
   if (it.Eof()) {
      ownerScope->info.classSize += sizeof(VMTEntry);
      ownerScope->info.methods.add(scope.messageRef, true);
   }
   else *it = true;

   CodeScope codeScope(&scope);

   // if method parameter was declared save it as a variable or ignore it
   if (!emptystr(scope.param)) {
      int level = codeScope.newLocal(scope.param);

      _coder.newMethod(*codeScope.tape, scope.messageRef, true);
      _coder.newLocalInfo(*codeScope.tape, SELF_VAR, -3);
      _coder.newLocalInfo(*codeScope.tape, scope.param, level);
   }
   else {
      _coder.newMethod(*codeScope.tape, scope.messageRef, false);
      _coder.newLocalInfo(*codeScope.tape, SELF_VAR, -3);
   }

   DNode body = node.select(nsSubCode);
   // if method body is a set of statements
   if (body!=nsNone) {
      compileCode(body, codeScope);
   }
   // if method body is a returning expression (note in that case the first node is a parameter)
   else compileCode(node, codeScope);

   _coder.endMethod(*codeScope.tape);
}

void Compiler :: compileRedirectMethod(DNode node, MethodScope& scope, bool overridden)
{
   CodeScope codeScope(&scope);

   _coder.newRedirectMethod(*codeScope.tape, scope.messageRef);

   _coder.pushObject(*codeScope.tape, ObjectInfo(otVSelf, 0));

   compileExpression(node, codeScope, CTRL_ROOT);

   _coder.redirectMessage(*codeScope.tape);

   // if the parent also contains any handler
   if (overridden) {
      _coder.redirectMessageToParent(*codeScope.tape, ((ClassScope*)scope.getScope(Scope::slClass))->info.header.parentRef);
   }

   _coder.endRedirectMethod(*codeScope.tape);
}

void Compiler :: compileParentDeclaration(DNode node, ClassScope& scope, RoleMap& roles)
{
   // base class $elena'object must not to have a parent
   if (scope.info.header.parentRef == scope.reference) {
      if (node.Terminal() != nsNone)
         scope.raiseError(errInvalidSyntax, node.Terminal());

      scope.info.header.parentRef = 0;
   }
   // if the class has an implicit parent
   else if (node.Terminal() != nsNone) {
      TerminalInfo identifier = node.Terminal();
      if (identifier == tsIdentifier || identifier == tsPrivate) {
         scope.info.header.parentRef = scope.moduleScope->mapSymbol(node.Terminal(), true);
      }
      else scope.info.header.parentRef = scope.moduleScope->mapReference(identifier).reference;

      // !! temporal
      if (scope.info.header.parentRef == 0)
         scope.raiseError(errUnknownClass, node.Terminal());
   }
   if (scope.info.header.parentRef != 0) {
      InheritResult res = inheritClass(scope, scope.info.header.parentRef, roles);
      //if (res == irObsolete) {
      //   scope.raiseWarning(wrnObsoleteClass, node.Terminal());
      //}
      /*else */if (res == irUnsuccessfull)
         scope.raiseError(errUnknownClass, node.Terminal());
   }
}

void Compiler :: compileSymbolCode(ClassScope& scope)
{
   // creates implicit symbol
   SymbolScope symbolScope(scope.moduleScope, scope.reference);

   _coder.newSymbol(symbolScope.tape);

   if (test(scope.info.header.flags, elStateless)) {
      // constant symbol
      _coder.pushObject(symbolScope.tape, ObjectInfo(otConstant, scope.reference));
   }
   else  {
      // dynamic symbol
      int size = scope.info.size | gcBinary;
      if (!test(scope.info.header.flags, elStructureRole)) {
         size = scope.info.fields.Count();
      }
      _coder.pushNewObject(symbolScope.tape, size, scope.reference);
   }

   // send new message
   ModuleScope* moduleScope = scope.moduleScope;
   // new message parameter is taken from the previous stack frame
   _coder.pushObject(symbolScope.tape, ObjectInfo(otLocal, VSELF_PTR_OFFSET));

   // we need to place dummy breakpoint because sendMessage requires virtual end breakpoint (to cope with possible branching)
   _coder.newDummyBreakpoint(symbolScope.tape, dsVirtualEnd);

   _coder.sendMessage(symbolScope.tape, NEW_MESSAGE_ID, false);

   _coder.endSymbol(symbolScope.tape);

   // create byte code sections
   _coder.save(scope.reference, symbolScope.tape, scope.moduleScope->module, scope.moduleScope->debugModule);
}

void Compiler :: compileVMT(DNode member, ClassScope& scope)
{
   // reserve the first entry for role vmt, due to iocall(1) command
   if (test(scope.info.header.flags, elRoleVMT)) {
      compileIdleMethod(scope);
   }

   // if it is an action expression
   if (member.firstChild() == nsActionExpression) {
      MethodScope methodScope(&scope, PROCEED_MESSAGE_ID);

      // set a parameter name if any
      if (member == nsObject)
         methodScope.param = member.Terminal();

      compileMethod(member.firstChild(), methodScope);
   }
   else {
      while (member != nsNone) {
         DNode hints = skipHints(member);

         switch(member) {
            case nsMethod:
            {
               MethodScope methodScope(&scope, mapMessage(member.Terminal(), scope.moduleScope));

               methodScope.compileHints(hints);

               if (methodScope.isDefaultMethod) {
                  methodScope.messageRef = mapDefaultMessage(member.Terminal(), scope.moduleScope);
               }

               // set a parameter name if any
               methodScope.param = member.select(nsMethodArgument).Terminal();

               // check if there is no duplicate method
               if (scope.info.methods.exist(methodScope.messageRef, true))
                  scope.raiseError(errDuplicatedMethod, member.Terminal());

               compileMethod(member, methodScope);
               break;
            }
            case nsExtend:
            {
               bool overridden = test(scope.info.header.flags, elVMTAnyHandler);
               // any message handler - adding to special ANY VMT
               scope.info.header.flags |= elVMTAnyHandler;

               // for any handler message id is 0
               MethodScope methodScope(&scope, 0);

               compileRedirectMethod(member, methodScope, overridden);
               break;
            }
         }
         member = member.nextNode();
      }
   }
}

void Compiler :: compileRoleDeclarations(DNode& member, ClassScope& scope, RoleMap& roles)
{
   while (member != nsNone) {
      if (member==nsRole) {
         scope.info.header.flags |= elVMTWithRoles;

         const TCHAR* roleName = member.Terminal();
         ref_t reference = 0;

         // if role is overridden
         RoleScope* roleScope = NULL;
         if (scope.info.roles.exist(roleName)) {
            reference = scope.info.roles.get(roleName);

            roleScope = roles.get(reference);

            roleScope->tape.clear();
         }
         else {
            // find unique role name
            LocalReferenceName referenceName(scope.moduleScope->module->Name(), ROLE_POSTFIX);
            findUninqueName(scope.moduleScope->module, referenceName);

            reference = scope.moduleScope->module->mapReference(referenceName);

            scope.info.roles.add(roleName, reference);
			   roleScope = new RoleScope(&scope, reference);

			   roles.add(reference, roleScope);
         }
         // compile role vmt
         _coder.newClass(roleScope->tape);
         compileVMT(member.firstChild(), *roleScope);
         _coder.endClass(roleScope->tape);
      }
      else break;

      member = member.nextNode();
   }

   // create role table if any
   if (test(scope.info.header.flags, elVMTWithRoles)) {
      LocalReferenceName referenceName(scope.moduleScope->module->Name(), ROLE_POSTFIX);
      findUninqueName(scope.moduleScope->module, referenceName);

      scope.info.header.roleRef = scope.moduleScope->module->mapReference(referenceName);

      _coder.newRoleTable(scope.tape, scope.info.header.roleRef);

      ClassInfo::FieldMap::Iterator roleIt = scope.info.roles.start();
      while (!roleIt.Eof()) {
         _coder.newRole(scope.tape, *roleIt);

         // create byte code
         RoleScope* roleScope = roles.get(*roleIt);
         roleScope->save(_coder);

         roleIt++;
      }

      _coder.endRoleTable(scope.tape);
   }
}

void Compiler :: compileFieldDeclarations(DNode& member, ClassScope& scope)
{
   while (member != nsNone) {
      if (member==nsField) {
         DNode size = member.firstChild();
         // data field
         if (size==nsSize) {
            if (scope.info.fields.Count() != 0) {
               scope.raiseError(errIllegalField, size.Terminal());
            }
            int sizeValue = _ttoi(size.Terminal().value);

            scope.info.header.flags |= elStructureRole;
            scope.info.size += sizeValue;
         }
         // dynamic data field
         else if (member.Terminal() == nsNone) {
            if (scope.info.fields.Count() != 0) {
               scope.raiseError(errIllegalField, size.Terminal());
            }
            scope.info.header.flags |= elDynamicRole;
            scope.info.header.flags |= elStructureRole;
         }
         // normal field
         else {
            if (test(scope.info.header.flags, elStructureRole) || test(scope.info.header.flags, elDynamicRole))
               scope.raiseError(errIllegalField, member.Terminal());

            if (scope.info.fields.exist(member.Terminal()))
               scope.raiseError(errDuplicatedField, member.Terminal());

            scope.info.fields.add(member.Terminal(), scope.info.fields.Count() + 1);
         }
      }
      else break;

      member = member.nextNode();
   }
}

void Compiler :: compileClassDeclaration(DNode node, ClassScope& scope, DNode hints)
{
   scope.compileHints(hints);

   _coder.newClass(scope.tape);

   RoleMap roles(NULL, freeobj);

   DNode member = node.firstChild();
   if (member==nsBaseClass) {
      compileParentDeclaration(member, scope, roles);

      member = member.nextNode();
   }
   else compileParentDeclaration(DNode(), scope, roles);

   compileFieldDeclarations(member, scope);
   compileRoleDeclarations(member, scope, roles);

   // check if the class is stateless
   if (!test(scope.info.header.flags, elVMTWithRoles) && scope.info.fields.Count() == 0
      && !test(scope.info.header.flags, elStructureRole))
   {
      scope.info.header.flags |= elStateless;

      scope.moduleScope->defineConstant(scope.reference);
   }
   else scope.info.header.flags &= ~elStateless;

   // compile explicit symbol
   compileSymbolCode(scope);

   compileVMT(member, scope);

   _coder.endClass(scope.tape);

   // create byte code
   scope.save(_coder);
}

void Compiler :: compileSymbolDeclaration(DNode node, SymbolScope& scope, DNode hints, bool isStatic)
{
   scope.compileHints(hints);

   // compile symbol into byte codes
   if (isStatic) {
      _coder.newStaticSymbol(scope.tape, scope.reference, scope.moduleScope->module->mapReference(NIL_CLASS));
   }
   else _coder.newSymbol(scope.tape);

   CodeScope codeScope(&scope);

   // compile symbol body
   DNode expression = node.firstChild();
   if (expression == nsActionExpression) {
      compileSymbolExpression(node, codeScope, CTRL_ROOT | CTRL_ACTION);
   }
   // if symbol has a parameter
   else if (expression == nsSymbolParameter) {
      compileExpression(expression.nextNode(), codeScope, CTRL_ROOT);
   }
   else compileExpression(expression, codeScope, CTRL_ROOT);

   _coder.newBreakpoint(scope.tape, 0, 0, 0, dsVirtualEnd);

   if (isStatic) {
      _coder.endStaticSymbol(scope.tape, scope.reference);
   }
   else _coder.endSymbol(scope.tape);

   // create byte code sections
   _coder.save(scope.reference, scope.tape, scope.moduleScope->module, scope.moduleScope->debugModule);
}

void Compiler :: compileDirectives(DNode& member, ModuleScope& scope)
{
   while (member != nsNone) {
      DNode hints = skipHints(member);

      switch (member) {
         case nsShortcut:
         {
            TerminalInfo shortcut = member.Terminal();
            TerminalInfo reference = member.firstChild().Terminal();
            // define namespace shortcut
            if(shortcut == tsWildcard) {
               if (!scope.defineMask(shortcut.value, reference.value))
                  scope.raiseError(errDuplicatedDefinition, shortcut);
            }
            // define reference shortcut
            else {
               if (!scope.defineForward(shortcut.value, reference.value))
                  scope.raiseError(errDuplicatedDefinition, member.Terminal());

               scope.compileForwardHints(hints, reference.value);
            }
            break;
         }
         default:
            return;
      }
      member = member.nextNode();
   }
}

void Compiler :: compileModule(DNode node, ModuleScope& scope)
{
   DNode member = node.firstChild();

   compileDirectives(member, scope);

   while (member != nsNone && member != nsStart) {
      DNode hints = skipHints(member);

      ref_t reference = scope.mapSymbol(member.Terminal());

      // check for duplicate declaration
      if (scope.module->mapSection(reference | mskSymbolRef, true))
         scope.raiseError(errDuplicatedSymbol, member.Terminal());

      switch (member) {
         case nsClass:
         {
            ClassScope classScope(&scope, reference);
            compileClassDeclaration(member, classScope, hints);
            break;
         }
         case nsSymbol:
         {
            SymbolScope symbolScope(&scope, reference);
            if (member.firstChild() == nsSymbolParameter) {
               symbolScope.parameter = member.firstChild().Terminal();
            }
            compileSymbolDeclaration(member, symbolScope, hints, false);
            break;
         }
         case nsStatic:
         {
            SymbolScope symbolScope(&scope, reference);
            compileSymbolDeclaration(member, symbolScope, hints, true);
            break;
         }
      }
      member = member.nextNode();
   }
   // validate the forward refereces if unresolved reference warning is enabled
   scope.validateForwards();
}

void Compiler :: compile(const TCHAR* source, MemoryDump* buffer, ModuleScope& scope)
{
   // parse
   TextFileReader sourceFile(source, feAutodetect);
   if (!sourceFile.isOpened())
      scope.project->raiseError(errInvalidFile, source);

   buffer->clear();
   DumpWriter bufWriter(buffer);
   DerivationWriter writer(&bufWriter);
   _parser.parse(&sourceFile, &writer, scope.project->getTabSize());

   // compile
   DumpReader bufReader(buffer);
   DerivationReader reader(&bufReader);

   compileModule(reader.readRoot(), scope);
}

bool Compiler :: run(Project& project)
{
   bool withDebugInfo = project.BoolSetting(opWithDebugInfo);

   MemoryDump buffer;                // temporal derivation buffer
   for(SourceIterator it = project.getSourceIt() ; !it.Eof() ; it++) {
      try {
         project.printInfo(*it);

         _Module* module = project.createModule(it.key());
         _Module* debugModule = project.createDebugModule(it.key()); // create debug module if debug info is enabled

         // compile source
         ModuleScope scope(&project, module, debugModule, it.key());

         compile(*it, &buffer, scope);

         project.saveModule(module);

         // save debug module if debug info is enabled
         if(withDebugInfo)
            project.saveDebugModule(debugModule);
      }
      catch (LineTooLong& e)
      {
         project.raiseError(errLineTooLong, it.key(), e.row);
      }
      catch (InvalidChar& e)
      {
         project.raiseError(errInvalidChar, it.key(), e.row, e.column, e.ch);
      }
      catch (SyntaxError& e)
      {
         project.raiseError(e.error, it.key(), e.row, e.column, e.token);
      }
   }

   return !project.HasWarnings();
}

