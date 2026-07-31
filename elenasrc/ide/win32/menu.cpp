//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      Menu classes implementation 
//                                              (C)2005-2007, by Alexei Rakov
//---------------------------------------------------------------------------

#include "idecommon.h"
// --------------------------------------------------------------------------
#include "menu.h"

using namespace _GUI_;
using namespace _ELENA_;

// --- BaseMenu ---

void BaseMenu :: loadSubMenu(HWND hParent, int position)
{
   HMENU _hRoot = ::GetMenu(hParent);
   
   _hMenu = ::GetSubMenu(_hRoot, position);
}

void BaseMenu :: loadSubMenu(HWND hParent, int position, int subposition)
{
   HMENU _hRoot = ::GetMenu(hParent);
   
   HMENU hTopMenu = ::GetSubMenu(_hRoot, position);
   
   _hMenu = ::GetSubMenu(hTopMenu, subposition);
}

void BaseMenu :: enableItemById(int id, bool enable) const
{
   int flag = (enable ? MF_ENABLED:(MF_DISABLED | MF_GRAYED)) | MF_BYCOMMAND;
   
   ::EnableMenuItem(_hMenu, id, flag);
}

void BaseMenu :: enableItemByIndex(int index, bool enable) const
{
   int flag = (enable ? MF_ENABLED:(MF_DISABLED | MF_GRAYED)) | MF_BYPOSITION;
   
   ::EnableMenuItem(_hMenu, index, flag);
}

void BaseMenu :: checkItemById(int id, bool checked) const
{
   int flag = (checked ? MF_CHECKED:MF_UNCHECKED) | MF_BYCOMMAND;
   
   ::CheckMenuItem(_hMenu, id, flag);
}

void BaseMenu :: checkItemByIndex(int index, bool checked) const
{
   int flag = (checked ? MF_CHECKED:MF_UNCHECKED) | MF_BYPOSITION;
   
   ::CheckMenuItem(_hMenu, index, flag);
}

// --- ContextMenu ---

ContextMenu :: ContextMenu()
{
}

ContextMenu :: ~ContextMenu()
{
   if (isLoaded())
      ::DestroyMenu(_hMenu);
}

void ContextMenu :: create(int count, MenuInfo* items)
{
   _hMenu = ::CreatePopupMenu();

   for (int i = 0; i < count ; i++) {
	  if (items[i].key==0) {
         ::AppendMenu(_hMenu, MF_SEPARATOR, 0, EMPTY_STRING);             
	  }
	  else ::AppendMenu(_hMenu, MF_STRING, items[i].key, items[i].text); 
   }
}

void ContextMenu :: show(HWND hParent, Point& p) const 
{
   ::TrackPopupMenu(_hMenu, TPM_LEFTALIGN, p.x, p.y, 0, hParent, NULL);
};

// --- Menu ---

Menu :: Menu(HWND owner)
{
   _hMenu = ::GetMenu(owner);
}

void Menu :: insertItem(int index, int command, const TCHAR* caption)
{
   MENUITEMINFO mii;  
     
   mii.cbSize = sizeof(MENUITEMINFO);
   mii.fMask = MIIM_ID | MIIM_STRING;
   mii.fType = MFT_STRING;
   mii.wID = command;
   mii.dwTypeData = (TCHAR*)caption;     
     
   InsertMenuItem(_hMenu, index, TRUE, &mii);   
}

void Menu :: deleteItem(int index)
{
   ::DeleteMenu(_hMenu, index, MF_BYPOSITION);
}

void Menu :: getItemCaption(int index, TCHAR* caption, int length)
{
   ::GetMenuString(_hMenu, index, caption, length, MF_BYPOSITION);
}

void Menu :: setItem(int index, const TCHAR* caption, int command)
{
   MENUITEMINFO mii;  
     
   mii.cbSize = sizeof(MENUITEMINFO);
   mii.fMask = MIIM_ID | MIIM_STRING;
   mii.fType = MFT_STRING;
   mii.wID = command;
   mii.dwTypeData = (TCHAR*)caption;     
          
   ::SetMenuItemInfo(_hMenu, index, TRUE, &mii);  
}
