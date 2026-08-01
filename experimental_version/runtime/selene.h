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
 * LOGICALLY the result is {value, ok}. PHYSICALLY it is one machine word:
 * objects are slot-aligned, so bit 0 is free to carry the flag, and a
 * one-word result returns in a register under EVERY C ABI -- the Microsoft
 * x64 convention returns two-word structs through a hidden pointer, which
 * silently broke every call between C and generated code on Windows. When
 * fail: gains an argument the reason travels in the value bits with the
 * flag clear: nothing about this layout changes.
 *
 * This must match %selene.result in the code generator exactly.
 *   docs/plan/23-failure-abi.md
 *--------------------------------------------------------------------------*/

typedef selene_uword selene_result;

static inline void* selene_value(selene_result result)
{
   return (void*)(result & ~(selene_uword)1);
}

static inline bool selene_succeeded(selene_result result)
{
   return (result & 1) != 0;
}

static inline selene_result selene_ok(void* value)
{
   return (selene_uword)value | 1u;
}

static inline selene_result selene_failed(void* reason)
{
   /* `reason` is null today; when fail: gains an argument it rides in the
    * value bits, flag clear. */
   return (selene_uword)reason;
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

/* The predefined message the startup sends to the program object. Predefined
 * ids are global by construction -- they never pass through the per-link
 * message interning. */
#define SELENE_MESSAGE_PROCEED  ((selene_message)0x80000003u)

/* Upper bound on a VMT scan. Not a real limit on class size -- it is a guard so
 * a malformed or stale table fails the message instead of walking memory. */
#define SELENE_MAX_VMT_ENTRIES  4096
#define SELENE_MESSAGE_ANY      ((selene_message)0)

/* VMT header flag: this table belongs to a role, and its parent slot names the
 * owning class rather than a superclass. Value inherited from the 2009 layout
 * (elRoleVMT, elenaconst.h). */
#define SELENE_VMT_ROLE         ((selene_uword)0x10)

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

/* One call before anything runs, implemented by the platform directory --
 * where the console code page is set to UTF-8 on Windows, and nothing at all
 * happens on POSIX. The startup calls it; embedders must too. */
void  selene_platform_init(void);

void* selene_alloc(size_t slots);
void* selene_create(uint32_t slots, void* vmt);

/* The message parameter travels WITH the send: the byte code passes it on the
 * evaluation stack and the method prologue (sprepparam) adopts it as local #1.
 * A dispatcher that drops it hands every method a null argument -- which is
 * exactly what the first translation did. */
selene_result selene_send(void* receiver, void* param, selene_message message);
selene_result selene_send_static(void* receiver, void* param,
                                 selene_message message, void* vmt);

/* Re-send the message currently in flight to another receiver. The in-flight
 * message id is dispatcher state, not part of the method ABI: prepredir kept
 * it in a frame slot on x86, here selene_send records it before calling in.
 * Single-threaded until the MTA work lands. */
selene_result selene_redirect(void* target, void* param);
selene_result selene_redirect_super(void* target, void* param, void* vmt);

void selene_barrier(void* field, void* value);

/* Roles ("shift" technology): install role VMT `index` from the class's role
 * table into the live object's header, and put the owner class VMT back.
 * This is why a VMT load at a send site can never be treated as invariant. */
void selene_shift(void* object, uint32_t index);
void selene_unshift(void* object);

#endif /* SELENE_H */
