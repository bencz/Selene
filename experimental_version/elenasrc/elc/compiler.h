//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Compiler
//
//		This file contains ELENA compiler class.
//
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef compilerH
#define compilerH 1

#include "project.h"
#include "bccompiler.h"
#include "parser.h"

namespace _ELENA_
{

// --- Compiler class ---
class Compiler
{
   typedef Map<const TCHAR*, const TCHAR*, false> NamespaceMaskMap;
   typedef Map<const TCHAR*, ref_t, false>        ForwardMap;
   typedef Map<const TCHAR*, ref_t, false>        LocalMap;

   // InheritResult
   enum InheritResult
   {
      irNone = 0,
      irSuccessfull,
      irUnsuccessfull,
      irObsolete
   };

   // - ModuleScope -
   struct ModuleScope
   {
      struct Unresolved
      {
         TerminalInfo terminal;
         ref_t        reference; 

         Unresolved()
         {
            this->reference = 0;
         }
         Unresolved(TerminalInfo terminal, ref_t reference)
         {
            this->terminal = terminal;
            this->reference = reference;
         }
      };

      Project*     project;
      _Module*     module;
      _Module*     debugModule;
      const TCHAR* sourcePath;

      // short cuts
      NamespaceMaskMap masks;
      ForwardMap       forwards;

      // symbol hints
      Map<ref_t, ObjectType> symbolHints;

      // warning unresolved
      bool warnOnUnresolved;
      bool warnOnWeakUnresolved;

      // list of references to the current module which should be checked after the module compiled
      List<Unresolved> forwardsUnresolved;     

      ObjectInfo mapObject(TerminalInfo identifier);

      ObjectInfo mapReference(const TCHAR* reference, bool existing);
      ObjectInfo mapReference(const TCHAR* reference)
      {
         return mapReference(reference, false);
      }

      bool defineMask(const TCHAR* mask, const TCHAR* reference)
      {
         return masks.add(mask, reference, true);
      }
      bool defineForward(const TCHAR* forward, const TCHAR* referenceName)
      {
         ObjectInfo info = mapReference(referenceName, false);

         return forwards.add(forward, info.reference, true);
      }

      void compileForwardHints(DNode hints, const TCHAR* forward);

      ref_t defineConstant(const TCHAR* constant)
      {
         ref_t constRef = module->mapReference(constant);

         symbolHints.add(constRef, otConstant, true);

         return constRef;
      }
      void defineConstant(ref_t reference)
      {
         symbolHints.add(reference, otConstant, true);
      }

      const TCHAR* matchMask(const TCHAR* reference);

      void raiseError(const TCHAR* message, TerminalInfo terminal);
      void raiseWarning(const TCHAR* message, TerminalInfo terminal);

      ref_t mapSymbol(TerminalInfo terminal)
      {
         return mapSymbol(terminal, false);
      }

      ref_t mapSymbol(TerminalInfo terminal, bool existing)
      {
         if (terminal != tsReference) {
            ref_t reference = forwards.get(terminal);
            if (reference == 0) {
               LocalReferenceName name(module->Name(), terminal);

               return mapReference(name, existing).reference;
            }
            else return reference;
         }
         else return mapReference(terminal, existing).reference;
      }

      ObjectInfo getObjectInfo(ref_t reference)
      {
         // if reference is zero the symbol is unknown
         if (reference == 0) {
            return ObjectInfo();
         }
         // check if symbol should be treated like constant one
         else if (symbolHints.exist(reference, otConstant)) {
            return ObjectInfo(otConstant, reference);
         }
         // otherwise it is a normal one
         else return ObjectInfo(otSymbol, reference);
      }

      void validateReference(TerminalInfo terminal, ref_t reference);
      void validateForwards();

      ModuleScope(Project* project, _Module* module, _Module* debugModule, const TCHAR* sourcePath);
   };

   // - Scope -
   struct Scope
   {
      enum ScopeLevel
      {
         slClass,
         slSymbol,
         slMethod,
         slCode
      };

      ModuleScope* moduleScope;
      Scope*       parent;

      void raiseError(const TCHAR* message, TerminalInfo terminal)
      {
         moduleScope->raiseError(message, terminal);
      }

      void raiseWarning(const TCHAR* message, TerminalInfo terminal)
      {
         moduleScope->raiseWarning(message, terminal);
      }

      virtual ObjectInfo mapObject(TerminalInfo identifier)
      {
         if (parent) {
            return parent->mapObject(identifier);
         }
         else return moduleScope->mapObject(identifier);
      }

      virtual Scope* getScope(ScopeLevel level)
      {
         if (parent) {
            return parent->getScope(level);
         }
         else return NULL;
      }

      Scope(ModuleScope* moduleScope)
      {
         this->parent = NULL;
         this->moduleScope = moduleScope;
      }
      Scope(Scope* parent)
      {
         this->parent = parent;
         this->moduleScope = parent->moduleScope;
      }
   };

   // - SourceScope -
   struct SourceScope : public Scope
   {
      CommandTape tape;
      ref_t       reference;

      SourceScope(Scope* parent);
      SourceScope(ModuleScope* parent, ref_t reference);
   };

   // - ClassScope -
   struct ClassScope : public SourceScope
   {
      ClassInfo info;

      virtual ObjectInfo mapObject(TerminalInfo identifier);
      virtual ObjectInfo mapRole(TerminalInfo identifier);

      void compileHints(DNode hints);
      void setDebugWatchHints(DNode hintValue);

      virtual Scope* getScope(ScopeLevel level)
      {
         if (level == slClass) {
            return this;
         }
         else return Scope::getScope(level);
      }

      void save(ByteCodeCompiler& coder)
      {
         // save class meta data
         SectionWriter metaWriter(moduleScope->module->mapSection(reference | mskMetaDataRef, false));
         info.save(&metaWriter);

         // create byte code sections
         coder.save(reference, tape, moduleScope->module, moduleScope->debugModule);
      }

      ClassScope(ModuleScope* parent, ref_t reference);
   };

   // - RoleScope -
   struct RoleScope : public ClassScope
   {
      virtual ObjectInfo mapObject(TerminalInfo identifier);

      RoleScope(ClassScope* parent, ref_t reference);
   };

   // - SymbolScope -
   struct SymbolScope : public SourceScope
   {
      const TCHAR* parameter;

      void compileHints(DNode hints);

      virtual ObjectInfo mapObject(TerminalInfo identifier);

      virtual Scope* getScope(ScopeLevel level)
      {
         if (level == slSymbol) {
            return this;
         }
         else return Scope::getScope(level);
      }

      SymbolScope(ModuleScope* parent, ref_t reference);
   };

   // - MethodScope -
   struct MethodScope : public Scope
   {
      bool         isDefaultMethod;

      ref_t        messageRef;
      const TCHAR* param;

      void compileHints(DNode hints);

      virtual Scope* getScope(ScopeLevel level)
      {
         if (level == slMethod) {
            return this;
         }
         else return parent->getScope(level);
      }

      virtual ObjectInfo mapObject(TerminalInfo identifier);

      MethodScope(ClassScope* parent, ref_t messageRef);
   };

   // - CodeScope -
   struct CodeScope : public Scope
   {
      CommandTape* tape;
      LocalMap     locals;
      int          level;
      ref_t        methodRef;

      int newLocal(const TCHAR* local)
      {
         return mapKey(locals, local, level + locals.Count() + 1);
      }

      virtual ObjectInfo mapObject(TerminalInfo identifier);

      virtual Scope* getScope(ScopeLevel level)
      {
         if (level == slCode) {
            return this;
         }
         else return parent->getScope(level);
      }

      CodeScope(SourceScope* parent);
      CodeScope(MethodScope* parent);
      CodeScope(CodeScope* parent);
   };

   // - InlineClassScope -

   struct InlineClassScope : public ClassScope
   {
      struct Outer
	  {
         int        reference;
         ObjectInfo outerObject;

         Outer()
         {
            reference = -1;
         }
         Outer(int reference, ObjectInfo outerObject)
         {
            this->reference = reference;
            this->outerObject = outerObject;
         }
      };

      Map<const TCHAR*, Outer> outers;

      virtual ObjectInfo mapObject(TerminalInfo identifier);

      InlineClassScope(CodeScope* owner, ref_t reference);
   };

   typedef Map<ref_t, RoleScope*, false> RoleMap;

   ByteCodeCompiler _coder;
   Parser           _parser;

   MessageMap       _predefined;                     // list of predefined messages

   void loadPredefinedMessages();

   void recordStep(CodeScope& scope, TerminalInfo terminal, int stepType)
   {
      if (terminal != nsNone) {
         _coder.newBreakpoint(*scope.tape, terminal.row, terminal.disp, terminal.length, stepType);
      }
   }

   ref_t mapDefaultMessage(const TCHAR* message, ModuleScope* moduleScope);
   ref_t mapProtectedMessage(const TCHAR* message, ModuleScope* moduleScope);
   ref_t mapMessage(TerminalInfo terminal, ModuleScope* moduleScope);

   ref_t mapSymbolExpression(CodeScope& scope, int mode);

   void inheritRoles(ClassScope& scope, RoleMap& roles);

   InheritResult inheritClass(ClassScope& scope, ref_t parentRef, RoleMap& roles);

   ObjectInfo compileTerminal(TerminalInfo terminal, CodeScope& scope, int mode);

   void compileMessage(DNode node, CodeScope& scope, ObjectInfo object, int mode);
   void compileOperations(DNode node, CodeScope& scope, ObjectInfo object, int mode);
   void compileControlChain(DNode node, CodeScope& scope, ObjectInfo object, int mode);

   void compileSymbolExpression(DNode node, CodeScope& ownerScope, int mode);
   void compileSymbolExpression(DNode node, CodeScope& ownerScope, InlineClassScope& scope, int mode);

   void compileExternalFunction(DNode node, CodeScope& scope, int mode);
   void compileEmbeddedExpression(DNode node, CodeScope& scope, int mode);

   void compileCollection(DNode objectNode, CodeScope& scope, int mode);
   void compileCollection(DNode objectNode, CodeScope& scope, int mode, ref_t vmtReference);

   void compileGroup(DNode objectNode, CodeScope& scope, int mode, bool isBroadcasting);
   void compileExtend(DNode objectNode, CodeScope& scope, int mode);
   void compileType(DNode objectNode, CodeScope& scope, int mode);
   void compileShift(DNode node, CodeScope& scope);
   void compileUnShift(DNode node, CodeScope& scope);
   ObjectInfo compileObject(DNode objectNode, CodeScope& scope, int mode);
   void compileExpression(DNode node, CodeScope& scope, int mode);
   void compileControlExpression(DNode node, CodeScope& scope, int mode);
   void compileLoop(DNode node, CodeScope& scope, int mode);
   void compileAssignment(DNode node, CodeScope& scope, TerminalInfo variable);
   void compileVariable(DNode node, CodeScope& scope);

   void compileCode(DNode node, CodeScope& scope);
   void compileMethod(DNode node, MethodScope& scope);
   void compileRedirectMethod(DNode node, MethodScope& scope, bool overridden);
   void compileIdleMethod(ClassScope& scope);

   void compileVMT(DNode member, ClassScope& scope);
   void compileRoleDeclarations(DNode& member, ClassScope& scope, RoleMap& roles);
   void compileRoleDeclarations(ClassScope& scope, RoleMap& roles)
   {
      DNode idle;
      compileParentDeclaration(idle, scope, roles);
   }
   void compileFieldDeclarations(DNode& member, ClassScope& scope);
   void compileParentDeclaration(DNode node, ClassScope& scope, RoleMap& roles);

   void compileSymbolCode(ClassScope& scope);

   void compileClassDeclaration(DNode node, ClassScope& scope, DNode hints);
   void compileSymbolDeclaration(DNode node, SymbolScope& scope, DNode hints, bool isStatic);

   void compileDirectives(DNode& node, ModuleScope& scope);
   void compileExternalDeclaration(DNode hints, TerminalInfo reference,
                                   ModuleScope& scope);
   void compileModule(DNode node, ModuleScope& scope);

   void compile(const TCHAR* source, MemoryDump* buffer, ModuleScope& scope);

public:
   bool run(Project& project);

   Compiler(StreamReader* syntax);
};

} // _ELENA_

#endif // compilerH
