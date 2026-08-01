# Selene

A modernization of **ELENA Language 1.5.0.0** (2009, Alex Rakov, Apache 2.0) — a
pure message-passing object-oriented language in the Smalltalk lineage.

The original was Windows-only, x86-32, with a hand-written code generator and a
runtime written in x86 assembly. Selene rebuilds the implementation around
LLVM, portable C, and multiple architectures. The 2009 toolchain (asm2binx,
the PE linker, the x86 JIT, the assembly runtime) has been **deleted**: byte
code goes through `llvmgen` and the system linker on every platform.

> The upstream project (`elena-lang`) is still actively developed by its author.
> Selene is a divergent rewrite of the implementation, not a continuation of it.
> Original copyright headers must be preserved.

---

## Where things are

| Path | Contents |
|---|---|
| `docs/` | **The reference.** ~17,000 lines documenting the system as it is, plus the design proposals. Read `docs/README.md` first |
| `elenasrc/` | C++ toolchain: `common/`, `engine/`, `elc/`, `llvmgen/`, `semdump/`, `sg/` |
| `runtime/` | The C runtime: object model, dispatch, GC, natives; `posix/` and `win32/` platform bindings |
| `src/` | Selene standard library (`.sel`), including `posix/` and `win32/` console bindings |
| `examples/` | Sample programs |
| `bin/` | `elc.cfg`, project templates, `targets/<os>.cfg` platform forwards — source data, not artifacts |

## Build and use

```bash
cmake -S . -B build-64 -DELENA_32BIT=OFF
cmake --build build-64 -j
```

Produces `build-64/bin/{sg,elc,semdump}` and `build-64/libselene.a`. LLVM
(llvm-devel) is REQUIRED — elc *is* the LLVM compiler; there is no fallback
mode. Then, to build the library and a program:

```bash
mkdir -p lib
build-64/bin/elc --target=x64 -lstd -olib src/elena.sel
build-64/bin/elc --target=x64 -p$PWD/lib -csrc/std/std.prj     # then sys, ext, posix, gui, win32 (2 rounds)

build-64/bin/elc --target=x64 -p$PWD/lib -cexamples/helloworld/helloworld.prj
examples/helloworld/helloworld          # compiles, links and runs
```

`--target=x64-win` produces a Windows executable the same way (needs
`libselene-<target>.a` next to `libselene.a`, built with mingw). The
`--llvm-translate` flag is a debugging tool exposing the same machinery
without a project.

## Invariants — do not violate these

**1. No platform conditionals in shared code.**
`#ifdef _WIN32` in a file that both platforms compile is a defect. Use a
per-platform source directory (`elc/posix/`, `runtime/posix/`,
`runtime/win32/`) or add an entry to `Platform::` in
`common/<platform>/platform.h`. The build system selects the file; the code
never asks where it is running. See `docs/architecture/22-platform-layer.md`.

**2. UTF-8 is the character model — compiler AND runtime.**
All runtime text is UTF-8; a literal's payload is `[u32 byte length][UTF-8
bytes][NUL]`. Conversion happens only at an OS boundary, inside that
platform's runtime directory (Windows sets the console code pages to UTF-8 at
startup and converts nowhere by default). See
`docs/plan/19-runtime-in-c.md` §8.1. Never assume a character width.

**3. Character values are indexed unsigned.**
`TCHAR` is signed. Any byte above 127 arrives negative; table lookups must
mask to the actual width first (see `dfaColumn` in `elc/dfa.h`).

**4. Target properties never derive from the host.**
Slot sizes, byte order, pointer width come from `TargetInfo` — never
`sizeof(void*)`. The single exception is `getDefaultTarget()`.

**5. `.cfg` and `.prj` files keep Windows path separators.**
Those same strings feed `pathToName()` to derive module names, so rewriting
them breaks module naming. Separators are normalized where a path becomes a
syscall.

**6. The VMT load at a send site must never be `!invariant.load` or hoisted.**
`#shift` rewrites a live object's VMT at run time; an object's class is not a
compile-time fact.

**7. The failure result is ONE packed word.**
`selene_result` = value with the ok flag in bit 0 (objects are slot-aligned).
A two-field struct silently breaks the Microsoft x64 ABI. The packing lives
in exactly four helpers in `runtime/selene.h` and four in `llvmgen.cpp`.
See `docs/plan/23-failure-abi.md` §4.

## Design decisions already taken

| Decision | Where it is argued |
|---|---|
| LLVM backend + system linker; `elc -c<prj>` compiles, translates and links | `docs/plan/17-llvm-backend-and-targets.md` |
| Runtime in **C11** (freestanding-capable), static, LTO | `docs/plan/19-runtime-in-c.md` |
| Failure ABI: `{value, ok}` packed in one word, never unwind | `docs/plan/23-failure-abi.md` |
| Name-based linking: `selene.<tag>.<resolved name>`, `'` `:` `#` become dots | `elenasrc/llvmgen/llvmgen.h` header comment |
| Messages intern into one global id space per link unit | `elc/posix/elc.cpp` (link pipeline) |
| FFI: typed declarations, per-target via LLVM, platform selection by forwards | `docs/plan/18-ffi-design.md` |
| OS selection by forwards at link (`targets/<os>.cfg`); posix'io / win32'console | `docs/plan/20-os-development.md` |
| IDE is not ported; LSP + DAP instead | `docs/architecture/08-ide-debugger.md` |

## Where the project is going

1. ✅ Linux build, CMake, UTF-8, module format v2
2. ✅ LLVM backend: exact CFG stack-depth analysis, executes correctly
3. ✅ Name-based cross-module linking; one-command `elc -c<prj>` pipeline;
   same source runs on Linux and Windows (validated under Wine)
4. ✅ Legacy toolchain deleted
5. Typed FFI (plan 18) — then the io natives migrate into Selene source and
   the runtime shrinks toward GC + dispatch + object model
6. Real C bodies for the `standard'*` value natives; lazy initialisation for
   non-computable constant symbols
7. Threading (MTA/STA), shadow stack, a real collector
8. Long term: an operating system written in Selene

Targets: Linux, Windows, macOS × x86-64, arm64, ppc64/ppc64le, ppc32, s390x —
including **big-endian**, which is why byte order is treated explicitly.

## Working notes

- `doc/` (singular) is the original 2009 documentation and is partly wrong;
  `docs/` (plural) supersedes it. Known 2009 defects are catalogued in
  `docs/plan/15-modernization-roadmap.md` §6.
- The 32-bit build (`ELENA_32BIT=ON`) survives only for byte-compare tests of
  the module format; it builds no elc.
- Reply to the user in **Portuguese**; write code, comments and documentation
  in **English**.
