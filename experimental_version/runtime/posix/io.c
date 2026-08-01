/*---------------------------------------------------------------------------
 *      S E L E N E   P r o j e c t:  POSIX console bindings
 *
 *      The C bodies of the $package'posix natives that src/posix/io.sel
 *      reaches through #external. This file is the ONLY place these
 *      routines know they are on POSIX: the text logic (escape expansion,
 *      literal layout) is shared from natives.c, and the runtime text
 *      contract is UTF-8 -- which a POSIX descriptor takes as it is, so no
 *      conversion happens here at all. The Windows twin (win32/io.c) will
 *      convert to UTF-16 at its console boundary and nowhere else.
 *        docs/plan/19-runtime-in-c.md section 8.1
 *--------------------------------------------------------------------------*/

#include "../natives.h"

#include <unistd.h>

void selene_platform_init(void)
{
   /* Nothing: a POSIX terminal already speaks the runtime's UTF-8. */
}

static bool stdout_sink(const char* bytes, size_t length, void* context)
{
   (void)context;

   while (length > 0) {
      ssize_t written = write(1, bytes, length);
      if (written <= 0)
         return false;

      bytes  += written;
      length -= (size_t)written;
   }
   return true;
}

SELENE_NATIVE(posix_writelit, "selene.native.$package.posix.writelit")
{
   (void)first;

   if (!selene_write_expanded(second, stdout_sink, NULL))
      return selene_failed(NULL);

   return selene_ok(second);
}

SELENE_NATIVE(posix_readkey, "selene.native.$package.posix.readkey")
{
   (void)first;

   /* Wait for one byte -- the 2009 console's "press any key". End of input
    * counts as a key: a piped run must terminate, not spin. */
   char key;
   (void)read(0, &key, 1);

   return selene_ok(second);
}
