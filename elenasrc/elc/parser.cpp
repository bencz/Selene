//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Compiler
//
//		This file contains ELENA Parser class implementation.
//
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#include "elena.h"
// --------------------------------------------------------------------------
#include "parser.h"
#include "errors.h"

using namespace _ELENA_ ;

// --- SyntaxError ---

SyntaxError :: SyntaxError(int column, int row, const TCHAR* token)
{
   this->error = errInvalidSyntax;
   this->column = column;
   this->row = row;
   this->token = token;
}

SyntaxError :: SyntaxError(int column, int row, const TCHAR* token, const TCHAR* error)
{
   this->error = error;
   this->column = column;
   this->row = row;
   this->token = token;
}


inline TCHAR* getError(Symbol symbol)
{
   switch(symbol)
   {
   case nsErrDotExpected:
   case nsFieldErrDotExpected:
      return errDotExpectedSyntax;
   case nsErrClosingBracketExpected:
   case nsErrClosingBracketExpected2:
      return errCBrExpectedSyntax;
   case nsErrOpenBracketExpected:
      return errOBrExpectedSyntax;
   case nsErrOpenActionBracketExpected:
      return errOActionExpectedSyntax;
   case nsErrClosingSBracketExpected:
   case nsErrClosingSBracketExpected2:
   case nsErrClosingSBracketExpected3:
      return errCSBrExpectedSyntax;
   case nsErrClosingBraceExpected:
      return errCBraceExpectedSyntax;
   case nsErrVariableNameExpected:
   case nsErrVariableNameExpected2:
      return errVarNameExpectedSyntax;
   case nsErrExtensionNotExpected:
      return errExtensionNotAllowed;
   default:
      return errInvalidSyntax;
   }
}

TerminalInfo getTerminalInfo(ParserTable& table, LineInfo info)
{
   TerminalInfo terminal;
   terminal.value = info.line;
   terminal.col = info.column;
   terminal.row = info.row;
   terminal.disp = info.position;
   terminal.length = info.length;
   switch (info.type) {
      case dfaQuote:
         terminal.symbol = tsLiteral;
         break;
      case dfaEOF:
         terminal.symbol = tsEof;
         terminal.value = _T("<end of file>");
         break;
      case dfaIdentifier:
         terminal.symbol = tsIdentifier;
         break;
	   case dfaFullIdentifier:
         terminal.symbol = tsReference;
         break;
	   case dfaPrivate:
         terminal.symbol = tsPrivate;
         break;
	   case dfaInteger:
         terminal.symbol = tsInteger;
         break;
	   case dfaHexInteger:
         terminal.symbol = tsHexInteger;
         break;
	   case dfaReal:
         terminal.symbol = tsReal;
         break;
      case dfaWildcard:
         terminal.symbol = tsWildcard;
         break;
      case dfaProtected:
         terminal.symbol = tsProtected;
         break;
	   default:
         terminal.symbol = (Symbol)table.defineSymbol(terminal);
   }
   return terminal;
}

// --- Parser ---

Parser :: Parser(StreamReader* syntax)
{
   _table.load(syntax);
}

bool Parser :: derive(TerminalInfo& terminal, ParserStack& stack, DerivationWriter* writer, bool& traceble)
{
   Symbol current = (Symbol)stack.pop();
   while (!test(current, mskTerminal)) {
      traceble = test(current, mskTraceble);
      if (current == nsNone)
         writer->writeSymbol(nsNone);
      else {
         if (traceble) {
            stack.push(nsNone);
            writer->writeSymbol(current);
         }
         if (!_table.read(current, terminal.symbol, stack))
            return false;

         if (test(current, mskError))
            throw SyntaxError(terminal.Col(), terminal.Row(), terminal.value, getError(current));
      }
      current = (Symbol)stack.pop();
   }
   return (terminal == current);
}

void Parser :: parse(TextReader* reader, DerivationWriter* writer, int tabSize)
{
   SourceReader source(tabSize, reader);
   ParserStack  stack(tsEof);
   TerminalInfo terminal;

   stack.push(nsStart);
   writer->writeSymbol(nsStart);
   do {
      terminal = getTerminalInfo(_table, source.read(_buffer, IDENTIFIER_LEN));

      bool traceble = false;
      if (!derive(terminal, stack, writer, traceble))
         throw SyntaxError(terminal.Col(), terminal.Row(), _buffer);

      if (traceble)
         writer->writeTerminal(terminal);

   } while (terminal != tsEof);
}
