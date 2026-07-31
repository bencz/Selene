//---------------------------------------------------------------------------
//      E L E N A   P r o j e c t:  Platform layer -- Win32
//
//      Operating-system primitives, Win32 implementation.
//
//      CONTRACT: this header and common/posix/platform.h declare exactly the
//      same names. The build system puts the right directory on the include
//      path, so shared code writes
//
//          #include "platform.h"
//
//      and never asks which platform it is running on. Conditional compilation
//      belongs inside a platform header, never in code that both platforms
//      share.
//---------------------------------------------------------------------------

#ifndef win32_platformH
#define win32_platformH 1

#include <tchar.h>
#include <stdio.h>

namespace _ELENA_
{

namespace Platform
{
   // --- paths ---
   //
   // Win32 accepts both separators natively; '\\' is the one produced when
   // building a path.

   // Follows TCHAR, so callers need no cast in either character model.
   const TCHAR Separator = _T('\\');

   inline bool isSeparator(char ch)    { return ch == '\\'  || ch == '/';  }
   inline bool isSeparator(wchar_t ch) { return ch == L'\\' || ch == L'/'; }

   // --- file system ---

   inline void removeFile(const char* path)
   {
      remove(path);
   }

   inline void removeFile(const wchar_t* path)
   {
      _wremove(path);
   }
}

} // _ELENA_

#endif // win32_platformH
