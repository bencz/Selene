//---------------------------------------------------------------------------
//      S E L E N E   P r o j e c t:  module dumper / disassembler
//
//      Reads a compiled .sem module and prints its header, symbol tables and
//      disassembled byte code.
//
//      Two reasons this exists:
//
//        1. Nothing could inspect what the compiler emits. Every format change
//           so far had to be verified indirectly, by observing whether a later
//           stage failed -- which is how a desynchronized stream was mistaken
//           for a broken grammar and for a missing class.
//
//        2. It is the front half of the LLVM translator. Decoding a section
//           into (opcode, arguments) is the same work whether the consumer is a
//           printer or an IR builder.
//---------------------------------------------------------------------------

#include "elena.h"
// --------------------------------------------------------------------------
#include "module.h"
#include "bytecode.h"

#include <stdio.h>

using namespace _ELENA_;

// --- helpers ---------------------------------------------------------------

static void printNarrow(const TCHAR* s)
{
   if (!s) { printf("<null>"); return; }

   for (const TCHAR* p = s ; *p ; p++) {
      unsigned int c =
         (unsigned int)((unsigned long long)(*p) & ((1ULL << (8 * sizeof(TCHAR))) - 1));

      putchar(c < 0x80 ? (int)c : '?');
   }
}

static const char* maskName(ref_t mask)
{
   switch (mask) {
      case mskSymbolRef:   return "symbol code";
      case mskClassRef:    return "class code";
      case mskVMTRef:      return "class VMT";
      case mskMetaDataRef: return "meta data";
      case mskCodeRef:     return "code";
      case mskDataRef:     return "data";
      case mskStaticRef:   return "static";
      default:             return "?";
   }
}

// --- disassembler ----------------------------------------------------------

static void disassemble(Section* section)
{
   DumpReader reader(section);

   // A code section is a SEQUENCE of length-prefixed procedures: a symbol
   // section holds one, a class section holds every method back to back
   // (saveVMT calls saveProcedure per method). Stopping after the first
   // procedure hid all but one method of every class.
   while (reader.Position() + 4 <= section->Length()) {
      unsigned int codeSize = reader.getU32LE();
      size_t endPos = reader.Position() + codeSize;

      printf("      code size: %u bytes\n", codeSize);

      if (codeSize == 0 || endPos > section->Length()) {
         printf("      !! declared size exceeds the section (%u > %u)\n",
                (unsigned int)endPos, (unsigned int)section->Length());
         return;
      }

      size_t base = reader.Position();
      while (reader.Position() < endPos) {
         size_t offset = reader.Position() - base;

         unsigned char code = 0;
         if (!reader.read(&code, 1))
            break;

         const char* name = getByteCodeName(code);
         int args = getByteCodeArgCount(code);

         printf("      %06X  %02X  %-12s", (unsigned int)offset, code,
                name ? name : "???");

         if (args >= 1) printf(" %-11d", (int)reader.getU32LE());
         if (args >= 2) printf(" %d", (int)reader.getU32LE());

         // Static dispatch carries a third operand -- the VMT to search --
         // that the argument-count rule does not describe. Skipping it here
         // would misalign every byte after an ircall.
         if (code == (unsigned char)bcIRCall0 || code == (unsigned char)bcIRCall1)
            printf("  vmt:%08X", reader.getU32LE());

         if (!name)
            printf("   <- unknown opcode");

         printf("\n");
      }
   }
}

// --- main ------------------------------------------------------------------

int main(int argc, char* argv[])
{
   if (argc < 2) {
      printf("semdump <module.sem> [-c]\n");
      printf("  -c   disassemble code sections\n");
      return 1;
   }

   bool withCode = (argc > 2 && argv[2][0] == '-' && argv[2][1] == 'c');

   LocalString<0x200> path;
#ifdef _UNICODE
   path.convert(argv[1]);
#else
   path.copy(argv[1]);
#endif

   FileReader file(path, feRaw);
   if (!file.isOpened()) {
      printf("cannot open '%s'\n", argv[1]);
      return 1;
   }

   // --- raw header, before asking Module to validate it ---
   unsigned char header[MODULE_HEADER_SIZE];
   file.read(header, MODULE_HEADER_SIZE);
   file.seek(0);

   printf("file    : %s\n", argv[1]);
   printf("magic   : %.8s\n", (const char*)header);
   printf("format  : %d.%d\n", header[9], header[8]);
   printf("encoding: %d (1=utf8 2=utf16 3=utf32)\n", header[12]);
   printf("byte ord: %d (1=little 2=big)\n", header[13]);
   printf("word    : %d bits\n", header[14]);
   printf("\n");

   Module module;
   LoadResult result = module.load(file);
   if (result != lrSuccessful) {
      printf("load failed, result = %d\n", (int)result);
      return 1;
   }

   printf("module  : ");
   printNarrow(module.Name());
   printf("\n\n");

   // --- symbol tables ---
   // Resolution is by id, so probe upward until the ids run out.
   printf("references:\n");
   int count = 0;
   for (ref_t r = 1 ; r < 0x10000 ; r++) {
      const TCHAR* name = module.resolveReference(r);
      if (!name || !name[0]) {
         if (r > count + 8) break;      // tolerate small gaps
         continue;
      }
      printf("  %4d  ", (int)r);
      printNarrow(name);
      printf("\n");
      count = r;
   }
   printf("  (%d)\n\n", count);

   printf("messages:\n");
   count = 0;
   for (ref_t r = 1 ; r < 0x10000 ; r++) {
      const TCHAR* name = module.resolveMessage(r);
      if (!name || !name[0]) {
         if (r > count + 8) break;
         continue;
      }
      printf("  %4d  ", (int)r);
      printNarrow(name);
      printf("\n");
      count = r;
   }
   printf("  (%d)\n\n", count);

   // --- sections ---
   printf("sections:\n");
   static const ref_t masks[] = {
      mskSymbolRef, mskClassRef, mskVMTRef, mskMetaDataRef, mskCodeRef,
      mskDataRef, mskStaticRef
   };

   int sectionCount = 0;
   for (ref_t r = 1 ; r < 0x1000 ; r++) {
      const TCHAR* name = module.resolveReference(r);
      if (!name || !name[0])
         continue;

      for (size_t m = 0 ; m < sizeof(masks) / sizeof(masks[0]) ; m++) {
         Section* section = module.mapSection(r | masks[m], true);
         if (!section)
            continue;

         printf("  ref %d '", (int)r);
         printNarrow(name);
         printf("'  %s  %u bytes\n", maskName(masks[m]),
                (unsigned int)section->Length());
         sectionCount++;

         if (withCode && (masks[m] == mskSymbolRef || masks[m] == mskClassRef))
            disassemble(section);

         // [u32 size][roleRef][flags][parentRef][classSize][message, offset]*
         if (withCode && masks[m] == mskVMTRef) {
            DumpReader vmt(section);
            vmt.getU32LE();
            // Sequenced reads: as printf arguments their evaluation order is
            // unspecified, and gcc's right-to-left scrambled every field.
            unsigned int roleRef   = vmt.getU32LE();
            unsigned int flags     = vmt.getU32LE();
            unsigned int parentRef = vmt.getU32LE();
            unsigned int classSize = vmt.getU32LE();
            printf("      role %08X  flags %08X  parent %08X  size %u\n",
                   roleRef, flags, parentRef, classSize);
            while (vmt.Position() + 8 <= section->Length()) {
               unsigned int message = vmt.getU32LE();
               unsigned int offset  = vmt.getU32LE();
               printf("      message %08X -> +%u%s\n", message, offset,
                      message == 0 ? "  (any handler)" : "");
            }
            if (vmt.Position() != section->Length())
               printf("      !! %u trailing bytes\n",
                      (unsigned int)(section->Length() - vmt.Position()));
         }
      }
   }
   printf("  (%d sections)\n", sectionCount);

   return 0;
}
