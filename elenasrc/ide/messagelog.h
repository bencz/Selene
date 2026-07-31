//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      MessageLog class header         
//                                              (C)2005-2007, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef messagelogH
#define messagelogH

#include "listview.h"

namespace _GUI_
{

// --- MessageBookmark ---

struct MessageBookmark
{
   TCHAR* file;
   size_t col, row;

   MessageBookmark(const TCHAR* file, const TCHAR* col, const TCHAR* row)
   {
      this->file = _ELENA_::strdup(file);
      this->col =  _ttoi(col);
      this->row = _ttoi(row);
   }

   ~MessageBookmark()
   {
      _ELENA_::freestr(file);
   }
};

// --- MessageLog ---

class MessageLog : public ListView
{
   _ELENA_::Map<int, MessageBookmark*> _bookmarks;

public:
   virtual void create(HINSTANCE instance, HWND wndParent);

   MessageBookmark* getBookmark(int index) { return _bookmarks.get(index); }

   void addMessage(const TCHAR* message, const TCHAR* file, const TCHAR* row, const TCHAR* col);

   virtual void clear();

   MessageLog();
};

} // _GUI_

#endif // messagelogH
