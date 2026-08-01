//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      Debugger watch window header
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef browserH
#define browserH

#include "treeview.h"
#include "layout.h"
#include "debugcontroller.h"
#include "menu.h"

namespace _GUI_
{

class ContextBrowser;

// --- DebuggerWatch --

class DebuggerWatch : public _ELENA_::_DebuggerWatch
{
protected:
   ContextBrowser* _browser;
   TreeViewItem    _root;
   size_t          _objectAddress;
   size_t          _deepLevel;

   virtual TreeViewItem addNode(const TCHAR* variableName, const TCHAR* className, size_t address);
   virtual void editNode(TreeViewItem node, const TCHAR* variableName, const TCHAR* className, size_t address);

   virtual void refreshNode(TreeViewItem) {}

   virtual void writeSubWatch(_ELENA_::DebugController* controller, TreeViewItem node, size_t address);

public:
   virtual void expand();
   virtual void clear();

   virtual void write(_ELENA_::DebugController* controller, size_t address, 
                        const TCHAR* variableName, const TCHAR* className);
   virtual void write(_ELENA_::DebugController* controller, const char* value);
   virtual void write(_ELENA_::DebugController* controller, const wchar_t* value);
   virtual void write(_ELENA_::DebugController* controller, int value);
   virtual void write(_ELENA_::DebugController* controller, double value);
   virtual void write(_ELENA_::DebugController* controller, __int64 value);

   virtual void refresh(_ELENA_::DebugController* controller);

   DebuggerWatch(ContextBrowser* browser, TreeViewItem root, size_t objectAddress, size_t deepLevel) 
   { 
      this->_browser = browser; 
      this->_root = root; 
      this->_objectAddress = objectAddress;
      this->_deepLevel = deepLevel;
   }
};

// --- DebuggerAutoWatch ---

class DebuggerAutoWatch : public DebuggerWatch
{
   _ELENA_::Map<int, bool> _items;

   virtual TreeViewItem addNode(const TCHAR* variableName, const TCHAR* className, size_t address);
   virtual void editNode(TreeViewItem node, const TCHAR* variableName, const TCHAR* className, size_t address);
   virtual void refreshNode(TreeViewItem);

   virtual void writeSubWatch(_ELENA_::DebugController* controller, TreeViewItem node, size_t address);

public:
   void showContextMenu(short x, short y);

   virtual void refresh(_ELENA_::DebugController* controller);

   virtual void clear();

   DebuggerAutoWatch(ContextBrowser* browser, TreeViewItem root) 
      : DebuggerWatch(browser, root, 0, 0)
   {
   }
};

// --- ContextBrowser ---

class ContextBrowser : public TreeView
{
   ContextMenu        _menu;
   DebuggerAutoWatch* _autoWatch; 

public:
   virtual void create(HINSTANCE instance, HWND wndParent);

   void refresh(_ELENA_::DebugController* controller);
   void browse(_ELENA_::DebugController* controller);
   void browse(_ELENA_::DebugController* controller, TreeViewItem current);
   void showContextMenu(HWND owner, short x, short y);

   void reset() { _autoWatch->clear(); }

   ContextBrowser(int left, int top, size_t width, size_t height);
   virtual ~ContextBrowser() { freeobj(_autoWatch); }
};

} // _GUI_

#endif // browserH
