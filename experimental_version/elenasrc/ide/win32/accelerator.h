//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      Win32 accelerator manager class header         
//                                              (C)2005-2007, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef acceleratorH
#define acceleratorH

namespace _GUI_
{

// --- AcceleratorManager ---

class AcceleratorManager
{
   HACCEL _table;

public:
   void create(HINSTANCE hInstance);

   bool translate(HWND hWnd, LPMSG msg);

   AcceleratorManager() { _table = NULL; }
};

} // _GUI_

#endif // acceleratorH
