//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Engine
//               
//		This file contains the Debugger class and its helpers header
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef debuggerH
#define debuggerH

#include "windows.h"

namespace _ELENA_
{

// --- EventManager ---

#define DEBUG_CLOSE	     0
#define DEBUG_SUSPEND    1
#define DEBUG_RESUME     2
#define DEBUG_ACTIVE	 3
#define MAX_DEBUG_EVENT  4

class DebugEventManager
{
   HANDLE _events[MAX_DEBUG_EVENT];
      
public:
   void init();    
   void resetEvent(int event);
   void setEvent(int event);
   int  waitForAnyEvent();
   bool waitForEvent(int event, int timeout);
   void close();
      
   DebugEventManager()
   {
      for (int i = 0 ; i < MAX_DEBUG_EVENT ; i++)
         _events[i] = NULL;
   }   
   ~DebugEventManager()
   {
      close();                
   }
};

// --- Controller ---

class _Controller
{
public:
   virtual void debugThread() = 0;

   virtual ~_Controller() {}   
};

// --- ProcessException ---

struct ProcessException
{
   int code;
   int address;

   ProcessException()
   {
      code = 0;
   }
};

// --- ProcessContext ---

struct ProcessContext
{
   friend class Debugger;
   friend class BreakpointContext;
   
protected:
   void*   state;
   DWORD   dwProcessId;
   DWORD   dwThreadId;
   HANDLE  hProcess;
   HANDLE  hThread;
   CONTEXT context;

public:
   bool breakpointFlag;
   bool atCheckPoint;
   bool checkFailed;

   void* State() const { return state; }
   size_t EIP() const { return context.Eip; }
   size_t Self() const { return context.Edi; }
   size_t Local(int offset) { return context.Ebp - offset * 4; }
   size_t Current(int offset) { return context.Esp + offset * 4; }
   size_t ClassVMT(size_t address);
   size_t VMTFlags(size_t address);
   size_t ObjectPtr(size_t address);
   size_t LocalPtr(int offset) { return ObjectPtr(Local(offset)); }
   size_t CurrentPtr(int offset) { return ObjectPtr(Current(offset)); }

   void readDump(size_t address, char* dump, size_t length);
   void writeDump(size_t address, char* dump, size_t length);

   void init(HANDLE hProcess, HANDLE hThread);
   void refresh(DWORD dwProcessId, DWORD dwThreadId);

   void setCheckPoint();
   void setTrapFlag();
   void resetTrapFlag();
   void setHardwareBreakpoint(size_t breakpoint);
   unsigned char setSoftwareBreakpoint(size_t breakpoint);
   void setEIP(size_t address);

   void clearHardwareBreakpoint();
   void clearSoftwareBreakpoint(size_t breakpoint, char substitute);
   void reset();

   ProcessContext();
};

// --- BreakpointContext ---

struct BreakpointContext
{
   Map<size_t, char> breakpoints;
   bool              softwareBreakpoint;
   size_t            nextBreakpoint;
   size_t            stackLevel;

   void addBreakpoint(size_t address, ProcessContext& context, bool started);
   void removeBreakpoint(size_t address, ProcessContext& context, bool started);
   void setSoftwareBreakpoints(ProcessContext& context);
   void setHardwareBreakpoint(size_t address, ProcessContext& context, bool withStackLevelControl);

   bool processStep(ProcessContext& context, bool stepMode);
   bool processBreakpoint(ProcessContext& context);

   void clear();

   BreakpointContext();
};

// --- Debugger ---

class Debugger
{
   DWORD             threadId;

   bool              started;
   bool              trapped;
   bool              stepMode;
   bool              needToHandle;

   ProcessContext    context;
   BreakpointContext breakpoints;

   size_t            minAddress, maxAddress;

   MemoryMap<int, void*> steps;

   ProcessException exception;

   bool startProcess(const TCHAR* exePath, const TCHAR* cmdLine);
   void processEvent(size_t timeout);
   void processException(EXCEPTION_DEBUG_INFO* exception);
   void continueProcess();

   void processStep();

public:
   bool isStarted() const { return started; }
   bool isTrapped() const { return trapped; }

   ProcessContext* Context() { return &context; }
   ProcessException* Exception() { return exception.code == 0 ? NULL : &exception; }

   void resetException() { exception.code = 0; }

   void addStep(size_t address, void* state);
    
   void addBreakpoint(size_t address);
   void removeBreakpoint(size_t address);
   void clearBreakpoints();
   
   void setStepMode();
   void setBreakpoint(size_t address, bool withStackLevelControl);
   void setCheckMode();
   
   bool startThread(_Controller* controller);
   
   bool start(const TCHAR* exePath, const TCHAR* cmdLine);   
   void run();
   bool proceed(size_t timeout);
   void stop();

   void processVirtualStep(void* step);
   bool proceedCheckPoint();

   void reset();   

   void activate();

   Debugger();
};

// --- SetForegroundWindow() ---
inline void SetForegroundWindow(HWND hwnd)
{
   DWORD dwTimeoutMS;
   // Get the current lock timeout.
   ::SystemParametersInfo (0x2000, 0, &dwTimeoutMS, 0);

   // Set the lock timeout to zero
   ::SystemParametersInfo (0x2001, 0, 0, 0);

   // Perform the SetForegroundWindow
   ::SetForegroundWindow (hwnd);

   // Set the timeout back
   ::SystemParametersInfo (0x2001, 0, (LPVOID)dwTimeoutMS, 0);   //HWND hCurrWnd;
}

} // _ELENA_

#endif // debuggerH
