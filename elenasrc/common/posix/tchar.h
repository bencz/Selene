//---------------------------------------------------------------------------
//      E L E N A   P r o j e c t:  POSIX compatibility layer
//
//      Minimal <tchar.h> replacement for Linux / macOS builds.
//
//      Found via `#include <tchar.h>` because the directory containing it is
//      placed on the include path only on POSIX targets, so no source change
//      is needed at the include site.
//
//      Like the MSVC original, TCHAR follows _UNICODE:
//
//         _UNICODE defined    TCHAR = wchar_t   (4 bytes here, 2 on Windows)
//         _UNICODE undefined  TCHAR = char      (UTF-8 -- the target state)
//
//      The UTF-8 (char) configuration is the one the project is migrating to;
//      see docs/plan/16-syntax-evolution.md section S6. Both are kept working
//      so the switch can be flipped and compared.
//---------------------------------------------------------------------------

#ifndef posix_tcharH
#define posix_tcharH 1

#include <wchar.h>
#include <wctype.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// --- MSVC / MinGW integer extensions ---
// Used by _String::appendInt64 / appendHex64 (common/altstrings.h:84-85)
typedef long long          __int64;
typedef unsigned long long __uint64;

//---------------------------------------------------------------------------
// UTF-8 <-> wchar_t conversion
//
// Done by hand rather than via wcstombs()/mbstowcs() so that behaviour does
// not depend on the process locale. The Win32 original used CP_ACP, which is
// locale-dependent and is a known defect; UTF-8 is deterministic.
//---------------------------------------------------------------------------

namespace _elena_posix_
{
   // wchar_t (UCS-4) -> UTF-8. Returns bytes written excluding the terminator.
   inline size_t wideToUtf8(const wchar_t* src, char* dest, size_t destSize)
   {
      size_t out = 0;
      if (!src || !dest || destSize == 0)
         return 0;

      for ( ; *src ; src++) {
         unsigned int c = (unsigned int)*src;

         if (c < 0x80) {
            if (out + 1 >= destSize) break;
            dest[out++] = (char)c;
         }
         else if (c < 0x800) {
            if (out + 2 >= destSize) break;
            dest[out++] = (char)(0xC0 | (c >> 6));
            dest[out++] = (char)(0x80 | (c & 0x3F));
         }
         else if (c < 0x10000) {
            if (out + 3 >= destSize) break;
            dest[out++] = (char)(0xE0 | (c >> 12));
            dest[out++] = (char)(0x80 | ((c >> 6) & 0x3F));
            dest[out++] = (char)(0x80 | (c & 0x3F));
         }
         else {
            if (out + 4 >= destSize) break;
            dest[out++] = (char)(0xF0 | (c >> 18));
            dest[out++] = (char)(0x80 | ((c >> 12) & 0x3F));
            dest[out++] = (char)(0x80 | ((c >> 6) & 0x3F));
            dest[out++] = (char)(0x80 | (c & 0x3F));
         }
      }
      dest[out] = 0;
      return out;
   }

   // A stack buffer holding the UTF-8, native-separator form of a path, for
   // calls into the C library.
   //
   // Backslashes are rewritten to '/'. The project's own configuration files
   // are written with Windows separators -- bin/elc.cfg has
   // "console=templates\console.cfg" and "libpath=..\lib", and every .prj does
   // the same -- and those files must keep working unmodified, because their
   // paths are ALSO parsed by pathToName() to derive module names, so they
   // cannot simply be rewritten.
   //
   // Trade-off: a POSIX file whose name genuinely contains a backslash cannot
   // be opened through this layer. Accepted; no such file exists here, and the
   // alternative is breaking every shipped .cfg and .prj.
   struct Utf8Path
   {
      char buffer[4096];

      void normalize(size_t length)
      {
         for (size_t i = 0 ; i < length ; i++) {
            if (buffer[i] == '\\') buffer[i] = '/';
         }
      }

      Utf8Path(const wchar_t* path)
      {
         normalize(wideToUtf8(path, buffer, sizeof(buffer)));
      }

      Utf8Path(const char* path)
      {
         size_t i = 0;
         for ( ; path[i] && i < sizeof(buffer) - 1 ; i++) buffer[i] = path[i];
         buffer[i] = 0;

         normalize(i);
      }

      operator const char*() const { return buffer; }
   };

   // --- narrow helpers with no glibc equivalent ---

   inline char* strlwr_(char* s)
   {
      if (s) for (char* p = s ; *p ; p++) *p = (char)tolower((unsigned char)*p);
      return s;
   }

   inline char* strupr_(char* s)
   {
      if (s) for (char* p = s ; *p ; p++) *p = (char)toupper((unsigned char)*p);
      return s;
   }

   inline char* intToStr(long long value, char* buffer, int radix)
   {
      if (radix == 16) {
         sprintf(buffer, "%llx", (unsigned long long)value);
      }
      else if (radix == 8) {
         sprintf(buffer, "%llo", (unsigned long long)value);
      }
      else sprintf(buffer, "%lld", value);

      return buffer;
   }

   // --- wide helpers with no glibc equivalent ---

   inline wchar_t* wcslwr_(wchar_t* s)
   {
      if (s) for (wchar_t* p = s ; *p ; p++) *p = (wchar_t)towlower(*p);
      return s;
   }

   inline wchar_t* wcsupr_(wchar_t* s)
   {
      if (s) for (wchar_t* p = s ; *p ; p++) *p = (wchar_t)towupper(*p);
      return s;
   }

   inline wchar_t* intToWide(long long value, wchar_t* buffer, int radix)
   {
      char tmp[68];
      intToStr(value, tmp, radix);

      size_t i = 0;
      for ( ; tmp[i] ; i++) buffer[i] = (wchar_t)tmp[i];
      buffer[i] = 0;

      return buffer;
   }

   // --- wide printf format translation ---
   //
   // In glibc's wprintf, "%s" means char* and "%ls" means wchar_t*. In MSVC's,
   // "%s" already means wchar_t*. Every format string in this codebase is
   // written to the MSVC convention, so rewrite %s -> %ls before handing the
   // format to glibc. Doing it here keeps the message catalogue in errors.h
   // correct for BOTH character models, which matters while the UTF-8
   // migration has the two configurations coexisting.
   inline void fixWideFormat(const wchar_t* format, wchar_t* out, size_t outSize)
   {
      size_t o = 0;

      for (size_t i = 0 ; format[i] && o + 4 < outSize ; i++) {
         out[o++] = format[i];

         if (format[i] == L'%') {
            if (format[i + 1] == L'%') {
               out[o++] = format[++i];              // literal %%
            }
            else if (format[i + 1] == L's') {
               out[o++] = L'l';
               out[o++] = L's';
               i++;
            }
         }
      }
      out[o] = 0;
   }
}

inline int _wprintf_(const wchar_t* format, ...)
{
   wchar_t fixed[2048];
   _elena_posix_::fixWideFormat(format, fixed, 2048);

   va_list args;
   va_start(args, format);
   int result = vwprintf(fixed, args);
   va_end(args);

   return result;
}

inline int _vwprintf_(const wchar_t* format, va_list args)
{
   wchar_t fixed[2048];
   _elena_posix_::fixWideFormat(format, fixed, 2048);

   return vwprintf(fixed, args);
}

//---------------------------------------------------------------------------
// Wide-character file routines
//
// These are needed regardless of the _UNICODE setting, because the codebase
// declares both char and wchar_t overloads side by side (removeFile, strdup,
// getlength, ...).
//---------------------------------------------------------------------------

inline FILE* _wfopen_(const wchar_t* path, const wchar_t* mode)
{
   _elena_posix_::Utf8Path p(path);

   char m[8];
   size_t i = 0;
   for ( ; mode[i] && i < sizeof(m) - 1 ; i++) m[i] = (char)mode[i];
   m[i] = 0;

   return fopen(p, m);
}

inline int _waccess(const wchar_t* path, int mode)
{
   _elena_posix_::Utf8Path p(path);
   return access(p, mode);
}

inline int _wremove(const wchar_t* path)
{
   _elena_posix_::Utf8Path p(path);
   return remove(p);
}

inline int _wmkdir(const wchar_t* path)
{
   _elena_posix_::Utf8Path p(path);
   return mkdir(p, 0755);
}

inline int _wstat_(const wchar_t* path, struct stat* info)
{
   _elena_posix_::Utf8Path p(path);
   return stat(p, info);
}

// Narrow counterparts, which must also normalize separators.
inline FILE* _afopen_(const char* path, const char* mode)
{
   _elena_posix_::Utf8Path p(path);
   return fopen(p, mode);
}

inline int _astat_(const char* path, struct stat* info)
{
   _elena_posix_::Utf8Path p(path);
   return stat(p, info);
}

// files.h declares `struct _stat`
#define _stat stat

//---------------------------------------------------------------------------
// TCHAR selection
//---------------------------------------------------------------------------

#ifdef _UNICODE

typedef wchar_t TCHAR;
typedef wchar_t _TCHAR;

#define _T(x)     L##x
#define __T(x)    L##x
#define _TEXT(x)  L##x
#ifndef TEXT
#define TEXT(x)   L##x
#endif

#define _tcslen    wcslen
#define _tcscmp    wcscmp
#define _tcschr    wcschr
#define _tcsrchr   wcsrchr
#define _tcscpy    wcscpy
#define _tcsncpy   wcsncpy
#define _tcscat    wcscat
#define _tcsncat   wcsncat
#define _tcsstr    wcsstr
#define _tcstod    wcstod
#define _tcstoul   wcstoul
#define _tcstol    wcstol

#define _tcslwr    _elena_posix_::wcslwr_
#define _tcsupr    _elena_posix_::wcsupr_

#define _tprintf   _wprintf_
#define _ftprintf  fwprintf
#define _stprintf  swprintf
#define _vtprintf  _vwprintf_
#define _vftprintf vfwprintf

#define _tfopen    _wfopen_
#define _tstat     _wstat_

inline int _ttoi(const wchar_t* s) { return s ? (int)wcstol(s, NULL, 10) : 0; }

inline wchar_t* _itot(int value, wchar_t* buffer, int radix)
   { return _elena_posix_::intToWide(value, buffer, radix); }
inline wchar_t* _ltot(long value, wchar_t* buffer, int radix)
   { return _elena_posix_::intToWide(value, buffer, radix); }
inline wchar_t* _i64tot(long long value, wchar_t* buffer, int radix)
   { return _elena_posix_::intToWide(value, buffer, radix); }

#else // ANSI / UTF-8

typedef char TCHAR;
typedef char _TCHAR;

#define _T(x)     x
#define __T(x)    x
#define _TEXT(x)  x
#ifndef TEXT
#define TEXT(x)   x
#endif

#define _tcslen    strlen
#define _tcscmp    strcmp
#define _tcschr    strchr
#define _tcsrchr   strrchr
#define _tcscpy    strcpy
#define _tcsncpy   strncpy
#define _tcscat    strcat
#define _tcsncat   strncat
#define _tcsstr    strstr
#define _tcstod    strtod
#define _tcstoul   strtoul
#define _tcstol    strtol

#define _tcslwr    _elena_posix_::strlwr_
#define _tcsupr    _elena_posix_::strupr_

#define _tprintf   printf
#define _ftprintf  fprintf
#define _stprintf  sprintf
#define _vtprintf  vprintf
#define _vftprintf vfprintf

#define _tfopen    _afopen_
#define _tstat     _astat_

inline int _ttoi(const char* s) { return s ? (int)strtol(s, NULL, 10) : 0; }

inline char* _itot(int value, char* buffer, int radix)
   { return _elena_posix_::intToStr(value, buffer, radix); }
inline char* _ltot(long value, char* buffer, int radix)
   { return _elena_posix_::intToStr(value, buffer, radix); }
inline char* _i64tot(long long value, char* buffer, int radix)
   { return _elena_posix_::intToStr(value, buffer, radix); }

#endif

// Windows _gcvt(value, digits, buffer) -- narrow output on both configurations
inline char* _gcvt(double value, int digits, char* buffer)
{
   snprintf(buffer, 32, "%.*g", digits, value);
   return buffer;
}

#endif // posix_tcharH
