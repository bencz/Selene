//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      Menu classes header 
//                                              (C)2005-2007, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef menuH
#define menuH

namespace _GUI_
{

// --- BaseMenu ---

class BaseMenu
{
protected:
   HMENU _hMenu;

public:
   bool isLoaded() const { return _hMenu != NULL; }

   void loadSubMenu(HWND hParent, int position);
   void loadSubMenu(HWND hParent, int position, int subposition);

   void enableItemById(int id, bool doEnable) const;
   void enableItemByIndex(int index, bool doEnable) const;

   void checkItemById(int id, bool checked) const;
   void checkItemByIndex(int index, bool checked) const;

   BaseMenu() { _hMenu = NULL; }
   virtual ~BaseMenu() {}
};

// --- ContextMenu ---

struct MenuInfo
{
   size_t key;
   TCHAR* text;
};

class ContextMenu : public BaseMenu
{
public:
   void create(int count, MenuInfo* items);

   void show(HWND parent, Point& p) const;

   ContextMenu();
   ~ContextMenu();
};

// --- Menu ---

class Menu : public BaseMenu
{
public:
   void insertItem(int index, int command, const TCHAR* caption);
   void deleteItem(int index);
   void setItem(int index, const TCHAR* name, int command);

   void getItemCaption(int index, TCHAR* caption, int length);

   Menu(HWND owner);
   Menu() { _hMenu = NULL; }
   ~Menu() {}
};

} // _GUI_

#endif // menuH
