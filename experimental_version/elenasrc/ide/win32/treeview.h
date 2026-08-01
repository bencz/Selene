//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      TreeView class header
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------


#ifndef treeviewH
#define treeviewH

#include "window.h"
#include "layout.h"

namespace _GUI_
{

typedef HTREEITEM TreeViewItem;

class TreeView : public Control
{
protected:
   virtual int getStyle() 
   { 
	   return WS_VISIBLE | WS_CHILD | WS_BORDER | TVS_HASLINES | TVS_HASBUTTONS | TVS_SHOWSELALWAYS;
   }
   virtual const TCHAR* getClassName() { return WC_TREEVIEW; }

public:
   bool isExpanded(TreeViewItem parent);

   TreeViewItem getCurrent();
   TreeViewItem getChild(TreeViewItem parent);
   TreeViewItem getNext(TreeViewItem item);

   int getParam(TreeViewItem item);
   void getCaption(TreeViewItem item, TCHAR* caption, int length);

   void setParam(TreeViewItem item, int param);
   void setCaption(TreeViewItem item, const TCHAR* caption);

   TreeViewItem hitTest(short x, short y);   

   void select(TreeViewItem item);   
   void expand(TreeViewItem item);
   void collapse(TreeViewItem item);

   TreeViewItem insertTo(TreeViewItem parent, const TCHAR* caption, int param);
   void clear(TreeViewItem item);
   void erase(TreeViewItem item);

   TreeView(int left, int top, size_t width, size_t height);
};

} // _GUI_

#endif // treeviewH
