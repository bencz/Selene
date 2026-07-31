//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Assembler Compiler
//
//		This header contains abstract Assembler declarations
//
//                                              (C)2005-2006, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef assemblerH
#define assemblerH

namespace _ELENA_
{

// --- AssemblerException ---

struct AssemblerException
{
	const TCHAR* message;
	int          row;

	AssemblerException(const TCHAR* message, int row)
	{
		this->message = message;
		this->row = row;
	}
};

// --- Assembler ---

class Assembler
{
public:
	virtual void compile(TextReader* reader, const TCHAR* outputPath) = 0;

	virtual ~Assembler() {}
};

} // _ELENA_

#endif // assemblerH
