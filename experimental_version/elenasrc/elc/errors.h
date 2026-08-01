//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Compiler
//
//		This file contains the ELENA Compiler error messages
//
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef jeterrorsH
#define jeterrorsH 1

namespace _ELENA_
{
  // --- Parser error messages ---
   #define errLineTooLong           _T("%s(%d): error 001: Line too long\n")
   #define errInvalidChar           _T("%s(%d:%d): error 002: Invalid char %c\n")
   #define errInvalidSyntax         _T("%s(%d:%d): error 004: Invalid syntax near '%s'\n")
   #define errDotExpectedSyntax     _T("%s(%d:%d): error 005: '.' expected\n")
   #define errCBrExpectedSyntax     _T("%s(%d:%d): error 006: ')' expected\n")
   #define errOBrExpectedSyntax     _T("%s(%d:%d): error 007: '(' expected\n")
   #define errOActionExpectedSyntax _T("%s(%d:%d): error 008: '(' or '[' expected\n")
   #define errCSBrExpectedSyntax    _T("%s(%d:%d): error 009: ']' expected\n")
   #define errCBraceExpectedSyntax  _T("%s(%d:%d): error 010: '}' expected\n")
   #define errVarNameExpectedSyntax _T("%s(%d:%d): error 011: public or private identifier expected\n")
   #define errExtensionNotAllowed   _T("%s(%d:%d): error 012: role cannot have an extension\n")

  // --- Compiler error messages ---
   #define errDuplicatedSymbol	   _T("%s(%d:%d): error 102: Class '%s' already exists\n")
   #define errDuplicatedMethod      _T("%s(%d:%d): error 103: Method '%s' already exists in the class\n")
   #define errUnknownClass	         _T("%s(%d:%d): error 104: Class '%s' doesn't exists\n")
   #define errDuplicatedLocal       _T("%s(%d:%d): error 105: Variable '%s' already exists\n")
   #define errUnknownObject         _T("%s(%d:%d): error 106: Unknown object '%s'\n")
   #define errInvalidOperation	   _T("%s(%d:%d): error 107: Invalid operation with '%s'\n")
   #define errDuplicatedField       _T("%s(%d:%d): error 109: Field '%s' already exists in the class\n")
   #define errIllegalField          _T("%s(%d:%d): error 111: Illegal field declaration '%s'\n")
   #define errTooManyParameters     _T("%s(%d:%d): error 113: Too many parameters for embedded function '%s'\n")
   #define errUnknownRole           _T("%s(%d:%d): error 117: Unknown role '%s'\n")
   #define errInvalidShift          _T("%s(%d:%d): error 118: '#shift<>' statement should be used only in roles\n")
   #define errDuplicatedDefinition  _T("%s(%d:%d): error 119: Duplicate definition '%s' already exists\n")
   #define errInvalidProperty       _T("%s(%d:%d): error 121: Invalid or none-existing property '%s'\n")
   #define errExtTooManyParameters  _T("%s(%d:%d): error 124: Too many parameters for external function %s\n")
   #define errInvalidRedirectMessage _T("%s(%d:%d): error 127: It is not possible to use redirect message in this case\n")
   #define errInvalidIntNumber      _T("%s(%d:%d): error 130: Invalid integer value %s\n")   

  // --- Linker error messages ---
   #define errUnknownModule         _T("linker: error 201: Unknown module '%s'\n")
   #define errUnresovableLink       _T("linker: error 202: Link '%s' is not resolved\n")
   #define errInvalidModule	      _T("linker: error 203: Invalid module file '%s'\n")
   #define errCannotCreate	         _T("linker: error 204: Cannot create a file '%s'\n")
   #define errInvalidFile           _T("linker: error 205: Invalid file '%s'\n")
   #define errDuplicatedModule      _T("linker: error 208: Module '%s' already exists in the project\n")
   #define errInvalidModuleVersion  _T("linker: error 210: Obsolete module file '%s'\n")
   #define errModuleEncoding        _T("linker: error 211: Module '%s' was built with a different character width; rebuild it\n")
   #define errModuleByteOrder       _T("linker: error 212: Module '%s' was built on a host of opposite byte order; rebuild it\n")
   #define errModuleWordSize        _T("linker: error 213: Module '%s' was built by a 32/64-bit compiler mismatch; rebuild it\n")

  // --- Compiler internal error messages ---
   #define errReferenceOverflow     _T("error 301: The section reference overflow\n")

  // --- Compiler warnings ---
   #define wrnUnresovableLink       TEXT("%s(%d:%d): warning 401: Link %s is unresolvable\n")
   #define wrnUnknownHint           TEXT("%s(%d:%d): warning 404: Unknown hint '%s'\n")
   #define wrnUnknownHintValue      TEXT("%s(%d:%d): warning 405: Unknown class hint value '%s'\n")
   #define wrnInvalidHint           TEXT("%s(%d:%d): warning 406: Hint '%s' cannot be applied here\n")

} // _ELENA_

#endif // jeterrors
