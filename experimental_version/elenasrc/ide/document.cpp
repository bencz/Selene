//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      Document class implementation
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#include "idecommon.h"
// --------------------------------------------------------------------------
#include "document.h"
#include "idesettings.h"

using namespace _GUI_;

#define TERMINATORS       TEXT(" (){}[]:=<>\r.,^@+-*/!~?\t;\"")
#define WHITESPACE        TEXT(" \r\t")
#define OPERATORS         TEXT("(){}[]:=<>.,^@+-*/!~?;\"")

// --- LineInfo ---

void _GUI_::LineInfo :: readLine(Document* document, StreamWriter* writer)
{
   bandLine = false;

   read = document->defineStyle(*this);

   document->getText()->copyLineTo(bookmark, writer, read, true);

   writer->writeChar(0);
}

bool _GUI_::LineInfo :: goToNext()
{
  if (bookmark.getEOF())
     return false;

   bookmark.moveOn(read);
   if (bookmark.getColumn() >= (size_t)frame.bottomRight.x || bookmark.getEOL()) {
      if (bookmark.getRow() >= (size_t)frame.bottomRight.y)
	     return false;

      bookmark.moveTo(frame.topLeft.x, bookmark.getRow() + 1);
      newLine = true;
   }
   return true;
}

// --- Document ---

Document :: Document(Text* text, FileEncoding encoding)
   : _undoBuffer(UNDO_BUFFER_SIZE)
{
   _text = text;
   _encoding = encoding;
   _overwrite = false;
   _selection = 0;
   _maxColumn = 0;

   _text->attachWatcher(this);
}

Document :: ~Document()
{
   _text->detachWatcher(this);
}

void Document :: onInsert(size_t position, size_t length, const TCHAR* line)
{
   _undoBuffer.onInsert(position, length, line);

   _frame.invalidate();

   if (_caret.getPosition() > position) {
	  _caret.invalidate();
   }
}

void Document :: onErase(size_t position, size_t length, const TCHAR* line)
{
   _undoBuffer.onErase(position, length, line);

   _frame.invalidate();

   if (_caret.getPosition() > position) {
      _caret.invalidate();
   }
}

bool Document :: eraseSelection()
{
   if (_selection==0)
      return false;

   _text->validateBookmark(_caret);

   int rowCount = _text->getRowCount();

   if (_selection < 0) {
      _caret.moveOn(_selection);
      _selection = -_selection;
   }
   _text->eraseLine(_caret, _selection);
   _selection = 0;

   status.rowDifference += (_text->getRowCount() - rowCount);

   return true;
}

bool Document :: canUndo()
{
   return !_undoBuffer.Bof();
}

bool Document :: canRedo()
{
   return !_undoBuffer.Eof();
}

void Document :: undo()
{
   int rowCount = _text->getRowCount();

   _undoBuffer.undo(_text, _caret);

   setCaret(_caret.getCaret(false), false);

   status.rowDifference += (_text->getRowCount() - rowCount);

   onChange();
}

void Document :: redo()
{
   int rowCount = _text->getRowCount();

   _undoBuffer.redo(_text, _caret);

   setCaret(_caret.getCaret(false), false);

   status.rowDifference += (_text->getRowCount() - rowCount);

   onChange();
}

int Document :: getSelectionLength()
{
   return abs(_selection);
}

void Document :: resize(Point size)
{
   _size = size;
   if (_maxColumn < (size_t)_size.x) {
      _maxColumn = _size.x * 2;

      status.maxColChanged = true;
   }
   setCaret(_caret.getCaret(), false);
}

bool Document :: setCaret(Point caret, bool selecting)
{
	return setCaret(caret.x, caret.y, selecting);
}

int Document :: retrieveColumn(int row, int disp)
{
   TextBookmark bm = _caret;
   bm.moveTo(0, row);
   bm.moveOn(disp);

   return bm.getColumn();
}

bool Document :: setCaret(TrackInfo info, bool selecting)
{
   if (info.col != 0) {
      return setCaret(info.col, info.row, selecting);
   }
   else return setCaret(retrieveColumn(info.row, info.disp), info.row, selecting);
}

bool Document :: setCaret(int column, int row, bool selecting)
{
   bool uncached = false;
   if (column < 0) column = 0;
   if (row < 0) row = 0;
   else if ((size_t)row >= _text->getRowCount()) row = _text->getRowCount() - 1;

   _text->validateBookmark(_caret);

   int position = _caret.getPosition();

   _caret.moveTo(column, row);
   if (_maxColumn < _caret.getLength() + _size.x) {
      _maxColumn = _caret.getLength() + _size.x;

      status.maxColChanged = true;
   }

   Point frame = _frame.getCaret();
   Point caret = _caret.getCaret(false);
   if (caret.x < frame.x) {
      frame.x = caret.x;
   }
   else if (frame.x + _size.x - 2 <= caret.x) {
      frame.x = caret.x - _size.x + 3;
   }

   if (caret.y < frame.y) {
      frame.y = caret.y;
   }
   else if (frame.y + _size.y - 2 <= caret.y) {
      frame.y = caret.y - _size.y + 3;
   }

   if (_frame.getCaret()!=frame) {
      _text->validateBookmark(_frame);
      _frame.moveTo(frame.x, frame.y);
      uncached = true;
   }
   if (selecting) {
      _selection += position - _caret.getPosition();
      uncached = true;
   }
   else {
      if (_selection != 0)
         uncached = true;

      _selection = 0;
   }
   // if charcter was highlighted
   uncached |= highlightCharacter();

   return uncached;
}

bool Document :: moveFirst(bool selecting)
{
   return setCaret(0, 0, selecting);
}

bool Document :: moveLast(bool selecting)
{
   size_t lastRow = _text->getRowCount() - 1;
   return setCaret(_text->getRowLength(lastRow), lastRow, selecting);
}

bool Document :: moveLeft(bool selecting)
{
   if (selecting) {
      size_t pos = _caret.getPosition();
      _caret.moveOn(-1);

      _selection += pos - _caret.getPosition();
   }
   else _caret.moveOn(-1);

   return setCaret(_caret.getCaret(), selecting);
}

bool Document :: moveLeftToken(bool selecting)
{
   bool newToken = false;
   bool _operator = false;
   size_t pos = _caret.getPosition();

   _caret.moveOn(-1);

   while (_caret.getColumn() > 0 || _caret.getRow() > 0) {
      if (_caret.getColumn() == _caret.getLength())
         break;

      size_t length;
      const TCHAR* line = _text->getLine(_caret, length);

      if (chrpos(WHITESPACE, line[0])!=-1) {
         if (_operator || newToken) {
            _caret.moveOn(1);
            break;
         }
      }
      else if (chrpos(OPERATORS, line[0])!=-1) {
         if (newToken) {
            _caret.moveOn(1);
            break;
         }
         else _operator = true;
      }
      else {
         if (_operator) {
            _caret.moveOn(1);
            break;
         }
         else newToken = true;
      }
      _caret.moveOn(-1);
   }
   if (selecting) {
      _selection += pos - _caret.getPosition();
   }
   return setCaret(_caret.getCaret(), selecting);
}

bool Document :: moveRight(bool selecting)
{
   if (selecting) {
      size_t pos = _caret.getPosition();
	  _caret.moveOn(1);

	  _selection += pos - _caret.getPosition();
   }
   else _caret.moveOn(1);

   return setCaret(_caret.getCaret(), selecting);
}

bool Document :: moveRightToken(bool selecting)
{
   bool newToken = false;
   bool _operator = false;
   bool first = true;
   size_t pos = _caret.getPosition();
   while (first || _caret.getColumn() < _caret.getLength()) {

      size_t length;
      const TCHAR* line = _text->getLine(_caret, length);

      if (chrpos(WHITESPACE, line[0])!=-1) {
         newToken = true;
         _operator = false;
      }
      else if (chrpos(OPERATORS, line[0])!=-1) {
         if (!_operator && !first) {
            break;
         }
         _operator = true;
      }
	   else if (newToken || _operator)
         break;

      _caret.moveOn(1);
      first = false;
   }
   if (selecting) {
      _selection += pos - _caret.getPosition();
   }
   return setCaret(_caret.getCaret(), selecting);
}

bool Document :: moveUp(bool selecting)
{
   if (_caret.getRow() > 0) {
      return setCaret(_caret.getColumn(), _caret.getRow() - 1, selecting);
   }
   else return false;
}

bool Document :: moveFrameUp()
{
   if (scroll(SB_VERT, -1)) {
      if (_frame.getRow() + _size.y - 2 <= _caret.getRow()) {
         setCaret(_caret.getColumn(), _frame.getRow() + _size.y - 3, false);
      }
      return true;
   }
   else return false;
}

bool Document :: moveDown(bool selecting)
{
   if (_caret.getRow() < _text->getRowCount()) {
      return setCaret(_caret.getColumn(), _caret.getRow() + 1, selecting);
   }
   else return false;
}

bool Document :: moveFrameDown()
{
   if (scroll(SB_VERT, 1)) {
      if (_caret.getRow() < _frame.getRow()) {
         setCaret(_caret.getColumn(), _frame.getRow(), false);
      }
      return true;
   }
   else return false;
}

bool Document :: moveHome(bool selecting)
{
   return setCaret(0, _caret.getRow(), selecting);
}

bool Document :: moveEnd(bool selecting)
{
   return setCaret(_caret.getLength(), _caret.getRow(), selecting);
}

bool Document :: movePageUp(bool selecting)
{
   if (_caret.getRow()==0)
      return false;

   if (_frame.getRow()==0) {
      return setCaret(_caret.getColumn(), 0, selecting);
   }
   else if (scroll(SB_VERT, -_size.y)) {
      setCaret(_caret.getColumn(), _caret.getRow() - _size.y, selecting);

	  return true;
   }
   else return false;
}

bool Document :: movePageDown(bool selecting)
{
   if (_caret.getRow() + _size.y > _text->getRowCount() - 1) {
      return setCaret(_caret.getColumn(), _text->getRowCount() - 1, selecting);
   }
   else if (scroll(SB_VERT, _size.y)) {
      setCaret(_caret.getColumn(), _caret.getRow() + _size.y, selecting);

	  return true;
   }
   else return false;
}

bool Document :: moveToFrame(size_t column, size_t row, bool selecting)
{
   return setCaret(_frame.getColumn() + column, _frame.getRow() + row, selecting);
}

void Document :: selectWord()
{
   moveLeftToken(false);
   moveRightToken(true);

   while (true) {
      moveLeft(true);
      size_t length = 0;
      const TCHAR* line = _text->getLine(_caret, length);
      if (length==0 || chrpos(WHITESPACE, line[0])==-1) {
         moveRight(true);
         break;
      }
   }
}

bool Document :: scroll(int barType, int displacement)
{
   Point frame = _frame.getCaret();

   if (barType==SB_VERT) {
      frame.y += displacement;
      if (frame.y < 0)
         frame.y = 0;
      else if ((size_t)frame.y > _text->getRowCount())
         frame.y = _text->getRowCount();
   }
   else if (barType==SB_HORZ) {
      frame.x += displacement;
      if (frame.x < 0)
         frame.x = 0;
   }
   if (_frame.getCaret() != frame) {
      _frame.moveTo(frame.x, frame.y);
      return true;
   }
   else return false;
}

bool Document :: findLine(const TCHAR* text, bool matchCase, bool wholeWord)
{
   TextBookmark bookmark = _caret;
   if (_text->findWord(bookmark, text, matchCase, wholeWord ? TERMINATORS : NULL)) {
	  setCaret(bookmark.getCaret(false), false);
	  setCaret(_caret.getColumn() + getlength(text), _caret.getRow(), true);

	  return true;
   }
   else return false;
}

bool Document :: hasMultilineSelection() const
{
   if (!hasSelection())
      return false;

   TextBookmark bm = _caret;
   bm.moveOn(_selection);

   return (bm.getRow() != _caret.getRow());
}

void Document :: copySelection(TCHAR* text)
{
   if (_selection==0)  {
      text[0] = 0;
   }
   else {
      _text->copyTo(_caret, text, _selection);
   }
}

void Document :: insertChar(TCHAR ch)
{
   if (hasSelection()) {
      eraseSelection();
   }
   else if (_overwrite && _caret.getLength() > _caret.getColumn(false)) {
      _text->eraseChar(_caret);
   }

   int rowCount = _text->getRowCount();

   if (_text->insertChar(_caret, ch)) {
	  _caret.moveOn(1);
      setCaret(_caret.getCaret(), false);
   }
   
   status.rowDifference += (_text->getRowCount() - rowCount);

   onChange();
}

void Document :: insertLine(const TCHAR* text, int length)
{
   eraseSelection();

   int rowCount = _text->getRowCount();

   _text->insertLine(_caret, text, length);
   _caret.moveOn(length);

   setCaret(_caret.getCaret(), false);
   
   status.rowDifference += (_text->getRowCount() - rowCount);

   onChange();
}

void Document :: insertNewLine()
{
   eraseSelection();

   int rowCount = _text->getRowCount();

   if (_text->insertNewLine(_caret)) {
      setCaret(0, _caret.getRow() + 1, false);
   }
   
   status.rowDifference += (_text->getRowCount() - rowCount);

   onChange();
}

void Document :: trim()
{
   int rowCount = _text->getRowCount();

   Point caret = _caret.getCaret(false);
   bool space = false;
   if ((size_t)caret.x == _caret.getLength()) {
      _text->eraseChar(_caret);
   }
   else while ((size_t)caret.x < _caret.getLength()) {
      size_t length;
      const TCHAR* line = _text->getLine(_caret, length);

      if (line[0]==' ' || line[0]=='\t') {
         _text->eraseChar(_caret);
         space = true;
      }
      else if (!space) {
         _text->eraseChar(_caret);
      }
      else break;
   }
   
   status.rowDifference += (_text->getRowCount() - rowCount);

   onChange();
}

void Document :: eraseChar(bool moveback)
{
   if (_selection != 0) {
      eraseSelection();

      setCaret(_caret.getCaret(), false);
   }
   else {
      int rowCount = _text->getRowCount();

      if (moveback) {
         if (_caret.getColumn(false)==0 && _caret.getRow()==0)
            return;

         moveLeft(false);
      }
      _text->eraseChar(_caret);
   
      status.rowDifference += (_text->getRowCount() - rowCount);
   }
   onChange();
}

void Document :: eraseLine()
{
   int rowCount = _text->getRowCount();

   _caret.moveTo(0, _caret.getRow());

   _text->eraseLine(_caret, _caret.getLength());
   _text->eraseChar(_caret);
   
   status.rowDifference += (_text->getRowCount() - rowCount);

   onChange();
}

void Document :: duplicateLine()
{
   int rowCount = _text->getRowCount();

   Point caret = _caret.getCaret(false);

   _caret.moveTo(0, caret.y);

   TextBookmark bm = _caret;
   bm.moveTo(_caret.getLength(), caret.y);

   size_t length = bm.getPosition() - _caret.getPosition();
   TCHAR* buffer;
   createstr(buffer, length + 1);
   _text->copyTo(_caret, buffer, length);

   _caret.moveTo(0, caret.y + 1);
   _text->insertNewLine(_caret);
   _text->insertLine(_caret, buffer, length);

   freestr(buffer);

   setCaret(caret.x, caret.y + 1, false);
   
   status.rowDifference += (_text->getRowCount() - rowCount);

   onChange();
}

void Document :: indent(TCHAR space, size_t count)
{
   if (_selection < 0) {
      _caret.moveOn(_selection);
      _selection = abs(_selection);
   }
   TextBookmark start = _caret;
   TextBookmark end = _caret;

   end.moveOn(_selection);
   /*if (end.getColumn()==0) {
      end.moveOn(-1);
   }*/
   while (start.getRow() <= end.getRow()) {
      for (size_t i = 0 ; i < count ; i++) {
         _text->insertChar(start, space);
         if (_selection != 0)
            _selection++;
      };
      if (!start.moveTo(0, start.getRow() + 1))
         break;
   }
   onChange();
}

void Document :: outdent()
{
   if (_selection < 0) {
      _caret.moveOn(_selection);
      _selection = abs(_selection);
   }
   TextBookmark start = _caret;
   TextBookmark end = _caret;

   end.moveOn(_selection);
   if (end.getColumn()==0) {
      end.moveOn(-1);
   }
   while (start.getRow() <= end.getRow()) {
      for (size_t i = 0 ; i < Settings::tabSize ; i++) {
         size_t length;
         const TCHAR* s = _text->getLine(start, length);
         if (length!=0 && (s[0]==' ' || s[0]=='\t')) {
            bool tab = (s[0]=='\t');
            _text->eraseChar(start);
            if (_selection != 0)
               _selection--;

            if (tab) break;
         }
         else break;
      }
      if(!start.moveTo(0, start.getRow() + 1))
         break;
   }
   onChange();
}

void Document :: swap()
{
   if (_caret.getColumn() > 0 && _caret.getColumn() < _caret.getLength()) {
      TCHAR pair[3];

      _caret.moveOn(-1);
      _selection = 2;

      copySelection(pair);

      // swap
      TCHAR tmp = pair[0];
      pair[0] = pair[1];
      pair[1] = tmp;

      insertLine(pair, 2);

      _selection = 0;
   }
}

void Document :: toUppercase()
{
   if (getSelectionLength() > 0) {
      String buffer(getSelectionLength() + 1);

      copySelection(buffer.get(getSelectionLength()));

      buffer.upper();

      insertLine(buffer, buffer.Length());
   }
}

void Document :: toLowercase()
{
   if (getSelectionLength() > 0) {
      String buffer(getSelectionLength() + 1);

      copySelection(buffer.get(getSelectionLength()));

      buffer.lower();

      insertLine(buffer, buffer.Length());
   }
}

void Document :: commentBlock()
{
   int selection = _selection;
   _selection = 0;

   if (selection < 0) {
      _caret.moveOn(selection);
      selection = -selection;
   }
   Point caret = _caret.getCaret();
   TextBookmark end = _caret;
   end.moveOn(selection);

   while ((size_t)caret.y < end.getRow() || ((size_t)caret.y == end.getRow() && end.getColumn() > 0)) {
      if (!_caret.moveTo(0, caret.y))
         return;

      insertLine(TEXT("//"), 2);
      caret.y++;
   }
   onChange();
}

void Document :: uncommentBlock()
{
   int selection = _selection;
   _selection = 0;

   if (selection < 0) {
      _caret.moveOn(selection);
      selection = -selection;
   }
   Point caret = _caret.getCaret();
   TextBookmark end = _caret;
   end.moveOn(selection);

   TCHAR line[3];
   while ((size_t)caret.y <= end.getRow()) {
      if (!_caret.moveTo(0, caret.y))
         return;

      _text->copyTo(_caret, line, 2);
      if (compstr(line, _T("//"), 2)) {
         eraseChar(false);
         eraseChar(false);
      }
      caret.y++;
   }
   onChange();
}

void Document :: save(const TCHAR* path)
{
   _text->save(path, _encoding);

   status.modified = false;
   status.unnamed = false;
}

void Document :: addMarker(size_t row, int style)
{
   _markers.add(row, style, true);
}

bool Document :: checkMarker(size_t row, int style)
{
   return _markers.exist(row, style);
}

void Document :: removeMarker(size_t row)
{
   _markers.erase(row);
}

void Document :: shiftMarkers(size_t startRow, int offset)
{
   _markers.shiftKeys(startRow, offset);
}

void Document :: clearMarkers()
{
   _markers.clear();
}

size_t Document :: defineStyle(_GUI_::LineInfo& info)
{
   size_t count = info.frame.Width();
   
   info.style = STYLE_DEFAULT;
   if (_selection != 0) {
      size_t pos = info.bookmark.getPosition();
      size_t curPos = _caret.getPosition();
      size_t selPos = curPos + _selection;
      if (_selection > 0) {
         if (info.bookmark.getRow()==_caret.getRow() && pos < curPos) {
            count = curPos - pos;
         }
         else if (pos >= curPos && pos < selPos) {
            count = selPos - pos;
            info.style = STYLE_SELECTION;
         }
      }
      else {
         if (pos >= selPos && pos < curPos) {
            count = curPos - pos;
            info.style = STYLE_SELECTION;
         }
         else if (pos < selPos && pos + count > selPos) {
            count = selPos - pos;
         }
      }
   }
   return count;
}

_GUI_::LineInfo Document :: startReading(StreamWriter* writer)
{
   _text->validateBookmark(_frame);

   LineInfo info(_frame, _size);
   if (_text->getRowCount()==0) {
      writer->writeChar(0);
   }
   else {
      if ((size_t)info.frame.bottomRight.y >= _text->getRowCount()) {
         info.frame.bottomRight.y = _text->getRowCount() - 1;
      }
      info.readLine(this, writer);
   }
   return info;
}

bool Document :: continueReading(_GUI_::LineInfo& info, StreamWriter* writer)
{
   if (!info.goToNext())
      return false;

   info.readLine(this, writer);
   return true;
}

void Document :: onTabSizeChange()
{
   refresh();
   _caret.invalidate();
}
