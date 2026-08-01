//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Common Library
//
//		This file contains the implementation of ELENA Engine Data Section
//		classes.
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#include "common.h"
//---------------------------------------------------------------------------
#include "dump.h"

using namespace _ELENA_;

// --- MemoryDump ---

MemoryDump :: MemoryDump()
{
   _used = 0;
   _total = SECTION_PAGE_SIZE;

   _buffer = (char*)malloc(SECTION_PAGE_SIZE);
}

MemoryDump :: MemoryDump(size_t capacity)
{
   _used = 0;
   _total = capacity;

   _buffer = (capacity > 0) ? (char*)malloc(capacity) : NULL;
}

void MemoryDump :: reserve(size_t size)
{
   if (size > _total) {
      _total = align(size, SECTION_PAGE_SIZE);

      _buffer = (char*)realloc(_buffer, _total);
   }
}

void MemoryDump :: allocate(size_t size)
{
   resize(_used + size);
}

void MemoryDump :: resize(size_t size)
{
   if (size > _used) {
      _used = size;

      reserve(size);
   }
}

bool MemoryDump :: write(size_t position, const void* s, size_t length)
{
   if (position <= _used) {
      resize(position + length);

      memcpy(_buffer + position, s, length);

      return true;
   }
   else return false;
}

bool MemoryDump :: writeBytes(size_t position, char value, size_t length)
{
   if (position <= _used && length > 0) {
      resize(position + length);

      memset(_buffer + position, value, length);

      return true;
   }
   else return false;
}

void MemoryDump :: insert(size_t position, const char* s, size_t length)
{
   if (position <= _used) {
      resize(_used + length);

      memmove(_buffer + position + length, _buffer + position, _used - position - length);
      memcpy(_buffer + position, s, length);
   }
}

bool MemoryDump :: read(size_t position, void* s, size_t length)
{
   if (position < _used && _used >= position + length) {
      memcpy(s, _buffer + position, length);

      return true;
   }
   else return false;
}

void* MemoryDump :: get(size_t position) const
{
   if (position < _used) {
      return _buffer + position;
   }
   else return NULL;
}

// --- DumpWriter ---

DumpWriter :: DumpWriter(MemoryDump* dump)
{
   _dump = dump;
   _position = dump ? dump->Length() : 0;
}

DumpWriter :: DumpWriter(MemoryDump* dump, size_t position)
{
   _dump = dump;
   _position = position;
}

bool DumpWriter :: seek(size_t position)
{
   if (position <= _dump->Length()) {
      _position = position;

      return true;
   }
   else return false;
}

bool DumpWriter :: write(const void* s, size_t length)
{
   if (_dump->write(_position, s, length)) {
      _position += length;

      return true;
   }
   else return false;
}

void DumpWriter :: align(size_t alignment, unsigned char c)
{
   size_t aligned = ::align(_position, alignment);

   writeBytes(c, aligned - _position);
}

bool DumpWriter :: writeBytes(unsigned char ch, size_t count)
{
   if (_dump->writeBytes(_position, ch, count)) {
      _position += count;

      return true;
   }
   else return false;
}

// --- DumpReader ---

bool DumpReader :: seek(size_t position)
{
   if (position <= _dump->Length()) {
      _position = position;

      return true;
   }
   else return false;
}

bool DumpReader :: read(void* s, size_t length)
{
   if (_dump->read(_position, s, length)) {
      _position += length;

      return true;
   }
   else return false;
}

const TCHAR* DumpReader :: getLiteral()
{
   const TCHAR* s = (const TCHAR*)_dump->get(_position);

#ifdef _UNICODE
   _position += ((getlength(s) + 1) * sizeof(TCHAR));
#else
   _position += getlength(s) + 1;
#endif

   return s;
}
