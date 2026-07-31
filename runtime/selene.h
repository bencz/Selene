/*---------------------------------------------------------------------------
 *      S E L E N E   P r o j e c t:  runtime
 *
 *      The object model and the entry points generated code calls.
 *
 *      Written in C11 rather than C++ deliberately: the runtime has to be
 *      linkable into a kernel image with -ffreestanding -nostdlib, and C++
 *      would drag in an unwinder, a personality routine, RTTI tables and
 *      static-initialisation order. See docs/plan/19-runtime-in-c.md.
 *
 *      Compiled to bitcode and linked with LTO, so these functions inline into
 *      generated code exactly where the 2009 assembly pasted blobs by hand --
 *      but under a cost model rather than unconditionally.
 *--------------------------------------------------------------------------*/

#ifndef SELENE_H
#define SELENE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*---------------------------------------------------------------------------
 * Word size
 *
 * The runtime is compiled FOR its target, so here -- and only here -- the
 * host's pointer width is the target's. The compiler may never make this
 * assumption; see elenasrc/engine/targetinfo.h.
 *--------------------------------------------------------------------------*/

typedef intptr_t  selene_word;
typedef uintptr_t selene_uword;

#define SELENE_SLOT_BYTES ((size_t)sizeof(void*))

/*---------------------------------------------------------------------------
 * Objects
 *
 *   [obj - 2 slots]  size in slots, plus flags in the top bits
 *   [obj - 1 slot ]  VMT pointer
 *   [obj + 0      ]  fields, addressed 1-based by the byte code
 *
 * The header sits BELOW the object pointer so field access needs no offset,
 * which is what made the 2009 dispatch sequence short. That property is worth
 * keeping.
 *--------------------------------------------------------------------------*/

#define SELENE_FLAG_BINARY    ((selene_uword)1 << (sizeof(selene_uword) * 8 - 1))
#define SELENE_FLAG_COLLECTED ((selene_uword)1 << (sizeof(selene_uword) * 8 - 2))
#define SELENE_SIZE_MASK      (~(SELENE_FLAG_BINARY | SELENE_FLAG_COLLECTED))

typedef struct selene_vmt selene_vmt;

/* A message identifier. Predefined messages have the high bit set so they sort
 * before user messages in the signed ordering the VMT uses. */
typedef uint32_t selene_message;

/*---------------------------------------------------------------------------
 * The calling convention
 *
 * Every method returns a value and whether the message succeeded. Failure is
 * Selene's primary conditional -- #if and #loop are built on it -- so it is a
 * flag to branch on, never an unwind.
 *
 * This must match %selene.result in the code generator exactly.
 *   docs/plan/23-failure-abi.md
 *--------------------------------------------------------------------------*/

typedef struct {
   void* value;
   bool  ok;
} selene_result;

static inline selene_result selene_ok(void* value)
{
   return (selene_result){ value, true };
}

static inline selene_result selene_failed(void* reason)
{
   /* `reason` is null today. When fail: gains an argument it carries it, with
    * no change to this layout -- which is most of why {ptr, i1} was chosen. */
   return (selene_result){ reason, false };
}

/*---------------------------------------------------------------------------
 * VMT
 *
 *   [vmt - 3 slots]  role table
 *   [vmt - 2 slots]  flags
 *   [vmt - 1 slot ]  parent, or the any-message handler chain
 *   [vmt + 0      ]  entries, sorted by SIGNED message id, terminated by
 *                    SELENE_MESSAGE_TERMINAL
 *--------------------------------------------------------------------------*/

#define SELENE_MESSAGE_TERMINAL ((selene_message)0x7FFFFFFF)
#define SELENE_MESSAGE_ANY      ((selene_message)0)

typedef selene_result (*selene_method)(void* self, void* argument);

typedef struct {
   selene_message message;
   selene_method  method;
} selene_vmt_entry;

/* --- object accessors --- */

static inline selene_uword* selene_header(void* object)
{
   return (selene_uword*)object - 2;
}

static inline selene_vmt_entry* selene_vmt_of(void* object)
{
   return *((selene_vmt_entry**)object - 1);
}

static inline void selene_set_vmt(void* object, selene_vmt_entry* vmt)
{
   *((selene_vmt_entry**)object - 1) = vmt;
}

static inline size_t selene_size_of(void* object)
{
   return (size_t)(*selene_header(object) & SELENE_SIZE_MASK);
}

static inline bool selene_is_binary(void* object)
{
   return (*selene_header(object) & SELENE_FLAG_BINARY) != 0;
}

/*---------------------------------------------------------------------------
 * Entry points called by generated code
 *
 * Named symbols, never numbers. A wrong name is a link error; a wrong number
 * was a silent call to the wrong routine, which is what the 2009 scheme gave.
 *   docs/plan/19-runtime-in-c.md section 3.1
 *--------------------------------------------------------------------------*/

/* Where the heap comes from. A hosted build supplies an mmap-backed source; a
 * kernel supplies its own physical page allocator. This is what stops the heap
 * from being HeapAlloc, which is OS-development blocker 3. */
typedef struct {
   void* (*acquire)(size_t bytes);
   void  (*release)(void* address, size_t bytes);
} selene_region_source;

void  selene_runtime_init(const selene_region_source* source, size_t heap_bytes);
void  selene_runtime_shutdown(void);

void* selene_alloc(size_t slots);
void* selene_create(uint32_t slots, void* vmt);

selene_result selene_send(void* receiver, selene_message message);
selene_result selene_send_static(void* receiver, selene_message message, void* vmt);
selene_result selene_redirect(void* receiver);

void selene_barrier(void* field, void* value);
void selene_shift(void* object, void* role);

#endif /* SELENE_H */
