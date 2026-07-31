//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA command-Line Compiler
//
//		POSIX front end: application path lookup and main().
//
//		The platform-independent project handling lives in elc/elcproject.cpp.
//
//		SCOPE: this front end compiles .l sources to .nl bytecode modules and
//		stops there. It does NOT link.
//
//		Linking is deliberately absent rather than unfinished. The existing
//		linker emits PE/COFF and the existing code generator emits Win32-hosted
//		x86, so neither can produce anything runnable on POSIX. Both are to be
//		replaced by an LLVM backend plus a system linker.
//		  See docs/plan/17-llvm-backend-and-targets.md
//---------------------------------------------------------------------------

#include "elena.h"
// --------------------------------------------------------------------------
#include "elc.h"
#include "errors.h"
#include "compiler.h"
#include "targetinfo.h"
#ifdef ELENA_WITH_LLVM
#include "llvmgen.h"
#include "module.h"
#endif

#include <unistd.h>
#include <limits.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

using namespace _ELENA_;

// --- getAppPath ---

void getAppPath(Path& appPath)
{
   char path[PATH_MAX + 1];
   ssize_t length = -1;

#ifdef __APPLE__
   uint32_t size = PATH_MAX;
   if (_NSGetExecutablePath(path, &size) == 0)
      length = (ssize_t)strlen(path);
#else
   length = readlink("/proc/self/exe", path, PATH_MAX);
#endif

   if (length <= 0) {
      // Fall back to the current directory. The compiler needs this only to
      // locate elc.cfg and the parser table, so a wrong answer produces a
      // clear "file not found" rather than misbehaviour.
      if (!getcwd(path, PATH_MAX))
         path[0] = 0;
      length = (ssize_t)strlen(path);
   }
   path[length] = 0;

#ifdef _UNICODE
   wchar_t wide[PATH_MAX + 1];
   ansiToUnicode(path, wide, (size_t)length);
   wide[length] = 0;

   appPath.copyPath(wide);
#else
   appPath.copyPath(path);
#endif
}

// --- Main function ---

int main(int argc, char* argv[])
{
   int    exitCode = 0;
   _ELC_::Project project;

   // argv is char*; widen it once up front when TCHAR is wchar_t, and use it
   // directly when TCHAR is char (the UTF-8 configuration).
   const TCHAR** args = new const TCHAR*[argc];
#ifdef _UNICODE
   LocalString<0x200>* wide = new LocalString<0x200>[argc];
   for (int i = 0 ; i < argc ; i++) {
      wide[i].convert(argv[i]);
      args[i] = wide[i];
   }
#else
   for (int i = 0 ; i < argc ; i++)
      args[i] = argv[i];
#endif

#ifdef ELENA_WITH_LLVM
   // Smoke test for the code generation back end: brings up a TargetMachine for
   // every known target and emits an object file, so a broken LLVM integration
   // is reported here rather than discovered halfway through a compilation.
   if (argc == 2 && compstr(argv[1], "--llvm-selftest")) {
      size_t count = 0;
      const TargetInfo* list = getTargetList(count);
      int failed = 0;

      for (size_t i = 0 ; i < count ; i++) {
         LLVMGenerator generator;
         const char* error = NULL;

         printf("  %-11s %-34s ", list[i].name, list[i].triple);

         if (!generator.init(&list[i], &error)) {
            printf("INIT FAILED: %s\n", error ? error : "?");
            failed++;
            continue;
         }

         char path[256];
         snprintf(path, sizeof(path), "/tmp/selene-selftest-%s.o", list[i].name);

         if (!generator.emitObject(path, &error)) {
            printf("EMIT FAILED: %s\n", error ? error : "?");
            failed++;
            continue;
         }
         printf("ok\n");
      }
      printf("%d of %d targets failed\n", failed, (int)count);

      delete[] args;
      return failed ? -1 : 0;
   }
#endif

#ifdef ELENA_WITH_LLVM
   // Translates a compiled module's code sections to LLVM IR and emits both
   // textual IR and an object file. The bridge between the two halves that now
   // exist: the byte code reader and the code generator.
   if (argc >= 3 && compstr(argv[1], "--llvm-translate")) {
      LocalString<0x200> modulePath;
      modulePath.copy(argv[2]);

      FileReader file(modulePath, feRaw);
      if (!file.isOpened()) {
         printf("cannot open '%s'\n", argv[2]);
         delete[] args; return -1;
      }

      Module module;
      if (module.load(file) != lrSuccessful) {
         printf("'%s' is not a loadable module\n", argv[2]);
         delete[] args; return -1;
      }

      LLVMGenerator generator;
      const char* error = NULL;
      if (!generator.init(getCurrentTarget(), &error)) {
         printf("target setup failed: %s\n", error ? error : "?");
         delete[] args; return -1;
      }

      int translated = 0, skipped = 0;
      for (ref_t r = 1 ; r < 0x1000 ; r++) {
         const TCHAR* refName = module.resolveReference(r);
         if (!refName || !refName[0])
            continue;

         char plain[256];
         size_t k = 0;
         for ( ; refName[k] && k < sizeof(plain) - 1 ; k++)
            plain[k] = (char)refName[k];
         plain[k] = 0;

         // --- symbol code: one section, one procedure ---
         Section* symbol = module.mapSection(r | mskSymbolRef, true);
         if (symbol && symbol->Length() > 4) {
            DumpReader reader(symbol);
            unsigned int codeSize = reader.getU32LE();

            if (codeSize > 0 && codeSize + 4 <= symbol->Length()) {
               const unsigned char* code =
                  (const unsigned char*)symbol->getArray() + reader.Position();

               printf("  %-34s symbol %5u  ", plain, codeSize);
               if (generator.translateProcedure(plain, code, codeSize, &error)) {
                  printf("ok\n"); translated++;
               }
               else { printf("%s\n", error ? error : "?"); skipped++; }
            }
         }

         // --- class code: N methods concatenated, boundaries held by the VMT ---
         //
         // A class code section is not one procedure. The VMT records, per
         // message, the offset of that method's code, so the VMT is what
         // splits the section. Translating the section as a single function --
         // which is what happened before this -- produced one enormous
         // procedure with every method's code run together.
         Section* classCode = module.mapSection(r | mskClassRef, true);
         Section* vmt       = module.mapSection(r | mskVMTRef, true);

         if (classCode && vmt && vmt->Length() >= 8) {
            DumpReader vmtReader(vmt);
            vmtReader.getU32LE();                       // vmt size
            vmtReader.getU32LE();                       // class size

            // Collect (message, offset), then sort by offset so each method's
            // extent is the distance to the next one.
            struct Entry { unsigned int message, offset; };
            Entry entries[512];
            int count = 0;

            while (!vmtReader.Eof() && count < 512) {
               unsigned int message = vmtReader.getU32LE();
               unsigned int offset  = vmtReader.getU32LE();

               if (message == 0x7FFFFFFF)              // terminator
                  break;

               entries[count].message = message;
               entries[count].offset  = offset;
               count++;
            }

            for (int a = 0 ; a < count ; a++)
               for (int b = a + 1 ; b < count ; b++)
                  if (entries[b].offset < entries[a].offset) {
                     Entry tmp = entries[a]; entries[a] = entries[b]; entries[b] = tmp;
                  }

            for (int e = 0 ; e < count ; e++) {
               // The VMT offset points at the method's LENGTH word, not at its
               // code: saveProcedure writes [u32 size][byte code]. Translating
               // from the offset directly fed that length word to the decoder
               // as an opcode and every byte after it was misaligned -- which
               // is what made unknown opcodes appear across almost every
               // command family at once.
               unsigned int at = entries[e].offset;
               if (at + 4 > classCode->Length())
                  continue;

               const unsigned char* raw =
                  (const unsigned char*)classCode->getArray() + at;

               unsigned int size = (unsigned int)raw[0] | ((unsigned int)raw[1] << 8)
                                 | ((unsigned int)raw[2] << 16) | ((unsigned int)raw[3] << 24);

               unsigned int start = at + 4;

               // Compared against the REMAINING length, not start + size:
               // an inherited or abstract entry carries 0xFFFFFFFF as its size,
               // and start + size wrapped around to a small number that passed
               // the bounds test.
               if (size == 0 || size > classCode->Length() - start)
                  continue;

               unsigned int end = start + size;

               char name[320];
               snprintf(name, sizeof(name), "%s#%08X", plain, entries[e].message);

               const unsigned char* code =
                  (const unsigned char*)classCode->getArray() + start;

               printf("  %-34s method %5u  ", name, size);
               if (generator.translateProcedure(name, code, end - start, &error)) {
                  printf("ok\n"); translated++;
               }
               else { printf("%s\n", error ? error : "?"); skipped++; }
            }
         }
      }

      printf("\ntraduzidas: %d, nao traduzidas: %d\n", translated, skipped);

      if (!generator.verify(&error)) {
         printf("IR INVALIDO: %s\n", error ? error : "?");
         delete[] args; return -1;
      }
      printf("IR verificado pelo LLVM: ok\n");

      generator.emitIR("/tmp/selene-O0.ll", &error);

      if (!generator.optimize(2, &error)) {
         printf("otimizacao falhou: %s\n", error ? error : "?");
         delete[] args; return -1;
      }
      printf("pipeline de otimizacao: ok\n");

      generator.emitIR("/tmp/selene.ll", &error);
      if (generator.emitObject("/tmp/selene.o", &error))
         printf("emitidos /tmp/selene.ll e /tmp/selene.o\n");
      else printf("emissao falhou: %s\n", error ? error : "?");

      delete[] args;
      return 0;
   }
#endif

   bool* consumed = new bool[argc];
   int targetExit = 0;
   if (!_ELC_::processTargetOptions(argc, args, consumed, targetExit)) {
      delete[] args; delete[] consumed;
      return targetExit;
   }

   try {
      project.printInfo(ELC_GREETING, ELC_MAJOR_VERSION, ELC_MINOR_VERSION);
      printf("target  : %s (%s)\n", getCurrentTarget()->name, getCurrentTarget()->triple);

      if (argc < 2) {
         // show help if no parameters proveded
         _tprintf(ELC_HELP_INFO);
         delete[] args;
         return -3;
      }

      // Initializing..
      project.loadConfig(Path(project.appPath, DEFAULT_CONFIG), false);

      _ELC_::loadTargetConfig(project);

      // Initializing..
      for (int i = 1 ; i < argc ; i++) {
         if (consumed[i])
            continue;

         const TCHAR* arg = args[i];

         if (arg[0] == '-') {
            project.setOption(arg + 1);
         }
         else project.addSource(arg);
      }

      // load core module
      if (!project.BoolSetting(opStandart)) {
         project.loadStandardModule();
      }

      // Cleaning up
      project.printInfo(_T("Cleaning up..."));
      project.cleanUp();

      // Compiling..
      project.printInfo(ELC_COMPILING);

      Path syntaxPath(project.StrSetting(opAppPath), SYNTAX_FILE);
      FileReader syntaxFile(syntaxPath, feRaw);
      if (!syntaxFile.isOpened())
         project.raiseError(errInvalidFile, syntaxPath.asString());

      Compiler compiler(&syntaxFile);
      if (compiler.run(project))
         project.printInfo(ELC_SUCCESSFUL_COMPILATION);
      else {
         exitCode = -1;
         project.printInfo(ELC_WARNING_COMPILATION);
      }

      // Linking is not available on this platform yet -- see the note above.
      if (project.IntSetting(opSystemType) != ptLibrary) {
         project.printInfo(_T("Linking is not supported on this platform yet; ")
                           _T("stopping after byte code generation."));
      }
   }
   catch(_ELENA_::InternalError& e) {
      project.printInfo(ELC_INTERNAL_ERROR, e.message);
      exitCode = -2;

      project.cleanUp();
   }
   catch(_ELENA_::_Exception&) {
      project.printInfo(ELC_UNSUCCESSFUL);
      exitCode = -2;

      project.cleanUp();
   }

   delete[] args;
   delete[] consumed;
#ifdef _UNICODE
   delete[] wide;
#endif
   return exitCode;
}
