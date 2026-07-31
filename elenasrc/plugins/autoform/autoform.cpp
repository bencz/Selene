//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE plugins
//
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#include "autoform.h"

using namespace _GUI_;

Plugin plugin;

// --- Plugin ---

PluginResult Plugin :: onKeyDown(int keyCode, bool kbShift, bool kbCtrl, _Document* document)
{
   if (_previousOnKeyDown) {
      return _previousOnKeyDown(keyCode, kbShift, kbCtrl, document);
   }
   else return PluginResult::pgrNone;
}

PluginResult Plugin :: onKeyPressed(TCHAR ch, _Document* document)
{
   if (_previousOnKeyPressed) {
      _previousOnKeyPressed(ch, document);
   }
   else return PluginResult::pgrNone;
}

// --- Plugin Hooks ---

PluginResult onKeyPressedHook(TCHAR ch, _Document* document)
{
   return plugin.onKeyPressed(ch, document);
}

PluginResult onKeyDownHook(int keyCode, bool kbShift, bool kbCtrl, _Document* document)
{
   return plugin.onKeyDown(keyCode, kbShift, kbCtrl, document);
}

// --- RegisterPlugin ---

EXTERN_DLL_EXPORT int RegisterPlugin(_PluginManager* manager)
{
   OnKeyPressedType previousOnKeyPressed = manager->registerOnKeyPressed(onKeyPressedHook);
   OnKeyDownType previousOnKeyDown = manager->registerOnKeyDownType(onKeyDownHook);

   plugin.init(previousOnKeyPressed, previousOnKeyDown);

   return -1;
}
