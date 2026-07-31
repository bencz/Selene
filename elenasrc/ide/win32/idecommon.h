//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Win32 Common header
//      IDE common types header
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef ide32commonH
#define ide32commonH

#ifndef _WIN32_IE
#define _WIN32_IE 0x500
#endif
#define _WIN32_WINNT 0x500

#ifndef WINVER
#define WINVER 0x0500
#endif

#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>

#include "common.h"
#include "config.h"
#include "elenaconst.h"

#include "ideconst.h"

namespace _GUI_
{

// --- Point ---

struct Point
{
   int x;
   int y;

   Point operator +(const Point point) const
   {
      return Point(this->x + point.x, this->y + point.y);
   }

   Point operator -(const Point point) const
   {
      return Point(this->x - point.x, this->y - point.y);
   }

   Point& operator -=(const Point point)
   {
	  this->x -= point.x;
	  this->y -= point.y;

	  return *this;
   }

   Point& operator +=(const Point point)
   {
	  this->x += point.x;
	  this->y += point.y;

	  return *this;
   }

   bool operator ==(const Point point) const
   {
      return (this->x == point.x && this->y == point.y);
   }

   bool operator !=(const Point point) const
   {
      return (this->x != point.x || this->y != point.y);
   }

   Point()
   {
      x = 0;
      y = 0;
   }
   Point(int x, int y)
   {
      this->x = x;
      this->y = y;
   }
};

// --- Rectangle ---

struct Rectangle
{
   Point topLeft;
   Point bottomRight;

   int Width() const { return bottomRight.x - topLeft.x + 1; }
   int Height() const { return bottomRight.y - topLeft.y + 1; }

   bool isWithIn(Point point)
   {
      return topLeft.x <= point.x && topLeft.y <= point.y &&
		  point.x <= bottomRight.x && point.y <= bottomRight.y;
   }

   Rectangle()
   {
   }
   Rectangle(const Rectangle& rectangle)
   {
      topLeft = rectangle.topLeft;
      bottomRight = rectangle.bottomRight;
   }
   Rectangle(int left, int right, int width, int height)
   {
      topLeft = Point(left, right);
      bottomRight = topLeft + Point(width - 1, height - 1);
   }
};

// --- Colour ---

class Colour
{
   long _colour;

public:
   operator long() const { return _colour; }

   void set(unsigned int red, unsigned int green, unsigned int blue)
   {
      _colour = red | (green << 8) | (blue << 16);
   }

   Colour(unsigned int red, unsigned int green, unsigned int blue)
   {
      set(red, green, blue);
   }

   Colour(long colour = 0)
   {
	  _colour = colour;
   }
};

// --- Font ---

struct Font
{
   static _ELENA_::List<Font*> Cache;

   static Font* createFont(const TCHAR* fontName, int characterSet, int size,
		                        bool bold, bool italic);
   static void releaseFontCache();

public:
   HFONT        _fontID;

   const TCHAR* _fontName;
   bool         _bold;
   bool         _italic;
   int          _size;
   int          _characterSet;

   HFONT ID() const { return _fontID; }

   void create(HDC handler);
   void release();

   Font();
   Font(const TCHAR* faceName, int characterSet, int size, bool bold, bool italic);
   ~Font() { release(); }
};

// --- Style ---

class Canvas;

struct Style
{
   bool   valid;

   Colour foreground;
   Colour background;
   Font*  font;
   int    lineHeight;
   int    avgCharWidth;

   void validate(Canvas* canvas);

   Style();
   Style(Colour foreground, Colour background, Font* font);
   ~Style();
};

// --- Canvas ---

class Canvas
{
protected:
   HDC     _handler;
   bool    _cloned;

   HBRUSH  _oldBrush;
   HBRUSH  _brush;
   HFONT   _font;
   HFONT   _oldFont;
   HBITMAP _oldBitmap;
   HPEN    _oldPen;

   void setBrushColor(Colour background);
   void setPenColour(Colour foreground);
   void setFont(Font* font);

   void releaseFont();
   void releaseBrush();
   void releasePen();

public:
   static long Chrome();
   static long ButtonFace();
   static long ButtonShadow();

   bool isReleased() const;

   void clone(Canvas* canvas, size_t width, size_t height);

   void validateStyle(Style* style);

   int TextWidth(Style* style, const TCHAR* s, int length);

   void drawRectangle(Rectangle rect, Colour foreground, Colour background);
   void drawRectangle(Rectangle rect, Style style)
   {
      drawRectangle(rect, style.foreground, style.background);
   }

   void fillRectangle(Rectangle rect, Colour background);
   void fillRectangle(Rectangle rect, Style style)
   {
      fillRectangle(rect, style.background);
   }

   void drawEllipse(Rectangle rect, Colour foreground, Colour background);
   void drawEllipse(Rectangle rect, Style style)
   {
      drawEllipse(rect, style.foreground, style.background);
   }

   void setTransparentMode(bool on);

   void setClipArea(Rectangle rect);
   void drawText(Rectangle rect, const TCHAR* s, int length, Colour foreground, bool centered);
   void drawTextClipped(Rectangle rect, Font* font, int x, int y, const TCHAR* s, int length, Colour foreground, Colour background);
   void drawTextClipped(Rectangle rect, int x, int y, const TCHAR* s, int length, Style style)
   {
      drawTextClipped(rect, style.font, x, y, s, length, style.foreground, style.background);
   }
   void drawTextClippedTransporent(Rectangle rect, Font* font, int x, int y, const TCHAR* s, int length, Colour foreground);
   void drawTextClippedTransporent(Rectangle rect, int x, int y, const TCHAR* s, int length, Style style)
   {
      drawTextClippedTransporent(rect, style.font, x, y, s, length, style.foreground);
   }

   void copy(Rectangle rect, Point from, Canvas& sour);

   void release();

   Canvas();
   Canvas(HDC handler);
   virtual ~Canvas() { release(); }
};

// --- DateTime ---

struct DateTime
{
private:
   SYSTEMTIME _time;

public:
   static DateTime getFileTime(const TCHAR* path);

   bool operator > (const DateTime dt) const
   {
      if (_time.wYear > dt._time.wYear)
         return true;

      if (_time.wYear==dt._time.wYear) {
         if (_time.wMonth > dt._time.wMonth)
            return true;

         if (_time.wMonth==dt._time.wMonth) {
            if (_time.wDay > dt._time.wDay)
               return true;

            if (_time.wDay==dt._time.wDay) {
               if (_time.wHour > dt._time.wHour)
                  return true;

               if (_time.wHour==dt._time.wHour) {
                  if (_time.wMinute > dt._time.wMinute)
                     return true;

                  if (_time.wMinute==dt._time.wMinute) {
                     if (_time.wSecond > dt._time.wSecond)
                        return true;

                     if (_time.wSecond==dt._time.wSecond) {
                        return (_time.wMilliseconds > dt._time.wMilliseconds);
                     }
                  }
               }
            }
         }
      }
      return false;
   }

   DateTime()
   {
      _time.wYear = 0;
      //memset(&_time, 0, sizeof(_time));
   }
};

// --- Clipboard ---

class Clipboard
{
public:
   static bool isAvailable();

   bool open(HWND id);
   HGLOBAL create(int size);
   TCHAR* allocate(HGLOBAL buffer);
   void free(HGLOBAL buffer);
   void copy(HGLOBAL buffer);
   int getSize(HGLOBAL buffer);
   HGLOBAL get();

   void clear();
   void close();
};

// --- ExtNMHDR ---

struct ExtNMHDR
{
   NMHDR nmhrd;
   int   extParam;
};

// --- ExtNMHDR2 ---

struct ExtNMHDR2
{
   NMHDR nmhrd;
   int   extParam1;
   int   extParam2;
};

// --- MessageNMHDR ---

struct MessageNMHDR
{
   NMHDR        nmhrd;
   const TCHAR* message;
   int          param;
};

struct Message2NMHDR
{
   NMHDR        nmhrd;
   const TCHAR* message;
   int          param1;
   int          param2;
};

// --- TrackInfo ---
struct TrackInfo
{
   int row;
   int col;
   int disp;
   int length;

   TrackInfo()
   {
      row = disp = length = 0;
   }
   TrackInfo(int row, int disp, int length)
   {
      this->row = row;
      this->disp = disp;
      this->length = length;
      this->col = 0;
   }
   TrackInfo(int col, int row, int disp, int length)
   {
      this->col = col;
      this->row = row;
      this->disp = disp;
      this->length = length;
   }
};

// --- LineInfoNMHDR ---

struct LineInfoNMHDR
{
   NMHDR             nmhrd;
   const TCHAR*      file;
   TrackInfo         position;
};

// --- Exception ---

class Exception
{
   int _code;

public:
   Exception(int code)
   {
      _code = code;
   }
};

// --- ErrorManager ---

class ErrorManager
{
public:
   static void raiseError(int errorCode, const TCHAR* className)
   {
      _ELENA_::String message(_T("Error #"));
      message.appendHex(errorCode);
      message.append(_T(" in "));
      message.append(className);

      ::MessageBox(NULL, message, APP_NAME, MB_OK | MB_ICONSTOP);

      throw Exception(errorCode);
   }
};

// --- MsgBox ---

class MsgBox
{
public:
   static int show(HWND owner, const TCHAR* message, int type);
   static int show(HWND owner, const TCHAR* message, const TCHAR* param, int type);
   static int show(HWND owner, const TCHAR* message, const TCHAR* param1, const TCHAR* param2, int type);
};

inline RECT RectFromRectangle(_GUI_::Rectangle rect)
{
   RECT rc = {rect.topLeft.x, rect.topLeft.y, rect.bottomRight.x, rect.bottomRight.y};
   return rc;
}

} // _GUI_

#endif // ide32commonH
