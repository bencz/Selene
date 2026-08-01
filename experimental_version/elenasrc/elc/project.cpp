//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Compiler
//
//		This file contains the base class implementing ELENA Project interface.
//
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#include "elena.h"
// -------------------------------------------------------
#include "project.h"
#include "module.h"
#include "errors.h"

using namespace _ELENA_;

#define PMODULE_LEN _tcslen(PACKAGE_MODULE)

// --- Project ---

Project :: Project()
   : _modules(NULL, freeobj), _binaries(NULL, freeobj)
{
   _hasWarning = false;
}

const TCHAR* Project :: getLoadError(LoadResult result)
{
   switch(result)
   {
   case lrDuplicate:
      return errDuplicatedModule;
   case lrNotFound:
      return errUnknownModule;
   case lrWrongStructure:
      return errInvalidModule;
   case lrWrongVersion:
      return errInvalidModuleVersion;
   case lrIncompatibleEncoding:
      return errModuleEncoding;
   case lrIncompatibleByteOrder:
      return errModuleByteOrder;
   case lrIncompatibleWordSize:
      return errModuleWordSize;
   default:
      return NULL;
   }
}

inline const TCHAR* getName(const TCHAR* name, const TCHAR* package)
{
   if (name[0]=='$')
      name++;

   if (package!=NULL) {
      size_t skip = getlength(package) + 1;

      // Only skip the package prefix when the name is actually long enough to
      // carry one. This used to advance unconditionally, so compiling the
      // standard module (name "$elena") with a package of the same length
      // stepped past the terminator and built the output file name out of
      // uninitialised heap -- producing a differently-corrupted file name on
      // every run.
      if (getlength(name) >= skip) {
         name += skip;
      }
   }
   return name;
}

void Project :: nameToPath(const TCHAR* name, Path& path, const TCHAR* extension)
{
   name = getName(name, StrSetting(opPackage));

   path.nameToPath(name, extension);
}

void Project :: nameToPath(const TCHAR* name, LocalPath& path, const TCHAR* extension)
{
   name = getName(name, StrSetting(opPackage));

   path.nameToPath(name, extension);
}

_Module* Project :: resolveModule(const TCHAR* filePath)
{
   LocalReferenceName name(StrSetting(opPackage));
   name.pathToName(filePath);

   return _modules.get(name);
}

_Module* Project :: createModule(const TCHAR* filePath)
{
   LocalReferenceName name(StrSetting(opPackage));
   name.pathToName(filePath);

   if (BoolSetting(opStandart))
      name.copy(STANDARD_MODULE);

   _Module* module = new Module(name);

   if (!_modules.add(name, module, true)) {
      delete module;

      raiseError(errDuplicatedModule, filePath);
   }
   return module;
}

_Module* Project :: createDebugModule(const TCHAR* filePath)
{
   if (BoolSetting(opWithDebugInfo)) {
      LocalReferenceName name(StrSetting(opPackage));
      name.pathToName(filePath);

      return new Module(name);
   }
   else return NULL;
}

void Project :: saveModule(_Module* module)
{
   const TCHAR* name = module->Name();
   const TCHAR* outputPath = StrSetting(opOutputPath);
   LocalPath path(outputPath);

   nameToPath(name, path, MODULE_EXTENSION);

   createPath(outputPath, path);

   FileWriter writer(path, feRaw);
   if(!module->save(writer))
      raiseError(errCannotCreate, path.asString());
}

void Project :: saveDebugModule(_Module* module)
{
   const TCHAR* name = module->Name();
   const TCHAR* outputPath = StrSetting(opOutputPath);
   LocalPath path(outputPath);

   nameToPath(name, path, DEBUG_MODULE_EXTENSION);

   createPath(outputPath, path);

   FileWriter writer(path, feRaw);
   if(!module->save(writer))
      raiseError(errCannotCreate, path.asString());
}

const TCHAR* Project :: resolveForward(const TCHAR* forward)
{
   return _settings.get(opForwards, forward, DEFAULT_STR);
}

_Module* Project :: resolvePrimitive(const TCHAR* name, bool silentMode)
{
   _Module* binary = _binaries.get(name);
   if (!binary) {
      const TCHAR* path = _settings.get(opPrimitives, name, DEFAULT_STR);
      if (path) {
         binary = new Module();

         FileReader reader(path, feRaw);
         LoadResult result = ((Module*)binary)->load(reader);
         if(result!=lrSuccessful) {
            delete binary;

            if (!silentMode)
               raiseError(getLoadError(result), path);

            return NULL;
         }
         _binaries.add(name, binary);
      }
   }
   return binary;
}

_Module* Project :: loadModule(const TCHAR* path, bool silentMode)
{
   FileReader  reader(path, feRaw);
   Module*     module = new Module();

   LoadResult result = module->load(reader);
   if (result != lrSuccessful) {
      delete module;

      if (!silentMode)
         raiseError(getLoadError(result), path);

      return NULL;
   }
   else if (!_modules.add(module->Name(), module, true)) {
      delete module;

      result = lrDuplicate;

      if (!silentMode)
         raiseError(getLoadError(result), path);

      return NULL;
   }
   return module;
}

_Module* Project :: resolveModule(const TCHAR* referenceName, ref_t& reference, bool silentMode)
{
   while (isWeakReference(referenceName)) {
      referenceName = resolveForward(referenceName);
   }

   if (emptystr(referenceName))
      return NULL;

   if (compstr(referenceName, PACKAGE_MODULE, PMODULE_LEN) && referenceName[PMODULE_LEN]=='\'') {
      LocalNamespace package(referenceName + PMODULE_LEN + 1);

      _Module* primitive = resolvePrimitive(package, silentMode);
      reference = primitive ? primitive->mapReference(referenceName) : 0;

      return primitive;
   }
   else {
      LocalNamespace name(referenceName);

      _Module* module = _modules.get(name);
      if (!module) {
         LocalPath path;
         path.nameToPath(name, MODULE_EXTENSION);

         module = loadModule(path, silentMode);
      }
      reference = module ? module->mapReference(referenceName) : 0;

      return module;
   }
}
