
//		E L E N A   P r o j e c t:  ELENA Common Library
//
//		This header contains the declaration of ELENA Engine Data Memory dump
//		classes.
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef DumpH
#define DumpH 1

namespace _ELENA_
{

// --- Constant definition ---
#define SECTION_PAGE_SIZE           0x0040            // the section page size, should be aligned to power of two

// --- MemoryDump class ---

class MemoryDump
{
protected:
   char*  _buffer;
   size_t _total;
   size_t _used;

   void resize(size_t size);

public:
   void* getArray() const { return _buffer; }

   int& operator[](size_t position) const
   {
      return *(int*)(_buffer + position);
   }

   size_t Length() const { return _used; }
   size_t Size()   const { return _total; }

   void reserve(size_t size);

   void allocate(size_t size);

   bool read(size_t position, void* s, size_t length);

   bool write(size_t position, const void* s, size_t length);
   bool writeBytes(size_t position, char value, size_t length);

   bool writeDWord(size_t position, int value)
   {
      return write(position, (void*)&value, 4);
   }

   void write(size_t position, const wchar_t* s)
   {
      if (s && wcslen(s) > 0) {
         write(position, s, (wcslen(s) + 1) * sizeof(wchar_t));
      }
      else {
         wchar_t ch = 0;
         write(position, &ch, sizeof(wchar_t));
      }
   }
   void write(size_t position, const char* s)
   {
      if (s && strlen(s) > 0) {
         write(position, s, strlen(s) + 1);
      }
      else {
         char ch = 0;
         write(position, &ch, 1);
      }
   }

   virtual void insert(size_t position, const char* s, size_t length);

   void insertDWord(size_t position, int value)
   {
      insert(position, (char*)&value, 4);
   }
   void insertWord(size_t position, short value)
   {
      insert(position, (char*)&value, 2);
   }
   void insertByte(size_t position, char value)
   {
      insert(position, &value, 1);
   }

   void* get(size_t position) const;

   virtual void clear() { _used = 0; }

   MemoryDump();
   MemoryDump(size_t capacity);
   virtual ~MemoryDump() { freestr(_buffer); }
};

// --- DumpWriter class ---

class DumpWriter : public StreamWriter
{
protected:
   MemoryDump* _dump;
   size_t     _position;

public:
   virtual bool isOpened() { return (_dump != NULL); }

   virtual size_t Position() const { return _position; }

   void* getArray() const { return _dump->getArray(); }

   virtual void* Address() const { return _dump->get(_position); }

   virtual bool seek(size_t position);

   virtual void seekEOF()
   {
      _position = _dump->Length();
   }

   virtual bool write(const void* s, size_t length);

   virtual bool writeBytes(unsigned char ch, size_t count);

   void insertDWord(size_t position, int value)
   {
      _dump->insertDWord(position, value);

      if (position <= _position)
         _position += 4;
   }

   void insertWord(size_t position, short value)
   {
      _dump->insertWord(position, value);

      if (position <= _position)
         _position += 2;
   }

   void insertByte(size_t position, char value)
   {
      _dump->insertByte(position, value);

      if (position <= _position)
         _position += 1;
   }

   void align(size_t alignment, unsigned char c);

   DumpWriter(MemoryDump* dump);
   DumpWriter(MemoryDump* dump, size_t position);
};

// --- DumpReader class ---

class DumpReader : public StreamReader
{
   MemoryDump* _dump;
   size_t     _position;

public:
   virtual bool Eof() { return _position >= _dump->Length(); }

   virtual size_t Position() { return _position; }

   virtual bool seek(size_t position);

   virtual bool read(void* s, size_t length);

   virtual const TCHAR* getLiteral();

   void* getArray() const { return _dump->getArray(); }

   virtual void* Address() const { return _dump->get(_position); }

   DumpReader(MemoryDump* dump)
   {
      _dump = dump;
      _position = 0;
   }
   DumpReader(MemoryDump* dump, size_t position)
   {
      _dump = dump;
      _position = position;
   }
};

} // _ELENA_

#endif  // DumpH
