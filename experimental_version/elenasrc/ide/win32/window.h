//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      Base Window class header
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef windowH
#define windowH

namespace _GUI_
{

class Control
{
protected:
   HINSTANCE _instance;
   HWND      _self;
   HWND      _parent;

   int       _left;
   int       _top;
   size_t      _width;
   size_t      _height;

   size_t      _minWidth;
   size_t      _minHeight;

   virtual int getStyle() { return 0; }
   virtual int getExStyle() { return 0; }
   virtual const TCHAR* getClassName() = 0;
   virtual const TCHAR* getCaption() { return NULL; }

   void notify(int code);
   void notify(int code, int extParam);
   void notify(int code, int extParam1, int extParam2);

public:
   HWND getHandle() const { return _self; }
   HINSTANCE getInstance() const { return _instance; }

   virtual int getLeft() const { return _left; }
   virtual int getTop() const { return _top; }
   virtual size_t getWidth() const { return _width; }
   virtual size_t getHeight() const { return _height; }

   virtual void setCoordinate(int x, int y);
   virtual void setConstraint(int minWidth, int minHeight);

   virtual void setWidth(size_t width);
   virtual void setHeight(size_t height);

   Rectangle getClientRectangle();

   virtual bool isVisible(); 

   virtual void create(HINSTANCE instance, HWND wndParent);

   virtual void show(bool maximized = false);
   virtual void hide();
   virtual void refreshClient();
   virtual void resize();

   void setFocus();

   void setFont(Font* font);

   Control(int left, int top, size_t width, size_t height);
   virtual ~Control() {}
};

// --- Window ---

class Window : public Control
{
protected:
   virtual LRESULT Class_Proc(HWND hWnd, size_t Message, WPARAM wParam, LPARAM lParam);
   
   void setCursor(int type);
   
   virtual void onResize() {} 
   virtual void onClose();
   virtual void onSetFocus() {}
   virtual void onLoseFocus() {}
   virtual bool onSetCursor() { return false; }   

public:
   static LRESULT CALLBACK Window_Proc(HWND hWnd, size_t Message, WPARAM wParam, LPARAM lParam);

   Window(int left, int top, size_t width, size_t height);
};

} // _GUI_

#endif // windowH
