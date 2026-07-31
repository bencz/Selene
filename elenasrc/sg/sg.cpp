//---------------------------------------------------------------------------
//              E L E N A   p r o j e c t
//                Command line syntax generator main file
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#include "elena.h"
////---------------------------------------------------------------------------
#include "source.h"
#include "parsertable.h"
#include "syntax.h"

#include <stdarg.h>

using namespace _ELENA_;

int last_id = 0;

void printLine(const char* msg, ...)
{
   va_list argptr;

   va_start(argptr, msg);
   vprintf(msg, argptr);
   va_end(argptr);
   fflush(stdout);
}

void printLine(const wchar_t* msg, ...)
{
   va_list argptr;

   va_start(argptr, msg);
   vwprintf(msg, argptr);
   va_end(argptr);
   fflush(stdout);
}

int registerSymbol(ParserTable& table, TCHAR* symbol, int new_id)
{
	if (compstr(symbol, _T("||")))
		symbol++;

	if (compstr(symbol, _T("-->")))
		symbol++;

   int id = (int)table.defineSymbol(symbol);
   if (id == 0) {
      id = new_id;

      if ((symbol[0]<'A')||(symbol[0]>'Z'))
         id |= mskTerminal;

      table.registerSymbol(id, symbol);

      if (last_id < (id & ~mskAnySymbolMask)) last_id = id & ~mskAnySymbolMask;
   }
   return id;
}

int main(int argc, char* argv[])
{
   printLine(TEXT("ELENA command line syntax generator (C)2005-2009 by Alexei Rakov\n"));
   if (argc != 2) {
      printLine(TEXT("sg <syntax_file>"));
      return 0;
   }
   try {
#ifdef _UNICODE
      LocalString<260> param;
		param.convert(argv[1]);
      TextFileReader  sourceFile(param, feAutodetect);
#else
		TextFileReader  sourceFile(argv[1], feAutodetect);
#endif
      SourceReader    source(4, &sourceFile);
      ParserTable     table;
      LineInfo        info(0, 0, 0);
      TCHAR           token[IDENTIFIER_LEN + 1];
      int             rule[20];
      int             rule_len = 0;
      bool            arrayCheck = false;

      table.registerSymbol(ParserTable::nsEps, _T("eps"));

      while (true) {
         info = source.read(token, IDENTIFIER_LEN, false);

         if (info.type == dfaEOF) break;

         if (compstr(token, _T("__define"))) {
            source.read(token, IDENTIFIER_LEN, false);

            TCHAR number[10];
            source.read(number, 10, false);

            registerSymbol(table, token, _ttoi(number));
         }
         else if (compstr(token, _T("->")) && !arrayCheck) {
            if (rule_len > 2) {
               table.registerRule(rule[0], rule + 1, rule_len - 2);

               rule[0] = rule[rule_len - 1];
               rule_len = 1;
            }
            arrayCheck = true;
         }
         else if (compstr(token, _T("|")) && rule_len != 1) {
            arrayCheck = false;
            table.registerRule(rule[0], rule + 1, rule_len - 1);

            rule_len = 1;
         }
         else {
            arrayCheck = false;
            rule[rule_len++] = registerSymbol(table, token, last_id + 1);
            if (compstr(token, _T("|")))
               source.read(token, IDENTIFIER_LEN, false);
         }
      }
//      table.registerRule(rule[0], rule + 1, rule_len - 1);

      printLine(_T("generating...\n"));

      if (!table.generate()) {
         printLine(_T("error:syntax ambigous\n"));
         return -1;
      }

      printLine(_T("saving...\n"));

#ifdef _UNICODE
      LocalPath outputFile(NULL, param);
#else
      LocalPath outputFile(NULL, argv[1]);
#endif
      outputFile.changeExtension(_T("dat"));

      FileWriter file(outputFile, feRaw);
      table.save(&file);
   }
   catch(_ELENA_::InvalidChar& e) {
      printLine(_T("(%d:%d): Invalid char %c\n"), e.row, e.column, e.ch);
   }
   return 0;
}

