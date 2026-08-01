//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Engine
//
//		This file contains the common templates, classes,
//                                              (C)2005-2008, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef unicodeHdr
#define unicodeHdr 1

#ifndef WINVER
#define WINVER 0x0500
#endif

#include <windows.h>

namespace _elena_win32_
{
   // UTF-16 -> UTF-8.
   //
   // Win32 hands out UTF-16 (command line, file names, console), so a UTF-8
   // build has to convert at the API boundary. CP_UTF8 rather than CP_ACP:
   // the ACP is locale-dependent, which is why the original conversion made
   // compiler output vary by machine.
   inline size_t wideToUtf8(const wchar_t* src, char* dest, size_t destSize)
   {
      int written = ::WideCharToMultiByte(CP_UTF8, 0, src, -1, dest,
                                          (int)destSize, NULL, NULL);

      return (written > 0) ? (size_t)(written - 1) : 0;   // less the terminator
   }

   // UTF-8 -> UTF-16.
   inline size_t utf8ToWide(const char* src, wchar_t* dest, size_t destChars)
   {
      int written = ::MultiByteToWideChar(CP_UTF8, 0, src, -1, dest,
                                          (int)destChars);

      return (written > 0) ? (size_t)(written - 1) : 0;
   }
}

inline void ansiToUnicode(const char* sour, wchar_t* dest, size_t length)
{
   MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, sour, length, dest, length);
}

inline bool unicodeToAnsi(const wchar_t* sour, char* dest, size_t length)
{
   BOOL flag = false;

   WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, sour, length, dest, length, "?", &flag);

   return !flag;
}


#endif // unicodeHdr
