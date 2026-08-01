//---------------------------------------------------------------------------
//      E L E N A   P r o j e c t:  C runtime
//
//      Phase-P2 bring-up runtime: object model, bump allocator, hook chain
//      and the program harness. Dispatch and the coreapi surface arrive
//      incrementally; anything not implemented yet fails LOUDLY through
//      elena_unimplemented -- a missing capability is a report, never a
//      silent wrong answer.
//
//      OBJECT MODEL (64-bit generalization of the 2015 layout)
//      ------------------------------------------------------
//         [obj - 2 words]  size/flags: negative = binary byte length,
//                          positive = field count in slots
//         [obj - 1 word ]  VMT pointer
//         [obj + 0      ]  body
//---------------------------------------------------------------------------

#include <setjmp.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef intptr_t word;

#define HEADER_WORDS 2

// --- diagnostics -----------------------------------------------------------

void elena_unimplemented(const char* name)
{
   fprintf(stderr, "elena runtime: '%s' is not implemented yet\n", name);
   abort();
}

// --- allocator (bump; the generational collector is a later phase) ---------

static char*  heap_next;
static char*  heap_end;

static void heap_init(size_t bytes)
{
   heap_next = (char*)malloc(bytes);
   heap_end = heap_next ? heap_next + bytes : NULL;
   if (!heap_next) {
      fprintf(stderr, "elena runtime: cannot allocate the heap\n");
      abort();
   }
}

static void* allocate(void* vmt, word sizeField, size_t bodyBytes)
{
   size_t total = (HEADER_WORDS * sizeof(word) + bodyBytes + 15) & ~(size_t)15;
   if (heap_next + total > heap_end) {
      fprintf(stderr, "elena runtime: out of memory\n");
      abort();
   }
   word* header = (word*)heap_next;
   heap_next += total;

   void* object = header + HEADER_WORDS;
   header[0] = sizeField;
   header[1] = (word)vmt;
   memset(object, 0, bodyBytes);
   return object;
}

void* elena_new(void* vmt, word fields)
{
   return allocate(vmt, fields, (size_t)fields * sizeof(word));
}

void* elena_newbinary(void* vmt, word bytes)
{
   return allocate(vmt, -bytes, (size_t)bytes);
}

// --- object introspection --------------------------------------------------

static word size_field(void* object) { return ((word*)object)[-2]; }

word elena_length(void* object, word mode)
{
   word size = size_field(object);
   word bytes = size < 0 ? -size : size * (word)sizeof(word);
   switch (mode) {
      case 0x11: return size < 0 ? bytes / (word)sizeof(word) : size;  // len (slots)
      case 0x31: return bytes;                                         // blen
      case 0x32: return bytes / 2;                                     // wlen
      case 0x34: return bytes / 4;                                     // nlen
      case 0x30: return size;                                          // xlen (raw)
      case 0x33: elena_unimplemented("elena_length(flags)");           // flag
      default:   return bytes;
   }
   return 0;
}

void elena_validate(void* object)
{
   if (!object) {
      fprintf(stderr, "elena runtime: nil validation failed\n");
      abort();
   }
}

word elena_isheap(void* object)
{
   return ((char*)object >= heap_end - (heap_end - (char*)heap_next)
      && (char*)object < heap_end) ? 1 : 0;
}

void elena_setfield(void* object, word index, void* value)
{
   // the write barrier becomes real together with the collector
   ((word*)object)[index] = (word)value;
}

void elena_copy(void* target, void* source)
{
   word bytes = elena_length(target, 0x31);
   memcpy(target, source, (size_t)bytes);
}

// --- dispatch --------------------------------------------------------------
//
// A VMT is emitted by llvmgen as {word message, method fn} entries sorted
// ascending by interned message and terminated by an all-ones message;
// [-1] of the table is the class-class table, [-2] the flags, [-3] the
// entry count. An object's [-1] word is its VMT.

typedef void* (*elena_method)(void* self, word message, void* argframe);

struct vmt_entry
{
   word         message;
   elena_method fn;
};

static struct vmt_entry* vmt_of(void* object)
{
   return (struct vmt_entry*)((word*)object)[-1];
}

// acallvi: call by SLOT INDEX (0 = the dispatch method by sort order)
void* elena_send_vi(void* self, word message, void* argframe, word index)
{
   struct vmt_entry* table = vmt_of(self);
   if (!table || !table[index].fn) {
      fprintf(stderr, "elena runtime: empty dispatch table (message %lX)\n",
         (unsigned long)message);
      abort();
   }
   return table[index].fn(self, message, argframe);
}

// bsredirect: search by message; the table is merged, so no parent walk
void* elena_bsredirect(void* self, word message, void* argframe, char* found)
{
   struct vmt_entry* table = vmt_of(self);
   if (table) {
      for (struct vmt_entry* at = table ; at->message != (word)-1 ; at++) {
         if (at->message == message) {
            *found = 1;
            return at->fn(self, message, argframe);
         }
      }
   }
   *found = 0;
   if (getenv("ELENA_TRACE")) {
      fprintf(stderr, "bsredirect miss: message %lX, table:", (unsigned long)message);
      struct vmt_entry* at = vmt_of(self);
      for (int i = 0 ; at && at->message != (word)-1 && i < 8 ; at++, i++)
         fprintf(stderr, " %lX", (unsigned long)at->message);
      fprintf(stderr, "\n");
   }
   return NULL;
}

word elena_mindex(void* target, word message)
{
   (void)target; (void)message;
   elena_unimplemented("elena_mindex");
   return -1;
}

word elena_trylock(void* object)  { (void)object; return 1; }
void elena_freelock(void* object) { (void)object; }

// callextr reaches this until typed FFI (plan 18 / phase P4) lands: the
// import name travels with the call so a missing capability names itself
word elena_external_stub(const char* name)
{
   fprintf(stderr, "elena runtime: external '%s' needs the typed FFI (P4)\n", name);
   abort();
   return 0;
}

// --- exception chain -------------------------------------------------------
//
// The generated code allocates a 576-byte frame per hook site and calls
// _setjmp on frame+16; this layout is load-bearing on both sides.

struct hook_frame
{
   struct hook_frame* prev;
   char               padding[8];
   jmp_buf            buffer;
};

static struct hook_frame* hook_top;
static void*              current_exception;

void elena_hook_push(void* frame)
{
   struct hook_frame* hook = (struct hook_frame*)frame;
   hook->prev = hook_top;
   hook_top = hook;
}

void elena_unhook(void)
{
   if (hook_top)
      hook_top = hook_top->prev;
}

void* elena_current_exception(void)
{
   return current_exception;
}

void elena_throw(void* exception)
{
   current_exception = exception;
   if (!hook_top) {
      fprintf(stderr, "elena runtime: unhandled exception\n");
      abort();
   }
   struct hook_frame* hook = hook_top;
   hook_top = hook->prev;
   longjmp(hook->buffer, 1);
}

// --- program harness -------------------------------------------------------
//
// llvmgen emits elena_program as a thunk around the project's 'program
// symbol. Until the library console lands (typed FFI), the harness prints
// a returned literal object itself: the pipeline under test is
// source -> elc -> .nl -> LLVM -> .o -> this runtime.

extern void* elena_program(void);

int main(void)
{
   heap_init(16u << 20);

   void* result = elena_program();

   if (result && size_field(result) < 0) {
      // a binary object: print it as UTF-8 text
      fwrite(result, 1, strlen((const char*)result), stdout);
      fputc('\n', stdout);
   }
   else if (result) {
      printf("(object with %ld fields)\n", (long)size_field(result));
   }
   else printf("(nil)\n");

   return 0;
}
