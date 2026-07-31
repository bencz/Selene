//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      Settings class header
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef idesettingsH
#define idesettingsH

#include "ideproject.h"

namespace _GUI_
{

// --- Paths ---

struct Paths
{
   static _ELENA_::Path appPath;
   static _ELENA_::Path defaultPath;
   static _ELENA_::Path packageRoot;
   static _ELENA_::Path libraryRoot;
   static _ELENA_::Path lastPath;

   static void init(const TCHAR* packagePath, const TCHAR* libraryPath);

   static void resolveRelativePath(_ELENA_::Path& path, const TCHAR* rootPath);
   static void resolveRelativePath(_ELENA_::Path& path)
   {
      resolveRelativePath(path, defaultPath);
   }

   static void makeRelativePath(_ELENA_::Path& path, const TCHAR* rootPath);

   template<class String> static void canonicalize(_ELENA_::PathTemplate<String> & path);
};

// --- Settings ---

struct Settings
{
   static bool testMode;
   static bool appMaximized;
   static bool compilerOutput;
   static bool hexNumberMode;
   static bool lineNumberVisible;
   static bool tabCharUsing;
   static bool highlightSyntax;
   static int  scheme;
   static size_t tabSize;
   static bool unicodeELC;
   static bool lastPathRemember;
   static bool lastProjectRemember;
   static bool autoRecompile;
   static bool tabWithAboveScore;
   static bool highlightBrackets;

   static bool bytecode; // !! temporal

   static _ELENA_::FileEncoding defaultEncoding;

   static ProjectInfo   project;
   static _ELENA_::Path defaultProject;

   static _ELENA_::List<TCHAR*> defaultFiles;
   static _ELENA_::List<TCHAR*> plugins;

   static void load(_ELENA_::IniConfigFile& config);
   static void save(_ELENA_::IniConfigFile& config);

   static void clear()
   {
      defaultFiles.clear();
      project.reset();
   }
};

} // _GUI_

#endif // idesettingsH
