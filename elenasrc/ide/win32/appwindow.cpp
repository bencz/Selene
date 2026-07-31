//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      IDE main window class implementation
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#include "idecommon.h"
// --------------------------------------------------------------------------
#include "appwindow.h"
#include "idesettings.h"
#include "sourcedoc.h"
#include "module.h"

#define __DEBUGMODE 1
#define UNNAMED     "unnamed"

using namespace _GUI_;
using namespace _ELENA_;

int AppToolBarButtonNumber = 19;

ToolBarButton AppToolBarButtons[] =
{
   {IDM_FILE_NEW, IDR_FILENEW},
   {IDM_FILE_OPEN, IDR_FILEOPEN},
   {IDM_FILE_SAVE, IDR_FILESAVE},
   {IDM_FILE_SAVEALL, IDR_SAVEALL},
   {IDM_FILE_CLOSE, IDR_CLOSEFILE},
   {IDM_PROJECT_CLOSE, IDR_CLOSEALL},
   {0, IDR_SEPARATOR},
   {IDM_EDIT_CUT, IDR_CUT},
   {IDM_EDIT_COPY, IDR_COPY},
   {IDM_EDIT_PASTE, IDR_PASTE},
   {0, IDR_SEPARATOR},
   {IDM_EDIT_UNDO, IDR_UNDO},
   {IDM_EDIT_REDO, IDR_REDO},
   {0, IDR_SEPARATOR},
   {IDM_DEBUG_RUN, IDR_RUN},
   {IDM_DEBUG_STEPINTO, IDR_STEPINTO},
   {IDM_DEBUG_STEPOVER, IDR_STEPOVER},
   {IDM_DEBUG_STOP, IDR_STOP},
   {IDM_DEBUG_GOTOSOURCE, IDR_GOTO},
};

int StatusBarWidths[5] = {200, 90, 60, 30, 60};

// --- AppDebugger ---

AppDebugger :: AppDebugger(AppWindow* app)
{
   _app = app;
}

void AppDebugger :: onLoadModule(const TCHAR* name)
{
   notify(IDE_DEBUGGER_LOADMODULE, name);
}

void AppDebugger :: onStep(const TCHAR* source, int row, int disp, int length)
{
   notify(IDE_DEBUGGER_STEP, source, TrackInfo(row, disp, length));
}

void AppDebugger :: onStart()
{
   notify(IDE_DEBUGGER_START);
}

void AppDebugger :: onStop(bool failed)
{
#ifdef TIME_TRACKING
   _ended = clock();
#endif
   notify(failed ? IDE_DEBUGGER_BREAK : IDE_DEBUGGER_STOP);
}

void AppDebugger :: onCheckPoint(const TCHAR* message)
{
   notify(IDE_DEBUGGER_CHECKPOINT, message);
}

void AppDebugger :: onNotification(const TCHAR* message, size_t address, int code)
{
   notify(IDM_DEBUGGER_EXCEPTION, message, address, code);
}

void AppDebugger :: notify(int code)
{
   NMHDR notification;

   notification.code = code;
   notification.hwndFrom = NULL;

   ::SendMessage(_app->getHandle(), WM_NOTIFY, 0, (LPARAM)&notification);
}

void AppDebugger :: notify(int code, const TCHAR* message, int param)
{
   MessageNMHDR notification;

   notification.nmhrd.code = code;
   notification.nmhrd.hwndFrom = NULL;
   notification.message = message;
   notification.param = param;

   ::SendMessage(_app->getHandle(), WM_NOTIFY, 0, (LPARAM)&notification);
}

void AppDebugger :: notify(int code, const TCHAR* message, int param1, int param2)
{
   Message2NMHDR notification;

   notification.nmhrd.code = code;
   notification.nmhrd.hwndFrom = NULL;
   notification.message = message;
   notification.param1 = param1;
   notification.param2 = param2;

   ::SendMessage(_app->getHandle(), WM_NOTIFY, 0, (LPARAM)&notification);
}

void AppDebugger :: notify(int code, const TCHAR* source, TrackInfo info)
{
   LineInfoNMHDR notification;

   notification.nmhrd.code = code;
   notification.nmhrd.hwndFrom = NULL;
   notification.file = source;
   notification.position = info;

   ::SendMessage(_app->getHandle(), WM_NOTIFY, 0, (LPARAM)&notification);
}

_Module* AppDebugger :: loadDebugModule(const TCHAR* reference)
{
   // skip leading $ sign (for $elena)
   LocalNamespace name((reference[0] == '$') ? reference + 1 : reference);
   LocalPath      path;

   Settings::project.retrievePath(name, path, _T("dnl"));

   Module* module = (Module*)_modules.get(name);
   if (module == NULL) {
      module = new Module();

      FileReader reader(path, feRaw);
      LoadResult result = module->load(reader);
      if (result != lrSuccessful) {
         delete module;

         return NULL;
      }         
      _modules.add(name, module);
   }
   return module;
}

void AppDebugger :: clearDebugInfo()
{
   DebugController::clearDebugInfo();
   _modules.clear();
}

// --- AppWindowList ---

void AppWindowList :: appendWindow(const TCHAR* name)
{
   int count = _count > 10 ? 10 : _count;
   for (int i = 0 ; i <= count ; i++) {
	  _menu.checkItemByIndex(3 + i, false);
   }
   if (_count < 10) {
      if (!_menu.isLoaded()) {
         _menu.loadSubMenu(_owner->getHandle(), ((IDM_WINDOW - IDM) / 1000) - 1);
      }
      String caption(_T("&"));
      caption.appendInt(_count);
      caption.append(_T(": "), name);

      _menu.insertItem(3 + _count, IDM_WINDOW_FIRST + _count, caption);
	  _menu.checkItemByIndex(3 + _count, true);
   }
   _count++;
}

void AppWindowList :: checkWindow(int index)
{
   int count = _count > 10 ? 10 : _count;

   for (int i = 0 ; i < count ; i++) {
      _menu.checkItemByIndex(3 + i, false);
   }
   if (index >= 0 && index < 10) {
      _menu.checkItemByIndex(3 + index, true);
   }
}

void AppWindowList :: closeWindow(int index)
{
   if (index < 10) {
      _menu.deleteItem(3 + index);

      TCHAR caption[255];
      int count = _count > 10 ? 10 : _count;
      for (int i = index ; i < count - 1 ; i++) {
         _menu.getItemCaption(3 + i, caption, 255);
         caption[1] = (char)(0x30 + i);

         _menu.setItem(3 + i, caption, IDM_WINDOW_FIRST + i);
      }
   }
   _count--;
   if (_count >= 10 && index < 10) {
      String caption(_T("&9:"));
      caption.append(_owner->getTabName(9));

      _menu.insertItem(12, IDM_WINDOW_FIRST + 9, caption);
   }
}

// --- AppHistoryList ---

void AppHistoryList :: appendLink(const TCHAR* path)
{
   if (searchInList(_history, path) != -1)
      return;

   if (_history.Count() > 9) {
      _history.cut(_history.get(0));

	  _menu.deleteItem(11);

      for (size_t i = 0 ; i < _history.Count() ; i++) {
         String caption(_T("&"));
         caption.appendInt(i);
         caption.append(_T(": "), *_history.get(i));

         _menu.setItem(i + 2, caption, _command + i);
      }
   }
   if (!_menu.isLoaded()) {
      _menu.loadSubMenu(_owner->getHandle(), 0, _position);
   }
   if (_emptyMenu) {
      _emptyMenu = false;
      _menu.deleteItem(2);
   }

   String caption(_T("&"));
   caption.appendInt(_history.Count());
   caption.append(_T(": "), path);
   _menu.insertItem(2 + _history.Count(), _command + _history.Count(), caption);

   _history.add(_ELENA_::strdup(path));
}

void AppHistoryList :: clear()
{
   if (!_emptyMenu) {
      int count = _history.Count();
      _history.clear();
      while (count > 1) {
         _menu.deleteItem(2);
         count--;
      }
      _menu.setItem(2, _T("(none)"), 0);
      _menu.enableItemByIndex(2, false);
      _emptyMenu = true;
   }
}

inline void AppHistoryList :: load(IniConfigFile& config, const TCHAR* section)
{
   for(ConfigCategoryIterator it = config.getCategoryIt(section) ; !it.Eof() ; it++) {
      appendLink(it.key());
   }
}

void AppHistoryList :: save(IniConfigFile& config, const TCHAR* section)
{
   config.clear(section);
   for(List<TCHAR*>::Iterator it = _history.start() ; !it.Eof() ; it++) {
      config.setSetting(section, *it, DEFAULT_STR);
   }
}

// --- AppWindow ---

AppWindow :: AppWindow()
     : Window(0, 0, 800, 600),
     _mainEditor(this, &_pluginManager),
     _texts(NULL, freeobj),
     _tabBar(0, 0, 40, 40, &_mainEditor, true),
     _outputBar(0, 500, 800, 120, false),
     _output(0, 0, 800, 40),
     _contextBrowser(0, 0, 200, 80),
     _debugger(this),
	  _statusBar(5, StatusBarWidths),
	  _windowList(this),
     _fileHistory(this, IDM_RECENTFILES_CLEAR-IDM_FILE-1, IDM_PROJECT_FILES0),
     _projectHistory(this, IDM_RECENTPROJECTS_CLEAR-IDM_FILE-1, IDM_PROJECT_PROJECTS0),
	  _leftSplitter(&_contextBrowser, true),
	  _bottomSplitter(&_outputBar, false),
	  _findDialog(this)
{
   _state = 0;
   _unnamedIndex = 0;

   _bottomSplitter.setConstraint(60, 100);

   _layoutManager.setAsTop(&_toolBar);
   _layoutManager.setAsLeft(&_leftSplitter);
   _layoutManager.setAsBottom(&_bottomSplitter);
   _layoutManager.setAsClient(&_tabBar);
}

void AppWindow :: create(HINSTANCE instance, HWND wndParent)
{
   Control::create(instance, wndParent);

   _accelerators.create(instance);
   _tabBar.create(instance, _self);
   _mainEditor.create(instance, _tabBar.getHandle());
   _toolBar.create(instance, _self);
   _toolBar.assign(16, AppToolBarButtonNumber, AppToolBarButtons);

   _statusBar.create(instance, _self);

   _contextBrowser.create(instance, _self);
   doShowDebugWatch(false);

   _outputBar.create(instance, _self);
   _output.create(instance, _outputBar.getHandle());
   _messageList.create(instance, _outputBar.getHandle());

   _outputBar.addTab(_T("Output"), &_output);
   _outputBar.addTab(_T("Messages"), &_messageList);

   _leftSplitter.create(instance, _self);
   _bottomSplitter.create(instance, _self);

   _output.hide();
   _outputBar.selectTab(0);

   if (!Settings::compilerOutput) {
      _outputBar.hide();
   }

   ::MoveWindow(_self, _left, _top, _width, _height, FALSE);

   _toolBar.show();
   _statusBar.show();

   Menu menu(_self);
   menu.checkItemById(IDM_VIEW_OUTPUT, Settings::compilerOutput);
   menu.enableItemById(IDM_DEBUG_STOP, false);
   menu.enableItemById(IDM_DEBUG_GOTOSOURCE, false);

   doShowCompilerOutput(Settings::compilerOutput);

   onProjectChanged(true);
   onEditorHide();

   // opend default files
   if (!Settings::defaultProject.isEmpty())
      openProject(Settings::defaultProject);

   List<TCHAR*>::Iterator it = Settings::defaultFiles.start();
   while (!it.Eof()) {
      openFile(*it);

      it++;
   }

   // init plugins
   List<TCHAR*>::Iterator p_it = Settings::plugins.start();
   while (!p_it.Eof()) {
      _pluginManager.registerPlagin(*p_it);

      p_it++;
   }
   
}

bool AppWindow :: translateMessage(LPMSG lpMsg)
{
   return _accelerators.translate(_self, lpMsg);
}

LRESULT AppWindow :: Class_Proc(HWND hWnd, size_t Message, WPARAM wParam, LPARAM lParam)
{
   switch (Message)
   {
      case WM_DESTROY:
         ::PostQuitMessage(0);
         return 0;
      case WM_ACTIVATE:
         if (LOWORD(wParam) != WA_INACTIVE)  {
            onActivate();
         }
         return 0;
      case WM_COMMAND:
         doCommand(LOWORD(wParam), HIWORD(wParam));
         return 0;
      case WM_NOTIFY:
         onNotify((NMHDR*)lParam);
         return 0;
      case WM_DRAWITEM:
         onDwawItem((DRAWITEMSTRUCT*)lParam, HIWORD(wParam));
         return TRUE;
      case WM_GETMINMAXINFO:
      {
         MINMAXINFO *minMax = (MINMAXINFO*)lParam;

         minMax->ptMinTrackSize.y = 100;
         minMax->ptMinTrackSize.x = 400;

         return FALSE;
      }
   }
   return Window::Class_Proc(hWnd, Message, wParam, lParam);
}

void AppWindow :: onResize()
{
   Rectangle clientRect = getClientRectangle();
   clientRect.bottomRight.y -= _statusBar.getHeight();

   _layoutManager.resizeTo(clientRect);

   _statusBar.setCoordinate(clientRect.topLeft.x, clientRect.topLeft.y + clientRect.bottomRight.y);
   _statusBar.setWidth(clientRect.bottomRight.x);
   _statusBar.resize();
}

void AppWindow :: onActivate()
{
   if (_mainEditor.isVisible()) {
      _mainEditor.setFocus();
   }
}

void AppWindow :: onClose()
{
   if (closeAll())
      Window::onClose();
}

bool AppWindow :: onSetCursor()
{
   setCursor(CURSOR_ARROW);

   return true;
}

void AppWindow :: onDwawItem(DRAWITEMSTRUCT* item, int)
{
   if (item->hwndItem==_tabBar.getHandle()) {
      _tabBar.drawItem(item);
   }
   else if (item->hwndItem==_outputBar.getHandle()) {
      _outputBar.drawItem(item);
   }
}

void AppWindow :: onNotify(NMHDR* notification)
{
   switch (notification->code)
   {
      case TCN_SELCHANGE:
         onSelChange(notification->hwndFrom);
         break;
      case IDE_EDITOR_CHANGED:
         onEditorChanged(((ExtNMHDR*)notification)->extParam == -1);
         break;
       case IDE_EDITOR_MARGINCLICKED:
         toggleBreakpoint();
         break;
      case IDE_DEBUGGER_LOADMODULE:
         loadModule(((MessageNMHDR*)notification)->message);
         break;
      case IDE_DEBUGGER_START:
         onDebuggerStart();
         break;
      case IDE_DEBUGGER_STEP:
         onDebuggerStep(((LineInfoNMHDR*)notification)->file,
                        ((LineInfoNMHDR*)notification)->position);
         break;
      case IDE_DEBUGGER_STOP:
         onDebuggerStop(false);
         break;
      case IDE_DEBUGGER_BREAK:
         onDebuggerStop(true);
         break;
      case IDE_DEBUGGER_CHECKPOINT:
         onDebuggerCheckPoint(((MessageNMHDR*)notification)->message);
         break;
      case IDM_DEBUGGER_EXCEPTION:
      {
         String message(_T("Exception "));
         message.appendHex(((Message2NMHDR*)notification)->param2);
         message.append(_T(": "));
         message.append(((Message2NMHDR*)notification)->message);
         message.appendHex(((Message2NMHDR*)notification)->param1);
         ::MessageBox(getHandle(), message, APP_NAME, MB_OK | MB_ICONERROR); // !!
         break;
      }
      case NM_DBLCLK:
         onDoubleClick(notification);
         break;
      case TTN_GETDISPINFO:
         onToolTip((NMTTDISPINFO*)notification);
         break;
      case IDM_COMPILER_SUCCESSFUL:
         onCompilationEnd(_T("Successfully compiled"));

         if (((ExtNMHDR*)notification)->extParam) {            
            doCommand(((ExtNMHDR*)notification)->extParam, 0);
            _state &= ~IDE_STATE_AUTORECOMPILE;
         }
         break;
      case IDM_COMPILER_UNSUCCESSFUL:
         onCompilationEnd(_T("Compiled with errors"));
         displayErrors();
         cleanUpProject();
         break;
      case IDM_COMPILER_WITHWARNING:
         onCompilationEnd(_T("Successfully compiled with warnings"));
         displayErrors();
         break;
      case NM_RCLICK :
         onRClick(notification);
         break;
      case TVN_KEYDOWN:
         onChildKeyDown(notification);
         break;
      case IDM_LAYOUT_CHANGED:
         onResize();
         break;
      case IDE_EDITOR_ROWCOUNT_CHANGED:
         onRowCountChanged((ExtNMHDR2*)notification);
         break;
	  case TVN_ITEMEXPANDING:
         onTVItemExpanded((NMTREEVIEW*)notification);
         break;
   }
}

void AppWindow :: onToolTip(NMTTDISPINFO* toolTip)
{
   switch (toolTip->hdr.idFrom) {
      case IDM_FILE_NEW:
         toolTip->lpszText = _T("New File");
         break;
      case IDM_FILE_OPEN:
         toolTip->lpszText = _T("Open File");
         break;
      case IDM_FILE_SAVE:
         toolTip->lpszText = _T("Save File");
         break;
      case IDM_FILE_SAVEALL:
         toolTip->lpszText = _T("Save All");
         break;
      case IDM_FILE_CLOSE:
         toolTip->lpszText = _T("Close File");
         break;
      case IDM_PROJECT_CLOSE:
         toolTip->lpszText = _T("Close Project");
         break;
      case IDM_EDIT_CUT:
         toolTip->lpszText = _T("Cut");
         break;
      case IDM_EDIT_COPY:
         toolTip->lpszText = _T("Copy");
         break;
      case IDM_EDIT_PASTE:
         toolTip->lpszText = _T("Paste");
         break;
      case IDM_EDIT_UNDO:
         toolTip->lpszText = _T("Undo");
         break;
      case IDM_EDIT_REDO:
         toolTip->lpszText = _T("Redo");
         break;
      case IDM_DEBUG_RUN:
         toolTip->lpszText = _T("Run");
         break;
      case IDM_DEBUG_STOP:
         toolTip->lpszText = _T("Stop");
         break;
      case IDM_DEBUG_STEPINTO:
         toolTip->lpszText = _T("Step Into");
         break;
	  case IDM_DEBUG_STEPOVER:
         toolTip->lpszText = _T("Step Over");
         break;
	  case IDM_DEBUG_GOTOSOURCE:
         toolTip->lpszText = _T("Go to Source");
         break;
      default:
         return;
   }
}

void AppWindow :: onSelChange(HWND tabPageID)
{
   if (_tabBar.getHandle() == tabPageID) {
      _mainEditor.showDocument(_tabBar.getCurrentIndex());
      _mainEditor.setFocus();

      _windowList.checkWindow(_tabBar.getCurrentIndex());

	  onEditorChanged(false);
   }
   else if (_outputBar.getHandle() == tabPageID) {
      _outputBar.refreshChild();
      onEditorChanged(false);
   }
}

void AppWindow :: onDoubleClick(NMHDR* notification)
{
   if (_messageList.getHandle()==notification->hwndFrom) {
      highlightMessage(_messageList.getBookmark(((LPNMITEMACTIVATE)notification)->iItem));
   }
}

void AppWindow :: onRClick(NMHDR* notification)
{
   if (_contextBrowser.getHandle()==notification->hwndFrom) {
	  DWORD dwpos = ::GetMessagePos();

      HTREEITEM item = _contextBrowser.hitTest(LOWORD(dwpos), HIWORD(dwpos));
      if (item) {
         _contextBrowser.select(item);
      }
      _contextBrowser.showContextMenu(_self, LOWORD(dwpos), HIWORD(dwpos));
   }
}

void AppWindow :: onChildKeyDown(NMHDR* notification)
{
   if (_contextBrowser.getHandle()==notification->hwndFrom) {
      switch (((LPNMLVKEYDOWN)notification)->wVKey) {
         case 73:
            doCommand(IDM_DEBUG_INSPECT, (int)notification->hwndFrom); // !! check if ctrl was pressed too
            break;
      }
   }
}

void AppWindow :: onRowCountChanged(ExtNMHDR2* notification)
{
   int index = _tabBar.getCurrentIndex();
   if (index != -1) {
      const TCHAR* module = _tabBar.getTabName(index);
      int row = notification->extParam1;
      int diff = notification->extParam2;

      shiftBreakpoints(module, row, diff);
   }
}

void AppWindow :: onTVItemExpanded(NMTREEVIEW* notification)
{
   if (_contextBrowser.getHandle()==notification->hdr.hwndFrom) {
      _contextBrowser.browse(&_debugger, notification->itemNew.hItem);
   }
}

void AppWindow :: onEditorChanged(bool firstChange)
{
   Point caret = _mainEditor.getCaret();

   String line(_T("Ln "));
   line.appendInt(caret.y + 1);
   line.append(_T(" Col "));
   line.appendInt(caret.x + 1);

   _statusBar.setText(1, line);

   switch (_mainEditor.getOverwriteMode()) {
      case 0:
         _statusBar.setText(3, _T("INS"));
         break;
      case 1:
         _statusBar.setText(3, _T("OVR"));
         break;
      default:
         _statusBar.setText(3, EMPTY_STRING);
   }

   if (firstChange) {
      if (_mainEditor.isModified()) {
         _statusBar.setText(2, _T("Modified"));
         markTab(_tabBar.getCurrentIndex(), true);
      }
      else _statusBar.setText(2, EMPTY_STRING);
   }

   bool selected = _mainEditor.hasSelection();
   bool undo = _mainEditor.canUndo();
   bool redo = _mainEditor.canRedo();
   bool included = _mainEditor.isDocumentIncluded(_tabBar.getCurrentIndex());
   Menu menu(_self);

   menu.enableItemById(IDM_EDIT_PASTE, Clipboard::isAvailable());
   menu.enableItemById(IDM_EDIT_COPY, selected);
   menu.enableItemById(IDM_EDIT_DELETE, selected);
   menu.enableItemById(IDM_EDIT_CUT, selected);
   menu.enableItemById(IDM_EDIT_UNDO, undo);
   menu.enableItemById(IDM_EDIT_REDO, redo);
   menu.enableItemById(IDM_PROJECT_EXCLUDE, included);
   menu.enableItemById(IDM_PROJECT_INCLUDE, !included);

   _toolBar.enableButton(IDM_EDIT_PASTE, Clipboard::isAvailable());
   _toolBar.enableButton(IDM_EDIT_COPY, selected);
   _toolBar.enableButton(IDM_EDIT_CUT, selected);
   _toolBar.enableButton(IDM_EDIT_REDO, redo);
   _toolBar.enableButton(IDM_EDIT_UNDO, undo);

   if (test(_state, IDE_STATE_LINEHIGHLIGHTED)) {
      _mainEditor.clearTracker();
      _state &= ~IDE_STATE_LINEHIGHLIGHTED;
   }
}

void AppWindow :: onEditorShow()
{
   _statusBar.setText(0, _T("Ready"));

   Menu menu(_self);
   menu.enableItemById(IDM_FILE_SAVE, true);
   menu.enableItemById(IDM_FILE_SAVEAS, true);
   menu.enableItemById(IDM_FILE_SAVEALL, true);
   menu.enableItemById(IDM_FILE_CLOSE, true);
   menu.enableItemById(IDM_EDIT_PASTE, Clipboard::isAvailable());
   menu.enableItemById(IDM_EDIT_SELECTALL, true);
   menu.enableItemById(IDM_SEARCH_GOTOLINE, true);
   menu.enableItemById(IDM_SEARCH_FIND, true);
   menu.enableItemById(IDM_WINDOW_NEXT, true);
   menu.enableItemById(IDM_WINDOW_PREVIOUS, true);

   _toolBar.enableButton(IDM_FILE_SAVE, true);
   _toolBar.enableButton(IDM_FILE_SAVEALL, true);
   _toolBar.enableButton(IDM_FILE_CLOSE, true);
   _toolBar.enableButton(IDM_EDIT_PASTE, Clipboard::isAvailable());

   onEditorChanged(false);
}

void AppWindow :: onEditorHide()
{
   _statusBar.setText(0, DEFAULT_TEXT);
   _statusBar.setText(1, EMPTY_STRING);
   _statusBar.setText(2, EMPTY_STRING);

   Menu menu(_self);
   menu.enableItemById(IDM_FILE_SAVE, false);
   menu.enableItemById(IDM_FILE_SAVEAS, false);
   menu.enableItemById(IDM_FILE_SAVEALL, false);
   menu.enableItemById(IDM_FILE_CLOSE, false);
   menu.enableItemById(IDM_EDIT_UNDO, false);
   menu.enableItemById(IDM_EDIT_REDO, false);
   menu.enableItemById(IDM_EDIT_CUT, false);
   menu.enableItemById(IDM_EDIT_COPY, false);
   menu.enableItemById(IDM_EDIT_PASTE, false);
   menu.enableItemById(IDM_EDIT_DELETE, false);
   menu.enableItemById(IDM_EDIT_SELECTALL, false);
   menu.enableItemById(IDM_SEARCH_GOTOLINE, false);
   menu.enableItemById(IDM_SEARCH_FIND, false);
   menu.enableItemById(IDM_SEARCH_FINDNEXT, false);
   menu.enableItemById(IDM_WINDOW_NEXT, false);
   menu.enableItemById(IDM_WINDOW_PREVIOUS, false);
   menu.enableItemById(IDM_PROJECT_INCLUDE, false);
   menu.enableItemById(IDM_PROJECT_EXCLUDE, false);

   _toolBar.enableButton(IDM_FILE_SAVE, false);
   _toolBar.enableButton(IDM_FILE_SAVEALL, false);
   _toolBar.enableButton(IDM_FILE_CLOSE, false);
   _toolBar.enableButton(IDM_EDIT_CUT, false);
   _toolBar.enableButton(IDM_EDIT_COPY, false);
   _toolBar.enableButton(IDM_EDIT_PASTE, false);
   _toolBar.enableButton(IDM_EDIT_UNDO, false);
   _toolBar.enableButton(IDM_EDIT_REDO, false);
}

void AppWindow :: onProjectChanged(bool closed)
{
   Menu menu(_self);

   menu.enableItemById(IDM_FILE_SAVEPROJECT, !closed);
   menu.enableItemById(IDM_PROJECT_CLOSE, !closed);
   menu.enableItemById(IDM_PROJECT_COMPILE, !closed);
   menu.enableItemById(IDM_PROJECT_FORWARDS, !closed);
   menu.enableItemById(IDM_PROJECT_OPTION, !closed);
   menu.enableItemById(IDM_DEBUG_STEPOVER, !closed);
   menu.enableItemById(IDM_DEBUG_STEPINTO, !closed);
   menu.enableItemById(IDM_DEBUG_RUNTO, !closed);
   menu.enableItemById(IDM_DEBUG_BREAKPOINT, !closed);
   menu.enableItemById(IDM_DEBUG_CLEARBREAKPOINT, !closed);
   menu.enableItemById(IDM_DEBUG_RUN, !closed);

   _toolBar.enableButton(IDM_PROJECT_CLOSE, !closed);
   _toolBar.enableButton(IDM_DEBUG_RUN, !closed);

   bool started = _debugger.isStarted();
   _toolBar.enableButton(IDM_DEBUG_STOP, started);
   _toolBar.enableButton(IDM_DEBUG_STEPINTO, started);
   _toolBar.enableButton(IDM_DEBUG_STEPOVER, started);
   _toolBar.enableButton(IDM_DEBUG_GOTOSOURCE, started);
}

void AppWindow :: onProjectOperation(bool used)
{
   _toolBar.enableButton(IDM_PROJECT_COMPILE, !used);
   _toolBar.enableButton(IDM_DEBUG_RUN, !used);

   Menu menu(_self);
   menu.enableItemById(IDM_PROJECT_COMPILE, !used);
   menu.enableItemById(IDM_PROJECT_CLEAN, !used);
   menu.enableItemById(IDM_PROJECT_OPTION, !used);
   menu.enableItemById(IDM_PROJECT_FORWARDS, !used);
   menu.enableItemById(IDM_DEBUG_RUN, !used);
}

void AppWindow :: onCompilationStart()
{
   _state |= IDE_STATE_COMPILING;

   onProjectOperation(true);
}

void AppWindow :: onCompilationEnd(const TCHAR* message)
{
   _state &= ~IDE_STATE_COMPILING;

   onProjectOperation(false);

   _statusBar.setText(0, message);
}

bool AppWindow :: onDebugAction(int action, bool stepMode)
{
   if (test(_state, IDE_STATE_BUSY))
      return false;

   _mainEditor.clearTracker();
   _statusBar.setText(0, NULL);

   if (!_debugger.isStarted()) {
      bool recompile = Settings::autoRecompile && !test(_state, IDE_STATE_AUTORECOMPILE);
      if (!isOutaged(recompile)) {
         if (recompile) {
            doCompileProject(action);
         }
         return false;
      }
      if (!startDebugger(stepMode))
         return false;
   }
   return true;
}

void AppWindow :: onDebuggerStart()
{
   doShowDebugWatch(true);

   Menu menu(_self);
   menu.enableItemById(IDM_DEBUG_STOP, true);
   menu.enableItemById(IDM_DEBUG_GOTOSOURCE, true);

   _toolBar.enableButton(IDM_DEBUG_RUN, false);
   _toolBar.enableButton(IDM_DEBUG_STOP, true);
   _toolBar.enableButton(IDM_DEBUG_STEPOVER, true);
   _toolBar.enableButton(IDM_DEBUG_STEPINTO, true);
   _toolBar.enableButton(IDM_DEBUG_GOTOSOURCE, true);

   _mainEditor.setReadOnlyMode();
}

void AppWindow :: onDebuggerStep(const TCHAR* source, TrackInfo info)
{
   _ELENA_::SetForegroundWindow(_self);

   if (::IsIconic(_self))
      ::ShowWindowAsync(_self, Settings::appMaximized ? SW_MAXIMIZE : SW_SHOWNORMAL);

   if (!loadModule(source)) {
      MsgBox::show(getHandle(), _T("Could not locate the module "), source, MB_OK | MB_ICONWARNING);

      return;
   }

   _mainEditor.setTracker(info, STYLE_TRACE_LINE, STYLE_TRACE);

   _contextBrowser.refresh(&_debugger);
   refreshDebugStatus();
}

void AppWindow :: onDebuggerStop(bool broken)
{
   _ELENA_::SetForegroundWindow(_self);

   if (::IsIconic(_self))
      ::ShowWindowAsync(_self, Settings::appMaximized ? SW_MAXIMIZE : SW_SHOWNORMAL);

   doShowDebugWatch(false);

   Menu menu(_self);
   menu.enableItemById(IDM_DEBUG_STOP, false);
   menu.enableItemById(IDM_DEBUG_GOTOSOURCE, false);

   _toolBar.enableButton(IDM_DEBUG_RUN, true);
   _toolBar.enableButton(IDM_DEBUG_STOP, false);
   _toolBar.enableButton(IDM_DEBUG_STEPOVER, false);
   _toolBar.enableButton(IDM_DEBUG_STEPINTO, false);
   _toolBar.enableButton(IDM_DEBUG_GOTOSOURCE, false);

   _statusBar.setText(0, broken ? _T(" Program broken") : _T(" Program stopped"));

   //_mainEditor.clearTracker();

   _mainEditor.resetReadOnlyMode();
   _contextBrowser.reset();

   _debugger.release();

#ifdef TIME_TRACKING
   time_t elapsed = _debugger.getExecutionTime();

   String message(_T("The execution time of the program - "));
   message.appendLong(elapsed);

   ::MessageBox(getHandle(), message, APP_NAME, MB_OK | MB_ICONINFORMATION); // !!
#endif
}

void AppWindow :: onDebuggerCheckPoint(const TCHAR* message)
{
   _statusBar.setText(0, message);
}

void AppWindow :: doCommand(int id, int)
{
   switch (id) {
      case IDM_PROJECT_NEW:
         doCreateProject();
         break;
      case IDM_PROJECT_CLOSE:
         closeAll();
         break;
      case IDM_FILE_NEW:
         doCreateFile();
         break;
      case IDM_FILE_OPEN:
         doOpenFile();
         break;
      case IDM_FILE_CLOSE:
         doCloseFile();
         break;
      case IDM_FILE_CLOSEALL:
         while (_tabBar.getCount() > 0) {
            doCloseFile();
         }
         break;
      case IDM_FILE_CLOSEALLBUT:
         doCloseAllButActive();
         break;
      case IDM_FILE_SAVE:
         doSave(false);
         break;
      case IDM_FILE_SAVEAS:
         doSave(true);
         break;
      case IDM_FILE_SAVEPROJECT:
         doSaveProject(true);
         break;
      case IDM_FILE_SAVEALL:
         doSaveAll(true);
         break;
      case IDM_PROJECT_OPEN:
         doOpenProject();
         break;
      case IDM_PROJECT_COMPILE:
         if (!test(_state, IDE_STATE_BUSY)) {
            doCompileProject(0);
         }
         break;
      case IDM_DEBUG_STEPOVER:
         if (!test(_state, IDE_STATE_BUSY)) {
            if (onDebugAction(id, true)) {
               _debugger.stepOver();
            }
         }
         break;
      case IDM_DEBUG_STEPINTO:
         if (!test(_state, IDE_STATE_BUSY)) {
            if (onDebugAction(id, true)) {
               _debugger.stepInto();
            }
         }
         break;
      case IDM_DEBUG_RUNTO:
         if (!test(_state, IDE_STATE_BUSY)) {
            if (onDebugAction(id, true)) {
               runToCursor();
            }
         }
         break;
      case IDM_DEBUG_RUN:
         if (!test(_state, IDE_STATE_BUSY)) {
            if (onDebugAction(id, false)) {
               _debugger.run();
            }
         }
         break;
      case IDM_DEBUG_STOP:
         _mainEditor.clearTracker();
         _debugger.stop();
         break;
      case IDM_DEBUG_GOTOSOURCE:
         _debugger.showCurrentModule();
         _mainEditor.setFocus();
         break;
      case IDM_PROJECT_OPTION:
         showProjectSettings();
         break;
      case IDM_PROJECT_FORWARDS:
         showProjectForwards();
         break;
      case IDM_SEARCH_GOTOLINE:
         doGoToLine();
         break;
      case IDM_EDIT_COPY:
         doEditCopy();
         break;
      case IDM_EDIT_PASTE:
         doEditPaste();
         break;
      case IDM_EDIT_CUT:
         if (doEditCopy())
            doEditDelete();
         break;
      case IDM_EDIT_DELETE:
         doEditDelete();
         break;
      case IDM_EDIT_UNDO:
         doUndo();
         break;
      case IDM_EDIT_REDO:
         doRedo();
         break;
      case IDM_EDIT_SELECTALL:
         _mainEditor.selectAll();
         break;
      case IDM_EDIT_TRIM:
         _mainEditor.trim();
         break;
	  case IDM_EDIT_DUPLICATE:
         _mainEditor.duplicateLine();
		 break;
      case IDM_EDIT_ERASELINE:
         _mainEditor.eraseLine();
         break;
	  case IDM_EDIT_INDENT:
         _mainEditor.indent();
         break;
	  case IDM_EDIT_OUTDENT:
         _mainEditor.outdent();
         break;
	  case IDM_EDIT_COMMENT:
         _mainEditor.commentBlock();
         break;
      case IDM_EDIT_UNCOMMENT:
         _mainEditor.uncommentBlock();
         break;
      case IDM_EDIT_SWAP:
         _mainEditor.swap();
         break;
      case IDM_EDIT_UPPERCASE:
         _mainEditor.toUppercase();
         break;
      case IDM_EDIT_LOWERCASE:
         _mainEditor.toLowercase();
         break;
      case IDM_FILE_EXIT:
         exit();
         break;
      case IDM_WINDOW_NEXT:
	     doSwitchTab(true);
	     break;
      case IDM_WINDOW_PREVIOUS:
	     doSwitchTab(false);
	     break;
      case IDM_DEBUG_BREAKPOINT:
         toggleBreakpoint();
         break;
      case IDM_DEBUG_CLEARBREAKPOINT:
         clearBreakpoints();
         break;
      case IDM_PROJECT_INCLUDE:
         doInclude();
         break;
      case IDM_PROJECT_EXCLUDE:
         doExclude();
         break;
      case IDM_DEBUG_INSPECT:
         _contextBrowser.browse(&_debugger);
         break;
      case IDM_DEBUG_SWITCHHEXVIEW:
         Settings::hexNumberMode = !Settings::hexNumberMode;
         _contextBrowser.refresh(&_debugger);
         break;
      case IDM_WINDOW_FIRST:
      case IDM_WINDOW_SECOND:
      case IDM_WINDOW_THIRD:
      case IDM_WINDOW_FOURTH:
      case IDM_WINDOW_FIFTH:
      case IDM_WINDOW_SIXTH:
      case IDM_WINDOW_SEVENTH:
      case IDM_WINDOW_EIGHTH:
      case IDM_WINDOW_NINTH:
      case IDM_WINDOW_TENTH:
         _tabBar.selectTab(id - IDM_WINDOW_FIRST);
         break;
      case IDM_WINDOW_WINDOWS:
         doSelectWindow();
         break;
      case IDM_EDITOR_OPTIONS:
         showEditorSettings();
         break;
      case IDM_SEARCH_FIND:
         doFind();
         break;
      case IDM_SEARCH_FINDNEXT:
         doFindNext();
         break;;
      case IDM_SEARCH_REPLACE:
         doReplace();
         break;
      case IDM_PROJECT_FILES0:
      case IDM_PROJECT_FILES1:
      case IDM_PROJECT_FILES2:
      case IDM_PROJECT_FILES3:
      case IDM_PROJECT_FILES4:
      case IDM_PROJECT_FILES5:
      case IDM_PROJECT_FILES6:
      case IDM_PROJECT_FILES7:
      case IDM_PROJECT_FILES8:
      case IDM_PROJECT_FILES9:
         openFile(_fileHistory.getLink(id - IDM_PROJECT_FILES0));
         break;
      case IDM_PROJECT_PROJECTS0:
      case IDM_PROJECT_PROJECTS1:
      case IDM_PROJECT_PROJECTS2:
      case IDM_PROJECT_PROJECTS3:
      case IDM_PROJECT_PROJECTS4:
      case IDM_PROJECT_PROJECTS5:
      case IDM_PROJECT_PROJECTS6:
      case IDM_PROJECT_PROJECTS7:
      case IDM_PROJECT_PROJECTS8:
      case IDM_PROJECT_PROJECTS9:
	   {
         const TCHAR* link = _projectHistory.getLink(id - IDM_PROJECT_PROJECTS0);
         openProject(link);
         break;
	   }
      case IDM_HELP_API:
      {
         Path apiPath(Paths::appPath, _T("..\\doc\\api\\index.html"));

         ShellExecute(NULL, _T("open"), apiPath, NULL, NULL, SW_SHOW);
         break;
      }
      case IDM_HELP_ABOUT:
         doShowAbout();
         break;
      case IDM_WATCH_OPEN:
         _contextBrowser.setFocus();
         break;
      case IDM_VIEW_OUTPUT:
         doShowCompilerOutput(!Settings::compilerOutput);
         break;
      case IDM_VIEW_WATCH:
         doShowDebugWatch(!_contextBrowser.isVisible());
         break;
      case IDM_RECENTFILES_CLEAR:
         _fileHistory.clear();
         Settings::defaultFiles.clear();
         break;
      case IDM_RECENTPROJECTS_CLEAR:
         _projectHistory.clear();
         Settings::defaultProject.clear();
         Settings::defaultFiles.clear();
         break;
      case IDM_PROJECT_CLEAN:
         cleanUpProject();
         break;
  }
}

bool AppWindow :: doEditCopy()
{
   if (_clipboard.open(_self)) {
      _clipboard.clear();

      bool result = _mainEditor.copyClipboard(_clipboard);

      _clipboard.close();

      onEditorChanged(false);

      return result;
   }
   else return false;
}

void AppWindow :: doEditPaste()
{
   if (_clipboard.open(_self)) {
      _mainEditor.pasteClipboard(_clipboard);

      _clipboard.close();
   }
}

void AppWindow :: doEditDelete()
{
   _mainEditor.eraseSelection();
}

void AppWindow :: doUndo()
{
   _mainEditor.undo();
}

void AppWindow :: doRedo()
{
   _mainEditor.redo();
}

void AppWindow :: doCreateProject()
{
   if (!closeAll())
      return;

   onProjectChanged(false);

   showProjectSettings();
}

void AppWindow :: doOpenProject()
{
   FileDialog dialog(this, FileDialog::ProjectFilter, _T("Open Project"), Paths::lastPath);
   const TCHAR* path = dialog.openFile();
   if (path) {
      openProject(path);
      _projectHistory.appendLink(path);
   }
}

bool AppWindow :: doSaveProject(bool saveAsMode)
{
   if (saveAsMode || Settings::project.isUnnamed()) {
	  FileDialog dialog(this, FileDialog::ProjectFilter, _T("Save Project As"));
	  Path       path;

	  if (dialog.saveFile(_T("prj"), path)) {
         Settings::project.setName(path);

		 setCaption(Settings::project.getName());
	  }
	  else return false;
   }
   Settings::project.save();
   onProjectChanged(false);

   return true;
}

bool AppWindow :: doCloseProject()
{
   if (Settings::project.isChanged()) {
      int result = MsgBox::show(getHandle(), _T("Save changes to the project?"), MB_YESNOCANCEL | MB_ICONQUESTION);
      if (result==IDCANCEL) {
         return false;
      }
      else if (result==IDYES) {
         doSaveProject(false);
      }
   }
   clearBreakpoints();

   _messageList.clear();
   _output.clear();

   Settings::project.reset();

   setCaption(NULL);
   onProjectChanged(true);

   return true;
}

void AppWindow :: doSaveAll(bool forced)
{
   if (_mainEditor.isReadOnly())
      return;

   if (Settings::project.isUnnamed()) {
      if (!doSaveProject(false))
         return;
   }
   for (int index = 0 ; index < _tabBar.getCount() ; index++) {
      if (_mainEditor.isDocumentModified(index) || _mainEditor.isDocumentUnnamed(index)) {
         if (forced || _mainEditor.isDocumentUnnamed(index)) {
            doSave(index, false);
         }
         else {
            const TCHAR* path = _mainEditor.retrievePath(_texts, index);
            int result = MsgBox::show(getHandle(), _T("Save changes to "), path, MB_YESNOCANCEL | MB_ICONQUESTION);
            if (result==IDCANCEL) {
               return;
            }
            else if (result==IDYES) {
               _mainEditor.saveDocument(index, path);
               markTab(index, false);
            }
         }
      }
   }
   if (Settings::project.isChanged()) {
      if (!forced && !Settings::project.isUnnamed()) {
         int result = MsgBox::show(getHandle(), _T("Save the project changes?"), MB_YESNO | MB_ICONQUESTION);
         if (result==IDYES) {
            doSaveProject(false);
         }
      }
      else doSaveProject(false);
   }
   onEditorChanged(false);
}

void AppWindow :: doCreateFile()
{
   Text* text = new Text();
   text->create();

   String name(_T("unnamed"));
   name.appendInt(_unnamedIndex++);

   _texts.add(name, text);

   Document* doc = new SourceDoc(text, Settings::defaultEncoding);
   doc->status.unnamed = true;

   _mainEditor.newDocument(doc);
   _tabBar.addTab(name);

   _mainEditor.show();
   _mainEditor.setFocus();
   onEditorShow();

   _windowList.appendWindow(name);
}

void AppWindow :: doOpenFile()
{
   FileDialog dialog(this, FileDialog::SourceFilter, _T("Open File"), Paths::lastPath);

   List<TCHAR*> files(NULL, freestr);
   if (dialog.openFiles(files)) {
      List<TCHAR*>::Iterator it = files.start();
      while (!it.Eof()) {
         if (openFile(*it))
            _fileHistory.appendLink(*it);

         it++;
      }
   }
}

bool AppWindow :: doSave(bool saveAsMode)
{
   if (_mainEditor.isReadOnly())
      return false;

   if (Settings::project.isUnnamed() && _mainEditor.isDocumentIncluded(_tabBar.getCurrentIndex())) {
      if (!doSaveProject(false))
         return false;

	  if (!doSave(_tabBar.getCurrentIndex(), saveAsMode))
         return false;

	  Settings::project.save();	// to save the project config file after new file is included
   }
   else {
      if (!doSave(_tabBar.getCurrentIndex(), saveAsMode))
         return false;
   }
   onEditorChanged(false);
   return true;
}

bool AppWindow :: doSave(int docIndex, bool saveAsMode)
{
   bool modified = _mainEditor.isDocumentModified(docIndex);
   bool unnamed = _mainEditor.isDocumentUnnamed(docIndex);
   bool included = _mainEditor.isDocumentIncluded(docIndex);
   Path oldPath(_mainEditor.retrievePath(_texts, docIndex));
   if (unnamed || saveAsMode) {
      FileDialog dialog(this, FileDialog::SourceFilter, _T("Save File As"), Settings::project.getPath());

      Path newPath;
	   if (dialog.saveFile(_T("l"), newPath)) {
         LocalReferenceName name;
         Settings::project.retrieveName(newPath, name);

         renameFileAs(docIndex, newPath, name, oldPath, included);
      }
      else return false;

      if(unnamed && !included) {
         int result = MsgBox::show(getHandle(), _T("Include the document "), newPath, _T(" into the project?"),
                         MB_YESNO | MB_ICONQUESTION);
         if (result==IDYES) {
			   _mainEditor.markDocumentAsIncluded(docIndex);
            Settings::project.includeSource(newPath);
         }
      }
      _mainEditor.saveDocument(docIndex, newPath);
   }
   else _mainEditor.saveDocument(docIndex, oldPath);

   if (modified)
      markTab(docIndex, false);

   return true;
}

bool AppWindow :: closeTab(int index)
{
   const TCHAR* path = _mainEditor.retrievePath(_texts, index);

   if (_mainEditor.isModified()) {
      int result = MsgBox::show(getHandle(), _T("Save changes to "), path, MB_YESNOCANCEL | MB_ICONQUESTION);
      if (result==IDCANCEL) {
         return false;
      }
      else if (result==IDYES) {
         if (!doSave(false))
            return false;
      }
   }
   _mainEditor.closeDocument(index);
   _texts.erase(path);
   _tabBar.deleteTab(index);
   _windowList.closeWindow(index);

   if (_tabBar.getCurrentIndex()==-1) {
      _mainEditor.hide();
      onEditorHide();
   }
   return true;
}

void AppWindow :: doCloseAllButActive()
{
   int index = _tabBar.getCurrentIndex();
   for (int i = 0 ; i < index ; i++) {
      _tabBar.selectTab(0);
      if (!doCloseFile())
         return;
   }
   int count = _tabBar.getCount();
   for (int i = 1 ; i < count ; i++) {
      _tabBar.selectTab(1);
      if (!doCloseFile())
         return;
   }
}

bool AppWindow :: doCloseFile()
{
   int index = _tabBar.getCurrentIndex();
   if (index != -1) {
      return closeTab(index);
   }
   else return true;
}

void AppWindow :: doInclude()
{
   if (Settings::project.isUnnamed()) {
      if (!doSaveProject(false))
         return;
   }

   if (_tabBar.getCurrentIndex() != -1) {
	  _mainEditor.markDocumentAsIncluded(_tabBar.getCurrentIndex());

      const TCHAR* path = _mainEditor.retrievePath(_texts);
	  Settings::project.includeSource(path);
   }
   onEditorChanged(false);
}

void AppWindow :: doExclude()
{
   if (_tabBar.getCurrentIndex() != -1) {
	  _mainEditor.markDocumentAsExcluded(_tabBar.getCurrentIndex());

      const TCHAR* path = _mainEditor.retrievePath(_texts);
	  Settings::project.excludeSource(path);
   }
   onEditorChanged(false);
}

void AppWindow :: doSwitchTab(bool forward)
{
   int tabIndex = _tabBar.getCurrentIndex();
   if (forward) {
      if (tabIndex == _tabBar.getCount() - 1) {
         _tabBar.selectTab(0);
      }
      else _tabBar.selectTab(tabIndex + 1);
   }
   else {
      if (tabIndex == 0) {
         _tabBar.selectTab(_tabBar.getCount() - 1);
      }
      else _tabBar.selectTab(tabIndex - 1);
   }
}

void AppWindow :: doFind()
{
   if (_findDialog.showModal()) {
      if (_mainEditor.findText(_findDialog.getTextToFind(),
		  _findDialog.isMatchCase(), _findDialog.isWholeWord()))
	  {
	     Menu menu(_self);
		 menu.enableItemById(IDM_SEARCH_FINDNEXT, true);
      }
	  else MsgBox::show(getHandle(), NOT_FOUND_TEXT, MB_ICONINFORMATION | MB_OK);
   }
}

void AppWindow :: doFindNext()
{
   if (!_mainEditor.findText(_findDialog.getTextToFind(),
	   _findDialog.isMatchCase(), _findDialog.isWholeWord()))
   {
      MsgBox::show(getHandle(), NOT_FOUND_TEXT, MB_ICONINFORMATION | MB_OK);
   }
}

void AppWindow :: doReplace()
{
   if (_findDialog.showModal(true)) {
      if (_mainEditor.replaceText(_findDialog.getTextToFind(),
         _findDialog.getTextToReplace(), _findDialog.isMatchCase(), _findDialog.isWholeWord()))
	  {
	     Menu menu(_self);
		 menu.enableItemById(IDM_SEARCH_FINDNEXT, true);
      }
      else MsgBox::show(getHandle(), NOT_FOUND_TEXT, MB_ICONINFORMATION | MB_OK);
   }
}

void AppWindow :: doSelectWindow()
{
   List<int> closedTabs;
   WindowsDialog dialog(this, &_tabBar, &closedTabs);

   if (dialog.showModal()==-2) {
      List<int>::Iterator it = closedTabs.start();
      int offset = 0;
      while (!it.Eof()) {
         if(!closeTab(*it - offset))
            return;

         it++;
         offset++;
      }
   }
}

void AppWindow :: doShowAbout()
{
   AboutDialog dlg(this);

   dlg.showModal();
}

void AppWindow :: doShowCompilerOutput(bool checked)
{
   if (Settings::compilerOutput != checked) {
      Settings::compilerOutput = checked;

	  Menu menu(_self);
	  menu.checkItemById(IDM_VIEW_OUTPUT, Settings::compilerOutput);

      if (checked) {
         _outputBar.show();
      }
      else _outputBar.hide();

      onResize();
   }
}

void AppWindow :: doShowDebugWatch(bool visible)
{
   Menu menu(_self);
   menu.checkItemById(IDM_VIEW_WATCH, visible);

   if (visible) {
      _contextBrowser.show();
   }
   else _contextBrowser.hide();

   onResize();
}

void AppWindow :: doCompileProject(int postponedAction)
{
   onCompilationStart();

   doSaveAll(false);
   if (!Settings::project.isUnnamed()) {
	  Path path(Settings::project.getPath(), Settings::project.getName());
	  path.appendExtension(_T("prj"));

      if (!compileProject(path, postponedAction))
         onCompilationEnd(_T("Could not start the compilation process"));
   }
}

void AppWindow :: doGoToLine()
{
   if (_mainEditor.isVisible()) {
      Point caret = _mainEditor.getCaret();

      GoToLineDialog dlg(this, caret.y + 1);
      if (dlg.showModal()) {
         caret.y = dlg.getLineNumber() - 1;

         _mainEditor.setCaret(caret);
      }
   }
}

bool AppWindow :: loadModule(const TCHAR* name)
{
   if (!_debugger.isStarted())
      return false;

   int index = _tabBar.getTabIndex(name);
   if (index == -1) {
	   Path path(Paths::packageRoot.asString());

      if (name[0]=='$') {
         path.nameToPath(name + 1, _T("l"));
      }
      else path.nameToPath(name, _T("l"));

      openFile(path, name);
   }
   else _tabBar.selectTab(index);

   return true;
}

bool AppWindow :: startDebugger(bool stepMode)
{
   const TCHAR* target = Settings::project.getTarget();
   const TCHAR* arguments = Settings::project.getArguments();

   if (!emptystr(target)) {
      Path exePath(Settings::project.getPath(), target);
      // provide the whole command line including the executable path and name
      String commandLine(exePath.asString(), _T(" "), arguments);

      if (Settings::project.getDebugInfoEnabled()) {
         if (!_debugger.start(exePath, commandLine, true, _breakpoints)) {
            MsgBox::show(getHandle(), _T("A debugger cannot be started. Invalid or absent debug info\nPlease compile the project"), MB_ICONEXCLAMATION);

            return false;
         }
         else return true;
      }
      else if(stepMode) {
         MsgBox::show(getHandle(), _T("A debugger cannot be started. Invalid or absent debug info\nPlease turn on debug info option and recompile the project"), MB_ICONEXCLAMATION);

         return false;
      }
      else {
         if (!_debugger.start(exePath, commandLine, false, _breakpoints)) {
            MsgBox::show(getHandle(), _T("A program cannot be started\nPlease re-compile the project"), MB_ICONEXCLAMATION);

            return false;
         }
         else return true;
      }
   }
   else {
      MsgBox::show(getHandle(), _T("A project with not specified target cannot be started"), MB_ICONEXCLAMATION);

      return false;
   }
}

bool AppWindow :: isOutaged(bool noWarning)
{
   if (_mainEditor.isAnyModified()) {
      if (!noWarning)
         MsgBox::show(getHandle(), _T("The project modules are out of date\nPlease recompile the project"), MB_ICONEXCLAMATION);

      return false;
   }

   for (ConfigCategoryIterator it = Settings::project.SourceFiles() ; !it.Eof() ; it++) {
      FileName fileName(it.key());
      Path source(Settings::project.getPath(), it.key());
      Path module(Settings::project.getPath(), Settings::project.getOutputPath(), it.key());

      module.changeExtension(_T("nl"));

      DateTime sourceDT = DateTime::getFileTime(source);
      DateTime moduleDT = DateTime::getFileTime(module);

      if (sourceDT > moduleDT) {
         if (!noWarning)
            MsgBox::show(getHandle(), _T("The project modules are out of date\nPlease recompile the project"), MB_ICONEXCLAMATION);

         return false;
      }
   }
   return true;
}

void AppWindow :: renameFileAs(int index, const TCHAR* newPath, const TCHAR* name,
							   const TCHAR* oldPath, bool included)
{
   Text* text = _texts.exclude(oldPath);
   _texts.add(newPath, text);

   _tabBar.renameTab(index, name);

   if (included) {
      Settings::project.excludeSource(oldPath);
      Settings::project.includeSource(newPath);
   }
   _mainEditor.setFocus();
   onEditorChanged(false);
}

bool AppWindow :: openFile(const TCHAR* path)
{
   LocalReferenceName name;
   Settings::project.retrieveName(path, name);

   return openFile(path, name);
}

bool AppWindow :: openFile(const TCHAR* path, const TCHAR* name)
{
#ifdef __DEBUGMODE
   if (::PathIsRelative(path)) {
      MsgBox::show(getHandle(), _T("Relative path "), path, MB_OK | MB_ICONSTOP);
   }
#endif
   if (_texts.exist(path)) {
      _tabBar.selectTab(name);

      return false;
   }
   FileEncoding encoding;
   Text* text = new Text();
   if (!text->load(path, encoding)) {
      freeobj(text);
      return false;
   }
   _texts.add(path, text);
   _mainEditor.newDocument(new SourceDoc(text, encoding));
   _tabBar.addTab(name);

   // check if the file belongs to the project
   if (Settings::project.isIncluded(path)) {
      _mainEditor.markDocumentAsIncluded(_tabBar.getCurrentIndex());
   }

   // add to windows menu
   _windowList.appendWindow(name);
   if (!_mainEditor.isVisible()) {
      _mainEditor.show();
      _mainEditor.setFocus();
   }
   onEditorShow();

   return true;
}

void AppWindow :: openProject(const TCHAR* path)
{
   if (!closeAll())
      return;

   if (!Settings::project.open(path))
      return;

   if (Settings::lastProjectRemember)
      Settings::defaultProject.copy(path);

   if (Settings::lastPathRemember)
      Paths::lastPath.copyPath(path);

   Path sourcePath;
   ConfigCategoryIterator it = Settings::project.SourceFiles();
   while (!it.Eof()) {
      sourcePath.copy(it.key());
      Paths::resolveRelativePath(sourcePath, Settings::project.getPath());

      if (!openFile(sourcePath))
         MsgBox::show(getHandle(), _T("Cannot open file "), it.key(), MB_OK | MB_ICONERROR);

      it++;
   }
   setCaption(Settings::project.getName());

   onProjectChanged(false);
}

bool AppWindow :: closeAll()
{
   if (test(_state, IDE_STATE_COMPILING)) {
      int result = MsgBox::show(getHandle(), _T("The project is compiling. Close anyway?"), MB_YESNO | MB_ICONQUESTION);
      if (result==IDNO) {
         return false;
      }
   }

   while (_tabBar.getCurrentIndex() != -1) {
      if (!doCloseFile())
         return false;
   }
   _unnamedIndex = 0;
   return doCloseProject();
}

void AppWindow :: exit()
{
   ::SendMessage(_self, WM_CLOSE, 0, 0);
}

void AppWindow :: highlightMessage(MessageBookmark* bookmark)
{
   if (bookmark) {
      LocalReferenceName name;
      Settings::project.retrieveName(bookmark->file, name);

	  if (_tabBar.selectTab(name)) {
         _state &= ~IDE_STATE_LINEHIGHLIGHTED;

         _mainEditor.setFocus();
         _mainEditor.setTracker(TrackInfo(bookmark->col - 1, bookmark->row - 1, 0, 0), STYLE_ERROR_LINE, STYLE_ERROR_LINE); // !!

         _state |= IDE_STATE_LINEHIGHLIGHTED;
	  }
   }
}

void AppWindow :: setCaption(const TCHAR* projectName)
{
   String title(APP_NAME);
   if (!emptystr(projectName)) {
      title.append(_T(" - ["), projectName, _T("]"));
   }

   ::SetWindowText(_self, title);
}

void AppWindow :: showProjectSettings()
{
   ProjectSettingsDialog dlg(this);

   dlg.showModal();
}

void AppWindow :: showProjectForwards()
{
   ProjectForwardsDialog dlg(this);

   dlg.showModal();
}

void AppWindow :: showEditorSettings()
{
   EditorSettings dlg(this);

   if (dlg.showModal()) {
      _mainEditor.reloadSettings();
   }
}

void AppWindow :: refreshDebugStatus()
{
   if (Settings::testMode)  {
      String address(_T("@"));
      address.appendHex(_debugger.getEIP());
      _statusBar.setText(4, address);
   }
}

bool AppWindow :: compileProject(const TCHAR* path, int postponedAction)
{
   _outputBar.selectTab(0);

   _statusBar.setText(0, _T("Compiling the project..."));

//  !! _debugger.clear();
   _messageList.clear();
   _state &= ~IDE_STATE_LINEHIGHLIGHTED;
   _mainEditor.clearTracker();

   LocalPath appPath(Paths::appPath, _T("elc.exe"));
   LocalPath curDir(path, lastchrpos(path, '\\'));

   String cmdLine(Settings::unicodeELC ? _T("elc.exe -xunicode -c") : _T("elc.exe -c"));
   cmdLine.append(path);

   //!! HOT FIX to deal with variable tab size
   cmdLine.append(_T(" -xtab"));
   cmdLine.appendInt(Settings::tabSize);

   // !! temporal
   if (Settings::bytecode) {
      cmdLine.append(_T(" -xbytecode"));
   }

   const TCHAR* options = Settings::project.getOptions();
   if (!emptystr(options)) {
      cmdLine.append(' ');
      cmdLine.append(Settings::project.getOptions());
   }

   if (postponedAction) {
      _state |= IDE_STATE_AUTORECOMPILE;
   }
   return _output.execute(appPath, cmdLine, curDir, postponedAction);
}

void AppWindow :: runToCursor()
{
   int index = _tabBar.getCurrentIndex();
   if (index != -1) {
      const TCHAR* module = _tabBar.getTabName(index);
	   Point currentCaret = _mainEditor.getCaret();

      _debugger.runToCursor(module, currentCaret.y, 0);
   }
}

bool AppWindow :: toggleBreakpoint(const TCHAR* source, size_t row)
{
   List<Breakpoint>::Iterator it = _breakpoints.start();
   while (!it.Eof()) {
      if (compstr((*it).source, source) && row==(*it).row) {
         if (_debugger.isStarted()) {
            _debugger.toggleBreakpoint(*it, false);
         }
         _breakpoints.cut(it);
         return false;
      }
      it++;
   }
   Breakpoint breakpoint(source, row);
   _breakpoints.add(breakpoint);

   if (_debugger.isStarted()) {
      _debugger.toggleBreakpoint(breakpoint, true);
   }

   return true;
}

void AppWindow :: toggleBreakpoint()
{
   int index = _tabBar.getCurrentIndex();
   if (index != -1) {
      const TCHAR* module = _tabBar.getTabName(index);
      size_t row = _mainEditor.getCaret().y;

      if (toggleBreakpoint(module, row)) {
         _mainEditor.addMarker(row, STYLE_BREAKPOINT);
      }
      else _mainEditor.removeMarker(row);
   }
}

void AppWindow :: shiftBreakpoints(const TCHAR* source, size_t startRow, int offset)
{
   List<Breakpoint>::Iterator it = _breakpoints.start();
   while (!it.Eof()) {
      if (compstr((*it).source, source)) {
         if ((*it).row >= startRow + offset && (*it).row <= startRow) {
            size_t row = (*it).row;
            it++;
            toggleBreakpoint(source, row);

            continue;
         }
         else if ((*it).row >= startRow) {
            (*it).row = (*it).row + offset;
         }
      }
      it++;
   }
}

void AppWindow :: clearBreakpoints()
{
   _breakpoints.clear();

   if (_debugger.isStarted())
      _debugger.clearBreakpoints();

   _mainEditor.clearMarkers();
}

void AppWindow :: cleanUpProject()
{
   // clean exe and dm files
   if (!emptystr(Settings::project.getTarget()) && Settings::project.getType() != ptLibrary)
   {
      Path targetFile(Settings::project.getPath(), Settings::project.getTarget());
      removeFile(targetFile);

      targetFile.changeExtension(_T("dn"));
      removeFile(targetFile);
   }
   // clean module files
   _ELENA_::ConfigCategoryIterator it = Settings::project.SourceFiles();
   Path modulePath;
   LocalReferenceName moduleName;
   while (!it.Eof()) {
      moduleName.copy(Settings::project.getPackage());
      moduleName.pathToName(it.key());

      const TCHAR* name = moduleName;
      const TCHAR* package = Settings::project.getPackage();
      if (package!=NULL) {
         name += getlength(package) + 1;
      }
      // remove module
      modulePath.copy(Settings::project.getPath());
      modulePath.combine(Settings::project.getOutputPath());
      modulePath.nameToPath(name, _T("nl"));

      removeFile(modulePath);

      // remove debug info module
      modulePath.changeExtension(_T("dnl"));
      removeFile(modulePath);

      it++;
   }
}

void AppWindow :: displayErrors()
{
   String message;
   String file, colStr, rowStr;

   String buffer;
   _output.getOutput(buffer);

   const TCHAR* s = buffer;
   while (s) {
      const TCHAR* err = _tcsstr(s, _T(": error "));
      if (err==NULL) {
         err = _tcsstr(s, _T(": warning "));
      }
      if (err==NULL)
         break;

      const TCHAR* line = err - 1;
      const TCHAR* row = NULL;
      const TCHAR* col = NULL;
      while (true) {
         if (*line=='(') {
            row = line + 1;
         }
         else if (*line==':') {
            col = line + 1;
         }
         else if (*line == '\n')
            break;

         line--;
      }
      s = _tcschr(err, '\n');

      message.copy(err + 2, s - err- 3);
      if (row==NULL) {
         file.clear();
         colStr.clear();
         rowStr.clear();
      }
      else {
         file.copy(line + 1, row - line - 2);
         if (col != NULL) {
            rowStr.copy(row, col - row - 1);
            colStr.copy(col, err - col - 1);
         }
         else {
            rowStr.copy(row, err - row - 1);
            colStr.clear();
         }
      }
      _messageList.addMessage(message, file, rowStr, colStr);

      //break;
   }
   doShowCompilerOutput(true);
   _outputBar.selectTab(1);
}

void AppWindow :: loadHistory(IniConfigFile& config)
{
   _fileHistory.load(config, _T("recentfiles"));
   _projectHistory.load(config, _T("recentprojects"));
}

void AppWindow :: saveHistory(IniConfigFile& config)
{
   _fileHistory.save(config, _T("recentfiles"));
   _projectHistory.save(config, _T("recentprojects"));
}

void AppWindow :: markTab(int index, bool changed)
{
   String caption(_tabBar.getTabName(index));
   if (changed)
      caption.append('*');

   _tabBar.renameTabCaption(index, caption);
}

void AppWindow :: saveAllAsDraft()
{
   for (int index = 0 ; index < _tabBar.getCount() ; index++) {
      if (_mainEditor.isDocumentModified(index)) {
         Path path(_mainEditor.retrievePath(_texts, index));
         path.changeExtension(_T("bak"));

         _mainEditor.saveDocument(index, path);
      }
      else if (_mainEditor.isDocumentUnnamed(index)) {
         Path path(Settings::project.getPath(), _tabBar.getTabName(index));
         path.changeExtension(_T("bak"));

         _mainEditor.saveDocument(index, path);
      }
   }
}