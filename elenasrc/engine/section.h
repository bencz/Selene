//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Engine
//               
//		This header contains the declaration of ELENA Engine Data Section 
//		classes.
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef SectionH
#define SectionH 1

namespace _ELENA_
{

// --- Section Fixup Hash function ---

inline size_t indexReference(size_t reference)
{
   // `&&` where `&` was meant: the expression evaluated to 0 or 1, and >> 2
   // then made it 0 unconditionally, so every fixup landed in bucket 0 and the
   // hash table degenerated into a linear list. Clang warns about this; MSVC
   // never did.
   return ((reference & ~mskAnyRef) >> 2);
}

// --- Section Fixup map ---
typedef MemoryHashTable<ref_t, int, indexReference, cnHashSize> RelocationFixMap;

// --- Section mapping types ---
class Section;
typedef Map<ref_t, Section*> SectionMap;

// --- Section class ---
typedef MemoryMap<ref_t, ref_t> RelocationMap;

class Section : public MemoryDump
{
   RelocationMap _references;

public:
   RelocationMap::Iterator References() { return _references.start(); }

   virtual void insert(size_t position, const char* s, size_t length);
    
   void addReference(ref_t reference, size_t position)
   {
      _references.add(reference, position);
   }

   void fixupReferences(RelocationFixMap& fixupTable, int base, bool relative);
   void fixupReferences(int base, size_t mask, ref_t(realloc)(ref_t));

   friend void _readToMap(StreamReader* reader, SectionMap* map, size_t counter, ref_t& key, Section* section) 
   {
      while (counter > 0) {
         section = new Section();
         unsigned int rawKey = 0, rawLength = 0;
         reader->readU32LE(rawKey);
         reader->readU32LE(rawLength);
         key = (ref_t)rawKey;
         size_t length = (size_t)rawLength;

         DumpWriter writer(section);
         writer.read(reader, length);
         section->_references.read(reader);

         map->add(key, section);
         counter--;
      }
   }
   
   friend void _writeIterator(StreamWriter* writer, int key, Section* section)
   {
      writer->writeU32LE((unsigned int)key);
      writer->writeU32LE((unsigned int)section->Length());

      DumpReader reader(section);
      writer->read(&reader, section->Length());

      section->_references.write(writer);
   }   
};

// --- SectionWriter class ---
      
class SectionWriter : public DumpWriter
{
public:
   Section* getSection() { return (Section*)_dump; }

   virtual bool writeRef(ref_t reference, size_t value);

   SectionWriter(Section* section);
};

} // _ELENA_

#endif	// SectionH
