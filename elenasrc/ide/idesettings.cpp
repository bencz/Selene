//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      Settings class implementation
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#include "idecommon.h"
// --------------------------------------------------------------------------
#include "idesettings.h"
#include <direct.h>

using namespace _GUI_;
using namespace _ELENA_;

// --- Static variable initializing ---

Path Paths :: appPath = Path();

Path Paths :: defaultPath = Path();

Path Paths :: packageRoot = Path();

Path Paths :: libraryRoot = Path();

Path Paths :: lastPath = Path();

bool Settings :: appMaximized = true;

bool Settings :: compilerOutput = true;

bool Settings :: testMode = false;

bool Settings :: hexNumberMode = true;

bool Settings :: lineNumberVisible = false;

bool Settings :: tabCharUsing = false;

bool Settings :: highlightSyntax = true;

bool Settings :: unicodeELC = false;

bool Settings :: lastPathRemember = true;

bool Settings :: lastProjectRemember = true;

bool Settings :: autoRecompile = true;

bool Settings :: tabWithAboveScore = true;

bool Settings :: highlightBrackets = true;

bool Settings :: bytecode = false; // !! temporal

int Settings :: scheme = 0;

size_t Settings :: tabSize = 4;

FileEncoding Settings :: defaultEncoding = feAnsi;

Path Settings :: defaultProject = Path();

List<TCHAR*> Settings :: defaultFiles = List<TCHAR*>(NULL, freestr);

List<TCHAR*> Settings :: plugins = List<TCHAR*>(NULL, freestr);

ProjectInfo Settings :: project = ProjectInfo();

// -- Paths ---

void Paths :: init(const TCHAR* packagePath, const TCHAR* libraryPath)
{
   TCHAR path[MAX_PATH];
   ::GetModuleFileName(NULL, path, MAX_PATH);
   ::PathRemoveFileSpec(path);

   appPath.copy(path);

   packageRoot.copy(packagePath);
   resolveRelativePath(packageRoot, appPath);
   packageRoot.lower();

   libraryRoot.copy(libraryPath);
   resolveRelativePath(libraryRoot, appPath);
   libraryRoot.lower();


#ifdef _UNICODE
   _wgetcwd(path, MAX_PATH);
#else
   _getcwd(path, MAX_PATH);
#endif

   defaultPath.copy(path);
   lastPath.copy(path);
}

template<class String> void Paths :: canonicalize(_ELENA_::PathTemplate<String> & path)
{
   TCHAR p[MAX_PATH];

   ::PathCanonicalize(p, path);

   path.copy(p);

}

void Paths :: resolveRelativePath(Path& path, const TCHAR* rootPath)
{
   if (::PathIsRelative(path)) {
      Path fullPath(rootPath);
      fullPath.combine(path);

      path.copy(fullPath);
   }
   canonicalize(path);
}

void Paths :: makeRelativePath(Path& path, const TCHAR* rootPath)
{
   TCHAR tmpPath[MAX_PATH];

   ::PathRelativePathTo(tmpPath, rootPath, FILE_ATTRIBUTE_DIRECTORY, path, FILE_ATTRIBUTE_NORMAL);
   if (!emptystr(tmpPath)) {
      if (compstr(tmpPath, _T(".\\"), 2)) {
         path.copy(tmpPath + 2);
      }
      else path .copy(tmpPath);
   }
}

// --- Settings ---

void Settings :: load(IniConfigFile& config)
{
   defaultProject.copy(config.getSetting(_T("settings"), _T("defaultproject")));

   compilerOutput = compstr(config.getSetting(_T("settings"), _T("compileroutput")), _T("-1"));
   lineNumberVisible = compstr(config.getSetting(_T("settings"), _T("linenumbers")), _T("-1"));
   tabCharUsing = compstr(config.getSetting(_T("settings"), _T("tabusing")), _T("-1"));
   unicodeELC = compstr(config.getSetting(_T("settings"), _T("unicode_output")), _T("-1"));
   highlightBrackets = compstr(config.getSetting(_T("settings"), _T("highlightbrackets"), _T("-1")), _T("-1"));

   const TCHAR* value = config.getSetting(_T("settings"), _T("highlightsyntax"));
   if (!emptystr(value))
      highlightSyntax = compstr(value, _T("-1"));

   String index(config.getSetting(_T("settings"), _T("style")));
   if (!index.isEmpty()) {
      scheme = _ttoi(index);
      if (scheme > SCHEME_COUNT) {
         scheme = 0;
      }
   }
   if (config.getSetting(_T("settings"), _T("tabsize"))) {
      tabSize = _ttoi(config.getSetting(_T("settings"), _T("tabsize")));
      if (tabSize <= 0 || tabSize > 20) {
         tabSize = 4;
      }
   }
   if (config.getSetting(_T("settings"), _T("encoding"))) {
      defaultEncoding = (FileEncoding)_ttoi(config.getSetting(_T("settings"), _T("encoding")));
   }
   if (config.getSetting(_T("settings"), _T("remeber_path"))) {
      lastPathRemember = compstr(config.getSetting(_T("settings"), _T("remeber_path")), _T("-1"));
   }
   if (config.getSetting(_T("settings"), _T("remeber_project"))) {
      lastProjectRemember = compstr(config.getSetting(_T("settings"), _T("remeber_project")), _T("-1"));
   }
   if (config.getSetting(_T("settings"), _T("autocomp"))) {
      autoRecompile = compstr(config.getSetting(_T("settings"), _T("autocomp")), _T("-1"));
   }
   if (config.getSetting(_T("settings"), _T("tabscore"))) {
      tabWithAboveScore = compstr(config.getSetting(_T("settings"), _T("tabscore")), _T("-1"));
   }

   // load plugins
   for(ConfigCategoryIterator it = config.getCategoryIt(_T("plugins")); !it.Eof(); it++) {
      plugins.add(_ELENA_::strdup(it.key()));
   }
}

void Settings :: save(IniConfigFile& config)
{
   if (!defaultProject.isEmpty() && lastProjectRemember)
      config.setSetting(_T("settings"), _T("defaultproject"), defaultProject);

   config.setSetting(_T("settings"), _T("compileroutput"), compilerOutput);
   config.setSetting(_T("settings"), _T("highlightsyntax"), highlightSyntax);
   config.setSetting(_T("settings"), _T("style"), scheme);
   config.setSetting(_T("settings"), _T("linenumbers"), lineNumberVisible);
   config.setSetting(_T("settings"), _T("tabusing"), tabCharUsing);
   config.setSetting(_T("settings"), _T("tabsize"), tabSize);
   config.setSetting(_T("settings"), _T("encoding"), (int)defaultEncoding);
   config.setSetting(_T("settings"), _T("unicode_output"), unicodeELC);
   config.setSetting(_T("settings"), _T("remeber_path"), lastPathRemember);
   config.setSetting(_T("settings"), _T("remeber_project"), lastProjectRemember);
   config.setSetting(_T("settings"), _T("autocomp"), autoRecompile);
   config.setSetting(_T("settings"), _T("tabscore"), tabWithAboveScore);

   // save only if negative value
   if(!highlightBrackets)
      config.setSetting(_T("settings"), _T("highlightbrackets"), highlightBrackets);

   // save plugins
   List<TCHAR*>::Iterator p_it = plugins.start();
   while (!p_it.Eof()) {
      config.setSetting(_T("plugins"), *p_it, DEFAULT_STR);

      p_it++;
   }

}
