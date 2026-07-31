//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Common Library
//
//		This file contains the common ELENA Project routine functions
//
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef toolsH
#define toolsH 1

namespace _ELENA_
{
// --- resource freeing routines ---

inline void freestr(char* s)
{
   if (s != NULL) {
      free(s);
   }
}

inline void freestr(wchar_t* s)
{
   if (s != NULL) {
      free(s);
   }
}

template <class T> void freeobj(T obj)
{
   if (obj != NULL) {
      delete obj;
   }
}

// --- miscellaneous routines ---

inline bool test(int number, int mask)
{
   return ((number & mask) == mask);
}

inline bool isbetween(int starting, int len , int value)
{
   return (starting < value && value < starting + len);
}

inline void removeFile(const char* name)
{
   Platform::removeFile(name);
}

inline void removeFile(const wchar_t* name)
{
   Platform::removeFile(name);
}

// --- calcTabShift ---

inline size_t calcTabShift(int col, int tabSize)
{
   int nextCol = (col / tabSize * tabSize) + tabSize;

   return nextCol - col;
}

// --- miscellaneous string routines ---

inline int lastchrpos(const TCHAR* s, const TCHAR c)
{
   const TCHAR* p = _tcsrchr(s, c);
   if (p==NULL) {
      return -1;
   }
   else return p - s;
}

inline int lastchrpos(const TCHAR* s, const TCHAR c, int defvalue)
{
   const TCHAR* p = _tcsrchr(s, c);
   if (p==NULL) {
      return defvalue;
   }
   else return p - s;
}

inline int chrpos(const TCHAR* s, const TCHAR c)
{
   const TCHAR* p = _tcschr(s, c);
   if (p==NULL) {
      return -1;
   }
   else return p - s;
}

// --- path separator handling ---
//
// Windows accepts both '\\' and '/', POSIX only '/', but the project's own
// .cfg and .prj files are written with '\\'. Both characters are therefore
// accepted as separators on every platform, while the native one is used
// when building a path. This is what lets bin/elc.cfg keep "libpath=..\\lib"
// and still resolve on Linux and macOS.

#define PATH_SEPARATOR (Platform::Separator)

inline bool isPathSeparator(TCHAR ch)
{
   return Platform::isSeparator(ch);
}

inline int lastPathSeparatorPos(const TCHAR* s)
{
   int last = -1;
   if (s) for (int i = 0 ; s[i] ; i++) {
      if (isPathSeparator(s[i])) last = i;
   }
   return last;
}

inline int firstPathSeparatorPos(const TCHAR* s)
{
   if (s) for (int i = 0 ; s[i] ; i++) {
      if (isPathSeparator(s[i])) return i;
   }
   return -1;
}

inline bool grtstr(const TCHAR* s1, const TCHAR* s2)
{
   if (s1 && s2) return (_tcscmp(s1, s2) > 0);
   else return false;
}

inline bool compstr(const char* s1, const char* s2)
{
   if (s1 && s2) return (strcmp(s1, s2)==0);
   else return false;
}

inline bool compstr(const wchar_t* s1, const wchar_t* s2)
{
   if (s1 && s2) return (wcscmp(s1, s2)==0);
   else return false;
}

inline bool compstr(const char* s1, const char* s2, size_t n)
{
   if (s1 && s2) return (strncmp(s1, s2, n)==0);
   else return false;
}

inline bool compstr(const wchar_t* s1, const wchar_t* s2, size_t n)
{
   if (s1 && s2) return (wcsncmp(s1, s2, n)==0);
   else return false;
}

inline bool emptystr(const char* s)
{
   return (s == NULL || s[0]==0);
}

inline bool emptystr(const wchar_t* s)
{
   return (s == NULL || s[0]==0);
}

inline char* strdup(const char* s)
{
   if (emptystr(s)) return NULL;

   char* dup = (char*)malloc(strlen(s) + 1);
   return strcpy(dup, s);
}

inline wchar_t* strdup(const wchar_t* s)
{
   if (emptystr(s)) return NULL;

   wchar_t* dup = (wchar_t*)malloc((wcslen(s) + 1) * sizeof(wchar_t));
   return wcscpy(dup, s);
}

inline void createstr(char* &s, size_t length)
{
   s = (char*)malloc(length);
}

inline void createstr(wchar_t* &s, size_t length)
{
   s = (wchar_t*)malloc(length * sizeof(wchar_t));
}

inline void recreatestr(char* &s, size_t length)
{
   s = (char*)realloc(s, length);
}

inline void recreatestr(wchar_t* &s, size_t length)
{
   s = (wchar_t*)realloc(s, length * sizeof(wchar_t));
}

inline size_t getlength(const wchar_t* s)
{
   return (s==NULL) ? 0 : wcslen(s);
}

inline size_t getlength(const char* s)
{
   return (s==NULL) ? 0 : strlen(s);
}

inline void doubleToStr(double value, int digit, char* s)
{
   _gcvt(value, digit, s);
}

inline void doubleToStr(double value, int digit, wchar_t* s)
{
   char tmp[20];

   _gcvt(value, digit, tmp);

   ansiToUnicode(tmp, s, strlen(tmp));
   s[strlen(tmp)] = 0;
}

inline void insertstr(TCHAR* s, int pos, const TCHAR* subs)
{
   size_t len = _tcslen(subs);
   //s[getlength(s) + len + 1] = 0;
   for (int i = _tcslen(s) ; i >= pos ; i--) {
      s[i+len] = s[i];
   }
   _tcsncpy(s + pos, subs, len);
}

inline void movestr(char* s1, const char* s2, size_t length)
{
   memmove(s1, s2, length);
}

inline void movestr(wchar_t* s1, const wchar_t* s2, size_t length)
{
   memmove(s1, s2, length * sizeof(wchar_t));
}

inline TCHAR _tchlwr(TCHAR ch)
{
   TCHAR tmp[2];
   tmp[0] = ch;
   tmp[1] = 0;

   _tcslwr(tmp);
   return tmp[0];
}

// --- alignment routines ---

inline unsigned int align(unsigned int number, const unsigned int alignment)
{
   if (number & (alignment - 1)) {
      return (number & ~(alignment - 1)) + alignment;
   }
   else return number & ~(alignment - 1);
}

// --- mapping keys ---

// FNV-1a over the whole string.
//
// This replaces two hashes that each keyed on a SINGLE character -- the first
// letter of the last namespace component for references, the first letter for
// literals -- giving 27 possible values. Measured over the 393 class and symbol
// names in the standard library, that filled 21 of 27 buckets with the largest
// holding 62 entries: a 4.3x imbalance.
//
// Changing this does NOT change the module file format. Maps are serialized as
// (key, value) pairs and rehashed on load, precisely so the hash stays a free
// implementation choice.
inline size_t hashString(const TCHAR* s)
{
   unsigned int hash = 2166136261u;                 // FNV offset basis

   if (s) for (const TCHAR* p = s ; *p ; p++) {
      // Mask to the code unit width first: TCHAR is signed in both character
      // models, so anything above 127 would otherwise sign-extend.
      unsigned int unit =
         (unsigned int)((unsigned long long)(*p) & ((1ULL << (8 * sizeof(TCHAR))) - 1));

      for (size_t i = 0 ; i < sizeof(TCHAR) ; i++) {
         hash ^= (unit >> (i * 8)) & 0xFF;
         hash *= 16777619u;                         // FNV prime
      }
   }

   return (size_t)hash;
}

inline size_t mapReferenceKey(const TCHAR* key)
{
   return hashString(key);
}

inline size_t mapLiteralKey(const TCHAR* key)
{
   return hashString(key);
}

} // _ELENA_

#endif // toolsH
