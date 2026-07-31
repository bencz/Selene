//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Compiler
//
//		This header contains ELENA Source Reader class declaration.
//
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef sourceH
#define sourceH 1

namespace _ELENA_
{

// --- ELENA DFA Constants ---
typedef TCHAR LineType;

const LineType dfaStart          = 'a';
const LineType dfaSlashOperator  = 'b';
const LineType dfaLineComment    = 'c';
const LineType dfaWhitespace     = 'd';
const LineType dfaKeyword        = 'e';
const LineType dfaIdentifier     = 'f';
const LineType dfaBracket        = 'g';
const LineType dfaOperator       = 'h';
const LineType dfaDblOperator    = 'i';
const LineType dfaFullIdentifier = 'j';
const LineType dfaPrivate        = 'k';
const LineType dfaComment        = 'n';
const LineType dfaMinus          = 'o';
const LineType dfaInteger        = 'p';
const LineType dfaQuote          = 'r';
const LineType dfaHexInteger     = 't';
const LineType dfaReal           = '{';
const LineType dfaWildcard       = 'x';
const LineType dfaProtected      = 'y';
const LineType dfaEOF            = '.';
const LineType dfaError          = '?';
const LineType dfaBack           = '!';

// --- LineTooLong exception class ---

class LineTooLong : _Exception
{
public:
   size_t row;

   LineTooLong(size_t row)
   {
      this->row = row;
   }
};

// --- InvalidChar exception class ---

class InvalidChar : _Exception
{
public:
   int   column, row;
   TCHAR ch;

   InvalidChar(int column, int row, TCHAR ch)
   {
      this->column = column;
      this->row = row;
      this->ch = ch;
   }
};

// --- ELENA Source Reader class ---

struct LineInfo
{
   const TCHAR* line;
   LineType     type;
   int          length;

   int position;
   int column, row;

   LineInfo()
   {
      line = NULL;
      type = 0;
   }
   LineInfo(int position, int column, int row)
   {
      this->position = position;
      this->column = column;
      this->row = row;
   }
};

class SourceReader
{
   TextReader* _source;
   int         _tabSize;
   TCHAR*      _line;
   size_t      _position;
   size_t      _column, _row;

   void cacheLine();

   void nextColumn(int position)
   {
      if (_line[position]=='\t') {
         _column += calcTabShift(_column - 1, _tabSize);
      }
      else _column++;
   }

public:
   LineInfo read(TCHAR* token, size_t length, bool lowerCase = true);

   SourceReader(int tabSize, TextReader* source);
   ~SourceReader() { freestr(_line); }
};
    
} // _ELENA_

#endif // sourceH
