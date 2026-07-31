//---------------------------------------------------------------------------
//      E L E N A   P r o j e c t:  POSIX compatibility layer
//
//      Minimal <io.h> replacement. The MSVC header supplies low-level file
//      descriptor routines; the codebase uses only the access() family, which
//      lives in <unistd.h> on POSIX.
//
//      Paths go through Utf8Path for separator normalization -- see the note
//      in direct.h.
//---------------------------------------------------------------------------

#ifndef posix_ioH
#define posix_ioH 1

#include <tchar.h>

#include <unistd.h>
#include <sys/stat.h>

#ifndef F_OK
#define F_OK 0
#endif

inline int _access_(const char* path, int mode)
{
   _elena_posix_::Utf8Path p(path);

   return access(p, mode);
}

#define _access _access_

#endif // posix_ioH
