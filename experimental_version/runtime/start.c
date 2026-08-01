/*---------------------------------------------------------------------------
 *      S E L E N E   P r o j e c t:  hosted startup
 *
 *      What $package'elena'startup was in 2009 assembly: evaluate the
 *      program symbol, send it `proceed`, and turn the outcome into an exit
 *      code. The program is reached through the fixed name `selene.program`,
 *      a thunk the translator emits from the 'program forward -- the runtime
 *      never knows a program's actual name.
 *
 *      Hosted only: a kernel image replaces this file with its own entry.
 *      Sits in the static library, so it links in exactly when nothing else
 *      defines main().
 *--------------------------------------------------------------------------*/

#include "selene.h"

extern const selene_region_source selene_hosted_region;

extern selene_result selene_program(void* self, void* argument)
   __asm__("selene.program");

extern char selene_nil[] __asm__("selene.const.$elena.$nil");

int main(void)
{
   selene_platform_init();
   selene_runtime_init(&selene_hosted_region, 16u << 20);

   selene_result program = selene_program(NULL, selene_nil);

   selene_result run = selene_succeeded(program)
      ? selene_send(selene_value(program), selene_nil, SELENE_MESSAGE_PROCEED)
      : program;

   selene_runtime_shutdown();

   return selene_succeeded(run) ? 0 : 1;
}
