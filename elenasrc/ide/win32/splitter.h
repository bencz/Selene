//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      Splitter class header 
//                                              (C)2005-2007, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef splitterH
#define splitterH

#include "window.h"
#include "layout.h"

namespace _GUI_
{

class Splitter : public Window
{
   Control* _client;
   bool     _vertical;
   int      _cursor;

   POINT    _srcPos;
   bool     _mouseCaptured;

   virtual int getStyle() { return WS_CHILD | WS_VISIBLE; }
   virtual const TCHAR* getClassName() { return _vertical ? VSPLTR_WND_CLASS : HSPLTR_WND_CLASS; }

   virtual LRESULT Class_Proc(HWND hWnd, size_t Message, WPARAM wParam, LPARAM lParam);

   virtual bool onSetCursor();

   void shiftOn(int delta);

public:
   virtual bool isVisible(); 

   virtual size_t getWidth() const;
   virtual size_t getHeight() const;

   virtual void setCoordinate(int x, int y);
   virtual void setWidth(size_t width);
   virtual void setHeight(size_t height);
   
   virtual void resize();

   Splitter(Control* client, bool vertical);
};

} // _GUI_

#endif // splitterH

