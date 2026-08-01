//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Compiler
//
//		This file contains ELENA Source Reader class implementation.
//
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#include "elena.h"
// --------------------------------------------------------------------------
#include "source.h"
#include "dfa.h"

using namespace _ELENA_ ;

// --- DFA Table ---

const TCHAR* DFA_table[27] =
{
     _T(".????????dd??d??????????????????dhqek?hzgghhgogbpppppppppphghhhhhffffffffffffffffffffffffffg?ggf?ffffffffffffffffffffffffffgug?f"),
     _T("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaalaaaacaaaaaaaaaaaaaiaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
     _T("acccccccccaccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"),
     _T("aaaaaaaaaddaaaaaaaaaaaaaaaaaaaaadaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
     _T("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaeaaeaaaaaaaaeeeeeeeeeeaaaaaaaeeeeeeeeeeeeeeeeeeeeeeeeeeaaaaaaeeeeeeeeeeeeeeeeeeeeeeeeeeaaaaa"),
     _T("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaazaaaaaaaaffffffffffaaaaaaaffffffffffffffffffffffffffaaaafaffffffffffffffffffffffffffaaaaf"),
     _T("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
     _T("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaiaiiiaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
     _T("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
     _T("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaazaaxaaaaajjjjjjjjjjaaaaaaajjjjjjjjjjjjjjjjjjjjjjjjjjaaaajajjjjjjjjjjjjjjjjjjjjjjjjjjaaaaj"),
     _T("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa?aaaaaaaaaaakkkkkkkkkkaaaaaaakkkkkkkkkkkkkkkkkkkkkkkkkkaaaakakkkkkkkkkkkkkkkkkkkkkkkkkkaaaak"),
     _T("?lllllllllllllllllllllllllllllllllllllllllmlllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllll"),
     _T("?llllllllllllllllllllllllllllllllllllllllllllllnllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllll"),
     _T("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
     _T("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaappppppppppaaaiiaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
     _T("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaavappppppppppaaaaaaassssssaaaaaaaaaaaaaaaaaaaaaaaaaassssssataaaaaaaaaaaaaaaaaaaaaaa"),
     _T("?qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqrqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq"),
     _T("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaqaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
     _T("????????????????????????????????????????????????ssssssssss???????ssssss??????????????????????????ssssss?t???????????????????????"),
     _T("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
     _T("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaiaaa"),
     _T("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!wwwwwwwwww!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"),
     _T("????????????????????????????????????????????????wwwwwwwwww????????????????????????????????????????????????????????{?????????????"),
     _T("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
     _T("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa?aaaaaaaaaaayyyyyyyyyyaaaaaaayyyyyyyyyyyyyyyyyyyyyyyyyyaaaayayyyyyyyyyyyyyyyyyyyyyyyyyyaaaay"),
     _T("????????????????????????????????????y?????x?????jjjjjjjjjj???????jjjjjjjjjjjjjjjjjjjjjjjjjj????j?jjjjjjjjjjjjjjjjjjjjjjjjjj????j"),
     _T("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")
};

// --- SourceReader ---

SourceReader :: SourceReader(int tabSize, TextReader* source)
{
   _tabSize = tabSize;
   _source = source;
   _row = 0;
   createstr(_line, LINE_LEN + 1);

   cacheLine();
}

void SourceReader :: cacheLine()
{
   if (!_source->read(_line, LINE_LEN))
      _line[0] = 0;

   _position = 0;
   _row++;
   _column = 1;
   if (getlength(_line)==LINE_LEN)
      throw LineTooLong(_row);
}

LineInfo SourceReader :: read(TCHAR* token, size_t length, bool lowerCase)
{
   LineInfo info(_position, _column, _row);

   DFA<DFA_table, dfaStart, dfaWhitespace, dfaBack, dfaLineComment, dfaComment> dfa;
   while (true) {
      if (_line[_position]=='\0') {
         cacheLine();
         info = LineInfo(_position, _column, _row);
      }
      if (dfa.makeStep(_line[_position], _line[_position+1])) {
         if (dfa.back)
            break;

         nextColumn(_position++);
         if (dfa.isSkipState()) {
            info = LineInfo(_position, _column, _row);
            dfa.reset();
         }
         else break;
      }
      else nextColumn(_position++);
   }
   if (dfa.state == dfaError)
      throw InvalidChar(info.column, info.row, _line[_position - 1]);

   info.type = dfa.state;
   info.length = _position - info.position;
   if (dfa.state == dfaQuote)
      info.line = _line + info.position;
   else {
      info.line = token;

      _tcsncpy(token, _line + info.position, info.length);
      token[info.length] = 0;
      if (lowerCase)
         _tcslwr(token);
   }
   return info;
}
