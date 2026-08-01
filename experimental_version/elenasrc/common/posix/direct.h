//---------------------------------------------------------------------------
//      E L E N A   P r o j e c t:  POSIX compatibility layer
//
//      Minimal <direct.h> replacement. The MSVC header supplies directory
//      routines; the POSIX equivalents live in <unistd.h> and <sys/stat.h>.
//
//      Paths go through Utf8Path so that Windows separators coming out of the
//      project's .cfg and .prj files are accepted here exactly as they are in
//      the wide-character routines. Without this the narrow (UTF-8) build fails
//      to create output directories while the wide build succeeds.
//---------------------------------------------------------------------------

#ifndef posix_directH
#define posix_directH 1

#include <tchar.h>

#include <unistd.h>
#include <sys/stat.h>

inline int _mkdir(const char* path)
{
   _elena_posix_::Utf8Path p(path);

   return mkdir(p, 0755);
}

inline int _rmdir_(const char* path)
{
   _elena_posix_::Utf8Path p(path);

   return rmdir(p);
}

#define _rmdir  _rmdir_
#define _chdir  chdir
#define _getcwd getcwd

#endif // posix_directH
