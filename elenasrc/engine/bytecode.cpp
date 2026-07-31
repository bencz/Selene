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
