//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      Text class header
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef textH
#define textH

#define PAGE_SIZE	0x100

#define BM_INVALID  (size_t)-1

using namespace _ELENA_;

namespace _GUI_
{

class Text;
struct TextScanner;

// --- TextWatcher ---

class _TextWatcher
{
public:
   virtual void onInsert(size_t position, size_t length, const TCHAR* line) = 0;
   virtual void onErase(size_t position, size_t length, const TCHAR* line) = 0;
};

typedef List<_TextWatcher*> TextWatchers;

// --- Page ---

struct Page
{
   size_t used;
   size_t rows;
   TCHAR  text[PAGE_SIZE];

   Page()
   {
      used = 0;
      rows = 0;
   }
   Page(int size)
   {
      rows = 0;
      used = size;
   }
   Page(const Page& page)
   {
      rows = page.rows;
      used = page.used;
      _tcsncpy(text, page.text, used);
   }
};

typedef BList<Page>         Pages;

// --- TextBookmark ---

struct TextBookmark
{
   friend class Text;
   friend struct TextScanner;

private:
   // bookmark text position
   size_t         _column;
   size_t         _virtual_column;
   size_t         _row;
   size_t         _length;

   // bookmark stream position
   size_t          _position;
   size_t          _offset;
   Pages::Iterator _page;

   void assign(Pages* pages);

   bool moveToPreviousPage(bool allowEmpty = false);
   bool moveToNextPage(bool allowEmpty = false);
   bool skipEmptyPages(bool onlyForward = false);

   bool goTo(int disp);        // moving without tracing caret coordinate
   bool move(int disp);        // moving with tracing caret coordinate

   bool goBackToBOL();
   bool goNextToBOL();
   size_t seekEOL();

   bool moveToStart();
   void moveToClosestRow(size_t row);
   bool moveToClosestPosition(size_t position);
   void moveToClosestColumn(size_t column);

   //size_t seekBOL();
public:
   TextBookmark& operator =(const TextBookmark& bookmark)
   {
      this->_column = bookmark._column;
      this->_virtual_column = bookmark._virtual_column;
      this->_row = bookmark._row;
      this->_position = bookmark._position;
      this->_page = bookmark._page;
      this->_offset = bookmark._offset;
      this->_length = bookmark._length;

      return *this;
   }

   bool isValid() const { return _position != BM_INVALID; }

   bool getEOL() { return (getLength() <= _column); }

   bool getEOF() const { return _page.Last() && _offset==(*_page).used; }

   size_t getPosition() const { return _position + _offset; }

   size_t getRow() const { return _row; }
   size_t getColumn(bool _virtual = true) const { return _virtual ? _virtual_column : _column; }
   Point getCaret(bool _virtual = true) const
   {
	   return Point(_virtual ? _virtual_column : _column, _row);

   }
   size_t getLength();

   void resetVirtualColumn() { _virtual_column = _column; }

   int getVirtualDiff() const { return _column - _virtual_column; }

   bool moveTo(size_t column, size_t row);
   bool moveOn(int disp);

   void invalidate()
   {
      _position = BM_INVALID;
      _length = BM_INVALID;
   }

   TextBookmark();
   ~TextBookmark();
};

// --- TextScanner ---

struct TextScanner
{
private:
   Text*        _text;
   TextBookmark _bookmark;

public:
   size_t getPosition() const { return _bookmark.getPosition(); }

   const TCHAR* getLine(size_t& length);

   bool goTo(int disp)
   {
      return _bookmark.goTo(disp);
   }

   TextScanner(Text* text);
};

// --- Text ---

class Text
{
   Pages        _pages;
   size_t       _rowCount;

   TextWatchers _watchers;

   void refreshPage(Pages::Iterator page);
   void refreshNextPage(Pages::Iterator page);

   bool compare(TextBookmark bookmark, const TCHAR* line, int len, bool matchCase, const TCHAR* terminators);

   void insert(TextBookmark bookmark, const TCHAR* s, size_t length, bool checkRowCount);
   void erase(TextBookmark bookmark, size_t length, bool checkRowCount);

   int retrieveRowCount();

public:
   size_t getRowCount() const { return _rowCount; }
   size_t getRowLength(size_t row);

   void validateBookmark(TextBookmark& bookmark);

   void create();
   bool load(const TCHAR* path, FileEncoding& encoding);
   void save(const TCHAR* path, FileEncoding encoding);

   void copyLineTo(TextBookmark bookmark, StreamWriter* writer, size_t& length, bool stopOnEOL);
   void copyTo(TextBookmark bookmark, TCHAR* buffer, int length);

   const TCHAR* getLine(TextBookmark& bookmark, size_t& length);
   TCHAR getChar(TextBookmark& bookmark);

   bool findWord(TextBookmark& bookmark, const TCHAR* text, bool matchCase, const TCHAR* terminators);

   bool insertChar(TextBookmark& bookmark, TCHAR ch);
   bool insertLine(TextBookmark& bookmark, const TCHAR* s, size_t length);
   bool insertNewLine(TextBookmark& bookmark);

   bool eraseLine(TextBookmark& bookmark, size_t length);
   bool eraseChar(TextBookmark& bookmark);

   void attachWatcher(_TextWatcher* watcher);
   void detachWatcher(_TextWatcher* watcher);

   Text();
   virtual ~Text();
};

// --- TextHistory ---

class TextHistory : public _TextWatcher
{
   enum Operation { opNone, opInsert, opErase };

   bool     _locking;

   TCHAR*   _buffer1;
   TCHAR*   _buffer2;

   int      _capacity;
   int      _used;
   int      _offset;
   TCHAR*   _buffer;

   TCHAR*   _previous;
   int      _usedPrevious;

   Operation   _lastOperation;
   size_t      _lastLength;
   size_t      _lastPosition;

   void addRecord(Operation operation, size_t position, size_t length, const TCHAR* line);

   void switchBuffer();

   void endRecord();

public:
   bool Eof() const;
   bool Bof() const;

   virtual void onInsert(size_t position, size_t length, const TCHAR* line);
   virtual void onErase(size_t position, size_t length, const TCHAR* line);

   void undo(Text* text, TextBookmark& caret);
   void redo(Text* text, TextBookmark& caret);

   TextHistory(int capacity); // at least 20
   virtual ~TextHistory() { freestr(_buffer1); freestr(_buffer2); }
};

} // _GUI_

#endif // textH
