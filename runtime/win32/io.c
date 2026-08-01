/*---------------------------------------------------------------------------
 *      S E L E N E   P r o j e c t:  Windows console bindings
 *
 *      The C bodies of the $package'wincon natives that src/win32/console.sel
 *      reaches through #external. This file is the ONLY place these routines
 *      know they are on Windows.
 *
 *      Text stays UTF-8 END TO END: since Windows 10 1903 -- Selene's
 *      Windows baseline -- the console renders UTF-8 directly once the code
 *      pages say so, which selene_platform_init does exactly once. WriteFile
 *      then takes the runtime's bytes as they are (Chinese, Russian,
 *      Portuguese included), and redirected output gets clean UTF-8 with no
 *      BOM. Converting to UTF-16 is NOT policy; it is a per-call exception
 *      reserved for APIs that leave no choice (console input on builds where
 *      the -A read path is broken is the known case, handled when real line
 *      input lands). See docs/plan/19-runtime-in-c.md section 8.1.
 *--------------------------------------------------------------------------*/

#include "../natives.h"

#include <windows.h>

void selene_platform_init(void)
{
   SetConsoleOutputCP(CP_UTF8);
   SetConsoleCP(CP_UTF8);
}

static bool stdout_sink(const char* bytes, size_t length, void* context)
{
   (void)context;

   HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
   while (length > 0) {
      DWORD written = 0;
      if (!WriteFile(out, bytes, (DWORD)length, &written, NULL) || written == 0)
         return false;

      bytes  += written;
      length -= written;
   }
   return true;
}

SELENE_NATIVE(wincon_writelit, "selene.native.$package.wincon.writelit")
{
   (void)first;

   if (!selene_write_expanded(second, stdout_sink, NULL))
      return selene_failed(NULL);

   return selene_ok(second);
}

SELENE_NATIVE(wincon_readkey, "selene.native.$package.wincon.readkey")
{
   (void)first;

   /* Wait for one byte -- the 2009 console's "press any key". End of input
    * counts as a key: a piped run must terminate, not spin. */
   char  key;
   DWORD got = 0;
   (void)ReadFile(GetStdHandle(STD_INPUT_HANDLE), &key, 1, &got, NULL);

   return selene_ok(second);
}
