//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Engine
//
//		This file contains implematioon of the DebugController class and
//      its helpers
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#include "elena.h"
//---------------------------------------------------------------------------
#include "debugcontroller.h"
#include "ideconst.h"

using namespace _ELENA_;

inline TCHAR* getException_T(int code)
{
   switch (code) {
      case EXCEPTION_ACCESS_VIOLATION:
         return ACCESS_VIOLATION_EXCEPTION_TEXT;
      case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
         return ARRAY_BOUNDS_EXCEEDED_EXCEPTION_TEXT;
      case EXCEPTION_DATATYPE_MISALIGNMENT:
         return DATATYPE_MISALIGNMENT_EXCEPTION_TEXT;
      case EXCEPTION_FLT_DENORMAL_OPERAND:
         return FLT_DENORMAL_OPERAND_EXCEPTION_TEXT;
      case EXCEPTION_FLT_DIVIDE_BY_ZERO:
         return FLT_DIVIDE_BY_ZERO_EXCEPTION_TEXT;
      case EXCEPTION_FLT_INEXACT_RESULT:
         return FLT_INEXACT_RESULT_EXCEPTION_TEXT;
      case EXCEPTION_FLT_INVALID_OPERATION:
         return FLT_INVALID_OPERATION_EXCEPTION_TEXT;
      case EXCEPTION_FLT_OVERFLOW:
         return FLT_OVERFLOW_EXCEPTION_TEXT;
      case EXCEPTION_FLT_STACK_CHECK:
         return FLT_STACK_CHECK_EXCEPTION_TEXT;
      case EXCEPTION_FLT_UNDERFLOW:
         return FLT_UNDERFLOW_EXCEPTION_TEXT;
      case EXCEPTION_ILLEGAL_INSTRUCTION:
         return ILLEGAL_INSTRUCTION_EXCEPTION_TEXT;
      case EXCEPTION_IN_PAGE_ERROR:
         return PAGE_ERROR_EXCEPTION_TEXT;
      case EXCEPTION_INT_DIVIDE_BY_ZERO:
         return INT_DIVIDE_BY_ZERO_EXCEPTION_TEXT;
      case EXCEPTION_INT_OVERFLOW:
         return INT_OVERFLOW_EXCEPTION_TEXT;
      case EXCEPTION_INVALID_DISPOSITION:
         return INVALID_DISPOSITION_EXCEPTION_TEXT;
      case EXCEPTION_NONCONTINUABLE_EXCEPTION:
         return NONCONTINUABLE_EXCEPTION_EXCEPTION_TEXT;
      case EXCEPTION_PRIV_INSTRUCTION:
         return PRIV_INSTRUCTION_EXCEPTION_TEXT;
      case EXCEPTION_STACK_OVERFLOW:
         return STACK_OVERFLOW_EXCEPTION_TEXT;
      case ELENA_ERR_OUTOF_MEMORY:
         return GC_OUTOF_MEMORY_EXCEPTION_TEXT;
	  default:
         return UNKNOWN_EXCEPTION_TEXT;
   }
}

// --- DebugController ---

void DebugController :: debugThread()
{
   if (!_debugger.start(_debuggee, _arguments))
      return;

   _running = true;
   while (_debugger.isStarted()) {
      int event = _events.waitForAnyEvent();

      switch (event) {
         case DEBUG_ACTIVE:
            if (_debugger.Exception()) {
               processStep();
               _debugger.resetException();
               _debugger.run();
            }
            else if (!_debugger.proceed(100)) {
               _events.resetEvent(DEBUG_ACTIVE);
               processStep();
               _running = false;
            }
            else _debugger.run();

            break;
         case DEBUG_RESUME:
            _running = true;
            _debugger.run();
            _events.resetEvent(DEBUG_RESUME);
            _events.setEvent(DEBUG_ACTIVE);
            break;
         case DEBUG_SUSPEND:
            _events.resetEvent(DEBUG_ACTIVE);
            _events.resetEvent(DEBUG_SUSPEND);
            _running = false;
            break;
         case DEBUG_CLOSE:
            _debugger.stop();
            _events.setEvent(DEBUG_ACTIVE);
            _events.resetEvent(DEBUG_CLOSE);
            break;
      }
   }
   _running = false;
   _currentModule = NULL;
   _events.close();

   onStop(_debugger.proceedCheckPoint());
}

_Module* DebugController :: getDebugModule(size_t address)
{
   ModuleMap::Iterator it = _modules.start();
   while (!it.Eof()) {
      Section* section = (*it)->mapSection(DEBUG_LINEINFO_ID, true);
      if (section != NULL) {
         size_t starting = (size_t)section->getArray();
         if (starting <= address && (address - starting) < section->Length()) {
            return *it;
         }
      }
      it++;
   }
   return NULL;
}

DebugLineInfo* DebugController :: seekDebugLineInfo(size_t lineInfoAddress, const TCHAR* &moduleName)
{
   _Module* module = getDebugModule(lineInfoAddress);
   if (module) {
      moduleName = module->Name();

      return (DebugLineInfo*)lineInfoAddress;
   }
   else return NULL;
}

DebugLineInfo* DebugController :: seekClassInfo(size_t address, const TCHAR* &className)
{
   // read class VMT address
   size_t vmtPtr = _debugger.Context()->ClassVMT(address);
   if (vmtPtr==0)
      return NULL;

   // if it is role, read the role owner
   int flags = _debugger.Context()->VMTFlags(vmtPtr);  
   if (test(flags, elRoleVMT)) {
      vmtPtr = _debugger.Context()->ClassVMT(vmtPtr);
   }

   // get class debug info address
   size_t position = _classes.get(vmtPtr);
   _Module* module = getDebugModule(position);
   Section* section = (module != NULL) ? module->mapSection(DEBUG_LINEINFO_ID, true) : NULL;

   if (position != 0 && section != NULL) {
      // to resolve class name we need offset in the section rather then the real address
      className = module->resolveReference(position - (size_t)section->getArray());
      
      return (DebugLineInfo*)position;
   }
   return NULL;
}

DebugLineInfo* DebugController :: getNextStep(DebugLineInfo* step, bool alwaysReturn)
{
   if (step->symbol != dsEnd) {
      DebugLineInfo* next = &step[1];
      while (next->symbol == dsLocal) {
         next = &next[1];
      }

      if ((next->symbol & dsDebugMask) == dsStep) {
         // return next step only if its address is not equal to the current step one
         if (alwaysReturn || next->addresses.step.address != _debugger.Context()->EIP()) 
            return next;
      }
   }
   return NULL;
}

size_t DebugController :: findNearestAddress(_Module* module, size_t row, size_t col)
{
   Section* section = module->mapSection(DEBUG_LINEINFO_ID | mskDataRef, true);
   DebugLineInfo* info = (DebugLineInfo*)section->get(4);
   int count = section->Length() / sizeof(DebugLineInfo);

   // scanning the array of debug lines until closest step is found
   size_t address = (size_t)-1;
   int nearestCol = -1;
   for (size_t i = 0 ; i < count ; i++) {
      if ((info[i].symbol & dsDebugMask) == dsStep) {
         if (row == info[i].row) {
            int lineCol = info[i].col & 0xFFFF;
            if (nearestCol == -1 || ( (size_t)lineCol >= col && lineCol < nearestCol)) {
               nearestCol = lineCol;
               address = info[i].addresses.step.address;
            }
         }
      }
   }
   return address;
}

void DebugController :: processStep()
{
   if (_debugger.isTrapped()) {
      showCurrentModule();
   }
   if (_debugger.Context()->checkFailed) {
      onCheckPoint(_T("Operation failed"));
   }
   if (_debugger.Exception()!=NULL) {
      onNotification(getException_T(_debugger.Exception()->code), _debugger.Exception()->address,
         _debugger.Exception()->code);
   }
}

bool DebugController :: start()
{
   _events.init();
   _events.setEvent(DEBUG_SUSPEND);

   _debugger.startThread(this);

   while (_events.waitForEvent(DEBUG_ACTIVE, 0));

   onStart();

   return _debugger.isStarted();
}

//void DebugController :: clearDebugInfo()
//{
//   _strings.clear();
//   _symbols.clear();
//   _lines.clear();
//}

bool  DebugController :: loadDebugData(const TCHAR* path)
{
   FileReader	file(path, _T("rb+"), feRaw);

   if (file.isOpened()) {
      // load signature
      char signature[10];
      file.read(signature, getlength(DEBUG_MODULE_SIGNATURE));
      if (!compstr(signature, DEBUG_MODULE_SIGNATURE, getlength(DEBUG_MODULE_SIGNATURE)))
         return false;

      return loadDebugData(file);
   }
   else return false;
}   

bool DebugController :: loadSymbolDebugInfo(const TCHAR* reference, StreamReader& addressReader)
{
   bool isClass = true;
   _Module* module = NULL;
   // if symbol 
   if (reference[0]=='#') {
      module = loadDebugModule(reference + 1);
      isClass = false;
   }
   else module = loadDebugModule(reference);

   size_t position = (module != NULL) ? module->mapReference(reference, true) : 0;
   if (position != 0) {
      // place reader on the next after symbol record
      DumpReader reader(module->mapSection(DEBUG_LINEINFO_ID | mskDataRef, true), position);
      DumpReader stringReader(module->mapSection(DEBUG_STRINGS_ID | mskDataRef, true));

      // map vmt address for a class
      if (isClass) {
         ref_t vmtPtr = addressReader.getDWord();

         _classes.add(vmtPtr, (size_t)reader.Address());
      }

      // start to read lineinfo until end symbol
      DebugLineInfo info;
      void* current = NULL;
      int level = 1;
      while (!reader.Eof()) {
         current =  reader.Address();

         reader.read(&info, sizeof(DebugLineInfo));
         if (info.symbol == dsProcedure) {
            level++;
         }
         else if (info.symbol == dsEnd) {
            if (level == 1) {
               break;
            }
            else level--;
         }
         else if (info.symbol == dsField || info.symbol == dsLocal) {
            // replace field name reference with the name
            stringReader.seek(info.addresses.symbol.nameRef);

            ((DebugLineInfo*)current)->addresses.symbol.nameRef = (ref_t)stringReader.Address();
         }
         else if ((info.symbol & dsDebugMask) == dsStep) {
            ref_t stepAddress = addressReader.getDWord();

            ((DebugLineInfo*)current)->addresses.step.address = stepAddress;
            if (info.symbol != dsVirtualEnd)
               _debugger.addStep(stepAddress, (void*)current);
         }
      }
      return true;
   }
   else return false;
}

bool DebugController :: loadDebugData(StreamReader& reader)
{
   clearDebugInfo();

   // load entry point
   reader.readDWord(_entryPoint);

   LocalString<IDENTIFIER_LEN + 1> reference;
   while (!reader.Eof()) {
      // read reference
      reader.readString(reference);

      loadSymbolDebugInfo(reference, reader);
   }

   return true;
}

void DebugController :: loadBreakpoints(List<Breakpoint>& breakpoints)
{
   List<Breakpoint>::Iterator breakpoint = breakpoints.start();
   while (!breakpoint.Eof()) {
      _Module* module = _modules.get((*breakpoint).source);
      if (module != NULL) {
         size_t address = findNearestAddress(module, (*breakpoint).row, 0);
         if (address != 0xFFFFFFFF) {
            _debugger.addBreakpoint(address);
         }
      }
      breakpoint++;
   }
}

void DebugController :: toggleBreakpoint(Breakpoint& breakpoint, bool adding)
{
   if (_debugger.isStarted()) {
      _Module* module = _modules.get(breakpoint.source);
      if (module != NULL) {
         size_t address = findNearestAddress(module, breakpoint.row, 0);
         if (address != 0xFFFFFFFF) {
            if (adding) {
               _debugger.addBreakpoint(address);
            }
            else _debugger.removeBreakpoint(address);
         }
      }
   }
}

void DebugController :: clearBreakpoints()
{
   _debugger.clearBreakpoints();
}

bool DebugController :: start(const TCHAR* programPath, const TCHAR* arguments, bool debugMode, List<Breakpoint>& breakpoints)
{
   _currentModule = NULL;
   _started = false;

   _debugger.reset();

   _debuggee.copy(programPath);
   _arguments.copy(arguments);

   if (debugMode) {
      LocalPath debugDataPath(programPath);
      debugDataPath.changeExtension(DEBUG_FILE_EXTENSION);

      if (!loadDebugData(debugDataPath))
         return false;

      loadBreakpoints(breakpoints);
   }
   else clearBreakpoints();
   
   return start();
}

void DebugController :: run()
{
   if (_running || !_debugger.isStarted())
      return;

   _started = true;

   _events.setEvent(DEBUG_RESUME);

   _debugger.activate();
}

void DebugController :: runToCursor(const TCHAR* name, int row, int col)
{
   if (_running || !_debugger.isStarted())
      return;

   _Module* module = _modules.get(name);
   if (module != NULL) {
      size_t address = findNearestAddress(module, row, col);
      if (address != 0xFFFFFFFF) {
         _debugger.setBreakpoint(address, _started);
         _started = true;

         _events.setEvent(DEBUG_RESUME);
      }
   }
}

void DebugController :: stepOver()
{
   if (_running || !_debugger.isStarted())
      return;

   if (!_started) {
      _started = true;

      _debugger.setBreakpoint(_entryPoint, false);
   }
   else {
      DebugLineInfo* lineInfo = (DebugLineInfo*)_debugger.Context()->State();

      // if virtual step go directly to the next step
      if (lineInfo->symbol == dsVirtualStep) {
         _debugger.processVirtualStep(getNextStep(lineInfo, true));
         processStep();
         return;
      }

      // if debugger should notify on the step result
      if (test(lineInfo->symbol, dsProcedureStep))
         _debugger.setCheckMode();

      // if next step is available set the breakpoint
      DebugLineInfo* nextStep = getNextStep(lineInfo);
      if (nextStep) {
         _debugger.setBreakpoint(nextStep->addresses.step.address, true);
      }
      // else set step mode
      else _debugger.setStepMode();
   }
   _events.setEvent(DEBUG_RESUME);
}

void DebugController :: stepInto()
{
   if (_running || !_debugger.isStarted())
      return;

   if (!_started) {
      _started = true;

      _debugger.setBreakpoint(_entryPoint, true);
   }
   else {
      DebugLineInfo* lineInfo = (DebugLineInfo*)_debugger.Context()->State();

      // if virtual step go directly to the next step
      if (lineInfo->symbol == dsVirtualStep) {
         _debugger.processVirtualStep(getNextStep(lineInfo, true));
         processStep();
         return;
      }

      // if debugger should notify on the step result
      if (test(lineInfo->symbol, dsProcedureStep))
         _debugger.setCheckMode();

      // if atomic step set breakpoint
      if (test(lineInfo->symbol, dsAtomicStep)) {
         DebugLineInfo* nextStep = getNextStep(lineInfo);

         // if we have next step
         if (nextStep) {
            _debugger.setBreakpoint(nextStep->addresses.step.address, true);
         }
         // else set step mode
         else _debugger.setStepMode();
      } 
      // else set step mode
      else _debugger.setStepMode();
   }

   _events.setEvent(DEBUG_RESUME);
}

void DebugController :: stop()
{
   if (_debugger.isStarted()) {
      _events.setEvent(DEBUG_CLOSE);
   }
}

const char* DebugController :: getValue(size_t address, char* value, size_t length)
{
   _debugger.Context()->readDump(address, value, length);

   return value;
}

const wchar_t* DebugController :: getValue(size_t address, wchar_t* value, size_t length)
{
   _debugger.Context()->readDump(address, (char*)value, length * 2);

   return value;
}

int DebugController :: getEIP()
{
   return _debugger.Context()->EIP();
}

void DebugController :: readFields(_DebuggerWatch* watch, DebugLineInfo* info, size_t address)
{
   int index = 1;
   while (info[index].symbol == dsField) {
      const TCHAR* fieldName = (const TCHAR*)info[index].addresses.symbol.nameRef;

      size_t fieldPtr = _debugger.Context()->ObjectPtr(address);
      if (fieldPtr==0) {
         watch->write(this, fieldPtr, fieldName, _T("<nil>"));
      }
      else {
         const TCHAR* className = NULL;
         DebugLineInfo* field = seekClassInfo(fieldPtr, className);
         if (field) {
            watch->write(this, fieldPtr, fieldName, className);
         }
         else watch->write(this, fieldPtr, fieldName, _T("<unknown>"));
      }
      address += 4;
      index++;
   }
}

void DebugController :: readList(_DebuggerWatch* watch, int* list, int length)
{
   LocalString<10> index;
   for (int i = 0 ; i < length ; i++)  {
      index.copy(_T("["));
      index.appendInt(i);
      index.append(_T("]"));
      size_t memberPtr = list[i];
      if (memberPtr==0) {
         watch->write(this, memberPtr, index, _T("<nil>"));
      }
      else {
         const TCHAR* className = NULL;
         DebugLineInfo* item = seekClassInfo(memberPtr, className);
         if (item) {
            watch->write(this, memberPtr, index, className);
         }
         else watch->write(this, memberPtr, index, _T("<unknown>"));
      }
   }
}

void DebugController :: readObject(_DebuggerWatch* watch, ref_t address, const TCHAR* name)
{
   if (address != 0) {
      const TCHAR* className = NULL;
      DebugLineInfo* info = seekClassInfo(address, className);
      if (info != NULL) {
         watch->write(this, address, name, className);
      }
      else watch->write(this, address, name, _T("<unknown>"));
   }
   else watch->write(this, address, name, _T("<nil>"));
}

void DebugController :: readAutoContext(_DebuggerWatch* watch)
{
   if (_debugger.isStarted()) {
      const TCHAR*   moduleName = NULL;
      DebugLineInfo* lineInfo = seekDebugLineInfo((size_t)_debugger.Context()->State(), moduleName);
      int index = 0;
      while (lineInfo[index].symbol != dsProcedure) {
         // write self
         if (lineInfo[index].symbol == dsBase) {
            ref_t selfPtr = 0;
            if (lineInfo[index].addresses.local.level < 0) {
               selfPtr = _debugger.Context()->LocalPtr(lineInfo[index].addresses.local.level);
            }
   //            // write self for noninline classes only
   //            if (!test(self->header.flags, elInlineClass)) {
   //               watch->write(this, selfPtr, _T("self"), _strings.get(self->header.nameRef));
   //            }
   //            // write fields for inline classes
   //            else readFields(watch, self, selfPtr);
            readObject(watch, selfPtr, _T("self"));
         }
         else if (lineInfo[index].symbol == dsLocal) {
            // write local variable
            int localPtr = _debugger.Context()->LocalPtr(lineInfo[index].addresses.local.level);
            readObject(watch, localPtr, (const TCHAR*)lineInfo[index].addresses.local.nameRef);
         }
         index--;
      }
   }
}

void DebugController :: readContext(_DebuggerWatch* watch, size_t selfPtr)
{
   if (_debugger.isStarted()) {
      const TCHAR* className = NULL;
      DebugLineInfo* info = seekClassInfo(selfPtr, className);
      if (info) {
         int type = info->addresses.symbol.flags & elDebugMask;
         if (type==elDebugLiteral) {
            wchar_t value[261];
            getValue(selfPtr, value, 260);

            int length = *(int*)value;
            if (length > 255) {
               value[2 + 256] = 0;
            }
            else value[2 + length] = 0;

            watch->write(this, value + 2);
         }
         else if (type==elDebugDWORD) {
            char value[4];
            getValue(selfPtr, value, 4);

            watch->write(this, *(int*)value);
         }
         else if (type==elDebugReal64) {
            char value[8];
            getValue(selfPtr, value, 8);

            watch->write(this, *(double*)value);
         }
         else if (type==elDebugQWORD) {
            char value[8];
            getValue(selfPtr, value, 8);

            watch->write(this, *(__int64*)value);
         }
         else if (type==elDebugArray) {
            int list[100];
            getValue(selfPtr, (char*)list, 400);

            int length = 0;
            getValue(selfPtr - 8, (char*)&length, 4);

            if (length > 100)
               length = 100;		// !!

            readList(watch, list, length);
         }
         else if (compstr(className, NIL_CLASS)) {
            watch->write(this, _T("<nil>"));
         }
         else readFields(watch, info, selfPtr);
      }
   }
}

//void DebugController :: readRegisters(_DebuggerWatch* watch)
//{
//   watch->write(this, _debugger.Context()->EIP(), _T("EIP"), EMPTY_STRING);
//}

void DebugController :: showCurrentModule()
{
   const TCHAR*   moduleName = NULL;
   DebugLineInfo* lineInfo = seekDebugLineInfo((size_t)_debugger.Context()->State(), moduleName);

   if (lineInfo) {
      if (!compstr(_currentModule, moduleName)) {
         onLoadModule(moduleName);
         _currentModule = moduleName;
      }
      onStep(moduleName, lineInfo->row, lineInfo->col, lineInfo->length);
   }
}
