//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Engine
//
//		This file contains String class implementations
//
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
// --------------------------------------------------------------------------
#include "elena.h"
#include "x86assembler.h"
#include "source.h"

int main(int argc, char* argv[])
{
	printf("ELENA command line Simplified Assembler Compiler (C)2007-2009 by Alexei Rakov\n");

   if (argc<2) {
      printf("asm2bin <file.asm> <output path>");
      return 0;
   }
	_ELENA_::Path target;

#ifdef _UNICODE
	_ELENA_::String prm1;
    prm1.convert(argv[1]);

	if (argc==3) {
      _ELENA_::FileName name(prm1);

		_ELENA_::String prm2;
		prm2.convert(argv[2]);
		target.copy(prm2);
		target.combine(name);
	}
	else target.copy(prm1);
#else
	if (argc==3) {
		_ELENA_::FileName name(argv[1]);

		target.copy(argv[2]);
		target.combine(name);
	}
	else target.copy(argv[1]);
#endif

	target.changeExtension(TEXT("bin"));

#ifdef _UNICODE
	_ELENA_::TextFileReader reader(prm1, _ELENA_::feAutodetect);
#else
	// The ANSI branch predates the encoding parameter and stopped compiling
	// when it was added; it was never exercised because every build was Unicode.
	_ELENA_::TextFileReader reader(argv[1], _ELENA_::feAutodetect);
#endif
	_ELENA_::x86Assembler	assembler;

	try {
		assembler.compile(&reader, target);

		printf("Successfully compiled");
	}
	catch(_ELENA_::InvalidChar& e) {
       printf("(%d): Invalid char %c\n", e.row, e.ch);
	}
	catch(_ELENA_::AssemblerException& e) {
#ifdef _UNICODE
      wprintf(e.message, e.row);
#else
		printf(e.message, e.row);
#endif
	}
   return 0;
}
