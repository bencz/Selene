/*---------------------------------------------------------------------------
 *      S E L E N E   P r o j e c t:  native procedure internals
 *
 *      Shared between the generic natives (natives.c) and the per-platform
 *      binding files (posix/io.c, win32/io.c). INTERNAL to the runtime --
 *      the public ABI is selene.h; nothing here is visible to generated code
 *      beyond the symbol names the SELENE_NATIVE definitions export.
 *
 *      Calling convention (docs/plan/23-failure-abi.md):
 *        embedded blobs (rcallemb): (receiver, argument) -> {value, ok}
 *        externals (rcallext):      (first-pushed, last-pushed) -> {_, ok};
 *                                   the stack is left alone, results travel
 *                                   through the argument objects.
 *--------------------------------------------------------------------------*/

#ifndef SELENE_NATIVES_H
#define SELENE_NATIVES_H

#include "selene.h"

/* A native's C name is private; its SYMBOL is the resolved reference name
 * (apostrophes as dots -- see llvmgen.h), so generated code links against it
 * with no table in between. */
#define SELENE_NATIVE(cname, symbol) \
   selene_result cname(void* first, void* second) __asm__(symbol); \
   selene_result cname(void* first, void* second)

/*---------------------------------------------------------------------------
 * Text
 *
 * All runtime text is UTF-8 -- the same invariant the compiler build holds,
 * extended to the object model. A literal object's payload is:
 *
 *    [u32 length in BYTES][UTF-8 bytes][NUL]
 *
 * This is the layout the code generator materialises for constants
 * (llvmgen.cpp, valueConstant) -- the two must not drift. Byte-oriented
 * UTF-8 is also what makes the layout identical on the big-endian targets.
 * Platform code converts ONLY at the OS boundary (a Windows console wants
 * UTF-16 at the WriteConsoleW call; POSIX descriptors take the bytes as
 * they are). See docs/plan/19-runtime-in-c.md section 8.1.
 *--------------------------------------------------------------------------*/

static inline uint32_t selene_literal_length(void* object)
{
   return *(uint32_t*)object;
}

static inline const char* selene_literal_bytes(void* object)
{
   return (const char*)object + sizeof(uint32_t);
}

/* Where expanded text goes: a run of UTF-8 bytes to a platform-owned
 * destination. Returns false on failure. */
typedef bool (*selene_byte_sink)(const char* bytes, size_t length, void* context);

/* Writes a literal through `sink`, expanding the 2009 control escapes
 * ('%n' is a newline). Text logic is generic; only the sink knows about
 * descriptors, consoles or encodings beyond UTF-8. */
bool selene_write_expanded(void* literal, selene_byte_sink sink, void* context);

#endif /* SELENE_NATIVES_H */
