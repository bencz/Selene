//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      Text class implementation
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#include "idecommon.h"
// --------------------------------------------------------------------------
#include "text.h"
#include "idesettings.h"

using namespace _GUI_;

//typedef FileReader _Reader;
//typedef FileWriter _Writer;

#ifdef _UNICODE

#define dword_size 2

#else

#define dword_size 4

#endif

// --- Text ---

Text :: Text()
{
   _rowCount = 0;
}

Text :: ~Text()
{
}

void Text :: create()
{
   _pages.clear();
   _pages.add(Page());

   _rowCount = 0;
}

bool Text :: load(const TCHAR* path, FileEncoding& encoding)
{
   FileReader file(path, feAutodetect);
   encoding = file.getEncoding();

   if (!file.isOpened())
      return false;

   while (!file.Eof()) {
      Page page;
      file.readLiteral(page.text, PAGE_SIZE, page.used);

      _pages.add(page);
   }
   if (_pages.Count()==0) {
      create();
   }
   else _rowCount = 1;

   Pages::Iterator it = _pages.start();
   while (!it.Eof()) {
      refreshPage(it);
	  _rowCount += (*it).rows;

      it++;
   }
   return true;
}

void Text :: save(const TCHAR* path, FileEncoding encoding)
{
   FileWriter	writer(path, encoding);

   Pages::Iterator it = _pages.start();
   while (!it.Eof()) {
      writer.writeLiteral((*it).text, (*it).used);

      it++;
   }
}

void Text :: copyTo(TextBookmark bookmark, TCHAR* buffer, int length)
{
   validateBookmark(bookmark);

   if (length < 0) {
      bookmark.goTo(length);
      length = -length;
   }

   buffer[length] = 0;
   while (length > 0) {
      if (bookmark._offset >= (*bookmark._page).used) {
         bookmark.moveToNextPage();
      }
      size_t copied = (*bookmark._page).used - bookmark._offset;
      if (copied > (size_t)length) {
         copied = length;
      }
      _tcsncpy(buffer, (*bookmark._page).text + bookmark._offset, copied);

      if (!copied)
         break;

      bookmark.goTo(copied);
      buffer += copied;
      length -= copied;
   }
}

const TCHAR* Text :: getLine(TextBookmark& bookmark, size_t& length)
{
   validateBookmark(bookmark);

   if ((*bookmark._page).used <= bookmark._offset) {
      bookmark.moveToNextPage(false);
   }
   length = (*bookmark._page).used - bookmark._offset;
   return (*bookmark._page).text + bookmark._offset;
}

TCHAR Text :: getChar(TextBookmark& bookmark)
{
   validateBookmark(bookmark);

   if ((*bookmark._page).used <= bookmark._offset) {
      bookmark.moveToNextPage(false);
   }
   return *((*bookmark._page).text + bookmark._offset);
}

void Text :: copyLineTo(TextBookmark bookmark, StreamWriter* writer, size_t& length, bool stopOnEOL)
{
   validateBookmark(bookmark);

   int diff = bookmark.getVirtualDiff();
   if (diff > 0) {
      writer->writeChars(' ', diff);
      diff = 0;
   }

   size_t copied = 0;
   int col = bookmark._column;
   while (length > 0) {
      size_t offset = bookmark._offset;
      if (offset >= (*bookmark._page).used) {
         if (!bookmark.moveToNextPage())
	        break;

         offset = bookmark._offset;
      }
      size_t count = (*bookmark._page).used - offset;
      if (count > length) {
         count = length;
      }
      const TCHAR* line = (*bookmark._page).text + offset;
      for (size_t i = 0 ; i < count ; i++) {
         if (stopOnEOL && line[i]==0x0D) {
            length = copied;
            return;
         }
         else if (line[i]=='\t') {
            int disp = calcTabShift(col, Settings::tabSize);
            writer->writeChars(' ',  disp + diff);
            diff = 0;
            col += disp;
         }
         else {
            writer->writeChar(line[i]);
            col++;
         }
         copied++;
      }
      bookmark.goTo(count);
      length -= count;
   }
   length = copied;
}

inline bool check(TCHAR ch1, TCHAR ch2, bool matchCase)
{
   if (matchCase) {
      return (ch1==ch2);
   }
   else return (_tchlwr(ch1)==_tchlwr(ch2));
}

bool Text :: findWord(TextBookmark& bookmark, const TCHAR* line, bool matchCase, const TCHAR* terminators)
{
   validateBookmark(bookmark);

   int len = getlength(line);
   TCHAR ch = line[0];
   while (true) {
      if (check((*bookmark._page).text[bookmark._offset], ch, matchCase)) {
         if (compare(bookmark, line, len, matchCase, terminators)) {
            bookmark._virtual_column = bookmark._column;
            return true;
         }
      }
      if (!bookmark.move(1))
         break;
   }
   return false;
}

bool Text :: insertChar(TextBookmark& bookmark, TCHAR ch)
{
   validateBookmark(bookmark);

   insert(bookmark, &ch, 1, false);
   if (ch=='\t') {
      bookmark._length = BM_INVALID;
   }
   else if (bookmark._length != BM_INVALID)
      bookmark._length++;

   if (_rowCount==0) {
      _rowCount++;
   }
   return true;
}

bool Text :: insertLine(TextBookmark& bookmark, const TCHAR* s, size_t length)
{
   validateBookmark(bookmark);

   insert(bookmark, s, length, true);
   bookmark._length = BM_INVALID;

   _rowCount = retrieveRowCount();

   return true;
}

bool Text :: insertNewLine(TextBookmark& bookmark)
{
   validateBookmark(bookmark);

   insert(bookmark, TEXT("\r\n"), 2, true);
   bookmark._length = BM_INVALID;

   _rowCount++;

   return true;
}

bool Text :: eraseLine(TextBookmark& bookmark, size_t length)
{
   validateBookmark(bookmark);

   erase(bookmark, length, true);
   bookmark._length = BM_INVALID;
   bookmark.skipEmptyPages();

   _rowCount = retrieveRowCount();

   return true;
}

bool Text :: eraseChar(TextBookmark& bookmark)
{
   validateBookmark(bookmark);

   if (bookmark._column==bookmark.getLength()) {
      if (bookmark._row != _rowCount - 1) {
         erase(bookmark, 2, true);

         _rowCount--;
      }
      else return false;
   }
   else {
      erase(bookmark, 1, false);
	   if (bookmark._length != BM_INVALID)
         bookmark._length--;
   }
   bookmark._length = BM_INVALID;
   bookmark.skipEmptyPages();

   return true;
}

size_t Text :: getRowLength(size_t row)
{
   if (row < _rowCount) {
      TextBookmark bookmark;
      validateBookmark(bookmark);

      bookmark.moveTo(0, row);

      return bookmark.getLength();
   }
   else return 0;
}

void Text :: attachWatcher(_TextWatcher* watcher)
{
   _watchers.add(watcher);
}

void Text :: detachWatcher(_TextWatcher* watcher)
{
   _watchers.cut(watcher);
}

void Text :: validateBookmark(TextBookmark& bookmark)
{
   if (!bookmark.isValid()) {
      bookmark.assign(&_pages);
   }
}

void Text :: refreshPage(Pages::Iterator page)
{
   int used = (*page).used;
   TCHAR* text = (*page).text;

   (*page).rows = 0;
   for (int i = 0 ; i < used ; i++) {
      if (text[i]==0x0A) {
         (*page).rows++;
      }
   }
}

void Text :: refreshNextPage(Pages::Iterator page)
{
   if (!page.Last()) {
      page++;
      refreshPage(page);
   }
}

bool Text :: compare(TextBookmark bookmark, const TCHAR* line, int len, bool matchCase, const TCHAR* terminators)
{
   if (terminators) {
      if (bookmark.goTo(-1)) {
         if((chrpos(terminators, (*bookmark._page).text[bookmark._offset])==-1)) {
            return false;
         }
         else bookmark.goTo(1);
      }      
   }

   for (int i = 0 ; i < len ; i++) {
	  if (!check((*bookmark._page).text[bookmark._offset], line[i], matchCase))
	     return false;

	  if (!bookmark.goTo(1))
	     return (i==(len-1));
   }
   if (terminators) {
      return (chrpos(terminators, (*bookmark._page).text[bookmark._offset])!=-1);
   }
   else return true;
}

void Text :: insert(TextBookmark bookmark, const TCHAR* s, size_t length, bool checkRowCount)
{
   TextWatchers::Iterator it = _watchers.start();
   while (!it.Eof()) {
      (*it)->onInsert(bookmark.getPosition(), length, s);
      it++;
   }
   size_t offset = bookmark._offset;
   size_t size;
   while (length > 0) {
      Pages::Iterator page = bookmark._page;
      size = PAGE_SIZE - (*page).used;
      if (size > length)
         size = length;

      if (offset < (*page).used) {
         if (size==0) {
            size = (*page).used - offset;
            (*page).used = offset;

            Page newPage(size);
            _tcsncpy(newPage.text, (*page).text + offset, size);

            _pages.insertAfter(page, newPage);

            if (!checkRowCount) {
               refreshPage(page);
            }
            refreshNextPage(page);

            if (size > length)
               size = length;
         }
         else movestr((*page).text + offset + size, (*page).text + offset, (*page).used - offset);
      }
      else if (size==0) {
         if (!bookmark.moveToNextPage(true)) {
            Page newPage;

            _pages.insertAfter(bookmark._page, newPage);
            bookmark.moveToNextPage(true);
         }
         offset = bookmark._offset;
         continue;
      }
      _tcsncpy((*page).text + offset, s, size);

      (*page).used += size;
      if (checkRowCount) {
         refreshPage(page);
      }
      length -= size;
      offset += size;
      s += size;
   }
}

void Text :: erase(TextBookmark bookmark, size_t length, bool checkRowCount)
{
   size_t size = 0;
   size_t offset = bookmark._offset;
   size_t position = bookmark.getPosition();

   while (length > 0) {
      Pages::Iterator page = bookmark._page;
      size = length;
      if (size > (*page).used - offset)
         size = (*page).used - offset;

      TextWatchers::Iterator it = _watchers.start();
      while (!it.Eof()) {
         (*it)->onErase(position, size, (*page).text + offset);
         it++;
      }

      if (offset + size < (*page).used) {
         _tcsncpy((*page).text + offset, (*page).text + offset + size,
                  (*page).used - offset);
      }
      (*page).used -= size;
      length -= size;
      if (checkRowCount) {
         refreshPage(page);
      }
      if (length != 0) {
         if (!bookmark.moveToNextPage())
            break;

         offset = bookmark._offset;
         position = bookmark.getPosition();
      }
   }
}

int Text :: retrieveRowCount()
{
   Pages::Iterator it = _pages.start();
   int count = 1;
   while (!it.Eof()) {
      count += (*it).rows;

      it++;
   }
   return count;
}

// --- TextBookmark ---

TextBookmark :: TextBookmark()
{
   _position = BM_INVALID;
   _row = _virtual_column = _column = 0;
   _length = BM_INVALID;
}

TextBookmark :: ~TextBookmark()
{
}

bool TextBookmark :: moveTo(size_t column, size_t row)
{
   if (_position==BM_INVALID)
      return false;

   // backward navigation
   if (row < _row) {
      // estimate if backward navigation is feasible (two times less than number of rows on the page)
	   if ((_row - row) < ((*_page).rows << 1)) {
         while (_row > row) {
	         if (!goBackToBOL())
                return false;
         }
	   }
	   else moveToClosestRow(row);
   }
   // estimate if forward navigation is feasible (two times less than number of rows on the page)
   if (((row - _row) > 2) && (row - _row) > ((*_page).rows << 1)) {
      moveToClosestRow(row);
   }
   while (row > _row) {
      if (!goNextToBOL())
         return false;
   }
   _virtual_column = column;
   if (column > getLength())
      column = getLength();

   moveToClosestColumn(column);
   return true;
}

bool TextBookmark :: moveOn(int disp)
{
   bool result = false;
   if (disp < 0) {
      disp = abs(disp);
      if (disp < (PAGE_SIZE << 2)) {
         result = move(-disp);
      }
      else result = moveToClosestPosition(getPosition() - disp);
   }
   else if (disp > 0) {
      if (disp < (PAGE_SIZE << 2)) {
         result = move(disp);
      }
      else result = moveToClosestPosition(getPosition() + disp);
   }
   _virtual_column = _column;

   return result;
}

size_t TextBookmark :: getLength()
{
   if (_length==BM_INVALID) {
      _length = _column + seekEOL();
   }
   return _length;
}

void TextBookmark :: assign(Pages* pages)
{
   Point target = getCaret();

   _position = 0;
   _offset = 0;
   _page = pages->start();

   _row = _column = 0;
   _length = BM_INVALID;

   moveTo(target.x, target.y);
}

bool TextBookmark :: goBackToBOL()
{
   if (_row==0) {
      moveToStart();
      return true;
   }
   goTo(-1);
   if ((*_page).text[_offset]==0x0A) {
      _row--;
      goTo(-1);
   }
   _column = 0;
   _length = BM_INVALID;
   while ((*_page).text[_offset]!=0x0A) {
      if (!goTo(-1))
         return false;
   }
   goTo(1);
   return true;
}

bool TextBookmark :: goNextToBOL()
{
   if (_offset == (*_page).used) {
      if (!moveToNextPage())
	     return false;
   }
   while ((*_page).text[_offset]!=0x0A) {
      if (!goTo(1))
	     return false;
   }
   goTo(1);
   _column = 0;
   _length = BM_INVALID;
   _row++;

   return true;
}

bool TextBookmark :: moveToStart()
{
   //if (_page.First() && _position == 0 && _offset == 0)
   //   return false;

   while (moveToPreviousPage());

   _column = 0;
   _row = 0;
   _length = BM_INVALID;

   skipEmptyPages(true);
   return true;
}

bool TextBookmark :: moveToPreviousPage(bool allowEmpty)
{
   do {
      if (!_page.First()) {
         _page--;
         _position -= (*_page).used;
         _offset = (*_page).used - 1;
      }
      else {
         _position = 0;
         _offset = 0;
         return false;
	  }
   } while (!allowEmpty && (*_page).used == 0);

   return true;
}

bool TextBookmark :: moveToNextPage(bool allowEmpty)
{
   do {
      if (!_page.Last()) {
         _position += (*_page).used;
         _offset = 0;
         _page++;
      }
      else {
         _offset = (*_page).used;
         return false;
      }
   } while (!allowEmpty && (*_page).used == 0);

   return true;
}

bool TextBookmark :: skipEmptyPages(bool onlyForward)
{
   if ((*_page).used==0) {
      if (!moveToNextPage()) {
         if (!onlyForward && moveToPreviousPage()) {
            _offset = (*_page).used;
            return false;
         }
      }
   }
   return true;
}

bool TextBookmark :: goTo(int disp)
{
   if (disp < 0) {
      disp = abs(disp);
      while (disp > 0) {
         if ((size_t)disp > _offset) {
            disp -= (_offset + 1);
            if (!moveToPreviousPage())
               return false;
         }
         else {
            _offset -= disp;
            return true;
         }
      }
   }
   else while (disp >= 0) {
      if (disp + _offset >= (*_page).used) {
         disp -= (*_page).used - _offset;
         if (!moveToNextPage())
            return false;
      }
      else {
         _offset += disp;
         return true;
      }
   }
   return true;
}

bool TextBookmark :: move(int disp)
{
   bool eol = false;
   if (disp < 0) {
      while (disp < 0) {
         if (_offset == 0) {
            if (!moveToPreviousPage())
               return false;
         }
         else _offset--;

         if ((*_page).text[_offset]==0x0A) {
            _row--;

            TextBookmark bm = *this;
            bm.goBackToBOL();
            _length = bm.seekEOL();
            _column = _length;
            eol = true;
         }
         else if ((*_page).text[_offset]==0x0D) {
            eol = false;
         }
         else if ((*_page).text[_offset]==0x09) {
            TextBookmark bm = *this;
            bm.goBackToBOL();
            bm.move(getPosition() - bm.getPosition());
            _column = bm._column;
         }
         else _column--;

         disp++;
      }
      if (eol) goTo(-1);
   }
   else if (disp > 0) {
      if (_offset == (*_page).used) {
         if (!moveToNextPage())
            return false;
      }
      while (disp > 0) {
         if ((*_page).text[_offset]==0x0D) {
            _row++;
            _column = 0;
            _length = BM_INVALID;
            eol = true;
         }
         else if ((*_page).text[_offset]==0x0A) {
            eol = false;
         }
         else if ((*_page).text[_offset]==0x09) {
            _column += calcTabShift(_column, Settings::tabSize);
         }
         else _column++;
         disp--;

         if (_offset == (*_page).used - 1) {
            if (!moveToNextPage())
               return false;
         }
         else _offset++;
      }
      if (eol) goTo(1);
   }
   return true;
}

void TextBookmark :: moveToClosestRow(size_t row)
{
   moveToStart();
   if (row != _row) {
      size_t closestRow = 0;
      while (closestRow + (*_page).rows < row) {
         closestRow += (*_page).rows;
         if (!moveToNextPage())
            return;
      }
	  _row = closestRow;
	  goNextToBOL();
   }
}

bool TextBookmark :: moveToClosestPosition(size_t position)
{
   bool result = moveToStart();
   if (position != 0) {
      size_t closestRow = 0;
      while (getPosition() + (*_page).used < position) {
         closestRow += (*_page).rows;
         if (!moveToNextPage())
            return result;
      }
      _row = closestRow;
      result |= goNextToBOL();

      result |= move(position - getPosition());
   }
   return result;
}

void TextBookmark :: moveToClosestColumn(size_t column)
{
   if (column > _column) {
      while (column > _column) {
         if (!move(1))
            return;
      }
   }
   else if (column < _column) {
      while (column < _column) {
         if (!move(-1))
            return;
      }
   }
}

size_t TextBookmark :: seekEOL()
{
   TextBookmark bm = *this;

   if (bm._offset == (*bm._page).used) {
      if (!bm.moveToNextPage())
	     return 0;
   }
   while ((*bm._page).text[bm._offset]!=0x0D) {
      if (!bm.move(1))
         break;
   }
   return bm._column - _column;
}

/*
size_t TextBookmark :: seekBOL()
{
   TextBookmark bm = *this;

   size_t columnDisp = 0;
   while (true) {
      if (!bm.goTo(-1))
	     return columnDisp;

	  char ch = (*bm._page).text[bm._offset];
	  if (ch==0x09) {
         columnDisp += calcTabShift(bm._column, Settings::tabSize);
	  }
	  else if (ch==0x0D) {
	  }
	  else if (ch==0x0A) {
	     break;
	  }
	  else columnDisp++;
   }
   return columnDisp;
}
*/
// --- TextScanner ---

TextScanner :: TextScanner(Text* text)
{
   _text = text;
   _text->validateBookmark(_bookmark);
}

const TCHAR* TextScanner :: getLine(size_t& length)
{
   return _text->getLine(_bookmark, length);
}

// --- TextHistorty ---

class BackReader
{
   TCHAR* _buffer;
   size_t _offset;

public:
   size_t getPosition() const { return _offset; }

   int readDWord()
   {
      _offset -= dword_size;

      int dword;
      void* p = _buffer + _offset;
      memcpy(&dword, p, 4);

      return dword;
   }

   const TCHAR* readLine(size_t length)
   {
      _offset -= length + 1;

      return _buffer + _offset;
   }

   BackReader(TCHAR* buffer, size_t offset)
   {
      _buffer = buffer;
      _offset = offset;
   }
};

TextHistory :: TextHistory(int capacity)
{
   _locking = false;
   _capacity = capacity >> 1;

   createstr(_buffer1, _capacity);
   createstr(_buffer2, _capacity);

   _lastOperation = opNone;
   _lastLength = 0;
   _used = _offset = 0;

   _buffer = _buffer1;
   _previous = NULL;
}

void TextHistory :: onInsert(size_t position, size_t length, const TCHAR* line)
{
   if (_locking)
      return;

   addRecord(opInsert, position, length, line);
}

void TextHistory :: onErase(size_t position, size_t length, const TCHAR* line)
{
   if (_locking)
      return;

   addRecord(opErase, position, length, line);
}

void TextHistory :: addRecord(Operation operation, size_t position, size_t length, const TCHAR* line)
{
   LiteralWriter writer(_buffer, _capacity, _offset);

   size_t shift = (operation == opInsert) ? _lastLength : 0;
   size_t freeSpace = _capacity - _offset;
   if (_lastOperation == operation && (_lastPosition + shift) == position) {
	  if (freeSpace > length + dword_size + 1) {
         _lastLength += length;
         writer.writeLiteral(line, length);
	  }
	  else {
         size_t sublength = 0;
         if (freeSpace > dword_size + 1) {
            sublength = freeSpace - (dword_size + 1);
            writer.writeLiteral(line, sublength);
         }
         writer.writeChar(0);
         writer.writeDWord(_lastLength + sublength);

         _lastOperation = opNone;
         _used = writer.Position();
         switchBuffer();
         shift = (operation == opInsert) ? sublength : 0;
         addRecord(operation, position + shift, length - sublength, line + sublength);
         return;
      }
   }
   else {
      if (_lastOperation != opNone) {
         writer.writeChar(0);
         writer.writeDWord(_lastLength);
         freeSpace -= (dword_size + 1);
      }
      _lastOperation = operation;
      _lastPosition = position;
      if (freeSpace >= length + (dword_size * 2 + 1)) {
         _lastLength = length;
         writer.writeDWord((operation == opInsert) ? position : position | 0x80000000);
         writer.writeLiteral(line, length);
      }
      else {
         size_t sublength = 0;
         if (freeSpace > (dword_size * 2 + 1)) {
            sublength = freeSpace - (dword_size * 2 + 1);
            writer.writeDWord((operation == opInsert) ? position : position | 0x80000000);
            writer.writeLiteral(line, sublength);
            writer.writeChar(0);
            writer.writeDWord(sublength);
         }
         _lastOperation = opNone;
         _used = writer.Position();
         switchBuffer();
         shift = operation == opInsert ? sublength : 0;
         addRecord(operation, position + shift, length - sublength, line + sublength);
         return;
      }
   }
   _offset = writer.Position();
   _used = _offset;
}

bool TextHistory :: Bof() const
{
   return (_offset == 0 && (_previous == NULL));
}

bool TextHistory :: Eof() const
{
   if (_used==_offset) {
      return true;
   }
   else return false;
}

void TextHistory :: switchBuffer()
{
   _previous = _buffer;
   _usedPrevious = _used;
   if (_buffer1==_buffer) {
      _buffer = _buffer2;
   }
   else _buffer = _buffer1;

   _used = _offset = 0;
}

void TextHistory :: endRecord()
{
   LiteralWriter writer(_buffer, _capacity, _offset);

   writer.writeChar(0);
   writer.writeDWord(_lastLength);

   _offset = writer.Position();
   _used = _offset;
   _lastOperation = opNone;
}

void TextHistory :: undo(Text* text, TextBookmark& caret)
{
   if (Bof())
      return;

   text->validateBookmark(caret);

   if (_lastOperation != opNone) {
      endRecord();
   }

   if (_offset==0 && _previous) {
      _buffer = _previous;
      _previous = NULL;
	  _used = _offset = _usedPrevious;
   }

   BackReader reader(_buffer, _offset);
   size_t length = reader.readDWord();
   const TCHAR* line = reader.readLine(length);
   size_t position = reader.readDWord();
   _locking = true;
   if (test(position, 0x80000000)) { // opDelete operation
      caret.moveOn((position & 0x7FFFFFFF) - caret.getPosition());
      text->insertLine(caret, line, length);
      caret.moveOn(length);
   }
   else {                            // opInsert operation
      caret.moveOn(position - caret.getPosition());
      text->eraseLine(caret, length);
   }
   _offset = reader.getPosition();
   _locking = false;
}

void TextHistory :: redo(Text* text, TextBookmark& caret)
{
   if (Eof())
      return;

   text->validateBookmark(caret);

   bool eol = false;
   LiteralReader reader(_buffer, _offset);
   size_t position = reader.readDWord();
   const TCHAR* line = (const TCHAR*)reader.getLiteral();
   size_t length = reader.readDWord();
   _locking = true;
   if (test(position, 0x80000000)) { // opDelete operation
      caret.moveOn((position & 0x7FFFFFFF) - caret.getPosition());
      text->eraseLine(caret, length);
   }
   else {                            // opInsert operation
      caret.moveOn(position - caret.getPosition());
      text->insertLine(caret, line, length);
   }
   _locking = false;
   _offset = reader.Position();

   if (_offset==_used && !_previous) {
      switchBuffer();
   }
}
