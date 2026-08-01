//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      Plugin manager header
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#include "idecommon.h"
// --------------------------------------------------------------------------
#include "pluginmanager.h"

using namespace _GUI_;

void PluginManager :: registerPlagin(const TCHAR* path)
{
   HMODULE hModule = ::LoadLibrary(path);
   if (hModule) {
      RegisterFunction function = (RegisterFunction)::GetProcAddress(hModule, PLUGIN_REGISTER_FUN);

      function(this);
   }
}