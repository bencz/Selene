//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Common Library
//
//		This file contains Config File class header
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef configH
#define configH

#include "common.h"

namespace _ELENA_
{

// --- IniConfigFile ---

class IniConfigFile : public _ConfigFile
{
   ConfigSettings _settings;

public:
   virtual ConfigCategoryIterator getCategoryIt(const TCHAR* name)
   {
      return _settings.getIt(name);
   }

   virtual const TCHAR* getSetting(const TCHAR* category, const TCHAR* key, const TCHAR* defaultValue = NULL);

   void setSetting(const TCHAR* category, const TCHAR* key, const TCHAR* value);
   void setSetting(const TCHAR* category, const TCHAR* key, int value);
   void setSetting(const TCHAR* category, const TCHAR* key, size_t value);
   void setSetting(const TCHAR* category, const TCHAR* key, bool value);

   void clear(const TCHAR* category, const TCHAR* key);
   void clear(const TCHAR* category);
   void clear();

   virtual bool load(const TCHAR* path);
   virtual bool save(const TCHAR* path, FileEncoding encoding);
   virtual bool save(const TCHAR* path)
   {
      return save(path, feAnsi);
   }

   IniConfigFile();
   IniConfigFile(bool allowDuplicates);
   ~IniConfigFile() { }
};

} // _ELENA_

#endif // configH
