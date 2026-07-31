//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Assembler Compiler
//
//		This file contains the implementation of ELENA x86Compiler
//		classes.
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#include "elena.h"
//---------------------------------------------------------------------------
#include "x86assembler.h"
#include "module.h"

#include <float.h>

using namespace _ELENA_;

#define _EL_NIL             _T("'nil")
#define _SYSTEM             _T("'system")

#define _EL_EMPTYOBJECT     _T("'el_emptyobject")
#define _EL_EMPTYOBJECT_AGN _T("'gc_empty_object_aligned")

#define _EL_MINIMAL_SIZE    _T("'gc_heap_minimal")

//#define _GC_PAGE_SIZE	    _T("'gc_pagesize")
#define _GC_PAGE_MASK       _T("'gc_page_mask")
#define _GC_PAGE_LOG        _T("'gc_page_log")
#define _GC_COLLECTED       _T("'gc_collected")
#define _GC_COLLECT_INV     _T("'gc_collectedInv")
#define _GC_BINARY          _T("'gc_binary")

#define _GC_CURRENT_FRAME   _T("'gs_current_frame")
#define _GC_HEAP_START      _T("'gc_heap_start")
#define _GC_YG_HEAP         _T("'gc_yg_heap")
#define _GC_MG_HEAP         _T("'gc_mg_heap")
#define _GC_OG_HEAP         _T("'gc_og_heap")
#define _GC_STATICSIZE      _T("'gc_static_size")
#define _GC_HEAP_END        _T("'gc_heap_end")
#define _GC_MGPTR           _T("'gc_mgptr2")
#define _GC_MGPTR_END       _T("'gc_mgptr2_end")
#define _GC_OGPTR           _T("'gc_ogptr2")
#define _GC_OGPTR_END       _T("'gc_ogptr2_end")
#define _GC_FLAG            _T("'gc_flag")
#define _GC_HEAPSIZE        _T("'gc_heapsize")

#define ARGUMENT1           _T("__arg1")
#define ARGUMENT2           _T("__arg2")
#define ARGUMENTOBJ1        _T("__arg1obj")
#define ARGUMENTOBJ2        _T("__arg2obj")
#define ARGUMENTVMT1        _T("__arg1vmt")
#define ARGUMENTVMT2        _T("__arg2vmt")
#define ARGUMENTFUN1        _T("__arg1fun")
#define ARGUMENTFUN2        _T("__arg2fun")

#define _STRUCTURE          _T("'structure")

int logth(int n) // logarithm
{
   double f = n;

   return _logb(f);
}

void x86Assembler :: loadDefaultConstants()
{
   constants.add(_EL_EMPTYOBJECT, elEmptyObject);
   constants.add(_EL_EMPTYOBJECT_AGN, elEmptyObject + gcPageSize - 1);
   constants.add(_EL_MINIMAL_SIZE, gcPageSize * 0x10);
//   constants.add(_GC_PAGE_SIZE, gcPageSize);
   constants.add(_GC_PAGE_MASK, ~(gcPageSize - 1));
   constants.add(_GC_PAGE_LOG, logth(gcPageSize));
   constants.add(_GC_COLLECTED, gcCollected);
   constants.add(_GC_COLLECT_INV, ~gcCollected);
   constants.add(_GC_BINARY, gcBinary);
}

void x86Assembler :: readParameterList(TokenInfo& token, ProcedureInfo& info)
{
   while (true) {
      token.read();

      if (token.terminal.type==dfaIdentifier) {
         info.parameters.add(token.value, info.parameters.Count());
		   token.read();

		   if (token.check(_T(")"))) {
             break;
		   }
		   else if (!token.check(_T(",")))
             token.raiseErr(_T("Comma expected (%d)\n"));
	   }
	   else token.raiseErr(_T("Invalid parameter list syntax (%d)\n"));
   }
}

int x86Assembler :: readStReg(TokenInfo& token)
{
	token.read(_T("("), _T("'(' expected (%d)\n"));
	int index = token.readInteger();
	token.read(_T(")"), _T("')' expected (%d)\n"));

	return index;
}

bool x86Assembler :: readOffset(TokenInfo& token, ProcedureInfo& info/*, _Module* binary*/, const TCHAR* err, Operand& operand)
{
	Operand disp;
	token.read();
	if (token.check(_T("+"))) {
		token.read();
		disp = defineOperand(token, info/*, binary*/, err);
	}
	else if (token.check(_T("-"))) {
		token.read();
		disp = defineOperand(token, info/*, binary*/, err);
		disp.offset = -disp.offset;
	}
   else if(token.value[0]=='-' && (token.terminal.type==dfaInteger || token.terminal.type==dfaHexInteger)) {
		token.getInteger(disp.offset);
		setOffsetSize(disp);
	}
	else if (operand.ebpReg && operand.type == x86Helper::otDisp32) {
      operand.type = (OperandType)(operand.type | x86Helper::otM32disp8);
	   operand.offset = 0;
	   return false;
	}
	else return false;

	if (disp.reference==0) {
		operand.offset += disp.offset;
	}
	else if (operand.reference==0) {
		operand.offset += disp.offset;
		operand.reference = disp.reference;
	}
	else token.raiseErr(err);

	// !! to deal with the conflict bitween [ebp] and disp32
	if ((operand.type==x86Helper::otDisp32 || operand.type==x86Helper::otDD || operand.type==x86Helper::otDB)
      && !operand.ebpReg && (disp.type == x86Helper::otDD || disp.type == x86Helper::otDB))
   {
		return true;
	}
	else if (test(operand.type, x86Helper::otM32)) {
		if (disp.type==x86Helper::otDB) {
			operand.type = (OperandType)(operand.type | x86Helper::otM32disp8);
			return true;
		}
		else if (disp.type==x86Helper::otDD) {
         operand.type = (OperandType)(operand.type | x86Helper::otM32disp32);
			return true;
		}
		else if (test(disp.type, x86Helper::otR32)) {
			if (!test(disp.type, x86Helper::otSIB)) {
				int sibcode = ((char)operand.type + ((char)disp.type << 3)) << 26;

				operand.type = (OperandType)((operand.type & 0xFFFFFFF8) | x86Helper::otSIB | sibcode);
				return true;
			}
		}
	}
	else if (test(operand.type, x86Helper::otM8)) {
		if (disp.type==x86Helper::otDB) {
			operand.type = (OperandType)(operand.type | x86Helper::otM8disp8);
			return true;
		}
		else if (test(disp.type, x86Helper::otR32)) {
			if (!test(disp.type, x86Helper::otSIB)) {
				int sibcode = ((char)operand.type + ((char)disp.type << 3)) << 26;

				operand.type = (OperandType)((operand.type & 0xFFFFFFF8) | x86Helper::otSIB | sibcode);
				return true;
			}
		}
	}
	else if (test(operand.type, x86Helper::otM16)) {
		if (disp.type==x86Helper::otDB) {
			operand.type = (OperandType)(operand.type | x86Helper::otM16disp8);
			return true;
		}
	}
	token.raiseErr(err);
	return false;
}

x86Assembler::Operand x86Assembler :: defineRegister(TokenInfo& token)
{
	if (token.check(_T("eax"))) {
		return x86Helper::otEAX;
	}
	else if (token.check(_T("ecx"))) {
		return x86Helper::otECX;
	}
	else if (token.check(_T("ebx"))) {
		return x86Helper::otEBX;
	}
	else if (token.check(_T("esi"))) {
		return x86Helper::otESI;
	}
	else if (token.check(_T("edi"))) {
		return x86Helper::otEDI;
	}
	else if (token.check(_T("ebp"))) {
		return Operand(x86Helper::otEBP);
	}
	else if (token.check(_T("edx"))) {
		return Operand(x86Helper::otEDX);
	}
	else if (token.check(_T("esp"))) {
		return Operand(x86Helper::otESP);
	}
	else if (token.check(_T("al"))) {
		return Operand(x86Helper::otAL);
	}
	else if (token.check(_T("bl"))) {
		return Operand(x86Helper::otBL);
	}
	else if (token.check(_T("cl"))) {
		return Operand(x86Helper::otCL);
	}
	else if (token.check(_T("dl"))) {
		return Operand(x86Helper::otDL);
	}
	else if (token.check(_T("dh"))) {
		return Operand(x86Helper::otDH);
	}
	else if (token.check(_T("ah"))) {
		return Operand(x86Helper::otAH);
	}
	else if (token.check(_T("bh"))) {
		return Operand(x86Helper::otBH);
	}
	else if (token.check(_T("ax"))) {
		return Operand(x86Helper::otAX);
	}
	else if (token.check(_T("bx"))) {
		return Operand(x86Helper::otBX);
	}
	else if (token.check(_T("cx"))) {
		return Operand(x86Helper::otCX);
	}
	else if (token.check(_T("dx"))) {
		return Operand(x86Helper::otDX);
	}
   else return Operand(x86Helper::otUnknown);
}

x86Assembler::Operand x86Assembler :: defineOperand(TokenInfo& token, ProcedureInfo& info, const TCHAR* err)
{
	Operand operand = defineRegister(token);
   if (operand.type == x86Helper::otUnknown) {
		if (token.check(_T("("))) {
			token.read();
			operand = defineOperand(token, info/*, binary*/, err);
			readOffset(token, info/*, binary*/, err, operand);
			token.read(_T(")"), err);
		}
		else if (token.getInteger(operand.offset)) {
			setOffsetSize(operand);
		}
		else if (token.terminal.line[0]=='#') {
			operand.type = x86Helper::otDD;
         operand.reference = info.binary->mapMessage(token.value + 1);
		}
		else if (token.check(_T("'statroots"))) {
			operand.type = x86Helper::otDD;
         operand.reference = info.binary->mapReference(GC_ROOT) | mskNativeStaticRef;
		}
		else if (token.check(_T("'windproc"))) {
           operand.type = x86Helper::otDD;
           operand.reference = info.binary->mapReference(WIND32PROC) | mskNativeCodeRef;
		}
		else if (token.check(ARGUMENT1)) {
			operand.type = x86Helper::otDD;
         operand.reference = -1;
		}
		else if (token.check(ARGUMENT2)) {
			operand.type = x86Helper::otDD;
         operand.reference = -2;
		}
      else if (token.check(ARGUMENTOBJ1)) {
			operand.type = x86Helper::otDD;
         operand.reference = -3;
         operand.offset = elEmptyObject;
		}
		else if (token.check(ARGUMENTOBJ2)) {
			operand.type = x86Helper::otDD;
         operand.reference = -4;
         operand.offset = elEmptyObject;
		}
      else if (token.check(ARGUMENTVMT1)) {
			operand.type = x86Helper::otDD;
         operand.reference = -5;
         operand.offset = elVMTOffset;
		}
		else if (token.check(ARGUMENTVMT2)) {
			operand.type = x86Helper::otDD;
         operand.reference = -6;
         operand.offset = elVMTOffset;
		}
		else if (token.check(_GC_HEAPSIZE)) {
			operand.type = x86Helper::otDD;
         operand.reference = lnGCSize | mskLinkerConstant;
		}
		else if (token.check(_GC_CURRENT_FRAME)) {
		   operand.type = x86Helper::otDD;
         operand.reference = info.binary->mapReference(GC_TABLE) | mskNativeDataRef;
		   operand.offset = 0x0;
		}
		else if (token.check(_GC_HEAP_START)) {
		   operand.type = x86Helper::otDD;
		   operand.reference = info.binary->mapReference(GC_TABLE) | mskNativeDataRef;
		   operand.offset = 0x4;
		}
		else if (token.check(_GC_YG_HEAP)) {
		   operand.type = x86Helper::otDD;
		   operand.reference = info.binary->mapReference(GC_TABLE) | mskNativeDataRef;
		   operand.offset = 0x8;
		}
		else if (token.check(_GC_MG_HEAP)) {
		   operand.type = x86Helper::otDD;
		   operand.reference = info.binary->mapReference(GC_TABLE) | mskNativeDataRef;
		   operand.offset = 0xC;
		}
		else if (token.check(_GC_OG_HEAP)) {
		   operand.type = x86Helper::otDD;
		   operand.reference = info.binary->mapReference(GC_TABLE) | mskNativeDataRef;
		   operand.offset = 0x10;
		}
		else if (token.check(_GC_STATICSIZE)) {
           operand.type = x86Helper::otDD;
           operand.reference = info.binary->mapReference(GC_TABLE) | mskNativeDataRef;
           operand.offset = 0x14;
		}
		else if (token.check(_GC_HEAP_END)) {
		   operand.type = x86Helper::otDD;
		   operand.reference = info.binary->mapReference(GC_TABLE) | mskNativeDataRef;
		   operand.offset = 0x18;
		}
		else if (token.check(_GC_MGPTR)) {
		   operand.type = x86Helper::otDD;
		   operand.reference = info.binary->mapReference(GC_TABLE) | mskNativeDataRef;
		   operand.offset = 0x1C;
		}
		else if (token.check(_GC_MGPTR_END)) {
		   operand.type = x86Helper::otDD;
		   operand.reference = info.binary->mapReference(GC_TABLE) | mskNativeDataRef;
		   operand.offset = 0x20;
		}
		else if (token.check(_GC_OGPTR)) {
		   operand.type = x86Helper::otDD;
		   operand.reference = info.binary->mapReference(GC_TABLE) | mskNativeDataRef;
		   operand.offset = 0x24;
		}
		else if (token.check(_GC_OGPTR_END)) {
		   operand.type = x86Helper::otDD;
		   operand.reference = info.binary->mapReference(GC_TABLE) | mskNativeDataRef;
		   operand.offset = 0x28;
		}
		else if (token.check(_GC_FLAG)) {
		   operand.type = x86Helper::otDD;
		   operand.reference = info.binary->mapReference(GC_TABLE) | mskNativeDataRef;
		   operand.offset = 0x2C;
      }
      else if (token.check(_EL_NIL)) {
         operand.type = x86Helper::otDD;
         operand.reference = info.binary->mapReference(NIL_CLASS) | mskConstantRef;
         operand.offset = elEmptyObject;
      }
      else if (token.check(_STRUCTURE)) {
         token.read(_T(":"), err);
         token.read();
         String structRef(token.terminal.line + 1, token.terminal.length-2);

         operand.type = x86Helper::otDD;
         operand.reference = info.binary->mapReference(structRef) | mskNativeDataRef;
      }
      else if (token.check(_SYSTEM)) {
        operand.type = x86Helper::otDD;
        operand.reference = info.binary->mapReference(GUI_CLASS) | mskStaticConstRef;
        operand.offset = 0;
      }
      else if (token.terminal.type==dfaQuote) {
         operand.type = x86Helper::otDD;
         operand.offset = elVMTOffset;
         String classRef(token.terminal.line + 1, token.terminal.length-2);
         operand.reference = info.binary->mapReference(classRef) | mskVMTRef;
      }
      else if (info.parameters.exist(token.value)) {
         operand.type = x86Helper::addPrefix(x86Helper::otEBP, x86Helper::otM32disp32);
         operand.offset = (info.parameters.Count() - info.parameters.get(token.value))*4;
      }
      else if (constants.exist(token.value)) {
         operand.offset = constants.get(token.value);
         setOffsetSize(operand);
      }
      else token.raiseErr(err);
   }
   return operand;
}

x86Assembler::Operand x86Assembler :: readOperand(TokenInfo& token, ProcedureInfo& info, const TCHAR* err, OperandType prefix)
{
	Operand operand = defineOperand(token, info, err);

   operand.type = x86Helper::addPrefix(operand.type, prefix);

	if (readOffset(token, info, err, operand)) {
		if (readOffset(token, info, err, operand))
			token.read();
	}

	return operand;
}

x86Assembler::Operand x86Assembler :: readPtrOperand(TokenInfo& token, ProcedureInfo& info, const TCHAR* err, OperandType prefix)
{
	token.read(_T("ptr"), _T("'ptr' expected (%d)\n"));

	token.read();
	if (token.check(_T("["))) {
	   token.read();
	   Operand operand = readOperand(token, info, err, prefix);
	   if(!token.check(_T("]"))) {
          token.raiseErr(_T("']' expected (%d)\n"));
	   }
	   else token.read();

      return operand;
	}
	else {
      Operand operand = defineOperand(token, info, err);
      if (operand.type == x86Helper::otDD && operand.reference == 0 || operand.type==x86Helper::otDB) {
         if (prefix==x86Helper::otM16) {
            operand.type = x86Helper::otDW;
         }
         else if (prefix==x86Helper::otM8) {
            operand.type = x86Helper::otDB;
         }
      }
	   else token.raiseErr(_T("'[' expected (%d)\n"));

      token.read();

      return operand;
	}
}

x86Assembler::Operand x86Assembler :: compileOperand(TokenInfo& token, ProcedureInfo& info/*, _Module* binary*/, const TCHAR* err)
{
	Operand	    operand;

	token.read();
	if (token.check(_T("["))) {
		token.read();
      operand = readOperand(token, info, err, x86Helper::otM32);

	   if(!token.check(_T("]"))) {
          token.raiseErr(_T("']' expected (%d)\n"));
	   }
	   else token.read();
	}
	else if (token.check(_T("dword"))) {
		operand = readPtrOperand(token, info, err, x86Helper::otM32);
	}
	else if (token.check(_T("word"))) {
		operand = readPtrOperand(token, info, err, x86Helper::otM16);
	}
	else if (token.check(_T("byte"))) {
		operand = readPtrOperand(token, info, err, x86Helper::otM8);
	}
   else if (token.check(_T("fs"))) {
      token.read(_T(":"), _T("Column is expected"));
      operand = compileOperand(token, info, err);

      if (operand.prefix != x86Helper::spNone) {
         token.raiseErr(err);
      }
      else operand.prefix = x86Helper::spFS;
   }
	else operand = readOperand(token, info, err, x86Helper::otNone);

	return operand;
}

void x86Assembler :: compileMOV(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	Operand sour = compileOperand(token, info, _T("Invalid source operand (%d)\n"));

	checkComma(token);

	Operand dest = compileOperand(token, info, _T("Invalid destination operand (%d)\n"));

   // write segment prefix
   if (sour.prefix != x86Helper::spNone) {
      if (dest.prefix != x86Helper::spNone)
         token.raiseErr(_T("Invalid command (%d)"));

      code->writeByte(sour.prefix);
   }
   else if (dest.prefix != x86Helper::spNone) {
      if (sour.prefix != x86Helper::spNone)
         token.raiseErr(_T("Invalid command (%d)"));

      code->writeByte(dest.prefix);
   }

	if (test(sour.type, x86Helper::otPtr16)) {
		sour.type = x86Helper::overrideOperand16(sour.type);
		if (test(dest.type, x86Helper::otPtr16)) {
			dest.type = x86Helper::overrideOperand16(dest.type);
		}
		else if (dest.type==x86Helper::otDD) {
			dest.type = x86Helper::otDW;
		}
		code->writeByte(0x66);
	}

	if (sour.type == x86Helper::otEAX && dest.type == x86Helper::otDisp32) {
		code->writeByte(0xA1);
		code->writeRef(dest.reference, dest.offset);
	}
	else if (sour.type == x86Helper::otDisp32 && dest.type == x86Helper::otEAX) {
		code->writeByte(0xA3);
		code->writeRef(sour.reference, sour.offset);
	}
	else if (test(sour.type, x86Helper::otR32) && (test(dest.type, x86Helper::otR32)||test(dest.type, x86Helper::otM32))) {
		code->writeByte(0x8B);
		x86Helper::writeModRM(code, sour, dest);
	}
	else if (test(sour.type, x86Helper::otM32) && test(dest.type, x86Helper::otR32)) {
		code->writeByte(0x89);
		x86Helper::writeModRM(code, dest, sour);
	}
	else if (test(sour.type, x86Helper::otR32) && (dest.type==x86Helper::otDD || dest.type==x86Helper::otDB)) {
		dest.type = x86Helper::otDD;
		code->writeByte(0xB8 + (char)sour.type);
		x86Helper::writeImm(code, dest);
	}
	else if ((test(sour.type, x86Helper::otR32) || test(sour.type, x86Helper::otM32))
      && (dest.type==x86Helper::otDD || dest.type==x86Helper::otDB))
   {
		dest.type = x86Helper::otDD;
		code->writeByte(0xC7);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 0), sour);
		x86Helper::writeImm(code, dest);
	}
	else if (test(sour.type, x86Helper::otR8) && dest.type==x86Helper::otDB) {
		code->writeByte(0xB0 + (char)sour.type);
		code->writeByte(dest.offset);
	}
	else if ((test(sour.type, x86Helper::otR8)||test(sour.type, x86Helper::otM8))&& test(dest.type, x86Helper::otR8)) {
		code->writeByte(0x88);
		x86Helper::writeModRM(code, dest, sour);
	}
	else if (test(sour.type, x86Helper::otR8) && (test(dest.type, x86Helper::otR8)||test(dest.type, x86Helper::otM8))) {
		code->writeByte(0x8A);
		x86Helper::writeModRM(code, sour, dest);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileCMP(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	Operand sour = compileOperand(token, info, _T("Invalid source operand (%d)\n"));

	checkComma(token);

	Operand dest = compileOperand(token, info, _T("Invalid destination operand (%d)\n"));

	if (test(sour.type, x86Helper::otPtr16)) {
      sour.type = x86Helper::overrideOperand16(sour.type);
		if (test(dest.type, x86Helper::otPtr16)) {
			dest.type = x86Helper::overrideOperand16(dest.type);
		}
		else if (dest.type==x86Helper::otDD) {
			dest.type = x86Helper::otDW;
		}
		code->writeByte(0x66);
	}

	if (test(sour.type, x86Helper::otR32) && (test(dest.type, x86Helper::otR32) || test(dest.type, x86Helper::otM32))) {
		code->writeByte(0x3B);
		x86Helper::writeModRM(code, sour, dest);
	}
	else if ((test(sour.type, x86Helper::otR32) || test(sour.type, x86Helper::otM32)) && test(dest.type, x86Helper::otR32)) {
		code->writeByte(0x39);
		x86Helper::writeModRM(code, dest, sour);
	}
	else if ((test(sour.type, x86Helper::otR32)||test(sour.type, x86Helper::otM32)) && dest.type==x86Helper::otDB) {
		code->writeByte(0x83);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 7), sour);
		code->writeByte(dest.offset);
	}
	else if ((test(sour.type, x86Helper::otR32)||test(sour.type, x86Helper::otM32)) && dest.type==x86Helper::otDD) {
		code->writeByte(0x81);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 7), sour);
		x86Helper::writeImm(code, dest);
	}
	else if (sour.type==x86Helper::otAL && dest.type==x86Helper::otDB) {
		code->writeByte(0x3C);
		code->writeByte(dest.offset);
	}
	else if (test(sour.type, x86Helper::otR8) && test(dest.type, x86Helper::otM8)) {
		code->writeByte(0x3A);
		x86Helper::writeModRM(code, sour, dest);
	}
	else if ((test(sour.type, x86Helper::otR8) || test(sour.type, x86Helper::otM8)) && dest.type == x86Helper::otDB) {
		code->writeByte(0x80);
		x86Helper::writeModRM(code, Operand(x86Helper::otR8 + 7), sour);
		code->writeByte(dest.offset);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileADD(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	Operand sour = compileOperand(token, info, _T("Invalid source operand (%d)\n"));

	checkComma(token);

	Operand dest = compileOperand(token, info, _T("Invalid destination operand (%d)\n"));

	if (sour.type==x86Helper::otEAX && dest.type == x86Helper::otDD) {
		code->writeByte(0x05);
		x86Helper::writeImm(code, dest);
	}
	else if (test(sour.type, x86Helper::otR32) && (test(dest.type, x86Helper::otR32)||test(dest.type, x86Helper::otM32))) {
		code->writeByte(0x03);
		x86Helper::writeModRM(code, sour, dest);
	}
	else if ((test(sour.type, x86Helper::otR32)||test(sour.type, x86Helper::otM32)) && test(dest.type, x86Helper::otR32)) {
		code->writeByte(0x01);
		x86Helper::writeModRM(code, dest, sour);
	}
	else if ((test(sour.type, x86Helper::otR32)||test(sour.type, x86Helper::otM32)) && dest.type==x86Helper::otDB) {
		code->writeByte(0x83);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 0), sour);
		code->writeByte(dest.offset);
	}
	else if ((test(sour.type, x86Helper::otR8)||test(sour.type, x86Helper::otM8)) && dest.type==x86Helper::otDB) {
		code->writeByte(0x80);
		x86Helper::writeModRM(code, Operand(x86Helper::otR8 + 0), sour);
		x86Helper::writeImm(code, dest);
	}
	else if (test(sour.type, x86Helper::otR8)&&(test(dest.type, x86Helper::otR8)||test(dest.type, x86Helper::otM8))) {
		code->writeByte(0x02);
		x86Helper::writeModRM(code, sour, dest);
	}
	else if ((test(sour.type, x86Helper::otR32)||test(sour.type, x86Helper::otM32)) && dest.type==x86Helper::otDD) {
		code->writeByte(0x81);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 0), sour);
		code->writeDWord(dest.offset);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileADC(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	Operand sour = compileOperand(token, info, _T("Invalid source operand (%d)\n"));

	checkComma(token);

	Operand dest = compileOperand(token, info, _T("Invalid destination operand (%d)\n"));

	if (sour.type== x86Helper::otEAX && dest.type == x86Helper::otDD) {
		code->writeByte(0x15);
		x86Helper::writeImm(code, dest);
	}
	else if (test(sour.type, x86Helper::otR32) && (test(dest.type, x86Helper::otR32)||test(dest.type, x86Helper::otM32))) {
		code->writeByte(0x13);
		x86Helper::writeModRM(code, sour, dest);
	}
	else if ((test(sour.type, x86Helper::otR32)||test(sour.type, x86Helper::otM32)) && test(dest.type, x86Helper::otR32)) {
		code->writeByte(0x11);
		x86Helper::writeModRM(code, dest, sour);
	}
	else if ((test(sour.type, x86Helper::otR32)||test(sour.type, x86Helper::otM32)) && dest.type==x86Helper::otDB) {
		code->writeByte(0x83);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 2), sour);
		code->writeByte(dest.offset);
	}
	else if ((test(sour.type, x86Helper::otR8)||test(sour.type, x86Helper::otM8)) && dest.type==x86Helper::otDB) {
		code->writeByte(0x80);
		x86Helper::writeModRM(code, Operand(x86Helper::otR8 + 2), sour);
		x86Helper::writeImm(code, dest);
	}
	else if (test(sour.type, x86Helper::otR8)&&(test(dest.type, x86Helper::otR8)||test(dest.type, x86Helper::otM8))) {
		code->writeByte(0x12);
		x86Helper::writeModRM(code, sour, dest);
	}
	else if ((test(sour.type, x86Helper::otR32)||test(sour.type, x86Helper::otM32)) && dest.type==x86Helper::otDD) {
		code->writeByte(0x81);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 2), sour);
		code->writeDWord(dest.offset);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileAND(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	bool overridden = false;

	Operand sour = compileOperand(token, info, _T("Invalid source operand (%d)\n"));

	checkComma(token);

	Operand dest = compileOperand(token, info, _T("Invalid destination operand (%d)\n"));
	if (test(sour.type, x86Helper::otR16) && dest.type==x86Helper::otDD) {
		sour.type = x86Helper::overrideOperand16(sour.type);
		code->writeByte(0x66);
		overridden = true;
	}

	if (test(sour.type, x86Helper::otR32) && test(dest.type, x86Helper::otDB)) {
		code->writeByte(0x83);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 4), sour);
		code->writeByte(dest.offset);
	}
	else if (test(sour.type, x86Helper::otR32) && (test(dest.type, x86Helper::otR32) || test(dest.type, x86Helper::otM32))) {
		code->writeByte(0x23);
		x86Helper::writeModRM(code, sour, dest);
	}
	else if ((test(sour.type, x86Helper::otR32)||test(sour.type, x86Helper::otM32)) && test(dest.type, x86Helper::otR32)) {
		code->writeByte(0x21);
		x86Helper::writeModRM(code, dest, sour);
	}
	else if ((test(sour.type, x86Helper::otR32)||test(sour.type, x86Helper::otM32)) && dest.type==x86Helper::otDD) {
		code->writeByte(0x81);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 4), sour);
		if (overridden) {
			code->writeWord(dest.offset);
		}
		else x86Helper::writeImm(code, dest);
	}
   else if (test(sour.type, x86Helper::otR8) && test(dest.type, x86Helper::otDB)) {
		code->writeByte(0x80);
		x86Helper::writeModRM(code, Operand(x86Helper::otR8 + 4), sour);
      x86Helper::writeImm(code, dest);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileXOR(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	Operand sour = compileOperand(token, info, _T("Invalid source operand (%d)\n"));

	checkComma(token);

	Operand dest = compileOperand(token, info, _T("Invalid destination operand (%d)\n"));

	if (test(sour.type, x86Helper::otR32) && test(dest.type, x86Helper::otR32)) {
		code->writeByte(0x33);
		x86Helper::writeModRM(code, sour, dest);
	}
	else if (test(sour.type, x86Helper::otR32) && test(dest.type, x86Helper::otDB)) {
		code->writeByte(0x83);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 6), sour);
		code->writeByte(dest.offset);
	}
	else if ((test(sour.type, x86Helper::otR32)||test(sour.type, x86Helper::otM32)) && dest.type==x86Helper::otDD) {
		code->writeByte(0x81);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 6), sour);
		x86Helper::writeImm(code, dest);
	}
	else if ((test(sour.type, x86Helper::otR32)||test(sour.type, x86Helper::otM32)) && test(dest.type, x86Helper::otR32)) {
		code->writeByte(0x31);
		x86Helper::writeModRM(code, dest, sour);
		x86Helper::writeImm(code, dest);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileOR(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	bool overridden = false;

	Operand sour = compileOperand(token, info, _T("Invalid source operand (%d)\n"));

	checkComma(token);

	Operand dest = compileOperand(token, info, _T("Invalid destination operand (%d)\n"));

	if (test(sour.type, x86Helper::otR16) && dest.type==x86Helper::otDD) {
		sour.type = x86Helper::overrideOperand16(sour.type);
		code->writeByte(0x66);
		overridden = true;
	}

	if (sour.type== x86Helper::otEAX && dest.type==x86Helper::otDD) {
		code->writeByte(0x0D);
		if (overridden) {
			code->writeWord(dest.offset);
		}
		else x86Helper::writeImm(code, dest);
	}
	else if ((test(sour.type, x86Helper::otR32)||test(sour.type, x86Helper::otM32)) && test(dest.type, x86Helper::otDB)) {
		code->writeByte(0x83);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 1), sour);
		code->writeByte(dest.offset);
	}
	else if ((test(sour.type, x86Helper::otR8)||test(sour.type, x86Helper::otM8)) && test(dest.type, x86Helper::otR8)) {
		code->writeByte(0x08);
		x86Helper::writeModRM(code, dest, sour);
	}
	else if ((test(sour.type, x86Helper::otR32)||test(sour.type, x86Helper::otM32)) && dest.type==x86Helper::otDD) {
		code->writeByte(0x81);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 1), sour);
		x86Helper::writeImm(code, dest);
	}
	else if ((test(sour.type, x86Helper::otR8)||test(sour.type, x86Helper::otM8)) && dest.type==x86Helper::otDB) {
		code->writeByte(0x80);
		x86Helper::writeModRM(code, Operand(x86Helper::otR8 + 1), sour);
		x86Helper::writeImm(code, dest);
	}
	else if ((test(sour.type, x86Helper::otR32)||test(sour.type, x86Helper::otM32)) && test(dest.type, x86Helper::otR32)) {
		code->writeByte(0x09);
		x86Helper::writeModRM(code, dest, sour);
	}
	else if (test(sour.type, x86Helper::otR16) && test(dest.type, x86Helper::otR16)) {
		code->writeByte(0x66);
		code->writeByte(0x09);
		x86Helper::writeModRM(code, dest, sour);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileTEST(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	Operand sour = compileOperand(token, info, _T("Invalid source operand (%d)\n"));

	checkComma(token);

	Operand dest = compileOperand(token, info, _T("Invalid destination operand (%d)\n"));

	if (sour.type == x86Helper::otEAX && dest.type == x86Helper::otDD) {
		code->writeByte(0xA9);
		x86Helper::writeImm(code, dest);
	}
	else if ((test(sour.type, x86Helper::otR32)||test(sour.type, x86Helper::otM32)) && test(dest.type, x86Helper::otR32)) {
		code->writeByte(0x85);
		x86Helper::writeModRM(code, dest, sour);
	}
	else if ((test(sour.type, x86Helper::otR32)||test(sour.type, x86Helper::otM32)) && dest.type==x86Helper::otDD) {
		code->writeByte(0xF7);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 0), sour);
		x86Helper::writeImm(code, dest);
	}
	else if ((test(sour.type, x86Helper::otR32)||test(sour.type, x86Helper::otM32)) && dest.type==x86Helper::otDB) {
		code->writeByte(0xF7);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 0), sour);
		code->writeDWord(dest.offset);
	}
	else if ((test(sour.type, x86Helper::otR8)||test(sour.type, x86Helper::otM8)) && dest.type==x86Helper::otDB) {
		code->writeByte(0xF6);
		x86Helper::writeModRM(code, Operand(x86Helper::otR8 + 0), sour);
		x86Helper::writeImm(code, dest);
	}
	else if ((test(sour.type, x86Helper::otR8)||test(sour.type, x86Helper::otM8)) && test(dest.type, x86Helper::otR8)) {
		code->writeByte(0x84);
		x86Helper::writeModRM(code, dest, sour);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileSUB(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	Operand sour = compileOperand(token, info, _T("Invalid source operand (%d)\n"));

	checkComma(token);

	Operand dest = compileOperand(token, info, _T("Invalid destination operand (%d)\n"));

	if (test(sour.type, x86Helper::otR32) && (test(dest.type, x86Helper::otR32)||test(dest.type, x86Helper::otM32))) {
		code->writeByte(0x2B);
		x86Helper::writeModRM(code, sour, dest);
	}
   else if (sour.type==x86Helper::otAL && dest.type==x86Helper::otDB) {
		code->writeByte(0x2C);
		code->writeByte(dest.offset);
	}
	else if ((test(sour.type, x86Helper::otR32)||test(sour.type, x86Helper::otM32)) && test(dest.type, x86Helper::otR32)) {
		code->writeByte(0x29);
		x86Helper::writeModRM(code, dest, sour);
	}
	else if ((test(sour.type, x86Helper::otR32) ||test(sour.type, x86Helper::otM32)) && dest.type==x86Helper::otDB) {
		code->writeByte(0x83);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 5), sour);
		code->writeByte(dest.offset);
	}
	else if ((test(sour.type, x86Helper::otR8)||test(sour.type, x86Helper::otM8)) && dest.type==x86Helper::otDB) {
		code->writeByte(0x80);
		x86Helper::writeModRM(code, Operand(x86Helper::otR8 + 5), sour);
		x86Helper::writeImm(code, dest);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileSBB(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	Operand sour = compileOperand(token, info, _T("Invalid source operand (%d)\n"));

	checkComma(token);

	Operand dest = compileOperand(token, info, _T("Invalid destination operand (%d)\n"));

	if ((test(sour.type, x86Helper::otR32) ||test(sour.type, x86Helper::otM32)) && dest.type==x86Helper::otDB) {
		code->writeByte(0x83);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 3), sour);
		code->writeByte(dest.offset);
	}
	else if (test(sour.type, x86Helper::otR32) && (test(dest.type, x86Helper::otR32)||test(dest.type, x86Helper::otM32))) {
		code->writeByte(0x1B);
		x86Helper::writeModRM(code, sour, dest);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileLEA(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	Operand sour = compileOperand(token, info, _T("Invalid source operand (%d)\n"));

	checkComma(token);

	Operand dest = compileOperand(token, info, _T("Invalid destination operand (%d)\n"));

	if (test(sour.type, x86Helper::otR32) && test(dest.type, x86Helper::otM32)) {
		code->writeByte(0x8D);
		x86Helper::writeModRM(code, sour, dest);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileSHR(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	Operand sour = compileOperand(token, info, _T("Invalid source operand (%d)\n"));

	checkComma(token);

	Operand dest = compileOperand(token, info, _T("Invalid destination operand (%d)\n"));

	if (test(sour.type, x86Helper::otR32) && dest.type==x86Helper::otDB) {
		code->writeByte(0xC1);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 5), sour);
		code->writeByte(dest.offset);
	}
	else if (test(sour.type, x86Helper::otR32) && dest.type==x86Helper::otCL) {
		code->writeByte(0xD3);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 5), sour);
	}
	else if (test(sour.type, x86Helper::otR8) && dest.type==x86Helper::otDB && dest.offset == 1) {
		code->writeByte(0xD0);
		x86Helper::writeModRM(code, Operand(x86Helper::otR8 + 5), sour);
	}
	else if (test(sour.type, x86Helper::otR8) && dest.type==x86Helper::otDB) {
		code->writeByte(0xC0);
		x86Helper::writeModRM(code, Operand(x86Helper::otR8 + 5), sour);
		code->writeByte(dest.offset);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileSAR(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	Operand sour = compileOperand(token, info, _T("Invalid source operand (%d)\n"));

	checkComma(token);

	Operand dest = compileOperand(token, info, _T("Invalid destination operand (%d)\n"));

	if (test(sour.type, x86Helper::otR32) && dest.type==x86Helper::otDB) {
		code->writeByte(0xC1);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 7), sour);
		code->writeByte(dest.offset);
	}
	else if (test(sour.type, x86Helper::otR32) && dest.type==x86Helper::otCL) {
		code->writeByte(0xD3);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 7), sour);
	}
	else if (test(sour.type, x86Helper::otR8) && dest.type==x86Helper::otDB && dest.offset == 1) {
		code->writeByte(0xD0);
		x86Helper::writeModRM(code, Operand(x86Helper::otR8 + 7), sour);
	}
	else if (test(sour.type, x86Helper::otR8) && dest.type==x86Helper::otDB) {
		code->writeByte(0xC0);
		x86Helper::writeModRM(code, Operand(x86Helper::otR8 + 7), sour);
		code->writeByte(dest.offset);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileSHL(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	Operand sour = compileOperand(token, info, _T("Invalid source operand (%d)\n"));

	checkComma(token);

	Operand dest = compileOperand(token, info, _T("Invalid destination operand (%d)\n"));

	if (test(sour.type, x86Helper::otR32) && dest.type==x86Helper::otDB) {
		code->writeByte(0xC1);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 4), sour);
		code->writeByte(dest.offset);
	}
	else if (test(sour.type, x86Helper::otR32) && dest.type==x86Helper::otCL) {
		code->writeByte(0xD3);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 4), sour);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileSHLD(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	Operand sour = compileOperand(token, info, _T("Invalid source operand (%d)\n"));

	checkComma(token);

	Operand dest = compileOperand(token, info, _T("Invalid destination operand (%d)\n"));

	checkComma(token);

	Operand third = compileOperand(token, info, _T("Invalid third operand (%d)\n"));

	if (test(sour.type, x86Helper::otR32) && test(dest.type, x86Helper::otR32) && third.type==x86Helper::otDB) {
      code->writeByte(0x0F);
		code->writeByte(0xA4);
      x86Helper::writeImm(code, third);
	}
	else if (test(sour.type, x86Helper::otR32) && test(dest.type, x86Helper::otR32) && third.type==x86Helper::otCL) {
      code->writeByte(0x0F);
		code->writeByte(0xA5);
		x86Helper::writeModRM(code, dest, sour);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileSHRD(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	Operand sour = compileOperand(token, info, _T("Invalid source operand (%d)\n"));

	checkComma(token);

	Operand dest = compileOperand(token, info, _T("Invalid destination operand (%d)\n"));

	checkComma(token);

	Operand third = compileOperand(token, info, _T("Invalid third operand (%d)\n"));

	if (test(sour.type, x86Helper::otR32) && test(dest.type, x86Helper::otR32) && third.type==x86Helper::otDB) {
      code->writeByte(0x0F);
		code->writeByte(0xAC);
      x86Helper::writeImm(code, third);
	}
	else if (test(sour.type, x86Helper::otR32) && test(dest.type, x86Helper::otR32) && third.type==x86Helper::otCL) {
      code->writeByte(0x0F);
		code->writeByte(0xAD);
		x86Helper::writeModRM(code, dest, sour);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileROL(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	Operand sour = compileOperand(token, info, _T("Invalid source operand (%d)\n"));

	checkComma(token);

	Operand dest = compileOperand(token, info, _T("Invalid destination operand (%d)\n"));

	if (test(sour.type, x86Helper::otR8) && dest.type==x86Helper::otDB) {
		code->writeByte(0xC0);
		x86Helper::writeModRM(code, Operand(x86Helper::otR8 + 0), sour);
		code->writeByte(dest.offset);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileROR(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	Operand sour = compileOperand(token, info, _T("Invalid source operand (%d)\n"));

	checkComma(token);

	Operand dest = compileOperand(token, info, _T("Invalid destination operand (%d)\n"));

	if (test(sour.type, x86Helper::otR16)) {
		sour.type = (OperandType)x86Helper::overrideOperand16(sour.type);
		code->writeByte(0x66);
	}

	if (test(sour.type, x86Helper::otR32) && dest.type==x86Helper::otDB) {
		code->writeByte(0xC1);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 1), sour);
		code->writeByte(dest.offset);
	}
	else if (test(sour.type, x86Helper::otR8) && dest.type==x86Helper::otDB) {
		code->writeByte(0xC0);
		x86Helper::writeModRM(code, Operand(x86Helper::otR8 + 1), sour);
		code->writeByte(dest.offset);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileRCR(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	Operand sour = compileOperand(token, info, _T("Invalid source operand (%d)\n"));

	checkComma(token);

	Operand dest = compileOperand(token, info, _T("Invalid destination operand (%d)\n"));

	if (test(sour.type, x86Helper::otR32) && dest.type==x86Helper::otDB && dest.offset==1) {
		code->writeByte(0xD1);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 3), sour);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileXCHG(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	Operand sour = compileOperand(token, info, _T("Invalid source operand (%d)\n"));

	checkComma(token);

	Operand dest = compileOperand(token, info, _T("Invalid destination operand (%d)\n"));

	if (sour.type== x86Helper::otEAX && test(dest.type, x86Helper::otR32)) {
		code->writeByte(0x90 + (char)dest.type);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileRCL(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	Operand sour = compileOperand(token, info, _T("Invalid source operand (%d)\n"));

	checkComma(token);

	Operand dest = compileOperand(token, info, _T("Invalid destination operand (%d)\n"));

	if (test(sour.type, x86Helper::otR32) && dest.type==x86Helper::otDB && dest.offset==1) {
		code->writeByte(0xD1);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 2), sour);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileMOVZX(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	Operand sour = compileOperand(token, info, _T("Invalid source operand (%d)\n"));

	checkComma(token);

	Operand dest = compileOperand(token, info, _T("Invalid destination operand (%d)\n"));

	if (test(sour.type, x86Helper::otR32) && (test(dest.type, x86Helper::otR8)||test(dest.type, x86Helper::otM8))) {
		code->writeWord(0xB60F);
		x86Helper::writeModRM(code, Operand(x86Helper::otR8 + (char)sour.type), dest);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compilePUSH(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	Operand sour = compileOperand(token, info, _T("Invalid operand (%d)\n"));
	if (test(sour.type, x86Helper::otR32)) {
		code->writeByte(0x50 + (char)sour.type);
	}
	else if (test(sour.type, x86Helper::otM32)) {
		code->writeByte(0xFF);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 6), sour);
	}
	else if (test(sour.type, x86Helper::otM16)) {
		code->writeByte(0x66);
		code->writeByte(0xFF);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 6), sour);
	}
	else if (sour.type==x86Helper::otDB) {
		code->writeByte(0x6A);
		code->writeByte(sour.offset);
	}
	else if (sour.type==x86Helper::otDD) {
		code->writeByte(0x68);
		x86Helper::writeImm(code, sour);
	}
	else if (sour.type==x86Helper::otDW) {
      code->writeByte(0x66);
		code->writeByte(0x68);
		x86Helper::writeImm(code, sour);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compilePOP(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	Operand sour = compileOperand(token, info, _T("Invalid operand (%d)\n"));
	if (test(sour.type, x86Helper::otR32)) {
		code->writeByte(0x58 + (char)sour.type);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileMUL(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	Operand sour = compileOperand(token, info, _T("Invalid operand (%d)\n"));
	if (test(sour.type, x86Helper::otR32)||test(sour.type, x86Helper::otM32)) {
		code->writeByte(0xF7);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 4), sour);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileIMUL(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	Operand sour = compileOperand(token, info, _T("Invalid source operand (%d)\n"));

	checkComma(token);

	Operand dest = compileOperand(token, info, _T("Invalid destination operand (%d)\n"));

	if (test(sour.type, x86Helper::otR32) && dest.type==x86Helper::otDB) {
		code->writeByte(0x6B);
		x86Helper::writeModRM(code, sour, sour);
		code->writeByte(dest.offset);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileIDIV(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	Operand sour = compileOperand(token, info, _T("Invalid operand (%d)\n"));
	if (test(sour.type, x86Helper::otR32)||test(sour.type, x86Helper::otM32)) {
		code->writeByte(0xF7);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 7), sour);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileDIV(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	Operand sour = compileOperand(token, info, _T("Invalid operand (%d)\n"));
	if (test(sour.type, x86Helper::otR32)||test(sour.type, x86Helper::otM32)) {
		code->writeByte(0xF7);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 6), sour);
	}
	else if (test(sour.type, x86Helper::otR8)||test(sour.type, x86Helper::otM8)) {
		code->writeByte(0xF6);
		x86Helper::writeModRM(code, Operand(x86Helper::otR8 + 6), sour);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileDEC(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	Operand sour = compileOperand(token, info, _T("Invalid operand (%d)\n"));
	if (test(sour.type, x86Helper::otR32)) {
		code->writeByte(0x48 + (char)sour.type);
	}
	else if (test(sour.type, x86Helper::otR8)||test(sour.type, x86Helper::otM8)) {
		code->writeByte(0xFE);
		x86Helper::writeModRM(code, Operand(x86Helper::otR8 + 1), sour);
	}
	else if (test(sour.type, x86Helper::otM32)) {
		code->writeByte(0xFF);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 1), sour);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileINC(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	Operand sour = compileOperand(token, info, _T("Invalid operand (%d)\n"));
	if (test(sour.type, x86Helper::otR32)) {
		code->writeByte(0x40 + (char)sour.type);
	}
	else if (test(sour.type, x86Helper::otR8)||test(sour.type, x86Helper::otM8)) {
		code->writeByte(0xFE);
		x86Helper::writeModRM(code, Operand(x86Helper::otR8 + 0), sour);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileNEG(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	Operand sour = compileOperand(token, info, _T("Invalid operand (%d)\n"));
	if (test(sour.type, x86Helper::otR32)) {
		code->writeByte(0xF7);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 3), sour);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileNOT(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	Operand sour = compileOperand(token, info, _T("Invalid operand (%d)\n"));
	if (test(sour.type, x86Helper::otR32)) {
		code->writeByte(0xF7);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 2), sour);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileRET(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeByte(0xC3);

	token.read();
}

void x86Assembler :: compileCDQ(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeByte(0x99);

	token.read();
}

void x86Assembler :: compileSTC(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeByte(0xF9);

	token.read();
}

void x86Assembler :: compileSAHF(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeByte(0x9E);

	token.read();
}

void x86Assembler :: compileNOP(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeByte(0x90);

	token.read();
}

void x86Assembler :: compileREP(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeByte(0xF3);

	token.read();
}

void x86Assembler :: compileREPZ(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeByte(0xF3);

	token.read();
}

void x86Assembler :: compileLODSD(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeByte(0xAD);

	token.read();
}

void x86Assembler :: compileLODSW(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
   	code->writeByte(0x66);
	code->writeByte(0xAD);

	token.read();
}

void x86Assembler :: compileLODSB(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeByte(0xAC);

	token.read();
}

void x86Assembler :: compileMOVSB(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeByte(0xA4);

	token.read();
}

void x86Assembler :: compileSTOSD(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeByte(0xAB);

	token.read();
}

void x86Assembler :: compileSTOSB(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeByte(0xAA);

	token.read();
}

void x86Assembler :: compileSTOSW(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeByte(0x66);
	code->writeByte(0xAB);

	token.read();
}

void x86Assembler :: compileCMPSB(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeByte(0xA6);

	token.read();
}

void x86Assembler :: compilePUSHFD(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeByte(0x9C);

	token.read();
}

void x86Assembler :: compilePOPFD(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeByte(0x9D);

	token.read();
}

void x86Assembler :: compileJxx(TokenInfo& token, ProcedureInfo& info, SectionWriter* code, int prefix, x86JumpHelper& helper)
{
	token.read();

	bool shortJump = false;
	if (token.check(_T("short"))) {
		shortJump = true;
		token.read();
	}
   else if (token.check(_T("short"))) {
      token.raiseErr(_T("Use short prefix instead"));
   }

   // if jump forward
	if (!helper.checkDeclaredLabel(token.value)) {
      helper.writeJxxForward(token.value, prefix, shortJump);
	}
   // if jump backward
	else helper.writeJxxBack(token.value, prefix, shortJump);

	token.read();
}

void x86Assembler :: compileJMP(TokenInfo& token, ProcedureInfo& info, SectionWriter* code, x86JumpHelper& helper)
{
	token.read();

	bool shortJump = false;
	if (token.check(_T("short"))) {
		shortJump = true;
		token.read();
	}
   else if (token.check(_T("short"))) {
      token.raiseErr(_T("Use short prefix instead"));
   }

   // if jump forward
	if (!helper.checkDeclaredLabel(token.value)) {
      helper.writeJmpForward(token.value, shortJump);
	}
   // if jump backward
	else helper.writeJmpBack(token.value, shortJump);

	token.read();
}

void x86Assembler :: compileLOOP(TokenInfo& token, ProcedureInfo& info, SectionWriter* code, x86JumpHelper& helper)
{
	token.read();

   // if jump forward
	if (!helper.checkDeclaredLabel(token.value)) {
      helper.writeLoopForward(token.value);
	}
   // if jump backward
	else helper.writeLoopBack(token.value);

	token.read();
}

void x86Assembler :: compileCALL(TokenInfo& token, ProcedureInfo& info, SectionWriter* code, x86JumpHelper& helper)
{
	token.read();

	if (token.value[0]=='\'') {
		if (compstr(token.value, _T("'dlls'"), 6)) {
         LocalReferenceName function(DLL_NAMESPACE, token.value + 6);

			token.read(_T("."), _T("dot expected (%d)\n"));
			function.append(_T("."));
			function.append(token.read());

			int ref = info.binary->mapReference(function) | mskExternalRef;

			code->writeWord(0x15FF);
			code->writeRef(ref, 0);
		}
		else token.raiseErr(_T("Invalid call label (%d)\n"));
	}
	else if (token.check(_T("["))) {
		token.read();
		Operand operand = readOperand(token, info, _T("Invalid call target (%d)\n"), x86Helper::otM32);
		if (!token.check(_T("]")))
			token.raiseErr(_T("']' expected(%d)\n"));

		if (test(operand.type, x86Helper::otM32)) {
			code->writeByte(0xFF);
			x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 2), operand);
			x86Helper::writeImm(code, operand);
		}
		else token.raiseErr(_T("Invalid call target (%d)\n"));
	}
	else {
		if (token.check(_T("@"))) {
			size_t ref = 0;

			token.read();
			if (token.terminal.type==dfaQuote) {
				String funRef(token.terminal.line + 1, token.terminal.length-2);
            ref = info.binary->mapReference(funRef) | mskNativeCodeRef | mskRelativeRef;
			}
			else ref = info.binary->mapReference(token.value) | mskSymbolRef | mskRelativeRef;

		   code->writeByte(0xE8);
			code->writeRef(ref, 0);
		}
      else if (token.check(ARGUMENTFUN1)) {
		   code->writeByte(0xE8);
         code->writeRef(-7, 0);
      }
      else if (token.check(ARGUMENTFUN2)) {
		   code->writeByte(0xE8);
         code->writeRef(-8, 0);
      }
      // if jump forward
	   else if (!helper.checkDeclaredLabel(token.value)) {
         helper.writeCallForward(token.value);
	   }
      // if jump backward
      else helper.writeCallBack(token.value);
	}
	token.read();
}

void x86Assembler :: fixJump(TokenInfo& token, ProcedureInfo& info, SectionWriter* code, x86JumpHelper& helper)
{
	if (helper.checkDeclaredLabel(token.value)) {
		token.raiseErr(_T("Label with such a name already exists (%d)\n"));
	}

	if (!helper.addLabel(token.value))
      token.raiseErr(_T("Invalid near jump to this label (%d)\n"));

	token.read(_T(":"), _T("Invalid command or label (%d)\n"));
}

void x86Assembler :: compileFBLD(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	Operand sour = compileOperand(token, info, _T("Invalid operand (%d)\n"));
	if (test(sour.type, x86Helper::otM32)) {
		code->writeByte(0xDF);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 4), sour);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileFILD(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	token.read();

	if (token.check(_T("dword"))) {
		Operand sour = readPtrOperand(token, info, _T("Invalid operand (%d)\n"), x86Helper::otM32);

		if (test(sour.type, x86Helper::otM32)) {
			code->writeByte(0xDB);
			x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 0), sour);
		}
		else token.raiseErr(_T("Invalid command (%d)"));
	}
   else if (token.check(_T("qword"))) {
		Operand sour = readPtrOperand(token, info, _T("Invalid operand (%d)\n"), x86Helper::otM32);

		if (test(sour.type, x86Helper::otM32)) {
			code->writeByte(0xDF);
			x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 5), sour);
		}
		else token.raiseErr(_T("Invalid command (%d)"));
   }
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileFIST(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	Operand sour = compileOperand(token, info, _T("Invalid operand (%d)\n"));
	if (test(sour.type, x86Helper::otM32)) {
		code->writeByte(0xDB);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 2), sour);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileFISTP(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	Operand sour = compileOperand(token, info, _T("Invalid operand (%d)\n"));
	if (test(sour.type, x86Helper::otM32)) {
		code->writeByte(0xDB);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 3), sour);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileFLD(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	token.read();
	if (token.check(_T("st"))) {
		int index = readStReg(token);

		code->writeByte(0xD9);
		code->writeByte(0xC0 + index);

		token.read();
	}
	else if (token.check(_T("qword"))) {
		Operand operand = readPtrOperand(token, info, _T("Invalid operand (%d)\n"), x86Helper::otM32);

		code->writeByte(0xDD);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 0), operand);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileFXCH(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	token.read();
	if (token.check(_T("st"))) {
		int index = readStReg(token);

		token.read();

		code->writeByte(0xD9);
		code->writeByte(0xC8 + index);
	}
   else {
		code->writeByte(0xD9);
		code->writeByte(0xC9);
   }
}

void x86Assembler :: compileFSUB(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	token.read();
	if (token.check(_T("st"))) {
		int sour = readStReg(token);

		token.read(_T(","),_T("',' comma expected(%d)\n"));

		if (sour != 0) {
			token.read(_T("st"), _T("'st' expected (%d)\n"));
			if (readStReg(token)!=0)
				token.raiseErr(_T("'0' expected (%d)"));

			token.read();

			code->writeByte(0xDC);
			code->writeByte(0xE8 + sour);
		}
		else {
			token.read(_T("st"), _T("'st' expected (%d)\n"));
			sour = readStReg(token);
			token.read();

			code->writeByte(0xD8);
			code->writeByte(0xE0 + sour);
		}
	}
	else if (token.check(_T("qword"))) {
		Operand operand = readPtrOperand(token, info, _T("Invalid operand (%d)\n"), x86Helper::otM32);

		code->writeByte(0xDC);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 4), operand);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileFADD(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	token.read();
	if (token.check(_T("st"))) {
		int sour = readStReg(token);

		token.read(_T(","),_T("',' comma expected(%d)\n"));

		if (sour != 0) {
			token.read(_T("st"), _T("'st' expected (%d)\n"));
			if (readStReg(token)!=0)
				token.raiseErr(_T("'0' expected (%d)"));

			token.read();

			code->writeByte(0xDC);
			code->writeByte(0xC0 + sour);
		}
		else {
			token.read(_T("st"), _T("'st' expected (%d)\n"));
			sour = readStReg(token);
			token.read();

			code->writeByte(0xD8);
			code->writeByte(0xC0 + sour);
		}
	}
	else if (token.check(_T("qword"))) {
		Operand operand = readPtrOperand(token, info, _T("Invalid operand (%d)\n"), x86Helper::otM32);

		code->writeByte(0xDC);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 0), operand);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileFMUL(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	token.read();
	if (token.check(_T("st"))) {
		int sour = readStReg(token);

		token.read(_T(","),_T("',' comma expected(%d)\n"));

		if (sour != 0) {
			token.read(_T("st"), _T("'st' expected (%d)\n"));
			if (readStReg(token)!=0)
				token.raiseErr(_T("'0' expected (%d)"));

			token.read();

			code->writeByte(0xDC);
			code->writeByte(0xC8 + sour);
		}
		else {
			token.read(_T("st"), _T("'st' expected (%d)\n"));
			sour = readStReg(token);
			token.read();

			code->writeByte(0xD8);
			code->writeByte(0xC8 + sour);
		}
	}
	else if (token.check(_T("qword"))) {
		Operand operand = readPtrOperand(token, info, _T("Invalid operand (%d)\n"), x86Helper::otM32);

		code->writeByte(0xDC);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 1), operand);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileFDIV(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	token.read();
	if (token.check(_T("qword"))) {
		Operand operand = readPtrOperand(token, info, _T("Invalid operand (%d)\n"), x86Helper::otM32);

		code->writeByte(0xDC);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 6), operand);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileFCOMIP(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	token.read();
	if (token.check(_T("st"))) {
		token.read(_T(","), _T("',' expected (%d)\n"));
		token.read(_T("st"), _T("'st' expected (%d)\n"));

		int dest = readStReg(token);

		token.read();

		code->writeByte(0xDF);
		code->writeByte(0xF0 + dest);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileFCOMP(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	token.read();
	if (token.check(_T("st"))) {
		token.read(_T(","), _T("',' expected (%d)\n"));
		token.read(_T("st"), _T("'st' expected (%d)\n"));

		int dest = readStReg(token);

		token.read();

		code->writeByte(0xD8);
		code->writeByte(0xD8 + dest);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileFSTP(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	token.read();
	if (token.check(_T("st"))) {
		int sour = readStReg(token);
		token.read();

		code->writeByte(0xDD);
		code->writeByte(0xD8 + sour);
	}
	else if (token.check(_T("qword"))) {
		Operand operand = readPtrOperand(token, info, _T("Invalid operand (%d)\n"), x86Helper::otM32);

		code->writeByte(0xDD);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 3), operand);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileFBSTP(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	token.read();
	if (token.check(_T("tbyte"))) {
		Operand operand = readPtrOperand(token, info, _T("Invalid operand (%d)\n"), x86Helper::otM32);

		code->writeByte(0xDF);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 6), operand);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileFSTSW(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	token.read();
	if (token.check(_T("ax"))) {
		token.read();

		code->writeByte(0x9B);
		code->writeWord(0xE0DF);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileFNSTSW(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	token.read();
	if (token.check(_T("ax"))) {
		token.read();

		code->writeWord(0xE0DF);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileFSTCW(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	token.read();
	if (token.check(_T("word"))) {
		Operand operand = readPtrOperand(token, info, _T("Invalid operand (%d)\n"), x86Helper::otM32);

		code->writeByte(0x9B);
		code->writeByte(0xD9);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 7), operand);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileFLDCW(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	token.read();
	if (token.check(_T("word"))) {
		Operand operand = readPtrOperand(token, info, _T("Invalid operand (%d)\n"), x86Helper::otM32);

		code->writeByte(0xD9);
		x86Helper::writeModRM(code, Operand(x86Helper::otR32 + 5), operand);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileFLDZ(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeWord(0xEED9);

	token.read();
}

void x86Assembler :: compileFLD1(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeWord(0xE8D9);

	token.read();
}

void x86Assembler :: compileFADDP(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeWord(0xC1DE);

	token.read();
}

void x86Assembler :: compileF2XM1(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeWord(0xF0D9);

	token.read();
}

void x86Assembler :: compileFLDL2T(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeWord(0xE9D9);

	token.read();
}

void x86Assembler :: compileFLDLG2(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeWord(0xECD9);

	token.read();
}

void x86Assembler :: compileFRNDINT(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeWord(0xFCD9);

	token.read();
}

void x86Assembler :: compileFSCALE(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeWord(0xFDD9);

	token.read();
}

void x86Assembler :: compileFXAM(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeWord(0xE5D9);

	token.read();
}

void x86Assembler :: compileFMULP(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeWord(0xC9DE);

	token.read();
}

void x86Assembler :: compileFABS(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeWord(0xE1D9);

	token.read();
}

void x86Assembler :: compileFSQRT(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeWord(0xFAD9);

	token.read();
}

void x86Assembler :: compileFSIN(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeWord(0xFED9);

	token.read();
}

void x86Assembler :: compileFCOS(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeWord(0xFFD9);

	token.read();
}

void x86Assembler :: compileFYL2X(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeWord(0xF1D9);

	token.read();
}

void x86Assembler :: compileFTST(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeWord(0xE4D9);

	token.read();
}

void x86Assembler :: compileFLDPI(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeWord(0xEBD9);

	token.read();
}

void x86Assembler :: compileFPREM(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeWord(0xF8D9);

	token.read();
}

void x86Assembler :: compileFPATAN(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeWord(0xF3D9);

	token.read();
}

void x86Assembler :: compileFLDL2E(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeWord(0xEAD9);

	token.read();
}

void x86Assembler :: compileFLDLN2(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	code->writeWord(0xEDD9);

	token.read();
}

void x86Assembler :: compileFFREE(TokenInfo& token, ProcedureInfo& info, SectionWriter* code)
{
	token.read();
	if (token.check(_T("st"))) {
		int sour = readStReg(token);
		token.read();

		code->writeByte(0xDD);
		code->writeByte(0xC0 + sour);
	}
	else token.raiseErr(_T("Invalid command (%d)"));
}

void x86Assembler :: compileStructure(TokenInfo& token, _Module* binary)
{
	token.read();

	ReferenceName refName(PACKAGE_MODULE, token.value);
   ProcedureInfo info(binary);

   int ref = binary->mapReference(refName, true);

   if (binary->mapSection(ref, true)!=NULL) {
      throw AssemblerException(_T("Structure / Procedure already exists (%d)\n"), token.terminal.row);
   }
   size_t operationRef = binary->mapReference(refName);
   Section* code = binary->mapSection(operationRef | mskNativeDataRef, false);
   SectionWriter writer(code);

   token.read();
   while (!token.check(_T("end"))) {
      if (token.check(_T("dd"))) {
         token.read();
         Operand operand = readOperand(token, info, _T("Invalid constant"), x86Helper::otDD);
         if (operand.type==x86Helper::otDD) {
            x86Helper::writeImm(&writer, operand);
         }
         else token.raiseErr(_T("Invalid operand (%d)"));
      }
      else if (token.Eof()) {
         token.raiseErr(_T("Invalid end of the file\n"));
      }
      else token.read();
   }
}

bool x86Assembler :: compileCommandA(TokenInfo& token, ProcedureInfo& info, SectionWriter& writer)
{
	if (token.check(_T("add"))) {
		compileADD(token, info, &writer);
      return true;
	}
	else if (token.check(_T("and"))) {
		compileAND(token, info, &writer);
      return true;
	}
	else if (token.check(_T("adc"))) {
		compileADC(token, info, &writer);
      return true;
	}
   else return false;
}
bool x86Assembler :: compileCommandB(TokenInfo& token)
{
   return false;
}
bool x86Assembler :: compileCommandC(TokenInfo& token, ProcedureInfo& info, SectionWriter& writer, x86JumpHelper& helper)
{
	if (token.check(_T("cmp"))) {
		compileCMP(token, info, &writer);
      return true;
	}
	else if (token.check(_T("call"))) {
		compileCALL(token, info, &writer, helper);
      return true;
	}
	else if (token.check(_T("cdq"))) {
		compileCDQ(token, info, &writer);
      return true;
	}
	else if (token.check(_T("cmpsb"))) {
		compileCMPSB(token, info, &writer);
      return true;
	}
   else return false;
}
bool x86Assembler :: compileCommandD(TokenInfo& token, ProcedureInfo& info, SectionWriter& writer)
{
	if (token.check(_T("dec"))) {
		compileDEC(token, info, &writer);
      return true;
	}
	else if (token.check(_T("div"))) {
		compileDIV(token, info, &writer);
      return true;
	}
   else return false;
}
bool x86Assembler :: compileCommandE(TokenInfo& token)
{
   return false;
}
bool x86Assembler :: compileCommandF(TokenInfo& token, ProcedureInfo& info, SectionWriter& writer)
{
	if (token.check(_T("fldz"))) {
		compileFLDZ(token, info, &writer);
      return true;
	}
	else if (token.check(_T("fld1"))) {
		compileFLD1(token, info, &writer);
      return true;
	}
	else if (token.check(_T("f2xm1"))) {
		compileF2XM1(token, info, &writer);
      return true;
	}
	else if (token.check(_T("fbld"))) {
		compileFBLD(token, info, &writer);
      return true;
	}
	else if (token.check(_T("fild"))) {
		compileFILD(token, info, &writer);
      return true;
	}
	else if (token.check(_T("fist"))) {
		compileFIST(token, info, &writer);
      return true;
	}
	else if (token.check(_T("fistp"))) {
		compileFISTP(token, info, &writer);
      return true;
	}
	else if (token.check(_T("fld"))) {
		compileFLD(token, info, &writer);
      return true;
	}
	else if (token.check(_T("fadd"))) {
		compileFADD(token, info, &writer);
      return true;
	}
	else if (token.check(_T("fsub"))) {
		compileFSUB(token, info, &writer);
      return true;
	}
	else if (token.check(_T("fcomip"))) {
		compileFCOMIP(token, info, &writer);
      return true;
	}
	else if (token.check(_T("fcomp"))) {
		compileFCOMP(token, info, &writer);
      return true;
	}
	else if (token.check(_T("fmulp"))) {
		compileFMULP(token, info, &writer);
      return true;
	}
	else if (token.check(_T("fmul"))) {
		compileFMUL(token, info, &writer);
      return true;
	}
	else if (token.check(_T("fdiv"))) {
		compileFDIV(token, info, &writer);
      return true;
	}
	else if (token.check(_T("faddp"))) {
		compileFADDP(token, info, &writer);
      return true;
	}
	else if (token.check(_T("fldl2t"))) {
		compileFLDL2T(token, info, &writer);
      return true;
	}
	else if (token.check(_T("fldlg2"))) {
		compileFLDLG2(token, info, &writer);
      return true;
	}
	else if (token.check(_T("frndint"))) {
		compileFRNDINT(token, info, &writer);
      return true;
	}
	else if (token.check(_T("fxch"))) {
		compileFXCH(token, info, &writer);
      return true;
	}
	else if (token.check(_T("fstp"))) {
		compileFSTP(token, info, &writer);
      return true;
	}
	else if (token.check(_T("fbstp"))) {
		compileFBSTP(token, info, &writer);
      return true;
	}
	else if (token.check(_T("fstsw"))) {
		compileFSTSW(token, info, &writer);
      return true;
	}
	else if (token.check(_T("fnstsw"))) {
		compileFNSTSW(token, info, &writer);
      return true;
	}
	else if (token.check(_T("fstcw"))) {
		compileFSTCW(token, info, &writer);
      return true;
	}
	else if (token.check(_T("fldcw"))) {
		compileFLDCW(token, info, &writer);
      return true;
	}
	else if (token.check(_T("fscale"))) {
		compileFSCALE(token, info, &writer);
      return true;
	}
	else if (token.check(_T("fxam"))) {
		compileFXAM(token, info, &writer);
      return true;
	}
	else if (token.check(_T("fabs"))) {
		compileFABS(token, info, &writer);
      return true;
	}
	else if (token.check(_T("fsqrt"))) {
		compileFSQRT(token, info, &writer);
      return true;
	}
	else if (token.check(_T("fcos"))) {
		compileFCOS(token, info, &writer);
      return true;
	}
	else if (token.check(_T("fsin"))) {
		compileFSIN(token, info, &writer);
      return true;
	}
	else if (token.check(_T("fyl2x"))) {
		compileFYL2X(token, info, &writer);
      return true;
	}
	else if (token.check(_T("ftst"))) {
		compileFTST(token, info, &writer);
      return true;
	}
	else if (token.check(_T("fldpi"))) {
		compileFLDPI(token, info, &writer);
      return true;
	}
	else if (token.check(_T("fprem"))) {
		compileFPREM(token, info, &writer);
      return true;
	}
	else if (token.check(_T("fpatan"))) {
		compileFPATAN(token, info, &writer);
      return true;
	}
	else if (token.check(_T("fldl2e"))) {
		compileFLDL2E(token, info, &writer);
      return true;
	}
	else if (token.check(_T("fldln2"))) {
		compileFLDLN2(token, info, &writer);
      return true;
	}
	else if (token.check(_T("ffree"))) {
		compileFFREE(token, info, &writer);
      return true;
	}
   else return false;
}
bool x86Assembler :: compileCommandG(TokenInfo& token)
{
   return false;
}
bool x86Assembler :: compileCommandH(TokenInfo& token)
{
   return false;
}
bool x86Assembler :: compileCommandI(TokenInfo& token, ProcedureInfo& info, SectionWriter& writer)
{
	if (token.check(_T("inc"))) {
		compileINC(token, info, &writer);
      return true;
	}
	else if (token.check(_T("imul"))) {
		compileIMUL(token, info, &writer);
      return true;
	}
	else if (token.check(_T("idiv"))) {
		compileIDIV(token, info, &writer);
      return true;
	}
   else return false;
}
bool x86Assembler :: compileCommandJ(TokenInfo& token, ProcedureInfo& info, SectionWriter& writer, x86JumpHelper& helper)
{
	if (token.check(_T("jb"))||token.check(_T("jc"))) {
      compileJxx(token, info, &writer, x86Helper::JUMP_TYPE_JB, helper);
      return true;
	}
	else if (token.check(_T("jnb"))||token.check(_T("jnc"))||token.check(_T("jae"))) {
		compileJxx(token, info, &writer, x86Helper::JUMP_TYPE_JAE, helper);
      return true;
	}
	else if (token.check(_T("jz"))||token.check(_T("je"))) {
      compileJxx(token, info, &writer, x86Helper::JUMP_TYPE_JZ, helper);
      return true;
	}
	else if (token.check(_T("jnz"))) {
      compileJxx(token, info, &writer, x86Helper::JUMP_TYPE_JNZ, helper);
      return true;
	}
	else if (token.check(_T("jbe"))) {
		compileJxx(token, info, &writer, x86Helper::JUMP_TYPE_JBE, helper);
      return true;
	}
	else if (token.check(_T("ja"))||token.check(_T("jnbe"))) {
		compileJxx(token, info, &writer, x86Helper::JUMP_TYPE_JA, helper);
      return true;
	}
	else if (token.check(_T("js"))) {
		compileJxx(token, info, &writer, x86Helper::JUMP_TYPE_JS, helper);
      return true;
	}
	else if (token.check(_T("jns"))) {
		compileJxx(token, info, &writer, x86Helper::JUMP_TYPE_JNS, helper);
      return true;
	}
	else if (token.check(_T("jpe")) || token.check(_T("jp"))) {
		compileJxx(token, info, &writer, x86Helper::JUMP_TYPE_JP, helper);
      return true;
	}
	else if (token.check(_T("jpo")) || token.check(_T("jnp"))) {
		compileJxx(token, info, &writer, x86Helper::JUMP_TYPE_JPO, helper);
      return true;
	}
	else if (token.check(_T("jl"))) {
		compileJxx(token, info, &writer, x86Helper::JUMP_TYPE_JL, helper);
      return true;
	}
	else if (token.check(_T("jge"))) {
		compileJxx(token, info, &writer, x86Helper::JUMP_TYPE_JGE, helper);
      return true;
	}
	else if (token.check(_T("jle"))) {
		compileJxx(token, info, &writer, x86Helper::JUMP_TYPE_JLE, helper);
      return true;
	}
	else if (token.check(_T("jg"))) {
		compileJxx(token, info, &writer, x86Helper::JUMP_TYPE_JG, helper);
      return true;
	}
	else if (token.check(_T("jmp"))) {
		compileJMP(token, info, &writer, helper);
      return true;
	}
   else return false;
}
bool x86Assembler :: compileCommandK(TokenInfo& token)
{
   return false;
}
bool x86Assembler :: compileCommandL(TokenInfo& token, ProcedureInfo& info, SectionWriter& writer, x86JumpHelper& helper)
{
	if (token.check(_T("lea"))) {
		compileLEA(token, info, &writer);
      return true;
	}
	else if (token.check(_T("loop"))) {
		compileLOOP(token, info, &writer, helper);
      return true;
	}
	else if (token.check(_T("lodsd"))) {
		compileLODSD(token, info, &writer);
      return true;
	}
	else if (token.check(_T("lodsw"))) {
		compileLODSW(token, info, &writer);
      return true;
	}
	else if (token.check(_T("lodsb"))) {
		compileLODSB(token, info, &writer);
      return true;
	}
   else return false;
}
bool x86Assembler :: compileCommandM(TokenInfo& token, ProcedureInfo& info, SectionWriter& writer)
{
	if (token.check(_T("mov"))) {
		compileMOV(token, info, &writer);
      return true;
	}
	else if (token.check(_T("mul"))) {
		compileMUL(token, info, &writer);
      return true;
	}
	else if (token.check(_T("movzx"))) {
		compileMOVZX(token, info, &writer);
      return true;
	}
	else if (token.check(_T("movsb"))) {
		compileMOVSB(token, info, &writer);
      return true;
	}
   else return false;
}
bool x86Assembler :: compileCommandN(TokenInfo& token, ProcedureInfo& info, SectionWriter& writer)
{
	if (token.check(_T("neg"))) {
		compileNEG(token, info, &writer);
      return true;
	}
	else if (token.check(_T("not"))) {
		compileNOT(token, info, &writer);
      return true;
	}
	else if (token.check(_T("nop"))) {
		compileNOP(token, info, &writer);
      return true;
	}
   else return false;
}
bool x86Assembler :: compileCommandO(TokenInfo& token, ProcedureInfo& info, SectionWriter& writer)
{
	if (token.check(_T("or"))) {
		compileOR(token, info, &writer);
      return true;
	}
   else return false;
}
bool x86Assembler :: compileCommandP(TokenInfo& token, ProcedureInfo& info, SectionWriter& writer)
{
	if (token.check(_T("push"))) {
		compilePUSH(token, info, &writer);
      return true;
	}
	else if (token.check(_T("pop"))) {
		compilePOP(token, info, &writer);
      return true;
	}
	else if (token.check(_T("pushfd"))) {
		compilePUSHFD(token, info, &writer);
      return true;
	}
	else if (token.check(_T("popfd"))) {
		compilePOPFD(token, info, &writer);
      return true;
	}
   else return false;
}
bool x86Assembler :: compileCommandQ(TokenInfo& token)
{
   return false;
}
bool x86Assembler :: compileCommandR(TokenInfo& token, ProcedureInfo& info, SectionWriter& writer)
{
	if (token.check(_T("ret"))) {
		compileRET(token, info, &writer);
      return true;
	}
	else if (token.check(_T("rol"))) {
		compileROL(token, info, &writer);
      return true;
	}
	else if (token.check(_T("ror"))) {
		compileROR(token, info, &writer);
      return true;
	}
	else if (token.check(_T("rcr"))) {
		compileRCR(token, info, &writer);
      return true;
	}
	else if (token.check(_T("rcl"))) {
		compileRCL(token, info, &writer);
      return true;
	}
	else if (token.check(_T("rep"))) {
		compileREP(token, info, &writer);
      return true;
	}
	else if (token.check(_T("repz"))) {
		compileREPZ(token, info, &writer);
      return true;
	}
   else return false;
}
bool x86Assembler :: compileCommandS(TokenInfo& token, ProcedureInfo& info, SectionWriter& writer)
{
	if (token.check(_T("sub"))) {
		compileSUB(token, info, &writer);
      return true;
	}
	else if (token.check(_T("sahf"))) {
		compileSAHF(token, info, &writer);
      return true;
	}
	else if (token.check(_T("sbb"))) {
		compileSBB(token, info, &writer);
      return true;
	}
	else if (token.check(_T("shr"))) {
		compileSHR(token, info, &writer);
      return true;
	}
	else if (token.check(_T("sar"))) {
		compileSAR(token, info, &writer);
      return true;
	}
	else if (token.check(_T("shl"))) {
		compileSHL(token, info, &writer);
      return true;
	}
	else if (token.check(_T("stc"))) {
		compileSTC(token, info, &writer);
      return true;
	}
	else if (token.check(_T("stosd"))) {
		compileSTOSD(token, info, &writer);
      return true;
	}
	else if (token.check(_T("stosb"))) {
		compileSTOSB(token, info, &writer);
      return true;
	}
	else if (token.check(_T("stosw"))) {
		compileSTOSW(token, info, &writer);
      return true;
	}
	else if (token.check(_T("shld"))) {
		compileSHLD(token, info, &writer);
      return true;
	}
	else if (token.check(_T("shrd"))) {
		compileSHRD(token, info, &writer);
      return true;
	}
   else return false;
}
bool x86Assembler :: compileCommandT(TokenInfo& token, ProcedureInfo& info, SectionWriter& writer)
{
	if (token.check(_T("test"))) {
		compileTEST(token, info, &writer);
      return true;
	}
   else return false;
}
bool x86Assembler :: compileCommandU(TokenInfo& token)
{
   return false;
}
bool x86Assembler :: compileCommandV(TokenInfo& token)
{
   return false;
}
bool x86Assembler :: compileCommandW(TokenInfo& token)
{
   return false;
}
bool x86Assembler :: compileCommandX(TokenInfo& token, ProcedureInfo& info, SectionWriter& writer)
{
	if (token.check(_T("xor"))) {
		compileXOR(token, info, &writer);
      return true;
	}
	else if (token.check(_T("xchg"))) {
		compileXCHG(token, info, &writer);
      return true;
	}
   else return false;
}
bool x86Assembler :: compileCommandY(TokenInfo& token)
{
   return false;
}
bool x86Assembler :: compileCommandZ(TokenInfo& token)
{
   return false;
}
bool x86Assembler :: compileCommand(TokenInfo& token, ProcedureInfo& info, SectionWriter& writer, x86JumpHelper& helper)
{
   bool recognized = false;
   if (token.value[0]=='a') {
      recognized = compileCommandA(token, info, writer);
   }
   else if (token.value[0]=='b') {
      recognized = compileCommandB(token);
   }
   else if (token.value[0]=='c') {
      recognized = compileCommandC(token, info, writer, helper);
   }
   else if (token.value[0]=='d') {
      recognized = compileCommandD(token, info, writer);
   }
   else if (token.value[0]=='e') {
      recognized = compileCommandE(token);
   }
   else if (token.value[0]=='f') {
      recognized = compileCommandF(token, info, writer);
   }
   else if (token.value[0]=='g') {
      recognized = compileCommandG(token);
   }
   else if (token.value[0]=='h') {
      recognized = compileCommandH(token);
   }
   else if (token.value[0]=='i') {
      recognized = compileCommandI(token, info, writer);
   }
   else if (token.value[0]=='j') {
      recognized = compileCommandJ(token, info, writer, helper);
   }
   else if (token.value[0]=='k') {
      recognized = compileCommandK(token);
   }
   else if (token.value[0]=='l') {
      recognized = compileCommandL(token, info, writer, helper);
   }
   else if (token.value[0]=='m') {
      recognized = compileCommandM(token, info, writer);
   }
   else if (token.value[0]=='n') {
      recognized = compileCommandN(token, info, writer);
   }
   else if (token.value[0]=='o') {
      recognized = compileCommandO(token, info, writer);
   }
   else if (token.value[0]=='p') {
      recognized = compileCommandP(token, info, writer);
   }
   else if (token.value[0]=='q') {
      recognized = compileCommandQ(token);
   }
   else if (token.value[0]=='r') {
      recognized = compileCommandR(token, info, writer);
   }
   else if (token.value[0]=='s') {
      recognized = compileCommandS(token, info, writer);
   }
   else if (token.value[0]=='t') {
      recognized = compileCommandT(token, info, writer);
   }
   else if (token.value[0]=='u') {
      recognized = compileCommandU(token);
   }
   else if (token.value[0]=='v') {
      recognized = compileCommandV(token);
   }
   else if (token.value[0]=='w') {
      recognized = compileCommandW(token);
   }
   else if (token.value[0]=='x') {
      recognized = compileCommandX(token, info, writer);
   }
   else if (token.value[0]=='y') {
      recognized = compileCommandY(token);
   }
   else if (token.value[0]=='z') {
      recognized = compileCommandZ(token);
   }

   if (!recognized) {
      if (token.Eof()) {
			token.raiseErr(_T("Invalid end of the file\n"));
		}
		else {
			fixJump(token, info, &writer, helper);
			token.read();
		}
   }
   return recognized;
}

void x86Assembler :: compileProcedure(TokenInfo& token, _Module* binary, bool aligned)
{
	token.read();

   LocalReferenceName refName(PACKAGE_MODULE, token.value);

   ref_t ref = binary->mapReference(refName, true);

	if (binary->mapSection(ref, true)!=NULL) {
		throw AssemblerException(_T("Procedure already exists (%d)\n"), token.terminal.row);
	}
	size_t operationRef = binary->mapReference(refName);
   Section* code = binary->mapSection(operationRef | mskNativeCodeRef, false);
	SectionWriter writer(code);

	x86JumpHelper helper(&writer);
	ProcedureInfo info(binary);

	token.read();

	if (token.check(_T("("))) {
	   readParameterList(token, info);
	   token.read();
	}

	while (!token.check(_T("end"))) {
      compileCommand(token, info, writer, helper);
	}
   if (aligned)
	   writer.align(4, 0x90);
}

void x86Assembler :: compile(TextReader* source, const TCHAR* outputPath)
{
	Module		 binary(_T("$binary"));
	SourceReader reader(4, source);

	TokenInfo	 token(&reader);

	token.read();
	do {
		if (token.check(_T("define"))) {
         LocalString<0x100> name(token.read());
			size_t value = token.readInteger();

			if (!constants.add(name, value, true))
				token.raiseErr(_T("Constant already exists (%d)\n"));

			token.read();
		}
		else if (token.check(_T("procedure"))) {
			compileProcedure(token, &binary, true);

			token.read();
		}
		else if (token.check(_T("inline"))) {
			compileProcedure(token, &binary, false);

			token.read();
		}
      else if (token.check(_T("structure"))) {
			compileStructure(token, &binary);

			token.read();
      }
		else token.raiseErr(_T("Invalid statement (%d)\n"));

	} while (!token.Eof());

   FileWriter writer(outputPath, feRaw);
   binary.save(writer);
}
