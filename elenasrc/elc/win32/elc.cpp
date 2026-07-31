//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA command-Line Compiler
//
//		Win32 front end: application path lookup and main().
//
//		The platform-independent project handling lives in elc/elcproject.cpp.
//
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#define __MSVCRT_VERSION__ 0x0800

#include "elena.h"
// --------------------------------------------------------------------------
#include "elc.h"
#include "errors.h"
#include "compiler.h"
#include "linker.h"
#include "win32/x86jitcompiler.h"

#include <windows.h>
#include <fcntl.h>

using namespace _ELENA_;

// --- getAppPath ---

void getAppPath(Path& appPath)
{
   TCHAR path[MAX_PATH + 1];

   ::GetModuleFileName(NULL, path, MAX_PATH);

   appPath.copyPath(path);
}

// --- Main function ---

int main()
{
   int argc;
   TCHAR **argv = CommandLineToArgvW(GetCommandLineW(), &argc);

   // switch to unicode command line output if requiered
   if (argc > 1 && compstr(argv[1] + 1, ELC_PRM_UNICODE))
      setmode(_fileno(stdout), _O_WTEXT);

   int    exitCode = 0;
   _ELC_::Project project;

   try {
      project.printInfo(ELC_GREETING, ELC_MAJOR_VERSION, ELC_MINOR_VERSION);

      if (argc < 2) {
         // show help if no parameters proveded
         _tprintf(ELC_HELP_INFO);
         return -3;
      }

      // Initializing..
      project.loadConfig(Path(project.appPath, DEFAULT_CONFIG), false);

      // Initializing..
      for (int i = 1 ; i < argc ; i++) {
         if (argv[i][0]=='-') {
            project.setOption(argv[i] + 1);
         }
         else project.addSource(argv[i]);
      }

      // load core module
      if (!project.BoolSetting(opStandart)) {
         project.loadStandardModule();
      }

      // Cleaning up
      project.printInfo(_T("Cleaning up..."));
      project.cleanUp();

      // Compiling..
      project.printInfo(ELC_COMPILING);

      Path syntaxPath(project.StrSetting(opAppPath), SYNTAX_FILE);
      FileReader syntaxFile(syntaxPath, feRaw);
      if (!syntaxFile.isOpened())
         project.raiseError(errInvalidFile, syntaxPath.asString());

      Compiler compiler(&syntaxFile);
      if (compiler.run(project))
         project.printInfo(ELC_SUCCESSFUL_COMPILATION);
      else {
         exitCode = -1;
         project.printInfo(ELC_WARNING_COMPILATION);
      }

      // Linking..
      if (project.IntSetting(opSystemType) != ptLibrary) {
         project.printInfo(ELC_LINKING);

         Linker linker(&project, new x86JITCompiler(project.resolvePrimitive(CORE_BINARY_MODULE, false)));
         linker.run();

         project.printInfo(ELC_SUCCESSFUL_LINKING);
      }
   }
   catch(_ELENA_::InternalError& e) {
      project.printInfo(ELC_INTERNAL_ERROR, e.message);
      exitCode = -2;

      project.cleanUp();
   }
   catch(_ELENA_::_Exception&) {
      project.printInfo(ELC_UNSUCCESSFUL);
      exitCode = -2;

      project.cleanUp();
   }
   return exitCode;
}
