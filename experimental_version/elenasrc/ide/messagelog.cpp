//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      MessageLog class implementation
//                                              (C)2005-2007, by Alexei Rakov
//---------------------------------------------------------------------------

#include "idecommon.h"
// --------------------------------------------------------------------------
#include "messagelog.h"

using namespace _GUI_;
using namespace _ELENA_;

// --- MessageLog ---

MessageLog :: MessageLog()
   : ListView(0, 0, 800, 60), _bookmarks(NULL, freeobj)
{
}

void MessageLog :: create(HINSTANCE instance, HWND wndParent)
{
   ListView::create(instance, wndParent);

   addColumn(TEXT("Description"), 0, 600, LVCFMT_LEFT);
   addColumn(TEXT("File"), 1, 100, LVCFMT_LEFT);
   addColumn(TEXT("Line"), 2, 100, LVCFMT_LEFT);
   addColumn(TEXT("Column"), 3, 100, LVCFMT_LEFT);
}

void MessageLog :: addMessage(const TCHAR* message, const TCHAR* file, const TCHAR* row, const TCHAR* col)
{
   MessageBookmark* bookmark = new MessageBookmark(file, col, row);

   int index = addItem(message);
   setItemText(file, index, 1);
   setItemText(row, index, 2);
   setItemText(col, index, 3);

   _bookmarks.add(index, bookmark);
}

void MessageLog :: clear()
{
   ListView :: clear();

   _bookmarks.clear();
}
