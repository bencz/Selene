//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Common Library
//
//		This file contains the common templates, classes,
//		structures, functions and constants
//                                              (C)2005-2008, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef commonH
#define commonH 1

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>

// --- Common defintions ---
#define ref_t       size_t
#define DEFAULT_STR (const TCHAR*)NULL

// --- Common headers ---
//
// "unicode.h" and "platform.h" exist once per platform under common/win32 and
// common/posix. The build system places exactly one of those directories on the
// include path, so these resolve without conditional compilation.
#include "unicode.h"
#include "platform.h"
#include "tools.h"
#include "altstrings.h"
#include "streams.h"
#include "dump.h"
#include "lists.h"
#include "files.h"

namespace _ELENA_
{
// --- Common mapping type definitions ---
typedef Dictionary2D<const TCHAR*, const TCHAR*> ConfigSettings;
typedef _Iterator<ConfigSettings::VItem, _MapItem<const TCHAR*, ConfigSettings::VItem>, const TCHAR*> ConfigCategoryIterator;

typedef Map<const TCHAR*, TCHAR*> CategoryMap;

// --- Base Config File ---
class _ConfigFile
{
public:
   virtual bool load(const TCHAR* path) = 0;

   virtual ConfigCategoryIterator getCategoryIt(const TCHAR* name) = 0;

   virtual const TCHAR* getSetting(const TCHAR* category, const TCHAR* key, const TCHAR* defaultValue = NULL) = 0;
   virtual int getIntSetting(const TCHAR* category, const TCHAR* key, int defaultValue = 0)
   {
      const TCHAR* value = getSetting(category, key);
      if (value) {
         return _ttoi(value);
      }
      else return defaultValue;
   }

   virtual bool getBoolSetting(const TCHAR* category, const TCHAR* key, bool defaultValue = false)
   {
      const TCHAR* value = getSetting(category, key);
      if (value) {
         return compstr(value, _T("-1"));
      }
      else return defaultValue;
   }

   virtual ~_ConfigFile() {}
};

} // _ELENA_

#endif // commonH
