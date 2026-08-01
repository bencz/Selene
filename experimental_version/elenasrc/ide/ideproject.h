//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      ProjectInfo class header
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef ideprojectH
#define ideprojectH

#include "elena.h"

namespace _GUI_
{

class ProjectInfo
{
   bool                _changed;
   _ELENA_::FileName   _name;
   _ELENA_::Path       _path;

   _ELENA_::IniConfigFile _config;

public:
   bool isChanged() const { return _changed; }
   bool isUnnamed() const { return _name.isEmpty(); }

   const TCHAR* getName() const { return _name; }
   const TCHAR* getPath() const { return _path; }

   const TCHAR* getArguments();
   const TCHAR* getPackage();
   const TCHAR* getStartSymbol();
   const TCHAR* getTarget();
   const TCHAR* getTemplate();
   const TCHAR* getOutputPath();
   const TCHAR* getOptions();
   int  getType();
   bool getDebugInfoEnabled();

   bool getBoolSetting(const TCHAR* name);   

   void setArguments(const TCHAR* target);
   void setTarget(const TCHAR* target);
   void setTemplate(const TCHAR* target);
   void setOutputPath(const TCHAR* path);
   void setStartSymbol(const TCHAR* startSymbol);
   void setPackage(const TCHAR* package);
   void setOptions(const TCHAR* options);
   void setType(int type);
   void setDebugInfoEnabled(bool enabled);

   void setBoolSetting(const TCHAR* key, bool value);

   _ELENA_::ConfigCategoryIterator SourceFiles()
   {
      return _config.getCategoryIt(IDE_FILES_SECTION);
   }

   _ELENA_::ConfigCategoryIterator Forwards()
   {
      return _config.getCategoryIt(IDE_FORWARDS_SECTION);
   }

   bool isIncluded(const TCHAR* path);
   void includeSource(const TCHAR* path);
   void excludeSource(const TCHAR* path);

   void clearForwards();
   void addForward(const TCHAR* name, const TCHAR* reference);

   void setName(const TCHAR* path);

   void retrieveName(const TCHAR* path, _ELENA_::LocalReferenceName & name);
   void retrievePath(const TCHAR* reference, _ELENA_::LocalPath & path, const TCHAR* extension);

   bool open(const TCHAR* path);
   void save();
   void reset();

   ProjectInfo()
   {
      _changed = false;
   }
};

} // _GUI_

#endif // ideproject
