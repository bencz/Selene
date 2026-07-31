//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      Splitter class implementation
//                                              (C)2005-2007, by Alexei Rakov
//---------------------------------------------------------------------------

#include "idecommon.h"
// --------------------------------------------------------------------------
#include "splitter.h"

using namespace _GUI_;

#ifndef WH_MOUSE_LL
#define WH_MOUSE_LL 14
#endif

static HWND	 hWndMouse = NULL;
static HHOOK hookMouse = NULL;

static LRESULT CALLBACK hookProcMouse(int nCode, WPARAM wParam, LPARAM lParam)
{
   if(nCode >= 0) {
      switch (wParam) {
         case WM_MOUSEMOVE:
         case WM_NCMOUSEMOVE:
            ::PostMessage(hWndMouse, wParam, 0, 0);
            break;
         case WM_LBUTTONUP:
         case WM_NCLBUTTONUP:
            ::PostMessage(hWndMouse, wParam, 0, 0);
            return TRUE;
         default:
            break;
      }
   }
   return ::CallNextHookEx(hookMouse, nCode, wParam, lParam);
}

// --- Splitter ---

Splitter :: Splitter(Control* client, bool vertical)
   : Window(client->getLeft(), client->getTop(), 0, 0)
{
   _client = client;
   _vertical = vertical;
   _mouseCaptured = false;

   _minWidth = 4;
   _minHeight = 4;
   _cursor = _vertical ? CURSOR_SIZEWE : CURSOR_SIZENS;

   setWidth(_client->getWidth());
   setHeight(_client->getHeight());
}

bool Splitter :: onSetCursor()
{
   setCursor(_cursor);

   return true;
}

bool Splitter :: isVisible()
{
   if (_client->isVisible()!=Window::isVisible()) {
      if (_client->isVisible()) {
         show();
      }
      else hide();
   }
   return Window::isVisible();
}

size_t Splitter :: getWidth() const
{
   if (_vertical) {
	  return _client->getWidth() + _width;
   }
   else return _height;
}

size_t Splitter :: getHeight() const
{
   if (!_vertical) {
	  return _client->getHeight() + _height;
   }
   else return _width;
}

void Splitter :: setCoordinate(int x, int y)
{   
   if (_vertical) {
      _client->setCoordinate(x, y);
      _left = x + _client->getWidth();
	  _top = y;
   }
   else {
      _left = x;
	  _top = y;
	  _client->setCoordinate(x, y + 3);
   }
}

void Splitter :: setWidth(size_t width)
{
   if (_vertical) {
	  if (width > 3) {
         _client->setWidth(width - 3);
	  }
	  else _client->setWidth(1);

      _left = _client->getLeft() + _client->getWidth();
      _width = 3;
   }
   else {
      _client->setWidth(width);
	  Control::setWidth(width);
   }
}

void Splitter :: setHeight(size_t height)
{
   if (!_vertical) {
	  if (height > 3) {
         _client->setHeight(height - 3);
	  }
	  else _client->setHeight(1);

      _height = 3;
   }
   else {
      _client->setHeight(height);
	  Control::setHeight(height);
   }
}

void Splitter :: resize()
{
   _client->resize();
   Control::resize();
}

LRESULT Splitter :: Class_Proc(HWND hWnd, size_t Message, WPARAM wParam, LPARAM lParam)
{
   switch (Message)
   {
      case WM_LBUTTONDOWN:
         hWndMouse = hWnd;
         hookMouse = ::SetWindowsHookEx(WH_MOUSE_LL, (HOOKPROC)hookProcMouse, _instance, 0);

         ::SetCapture(_self);
         ::GetCursorPos(&_srcPos);
         _mouseCaptured = true;
         break;
      case WM_LBUTTONUP:
      case WM_NCLBUTTONUP:
         if (hookMouse) {
            ::UnhookWindowsHookEx(hookMouse);
            hookMouse = NULL;
         }
         ::SetCapture(NULL);
         _mouseCaptured = false;
         break;
      case WM_MOUSEMOVE:
      case WM_NCMOUSEMOVE:
         if (_mouseCaptured) {
            POINT	destPos;
            ::GetCursorPos(&destPos);

			if (!_vertical && (_srcPos.y != destPos.y)) {
               shiftOn(_srcPos.y - destPos.y);
            }
            else if (_srcPos.x != destPos.x) {
               shiftOn(destPos.x - _srcPos.x);
            }
            _srcPos = destPos;
         }
         break;
   }
   return Window::Class_Proc(hWnd, Message, wParam, lParam);
}

void Splitter :: shiftOn(int delta)
{
   if (!_vertical) {
	  if (getHeight() + delta > _minHeight) {
         setHeight(getHeight() + delta);
	  }
	  else setHeight(_minHeight);
   }
   else {	   
	  if (getWidth() + delta > _minWidth) {
         setWidth(getWidth() + delta);
	  }
	  else setWidth(_minWidth);
   } 
   notify(IDM_LAYOUT_CHANGED);
}
