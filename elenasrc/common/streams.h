//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Common Library
//
//      This header contains the declaration of abstract stream reader
//      and writer classes
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef streamsH
#define streamsH 1

namespace _ELENA_
{

// --- Constant definition ---
#define BLOCK_SIZE                  0x0200            // the temporal exchange buffer size

// --- StreamReader interface ---

class StreamReader
{
public:
   virtual bool Eof() = 0;
   virtual size_t Position() = 0;

   virtual bool seek(size_t position) = 0;

   virtual bool read(void* s, size_t length) = 0;

   virtual const TCHAR* getLiteral() = 0;

   int getDWord()
   {
      int value = 0;
      readDWord(value);

      return value;
   }

   unsigned char getByte()
   {
      unsigned char value = 0;
      read(&value, 1);

      return value;
   }

   bool readDWord(int& dword)
   {
      return read((void*)&dword, 4);
   }

   bool readDWord(size_t& dword)
   {
      return read((void*)&dword, 4);
   }

   bool readByte(char& ch)
   {
      return read((void*)&ch, 1);
   }

   bool readChar(TCHAR& ch)
   {
      return read(&ch, sizeof(TCHAR));
   }

   bool readLiteral(wchar_t* s, size_t length)
   {
      return read((void*)s, length * sizeof(wchar_t));
   }

   bool readLiteral(char* s, size_t length)
   {
      return read((void*)s, length);
   }

   bool readString(_String& s, size_t length)
   {
      s.clear();

      TCHAR buffer[BLOCK_SIZE];
      size_t size = BLOCK_SIZE;
      while (length > 0) {
         if (length < size) {
            size = length;
         }
         if (!readLiteral(buffer, size))
            return false;

         s.append(buffer, size);

         length -= size;
      }
      return true;
   }

   bool readString(_String& s)
   {
      s.clear();

      TCHAR ch = 0;
      while (readChar(ch)) {
         if (ch != 0) {
            s.append(ch);
         }
         else return true;
      }
      return false;
   }

   virtual ~StreamReader() {}
};

// --- StreamWriter interface ---

class StreamWriter
{
public:
   virtual bool isOpened() = 0;

   virtual size_t Position() const = 0;

   virtual bool write(const void* s, size_t length) = 0;

   bool writeLiteral(const TCHAR* s)
   {
      return writeLiteral(s, getlength(s) + 1);
   }

   bool writeLiteral(const char* s, size_t length)
   {
      return write((void*)s, length);
   }

   bool writeLiteral(const wchar_t* s, size_t length)
   {
      return write((void*)s, length * sizeof(wchar_t));
   }

   bool writeChar(TCHAR ch)
   {
      return writeLiteral(&ch, 1);
   }

   void writeDWord(int dword)
   {
      write(&dword, 4);
   }

   void writeWord(unsigned short word)
   {
      write(&word, 2);
   }

   bool writeByte(unsigned char ch)
   {
      return write((void*)&ch, 1);
   }

   virtual bool writeBytes(unsigned char ch, size_t count)
   {
	  while (count > 0) {
         if (!write((char*)&ch, 1))
            return false;
		 count--;
	  }
	  return true;
   }

   virtual bool writeChars(TCHAR ch, size_t count)
   {
      while (count > 0) {
         if (!writeLiteral(&ch, 1))
            return false;
         count--;
      }
      return true;
   }

   void writeAsciiLiteral(const char* s, size_t length)
   {
      write((void*)s, length);
   }

   void writeAsciiLiteral(const wchar_t* s, size_t length)
   {
      for (size_t i = 0 ; i < length ; i++) {
         write((void*)&s[i], 1);
      }
   }

   void writeAsciiLiteral(const TCHAR* s)
   {
      writeAsciiLiteral(s, getlength(s) + 1);
   }

   void read(StreamReader* reader, size_t length)
   {
      char    buffer[BLOCK_SIZE];
      size_t  blockLen = BLOCK_SIZE;
      while (length > 0) {
         if (blockLen >= length)
            blockLen = length;

         reader->read(buffer, blockLen);
         write(buffer, blockLen);

         length -= blockLen;
      }
   }

   virtual ~StreamWriter() {}
};

// --- TextReader ---

class TextReader
{
public:
   virtual bool read(TCHAR* s, size_t length) = 0;

   bool readString(String& s)
   {
      s.clear();

      TCHAR buffer[BLOCK_SIZE];
      while (read(buffer, BLOCK_SIZE)) {
         s.append(buffer);

         if (buffer[getlength(buffer) - 1] == '\n')
            return true;
      }
      return (getlength(s) != 0);
   }
};

// --- TextReader ---

class TextWriter
{
public:
   virtual bool writeLine(const TCHAR* s) = 0;
   virtual bool write(const TCHAR* s, size_t length) = 0;

   virtual void writeChar (TCHAR ch)
   {
      write(&ch, 1);
   }

   virtual void writeStr (const TCHAR* s)
   {
      write(s, getlength(s));
   }
};

// --- LiteralWriter ---

class LiteralWriter : public StreamWriter
{
   TCHAR* _text;
   size_t _offset;
   size_t _size;

public:
   virtual bool isOpened() { return _text != NULL; }

   virtual size_t Position() const { return _offset; }

   virtual void reset()
   {
      _offset = 0;
   }

   bool writeLiteral(const TCHAR* s)
   {
      return writeLiteral(s, getlength(s) + 1);
   }

   bool writeLiteral(const TCHAR* s, size_t length)
   {
      if (_offset + length <= _size) {
         _tcsncpy(_text + _offset, s, length);
         _offset += length;

         return true;
      }
      else return false;
   }

#ifdef _UNICODE

   bool writeByte(unsigned char ch)
   {
      return false;
   }

   virtual bool write(const void* s, size_t length) 
   {
      size_t size = length >> 1;
      if (_offset + size <= _size) {
         void* p = _text + _offset;
         memcpy(p, s, length);

         _offset += size;

         return true;
      }
      else return false;
   }

#else

   virtual bool write(void* s, size_t length) 
   {
      if (_offset + length <= _size) {
         void* p = _text + _offset;
         memcpy(p, s, length);

         _offset += length;
      }      
   }

#endif

   bool writeChar(TCHAR ch)
   {
      if (_offset < _size) {
         _text[_offset++] = ch;

         return true;
      }
      else return false;
   }

   LiteralWriter(TCHAR* text, int size)
   {
      _text = text;
	  _offset = 0;
	  _size = size;
   }
   LiteralWriter(TCHAR* text, int size, int offset)
   {
      _text = text;
	  _offset = offset;
	  _size = size;
   }

   virtual ~LiteralWriter() {}
};

// --- LiteralReader ---

class LiteralReader : public StreamReader
{
   const TCHAR* _text;
   size_t       _offset;

public:
   virtual bool Eof() { return getlength(_text) == _offset; }

   virtual size_t Position() { return _offset; }

   virtual bool seek(size_t position)
   {
      _offset = position;

      return true;
   }

   virtual const TCHAR* getLiteral()
   {
      const TCHAR* s = _text + _offset;

      _offset += getlength(s) + 1;

      return s;
   }

   bool readLiteral(TCHAR* s, size_t length)
   {
      _tcsncpy(s, (_text + _offset), length);
	  _offset += length;

      return true;
   }

#ifdef _UNICODE

   virtual bool read(void* s, size_t length)
   {
      size_t size = length >> 1;

      _tcsncpy((TCHAR*)s, (_text + _offset), size >> 1);
      _offset += size;

      return true;
   }

   bool readLiteral(char*, size_t)
   {
      return false;
   }

   int readDWord()
   {
      int dword = 0;
      void* p = (void*)(_text + _offset);
      memcpy(&dword, p, 4);
      _offset += 2;

      return dword;
   }

#else

   virtual bool read(void* s, size_t length)
   {
      _tcsncpy((TCHAR*)s, (_text + _offset), length);
      _offset += length;

      return true;
   }

#endif
   LiteralReader(const TCHAR* text)
   {
      _text = text;
	  _offset = 0;
   }
   LiteralReader(const TCHAR* text, int offset)
   {
      _text = text;
	  _offset = offset;
   }

   virtual ~LiteralReader() {}
};

  
} // _ELENA_

#endif // streamsH
