//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Engine
//               
//		This file contains the Debugger class and its helpers implementation
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#include "elena.h"
//---------------------------------------------------------------------------
#include "debugger.h"

using namespace _ELENA_;

// --- main thread that is the debugger residing over a debuggee ---

BOOL WINAPI debugEventThread(_Controller* controller)
{
   controller->debugThread();

   ExitThread(TRUE);

   return TRUE;
}

// --- DebugEventManager ---

void DebugEventManager :: init()
{
   _events[DEBUG_ACTIVE] = CreateEvent(NULL, TRUE, TRUE, NULL);
   _events[DEBUG_CLOSE] = CreateEvent(NULL, TRUE, FALSE, NULL);
   _events[DEBUG_SUSPEND] = CreateEvent(NULL, TRUE, FALSE, NULL);
   _events[DEBUG_RESUME] = CreateEvent(NULL, TRUE, FALSE, NULL);
}

void DebugEventManager :: resetEvent(int event)
{
   ResetEvent(_events[event]);
}

void DebugEventManager :: setEvent(int event)
{
   SetEvent(_events[event]);
}

int DebugEventManager :: waitForAnyEvent()
{
   return WaitForMultipleObjects (MAX_DEBUG_EVENT, _events, FALSE, INFINITE);
}

bool DebugEventManager :: waitForEvent(int event, int timeout)
{
   return (WaitForSingleObject(_events[event], timeout)==WAIT_OBJECT_0);
}

void DebugEventManager :: close()
{
   for (int i = 0 ; i < MAX_DEBUG_EVENT ; i++) {
      if (_events[i]) {
         CloseHandle(_events[i]);
         _events[i] = NULL;
      }
   }
}

// --- ProcessContext ---

ProcessContext :: ProcessContext()
{
   hProcess = NULL;
   hThread = NULL;
   state = NULL;
   breakpointFlag = false;
   atCheckPoint = false;
}

void ProcessContext :: init(HANDLE hProcess, HANDLE hThread)
{
   this->hProcess = hProcess;
   this->hThread = hThread;
}

void ProcessContext :: refresh(DWORD dwProcessId, DWORD dwThreadId)
{
   this->dwProcessId = dwProcessId;
   this->dwThreadId = dwThreadId;
   if (hThread!=NULL) {
      context.ContextFlags = CONTEXT_FULL;
      GetThreadContext(hThread, &context);
      if (context.SegFs==0) {                                 // !! hotfix
         context.SegFs=0x38;
         SetThreadContext(hThread, &context);
      }
   }
}

void ProcessContext :: setCheckPoint()
{
   atCheckPoint = true;
}

void ProcessContext :: setTrapFlag()
{
   if (hThread!=NULL) {
      context.ContextFlags = CONTEXT_CONTROL;
      context.EFlags |= 0x100;
      SetThreadContext(hThread, &context);
   }
}

void ProcessContext :: resetTrapFlag()
{
   if (hThread!=NULL) {
      context.ContextFlags = CONTEXT_CONTROL;
      context.EFlags &= ~0x100;
      SetThreadContext(hThread, &context);
   }
}

void ProcessContext :: setHardwareBreakpoint(size_t breakpoint)
{
   if (hThread!=NULL) {
      context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
      context.Dr0 = breakpoint;
      context.Dr7 = 0x000001;
      SetThreadContext(hThread, &context);
      breakpointFlag = true;
   }
}

void ProcessContext :: clearHardwareBreakpoint()
{
   context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
   context.Dr0 = 0x0;
   context.Dr7 = 0x0;
   SetThreadContext(hThread, &context);
   breakpointFlag = false;
}

void ProcessContext :: clearSoftwareBreakpoint(size_t breakpoint, char substitute)
{
   writeDump(breakpoint, &substitute, 1);
}

unsigned char ProcessContext :: setSoftwareBreakpoint(size_t breakpoint)
{
   unsigned char code;
   unsigned char terminator = 0xCC;

   readDump(breakpoint, (char*)&code, 1);
   writeDump(breakpoint, (char*)&terminator, 1);

   return code;
}

void ProcessContext :: readDump(size_t address, char* dump, size_t length)
{
   unsigned long   size = 0;

   ReadProcessMemory(hProcess, (void*)(address), dump, length, &size);
}

void ProcessContext :: writeDump(size_t address, char* dump, size_t length)
{
   unsigned long   size = 0;

   WriteProcessMemory(hProcess, (void*)(address), dump, length, &size);
}

size_t ProcessContext :: ClassVMT(size_t objectPtr)
{
   int             dump = -1;
   unsigned long   size = 0;

   ReadProcessMemory(hProcess, (void*)(objectPtr - 4), &dump, 4, &size);

   return dump;
}

size_t ProcessContext :: VMTFlags(size_t vmtPtr)
{
   int             dump = -1;
   unsigned long   size = 0;

   ReadProcessMemory(hProcess, (void*)(vmtPtr - 8), &dump, 4, &size);

   return dump;
}

size_t ProcessContext :: ObjectPtr(size_t address)
{
   int             dump = -1;
   unsigned long   size = 0;

   ReadProcessMemory(hProcess, (void*)(address), &dump, 4, &size);

   return dump;
}

void ProcessContext :: setEIP(size_t address)
{
   if (hThread!=NULL) {
      context.ContextFlags = CONTEXT_CONTROL;
      GetThreadContext(hThread, &context);
      context.Eip = address;
      SetThreadContext(hThread, &context);
   }
}

void ProcessContext :: reset()
{
   state = NULL;
   hProcess = NULL;
   hThread = NULL;

   breakpointFlag = false;
   atCheckPoint = false;
   checkFailed = false;

   context.Dr0 = 0x0;
   context.Dr7 = 0x0;
   context.ContextFlags = 0;
   context.Ebp = 0;

   //clearBreakpoint();
}

// --- BreakpointContext ---

BreakpointContext :: BreakpointContext()
{
   softwareBreakpoint = false;
   nextBreakpoint = 0;
   stackLevel = 0;
}

void BreakpointContext :: addBreakpoint(size_t address, ProcessContext& context, bool started)
{
   if (started) {
      breakpoints.add(address, context.setSoftwareBreakpoint(address));
   }
   else breakpoints.add(address, 0);
}

void BreakpointContext :: removeBreakpoint(size_t address, ProcessContext& context, bool started)
{
   if (started) {
      context.clearSoftwareBreakpoint(address, breakpoints.get(address));
      if (softwareBreakpoint && nextBreakpoint==address) {
         softwareBreakpoint = false;
         nextBreakpoint = 0;
         context.resetTrapFlag();
      }
   }
   breakpoints.erase(address);
}

void BreakpointContext :: setSoftwareBreakpoints(ProcessContext& context)
{
   Map<size_t, char>::Iterator breakpoint = breakpoints.start();
   while (!breakpoint.Eof()) {
      *breakpoint = context.setSoftwareBreakpoint(breakpoint.key());

      breakpoint++;
   }
}

void BreakpointContext :: setHardwareBreakpoint(size_t address, ProcessContext& context, bool withStackControl)
{
   if (address==context.context.Eip) {
      context.setTrapFlag();
      nextBreakpoint = address;
   }
   else context.setHardwareBreakpoint(address);

   if (withStackControl) {
      stackLevel = context.context.Ebp;
   }
   else stackLevel = 0;
}

bool BreakpointContext :: processStep(ProcessContext& context, bool stepMode)
{
   if (nextBreakpoint != 0) {
      if (softwareBreakpoint) {
         context.setSoftwareBreakpoint(nextBreakpoint);
         softwareBreakpoint = false;
      }
      else context.setHardwareBreakpoint(nextBreakpoint);
      nextBreakpoint = 0;
      if (stepMode)
         context.setTrapFlag();

      return true;
   }

   if (context.breakpointFlag) {
      context.clearHardwareBreakpoint();

      // check stack level to skip recursive entries
      if (context.context.Ebp < stackLevel) {
         nextBreakpoint = context.context.Eip;
         context.setTrapFlag();
         return true;
      }      
   }

   return false;
}

bool BreakpointContext :: processBreakpoint(ProcessContext& context)
{
   if (breakpoints.exist(context.context.Eip - 1)) {
      nextBreakpoint = context.context.Eip - 1;

      context.setEIP(nextBreakpoint);
      char substitute = breakpoints.get(nextBreakpoint);
      context.writeDump(nextBreakpoint, &substitute, 1);

      softwareBreakpoint = true;

      return true;
   }
   else return false;
}

void BreakpointContext :: clear()
{
   breakpoints.clear();
   softwareBreakpoint = false;
   nextBreakpoint = 0;
   stackLevel = 0;
}

// --- Debugger ---

Debugger :: Debugger()
{
   started = false;
   threadId = 0;
//   clear();
}

bool Debugger :: startProcess(const TCHAR* exePath, const TCHAR* cmdLine)
{
   PROCESS_INFORMATION pi = { NULL, NULL, 0, 0 };
   STARTUPINFO         si;
   Path				   currentPath;

   currentPath.copyPath(exePath);

   memset(&si, 0, sizeof(si));

   si.dwFlags = STARTF_USESHOWWINDOW;
   si.wShowWindow = SW_SHOWNORMAL;

   if (!CreateProcess(exePath, (TCHAR*)cmdLine, NULL, NULL, FALSE,
	   CREATE_NEW_CONSOLE | DEBUG_PROCESS, NULL, currentPath, &si, &pi))
   {
      return false;
   }

   if (pi.hProcess)
      CloseHandle(pi.hProcess);

   if (pi.hThread)
      CloseHandle(pi.hThread);

   started = true;
   exception.code = 0;
   needToHandle = false;

   return true;
}

void Debugger :: processEvent(size_t timeout)
{
   DEBUG_EVENT event;

   trapped = false;
   if (WaitForDebugEvent(&event, timeout)) {
      context.refresh(event.dwProcessId, event.dwThreadId);
      switch (event.dwDebugEventCode) {
         case CREATE_PROCESS_DEBUG_EVENT:
            context.init(event.u.CreateProcessInfo.hProcess, event.u.CreateProcessInfo.hThread);
            breakpoints.setSoftwareBreakpoints(context);

            ::CloseHandle(event.u.CreateProcessInfo.hFile);
            break;
         case EXIT_PROCESS_DEBUG_EVENT:
            context.hProcess = NULL;
            context.hThread = NULL;
            started = false;
            break;
         case CREATE_THREAD_DEBUG_EVENT:
            break;
         case EXIT_THREAD_DEBUG_EVENT:
            started = false;
            break;
         case LOAD_DLL_DEBUG_EVENT:
            ::CloseHandle(event.u.LoadDll.hFile);
            break;
         case UNLOAD_DLL_DEBUG_EVENT:
            break;
         case OUTPUT_DEBUG_STRING_EVENT:
            break;
         case RIP_EVENT:
            started = false;
            break;
         case EXCEPTION_DEBUG_EVENT:
            processException(&event.u.Exception);
            context.refresh(event.dwProcessId, event.dwThreadId);
            break;
      }
   }
}

void Debugger :: processException(EXCEPTION_DEBUG_INFO* exception)
{
   switch (exception->ExceptionRecord.ExceptionCode) {
      case EXCEPTION_SINGLE_STEP:
         if (breakpoints.processStep(context, stepMode))
            break;

         if (context.context.Eip >= minAddress && context.context.Eip <= maxAddress) {
            processStep();
         }
         if (!trapped)
            context.setTrapFlag();

         break;
      case EXCEPTION_BREAKPOINT:
         if (breakpoints.processBreakpoint(context)) {
            context.state = steps.get(context.context.Eip);
            trapped = true;
            stepMode = false;
            context.setTrapFlag();
         }
         break;
      default:
         if (exception->dwFirstChance != 0) {
            needToHandle = true;
         }
         else {
            this->exception.code = exception->ExceptionRecord.ExceptionCode;
            this->exception.address = (int)exception->ExceptionRecord.ExceptionAddress;
            TerminateProcess(context.hProcess, 1);
         }
         break;
   }
}

void Debugger :: processStep()
{
   context.state = steps.get(context.context.Eip);
   if (context.state != NULL) {
      trapped = true;
      stepMode = false;
      proceedCheckPoint();
   }
}

bool Debugger :: proceedCheckPoint()
{
   if (context.atCheckPoint) {
      context.checkFailed = (context.context.Eax == 0);
      context.atCheckPoint = false;
   }
   else context.checkFailed = false;

   return context.checkFailed;
}

void Debugger :: processVirtualStep(void* state)
{
   context.state = state;
}

void Debugger :: continueProcess()
{
   int code = needToHandle ? DBG_EXCEPTION_NOT_HANDLED : DBG_CONTINUE;

   ContinueDebugEvent(context.dwProcessId, context.dwThreadId, code);

   needToHandle = false; 
}

void Debugger :: addStep(size_t address, void* state)
{
   steps.add(address, state);
   if (address < minAddress)
      minAddress = address;

   if (address > maxAddress)
      maxAddress = address;
}

void Debugger :: setStepMode()
{
   context.setTrapFlag();
   stepMode = true;
}

void Debugger :: setBreakpoint(size_t address, bool withStackLevelControl)
{
   breakpoints.setHardwareBreakpoint(address, context, withStackLevelControl);
}

void Debugger :: setCheckMode()
{
   context.setCheckPoint();
}

bool Debugger :: start(const TCHAR* exePath, const TCHAR* cmdLine)
{
   if (startProcess(exePath, cmdLine)) {
      processEvent(INFINITE);

      return true;
   }
   else return false;
}

bool Debugger :: proceed(size_t timeout)
{
   processEvent(timeout);

   return !trapped;
}

void Debugger :: run()
{
   continueProcess();
}

void Debugger :: stop()
{
   if (!started)
      return;

   ::TerminateProcess(context.hProcess, 1);

   continueProcess();
}

void Debugger :: addBreakpoint(size_t address)
{
   breakpoints.addBreakpoint(address, context, started);
}

void Debugger :: removeBreakpoint(size_t address)
{
   breakpoints.removeBreakpoint(address, context, started);
}

void Debugger :: clearBreakpoints()
{
   breakpoints.clear();
}

bool Debugger :: startThread(_Controller* controller)
{
   HANDLE hThread = CreateThread(NULL, 4096,
                     (LPTHREAD_START_ROUTINE)debugEventThread,
                     (LPVOID)controller,
                     0, &threadId);

   if (!hThread) {
      return false;
   }
   else ::CloseHandle(hThread);

   return true;
}

void Debugger :: reset()
{
   trapped = false;

   context.reset();

   minAddress = 0xFFFFFFFF;
   maxAddress = 0;

   steps.clear();

   breakpoints.clear();

   stepMode = false;
   needToHandle = false;
}

BOOL CALLBACK EnumThreadWndProc(HWND hwnd, LPARAM lParam)
{
   if (GetWindowThreadProcessId(hwnd, NULL)==(DWORD)lParam) {
      _ELENA_::SetForegroundWindow(hwnd);

      return FALSE;
   }
   else return TRUE;
}

void Debugger :: activate()
{
   if (started) {
      EnumWindows(EnumThreadWndProc, context.dwThreadId);
   }
}
