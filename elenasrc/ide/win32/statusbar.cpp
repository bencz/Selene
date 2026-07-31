//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      StatusBar class implementation
//                                              (C)2005-2007, by Alexei Rakov
//---------------------------------------------------------------------------

#include "idecommon.h"
// --------------------------------------------------------------------------
#include "statusbar.h"

using namespace _GUI_;

// --- StatusBar ---

StatusBar :: StatusBar(int partCount, int* widths)
   : Control(0, 100, 100, 20)
{
   _partCount = partCount;
   _partWidths = new int[partCount];
   
   for (int i = 0 ; i < _partCount ; i++)
      _partWidths[i] = widths[i];
       
   _hMem = ::LocalAlloc(LHND, sizeof(int) * _partCount);
   _parts = (LPINT)::LocalLock(_hMem);
}

StatusBar :: ~StatusBar()
{
   delete[] _partWidths;
   
   if (_hMem) {
      ::LocalUnlock(_hMem);
      ::LocalFree(_hMem);
   }
}

bool StatusBar :: setText(int part, const TCHAR* str)
{
   if (part > _partCount) 
      return false;
        
   return (::SendMessage(_self, SB_SETTEXT, part, (LPARAM)str) == TRUE);
};

void StatusBar :: resize()
{
   Control::resize();
   
   int width = _width - 15;
   for (int i = _partCount - 1 ; i >= 0 ; i--) {
      _parts[i] = width;
      width -= _partWidths[i];
   }
   ::SendMessage(_self, SB_SETPARTS, (WPARAM)_partCount, (LPARAM)_parts);
}

