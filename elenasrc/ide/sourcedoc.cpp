//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      SourceDoc class implementation
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#include "idecommon.h"
// --------------------------------------------------------------------------
#include "sourcedoc.h"
#include "idesettings.h"

using namespace _GUI_;

#define OPENING_BRACKET   TEXT("({[") 
#define CLOSING_BRACKET   TEXT(")}]") 

// --- Lexical DFA Table ---

const TCHAR lexStart = 'a';
const TCHAR lexLookahead = 'b';
const TCHAR lexOperator = 'c';
const TCHAR lexLineComment = 'd';
const TCHAR lexKeyword = 'e';
const TCHAR lexMessage = 'f';
const TCHAR lexColon = 'g';
const TCHAR lexObject = 'i';
const TCHAR lexDigit = 'j';
const TCHAR lexQuote = 'k';
const TCHAR lexQuote2 = 'l';
const TCHAR lexComment = 'm';
const TCHAR lexComment2= 'n';
const TCHAR lexStick = 'o';
const TCHAR lexCloseBracket = 'p';
const TCHAR lexProperty = 'q';      // !! obsolete
const TCHAR lexHintOrStart = 'r';
const TCHAR lexHint = 's';
const TCHAR lexOperator2 = 't';
const TCHAR lexOperator3 = 'u';

const TCHAR* lexDFA[] =
{
     _T("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaackeiacacpccctcbjjjjjjjjjjgcccccciiiiiiiiiiiiiiiiiiiiiiiiiicacciaiiiiiiiiiiiiiiiiiiiiiiiiiicopaa"),
     _T("cccccccccccccccccccccccccccccccccccccccccpmccccdccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccopaa"),
     _T("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaackeaaaacpcccccbjjjjjjjjjjgccccccaaaaaaaaaaaaaaaaaaaaaaaaaacaccaaaaaaaaaaaaaaaaaaaaaaaaaaaacopaa"),
     _T("dddddddddddddadddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd"),
     _T("aaaaaaaaaraaaaaaaaaaaaaaaaaaaaaarckaeaaacpcccccbaaaaaaaaaaacccccceeeeeeeeeeeeeeeeeeeeeeeeeesacceaeeeeeeeeeeeeeeeeeeeeeeeeeecopaa"),
     _T("aaaaaaaaaffaafaaaaaaaaaaaaaaaaaafckefacacpccctcbffffffffffgccccccffffffffffffffffffffffffffcaccfaffffffffffffffffffffffffffcopaa"),
     _T("aaaaaaaaahhaahaaaaaaaaaaaaaaaaaahckeaacacpcccccbjjjjjjjjjjccccccciiiiiiiiiiiiiiiiiiiiiiiiiicacciaiiiiiiiiiiiiiiiiiiiiiiiiiicopii"),
     _T("hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhckfffcfcpfcfffbjjjjjjjjjjfcccccciiiiiiiiiiiiiiiiiiiiiiiiiicacciaiiiiiiiiiiiiiiiiiiiiiiiiiicopii"),
     _T("aaaaaaaaaafaafaaaaaaaaaaaaaaaaaafckeiaaacpacctcbiiiiiiiiiigcccccciiiiiiiiiiiiiiiiiiiiiiiiiicacciaiiiiiiiiiiiiiiiiiiiiiiiiiicopaa"),
     _T("aaaaaaaaaffaafaaaaaaaaaaaaaaaaaafckaaacacpcccccbjjjjjjjjjjgccccccjjjjjjaaaaaaaaaaaaaaaaaaaaccccaajjjjjjajaaaaaaaaajaaaaaaaacopaa"),
     _T("kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkklkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk"),
     _T("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaafakaaaaaaaaaaaaaaaaaaaaaaagaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
     _T("mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmnmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm"),
     _T("mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmcmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm"),
     _T("fffffffffffffffffffffffffffffffffckfffffcpfccffbfffffffffffccfcccffffffffffffffffffffffffffcffffffffffffffffffffffffffffffffofff"),
     _T("fffffffffffffffffffffffffffffffffckfffffcpccctcbffffffffffgccfcccffffffffffffffffffffffffffcfffffffffffffffffffffffffffffffpopff"),
     _T("aaaaaaaaaffaaqaaaaaaaaaaaaaaaaaafckeqaaqcpccctcbqqqqqqqqqqgccccccqqqqqqqqqqqqqqqqqqqqqqqqqqcaccqaqqqqqqqqqqqqqqqqqqqqqqqqqqcopaa"),
     _T("aaaaaaaaaraaaaaaaaaaaaaaaaaaaaaarcaaaaaaaacccccbaaaaaaaaaaaccccccaaaaaaaaaaaaaaaaaaaaaaaaaasaccaaaaaaaaaaaaaaaaaaaaaaaaaaaacaaaa"),
     _T("sssssssssssssssssssssssssssssssssssssssssssssssbssssssssssssssssssssssssssssssssssssssssssssscssssssssssssssssssssssssssssssssss"),
     _T("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaackeaaaacpcccccbjjjjjjjjjjgcccuccaaaaaaaaaaaaaaaaaaaaaaaaaacaccaaaaaaaaaaaaaaaaaaaaaaaaaaaacopaa"),
     _T("qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqbqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq")
};

inline TCHAR makeStep(TCHAR ch, TCHAR state)
{
   return ch < 128 ? lexDFA[state - lexStart][ch] : lexDFA[state - lexStart][127];
}

inline size_t defineStyle(TCHAR state, size_t style)
{
   switch (state) {
   case lexStart:
   case lexObject:
      return STYLE_DEFAULT;
   case lexKeyword:
      return STYLE_KEYWORD;
   case lexMessage:
      return STYLE_MESSAGE;
   case lexOperator:
   case lexOperator2:
   case lexOperator3:
   case lexColon:
   case lexStick:
   case lexCloseBracket:
   case lexLookahead:
      return STYLE_OPERATOR;
   case lexLineComment:
   case lexComment:
   case lexComment2:
      return STYLE_COMMENT;
   case lexDigit:
      return STYLE_NUMBER;
   case lexQuote:
   case lexQuote2:
      return STYLE_STRING;
   case lexProperty:
      return STYLE_PROPERTY;
   case lexHint:
      return STYLE_HINT;
   default:
      return style;
   }
}

// -- LexicalStylist ---

LexicalStylist :: LexicalStylist()
{
}

void LexicalStylist :: parse(Text* text)
{
   _index.clear();
   _lexic.clear();

   DumpWriter indexWriter(&_index, 0);
   DumpWriter writer(&_lexic, 0);
   size_t       indexedPos = INDEX_STEP;

   TCHAR state = lexStart;
   const TCHAR* s;
   size_t length, style = STYLE_DEFAULT;
   bool lookAhead = false;

   TextScanner scanner(text);
   indexWriter.writeDWord(0);
   while (true) {
	  if (scanner.getPosition() == indexedPos) {
         indexWriter.writeDWord(writer.Position());
         indexedPos += INDEX_STEP;
      }
	  s = scanner.getLine(length);
	  for (size_t i = 0 ; i < length ; i++) {
	     state = makeStep(s[i], state);
         if (lookAhead) {
            style = defineStyle(state, style);
         }
         else if (style != defineStyle(state, style) || state == lexLookahead) {
            writer.writeDWord(style);
            writer.writeDWord(scanner.getPosition() + i);
            style = defineStyle(state, style);
         }
         lookAhead = (state == lexLookahead);
	  }
	  if (!scanner.goTo(length))
         break;
   }
   writer.writeDWord(style);
   writer.writeDWord(scanner.getPosition() + length);
}

size_t LexicalStylist :: proceed(_GUI_::LineInfo& info)
{
   size_t count = 0;
   if (info.param==0) {
	  info.param = retrievePosition(info.bookmark.getPosition());
   }
   size_t infoPos = info.bookmark.getPosition();
   DumpReader reader(&_lexic, info.param);
   size_t style = 0, position = 0;
   while (reader.readDWord(style)) {
      reader.readDWord(position);
      if (position > infoPos) {
         info.style = style;
         count = position - infoPos;
         break;
      }
      info.param = reader.Position();
   }
   return count;
}

// --- SourceDoc ---

SourceDoc :: SourceDoc(Text* text, FileEncoding encoding)
   : Document(text, encoding)
{
   _tracking = false;

   _stylist.parse(text);

   _withBracketHighlighting = Settings::highlightBrackets;
   _highlightedBrackets.x = -1;
   _highlightedBrackets.y = -1;
}

void SourceDoc :: onChange()
{
   if (Settings::highlightSyntax) {
      _stylist.parse(getText());
   }

   Document::onChange();
}

void SourceDoc :: refresh()
{
   if (Settings::highlightSyntax) {
      _stylist.parse(getText());
   }
}

void SourceDoc :: setTracker(TrackInfo info, int lineStyle, int style)
{
   _tracking = true;
   _tracker = info;
   _trackLineStyle = lineStyle;
   _trackStyle = style;
}

void SourceDoc :: clearTracker()
{
   _tracking = false;
}

inline size_t evaluateLength(_GUI_::TextBookmark& bookmark, size_t column)
{
   _GUI_::TextBookmark bm = bookmark;

   bm.moveTo(column, bm.getRow());

   return bm.getPosition() - bookmark.getPosition();
}

size_t SourceDoc :: defineStyle(_GUI_::LineInfo& info)
{
   size_t pos = info.bookmark.getPosition();
   size_t count = Document::defineStyle(info);

   if (info.style == STYLE_DEFAULT) {
      size_t styleLen = count;
      if (Settings::highlightSyntax)
         styleLen = _stylist.proceed(info);

      Point caret = info.bookmark.getCaret();
      if (_tracking && caret.y == _tracker.row) {
         int trackColumn = (_tracker.col == 0) ? retrieveColumn(_tracker.row, _tracker.disp) : _tracker.col;
         info.bandLine = true;
         if (caret.x < trackColumn) {
            styleLen =  evaluateLength(info.bookmark, trackColumn);
            info.style = _trackLineStyle;
         }
         else if (caret.x >= trackColumn && caret.x < trackColumn + _tracker.length) {
            styleLen = _tracker.length - evaluateLength(info.bookmark, trackColumn);
            info.style = _trackStyle;
         }
         else info.style = _trackLineStyle;
      }
      else {
         int marker = _markers.get(caret.y);
         if (marker != 0) {
            info.style = marker;
            info.bandLine = true;
         }
         else {
            // highlight openning and closing brackets
            if (_highlightedBrackets.x == pos || _highlightedBrackets.y == pos) {
               info.style = STYLE_HIGHLIGHTED_BRACKET;
               count = 1;               
            }
         }
      }
      if (count > styleLen) {
         count = styleLen;
      }
      // to allow bracket highlighting foregoing by another operator
      if(_withBracketHighlighting && info.style == STYLE_OPERATOR) {
         if (isbetween(pos, count, _highlightedBrackets.x)) {
            count = _highlightedBrackets.x - pos;
         }
         else if (isbetween(pos, count, _highlightedBrackets.y))
            count = _highlightedBrackets.y - pos;
      }
   }
   return count;
}

bool SourceDoc :: findBracket(TextBookmark& bookmark, char starting, char ending, bool forward)
{
   // define the upper / lower border of bracket search
   int frameY = _frame.getRow();
   if (forward)
      frameY += _size.x;

   int counter = 0;
   while (true) {
      TCHAR ch = _text->getChar(bookmark);
      if (ch == starting)
         counter++;
      else if (ch == ending) {
         counter--;
         if (counter==0)
            return true;
      }

      if (forward) {
         if (!bookmark.moveOn(1) || (bookmark.getRow() > frameY))
            break;
      }
      else {
         if (!bookmark.moveOn(-1) || bookmark.getRow() < frameY)
            break;
      }
   }
   return false;
}

bool SourceDoc :: highlightCharacter()
{
   if(!_withBracketHighlighting)
      return false;

   TCHAR current_ch = _text->getChar(_caret);

   if (chrpos(OPENING_BRACKET, current_ch) != -1) {
      int pos = chrpos(OPENING_BRACKET, current_ch);
      _highlightedBrackets.x = _caret.getPosition();

      TextBookmark bookmark = _caret;
      if (findBracket(bookmark, OPENING_BRACKET[pos], CLOSING_BRACKET[pos], true)) {
         _highlightedBrackets.y = bookmark.getPosition();
      }
      else _highlightedBrackets.y = -1;

      return true;
   }
   else if (chrpos(CLOSING_BRACKET, current_ch) != -1) {
      int pos = chrpos(CLOSING_BRACKET, current_ch);
      _highlightedBrackets.y = _caret.getPosition();

      TextBookmark bookmark = _caret;
      if (findBracket(bookmark, CLOSING_BRACKET[pos], OPENING_BRACKET[pos], false)) {
         _highlightedBrackets.x = bookmark.getPosition();
      }
      else _highlightedBrackets.x = -1;

      return true;
   }
   else if(_highlightedBrackets.x > -1) {
      _highlightedBrackets.x = -1;
      _highlightedBrackets.y = -1;

      return true;
   }

   return false;
}
