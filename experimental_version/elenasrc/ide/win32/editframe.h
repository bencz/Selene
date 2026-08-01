//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      EditFrame class header
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef editframeH
#define editframeH

#include "window.h"
#include "menu.h"
#include "document.h"
#include "pluginmanager.h"

namespace _GUI_
{

// --- StyleInfo ---

struct StyleInfo
{
   Colour      foreground;
   Colour      background;
   const TCHAR* faceName;
   int         characterSet;
   int         size;
   bool        bold;
   bool        italic;
};

// --- ViewStyle ---

class ViewStyles
{
   Style _styles[STYLE_MAX + 1];
   int   _lineHeight;
   int   _marginWidth;

public:
   Style operator[](unsigned int index)
   {
      return _styles[index];
   }

   int getLineHeight() const { return _lineHeight; }
   int getMarginWidth() const { return _marginWidth; }

   void assign(StyleInfo* styles, int lineHeight, int marginWidth);

   void validate(Canvas* canvas);

   ViewStyles() {}
};

// --- EditFrame ---

class EditFrame : public Window
{
public:
   typedef List<Document*> Documents;

private:
   Window*        _appWindow;
   PluginManager* _pluginManager;

   Canvas         _zbuffer;
   bool           _cached;
   bool           _caretValid;
   bool           _caretVisible;
   bool           _mouseCaptured;

   Documents      _documents;
   Document*      _currentDoc;
   bool           _readOnly;
   bool           _tabUsing;
   size_t         _tabSize;

   ContextMenu    _contextMenu;

   ViewStyles     _styles[SCHEME_COUNT];
   int            _scheme;

   bool           _lineNumbersVisible;

   virtual int getStyle() { return WS_CHILD | WS_VSCROLL | WS_HSCROLL | WS_CLIPCHILDREN | WS_EX_RTLREADING; }
   virtual int getExStyle() { return WS_EX_CLIENTEDGE; }
   virtual const TCHAR* getClassName() { return EDIT_WND_CLASS; }
   virtual const TCHAR* getCaption() { return _T("Editor"); }

   virtual LRESULT Class_Proc(HWND hWnd, size_t Message, WPARAM wParam, LPARAM lParam);
   virtual void paint(Canvas& canvas, Rectangle clientRect);

   virtual void onPaint();
   virtual bool onSetCursor();
   virtual bool onKeyPressed(TCHAR ch);
   virtual bool onKeyDown(int keyCode, bool kbShift, bool kbCtrl);
   virtual void onScroll(int bar, int type);
   virtual void onMouseWheel(short wheelDelta, bool kbCtrl);
   virtual void onMouseMove(Point point, bool kbLButton);
   virtual void onButtonDown(Point point, bool kbShift);
   virtual void onButtonUp();
   virtual void onDoubleClick();
   virtual void onResize();
   virtual void onSetFocus();
   virtual void onLoseFocus();

   void onContextMenu(HWND, short x, short y);

   void onEditorChange(bool firstChange, bool repaint = false);
   void onEditorRowChange(bool eol, bool firstChange, bool repaint = false);

   int getLineNumberMargin();

   void mouseToScreen(Point point, int& col, int& row, bool& margin);
   void captureMouse();
   bool isMouseCaptured();
   void releaseMouse();

   int getScrollerPosition(int barType);
   bool getScrollInfo(int bar, SCROLLINFO* info);

   void setScrollPosition(int bar, int position);
   void setScrollInfo(int bar, int max, int page);

   void createCaret(int height);
   void destroyCaret();
   void showCaret();
   void hideCaret();
   void locateCaret(int x, int y);

   void refresh(bool resize = true);
   void refreshScrollers(int bar, bool resize);
   void resizeDocuments();

public:
   int  getOverwriteMode();
   bool isModified();
   bool isAnyModified();
   bool hasSelection();
   bool canUndo();
   bool canRedo();
 
   bool isReadOnly() const { return _readOnly; }
   void setReadOnlyMode() { _readOnly = true; }
   void resetReadOnlyMode() { _readOnly = false; }

   void reloadSettings(bool refresh = true);

   Point getCaret();
   void  setCaret(Point caret);

   void selectAll();
   bool copyClipboard(Clipboard& board);
   void pasteClipboard(Clipboard& board);
   void eraseSelection();

   void trim();
   void eraseLine();
   void duplicateLine();
   void indent();
   void outdent();
   void commentBlock();
   void uncommentBlock();
   void swap();
   void toUppercase();
   void toLowercase();

   void undo();
   void redo();

   bool findText(const TCHAR* text, bool matchCase, bool wholeWord);
   bool replaceText(const TCHAR* text, const TCHAR* newText, bool matchCase, bool wholeWord);

   bool isDocumentUnnamed(int index);
   bool isDocumentModified(int index);
   bool isDocumentIncluded(int index);
   void markDocumentAsIncluded(int index);
   void markDocumentAsExcluded(int index);
   void saveDocument(int index, const TCHAR* path);

   const TCHAR* retrievePath(_ELENA_::Map<const TCHAR*, Text*>& texts, int index = -1);

   virtual void newDocument(Document* document);
   virtual void showDocument(int index);
   virtual void closeDocument(int index);

   virtual void addMarker(int row, int style);
   virtual void removeMarker(int row);
   virtual void clearMarkers();

   virtual void setTracker(TrackInfo info, int lineStyle, int style);
   virtual void clearTracker();

   EditFrame(Window* appWindow, PluginManager* pluginManager);
   virtual ~EditFrame();
};

} // _GUI_

#endif // editframeH
