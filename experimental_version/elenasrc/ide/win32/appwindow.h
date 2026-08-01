//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      IDE main window class header
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef appwindowH
#define appwindowH

// #define TIME_TRACKING

#include "window.h"
#include "accelerator.h"
#include "tabbar.h"
#include "editframe.h"
#include "statusbar.h"
#include "toolbar.h"
#include "browser.h"
#include "output.h"
#include "messagelog.h"
#include "debugcontroller.h"
#include "dialogs.h"
#include "splitter.h"
#include "pluginmanager.h"

#ifdef TIME_TRACKING
#include <time.h>
#endif

namespace _GUI_
{

class AppWindow;

// --- AppDebugger ---

class AppDebugger : public _ELENA_::DebugController
{
   AppWindow*      _app;

   virtual _ELENA_::_Module* loadDebugModule(const TCHAR* reference);
   virtual void clearDebugInfo();

   void notify(int code);
   void notify(int code, const TCHAR* message, int param = 0);
   void notify(int code, const TCHAR* message, int param1, int param2);
   void notify(int code, const TCHAR* source, TrackInfo info);

public:
   virtual void onLoadModule(const TCHAR* name);
   virtual void onStart();
   virtual void onStep(const TCHAR* source, int row, int disp, int length);
   virtual void onStop(bool broken);
   virtual void onCheckPoint(const TCHAR* message);
   virtual void onNotification(const TCHAR* message, size_t address, int code);

   AppDebugger(AppWindow* app);
};

// --- AppWindowList ---

class AppWindowList
{
   Menu     _menu;
   int      _count;

   AppWindow* _owner; 

public:
   void appendWindow(const TCHAR* name);
   void checkWindow(int index);
   void closeWindow(int index);

   AppWindowList(AppWindow* owner)
   {
      _count = 0;
      _owner = owner;
   }
};

// --- AppHistoryList ---

class AppHistoryList
{
   Menu        _menu;
   int         _command;
   int         _position;
   bool        _emptyMenu;

   List<TCHAR*> _history;

   AppWindow*  _owner; 

public:
   const TCHAR* getLink(int index) { return *_history.get(index); }

   void appendLink(const TCHAR* path);
   void clear();

   void load(IniConfigFile& config, const TCHAR* section);
   void save(IniConfigFile& config, const TCHAR* section);

   AppHistoryList(AppWindow* owner, int position, int command)
      : _history(NULL, freestr)
   {
      _owner = owner;  
      _command = command;
      _position = position;
      _emptyMenu = true;
   }
};

// --- AppWindow ---

#define IDE_STATE_BUSY				   0x0001
#define IDE_STATE_COMPILING			0x0001
#define IDE_STATE_LINEHIGHLIGHTED	0x0002
#define IDE_STATE_AUTORECOMPILE	   0x0004

typedef _ELENA_::Breakpoint Breakpoint;

class AppWindow : public Window
{
   typedef _ELENA_::Map<const TCHAR*, Text*> Texts;

   int                _state;
   int                _unnamedIndex;

   Texts              _texts;
   
   PluginManager      _pluginManager;

   AcceleratorManager _accelerators;
   LayoutManager      _layoutManager;   		

   EditFrame          _mainEditor;
   TabBar             _tabBar;
   ToolBar            _toolBar;
   StatusBar          _statusBar;
   TabBarPlus         _outputBar;
   ContextBrowser     _contextBrowser;
   Output             _output;
   MessageLog         _messageList;

   Splitter           _leftSplitter;
   Splitter           _bottomSplitter;

   AppWindowList      _windowList;
   AppHistoryList     _fileHistory;
   AppHistoryList     _projectHistory;
   FindDialog         _findDialog;

   List<Breakpoint>   _breakpoints;
   AppDebugger        _debugger;

   Clipboard          _clipboard;

   virtual int getStyle() { return WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN; }
   virtual const TCHAR* getClassName() { return APP_WND_CLASS; }
   virtual const TCHAR* getCaption() { return APP_NAME; }

   virtual LRESULT Class_Proc(HWND hWnd, size_t Message, WPARAM wParam, LPARAM lParam);

   void onEditorShow();
   void onEditorHide();
   void onEditorChanged(bool firstChange);
   void onProjectChanged(bool closed);
   void onProjectOperation(bool used);

   virtual void onResize();
   virtual void onClose();
   virtual bool onSetCursor();

   void onActivate();
   void onNotify(NMHDR* notification);
   void onSelChange(HWND tabPageID);
   void onDoubleClick(NMHDR* notification);
   void onToolTip(NMTTDISPINFO* toolTip);
   void onRClick(NMHDR* notification);
   void onChildKeyDown(NMHDR* notification);
   void onTVItemExpanded(NMTREEVIEW * notification);
   void onRowCountChanged(ExtNMHDR2* notification);
   void onDwawItem(DRAWITEMSTRUCT* item, int command);

   void onCompilationStart();
   void onCompilationEnd(const TCHAR* message);
   
   bool onDebugAction(int action, bool stepMode);
   void onDebuggerStart();
   void onDebuggerStep(const TCHAR* source, TrackInfo info);
   void onDebuggerStop(bool broken);
   void onDebuggerCheckPoint(const TCHAR* message);

   void doCommand(int id, int command);

   bool doEditCopy();
   void doEditPaste();
   void doEditDelete();
   void doUndo();
   void doRedo();

   void doCreateProject();
   void doOpenProject();
   bool doSaveProject(bool saveAsMode);
   bool doCloseProject();
   void doSaveAll(bool forced);
   void doCloseAllButActive();

   void doCreateFile();
   void doOpenFile();
   bool doSave(bool saveAsMode);
   bool doSave(int docIndex, bool saveAsMode);
   bool doCloseFile();
   void doInclude();
   void doExclude();
   void doSwitchTab(bool forward);

   void doShowDebugWatch(bool visible);
   void doShowCompilerOutput(bool checked);

   void doCompileProject(int postponedAction);

   void doGoToLine();   

   void markTab(int index, bool changed);

   bool loadModule(const TCHAR* name);
   bool startDebugger(bool stepMode);
   bool isOutaged(bool noWarning);
   void runToCursor();

   bool toggleBreakpoint(const TCHAR* source, size_t row);

   void toggleBreakpoint();
   void clearBreakpoints();
   void shiftBreakpoints(const TCHAR* source, size_t startRow, int offset);

   void cleanUpProject();

   bool closeTab(int index);

   void doFind();
   void doFindNext();
   void doReplace();
   void doSelectWindow();
   void doShowAbout();

   void showProjectSettings();
   void showProjectForwards();
   void showEditorSettings();
   void refreshDebugStatus();

   void highlightMessage(MessageBookmark* bookmark);
   void displayErrors();

   void setCaption(const TCHAR* projectName);

   void renameFileAs(int index, const TCHAR* newPath, const TCHAR* name, const TCHAR* oldPath, bool included);

   bool closeAll();
   void exit();

public:
   PluginManager* getPluginManager() { return &_pluginManager; }

   void loadHistory(IniConfigFile& config);
   void saveHistory(IniConfigFile& config);

   void create(HINSTANCE instance, HWND wndParent);

   bool translateMessage(LPMSG lpMsg);

   void openProject(const TCHAR* path);
   bool openFile(const TCHAR* path);
   bool openFile(const TCHAR* path, const TCHAR* name);

   const TCHAR* getTabName(int index) { return _tabBar.getTabName(index); }

   bool compileProject(const TCHAR* path, int postponedAction);

   void saveAllAsDraft();

   AppWindow();
};

} // _GUI_

#endif // appwindowH
