//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Common Library
//
//		This file contains String classes implementations
//
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#include "common.h"
// --------------------------------------------------------------------------
#include "altstrings.h"

using namespace _ELENA_;

// --- _String ---

void _String :: appendHex(int n)
{
   int pos = Length();
   reserve(pos + 12);

   TCHAR* s = getBody();
   _itot(n, s + pos, 16);

   _tcsupr(s + pos);
}

void _String :: appendHex64(__int64 n)
{
   int pos = Length();

   reserve(pos + 22);

   TCHAR* s = getBody();
   _i64tot(n, s + pos, 16);

   _tcsupr(s + pos);
}

void _String :: appendInt64(__int64 n)
{
   int pos = Length();

   reserve(pos + 22);

   TCHAR* s = getBody();
   _i64tot(n, s + pos, 10);

   _tcsupr(s + pos);
}

void _String :: appendInt(int n)
{
   int len = Length();

   reserve(len + 12);

   TCHAR* s = getBody();
   _itot(n, s + len, 10);
}

void _String :: appendLong(long n)
{
   int len = Length();

   reserve(len + 12);

   TCHAR* s = getBody();
   _ltot(n, s + len, 10);
}

void _String :: appendDouble(double n)
{
   int len = Length();

   reserve(len + 17);

   TCHAR* s = getBody();
   doubleToStr(n, 12, s + Length());
   if (s[len - 1]=='.')
      append(_T("0"));
}

// --- String ---

String :: String()
{
   _string = NULL;
   allocate(STR_PAGE_SIZE);
}

String :: String(size_t length)
{
   _string = NULL;
   allocate(length);
}

String :: String(const TCHAR* s)
{
   _string = NULL;
   if (!emptystr(s)) {
      allocate(getlength(s) + 1);
      append(s);
   }
   else allocate(STR_PAGE_SIZE);
}

String :: String(const TCHAR* s1, const TCHAR* s2)
{
   _string = NULL;
   allocate(getlength(s1) + getlength(s2) + 1);

   append(s1);
   append(s2);
}

String :: String(const TCHAR* s1, const TCHAR* s2, const TCHAR* s3)
{
   _string = NULL;
   allocate(getlength(s1) + getlength(s2) + getlength(s3) + 1);

   append(s1);
   append(s2);
   append(s3);
}

String :: String(const TCHAR* s1, const TCHAR* s2, const TCHAR* s3, const TCHAR* s4)
{
   _string = NULL;
   allocate(getlength(s1) + getlength(s2) + getlength(s3) + getlength(s4) + 1);

   append(s1);
   append(s2);
   append(s3);
   append(s4);
}

String :: String(const TCHAR* s, size_t length)
{
   _string = NULL;
   allocate(length);

   append(s, length);
}

bool String :: _copy(const TCHAR* s, size_t length)
{
   reallocate(length + 1);
   _tcsncpy(_string, s, length);

   _string[length] = 0;
   return true;
}

bool String :: _append(const TCHAR* s, size_t length)
{
   if (!emptystr(s)) {
      reallocate(getlength(*this) + length + 1);
      _tcsncat(_string, s, length);
   }
   return true;
}

void String :: trim(TCHAR ch)
{
   size_t length = getlength(_string);
   while (length > 0 && _string[length - 1] == ch) {
      _string[length - 1] = 0;
      length = getlength(_string);
   }
}

#ifdef _UNICODE

void String :: convert(const char* s)
{
   reserve(strlen(s) + 1);
   clear();

   ansiToUnicode(s, _string, strlen(s));
   _string[strlen(s)] = 0;
}

#endif

