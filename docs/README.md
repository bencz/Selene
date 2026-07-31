# ELENA 1.5.0.0 — Documentation

Documentation set for the ELENA Language 1.5.0.0 source tree (2009), written as the basis
for its modernization: cross-platform builds, an LLVM backend, removal of the hand-written
assembly runtime, multithreading, a new GC, and eventually an operating system written in
ELENA.

**~16,500 lines across 15 documents.** Everything describes the code *as it is* unless the
document is under `plan/`, which is explicitly forward-looking.

---

## Start here

| Document | What it covers |
|---|---|
| [`00-project-overview.md`](00-project-overview.md) | What ELENA is, the execution model, repo layout, version history, what is missing from this checkout |
| [`plan/15-modernization-roadmap.md`](plan/15-modernization-roadmap.md) | **Synthesis of all audits** — the five decisions to make first, what is actually hard, phased plan, live defects |

## Architecture — the toolchain as it exists

| Document | Lines | Covers |
|---|---:|---|
| [`architecture/02-compiler-frontend-elc.md`](architecture/02-compiler-frontend-elc.md) | 1,491 | Lexer, LL(1) parser, derivation stream, semantic analysis, lowering to bytecode |
| [`architecture/03-engine-bytecode-jit.md`](architecture/03-engine-bytecode-jit.md) | 2,202 | **Complete 40-opcode reference** with emitted x86 bytes, object/VMT layout, `.nl` format, JIT linker |
| [`architecture/04-pe-linker.md`](architecture/04-pe-linker.md) | 293 | PE/COFF writer, import tables, relocation, the `_LoaderHelper` platform seam |
| [`architecture/05-common-infrastructure.md`](architecture/05-common-infrastructure.md) | 1,061 | Container library, string model, streams, files, config; portability audit |
| [`architecture/06-assembler-asm2bin.md`](architecture/06-assembler-asm2bin.md) | 621 | The custom x86 assembler; deletion analysis |
| [`architecture/07-runtime-core-asm.md`](architecture/07-runtime-core-asm.md) | 1,202 | **Behavioural spec of the runtime**: GC algorithm, dispatch, primitives, Win32 layer |
| [`architecture/08-ide-debugger.md`](architecture/08-ide-debugger.md) | 1,247 | Text engine, UI layer, and the debugger architecture in depth |

## Language

| Document | Lines | Covers |
|---|---:|---|
| [`language/10-elena-language-reference.md`](language/10-elena-language-reference.md) | 2,086 | **Complete reference for the 1.5 dialect** — grammar, all 17 `#` directives, the failure model, roles/shift, annex/cast, idiom cookbook |
| [`language/11-standard-library.md`](language/11-standard-library.md) | 928 | Package map, per-module class catalogue, dependency graph, platform coupling |

## Build & porting

| Document | Lines | Covers |
|---|---:|---|
| [`build/13-build-system-and-tools.md`](build/13-build-system-and-tools.md) | 1,512 | All build targets, the bootstrap chain, `sg` and `api2html`, config reference, CMake migration plan |
| [`porting/14-platform-dependency-audit.md`](porting/14-platform-dependency-audit.md) | 1,157 | Tree-wide platform inventory, heat map, the 64-bit problem, **measured** modern-compiler errors |

## Plan — forward-looking proposals

| Document | Covers |
|---|---|
| [`plan/15-modernization-roadmap.md`](plan/15-modernization-roadmap.md) | The synthesis: decisions, hard problems, phases, defects, traps |
| [`plan/16-syntax-evolution.md`](plan/16-syntax-evolution.md) | Language changes: failures that carry information, named intrinsics replacing `#inline pkg'N`, typed structs, gated pointers, UTF-8 |
| [`plan/17-llvm-backend-and-targets.md`](plan/17-llvm-backend-and-targets.md) | LLVM layer, cross-compilation, **big-endian and multi-arch** (ppc64/ppc32/s390x), module format v2, dispatch, GC |
| [`plan/18-ffi-design.md`](plan/18-ffi-design.md) | Typed FFI, platform selection via forwards, GC-safe transitions, callback trampolines |
| [`plan/19-runtime-in-c.md`](plan/19-runtime-in-c.md) | **Design revision** — `src/asm/` becomes a C11 runtime, not compiler-emitted IR; LTO gives inlining for free |
| [`plan/20-os-development.md`](plan/20-os-development.md) | What writing a kernel in ELENA requires, and what it changes about decisions being made **now** |

---

## Key facts established by the audits

- **There is no VM.** Bytecode is translated to native x86 at *link* time, ahead of
  execution. The `.exe` contains only machine code.
- **Bootstrap is not circular.** ELENA 1.5 is not self-hosting; no prebuilt binary is
  needed. The tree is self-sufficient.
- **Modern C++ is not a blocker.** Measured with GCC 16.1.1 and Clang 22.1.8: 25 errors at
  `-m64`, 3 at `-m32`, and `-std=c++17` adds *zero* new rejections. The blockers are
  platform and word size, not language version.
- **64% of the standard library is already portable**, because the *forwards* mechanism —
  link-time dependency injection — is a genuine platform seam.
- **Multithreading requires the rewrite.** `asm2bin` cannot encode `lock`, `cmpxchg` or
  `xadd`; there is one safepoint in the entire system; the GC has a single global root
  chain.
- **`doc/tech/bytecode.txt` is wrong** — a pre-1.5 sketch omitting 12 of 40 opcodes and
  listing two that do not exist. Use `architecture/03` instead.

## A note on file extensions

Selene renamed the file extensions. `.l` had to go: it is the canonical
extension for **Lex/Flex**, so editors, GitHub Linguist and build tools all
classified the sources as lexer grammars.

| Role | ELENA 1.5 | Selene |
|---|---|---|
| Source | `.l` | `.sel` |
| Compiled module | `.nl` | `.sem` |
| Per-module debug info | `.dnl` | `.sdm` |
| Linked debug info | `.dn` | `.sdi` |
| Project | `.prj` | `.prj` (unchanged) |
| Module magic | `EN!10` | `SELENE20` |

The **descriptive** documents below — everything under `architecture/`,
`language/` and `porting/` — deliberately keep the original extensions, because
they describe the 2009 system as it was and rewriting them would make the
description inaccurate. Only `build/` and `plan/` use the new names, since those
contain commands you actually run.

## Corrections to the original documentation

| Original claim | Reality |
|---|---|
| `readme.txt:18` "ELENA Virtual machine (in developing)" | No VM exists or is being built; the design is AOT-to-native |
| `doc/tech/bytecode.txt` opcode list | Incomplete and partly fictional; see `architecture/03` §10 |
| `readme.txt:95` "compiled with CodeBlocks and Mingw32" | Also VS 7/8/9/10; no `.sln`, no makefile, no build script exists |
