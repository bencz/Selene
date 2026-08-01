# Selene

A pure message-passing object-oriented language in the Smalltalk lineage, and a
modern implementation of it.

Selene is a divergent rewrite of **ELENA Language 1.5.0.0** (2009, Alex Rakov,
Apache 2.0) — a language whose ideas aged remarkably well, wrapped in an
implementation that could only ever run on Windows and x86-32. The language is
kept. The implementation is being rebuilt around LLVM, portable C, and a
multi-architecture, multi-OS target set.

> The original project is still actively developed by its author as
> [`elena-lang`](https://github.com/ELENA-LANG/elena-lang). Selene is not a
> continuation of it and does not track it. Original copyright headers are
> preserved throughout.

---

## The language in thirty seconds

Everything is an object; everything is a message send; a send either **succeeds
with a value** or **fails** — and failure is the control-flow mechanism, not an
error mechanism.

```elena
#symbol Program =
{
    proceed
    [
        'program'output << "Hello World!!%n".
    ]
}.
```

Two features are unusual enough to be worth naming:

- **Roles ("shift")** — an object can rewrite its own VMT pointer at run time
  and change which methods answer. It is how the standard library expresses
  sentinels, state machines and lazy initialization without null checks or enums.
- **Annex / cast** — an object can delegate everything it does not understand to
  another object, installed as an any-message handler. Composition of protocols
  without inheritance.

A full reference for the dialect is in
[`docs/language/10-elena-language-reference.md`](docs/language/10-elena-language-reference.md).
None existed before; it was reconstructed from the grammar, the compiler and the
library sources.

## Status

**Early. The compiler builds and runs on Linux; it does not link yet.**

| | |
|---|---|
| Compiler (`elc`) on Linux | ✅ compiles sources to `.sem` bytecode modules |
| Syntax generator (`sg`) | ✅ |
| Standard library | ✅ 28 modules compile |
| Character model | ✅ UTF-8 |
| Module format | ✅ v2, with a versioned header |
| **Linking / executables** | ❌ waiting on the LLVM backend |
| Windows, macOS | ❌ not yet built |
| 64-bit | ❌ blocked on module format v2 payload |

Linking is deliberately absent rather than unfinished: the 2009 linker emits
PE/COFF only and the code generator emits Win32-hosted x86, so neither can
produce anything runnable off Windows. Both are replaced by LLVM, not ported.

## Build

Requires CMake, a C++17 compiler, and a 32-bit toolchain
(`glibc-devel.i686`, `libstdc++-devel.i686` on Fedora).

```bash
cmake -S . -B build
cmake --build build -j

mkdir -p lib
build/bin/elc -lstd -olib src/elena.sel
build/bin/elc -p$PWD/lib -csrc/std/std.prj      # then sys, ext, gui, win32, socket
```

Full instructions, including the two things that are easy to get wrong, are in
[`docs/build/21-building-on-linux.md`](docs/build/21-building-on-linux.md).

## Where this is going

| Phase | |
|---|---|
| 1 | ✅ Build on Linux — CMake, UTF-8, versioned module header |
| 2 | Module format v2 payload — canonical little-endian, slot indices instead of byte offsets. **Critical path** |
| 3 | LLVM IR backend, differential-tested against the legacy x86 path |
| 4 | Runtime rewritten in C11; `src/asm/` deleted |
| 5 | Threading (MTA/STA) and a garbage collector that supports both |
| 6 | A cross-platform IDE with debugging and completion |
| 7 | An operating system written in Selene |

Target set: **Linux, Windows, macOS** × **x86-64, arm64, ppc64, ppc64le, ppc32,
s390x** — including big-endian, which is why byte order is treated explicitly
everywhere rather than assumed.

## Documentation

[`docs/`](docs/README.md) — around 17,000 lines, in two parts:

- **What the system is.** The complete 40-opcode bytecode reference with the
  x86 each one emits, the object and VMT layout, the garbage collector
  algorithm, the module file format, the language reference, the standard
  library catalogue, and a tree-wide platform audit.
- **Where it is going.** Design proposals for the LLVM backend and
  multi-architecture support, the C runtime, the FFI, syntax evolution, and what
  writing an operating system in this language actually requires.

Start with [`docs/plan/15-modernization-roadmap.md`](docs/plan/15-modernization-roadmap.md)
for the synthesis: what is hard, in what order, and why.

## License

Apache License 2.0, inherited from ELENA. See [`license.txt`](license.txt).

Original work © 2005-2009 Alex Rakov.
