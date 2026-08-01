//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      Document class header         
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef documentH
#define documentH

#include "text.h"
#include "plugins.h"

namespace _GUI_
{

#define UNDO_BUFFER_SIZE 0x20000

class Document;

// --- LineInfo ---

struct LineInfo
{
   TextBookmark bookmark;
   Rectangle    frame;
   size_t         read;

   size_t         style;
   size_t         param;

   bool         bandLine;
   bool         newLine;   

   void readLine(Document* document, StreamWriter* writer);

   bool goToNext();

   LineInfo(TextBookmark& start, Point size)
   {
      bookmark = start;
      frame.topLeft = start.getCaret();
      frame.bottomRight = frame.topLeft + size - Point(1, 1);

      read = style = param = 0;

      newLine = true;
   }
   LineInfo()
   {
      param = 0;
   }
};

struct DocStatus
{
   bool maxColChanged;
   bool modified;
   bool unnamed;
   bool included;

   int  rowDifference;

   DocStatus()
   {
      maxColChanged = false;
      modified = false;
      unnamed = false;
      included = false;

      rowDifference = 0;
   }
};

// --- Document --- 
class Document : public _TextWatcher, public _Document
{
protected:
   friend struct LineInfo;

   FileEncoding  _encoding;
   Text*         _text;
   TextHistory   _undoBuffer;

   Point         _size;
   TextBookmark  _frame;
   TextBookmark  _caret;
   int           _selection; 
   bool          _overwrite;

   size_t        _maxColumn;

   Map<size_t, int> _markers; 
   
   virtual void onErase(size_t position, size_t length, const TCHAR* line);
   virtual void onInsert(size_t position, size_t length, const TCHAR* line);

   virtual void onChange() { status.modified = true; }

   virtual size_t defineStyle(LineInfo& info);
   
   bool eraseSelection();

   virtual bool highlightCharacter() { return false; }

public:
   int retrieveColumn(int row, int disp);

   DocStatus status;

   bool isCaretEOL() { return _caret.getEOL(); }
   bool isOverwriteMode() const { return _overwrite; }
   bool hasSelection() const { return (_selection != 0); }
   bool hasMultilineSelection() const;
   bool canUndo();
   bool canRedo();

   Text* getText() const { return _text; }

   Point getFrame() const { return _frame.getCaret(); }
   Point getCaret(bool _virtual = true) const { return _caret.getCaret(_virtual); }
   Point getSize() const { return _size; }

   int getCaretRow() const { return _caret.getCaret(true).y; }
   int getRowCount() const { return _text->getRowCount(); }
   int getMaxColumn() const { return _maxColumn; }
   int getSelectionLength();

   LineInfo startReading(StreamWriter* writer);
   bool continueReading(LineInfo& info, StreamWriter* writer);

   bool setCaret(TrackInfo info, bool selecting);
   bool setCaret(int column, int row, bool selecting);
   bool setCaret(Point caret, bool selecting);
   bool scroll(int barType, int displacement);
   bool moveFirst(bool selecting);
   bool moveLast(bool selecting);
   bool moveLeft(bool selecting);
   bool moveLeftToken(bool selecting);
   bool moveRight(bool selecting);
   bool moveRightToken(bool selecting);
   bool moveFrameUp();
   bool moveUp(bool selecting);
   bool moveFrameDown();
   bool moveDown(bool selecting);
   bool moveHome(bool selecting);
   bool moveEnd(bool selecting);
   bool movePageUp(bool selecting);
   bool movePageDown(bool selecting);
   bool moveToFrame(size_t column, size_t row, bool selecting);
   void selectWord(); 

   bool findLine(const TCHAR* text, bool matchCase, bool wholeWord);

   void copySelection(TCHAR* text);

   void insertChar(TCHAR ch);
   void insertNewLine();
   void insertLine(const TCHAR* text, int length);
   void duplicateLine();
   void eraseChar(bool moveback);
   void eraseLine();
   void trim();
   void indent(TCHAR space, size_t count);
   void outdent();
   void swap();
   void toLowercase();
   void toUppercase();

   void commentBlock();
   void uncommentBlock();

   void undo();
   void redo();

   void setOverwriteMode(bool value) { _overwrite = value; }

   void save(const TCHAR* path);

   void resize(Point size);

   virtual void addMarker(size_t row, int style);
   virtual bool checkMarker(size_t row, int style);
   virtual void removeMarker(size_t row);
   virtual void shiftMarkers(size_t startRow, int offset);
   virtual void clearMarkers();

   virtual void setTracker(TrackInfo info, int lineStyle, int style) {}
   virtual void clearTracker() {}

   virtual void refresh() {}

   virtual void onTabSizeChange();

   Document(Text* text, FileEncoding encoding);
   virtual ~Document();
};

} // _GUI_

#endif // documentH
