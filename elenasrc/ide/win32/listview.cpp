//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      ListView class implementation
//                                              (C)2005-2007, by Alexei Rakov
//---------------------------------------------------------------------------

#include "idecommon.h"
// --------------------------------------------------------------------------
#include "listview.h"

using namespace _GUI_;
using namespace _ELENA_;

// --- TreeView ---

ListView :: ListView(int left, int top, size_t width, size_t height)
   : Control(left, top, width, height)
{
}

void ListView :: create(HINSTANCE instance, HWND wndParent)
{
   Control::create(instance, wndParent);

   ListView_SetExtendedListViewStyle(_self, LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT);
}

void ListView :: addColumn(const TCHAR* header, int column, int width, int alignment)
{
   LVCOLUMN lvColumn; 
   
   lvColumn.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM; 
   lvColumn.iSubItem = column;
   lvColumn.pszText = (TCHAR*)header;	
   lvColumn.cx = width;    
   lvColumn.fmt = alignment;
	  
   ListView_InsertColumn(_self, column, &lvColumn);
}

int ListView :: addItem(const TCHAR* item)
{
   if (emptystr(item))  
      return -1;
     
   int row = ListView_GetItemCount(_self);

   LVITEM lvItem; 
   
   lvItem.mask = LVIF_TEXT; 
   lvItem.state = 0;    
   lvItem.stateMask = 0; 
   lvItem.iItem = row;
   lvItem.iSubItem = 0;
   lvItem.pszText = (TCHAR*)item; 

   row = ::SendMessage(_self, LVM_INSERTITEM, 0, (LPARAM)&lvItem);

   return row;
}

void ListView :: setItemText(const TCHAR* item, int row, int column)
{
   if (emptystr(item))  
      return;

   LVITEM lvItem; 
   
   lvItem.mask = LVIF_TEXT; 
   lvItem.state = 0;    
   lvItem.stateMask = 0; 
   lvItem.iItem = row;
   lvItem.iSubItem = column;
   lvItem.pszText = (TCHAR*)item; 

   ::SendMessage(_self, LVM_SETITEMTEXT, row, (LPARAM)&lvItem);
}

void ListView :: clear()
{
   ListView_DeleteAllItems(_self);
}
