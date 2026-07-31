# ELENA Language 1.5.0.0 — Project Overview

> **Status of this document:** written from the 2009 source tree as found, before any
> modernization work. It describes the system *as it is*, not as it will become.
> Modernization proposals live in [`plan/`](plan/).

---

## 1. What ELENA is

ELENA is a **pure polymorphic, message-passing object-oriented language**, created by
Alex Rakov starting in 2005. Version 1.5.0 is dated December 2009 and is the state
captured in this repository.

Its defining ideas:

| Concept | Meaning |
|---|---|
| **Everything is an object, everything is a message** | Smalltalk-style uniformity: control flow, arithmetic and comparison are all message sends. |
| **Messages can fail** | A message send that finds no matching method *fails* rather than raising an error. Failure is a first-class control-flow signal, used to implement `#if`, `#loop` and alternative branches (`\|`). |
| **"Shift" technology** | An object can change its own behaviour at run time by shifting into a **role** — a secondary VMT that intercepts messages before the base class sees them. |
| **Class mutation (`annex` / `cast`)** | An object can be dynamically wrapped or re-typed, delegating unmatched messages to an annexed inner object. |
| **Open architecture** | The stated ultimate goal (`doc/roadmap.txt`) is "creating systems capable to be modified during run-time". |

The language is *not* statically typed and has no exception mechanism in 1.5 — the
failure protocol is the only error channel.

## 2. Execution model in 1.5.0

This is the point most often misunderstood, so it is stated plainly:

**There is no virtual machine in 1.5.0.** The `readme.txt` line "ELENA Virtual machine
(in developing)" refers to a *future* goal. What actually happens is:

```
.l source ──elc──▶ .nl bytecode module ──JIT at link time──▶ native x86 ──▶ PE .exe
```

1. `elc` parses ELENA source and emits **ELENA bytecode** into `.nl` module files
   (new in 1.5.0 — see `whatsnew.txt:20`, *"ELC generates byte code modules instead of
   native ones"*). Prior versions emitted native code directly.
2. At link time, the **JIT compiler** (`elenasrc/engine/win32/x86jitcompiler.cpp`)
   translates that bytecode into x86 machine code in memory.
3. The **JIT linker** (`elenasrc/engine/jitlinker.cpp`) resolves references, lays out
   VMTs and data sections, and pulls in the precompiled assembly runtime.
4. The **PE linker** (`elenasrc/elc/win32/linker.cpp`) writes the result out as a
   Win32 PE executable.

So the "JIT" is really an **ahead-of-time code generator that happens to work in
memory**. There is no run-time compilation and no interpreter. The bytecode is an
intermediate representation, not something ever executed as bytecode.

The **runtime** — garbage collector, dynamic dispatch, arithmetic primitives, and the
whole Win32 syscall layer — is **hand-written x86 assembly** in `src/asm/`, assembled
by a **custom assembler** (`asm2bin`) that is part of this repository.

## 3. Repository layout

```
ELENA-1.5.0.0/
├── elenasrc/           C++ source of the toolchain (~33,500 LOC)
│   ├── common/         Shared C++ infrastructure: containers, strings, streams, files
│   ├── engine/         Bytecode, module format, JIT compiler, JIT linker
│   │   └── win32/      x86 JIT backend + PE image sections
│   ├── elc/            The compiler: lexer, parser, semantic analysis, codegen
│   │   └── win32/      Entry point + PE linker
│   ├── asm2bin/        Custom x86 assembler for the runtime .asm files
│   ├── ide/            "elide" — Win32 GUI IDE, editor and debugger
│   │   ├── win32/      Raw Win32 UI layer
│   │   └── gtk/        Abandoned GTK port stub
│   ├── sg/             Syntax generator: grammar → parser table
│   ├── api2html/       API documentation generator
│   └── plugins/        IDE plugin (autoform)
├── src/                ELENA standard library (~13,500 LOC of .l)
│   ├── asm/            Hand-written runtime in x86 assembly (~6,150 LOC)
│   ├── elena.l         Root module
│   ├── std/  sys/  ext/    Portable-ish library packages
│   ├── gui/            GUI abstraction
│   └── win32/          Win32 platform bindings
├── examples/           14 sample programs, incl. the large "upndown" card game
├── bin/                elc.cfg + project templates (no prebuilt binaries present)
├── dat/                sg grammar input + api2html input
├── doc/                Original 2009 documentation (sparse)
└── docs/               ← THIS documentation set (new)
```

### Size breakdown

| Area | Language | LOC |
|---|---|---|
| Toolchain (`elenasrc/`) | C++ | ~33,500 |
| Standard library + examples (`src/`, `examples/`) | ELENA (`.l`) | ~13,500 |
| Runtime (`src/asm/`) | x86 assembly | ~6,150 |
| **Total hand-written source** | | **~53,000** |

The largest single files are `elenasrc/common/lists.h` (2,763), `src/asm/standard.asm`
(2,804), `elenasrc/asm2bin/x86assembler.cpp` (2,772), `elenasrc/ide/win32/appwindow.cpp`
(2,167) and `elenasrc/elc/compiler.cpp` (1,756).

## 4. The five programs

| Binary | Source | LOC | Purpose |
|---|---|---|---|
| `elc.exe` | `elenasrc/elc/` + `engine/` + `common/` | ~7,900 | Command-line compiler and linker |
| `elide.exe` | `elenasrc/ide/` + `common/` | ~12,900 | GUI IDE, editor, debugger |
| `asm2binx.exe` | `elenasrc/asm2bin/` | ~3,300 | Custom x86 assembler for the runtime |
| `sg.exe` | `elenasrc/sg/` | ~150 | Grammar → LL parser table generator |
| `api2html.exe` | `elenasrc/api2html/` | ~470 | API doc generator |

`sg` and `asm2bin` are **bootstrap tools**: without them the compiler's parser table
and the runtime cannot be rebuilt.

## 5. Platform reality

`readme.txt:7` states it plainly: *"Currently only Win32-i386 (2000/XP/Vista/7)
platform is supported."*

The coupling is total and runs through every layer:

- **Executable format** — the linker emits PE/COFF only.
- **Code generation** — x86-32 machine code is emitted byte by byte, by hand.
- **Runtime** — x86 assembly calling Win32 APIs directly.
- **Standard library** — the `win32` package is a direct binding layer; `gui` sits on it.
- **IDE** — raw Win32 API, plus the Windows debug API (`WaitForDebugEvent`) for debugging.
- **Word size** — 32-bit pointers assumed throughout the object model, bytecode and
  module format.

A detailed, quantified inventory is in
[`porting/14-platform-dependency-audit.md`](porting/14-platform-dependency-audit.md).

## 6. Version history

23 releases are recorded in `whatsnew.txt`, from `0.9.2` (late alpha) to `1.5.0`:

| Milestone | Version | Change |
|---|---|---|
| Packages introduced | 0.9.2 | First recorded release |
| GUI IDE `elide` replaces console IDE | 0.9.9 | |
| First release candidate | 1.0.0 | |
| Square brackets for hints | 1.4.4 | Syntax break |
| `$super` removed; operator precedence levels | 1.4.6 | Syntax break |
| **Bytecode modules replace native output** | **1.5.0** | Architectural change |

Version 1.5.0 also credits a new `Agenda` sample to Alexandre Bencz (`whatsnew.txt:28`).

## 7. What the original author planned next

From `doc/roadmap.txt` — the 1.5.x wish list, none of which shipped:

- Linux support (elc + elide)
- Just-in-time compiler *(proper, run-time)*
- DLL support; every package compiled as a DLL
- **x64 platform support**
- Optimized output code and optimized GC
- **Multi-thread support**
- External linking of C++ `.obj` files
- Reflection / "active code" concept

`doc/todo.txt` (802 lines) is a remarkably candid backlog. Items that directly
prefigure the current modernization effort:

| todo.txt | Item |
|---|---|
| `:29` | *"what about parallel programming. Problem with GC. Multithread support overview"* |
| `:348` | *"rewrite GC on C (better debugging / optimizing)"* |
| `:355` | *"start to support 64bit platform"* |
| `:334` | *"do not use processor specified offsets in byte code"* |
| `:530` | *"how could GC algorithm modified to deal with multi-thread applications (thread stacks, synchronization)"* |
| `:572` | *"review compiler code to remove all dependencies on platform, CPU"* |
| `:606` | *"refactor IDE to extract platform dependent code"* |
| `:663` | *"port elc to Linux"* |

The author knew exactly where the debt was. He simply never got to it.

`doc/knownbugs.txt` lists 12 open defects, including `#00024` — *"gc bug (maybe with
saved message) happens time to time"* — an unresolved intermittent GC failure.

## 8. What this checkout does *not* contain

- **No compiled binaries** — no `elc.exe`, `elide.exe`, `asm2binx.exe`, `sg.exe`.
- **No compiled library modules** — no `.nl` files for the standard library.
- **No prebuilt parser table** — must be regenerated by `sg` from `dat/sg/syntax.txt`.
- **No assembled runtime** — `src/asm/*.asm` must be assembled by `asm2bin`.
- **Not a git repository** — there is no version history to consult.

Consequence: the tree cannot currently produce a working compiler on any platform
without first building the C++ toolchain from source. See
[`build/13-build-system-and-tools.md`](build/13-build-system-and-tools.md) for the
bootstrap chain and its chicken-and-egg problems.

## 9. Where the modernization is going

The intended direction, for context while reading the rest of these docs:

| # | Goal |
|---|---|
| 1 | Build on **Linux, Windows and macOS** with CMake and a modern C++ compiler |
| 2 | Replace the hand-written x86 code generator with an **LLVM backend** |
| 3 | **Remove the `src/asm/` runtime**, reimplementing it in portable C / LLVM IR |
| 4 | Retire `asm2bin` once nothing depends on it |
| 5 | Target **x86-64 and ARM64**, emitting ELF / PE / Mach-O |
| 6 | Add **multithreading** with STA and MTA models |
| 7 | Replace the GC with one that supports both threading models |
| 8 | Long term: **build an operating system in ELENA** |

Every subsystem document ends with a *Modernization notes* section feeding into that
plan.

---

*Next: [`01-toolchain-architecture.md`](01-toolchain-architecture.md) for how the
pieces fit together.*
