//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Engine
//               
//		This file contains the implementation of ELENA Engine Data Section 
//		classes.
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#include "elena.h"
//---------------------------------------------------------------------------
#include "section.h"

using namespace _ELENA_;

// --- Section ---

void Section :: insert(size_t position, const char* s, size_t length)
{
   MemoryDump::insert(position, s, length);

   ::shift(_references.start(), position, length);
}

void Section :: fixupReferences(RelocationFixMap& fixupTable, int base, bool relative)
{
   RelocationMap::Iterator it = _references.start();
   while (!it.Eof()) {
      if (fixupTable.exist(it.key())) {
         if (relative && test(it.key(), mskRelativeRef)) {
            *((int*)(_buffer + *it)) = fixupTable.get(it.key()) - (base + *it + 4);
         }
         if (!relative && !test(it.key(), mskRelativeRef)) {
            *((int*)(_buffer + *it)) += (base + fixupTable.get(it.key()));
         }
      }
      it++;
   }
}

void Section :: fixupReferences(int base, size_t mask, ref_t(realloc)(ref_t))
{
   RelocationMap::Iterator it = _references.start();
   while (!it.Eof()) {
      ref_t key = it.key();
      size_t pos = *it;

      if ((key & mskSectionMask) == mask) {
         ref_t address = realloc(key);
         if (test(key, mskRelativeRef)) {
            *((int*)(_buffer + pos)) = address - pos - 4;
         }
         else *((int*)(_buffer + pos)) += (base + address);
      }

      it++;
   }
}

// --- SectionWriter ---

SectionWriter :: SectionWriter(Section* section)
   : DumpWriter(section)
{
}

bool SectionWriter :: writeRef(ref_t reference, size_t value)
{
   ((Section*)_dump)->addReference(reference, _position);
   writeDWord(value);

   return true;
}
