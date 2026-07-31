//---------------------------------------------------------------------------
//      E L E N A   P r o j e c t:  Platform layer -- POSIX
//
//      Operating-system primitives, POSIX implementation.
//
//      CONTRACT: this header and common/win32/platform.h declare exactly the
//      same names. The build system puts the right directory on the include
//      path, so shared code writes
//
//          #include "platform.h"
//
//      and never asks which platform it is running on. Conditional compilation
//      belongs inside a platform header, never in code that both platforms
//      share.
//---------------------------------------------------------------------------

#ifndef posix_platformH
#define posix_platformH 1

#include <tchar.h>

namespace _ELENA_
{

namespace Platform
{
   // --- paths ---
   //
   // Both '/' and '\\' are accepted as separators on every platform, because
   // the project's own .cfg and .prj files are written with Windows separators
   // and those same strings are parsed by pathToName() to derive module names,
   // so they cannot simply be rewritten. The native separator is used when
   // building a path.

   // Follows TCHAR, so callers need no cast in either character model.
   const TCHAR Separator = _T('/');

   inline bool isSeparator(char ch)    { return ch == '/'  || ch == '\\';  }
   inline bool isSeparator(wchar_t ch) { return ch == L'/' || ch == L'\\'; }

   // --- file system ---
   //
   // Paths are normalized before reaching the C library.

   inline void removeFile(const char* path)
   {
      remove(_elena_posix_::Utf8Path(path));
   }

   inline void removeFile(const wchar_t* path)
   {
      _wremove(path);
   }
}

} // _ELENA_

#endif // posix_platformH
