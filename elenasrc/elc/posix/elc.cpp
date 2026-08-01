//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  Selene command-line compiler
//
//		POSIX front end: application path lookup, main(), and the LLVM link
//		pipeline. `elc -c<project>` compiles the sources to byte code
//		modules, translates the module closure to one object file and runs
//		the system linker -- the project configuration drives every step,
//		exactly as it drove the 2009 linker.
//
//		The platform-independent project handling lives in elc/elcproject.cpp;
//		the byte-code-to-IR translation in llvmgen/llvmgen.cpp.
//		  See docs/plan/17-llvm-backend-and-targets.md
//---------------------------------------------------------------------------

#include "elena.h"
// --------------------------------------------------------------------------
#include "elc.h"
#include "errors.h"
#include "compiler.h"
#include "targetinfo.h"
#include "llvmgen.h"
#include "module.h"

#include <map>
#include <string>
#include <vector>
#include <list>
#include <set>

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

// --- The LLVM link pipeline -----------------------------------------------
//
// Byte code references are MODULE-LOCAL indices, so nothing links until they
// become names: every reference resolves through its module's own tables
// (with the forward aliases applied), and messages intern into ONE global id
// space across everything being linked. This is the linker half the 2009
// system buried inside the JIT. Driven by the project configuration from
// `elc -c<prj>`; `--llvm-translate` exposes the same machinery as a
// debugging tool.

#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>

namespace
{
   std::string narrowName(const TCHAR* raw)
   {
      std::string out;
      for (size_t i = 0 ; raw && raw[i] ; i++)
         out += (char)raw[i];
      return out;
   }

   struct TranslateState
   {
      std::map<std::string, std::string>  forwards;
      std::map<std::string, unsigned int> messageIds;
      int translated = 0;
      int skipped    = 0;
   };

   struct ModuleResolver
   {
      Module*         module;
      TranslateState* state;
      std::list<std::string> storage;

      const char* keep(const std::string& s)
      {
         storage.push_back(s);
         return storage.back().c_str();
      }

      const char* name(unsigned int naked)
      {
         const TCHAR* raw = module->resolveReference(naked);
         if (!raw || !raw[0])
            return NULL;

         std::string full = narrowName(raw);

         // A leading quote is a forward alias, bound here -- at link time,
         // same as 2009. An unbound alias keeps its name and surfaces as an
         // undefined symbol, which is the truth.
         if (full[0] == '\'') {
            std::map<std::string, std::string>::iterator bound =
               state->forwards.find(full);
            if (bound != state->forwards.end())
               full = bound->second;
         }
         return keep(full);
      }

      const char* spelling(unsigned int naked)
      {
         const TCHAR* raw = module->resolveConstant(naked);
         if (!raw)
            return NULL;

         return keep(narrowName(raw));
      }

      unsigned int message(unsigned int reference)
      {
         if (reference & 0x80000000u)             // predefined: already global
            return reference;

         const TCHAR* raw = module->resolveMessage(reference);
         if (!raw || !raw[0])
            return reference;

         // Ids are dense from 1; predefined ids keep the high bit, which
         // keeps them first in the signed order the VMTs sort by.
         std::string name = narrowName(raw);
         std::map<std::string, unsigned int>::iterator known =
            state->messageIds.find(name);
         if (known != state->messageIds.end())
            return known->second;

         unsigned int id = (unsigned int)state->messageIds.size() + 1;
         state->messageIds[name] = id;
         return id;
      }
   };

   bool translateSemModule(LLVMGenerator& generator, const char* path,
                           TranslateState& state, bool verbose)
   {
      const char* error = NULL;

      LocalString<0x200> modulePath;
      modulePath.copy(path);

      FileReader file(modulePath, feRaw);
      if (!file.isOpened()) {
         printf("cannot open '%s'\n", path);
         return false;
      }

      Module module;
      if (module.load(file) != lrSuccessful) {
         printf("'%s' is not a loadable module\n", path);
         return false;
      }

      ModuleResolver resolver;
      resolver.module = &module;
      resolver.state  = &state;

      generator.setResolver(
         [](void* ctx, unsigned int r) { return ((ModuleResolver*)ctx)->name(r); },
         [](void* ctx, unsigned int r) { return ((ModuleResolver*)ctx)->spelling(r); },
         [](void* ctx, unsigned int r) { return ((ModuleResolver*)ctx)->message(r); },
         &resolver);

      if (verbose)
         printf("== %s\n", path);

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

               // The function carries the CANONICAL name rcall sites link
               // against; named `plain` alone, every cross-module symbol call
               // was an undefined reference.
               std::string canonical = std::string("selene.sym:") + plain;

               if (verbose)
                  printf("  %-40s symbol %5u  ", plain, codeSize);
               if (generator.translateProcedure(canonical.c_str(), code, codeSize, &error)) {
                  if (verbose) printf("ok\n");
                  state.translated++;
               }
               else {
                  printf("%s%s: %s\n", verbose ? "  " : "", plain,
                         error ? error : "?");
                  state.skipped++;
               }
            }
         }

         // --- class code: N methods concatenated, boundaries held by the VMT ---
         //
         // A class code section is not one procedure. The VMT records, per
         // message, the offset of that method's code, so the VMT is what
         // splits the section.
         Section* classCode = module.mapSection(r | mskClassRef, true);
         Section* vmt       = module.mapSection(r | mskVMTRef, true);

         if (classCode && vmt && vmt->Length() >= 8) {
            DumpReader vmtReader(vmt);

            // [u32 size][roleRef][flags][parentRef][u32 classSize][entries...]
            vmtReader.getU32LE();                       // section size
            vmtReader.getU32LE();                       // roleRef
            unsigned int classFlags = vmtReader.getU32LE();
            unsigned int parentRef  = vmtReader.getU32LE();
            vmtReader.getU32LE();                       // classSize

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

            unsigned int vmtMessages[512];
            const char*  vmtMethods[512];
            char         vmtNames[512][320];
            int          emitted = 0;

            for (int e = 0 ; e < count ; e++) {
               // The VMT offset points at the method's LENGTH word:
               // saveProcedure writes [u32 size][byte code].
               unsigned int at = entries[e].offset;
               if (at + 4 > classCode->Length())
                  continue;

               const unsigned char* raw =
                  (const unsigned char*)classCode->getArray() + at;

               unsigned int size = (unsigned int)raw[0] | ((unsigned int)raw[1] << 8)
                                 | ((unsigned int)raw[2] << 16) | ((unsigned int)raw[3] << 24);

               unsigned int start = at + 4;

               // Compared against the REMAINING length, not start + size: an
               // inherited or abstract entry carries 0xFFFFFFFF as its size,
               // and start + size wrapped around to a small number.
               if (size == 0 || size > classCode->Length() - start)
                  continue;

               unsigned int end = start + size;

               // Method names carry the GLOBAL message id: the VMT entry, the
               // function and every other module agree on it by construction.
               unsigned int globalMessage = resolver.message(entries[e].message);

               char name[320];
               snprintf(name, sizeof(name), "%s#%08X", plain, globalMessage);

               const unsigned char* code =
                  (const unsigned char*)classCode->getArray() + start;

               if (verbose)
                  printf("  %-40s method %5u  ", name, size);
               if (generator.translateProcedure(name, code, end - start, &error)) {
                  if (verbose) printf("ok\n");
                  state.translated++;

                  if (emitted < 512) {
                     vmtMessages[emitted] = globalMessage;
                     snprintf(vmtNames[emitted], sizeof(vmtNames[emitted]), "%s", name);
                     vmtMethods[emitted] = vmtNames[emitted];
                     emitted++;
                  }
               }
               else {
                  printf("%s%s: %s\n", verbose ? "  " : "", name,
                         error ? error : "?");
                  state.skipped++;
               }
            }

            // The VMT is emitted alongside its methods. Emitted even with
            // ZERO own methods: an inherited-only class still needs its
            // table -- the parent chain lives in its header, and its constant
            // instance points at it.
            {
               std::string vmtName = std::string("selene.vmt:") + plain;

               const char* parentPlain =
                  parentRef ? resolver.name(parentRef) : NULL;
               std::string parentName = parentPlain
                  ? std::string("selene.vmt:") + parentPlain
                  : std::string();

               generator.emitVMT(vmtName.c_str(),
                                 parentName.empty() ? NULL : parentName.c_str(),
                                 classFlags, vmtMessages, vmtMethods,
                                 (unsigned int)emitted, &error);
            }
         }

         // --- data sections ---
         {
            Section* data = module.mapSection(r | mskDataRef, true);
            if (data && data->Length() > 0) {
               std::string dataName = std::string("selene.data:") + plain;
               generator.emitData(dataName.c_str(),
                                  (const unsigned char*)data->getArray(),
                                  data->Length(), &error);
            }
         }
      }

      // --- constant symbol instances ---
      //
      // A reference carrying mskConstantRef means "the singleton instance of
      // this symbol". The 2009 linker produced it by EXECUTING the symbol at
      // link time and freezing the result; materialising it statically costs
      // nothing at startup and keeps the object outside the heap. Symbols
      // whose expression actually runs code need lazy initialisation and are
      // not emitted here.
      for (ref_t r = 1 ; r < 0x1000 ; r++) {
         const char* plain = resolver.name((unsigned int)r);
         if (!plain)
            continue;

         Section* vmt = module.mapSection(r | mskVMTRef, true);
         if (!vmt || vmt->Length() < 20)
            continue;

         // classSize sits after the size word and the three header words.
         DumpReader reader(vmt);
         reader.getU32LE();                          // section size
         reader.getU32LE();                          // roleRef
         reader.getU32LE();                          // flags
         reader.getU32LE();                          // parentRef
         unsigned int classSize = reader.getU32LE();

         std::string constName = std::string("selene.const:") + plain;
         std::string vmtName   = std::string("selene.vmt:") + plain;

         generator.emitConstantObject(constName.c_str(), vmtName.c_str(),
                                      classSize, &error);
      }

      return true;
   }

   // The runtime startup calls the fixed name `selene.program`; the 'program
   // forward says which symbol that is.
   bool emitProgramEntry(LLVMGenerator& generator, TranslateState& state,
                         bool required)
   {
      const char* error = NULL;

      std::map<std::string, std::string>::iterator program =
         state.forwards.find("'program");
      if (program == state.forwards.end()) {
         if (required)
            printf("erro: nenhum forward 'program define a entrada\n");
         return !required;
      }

      std::string entry = std::string("selene.sym:") + program->second;
      generator.emitEntry(entry.c_str(), &error);
      printf("entrada: selene.program -> %s\n", program->second.c_str());
      return true;
   }

   void collectSemModules(const std::string& root, std::set<std::string>& out)
   {
      DIR* dir = opendir(root.c_str());
      if (!dir)
         return;

      struct dirent* entry;
      while ((entry = readdir(dir)) != NULL) {
         if (entry->d_name[0] == '.')
            continue;

         std::string path = root + "/" + entry->d_name;

         struct stat info;
         if (stat(path.c_str(), &info) != 0)
            continue;

         if (S_ISDIR(info.st_mode)) {
            collectSemModules(path, out);
         }
         else {
            size_t length = path.size();
            if (length > 4 && path.compare(length - 4, 4, ".sem") == 0)
               out.insert(path);
         }
      }
      closedir(dir);
   }

   bool fileExists(const std::string& path)
   {
      struct stat info;
      return stat(path.c_str(), &info) == 0;
   }

   // The symbol index of a GNU `ar` archive: "!<arch>\n", 60-byte member
   // headers, and the first member -- named "/" -- holds the System V map:
   // a big-endian count, that many member offsets, then the NUL-terminated
   // symbol names. Reading it is what lets the stub emitter skip everything
   // the runtime really provides, without ever running nm.
   void readArchiveSymbols(const std::string& path, std::set<std::string>& out)
   {
      FILE* file = fopen(path.c_str(), "rb");
      if (!file)
         return;

      char magic[8];
      if (fread(magic, 1, 8, file) != 8 || memcmp(magic, "!<arch>\n", 8) != 0) {
         fclose(file);
         return;
      }

      char header[60];
      while (fread(header, 1, 60, file) == 60) {
         char sizeText[11];
         memcpy(sizeText, header + 48, 10);
         sizeText[10] = 0;
         long size = strtol(sizeText, NULL, 10);
         if (size < 0)
            break;

         if (header[0] == '/' && header[1] == ' ') {
            std::vector<unsigned char> body((size_t)size);
            if (fread(body.data(), 1, (size_t)size, file) == (size_t)size
               && size >= 4)
            {
               unsigned int count = ((unsigned int)body[0] << 24)
                                  | ((unsigned int)body[1] << 16)
                                  | ((unsigned int)body[2] << 8)
                                  |  (unsigned int)body[3];

               size_t at = 4 + (size_t)count * 4;
               for (unsigned int i = 0 ; i < count && at < (size_t)size ; i++) {
                  std::string name((const char*)&body[at]);
                  out.insert(name);
                  at += name.size() + 1;
               }
            }
            break;                                 // the map is member one
         }

         fseek(file, size + (size & 1), SEEK_CUR); // members are 2-aligned
      }
      fclose(file);
   }

   // Compile is done; turn the byte code into an executable. Everything the
   // step needs -- forwards, value classes, the executable name, the library
   // path -- comes from the project configuration, exactly as it did for the
   // 2009 linker.
   bool linkProject(_ELC_::Project& project)
   {
      const char* error = NULL;

      LLVMGenerator generator;
      if (!generator.init(getCurrentTarget(), &error)) {
         printf("target setup failed: %s\n", error ? error : "?");
         return false;
      }

      std::string literalClass = narrowName(project.StrSetting(opLiteralClass));
      std::string integerClass = narrowName(project.StrSetting(opIntegerClass));
      std::string realClass    = narrowName(project.StrSetting(opRealClass));
      std::string nilSymbol    = narrowName(NIL_CLASS);

      generator.setValueClasses(literalClass.c_str(), integerClass.c_str(),
                                realClass.c_str(), nilSymbol.c_str());

      TranslateState state;
      for (SourceIterator f = project.getForwardIt() ; !f.Eof() ; f++)
         state.forwards[narrowName(f.key())] = narrowName(*f);

      // The link closure: the library plus the project's own output.
      std::string libPath    = narrowName(project.StrSetting(opLibPath));
      std::string outputPath = narrowName(project.StrSetting(opOutputPath));
      if (outputPath.empty())
         outputPath = ".";

      std::set<std::string> modules;
      collectSemModules(libPath, modules);
      collectSemModules(outputPath, modules);

      printf("ligando %u modulos de '%s' + '%s'\n",
             (unsigned int)modules.size(), libPath.c_str(), outputPath.c_str());

      for (std::set<std::string>::iterator m = modules.begin() ;
           m != modules.end() ; ++m)
      {
         if (!translateSemModule(generator, m->c_str(), state, false))
            return false;
      }

      if (!emitProgramEntry(generator, state, true))
         return false;

      printf("procedimentos: %d traduzidos, %d falharam\n",
             state.translated, state.skipped);
      if (state.skipped > 0)
         return false;

      if (!generator.verify(&error)) {
         printf("IR invalido: %s\n", error ? error : "?");
         return false;
      }

      if (!generator.optimize(2, &error)) {
         printf("otimizacao falhou: %s\n", error ? error : "?");
         return false;
      }

      // The executable's name comes from the project -- `executable=` is a
      // path option, already resolved against the project directory. The
      // .exe suffix is a Windows spelling and comes off everywhere else.
      const TargetInfo* target = getCurrentTarget();
      bool forWindows = strstr(target->triple, "windows") != NULL;

      std::string base = narrowName(project.StrSetting(opTarget));
      if (base.empty())
         base = outputPath + "/program";
      if (base.size() > 4 && base.compare(base.size() - 4, 4, ".exe") == 0)
         base.resize(base.size() - 4);

      std::string objPath   = base + ".o";
      std::string stubsPath = base + ".stubs.o";
      std::string exePath   = forWindows ? base + ".exe" : base;

      // The runtime archive sits next to the compiler: libselene.a for the
      // host, libselene-<target>.a for a cross target.
      std::string appPath = narrowName(project.StrSetting(opAppPath));
      bool hostTarget =
         strcmp(target->triple, getDefaultTarget()->triple) == 0;

      std::string runtimeArchive = hostTarget
         ? appPath + "/../libselene.a"
         : appPath + "/../libselene-" + target->name + ".a";

      if (!generator.emitObject(objPath.c_str(), &error)) {
         printf("emissao falhou: %s ('%s')\n", error ? error : "?",
                objPath.c_str());
         return false;
      }

      // Stub what neither the object nor the runtime archive defines.
      std::set<std::string> provided;
      readArchiveSymbols(runtimeArchive, provided);

      std::vector<const char*> providedNames;
      for (std::set<std::string>::iterator s = provided.begin() ;
           s != provided.end() ; ++s)
      {
         providedNames.push_back(s->c_str());
      }

      if (!generator.emitStubs(stubsPath.c_str(),
                               providedNames.empty() ? NULL : &providedNames[0],
                               (unsigned int)providedNames.size(), &error))
      {
         printf("emissao de stubs falhou: %s\n", error ? error : "?");
         return false;
      }

      std::string linker;
      if (hostTarget) {
         linker = "cc";
      }
      else if (forWindows) {
         std::string arch(target->triple,
                          strchr(target->triple, '-') - target->triple);
         linker = arch + "-w64-mingw32-gcc";
      }

      if (linker.empty() || !fileExists(runtimeArchive)) {
         printf("objeto emitido: %s\n", objPath.c_str());
         printf("sem %s para este alvo; ligue manualmente:\n",
                linker.empty() ? "linker" : runtimeArchive.c_str());
         printf("  <cc> %s <libselene> %s -o %s\n",
                objPath.c_str(), stubsPath.c_str(), exePath.c_str());
         return true;
      }

      // Order matters: the weak stubs come AFTER the runtime archive, so a
      // real native wins and only the genuinely absent ones fall through to
      // the reporting stubs.
      std::string command = linker + " \"" + objPath + "\" \""
                          + runtimeArchive + "\" \"" + stubsPath + "\" -o \""
                          + exePath + "\"";

      printf("%s\n", command.c_str());
      int status = system(command.c_str());
      if (status != 0) {
         printf("link falhou (%d)\n", status);
         return false;
      }

      printf("executavel: %s\n", exePath.c_str());
      return true;
   }
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

   // Debug tool: translate modules into a single object without a project.
   //
   //   elc --llvm-translate [--target=<t>] [-f'alias=name ...] <module.sem> ...
   //
   // The same machinery `elc -c<prj>` drives from the project configuration,
   // exposed for inspecting a translation in isolation. Emits /tmp/selene.ll
   // and /tmp/selene.o.
   if (argc >= 3 && compstr(argv[1], "--llvm-translate")) {
      // --target applies here too. The general option pass runs after this
      // branch returns -- too late for the generator -- so it runs early.
      {
         bool* consumedHere = new bool[argc];
         int   targetExit   = 0;
         bool  proceed = _ELC_::processTargetOptions(argc, args, consumedHere,
                                                     targetExit);
         delete[] consumedHere;
         if (!proceed) {
            delete[] args; return targetExit;
         }
      }

      LLVMGenerator generator;
      const char* error = NULL;
      if (!generator.init(getCurrentTarget(), &error)) {
         printf("target setup failed: %s\n", error ? error : "?");
         delete[] args; return -1;
      }
      printf("alvo: %s (%s)\n", getCurrentTarget()->name,
             getCurrentTarget()->triple);

      // Standalone defaults; the project path reads these from [compiler].
      generator.setValueClasses("std'basic'literal", "std'basic'intnumber",
                                "std'basic'realnumber", "$elena'$nil");

      TranslateState state;
      std::vector<const char*> modulePaths;
      for (int a = 2 ; a < argc ; a++) {
         if (strncmp(argv[a], "--target=", 9) == 0)
            continue;                              // consumed above

         if (argv[a][0] == '-' && argv[a][1] == 'f') {
            const char* pair = argv[a] + 2;
            const char* eq = strchr(pair, '=');
            if (eq && eq != pair) {
               state.forwards[std::string(pair, (size_t)(eq - pair))] = eq + 1;
            }
            else printf("aviso: forward invalido '%s'\n", argv[a]);
         }
         else modulePaths.push_back(argv[a]);
      }

      for (size_t pathAt = 0 ; pathAt < modulePaths.size() ; pathAt++) {
         if (!translateSemModule(generator, modulePaths[pathAt], state, true)) {
            delete[] args; return -1;
         }
      }

      emitProgramEntry(generator, state, false);

      printf("\ntraduzidas: %d, nao traduzidas: %d\n",
             state.translated, state.skipped);

      // The global message vocabulary this link unit settled on -- the ids a
      // host program needs to send messages of its own.
      printf("mensagens globais:\n");
      for (std::map<std::string, unsigned int>::iterator m = state.messageIds.begin() ;
           m != state.messageIds.end() ; ++m)
      {
         printf("  %6u  %s\n", m->second, m->first.c_str());
      }

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

      // Executable projects continue into the LLVM link pipeline: translate
      // the byte code closure, emit an object, run the system linker. The
      // project configuration drives all of it, exactly as it drove the 2009
      // linker.
      if (project.IntSetting(opSystemType) != ptLibrary) {
         project.printInfo(_T("Linking..."));
         if (!linkProject(project))
            exitCode = -1;
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
