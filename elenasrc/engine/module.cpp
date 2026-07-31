//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Engine
//
//		This file contains the class implementing ELENA Engine Module class
//
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#include "elena.h"
// --------------------------------------------------------------------------
#include "module.h"
#include "errors.h"

using namespace _ELENA_;

// --- Module ---

Module :: Module()
   : _references(0), _messages(0), _constants(0), _sections(NULL, freeobj)
{
}

Module :: Module(const TCHAR* name)
   : _name(name), _references(0), _messages(0), _constants(0), _sections(NULL, freeobj)
{
}

const TCHAR* Module :: resolveMessage(ref_t reference)
{
   const TCHAR* key = _resolvedMessages.get(reference);
   if (!key) {
      key = retrieveKey(_messages.start(), reference, DEFAULT_STR);

      _resolvedMessages.add(reference, key);
   }
   return key;
}

const TCHAR* Module :: resolveReference(ref_t reference)
{
   const TCHAR* key = _resolvedReferences.get(reference);
   if (!key) {
      key = retrieveKey(_references.start(), reference, DEFAULT_STR);

      _resolvedReferences.add(reference, key);
   }
   return key;
}

const TCHAR* Module :: resolveConstant(ref_t reference)
{
   return retrieveKey(_constants.start(), reference, DEFAULT_STR);
}

void Module :: mapPredefinedReference(const TCHAR* name, ref_t reference)
{
   _references.add(name, reference);
}

ref_t Module :: mapReference(const TCHAR* reference)
{
   size_t nextId = _references.Count() + 1;

   // generate an exception if reference id is out of range
   if (nextId > ~mskAnyRef)
      throw InternalError(errReferenceOverflow);

   ref_t refId = mapKey(_references, reference, nextId);

   // if we added new reference, clear resolved reference cache (due to possible string relocation)
   if (refId == nextId)
      _resolvedReferences.clear();

   return refId;
}

ref_t Module :: mapMessage(const TCHAR* message)
{
   size_t nextId = _messages.Count() + 1;

   ref_t refId = mapKey(_messages, message, nextId);

   // if we added new message, clear resolved message cache (due to possible string relocation)
   if (refId == nextId)
      _resolvedMessages.clear();

   return refId;
}

ref_t Module :: mapConstant(const TCHAR* constant)
{
   size_t nextId = _constants.Count() + 1;

   return mapKey(_constants, constant, nextId);
}

ref_t Module :: mapReference(const TCHAR* reference, bool existing)
{
   if (existing) {
      return _references.get(reference);
   }
   else return mapReference(reference);
}

Section* Module :: mapSection(ref_t reference, bool existing)
{
   Section* section = _sections.get(reference);
   if (!existing && section==NULL) {
      section = new Section();

      _sections.add(reference, section);
   }
   return section;
}

// --- module header serialization ---
//
// Written and read one byte at a time on purpose: the header must be legible on
// a host whose byte order differs from the writer's, which is the whole point of
// carrying a byteOrder field.

inline void writeByte(StreamWriter& writer, unsigned int value)
{
   unsigned char b = (unsigned char)(value & 0xFF);

   writer.write(&b, 1);
}

inline void writeU16(StreamWriter& writer, unsigned int value)
{
   unsigned char b[2];

   b[0] = (unsigned char)(value & 0xFF);
   b[1] = (unsigned char)((value >> 8) & 0xFF);

   writer.write(b, 2);
}

inline bool readByte(StreamReader& reader, unsigned int& value)
{
   unsigned char b = 0;

   if (!reader.read(&b, 1))
      return false;

   value = b;
   return true;
}

inline bool readU16(StreamReader& reader, unsigned int& value)
{
   unsigned char b[2] = { 0, 0 };

   if (!reader.read(b, 2))
      return false;

   value = (unsigned int)b[0] | ((unsigned int)b[1] << 8);
   return true;
}

LoadResult Module :: load(StreamReader& reader)
{
   if (reader.Eof())
      return lrNotFound;

   // load magic...
   char magic[MODULE_MAGIC_SIZE];
   memset(magic, 0, sizeof(magic));
   reader.read(magic, MODULE_MAGIC_SIZE);

   if (!compstr(magic, MODULE_MAGIC, MODULE_MAGIC_SIZE)) {
      // A v1 module begins with "EN!10" and carries no header at all. Recognise
      // it so the user gets "wrong version" rather than "wrong structure".
      return compstr(magic, MODULE_SIGNATURE, 2) ? lrWrongVersion : lrWrongStructure;
   }

   unsigned int formatVersion = 0, headerSize = 0;
   unsigned int encoding = 0, byteOrder = 0, wordBits = 0, flags = 0;

   if (!readU16(reader, formatVersion) || !readU16(reader, headerSize))
      return lrWrongStructure;

   if (!readByte(reader, encoding) || !readByte(reader, byteOrder)
       || !readByte(reader, wordBits) || !readByte(reader, flags))
   {
      return lrWrongStructure;
   }

   // Only the major version has to match; a larger headerSize is skipped so a
   // future minor revision can add fields without breaking this reader.
   if ((formatVersion & 0xFF00) != (MODULE_FORMAT_VERSION & 0xFF00))
      return lrWrongVersion;

   if (headerSize < MODULE_HEADER_SIZE)
      return lrWrongStructure;

   // skip reserved bytes and anything a newer writer appended
   for (unsigned int i = MODULE_MAGIC_SIZE + 8 ; i < headerSize ; i++) {
      unsigned int ignored = 0;

      if (!readByte(reader, ignored))
         return lrWrongStructure;
   }

   // The payload is a raw image of host structures, so these three properties
   // must match exactly. Each mismatch was previously read as garbage.
   if (byteOrder != (unsigned int)getHostByteOrder())
      return lrIncompatibleByteOrder;

   if (encoding != (unsigned int)getHostEncoding())
      return lrIncompatibleEncoding;

   if (wordBits != (unsigned int)getHostWordBits())
      return lrIncompatibleWordSize;

   // load name...
   reader.readString(_name);

   // load references...
   _references.read(&reader);

   // load references...
   _messages.read(&reader);

   // load references...
   _constants.read(&reader);

   // load sections..
   _sections.read(&reader);

   return lrSuccessful;
}

bool Module :: save(StreamWriter& writer)
{
   if (!writer.isOpened())
      return false;

   // save header...
   writer.writeLiteral(MODULE_MAGIC, MODULE_MAGIC_SIZE);

   writeU16(writer, MODULE_FORMAT_VERSION);
   writeU16(writer, MODULE_HEADER_SIZE);

   writeByte(writer, getHostEncoding());
   writeByte(writer, getHostByteOrder());
   writeByte(writer, getHostWordBits());
   writeByte(writer, mhfNone);

   // reserved -- must be zero
   for (int i = MODULE_MAGIC_SIZE + 8 ; i < MODULE_HEADER_SIZE ; i++)
      writeByte(writer, 0);

   // save name...
   writer.writeLiteral(_name, getlength(_name) + 1);

   // save references...
   _references.write(&writer);

   // save messages...
   _messages.write(&writer);

   // save constants...
   _constants.write(&writer);

   // save sections..
   _sections.write(&writer);

   return true;
}
