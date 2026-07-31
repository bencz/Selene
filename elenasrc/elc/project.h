//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Compiler
//               
//		This header contains the declaration of the base class implementing 
//      ELENA Project interface.
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef projectH
#define projectH 1

namespace _ELENA_
{

// --- Project list types ---
typedef Dictionary2D<int, const TCHAR*> ProjectSettings;
typedef _Iterator<ProjectSettings::VItem, _MapItem<const TCHAR*, ProjectSettings::VItem>, const TCHAR*> SourceIterator;

// --- ELENA Project options ---
enum ProjectSetting
{
   opNone                  = 0x0000,

   // compiler options
   opAppPath		         = 0x0001,
   opProjectPath           = 0x0002,
   opLibPath               = 0x0003,
   opMapFile               = 0x0007,
   opPackage               = 0x0004,
   opStandart              = 0x0005,
   opTarget                = 0x0006,
   opOutputPath            = 0x0008,
   opEntry                 = 0x0009,
   opWithDebugInfo         = 0x000A,

   opLiteralClass          = 0x000B,
   opIntegerClass          = 0x000C,
   opRealClass             = 0x000D,
   opArrayClass            = 0x000E,

   // linker options
   opImageBase             = 0x0010,
   opSectionAlignment      = 0x0011,
   opFileAlignment         = 0x0012,
   opSystemType            = 0x0013,
   opGCHeapSize            = 0x0014,
   opSizeOfStackReserv     = 0x0016,
   opSizeOfStackCommit     = 0x0017,
   opSizeOfHeapReserv      = 0x0018,
   opSizeOfHeapCommit      = 0x0019,

   opWarnOnUnresolved      = 0x0021,
   opWarnOnWeakUnresolved  = 0x0022,
   
   opPrimitives            = 0x0030,
   opForwards              = 0x0031,
   opSources               = 0x0032,
   opTemplates             = 0x0033,
};

// --- Project ---

class Project
{
protected:
   bool            _hasWarning;

   ProjectSettings _settings;
   ModuleMap       _modules;
   ModuleMap       _binaries;

   virtual _Module* loadModule(const TCHAR* path, bool silentMode);

   virtual _Module* resolveModule(const TCHAR* path);

public:
   virtual _Module* resolvePrimitive(const TCHAR* name, bool silentMode);

   virtual const TCHAR* getLoadError(LoadResult result);

   bool HasWarnings() const { return _hasWarning; }

   virtual int IntSetting(ProjectSetting key, int defaultValue = 0)
   {
      return _settings.get(key, defaultValue);
   }

   virtual const TCHAR* StrSetting(ProjectSetting key)
   {
      return _settings.get(key, DEFAULT_STR);
   }

   virtual bool BoolSetting(ProjectSetting key)
   {
      return (_settings.get(key, 0) != 0);
   }

   SourceIterator getSourceIt()
   {
      return _settings.getIt(opSources);
   }

   virtual int getTabSize() { return 4; }

   virtual void printInfo(const TCHAR* msg, ...) = 0;
   virtual void raiseError(const TCHAR* msg, ...) = 0;

   void indicateWarning() 
   {
      _hasWarning = true;
   }

   void nameToPath(const TCHAR* name, Path& path, const TCHAR* extension);
   void nameToPath(const TCHAR* name, LocalPath& path, const TCHAR* extension);

   virtual _Module* createModule(const TCHAR* filePath);
   virtual void saveModule(_Module* module);

   virtual _Module* createDebugModule(const TCHAR* filePath);
   virtual void saveDebugModule(_Module* module);

   virtual const TCHAR* resolveForward(const TCHAR* forward);

   virtual _Module* resolveModule(const TCHAR* referenceName, ref_t& reference, bool silentMode = false);

   Project();
   virtual ~Project() {}   
};

} // _ELENA_

#endif // projectH
