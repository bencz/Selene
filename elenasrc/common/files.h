//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Common Library
//
//		This file contains ELENA Engine File class declarations.
//
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef filesH
#define filesH 1

namespace _ELENA_
{

#define LOCAL_PATH_LENGTH 0x200

// --- Path ---

template<class String> class PathTemplate
{
   String _string;

public:
   static bool checkExtension(const TCHAR*  path, const TCHAR* extension)
   {
      int namepos = lastPathSeparatorPos(path) + 1;

      int pos = lastchrpos(path + namepos, '.');
      if (pos != -1) {
         return compstr(path + namepos + pos + 1, extension);
      }
      else return emptystr(extension);
   }

   operator const TCHAR*() const { return _string; }

   const TCHAR* asString() const { return *this; }

   bool isEmpty() const { return emptystr(*this); }

   void appendExtension(const TCHAR* extension)
   {
      _string.append('.');
      _string.append(extension);
   }

   void nameToPath(const TCHAR* name, const TCHAR* extension)
   {
      while (true) {
         int pos = chrpos(name, '\'');
         if (pos != -1) {
            combine(name, pos);
            name += pos + 1;
         }
         else {
            combine(name);
            break;
         }
      }
      appendExtension(extension);
   }

   bool copyPath(const TCHAR* path)
   {
      int pos = lastPathSeparatorPos(path);
      if (pos > 0)
         return _string.copy(path, pos);
      else {
         _string.clear();

         return true;
      }
   }

   bool copy(const TCHAR* path)
   {
      return _string.copy(path);
   }

   bool combine(const TCHAR* path, size_t length)
   {
      if (length > 0) {
         size_t strLength = getlength(*this);
         if(strLength > 0 && !isPathSeparator((*this)[strLength - 1]))
            _string.append(PATH_SEPARATOR);

         return _string.append(path, length);
      }
      else return true;
   }

   bool combine(const TCHAR* path)
   {
      return combine(path, getlength(path));
   }

   void lower()
   {
      _string.lower();
   }

   void changeExtension(const TCHAR* extension)
   {
      const TCHAR* path = _string;
      int namepos = lastPathSeparatorPos(path) + 1;

      int index = lastchrpos(path + namepos, '.');
      if (index >= 0) {
         _string[(size_t)index + namepos] = 0;
      }
      _string.append('.');
      _string.append(extension);
   }

   void clear()
   {
      _string.clear();
   }

   bool exists()
   {
      struct _stat fileInfo;

      int res = _tstat(*this, &fileInfo); 

      // if we were able to get file attributes - file exists
      return (res == 0);
   }

   PathTemplate() {}
   PathTemplate(const TCHAR* path)
      : _string(path)
   {
   }
   PathTemplate(const TCHAR* path, size_t length)
      : _string(path, length)
   {
   }
   PathTemplate(const TCHAR* rootPath, const TCHAR* filePath)
      : _string(getlength(rootPath) + getlength(filePath) + 2)
   {
      _string.copy(rootPath);
      combine(filePath);
   }
   PathTemplate(const TCHAR* rootPath, const TCHAR* folderPath, const TCHAR* fileName)
   {
      _string.copy(rootPath);
      combine(folderPath);
      combine(fileName);
   }
};

// --- FileName String class ---

template<class String> class FileNameTemplate
{
   String _string;

public:
   operator const TCHAR*() const { return _string; }

   bool isEmpty() const { return emptystr(*this); }

   void clear()
   {
      _string.clear();
   }

   FileNameTemplate()
   {
   }
   FileNameTemplate(const TCHAR* path)
   {
      copyName(path);
   }

   void copyName(const TCHAR* path)
   {
      _string.clear();

      int sep = lastPathSeparatorPos(path);
      const TCHAR* p = (sep >= 0) ? (path + sep) : NULL;
      if (p) p++;
      else p = path;

      while (*p != '\0' && *p != '.')	{
         _string.append(p++, 1);
      }
   }
};

// --- Pathtemplate shortcuts ---

typedef PathTemplate<String>            Path;
typedef FileNameTemplate<String>        FileName;
typedef LocalString<LOCAL_PATH_LENGTH>  LocalPathString;
typedef PathTemplate<LocalPathString>   LocalPath;

// --- File class ---

enum FileEncoding { feAutodetect = 0, feRaw = 1, feAnsi = 2, feUTF8 = 3, feUTF16 = 4};

class File
{
   FILE*    _file;
   FileEncoding _encoding;

public:
   FileEncoding getEncoding() const { return _encoding; }

   bool Eof();
   bool isOpened() const { return (_file != NULL); }

   long Position() const;
   long Length();

   bool seek(long position);

   bool write(const void* s, size_t length);
   bool read(void* s, size_t length);
   
   bool writeLiteral(const char* s, size_t length);
   bool writeLiteral(const wchar_t* s, size_t length);

   bool readLiteral(char* s, size_t length);
   bool readLiteral(wchar_t* s, size_t length);

   bool readLiteral(char* s, size_t length, size_t& wasread);
   bool readLiteral(wchar_t* s, size_t length, size_t& wasread);

   bool readLine(char* s, size_t length);   
   bool readLine(wchar_t* s, size_t length);

   bool writeNewLine();

   void rewind();

   File(const TCHAR* path, const TCHAR* mode, FileEncoding encoding);
   ~File();
};

// --- FileReader class ---

class FileReader : public StreamReader
{
   File _file;

public:
   virtual bool Eof() { return _file.Eof(); }

   FileEncoding getEncoding() const { return _file.getEncoding(); }

   bool isOpened() const { return _file.isOpened(); }

   virtual size_t Position() { return _file.Position(); }
   virtual size_t Length() { return _file.Length(); }         

   virtual bool seek(size_t position) { return _file.seek(position); }

   virtual bool read(void* s, size_t length);

   bool readLiteral(wchar_t* s, size_t length, size_t& wasread)
   {
      return _file.readLiteral(s, length, wasread);
   }

   bool readLiteral(char* s, size_t length, size_t& wasread)
   {
      return _file.readLiteral(s, length, wasread);
   }

   virtual const TCHAR* getLiteral() { return NULL; }

   FileReader(const TCHAR* path, FileEncoding encoding);
   FileReader(const TCHAR* path, const TCHAR* mode, FileEncoding encoding);
};

// --- FileWriter class ---

class FileWriter : public StreamWriter
{
   File _file;

public:
   FileEncoding getEncoding() const { return _file.getEncoding(); }

   virtual size_t Position() const { return _file.Position(); }
   virtual size_t Length()   { return _file.Length(); }

   virtual bool isOpened() { return _file.isOpened(); }

   virtual bool write(const void* s, size_t length);

   bool writeLiteral(const TCHAR* s)
   {
      return writeLiteral(s, getlength(s) + 1);
   }

   bool writeLiteral(const TCHAR* s, size_t length)
   {
      return _file.writeLiteral(s, length);
   }

   bool writeChar(TCHAR ch)
   {
      return writeLiteral(&ch, 1);
   }

   virtual void align(int alignment);

   FileWriter(const TCHAR* path, FileEncoding encoding);
};

// --- TextFileReader class ---

class TextFileReader : public TextReader
{
   File _file;

public:
   FileEncoding getEncoding() const { return _file.getEncoding(); }

   bool isOpened() const { return _file.isOpened(); }

   virtual bool read(TCHAR* s, size_t length);

   TextFileReader(const TCHAR* path, FileEncoding encoding);
};

// --- TextFileWriter class ---

class TextFileWriter : public TextWriter
{
   File _file;

public:
   FileEncoding getEncoding() const { return _file.getEncoding(); }

   bool isOpened() const { return _file.isOpened(); }

   virtual bool writeLine(const TCHAR* s);
   virtual bool write(const TCHAR* s, size_t length);

   TextFileWriter(const TCHAR* path, FileEncoding encoding);
};

bool createPath(const TCHAR* root, const TCHAR* path);

} // _ELENA_

#endif // filesH
