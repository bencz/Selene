//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      Layout manager header         
//                                              (C)2005-2007, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef layoutH
#define layoutH

#include "window.h"

namespace _GUI_
{

// --- LayoutManager ---

class LayoutManager
{
   Control* _top;
   Control* _left;
   Control* _right;
   Control* _bottom;
   Control* _client;

public:
   void setAsTop(Control* control) { _top = control; }
   void setAsBottom(Control* control) { _bottom = control; }
   void setAsLeft(Control* control) { _left = control; }
   void setAsRight(Control* control) { _right = control; }
   void setAsClient(Control* control) { _client = control; }

   void resizeTo(Rectangle area);

   LayoutManager()
   {
      _top = NULL;
	  _left = NULL;
	  _right = NULL;
	  _bottom = NULL;
	  _client = NULL;
   } 
};

} // _GUI_

#endif // layoutH
