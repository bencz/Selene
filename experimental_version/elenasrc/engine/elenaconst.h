//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Compiler Engine
//
//		This file contains the common ELENA Engine constants
//
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef elenaconstH
#define elenaconstH 1

namespace _ELENA_
{
  // --- Common ELENA Engine constants ---
   #define ENGINE_MAJOR_VERSION     0x0005             // ELENA Engine version
   #define ENGINE_MINOR_VERSION     0x0000

   #define LINE_LEN                 0x1000              // the maximal source line length
   #define IDENTIFIER_LEN           0x0100              // the maximal identifier length

  // --- ELENA Standard module references ---
   #define STANDARD_MODULE          _T("$elena")        // the standard module name
   #define DLL_NAMESPACE            _T("$dlls")
   #define PACKAGE_MODULE           _T("$package")
   #define CORE_BINARY_MODULE       _T("elena")

   #define GC_TABLE                 _T("$elena'@gctable")
   #define GC_ROOT                  _T("$elena'@gcroot")             // static roots

   #define SUPER_CLASS              _T("$elena'object") // the common class predecessor
   #define STARTUP_CLASS            _T("'starter")      // the program starter
   #define NIL_CLASS                _T("$elena'$nil")   // the nil reference
   #define GROUP_CLASS              _T("$elena'$group") // the special group class
   #define CAST_CLASS               _T("$elena'$cast")  // the special group class
   #define TYPEINSTANCE_CLASS       _T("$elena'$typeinstance") // the type instance
   #define TYPE_CLASS               _T("$elena'type")   // the type property
   #define GUI_CLASS                _T("win32'system'gui")           // GUI helper class

   #define GROUP_SYMBOL             _T("group")
   #define CAST_SYMBOL              _T("cast")

   // core binary function references
   #define ALLOC_FUNCTION           _T("$package'elena'alloc")
   #define PREP_FUNCTION            _T("$package'elena'prep")
   #define SPREP_FUNCTION           _T("$package'elena'sprep")
   #define RETURN_FUNCTION          _T("$package'elena'return")
   #define IOCALLN_FUNCTION         _T("$package'elena'iocalln")
   #define SEXIT_FUNCTION           _T("$package'elena'sexit")
   #define SRETURN_FUNCTION         _T("$package'elena'sreturn")
   #define RRETURNIF_FUNCTION       _T("$package'elena'rreturnif")
   #define OCREATE_FUNCTION         _T("$package'elena'ocreate")
   #define OCREATE2_FUNCTION        _T("$package'elena'ocreate2")
   #define OCREATE4_FUNCTION        _T("$package'elena'ocreate4")
   #define OCREATE6_FUNCTION        _T("$package'elena'ocreate6")
   #define OCREATE0_FUNCTION        _T("$package'elena'ocreate0")
   #define CALLEXT_FUNCTION         _T("$package'elena'callext")
   #define PREPREDIR_FUNCTION       _T("$package'elena'prepredir")
   #define EXITREDIR_FUNCTION       _T("$package'elena'exitredir")
   #define REDIRECT_FUNCTION        _T("$package'elena'redirect")
   #define RREDIRECT_FUNCTION       _T("$package'elena'rredirect")
   #define GROUP_FUNCTION           _T("$package'elena'group")
   #define IOSWAP_FUNCTION          _T("$package'elena'ioswap")
   #define IOSET_FUNCTION           _T("$package'elena'ioset")
   #define SHIFT_FUNCTION           _T("$package'elena'shift")
   #define UNSHIFT_FUNCTION         _T("$package'elena'unshift")
   #define IRCALL_FUNCTION          _T("$package'elena'ircall")
   #define ASSIGN_FUNCTION          _T("$package'elena'assign")
   #define CAST_FUNCTION            _T("$package'elena'cast")
   #define WIND32PROC               _T("$package'elena'wndproc")

  // --- ELENA predefined messages ---
   #define FAIL_MESSAGE			      _T("fail")
   #define NEW_MESSAGE			      _T("new")
   #define PROCEED_MESSAGE		      _T("proceed")
   #define COPY_MESSAGE			      _T("<<")
   #define COPYTO_MESSAGE			   _T(">>")
   #define OPROCEED_MESSAGE		   _T("=>")

   #define NOTNIL_MESSAGE           _T("ifnotnil")
   #define OF_MESSAGE			      _T("of")
   #define IF_MESSAGE			      _T("?")
   #define IFNOT_MESSAGE		      _T("!")
   #define ADD_MESSAGE			      _T("+")
   #define SUB_MESSAGE			      _T("-")
   #define MUL_MESSAGE			      _T("*")
   #define DIV_MESSAGE			      _T("/")
   #define BIGGEREQ_MESSAGE		   _T(">=")
   #define SMALLEREQ_MESSAGE	      _T("<=")
   #define BIGGER_MESSAGE		      _T(">")
   #define SMALLER_MESSAGE		      _T("<")
   #define EQUAL_MESSAGE		      _T("==")
   #define NOTEQUAL_MESSAGE		   _T("!=")
   #define ADD2_MESSAGE			      _T("+=")
   #define SUB2_MESSAGE			      _T("-=")
   #define BACK_MESSAGE			      _T("back")
   #define RUN_MESSAGE		         _T("run")
   #define SAME_MESSAGE			      _T("ifsame")

   #define ANY_MESSAGE             _T("#any")

  // --- ELENA explicit messages ---
   #define REDIRECT_MESSAGE        _T("$invoke")                    // redirect message

  // --- ELENA Standart message constants ---
   #define MAXIMAL_MESSAGE_REF     0x0000FFFF

   #define PREDEFINED_REF          0x80000000
   #define TERMINAL_MESSAGE_ID     0x7FFFFFFF   // terminal message id should be never used

   #define VMT_INDEX_SIZE          4

   #define FAIL_MESSAGE_ID         0x80000000
   #define NEW_MESSAGE_ID          0x80000001

   #define DUMMY_MESSAGE_ID        0x80000002   // dummy message id, used only as a place holder

   #define PROCEED_MESSAGE_ID      0x80000003
   #define COPY_MESSAGE_ID         0x80000004
   #define COPYTO_MESSAGE_ID       0x80000005

   #define NOTNIL_MESSAGE_ID       0x80000006
   #define OF_MESSAGE_ID			  0x80000007
   #define IF_MESSAGE_ID			  0x80000008
   #define IFNOT_MESSAGE_ID		  0x80000009
   #define ADD_MESSAGE_ID			  0x8000000A
   #define SUB_MESSAGE_ID			  0x8000000B
   #define MUL_MESSAGE_ID			  0x8000000C
   #define DIV_MESSAGE_ID			  0x8000000D
   #define OPROCEED_MESSAGE_ID     0x8000000E
   #define BIGGEREQ_MESSAGE_ID     0x8000000F
   #define SMALLEREQ_MESSAGE_ID	  0x80000010
   #define BIGGER_MESSAGE_ID		  0x80000011
   #define SMALLER_MESSAGE_ID		  0x80000012
   #define EQUAL_MESSAGE_ID		  0x80000013
   #define NOTEQUAL_MESSAGE_ID     0x80000014
   #define ADD2_MESSAGE_ID			  0x80000015
   #define SUB2_MESSAGE_ID			  0x80000016
   #define BACK_MESSAGE_ID			  0x80000017
   #define RUN_MESSAGE_ID		     0x80000018
   #define SAME_MESSAGE_ID			  0x80000019

  // --- ELENA explicit variables ---
   #define SELF_VAR                _T("self")          // the current object group
   #define THIS_VAR                _T("$self")         // the current class instance
   #define SUPER_VAR               _T("super")         // the predecessor class

  //--------------------------------------------------------------------------
  // File extensions
  //
  // Named here rather than spelled as literals at each use, so that renaming
  // one is a single edit. The source extension in particular had to change:
  // ".l" is the canonical extension for Lex/Flex, so editors, GitHub Linguist
  // and build tools all misidentified Selene sources as lexer grammars.
  //--------------------------------------------------------------------------
   #define SOURCE_EXTENSION         _T("sel")          // Selene source
   #define PROJECT_EXTENSION        _T("prj")          // project file
   #define MODULE_EXTENSION         _T("sem")          // compiled module
   #define DEBUG_MODULE_EXTENSION   _T("sdm")          // per-module debug info
   #define DEBUG_FILE_EXTENSION     _T("sdi")          // linked debug info

  // --- ELENA Module structure constants ---
   #define ELENA_SIGNITURE          "ELENA.150"        // the language version
   #define MODULE_SIGNATURE         "EN!10"            // v1 magic (read-only, for diagnostics)
   #define DEBUG_MODULE_SIGNATURE   "EN.D10!"

  //--------------------------------------------------------------------------
  // ELENA module file (.nl) header, format v2
  //
  // The v1 format began with the 5 bytes "EN!10" and nothing else: no version
  // field, no record of character width, pointer size or byte order. Since the
  // reference maps are serialized as raw images of host structures, a module
  // written by a build with different properties was read as garbage, silently.
  // That is the defect this header exists to close.
  //
  // Layout -- 32 bytes. Every field is written and read byte by byte, so the
  // header itself is legible on any host regardless of byte order. (The payload
  // that follows is NOT yet byte-order independent; see the note below.)
  //
  //   offset size field
  //   0      8    magic          "ELENA2NL"
  //   8      2    formatVersion  u16 LE
  //   10     2    headerSize     u16 LE   -- lets later versions grow the header
  //   12     1    charEncoding   see mheEncoding
  //   13     1    byteOrder      see mheByteOrder
  //   14     1    wordBits       32 | 64  -- pointer width of the writing process
  //   15     1    flags          see mheFlags
  //   16     16   reserved       must be zero
  //
  // NOTE: recording byteOrder is detection, not portability. The payload maps
  // are still written in host order, so a big-endian writer produces a file a
  // little-endian reader cannot use -- but it will now be REJECTED with a clear
  // diagnostic instead of misread. Making the payload canonical little-endian
  // is the remaining half of format v2.
  //   See docs/plan/17-llvm-backend-and-targets.md section 3.
  //--------------------------------------------------------------------------

  // On-disk size of a serialized ClassHeader: three 32-bit fields.
  //
  // NOT sizeof(ClassHeader) -- that is 12 bytes under ILP32 and 24 under LP64,
  // and the file format must not follow the host. A VMT section is
  //   [u32 size][roleRef][flags][parentRef][u32 classSize][{u32 msg, u32 offset}...]
  // so entries begin CLASSHEADER_DISK_SIZE + 8 bytes into the section.
   #define CLASSHEADER_DISK_SIZE    12

   #define MODULE_MAGIC             "SELENE20"         // 8 bytes, no terminator
   #define MODULE_MAGIC_SIZE        8
   #define MODULE_HEADER_SIZE       32
   #define MODULE_FORMAT_VERSION    0x0200             // v2.0

   enum ModuleEncoding
   {
      mheEncodingUnknown = 0,
      mheEncodingUtf8    = 1,
      mheEncodingUtf16   = 2,
      mheEncodingUtf32   = 3
   };

   enum ModuleByteOrder
   {
      mheOrderUnknown = 0,
      mheOrderLittle  = 1,
      mheOrderBig     = 2
   };

   enum ModuleFlags
   {
      mhfNone      = 0x00,
      mhfDebugInfo = 0x01                              // a matching .dnl exists
   };

  // --- properties recorded in a module ---

   // The payload is written in canonical little endian throughout -- maps,
   // section contents, class records -- so this describes the FILE, not the
   // machine that produced it. It stays in the header so a non-conforming
   // writer is caught rather than silently misread.
   inline int getModuleByteOrder()
   {
      return mheOrderLittle;
   }

   // Derived from the character width rather than hard-coded, so it stays
   // correct through the migration from wchar_t to UTF-8 char.
   inline int getModuleEncoding()
   {
      switch (sizeof(TCHAR)) {
         case 1:  return mheEncodingUtf8;
         case 2:  return mheEncodingUtf16;
         case 4:  return mheEncodingUtf32;
         default: return mheEncodingUnknown;
      }
   }

  // --- ELENA class prefixes / postfixes ---
   #define INLINE_POSTFIX           _T("#inline")
   #define ROLE_POSTFIX             _T("#role")
   #define ROLETABLE_POSTFIX        _T("#roles")

  // --- ELENA hints ---
   #define HINT_CONSTANT           _T("const")

   // Foreign function declaration hints -- the in-language FFI surface:
   //   #define[external, lib:c, sym:write, args:i32'str'usize, ret:isize] write = c'write.
   // The apostrophes of the args reference stand in for commas, which the
   // hint grammar reserves as its separator. docs/plan/18-ffi-design.md
   #define HINT_EXTERNAL           _T("external")
   #define HINT_FFI_LIB            _T("lib")
   #define HINT_FFI_SYM            _T("sym")
   #define HINT_FFI_ARGS           _T("args")
   #define HINT_FFI_RET            _T("ret")
   #define HINT_DEBUG              _T("dbg")            // debugger watch hint
   #define HINT_DEFAULT            _T("def")            // $elena method hint

   #define HINT_DEBUG_INT          _T("int")
   #define HINT_DEBUG_LITERAL      _T("literal")
   #define HINT_DEBUG_ARRAY        _T("array")
   #define HINT_DEBUG_REAL         _T("real")
   #define HINT_DEBUG_LONG         _T("long")

  // --- ELENA Reference masks ---
   enum ReferenceType
   {
      // masks
      mskAnyRef              = 0xFF000000,
      mskImageMask           = 0xF0000000,
      mskSectionMask         = 0x70000000,
      mskNativeMask          = 0x08000000,

      mskRelativeRef         = 0x80000000,

      mskExternalRef         = 0x10000000,
      mskDataRef             = 0x20000000,
      mskCodeRef             = 0x40000000,
      mskStaticRef           = 0x60000000,

      mskNativeDataRef       = 0x28000000,
      mskNativeCodeRef       = 0x48000000,
      mskNativeStaticRef     = 0x68000000,

      mskSymbolRef           = 0x42000000,   // symbol code
      mskVMTRef              = 0x21000000,   // class VMT
      mskClassRef            = 0x41000000,   // class code

      mskMetaDataRef         = 0x24000000,   // meta data

      mskStaticConstRef      = 0x01000000,   // reference to static constant
      mskConstantRef         = 0x08000000,   // reference to constant
      mskLiteralRef          = 0x09000000,   // reference to constant literal
      mskInt32Ref            = 0x0A000000,   // reference to constant integer number
      mskRealRef             = 0x0C000000,   // reference to constant real number
      mskLinkerConstant      = 0x0D000000    // linker constant
   };

   // --- ELENA Debug symbol constants ---
   enum DebugSymbol
   {
      dsNone                      = 0x00,

      dsStep                      = 0x10,
      dsAtomicStep                = 0x18,    // "step into" is always treated as step over, used for external code
      dsProcedureStep             = 0x14,    // check the step result
      dsEOP                       = 0x11,    // end of procedure
      dsVirtualStep               = 0x12,    // virtual step
      dsVirtualEnd                = 0x13,    // virtual end of statement, debugger should skip it automatically

      dsSymbol                    = 0x20,
      dsClass                     = 0x30,
      dsBase                      = 0x40,
      dsField                     = 0x50,
      dsLocal                     = 0x60,
      dsProcedure                 = 0x70,
      dsEnd                       = 0x80,

      dsDebugMask                 = 0xF0
   };

   // predefined debug module sections
   #define DEBUG_LINEINFO_ID      (size_t)-1
   #define DEBUG_STRINGS_ID       (size_t)-2

   // --- LoadResult enum ---
   enum LoadResult
   {
      lrSuccessful = 0,
      lrNotFound,
      lrWrongVersion,
      lrWrongStructure,
      lrDuplicate,

      // Reported by the format v2 header check. Each one used to be a silent
      // misread producing corrupted output.
      lrIncompatibleEncoding,      // module written with a different char width
      lrIncompatibleByteOrder,     // module written on a host of opposite endianness
      lrIncompatibleWordSize       // module written by a 32/64-bit build, we are the other
   };

  // --- ELENA Subsystem Types ---
   enum ProjectType {
      ptLibrary = 0x0000,
      ptConsole = 0x0001,
      ptGUI     = 0x0002,
   };

   // --- ELENA Parse Table constants ---
   const int cnHashSize            = 0x0100;              // the parse table hash size
   const int cnTablePower          = 0x0010;
   const int cnTableKeyPower       = cnTablePower + 1;
   const int cnSyntaxPower         = 0x0008;

  // --- ELENA Class constants ---
   const int elEmptyObject         = 0x0008;           // an object header size
   const int elVMTOffset           = 0x000C;           // a VMT header size

   const int elAnyHandlerSize      = 0x0010;           // size of any handler VMT (2 entries)

  // --- ELENA VMT flags ---
   const int elStandartVMT         = 0x00000001;
   const int elInlineClass         = 0x00000002;
   const int elDynamicRole         = 0x00000004;
   const int elStructureRole       = 0x00000008;
   const int elRoleVMT             = 0x00000010;
   const int elVMTWithRoles        = 0x00000020;
   const int elVMTAnyHandler       = 0x00000040;
   const int elStateless           = 0x00000080;

   const int elDebugMask           = 0x000F0000;
   const int elDebugDWORD          = 0x00010000;
   const int elDebugReal64         = 0x00020000;
   const int elDebugLiteral        = 0x00030000;
   const int elDebugArray          = 0x00050000;
   const int elDebugQWORD          = 0x00060000;

  // --- ELENA Garbage Collection constants ---
   const int  gcPageSize           = 0x10;             // a heap page size constant
   const int  gcCollected		     = 0x40000000;
   const int  gcBinary			     = 0x80000000;

  // --- ELENA Linker constants ---
   const int lnGCSize              = 0x00000001;

  // ELENA run-time exceptions
   #define ELENA_ERR_OUTOF_MEMORY  0x190

} // _ELENA_

#endif // elenaconstH
