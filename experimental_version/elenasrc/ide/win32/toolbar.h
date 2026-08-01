//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      ToolBar class header
//                                              (C)2005-2007, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef toolbarH
#define toolbarH

#include "window.h"
#include "layout.h"

namespace _GUI_
{

struct ToolBarButton
{
   int command;
   int iconId; 
};

class ToolBar : public Control
{
protected:
   TBBUTTON*    _buttons;

   virtual int getStyle() 
   { 
	   return WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | TBSTYLE_TOOLTIPS |TBSTYLE_FLAT | CCS_TOP 
		   | BTNS_AUTOSIZE; 
   }
   virtual int getExStyle() { return WS_EX_PALETTEWINDOW; }
   virtual const TCHAR* getClassName() { return TOOLBARCLASSNAME; }

public:
   void assign(int iconSize, int count, ToolBarButton* buttons);

   void enableButton(int id, bool enabled);

   ToolBar();
   virtual ~ToolBar();
};

} // _GUI_

#endif // toolbarH
