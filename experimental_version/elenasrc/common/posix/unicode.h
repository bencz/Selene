//---------------------------------------------------------------------------
//      E L E N A   P r o j e c t:  POSIX compatibility layer
//
//      POSIX counterpart of common/win32/unicode.h
//
//      The Win32 original uses MultiByteToWideChar/WideCharToMultiByte with
//      CP_ACP, so its behaviour depends on the machine's ANSI code page --
//      compiler output was locale-dependent. This version uses UTF-8
//      unconditionally, which is both correct and deterministic.
//
//      For pure ASCII input the conversion is 1:1, which is what every
//      current caller relies on.
//---------------------------------------------------------------------------

#ifndef posix_unicodeHdr
#define posix_unicodeHdr 1

#include <stddef.h>

// UTF-8 -> wchar_t. Consumes up to `length` input bytes.
inline void ansiToUnicode(const char* sour, wchar_t* dest, size_t length)
{
   const unsigned char* s = (const unsigned char*)sour;
   size_t i = 0, out = 0;

   while (i < length) {
      unsigned int c = s[i];

      if (c < 0x80) {
         dest[out++] = (wchar_t)c;
         i += 1;
      }
      else if ((c & 0xE0) == 0xC0 && i + 1 < length) {
         dest[out++] = (wchar_t)(((c & 0x1F) << 6) | (s[i + 1] & 0x3F));
         i += 2;
      }
      else if ((c & 0xF0) == 0xE0 && i + 2 < length) {
         dest[out++] = (wchar_t)(((c & 0x0F) << 12) | ((s[i + 1] & 0x3F) << 6)
                                 | (s[i + 2] & 0x3F));
         i += 3;
      }
      else if ((c & 0xF8) == 0xF0 && i + 3 < length) {
         dest[out++] = (wchar_t)(((c & 0x07) << 18) | ((s[i + 1] & 0x3F) << 12)
                                 | ((s[i + 2] & 0x3F) << 6) | (s[i + 3] & 0x3F));
         i += 4;
      }
      else {
         // invalid sequence - substitute, matching the Win32 '?' behaviour
         dest[out++] = (wchar_t)'?';
         i += 1;
      }
   }
}

// wchar_t -> UTF-8. Consumes `length` wide characters.
// Returns true when nothing was lost (the Win32 version returns !usedDefaultChar).
inline bool unicodeToAnsi(const wchar_t* sour, char* dest, size_t length)
{
   size_t out = 0;
   bool lossless = true;

   for (size_t i = 0 ; i < length ; i++) {
      unsigned int c = (unsigned int)sour[i];

      if (c < 0x80) {
         dest[out++] = (char)c;
      }
      else if (c < 0x800) {
         dest[out++] = (char)(0xC0 | (c >> 6));
         dest[out++] = (char)(0x80 | (c & 0x3F));
      }
      else if (c < 0x10000) {
         dest[out++] = (char)(0xE0 | (c >> 12));
         dest[out++] = (char)(0x80 | ((c >> 6) & 0x3F));
         dest[out++] = (char)(0x80 | (c & 0x3F));
      }
      else if (c <= 0x10FFFF) {
         dest[out++] = (char)(0xF0 | (c >> 18));
         dest[out++] = (char)(0x80 | ((c >> 12) & 0x3F));
         dest[out++] = (char)(0x80 | ((c >> 6) & 0x3F));
         dest[out++] = (char)(0x80 | (c & 0x3F));
      }
      else {
         dest[out++] = '?';
         lossless = false;
      }
   }

   return lossless;
}

#endif // posix_unicodeHdr
