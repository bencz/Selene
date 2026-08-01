//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Common Library
//
//		This file contains Config File class implementation
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "config.h"

using namespace _ELENA_;

// --- IniConfigFile ---

IniConfigFile :: IniConfigFile()
{
}

IniConfigFile :: IniConfigFile(bool allowDuplicates)
   : _settings(allowDuplicates)
{
}

bool IniConfigFile :: load(const TCHAR* path)
{
   String line(256);
   String key(256);
   String subKey(256);
   TextFileReader reader(path, feAutodetect);

   if (!reader.isOpened())
      return false;

   while (reader.readString(line)) {
      line.trim('\n');
      line.trim('\r');
      line.trim(' ');

      if (emptystr(line)) continue;

      // Comments. The parser had no comment syntax at all, so a '#' line before
      // the first [section] hit the "no current section" check below and made
      // the whole file fail to load -- silently, since callers pass
      // requiered=false for optional configs.
      if (line[0] == '#' || line[0] == ';') continue;

      const TCHAR* value = line;
      if (value[0]=='[' && value[getlength(value) - 1]==']') {
         if (getlength(value) < 3) {
            return false;
         }
         key.copy(value + 1, getlength(value) - 2);
      }
      else {
         if (emptystr(key)) {
            return false;
         }
         if (chrpos(value, '=') != -1) {
            int pos = chrpos(value, '=');
            subKey.copy(line, pos);

            _settings.add(key, subKey, strdup(value + pos + 1));
         }
         else _settings.add(key, value, DEFAULT_STR);
      }
   }
   return true;
}

bool IniConfigFile :: save(const TCHAR* path, FileEncoding encoding)
{
   TextFileWriter  writer(path, encoding);

   if (!writer.isOpened())
      return false;

   // goes through the section keys
   _Iterator<ConfigSettings::VItem, _MapItem<const TCHAR*, ConfigSettings::VItem>, const TCHAR*> it = _settings.start();
   while (!it.Eof()) {
      ConfigCategoryIterator cat_it = _settings.getIt(it.key());
      if (!cat_it.Eof()) {
         writer.writeChar('[');
         writer.writeStr(it.key());
         writer.writeLine(_T("]"));

         while (!cat_it.Eof()) {
            writer.writeStr(cat_it.key());
            if (!emptystr(*cat_it)) {
               writer.writeChar('=');
               writer.writeLine(*cat_it);
            }
            else writer.writeLine(NULL);

            cat_it++;
         }
         writer.writeLine(NULL);
      }
      it++;
   }
   return true;
}

const TCHAR* IniConfigFile :: getSetting(const TCHAR* category, const TCHAR* key, const TCHAR* defaultValue)
{
   return _settings.get(category, key, defaultValue);
}

void IniConfigFile :: setSetting(const TCHAR* category, const TCHAR* key, const TCHAR* value)
{
   _settings.add(category, key, _ELENA_::strdup(value));
}

void IniConfigFile :: setSetting(const TCHAR* category, const TCHAR* key, int value)
{
   LocalString<15> string;
   string.appendInt(value);

   _settings.add(category, key, string.Clone());
}

void IniConfigFile :: setSetting(const TCHAR* category, const TCHAR* key, size_t value)
{
   LocalString<15> string;
   string.appendInt(value);

   _settings.add(category, key, string.Clone());
}

void IniConfigFile :: setSetting(const TCHAR* category, const TCHAR* key, bool value)
{
   _settings.add(category, key, value ? _T("-1") : _T("0"));
}

void IniConfigFile :: clear(const TCHAR* category, const TCHAR* key)
{
	_settings.clear(category, key);
}

void IniConfigFile :: clear(const TCHAR* category)
{
   _settings.clear(category);
}

void IniConfigFile :: clear()
{
    _settings.clear();
}
