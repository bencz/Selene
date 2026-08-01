//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      Base Window class implementation
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#include "idecommon.h"
// --------------------------------------------------------------------------
#include "window.h"

using namespace _GUI_;
using namespace _ELENA_;

// --- Control ---

Control :: Control(int left, int top, size_t width, size_t height)
{
   _self = NULL;
   _parent = NULL;
   _instance = NULL;
   _minWidth = 0;
   _minHeight = 0;

   _left = left;
   _top = top;
   _width = width;
   _height = height;
}

void Control :: setConstraint(int minWidth, int minHeight)
{
   _minWidth = minWidth;
   _minHeight = minHeight;
}

void Control :: setCoordinate(int x, int y)
{
   _left = x;
   _top = y;
}

void Control :: setWidth(size_t width)
{
   if (width < _minWidth) {
      _width = _minWidth;
   }
   else _width = width;
}

void Control :: setHeight(size_t height)
{
   if (height < _minHeight) {
      _height = _minHeight;
   }
   else _height = height;
}

void Control :: create(HINSTANCE instance, HWND wndParent)
{
   _instance = instance; 
   _parent = wndParent;
   _self = ::CreateWindowEx(
      getExStyle(), 
      getClassName(), getCaption(), getStyle(), 
	  _left, _top, _width, _height, wndParent, NULL, instance, (LPVOID)this);

   if (!_self) {
      ErrorManager::raiseError(ERR_WND_NOT_OPENED, getClassName());
   }
}

void Control :: refreshClient()
{
   ::InvalidateRect(_self, NULL, false);
   ::UpdateWindow(_self);
}

void Control :: show(bool maximized)
{
   ::ShowWindow(_self, maximized ? SW_MAXIMIZE : SW_SHOW);
}

void Control :: hide()
{
   ::ShowWindow(_self, SW_HIDE);
}

void Control :: setFocus()
{
   ::SetFocus(_self);
}

void Control :: resize()
{
   ::MoveWindow(_self, _left, _top, _width, _height, TRUE);

   refreshClient();
}

_GUI_::Rectangle Control :: getClientRectangle()
{
   RECT rc={0,0,0,0};
   ::GetClientRect(_self, &rc);

   return Rectangle(rc.left, rc.top, rc.right, rc.bottom);
}

bool Control :: isVisible()
{
   return ::IsWindowVisible(_self) ? true : false;
}

void Control :: notify(int code)
{
   NMHDR notification;

   notification.code = code;
   notification.hwndFrom = _self;

   ::SendMessage(_parent, WM_NOTIFY, 0, (LPARAM)&notification);
}

void Control :: notify(int code, int extParam)
{
   ExtNMHDR notification;

   notification.nmhrd.code = code;
   notification.nmhrd.hwndFrom = _self;
   notification.extParam = extParam;

   ::SendMessage(_parent, WM_NOTIFY, 0, (LPARAM)&notification);
}

void Control :: notify(int code, int extParam1, int extParam2)
{
   ExtNMHDR2 notification;

   notification.nmhrd.code = code;
   notification.nmhrd.hwndFrom = _self;
   notification.extParam1 = extParam1;
   notification.extParam2 = extParam2;

   ::SendMessage(_parent, WM_NOTIFY, 0, (LPARAM)&notification);
}

void Control :: setFont(Font* font)
{
   ::SendMessage(_self, WM_SETFONT, (WPARAM)font->ID(), 0);
}

// --- Window ---

Window :: Window(int left, int top, size_t width, size_t height)
   : Control(left, top, width, height)
{
}

LRESULT CALLBACK Window :: Window_Proc(HWND hWnd, size_t Message, WPARAM wParam, LPARAM lParam)
{
   Window* window = (Window*)::GetWindowLong(hWnd, GWL_USERDATA);
   if (window==NULL) {
      if (Message==WM_CREATE) {
         window = (Window*)((LPCREATESTRUCT)lParam)->lpCreateParams;
         ::SetWindowLong(hWnd, GWL_USERDATA, (LONG)window);

         return window->Class_Proc(hWnd, Message, wParam, lParam);
      }
      else return ::DefWindowProc(hWnd, Message, wParam, lParam);
   }
   else return window->Class_Proc(hWnd, Message, wParam, lParam);
}

LRESULT Window :: Class_Proc(HWND hWnd, size_t Message, WPARAM wParam, LPARAM lParam)
{
   switch (Message)
   {
      case WM_SIZE:
         if (wParam != SIZE_MINIMIZED) {
            onResize();
         }
         return 0;
      case WM_SETFOCUS:
         onSetFocus();
         return 0;
      case WM_KILLFOCUS:
         onLoseFocus();
         return 0;
      case WM_CLOSE:
         onClose();
         return 0;
      case WM_SETCURSOR:
         if (LOWORD(lParam) == HTCLIENT) {
            if (onSetCursor())
               return TRUE;
         }
         break;
   }
   return ::DefWindowProc(hWnd, Message, wParam, lParam);
}

void Window :: onClose()
{
   ::DestroyWindow(_self);
}

void Window :: setCursor(int type)
{
   HCURSOR cursor;

   switch (type) {
      case CURSOR_TEXT:
         cursor = ::LoadCursor(NULL, IDC_IBEAM);
         break;
      case CURSOR_ARROW:
         cursor = ::LoadCursor(NULL, IDC_ARROW);
         break;
	  case CURSOR_SIZENS:
		 cursor = ::LoadCursor(NULL, IDC_SIZENS);
		 break;
	  case CURSOR_SIZEWE:
		 cursor = ::LoadCursor(NULL, IDC_SIZEWE);
		 break;
      default:
         cursor = ::LoadCursor(NULL, IDC_IBEAM);
   }
   ::SetCursor(cursor);
}
