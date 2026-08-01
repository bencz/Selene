/*---------------------------------------------------------------------------
 *      S E L E N E   P r o j e c t:  message dispatch
 *
 *      The 2009 assembly walked the VMT linearly, restarting one chain link
 *      along on overshoot. Entries are SORTED by signed message id and always
 *      were -- so binary search is available with no layout change at all.
 *      That is the one free improvement in the whole runtime.
 *
 *      What it is NOT is the answer to dispatch cost. The assembly scan was
 *      inlined at every send site with zero call overhead, so a plain C
 *      elena_send() is slower than 2009 even with a better algorithm. Inline
 *      caches at the call site are what close that gap; they are the code
 *      generator's job, not this file's.
 *        docs/plan/17-llvm-backend-and-targets.md section 5
 *--------------------------------------------------------------------------*/

#include "selene.h"

/* Message ids are compared as SIGNED values: predefined messages are
 * 0x8000000X, which is negative, so they sort before user messages. The 2009
 * VMT layout depends on this and so does the search. */
static inline int32_t as_signed(selene_message message)
{
   return (int32_t)message;
}

/*---------------------------------------------------------------------------
 * Look a message up in one VMT.
 *
 * Returns NULL when the VMT does not answer it, leaving the caller to walk the
 * parent chain -- which is also where an any-message handler (annex) is found.
 *--------------------------------------------------------------------------*/
static selene_method lookup(selene_vmt_entry* vmt, selene_message message)
{
   if (!vmt)
      return NULL;

   const int32_t wanted = as_signed(message);

   /* The table is terminated rather than counted, so find the end first.
    *
    * Bounded: an unterminated or bogus table would otherwise walk memory until
    * it faulted. A dispatcher that cannot trust its input is worth more than a
    * marginally shorter loop -- generated code reaching here with a stale or
    * mis-derived receiver should fail the message, not crash the program. */
   size_t high = 0;
   while (as_signed(vmt[high].message) != (int32_t)SELENE_MESSAGE_TERMINAL) {
      if (++high > SELENE_MAX_VMT_ENTRIES)
         return NULL;
   }

   size_t low = 0;
   while (low < high) {
      size_t middle = low + (high - low) / 2;
      int32_t here  = as_signed(vmt[middle].message);

      if (here == wanted)
         return vmt[middle].method;

      if (here < wanted) {
         low = middle + 1;
      }
      else high = middle;
   }

   return NULL;
}

/* The chain link a VMT uses for its parent, or for an any-message handler. */
static inline selene_vmt_entry* parent_of(selene_vmt_entry* vmt)
{
   return vmt ? *((selene_vmt_entry**)vmt - 1) : NULL;
}

/* The message currently being dispatched. Redirect methods re-send it to a
 * different receiver; on x86 the prepredir prologue parked it in a frame slot,
 * here it is dispatcher state. Plain static because the runtime is
 * single-threaded until the MTA work lands. */
static selene_message in_flight;

selene_result selene_send(void* receiver, void* param, selene_message message)
{
   if (!receiver)
      return selene_failed(NULL);

   /* Read the VMT on every send.
    *
    * It cannot be cached across anything that might shift: #shift rewrites a
    * live object's VMT pointer, which is the mechanism roles are built on. Any
    * optimisation assuming an object's class is fixed is unsound in Selene.
    *   docs/plan/17-llvm-backend-and-targets.md section 5.2 */
   selene_vmt_entry* vmt = selene_vmt_of(receiver);

   while (vmt) {
      selene_method method = lookup(vmt, message);
      if (!method) {
         /* Not found here: an any-message handler answers everything. */
         method = lookup(vmt, SELENE_MESSAGE_ANY);
      }

      if (method) {
         in_flight = message;
         return method(receiver, param);
      }

      vmt = parent_of(vmt);
   }

   /* Message not understood. Indistinguishable from a method that returned a
    * failure -- which is the defect the tagged-failure language change fixes.
    *   docs/plan/16-syntax-evolution.md section S1 */
   return selene_failed(NULL);
}

selene_result selene_send_static(void* receiver, void* param,
                                 selene_message message, void* vmt)
{
   /* Static dispatch: the class is known at the call site, so the search starts
    * from a named VMT instead of the receiver's own. This is how super sends
    * and directly-typed calls avoid re-resolving. */
   selene_method method = lookup((selene_vmt_entry*)vmt, message);
   if (method) {
      in_flight = message;
      return method(receiver, param);
   }

   return selene_failed(NULL);
}

selene_result selene_redirect(void* target, void* param)
{
   /* Save before re-sending: the send below overwrites in_flight. */
   selene_message message = in_flight;

   return selene_send(target, param, message);
}

selene_result selene_redirect_super(void* target, void* param, void* vmt)
{
   selene_message message = in_flight;

   return selene_send_static(target, param, message, vmt);
}
