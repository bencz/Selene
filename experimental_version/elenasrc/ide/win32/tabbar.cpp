//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      TabBar class implementation
//                                              (C)2005-2009, by Alexei Rakov
//                                based on (C)2003 Don HO ( donho@altern.org )
//---------------------------------------------------------------------------

#include "idecommon.h"
// --------------------------------------------------------------------------
#include "tabbar.h"

using namespace _GUI_;
using namespace _ELENA_;

// --- TabBar ---

TabBar :: TabBar(int left, int top, size_t width, size_t height, Control* child, bool withAbovescore)
   : Control(left, top, width, height), _pages(NULL, freestr)
{
   _withAbovescore = withAbovescore;
   _child = child;
   if (_child) {
      _child->setCoordinate(4, 28);
   }   
}

int TabBar :: getTabIndex(const TCHAR* name)
{
   return searchInList(_pages, name);
}

int TabBar :: getCurrentIndex()
{
   return (int)::SendMessage(_self, TCM_GETCURSEL, 0, 0);
}

int TabBar :: getCount()
{
   return _pages.Count();
}

void TabBar :: selectTab(int index)
{
   int previous = ::SendMessage(_self, TCM_SETCURSEL, index, 0);
   if (previous != index) {
      notify(TCN_SELCHANGE);
   }
}

bool TabBar :: selectTab(const TCHAR* name)
{
   int index = getTabIndex(name);
   if (index != -1) {
      selectTab(getTabIndex(name));

	  return true;
   }
   else return false;
}

const TCHAR* TabBar :: getTabName(int index)
{
   return *_pages.get(index);
}

void TabBar :: addTab(const TCHAR* name)
{
   TCHAR* pageName = _ELENA_::strdup(name);

   _pages.add(pageName);

   TCITEM tie;
   tie.mask = TCIF_TEXT | TCIF_IMAGE;
   tie.iImage = -1;
   tie.pszText = pageName;

   ::SendMessage(_self, TCM_INSERTITEM, _pages.Count() - 1, (LPARAM)&tie);

   selectTab(_pages.Count() - 1);
}

void TabBar :: renameTab(int index, const TCHAR* newName)
{
   TCHAR* pageName = _ELENA_::strdup(newName);

   TabPages::Iterator it = _pages.get(index);

   _pages.set(it, pageName);

   TCITEM tie;
   tie.mask = TCIF_TEXT | TCIF_IMAGE;
   tie.iImage = -1;
   tie.pszText = pageName;

   ::SendMessage(_self, TCM_SETITEM, index, (LPARAM)&tie);
}

void TabBar :: renameTabCaption(int index, const TCHAR* newName)
{
   TCITEM tie;
   tie.mask = TCIF_TEXT | TCIF_IMAGE;
   tie.iImage = -1;
   tie.pszText = (TCHAR*)newName;

   ::SendMessage(_self, TCM_SETITEM, index, (LPARAM)&tie);
}

void TabBar :: deleteTab(int index)
{
   ::SendMessage(_self, TCM_DELETEITEM, index, 0);  
   _pages.cut(*_pages.get(index));

   if(_pages.Count() > 0) {
      if ((size_t)index >= _pages.Count()) {
         index = _pages.Count() - 1;
      }
      selectTab(index);
   }
};

void TabBar :: deleteAll()
{
   ::SendMessage(_self, TCM_DELETEALLITEMS, 0, 0);
   _pages.clear();
};

void TabBar :: onSetFocus()
{
   if (_child) {
      _child->setFocus();
   }
}

void TabBar :: setWidth(size_t width)
{   
   Control::setWidth(width);
   if (_child) {
      _child->setWidth(width - 8);
   }
}

void TabBar :: setHeight(size_t height)
{ 
   Control::setHeight(height);
   if (_child) {
      _child->setHeight(height - 32);
   }
}

void TabBar :: resize()
{
   Control::resize();
   _child->resize();	
}

void TabBar :: drawItem(DRAWITEMSTRUCT* item)
{
   Canvas    canvas(item->hDC);
   Rectangle rect(item->rcItem.left, item->rcItem.top, item->rcItem.right - item->rcItem.left + 1, item->rcItem.bottom - item->rcItem.top + 1);

	// For some bizarre reason the rcItem you get extends above the actual
	// drawing area. We have to workaround this "feature".
   rect.bottomRight.y += ::GetSystemMetrics(SM_CYEDGE);

   int  index = item->itemID;
   bool isSelected = (index == getCurrentIndex());

   TCHAR label[IDENTIFIER_LEN];
	TCITEM tci;
	tci.mask = TCIF_TEXT;
	tci.pszText = label;     
	tci.cchTextMax = IDENTIFIER_LEN-1;
	::SendMessage(_self, TCM_GETITEM, index, (LPARAM)&tci);

   //const TCHAR* label = getTabName(index);

   canvas.fillRectangle(rect, Canvas::ButtonFace());
	if (isSelected) {
      if (_withAbovescore) {
         Rectangle barRect(rect);
         barRect.bottomRight.y = 6;

         canvas.fillRectangle(barRect, Colour(255, 190, 128));
      }
   }
   else {
      canvas.fillRectangle(rect, Colour(220, 220, 220));
   }   

   canvas.setTransparentMode(true);

	if (isSelected) {
      //rect.topLeft.y -= ::GetSystemMetrics(SM_CYEDGE);
      rect.topLeft.y += 1;

      canvas.drawText(rect, label, getlength(label), Colour(0, 0, 0), true);
	} 
	else canvas.drawText(rect, label, getlength(label), Colour(128, 128, 128), true);
}

// --- TabBarPlus ---

void TabBarPlus :: addTab(const TCHAR* name, Control* child)
{
   child->setCoordinate(4, 28);
   child->setWidth(_width - 14);
   child->setHeight(_height - 36);

   _children.add(child);
   _child = child;

   TabBar::addTab(name);
}

void TabBarPlus :: selectTab(int index)
{
   _child->hide();

   _child = *_children.get(index);
   if (_child) {
      _child->show();
   }
   TabBar::selectTab(index);
}

void TabBarPlus :: setWidth(size_t width)
{   
   Control::setWidth(width);
   List<Control*>::Iterator it = _children.start();
   while (!it.Eof()) {
	  (*it)->setWidth(width - 14);
      it++;
   }   
}

void TabBarPlus :: setHeight(size_t height)
{ 
   Control::setHeight(height);
   List<Control*>::Iterator it = _children.start();
   while (!it.Eof()) {
	  (*it)->setHeight(height - 36);
      it++;
   }   
}

void TabBarPlus :: resize()
{
   Control::resize();
   List<Control*>::Iterator it = _children.start();
   while (!it.Eof()) {
	  (*it)->resize();
      it++;
   }   
}

void TabBarPlus :: refreshChild()
{
   Control* current = *_children.get(getCurrentIndex());
   if (_child != current) {
      _child->hide();
      current->show();

      _child = current;
   }
}
