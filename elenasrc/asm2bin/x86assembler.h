//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Assembler Compiler
//
//		This header contains abstract Assembler declarations
//
//                                              (C)2005-2006, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef x86assemblerH
#define x86assemblerH

#include "elena.h"
#include "source.h"
#include "assembler.h"
#include "win32/x86helper.h"
#include "x86jumphelper.h"

namespace _ELENA_
{

// --- x86Assembler ---

class x86Assembler : public Assembler
{
   typedef x86Helper::Operand       Operand;
   typedef x86Helper::OperandType   OperandType;

protected:
	Map<const TCHAR*, size_t> constants;

	struct TokenInfo
	{
		SourceReader* reader;
		TCHAR         value[50];
		LineInfo      terminal;

      bool Eof() const { return terminal.type == dfaEOF; }

		void raiseErr(const TCHAR* err)
		{
			throw AssemblerException(err, terminal.row);
		}

		bool getInteger(int& integer)
		{
         if (terminal.type==dfaInteger) {
				integer = _ttoi(value);
				return true;
			}
			else if (terminal.type==dfaHexInteger) {
				value[getlength(value)-1] = 0;
				integer = _tcstoul(value, NULL, 16);
				return true;
			}
			else return false;
		}

		const TCHAR* read()
		{
			terminal = reader->read(value, 50, false);

			return value;
		}

		const TCHAR* read(const TCHAR* word, const TCHAR* err)
		{
			read();
			if (!check(word))
				raiseErr(err);

			return value;
		}

		int readInteger()
		{
			read();
			int integer;
			if (getInteger(integer)) {
				return integer;
			}
			else raiseErr(_T("Invalid number (%d)\n"));
			return 0;
		}

		bool check(const TCHAR* word)
		{
			return compstr(value, word);
		}

      TokenInfo(SourceReader* reader)
      {
         this->reader = reader;
      }
	};

	struct ProcedureInfo
	{
      _Module* binary;

	   Map<const TCHAR*, int> parameters;

      ProcedureInfo(_Module* binary)
      {
         this->binary = binary;
      }
	};

	void checkComma(TokenInfo& token)
	{
		if (!token.check(_T(",")))
			throw AssemblerException(_T("',' exprected(%d)\n"), token.terminal.row);
	}

	void setOffsetSize(Operand& operand)
	{
		if (abs(operand.offset) <= 0x80 && (size_t)operand.offset != 0x80000000) {
         operand.type = x86Helper::otDB;
		}
		else operand.type = x86Helper::otDD;
	}

   void loadDefaultConstants();

   void readParameterList(TokenInfo& token, ProcedureInfo& info);

	int readStReg(TokenInfo& token);

	bool readOffset(TokenInfo& token, ProcedureInfo& info, const TCHAR* err, Operand& operand);

	Operand defineRegister(TokenInfo& token);
	Operand defineOperand(TokenInfo& token, ProcedureInfo& info, const TCHAR* err);

   Operand readOperand(TokenInfo& token, ProcedureInfo& info, const TCHAR* err, OperandType prefix);
	Operand readPtrOperand(TokenInfo& token, ProcedureInfo& info, const TCHAR* err, OperandType prefix);

	Operand compileOperand(TokenInfo& token, ProcedureInfo& info, const TCHAR* err);

	void compileMOV(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileCMP(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileADD(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileADC(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileAND(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileXOR(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileOR(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileLEA(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileSUB(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileSBB(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileTEST(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileSHR(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
   void compileSAR(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileSHL(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
   void compileSHLD(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
   void compileSHRD(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileROL(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileROR(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileRCR(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileRCL(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileXCHG(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);

	void compileMOVZX(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);

	void compileRET(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileNOP(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileCDQ(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileLODSD(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileLODSW(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileLODSB(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileSTOSD(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileSTOSB(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileMOVSB(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileSTOSW(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileCMPSB(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileSTC(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileSAHF(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compilePUSHFD(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compilePOPFD(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);

	void compileREP(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileREPZ(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);

	void compilePUSH(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compilePOP(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);

	void compileDEC(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileINC(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileNEG(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileNOT(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileMUL(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileIMUL(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileIDIV(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileDIV(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);

	void compileCALL(TokenInfo& token, ProcedureInfo& info, SectionWriter* code, x86JumpHelper& helper);
	void compileLOOP(TokenInfo& token, ProcedureInfo& info, SectionWriter* code, x86JumpHelper& helper);

   void compileJxx(TokenInfo& token, ProcedureInfo& info, SectionWriter* code, int prefix, x86JumpHelper& helper);
	void compileJMP(TokenInfo& token, ProcedureInfo& info, SectionWriter* code, x86JumpHelper& helper);

	void fixJump(TokenInfo& token, ProcedureInfo& info, SectionWriter* code, x86JumpHelper& helper);

	void compileFLDZ(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileFLDL2T(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileFLDLG2(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileFMULP(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileFRNDINT(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileF2XM1(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileFLD1(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileFADDP(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileFSCALE(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileFXAM(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileFABS(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
   void compileFSQRT(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
   void compileFSIN(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
   void compileFCOS(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileFYL2X(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileFTST(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);

	void compileFBLD(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileFILD(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileFIST(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
   void compileFISTP(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileFLD(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileFADD(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileFSUB(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileFMUL(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileFDIV(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileFXCH(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileFSTP(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileFBSTP(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileFSTSW(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
   void compileFNSTSW(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileFSTCW(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileFLDCW(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
	void compileFCOMIP(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
   void compileFCOMP(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
   void compileFLDPI(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
   void compileFPREM(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
   void compileFPATAN(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
   void compileFLDL2E(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
   void compileFLDLN2(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);
   void compileFFREE(TokenInfo& token, ProcedureInfo& info, SectionWriter* code);

   bool compileCommandA(TokenInfo& token, ProcedureInfo& info, SectionWriter& writer);
   bool compileCommandB(TokenInfo& token);
   bool compileCommandC(TokenInfo& token, ProcedureInfo& info, SectionWriter& writer, x86JumpHelper& helper);
   bool compileCommandD(TokenInfo& token, ProcedureInfo& info, SectionWriter& writer);
   bool compileCommandE(TokenInfo& token);
   bool compileCommandF(TokenInfo& token, ProcedureInfo& info, SectionWriter& writer);
   bool compileCommandG(TokenInfo& token);
   bool compileCommandH(TokenInfo& token);
   bool compileCommandI(TokenInfo& token, ProcedureInfo& info, SectionWriter& writer);
   bool compileCommandJ(TokenInfo& token, ProcedureInfo& info, SectionWriter& writer, x86JumpHelper& helper);
   bool compileCommandK(TokenInfo& token);
   bool compileCommandL(TokenInfo& token, ProcedureInfo& info, SectionWriter& writer, x86JumpHelper& helper);
   bool compileCommandM(TokenInfo& token, ProcedureInfo& info, SectionWriter& writer);
   bool compileCommandN(TokenInfo& token, ProcedureInfo& info, SectionWriter& writer);
   bool compileCommandO(TokenInfo& token, ProcedureInfo& info, SectionWriter& writer);
   bool compileCommandP(TokenInfo& token, ProcedureInfo& info, SectionWriter& writer);
   bool compileCommandQ(TokenInfo& token);
   bool compileCommandR(TokenInfo& token, ProcedureInfo& info, SectionWriter& writer);
   bool compileCommandS(TokenInfo& token, ProcedureInfo& info, SectionWriter& writer);
   bool compileCommandT(TokenInfo& token, ProcedureInfo& info, SectionWriter& writer);
   bool compileCommandU(TokenInfo& token);
   bool compileCommandV(TokenInfo& token);
   bool compileCommandW(TokenInfo& token);
   bool compileCommandX(TokenInfo& token, ProcedureInfo& info, SectionWriter& writer);
   bool compileCommandY(TokenInfo& token);
   bool compileCommandZ(TokenInfo& token);
   bool compileCommand(TokenInfo& token, ProcedureInfo& info, SectionWriter& writer, x86JumpHelper& helper);

   virtual void compileProcedure(TokenInfo& token, _Module* binary, bool aligned);
   virtual void compileStructure(TokenInfo& token, _Module* binary);

public:
	virtual void compile(TextReader* reader, const TCHAR* outputPath);

	x86Assembler()
	{
	   loadDefaultConstants();
	}
	virtual ~x86Assembler() {}
};

} // _ELENA_

#endif // x86assemblerH
