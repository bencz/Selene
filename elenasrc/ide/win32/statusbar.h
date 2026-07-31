//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      StatusBar class header
//                                              (C)2005-2007, by Alexei Rakov
//---------------------------------------------------------------------------


#ifndef statusbarH
#define statusbarH

#include "window.h"

namespace _GUI_
{

class StatusBar : public Control
{
   int  _partCount;
   int* _partWidths;

   HLOCAL _hMem;
   LPINT  _parts;
          
   virtual int getStyle() { return WS_CHILD | SBARS_SIZEGRIP; }
   virtual const TCHAR* getClassName() { return STATUSCLASSNAME; }

public:
   bool setText(int part, const TCHAR* str);

   virtual void setHeight(size_t) { _height = 20; }

   virtual void resize();

   StatusBar(int partCount, int* widths);
   virtual ~StatusBar();
};


} // _GUI_

#endif // statusbarH
