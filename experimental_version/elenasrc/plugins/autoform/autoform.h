//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE plugins
//
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef autoformH
#define autoformH 1

#ifndef WINVER
#define WINVER 0x0500
#endif

#include <windows.h>
#include "common.h"
#include "plugins.h"

#define EXTERN_DLL_EXPORT extern "C" __declspec(dllexport)

// declare register function

EXTERN_DLL_EXPORT int RegisterPlugin(_GUI_::_PluginManager* manager);

// --- Plugin ---

namespace _GUI_
{

class Plugin
{
   OnKeyPressedType _previousOnKeyPressed;
   OnKeyDownType    _previousOnKeyDown;

public:
   void init(OnKeyPressedType previousOnKeyPressed, OnKeyDownType previousOnKeyDown)
   {
      _previousOnKeyPressed = previousOnKeyPressed;
      _previousOnKeyDown = previousOnKeyDown;
   }

   PluginResult onKeyDown(int keyCode, bool kbShift, bool kbCtrl, _Document* document);
   PluginResult onKeyPressed(TCHAR ch, _Document* document);

   Plugin()
   {
   }
};

} // _GUI_

#endif // autoformH