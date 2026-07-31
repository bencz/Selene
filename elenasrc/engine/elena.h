//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Compiler Engine
//
//		This file contains the common ELENA Compiler Engine templates,
//		classes, structures, functions and constants
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef elenaH
#define elenaH 1

#include "common.h"
#include "elenaconst.h"
#include "section.h"

namespace _ELENA_
{

// --- _Module interface ---

class _Module
{
public:
   virtual const TCHAR* Name() const = 0;

   virtual const TCHAR* resolveReference(ref_t reference) = 0;
   virtual const TCHAR* resolveMessage(ref_t reference) = 0;
   virtual const TCHAR* resolveConstant(ref_t reference) = 0;

   virtual ref_t mapReference(const TCHAR* reference) = 0;
   virtual ref_t mapReference(const TCHAR* reference, bool existing) = 0;

   virtual ref_t mapMessage(const TCHAR* reference) = 0;
   virtual ref_t mapConstant(const TCHAR* reference) = 0;

   virtual void mapPredefinedReference(const TCHAR* name, ref_t reference) = 0;

   virtual Section* mapSection(ref_t reference, bool existing) = 0;

   virtual bool save(StreamWriter& writer) = 0;

   virtual ~_Module() {}
};

// --- SectionInfo ---

struct SectionInfo
{
   _Module* module;
   Section* section;

   SectionInfo()
   {
      module = NULL;
      section = NULL;
   }
};

// --- ClassSectionInfo ---

struct ClassSectionInfo
{
   _Module* module;
   Section* codeSection;
   Section* vmtSection;

   ClassSectionInfo()
   {
      module = NULL;
      codeSection = vmtSection = NULL;
   }
};

// --- VMTEntry ---

struct VMTEntry
{
   int messageID;
   int address;
};

// --- ClassHeader ---

struct ClassHeader
{
   ref_t      roleRef;
   size_t     flags;
   ref_t      parentRef;
};

// --- ClassInfo ---

struct ClassInfo
{
   typedef MemoryMap<ref_t, bool, false> MethodMap; // true means overridden / newly implemented; false means inherited
   typedef MemoryMap<const TCHAR*, int, true> FieldMap;

   ClassHeader header;
   size_t      classSize; // VMT size
   size_t      size;      // Object size
   MethodMap   methods;
   FieldMap    fields;
   FieldMap    roles;

   void save(StreamWriter* writer)
   {
      writer->write((void*)this, sizeof(ClassHeader));
      writer->writeDWord(classSize);
      writer->writeDWord(size);
      methods.write(writer);
      fields.write(writer);
      roles.write(writer);
   }

   void load(StreamReader* reader, bool headerOnly)
   {
      reader->read((void*)&header, sizeof(ClassHeader));
      classSize = reader->getDWord();
      size = reader->getDWord();
      methods.read(reader);
      fields.read(reader);
      roles.read(reader);
   }

   ClassInfo()
      : fields(-1)
   {
   }
};

// --- DebugLineInfo ---

struct DebugLineInfo             
{
   DebugSymbol symbol;
   int         col, row, length;
   union
   {
      struct Module { int nameRef; int flags; } symbol;
      struct Step   { size_t address;         } step;
      struct Local  { int nameRef; int level; } local;
   } addresses;
   

   DebugLineInfo()
   {
      symbol = dsNone;
   }
   DebugLineInfo(DebugSymbol symbol, int length, int col, int row)
   {
      this->symbol = symbol;
      this->col = col;
      this->row = row;
      this->length = length;

      this->addresses.symbol.nameRef = 0;
      this->addresses.symbol.flags = 0;
   }
};

// --- Exception base class ---

struct _Exception
{
};

// --- InternalError ---

struct InternalError : _Exception
{
   const TCHAR* message;

   InternalError(const TCHAR* message)
   {
      this->message = message;
   }
};

// --- key mapping routines ---

inline size_t syntaxRule(size_t key)
{
   return key >> cnSyntaxPower;
}

inline size_t tableRule(size_t key)
{
   return key >> cnTableKeyPower;
}

// --- Common type definitions ---

typedef Map<const TCHAR*, _Module*> ModuleMap;

typedef ReferenceNameTemplate<String>  ReferenceName;
typedef NamespaceTemplate<String>      Namespace;
typedef IdentifierTemplate<String>     Identifier;

typedef LocalString<IDENTIFIER_LEN>                  LocalReferenceString;
typedef ReferenceNameTemplate<LocalReferenceString>  LocalReferenceName;
typedef NamespaceTemplate<LocalReferenceString>      LocalNamespace;
typedef IdentifierTemplate<LocalReferenceString>     LocalIdentifier;
typedef PrivateMessageTemplate<LocalReferenceString> LocalPrivateMessage;

// --- Reference mapping types ---
typedef MemoryHashTable<const TCHAR*, ref_t, mapReferenceKey, 29> ReferenceMap;
typedef MemoryHashTable<const TCHAR*, ref_t, mapLiteralKey, 29>   ConstantMap;

// --- Message mapping types ---
typedef Map<const TCHAR*, ref_t, false> MessageMap;

// --- ParserTable auxiliary types ---
typedef Stack<int>                                           ParserStack;
typedef MemoryMap<const TCHAR*, int>                      SymbolMap;
typedef MemoryHashTable<size_t, int, syntaxRule, cnHashSize> SyntaxHash;
typedef MemoryHashTable<size_t, int, tableRule, cnHashSize>  TableHash;

// --- miscellaneous routines ---

inline bool isWeakReference(const TCHAR* referenceName)
{
   return (referenceName != NULL && referenceName[0] != 0 && referenceName[0]=='\'');
}

#define VA_ALIGNMENT       0x08
#define VA_ALIGNMENT_POWER 0x03

inline ref_t reallocateReference(ref_t vaddress)
{
   return vaddress << VA_ALIGNMENT_POWER;
}

inline ref_t returnReference(ref_t vaddress)
{
   return vaddress & ~mskImageMask;
}

} // _ELENA_

#endif // elenaH
