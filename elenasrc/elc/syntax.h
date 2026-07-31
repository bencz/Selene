//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Compiler
//
//		This file contains ELENA Parser Symbol constants
//
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef syntaxH
#define syntaxH 1

namespace _ELENA_
{

// --- ELENA Parser Symbol constants ---
enum Symbol
{
   mskAnySymbolMask             = 0x10500,               // masks
   mskTerminal                  = 0x10000,
   mskTraceble                  = 0x00100,
   mskError                     = 0x00400,

   nsNone                       = 0x00000,               // defaults
   nsStart                      = 0x00001,
   nsEps                        = 0x00002,

   tsEof                        = 0x10003,               // terminals
   tsLiteral                    = 0x10004,
   tsIdentifier                 = 0x10005,
   tsPrivate                    = 0x10006,
   tsReference                  = 0x10007,
   tsInteger                    = 0x10008,
   tsHexInteger                 = 0x10009,
   tsReal                       = 0x1000A,
   tsProtected                  = 0x1000D,
   tsWildcard                   = 0x1000C,

   nsShortcut                   = 0x0010E,               // non-terminals
   nsShortcutReference          = 0x00110,
   nsNSShortcutReference        = 0x00112,
   nsClass                      = 0x00114,
   nsSymbol                     = 0x00116,
   nsStatic                     = 0x00118,
   nsCollectionSymbol           = 0x0011A,
   nsObjectExpression           = 0x0011C,
   nsActionExpression           = 0x0011E,
   nsExternalExpression         = 0x00120,
   nsEmbeddedExpression         = 0x00122,
   nsInlineSymbol               = 0x00126,
   nsExtension                  = 0x0012A,
   nsExpression                 = 0x00132,
   nsMethod                     = 0x00136,
   nsMethodArgument             = 0x00138,
   nsVariable                   = 0x0013C,
   nsObject                     = 0x0013E,
   nsSubCode                    = 0x00140,
   nsBaseClass                  = 0x00142,
   nsField                      = 0x00144,
   nsSize                       = 0x00146,
   nsRole                       = 0x0014A,
   nsAssigning                  = 0x0014C,
   nsRetStatement               = 0x0014E,
   nsLoop                       = 0x00150,
   nsShift                      = 0x00152,
   nsCodeEnd                    = 0x00154,
   nsShiftParam                 = 0x00156,
   nsAlternative                = 0x00158,
   nsSymbolParameter            = 0x0015E,
   nsExtend                     = 0x00160,
   nsControl                    = 0x00162,
   nsHint                       = 0x00164,
   nsHintValue                  = 0x0016C,
   nsL0Operation                = 0x0016E,
   nsL1Operation                = 0x00172,
   nsL2Operation                = 0x00174,
   nsL3Operation                = 0x00176,
   nsL4Operation                = 0x00178,
   nsL0Expression               = 0x0017A,
   nsL1Expression               = 0x0017E,
   nsL2Expression               = 0x00180,
   nsL3Expression               = 0x00182,
   nsL4Expression               = 0x00184,
   nsGroup                      = 0x00186,
   nsType                       = 0x00188,
   nsCast                       = 0x0018A,

   nsErrDotExpected               = 0x00400,               // error-terminals
   nsFieldErrDotExpected          = 0x00401,
   nsErrClosingBracketExpected    = 0x00402,
   nsErrClosingBracketExpected2   = 0x00403,
   nsErrOpenBracketExpected       = 0x00404,
   nsErrOpenActionBracketExpected = 0x00405,
   nsErrClosingSBracketExpected   = 0x00406,               // closing square bracket expected
   nsErrClosingSBracketExpected2  = 0x00407,               
   nsErrClosingBraceExpected      = 0x00408,
   nsErrVariableNameExpected      = 0x00409,
   nsErrClosingSBracketExpected3  = 0x0040A,
   nsErrVariableNameExpected2     = 0x0040B,
   nsErrExtensionNotExpected      = 0x0040C,
};

} // _ELENA_

#endif // syntaxH
