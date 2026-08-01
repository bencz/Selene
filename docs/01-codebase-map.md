# ELENA 1.9.23 / 2.0 — Codebase Map

*The system as it is, before modernization. Mapped 2026-08-01 from the
Dec 2015 upstream snapshot at the repo root. This document supersedes
`doc/` (singular), which is the original author's documentation and is
partly stale (e.g. `doc/tech/bytecode.txt` predates several opcodes).*

**Version identity.** `readme.txt` says 1.9.23; `README.md` says "V. 2.0
(C)2005-2015". They describe the same package: 1.9.23 is the last of the
1.9.x line that was becoming 2.0. Internal markers: engine version
`0x0009`/`0x0002` (`elenasrc2/engine/elenaconst.h:15`), module magic
`"ELENA.9.02"` (`elenaconst.h:340`), `src30/asm/core_routines.esm:1` says
"ELENA Language 2.0". Supported platforms as shipped: Win32-x86 (complete),
Linux-i386 (alpha, console only).

---

## 1. Tree layout

| Path | Contents | Lines |
|---|---|---|
| `elenasrc2/common/` | Container/string/stream/file/config infrastructure, no ELENA knowledge | 8,018 |
| `elenasrc2/engine/` | Bytecode, module format, JIT compiler+linker, library manager, runtime metadata | 9,017 |
| `elenasrc2/elc/` | The compiler: lexer, LL(1) parser, semantic compiler, bytecode writer, image builder, **hand-written PE32 and ELF32 executable writers** | 15,513 |
| `elenasrc2/elenavm/` | "VM" — actually a JIT-compiler-and-dynamic-linker DLL (no interpreter loop) | 1,879 |
| `elenasrc2/elenart/` | Runtime *metadata* DLL: backtraces, address→name, reflection lookups | 505 |
| `elenasrc2/elenasm/` | Script engine: runtime-definable CF grammar → tape → fed to elenavm | 2,593 |
| `elenasrc2/tools/` | `asm2bin` (x86 + e-code assemblers), `ecv` (module disassembler), `elt` (VM REPL), `og` (peephole-rule compiler), `sg` (parser-table generator) | 6,008 |
| `elenasrc2/ide/`, `gui/` | Win32 IDE + GTK skeleton. **Not ported** (LSP + DAP instead) | 25,221 |
| `src30/` | The standard library in ELENA (`system`, `extensions`, `forms`, `net`, `sqlite`) + `asm/` (see §7) + `cpuvm/` (a sample program, not toolchain) | ~22,000 |
| `dat/sg/syntax.txt` | The grammar (LL(1) BNF with numeric ids) → compiled to `syntax.dat` | 650 |
| `dat/og/rules.txt` | Peephole rewrite rules over bytecode → compiled to `rules.dat` | 30 |
| `bin/` | Configs (`elc.cfg`/`elc.config`), link templates (`templates/*.cfg`), script grammars (`scripts/*.es`), prebuilt `x32/*.bin` cores | — |
| `experimental_version/` | The completed ELENA 1.5 → Selene experiment (LLVM backend, C runtime). Reference material; see `docs/02-modernization-plan.md` §2 | — |

## 2. The official build pipeline (`rebuild.bat`)

```
sg  dat/sg/syntax.txt            → bin/syntax.dat      (parser table)
og  dat/og/rules.txt             → bin/rules.dat       (peephole trie)
asm2binx src30/asm/core_routines.esm lib30/system      (e-code → system'core_routines.nl)
asm2binx src30/asm/ext_routines.esm  lib30/system      (e-code → system'ext_routines.nl)
asm2binx src30/asm/x32/*.asm     → bin/x32/*.bin       (x86 asm → native core modules)
elc -csrc30/{system,extensions,net,forms,sqlite}/*.prj (library modules → lib30/**.nl)
```

Known staleness: `rebuild.bat` still references `src30/asm/x32/commands.asm`,
which no longer exists; `bin/x32/` lacks the `core_lnx.bin`/`core_rt_lnx.bin`
the Linux template requires.

## 3. Execution model

### 3.1 The e-code machine

Register-based (vs the pure stack machine of 1.5). Registers, with their
x86-32 bindings in the JIT (`engine/x86jitcompiler.cpp:880-1015`):

| Abstract | x86 | Role |
|---|---|---|
| A (acc) | `eax` | accumulator — current object |
| B (base) | `edi` | secondary object pointer |
| D (index) | `esi` | integer data register |
| E (ext) | `ecx` | message register / counter / length |
| fp | `ebp` | frame pointer (`FI` operands: `[ebp - n*4]`) |
| sp | `esp` | stack pointer (`SI` operands: `[esp + n*4]`) |

### 3.2 Bytecode encoding (`engine/bytecode.h:269-270`)

- `0x00–0x8F`: one byte, no operand.
- `0x90–0xEF`: one byte + one LE dword (`argument`).
- `0xF0–0xFF`: one byte + two LE dwords (`argument`, `additional`).

~150 assigned opcodes (mnemonic table `_fnOpcodes`, `engine/bytecode.cpp:15-64`;
enum in `bytecode.h`). Families: register moves (`acopyr`, `bcopya`, `dcopy`,
`ecopy`…), stack (`pusha`/`popa`/`aloadfi`/`asavesi`…), memory
(`aloadai`/`asavebi`/`bread`/`nwrite`…), object
(`new`/`newn`/`create`/`bsredirect`/`acallvi`/`xcallrm`/`class`/`flag`…),
control (`jump`/`if`/`else`/`ifr`/`elsen`/`next`/`hook`/`throw`/`unhook`),
frame (`open`/`close`/`quitn`/`equit`/`reserve`/`exclude`),
arithmetic int/long/real (`nadd`/`ladd`/`radd`/`rsin`…),
threading (`snop`/`trylock`/`freelock`).

Caveat: for the two-dword conditional forms (`lessn`, `ifm`, `elsem`, `ifr`,
`elser`, `ifn`, `elsen`) the serialized operand order is *reversed* relative
to the in-memory `CommandTape` (`bytecode.h:223-225`, "HOT FIX" comments in
`x86jitcompiler.cpp:808-855`).

Meta-commands ≥ `0x8000` (`blBegin`/`blEnd`/`blLabel`, `bcAllocStack`…,
`bd*` debug pseudo-ops) exist only in the in-memory tape, never in sections.

### 3.3 Message encoding (`elenaconst.h:22-26`, `elena.h:590-643`)

```
bit 31      MESSAGE_MASK  (set when materialized in an image)
bits 24-30  verb id       (127 verbs; DISPATCH=0x01, EVAL=0x05, GET=0x06 …)
bits  4-23  signature/subject reference (20 bits)
bits  0- 3  parameter count (>= 0x0C = open/variadic arg list)
```

Subject ids are module-local and re-interned globally at link time
(`jitlinker.cpp:131-149` → `_ImageLoader::_subjects` monotonic counter).

### 3.4 Object and VMT layout

Object (`jitcompiler.cpp:35-45`, `doc/tech/knowhow.txt`):
16-byte pad (`elObjectOffset = 0x10`), live header in the last 8 bytes:
`[ptr-8]` = size/length (negative ⇒ binary structure), `[ptr-4]` = VMT.
The `corex` (multithreaded) variant adds a sync field.

VMT (`jitcompiler.cpp:16-17, 177-205`):
```
[vmt-0x0C] entry count      [vmt-0x08] flags
[vmt-0x04] class-class VMT  [vmt+0x00] VMTEntry{message, address}[]
```
Entries sorted ascending by message id — `bsredirect` binary-searches.
Entry for `DISPATCH_MESSAGE_ID` doubles as the not-understood fallback.
VMT flags (`elenaconst.h:283-316`): `elStandartVMT`, `elStructureRole`,
`elSealed`, `elClosed`, `elStateless`, `elWithGenerics`, `elDebugMask`… —
several are composite bit patterns, not single bits.

### 3.5 GC (in assembly, `src30/asm/x32/core.asm` `%GC_ALLOC`)

Generational copying/compacting collector: young generation with shadow
semispace, mature generation with mark-compact, card-table write barrier
(`gc_mg_wbar`), promotion threshold, forwarding-table fixup. `corex.asm` is
the multithreaded variant: TLS per-thread state, `lock cmpxchg` spinlock
around allocation, thread table. Heap layout doc: `doc/tech/knowhow.txt` §2.

## 4. Module format (`.nl` / `.dnl` / `.bin` — same container)

`Module::save` (`engine/module.cpp:199-223`):

```
"ELENA.9.02"                      10 bytes, no NUL
module name                       NUL-terminated UTF-8
references   ReferenceMap block   [dword count]{key string, ref_t value}…
subjects     ReferenceMap block
constants    ReferenceMap block
[dword totalSectionsSize]
section records:
  [dword key = refId | mask]
  [dword bodyLength][body bytes]
  [dword relocCount]{[dword refKey][ref_t position]}…
```

- Procedure body inside a code section: `[dword byteLength][bytecode…]`.
- VMT tape (`elc/bcwriter.cpp:1281-1340`): `[dword size][dword classClassRef]
  [ClassHeader raw][VMTEntry raw]…`.
- `ClassInfo` (compiler metadata) lives in `mskMetaRDataRef` sections;
  per-module `#types` / `#extensions` meta sections carry the type system.
- **Everything is host-endian, unversioned beyond the magic, and
  `sizeof()`-dependent** (`ClassHeader` = {size_t,size_t,ref_t},
  `VMTEntry` = {size_t,int}) — the format is ABI-locked to ILP32 LE.
  Bytes 8-9 of the magic are read but never validated (`module.cpp:176-179`).

Reference handles: `ref_t = size_t` (`common/common.h:34`) holding
`(24-bit id) | (8-bit mask in bits 24-31)`. Image masks
(`elenaconst.h:130-180`): `mskCodeRef`, `mskRelCodeRef`, `mskRDataRef`,
`mskStatRef`, `mskDataRef`, `mskTLSRef`, `mskImportRef`; typed masks:
`mskSymbolRef`, `mskVMTRef 0x41…`, `mskClassRef 0x11…`, `mskMetaRDataRef`,
`mskLiteralRef`, `mskInt32Ref`, `mskMessage`, `mskVMTMethodAddress`,
`mskVMTEntryOffset`, `mskPreloaded`…

## 5. The compiler (`elenasrc2/elc`)

Pipeline (driver: `elc/linux32/elc.cpp:363` / `elc/win32/elc.cpp:463`):

1. **Lexer** — `source.cpp`: hand-coded 29×128 ASCII DFA (**not** generated
   by sg; UTF-8 only inside quoted literals).
2. **Parser** — `parser.cpp`: LL(1) stack machine driven by `syntax.dat`
   (`ParserTable::load`). Symbol ids are duplicated by hand between
   `dat/sg/syntax.txt` (`__define`) and `elc/syntax.h` — a hard coupling.
3. **Derivation stream** — `derivation.cpp`: flat dword stream, re-scanned
   (no random access); `DNode` navigation.
4. **Semantic compiler** — `compiler.cpp` (5,668 lines): scope chain
   (`ModuleScope`→`ClassScope`→`MethodScope`→`CodeScope`), `ObjectInfo`
   kinds, hint system (`sealed`, `struct`, `embeddable`, `stacksafe`,
   `generic`, `suppress`…), message mapping, extension resolution, boxing,
   two passes (declarations, then implementations). Produces a typed
   **SyntaxTree** IR (`syntaxtree.cpp`, `lx*` node kinds) per method/symbol.
5. **Bytecode writer** — `bcwriter.cpp`: SyntaxTree → `CommandTape`
   (`generateTree`, `bcwriter.cpp:3405`) plus imperative emitters for
   prologues/dispatchers; tape optimization = jump cleanup + the `rules.dat`
   peephole trie (`Compiler::optimizeTape`, `compiler.cpp:1540`); then
   serialization (`writeProcedure`/`writeVMT`) into module sections and
   `.dnl` debug info.
6. **Image builder** — `image.cpp`: drives `JITLinker` in *virtual mode* to
   materialize `.text/.rdata/.bss/.stat/.import/.tls/.debug` from the module
   closure, pulling `bin/x32/*.bin` native cores for the runtime.
7. **Executable writer** — `elc/win32/linker.cpp` writes **PE32 by hand**;
   `elc/linux32/linker.cpp` writes **ELF32 by hand** (synthesized PLT/GOT/
   dynamic segment, hardcoded `/lib/ld-linux.so.2`, base `0x08048000`,
   `EM_386`, no section headers). There is no external assembler or linker.

Project files (`.prj` Windows / `.project` Linux) are INI: `[project]`
(namespace, template, output, entry), `[files]` (ordered source list),
`[forwards]`, `[linker]`, `[system] platform=<bitfield>`. Module name =
namespace + source sub-path with `'` separators; module `a'b'c` ↔ file
`<output>/a/b/c.nl` (`Path::nameToPath`, `common/files.h:164-187`).
Config layering: `elc.cfg` → template chain (`lib`/`console`/`core`…) →
project → command line. The `[forwards]` table (`'$super=system'Object` …)
is how the compiler stays library-agnostic.

## 6. The JIT and the "VM" (all of it dies)

- `engine/x86jitcompiler.cpp` + `x86helper.cpp` (~2,600 lines): template
  JIT. Each opcode's x86 body lives in `core.asm`/`corex.asm` as
  `inline % <opcode>` sections, compiled by asm2binx into `.bin` modules;
  the JIT memcpy's the template and patches operands via a relocation
  table appended to each body. No instruction selection.
- `engine/jitlinker.cpp` (904 lines): **the semantic core** — resolves
  `name+mask` → emitted bytes: demand-driven, memoized; VMT construction
  (parent copy + sorted insert + override), constant materialization
  (payload + VMT pointer at slot −4), message interning, deferred fixups
  (`mskVMTMethodAddress`/`mskVMTEntryOffset`), debug records. An LLVM
  backend must reproduce these *semantics* with name-based linking.
- `elenavm/`: not an interpreter — a JIT-in-a-DLL (Win32-only) with three
  exports (`Interpret`/`Evaluate`/`GetLVMStatus`, `engine/elenavm.h`).
  Shares `JITLinker`+`x86JITCompiler` with elc (real-address mode vs elc's
  virtual mode). Its `lnVMAPI_*` callback table (`elenaconst.h:324-331`) is
  the runtime reflection surface — in the new world these become ordinary
  C runtime functions.
- `elenart/`: 505 lines; **not** the GC, **not** the entry point — a debug
  metadata reader (backtraces, address→name, `GetSymbolRef`) bound at
  program startup by `core_rt.asm` via LoadLibrary/dlopen + GetProcAddress.
  The *contract* survives (reflection needs); the code does not.
- `elenasm/` + `tools/elt`: script engine (runtime-definable CF grammar +
  DSA actions → inline-script tape) hard-linked to elenavm, and its REPL.
  Both die with the VM; `cfparser.cpp` is the only salvageable piece.

## 7. `src30/asm` — the two assembly layers (both die)

- **`core_routines.esm` (5,279 lines, 252 procedures) + `ext_routines.esm`**:
  ELENA *bytecode* assembly, compiled by asm2binx's `ECodesAssembler` into
  ordinary `.nl` modules. This is the primitive layer of the stdlib:
  numeric/string/array primitives, dispatch helpers (`dispatch`,
  `bsredirect` wrapper, `handle_*`), reflection (`symbol_new`,
  `callstack_load`), platform glue. Reached from ELENA source via
  `=> system'internal'<name>` — the compiler strips the prefix, resolves the
  `'$import` forward (`system'core_routines`) and **splices the bytecode
  inline** into the caller (`Compiler::importCode`, `compiler.cpp:1665`).
  421 use sites in 24 library files.
- **`x32/*.asm` (11,695 lines)**: the native runtime in x86-32 —
  `core.asm` (ST core: opcode templates + GC), `corex.asm` (MT variant),
  `coreapi.asm` (73 helper procedures: entry, frames, conversions, math),
  `core_win.asm`/`core_lnx.asm` (7-procedure OS layer: NEW_HEAP, EXIT,
  BREAK…), `core_vm.asm`/`core_rt*.asm` (VM/elenart bridges). Assembled by
  asm2binx's own x86 assembler (3,418 lines) into `bin/x32/*.bin`.

## 8. FFI as it exists today

Two mechanisms, both keyed on magic namespaces (`elenaconst.h:486-497`):

1. **`system'external'<ALIAS> <Func> &<subject>:arg …`** — the call site is
   the declaration; no prototypes. Alias resolution
   (`elc/project.cpp:338`): `[winapi]` config section ⇒ stdcall;
   `[externals]` ⇒ cdecl (`libc=libc.so.6`); otherwise verbatim. 182 call
   sites (KERNEL32/USER32/GDI32/WS2_32/sqlite3/libc/libdl). Typed by
   subjects (`&int:`, `&wide:`, `&dirty_ptr:`…) — a real step up from 1.5,
   but still untyped returns and no per-target marshalling.
2. **`system'internal'<routine>`** — inline e-code splice from the `.esm`
   modules (see §7).

Notable wart: Linux console output resolves libc's `stdout` by
dlopen/dlsym at startup and writes byte-at-a-time via `putchar`
(`src30/system/io/lnx32_console.l`).

## 9. Tools inventory and verdicts

| Tool | What it does | Verdict for Selene v2 |
|---|---|---|
| `sg` | `syntax.txt` → LL(1) `syntax.dat`. Required by elc at startup | **Keep** (orthogonal to codegen) |
| `og` | `rules.txt` → peephole trie `rules.dat` used by elc `-l0` | Delete once LLVM owns optimization (verify the 2 frame-collapsing rules are subsumed) |
| `ecv` | Interactive `.nl` disassembler; complete e-code + format oracle | **Keep** (differential oracle for format v2) |
| `asm2bin` / e-code half | `.esm` → `.nl` primitive modules | Keep short-term (only writer of primitive bodies), delete after runtime-in-C |
| `asm2bin` / x86 half | `.asm` → `.bin` | Delete |
| `elt` | REPL over elenasm→elenavm | Delete |

## 10. 64-bit / endianness hazard inventory (condensed)

The full details live in the exploration transcripts; the classes of defect:

1. **`ref_t = size_t`** with the mask in bits 24-31 of an assumed-32-bit
   word; `~mskAnyRef` promotes wrongly on LP64 (`module.cpp:75`).
2. **Serialization truncation**: `readDWord(size_t&)` reads 4 bytes into 8
   (`streams.h:88-91`); `ROSection::Length()` reads 8 from a 4-byte field
   (`module.h:88`); `writeDWord` of `size_t` fields everywhere.
3. **Raw struct I/O**: `ClassHeader`, `VMTEntry`, `DebugLineInfo`,
   `ClassInfo::save` — `sizeof`-dependent and endian-dependent.
4. **4-byte slot arithmetic**: `_Memory::operator[]` returns `int&`
   (`streams.h:23`); `<<2`/`>>2` everywhere (stack indexes, static count,
   VMT stride `<<3`, `IntFixedMap` iterator with hardcoded 8-byte stride —
   `lists.h:2831`).
5. **Pointer↔int round-trips**: `*(int*)((int)refVMT - 0x0C)`
   (`jitcompiler.cpp:124`), vaddresses stored in `ref_t` maps, and in the
   front end `ident_t` strings smuggled through 32-bit tree arguments
   (`bcwriter.cpp:3333+`) — the single worst 64-bit blocker.
6. **Hardcoded sizes in the front end**: `info.size = 4` for
   `message`/`symbol`/`signature`/`pointer` hints (`compiler.cpp:942-1043`),
   boxed-constant VMT slot at −4, `assignLong` split into two dwords.
7. **No byte-swapping anywhere**; unaligned direct loads
   (`*(int*)(_buffer+pos)`, `section.cpp:70`) — UB on strict-alignment ISAs.

Latent bugs found while mapping (worth fixing or deleting with their hosts):
`section.cpp:87` uses `&&` for `&`; `bytecode.h:9` include guard typo
(`bytecbcpopodeH`); `bdLocalInfo`/`bdStruct` id collision (`bytecode.h:258`);
`optimizeIdleBreakpoints` never reports `modified`; dead 0-byte stdlib files
(`system/app.l`, `net/win32_server.l`, `forms/win32_types.l`,
`extensions/dynamic/scripttools.l`).
