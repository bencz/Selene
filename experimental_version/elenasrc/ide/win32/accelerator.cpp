//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      Win32 accelerator manager class implementation
//                                              (C)2005-2007, by Alexei Rakov
//---------------------------------------------------------------------------

#include "idecommon.h"
// --------------------------------------------------------------------------
#include "accelerator.h"

using namespace _GUI_;

// --- AcceleratorManager ---

void AcceleratorManager :: create(HINSTANCE hInstance)
{
   _table = ::LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_IDE_ACCELERATORS));
}

bool AcceleratorManager :: translate(HWND hWnd, LPMSG msg)
{
   return ::TranslateAccelerator(hWnd, _table, msg) != 0;  
}
