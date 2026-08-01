/*---------------------------------------------------------------------------
 *      S E L E N E   P r o j e c t:  native procedures (generic)
 *
 *      The C bodies of the routines byte code reaches through rcallemb and
 *      rcallext. In 2009 these were x86 blobs pasted inline; here each is an
 *      ordinary function whose SYMBOL NAME is the resolved reference name,
 *      so generated code links against it with no table in between.
 *
 *      ONLY platform-free code lives here: the object-model natives of
 *      $package'elena, the text logic they share, and -- as they are
 *      implemented -- the $package'standard value natives. Anything that
 *      touches an OS belongs in the per-platform directory (posix/io.c,
 *      win32/io.c); the build selects the file, the code never asks where
 *      it is running.
 *
 *      Everything not yet implemented is generated as a loud failing stub
 *      at link time.
 *--------------------------------------------------------------------------*/

#include "natives.h"

/*---------------------------------------------------------------------------
 * Shared text logic
 *
 * Escape expansion is language semantics, not platform behaviour, so every
 * platform's console gets it from here rather than reimplementing it.
 *--------------------------------------------------------------------------*/

bool selene_write_expanded(void* literal, selene_byte_sink sink, void* context)
{
   if (!literal)
      return false;

   const char* text   = selene_literal_bytes(literal);
   uint32_t    length = selene_literal_length(literal);

   uint32_t at = 0;
   while (at < length) {
      uint32_t run = at;
      while (run < length && !(text[run] == '%' && run + 1 < length
                               && text[run + 1] == 'n'))
         run++;

      if (run > at && !sink(text + at, run - at, context))
         return false;

      if (run < length) {                       /* found "%n" */
         if (!sink("\n", 1, context))
            return false;
         run += 2;
      }
      at = run;
   }

   return true;
}

/* --- $package'elena -------------------------------------------------- */

SELENE_NATIVE(elena_identical, "selene.native.$package.elena.identical")
{
   return first == second ? selene_ok(first) : selene_failed(NULL);
}

SELENE_NATIVE(elena_vmtof, "selene.native.$package.elena.vmtof")
{
   (void)second;

   return first ? selene_ok((void*)selene_vmt_of(first)) : selene_failed(NULL);
}

SELENE_NATIVE(elena_sametype, "selene.native.$package.elena.sametype")
{
   if (first && second && selene_vmt_of(first) == selene_vmt_of(second))
      return selene_ok(first);

   return selene_failed(NULL);
}

SELENE_NATIVE(elena_assign, "selene.native.$package.elena.assign")
{
   /* The write-barrier tail the 2009 compiler appended after every field
    * assignment. The store itself is emitted by the code generator (with
    * selene_barrier); this records nothing further until the collector has
    * generations to care about. */
   (void)second;

   return selene_ok(first);
}
