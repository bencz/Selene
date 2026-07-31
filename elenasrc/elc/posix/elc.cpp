//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA command-Line Compiler
//
//		POSIX front end: application path lookup and main().
//
//		The platform-independent project handling lives in elc/elcproject.cpp.
//
//		SCOPE: this front end compiles .l sources to .nl bytecode modules and
//		stops there. It does NOT link.
//
//		Linking is deliberately absent rather than unfinished. The existing
//		linker emits PE/COFF and the existing code generator emits Win32-hosted
//		x86, so neither can produce anything runnable on POSIX. Both are to be
//		replaced by an LLVM backend plus a system linker.
//		  See docs/plan/17-llvm-backend-and-targets.md
//---------------------------------------------------------------------------

#include "elena.h"
// --------------------------------------------------------------------------
#include "elc.h"
#include "errors.h"
#include "compiler.h"

#include <unistd.h>
#include <limits.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

using namespace _ELENA_;

// --- getAppPath ---

void getAppPath(Path& appPath)
{
   char path[PATH_MAX + 1];
   ssize_t length = -1;

#ifdef __APPLE__
   uint32_t size = PATH_MAX;
   if (_NSGetExecutablePath(path, &size) == 0)
      length = (ssize_t)strlen(path);
#else
   length = readlink("/proc/self/exe", path, PATH_MAX);
#endif

   if (length <= 0) {
      // Fall back to the current directory. The compiler needs this only to
      // locate elc.cfg and the parser table, so a wrong answer produces a
      // clear "file not found" rather than misbehaviour.
      if (!getcwd(path, PATH_MAX))
         path[0] = 0;
      length = (ssize_t)strlen(path);
   }
   path[length] = 0;

#ifdef _UNICODE
   wchar_t wide[PATH_MAX + 1];
   ansiToUnicode(path, wide, (size_t)length);
   wide[length] = 0;

   appPath.copyPath(wide);
#else
   appPath.copyPath(path);
#endif
}

// --- Main function ---

int main(int argc, char* argv[])
{
   int    exitCode = 0;
   _ELC_::Project project;

   // argv is char*; widen it once up front when TCHAR is wchar_t, and use it
   // directly when TCHAR is char (the UTF-8 configuration).
   const TCHAR** args = new const TCHAR*[argc];
#ifdef _UNICODE
   LocalString<0x200>* wide = new LocalString<0x200>[argc];
   for (int i = 0 ; i < argc ; i++) {
      wide[i].convert(argv[i]);
      args[i] = wide[i];
   }
#else
   for (int i = 0 ; i < argc ; i++)
      args[i] = argv[i];
#endif

   try {
      project.printInfo(ELC_GREETING, ELC_MAJOR_VERSION, ELC_MINOR_VERSION);

      if (argc < 2) {
         // show help if no parameters proveded
         _tprintf(ELC_HELP_INFO);
         delete[] args;
#ifdef _UNICODE
         delete[] wide;
#endif
         return -3;
      }

      // Initializing..
      project.loadConfig(Path(project.appPath, DEFAULT_CONFIG), false);

      // Initializing..
      for (int i = 1 ; i < argc ; i++) {
         const TCHAR* arg = args[i];

         if (arg[0] == '-') {
            project.setOption(arg + 1);
         }
         else project.addSource(arg);
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

      // Linking is not available on this platform yet -- see the note above.
      if (project.IntSetting(opSystemType) != ptLibrary) {
         project.printInfo(_T("Linking is not supported on this platform yet; ")
                           _T("stopping after byte code generation."));
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

   delete[] args;
#ifdef _UNICODE
   delete[] wide;
#endif
   return exitCode;
}
