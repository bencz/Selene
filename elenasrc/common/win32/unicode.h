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
