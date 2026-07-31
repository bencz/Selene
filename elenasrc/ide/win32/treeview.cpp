//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      TreeView class implementation
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#include "idecommon.h"
// --------------------------------------------------------------------------
#include "treeview.h"

using namespace _GUI_;

// --- TreeView ---

TreeView :: TreeView(int left, int top, size_t width, size_t height)
   : Control(left, top, width, height)
{
}

TreeViewItem TreeView :: getCurrent()
{
   return TreeView_GetSelection(_self);
}

TreeViewItem TreeView :: getChild(TreeViewItem parent)
{
   return TreeView_GetChild(_self, parent);
}

TreeViewItem TreeView :: getNext(TreeViewItem item)
{
   return TreeView_GetNextItem(_self, item, TVGN_NEXT);
}

int TreeView :: getParam(TreeViewItem item)
{
   TVITEM itemRec; 
    
   itemRec.mask = TVIF_PARAM;
   itemRec.hItem = item;
   itemRec.lParam = -1;

   TreeView_GetItem(_self, &itemRec);

   return itemRec.lParam;
}

bool TreeView :: isExpanded(TreeViewItem parent)
{
   int state = TreeView_GetItemState(_self, parent, TVIS_EXPANDEDONCE);

   return _ELENA_::test(state, TVIS_EXPANDEDONCE);
}

void TreeView :: getCaption(TreeViewItem item, TCHAR* caption, int length)
{
   TVITEM itemRec;

   itemRec.mask = TVIF_TEXT;
   itemRec.hItem = item;
   itemRec.cchTextMax = length;
   itemRec.pszText = caption;

   TreeView_GetItem(_self, &itemRec);
}

void TreeView :: setCaption(TreeViewItem item, const TCHAR* caption)
{
   TVITEM itemRec;

   itemRec.mask = TVIF_TEXT;
   itemRec.hItem = item;
   itemRec.cchTextMax = _ELENA_::getlength(caption);
   itemRec.pszText = (TCHAR*)caption;

   TreeView_SetItem(_self, &itemRec);
}

void TreeView :: setParam(TreeViewItem item, int param)
{
   TVITEM itemRec; 
    
   itemRec.mask = TVIF_PARAM;
   itemRec.hItem = item;
   itemRec.lParam = param;

   TreeView_SetItem(_self, &itemRec);
}

TreeViewItem TreeView :: insertTo(TreeViewItem parent, const TCHAR* caption, int param)
{
   TV_INSERTSTRUCT item;

   item.hParent=parent;
   item.hInsertAfter= parent ? TVI_LAST : TVI_ROOT;
   item.item.mask= TVIF_TEXT | TVIF_PARAM;
   item.item.pszText=(TCHAR*)caption;
   item.item.lParam = param;

   return (TreeViewItem)::SendMessage(_self, TVM_INSERTITEM, 0, (LPARAM)&item);
}

void TreeView :: clear(TreeViewItem parent)
{
   HTREEITEM item = TreeView_GetChild(_self, parent);
   while (item) {
      HTREEITEM next = TreeView_GetNextSibling(_self, item);
      TreeView_DeleteItem(_self, item);

      item = next;
   }
}

void TreeView :: erase(TreeViewItem item)
{
   TreeView_DeleteItem(_self, item);
}

void TreeView :: expand(TreeViewItem item)
{
   TreeView_Expand(_self, item, TVE_EXPAND);
}

void TreeView :: collapse(TreeViewItem item)
{
   TreeView_Expand(_self, item, TVE_COLLAPSE);
}

TreeViewItem TreeView :: hitTest(short x, short y)
{
   TVHITTESTINFO hit;
   
   hit.pt.x = x;
   hit.pt.y = y;
   
   ::ScreenToClient(_self, &hit.pt);
   
   HTREEITEM i = TreeView_HitTest(_self, &hit);
   
   return i;
}

void TreeView :: select(TreeViewItem item)
{
   TreeView_SelectItem(_self, item); 
}
