//------------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Engine
//
//		This file contains common ELENA byte code classes and constants
//		
//                                              (C)2009, by Alexei Rakov
//------------------------------------------------------------------------------

#ifndef bytecodeH
#define bytecodeH 1

namespace _ELENA_
{

// --- Byte code command set ---
enum ByteCode
{  
   // commands:
   bcNop         = 0x00,

   // commands   
   bcPrep        = 0x10,
   bcPrepRedir   = 0x14,
   bcSPrep       = 0x18,
   bcSPrepParam  = 0x1C,
   bcPush        = 0x20,
   bcRPush       = 0x21,
   bcIPush       = 0x22, 
   bcIOPush      = 0x26,
   bcSPush       = 0x28,
   bcRPushPtr    = 0x29,
   bcIFPush      = 0x2A,
   bcISPush      = 0x2E,
   bcReturn      = 0x30,
   bcSReturn     = 0x38,
//   bcCall        = 0x40,
   bcRCall       = 0x43,
//   bcCallRedir   = 0x46,
   bcRCallExt    = 0x47,
   bcRCallEmb    = 0x4F,
   bcPop         = 0x50,
//   bcMove        = 0x60,
   bcIOMove      = 0x66,
   bcRMovePtr    = 0x69,
   bcIFMove      = 0x6A,
   bcISMove      = 0x6E,
   bcOMovePtr    = 0x6F,
//   bcExit        = 0x70,
   bcExitRedir   = 0x74,
   bcSExit       = 0x78,
   bcRedirect    = 0x80,
   bcRRedirect   = 0x81,
   bcIOSet       = 0x93,
   bcIFSet       = 0x9A,
   bcRReturnIf   = 0xB1,
   bcIOCall0     = 0xC3,
   bcIOCall1     = 0xC7,
   bcIRCall0     = 0xCB,
   bcIRCall1     = 0xCF,
   bcUnShift     = 0xF0,
   bcIJump       = 0xF2,
   bcOCreate     = 0xF3,
   bcIOSwap      = 0xFA,
   bcShift       = 0xFE,
   bcDebug       = 0xFC,

   // command operands:
   bcROperand    = 0x01,   // reference operand
   bcIOperand    = 0x02,   // integer operand
   bcIOOperand   = 0x06,   // [sp:offset]
   bcRPtrOperand = 0x09,   // reference ptr operand
   bcIFOperand   = 0x0A,
   bcISOperand   = 0x0E,
   bcOPtrOperand = 0x0F,

   bcRedirOperand= 0x04,
   bcSOperand    = 0x08,
   bcSPrmOperand = 0x0C,

   bcREOperand   = 0x03,
   bcION0        = 0x03,
   bcION1        = 0x07,
   bcIRN0        = 0x0B,
   bcIRN1        = 0x0F,
   bcExtOperand  = 0x07,
   bcSwapOperand = 0x0A,
   bcEmbOperand  = 0x0F,

   bcRoleOperand = 0x0E,

   bcExtraParam  = 0x100,

   bcAllocStack  = 0x101,
   bcFreeStack   = 0x102,  // meta command, used to indicate that the previous command release number of items from stack

   // labels:
   blBegin       = 0x110,
   blEnd         = 0x111,
   blFailure     = 0x112,
   blDeclare     = 0x113,

   // debug info
   bdBreakpoint  = 0x201,
   bdBreakCoord  = 0x202,
   bdLocal       = 0x203,

   // masks:
   bcCommand     = 0xFF0
};

enum LabelType
{
   bltNone      = 0,
   bltSymbol    = 1,
   bltClass     = 2,
   bltMethod    = 3,
   bltProc      = 4,
   bltBranch    = 5,
   bltRole      = 6,
   bltLoop      = 7
};

struct ByteCommand
{
   ByteCode code;
   int      argument;
   union {
      int       value; 
      LabelType type;
   } hint;

   LabelType Type() const { return hint.type; }

   int Hint() const { return hint.value; }

   operator ByteCode() const { return code; }

   ByteCommand()
   {
      code = bcNop;
   }
   ByteCommand(ByteCode code)
   {
      this->code = code;
      this->argument = 0;      
      this->hint.value = 0;
   }
   ByteCommand(ByteCode code, int argument)
   {
      this->code = code;
      this->argument = argument;
      this->hint.value = 0;
   }
   ByteCommand(ByteCode code, int argument, LabelType type)
   {
      this->code = code;
      this->argument = argument;
      this->hint.type = type;
   }
   ByteCommand(ByteCode code, int argument, int hint)
   {
      this->code = code;
      this->argument = argument;
      this->hint.value = hint;
   }

   void save(DumpWriter* writer, bool commandOnly = false)
   {
      writer->writeByte((unsigned char)code);
      if (!commandOnly && ((int)code & 0x3) != 0) {
         // Canonical little endian: byte code is a portable artifact and must
         // not carry the byte order of whichever host compiled it.
         writer->writeU32LE((unsigned int)argument);
      }
   }
};

// --- byte code introspection ---
//
// Returns NULL for a byte that is not a known opcode.
const char* getByteCodeName(unsigned char code);

// Number of 32-bit arguments that follow the opcode byte.
int getByteCodeArgCount(unsigned char code);

// --- CommandTape ---
typedef BList<ByteCommand>::Iterator ByteCodeIterator; 

struct CommandTape
{
   BList<ByteCommand> tape;   // !! should we better use an array?

   ByteCodeIterator start() { return tape.start(); }

   void write(ByteCode code);
   void write(ByteCode code, int argument);
   void write(ByteCode code, int argument, int hint);
   void write(ByteCode code, int argument, LabelType type);
   void write(ByteCommand command);

   void clear()
   {
      tape.clear();
   }
};

} // _ELENA_

#endif // bytecodeH
