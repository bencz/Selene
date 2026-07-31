//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Compiler
//
//		This file contains the ELENA Source DFA implementation,
//
//                                              (C)2005-2008, by Alexei Rakov
//---------------------------------------------------------------------------


#ifndef DfaH
#define DfaH 1

namespace _ELENA_
{

const TCHAR dfaMaxChar        = 127;

// Map a character to a table column.
//
// TCHAR is signed in both configurations -- `char` under UTF-8 and `wchar_t`
// on Linux -- so any code point above 127 arrives here as a NEGATIVE value.
// The original clamp was `if (ch > dfaMaxChar) ch = dfaMaxChar;`, a signed
// comparison that never fires for such a value, and the table was then indexed
// with a negative subscript. With wchar_t that only mattered for source files
// above U+7FFF; with UTF-8 it happens on EVERY byte of EVERY multi-byte
// sequence, so any accented character read out of bounds.
//
// Masking to the actual width turns the value into its unsigned bit pattern
// before the comparison, so everything non-ASCII lands on column 127 -- the
// identifier column -- which is the behaviour the table was built for.
inline unsigned int dfaColumn(TCHAR ch)
{
   unsigned int code =
      (unsigned int)((unsigned long long)ch & ((1ULL << (8 * sizeof(TCHAR))) - 1));

   return (code > (unsigned int)dfaMaxChar) ? (unsigned int)dfaMaxChar : code;
}

// --- ELENA DFA ---

template <const TCHAR* DFA_table[23], TCHAR start, TCHAR whitespace, TCHAR backState, TCHAR lineComment, TCHAR comment> struct DFA
{
   TCHAR state;
   bool  back;

   bool isSkipState() const { return state == whitespace || state == lineComment || state == comment;}

   bool makeStep(TCHAR ch, TCHAR nextCh)
   {
      back = false;

      unsigned int column     = dfaColumn(ch);
      unsigned int nextColumn = dfaColumn(nextCh);

	   TCHAR next = DFA_table[state - start][column];
	   // !! HOT FIX: to deal with tailing dot after digit
	   if ((next >= start) && (DFA_table[next - start][nextColumn]==backState)) {
         back = true;

		   return true;
	   }
      state = next;

      return (state < start || DFA_table[state - start][nextColumn]==start);
   }

   void reset() { state = start; back = false; }

   DFA()
   {
      state = dfaStart;
	   back = false;
   }
};

} // _ELENA_

#endif // DfaH
