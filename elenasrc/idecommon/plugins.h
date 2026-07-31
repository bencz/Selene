//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      Plugin shared header
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef pluginsH
#define pluginsH

namespace _GUI_
{

// Plugin register function name
#define PLUGIN_REGISTER_FUN   "RegisterPlugin"

// --- PluginResult ---

enum PluginResult
{
   pgrNone = 0,
   pgrSuccessful = 1,
   pgrNeedToRepaint = 3
};

// --- _Editor ---

class _Document
{
};

// --- Hook declarations ---

typedef PluginResult(*OnKeyPressedType)(TCHAR ch, _Document* document);
typedef PluginResult(*OnKeyDownType)(int keyCode, bool kbShift, bool kbCtrl, _Document* document);

// --- _PluginManager ---

class _PluginManager
{
public:
   virtual OnKeyPressedType registerOnKeyPressed(OnKeyPressedType hook) = 0;
   virtual OnKeyDownType registerOnKeyDownType(OnKeyDownType hook) = 0;

   virtual ~_PluginManager() {}
};

// --- Plugin importing functions ---

typedef int(__cdecl *RegisterFunction)(_PluginManager* manager);

} // _GUI_

#endif	// pluginsH