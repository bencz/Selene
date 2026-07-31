//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Engine
//               
//		This file contains the DebugController class and its helpers header
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef debugcontrollerH
#define debugcontrollerH

#include "win32/debugger.h"
#include "elena.h"

namespace _ELENA_
{

class DebugController;

// --- _DebuggerWatch ---

class _DebuggerWatch
{
public:
   virtual void write(DebugController* controller, size_t address, const TCHAR* variableName, const TCHAR* className) = 0;
   virtual void write(_ELENA_::DebugController* controller, const wchar_t* value) = 0;
   virtual void write(_ELENA_::DebugController* controller, const char* value) = 0;
   virtual void write(_ELENA_::DebugController* controller, int value) = 0;
   virtual void write(_ELENA_::DebugController* controller, double value) = 0;
   virtual void write(_ELENA_::DebugController* controller, __int64 value) = 0;

   virtual ~_DebuggerWatch() {}
};

// --- Breakpoint ---

struct Breakpoint
{
   const TCHAR* source;
   size_t       row;
   size_t       address;

   Breakpoint()
   {
      source = NULL;
      address = 0xFFFFFFFF;
   }
   Breakpoint(const TCHAR* source, size_t row)
   {
      this->source = source;
      this->row = row;
      this->address = 0xFFFFFFFF;
   }
};

// --- DebugController ---

class DebugController : _Controller
{
   // class mapping between vmt ptr and debuglineinfo position in debug module
   typedef MemoryMap<ref_t, size_t, false> ClassInfoMap;

   Debugger                 _debugger;
   DebugEventManager        _events;

   ClassInfoMap             _classes; 

   bool                     _started;
   bool                     _running;
   String                   _debuggee;
   String                   _arguments;

   size_t			          _entryPoint;

   const TCHAR*             _currentModule;

   bool loadSymbolDebugInfo(const TCHAR* reference, StreamReader& addressReader);

   bool loadDebugData(const TCHAR* path);
   bool loadDebugData(StreamReader& reader);
   
   void loadBreakpoints(List<Breakpoint>& breakpoints);

   _Module* getDebugModule(size_t address);
   DebugLineInfo* seekDebugLineInfo(size_t address, const TCHAR* &moduleName);
   DebugLineInfo* seekClassInfo(size_t address, const TCHAR* &className);

   DebugLineInfo* getNextStep(DebugLineInfo* step, bool alwaysReturn = false);

   size_t findNearestAddress(_Module* module, size_t row, size_t col);

   void readFields(_DebuggerWatch* watch, DebugLineInfo* self, size_t address);
   void readList(_DebuggerWatch* watch, int* list, int length);
   void readObject(_DebuggerWatch* watch, ref_t selfPtr, const TCHAR* name);

   const char* getValue(size_t address, char* value, size_t length);
   const wchar_t* getValue(size_t address, wchar_t* value, size_t length);
//   SymbolInfo* getObject(size_t address);   

   bool start();

   void processStep();

protected:
   ModuleMap _modules;

   virtual _ELENA_::_Module* loadDebugModule(const TCHAR* reference) = 0;

   virtual void debugThread();

   virtual void onStart() = 0;
   virtual void onLoadModule(const TCHAR* name) = 0;
   virtual void onStep(const TCHAR* source, int row, int disp, int length) = 0;
   virtual void onStop(bool failed) = 0;
   virtual void onCheckPoint(const TCHAR* message) = 0;
   virtual void onNotification(const TCHAR* message, size_t address, int code) = 0;

   virtual void clearDebugInfo()
   {
      _classes.clear();
   }

public:
   void toggleBreakpoint(Breakpoint& breakpoint, bool adding);

   bool isStarted() const { return _debugger.isStarted(); }

   int getEIP();

   void clearBreakpoints();

   void readAutoContext(_DebuggerWatch* watch);
   void readContext(_DebuggerWatch* watch, size_t selfPtr);
   //void readRegisters(_DebuggerWatch* watch);

   void release()
   {
      _debugger.reset();
      clearDebugInfo();
   }

   void showCurrentModule();

   bool start(const TCHAR* programPath, const TCHAR* arguments, bool debugMode, List<Breakpoint>& _breakpoints);
   void run();
   void runToCursor(const TCHAR* name, int col, int row);
   void stepOver();
   void stepInto();
   void stop();

   DebugController()
      : _modules(NULL, freeobj)
   {
      _started = false;
      _currentModule = NULL;
      _running = false;
   }
   virtual ~DebugController()
   {
   }
};

}

#endif // debugcontrollerH
