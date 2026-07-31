//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Assembler Compiler
//
//		This header contains abstract Assembler declarations
//
//                                              (C)2005-2006, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef x86jumphelperH
#define x86jumphelperH

#include "win32/x86helper.h"

namespace _ELENA_
{

class x86JumpHelper
{
   x86LabelHelper            _helper;
   Map<const TCHAR*, size_t> _labels;
   Map<const TCHAR*, size_t> _declaredLabels;

public:
   bool checkDeclaredLabel(const TCHAR* label)
   {
      return _declaredLabels.exist(label);
   }

   void writeJxxForward(const TCHAR* label, int prefix, bool shortJump);
   void writeJxxBack(const TCHAR* label, int prefix, bool shortJump);

   void writeJmpForward(const TCHAR* label, bool shortJump);
   void writeJmpBack(const TCHAR* label, bool shortJump);

   void writeLoopForward(const TCHAR* label);
   void writeLoopBack(const TCHAR* label);

   void writeCallForward(const TCHAR* label);
   void writeCallBack(const TCHAR* label);

   bool addLabel(const TCHAR* label);

	x86JumpHelper(SectionWriter* code)
      : _helper(code)
	{
	}
};

} // _ELENA_

#endif // x86jumphelperH

