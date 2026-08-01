//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      ListView class header
//                                              (C)2005-2007, by Alexei Rakov
//---------------------------------------------------------------------------


#ifndef listviewH
#define listviewH

#include "window.h"

namespace _GUI_
{

class ListView : public Control
{
protected:
   virtual int getStyle() 
   { 
      return WS_VISIBLE | WS_BORDER | WS_CHILD | LVS_REPORT/* | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES*/; 
   }
   virtual const TCHAR* getClassName() { return WC_LISTVIEW; }

public:
   virtual void ListView :: create(HINSTANCE instance, HWND wndParent);

   void addColumn(const TCHAR* header, int column, int width, int alignment);

   int addItem(const TCHAR* item);
   void setItemText(const TCHAR* item, int row, int column);

   virtual void clear();

   ListView(int left, int top, size_t width, size_t height);
};

} // _GUI_

#endif // treeviewH
