# Building on Linux

> **Status: working.** `sg` and `elc` build and run on Linux. The full standard
> library (28 modules) and the examples compile to `.sem` bytecode.
>
> **Linking is not available yet** — see §5.

---

## 1. Quick start

```bash
cmake -S . -B build
cmake --build build -j

./bin/sg dat/sg/syntax.txt        # generate the LL(1) parser table
cp dat/sg/syntax.dat bin/         # elc loads it from its own directory

mkdir -p lib
./bin/elc -lstd -olib src/elena.sel                 # the root module
./bin/elc -csrc/std/std.prj
./bin/elc -csrc/sys/sys.prj
./bin/elc -csrc/ext/ext.prj
./bin/elc -csrc/gui/gui.prj
./bin/elc -csrc/win32/win32.prj                   # must follow gui
./bin/elc -csrc/win32/socket/win32socket.prj
```

Result: 28 `.sem` modules and 27 `.sdm` debug modules under `lib/`.

Two things that are easy to get wrong:

- **`-c` takes no space**: `-csrc/std/std.prj`, not `-c src/std/std.prj`.
- **Do not pass `-g` when compiling `src/elena.sel`.** The standard module must be
  built with no package option; passing one used to corrupt the output file name
  (see §4).

## 2. What was needed

| Area | Change |
|---|---|
| POSIX compat layer | New `elenasrc/common/posix/`: `tchar.h`, `io.h`, `direct.h`, `unicode.h`. Placed on the include path so the original `#include <tchar.h>` and `<io.h>` resolve to it with no source change |
| Build system | New root `CMakeLists.txt`, replacing the CodeBlocks `.cbp` and Visual Studio `.vcproj` files. There was no makefile or build script of any kind in the tree |
| Backslash includes | 4 sites fixed (`common.h`, `x86assembler.h`, `x86jumphelper.h`, `elc.cpp`, `debugcontroller.h`) |
| Character width | 9 sites assuming `sizeof(wchar_t) == 2` |
| Path separators | Both `/` and `\` now accepted everywhere; the native one is used when building paths |
| Wide printf | 60 format specifiers: `%s` → `%ls` |
| Platform split | `elc/win32/elc.cpp` split into shared `elc/elcproject.cpp` plus per-platform front ends |

### The compat layer's one deliberate compromise

`TCHAR` is `wchar_t`, which is **4 bytes on Linux and 2 on Windows**. So `.sem`
modules produced on Linux are **not byte-compatible** with those produced on
Windows — the string tables differ in character width, and the format records
neither the width nor a version number, so a mismatch would go undetected.

This is accepted for now because no prebuilt `.sem` exists anywhere, each
platform's toolchain is self-consistent, and the real fix is the planned UTF-8
migration (P1) followed by module format v2 (P2).

## 3. Bugs found and fixed

All four were pre-existing defects, not porting artifacts.

| Bug | Location | Effect |
|---|---|---|
| **In-place ANSI→wide expansion hard-coded 2-byte characters** | `common/files.cpp:126`, `:177` | Every source file read dropped every second character. `__define` became `_dfn`, so `sg` reported the grammar as ambiguous when it is not. Also written as byte arithmetic, so it would have been wrong on any big-endian target |
| **`getName()` read past the string terminator** | `elc/project.cpp:44` | When the module name was no longer than the package, it advanced past the NUL into uninitialised heap. Output file names were built from freed memory and differed on every run |
| **`getArray()` missing `return`** ×2 | `common/dump.h:112`, `:175` | Undefined behaviour |
| **`sizeof(wchar_t)` assumed to be 2** ×9 | `tools.h`, `streams.h`, `dump.h`, `dump.cpp` | Half-sized allocations; heap overflow on any 4-byte-`wchar_t` platform |

The first one is worth dwelling on: it made a **correct grammar look ambiguous**.
Anyone porting this without checking would reasonably have concluded that
`dat/sg/syntax.txt` was broken and started editing the grammar.

## 4. Things that look like bugs and are not

- **`Link ... is unresolvable` warnings** during library compilation are normal.
  These are forward references resolved at link time.
- **`win32.prj` must be compiled after `gui.prj`.** It depends on
  `lib/gui/graphics.sem`. Compiling in the wrong order gives
  `error 201: Unknown module`.
- **`.cfg` and `.prj` files keep Windows separators** (`libpath=..\lib`,
  `basic\memory.sel`). They are not rewritten, because those same strings are also
  parsed by `pathToName()` to derive module names. Backslashes are normalised at
  the point a path becomes a syscall.

## 5. What is deliberately not built

| Component | Why |
|---|---|
| `asm2binx` | Its `.bin` output contains x86 assembly calling `kernel32`/`user32`, and the only consumer is the PE linker. It cannot produce anything usable on Linux. It already builds on Windows and needs no port |
| `elc/win32/linker.cpp` | Emits PE/COFF only |
| `engine/win32/x86jitcompiler.cpp` | Emits Win32-hosted x86 |
| `elide` (the IDE) | 8,802 of 14,551 lines are raw Win32; `ideconst.h` is the only file in `ide/` that compiles without `windows.h` |

So the POSIX `elc` stops after byte code generation and says so. This is a
boundary, not an unfinished edge: both the linker and the code generator are to
be replaced by an LLVM backend plus a system linker.

**Consequence:** the library compiles on Linux even though the runtime is still
Win32 assembly, because `#inline pkg'N` and `#external` are *references recorded
in the module* and are only resolved at link time.

## 6. Build configuration

| Option | Default | Meaning |
|---|---|---|
| `ELENA_32BIT` | `ON` | Required. The object model, bytecode operands and `.sem` format all assume 32-bit pointers; a 64-bit build produces 25 hard errors and silently different file formats |
| `ELENA_BUILD_SG` | `ON` | Syntax generator |
| `ELENA_BUILD_ELC` | `ON` | Compiler |
| `ELENA_BUILD_ASM2BIN` | `${WIN32}` | Windows only, by design |

Verified with GCC 16.1.1 and CMake 4.3.0 on Fedora, target `-m32`.
Requires 32-bit libstdc++ and glibc development packages.

## 7. Next

1. **UTF-8 internally** (P1) — removes `TCHAR`, makes Linux and Windows `.sem`
   identical, and eliminates byte-order concerns for the big-endian targets.
2. **Module format v2** (P2) — explicit serialization, magic and version, slot
   indices instead of byte offsets. Blocks all codegen verification, so it is the
   critical path.
3. Only then the LLVM backend, which is what makes linking possible again — on
   every platform at once rather than one at a time.

`syntax.dat` has the same defect class as `.sem`: no magic, no version, and it
embeds `sizeof(size_t)` and `sizeof(TCHAR)` (`parsertable.cpp:208`). A 64-bit
`sg` feeding a 32-bit `elc` loads garbage silently. Fixing that is ~10 lines and
should happen before the 32→64-bit transition starts.
