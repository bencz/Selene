//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA command-Line Compiler
//
//		This file contains the platform-independent part of the command-line
//		compiler project handling.
//
//		Extracted from elc/win32/elc.cpp so that the POSIX and Win32 front ends
//		can share it. The only genuinely platform-specific pieces are
//		getAppPath() and main(), which live in elc/<platform>/elc.cpp.
//
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#include "elena.h"
// --------------------------------------------------------------------------
#include "elc.h"
#include "errors.h"
#include "compiler.h"
#include "targetinfo.h"

using namespace _ELENA_;

// Provided by the platform front end (elc/win32/elc.cpp, elc/posix/elc.cpp)
void getAppPath(Path& appPath);

// --- Project ---

_ELC_::Project :: Project()
{
   getAppPath(appPath);

   _tabSize = 4;
}

void _ELC_::Project :: addSource(const TCHAR* path)
{
   LocalPath fullPath(StrSetting(opProjectPath), path);

   fullPath.lower();

   _settings.add(opSources, path, _ELENA_::strdup(fullPath));
}

void _ELC_::Project :: addForward(const TCHAR* forward, const TCHAR* reference)
{
   LocalString<IDENTIFIER_LEN> fwd(forward);

   fwd.lower();

   _settings.add(opForwards, fwd, _tcslwr(_ELENA_::strdup(reference)));
}

void _ELC_::Project :: cleanUp()
{
   for(SourceIterator it = getSourceIt() ; !it.Eof() ; it++) {
      LocalReferenceName name(StrSetting(opPackage));
      const TCHAR* outputPath = StrSetting(opOutputPath);
      LocalPath path(outputPath);

      name.pathToName(it.key());          // get a full name

      nameToPath(name, path, MODULE_EXTENSION);   // get a full path

      removeFile(path);
   }
}

void _ELC_::Project :: loadCategory(_ConfigFile& config, const TCHAR* path, ProjectSetting setting, const TCHAR* category)
{
   String key(100);

   ConfigCategoryIterator it = config.getCategoryIt(category);
   while (!it.Eof()) {
      // copy line key
      key.copy(it.key());
      key.lower();

      // copy value or key if the value is absent
      const TCHAR* value = *it;
      if(emptystr(value))
         value = key;

      // add path if provided
      if (!emptystr(path)) {
         LocalPath filePath(path, value);

         // NOT lowercased. When a path is prepended the value is a FILE PATH,
         // and lowercasing it breaks on any case-sensitive filesystem the
         // moment a directory has a capital in it -- which is why templates,
         // sources and primitives silently failed to load on Linux while
         // appearing to work on Windows.
         _settings.add(setting, key, _ELENA_::strdup(filePath));
      }
      // With no path the value is a reference name, and Selene resolves those
      // case-insensitively, so lowercasing is correct here.
      else _settings.add(setting, key, _tcslwr(_ELENA_::strdup(value)));

      it++;
   }
}

void _ELC_::Project :: setPathOption(ProjectSetting setting, const TCHAR* configPath, _ConfigFile& config,
                                     const TCHAR* category, const TCHAR* key)
{
   const TCHAR* value = config.getSetting(category, key);
   if (value) {
      LocalPath path(configPath, value);

      _settings.add(setting, _ELENA_::strdup(path));
   }
}

void _ELC_::Project :: loadConfig(const TCHAR* path, bool requiered)
{
   ElcConfigFile config;
   LocalPath     configPath;

   configPath.copyPath(path);

   if (!config.load(path)) {
      // raiseErrorIf prints unconditionally and only the THROW is conditional,
      // so an optional config that is simply absent used to report an error.
      if (requiered)
         raiseError(ELC_ERR_INVALID_PATH, path);

      return;
   }

   // load template list
   loadCategory(config, configPath, opTemplates, TEMPLATE_CATEGORY);

   // load template
   const TCHAR* projectTemplate = config.getSetting(PROJECT_CATEGORY, ELC_PROJECT_TEMPLATE);
   if (!emptystr(projectTemplate)) {
      const TCHAR* templateFile = _settings.get(opTemplates, projectTemplate, DEFAULT_STR);
      if (emptystr(templateFile))
         raiseErrorIf(requiered, ELC_ERR_INVALID_TEMPLATE, projectTemplate);

      loadConfig(templateFile, false);
   }

   // load entry symbol
   const TCHAR* entry = config.getSetting(PROJECT_CATEGORY, ELC_PROJECT_ENTRY);
   if (entry) {
      addForward(STARTUP_CLASS, entry);
   }

   // load project settings (if setting is absent previous value is used)
   setOption(opEntry, config, PROJECT_CATEGORY, ELC_PROJECT_START);
   setOption(opPackage, config, PROJECT_CATEGORY, ELC_PACKAGE);
   setPathOption(opTarget, configPath, config, PROJECT_CATEGORY, ELC_TARGET);
   setPathOption(opLibPath, configPath, config, PROJECT_CATEGORY, ELC_LIB_PATH);
   setPathOption(opOutputPath, configPath, config, PROJECT_CATEGORY, ELC_OUTPUT_PATH);
   setBoolOption(opWarnOnUnresolved, config, PROJECT_CATEGORY, ELC_WARNON_UNRESOLVED);
   setBoolOption(opWithDebugInfo, config, PROJECT_CATEGORY, ELC_DEBUGINFO);

   // load compiler settings
   setOption(opLiteralClass, config, COMPILER_CATEGORY, ELC_COMPILER_LITERALCLASS);
   setOption(opIntegerClass, config, COMPILER_CATEGORY, ELC_COMPILER_INTEGERCLASS);
   setOption(opRealClass, config, COMPILER_CATEGORY, ELC_COMPILER_REALCLASS);
   setOption(opArrayClass, config,   COMPILER_CATEGORY, ELC_COMPILER_ARRAYCLASS);

   // load linker settings
   setIntOption(opGCHeapSize, config, LINKER_CATEGORY, ELC_GC_PAGESIZE);
   setIntOption(opSystemType, config, LINKER_CATEGORY, ELC_SYSTEMTYPE);

   // load primitives
   loadCategory(config, configPath, opPrimitives, PRIMITIVE_CATEGORY);

   // load sources
   loadCategory(config, configPath, opSources, SOURCE_CATEGORY);

   // load forwards
   loadCategory(config, NULL, opForwards, FORWARD_CATEGORY);
}

void _ELC_::Project :: setOption(const TCHAR* value)
{
   switch (value[0]) {
      case ELC_PRM_LIB_PATH:
         _settings.add(opLibPath, value + 1);
         break;
      case	ELC_PRM_OUTPUT_PATH:
         _settings.add(opOutputPath, value + 1);
         break;
      case ELC_PRM_PACKAGE:
         _settings.add(opPackage, value + 1);
         break;
      case ELC_PRM_LIBRARY:
         if (compstr(value, ELC_PRM_STANDART_LIBRARY)) {
            _settings.add(opStandart, -1);
         }
         else raiseError(ELC_ERR_INVALID_OPTION, value);
         break;
      case ELC_PRM_EXTRA:
   		if (compstr(value, ELC_PRM_TABSIZE, 4)) {
            _tabSize = _ttoi(value + 4);
         }
         else if (compstr(value, ELC_PRM_UNICODE)) {
            // !! defect preserved from the original: this sets the OUTPUT PATH
            // to the literal string "unicode". It is harmless only because
            // main() intercepts -xunicode separately before this runs.
            _settings.add(opOutputPath, value + 1);
         }
         else if (compstr(value, ELC_PRM_PROJECTPATH, getlength(ELC_PRM_PROJECTPATH))) {
            _settings.add(opProjectPath, value + getlength(ELC_PRM_PROJECTPATH));
            // If output path is not provided use project path
            if (emptystr(StrSetting(opOutputPath)))
               _settings.add(opOutputPath, value + getlength(ELC_PRM_PROJECTPATH));
         }
         else raiseError(ELC_ERR_INVALID_OPTION, value);
         break;
      case ELC_PRM_WARNING:
         if (compstr(value, ELC_W_UNRESOLVED)) {
            _settings.add(opWarnOnUnresolved, -1);
         }
         else if (compstr(value, ELC_W_WEAKUNRESOLVED)) {
            _settings.add(opWarnOnWeakUnresolved, -1);
         }
         else raiseError(ELC_ERR_INVALID_OPTION, value);
         break;
      case ELC_PRM_TARGET:
         _settings.add(opTarget, value + 1);
         break;
      case ELC_PRM_MAP:
         _settings.add(opMapFile, value + 1);
         break;
      case ELC_PRM_ENTRY:
         addForward(STARTUP_CLASS, value + 1);
         break;
      case ELC_PRM_START:
         _settings.add(opEntry, value + 1);
         break;
      case ELC_PRM_DEBUGINFO:
         _settings.add(opWithDebugInfo, -1);
         break;
      case ELC_PRM_CONFIG:
         loadConfig(value + 1);
         // If output path is not provided use project path
         if (emptystr(StrSetting(opOutputPath))) {
            LocalPath projectPath;
            projectPath.copyPath(value + 1);
            _settings.add(opOutputPath, _ELENA_::strdup(projectPath));
         }
         break;
      default:
         raiseError(ELC_ERR_INVALID_OPTION, value[0]);
   }
}

_Module* _ELC_::Project :: loadModule(const TCHAR* path, bool silentMode)
{
   LocalPath fullPath(_settings.get(opLibPath, DEFAULT_STR), path);

   return _ELENA_::Project::loadModule(fullPath, silentMode);
}

// --- --target= handling ---
//
// Shared by both front ends. Intercepted before the ordinary option loop
// because the leading "--" would otherwise be parsed as an empty option letter.
//
// Returns false when the caller should stop: either the target was invalid, or
// "--target=?" was asked for and the listing has been printed.
bool _ELC_::processTargetOptions(int argc, const TCHAR** args, bool* consumed, int& exitCode)
{
   for (int i = 1 ; i < argc ; i++) {
      consumed[i] = false;

      if (!compstr(args[i], _T("--target="), 9))
         continue;

      consumed[i] = true;

      const TCHAR* name = args[i] + 9;

      if (compstr(name, _T("?"))) {
         size_t count = 0;
         const TargetInfo* list = getTargetList(count);

         _tprintf(_T("known targets:\n"));
         for (size_t k = 0 ; k < count ; k++) {
            printf("  %-11s %-34s %2d-bit %s%s\n",
                   list[k].name, list[k].triple, list[k].pointerBits,
                   list[k].isBigEndian() ? "big-endian" : "little-endian",
                   list[k].functionDescriptors ? ", function descriptors" : "");
         }
         exitCode = 0;
         return false;
      }

      char plain[0x40];
      size_t length = 0;
      for ( ; name[length] && length < sizeof(plain) - 1 ; length++)
         plain[length] = (char)name[length];
      plain[length] = 0;

      const TargetInfo* selected = getTargetByName(plain);
      if (!selected) {
         _tprintf(ELC_ERR_INVALID_TARGET, name);
         exitCode = -3;
         return false;
      }

      setCurrentTarget(selected);
   }

   return true;
}

// --- platform bindings for the selected target ---
//
// Loaded after elc.cfg so it can rely on the library paths, and before the
// project's own template so a project can still override an individual forward.
void _ELC_::loadTargetConfig(_ELC_::Project& project)
{
   const char* osName = getCurrentTarget()->osConfigName();
   if (!osName)
      return;

   LocalString<0x40> name;
   size_t i = 0;
   for ( ; osName[i] ; i++) name.append((TCHAR)osName[i]);

   Path config(project.appPath, _T("targets"));
   config.combine(name);
   config.appendExtension(_T("cfg"));

   project.loadConfig(config, false);
}
