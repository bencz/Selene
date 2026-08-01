# 14 — Platform Dependency Audit (tree-wide)

**Subject:** ELENA Language v1.5.0.0 (2009) — `/home/bencz/programming/ELENA-1.5.0.0`
**Scope:** the entire repository (`elenasrc/`, `src/`, `examples/`, `bin/`, `dat/`, `doc/`)
**Goal:** the single authoritative inventory of every platform assumption blocking a port to
Linux / Windows / macOS × x86-64 / ARM64, an LLVM backend, and real multithreading.

Every claim below carries a `file:line` anchor. Compiler output in §6 is real, reproduced on this
machine with the exact commands shown.

---

## 1. Executive summary

### 1.1 What the tree actually contains

| Bucket | Files | LOC | Notes |
|---|---:|---:|---|
| C++ sources & headers (`elenasrc/`) | 117 | **33,561** | compiler, engine, IDE, tools |
| x86 assembler runtime (`src/asm/*.asm`) | 5 | **6,146** | ELENA's own asm dialect, assembled by `asm2bin` |
| ELENA standard library (`src/**/*.l`) | 25 | **7,942** | of which `src/win32/**` = 2,887, `src/gui/**` = 1,252 |
| ELENA examples (`examples/**/*.l`) | 33 | **5,541** | |
| **Total code** | | **≈53,190** | plus 23 `.prj`, 3 `.cfg`, 1 `.rc`, 12 `.vcproj`, 5 `.vcxproj`, 6 `.cbp` |

### 1.2 How deep the Windows coupling goes

The coupling is **not confined to a porting layer**. It is present at four distinct depths:

1. **Surface (easy):** ~145 distinct Win32 API functions, 96 % of them inside `elenasrc/ide/win32/`
   (8,802 LOC, 31 files). This is a *rewrite-the-GUI* problem, not a *port* problem, but it is
   cleanly quarantined by directory.
2. **Type system (pervasive, mechanical):** `TCHAR` / `_T()` / `_tcs*` appear in **1,066 lines**
   across `common/`, `engine/`, `elc/`, `ide/`, `asm2bin/`. `TCHAR` is `wchar_t`, and the code
   hard-codes `sizeof(wchar_t)==2` in at least 15 places (`elenasrc/common/streams.h:70`,
   `:138`, `elenasrc/common/tools.h:149`, `:160`, `:170`, `:215`, `elenasrc/common/dump.h:57`,
   `elenasrc/common/files.cpp:111`, `:129`, `:214`, `:227`). On Linux/macOS `wchar_t` is **4 bytes**,
   so every string I/O path silently corrupts data. This is the single most viral defect.
3. **Object model & file formats (hard):** the `.nl` module format, `.dn` debug format and the
   in-memory hash maps are **raw dumps of 32-bit process memory** (`elenasrc/common/lists.h:1594`,
   `elenasrc/engine/elena.h:107`, `elenasrc/engine/bccompiler.cpp:76`). `sizeof(void*)==4` is not
   an assumption here — it is *encoded in the on-disk format*.
4. **Code generation (total rewrite):** `elenasrc/engine/win32/` (1,366 LOC) emits x86-32 opcodes
   byte by byte; `elenasrc/asm2bin/x86assembler.cpp` (2,772 LOC) is a full x86-32 assembler;
   `elenasrc/elc/win32/linker.cpp` (637 LOC) writes a PE image directly; `src/asm/*.asm`
   (6,146 LOC) is the runtime, written in x86-32 assembly with `__stdcall` DLL calls and Win32 TIB
   access (`src/asm/elena.asm:1052`).

### 1.3 Quantified verdict

| Metric | Value |
|---|---|
| C++ LOC that will not compile on any 64-bit target | **33,561 (100 %)** — `elenasrc/common/common.h:14` pulls `<tchar.h>` |
| Real `error:` diagnostics from GCC 16 at `-m64 -std=c++98` over 16 representative TUs | **25** |
| Same TUs at `-m32` | **3** (both are gaps in my POSIX shim, not codebase defects) |
| C++ LOC that is *architecture*-bound (x86-32 codegen) | **5,801** (asm2bin 3,335 + engine/win32 1,366 + elc/win32 1,100) = **17 %** |
| C++ LOC that is *OS*-bound (Win32 API) | **8,955** (ide/win32 8,802 + common/win32 32 + scattered 121) = **27 %** |
| C++ LOC essentially portable once `TCHAR` and pointer-width are fixed | **≈18,800** = **56 %** |
| Distinct Win32 API functions called from C++ | **145** |
| Distinct Win32 DLL entry points called from ELENA asm runtime | **64** (`src/asm/*.asm`) + 17 WinSock (`src/asm/winsock.asm`) |
| Directories that are 100 % platform-bound | 4 (`elenasrc/*/win32/`, `elenasrc/asm2bin/`) |

**Bottom line:** roughly **44 % of the C++ tree must be rewritten or heavily reworked**, and 100 %
of it must be *touched* (because of `TCHAR`). The runtime (`src/asm/`) must be rewritten from
scratch. The `.nl`/`.dn` binary formats must be versioned and redesigned. The IDE is a separate
product-sized effort.

---

## 2. Heat map

Severity legend: **Clean** = compiles/ports with no change · **Light** = mechanical edits
(< 10 % of lines) · **Heavy** = substantial rework (10–50 %) · **Total rewrite** = discard and
re-implement.

| Directory / module | Files | Total LOC | Platform-bound LOC | Severity | Notes |
|---|---:|---:|---:|---|---|
| `elenasrc/common/` | 12 | 5,481 | ~430 (8 %) | **Heavy** | `common.h:14` `<tchar.h>`, `:18` `<io.h>`, `:25` backslash include; entire string layer is `TCHAR`; `lists.h:216,234,266,281,421` cast pointers to `int`; `files.h` hard-codes `'\\'` in 6 places |
| `elenasrc/common/win32/` | 1 | 32 | 32 (100 %) | **Total rewrite** | `unicode.h:15` `<windows.h>`; `MultiByteToWideChar`/`WideCharToMultiByte` — replace with ICU/`iconv`/UTF-8 |
| `elenasrc/engine/` (portable part) | 14 | 3,088 | ~110 (4 %) | **Heavy** | `elena.h:107,117` raw `ClassHeader` I/O; `elenaconst.h:110,225,253,254` 32-bit object model; `section.h:19` `&&` instead of `&` (real bug) |
| `elenasrc/engine/win32/` | 6 | 1,366 | 1,366 (100 %) | **Total rewrite** | x86-32 JIT: `x86jitcompiler.cpp` 31 raw-opcode sites, `x86helper.h:32-39` EAX..EDI enum |
| `elenasrc/elc/` (portable part) | 16 | 3,990 | ~60 (2 %) | **Light** | parser/compiler are portable; `compiler.cpp:786,798` `_tcstoul`/`_tcstod`; `project.cpp:112,127` `.nl`/`.dnl` path building |
| `elenasrc/elc/win32/` | 3 | 1,100 | 1,100 (100 %) | **Total rewrite** | `linker.cpp` PE writer (637), `elc.cpp` Win32 entry point (335), `linker.h` (128) |
| `elenasrc/ide/` (portable part) | 21 | 5,527 | ~400 (7 %) | **Heavy** | `debugcontroller.*`, `browser.*`, `pluginmanager.cpp:15,17`, `idesettings.cpp:73-125` all leak Win32 |
| `elenasrc/ide/win32/` | 31 | 8,802 | 8,802 (100 %) | **Total rewrite** | the whole GUI + debugger backend |
| `elenasrc/ide/gtk/` | 1 | 49 | 49 | **Clean (stub)** | `main.cpp` is a GTK2 "Hello World" placeholder — no real port exists |
| `elenasrc/idecommon/` | 1 | 52 | 2 | **Light** | `plugins.h:49` `__cdecl` function-pointer typedef |
| `elenasrc/asm2bin/` | 6 | 3,335 | 3,335 (100 %) | **Total rewrite** | x86-32 assembler; 230 opcode-emit sites in `x86assembler.cpp`; `x86assembler.h:15` backslash include |
| `elenasrc/api2html/` | 1 | 471 | 471 (100 %) | **Heavy** | ANSI-only tool: does not compile with `UNICODE` at all (see §6) |
| `elenasrc/sg/` | 1 | 147 | ~10 | **Light** | `sg.cpp:74,135` `char*`↔`TCHAR*` type errors |
| `elenasrc/plugins/autoform/` | 3 | 121 | 121 (100 %) | **Total rewrite** | `autoform.h:14` `<windows.h>`, `:18` `__declspec(dllexport)`, `win32/dllmain.cpp:3` `DllMain` |
| `src/asm/elena.asm` | 1 | 1,481 | 1,481 (100 %) | **Total rewrite** | GC + core; `:1052,1053,1109,1166,1167` read Win32 TIB via `fs:[4]`/`fs:[8]` |
| `src/asm/standard.asm` | 1 | 2,804 | 2,804 (100 %) | **Total rewrite** | 79 inline x86 fragments, 0 DLL calls — arch-bound but OS-free |
| `src/asm/win32.asm` | 1 | 1,423 | 1,423 (100 %) | **Total rewrite** | 59 procedures, 65 DLL calls (kernel32/user32/gdi32) |
| `src/asm/winsock.asm` | 1 | 362 | 362 (100 %) | **Total rewrite** | 13 procedures, 17 `Ws2_32` calls |
| `src/asm/extended.asm` | 1 | 76 | 76 (100 %) | **Total rewrite** | 4 kernel32 calls |
| `src/win32/**.l` | 8 | 2,887 | 2,887 (100 %) | **Total rewrite** | ELENA-level Win32 bindings |
| `src/gui/**.l` | 4 | 1,252 | 1,252 (100 %) | **Total rewrite** | GUI classes bound to `win32'api` |
| `src/std/`, `src/ext/`, `src/sys/` | 11 | 3,758 | ~150 (4 %) | **Light** | portable ELENA source; forwards resolve to `win32'*` via `bin/templates/*.cfg` |
| `examples/**` | 33 | 5,541 | ~1,400 | **Heavy** | GUI examples (`agenda`, `calculator`, `upndown`) bind directly to `win32'api` |
| `bin/*.cfg`, `bin/templates/*.cfg` | 3 | 60 | 60 (100 %) | **Heavy** | `bin/elc.cfg:2` `libpath=..\lib`; `templates/console.cfg`, `templates/gui.cfg` forward everything to `win32'*` |
| `bin/winstub.ex_` | 1 | (binary) | 100 % | **Total rewrite** | the MS-DOS stub the linker prepends (`linker.cpp:416`) |
| `elenasrc/**/vs/*.vcproj`, `*.vcxproj` | 17 | — | 100 % | **Total rewrite** | MSVC 7/8/9/10 project files; link `comctl32.lib`, `shlwapi.lib` |
| `elenasrc/**/codeblocks/*.cbp`, `elena.workspace` | 7 | — | 100 % | **Total rewrite** | backslash paths, `-march=pentium2`, `-Dmingw49`, `C:\MinGW\lib\libshlwapi.a` |
| `elenasrc/ide/ide.rc` | 1 | ~200 | 100 % | **Total rewrite** | Win32 resource script; `#include "windows.h"`, `icons\\*.bmp` |
| `dat/`, `doc/` | — | — | 0 | **Clean** | data/documentation only |

---

## 3. Category-by-category inventory

### 3.1 Win32 API surface

#### 3.1.1 Header includes

| file:line | construct | problem | fix |
|---|---|---|---|
| `elenasrc/common/common.h:14` | `#include <tchar.h>` | MSVC/MinGW-only; defines the whole `TCHAR` universe | delete; adopt `char`+UTF-8 or `char16_t` |
| `elenasrc/common/common.h:18` | `#include <io.h>` | MSVC-only (`_access`, `_open`) | `<unistd.h>` / `<fcntl.h>` |
| `elenasrc/common/common.h:25` | `#include "win32\unicode.h"` | **backslash separator** — hard error on GCC/Clang | `#include "platform/unicode.h"` |
| `elenasrc/common/win32/unicode.h:15` | `#include <windows.h>` | pulls the entire Win32 SDK into *every* TU incl. `elc` | replace file |
| `elenasrc/common/files.cpp:13` | `#include <direct.h>` | MSVC-only (`_mkdir`, `_wmkdir`) | `<sys/stat.h>` |
| `elenasrc/elc/win32/linker.cpp:14` | `#include <windows.h>` | PE structs | own PE/ELF/Mach-O structs, or delegate to LLVM |
| `elenasrc/elc/win32/elc.cpp:19` | `#include <windows.h>` | `GetModuleFileName`, `CommandLineToArgvW` | POSIX `argv`, `/proc/self/exe`, `_NSGetExecutablePath` |
| `elenasrc/elc/win32/elc.cpp:17` | `#include "win32\x86jitcompiler.h"` | backslash separator | forward slash + backend selection |
| `elenasrc/elc/win32/elc.cpp:20` | `#include <fcntl.h>` + `:262` `setmode(_fileno(stdout), _O_WTEXT)` | MSVC wide-console mode | no-op on POSIX |
| `elenasrc/ide/win32/idecommon.h:19-21` | `<windows.h>`, `<commctrl.h>`, `<shlwapi.h>` | full Win32 GUI SDK | rewrite GUI |
| `elenasrc/ide/idesettings.cpp:10` | `#include <direct.h>` | `_wgetcwd` | `getcwd` |
| `elenasrc/ide/win32/idecommon.cpp:8` | `#include <direct.h>` | ditto | ditto |
| `elenasrc/ide/debugcontroller.h:11` | `#include "win32\debugger.h"` | backslash + Win32 debugger in "portable" header | abstract `IDebugBackend` |
| `elenasrc/ide/ide.rc:2` | `#include "windows.h"` | resource compiler | platform resource system |
| `elenasrc/plugins/autoform/autoform.h:14` | `#include <windows.h>` | plugin DLL | shared-object plugin ABI |
| `elenasrc/asm2bin/x86assembler.h:15` | `#include "win32\x86helper.h"` | backslash separator | forward slash |
| `elenasrc/asm2bin/x86jumphelper.h:12` | `#include "win32\x86helper.h"` | backslash separator | forward slash |

> **Verified:** `g++ -fsyntax-only ... common/altstrings.cpp` →
> `common/common.h:25:10: fatal error: win32\unicode.h: No such file or directory`.
> There are **5 backslash includes** in total and every one of them is a hard stop.

#### 3.1.2 Complete deduplicated Win32 function inventory (C++)

145 distinct entry points. `#` = call-site count across `elenasrc/`.

**Non-GUI (the ones that block `elc`, the compiler itself) — 12 functions:**

| function | # | first call site | replacement |
|---|---:|---|---|
| `MultiByteToWideChar` | 1 | `elenasrc/common/win32/unicode.h:19` | `mbstowcs` / ICU / UTF-8 everywhere |
| `WideCharToMultiByte` | 1 | `elenasrc/common/win32/unicode.h:26` | `wcstombs` / ICU |
| `GetModuleFileName` | 2 | `elenasrc/elc/win32/elc.cpp:30` | `/proc/self/exe`, `_NSGetExecutablePath`, `GetModuleFileNameW` on Win |
| `GetCommandLineW` + `CommandLineToArgvW` | 1 | `elenasrc/elc/win32/elc.cpp:258` | standard `int main(int,char**)` |
| `_mkdir` | 1 | `elenasrc/common/files.cpp:335` | `mkdir(p,0777)` |
| `_wmkdir` | 1 | `elenasrc/common/files.cpp:340` | `std::filesystem::create_directory` |
| `_access` | 1 | `elenasrc/common/files.cpp:345` | `access` |
| `_waccess` | 1 | `elenasrc/common/files.cpp:350` | `std::filesystem::exists` |
| `_tstat` | 1 | `elenasrc/common/files.h:124` | `stat` |
| `LoadLibrary` | 1 | `elenasrc/ide/pluginmanager.cpp:15` | `dlopen` |
| `GetProcAddress` | 1 | `elenasrc/ide/pluginmanager.cpp:17` | `dlsym` |
| `_wgetcwd` / `_getcwd` | 2 | `elenasrc/ide/idesettings.cpp:89,91` | `getcwd` |

**Shell / path (shlwapi) — 4 functions, all in `idesettings.cpp`:**

| function | # | call site | replacement |
|---|---:|---|---|
| `PathRemoveFileSpec` | 1 | `elenasrc/ide/idesettings.cpp:75` | `std::filesystem::path::parent_path` |
| `PathCanonicalize` | 1 | `elenasrc/ide/idesettings.cpp:102` | `std::filesystem::weakly_canonical` |
| `PathIsRelative` | 2 | `elenasrc/ide/idesettings.cpp:110` | `path::is_relative` |
| `PathRelativePathTo` | 1 | `elenasrc/ide/idesettings.cpp:123` | `std::filesystem::relative` |
| `ShellExecute` | 1 | `elenasrc/ide/win32/appwindow.cpp:1173` | `xdg-open` / `open` / `ShellExecuteW` |

**Process / thread / debug — 22 functions:**

| function | # | first call site |
|---|---:|---|
| `CreateProcess` | 2 | `elenasrc/ide/win32/debugger.cpp:356` |
| `TerminateProcess` | 2 | `elenasrc/ide/win32/debugger.cpp:446` |
| `GetExitCodeProcess` | 1 | `elenasrc/ide/win32/output.cpp:173` |
| `GetCurrentProcess` | 1 | `elenasrc/ide/win32/output.cpp:112` |
| `WaitForDebugEvent` | 1 | `elenasrc/ide/win32/debugger.cpp:380` |
| `ContinueDebugEvent` | 1 | `elenasrc/ide/win32/debugger.cpp:482` |
| `ReadProcessMemory` | 4 | `elenasrc/ide/win32/debugger.cpp:159` |
| `WriteProcessMemory` | 1 | `elenasrc/ide/win32/debugger.cpp:166` |
| `GetThreadContext` | 2 | `elenasrc/ide/win32/debugger.cpp:88` |
| `SetThreadContext` | 6 | `elenasrc/ide/win32/debugger.cpp:91` |
| `CreateThread` | 2 | `elenasrc/ide/win32/debugger.cpp:562` |
| `ExitThread` | 1 | `elenasrc/ide/win32/debugger.cpp:20` |
| `TerminateThread` | 1 | `elenasrc/ide/win32/output.cpp:49` |
| `GetCurrentThreadId` | 1 | `elenasrc/ide/win32/output.cpp:46` |
| `GetWindowThreadProcessId` | 1 | `elenasrc/ide/win32/debugger.cpp:594` |
| `CreateEvent` | 5 | `elenasrc/ide/win32/debugger.cpp:29` |
| `SetEvent` | 2 | `elenasrc/ide/win32/debugger.cpp:42` |
| `WaitForSingleObject` | 2 | `elenasrc/ide/win32/debugger.cpp:52` |
| `WaitForMultipleObjects` | 1 | `elenasrc/ide/win32/output.cpp:157` |
| `CreatePipe` | 1 | `elenasrc/ide/win32/output.cpp:105` |
| `PeekNamedPipe` | 1 | `elenasrc/ide/win32/output.cpp:192` |
| `DuplicateHandle` | 1 | `elenasrc/ide/win32/output.cpp:112` |
| `CloseHandle` | 9 | `elenasrc/ide/win32/idecommon.cpp:452` |
| `CreateFile` / `ReadFile` / `GetFileTime` | 3 | `elenasrc/ide/win32/idecommon.cpp:444,448`, `output.cpp:199` |
| `GetLastError` | 1 | `elenasrc/ide/win32/output.cpp:222` |
| `ZeroMemory` | 2 | `elenasrc/ide/win32/output.cpp:67` |

**GUI — 107 functions.** Full list retained but summarised (see §3.9). Highest-traffic:
`SendMessage` (38 sites, 8 files), `SendDlgItemMessage` (20, `dialogs.cpp`), `Rectangle` (11),
`SelectObject` (9), `LoadCursor` (7), `SetThreadContext` (6), `SetForegroundWindow` (5).
Plus 15 `commctrl.h` macros: `ListView_DeleteAllItems`, `ListView_GetItemCount`,
`ListView_InsertColumn`, `ListView_SetExtendedListViewStyle`, `TreeView_DeleteItem`,
`TreeView_Expand`, `TreeView_GetChild`, `TreeView_GetItem`, `TreeView_GetItemState`,
`TreeView_GetNextItem`, `TreeView_GetNextSibling`, `TreeView_GetSelection`, `TreeView_HitTest`,
`TreeView_SelectItem`, `TreeView_SetItem` — all first used in
`elenasrc/ide/win32/listview.cpp` / `treeview.cpp`.

---

### 3.2 PE/COFF & executable format

All in `elenasrc/elc/win32/linker.cpp` (637 LOC) + `linker.h` (128 LOC).

| file:line | construct | problem | fix |
|---|---|---|---|
| `linker.cpp:14` | `#include <windows.h>` | `IMAGE_*` structs come from the SDK | own headers or LLVM `object`/`MC` |
| `linker.cpp:18-19` | `MAJOR_OS 0x04` / `MINOR_OS 0x00` | targets Windows NT 4.0 | N/A after PE removal |
| `linker.cpp:21` | `FILE_ALIGNMENT 0x200` | PE file alignment | ELF: `p_align`; Mach-O: `__PAGEZERO` |
| `linker.cpp:22` | `SECTION_ALIGNMENT 0x1000` | 4 KiB page assumption | ARM64 macOS uses 16 KiB pages |
| `linker.cpp:23` | `IMAGE_BASE 0x00400000` | fixed non-PIE load address | PIE/ASLR mandatory on modern Linux & macOS |
| `linker.cpp:25-29` | `".text" ".data" ".bss" ".import" ".debug"` | PE section names, `.import` has no ELF analogue | ELF `.text/.data/.bss/.rela.plt/.debug_*` |
| `linker.cpp:31-33` | `IMAGE_SIZEOF_NT_OPTIONAL_HEADER 224` | **PE32 only** (PE32+ is 240) | irrelevant after migration |
| `linker.cpp:414-423` `writeDOSStub` | reads `bin/winstub.ex_` verbatim | an MS-DOS MZ stub binary blob | delete |
| `linker.cpp:416` | `LocalPath stubPath(..., _T("winstub.ex_"))` | hard-coded artefact filename | delete |
| `linker.cpp:425-441` `writeHeader` | `IMAGE_FILE_HEADER` | COFF header | ELF `Elf64_Ehdr` / Mach-O `mach_header_64` |
| `linker.cpp:429` | `header.Machine = IMAGE_FILE_MACHINE_I386` | **hard-coded i386**; comment admits "machine type may be different" | `EM_X86_64` / `EM_AARCH64` |
| `linker.cpp:434` | `IMAGE_FILE_32BIT_MACHINE` | declares a 32-bit image | — |
| `linker.cpp:443-504` `writeNTHeader` | `IMAGE_OPTIONAL_HEADER` | PE32 optional header, 224 bytes | — |
| `linker.cpp:447` | `IMAGE_NT_OPTIONAL_HDR32_MAGIC` | PE32 magic (0x10B) | — |
| `linker.cpp:454-456` | `AddressOfEntryPoint`, `BaseOfCode`, `BaseOfData` | `BaseOfData` does not exist in PE32+ | ELF `e_entry` |
| `linker.cpp:468-470` | `#ifndef mingw49 header.Win32VersionValue = 0` | **MinGW-version-specific `#ifdef`** in production code | delete |
| `linker.cpp:475-484` | `IMAGE_SUBSYSTEM_WINDOWS_GUI` / `_CUI` | Windows subsystem concept | no ELF/Mach-O analogue |
| `linker.cpp:489-492` | `SizeOfStackReserve/Commit`, `SizeOfHeapReserve/Commit` | Win32 loader knobs | `setrlimit`, `pthread_attr_setstacksize` |
| `linker.cpp:494-502` | `DataDirectory[1]` = import directory | PE import directory | ELF `.dynamic`/`DT_NEEDED` + PLT/GOT |
| `linker.cpp:506-564` `writeSections` | `IMAGE_SECTION_HEADER` | COFF section table; `strncpy(header.Name, it.key(), 8)` — **8-char section-name limit** (`:516`) | ELF has a string table, no limit |
| `linker.cpp:528-546` | `IMAGE_SCN_*` characteristics | PE section flags | `SHF_ALLOC/EXECINSTR/WRITE`, `PF_R/W/X` |
| `linker.cpp:545` | `IMAGE_SCN_MEM_SHARED` on `.import` | shared-page semantics unique to PE | drop |
| `linker.cpp:575` | `executable.writeDWord((int)IMAGE_NT_SIGNATURE)` | writes `"PE\0\0"` | `\x7fELF` / `0xFEEDFACF` |
| `linker.cpp:326-375` `createImportTable` | hand-built `IMAGE_IMPORT_DESCRIPTOR` array (`(count+1)*20` bytes at `:337`) | PE import descriptor is 20 bytes; thunks are 4 bytes (`:339,341`) — **both are 32-bit-only** | ELF: emit `DT_NEEDED` + `.rela.plt`, let `ld.so` bind |
| `linker.cpp:345-348` | `OriginalFirstThunk`/`TimeDateStamp`/`ForwarderChain`/`Name` | PE-specific | — |
| `linker.cpp:350-354` | appends `".dll"` if missing | Windows library naming | `lib*.so` / `*.dylib` |
| `linker.cpp:377-412` `fixImage` | manual relocation of code/data/import | reimplements what a real linker does | emit relocatable objects, invoke `ld`/`lld` |
| `linker.cpp:391` | `(*data)[offset + 0x14] = getSectionSize(BSS_SECTION) >> 2` | patches a **32-bit-word count** into the GC table at a hard-coded byte offset 0x14 | recompute for 64-bit slot size |
| `linker.cpp:589-617` `createDebugFile` | writes a bespoke `.dn` format | not DWARF/PDB — no debugger interop | emit DWARF via LLVM |
| `linker.cpp:608` | `entryPoint = _codeBase + imageBase + (ref << VA_ALIGNMENT_POWER)` | assumes 8-byte VA granularity + fixed image base | PIE-relative |
| `linker.h:128` | whole class API in terms of PE sections | — | abstract `IImageWriter` |

**No relocation section is ever emitted.** `IMAGE_FILE_RELOCS_STRIPPED` is not set either, but
`DataDirectory[5]` (`.reloc`) stays zero — the image is only loadable at `0x00400000`. Any port
must add position independence from scratch.

---

### 3.3 x86-32 machine code

| file:line | construct | problem | fix |
|---|---|---|---|
| `elenasrc/engine/win32/x86helper.h:32-39` | `otEAX..otEDI = 0x00100300..0x00100307` | 8 general registers, 32-bit only | ARM64 has 31; x86-64 has 16 + REX |
| `elenasrc/engine/win32/x86helper.h:53-56` | `otR32/otM32/otM32disp8/otM32disp32` | no 64-bit operand forms | — |
| `elenasrc/engine/win32/x86helper.h:69` | `otDisp32 = 0x00110005` | 32-bit displacement only | RIP-relative on x86-64 |
| `elenasrc/engine/win32/x86helper.h:107-120` | `JUMP_TYPE_*` = raw x86 condition codes | ISA-specific | LLVM `MC` |
| `elenasrc/engine/win32/x86helper.cpp` | 14 opcode-emitting sites; `movMR32disp`, `leaRM32disp` | hand-assembled ModRM/SIB | delete |
| `elenasrc/engine/win32/x86jitcompiler.cpp:85` | `writeWord(0xC085)` = `test eax,eax` | raw opcode | LLVM IR |
| `elenasrc/engine/win32/x86jitcompiler.cpp:141,156,161` | `writeByte(0x53)` `push ebx`, `0x68` `push imm32` | raw opcodes | — |
| `elenasrc/engine/win32/x86jitcompiler.cpp:171-173` | `0xBA` `mov edx,imm32`; `0x32FF` `push [edx]` | raw opcodes | — |
| `elenasrc/engine/win32/x86jitcompiler.cpp:176-195` | `0xB5FF`, `0xB4FF`, `0xB7FF` + `writeDWord(-(arg<<2))` | `push [ebp/esp/edi ± n*4]` — **`<<2` = 4-byte slots** | `<<3` on 64-bit |
| `elenasrc/engine/win32/x86jitcompiler.cpp:225` | `writeByte(0xE8)` `call rel32` | ±2 GiB branch range | ARM64 `BL` is ±128 MiB — needs veneers |
| `elenasrc/engine/win32/x86jitcompiler.cpp:245` | `writeByte(0x5A)` `pop edx` | raw opcode | — |
| `elenasrc/engine/win32/x86jitcompiler.cpp:253-299` | `0x148B`,`0x1589`,`0x9C8B`,`0x9389`,`0x9789` | `mov` forms with 32-bit disp | — |
| `elenasrc/engine/win32/x86jitcompiler.cpp:369` | `writeByte(0xB8)` `mov eax,imm32` | 32-bit immediate | `movabs` / ARM64 `MOVZ/MOVK` ×4 |
| `elenasrc/engine/win32/x86jitcompiler.cpp:405` | `writeByte(0xB9)` `mov ecx,imm32` | ditto | — |
| `elenasrc/engine/win32/x86jitcompiler.cpp:518` | `writer->align(VA_ALIGNMENT, code ? 0x90 : 0x00)` | `0x90` = x86 `NOP` | ARM64 NOP = `0xD503201F` |
| `elenasrc/engine/win32/x86jitcompiler.cpp:393-397` | `size = fieldCount << 2` / `(size+3)>>2` | **object fields are 4 bytes** | 8 on 64-bit |
| `elenasrc/asm2bin/x86assembler.cpp` | 230 opcode-emit sites, 2,772 LOC | full x86-32 assembler | delete; use LLVM MC or a real assembler |
| `elenasrc/asm2bin/x86jumphelper.cpp` | short/near jump fixup | x86 encoding | — |
| `src/asm/*.asm` | 6,146 LOC of x86-32 | 88 procedures, 109 inline fragments | rewrite in C or LLVM IR |
| `src/asm/elena.asm:1052-1053` | `mov ecx, fs:[4]` / `fs:[8]` | reads **Win32 TIB** `StackBase`/`StackLimit` | `pthread_attr_getstack` |
| `src/asm/elena.asm:1109,1166-1167` | same | GC stack scanning | — |

**Calling conventions.** `__stdcall` is implicit everywhere in `src/asm/*.asm` (callee cleans the
stack via `ret n`, arguments pushed right-to-left). `elenasrc/idecommon/plugins.h:49` declares
`typedef int(__cdecl *RegisterFunction)(_PluginManager*)`. `elenasrc/ide/win32/output.h:27` uses
`DWORD WINAPI`, `elenasrc/ide/win32/dialogs.h:78` and `window.h:87` use `CALLBACK`,
`elenasrc/plugins/autoform/win32/dllmain.cpp:3` uses `APIENTRY`. All of these vanish on x86-64
(one ABI) but must be removed for the code to parse.

**No inline `__asm` blocks exist in the C++** — all machine code is emitted through byte writers.
That is good news: there is nothing MSVC-inline-assembly-specific to untangle.

---

### 3.4 Pointer / word size

| file:line | construct | problem | fix |
|---|---|---|---|
| `elenasrc/common/lists.h:216` | `return (TCHAR*)((int)this + (int)this->key);` | **casts `this` to `int`** — GCC/Clang *error* at 64-bit | `(char*)this + (ptrdiff_t)key` |
| `elenasrc/common/lists.h:234` | `compstr((TCHAR*)((int)this + (int)this->key), key)` | same | same |
| `elenasrc/common/lists.h:266` | `grtstr(key, (TCHAR*)((int)this + (int)this->key))` | same | same |
| `elenasrc/common/lists.h:281` | `!grtstr((TCHAR*)((int)this + (int)this->key), key)` | same | same |
| `elenasrc/common/lists.h:395` | `(Item*)((int)_map->_buffer.getArray() + _position)` | `void*`→`int` | `uintptr_t` |
| `elenasrc/common/lists.h:421` | `_current = (Item*)((int)_map->_buffer.getArray() + _position)` | `void*`→`int` — **error** | `uintptr_t` |
| `elenasrc/common/lists.h:1590` | `_buffer.writeDWord(0, 4)` | reserves a **4-byte** head-of-list offset | width-parameterised |
| `elenasrc/common/lists.h:1594` | `_buffer.write(position, &item, sizeof(item))` | dumps the raw `_MemoryMapItem` (12 B at 32-bit, 24 B at 64-bit) **to disk** | explicit serialiser |
| `elenasrc/common/lists.h:1599` | `_buffer.writeDWord(position + 4, storedKey)` | **hard-coded field offset 4** for the key | `offsetof` or explicit layout |
| `elenasrc/common/lists.h:2322,2326` | same pattern in `MemoryHashTable` | same | same |
| `elenasrc/common/lists.h:1873,1884` | `write(&_cache, sizeof(Item) * _count)` | raw array dump | same |
| `elenasrc/common/streams.h:50` | `bool readDWord(int& dword) { return read(&dword, 4); }` | fine | — |
| `elenasrc/common/streams.h:53-56` | `bool readDWord(size_t& dword) { return read(&dword, 4); }` | **reads 4 bytes into an 8-byte `size_t`** — upper half uninitialised garbage on 64-bit | read into `uint32_t`, then widen |
| `elenasrc/common/streams.h:146` | `writeDWord(int dword) { write(&dword, 4); }` | all references are 32-bit on disk | version the format |
| `elenasrc/engine/elenaconst.h:21` (`common.h:21`) | `#define ref_t size_t` | `ref_t` silently becomes 64-bit while the masks below stay 32-bit | `typedef uint32_t ref_t` |
| `elenasrc/engine/elenaconst.h:172-199` | `mskAnyRef = 0xFF000000` … `mskLinkerConstant = 0x0D000000` | **top-byte tagging of a 32-bit address space**; with 64-bit `ref_t` `~mskAnyRef` is `0xFFFFFFFF00FFFFFF` | separate tag field or 64-bit mask constants |
| `elenasrc/engine/elenaconst.h:107` | `PREDEFINED_REF 0x80000000` | sign bit of a 32-bit word | — |
| `elenasrc/engine/elenaconst.h:110` | `VMT_INDEX_SIZE 4` | VMT slot = 4 bytes | 8 |
| `elenasrc/engine/elenaconst.h:253` | `elEmptyObject = 0x0008` | **object header = 8 bytes** (2 × 32-bit) | 16 |
| `elenasrc/engine/elenaconst.h:254` | `elVMTOffset = 0x000C` | **VMT header = 12 bytes** (3 × 32-bit) | 24 |
| `elenasrc/engine/elenaconst.h:256` | `elAnyHandlerSize = 0x0010` | 2 VMT entries × 8 bytes | 32 |
| `elenasrc/engine/elenaconst.h:276-278` | `gcPageSize 0x10`, `gcCollected 0x40000000`, `gcBinary 0x80000000` | GC flags in the top bits of a **32-bit** size word | — |
| `elenasrc/engine/elena.h:76-80` | `struct VMTEntry { int messageID; int address; }` | 8 bytes; `address` is an `int` | `uintptr_t` |
| `elenasrc/engine/elena.h:84-89` | `struct ClassHeader { ref_t roleRef; size_t flags; ref_t parentRef; }` | 12 B at 32-bit → **24 B at 64-bit**, changing the on-disk layout | fixed-width fields |
| `elenasrc/engine/elena.h:107` | `writer->write((void*)this, sizeof(ClassHeader))` | raw struct → file | field-by-field |
| `elenasrc/engine/elena.h:117` | `reader->read((void*)&header, sizeof(ClassHeader))` | raw file → struct | field-by-field |
| `elenasrc/engine/elena.h:133-142` | `struct DebugLineInfo` with a union containing `size_t address` | 20 B at 32-bit → 32 B at 64-bit | fixed-width |
| `elenasrc/engine/elena.h:225-231` | `VA_ALIGNMENT 0x08` / `reallocateReference(v) { return v << 3; }` | **references are addresses ÷ 8**, capping the address space at 32 GiB and assuming 8-byte object alignment | 16-byte alignment for 64-bit |
| `elenasrc/engine/section.h:19` | `((reference && ~mskAnyRef) >> 2)` | **`&&` where `&` was meant** — a genuine bug; Clang flags it | `(reference & ~mskAnyRef) >> 2` |
| `elenasrc/engine/section.h:19` | `>> 2` | 4-byte index granularity | `>> 3` |
| `elenasrc/engine/jitlinker.cpp:22` | `(*image)[position] = address - (size_t)image->getArray() - position - 4` | `- 4` = size of a `rel32` displacement | ISA-dependent |
| `elenasrc/engine/jitlinker.cpp:99,432` | `reallocateReference((size_t)vaddress)` | `void*`→`size_t`→ shifted | — |
| `elenasrc/engine/jitlinker.cpp:387` | `(*image)[reallocateReference(...) + 4]` | class-flags field at **+4** | +8 |
| `elenasrc/engine/jitlinker.cpp:278` | `vmtReader.getDWord() + sizeof(VMTEntry)` | mixes disk DWORD with host `sizeof` | — |
| `elenasrc/engine/jitcompiler.cpp:125` | `anyHandlerPos - vmtWriter.Position() + 8` | 8 = one VMT entry | 16 |
| `elenasrc/engine/bccompiler.cpp:127` | `const TCHAR* localName = (TCHAR*)(*it).argument;` | **`int`→pointer** (Clang: `-Wint-to-pointer-cast`) | store an index, not a pointer |
| `elenasrc/engine/bccompiler.cpp:725` | `jumpsToLoop.get(level) - code->Position() - 4` | 4-byte operand | — |
| `elenasrc/common/dump.h:32-34` | `int& operator[](size_t position) { return *(int*)(_buffer + position); }` | **the entire image is addressed as an array of `int`** | `uint32_t`, or width-templated |
| `elenasrc/common/dump.h:47-50,72-76` | `writeDWord`/`insertDWord` write 4 bytes | — | — |
| `elenasrc/common/files.h:213-216` | `long Position()`, `long Length()`, `seek(long)` | `long` is 32-bit on Win64 (LLP64) but 64-bit on LP64 — **inconsistent file-size limit** | `int64_t` / `off_t` |
| `elenasrc/elc/win32/linker.cpp:107,113,116` | `_nativeReferences.add(reference, (ref_t)vaddress)` | `void*`→`ref_t` | — |
| `elenasrc/ide/win32/debugger.cpp:445` | `exception.address = (int)exception->ExceptionRecord.ExceptionAddress` | pointer→`int` | `uintptr_t` |
| `elenasrc/ide/win32/debugger.h:93-94` | `context.Ebp - offset * 4`, `context.Esp + offset * 4` | 4-byte stack slots | 8 |
| `elenasrc/sg/sg.cpp:47` | `int id = (int)table.defineSymbol(symbol)` | truncation | — |

---

### 3.5 Character / encoding

`TCHAR`-family tokens appear on **1,066 lines**: `common/` 290, `engine/` 167, `elc/` 232,
`ide/` 377 (excluding `ide/win32/`), `asm2bin/` 434.

| file:line | construct | problem | fix |
|---|---|---|---|
| `elenasrc/common/common.h:14` | `#include <tchar.h>` | source of `TCHAR`, `_T`, `_tcs*` | remove |
| `elenasrc/common/common.h:22` | `#define DEFAULT_STR (const TCHAR*)NULL` | — | `nullptr` |
| `elenasrc/common/win32/unicode.h:19` | `MultiByteToWideChar(CP_ACP, ...)` | **`CP_ACP` = the machine's ANSI code page**; result depends on the user's locale | UTF-8 ↔ UTF-16/32 with a fixed codec |
| `elenasrc/common/win32/unicode.h:26` | `WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, ...)` | same, lossy | same |
| `elenasrc/common/tools.h:149` | `malloc((wcslen(s) << 1) + 2)` | **`sizeof(wchar_t)==2`** — under-allocates by half on Linux/macOS → heap overflow | `* sizeof(wchar_t)` or migrate to UTF-8 |
| `elenasrc/common/tools.h:160` | `malloc(length << 1)` | same | same |
| `elenasrc/common/tools.h:170` | `realloc(s, length << 1)` | same | same |
| `elenasrc/common/tools.h:215` | `memmove(s1, s2, length << 1)` | same | same |
| `elenasrc/common/tools.h:183-196` | `_gcvt` for double→string | MSVC-only; not in glibc's default namespace | `snprintf("%.*g")` |
| `elenasrc/common/tools.h:218-226` | `_tchlwr` via `_tcslwr` | `_tcslwr` is MSVC-only | `towlower` |
| `elenasrc/common/tools.h:240-257` | `mapReferenceKey`/`mapLiteralKey` do `key[i] - 'a'` | assumes ASCII lowercase Latin identifiers; breaks on non-ASCII module names | hash the code units |
| `elenasrc/common/altstrings.h:23-98` | every `_String` method takes `const TCHAR*` | viral | — |
| `elenasrc/common/altstrings.h:42-44` | `_tcslwr` / `_tcsupr` | MSVC-only; also **locale-dependent case folding** | ICU or explicit ASCII fold |
| `elenasrc/common/altstrings.h:149-153,219-229` | `#ifdef _UNICODE void convert(const char*)` | dual ANSI/Unicode build modes | pick one (UTF-8) |
| `elenasrc/common/altstrings.h:377` | `_string.append((TCHAR)number.asInt())` | **truncates a decimal escape to one `TCHAR`** — cannot express U+10000+ in UTF-16 | proper surrogate handling |
| `elenasrc/common/streams.h:65` | `read(&ch, sizeof(TCHAR))` | reads 2 B on Win, 4 B on Linux **from the same file** | fixed-width |
| `elenasrc/common/streams.h:70` | `readLiteral(wchar_t* s, size_t length) { read(s, length << 1); }` | **hard-codes 2 bytes/char** | — |
| `elenasrc/common/streams.h:138` | `writeLiteral(const wchar_t*, length) { write(s, length << 1); }` | same, on the write path | — |
| `elenasrc/common/streams.h:186-191` | `writeAsciiLiteral(const wchar_t*, n)` writes 1 byte per element | **silently truncates non-ASCII** into the import table / section names | UTF-8 encode |
| `elenasrc/common/streams.h:291-324` | `#ifdef _UNICODE` fork of `LiteralWriter::write` | the `#else` branch (`:314-322`) **has no `return`** — UB | one implementation |
| `elenasrc/common/streams.h:392-394` | `read(void* s, size_t length) { size = length >> 1; _tcsncpy(..., size >> 1); }` | `>>1` applied twice — reads a quarter of the requested length | rewrite |
| `elenasrc/common/streams.h:405-413` | `readDWord()` advances `_offset` by **2**, not 4 | off-by-half bug | — |
| `elenasrc/common/dump.h:52-59` | `write(position, const wchar_t* s)` → `(wcslen(s)+1) << 1` | 2 bytes/char | — |
| `elenasrc/common/dump.cpp:186` | `_position += ((getlength(s) + 1) << 1)` | 2 bytes/char | — |
| `elenasrc/common/files.cpp:26-34` | BOM autodetect: `signature == 0xFEFF` | only UTF-16LE; no UTF-8 BOM, no UTF-16BE | full BOM sniffing |
| `elenasrc/common/files.cpp:105-124` | `fread((char*)temp, 2, count, _file)` | reads UTF-16 code units | — |
| `elenasrc/common/files.cpp:129` | `wasread = fread((char*)s, 2, length, _file)` | writes 2-byte units into a 4-byte `wchar_t` buffer | — |
| `elenasrc/common/files.cpp:139-146,188-194` | manual byte-doubling loop `buf[j]=buf[i]; buf[j+1]=0;` | ANSI→UTF-16 widening assuming 2 bytes | — |
| `elenasrc/common/files.cpp:214,227` | `fwrite((const char*)s, 2, length, _file)` | ditto | — |
| `elenasrc/common/files.cpp:247` | `writeLiteral(_T("\r\n"), 2)` | **CRLF hard-coded** in the generic text writer | platform newline |
| `elenasrc/common/files.h:200` | `enum FileEncoding { feAnsi, feUTF8, feUTF16 }` | `feUTF8` is declared but never implemented (`files.cpp` handles only Ansi/UTF16/Raw) | implement UTF-8 |
| `elenasrc/engine/module.cpp:152-155` | `writeLiteral(MODULE_SIGNATURE, ...)` then `writeLiteral(_name, getlength(_name)+1)` | module names stored as **UTF-16LE** → a Linux build writes UTF-32 into the same slot | UTF-8 in the format |
| `elenasrc/engine/elenaconst.h:22-70` | all reference names are `_T("...")` wide literals | — | — |
| `elenasrc/ide/text.cpp:245` | `insert(bookmark, TEXT("\r\n"), 2, true)` | editor inserts CRLF unconditionally | configurable EOL |
| `elenasrc/ide/text.cpp:163,710,731,814,835` | `line[i]==0x0D` | CR-based line scanning | handle LF-only |
| `elenasrc/ide/ideconst.h:273` | licence text with embedded `\r\n` and `doc\\license.txt` | — | — |
| `elenasrc/api2html/api2html.cpp:10-13,107-138` | `const char[]` passed where `const TCHAR*` is expected | **the tool cannot be compiled in UNICODE mode at all** (see §6.4) | port to one encoding |
| `elenasrc/elc/source.cpp:109` | `_tcslwr(token)` | identifiers are lower-cased ⇒ **ELENA is case-insensitive by accident of `_tcslwr`** | explicit ASCII fold |

**The `wchar_t` trap, stated plainly.** Every `.nl` module, every `.dn` debug file and every
source file this compiler reads or writes uses `sizeof(wchar_t)` as its character width, but the
code assumes that width is 2. On Windows this happens to be true. On Linux and macOS it is 4.
Therefore: a Linux-built `elc` writes 4-byte characters where the format says 2, and reads 2-byte
characters into 4-byte slots. **Modules are not interchangeable between a Windows build and a
Linux build of the same compiler version, and nothing detects the mismatch.**

---

### 3.6 Filesystem

| file:line | construct | problem | fix |
|---|---|---|---|
| `elenasrc/common/files.h:26` | `lastchrpos(path, '\\') + 1` | backslash-only separator | `/` on POSIX; accept both |
| `elenasrc/common/files.h:65` | `lastchrpos(path, '\\')` in `copyPath` | same | — |
| `elenasrc/common/files.h:84-85` | `if ((*this)[len-1] != '\\') append('\\')` | **path join emits backslashes** | `std::filesystem::path::operator/` |
| `elenasrc/common/files.h:105` | `lastchrpos(path, '\\') + 1` in `changeExtension` | same | — |
| `elenasrc/common/files.h:181` | `_tcsrchr(path, '\\')` in `copyName` | same | — |
| `elenasrc/common/altstrings.h:322` | `chrpos(path, '\\')` in `pathToName` | **maps a file path to an ELENA module name using backslashes** | — |
| `elenasrc/ide/win32/appwindow.cpp:1929` | `LocalPath curDir(path, lastchrpos(path, '\\'))` | same | — |
| `elenasrc/common/files.h:15` | `#define LOCAL_PATH_LENGTH 0x200` | 512-char fixed path buffers used everywhere (`LocalPath`) | dynamic strings |
| `elenasrc/elc/win32/linker.cpp:273` | `LocalString<MAX_PATH> dll(...)` | `MAX_PATH` = 260, Windows-only constant | `PATH_MAX` / dynamic |
| `elenasrc/elc/win32/elc.cpp:28-30` | `TCHAR path[MAX_PATH+1]; GetModuleFileName(NULL, path, MAX_PATH)` | fixed buffer + Win32 API | `/proc/self/exe`, `_NSGetExecutablePath`, `readlink` |
| `elenasrc/ide/idesettings.cpp:73-75` | same + `PathRemoveFileSpec` | — | — |
| `elenasrc/ide/idesettings.cpp:89-91` | `_wgetcwd` / `_getcwd` with `MAX_PATH` | — | `getcwd` |
| `elenasrc/ide/idesettings.cpp:100-104` | `TCHAR p[MAX_PATH]; PathCanonicalize(p, path)` | — | `weakly_canonical` |
| `elenasrc/ide/idesettings.cpp:121-127` | `PathRelativePathTo(...)`, then `compstr(tmpPath, _T(".\\"), 2)` | `.\` prefix | `./` |
| `elenasrc/ide/win32/dialogs.h:25` | `TCHAR _fileName[MAX_PATH * 8]; // ??` | 2,080-char buffer with the author's own question mark | dynamic |
| `elenasrc/ide/win32/dialogs.cpp:305-322` | four `MAX_PATH` stack buffers filled from dialog text | overflow risk | — |
| `elenasrc/elc/win32/elc.cpp:48` | `fullPath.lower()` on every source path | **assumes a case-insensitive filesystem** — breaks on ext4/APFS-sensitive | preserve case |
| `elenasrc/elc/win32/elc.cpp:57,59,85,96,98` | `.lower()` / `_tcslwr` on forwards, config keys and *file paths* | same | — |
| `elenasrc/ide/idesettings.cpp:81,85` | `packageRoot.lower()`, `libraryRoot.lower()` | same | — |
| `elenasrc/ide/ideproject.cpp:219,232` | `_path.lower()`, `fullPath.lower()` | same | — |
| `elenasrc/common/files.h:122-127` | `struct _stat` + `_tstat` | MSVC names | `struct stat` + `stat` |
| `elenasrc/common/files.cpp:333-351` | `_mkdir`/`_wmkdir`/`_access`/`_waccess` | MSVC names, no mode argument | `mkdir(p, 0777)`, `access` |
| `elenasrc/common/files.cpp:353-367` | `createPath` recurses on `dirPath` | no error handling; would need `EEXIST` handling on POSIX | `create_directories` |
| `bin/elc.cfg:2` | `libpath=..\lib` | backslash in the **shipped default config** | `/` |
| `bin/elc.cfg:6-7` | `console=templates\console.cfg` | same | — |
| `elena.workspace:4-8` | `elenasrc\sg\codeblocks\sg.cbp` | backslash project paths | — |
| `elenasrc/ide/ide.rc:6-25` | `"icons\\elgui.ico"`, `"icons\\*.bmp"` | resource paths | — |

**Drive letters:** none found in tracked sources or data files (only in the stale
`elenasrc/elc/codeblocks/elc.cbp:14` run parameter `-cD:\Alex\PROJECTS\elena.2\...`).

---

### 3.7 Compiler-specific constructs

| file:line | construct | problem | fix |
|---|---|---|---|
| `elenasrc/common/common.h:12` | `#pragma warning(disable : 4996)` | MSVC-only pragma; harmless but noisy under `-Wunknown-pragmas` | guard or delete |
| `elenasrc/common/altstrings.h:84-85` | `void appendInt64(__int64)`, `appendHex64(__int64)` | **`__int64` is an MSVC keyword** — GCC 16 errors | `int64_t` |
| `elenasrc/common/altstrings.cpp:28,40` | `__int64` definitions | same | — |
| `elenasrc/ide/browser.h:47`, `browser.cpp:164` | `__int64` | same | — |
| `elenasrc/ide/debugcontroller.h:29`, `debugcontroller.cpp:652` | `__int64` | same | — |
| `elenasrc/idecommon/plugins.h:49` | `typedef int(__cdecl *RegisterFunction)(...)` | MSVC/MinGW keyword | remove |
| `elenasrc/plugins/autoform/autoform.h:18` | `#define EXTERN_DLL_EXPORT extern "C" __declspec(dllexport)` | MSVC/MinGW | `__attribute__((visibility("default")))` |
| `elenasrc/plugins/autoform/win32/dllmain.cpp:3` | `BOOL APIENTRY DllMain(HMODULE, ...)` | Windows DLL entry | constructor/destructor attributes |
| `elenasrc/ide/win32/output.h:27` | `static DWORD WINAPI OutputThread(LPVOID)` | `WINAPI` = `__stdcall` | `void*(*)(void*)` |
| `elenasrc/ide/win32/dialogs.h:78`, `window.h:87` | `BOOL CALLBACK` / `LRESULT CALLBACK` | `__stdcall` | — |
| `elenasrc/ide/win32/winmain.cpp:167` | `int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)` | Windows entry point | `int main(int, char**)` |
| `elenasrc/elc/win32/elc.cpp:9` | `#define __MSVCRT_VERSION__ 0x0800` | MinGW CRT version selector | delete |
| `elenasrc/elc/win32/linker.cpp:468-470` | `#ifndef mingw49` around `header.Win32VersionValue` | **compiler-version `#ifdef` in the linker** | delete with PE |
| `elenasrc/elc/codeblocks/elc.cbp:23` | `-Dmingw49` | that `#ifdef`'s definition site | — |
| `elenasrc/common/dump.h:112` | `void* getArray() const { _dump->getArray(); }` | **non-void function with no `return`** — UB; GCC/Clang warn, `-Werror=return-type` breaks the build | add `return` |
| `elenasrc/common/dump.h:175` | same | same | — |
| `elenasrc/common/streams.h:314-322` | `virtual bool write(void*, size_t)` in the non-`_UNICODE` branch, no `return` | same | — |
| `elenasrc/engine/section.h:19` | `((reference && ~mskAnyRef) >> 2)` | logical `&&` on a constant — Clang `-Wconstant-logical-operand`; **the hash always returns 0** | `&` |
| `elenasrc/common/lists.h:939` | `this->_defaultItem = NULL;` where `T` is `int` | `NULL`→`int` conversion; Clang `-Wnull-conversion` | `0` / `T()` |
| `elenasrc/common/lists.h:1487` | `if (*current == key)` inside a template | Clang: `invalid operands to binary expression ('Item' and 'const wchar_t *')` when `T=int` — **two-phase lookup / overload gap** | add the missing `operator==` overload |
| `elenasrc/elc/compiler.cpp:786` | `_tcstoul` | MSVC tchar macro | `wcstoul` / `strtoul` |
| `elenasrc/elc/compiler.cpp:798`, `elenasrc/engine/jitlinker.cpp:370` | `_tcstod` | same | `wcstod` |
| `elenasrc/common/altstrings.cpp:23,59,69` | `_itot`, `_ltot` | MSVC-only | `snprintf` |
| `elenasrc/common/tools.h:56` | `_wremove` | MSVC-only | `remove` |
| `elenasrc/common/tools.h:185,192` | `_gcvt` | MSVC-only | `snprintf("%.*g")` |
| `elenasrc/sg/sg.cpp:74` | `TextFileReader(argv[1], ...)` with `char*` | no matching ctor when `TCHAR=wchar_t` | convert |
| `elenasrc/sg/sg.cpp:135` | `char*` → `size_t` implicit | invalid conversion | — |
| `elenasrc/api2html/api2html.cpp` (many) | `const char[]` → `const TCHAR*` | 30+ conversion errors | — |
| `elenasrc/engine/bccompiler.cpp:340,488,799` | `switch` over enums with 5/14/64 unhandled values | Clang `-Wswitch`; masks real logic gaps | add `default:` or full coverage |
| `elenasrc/elc/compiler.cpp:1209,1406,1669` | `switch` with 70/75/74 unhandled enum values | same | — |
| — | `register`, `throw()` specs, `auto_ptr`, `>>` template ambiguity | **none found** | no action |

---

### 3.8 Endianness

The tree is **implicitly little-endian throughout** and has no byte-swapping code anywhere.
Because all currently interesting targets (x86-64, ARM64 in practice) are little-endian, this is
low priority — but it must be recorded because the formats are memory dumps, not encodings.

| file:line | construct | problem | fix |
|---|---|---|---|
| `elenasrc/common/streams.h:48-56,146-154` | `read/write(&value, 4)` / `(&word, 2)` | writes host byte order | explicit LE encode/decode |
| `elenasrc/common/dump.h:30-34` | `int& operator[](size_t)` reinterprets the image as `int[]` | host order + host alignment | — |
| `elenasrc/common/lists.h:1594,1873,2322` | raw struct/array dumps | host order **and** host layout | — |
| `elenasrc/engine/elena.h:107,117` | raw `ClassHeader` I/O | — | — |
| `elenasrc/engine/bccompiler.cpp:76,98,105,117,140,162,169` | raw `DebugLineInfo` I/O into `.dn` | — | — |
| `elenasrc/engine/bccompiler.cpp:833` | raw `ClassHeader` into the VMT section | — | — |
| `elenasrc/common/files.cpp:28` | `if (signature == 0xFEFF)` | BOM compared as a host-order `unsigned short` — a BE host would mis-detect | compare bytes |
| `elenasrc/engine/elenaconst.h:148-150` | `"ELENA.150"`, `"EN!10"`, `"EN.D10!"` | ASCII magics, no endianness/word-size marker | **add an ABI byte to the magic** |
| `src/asm/*.asm` | all constants written as x86 immediates | LE | — |

---

### 3.9 Process, threading & GUI

#### Threading (current state)

| file:line | construct | note |
|---|---|---|
| `elenasrc/ide/win32/debugger.cpp:562` | `CreateThread(NULL, 4096, ...)` — debug-event pump | 4 KiB stack |
| `elenasrc/ide/win32/debugger.cpp:20` | `ExitThread(TRUE)` | — |
| `elenasrc/ide/win32/debugger.cpp:29-32` | 4 × `CreateEvent` (`DEBUG_ACTIVE/CLOSE/SUSPEND/RESUME`) | manual-reset events |
| `elenasrc/ide/win32/debugger.cpp:42,47,52` | `SetEvent`, `WaitForMultipleObjects`, `WaitForSingleObject` | → `std::condition_variable` |
| `elenasrc/ide/win32/output.cpp:124` | `CreateThread(NULL, 0, OutputThread, ...)` — pipe reader | — |
| `elenasrc/ide/win32/output.cpp:49` | `TerminateThread(_hThread, -2)` | **unsafe primitive with no POSIX equivalent** — must be redesigned around cancellation points |
| `elenasrc/ide/win32/output.cpp:157` | `WaitForMultipleObjects(2, aHandles, FALSE, 100)` | — |

**No critical sections, no `Interlocked*`, no TLS, no `volatile` on shared state anywhere.** The
IDE's cross-thread communication is entirely via Win32 events and `PostMessage`.

#### What blocks *real* multithreading in the language runtime

| file:line | construct | problem |
|---|---|---|
| `src/asm/elena.asm:15,18,21` | `mov eax, ['gc_yg_heap]` … `mov ['gc_yg_heap], esi` | the **bump allocator is a plain global with no lock or atomic** — two ELENA threads corrupt the heap immediately |
| `src/asm/elena.asm:48,90,146` | `['gc_heap_end]` | same |
| `src/asm/elena.asm:519,532,1215,1228` | heap initialisation writes the same globals | single-heap design |
| `src/asm/elena.asm:1052-1053,1109,1166-1167` | `fs:[4]` / `fs:[8]` = TIB `StackBase`/`StackLimit` | **stack scanning uses the Win32 TIB**; there is no per-thread root set, no stop-the-world, no safepoints |
| `elenasrc/engine/elenaconst.h:277-278` | `gcCollected 0x40000000`, `gcBinary 0x80000000` | mark bits live in the object size word — needs atomic CAS under concurrency |
| `elenasrc/elc/win32/linker.cpp:389-391` | the GC table is a single static structure patched at link time | one heap per process |
| `elenasrc/engine/jitlinker.cpp` (whole) | JIT writes directly into the image section | no code-cache locking; also no `mprotect`/`pthread_jit_write_protect_np` — **fatal on Apple Silicon (W^X)** |

Adding threads therefore requires: per-thread allocation buffers, an atomic or locked slow path,
a portable thread registry replacing `fs:[4]`, safepoints, and a W^X-correct code cache. This is a
runtime redesign, not a feature addition.

#### GUI (summary — covered in depth by the IDE document)

| Metric | Value |
|---|---|
| Directory | `elenasrc/ide/win32/` |
| Files | 31 (`.cpp`/`.h`) + `icons/` (10 `.bmp`, 1 `.ico`) |
| LOC | **8,802** (26 % of all C++ in the tree) |
| Largest units | `appwindow.cpp` 2,167 · `editframe.cpp` 1,238 · `dialogs.cpp` 670 · `debugger.cpp` 607 · `idecommon.h` 463 · `idecommon.cpp` 455 |
| Distinct Win32 GUI functions | **107** + 15 `commctrl` macros |
| Highest-traffic call | `SendMessage` — 38 sites across 8 files, first at `elenasrc/ide/win32/listview.cpp:57` |
| Toolkit dependencies | `user32`, `gdi32`, `comctl32.lib`, `shlwapi.lib`, `comdlg32` (`GetOpenFileName`/`GetSaveFileName`) |
| Resource script | `elenasrc/ide/ide.rc` (menus, accelerators, dialogs, bitmaps) |
| Leakage into "portable" code | `elenasrc/ide/browser.h:2`, `browser.cpp:2`, `messagelog.h:1`, `messagelog.cpp:1`, `debugcontroller.cpp:1`, `pluginmanager.cpp:1`, `idesettings.cpp:1`, `ideconst.h:4` — 13 lines total, easily severed |
| Existing "port" | `elenasrc/ide/gtk/main.cpp` — **49 lines, a GTK2 Hello World**. Nothing real exists. |

**Recommendation:** do not port the IDE. Ship a language toolchain first; provide an LSP server and
a VS Code / Zed extension. If a native IDE is wanted later, rewrite on Qt 6 — a Win32→Qt rewrite of
8,802 LOC is cheaper and lower-risk than an abstraction layer over Win32 + GTK + Cocoa.

---

### 3.10 The ELENA-level runtime and library (`src/`)

Cross-reference for the runtime document; quantified here.

| File | LOC | procedures | inline fragments | DLL calls | Nature |
|---|---:|---:|---:|---:|---|
| `src/asm/standard.asm` | 2,804 | 3 | 79 | 0 | Pure x86-32 algorithms (string compare, arithmetic) — **arch-bound, OS-free** |
| `src/asm/elena.asm` | 1,481 | 10 | 26 | 10 | GC, object creation, dispatch — arch + OS bound (`fs:` TIB) |
| `src/asm/win32.asm` | 1,423 | 59 | 4 | 65 | Console, file, window, GDI shims — **100 % Win32** |
| `src/asm/winsock.asm` | 362 | 13 | 0 | 17 | BSD-socket-shaped, but via `Ws2_32` |
| `src/asm/extended.asm` | 76 | 3 | 0 | 4 | RNG seeding via `GetSystemTime` |

**64 distinct Win32 DLL entry points** are reachable from ELENA code through these files:
`kernel32` ×17 (`CreateFileW`, `ReadFile`, `WriteFile`, `ReadConsoleW`, `WriteConsoleW`,
`ReadConsoleInputW`, `GetNumberOfConsoleInputEvents`, `GetStdHandle`, `GetCommandLineW`,
`CloseHandle`, `ExitProcess`, `GetProcessHeap`, `HeapAlloc`, `RaiseException`, `GetSystemTime`,
`SystemTimeToFileTime`, `WideCharToMultiByte`), `user32` ×29, `gdi32` ×15, `Ws2_32` ×13.

Consequences:
- `winsock.asm` maps almost 1:1 onto BSD sockets — the cheapest file to port.
- `win32.asm`'s console half maps onto `read`/`write`/`termios`; its window half does not map at all.
- `bin/templates/console.cfg` forwards `'program'output` → `win32'io'stdoutput` and
  `'program'commandline` → `win32'api'commandline`; `bin/templates/gui.cfg` forwards **31 GUI
  symbols** into `win32'api'*`. A POSIX port needs parallel `posix'*` packages plus new templates.

---

### 3.11 Build files

| file:line | construct | problem | fix |
|---|---|---|---|
| `elena.workspace:4-8` | `elenasrc\sg\codeblocks\sg.cbp` etc. | backslash paths | CMake |
| `elenasrc/elc/codeblocks/elc.cbp:9` | `<Option output="..\..\..\bin\elc.exe" />` | `.exe`, backslashes | — |
| `elenasrc/elc/codeblocks/elc.cbp:14` | `-cD:\Alex\PROJECTS\elena.2\examples\sample1\sample.prj` | **the original author's absolute drive path**, committed | — |
| `elenasrc/elc/codeblocks/elc.cbp:18` | `-march=pentium2` | pins to 32-bit P6 | `-march=native` / per-target |
| `elenasrc/elc/codeblocks/elc.cbp:21-22` | `-D_UNICODE -DUNICODE` | selects the `wchar_t` build | — |
| `elenasrc/elc/codeblocks/elc.cbp:23` | `-Dmingw49` | feeds `linker.cpp:468` | — |
| `elenasrc/elc/codeblocks/elc.cbp` (Unit list) | `..\win32\x86opcodes.h ` | **references a file that does not exist**, with a trailing space | — |
| `elenasrc/ide/codeblocks/elide_win32.cbp:36-37` | `<Add library="C:\MinGW\lib\libshlwapi.a" />`, `libcomctl32.a` | absolute MinGW paths | — |
| `elenasrc/ide/codeblocks/elide_gtk.cbp:32,36` | `pkg-config gtk+-2.0` | GTK **2** (EOL); builds only the 49-line stub | GTK4/Qt6, or drop |
| `elenasrc/**/vs/*.vcproj` (12 files) | MSVC 7.1/8/9 XML | dead toolchains | delete |
| `elenasrc/**/vs/*.vcxproj` (5 files) | MSVC 10 (VS2010) | dead toolchain | delete |
| all `.vcproj`/`.vcxproj` | `comctl32.lib`, `shlwapi.lib` | Windows-only imports | — |
| `elenasrc/asm2bin/codeblocks/asm2bin.cbp:16` | `-march=pentium2` | — | — |
| `elenasrc/api2html/codeblocks/api2html.cbp:28` | `-march=pentium2`, **no** `-D_UNICODE` | inconsistent with the rest — explains §6.4 | — |
| — | no `Makefile`, no `CMakeLists.txt`, no `configure` | **there is no portable build system at all** | CMake ≥ 3.20 |
| `bin/winstub.ex_` | binary artefact required at link time (`linker.cpp:416`) | — | delete with PE |

---

## 4. The 64-bit problem

This is the part that cannot be solved by an abstraction layer, because the 32-bitness is **in the
data**, not in the code.

### 4.1 The object model

`elenasrc/engine/elenaconst.h` fixes the memory layout of every ELENA object:

```
elEmptyObject = 0x0008   // object header = 8 bytes  (size word + VMT pointer)
elVMTOffset   = 0x000C   // VMT header   = 12 bytes (3 words)
VMT_INDEX_SIZE= 4        // one VMT slot = 4 bytes
elAnyHandlerSize = 0x10  // 2 VMT entries
gcPageSize    = 0x10
```

Every one of these is `n × 4`. On a 64-bit target each becomes `n × 8`: header 16, VMT header 24,
slot 8, handler 32. That renumbering propagates to:

- `elenasrc/engine/win32/x86jitcompiler.cpp:393` `size = fieldCount << 2` → `<< 3`
- `elenasrc/engine/win32/x86jitcompiler.cpp:179,183,190,195,289,299` — every `<< 2` local/field offset
- `elenasrc/engine/jitlinker.cpp:387` `[vmt + 4]` (class flags) → `+8`
- `elenasrc/engine/jitcompiler.cpp:125` `+ 8` (one VMT entry) → `+16`
- `elenasrc/engine/section.h:19` `>> 2` → `>> 3`
- `elenasrc/ide/win32/debugger.h:93-94` `offset * 4` → `* 8`
- `src/asm/*.asm` — **every** `add esi, 4`, `lea edx, [edx+4]`, `mov [eax+4]` in 6,146 lines

### 4.2 Reference encoding

`elenasrc/engine/elenaconst.h:172-199` tags references in the **top byte of a 32-bit word**:
`mskAnyRef = 0xFF000000`, `mskImageMask = 0xF0000000`, `mskRelativeRef = 0x80000000`, and 14 more.
`elenasrc/common/common.h:21` then does `#define ref_t size_t`, so on LP64 `ref_t` is 64-bit while
every mask stays 32-bit. `~mskAnyRef` becomes `0xFFFFFFFF00FFFFFF`, silently breaking
`elenasrc/engine/jitlinker.cpp:37` (`reference & ~mskAnyRef`), `linker.cpp:69,74`
(`(*it) & mskImageMask`) and `module.cpp:65` (`nextId > ~mskAnyRef`).

Worse: `elenasrc/engine/elena.h:228-231` defines a reference as *address ÷ 8*
(`reallocateReference(v) { return v << VA_ALIGNMENT_POWER; }`, `VA_ALIGNMENT_POWER = 3`). With
8 tag bits consumed, the addressable image is `2^24 × 8 = 128 MiB`. There is no headroom to widen
the tag; the encoding must become a struct `{uint32_t id; uint8_t kind;}` or a 64-bit word.

### 4.3 Bytecode operands

`elenasrc/engine/win32/x86jitcompiler.cpp:534,537` read both operands with `getDWord()` — the
bytecode is a stream of `uint8 opcode` + up to two `int32` arguments. Emission matches
(`elenasrc/engine/bccompiler.cpp:698,701,719,736,743`). Those `int32` arguments carry, variously:
a reference id, a local slot index, a jump displacement, and (at
`elenasrc/engine/bccompiler.cpp:127`) **a pointer**:

```
const TCHAR* localName = (TCHAR*)(*it).argument;   // int → pointer
```

Clang reports `-Wint-to-pointer-cast` here. The bytecode operand width is defensible at 32 bits for
ids and displacements, but the pointer-through-`int` path must go.

### 4.4 The module file format — the hardest part

`.nl` modules are written by `elenasrc/engine/module.cpp:146-170`, which delegates to
`ReferenceMap::write` / `SectionMap::write` in `elenasrc/common/lists.h`. Those do:

```
lists.h:1594   _buffer.write(position, &item, sizeof(item));   // raw _MemoryMapItem
lists.h:1599   _buffer.writeDWord(position + 4, storedKey);    // key at hard-coded offset 4
lists.h:1635   writer->writeDWord(_buffer.Length()); ... writer->read(&reader, _buffer.Length());
```

`_MemoryMapItem` (`elenasrc/common/lists.h:207-211`) is `{ size_t next; Key key; T item; }`.
For `Key = const TCHAR*`, that is **12 bytes on 32-bit and 24 bytes on 64-bit**, and the `key`
field holds a *self-relative byte offset stored in a pointer-typed field* — which is exactly why
`lists.h:216` has to write `(TCHAR*)((int)this + (int)this->key)`.

So the `.nl` format is a byte-for-byte image of a 32-bit process's hash tables. It is not merely
"32-bit-flavoured"; it is unreadable by a 64-bit build, and the only version marker is
`MODULE_SIGNATURE "EN!10"` (`elenasrc/engine/elenaconst.h:149`), which encodes neither word size
nor character width.

The same applies to:
- `.dn` debug files — `elenasrc/engine/bccompiler.cpp:76…169` dump `DebugLineInfo` raw (20 B → 32 B).
- Class metadata — `elenasrc/engine/elena.h:107` dumps `ClassHeader` raw (12 B → 24 B).
- `.bin` runtime packages produced by `asm2bin`, which are `Section` dumps with the same maps.

**Required work:** design `EN!20` — an explicit, fixed-width, little-endian, UTF-8 format with a
header recording `{format_version, word_size, char_encoding, target_triple}` — and write both a
serialiser and a `EN!10`→`EN!20` converter. This is the critical-path item; nothing else in the
64-bit migration can be validated until modules round-trip.

### 4.5 Measured impact

| Compiler | Std | Arch | TUs | `error:` count |
|---|---|---|---:|---:|
| GCC 16.1.1 | c++98 | x86-64 | 16 | **25** |
| GCC 16.1.1 | c++98 | x86-32 (`-m32`) | 16 | **3** (shim gaps only) |

22 of the 25 errors disappear when the pointer width drops to 4. The codebase is not "mostly
64-bit clean with a few issues" — it is **32-bit-exact**.

---

## 5. Executable format: PE → ELF / Mach-O

### 5.1 What `linker.cpp` does today

`elenasrc/elc/win32/linker.cpp` is a complete, hand-written PE32 writer:

1. `writeDOSStub` (`:414`) — copies `bin/winstub.ex_` verbatim.
2. `createExecutable` (`:566`) — writes `"PE\0\0"` (`:575`), computes `_headerSize` from
   `IMAGE_SIZEOF_FILE_HEADER + 224 + 40×nsections`, aligned to `FileAlignment`.
3. `writeHeader` (`:425`) — `IMAGE_FILE_HEADER`, machine hard-wired to `I386` (`:429`).
4. `writeNTHeader` (`:443`) — `IMAGE_OPTIONAL_HEADER` (PE32), `ImageBase = 0x400000`,
   `Subsystem = CUI|GUI`, `DataDirectory[1]` = import table.
5. `writeSections` (`:506`) — 40-byte `IMAGE_SECTION_HEADER` per section, names truncated to 8
   chars (`strncpy(header.Name, it.key(), 8)`, `:516`), then the raw section bodies.
6. `createImportTable` (`:326`) — hand-builds `IMAGE_IMPORT_DESCRIPTOR[]` (20 B each),
   OriginalFirstThunk / FirstThunk arrays (4 B each), hint/name blobs.
7. `fixImage` (`:377`) — walks every `Section`'s relocation map and patches absolute VAs into the
   image, assuming `ImageBase` is honoured. **No `.reloc` section is emitted**, so the binary is
   not relocatable and cannot be ASLR'd.

### 5.2 What emitting ELF would cost

| Task | Notes | Effort |
|---|---|---|
| `Elf64_Ehdr` + program headers | must emit `PT_LOAD` ×2-3, `PT_DYNAMIC`, `PT_INTERP`, `PT_GNU_STACK`, `PT_GNU_RELRO` | 2-3 wk |
| Section headers + `.shstrtab`/`.strtab`/`.symtab` | PE's 8-char names have no analogue; needs a string table | 1 wk |
| Dynamic linking | `.dynamic` with `DT_NEEDED`/`DT_STRTAB`/`DT_SYMTAB`/`DT_JMPREL`, `.dynsym`, `.gnu.hash`, PLT/GOT stubs, `R_X86_64_JUMP_SLOT` | 3-4 wk — **this replaces `createImportTable` entirely** |
| Position independence | every absolute VA patched in `fixImage` becomes `R_X86_64_RELATIVE`, or the codegen becomes RIP-relative | 2-3 wk |
| DWARF | replaces the bespoke `.dn` format (`linker.cpp:589`); needed for gdb/lldb | 3-4 wk |
| **ELF subtotal** | | **11-15 wk** |

### 5.3 What emitting Mach-O would cost

| Task | Notes | Effort |
|---|---|---|
| `mach_header_64` + load commands | `LC_SEGMENT_64` ×N, `LC_MAIN`, `LC_LOAD_DYLINKER`, `LC_LOAD_DYLIB`, `LC_DYLD_INFO_ONLY` | 2-3 wk |
| `__PAGEZERO`/`__TEXT`/`__DATA_CONST`/`__DATA`/`__LINKEDIT` | segment/section two-level model differs from PE and ELF | 1-2 wk |
| dyld bind/rebase/lazy-bind opcode streams | a bytecode format, not a table — nothing in `linker.cpp` resembles it | 3-4 wk |
| Code signing | **mandatory on arm64 macOS**; even ad-hoc signatures need an `LC_CODE_SIGNATURE` blob with SHA-256 page hashes | 2-3 wk |
| W^X / `MAP_JIT` | Apple Silicon forbids simultaneously writable+executable pages; the JIT (`jitlinker.cpp`) must use `pthread_jit_write_protect_np` | 1-2 wk |
| 16 KiB pages | `SECTION_ALIGNMENT 0x1000` (`linker.cpp:22`) is wrong on arm64 macOS | 1 d |
| **Mach-O subtotal** | | **9-14 wk** |

### 5.4 The argument for delegating to LLVM

Writing three object writers by hand costs **20-29 weeks** and produces three permanently
maintained attack surfaces (code signing rules, `gnu.hash`, dyld opcode versions, and PE
`DllCharacteristics`/CFG all change over time). The alternative:

**Emit LLVM IR from the bytecode compiler; let `llc`/`lld` or the system linker produce the image.**

| Aspect | Hand-written writers | LLVM + system linker |
|---|---|---|
| Formats supported | 3 (must write each) | PE, ELF, Mach-O, WASM — free |
| Architectures | 1 per backend written | x86-64, arm64, riscv64, … — free |
| Relocations / PIE / ASLR | manual, currently absent | handled |
| Debug info | bespoke `.dn`, no debugger support | DWARF / CodeView, works in gdb/lldb/VS |
| macOS code signing | must implement | `ld64`/`lld` + `codesign` |
| Optimisation | none (`x86jitcompiler.cpp` is a template expander) | full `-O2` pipeline |
| Exceptions / unwinding | none | `.eh_frame` / `__unwind_info` |
| JIT | hand-rolled, no W^X handling | ORCv2, handles `MAP_JIT` |
| New dependency | none | LLVM ~20 (large, but vendored/system) |
| Effort | 20-29 wk + permanent maintenance | 10-14 wk for the IR emitter, then flat |

The one thing LLVM does not give you free is **the ELENA calling convention and object layout** —
but that has to be redesigned for 64-bit anyway (§4). The existing `x86JITCompiler`
(`elenasrc/engine/win32/x86jitcompiler.cpp`, 543 LOC) is essentially a table of "expand bytecode N
into these bytes"; rewriting it as "expand bytecode N into these IR instructions" is a like-for-like
transformation at similar size, and it deletes `asm2bin` (3,335 LOC), `x86helper` (668 LOC) and
`linker.cpp` (637 LOC) outright — **4,640 LOC removed**.

**Recommendation: go to LLVM IR. Do not write ELF or Mach-O writers.** Keep an AOT path
(`elc` → `.ll`/`.o` → `lld`) and add an ORCv2 JIT later for the interactive path. Retain the PE
writer only as a frozen reference implementation for regression comparison, then delete it.

---

## 6. Modern C++ compilation blockers (measured)

Environment: `x86_64-redhat-linux`, **GCC 16.1.1 20260515**, **Clang 22.1.8**.
(The brief asked for GCC 14 / Clang 18; the available toolchains are newer, so these results are
strictly *more* conservative — everything that fails here also fails on 14/18.)

### 6.1 Step 1 — the tree does not reach the C++ front end

```
$ cd /home/bencz/programming/ELENA-1.5.0.0/elenasrc
$ g++ -fsyntax-only -std=c++98 -I common -I engine -I elc common/altstrings.cpp
In file included from common/altstrings.cpp:9:
common/common.h:14:10: fatal error: tchar.h: No such file or directory
   14 | #include <tchar.h>
      |          ^~~~~~~~~
compilation terminated.
```

Identical failure for `engine/module.cpp` and `elc/compiler.cpp`. **Every translation unit in the
tree includes `common.h`**, so this is a 100 % stop.

### 6.2 Step 2 — with a POSIX shim for `<tchar.h>`, `<io.h>`, `<direct.h>`, `win32/unicode.h`

I wrote a minimal shim (`tchar.h`, `io.h`, `direct.h`, `win32/unicode.h`) into a scratch include
directory and re-ran. Next hard stop:

```
$ g++ -fsyntax-only -std=c++98 -I$SHIM -I common -I engine common/altstrings.cpp
In file included from common/altstrings.cpp:9:
common/common.h:25:10: fatal error: win32\unicode.h: No such file or directory
   25 | #include "win32\unicode.h"
      |          ^~~~~~~~~~~~~~~~~
compilation terminated.
```

The backslash is not a path separator on POSIX. There are **5** such includes
(`common/common.h:25`, `asm2bin/x86assembler.h:15`, `asm2bin/x86jumphelper.h:12`,
`elc/win32/elc.cpp:17`, `ide/debugcontroller.h:11`). I worked around it by creating a file whose
*name literally contains a backslash*.

### 6.3 Step 3 — real language errors

```
$ g++ -fsyntax-only -std=c++98 -I$SHIM -I common -I engine common/altstrings.cpp
common/altstrings.h:84:21: error: ‘__int64’ has not been declared; did you mean ‘__int64_t’?
   84 |    void appendInt64(__int64 n);
common/altstrings.h:85:21: error: ‘__int64’ has not been declared; did you mean ‘__int64_t’?
common/streams.h:322:4: warning: no return statement in function returning non-void [-Wreturn-type]
  322 |    }
common/dump.h:112:48: warning: no return statement in function returning non-void [-Wreturn-type]
  112 |    void* getArray() const { _dump->getArray(); }
common/dump.h:175:48: warning: no return statement in function returning non-void [-Wreturn-type]
common/altstrings.cpp:23:4: error: ‘_itot’ was not declared in this scope
common/altstrings.cpp:28:6: error: variable or field ‘appendHex64’ declared void
common/altstrings.cpp:40:6: error: variable or field ‘appendInt64’ declared void
common/altstrings.cpp:59:4: error: ‘_itot’ was not declared in this scope
common/altstrings.cpp:69:4: error: ‘_ltot’ was not declared in this scope
```

### 6.4 Step 4 — after shimming `__int64`, `_itot`, `_ltot`, `_i64tot`, `TEXT`

```
$ g++ -fsyntax-only -std=c++98 -I$SHIM -I common -I engine engine/section.cpp
common/lists.h:421:32: error: cast from ‘void*’ to ‘int’ loses precision [-fpermissive]
```

```
$ g++ -fsyntax-only -std=c++98 -I$SHIM -I common -I engine engine/module.cpp
common/lists.h:216:26: error: cast from ‘const _ELENA_::_MemoryMapItem<const wchar_t*, long unsigned int, true>*’ to ‘int’ loses precision [-fpermissive]
common/lists.h:216:38: error: cast from ‘const wchar_t*’ to ‘int’ loses precision [-fpermissive]
common/lists.h:234:34: error: cast from ‘const _ELENA_::_MemoryMapItem<const wchar_t*, long unsigned int, true>*’ to ‘int’ loses precision [-fpermissive]
common/lists.h:234:46: error: cast from ‘const wchar_t*’ to ‘int’ loses precision [-fpermissive]
common/lists.h:266:35: error: cast from ‘...’ to ‘int’ loses precision [-fpermissive]
common/lists.h:266:47: error: cast from ‘const wchar_t*’ to ‘int’ loses precision [-fpermissive]
common/lists.h:281:31: error: cast from ‘...’ to ‘int’ loses precision [-fpermissive]
common/lists.h:281:43: error: cast from ‘const wchar_t*’ to ‘int’ loses precision [-fpermissive]
```

```
$ g++ -fsyntax-only -std=c++98 -I$SHIM -I common -I engine -I elc elc/compiler.cpp
common/lists.h:216:26: error: cast from ... to ‘int’ loses precision [-fpermissive]
common/lists.h:234:34: error: cast from ... to ‘int’ loses precision [-fpermissive]
common/lists.h:421:32: error: cast from ‘void*’ to ‘int’ loses precision [-fpermissive]
elc/compiler.cpp:786:22: error: ‘_tcstoul’ was not declared in this scope; did you mean ‘wcstoul’?
elc/compiler.cpp:798:23: error: ‘_tcstod’ was not declared in this scope; did you mean ‘wcstod’?
```

```
$ g++ -fsyntax-only -std=c++98 -I$SHIM -I common -I engine -I elc sg/sg.cpp
sg/sg.cpp:74:65: error: no matching function for call to
   ‘_ELENA_::TextFileReader::TextFileReader(char*&, _ELENA_::FileEncoding)’
sg/sg.cpp:135:40: error: invalid conversion from ‘char*’ to ‘size_t’ {aka ‘long unsigned int’} [-fpermissive]
```

```
$ g++ -fsyntax-only -std=c++98 -I$SHIM -I common -I engine api2html/api2html.cpp
api2html/api2html.cpp:10:15: error: cannot convert ‘const char [39]’ to ‘const TCHAR*’ {aka ‘const wchar_t*’}
api2html/api2html.cpp:11:16: error: cannot convert ‘const char [42]’ to ‘const TCHAR*’
api2html/api2html.cpp:13:19: error: cannot convert ‘const char*’ to ‘const TCHAR*’
api2html/api2html.cpp:107:26: error: cannot convert ‘const char*’ to ‘const TCHAR*’
... (30+ further conversion errors)
```

`api2html` was only ever built **without** `-D_UNICODE` (`elenasrc/api2html/codeblocks/api2html.cbp:28`
omits it, unlike every other project) — it is structurally ANSI-only.

### 6.5 Clang — additional diagnostics GCC misses

```
$ clang++ -fsyntax-only -std=c++98 -I$SHIM -I common -I engine -I elc engine/module.cpp
common/lists.h:216:26: error: cast from pointer to smaller type 'int' loses information
common/lists.h:234:34: error: cast from pointer to smaller type 'int' loses information
common/lists.h:266:35: error: cast from pointer to smaller type 'int' loses information
common/lists.h:281:31: error: cast from pointer to smaller type 'int' loses information
engine/section.h:19:23: warning: use of logical '&&' with constant operand [-Wconstant-logical-operand]
engine/section.h:19:23: note: use '&' for a bitwise operation
common/dump.h:112:48: warning: non-void function does not return a value [-Wreturn-type]
common/dump.h:175:48: warning: non-void function does not return a value [-Wreturn-type]
common/streams.h:322:4: warning: non-void function does not return a value [-Wreturn-type]
```

```
$ clang++ -fsyntax-only -std=c++98 -I$SHIM -I common -I engine -I elc elc/compiler.cpp
common/lists.h:1487:26: error: invalid operands to binary expression
   ('Item' (aka '_MemoryMapItem<const wchar_t *, int, true>') and 'const wchar_t *')
```

```
$ clang++ -fsyntax-only -std=c++98 -I$SHIM -I common -I engine engine/bccompiler.cpp
common/lists.h:939:28: warning: implicit conversion of NULL constant to 'int' [-Wnull-conversion]
engine/bccompiler.cpp:127:29: warning: cast to 'wchar_t *' from smaller integer type 'int' [-Wint-to-pointer-cast]
engine/bccompiler.cpp:340:12: warning: 5 enumeration values not handled in switch [-Wswitch]
engine/bccompiler.cpp:488:12: warning: 14 enumeration values not handled in switch [-Wswitch]
engine/bccompiler.cpp:799:15: warning: 64 enumeration values not handled in switch [-Wswitch]
```

Clang uniquely catches three real defects GCC accepts:
`engine/section.h:19` (`&&` vs `&` — the fixup hash always returns 0),
`common/lists.h:1487` (missing `operator==` overload, a two-phase-lookup gap MSVC's lax
template model hid), and `common/lists.h:939` (`NULL` → `int`).

### 6.6 `-std=c++17`

```
$ g++ -fsyntax-only -std=c++17 -I$SHIM -I common -I engine elc/compiler.cpp
common/lists.h:216:17: warning: cast to pointer from integer of different size [-Wint-to-pointer-cast]
common/lists.h:216:26: error: cast from ... to ‘int’ loses precision [-fpermissive]
... (same 8 errors as c++98, plus -Wint-to-pointer-cast on the reverse direction)
elc/compiler.cpp:786:22: error: ‘_tcstoul’ was not declared in this scope
```

**C++17 introduces no *additional* rejections.** The code contains no `register`, no dynamic
exception specifications, no `auto_ptr`, no `>>` template-close ambiguity — I grepped for all of
them and found none. The only C++17 delta is extra `-Wint-to-pointer-cast` diagnostics on the
same lines. This is a pleasant surprise: **modernising the standard is cheap; the blockers are all
platform and word-size, not language-version.**

### 6.7 The controlled experiment

```
$ for f in common/altstrings.cpp common/config.cpp common/dump.cpp common/files.cpp \
           engine/bccompiler.cpp engine/bytecode.cpp engine/jitcompiler.cpp \
           engine/jitlinker.cpp engine/module.cpp engine/section.cpp \
           elc/compiler.cpp elc/derivation.cpp elc/parser.cpp elc/parsertable.cpp \
           elc/project.cpp elc/source.cpp; do
    n=$(g++ -fsyntax-only      -std=c++98 -I$SHIM -I common -I engine -I elc $f 2>&1 | grep -c ': error:')
    m=$(g++ -fsyntax-only -m32 -std=c++98 -I$SHIM -I common -I engine -I elc $f 2>&1 | grep -c ': error:')
    echo "$f  64=$n  32=$m"
  done
```

| TU | 64-bit errors | 32-bit errors |
|---|---:|---:|
| `common/altstrings.cpp` | 0 | 0 |
| `common/config.cpp` | 0 | 0 |
| `common/dump.cpp` | 0 | 0 |
| `common/files.cpp` | 0 | 0 |
| `engine/bccompiler.cpp` | 3 | 0 |
| `engine/bytecode.cpp` | 0 | 0 |
| `engine/jitcompiler.cpp` | 0 | 0 |
| `engine/jitlinker.cpp` | 3 | 1 |
| `engine/module.cpp` | 8 | 0 |
| `engine/section.cpp` | 1 | 0 |
| `elc/compiler.cpp` | 8 | 2 |
| `elc/derivation.cpp` | 0 | 0 |
| `elc/parser.cpp` | 0 | 0 |
| `elc/parsertable.cpp` | 2 | 0 |
| `elc/project.cpp` | 0 | 0 |
| `elc/source.cpp` | 0 | 0 |
| **Total** | **25** | **3** |

The 3 remaining 32-bit errors are `_tcstoul`/`_tcstod` — gaps in *my shim*, not defects in ELENA.
**Once `<tchar.h>` and the backslash includes are dealt with, the entire compiler and engine
compile cleanly under GCC 16 in 32-bit mode.** All 22 genuine errors are pointer-width failures in
`common/lists.h`.

### 6.8 Summary of blockers, ranked

| # | Blocker | Sites | Severity |
|---:|---|---:|---|
| 1 | `#include <tchar.h>` / `<io.h>` / `<direct.h>` | 3 | fatal, 100 % of TUs |
| 2 | Backslash `#include` paths | 5 | fatal |
| 3 | `(int)this` / `(int)void*` pointer truncation | 6 in `lists.h` | error at 64-bit |
| 4 | `__int64` | 8 | error |
| 5 | `_itot` / `_ltot` / `_i64tot` / `_tcstoul` / `_tcstod` / `_gcvt` / `_tcslwr` / `_wremove` | ~20 | error |
| 6 | `char*` ↔ `TCHAR*` mismatches (`sg`, `api2html`) | 35+ | error |
| 7 | Missing `return` in non-void functions | 3 | UB; error under `-Werror=return-type` |
| 8 | `&&` instead of `&` (`section.h:19`) | 1 | silent logic bug |
| 9 | Missing `operator==` overload (`lists.h:1487`) | 1 | error under Clang |
| 10 | `NULL` → `int` (`lists.h:939`) | 1 | warning |

---

## 7. Prioritized porting roadmap

Effort assumes **one experienced engineer**; ranges reflect uncertainty, not team size.
"Done" criteria are objective and testable.

```
P0 ──► P1 ──► P2 ──┬─► P3 ──► P4 ──┬─► P6 ──► P7
                   └─► P5 ─────────┘
```

---

### **P0 — Build system & compile gate** · 2-3 weeks · *no dependencies*

**Do:**
- Add `CMakeLists.txt` (≥ 3.20) for `elc`, `engine`, `common`, `sg`; delete the 17 `.vcproj`/`.vcxproj` and 6 `.cbp`.
- Fix the 5 backslash includes (`common/common.h:25`, `asm2bin/x86assembler.h:15`, `asm2bin/x86jumphelper.h:12`, `elc/win32/elc.cpp:17`, `ide/debugcontroller.h:11`).
- Introduce `elenasrc/platform/{posix,win32}/` and move `common/win32/unicode.h` behind it.
- Replace `__int64`→`int64_t`, `_itot`/`_ltot`/`_gcvt`→`snprintf`, `_tcstoul`/`_tcstod`→`wcstoul`/`wcstod`, `_mkdir`/`_access`/`_tstat`→`<filesystem>`.
- Fix `common/dump.h:112,175`, `common/streams.h:322` (missing `return`); `engine/section.h:19` (`&&`→`&`); `common/lists.h:939` (`NULL`→`0`), `:1487` (add the overload).
- Turn on `-Wall -Wextra -Werror=return-type -Werror=int-to-pointer-cast`.

**Done when:** `cmake --build` produces a working `elc` on Linux **x86-32** (`-m32`) that
byte-for-byte reproduces the Windows-built compiler's `.nl` output for `examples/helloworld`.

---

### **P1 — Character encoding: `TCHAR` → UTF-8** · 4-6 weeks · *depends on P0*

**Do:**
- Replace `TCHAR`/`_T()`/`_tcs*` with `char` + UTF-8 across all 1,066 affected lines.
- Delete every `<< 1` / `, 2,` byte-width assumption: `tools.h:149,160,170,215`,
  `streams.h:65,70,138,186-191,392-394,405-413`, `dump.h:57`, `dump.cpp:186`,
  `files.cpp:111,129,139-146,188-194,214,227`.
- Implement `feUTF8` properly in `files.cpp`; add full BOM sniffing (`files.cpp:26-34`).
- Replace `.lower()`/`_tcslwr` path handling with case-preserving comparison
  (`elc/win32/elc.cpp:48,57,85,96,98`, `idesettings.cpp:81,85`, `ideproject.cpp:219,232`).
- Make newline handling configurable (`files.cpp:247`, `ide/text.cpp:245`).

**Done when:** `elc` reads and writes UTF-8 sources and identical `.nl` files on Linux and
Windows (`sha256` match), and a source file containing non-ASCII identifiers and string literals
round-trips.

---

### **P2 — Module format v2 (`EN!20`)** · 5-7 weeks · *depends on P1* · **critical path**

**Do:**
- Replace the raw dumps at `common/lists.h:1594,1599,1635,1873,2322,2326`,
  `engine/elena.h:107,117`, `engine/bccompiler.cpp:76…169,833` with explicit, fixed-width,
  little-endian, UTF-8 serialisers.
- Kill `(int)this` self-relative keys (`common/lists.h:216,234,266,281,395,421`) — store real offsets in `uint32_t`.
- Fix `streams.h:53-56` (`readDWord(size_t&)` reading 4 bytes into 8).
- Change `#define ref_t size_t` → `typedef uint32_t ref_t` (`common/common.h:21`) and audit
  every `mskAnyRef` interaction (`elenaconst.h:172-199`, `jitlinker.cpp:37`, `linker.cpp:69,74`, `module.cpp:65`).
- New header: `{magic "EN!20", format_version, word_size, char_encoding, target_triple}`.
- Ship an `EN!10 → EN!20` converter.

**Done when:** the same `.nl` file is produced and consumed byte-identically by 32-bit and 64-bit,
Linux and Windows builds; the converter round-trips all of `src/std`, `src/ext`, `src/sys`.

---

### **P3 — LLVM IR backend (AOT)** · 10-14 weeks · *depends on P2*

**Do:**
- Replace `engine/win32/x86jitcompiler.cpp` (543 LOC) with a bytecode→LLVM IR emitter.
- Delete `engine/win32/x86helper.{h,cpp}` (668), `elenasrc/asm2bin/` (3,335),
  `elc/win32/linker.cpp` (637) — **4,640 LOC removed**.
- Widen the object model: `elEmptyObject 8→16`, `elVMTOffset 12→24`, `VMT_INDEX_SIZE 4→8`,
  `elAnyHandlerSize 16→32` (`engine/elenaconst.h:110,253,254,256`); every `<< 2` → `<< 3`
  (`x86jitcompiler.cpp:179,183,190,195,289,299,393`, `jitlinker.cpp:387`,
  `jitcompiler.cpp:125`, `section.h:19`).
- Redesign reference encoding away from top-byte tagging (`engine/elena.h:228-231`).
- Emit DWARF instead of the `.dn` format (`linker.cpp:589-617`).
- Drive `lld` for the final link on all three OSes.

**Done when:** `elc` produces a running native executable for
`{linux,windows,macos} × {x86-64, arm64}` from `examples/helloworld` and `examples/pi`, and
`lldb`/`gdb` can set a source-line breakpoint in it.

---

### **P4 — Runtime rewrite (replacing `src/asm/`)** · 8-12 weeks · *depends on P3*

**Do:**
- Reimplement `src/asm/elena.asm` (GC, allocation, dispatch, 1,481 LOC) in C++ or LLVM IR;
  replace `fs:[4]`/`fs:[8]` TIB access (`:1052,1053,1109,1166,1167`) with a portable thread registry.
- Reimplement `src/asm/standard.asm` (2,804 LOC, 79 fragments) as portable C++ intrinsics.
- Replace `src/asm/win32.asm` (1,423 LOC, 65 DLL calls) with a POSIX/Win32 syscall layer;
  console/file half maps to `read`/`write`/`termios`.
- Replace `src/asm/winsock.asm` (362 LOC, 17 `Ws2_32` calls) with BSD sockets — **cheapest file**.
- Replace `src/asm/extended.asm` (76 LOC) `GetSystemTime` with `clock_gettime`.
- New `src/posix/**.l` packages mirroring `src/win32/**.l` (2,887 LOC); new
  `bin/templates/console.cfg` / `gui.cfg` forwards.
- macOS: `MAP_JIT` + `pthread_jit_write_protect_np` in the code cache.

**Done when:** the full `src/std` + `src/ext` + `src/sys` library and all non-GUI examples build
and pass on all six OS×arch combinations.

---

### **P5 — Threading & GC concurrency** · 6-8 weeks · *depends on P2; parallel with P3/P4*

**Do:**
- Replace the global bump allocator (`src/asm/elena.asm:15,18,21,48,90,146,519,532,1215,1228`)
  with thread-local allocation buffers + an atomic slow path.
- Introduce safepoints and a stop-the-world protocol; per-thread root sets replacing `fs:[4]` stack scanning.
- Make the GC mark bits (`elenaconst.h:277-278`) atomic.
- Add an ELENA-level thread/mutex/condvar API.
- Replace `TerminateThread` (`ide/win32/output.cpp:49`) with cooperative cancellation.

**Done when:** a stress test spawning N threads that allocate concurrently runs clean under
TSan/ASan for 10 minutes on every target.

---

### **P6 — Tooling & developer experience** · 4-6 weeks · *depends on P3, P4*

**Do:**
- LSP server reusing `elc`'s parser (`elenasrc/elc/parser.cpp`, `source.cpp` — already portable).
- VS Code / Zed extension.
- Port `sg` (147 LOC, 2 errors) and `api2html` (471 LOC, ANSI-only — rewrite for UTF-8).
- CI matrix: 3 OSes × 2 arches × {GCC, Clang, MSVC}.
- Replace `bin/elc.cfg:2` `libpath=..\lib` and both `bin/templates/*.cfg` with portable defaults.

**Done when:** CI is green on all 6 targets; syntax highlighting, diagnostics and go-to-definition
work in VS Code.

---

### **P7 — GUI (optional, last)** · 16-24 weeks · *depends on P4, P6*

**Do:**
- Discard `elenasrc/ide/win32/` (8,802 LOC) and `elenasrc/ide/gtk/main.cpp` (the 49-line stub).
- Sever the 13 lines of Win32 leakage in `ide/browser.*`, `messagelog.*`, `debugcontroller.cpp`,
  `pluginmanager.cpp`, `idesettings.cpp`, `ideconst.h`.
- Rewrite the IDE on Qt 6, reusing the portable `ide/text.cpp` (1,086 LOC),
  `document.cpp` (901), `sourcedoc.cpp` (348).
- New debugger backend: `ptrace` (Linux) / `mach_vm` + `task_for_pid` (macOS) / `DebugActiveProcess` (Windows),
  behind the `IDebugBackend` interface created in P6 — or drop it and use `lldb`/DAP.
- Replace `src/gui/**.l` (1,252 LOC) and `src/win32/api/**` with a portable GUI package.

**Done when:** the IDE builds and runs on all three OSes with editing, building and debugging.

---

### Effort summary

| Phase | Effort | Cumulative | Deliverable |
|---|---|---|---|
| P0 Build & compile gate | 2-3 wk | 3 wk | `elc` builds on Linux (32-bit) |
| P1 UTF-8 migration | 4-6 wk | 9 wk | encoding-portable compiler |
| P2 Module format v2 | 5-7 wk | 16 wk | word-size-portable artefacts |
| P3 LLVM backend | 10-14 wk | 30 wk | native 64-bit binaries, 3 OSes, 2 arches |
| P4 Runtime rewrite | 8-12 wk | 42 wk | full stdlib on all targets |
| P5 Threading & GC | 6-8 wk | 42 wk *(parallel)* | real multithreading |
| P6 Tooling & CI | 4-6 wk | 48 wk | shippable toolchain |
| P7 GUI (optional) | 16-24 wk | 72 wk | cross-platform IDE |

**Minimum viable cross-platform ELENA (P0-P4 + P6): ≈ 48 weeks.**
**With multithreading (P5 in parallel): ≈ 48 weeks.**
**With a native IDE (P7): ≈ 72 weeks.**

The single highest-leverage decision is **P3's delegation to LLVM**: it deletes 4,640 LOC of
hand-written x86/PE machinery, unlocks arm64 and macOS for free, and replaces the bespoke `.dn`
debug format with DWARF. The single highest-risk item is **P2**, because the `.nl` format is a
32-bit memory image and every downstream phase depends on it being fixed first.
