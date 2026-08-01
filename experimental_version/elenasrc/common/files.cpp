//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Common Library
//
//		This file contains ELENA Engine File class implementations.
//
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#include "common.h"
// -------------------------------------------------------
#include "files.h"

#include <direct.h>

using namespace _ELENA_;

// --- File ---

File :: File(const TCHAR* path, const TCHAR* mode, FileEncoding encoding)
{
   _file = _tfopen(path, mode);
   if (!isOpened())
      return;

   if (encoding==feAutodetect) {
      unsigned short signature = 0;
      fread(&signature, 1, 2, _file);
      if (signature==0xFEFF) {
         _encoding = feUTF16;
      }
      else {
         _encoding = feAnsi;
         rewind();
      }
   }
   else _encoding = encoding;
}

File :: ~File()
{
   if (_file != NULL) {
      fclose(_file);
      _file = NULL;
   }
}

long File :: Position() const
{
   return ftell(_file);
}

long File :: Length()
{
   long position = ftell(_file);
   fseek(_file, 0, SEEK_END);

   long length = ftell(_file);
   fseek(_file, position, SEEK_SET);

   return length;
}

bool File :: Eof()
{
   if (_file) {
      int pos = ftell(_file);
      fgetc(_file);
      bool eof = (feof(_file)!=0);
      fseek(_file, pos, SEEK_SET);

      return eof;
   }
   else return true;
}

bool File :: seek(long position)
{
   return fseek(_file, position, SEEK_SET) == 0;
}

bool File :: read(void* s, size_t length)
{
   return (fread(s, 1, length, _file) > 0);
}

bool File :: readLiteral(char* s, size_t length)
{
   size_t wasread = 0;
   return readLiteral(s, length, wasread);
}

bool File :: readLiteral(wchar_t* s, size_t length)
{
   size_t wasread = 0;
   return readLiteral(s, length, wasread);
}

bool File :: readLiteral(char* s, size_t length, size_t& wasread)
{
   if (_encoding==feAnsi || _encoding==feRaw) {
      wasread = fread(s, 1, length, _file);

      return (wasread != 0);
   }
   else if (_encoding==feUTF16) {
      wasread = 0;
      wchar_t temp[0x100];
      int count;
      while (length > 0) {
         count = (length > 0x100) ? 0x100 : length;
         int wr = fread((char*)temp, 2, count, _file);
         if (wr <= 0)
            return false;

         unicodeToAnsi(temp, s, count);

         length -= count;
         s += count;
         wasread += wr;
      }
      return true;
   }
   else return false;
}

bool File :: readLiteral(wchar_t* s, size_t length, size_t& wasread)
{
   if (_encoding==feUTF16 || _encoding==feRaw) {
      wasread = fread((char*)s, 2, length, _file);
      return (wasread > 0);
   }
   else {
      char* buf = (char*)s;

      readLiteral(buf, length, wasread);
      if (wasread <= 0)
         return false;

      // Widen the bytes just read, in place, from the top down so the source
      // bytes are never overwritten before they are read.
      //
      // This was previously written as byte arithmetic assuming a 2-byte
      // wchar_t (buf[i << 1] = buf[i]; buf[(i << 1) + 1] = 0), which produced
      // garbage on any platform with a 4-byte wchar_t and would also have been
      // wrong on a big-endian target. Assigning through wchar_t* is correct for
      // every width and byte order.
      const unsigned char* src = (const unsigned char*)buf;
      for (int i = (int)length - 1 ; i >= 0 ; i--) {
         s[i] = (wchar_t)src[i];
      }
      return true;
   }
}

bool File :: readLine(char* s, size_t length)
{
   if (_encoding==feAnsi || _encoding==feRaw) {
      return (fgets(s, length, _file) != NULL);
   }
   else if (_encoding==feUTF16) {
      wchar_t temp[0x100];
      size_t count;
      while (length > 0) {
         count = (length > 0x100) ? 0x100 : length;

         if (fgetws(temp, count, _file) == NULL)
            return false;

         unicodeToAnsi(temp, s, wcslen(temp));
         s[count] = 0;

         if (wcslen(temp) < count)
            break;

         length -= count;
         s += count;
      }
      return true;
   }
   else return false;
}

bool File :: readLine(wchar_t* s, size_t length)
{
   if (_encoding==feUTF16 || _encoding==feRaw) {
      return (fgetws(s, length, _file) != NULL);
   }
   else {
      char* buf = (char*)s;

      if (!readLine(buf, length))
         return false;

      // See the note in readLiteral(wchar_t*) above: widen in place, top down,
      // through wchar_t* so the code is independent of character width and of
      // byte order.
      const unsigned char* src = (const unsigned char*)buf;
      for (int i = (int)length - 1 ; i >= 0 ; i--) {
         s[i] = (wchar_t)src[i];
      }
      return true;
   }
}

bool File :: write(const void* s, size_t length)
{
   return (fwrite((const char*)s, 1, length, _file) > 0);
}

bool File :: writeLiteral(const char* s, size_t length)
{
   if (_encoding==feAnsi || _encoding==feRaw) {
      return (fwrite(s, 1, length, _file) > 0);
   }
   else {
      wchar_t temp[0x100];
      int count;
      while (length > 0) {
         count = (length > 0x100) ? 0x100 : length;
         ansiToUnicode(s, temp, count);         
         if (fwrite((const char*)s, 2, length, _file) <= 0)
            return false;

         length -= count;
         s += count;
      }
      return true;
   }
}

bool File :: writeLiteral(const wchar_t* s, size_t length)
{
   if (_encoding==feUTF16 || _encoding==feRaw) {
      return (fwrite((const char*)s, 2, length, _file) > 0);
   }
   else {
      char temp[0x100];
      int count;
      while (length > 0) {
         count = (length > 0x100) ? 0x100 : length;
         unicodeToAnsi(s, temp, count);
         if (fwrite(temp, 1, count, _file) <= 0)
            return false;

         length -= count;
         s += count;
      }
      return true;
   }
}

bool File :: writeNewLine()
{
   return writeLiteral(_T("\r\n"), 2);
}

void File :: rewind()
{
   ::rewind(_file);
}

// --- TextFileReader ---

TextFileReader :: TextFileReader(const TCHAR* path, FileEncoding encoding)
   : _file(path, _T("rb"), encoding)
{
}

bool TextFileReader :: read(TCHAR* s, size_t length)
{
   return _file.readLine(s, length);
}

// --- FileReader ---

FileReader :: FileReader(const TCHAR* path, FileEncoding encoding)
   : _file(path, _T("rb+"), encoding)
{
}

FileReader :: FileReader(const TCHAR* path, const TCHAR* mode, FileEncoding encoding)
   : _file(path, mode, encoding)
{
}

bool FileReader :: read(void* s, size_t length)
{
   return _file.read(s, length);
}

// --- FileWriter ---

FileWriter :: FileWriter(const TCHAR* path, FileEncoding encoding)
   : _file(path, _T("wb+"), encoding)
{
   if (encoding == feUTF16 && isOpened()) {
      unsigned short signature = 0xFEFF;
      _file.write((void*)&signature, 2);
   }
}

bool FileWriter :: write(const void* s, size_t length)
{
   return _file.write(s, length);
}

// --- TextFileWriter ---

TextFileWriter :: TextFileWriter(const TCHAR* path, FileEncoding encoding)
   : _file(path, _T("wb+"), encoding)
{
   if (encoding == feUTF16 && isOpened()) {
      unsigned short signature = 0xFEFF;
      _file.write((void*)&signature, 2);
   }
}

bool TextFileWriter :: write(const TCHAR* s, size_t length)
{
   return _file.writeLiteral(s, length);
}

bool TextFileWriter :: writeLine(const TCHAR* s)
{
   if (emptystr(s) || write(s, getlength(s))) {
      _file.writeNewLine();
      return true;
   }
   else return false;
}

void FileWriter :: align(int alignment)
{
   int len = ::align(_file.Position(), alignment) - _file.Position();

   writeBytes('\0', len);
}
// --- createSubFolder ---

inline void createDir(const char* name)
{
   _mkdir(name);
}

inline void createDir(const wchar_t* name)
{
   _wmkdir(name);
}

inline int checkDir(const char* name, int mode)
{
   return _access(name, mode);
}

inline int checkDir(const wchar_t* name, int mode)
{
   return _waccess(name, mode);
}

bool _ELENA_::createPath(const TCHAR* root, const TCHAR* path)
{
   LocalPath dirPath;
   dirPath.copyPath(path);

   if (checkDir(dirPath, 0)!=0) {
	  if (!emptystr(dirPath) && !compstr(dirPath, root)) {
         _ELENA_::createPath(root, dirPath);
	  }
	  createDir(dirPath);

	  return true;
   }
   else return false;
}
