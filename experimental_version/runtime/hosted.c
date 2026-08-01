/*---------------------------------------------------------------------------
 *      S E L E N E   P r o j e c t:  hosted platform layer
 *
 *      The only file in the runtime that assumes an operating system exists.
 *      A freestanding build (a kernel) replaces it and links nothing else
 *      differently -- which is the point of keeping the heap behind a region
 *      source rather than calling the OS from the allocator.
 *--------------------------------------------------------------------------*/

#include "selene.h"

#include <stdlib.h>
#include <stdio.h>

static void* host_acquire(size_t bytes)
{
   return malloc(bytes);
}

static void host_release(void* address, size_t bytes)
{
   (void)bytes;
   free(address);
}

const selene_region_source selene_hosted_region = { host_acquire, host_release };

/* Where the weak link-time stubs land: a native or symbol nothing defined
 * was reached anyway. Reported loudly, failed cleanly -- the message-failure
 * protocol turns it into an ordinary failure at the call site. */
void selene_unimplemented(const char* name)
{
   fprintf(stderr, "[selene] unimplemented: %s\n", name);
}
