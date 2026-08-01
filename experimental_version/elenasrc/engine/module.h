//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Engine
//
//		This header contains the declaration of the class implementing
//      ELENA Engine Module class
//                                              (C)2005-2008, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef moduleH
#define moduleH 1

namespace _ELENA_
{

// --- Module class ---

class Module : public _Module
{
   typedef Cache<ref_t, const TCHAR*, 20> ResolveMap;

   String       _name;

   ReferenceMap _references;
   ReferenceMap _messages;
   ReferenceMap _constants;

   SectionMap   _sections;

   ResolveMap   _resolvedReferences;
   ResolveMap   _resolvedMessages;

public:
   virtual const TCHAR* Name() const { return _name; }

   virtual const TCHAR* resolveReference(ref_t reference);
   virtual const TCHAR* resolveMessage(ref_t reference);
   virtual const TCHAR* resolveConstant(ref_t reference);

   virtual void mapPredefinedReference(const TCHAR* name, ref_t reference);

   virtual ref_t mapReference(const TCHAR* reference);
   virtual ref_t mapReference(const TCHAR* reference, bool existing);

   virtual ref_t mapMessage(const TCHAR* message);
   virtual ref_t mapConstant(const TCHAR* constant);

   virtual Section* mapSection(ref_t reference, bool existing);

   virtual LoadResult load(StreamReader& reader);
   virtual bool save(StreamWriter& writer);

   Module();
   Module(const TCHAR* name);
};

} // _ELENA_

#endif // moduleH
