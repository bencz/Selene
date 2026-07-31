//------------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Engine
//
//		This file contains implementation of ELENA byte code routines.
//
//                                                  (C)2009, by Alexei Rakov
//------------------------------------------------------------------------------

#include "elena.h"
// -----------------------------------------------------------------------------
#include "bytecode.h"

using namespace _ELENA_;

// --- byte code names ---
//
// Indexed by the opcode byte as it appears in a section: the high nibble is the
// command family and the low nibble the operand form, so the combined value is
// what gets written and what is named here.
//
// Needed by any tool that has to read byte code back -- the disassembler today,
// the LLVM translator next. Its absence is why nothing could inspect what the
// compiler emits.

const char* _ELENA_::getByteCodeName(unsigned char code)
{
   switch (code) {
      case bcNop:         return "nop";

      // Meta commands. ByteCommand::save writes (unsigned char)code, so
      // bcAllocStack (0x101) and bcFreeStack (0x102) truncate to 0x01 and 0x02
      // and land in command family 0, which the x86 JIT maps to compileNop --
      // it skips them while still consuming their argument.
      //
      // They are not noise: they carry the stack depth the compiler computed.
      // The x86 backend discards that because it uses the machine stack
      // directly, but a stack-to-SSA translation needs exactly this, so the
      // LLVM backend must decode rather than skip them.
      case (unsigned char)bcAllocStack: return "allocstack";
      case (unsigned char)bcFreeStack:  return "freestack";
      case bcPrep:        return "prep";
      case bcPrepRedir:   return "prepredir";
      case bcSPrep:       return "sprep";
      case bcSPrepParam:  return "sprepparam";
      case bcPush:        return "push";
      case bcRPush:       return "rpush";
      case bcIPush:       return "ipush";
      case bcIOPush:      return "iopush";
      case bcSPush:       return "spush";
      case bcRPushPtr:    return "rpushptr";
      case bcIFPush:      return "ifpush";
      case bcISPush:      return "ispush";
      case bcReturn:      return "return";
      case bcSReturn:     return "sreturn";
      case bcRCall:       return "rcall";
      case bcRCallExt:    return "rcallext";
      case bcRCallEmb:    return "rcallemb";
      case bcPop:         return "pop";
      case bcIOMove:      return "iomove";
      case bcRMovePtr:    return "rmoveptr";
      case bcIFMove:      return "ifmove";
      case bcISMove:      return "ismove";
      case bcOMovePtr:    return "omoveptr";
      case bcExitRedir:   return "exitredir";
      case bcSExit:       return "sexit";
      case bcRedirect:    return "redirect";
      case bcRRedirect:   return "rredirect";
      case bcIOSet:       return "ioset";
      case bcIFSet:       return "ifset";
      case bcRReturnIf:   return "rreturnif";
      case bcIOCall0:     return "iocall0";
      case bcIOCall1:     return "iocall1";
      case bcIRCall0:     return "ircall0";
      case bcIRCall1:     return "ircall1";
      case bcUnShift:     return "unshift";
      case bcIJump:       return "ijump";
      case bcOCreate:     return "ocreate";
      case bcIOSwap:      return "ioswap";
      case bcShift:       return "shift";
      case bcDebug:       return "debug";
      default:            return NULL;
   }
}

// Number of 32-bit arguments that follow the opcode byte.
// Mirrors x86JITCompiler::compileMethod exactly.
int _ELENA_::getByteCodeArgCount(unsigned char code)
{
   if ((code & 0x3) == 0)
      return 0;

   return test(code, 0x3) ? 2 : 1;
}

// --- CommandTape ---

void CommandTape :: write(ByteCode code)
{
   write(ByteCommand(code));
}

void CommandTape :: write(ByteCode code, int argument)
{
   write(ByteCommand(code, argument));
}

void CommandTape :: write(ByteCode code, int argument, int hint)
{
   write(ByteCommand(code, argument, hint));
}

void CommandTape :: write(ByteCode code, int argument, LabelType type)
{
   write(ByteCommand(code, argument, type));
}

void CommandTape :: write(ByteCommand command)
{
   tape.add(command);
}
