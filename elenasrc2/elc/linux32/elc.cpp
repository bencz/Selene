//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA command-Line Compiler
//
//		This file contains the main body of the Linux command-line compiler
//
//                                              (C)2005-2015, by Alexei Rakov
//---------------------------------------------------------------------------

#include "elena.h"
// --------------------------------------------------------------------------
#include "elc.h"
#include "constants.h"
#include "errors.h"
#include "compiler.h"
#include "linker.h"
#include "image.h"
#include "x86jitcompiler.h"
#include "llvmgen.h"
#include "module.h"

#include <stdarg.h>
#include <unistd.h>
#include <limits.h>

// --- getAppPath ---

// the directory containing the running executable: configuration, the parser
// table and the rule table are resolved against it, never against hardcoded
// system locations
static void getAppPath(_ELENA_::Path& path)
{
   char buffer[PATH_MAX];
   ssize_t length = readlink("/proc/self/exe", buffer, PATH_MAX - 1);
   if (length > 0) {
      buffer[length] = 0;

      char* separator = strrchr(buffer, '/');
      if (separator)
         *separator = 0;

      path.copy(buffer);
   }
   else path.copy(".");
}

// --- ImageHelper ---

class ImageHelper : public _ELENA_::ExecutableImage::_Helper
{
   virtual void beforeLoad(_ELENA_::_JITCompiler* compiler, _ELENA_::ExecutableImage& image)
   {
   }

   virtual void afterLoad(_ELENA_::ExecutableImage& image)
   {
      _ELENA_::Project* project = image.getProject();

      _ELENA_::Section* debug = image.getDebugSection();

      // fix up debug section if required
      if (debug->Length() > 8) {
         debug->writeDWord(0, debug->Length());
         debug->addReference(image.getDebugEntryPoint(), 4);

         // save subject info if enabled
         _ELENA_::MemoryWriter debugWriter(debug);
         if (project->BoolSetting(_ELENA_::opDebugSubjectInfo)) {
            image.saveSubject(&debugWriter);
         }
         else debugWriter.writeDWord(0);
      }
      else debug->clear();
   }

public:
   ImageHelper()
   {
   }
};

// --- Project ---

void print(const char* msg, ...)
{
   va_list argptr;
   va_start(argptr, msg);

   vprintf(msg, argptr);
   va_end(argptr);
   printf("\n");

   fflush(stdout);
}

_ELC_::Project :: Project()
{
   getAppPath(appPath);
   _settings.add(_ELENA_::opAppPath, _ELENA_::StringHelper::clone(appPath));
   _settings.add(_ELENA_::opNamespace, _ELENA_::StringHelper::clone("unnamed"));

   _tabSize = 4;
   _encoding = _ELENA_::feUTF8;

   // !! temporally
   _settings.add(_ELENA_::opDebugSubjectInfo, -1);
}

void _ELC_::Project :: raiseError(const char* msg, const char* path, int row, int column, const char* s)
{
   print(msg, path, row, column, s);

   throw _ELENA_::_Exception();
}

void _ELC_::Project :: raiseError(const char* msg, const char* value)
{
   print(msg, value);

   throw _ELENA_::_Exception();
}

void _ELC_::Project :: printInfo(const char* msg, const char* s)
{
   print(msg, s);
}

void _ELC_::Project :: raiseErrorIf(bool throwExecption, const char* msg, const char* path)
{
   print(msg, path);

   if (throwExecption)
      throw _ELENA_::_Exception();
}

void _ELC_::Project :: raiseWarning(const char* msg, const char* path, int row, int column, const char* s)
{
   if (!indicateWarning())
      return;

   print(msg, path, row, column, s);
}

void _ELC_::Project :: raiseWarning(const char* msg, const char* path)
{
   if (!indicateWarning())
      return;

   print(msg, path);
}

_ELENA_::ConfigCategoryIterator _ELC_::Project :: getCategory(_ELENA_::_ConfigFile& config, _ELENA_::ProjectSetting setting)
{
   switch (setting)
   {
   case _ELENA_::opTemplates:
      return config.getCategoryIt(TEMPLATE_CATEGORY);
   case _ELENA_::opPrimitives:
      return config.getCategoryIt(PRIMITIVE_CATEGORY);
   case _ELENA_::opSources:
      return config.getCategoryIt(SOURCE_CATEGORY);
   case _ELENA_::opForwards:
      return config.getCategoryIt(FORWARD_CATEGORY);
   case _ELENA_::opExternals:
      return config.getCategoryIt(EXTERNALS_CATEGORY);
   default:
      return _ELENA_::ConfigCategoryIterator();
   }
}

const char* _ELC_::Project :: getOption(_ELENA_::_ConfigFile& config, _ELENA_::ProjectSetting setting)
{
   switch (setting)
   {
   case _ELENA_::opEntry:
      return config.getSetting(PROJECT_CATEGORY, ELC_PROJECT_ENTRY);
   case _ELENA_::opNamespace:
      return config.getSetting(PROJECT_CATEGORY, ELC_NAMESPACE);
   case _ELENA_::opGCMGSize:
      return config.getSetting(LINKER_CATEGORY, ELC_MG_SIZE);
   case _ELENA_::opGCYGSize:
      return config.getSetting(LINKER_CATEGORY, ELC_YG_SIZE);
//   case _ELENA_::opSizeOfStackReserv:
//      return config.getSetting(LINKER_CATEGORY, ELC_STACK_RESERV);
//   case _ELENA_::opSizeOfStackCommit:
//      return config.getSetting(LINKER_CATEGORY, ELC_STACK_COMMIT);
//   case _ELENA_::opSizeOfHeapReserv:
//      return config.getSetting(LINKER_CATEGORY, ELC_HEAP_RESERV);
//   case _ELENA_::opSizeOfHeapCommit:
//      return config.getSetting(LINKER_CATEGORY, ELC_HEAP_COMMIT);
//   case _ELENA_::opImageBase:
//      return config.getSetting(LINKER_CATEGORY, ELC_YG_IMAGEBASE);
   case _ELENA_::opPlatform:
      return config.getSetting(SYSTEM_CATEGORY, ELC_PLATFORMTYPE);
   case _ELENA_::opTarget:
      return config.getSetting(PROJECT_CATEGORY, ELC_TARGET);
   case _ELENA_::opLibPath:
      return config.getSetting(PROJECT_CATEGORY, ELC_LIB_PATH);
   case _ELENA_::opOutputPath:
      return config.getSetting(PROJECT_CATEGORY, ELC_OUTPUT_PATH);
   case _ELENA_::opWarnOnUnresolved:
      return config.getSetting(PROJECT_CATEGORY, ELC_WARNON_UNRESOLVED);
//   case _ELENA_::opWarnOnSignature:
//      return config.getSetting(PROJECT_CATEGORY, ELC_WARNON_SIGNATURE);
   case _ELENA_::opDebugMode:
      return config.getSetting(PROJECT_CATEGORY, ELC_DEBUGINFO);
   case _ELENA_::opThreadMax:
      return config.getSetting(SYSTEM_CATEGORY, ELC_SYSTEM_THREADMAX);
   case _ELENA_::opL0:
      return config.getSetting(COMPILER_CATEGORY, ELC_L0);
   case _ELENA_::opL1:
      return config.getSetting(COMPILER_CATEGORY, ELC_L1);
   case _ELENA_::opTemplate:
      return config.getSetting(PROJECT_CATEGORY, ELC_PROJECT_TEMPLATE);
   default:
      return NULL;
   }
}

void _ELC_::Project :: addSource(const char* path)
{
   // .prj files keep Windows path separators; normalize at ingestion so the
   // same project file drives both platforms (module names derive from the
   // normalized form, file access uses it as-is)
   _ELENA_::String<char, LOCAL_PATH_LENGTH> normalized(path);
   for (size_t i = 0; i < _ELENA_::getlength(normalized); i++) {
      if (normalized[i] == '\\')
         normalized[i] = '/';
   }

   _ELENA_::Path fullPath(StrSetting(_ELENA_::opProjectPath));
   fullPath.combine(normalized);
   fullPath.lower();

   _sources.add(normalized, _ELENA_::StringHelper::clone(fullPath));
}

void _ELC_::Project :: cleanUp()
{
//   _ELENA_::Path rootPath(StrSetting(_ELENA_::opProjectPath), StrSetting(_ELENA_::opOutputPath));
//
//   for(_ELENA_::SourceIterator it = getSourceIt() ; !it.Eof() ; it++) {
//      _ELENA_::Path path;
//      path.copyPath(it.key());
//
//      _ELENA_::ReferenceNs name(StrSetting(_ELENA_::opNamespace));
//      name.pathToName(path);          // get a full name
//
//      // remove module
//      path.copy(rootPath);
//      _loader.nameToPath(name, path, _T("nl"));
//      _wremove(path);
//
//      // remove debug module
//      path.copy(rootPath);
//      _loader.nameToPath(name, path, _T("dnl"));
//      _wremove(path);
//   }
}

void _ELC_::Project :: loadConfig(const char* path, bool root, bool requiered)
{
   ElcConfigFile config;
   _ELENA_::Path configPath;

   configPath.copySubPath(path);

   if (!config.load(path, getDefaultEncoding())) {
      raiseErrorIf(requiered, ELC_ERR_INVALID_PATH, path);
      return;
   }

   // load template list
   if (root)
      loadCategory(config, _ELENA_::opTemplates, configPath);

   // load template
   const char* projectTemplate = config.getSetting(PROJECT_CATEGORY, ELC_PROJECT_TEMPLATE);
   if (!_ELENA_::emptystr(projectTemplate)) {
      const char* templateFile = _settings.get(_ELENA_::opTemplates, projectTemplate, (const char*)NULL);
      if (_ELENA_::emptystr(templateFile)) {
        _ELENA_::String<char, 255> str(projectTemplate);

         raiseErrorIf(requiered, ELC_ERR_INVALID_TEMPLATE, (const char*)str);
      }
      else loadConfig(templateFile, false, false);
   }

   loadConfig(config, configPath);
}

void _ELC_::Project :: setOption(const char* value)
{
   switch ((char)value[0]) {
      case ELC_PRM_LIB_PATH:
         _settings.add(_ELENA_::opLibPath, _ELENA_::StringHelper::clone(value + 1));
         break;
      case ELC_PRM_OUTPUT_PATH:
         _settings.add(_ELENA_::opOutputPath, _ELENA_::StringHelper::clone(value + 1));
         break;
      case ELC_PRM_EXTRA:
         if (_ELENA_::StringHelper::compare(value, ELC_PRM_TABSIZE, 4)) {
            _tabSize = _ELENA_::StringHelper::strToInt(value + 4);
         }
         else if (_ELENA_::StringHelper::compare(value, ELC_PRM_PROJECTPATH, _ELENA_::getlength(ELC_PRM_PROJECTPATH))) {
            _settings.add(_ELENA_::opProjectPath, _ELENA_::StringHelper::clone(value + _ELENA_::getlength(ELC_PRM_PROJECTPATH)));
         }
         else if (_ELENA_::StringHelper::compare(value, ELC_PRM_OPTOFF)) {
            _settings.add(_ELENA_::opL0, 0);
            _settings.add(_ELENA_::opL1, 0);
         }
         else if (_ELENA_::StringHelper::compare(value, ELC_PRM_OPT1OFF)) {
            _settings.add(_ELENA_::opL1, 0);
         }
         else raiseError(ELC_ERR_INVALID_OPTION, value);
         break;
      case ELC_PRM_WARNING:
         if (_ELENA_::StringHelper::compare(value, ELC_W_UNRESOLVED)) {
            _settings.add(_ELENA_::opWarnOnUnresolved, -1);
         }
         else if (_ELENA_::StringHelper::compare(value, ELC_W_WEAKUNRESOLVED)) {
            _settings.add(_ELENA_::opWarnOnWeakUnresolved, -1);
         }
         else if (_ELENA_::StringHelper::compare(value, ELC_W_LEVEL1)) {
            _warningMasks |= _ELENA_::WARNING_MASK_1;
         }
         else if (_ELENA_::StringHelper::compare(value, ELC_W_LEVEL2)) {
            _warningMasks |= _ELENA_::WARNING_MASK_2;
         }
         else if (_ELENA_::StringHelper::compare(value, ELC_W_LEVEL3)) {
            _warningMasks |= _ELENA_::WARNING_MASK_3;
         }
         else if (_ELENA_::StringHelper::compare(value, ELC_W_OFF)) {
            _warningMasks = 0;
         }
         break;
      case ELC_PRM_TARGET:
         _settings.add(_ELENA_::opTarget, _ELENA_::StringHelper::clone(value + 1));
         break;
      case ELC_PRM_START:
         _settings.add(_ELENA_::opEntry, _ELENA_::StringHelper::clone(value + 1));
         break;
      case ELC_PRM_DEBUGINFO:
         _settings.add(_ELENA_::opDebugMode, -1);
         break;
      case ELC_PRM_CONFIG:
      {
         projectName.copy(value + 1);

         loadConfig(value + 1);

         _ELENA_::Path projectPath;
         projectPath.copySubPath(value + 1);
         _settings.add(_ELENA_::opProjectPath, projectPath.clone());

         break;
      }
      default:
         raiseError(ELC_ERR_INVALID_OPTION, value);
   }
}

_ELENA_::_JITCompiler* _ELC_::Project :: createJITCompiler()
{
   return new _ELENA_::x86JITCompiler(BoolSetting(_ELENA_::opDebugMode));
}

void setCompilerOptions(_ELC_::Project& project, _ELENA_::Compiler& compiler)
{
   if (project.IntSetting(_ELENA_::opL0, -1) != 0) {
      _ELENA_::Path rulesPath(project.appPath);
      rulesPath.combine(RULES_FILE);
      _ELENA_::FileReader rulesFile(rulesPath, _ELENA_::feRaw, false);
      if (!rulesFile.isOpened()) {
         project.raiseWarning(errInvalidFile, rulesPath);
      }
      else compiler.loadRules(&rulesFile);
   }
   if (project.IntSetting(_ELENA_::opL1, -1) != 0) {
      compiler.turnOnOptimiation(1);
   }
}

// --- llvmTranslateModule ---

static void llvmTranslateBody(_ELENA_::LLVMGenerator& generator, const char* name,
   _ELENA_::_Memory* section, size_t offset, unsigned int entryMessage,
   _ELENA_::TranslateStats& stats)
{
   // a procedure is serialized as [u32 byte length][e-code]
   unsigned int size = 0;
   section->read(offset, &size, 4);
   if (size == 0 || size == 0xFFFFFFFF || offset + 4 + size > section->Length()) {
      if (getenv("ELENA_LLVM_TRACE"))
         printf("  skipped %s: offset %X size %X section %X\n", name,
            (unsigned int)offset, size, (unsigned int)section->Length());
      return;   // inherited/abstract entry or trailing metadata
   }

   _ELENA_::GenError error;
   if (!generator.translateProcedure(name, (const unsigned char*)section->get(offset + 4),
      size, entryMessage, stats, error))
   {
      if (stats.failed <= 8)
         printf("  %-48s %s\n", name, (const char*)error);
   }
}

static const char* llvmReferenceName(void* context, unsigned int reference)
{
   return ((_ELENA_::Module*)context)->resolveReference(reference);
}

static const char* llvmConstantValue(void* context, unsigned int reference)
{
   return ((_ELENA_::Module*)context)->resolveConstant(reference);
}

static unsigned int llvmInternMessage(void*, unsigned int message)
{
   // per-module milestone: the module-local encoding IS the id; real
   // interning into one global id space arrives with the closure driver
   return message;
}

static int llvmTranslateModule(const char* path)
{
   _ELENA_::Path modulePath(path);
   _ELENA_::FileReader reader(modulePath, _ELENA_::feRaw, false);
   _ELENA_::Module module;
   if (module.load(reader) != _ELENA_::lrSuccessful) {
      printf("cannot load the module %s\n", path);
      return -1;
   }

   _ELENA_::LLVMGenerator generator;
   _ELENA_::GenError error;
   if (!generator.init(_ELENA_::getCurrentTarget(), error)) {
      printf("LLVM: %s\n", (const char*)error);
      return -1;
   }

   _ELENA_::TranslateCallbacks callbacks;
   callbacks.context = &module;
   callbacks.referenceName = llvmReferenceName;
   callbacks.constantValue = llvmConstantValue;
   callbacks.internMessage = llvmInternMessage;
   generator.setCallbacks(callbacks);

   _ELENA_::TranslateStats stats;
   int classes = 0, symbols = 0;

   for (_ELENA_::ReferenceMap::Iterator it = module.References() ; !it.Eof() ; it++) {
      _ELENA_::ref_t reference = *it & ~_ELENA_::mskAnyRef;

      _ELENA_::_Memory* symbol = module.mapSection(reference | _ELENA_::mskSymbolRef, true);
      if (symbol) {
         symbols++;
         llvmTranslateBody(generator, it.key(), symbol, 0, 0, stats);
      }

      _ELENA_::_Memory* vmt = module.mapSection(reference | _ELENA_::mskVMTRef, true);
      _ELENA_::_Memory* body = module.mapSection(reference | _ELENA_::mskClassRef, true);
      if (vmt && body) {
         classes++;

         // the VMT tape: [u32 size][u32 classClassRef][ClassHeader][entries]
         // where an entry is [u32 message][u32 code offset]
         unsigned int size = 0;
         vmt->read(0, &size, 4);

         size_t position = 8 + sizeof(_ELENA_::ClassHeader);
         size_t end = 4 + size;
         while (position + 8 <= end) {
            unsigned int message = 0, address = 0;
            vmt->read(position, &message, 4);
            vmt->read(position + 4, &address, 4);
            position += 8;

            _ELENA_::String<char, 300> name(it.key());
            name.append('.');
            name.appendHex(message);

            if (address != 0xFFFFFFFF)
               llvmTranslateBody(generator, name, body, address, message, stats);
         }
      }
   }

   printf("\n%d classes, %d symbols\n", classes, symbols);
   printf("procedures: %u   translated: %u   failed: %u  (%.1f%%)\n",
      stats.procedures, stats.translated, stats.failed,
      stats.procedures ? (100.0 * stats.translated / stats.procedures) : 0.0);

   // the emitted IR must be structurally valid regardless of coverage
   if (!generator.verify(error)) {
      printf("VERIFIER: %s\n", (const char*)error);
      return -2;
   }
   printf("module verified\n");

   _ELENA_::Path irPath("/tmp/elena-translate.ll");
   if (generator.emitIR("/tmp/elena-translate.ll", error))
      printf("IR written to /tmp/elena-translate.ll\n");

   if (!generator.optimize(2, error)) {
      printf("OPTIMIZER: %s\n", (const char*)error);
      return -2;
   }
   if (generator.emitObject("/tmp/elena-translate.o", error)) {
      printf("object written to /tmp/elena-translate.o\n");
   }
   else printf("OBJECT: %s\n", (const char*)error);

   bool any = false;
   for (int i = 0 ; i < 256 ; i++) {
      if (stats.failedOpcode[i] > 0) {
         if (!any) {
            printf("\nfailures by opcode:\n");
            any = true;
         }
         _ELENA_::ident_c mnemonic[30];
         _ELENA_::ByteCodeCompiler::decode((_ELENA_::ByteCode)i, mnemonic);
         printf("  %02X %-12s %u\n", i, mnemonic, stats.failedOpcode[i]);
      }
   }

   return stats.failed == 0 ? 0 : 1;
}

// --- target selection ---

// consumed BEFORE the ordinary option loop: the leading "--" would parse
// as an empty option letter there. --target=? lists the table and exits.
static bool processTargetOptions(int argc, char* argv[], bool& exit)
{
   exit = false;
   for (int i = 1 ; i < argc ; i++) {
      if (strncmp(argv[i], "--target=", 9) != 0)
         continue;

      const char* name = argv[i] + 9;
      if (strcmp(name, "?") == 0) {
         size_t count = 0;
         const _ELENA_::TargetInfo* targets = _ELENA_::getTargetList(count);
         for (size_t t = 0 ; t < count ; t++) {
            printf("  %-10s %-34s %s-bit %s\n", targets[t].name, targets[t].triple,
               targets[t].is64Bit() ? "64" : "32",
               targets[t].isBigEndian() ? "BE" : "LE");
         }
         exit = true;
         return true;
      }

      const _ELENA_::TargetInfo* target = _ELENA_::getTargetByName(name);
      if (!target) {
         printf("unknown target '%s' (use --target=? for the list)\n", name);
         return false;
      }
      _ELENA_::setCurrentTarget(target);
   }
   return true;
}

// loads targets/<os>.cfg -- the operating-system axis of platform
// selection: the forwards that decide WHICH library modules implement the
// program-facing names. Loaded after the root configuration (so it can
// rely on library paths) and before the project (so a project can still
// override an individual forward).
static void loadTargetConfig(_ELC_::Project& project)
{
   const char* osName = _ELENA_::getCurrentTarget()->osConfigName();
   if (!osName)
      return;

   _ELENA_::Path configPath(project.appPath);
   configPath.combine("targets");
   configPath.combine(osName);
   configPath.appendExtension("cfg");

   project.loadConfig(configPath, false, false);
}

// --- llvmBuildProgram ---
//
// The link half the 2009/2015 system buried inside the JIT, redone as
// name-based emission: one global subject-id space per link unit, class
// tables merged down the parent chain at build time, everything else
// resolved by symbol name.

// common/tools.h defines min/max as macros; they poison the std headers
#undef min
#undef max

#include <map>
#include <string>
#include <vector>

namespace
{

struct BuildClosure
{
   std::vector<_ELENA_::Module*>       modules;
   std::map<std::string, unsigned int> subjects;
   _ELENA_::Module*                    current;

   BuildClosure() : current(NULL) {}

   unsigned int internSubject(const char* name)
   {
      std::map<std::string, unsigned int>::iterator it = subjects.find(name);
      if (it != subjects.end())
         return it->second;
      unsigned int id = (unsigned int)subjects.size() + 1;
      subjects[name] = id;
      return id;
   }

   unsigned int internMessage(_ELENA_::Module* module, unsigned int message)
   {
      unsigned int sign = (message & 0x00FFFFF0) >> 4;
      if (sign == 0)
         return message;
      const char* name = module->resolveSubject(sign);
      if (!name)
         return message;
      return (message & 0xFF00000F) | (internSubject(name) << 4);
   }
};

struct OwnMethod
{
   unsigned int message;      // interned
   unsigned int address;      // offset in the class code section
};

struct ClassData
{
   _ELENA_::Module*       module;
   std::string            name;
   std::string            parent;
   std::string            classClass;
   unsigned long long     flags;
   std::vector<OwnMethod> own;

   ClassData() : module(NULL), flags(0) {}
};

}

static const char* closureReferenceName(void* context, unsigned int reference)
{
   BuildClosure* closure = (BuildClosure*)context;
   return closure->current->resolveReference(reference);
}

static const char* closureConstantValue(void* context, unsigned int reference)
{
   BuildClosure* closure = (BuildClosure*)context;
   return closure->current->resolveConstant(reference);
}

static unsigned int closureInternMessage(void* context, unsigned int message)
{
   BuildClosure* closure = (BuildClosure*)context;
   return closure->internMessage(closure->current, message);
}

static _ELENA_::Module* loadBuildModule(const char* path)
{
   _ELENA_::Path modulePath(path);
   _ELENA_::FileReader reader(modulePath, _ELENA_::feRaw, false);
   _ELENA_::Module* module = new _ELENA_::Module();
   if (module->load(reader) != _ELENA_::lrSuccessful) {
      delete module;
      return NULL;
   }
   return module;
}

static int llvmBuildProgram(_ELC_::Project& project, const char* modulePath,
   const char* programSymbol, const char* output, const char* libPath)
{
   BuildClosure closure;

   _ELENA_::Module* program = loadBuildModule(modulePath);
   if (!program) {
      printf("cannot load the module %s\n", modulePath);
      return -1;
   }
   closure.modules.push_back(program);

   // the library closure: coarse for now -- system + its submodules
   if (libPath) {
      static const char* names[] = {
         "system.nl", "system/collections.nl", "system/text.nl", "system/io.nl",
         "system/routines.nl", "system/dynamic.nl", "system/calendar.nl",
         "system/math.nl", "system/core_routines.nl", "extensions.nl", NULL };
      for (int i = 0 ; names[i] ; i++) {
         _ELENA_::String<char, 512> path(libPath);
         path.append("/");
         path.append(names[i]);
         if (_ELENA_::Module* library = loadBuildModule(path))
            closure.modules.push_back(library);
      }
      printf("closure: %d modules\n", (int)closure.modules.size());
   }

   _ELENA_::LLVMGenerator generator;
   _ELENA_::GenError error;
   if (!generator.init(_ELENA_::getCurrentTarget(), error)) {
      printf("LLVM: %s\n", (const char*)error);
      return -1;
   }

   _ELENA_::TranslateCallbacks callbacks;
   callbacks.context = &closure;
   callbacks.referenceName = closureReferenceName;
   callbacks.constantValue = closureConstantValue;
   callbacks.internMessage = closureInternMessage;
   generator.setCallbacks(callbacks);

   // -- collect every class of the closure (own methods, interned)
   std::map<std::string, ClassData> classes;
   for (size_t m = 0 ; m < closure.modules.size() ; m++) {
      _ELENA_::Module* module = closure.modules[m];
      for (_ELENA_::ReferenceMap::Iterator it = module->References() ; !it.Eof() ; it++) {
         _ELENA_::ref_t reference = *it & ~_ELENA_::mskAnyRef;
         _ELENA_::_Memory* vmt = module->mapSection(reference | _ELENA_::mskVMTRef, true);
         if (!vmt)
            continue;

         ClassData data;
         data.module = module;
         data.name = it.key();

         unsigned int size = 0, classClassRef = 0;
         vmt->read(0, &size, 4);
         vmt->read(4, &classClassRef, 4);

         _ELENA_::ClassHeader header;
         vmt->read(8, &header, sizeof(header));
         data.flags = header.flags;
         if (header.parentRef) {
            const char* parent = module->resolveReference(header.parentRef & 0x00FFFFFF);
            if (parent) data.parent = parent;
         }
         if (classClassRef) {
            const char* metaName = module->resolveReference(classClassRef & 0x00FFFFFF);
            if (metaName) data.classClass = metaName;
         }

         size_t position = 8 + sizeof(_ELENA_::ClassHeader);
         size_t end = 4 + size;
         while (position + 8 <= end) {
            unsigned int message = 0, address = 0;
            vmt->read(position, &message, 4);
            vmt->read(position + 4, &address, 4);
            position += 8;
            if (address != 0xFFFFFFFF) {
               OwnMethod method;
               method.message = closure.internMessage(module, message);
               method.address = address;
               data.own.push_back(method);
            }
         }
         if (getenv("ELENA_LLVM_TRACE") && strstr(it.key(), "greeter")) {
            printf("  class %s parent='%s' cc='%s' own:", it.key(),
               data.parent.c_str(), data.classClass.c_str());
            for (size_t o = 0 ; o < data.own.size() ; o++)
               printf(" %X@%X", data.own[o].message, data.own[o].address);
            printf("\n");
         }
         classes[data.name] = data;
      }
   }
   printf("closure: %d classes, %d interned subjects\n",
      (int)classes.size(), (int)closure.subjects.size());

   // -- emit every class table, merged down the parent chain
   for (std::map<std::string, ClassData>::iterator it = classes.begin() ;
        it != classes.end() ; it++)
   {
      // ancestors first, self last: children override
      std::vector<const ClassData*> chain;
      const ClassData* at = &it->second;
      while (at) {
         chain.push_back(at);
         if (at->parent.empty())
            break;
         std::map<std::string, ClassData>::iterator parentIt = classes.find(at->parent);
         at = (parentIt != classes.end()) ? &parentIt->second : NULL;
      }

      std::map<unsigned int, std::string> merged;
      for (size_t c = chain.size() ; c > 0 ; c--) {
         const ClassData* link = chain[c - 1];
         std::string base = "elena.m.";
         base += link->name;
         base += ".";
         for (size_t o = 0 ; o < link->own.size() ; o++) {
            char hex[12];
            snprintf(hex, sizeof(hex), "%X", link->own[o].message);
            std::string fn = base + hex;
            for (size_t ch = 0 ; ch < fn.size() ; ch++)
               if (fn[ch] == '\'' || fn[ch] == ':' || fn[ch] == '#') fn[ch] = '.';
            merged[link->own[o].message] = fn;
         }
      }

      std::vector<unsigned int> messages;
      std::vector<const char*> functions;
      std::vector<std::string> storage;
      storage.reserve(merged.size());
      for (std::map<unsigned int, std::string>::iterator entry = merged.begin() ;
           entry != merged.end() ; entry++)
      {
         messages.push_back(entry->first);
         storage.push_back(entry->second);
      }
      for (size_t s = 0 ; s < storage.size() ; s++)
         functions.push_back(storage[s].c_str());

      generator.emitVMT(it->first.c_str(),
         it->second.classClass.empty() ? NULL : it->second.classClass.c_str(),
         it->second.flags, (unsigned int)messages.size(),
         messages.empty() ? NULL : &messages[0],
         functions.empty() ? NULL : &functions[0], error);
      generator.emitClassConstant(it->first.c_str(), error);
   }

   // -- translate every procedure of every module
   _ELENA_::TranslateStats stats;
   for (size_t m = 0 ; m < closure.modules.size() ; m++) {
      _ELENA_::Module* module = closure.modules[m];
      closure.current = module;

      for (_ELENA_::ReferenceMap::Iterator it = module->References() ; !it.Eof() ; it++) {
         _ELENA_::ref_t reference = *it & ~_ELENA_::mskAnyRef;

         _ELENA_::_Memory* symbol = module->mapSection(reference | _ELENA_::mskSymbolRef, true);
         if (symbol)
            llvmTranslateBody(generator, it.key(), symbol, 0, 0, stats);

         std::map<std::string, ClassData>::iterator classIt = classes.find(it.key());
         if (classIt != classes.end() && classIt->second.module == module) {
            _ELENA_::_Memory* body = module->mapSection(reference | _ELENA_::mskClassRef, true);
            if (!body)
               continue;
            for (size_t o = 0 ; o < classIt->second.own.size() ; o++) {
               _ELENA_::String<char, 300> name(it.key());
               name.append('.');
               name.appendHex(classIt->second.own[o].message);
               llvmTranslateBody(generator, name, body,
                  classIt->second.own[o].address,
                  classIt->second.own[o].message, stats);
            }
         }
      }
   }
   printf("translated %u/%u procedures\n", stats.translated, stats.procedures);
   if (stats.failed)
      printf("continuing with %u untranslated procedures (stubs)\n", stats.failed);

   if (!generator.emitEntry(programSymbol, error)) {
      printf("ENTRY: %s\n", (const char*)error);
      return -1;
   }

   generator.emitStubs();

   if (!generator.verify(error)) {
      printf("VERIFIER: %s\n", (const char*)error);
      return -2;
   }

   // the IR as translated, before and after optimization -- debugging aid
   _ELENA_::String<char, 512> irPath(output);
   irPath.append(".ll");
   generator.emitIR(irPath, error);

   if (!generator.optimize(2, error)) {
      printf("OPTIMIZER: %s\n", (const char*)error);
      return -2;
   }

   _ELENA_::String<char, 512> optPath(output);
   optPath.append(".opt.ll");
   generator.emitIR(optPath, error);

   _ELENA_::String<char, 512> asmPath(output);
   asmPath.append(".s");
   generator.emitAssembly(asmPath, error);

   _ELENA_::String<char, 512> objectPath(output);
   objectPath.append(".o");
   if (!generator.emitObject(objectPath, error)) {
      printf("OBJECT: %s\n", (const char*)error);
      return -2;
   }

   // the system linker finishes the job; the runtime archive lives next to
   // the compiler's binary directory. The linker choice follows the TARGET,
   // never the host.
   const _ELENA_::TargetInfo* target = _ELENA_::getCurrentTarget();
   _ELENA_::String<char, 128> linker;
   _ELENA_::String<char, 128> archive("libelena_rt");
   if (target == _ELENA_::getDefaultTarget()) {
      linker.copy("cc");
   }
   else if (target->os == _ELENA_::toWindows) {
      linker.copy(target->is64Bit() ? "x86_64-w64-mingw32-gcc" : "i686-w64-mingw32-gcc");
      archive.append("-");
      archive.append(target->name);
   }
   else {
      linker.copy(target->triple);
      linker.append("-gcc");
      archive.append("-");
      archive.append(target->name);
   }

   _ELENA_::String<char, 1024> command(linker);
   command.append(" \"");
   command.append(objectPath);
   command.append("\" \"");
   command.append(project.appPath);
   command.append("/../");
   command.append(archive);
   command.append(".a\" -lm -o \"");
   command.append(output);
   command.append("\"");

   printf("%s\n", (const char*)command);
   int status = system(command);
   if (status != 0) {
      printf("link failed (%d); the object is kept at %s\n", status,
         (const char*)objectPath);
      return -3;
   }

   printf("executable written to %s\n", output);
   return 0;
}

// --- Main function ---

const char* showPlatform(int platform)
{
   if (platform == _ELENA_::ptLinux32Console) {
      return ELC_LINUX32CONSOLE;
   }
   else if (platform == _ELENA_::ptLibrary) {
      return ELC_LIBRARY;
   }
   else return ELC_UNKNOWN;
}

int main(int argc, char* argv[])
{
   int    exitCode = 0;
   _ELC_::Project project;

   try {
      print(ELC_GREETING, ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION, ELC_REVISION_NUMBER);

      if (argc < 2) {
         // show help if no parameters proveded
         print(ELC_HELP_INFO);
         return -3;
      }

      // --target= must be consumed before the ordinary option loop
      bool exitAfterTargets = false;
      if (!processTargetOptions(argc, argv, exitAfterTargets))
         return -3;
      if (exitAfterTargets)
         return 0;

      // the --llvm-* verbs may appear anywhere on the command line (after
      // a --target=, typically); collect the verb and its positional
      // arguments before the ordinary option loop runs
      {
         const char* verb = NULL;
         const char* positional[4] = { NULL, NULL, NULL, NULL };
         int found = 0;
         for (int i = 1 ; i < argc ; i++) {
            if (strncmp(argv[i], "--target=", 9) == 0)
               continue;
            if (strncmp(argv[i], "--llvm-", 7) == 0) {
               verb = argv[i];
            }
            else if (verb && found < 4)
               positional[found++] = argv[i];
         }

         if (verb && strcmp(verb, "--llvm-selftest") == 0)
            return _ELENA_::llvmSelfTest("/tmp");

         // translate every procedure of a compiled module and report
         // opcode coverage -- validated against the real library corpus
         if (verb && strcmp(verb, "--llvm-translate") == 0) {
            if (found < 1) {
               printf("usage: elc [--target=<t>] --llvm-translate <module.nl>\n");
               return -3;
            }
            return llvmTranslateModule(positional[0]);
         }

         // translate a module, close it with stubs, link against the C
         // runtime and produce a runnable executable
         if (verb && strcmp(verb, "--llvm-build") == 0) {
            if (found < 3) {
               printf("usage: elc [--target=<t>] --llvm-build <module.nl> <program symbol> <output> [<libpath>]\n");
               return -3;
            }
            return llvmBuildProgram(project, positional[0], positional[1], positional[2],
               positional[3]);
         }
      }

      // Initializing..
      _ELENA_::Path configPath(project.appPath);
      configPath.combine(DEFAULT_CONFIG);
      project.loadConfig(configPath, true, false);

      // the operating-system axis: targets/<os>.cfg forwards
      loadTargetConfig(project);

      // Initializing..
      for (int i = 1 ; i < argc ; i++) {
         if (strncmp(argv[i], "--target=", 9) == 0)
            continue;   // already consumed
         if (argv[i][0]=='-') {
            project.setOption(argv[i] + 1);
         }
         else project.addSource(argv[i]);
      }

      project.initLoader();

      int platform = project.IntSetting(_ELENA_::opPlatform);

      // Greetings
      print(ELC_STARTING, (_ELENA_::ident_t)project.projectName, showPlatform(platform));

//      // Cleaning up
//      print("Cleaning up...");
//      project.cleanUp();

      // Compiling..
      print(ELC_COMPILING);

      _ELENA_::Path syntaxPath(project.appPath);
      syntaxPath.combine(SYNTAX_FILE);
      _ELENA_::FileReader syntaxFile(syntaxPath, _ELENA_::feRaw, false);
      if (!syntaxFile.isOpened())
         project.raiseError(errInvalidFile, syntaxPath);

      // compile normal project
      bool result = false;
      _ELENA_::Compiler compiler(&syntaxFile);
      setCompilerOptions(project, compiler);

      result = compiler.run(project);

      if (result)
         print(ELC_SUCCESSFUL_COMPILATION);
      else {
         exitCode = -1;
         print(ELC_WARNING_COMPILATION);
      }

      // Linking..
      if (platform == _ELENA_::ptLinux32Console) {
         print(ELC_LINKING);

         ImageHelper helper;
         _ELENA_::ExecutableImage image(&project, project.createJITCompiler(), helper);
         _ELENA_::I386Linker32 linker;
         linker.run(project, image/*, -1*/);

         print(ELC_SUCCESSFUL_LINKING);
      }
//      if (project.IntSetting(_ELENA_::opPlatform) == _ELENA_::ptWin32ConsoleMT) {
//         print(ELC_LINKING);
//
//         _ELENA_::ExecutableImage image(&project, project.createJITCompiler());
//         _ELENA_::Linker linker;
//
//         void* directory = image.resolveReference(_ELENA_::ConstantIdentifier(TLS_KEY), _ELENA_::mskNativeRDataRef);
//
//         linker.run(project, image, (ref_t)directory & ~_ELENA_::mskAnyRef);
//
//         print(ELC_SUCCESSFUL_LINKING);
//      }
//      else if (project.IntSetting(_ELENA_::opPlatform) == _ELENA_::ptVMWin32Console) {
//         print(ELC_LINKING);
//
//         if (_ELENA_::emptystr(project.StrSetting(_ELENA_::opVMPath)))
//            project.raiseError(ELC_WRN_MISSING_VMPATH);
//
//         _ELENA_::VirtualMachineClientImage image(
//            &project, project.createJITCompiler(), project.StrSetting(_ELENA_::opAppPath));
//
//         _ELENA_::Linker linker;
//         linker.run(project, image, -1);
//
//         print(ELC_SUCCESSFUL_LINKING);
//      }
   }
   catch(_ELENA_::InternalError& e) {
      print(ELC_INTERNAL_ERROR, e.message);
      exitCode = -2;

      project.cleanUp();
   }
   catch(_ELENA_::JITUnresolvedException& ex)
   {
      project.printInfo(errUnresovableLink, ex.reference);
      print(ELC_UNSUCCESSFUL);
      exitCode = -2;

      project.cleanUp();
   }
   catch(_ELENA_::JITConstantExpectedException& ex)
   {
      project.printInfo(errConstantExpectedLink, ex.reference);
      print(ELC_UNSUCCESSFUL);
      exitCode = -2;

      project.cleanUp();
   }
   catch(_ELENA_::_Exception&) {
      print(ELC_UNSUCCESSFUL);
      exitCode = -2;

//      project.cleanUp();
   }
   return exitCode;
}
