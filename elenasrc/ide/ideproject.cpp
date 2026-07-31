//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      ProjectInfo class implementation
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#include "idecommon.h"
// --------------------------------------------------------------------------
#include "ideproject.h"
#include "idesettings.h"
#include "elenaconst.h"

using namespace _GUI_;
using namespace _ELENA_;

inline void convert(_ELENA_::IniConfigFile& config)
{
   if (emptystr(config.getSetting(IDE_PROJECT_SECTION, IDE_DEBUGINFO_SETTING))) {
      Settings::project.setDebugInfoEnabled(!emptystr(config.getSetting(IDE_PROJECT_SECTION, IDE_OLD_TYPE_DEBUG)));
   }

   if (emptystr(config.getSetting(IDE_PROJECT_SECTION, IDE_TYPE_SETTING))) {
      const TCHAR* type = config.getSetting(IDE_PROJECT_SECTION, IDE_OLD_TYPE_SETTING, _T("0"));
      if (compstr(type, _T("0")))
         Settings::project.setType(ptConsole);
      else if (compstr(type, _T("1")))
         Settings::project.setType(ptGUI);
      else if (compstr(type, _T("2")))
         Settings::project.setType(ptLibrary);
   }
}

// --- ProjectInfo ---

const TCHAR* ProjectInfo :: getArguments()
{
   return _config.getSetting(IDE_PROJECT_SECTION, IDE_ARGUMENT_SETTING);
}

const TCHAR* ProjectInfo :: getPackage()
{
   return _config.getSetting(IDE_PROJECT_SECTION, IDE_PACKAGE_SETTING);
}

const TCHAR* ProjectInfo :: getStartSymbol()
{
   return _config.getSetting(IDE_PROJECT_SECTION, IDE_ENTRY_SETTING);
}

const TCHAR* ProjectInfo :: getTarget()
{
   return _config.getSetting(IDE_PROJECT_SECTION, IDE_EXECUTABLE_SETTING);
}

const TCHAR* ProjectInfo :: getOutputPath()
{
   return _config.getSetting(IDE_PROJECT_SECTION, IDE_OUTPUT_SETTING);
}

bool ProjectInfo :: getDebugInfoEnabled()
{
   return getBoolSetting(IDE_DEBUGINFO_SETTING);
}

const TCHAR* ProjectInfo :: getTemplate()
{
   return _config.getSetting(IDE_PROJECT_SECTION, IDE_TEMPLATE_SETTING);
}

const TCHAR* ProjectInfo :: getOptions()
{
   return _config.getSetting(IDE_PROJECT_SECTION, IDE_COMPILER_OPTIONS);
}

bool ProjectInfo :: getBoolSetting(const TCHAR* name)
{
   const TCHAR* value = _config.getSetting(IDE_PROJECT_SECTION, name);

   return compstr(value, _T("-1"));
}

int ProjectInfo :: getType()
{
   const TCHAR* typeLabel = _config.getSetting(IDE_PROJECT_SECTION, IDE_TYPE_SETTING);
   int type = (typeLabel != NULL) ? _ttoi(typeLabel) : ptLibrary;

   switch (type) {
      case ptConsole:
         return 1;
      case ptGUI:
         return 2;
      case ptLibrary:
      default:
         return 0;
   }
}

void ProjectInfo :: setArguments(const TCHAR* target)
{
   if(!emptystr(target)) {
      _config.setSetting(IDE_PROJECT_SECTION, IDE_ARGUMENT_SETTING, target);
   }
   else _config.clear(IDE_PROJECT_SECTION, IDE_ARGUMENT_SETTING);

   _changed = true;
}

void ProjectInfo :: setTarget(const TCHAR* target)
{
   if (!emptystr(target)) {
      _config.setSetting(IDE_PROJECT_SECTION, IDE_EXECUTABLE_SETTING, target);
   }
   else _config.clear(IDE_PROJECT_SECTION, IDE_EXECUTABLE_SETTING);

   _changed = true;
}

void ProjectInfo :: setTemplate(const TCHAR* templateName)
{
   if (!emptystr(templateName)) {
      _config.setSetting(IDE_PROJECT_SECTION, IDE_TEMPLATE_SETTING, templateName);
   }
   else _config.clear(IDE_PROJECT_SECTION, IDE_TEMPLATE_SETTING);

   _changed = true;
}

void ProjectInfo :: setDebugInfoEnabled(bool enabled)
{
   if (enabled) {
      _config.setSetting(IDE_PROJECT_SECTION, IDE_DEBUGINFO_SETTING, _T("-1"));
   }
   else _config.clear(IDE_PROJECT_SECTION, IDE_DEBUGINFO_SETTING);

   _changed = true;
}

void ProjectInfo :: setOutputPath(const TCHAR* path)
{
   if (!emptystr(path)) {
      _config.setSetting(IDE_PROJECT_SECTION, IDE_OUTPUT_SETTING, path);
   }
   else _config.clear(IDE_PROJECT_SECTION, IDE_OUTPUT_SETTING);

   _changed = true;
}

void ProjectInfo :: setStartSymbol(const TCHAR* startSymbol)
{
   if (!emptystr(startSymbol)) {
      _config.setSetting(IDE_PROJECT_SECTION, IDE_ENTRY_SETTING, startSymbol);
   }
   else _config.clear(IDE_PROJECT_SECTION, IDE_ENTRY_SETTING);

   _changed = true;
}

void ProjectInfo :: setPackage(const TCHAR* package)
{
   if (!emptystr(package)) {
      _config.setSetting(IDE_PROJECT_SECTION, IDE_PACKAGE_SETTING, package);
   }
   else _config.clear(IDE_PROJECT_SECTION, IDE_PACKAGE_SETTING);

   _changed = true;
}

void ProjectInfo :: setOptions(const TCHAR* options)
{
   if (!emptystr(options)) {
      _config.setSetting(IDE_PROJECT_SECTION, IDE_COMPILER_OPTIONS, options);
   }
   else _config.clear(IDE_PROJECT_SECTION, IDE_COMPILER_OPTIONS);

   _changed = true;
}

void ProjectInfo :: setType(int type)
{
   switch (type) {
      case 0: 
         _config.setSetting(IDE_PROJECT_SECTION, IDE_TYPE_SETTING, _T("0"));      // !! ptModule
         break;
      case 1:
         _config.setSetting(IDE_PROJECT_SECTION, IDE_TYPE_SETTING, _T("1"));      // !! ptConsole
         break;
      case 2:
         _config.setSetting(IDE_PROJECT_SECTION, IDE_TYPE_SETTING, _T("2"));      // !! ptGUI
         break;
   }
   _changed = true;
}

void ProjectInfo :: setBoolSetting(const TCHAR* key, bool value)
{
   _config.setSetting(IDE_PROJECT_SECTION, key, value ? _T("-1") : _T("0"));

   _changed = true;
}

void ProjectInfo :: clearForwards()
{
   _config.clear(IDE_FORWARDS_SECTION);

   _changed = true;
}

void ProjectInfo :: addForward(const TCHAR* name, const TCHAR* reference)
{
   _config.setSetting(IDE_FORWARDS_SECTION, name, reference);

   _changed = true;
}

void ProjectInfo :: setName(const TCHAR* path)
{
   _name.copyName(path);
   _path.copyPath(path);
   _path.lower();

   Paths::resolveRelativePath(_path);

   _changed = true;
}

void ProjectInfo :: retrieveName(const TCHAR* path, _ELENA_::LocalReferenceName & name)
{
   const TCHAR* root = _path;

   Path fullPath(path);
   Paths::resolveRelativePath(fullPath, root);
   fullPath.lower();

   if (!emptystr(root) && compstr(fullPath, root, getlength(root))) {
      name.copy(getPackage());
      name.pathToName(fullPath.asString() + getlength(root) + 1);
   }
   else {
      root = Paths::packageRoot;
      if (!emptystr(root) && compstr(fullPath, root, getlength(root))) {
         name.pathToName(fullPath.asString() + getlength(root) + 1);
      }
      else {
         FileName fileName(fullPath);

         name.copy(fileName);
      }
   }
}

void ProjectInfo :: retrievePath(const TCHAR* name, _ELENA_::LocalPath & path, const TCHAR* extension)
{
   path.copy(_path);

   const TCHAR* package = getPackage();
   LocalNamespace ns(name);
   if (!emptystr(package) && compstr(ns, package)) {
      path.nameToPath(name + getlength(package) + 1, extension);
   }
   else {
      path.nameToPath(name, extension);
     
      if (!path.exists()) {
         // if file doesn't exist use package root
         path.copy(Paths::libraryRoot);

         path.nameToPath(name, extension);
      }
   }
}

bool ProjectInfo :: open(const TCHAR* path)
{
   _config.clear();
   if (!_config.load(path)) {
      MsgBox::show(NULL, IDE_MSG_INVALID_PROJECT, path, MB_OK | MB_ICONERROR);
      return false;
   }
   setName(path);

   // convert old style project
   if (!emptystr(_config.getSetting(IDE_PROJECT_SECTION, IDE_OLD_TYPE_SETTING)) 
      || !emptystr(_config.getSetting(IDE_PROJECT_SECTION, IDE_OLD_TYPE_DEBUG))) 
   {
      convert(_config);
      save();
   }

   _changed = false;
   return true;
}

void ProjectInfo :: save()
{
   Path cfgPath(_path, _name);
   cfgPath.appendExtension(_T("prj"));

   _config.save(cfgPath, Settings::defaultEncoding);

   _changed = false;
}

void ProjectInfo :: reset()
{
   _config.clear();

   _name.clear();
   _path.clear();

   // set default values
   setType(ptConsole);
   setStartSymbol(_T("'entry"));

   // should be the last to prevent being marked as changed
   _changed = false;
}

bool ProjectInfo :: isIncluded(const TCHAR* path)
{
   Path relPath(path);
   Paths::makeRelativePath(relPath, _path);

   ConfigCategoryIterator it = SourceFiles();
   while (!it.Eof()) {
	  if (compstr(relPath, it.key())) {
         return true;
      }
      it++;
   }
   return false;
}

void ProjectInfo :: includeSource(const TCHAR* path)
{
   Path relPath(path);
   Paths::makeRelativePath(relPath, _path);

   _config.setSetting(IDE_FILES_SECTION, _ELENA_::strdup(relPath), DEFAULT_STR);

   _changed = true;
}

void ProjectInfo :: excludeSource(const TCHAR* path)
{
   Path relPath(path);
   Paths::makeRelativePath(relPath, _path);

   _config.clear(IDE_FILES_SECTION, relPath);

   _changed = true;
}
