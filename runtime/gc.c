/*---------------------------------------------------------------------------
 *      S E L E N E   P r o j e c t:  allocation
 *
 *      STATUS: bump allocator, no collection yet.
 *
 *      That is deliberate and it is stated rather than hidden. A collector
 *      needs precise roots, and precise roots need the shadow stack, which
 *      needs the code generator to emit frame push/pop. None of that exists
 *      yet. A bump allocator lets generated code RUN, which is what makes
 *      everything after it verifiable.
 *
 *      What the 2009 collector did and why it cannot simply be translated:
 *
 *        - it scanned published stack-frame extents CONSERVATIVELY, treating
 *          every word in range as a possible pointer, and then MOVED objects.
 *          A false positive therefore corrupts data rather than merely
 *          retaining garbage. That is a correctness bug, not imprecision, and
 *          it is a strong candidate for knownbugs.txt #00024.
 *        - it had exactly ONE safepoint, inside the allocator.
 *        - its heap came from HeapAlloc, once, and never grew.
 *        - the bump pointer was a global with non-atomic read-modify-write,
 *          which is one of the reasons threads were impossible.
 *
 *      The replacement is a shadow stack for precise roots, per-thread
 *      allocation buffers, and safepoint polls at back edges.
 *        docs/plan/19-runtime-in-c.md section 3, docs/plan/20-os-development.md
 *--------------------------------------------------------------------------*/

#include "selene.h"

static const selene_region_source* _source;

static char*  _heap;
static char*  _bump;
static char*  _limit;
static size_t _heap_bytes;

void selene_runtime_init(const selene_region_source* source, size_t heap_bytes)
{
   _source     = source;
   _heap_bytes = heap_bytes;
   _heap       = (char*)source->acquire(heap_bytes);
   _bump       = _heap;
   _limit      = _heap + heap_bytes;
}

void selene_runtime_shutdown(void)
{
   if (_source && _heap)
      _source->release(_heap, _heap_bytes);

   _heap = _bump = _limit = NULL;
}

/*---------------------------------------------------------------------------
 * Allocation
 *
 * The header occupies two slots below the returned pointer, so field access
 * needs no offset. Objects are aligned to twice the slot size, matching the
 * alignment the object model assumes.
 *--------------------------------------------------------------------------*/
void* selene_alloc(size_t slots)
{
   const size_t align  = SELENE_SLOT_BYTES * 2;
   const size_t header = SELENE_SLOT_BYTES * 2;

   size_t bytes = header + slots * SELENE_SLOT_BYTES;
   bytes = (bytes + align - 1) & ~(align - 1);

   if (_bump + bytes > _limit) {
      /* No collector yet, so exhaustion is terminal rather than a trigger.
       * When the collector lands this becomes the one safepoint that already
       * existed, plus the polls that did not. */
      return NULL;
   }

   char* raw = _bump;
   _bump += bytes;

   void* object = raw + header;

   *selene_header(object) = (selene_uword)slots;
   selene_set_vmt(object, NULL);

   /* Fields start as null. The 2009 allocator left them uninitialised, which
    * is only safe while the collector is conservative -- a precise collector
    * reading an uninitialised slot as a root would follow garbage. */
   selene_uword* fields = (selene_uword*)object;
   for (size_t i = 0 ; i < slots ; i++)
      fields[i] = 0;

   return object;
}

void* selene_create(uint32_t slots, void* vmt)
{
   void* object = selene_alloc(slots);
   if (object)
      selene_set_vmt(object, (selene_vmt_entry*)vmt);

   return object;
}

/*---------------------------------------------------------------------------
 * Write barrier
 *
 * A plain store today, because there are no generations to record edges
 * between. It exists as a named entry point now so generated code already
 * routes through it: retrofitting a barrier into code that stores directly
 * would mean regenerating everything.
 *--------------------------------------------------------------------------*/
void selene_barrier(void* field, void* value)
{
   *(void**)field = value;
}

/*---------------------------------------------------------------------------
 * Roles ("shift")
 *
 * Rewrites a live object's VMT pointer. No allocation, and identity is
 * preserved -- the object stays the same object.
 *
 * The byte code names the role by INDEX into the class's role table, which
 * hangs off the VMT header. When the object is already inside a role, the
 * table is the owning class's -- roles do not nest.
 *
 * This is exactly why a send site may not cache the VMT, and why the load of
 * the VMT slot must never be marked invariant.
 *--------------------------------------------------------------------------*/

/* VMT header slots, below the entries: [-3] role table, [-2] flags,
 * [-1] parent (or owner, for a role). Mirrors the accessors in dispatch.c. */
static inline selene_uword vmt_flags(selene_vmt_entry* vmt)
{
   return *((selene_uword*)vmt - 2);
}

static inline selene_vmt_entry* vmt_parent(selene_vmt_entry* vmt)
{
   return *((selene_vmt_entry**)vmt - 1);
}

static inline selene_vmt_entry** vmt_role_table(selene_vmt_entry* vmt)
{
   return *((selene_vmt_entry***)vmt - 3);
}

void selene_shift(void* object, uint32_t index)
{
   if (!object)
      return;

   selene_vmt_entry* vmt = selene_vmt_of(object);
   if (vmt && (vmt_flags(vmt) & SELENE_VMT_ROLE))
      vmt = vmt_parent(vmt);                    /* already in a role: its owner */

   selene_vmt_entry** table = vmt ? vmt_role_table(vmt) : NULL;
   if (table && table[index])
      selene_set_vmt(object, table[index]);
}

void selene_unshift(void* object)
{
   if (!object)
      return;

   selene_vmt_entry* vmt = selene_vmt_of(object);
   if (vmt && (vmt_flags(vmt) & SELENE_VMT_ROLE))
      selene_set_vmt(object, vmt_parent(vmt));
}
