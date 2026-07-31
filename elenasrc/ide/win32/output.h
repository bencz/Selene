//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      Output class header
//                                              (C)2005-2007, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef outputH
#define outputH

#include "window.h"
#include "layout.h"

namespace _GUI_
{

class Output : public Control
{
protected:
   int    _postponedAction;

   HANDLE _hStdoutRead;
   HANDLE _hProcess;
   HANDLE _hEvtStop;		// event to notify the redir thread to exit
   HANDLE _hThread;		// thread to receive the output of the child process
   DWORD  _dwThreadId;		// id of the redir thread

   static DWORD WINAPI OutputThread(LPVOID lpvThreadParam);

   bool execute(const TCHAR* path, const TCHAR* cmdLine, const TCHAR* curDir, 
					HANDLE hStdOut);

   int redirectStdout();
   
   virtual int getStyle() 
   { 
      return WS_CHILD | WS_VISIBLE | WS_BORDER | WS_HSCROLL | WS_VSCROLL | ES_MULTILINE | 
		  ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_READONLY; 
   }
   virtual const TCHAR* getClassName() { return _T("edit"); }

public:
   bool execute(const TCHAR* path, const TCHAR* cmdLine, const TCHAR* curDir, int postponedAction);
   void close();

   void getOutput(_ELENA_::String& string);
   void clear();
   
   Output(int left, int top, size_t width, size_t height);
   ~Output();
};

} // _GUI_

#endif // outputH
