
//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      EditFrame class implementation
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#include "idecommon.h"
// --------------------------------------------------------------------------
#include "idesettings.h"
#include "editframe.h"

using namespace _GUI_;
using namespace _ELENA_;

inline bool isKeyDown(int key)
{
   return (::GetKeyState(key) & 0x80000000) != 0;
}

// --- ViewStyles constants ---

StyleInfo defaultStyles[STYLE_MAX + 1] = {
	{Colour(0), Colour(0xFF, 0xFF, 0xFF), TEXT("Courier New"), IDE_CHARSET_ANSI, 10, false, false},
	{Colour(0, 0, 0xFF), Colour(0xFF, 0xFF, 0xFF), TEXT("Courier New"), IDE_CHARSET_ANSI, 10, false, false},
   {Colour(0, 0x40, 0x80), Colour(0xFF, 0xFF, 0xFF), TEXT("Courier New"), IDE_CHARSET_ANSI, 10, true, false},
	{Colour(0), Colour(0xC0, 0xC0, 0xC0), TEXT("Courier New"), IDE_CHARSET_ANSI, 10, false, false},
	{Colour(0), Colour(0x0, 0xFF, 0xFF), TEXT("Courier New"), IDE_CHARSET_ANSI, 10, false, false},
	{Colour(0x60, 0x60, 0x60), Colour(0x0, 0xFF, 0xFF), TEXT("Courier New"), IDE_CHARSET_ANSI, 10, true, false},
   {Colour(0), Colour(Canvas::Chrome()), TEXT("Courier New"), IDE_CHARSET_ANSI, 10, false, false},		
	{Colour(0, 0x80, 0), Colour(0xFF, 0xFF, 0xFF), TEXT("Courier New"), IDE_CHARSET_ANSI, 10, false, false},
	{Colour(0, 0x80, 0x80), Colour(0xFF, 0xFF, 0xFF), TEXT("Courier New"), IDE_CHARSET_ANSI, 10, false, false},
	{Colour(0xFF, 0x80, 0x40), Colour(0xFF, 0xFF, 0xFF), TEXT("Courier New"), IDE_CHARSET_ANSI, 10, false, false},
	{Colour(0xFF, 0xFF, 0xFF), Colour(0x80, 0x0, 0x0), TEXT("Courier New"), IDE_CHARSET_ANSI, 10, false, false},
	{Colour(0xFF, 0xFF, 0xFF), Colour(0xFF, 0x0, 0x0), TEXT("Courier New"), IDE_CHARSET_ANSI, 10, false, false},
	{Colour(0), Colour(0xFF, 0xFF, 0xFF), TEXT("Courier New"), IDE_CHARSET_ANSI, 10, true, false},
   {Colour(0x00, 0x7F, 0x7F), Colour(0xFF, 0xFF, 0xFF), TEXT("Courier New"), IDE_CHARSET_ANSI, 10, true, false},
   {Colour(0, 0x80, 0), Colour(0xFF, 0xFF, 0xFF), TEXT("Courier New"), IDE_CHARSET_ANSI, 10, false, false},
   {Colour(0), Colour(0xFF, 0xFF, 0xFF), TEXT("Courier New"), IDE_CHARSET_ANSI, 10, true, false}
};

StyleInfo classicStyles[STYLE_MAX + 1] = {
	{Colour(0xFF, 0xFF, 0), Colour(0, 0, 0x80), TEXT("Courier New"), IDE_CHARSET_ANSI, 10, false, false},
	{Colour(0xFF, 0xFF, 0xFF), Colour(0, 0, 0x80), TEXT("Courier New"), IDE_CHARSET_ANSI, 10, false, false},
	{Colour(0xC0, 0xC0, 0xC0), Colour(0, 0, 0x80), TEXT("Courier New"), IDE_CHARSET_ANSI, 10, false, false},
	{Colour(0, 0, 0x80), Colour(0xC0, 0xC0, 0xC0), TEXT("Courier New"), IDE_CHARSET_ANSI, 10, false, false},
	{Colour(0), Colour(0x0, 0xFF, 0xFF), TEXT("Courier New"), IDE_CHARSET_ANSI, 10, false, false},
	{Colour(0x60, 0x60, 0x60), Colour(0x0, 0xFF, 0xFF), TEXT("Courier New"), IDE_CHARSET_ANSI, 10, true, false},
	{Colour(0), Colour(Canvas::Chrome()), TEXT("Courier New"), IDE_CHARSET_ANSI, 10, false, false},
	{Colour(0xC0, 0xC0, 0xC0), Colour(0, 0, 0x80), TEXT("Courier New"), IDE_CHARSET_ANSI, 10, false, false},
	{Colour(0, 0xFF, 0xFF), Colour(0, 0, 0x80), TEXT("Courier New"), IDE_CHARSET_ANSI, 10, false, false},
	{Colour(0, 0xFF, 0x80), Colour(0, 0, 0x80), TEXT("Courier New"), IDE_CHARSET_ANSI, 10, false, false},
	{Colour(0xFF, 0xFF, 0xFF), Colour(0x80, 0x0, 0x0), TEXT("Courier New"), IDE_CHARSET_ANSI, 10, false, false},
	{Colour(0xFF, 0xFF, 0xFF), Colour(0xFF, 0x0, 0x0), TEXT("Courier New"), IDE_CHARSET_ANSI, 10, false, false},
	{Colour(0xFF, 0xFF, 0), Colour(0, 0, 0x80), TEXT("Courier New"), IDE_CHARSET_ANSI, 10, true, false},
   {Colour(0x80, 0xFF, 0xFF), Colour(0, 0, 0x80), TEXT("Courier New"), IDE_CHARSET_ANSI, 10, false, false},
   {Colour(0xC0, 0xC0, 0xC0), Colour(0, 0, 0x80), TEXT("Courier New"), IDE_CHARSET_ANSI, 10, false, false},
   {Colour(0xFF, 0xFF, 0), Colour(0, 0, 0x80), TEXT("Courier New"), IDE_CHARSET_ANSI, 10, true, false}
};

MenuInfo contextMenuInfo[8] = {
   {IDM_FILE_CLOSE, TEXT("Close\tCtrl+W")},
   {0, NULL},
   {IDM_EDIT_CUT, TEXT("Cut\tCtrl+X")},
   {IDM_EDIT_COPY, TEXT("Copy\tCtrl+C")},
   {IDM_EDIT_PASTE, TEXT("Paste\tCtrl+V")},
   {0, NULL},
   {IDM_DEBUG_BREAKPOINT, TEXT("Toggle Breakpoint\tF5")},
   {IDM_DEBUG_RUNTO, TEXT("Run to Cursor\tF4")},
};

// --- ViewStyles ---

void ViewStyles :: assign(StyleInfo* styles, int lineHeight, int marginWidth)
{
   _lineHeight = lineHeight;
   _marginWidth = marginWidth;

   for (int i = 0 ; i <= STYLE_MAX ; i++) {
	  Font* font = Font::createFont(styles[i].faceName, styles[i].characterSet, styles[i].size,
		   styles[i].bold, styles[i].italic);

	  _styles[i] = Style(styles[i].foreground, styles[i].background, font);
   }
}

void ViewStyles :: validate(Canvas* canvas)
{
   for (int i = 0 ; i <= STYLE_MAX ; i++) {
      _styles[i].validate(canvas);
      if (_lineHeight < _styles[i].lineHeight) {
         _lineHeight = _styles[i].lineHeight;
      }
   }
}

// --- EditFrame ---

EditFrame :: EditFrame(Window* appWindow, PluginManager* pluginManager)
   : Window(0, 0, 800, 600), _documents(NULL, freeobj)
{
   _appWindow = appWindow;
   _pluginManager = pluginManager;

   _currentDoc = NULL;
   _readOnly = false;
   _cached = false;
   _caretValid = false;
   _caretVisible = true;
   _mouseCaptured = false;

   _styles[0].assign(defaultStyles, 15, 20);
   _styles[1].assign(classicStyles, 15, 20);
   _scheme = Settings::scheme;

   reloadSettings(false);

   _contextMenu.create(8, contextMenuInfo);
}

EditFrame :: ~EditFrame()
{
}

LRESULT EditFrame :: Class_Proc(HWND hWnd, size_t Message, WPARAM wParam, LPARAM lParam)
{
   switch (Message)
   {
      case WM_PAINT:
         onPaint();
         return 0;
      case WM_CHAR:
         if (onKeyPressed((TCHAR)wParam)) {
            return 0;
         }
         else break;
      case WM_KEYDOWN:
         if (onKeyDown(wParam, isKeyDown(VK_SHIFT), isKeyDown(VK_CONTROL))) {
            return 0;
         }
         else break;
      case WM_VSCROLL:
         onScroll(SB_VERT, LOWORD(wParam));
         return 0;
      case WM_HSCROLL:
         onScroll(SB_HORZ, LOWORD(wParam));
         return 0;
      case WM_MOUSEWHEEL:
         onMouseWheel(HIWORD(wParam), (wParam & MK_CONTROL) != 0);
         return 0;
      case WM_MOUSEMOVE:
         onMouseMove(Point(LOWORD(lParam), HIWORD(lParam)),
             (wParam & MK_LBUTTON) != 0);
         return 0;
      case WM_LBUTTONDOWN:
         onButtonDown(Point(LOWORD(lParam), HIWORD(lParam)),
             (wParam & MK_SHIFT) != 0);
         return 0;
      case WM_LBUTTONUP:
         onButtonUp();
         return 0;
      case WM_CONTEXTMENU:
         onContextMenu((HWND)wParam, LOWORD(lParam), HIWORD(lParam));
         break;
      case WM_LBUTTONDBLCLK:
         onDoubleClick();
         return 0;
   }
   return Window::Class_Proc(hWnd, Message, wParam, lParam);
}

void EditFrame :: paint(Canvas& canvas, _GUI_::Rectangle clientRect)
{
   Style defaultStyle = _styles[_scheme][STYLE_DEFAULT];
   size_t lineHeight = _styles[_scheme].getLineHeight();
   size_t marginWidth = _styles[_scheme].getMarginWidth();
   if (_lineNumbersVisible) {
      marginWidth += getLineNumberMargin();
   }

   if (!_cached) {
      Style marginStyle = _styles[_scheme][STYLE_MARGIN];

      if (!defaultStyle.valid) {
         _styles[_scheme].validate(&canvas);

         defaultStyle = _styles[_scheme][STYLE_DEFAULT];
         marginStyle = _styles[_scheme][STYLE_MARGIN];
         resizeDocuments();
      }

      // Draw background
      clientRect.bottomRight += Point(1, 1);
      canvas.fillRectangle(clientRect, defaultStyle);

      // Draw margin
      int marginWidth = _styles[_scheme].getMarginWidth();
      if (_lineNumbersVisible) {
         marginWidth += getLineNumberMargin();
      }
      Rectangle margin(clientRect.topLeft.x, clientRect.topLeft.y, 
	      marginWidth, clientRect.Height());

      canvas.fillRectangle(margin, marginStyle);

      // Draw Document
      canvas.setClipArea(clientRect);

      clientRect.bottomRight -= Point(1, 1);

      int x = clientRect.topLeft.x + marginWidth;
      int y = clientRect.topLeft.y - lineHeight + 1;
      int width = 0;
      TCHAR lineNumber[6];

      Style style = defaultStyle;

      int length = 0;
      TCHAR* buffer;
      createstr(buffer, _currentDoc->getSize().x + 1);
		  
      LiteralWriter writer(buffer, _currentDoc->getSize().x + 1);
      LineInfo info = _currentDoc->startReading(&writer);
      while (true) {
         style = _styles[_scheme][info.style];
         length = getlength(buffer);

         if (info.newLine) {
            info.newLine = false;
            x = clientRect.topLeft.x + marginWidth;

            if (_lineNumbersVisible) {
               _itot(info.bookmark.getRow(), lineNumber, 10);
               canvas.drawTextClipped(
                    Rectangle(x - marginWidth, y, x, lineHeight + 1),
                    x - marginStyle.avgCharWidth * getlength(lineNumber) - 4,
                    y,
                    lineNumber,
                    getlength(lineNumber),
                    marginStyle);
            }
            y += lineHeight;
            // !! HOTFIX: allow to see breakpoint ellipse on margin if STYLE_TRACELINe set for this line
            if (info.style == STYLE_BREAKPOINT || 
               (info.bandLine && _currentDoc->checkMarker(info.bookmark.getRow(), STYLE_BREAKPOINT))) 
            {
               canvas.drawEllipse(Rectangle(3, y + 2, 12, 12), style);
            }
         }
         if (info.bandLine) {
            canvas.fillRectangle(Rectangle(x, y, clientRect.bottomRight.x - x, lineHeight + 1), style);
         }

         //width = canvas.TextWidth(&style, info.line, info.length);
         width = (style.avgCharWidth) * length;

         //if (defaultStyle._background != style._background) {
         if (length==0 && info.style==STYLE_SELECTION) {
            canvas.drawTextClipped(Rectangle(x, y, style.avgCharWidth + 1, lineHeight + 1), x, y, 
			                        TEXT(" "), 1, style);
         }
         else canvas.drawTextClipped(Rectangle(x, y, width + 1, lineHeight + 1), x, y, 
			    buffer, length, style);
         //}
         //else canvas.drawTextClippedTransporent(Rectangle(x, y, width, lineHeight), x, y, info.line, info.length, style);

         x += width;
         writer.reset();
         if (!_currentDoc->continueReading(info, &writer))
            break;
      }
      _cached = true;
      freestr(buffer);
   }
   Point caret = _currentDoc->getCaret(false) - _currentDoc->getFrame();
   if (caret.x >= 0 && caret.y >= 0) {
      locateCaret(clientRect.topLeft.x + marginWidth + defaultStyle.avgCharWidth * caret.x,
          lineHeight * caret.y + 1);

      _caretVisible = true;
   }
   else _caretVisible = false;
}

void EditFrame :: onPaint()
{
   if (_currentDoc) {
      if (_caretValid && _caretVisible)
         hideCaret();

      PAINTSTRUCT ps;
      Rectangle   clientRect = getClientRectangle();

      ::BeginPaint(_self, &ps);

      Canvas canvas(ps.hdc);

      if (_zbuffer.isReleased()) {
         _cached = false;
         _zbuffer.clone(&canvas, clientRect.Width(), clientRect.Height());
      }
      paint(_zbuffer, clientRect);
      canvas.copy(clientRect, Point(0, 0), _zbuffer);

      ::EndPaint(_self, &ps);

      if (_caretValid && _caretVisible)
         showCaret();
   }
}

void EditFrame :: reloadSettings(bool refresh)
{
   bool resizing = (_lineNumbersVisible != Settings::lineNumberVisible);
   bool tabChanged = (_tabSize != Settings::tabSize);
   _cached &= (_scheme == Settings::scheme);
   _cached &= (_lineNumbersVisible == Settings::lineNumberVisible);
   _cached &= (_tabSize == Settings::tabSize);

   _lineNumbersVisible = Settings::lineNumberVisible;
   _tabUsing = Settings::tabCharUsing;
   _tabSize = Settings::tabSize;
   _scheme = Settings::scheme;

   if (tabChanged) {
      Documents::Iterator it = _documents.start();
      while (!it.Eof()) {
         (*it)->onTabSizeChange();
         it++;
      }
   }

   if (refresh) {
      if (_currentDoc)
	     _currentDoc->refresh();

      _cached = false;

      refreshClient();
      if (resizing)
         resizeDocuments();	  
   }
}

int EditFrame :: getLineNumberMargin()
{
   if (_lineNumbersVisible) {
      Style marginStyle = _styles[_scheme][STYLE_MARGIN];

      return marginStyle.avgCharWidth * 5;
   }
   else return 0;
}

void EditFrame :: mouseToScreen(Point point, int& col, int& row, bool& margin)
{
   Rectangle rect = getClientRectangle();
   Style defaultStyle = _styles[_scheme][STYLE_DEFAULT];
   int marginWidth = _styles[_scheme].getMarginWidth() + getLineNumberMargin();
   int offset = defaultStyle.avgCharWidth / 2;

   col = (point.x - rect.topLeft.x - marginWidth + offset) / defaultStyle.avgCharWidth;
   row = (point.y - rect.topLeft.y) / (_styles[_scheme].getLineHeight());
   margin = (point.x - rect.topLeft.x < marginWidth);
}

bool EditFrame :: onSetCursor()
{
   POINT pt;
   ::GetCursorPos(&pt);
   ::ScreenToClient(_self, &pt);

   int col, row;
   bool margin = false;
   mouseToScreen(_GUI_::Point(pt.x, pt.y), col, row, margin);

   if (margin) {
      setCursor(CURSOR_ARROW);
   }
   else setCursor(CURSOR_TEXT);

   return true;
}

bool EditFrame :: onKeyPressed(TCHAR ch)
{
   if (!_currentDoc || _readOnly)
      return false;

   bool firstChange = !_currentDoc->status.modified; // check if changed from saved to unsave
   bool eol = _currentDoc->isCaretEOL();

   // plugin hooking
   PluginResult result = _pluginManager->onKeyPressed(ch, dynamic_cast<_Document*>(_currentDoc));
   if (test(result, pgrSuccessful)) {
      onEditorRowChange(eol, firstChange, (result == pgrNeedToRepaint)); 

      return true;
   } 

   if (ch==0x0D) {
      _currentDoc->insertNewLine();
   }
   else if (ch==0x08) {
      _currentDoc->eraseChar(true);
      if (_currentDoc->status.rowDifference == -1)
         eol = _currentDoc->isCaretEOL();
   }
   else if (ch==0x09) {      
      if (_tabUsing) {
         if (_currentDoc->hasMultilineSelection()) {
            _currentDoc->indent('\t', 1);
         }
         else _currentDoc->insertChar('\t');
      }
      else {
         if (!_currentDoc->hasMultilineSelection()) {
            size_t shift = calcTabShift(_currentDoc->getCaret().x, Settings::tabSize);
            for (size_t i = 0 ; i < shift ; i++)
               _currentDoc->insertChar(' ');
         } 
         else _currentDoc->indent(' ', Settings::tabSize);
      }
   }
   else if (ch >= 0x20) {
      _currentDoc->insertChar(ch);
   }
   else return false;

   onEditorRowChange(eol, firstChange, true); 

   return true;
}

bool EditFrame :: onKeyDown(int keyCode, bool kbShift, bool kbCtrl)
{
   if (!_currentDoc)
      return false;

   bool firstChange = !_currentDoc->status.modified; // check if changed from saved to unsave
   bool repaint = false;
   switch (keyCode) {
      case VK_LEFT:
         if (kbCtrl) {
            repaint = _currentDoc->moveLeftToken(kbShift);
         }
         else repaint = _currentDoc->moveLeft(kbShift);
         break;
      case VK_RIGHT:
         if (kbCtrl) {
            repaint = _currentDoc->moveRightToken(kbShift);
         }
         else repaint = _currentDoc->moveRight(kbShift);
         break;
      case VK_UP:
         if (kbCtrl) {
            repaint = _currentDoc->moveFrameUp();
         }
         else repaint = _currentDoc->moveUp(kbShift);
         break;
      case VK_DOWN:
         if (kbCtrl) {
            repaint = _currentDoc->moveFrameDown();
         }
         else repaint = _currentDoc->moveDown(kbShift);
         break;
      case VK_HOME:
         if (kbCtrl) {
            repaint = _currentDoc->moveFirst(kbShift);
         }
         else repaint = _currentDoc->moveHome(kbShift);
         break;
      case VK_END:
         if (kbCtrl) {
            repaint = _currentDoc->moveLast(kbShift);
         }
         else repaint = _currentDoc->moveEnd(kbShift);
         break;
      case VK_DELETE:
         if (!_readOnly) {
            _currentDoc->eraseChar(false);

            onEditorRowChange(false, firstChange, true); 
            return true;
         }
         break;
      case VK_PRIOR:
         repaint = _currentDoc->movePageUp(kbShift);
         break;
      case VK_NEXT:
         repaint = _currentDoc->movePageDown(kbShift);
         break;
      case VK_INSERT:
         if (!_readOnly)
            _currentDoc->setOverwriteMode(!_currentDoc->isOverwriteMode());
         break;
      default:
         return false;
   }
   onEditorChange(firstChange, repaint); 
   return true;
}

void EditFrame :: onScroll(int bar, int type)
{
   SCROLLINFO info;

   getScrollInfo(bar, &info);
   int offset = 0;
   switch (type) {
      case SB_LINEUP:
         if (info.nPos <= info.nMin)
            return;

         offset = -1;
         break;
      case SB_LINEDOWN:
         if (info.nPos >= info.nMax)
            return;

         offset = 1;
         break;
      case SB_THUMBPOSITION:
      case SB_THUMBTRACK:
         offset = info.nTrackPos - getScrollerPosition(bar);
         break;
      case SB_PAGEDOWN:
         offset = info.nPage;
         break;
      case SB_PAGEUP:
         offset = -(int)info.nPage;
         break;
      default:
           return;
/*
      case SB_TOP: topLineNew = 0; break;
	  case SB_BOTTOM: topLineNew = MaxScrollPos(); break;
*/
   }
   if(_currentDoc->scroll(bar, offset)) {
      onEditorChange(false, true);
   }
}

void EditFrame :: onMouseWheel(short wheelDelta, bool kbCtrl)
{
   int offset = (wheelDelta > 0) ? -1 : 1;

   if (kbCtrl) {
      offset *= _currentDoc->getSize().y;
   }
   if(_currentDoc->scroll(SB_VERT, offset)) {
      onEditorChange(false, true);
   }   
}

void EditFrame :: onMouseMove(Point point, bool kbLButton)
{
   if (kbLButton && isMouseCaptured()) {
      int col = 0, row = 0;
      bool margin = false;
      mouseToScreen(point, col, row, margin);

      bool repaint = _currentDoc->moveToFrame(col, row, true);

	  refresh(true);
	  onEditorChange(false, repaint);
   }
}

void EditFrame :: onButtonUp()
{
   releaseMouse();
}

void EditFrame :: onButtonDown(Point point, bool kbShift)
{
   setFocus();

   if (_currentDoc) {
      int col = 0, row = 0;
      bool margin = false;
      mouseToScreen(point, col, row, margin);

      bool repaint = _currentDoc->moveToFrame(col, row, kbShift);
      if (margin) {
         notify(IDE_EDITOR_MARGINCLICKED);
      }
      captureMouse();

      refresh(true);
      onEditorChange(false, repaint);
   }
}

void EditFrame :: onContextMenu(HWND, short x, short y)
{
   Point p(x, y);

   bool selected = hasSelection();
   _contextMenu.enableItemById(IDM_EDIT_CUT, selected);
   _contextMenu.enableItemById(IDM_EDIT_COPY, selected);
   _contextMenu.enableItemById(IDM_EDIT_PASTE, Clipboard::isAvailable());

   _contextMenu.show(_appWindow->getHandle(), p);
}

void EditFrame :: onDoubleClick()
{
   if (_currentDoc) {
      _currentDoc->selectWord();
      
      onEditorChange(false, true);
   }
}

void EditFrame :: onResize()
{
   Window::onResize();

   _zbuffer.release();

   resizeDocuments();
   refresh(true);
}

void EditFrame :: onSetFocus()
{
   if (_currentDoc) {
      //if (!_caretValid) {
         _caretValid = true;
         createCaret(_styles[_scheme].getLineHeight());
		 showCaret();
         refreshClient();
      //}
   }
}

void EditFrame :: onLoseFocus()
{
   destroyCaret();
   _caretValid = false;
}

void EditFrame :: onEditorRowChange(bool eol, bool firstChange, bool repaint)
{
   // should be called only after insert / delete operation
   if (_currentDoc->status.rowDifference == 0) {
      onEditorChange(firstChange, repaint);
      return;
   }

   if (repaint)   
      _cached = false;

   refresh(true);
   
   int firstRow = _currentDoc->getCaretRow() - _currentDoc->status.rowDifference;
   if (eol)
      firstRow++;

   if (_currentDoc->status.rowDifference < 0) {
      for (int i = 0 ; i >= _currentDoc->status.rowDifference ; i--) {
         _currentDoc->removeMarker(firstRow + i);
      }
   }
   _currentDoc->shiftMarkers(firstRow, _currentDoc->status.rowDifference);

   _cached = false;
   refreshClient();

   notify(IDE_EDITOR_ROWCOUNT_CHANGED, firstRow, _currentDoc->status.rowDifference);
   notify(IDE_EDITOR_CHANGED, firstChange ? -1 : 0);

   _currentDoc->status.rowDifference = 0;
}

void EditFrame :: onEditorChange(bool firstChange, bool repaint)
{
   if (repaint)   
      _cached = false;

   refresh();
   
   notify(IDE_EDITOR_CHANGED, firstChange ? -1 : 0);
}

Point EditFrame :: getCaret()
{
   return _currentDoc ? _currentDoc->getCaret() : Point(0, 0);
}

void EditFrame :: setCaret(Point caret)
{
   if (_currentDoc) {
      if (_currentDoc->setCaret(caret.x, caret.y, false))
         _cached = false;

	  onEditorChange(false);
   }
}

int EditFrame :: getScrollerPosition(int barType)
{
   if (barType == SB_VERT) 
      return _currentDoc->getFrame().y;
   else if (barType==SB_HORZ) 
	  return _currentDoc->getFrame().x;
   else return 0;
}

bool EditFrame :: getScrollInfo(int bar, SCROLLINFO* info)
{
   memset(info, 0, sizeof(*info));
   info->cbSize = sizeof(*info);
   info->fMask = SIF_ALL;

   return ::GetScrollInfo(_self, bar, info) ? true : false;
}

int EditFrame :: getOverwriteMode()
{
   if (_currentDoc) {
      return _currentDoc->isOverwriteMode() ? 1 : 0;
   }
   else return -1;
}

bool EditFrame :: isModified()
{
   return _currentDoc ? _currentDoc->status.modified : false;
}

bool EditFrame :: isAnyModified()
{
   Documents::Iterator it = _documents.start();
   while (!it.Eof()) {
      if ((*it)->status.modified) {
	     return true;
	  }
      it++;
   }
   return false;
}

bool EditFrame :: canUndo()
{
   return _currentDoc && _currentDoc->canUndo();
}

bool EditFrame :: canRedo()
{
   return _currentDoc && _currentDoc->canRedo();
}

bool EditFrame :: hasSelection()
{
   return _currentDoc && _currentDoc->hasSelection();
}

bool EditFrame :: isDocumentIncluded(int index)
{
   if (index >= 0 && index < (int)_documents.Count()) {
      return (*_documents.get(index))->status.included;
   }
   else return false;
}

bool EditFrame :: isDocumentModified(int index)
{
   if (index >= 0 && index < (int)_documents.Count()) {
      return (*_documents.get(index))->status.modified;
   }
   else return false;
}

bool EditFrame :: isDocumentUnnamed(int index)
{
   if (index >= 0 && index < (int)_documents.Count()) {
      return (*_documents.get(index))->status.unnamed;
   }
   else return false;
}

void EditFrame :: markDocumentAsIncluded(int index)
{
   if (index >= 0 && (size_t)index < _documents.Count()) {
      (*_documents.get(index))->status.included = true;
   }
}

void EditFrame :: markDocumentAsExcluded(int index)
{
   if (index >= 0 && (size_t)index < _documents.Count()) {
      (*_documents.get(index))->status.included = false;
   }
}

void EditFrame :: saveDocument(int index, const TCHAR* path)
{
   if (index >= 0 && (size_t)index < _documents.Count()) {
      (*_documents.get(index))->save(path);
   }
}

const TCHAR* EditFrame :: retrievePath(_ELENA_::Map<const TCHAR*, Text*>& texts, int index)
{
   if (index==-1 && _currentDoc) {
      return retrieveKey(texts.start(), _currentDoc->getText(), EMPTY_STRING);
   }  
   else if (index >= 0 && (size_t)index < _documents.Count()) {
      return retrieveKey(texts.start(), (*_documents.get(index))->getText(), EMPTY_STRING);
   }
   else return NULL;
}

void EditFrame :: setScrollPosition(int bar, int position)
{
   ::SetScrollPos(_self, bar, position, TRUE);
}

void EditFrame :: setScrollInfo(int bar, int max, int page)
{
   SCROLLINFO info;

   info.cbSize = sizeof(info);
   info.fMask = SIF_PAGE | SIF_RANGE;
   info.nMin = 0;
   info.nMax = max;
   info.nPage = page;
   info.nPos = 0;
   info.nTrackPos = 1;

   ::SetScrollInfo(_self, bar, &info, TRUE);
}

void EditFrame :: refresh(bool resize)
{
   if (_currentDoc) {
      refreshScrollers(SB_VERT, resize);
      if (_currentDoc->status.maxColChanged) {
         refreshScrollers(SB_HORZ, true);
	     _currentDoc->status.maxColChanged = false;
      }
      else refreshScrollers(SB_HORZ, resize);
   }
   refreshClient();
}

void EditFrame :: refreshScrollers(int bar, bool resize)
{
   if (_currentDoc) {
      int position = getScrollerPosition(bar);

      if (resize) {
         Point size = _currentDoc->getSize();
         if (bar == SB_VERT) {
            int   max = _currentDoc->getRowCount();
            setScrollInfo(bar, max + size.y, size.y);
         }
         else {
            int max = _currentDoc->getMaxColumn();
            setScrollInfo(bar, max - 1, size.x);
         }
      }
      setScrollPosition(bar, position);
   }
}

void EditFrame :: resizeDocuments()
{
   Point     size;
   Rectangle client = getClientRectangle();

   int marginWidth = getLineNumberMargin();
   size.x = (client.Width() - marginWidth) / _styles[_scheme][STYLE_DEFAULT].avgCharWidth;
   size.y = client.Height() / _styles[_scheme].getLineHeight();

   Documents::Iterator doc = _documents.start();
   while (!doc.Eof()) {
      (*doc)->resize(size);

      doc++;
   }
   _cached = false;
}

void EditFrame :: newDocument(Document* document)
{
   _currentDoc = document;
   _documents.add(document);

   resizeDocuments();
}

void EditFrame :: showDocument(int index)
{
   if (index != -1) {
      _currentDoc = *_documents.get(index);

      refresh(true);
	  onEditorChange(false, true);
   }
}

void EditFrame :: closeDocument(int index)
{
   Document* doc = *_documents.get(index);

   if (_currentDoc==doc) {
      _currentDoc = NULL;
   }
   _documents.cut(doc);
}

void EditFrame :: addMarker(int row, int style)
{
   if (_currentDoc) {
      _currentDoc->addMarker(row, style);

	  _cached = false;
      refreshClient();	  
   }
}

void EditFrame :: removeMarker(int row)
{
   if (_currentDoc) {
      _currentDoc->removeMarker(row);

	  _cached = false;
      refreshClient();	  
   }
}

void EditFrame :: clearMarkers()
{
   Documents::Iterator it = _documents.start();
   while (!it.Eof()) {
      (*it)->clearMarkers();

      it++;
   }
   _cached = false;
   refreshClient();	
}

void EditFrame :: setTracker(TrackInfo info, int lineStyle, int style)
{
   if (_currentDoc) {
      _currentDoc->setTracker(info, lineStyle, style);
      _currentDoc->setCaret(info, false);
   }
   onEditorChange(false, true);
}

void EditFrame :: clearTracker()
{
   Documents::Iterator it = _documents.start();
   while (!it.Eof()) {
      (*it)->clearTracker();

      it++;
   }
   _cached = false;
   refreshClient();
}

bool EditFrame :: copyClipboard(Clipboard& board)
{
   if (_currentDoc && _currentDoc->hasSelection()) {
      HGLOBAL buffer = board.create(_currentDoc->getSelectionLength());
      TCHAR* text = board.allocate(buffer);

      _currentDoc->copySelection(text);

      board.free(buffer);
      board.copy(buffer);

      return true;
   }
   else return false;
}

void EditFrame :: pasteClipboard(Clipboard& board)
{
   if (_currentDoc && !_readOnly) {
      bool eol = _currentDoc->isCaretEOL();
      bool firstChange = !_currentDoc->status.modified; // check if changed from saved to unsave

      HGLOBAL buffer = board.get();
      TCHAR* text = board.allocate(buffer);
      if  (!emptystr(text)) {
         _currentDoc->insertLine(text, getlength(text));

         board.free(buffer);
      }
      onEditorRowChange(eol, firstChange, true);
   }
}

void EditFrame :: selectAll()
{
   if (_currentDoc) {
      _currentDoc->moveFirst(false);
      _currentDoc->moveLast(true);

	  onEditorChange(false, true);
   }
}

void EditFrame :: eraseSelection()
{
   if (_currentDoc && !_readOnly) {
      bool firstChange = !_currentDoc->status.modified; // check if changed from saved to unsave

      _currentDoc->eraseChar(false);

      onEditorRowChange(false, firstChange, true);
   }
}

void EditFrame :: trim()
{
   if (_currentDoc && !_readOnly) {       
      bool   firstChange = !_currentDoc->status.modified; // check if changed from saved to unsave

      _currentDoc->trim();

      onEditorRowChange(_currentDoc->isCaretEOL(), firstChange, true); 
   }
}

void EditFrame :: eraseLine()
{
   if (_currentDoc && !_readOnly) {
      bool   firstChange = !_currentDoc->status.modified; // check if changed from saved to unsave

      _currentDoc->eraseLine();

      onEditorRowChange(false, firstChange, true); 
   }
}

void EditFrame :: duplicateLine()
{
   if (_currentDoc && !_readOnly) {
      size_t rowCount = _currentDoc->getRowCount();
      bool   firstChange = !_currentDoc->status.modified; // check if changed from saved to unsave

      _currentDoc->duplicateLine();

      onEditorRowChange(true, firstChange, true); 
   }
}

void EditFrame :: indent()
{
   if (_currentDoc && !_readOnly) {
      bool firstChange = !_currentDoc->status.modified; // check if changed from saved to unsave

      if (Settings::tabCharUsing) {
         _currentDoc->indent('\t', 1);
      }
      else _currentDoc->indent(' ', Settings::tabSize);

      onEditorChange(firstChange, true); 
   }
}

void EditFrame :: outdent()
{
   if (_currentDoc && !_readOnly) {
      bool firstChange = !_currentDoc->status.modified; // check if changed from saved to unsave

      _currentDoc->outdent();

      onEditorChange(firstChange, true); 
   }
}

void EditFrame :: swap()
{
   if (_currentDoc && !_readOnly) {
      bool firstChange = !_currentDoc->status.modified; // check if changed from saved to unsave

      _currentDoc->swap();

      onEditorChange(firstChange, true); 
   }
}

void EditFrame :: toUppercase()
{
   if (_currentDoc && !_readOnly) {
      bool firstChange = !_currentDoc->status.modified; // check if changed from saved to unsave

      _currentDoc->toUppercase();

      onEditorChange(firstChange, true); 
   }
}

void EditFrame :: toLowercase()
{
   if (_currentDoc && !_readOnly) {
      bool firstChange = !_currentDoc->status.modified; // check if changed from saved to unsave

      _currentDoc->toLowercase();

      onEditorChange(firstChange, true); 
   }
}

void EditFrame :: commentBlock()
{
   if (_currentDoc && !_readOnly) {
      bool firstChange = !_currentDoc->status.modified; // check if changed from saved to unsave

      _currentDoc->commentBlock();

      onEditorChange(firstChange, true); 
   }
}

void EditFrame :: uncommentBlock()
{
   if (_currentDoc && !_readOnly) {
      bool firstChange = !_currentDoc->status.modified; // check if changed from saved to unsave

      _currentDoc->uncommentBlock();

      onEditorChange(firstChange, true); 
   }
}

void EditFrame :: undo()
{
   if (_currentDoc && !_readOnly) {
      bool firstChange = !_currentDoc->status.modified; // check if changed from saved to unsave

      _currentDoc->undo();

      onEditorRowChange(false, firstChange, true);
   }
}

void EditFrame :: redo()
{
   if (_currentDoc && !_readOnly) {
      bool firstChange = !_currentDoc->status.modified; // check if changed from saved to unsave

      _currentDoc->redo();

      onEditorRowChange(false, firstChange, true);
   }
}

bool EditFrame :: findText(const TCHAR* text, bool matchCase, bool wholeWord)
{
   if (_currentDoc) {
      if (_currentDoc->findLine(text, matchCase, wholeWord)) {
         onEditorChange(false, true);

         return true;
      }
   }
   return false;
}

bool EditFrame :: replaceText(const TCHAR* text, const TCHAR* newText, bool matchCase, bool wholeWord)
{
   if (_currentDoc) {
      while (true) {
         if (_currentDoc->findLine(text, matchCase, wholeWord)) {
            onEditorChange(false, true); 

            int retVal = MsgBox::show(getHandle(), REPLACE_TEXT, MB_ICONQUESTION | MB_YESNOCANCEL);
            if (retVal == IDCANCEL) {
               break;
            }
            else if (retVal==IDYES) {
               bool firstChange = !_currentDoc->status.modified; // check if changed from saved to unsave

               _currentDoc->insertLine(newText, _ELENA_::getlength(newText));

               onEditorChange(firstChange, true); 
            }
         }
         else break;
      }
   }
   return true;
}

void EditFrame :: showCaret()
{
   ::ShowCaret(_self);
}

void EditFrame :: hideCaret()
{
   ::HideCaret(_self);
}

void EditFrame :: captureMouse()
{
   _mouseCaptured = true;
   ::SetCapture(_self);
}

bool EditFrame :: isMouseCaptured()
{
   return _mouseCaptured;
}

void EditFrame :: releaseMouse()
{
   _mouseCaptured = false;
   ::ReleaseCapture();
}

void EditFrame :: createCaret(int height)
{
   int width = 1;

   ::CreateCaret(_self, NULL, width, height);
}

void EditFrame :: destroyCaret()
{
   ::DestroyCaret();
}

void EditFrame :: locateCaret(int x, int y)
{
   ::SetCaretPos(x, y);
}
