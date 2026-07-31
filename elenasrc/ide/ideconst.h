//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef ideconstH
#define ideconstH

#define EMPTY_STRING                            _T("")

// --- Common constants ---
#define APP_NAME                                _T("ELENA IDE 1.5.0")
#define APP_VERSION                             "Version 1.5.0.0"
#define EDITOR_NAME                             _T("ELENA IDE Editor")

#define DEFAULT_TEXT                            _T("Ctrl+O: open file; Ctrl+Shift+O: open project")
#define NOT_FOUND_TEXT                          _T("Search string not found")
#define REPLACE_TEXT                            _T("Replace this occurence?")

// --- WinAPI Window Classes ---
#define APP_WND_CLASS                           _T("ELENA APP CLASS")
#define EDIT_WND_CLASS                          _T("ELENA EDITOR CLASS")
#define VSPLTR_WND_CLASS                        _T("ELENA VSPLTR CLASS")
#define HSPLTR_WND_CLASS                        _T("ELENA HSPLTR CLASS")

// --- Cursors ---

#define CURSOR_TEXT                             0
#define CURSOR_ARROW                            1
#define CURSOR_SIZEWE						         2
#define CURSOR_SIZENS                           3

// --- Styles ---
#define SCHEME_COUNT                            2

#define STYLE_DEFAULT                           0
#define STYLE_KEYWORD                           1
#define STYLE_OPERATOR                          2
#define STYLE_SELECTION                         3
#define STYLE_TRACE_LINE                        4
#define STYLE_TRACE                             5
#define STYLE_MARGIN                            6
#define STYLE_COMMENT                           7
#define STYLE_STRING                            8
#define STYLE_NUMBER                            9
#define STYLE_ERROR_LINE                        10
#define STYLE_BREAKPOINT                        11
#define STYLE_MESSAGE                           12
#define STYLE_PROPERTY                          13 // !! obsolete
#define STYLE_HINT                              14
#define STYLE_HIGHLIGHTED_BRACKET               15
#define STYLE_MAX                               15

#define IDE_CHARSET_ANSI                        0
#define IDE_CHARSET_DEFAULT                     1

// --- Notification messages ---
#define IDE_EDITOR_CHANGED                      100
#define IDE_EDITOR_MARGINCLICKED                101
#define IDE_EDITOR_ROWCOUNT_CHANGED             102
#define IDE_DEBUGGER_STEP                       200
#define IDE_DEBUGGER_START                      201
#define IDE_DEBUGGER_STOP                       202
#define IDE_DEBUGGER_LOADMODULE                 203
#define IDE_DEBUGGER_CHECKPOINT                 204
#define IDM_DEBUGGER_EXCEPTION                  205
#define IDE_DEBUGGER_BREAK					         206
#define IDM_COMPILER_SUCCESSFUL                 300
#define IDM_COMPILER_UNSUCCESSFUL               301
#define IDM_COMPILER_WITHWARNING			         302
#define IDM_LAYOUT_CHANGED					         303

// --- Resources ---
#define	IDI_APP_ICON				               100

#define IDR_SEPARATOR                           0

#define	IDR_FILENEW                            201
#define	IDR_FILEOPEN                           202
#define	IDR_FILESAVE                           203
#define	IDR_SAVEALL                            204
#define	IDR_CLOSEFILE                          205
#define	IDR_CLOSEALL                           206
#define	IDR_CUT                                207
#define	IDR_COPY                               208
#define	IDR_PASTE                              209
#define	IDR_UNDO                               210
#define	IDR_REDO                               211
#define	IDR_RUN                                212
#define	IDR_STOP                               213
#define	IDR_STEPINTO						         214
#define	IDR_STEPOVER                           215
#define	IDR_GOTO                               216
#define	IDR_MAIN_MENU			                  1500
#define	IDR_IDE_ACCELERATORS		               1501

#define IDM                                     40000             // Menu constants

#define	IDM_FILE                               (IDM + 1000)
   #define	IDM_FILE_NEW                        (IDM_FILE + 1)
   #define  IDM_PROJECT_NEW                     (IDM_FILE + 2)
   #define	IDM_FILE_OPEN                       (IDM_FILE + 3)
   #define	IDM_PROJECT_OPEN                    (IDM_FILE + 4)
   #define	IDM_FILE_SAVE                       (IDM_FILE + 5)
   #define	IDM_FILE_SAVEAS                     (IDM_FILE + 6)
   #define	IDM_FILE_SAVEPROJECT                (IDM_FILE + 7)
   #define	IDM_FILE_SAVEALL                    (IDM_FILE + 8)
   #define	IDM_FILE_EXIT			   		      (IDM_FILE + 9)
   #define  IDM_FILE_CLOSE                      (IDM_FILE + 10)
   #define  IDM_FILE_CLOSEALL                   (IDM_FILE + 11)
   #define  IDM_FILE_CLOSEALLBUT                (IDM_FILE + 12)
   #define  IDM_PROJECT_CLOSE                   (IDM_FILE + 13)
   #define  IDM_RECENTFILES_CLEAR			      (IDM_FILE + 14)
   #define  IDM_RECENTPROJECTS_CLEAR		      (IDM_FILE + 15)
   #define  IDM_PROJECT_FILES0                  (IDM_FILE + 100)
   #define  IDM_PROJECT_FILES1                  (IDM_FILE + 101)
   #define  IDM_PROJECT_FILES2                  (IDM_FILE + 102)
   #define  IDM_PROJECT_FILES3                  (IDM_FILE + 103)
   #define  IDM_PROJECT_FILES4                  (IDM_FILE + 104)
   #define  IDM_PROJECT_FILES5                  (IDM_FILE + 105)
   #define  IDM_PROJECT_FILES6                  (IDM_FILE + 106)
   #define  IDM_PROJECT_FILES7                  (IDM_FILE + 107)
   #define  IDM_PROJECT_FILES8                  (IDM_FILE + 108)
   #define  IDM_PROJECT_FILES9                  (IDM_FILE + 109)
   #define  IDM_PROJECT_PROJECTS0               (IDM_FILE + 200)
   #define  IDM_PROJECT_PROJECTS1               (IDM_FILE + 201)
   #define  IDM_PROJECT_PROJECTS2               (IDM_FILE + 202)
   #define  IDM_PROJECT_PROJECTS3               (IDM_FILE + 203)
   #define  IDM_PROJECT_PROJECTS4               (IDM_FILE + 204)
   #define  IDM_PROJECT_PROJECTS5               (IDM_FILE + 205)
   #define  IDM_PROJECT_PROJECTS6               (IDM_FILE + 206)
   #define  IDM_PROJECT_PROJECTS7               (IDM_FILE + 207)
   #define  IDM_PROJECT_PROJECTS8               (IDM_FILE + 208)
   #define  IDM_PROJECT_PROJECTS9               (IDM_FILE + 209)

#define	IDM_EDIT                               (IDM + 2000)
   #define	IDM_EDIT_UNDO                       (IDM_EDIT + 1)
   #define	IDM_EDIT_REDO                       (IDM_EDIT + 2)
   #define	IDM_EDIT_COPY                       (IDM_EDIT + 3)
   #define	IDM_EDIT_PASTE                      (IDM_EDIT + 4)
   #define	IDM_EDIT_CUT                        (IDM_EDIT + 5)
   #define	IDM_EDIT_DELETE                     (IDM_EDIT + 6)
   #define  IDM_EDIT_SELECTALL				      (IDM_EDIT + 7)
   #define  IDM_EDIT_TRIM                       (IDM_EDIT + 8)
   #define  IDM_EDIT_ERASELINE                  (IDM_EDIT + 9)
   #define  IDM_EDIT_DUPLICATE                  (IDM_EDIT + 10)
   #define  IDM_EDIT_COMMENT                    (IDM_EDIT + 11)
   #define  IDM_EDIT_UNCOMMENT                  (IDM_EDIT + 12)
   #define  IDM_EDIT_INDENT                     (IDM_EDIT + 13)
   #define  IDM_EDIT_OUTDENT                    (IDM_EDIT + 14)
   #define  IDM_EDIT_SWAP                       (IDM_EDIT + 15)
   #define  IDM_EDIT_UPPERCASE                  (IDM_EDIT + 16)
   #define  IDM_EDIT_LOWERCASE                  (IDM_EDIT + 17)

#define IDM_VIEW                                (IDM + 3000)
   #define  IDM_VIEW_OUTPUT                     (IDM_VIEW + 1)
   #define  IDM_VIEW_WATCH                      (IDM_VIEW + 2)

#define	IDM_SEARCH                             (IDM + 4000)
   #define  IDM_SEARCH_FIND                     (IDM_SEARCH + 1)
   #define  IDM_SEARCH_FINDNEXT                 (IDM_SEARCH + 2)
   #define	IDM_SEARCH_GOTOLINE                 (IDM_SEARCH + 3)
   #define	IDM_SEARCH_REPLACE				      (IDM_SEARCH + 4)

#define IDM_PROJECT                             (IDM + 5000)
   #define IDM_PROJECT_INCLUDE                  (IDM_PROJECT + 1)
   #define IDM_PROJECT_EXCLUDE                  (IDM_PROJECT + 2)
   #define IDM_PROJECT_COMPILE                  (IDM_PROJECT + 3)
   #define IDM_PROJECT_FORWARDS                 (IDM_PROJECT + 4)
   #define IDM_PROJECT_OPTION                   (IDM_PROJECT + 5)
   #define IDM_PROJECT_CLEAN                    (IDM_PROJECT + 6)

#define IDM_DEBUG                               (IDM + 6000)
   #define IDM_DEBUG_RUN                        (IDM_DEBUG + 1)
   #define IDM_DEBUG_STEPOVER                   (IDM_DEBUG + 2)
   #define IDM_DEBUG_STEPINTO                   (IDM_DEBUG + 3)
   #define IDM_DEBUG_RUNTO                      (IDM_DEBUG + 4)
   #define IDM_DEBUG_BREAKPOINT                 (IDM_DEBUG + 5)
   #define IDM_DEBUG_CLEARBREAKPOINT            (IDM_DEBUG + 6)
   #define IDM_DEBUG_STOP                       (IDM_DEBUG + 7)
   #define IDM_DEBUG_INSPECT                    (IDM_DEBUG + 8)
   #define IDM_DEBUG_SWITCHHEXVIEW              (IDM_DEBUG + 9)
   #define IDM_DEBUG_GOTOSOURCE                 (IDM_DEBUG + 10)

#define IDM_TOOLS                               (IDM + 7000)
   #define IDM_EDITOR_OPTIONS                   (IDM_TOOLS + 1)

#define	IDM_WINDOW                             (IDM + 8000)
   #define IDM_WINDOW_NEXT                      (IDM_WINDOW + 1)
   #define IDM_WINDOW_PREVIOUS                  (IDM_WINDOW + 2)
   #define IDM_WINDOW_MRU_FIRST                 (IDM_WINDOW + 3)
   #define IDM_WINDOW_WINDOWS                   (IDM_WINDOW + 4)
   #define IDM_WINDOW_FIRST                     (IDM_WINDOW + 5)
   #define IDM_WINDOW_SECOND                    (IDM_WINDOW + 6)
   #define IDM_WINDOW_THIRD                     (IDM_WINDOW + 7)
   #define IDM_WINDOW_FOURTH                    (IDM_WINDOW + 8)
   #define IDM_WINDOW_FIFTH                     (IDM_WINDOW + 9)
   #define IDM_WINDOW_SIXTH                     (IDM_WINDOW + 10)
   #define IDM_WINDOW_SEVENTH                   (IDM_WINDOW + 11)
   #define IDM_WINDOW_EIGHTH                    (IDM_WINDOW + 12)
   #define IDM_WINDOW_NINTH                     (IDM_WINDOW + 13)
   #define IDM_WINDOW_TENTH                     (IDM_WINDOW + 14)

#define IDM_HELP                                (IDM + 9000)
   #define IDM_HELP_API                         (IDM_HELP + 1)
	#define IDM_HELP_ABOUT                       (IDM_HELP + 2)

#define IDM_WATCH                               (IDM + 10000)
   #define IDM_WATCH_OPEN                       (IDM_WATCH + 1)

#define IDD_SETTINGS                            500
#define IDC_SETTINGS_LABEL2                    (IDD_SETTINGS + 1)
#define IDC_SETTINGS_LABEL3                    (IDD_SETTINGS + 2)
#define IDC_SETTINGS_TARGET                    (IDD_SETTINGS + 3)
#define IDC_SETTINGS_ENTRY                     (IDD_SETTINGS + 4)
#define IDC_SETTINGS_PACKAGE                   (IDD_SETTINGS + 7)
#define IDC_SETTINGS_OUTPUT                    (IDD_SETTINGS + 8)
#define IDC_SETTINGS_DEBUG                     (IDD_SETTINGS + 9)
#define IDC_SETTINGS_TYPE                      (IDD_SETTINGS + 10)
#define IDC_SETTINGS_WARN_REF                  (IDD_SETTINGS + 11)
#define IDC_SETTINGS_TEPMPLATE                 (IDD_SETTINGS + 12)
#define IDC_SETTINGS_ARGUMENT                  (IDD_SETTINGS + 13)
#define IDC_SETTINGS_OPTIONS                   (IDD_SETTINGS + 14)

#define IDD_FORWARDS                            700
#define IDC_FORWARDS_LIST                       (IDD_FORWARDS + 1)
#define IDC_FORWARDS_EDIT                       (IDD_FORWARDS + 2)
#define IDC_FORWARDS_ADD                        (IDD_FORWARDS + 3)
#define IDC_FORWARDS_REPLACE                    (IDD_FORWARDS + 4)
#define IDC_FORWARDS_DELETE                     (IDD_FORWARDS + 5)
#define IDC_FORWARDS_LABEL1                     (IDD_FORWARDS + 8)
#define IDC_FORWARDS_SAVE                       (IDD_FORWARDS + 9)

#define IDD_GOTOLINE                            600
#define	IDC_GOTOLINE_LINENUMBER                (IDD_GOTOLINE + 3)
#define	IDC_GOTOLINE_LABEL1                    (IDD_GOTOLINE + 4)

#define IDD_WINDOWS                             800
#define IDC_WINDOWS_LIST                        (IDD_WINDOWS + 1)
#define IDC_WINDOWS_CLOSE                       (IDD_WINDOWS + 2)

#define IDD_EDITOR_SETTINGS                     900
#define IDC_EDITOR_LINENUMBERFLAG               (IDD_EDITOR_SETTINGS + 1)
#define IDC_EDITOR_COLORSCHEME                  (IDD_EDITOR_SETTINGS + 2)
#define IDC_EDITOR_USETAB                       (IDD_EDITOR_SETTINGS + 4)
#define IDC_EDITOR_TABSIZE                      (IDD_EDITOR_SETTINGS + 5)
#define IDC_EDITOR_HIGHLIGHSYNTAXFLAG           (IDD_EDITOR_SETTINGS + 6)
#define IDC_EDITOR_UNICODEFILES                 (IDD_EDITOR_SETTINGS + 7)
#define IDC_EDITOR_REMEMBERPATH                 (IDD_EDITOR_SETTINGS + 8)
#define IDC_EDITOR_REMEMBERPROJECT              (IDD_EDITOR_SETTINGS + 9)

#define IDD_EDITOR_FIND                         1000
#define IDC_FIND_TEXT                           (IDD_EDITOR_FIND + 4)
#define IDC_FIND_CASE                           (IDD_EDITOR_FIND + 5)
#define IDC_FIND_WHOLE                          (IDD_EDITOR_FIND + 6)
#define IDC_REPLACE_TEXT                        (IDD_EDITOR_FIND + 7)

#define IDD_EDITOR_REPLACE                      1100

#define IDD_ABOUT							            1200
#define IDC_ABOUT_HOME                          (IDD_ABOUT + 1)
#define IDC_ABOUT_LICENCE_TEXT                  (IDD_ABOUT + 2)

// --- Error Code Constants ---
#define ERR_WNDCLASS_NOT_REGISTERED             1
#define ERR_WND_NOT_OPENED                      2

#ifndef IDC_STATIC
#define IDC_STATIC (-1)
#endif

#define APACHE_LICENSE2 _T("The Apache License V2.0\r\n\r\nCopyright (C) 2005-2009 Alex Rakov\r\n\r\nLicensed under the Apache License, Version 2.0 (the \"License\"); you may not use this file except in compliance with the License. You may obtain a copy of the License at:\r\n\r\nhttp://www.apache.org/licenses/LICENSE-2.0\r\n\r\nUnless required by applicable law or agreed to in writing, software distributed under the License is distributed on an \"AS IS\" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.\r\n\r\nSee the license details in the file doc\\license.txt")
#define ELENA_HOMEPAGE _T("http://elenalang.sourceforge.net/")

// --- Debugger excepyopm texts ---

#define UNKNOWN_EXCEPTION_TEXT                  _T("Unknown exceptoin at address ")
#define ACCESS_VIOLATION_EXCEPTION_TEXT         _T("Access violation at address ")
#define ARRAY_BOUNDS_EXCEEDED_EXCEPTION_TEXT    _T("Array bounds exceeded at address ")
#define DATATYPE_MISALIGNMENT_EXCEPTION_TEXT    _T("Datatype misalignment at address ")
#define FLT_DENORMAL_OPERAND_EXCEPTION_TEXT     _T("Floating-point denormal operand at address ")
#define FLT_DIVIDE_BY_ZERO_EXCEPTION_TEXT       _T("Floating-point divide by zero at address ")
#define FLT_INEXACT_RESULT_EXCEPTION_TEXT       _T("Floating-point inexact result at address ")
#define FLT_INVALID_OPERATION_EXCEPTION_TEXT    _T("Floating-point invalid operation at address ")
#define FLT_OVERFLOW_EXCEPTION_TEXT             _T("Floating-point overflow at address ")
#define FLT_STACK_CHECK_EXCEPTION_TEXT          _T("Floating-point stack check at address ")
#define FLT_UNDERFLOW_EXCEPTION_TEXT            _T("Floating-point underflow at address ")
#define ILLEGAL_INSTRUCTION_EXCEPTION_TEXT      _T("Illegal instruction at address ")
#define PAGE_ERROR_EXCEPTION_TEXT               _T("Page error at address ")
#define INT_DIVIDE_BY_ZERO_EXCEPTION_TEXT       _T("Division by zero at address ")
#define INT_OVERFLOW_EXCEPTION_TEXT             _T("Range overflow at address ")
#define INVALID_DISPOSITION_EXCEPTION_TEXT      _T("Invalid disposition at address ")
#define NONCONTINUABLE_EXCEPTION_EXCEPTION_TEXT _T("Non continuabke exception at address ")
#define PRIV_INSTRUCTION_EXCEPTION_TEXT         _T("Private instruction at address ")
#define STACK_OVERFLOW_EXCEPTION_TEXT           _T("Stack overflow at address ")
#define GC_OUTOF_MEMORY_EXCEPTION_TEXT          _T("Out of memory at address ")

// --- IDE message constants ---

#define IDE_MSG_INVALID_PROJECT                 _T("Invalid project file ")

// --- Project settings ---

#define IDE_PROJECT_SECTION                     _T("project")
#define IDE_FILES_SECTION                       _T("files")
#define IDE_FORWARDS_SECTION                    _T("forwards")
#define IDE_LINKER_SECTION                      _T("linker")

#define IDE_ARGUMENT_SETTING                    _T("arguments")
#define IDE_PACKAGE_SETTING                     _T("package")
#define IDE_ENTRY_SETTING                       _T("entry")
#define IDE_EXECUTABLE_SETTING                  _T("executable")
#define IDE_OUTPUT_SETTING                      _T("output")
#define IDE_DEBUGINFO_SETTING                   _T("debuginfo")
#define IDE_TEMPLATE_SETTING                    _T("template")
#define IDE_TYPE_SETTING                        _T("projecttype")
#define IDE_COMPILER_OPTIONS                    _T("options")

// obsolete project settings
#define IDE_OLD_TYPE_SETTING                    _T("type")
#define IDE_OLD_TYPE_DEBUG                      _T("debug")

#endif // ideconstH
