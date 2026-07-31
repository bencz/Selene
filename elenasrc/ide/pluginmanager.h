//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      Plugin manager header
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef pluginManagerH
#define pluginManagerH

#include "plugins.h"

namespace _GUI_
{

// --- Pluginmanager ---

   class PluginManager : public _PluginManager
{
   OnKeyPressedType _onKeyPressed;
   OnKeyDownType    _onKeyDown;

public:
   virtual OnKeyPressedType registerOnKeyPressed(OnKeyPressedType hook)
   {
      OnKeyPressedType old = _onKeyPressed;
      _onKeyPressed = hook;

      return old;
   }

   virtual OnKeyDownType registerOnKeyDownType(OnKeyDownType hook)
   {
      OnKeyDownType old = _onKeyDown;
      _onKeyDown = hook;

      return old;
   }

   void registerPlagin(const TCHAR* path);

   PluginResult onKeyPressed(TCHAR ch, _Document* document)
   {
      if (_onKeyPressed != NULL) {
         return _onKeyPressed(ch, document);
      }
      else return pgrNone;
   }

   PluginResult onKeyDown(int keyCode, bool kbShift, bool kbCtrl, _Document* document)
   {
      if (_onKeyDown != NULL) {
         return _onKeyDown(keyCode, kbShift, kbCtrl, document);
      }
      else return pgrNone;
   }

   PluginManager()
   {
      _onKeyPressed = NULL;
      _onKeyDown = NULL;
   }
};

} // _GUI_

#endif	// pluginManagerH