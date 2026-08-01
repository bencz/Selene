//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      TabBar class header
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef tabbarH
#define tabbarH

#include "window.h"
#include "layout.h"

namespace _GUI_
{

// --- TabBar ---

class TabBar : public Control
{
protected:
   typedef _ELENA_::List<TCHAR*> TabPages;

   TabPages     _pages;
   Control*     _child;

   bool         _withAbovescore;

   virtual int getStyle() 
   { 
      return WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE | WS_BORDER |
                TCS_FOCUSNEVER | TCS_TABS | TCS_SINGLELINE | TCS_OWNERDRAWFIXED; 
   }
   virtual int getExStyle() { return TCS_EX_FLATSEPARATORS; }
   virtual const TCHAR* getClassName() { return WC_TABCONTROL; }
   virtual const TCHAR* getCaption() { return _T("Tab"); }

   virtual void onSetFocus();
   
public:
   void drawItem(DRAWITEMSTRUCT* item);

   int getCurrentIndex();
   int getCount();

   int getTabIndex(const TCHAR* name);
   const TCHAR* getTabName(int index); 

   virtual void renameTabCaption(int index, const TCHAR* newName);

   virtual void selectTab(int index);
   virtual bool selectTab(const TCHAR* name);

   virtual void addTab(const TCHAR* name);
   virtual void renameTab(int index, const TCHAR* newName);
   virtual void deleteTab(int index);
   virtual void deleteAll();

   virtual void setWidth(size_t width);
   virtual void setHeight(size_t height);
   virtual void resize();

   TabBar(int left, int top, size_t width, size_t height, Control* child, bool withAbovescore);
};

// --- TabBarPlus ---

class TabBarPlus : public TabBar
{
   _ELENA_::List<Control*> _children;

public:
   virtual void addTab(const TCHAR* name, Control* window);
   virtual void addTab(const TCHAR* name)
   {
      TabBar::addTab(name);
   }

   virtual void selectTab(int index);

   virtual void setWidth(size_t width);
   virtual void setHeight(size_t height);
   virtual void resize();

   virtual void refreshChild();

   TabBarPlus(int left, int top, size_t width, size_t height, bool withAbovescore)
	   : TabBar(left, top, width, height, NULL, withAbovescore)
   {
   }
};

} // _GUI_

#endif // tabbarH
