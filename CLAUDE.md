# Selene

A modernization of **ELENA Language 1.5.0.0** (2009, Alex Rakov, Apache 2.0) — a
pure message-passing object-oriented language in the Smalltalk lineage.

The original is Windows-only, x86-32, with a hand-written code generator and a
runtime written in x86 assembly. Selene takes that language and rebuilds the
implementation around LLVM, portable C, and multiple architectures.

> The upstream project (`elena-lang`) is still actively developed by its author.
> Selene is a divergent rewrite of the implementation, not a continuation of it.
> Original copyright headers must be preserved.

---

## Where things are

| Path | Contents |
|---|---|
| `docs/` | **The reference.** ~17,000 lines documenting the system as it is, plus the design proposals. Read `docs/README.md` first |
| `elenasrc/` | C++ toolchain: `common/`, `engine/`, `elc/`, `asm2bin/`, `ide/`, `sg/` |
| `src/` | ELENA standard library (`.l`) and the legacy x86 runtime (`src/asm/`) |
| `examples/` | 14 sample programs |
| `bin/` | `elc.cfg`, project templates, `winstub.ex_` — source data, not artifacts |

Start with `docs/plan/15-modernization-roadmap.md` for the synthesis of what is
hard and in what order.

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

Produces `build/bin/{sg,elc}` plus a generated `syntax.dat`. Then:

```bash
mkdir -p lib
build/bin/elc -lstd -olib src/elena.l
build/bin/elc -p$PWD/lib -csrc/std/std.prj      # then sys, ext, gui, win32, socket
```

Details, including the two things that are easy to get wrong, are in
`docs/build/21-building-on-linux.md`.

Requires a 32-bit toolchain (`glibc-devel.i686`, `libstdc++-devel.i686`).

## Invariants — do not violate these

**1. No platform conditionals in shared code.**
`#ifdef _WIN32` in a file that both platforms compile is a defect. Use a
per-platform source directory (`elc/posix/`, `elc/win32/`) or add an entry to
`Platform::` in `common/<platform>/platform.h`. The build system selects the
file; the code never asks where it is running. See
`docs/architecture/22-platform-layer.md`.

**2. UTF-8 is the character model.**
`ELENA_UTF8=ON` is the default. The `wchar_t` path still compiles for A/B
comparison and is scheduled for deletion. Do not add code that assumes a
particular character width — and never assume `sizeof(wchar_t) == 2`.

**3. Character values are indexed unsigned.**
`TCHAR` is signed in both models. Any byte above 127 arrives negative. Table
lookups must mask to the actual width first (see `dfaColumn` in `elc/dfa.h`).

**4. The 32-bit build is not a preference, it is a requirement.**
The object model, bytecode operands and `.nl` format all assume 32-bit pointers.
A 64-bit build produces hard errors and, worse, silently different file formats.
This is lifted by module format v2, not before.

**5. Do not port `asm2binx`, the PE linker, or the x86 JIT to POSIX.**
Their output is unusable off Windows by construction. They are replaced by the
LLVM backend, not ported. `ELENA_BUILD_ASM2BIN` defaults to Windows-only.

**6. `.cfg` and `.prj` files keep Windows path separators.**
Those same strings feed `pathToName()` to derive module names, so rewriting them
breaks module naming. Separators are normalized where a path becomes a syscall.

## Design decisions already taken

| Decision | Where it is argued |
|---|---|
| LLVM backend rather than hand-written ELF/Mach-O emitters | `docs/plan/17-llvm-backend-and-targets.md` |
| Runtime rewritten in **C11**, not C++ and not compiler-emitted IR — LTO gives inlining, and freestanding is required for the OS goal | `docs/plan/19-runtime-in-c.md` |
| Failure channel becomes `{ptr, i1}`; `fail:` gains an argument | `docs/plan/16-syntax-evolution.md` §S1 |
| `#inline pkg'N` replaced by named typed intrinsics | `docs/plan/16-syntax-evolution.md` §S2 |
| FFI reuses the existing link-time *forwards* mechanism | `docs/plan/18-ffi-design.md` |
| IDE is not ported; LSP + DAP instead | `docs/architecture/08-ide-debugger.md` |
| Target order changes one variable at a time: x86-64 → arm64 → s390x → ppc | `docs/plan/17-llvm-backend-and-targets.md` §10 |

## Where the project is going

1. ✅ Build on Linux, CMake, UTF-8, module header v2
2. Module format v2 payload — canonical little-endian, slot indices not byte
   offsets. **Critical path**: blocks all codegen verification
3. LLVM IR backend, differential-tested against the legacy x86 path
4. Runtime rewritten in C, `src/asm/` deleted
5. Threading (MTA/STA) and a GC that supports both
6. Long term: an operating system written in Selene

Targets: Linux, Windows, macOS × x86-64, arm64, ppc64/ppc64le, ppc32, s390x —
including **big-endian**, which is why byte order is treated explicitly.

## Working notes

- The tree had **no version control, no makefile and no build script** when this
  started. Several bugs found so far were pre-existing defects from 2009, not
  porting artifacts — they are listed in `docs/plan/15-modernization-roadmap.md`
  §6 and `docs/build/21-building-on-linux.md` §3.
- `doc/` (singular) is the original 2009 documentation. It is partly wrong —
  `doc/tech/bytecode.txt` omits 12 of 40 opcodes and lists two that do not
  exist. `docs/` (plural) supersedes it.
- Reply to the user in **Portuguese**; write code, comments and documentation in
  **English**.
