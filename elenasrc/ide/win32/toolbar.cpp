//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      ToolBar class implementation
//                                              (C)2005-2007, by Alexei Rakov
//---------------------------------------------------------------------------

#include "idecommon.h"
// --------------------------------------------------------------------------
#include "toolbar.h"

using namespace _GUI_;

// --- ToolBar ---

ToolBar :: ToolBar()
   : Control(0, 0, 800, 25)
{
   _buttons = NULL;
}

ToolBar :: ~ToolBar()
{
   if (_buttons) {
      delete[] _buttons;
   }
}

void ToolBar :: assign(int iconSize, int count, ToolBarButton* buttons)
{
   ::SendMessage(_self, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);

   //::SendMessage(_self, TB_LOADIMAGES, IDB_STD_SMALL_COLOR, (LPARAM)HINST_COMMCTRL);
   TBADDBITMAP bitmap = {_instance, 0};
   _buttons = new TBBUTTON[count];
   for (int i = 0 ; i < count ; i++) {
      _buttons[i].idCommand = buttons[i].command;
	  _buttons[i].fsState = TBSTATE_ENABLED;
   	  _buttons[i].dwData = 0;
	  _buttons[i].iString = 0;

      if (buttons[i].command != 0) {
         bitmap.nID = buttons[i].iconId;
	     _buttons[i].iBitmap = ::SendMessage(_self, TB_ADDBITMAP, 1, (LPARAM)&bitmap);

		 _buttons[i].fsStyle = BTNS_BUTTON;
      }
      else {
	     _buttons[i].iBitmap = 0;
		 _buttons[i].fsStyle = BTNS_SEP;
      }
   }
   ::SendMessage(_self, TB_SETBUTTONSIZE, 0, MAKELONG(iconSize, iconSize));
   ::SendMessage(_self, TB_ADDBUTTONS, (WPARAM)count, (LPARAM)_buttons);
   ::SendMessage(_self, TB_AUTOSIZE, 0, 0);
}

void ToolBar :: enableButton(int id, bool enabled)
{
   TBBUTTONINFO info;
   info.cbSize = sizeof(TBBUTTONINFO);
   info.dwMask = TBIF_STATE;
   info.fsState = (BYTE)(enabled ? TBSTATE_ENABLED : 0);

   ::SendMessage(_self, TB_SETBUTTONINFO, id, (LPARAM)&info);
}
