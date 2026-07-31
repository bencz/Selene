//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA command-Line Compiler
//
//      This file contains the common constants of the command-line
//      compiler and a ELC project class
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef elcH
#define elcH 1

#include <stdarg.h>
#include "project.h"
#include "config.h"

// --- ELC default file names ---
#define DEFAULT_CONFIG              _T("elc.cfg")
#define SYNTAX_FILE                 _T("syntax.dat")
// Built from MODULE_EXTENSION rather than spelled out, so the extension is
// defined in exactly one place (engine/elenaconst.h).
#define ELC_STANDARD_MODULE         _T("elena.") MODULE_EXTENSION

// --- ELC common constants ---
#define ELC_MAJOR_VERSION            0x0002             // ELENA Enging version
#define ELC_MINOR_VERSION            0x0000

// --- ELC command-line parameters ---
#define ELC_PRM_CONFIG              'c'
#define ELC_PRM_DEBUGINFO           'd'
#define ELC_PRM_ENTRY               'e'
#define ELC_PRM_PACKAGE             'g'
#define ELC_PRM_LIBRARY             'l'
#define ELC_PRM_STANDART_LIBRARY    _T("lstd")
#define ELC_PRM_MAP                 'm'
#define ELC_PRM_OUTPUT_PATH         'o'
#define ELC_PRM_LIB_PATH            'p'
#define ELC_PRM_START               's'
#define ELC_PRM_TARGET              't'
#define ELC_PRM_WARNING             'w'
#define ELC_W_UNRESOLVED            _T("wun")
#define ELC_W_WEAKUNRESOLVED        _T("wwun")
#define ELC_PRM_EXTRA               'x'
#define ELC_PRM_TABSIZE             _T("xtab")
#define ELC_PRM_UNICODE             _T("xunicode")
#define ELC_PRM_PROJECTPATH         _T("xpath")

// --- ELC config categories ---
#define COMPILER_CATEGORY           _T("compiler") 
#define TEMPLATE_CATEGORY           _T("templates")
#define PROJECT_CATEGORY            _T("project")
#define LINKER_CATEGORY             _T("linker")
#define PRIMITIVE_CATEGORY          _T("primitives")
#define SOURCE_CATEGORY             _T("files")
#define FORWARD_CATEGORY            _T("forwards")

// --- ELC config settings ---
#define ELC_PROJECT_TEMPLATE        _T("template")
#define ELC_PROJECT_ENTRY           _T("entry")
#define ELC_PACKAGE                 _T("package")
#define ELC_TARGET                  _T("executable")
#define ELC_LIB_PATH                _T("libpath")
#define ELC_OUTPUT_PATH             _T("output")
#define ELC_WARNON_UNRESOLVED       _T("warn:unresolved")
#define ELC_GC_PAGESIZE             _T("gcsize")
#define ELC_SYSTEMTYPE              _T("type")
#define ELC_PROJECT_START           _T("start")
#define ELC_DEBUGINFO               _T("debuginfo") 
#define ELC_COMPILER_LITERALCLASS   _T("literalclass")
#define ELC_COMPILER_INTEGERCLASS   _T("integerclass")
#define ELC_COMPILER_REALCLASS      _T("realclass")
#define ELC_COMPILER_ARRAYCLASS     _T("arrayclass")

// --- ELC information messages ---
#define ELC_GREETING                _T("ELENA command-line compiler %d.%d (C)2005-2009 by Alex Rakov\n")
#define ELC_INTERNAL_ERROR          _T("Internal error:%s\n")
#define ELC_COMPILING               _T("Compiling...")
#define ELC_LINKING                 _T("Linking...")
#define ELC_SUCCESSFUL_COMPILATION  _T("Successfully compiled\n")
#define ELC_WARNING_COMPILATION     _T("Compiled with warnings\n")
#define ELC_UNSUCCESSFUL            _T("Compiled with errors\n")
#define ELC_SUCCESSFUL_LINKING      _T("Successfully linked\n")
#define ELC_HELP_INFO               _T("elc {-key} {<input file>}\n\nkeys: -c<path>   - specifies the project file\n      -d<path>   - generates the debug info file\n      -e<symbol> - resolves the entry forward symbol\n      -g<name>   - specifies the package name\n      -lstd      - sets standard module flag\n      -m<path>   - generates the map file\n      -o<path>   - sets the output path\n      -p<path>   - inlcudes the path to the library\n      -t<path>   - sets the target executable file name\n      -wun       - sets on the unresolved warnings\n      -xguit     - sets GUI application type\n")

// --- ELC error messages ---
#define ELC_ERR_INVALID_OPTION	   _T("elc: error 401: Invalid command line parameter '%c'\n")
#define ELC_ERR_INVALID_PATH        _T("elc: error 402: Invalid or none-existing file '%s'\n")
#define ELC_ERR_INVALID_TEMPLATE    _T("elc: error 404: Invalid or none-existing template '%s'\n")

using namespace _ELENA_;

namespace _ELC_
{

// --- ELC type definitions ---
typedef _ELENA_::IniConfigFile ElcConfigFile;

// --- Command Line Project ---
class Project : public _ELENA_::Project
{
   int _tabSize;

public:
   _ELENA_::Path appPath;

   virtual void raiseError(const TCHAR* msg, ...)
   {
      va_list argptr;

      va_start(argptr, msg);
      _vtprintf(msg, argptr);
      va_end(argptr);
      _tprintf(_T("\n"));
      fflush(stdout);

      throw _Exception();
   }

   void raiseErrorIf(bool throwExecption, const TCHAR* msg, ...)
   {
      va_list argptr;

      va_start(argptr, msg);
      _vtprintf(msg, argptr);
      va_end(argptr);
      _tprintf(_T("\n"));
      fflush(stdout);

      if (throwExecption)
         throw _Exception();
   }

   virtual void printInfo(const TCHAR* msg, ...)
   {
      va_list argptr;

      va_start(argptr, msg);
      _vtprintf(msg, argptr);
      va_end(argptr);
      _tprintf(_T("\n"));
      fflush(stdout);
   }

   void addSource(const TCHAR* path);
   void addForward(const TCHAR* forward, const TCHAR* reference);

   void loadCategory(_ConfigFile& config, const TCHAR* configPath, ProjectSetting setting, const TCHAR* category);

   void loadConfig(const TCHAR* path, bool requiered = true);

   void setOption(const TCHAR* value);

   void setOption(ProjectSetting setting, _ConfigFile& config, const TCHAR* category, const TCHAR* key)
   {
      const TCHAR* value = config.getSetting(category, key);
      if (value) {
         _settings.add(setting, _ELENA_::strdup(value));
      }
   }

   void setPathOption(ProjectSetting setting, const TCHAR* configPath, _ConfigFile& config, const TCHAR* category, const TCHAR* key);

   void setIntOption(ProjectSetting setting, _ConfigFile& config, const TCHAR* category, const TCHAR* key, int defaultValue = 0)
   {
      int value = config.getIntSetting(category, key, defaultValue);
      if (value != defaultValue) {
         _settings.add(setting, value);
      }
   }

   void setBoolOption(ProjectSetting setting, _ConfigFile& config, const TCHAR* category, const TCHAR* key)
   {
      bool value = (config.getIntSetting(category, key, 0) != 0);
      if (value) {
         _settings.add(setting, -1);
      }
   }

   virtual int getTabSize() { return _tabSize; }

   virtual const TCHAR* StrSetting(ProjectSetting key)
   {
      if (key==opAppPath) {
         return appPath;
      }
      else return _ELENA_::Project::StrSetting(key);
   }

   virtual _Module* loadModule(const TCHAR* path, bool silentMode);

   void loadStandardModule()
   {
      loadModule(ELC_STANDARD_MODULE, false);
   }

   void cleanUp();

   Project();
};

} // _ELC_

#endif // elcH
