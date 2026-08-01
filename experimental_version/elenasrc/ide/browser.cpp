//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      Debugger watch window implementation
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#include "idecommon.h"
// --------------------------------------------------------------------------
#include "browser.h"
#include "idesettings.h"

using namespace _GUI_;
using namespace _ELENA_;

#define CAPTION_LEN  512

// --- DebuggerWatch ---

void DebuggerWatch :: expand()
{
   _browser->expand(_root);
}

void DebuggerWatch :: clear()
{
   _browser->clear(_root);
}

TreeViewItem DebuggerWatch :: addNode(const TCHAR* variableName, const TCHAR* className, size_t address)
{
   if (className[0]=='<') {
      _ELENA_::String node(variableName, _T(" = "), className);

      return _browser->insertTo(_root, node, address);
   }
   else {
      _ELENA_::String node(variableName, _T(" = {"), className, TEXT("}"));

      return _browser->insertTo(_root, node, address);
   }
}

void DebuggerWatch :: editNode(TreeViewItem node, const TCHAR* variableName, const TCHAR* className, size_t address)
{
   if (className[0]=='<') {
      _ELENA_::String name(variableName, _T(" = "), className);

      _browser->setCaption(node, name);
   }
   else {
      _ELENA_::String name(variableName, _T(" = {"), className, _T("}"));

      _browser->setCaption(node, name);
   }
   _browser->setParam(node, address);
   _browser->clear(node);
}

void DebuggerWatch :: writeSubWatch(DebugController* controller, TreeViewItem node, size_t address)
{
   if (_deepLevel < 3) {
      DebuggerWatch watch(_browser, node, address, _deepLevel + 1);

      watch.refresh(controller);
   }
   else if (_browser->isExpanded(node)) {
      DebuggerWatch watch(_browser, node, address, 2);

      watch.refresh(controller);
   }
   else refreshNode(node);
}

void DebuggerWatch :: write(DebugController* controller, size_t address, const TCHAR* variableName, const TCHAR* className)
{
   TCHAR itemName[CAPTION_LEN + 1];
   size_t nameLen = getlength(variableName);

   TreeViewItem item = _browser->getChild(_root);
   while (item != NULL) {
      size_t itemAddress = _browser->getParam(item);
      _browser->getCaption(item, itemName, CAPTION_LEN);

      if ((getlength(itemName) > nameLen + 1) &&  compstr(itemName, variableName, nameLen)
         && itemName[nameLen] == ' ')
      {
         if (itemAddress != address) {
            editNode(item, variableName, className, address);
         }
         writeSubWatch(controller, item, address);
         return;
      }
      item = _browser->getNext(item);
   }
   item = addNode(variableName, className, address);

   writeSubWatch(controller, item, address);
}

#ifdef _UNICODE

void DebuggerWatch :: write(DebugController* controller, const char* value)
{
   _browser->clear(_root);

   String unicodeValue(strlen(value) + 1);
   unicodeValue.convert(value);

   _browser->insertTo(_root, unicodeValue, 0);
}

void DebuggerWatch :: write(DebugController* controller, const wchar_t* value)
{
   _browser->clear(_root);

   bool   renamed = false;
   TCHAR  caption[CAPTION_LEN + 1];
   _browser->getCaption(_root, caption, CAPTION_LEN);

   // cut the value from the caption if any
   if (!_tcsstr(caption, _T(" = {"))) {
      TCHAR* type = caption + chrpos(caption, '{');

      _tcsncpy(caption + chrpos(caption, '=') + 2, type, getlength(type) + 1);
      renamed = true;
   } 

   // if line too long put the value as a subnode
   if (getlength(caption) + getlength(value) > CAPTION_LEN) {
      if (renamed)
         _browser->setCaption(_root, caption);

      _browser->insertTo(_root, value, 0);
   }
   // else insert the value into caption
   else {
      insertstr(caption, chrpos(caption, '{'), value);
      _browser->setCaption(_root, caption);
   }   
}

#else

void DebuggerWatch :: write(DebugController* controller, const char* value)
{
   _browser->clear(_root);
   _browser->insertTo(_root, value, 0);
}

#endif

void DebuggerWatch :: write(DebugController* controller, int value)
{
   String number;
   if (Settings::hexNumberMode) {
      number.appendHex(value);
      number.append('h');
   }
   else number.appendInt(value);

   write(controller, number);
}

void DebuggerWatch :: write(DebugController* controller, __int64 value)
{
   String number;
   if (Settings::hexNumberMode) {
      number.appendHex64(value);
      number.append('h');
   }
   else number.appendInt64(value);

   write(controller, number);
}

void DebuggerWatch :: write(DebugController* controller, double value)
{
   String number;
   number.appendDouble(value);

   write(controller, number);
}

void DebuggerWatch :: refresh(_ELENA_::DebugController* controller)
{
   controller->readContext(this, _objectAddress);
}

// --- DebuggerAutoWatch ---

TreeViewItem DebuggerAutoWatch :: addNode(const TCHAR* variableName, const TCHAR* className, size_t address)
{
   TreeViewItem item = DebuggerWatch::addNode(variableName, className, address);

   _items.add((int)item, false);

   return item;
}

void DebuggerAutoWatch :: editNode(TreeViewItem node, const TCHAR* variableName, const TCHAR* className, size_t address)
{
   DebuggerWatch::editNode(node, variableName, className, address);

   refreshNode(node);
}

void DebuggerAutoWatch :: writeSubWatch(DebugController* controller, TreeViewItem node, size_t address)
{
   DebuggerWatch :: writeSubWatch(controller, node, address);

   refreshNode(node);
}

void DebuggerAutoWatch :: refreshNode(TreeViewItem node)
{
   Map<int, bool>::Iterator it = _items.getIt((int)node);
   if (!it.Eof()) {
      *it = false;
   }
   else _items.add((int)node, false);
}

void DebuggerAutoWatch :: clear()
{
   DebuggerWatch :: clear();

   _items.clear();
}

void DebuggerAutoWatch :: refresh(_ELENA_::DebugController* controller)
{
   // mark all auto items
   Map<int, bool>::Iterator it = _items.start();
   while (!it.Eof()) {
      (*it) = true;

      it++;
   }
   controller->readAutoContext(this);

   // remove all marked items
   it = _items.start();
   while (!it.Eof()) {
      if (*it == true) {
         TreeViewItem item = (TreeViewItem)it.key();
         it++;

         _browser->erase(item);
         _items.erase((int)item);
      }
      else it++;
   }
}

// --- ContextBrowswer ---

MenuInfo contextMenu[3] = {
	   {IDM_DEBUG_INSPECT, TEXT("Inspect\tCtrl+I")},
	   {0, NULL},
	   {IDM_DEBUG_SWITCHHEXVIEW, TEXT("Show as hexadecimal")}
};

ContextBrowser :: ContextBrowser(int left, int top, size_t width, size_t height)
   : TreeView(left, top, width, height)
{
   _autoWatch = NULL;
}

void ContextBrowser :: create(HINSTANCE instance, HWND wndParent)
{
   TreeView::create(instance, wndParent);

   TreeViewItem autoItem = insertTo(NULL, TEXT("[auto]"), 0);
   _autoWatch = new DebuggerAutoWatch(this, autoItem);

   _menu.create(3, contextMenu);
}

void ContextBrowser :: browse(DebugController* controller)
{
   browse(controller, getCurrent());    
   expand(getCurrent());
}

void ContextBrowser :: browse(DebugController* controller, TreeViewItem current)
{
   if (current) {
      size_t address = getParam(current);
      DebuggerWatch subWatch(this, current, address, 0);

	  subWatch.refresh(controller);
   }   
}

void ContextBrowser :: refresh(DebugController* controller)
{
   if (_autoWatch) {
      _autoWatch->refresh(controller);

      _autoWatch->expand();
   }
}

void ContextBrowser :: showContextMenu(HWND owner, short x, short y)
{
   Point p(x, y);

   _menu.checkItemById(IDM_DEBUG_SWITCHHEXVIEW, Settings::hexNumberMode);
   _menu.show(owner, p);
}
