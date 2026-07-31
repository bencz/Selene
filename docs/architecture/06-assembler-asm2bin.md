# The Custom Assembler (`elenasrc/asm2bin`)

> 3 335 lines across 6 files. A hand-written, single-pass, ELENA-specific x86 assembler
> whose only job is to turn the five `src/asm/*.asm` files into ELENA *module* containers
> (`*.bin`) that the JIT linker can splice into a generated executable.
>
> **It is not a general-purpose assembler.** It cannot produce an object file, it has no
> sections, no symbols in the ELF/COFF sense, and no expression evaluator. It is a
> transliterator from a bespoke dialect into ELENA relocation records.

Related: [`07-runtime-core-asm.md`](07-runtime-core-asm.md) (what the assembled code
actually does), [`04-pe-linker.md`](04-pe-linker.md) (what consumes the result).

---

## 1. Purpose and position in the build

```
src/asm/elena.asm ─┐
src/asm/standard.asm ─┤
src/asm/win32.asm ─┼──> asm2binx.exe ──> lib/*.bin   (ELENA module container)
src/asm/winsock.asm ─┤                        │
src/asm/extended.asm ─┘                       │
                                              ▼
                            elc.cfg  [primitives]  elena=elena.bin ...
                                              │
                              Project::resolvePrimitive()  project.cpp:141
                                              │
                    ┌─────────────────────────┴───────────────────────┐
                    ▼                                                 ▼
      x86JITCompiler ctor (x86jitcompiler.cpp:466)       ReferenceLoader::loadNativeSection
      loads 22 sections as *inline templates*            (jitlinker.cpp:179) copies whole
      pasted into generated code                         sections into .text / .data
```

`asm2bin` is the **only** producer of the runtime. There is no C runtime, no `crt0`, no
libc: everything the generated executable does before reaching user code — heap creation,
allocation, garbage collection, message dispatch, integer/float arithmetic, Win32 calls —
lives in those five `.asm` files and reaches the executable through this tool.

### 1.1 Invocation

`asm2binx.cpp:17-75`:

```
asm2binx <file.asm> [<output path>]
```

| Behaviour | Source |
|---|---|
| One argument → output written beside the input | `asm2binx.cpp:39`, `:47` |
| Two arguments → output path + input basename | `asm2binx.cpp:31-38`, `:41-46` |
| Extension always forced to `.bin` | `asm2binx.cpp:50` |
| Input read with encoding autodetection (Unicode build) | `asm2binx.cpp:53` |
| Errors reported as `(row): message`, never a non-zero exit code | `asm2binx.cpp:64-73` |

**The tool always returns 0**, even on a fatal assembly error (`asm2binx.cpp:74`). Any build
script driving it must check for the output file, not the exit status.

### 1.2 Source files

| File | Lines | Role |
|---|---|---|
| `asm2binx.cpp` | 75 | `main()` — argument handling, file I/O, error printing |
| `assembler.h` | 41 | `AssemblerException`, abstract `Assembler` base |
| `x86assembler.h` | 286 | `x86Assembler` class — one `compileXXX` method per mnemonic |
| `x86assembler.cpp` | 2 772 | the entire implementation (parser + encoder) |
| `x86jumphelper.h/.cpp` | 52 + 109 | name→id mapping in front of the shared `x86LabelHelper` |

It reuses three components from the compiler proper:

| Reused component | Location | Used for |
|---|---|---|
| `SourceReader` + DFA table | `elenasrc/elc/source.cpp:18-100` | tokenisation (the *same* lexer as the ELENA language) |
| `x86Helper` / `x86LabelHelper` | `elenasrc/engine/win32/x86helper.h/.cpp` | ModRM/immediate encoding, label backpatching |
| `Module` / `Section` / `SectionWriter` | `elenasrc/engine/module.cpp`, `section.cpp` | output container and relocation map |

> **Portability note.** `x86assembler.h:15` contains `#include "win32\x86helper.h"` — a
> backslash path. The project as committed does not build on a case-sensitive, forward-slash
> filesystem without patching this line.

---

## 2. Lexical layer

There is no dedicated lexer. `x86Assembler::compile()` (`x86assembler.cpp:2733`) constructs
`SourceReader reader(4, source)` — tab width 4 — and pulls tokens through
`TokenInfo::read()` (`x86assembler.h:58-63`) with **`lowerCase = false`**. Consequently the
dialect is **case-sensitive**: every mnemonic, register and directive must be lowercase,
because every comparison is `compstr(value, _T("mov"))` and friends.

Token buffer is fixed at 50 characters (`x86assembler.h:34`, `:60`); an identifier longer
than 49 chars is silently truncated.

Token classes actually consumed (constants in `elenasrc/elc/source.h:18-39`):

| Class | DFA state | Example in `src/asm` | Meaning to the assembler |
|---|---|---|---|
| `dfaIdentifier` | `f` | `eax`, `mov`, `labStart`, `hHandle` | mnemonic / register / label / parameter |
| `dfaInteger` | `p` | `16`, `400` | decimal immediate (`x86assembler.h:46`) |
| `dfaHexInteger` | `t` | `0FFFFFFFFh`, `0Dh` | hex immediate; trailing `h` stripped (`x86assembler.h:50-54`) |
| `dfaFullIdentifier` | `j` | `'gc_yg_heap`, `'nil`, `'dlls'kernel32` | apostrophe-prefixed ELENA reference |
| `dfaKeyword` | `e` | `#win32'api'$onpaint` | `#`-prefixed *message* name |
| `dfaQuote` | `r` | `"$package'elena'2"` | quoted ELENA reference |
| `dfaOperator` / `dfaBracket` | `h` / `g` | `[ ] ( ) , + - : .` | punctuation |
| `dfaEOF` | `.` | — | end of input |

**Comments** are handled entirely by the DFA: `//` to end of line (`dfaLineComment`) and
`/* … */` (`dfaComment`). Both are skip states and never reach the assembler.

**Negative literals** are a special case: `readOffset` (`x86assembler.cpp:119`) checks
`token.value[0]=='-'` on an integer token, because the DFA emits `-5` as a single
`dfaInteger`/`dfaHexInteger` token in some positions and as `dfaMinus` + integer in others.

---

## 3. Grammar as implemented

Written as EBNF. Every production below corresponds to real code; anything not listed is a
syntax error (`"Invalid statement (%d)"`, `x86assembler.cpp:2766`).

```ebnf
unit          = { top_level } EOF ;                              (* x86assembler.cpp:2740-2768 *)

top_level     = define_decl | procedure | inline_proc | structure ;

define_decl   = "define" identifier integer ;                    (* :2742-2750 *)

procedure     = "procedure" name [ param_list ] { statement } "end" ;   (* :2751-2755 -> :2701 *)
inline_proc   = "inline"    name [ param_list ] { statement } "end" ;   (* :2756-2760 -> :2701 *)
structure     = "structure" name { data_item } "end" ;                  (* :2761-2765 -> :2046 *)

name          = identifier ;    (* mapped to "$package'" + name, see :2705, :2050 *)

param_list    = "(" identifier { "," identifier } ")" ;          (* :78-95 *)

statement     = instruction | label_decl ;
label_decl    = identifier ":" ;                                 (* :1549-1559 *)

data_item     = "dd" operand ;                                   (* :2064-2071 *)

instruction   = mnemonic [ operand { "," operand } ] ;
mnemonic      = ? see instruction table, section 5 ? ;

operand       = mem_operand
              | sized_ptr
              | seg_operand
              | plain_operand ;                                  (* :450-485 *)

mem_operand   = "[" addr_expr "]" ;                              (* -> otM32 prefix *)
sized_ptr     = ("dword" | "word" | "byte" | "qword" | "tbyte") "ptr" ("[" addr_expr "]" | plain_operand) ;
                                                                  (* :464-472, :417-448 *)
seg_operand   = "fs" ":" operand ;                               (* :473-481, only FS supported *)

addr_expr     = base { ("+" | "-") term } ;                      (* :106-185, max 2 displacement terms *)
base          = register | "(" addr_expr ")" | ref_atom | integer ;
term          = register | integer | ref_atom ;                  (* register term -> SIB byte, :156, :170 *)

plain_operand = register | integer | ref_atom | parameter | user_constant ;

register      = "eax"|"ecx"|"edx"|"ebx"|"esp"|"ebp"|"esi"|"edi"  (* :189-212 *)
              | "ax"|"cx"|"dx"|"bx"                              (* :234-245 *)
              | "al"|"cl"|"dl"|"bl"|"ah"|"dh"|"bh" ;             (* :213-233; note: NO "ch" *)

st_register   = "st" "(" integer ")" ;                           (* :97-104 *)
```

### 3.1 `procedure` vs `inline` — the only difference is alignment

`compileProcedure` is shared (`x86assembler.cpp:2701`); the `aligned` flag differs:

| Directive | `aligned` | Effect |
|---|---|---|
| `procedure` | `true` | section padded to a 4-byte boundary with `0x90` (`:2729-2730`) |
| `inline` | `false` | no padding |

**Neither emits a prologue, epilogue or `ret`.** Whether a section behaves as a callable
subroutine or as an inline template is decided entirely by the *consumer*:

* `#external name'N(...)` in ELENA source → `bcRCallExt` → wrapped by `elena'16` (callext) → the
  section is `call`ed, so it must end with `ret`.
* `#inline name'N(...)` in ELENA source → `bcRCallEmb` → `embed()`
  (`x86jitcompiler.cpp:209-218`) → the section bytes are **pasted into the caller**, so it must
  *not* contain `ret`.

Several sections declared `procedure` are used inline and correctly lack `ret`
(`standard'18` at `standard.asm:302`, `standard'27` at `standard.asm:619`); the extra NOP
padding is simply embedded. Conversely `standard'42` (`standard.asm:1532`) is `procedure`,
is used `#external`, and does end with `ret`. **The directive carries no semantics — do not
rely on it when porting.**

### 3.2 Parameters

A parameter list is legal on `procedure` *and* `inline` and creates named aliases for
`[ebp + disp32]` slots (`x86assembler.cpp:390-393`):

```
offset = (parameterCount - parameterIndex) * 4
```

So with `procedure win32'2 (handle, s)`: `handle` → `[ebp+8]`, `s` → `[ebp+4]`. That is,
**the first declared parameter is the deepest on the stack** — arguments are pushed in
declaration order by the caller. The `ebp` frame is established by `elena'16`
(`elena.asm:707-721`), which does `lea ebp, [esp+8]` after saving `edi`, the previous GC
frame pointer and `ebp`.

Parameter names take priority over user constants but are checked *after* all built-in
special references (`x86assembler.cpp:249-399`) — a parameter called `nil` or `system`
would be shadowed.

### 3.3 `define`

`define NAME value` inserts into the `constants` map (`x86assembler.cpp:2742-2749`).
Duplicates raise `"Constant already exists"`. Constants are **file-scoped and never reset**
between top-level items but the map is per-`x86Assembler` instance, so each invocation of
the tool starts fresh. Values are plain integers only — no expressions.

Examples from the corpus: `elena.asm:3-7`, `winsock.asm:3-7`.

### 3.4 `structure`

`compileStructure` (`x86assembler.cpp:2046-2077`) accepts only `dd <operand>` items and
ignores every other token (`:2075` silently `token.read()`s past it). The section is created
with mask `mskNativeDataRef`. The single instance in the corpus is `elena'38`
(`elena.asm:1453-1481`), the Win32 `WM_*` → ELENA message-id lookup table.

---

## 4. The reference vocabulary (the ELENA-specific part)

This is what makes the dialect non-portable to NASM. `defineOperand`
(`x86assembler.cpp:249-401`) recognises a fixed set of magic names and turns each into an
**ELENA relocation record** — a `(reference | mask, offset-in-section)` pair stored in the
section's `RelocationMap` by `SectionWriter::writeRef`.

### 4.1 Pre-seeded numeric constants — `loadDefaultConstants()` (`x86assembler.cpp:65-76`)

Resolved at assembly time into plain immediates; no relocation.

| Token | Value | Source of value |
|---|---|---|
| `'el_emptyobject` | `0x08` | `elEmptyObject`, `elenaconst.h:253` — object header size |
| `'gc_empty_object_aligned` | `0x17` (= 8 + 16 − 1) | `elEmptyObject + gcPageSize - 1` |
| `'gc_heap_minimal` | `0x100` (= 16 × 16) | `gcPageSize * 0x10` |
| `'gc_page_mask` | `0xFFFFFFF0` | `~(gcPageSize-1)` |
| `'gc_page_log` | `4` | `logb(16)`, via `logth()` at `:58-63` |
| `'gc_collected` | `0x40000000` | `gcCollected`, `elenaconst.h:277` — GC mark bit |
| `'gc_collectedInv` | `0xBFFFFFFF` | `~gcCollected` |
| `'gc_binary` | `0x80000000` | `gcBinary`, `elenaconst.h:278` — "no pointers inside" flag |

### 4.2 GC table fields — fixed offsets into the `$elena'@gctable` data block

All emit `mskNativeDataRef` (`0x28000000`) relocations against `GC_TABLE`
(`x86assembler.cpp:306-365`). The block is 0x30 bytes, allocated by
`ReferenceLoader::preloadCoreCode` (`jitlinker.cpp:510`).

| Token | Offset | Meaning |
|---|---|---|
| `'gs_current_frame` | `0x00` | head of the GC stack-frame chain |
| `'gc_heap_start` | `0x04` | base of the object heap |
| `'gc_yg_heap` | `0x08` | young-generation allocation pointer (bump pointer) |
| `'gc_mg_heap` | `0x0C` | mid-generation top |
| `'gc_og_heap` | `0x10` | old-generation top |
| `'gc_static_size` | `0x14` | number of static roots; patched by the PE linker at `linker.cpp:391` |
| `'gc_heap_end` | `0x18` | end of heap |
| `'gc_mgptr2` | `0x1C` | base of the mid→young remembered set |
| `'gc_mgptr2_end` | `0x20` | current top of that set |
| `'gc_ogptr2` | `0x24` | base of the old→young remembered set |
| `'gc_ogptr2_end` | `0x28` | current top of that set |
| `'gc_flag` | `0x2C` | 0 = young, 1 = mid, 2 = full collection next |

### 4.3 Other magic references

| Token | Reference produced | Mask | `x86assembler.cpp` |
|---|---|---|---|
| `'statroots` | `$elena'@gcroot` | `mskNativeStaticRef` | `:266-269` |
| `'windproc` | `$package'elena'37` | `mskNativeCodeRef` | `:270-273` |
| `'nil` | `$elena'$nil` + `elEmptyObject` | `mskConstantRef` | `:366-370` |
| `'system` | `win32'system'gui` | `mskStaticConstRef` | `:379-383` |
| `'gc_heapsize` | linker constant `lnGCSize` | `mskLinkerConstant` | `:302-305` |
| `'structure:"name"` | named data section | `mskNativeDataRef` | `:371-378` |
| `"class'name"` (quoted) | class VMT + `elVMTOffset` | `mskVMTRef` | `:384-389` |
| `#message'name` | message-id slot | mask `0` (resolved late) | `:262-265` |
| `'dlls'lib.Func` (in `call`) | `$dlls'lib.Func` | `mskExternalRef` | `:1489-1502` |
| `@name` (in `call`) | symbol, PC-relative | `mskSymbolRef\|mskRelativeRef` | `:1518-1530` |
| `@"name"` (in `call`) | native code, PC-relative | `mskNativeCodeRef\|mskRelativeRef` | `:1522-1525` |

`'gc_heapsize` resolves through `Linker::getLinkerConstant` (`linker.cpp:260-268`) to
`[linker] gcsize` in `bin/elc.cfg` — **4096** in the shipped configuration.

### 4.4 JIT template placeholders

These eight names have **negative reference ids** and no mask. They are placeholders that
`copySection` (`x86jitcompiler.cpp:18-79`) substitutes at JIT time with the operands of the
bytecode instruction being compiled. This is the mechanism that lets a single 20-byte
`.asm` template serve every `send`, `create` and `assign` site in a program.

| Token | Ref id | Substituted with | `x86jitcompiler.cpp` |
|---|---|---|---|
| `__arg1` | −1 | argument 1 as a raw dword | `:33-35` |
| `__arg2` | −2 | argument 2 as a raw dword | `:36-38` |
| `__arg1obj` | −3 | argument 1 as an object ref (`+8`) | `:39-41` |
| `__arg2obj` | −4 | argument 2 as an object ref (`+8`) | `:42-44` |
| `__arg1vmt` | −5 | argument 1 as a class ref (`+12`) | `:45-47` |
| `__arg2vmt` | −6 | argument 2 as a class ref (`+12`) | `:48-50` |
| `__arg1fun` | −7 | argument 1 as a call target (`call __arg1fun`) | `:51-53` |
| `__arg2fun` | −8 | argument 2 as a call target | `:54-56` |

Declared at `x86assembler.cpp:47-54`, decoded at `:274-301` and `:1531-1538`.

---

## 5. Supported x86 instruction subset

Dispatch is a hand-rolled trie on the *first letter* of the mnemonic
(`compileCommand`, `x86assembler.cpp:2607-2699`) — `compileCommandA` … `compileCommandZ`.
Letters `b, e, g, h, k, q, u, v, w, y, z` have **empty handlers** (`:2095`, `:2131`, `:2299`,
`:2303`, `:2387`, `:2479`, `:2575`, `:2579`, `:2583`, `:2599`, `:2603`), so any mnemonic
starting with those letters is silently reinterpreted as a **label declaration**
(`:2689-2697` → `fixJump`). A typo such as `bswap eax` produces `"Invalid command or label"`
only because the next token is not `:`.

### 5.1 Integer / general instructions

Notation: `r32` = 32-bit register, `r8`/`r16` = byte/word register, `m` = memory operand,
`imm8`/`imm32` = immediate, `cl` = the CL register used as a shift count.

| Mnemonic | Supported forms | Encoder |
|---|---|---|
| `mov` | `r32,r32/m`; `m,r32`; `r32/m,imm32`; `eax,[disp32]`; `[disp32],eax`; `r8,imm8`; `r8/m8,r8`; `r8,r8/m8`; 16-bit via `0x66` prefix; `fs:` prefix | `x86assembler.cpp:487` |
| `cmp` | `r32,r32/m`; `r32/m,r32`; `r32/m,imm8/imm32`; `al,imm8`; `r8,m8`; `r8/m8,imm8` | `:564` |
| `add` | `eax,imm32`; `r32,r32/m`; `r32/m,r32`; `r32/m,imm8/imm32`; `r8/m8,imm8`; `r8,r8/m8` | `:617` |
| `adc` | same shape as `add` | `:659` |
| `sub` | `r32,r32/m`; `r32/m,r32`; `r32/m,imm8`; `al,imm8`; `r8/m8,imm8` | `:867` |
| `sbb` | `r32/m,imm8`; `r32,r32/m` | `:900` |
| `and` | `r32,imm8`; `r32,r32/m`; `r32/m,r32`; `r32/m,imm32`; `r8,imm8`; 16-bit via `0x66` | `:701` |
| `or` | `eax,imm32`; `r32/m,imm8/imm32`; `r8/m8,r8`; `r8/m8,imm8`; `r32/m,r32`; `r16,r16` | `:775` |
| `xor` | `r32,r32`; `r32,imm8`; `r32/m,imm32`; `r32/m,r32` | `:745` |
| `test` | `eax,imm32`; `r32/m,r32`; `r32/m,imm32/imm8`; `r8/m8,imm8`; `r8/m8,r8` | `:829` |
| `lea` | `r32,m` only | `:920` |
| `shr` | `r32,imm8`; `r32,cl`; `r8,1`; `r8,imm8` | `:935` |
| `sar` | `r32,imm8`; `r32,cl`; `r8,1`; `r8,imm8` | `:964` |
| `shl` | `r32,imm8`; `r32,cl` | `:993` |
| `shld` | `r32,r32,imm8`; `r32,r32,cl` | `:1013` |
| `shrd` | `r32,r32,imm8`; `r32,r32,cl` | `:1038` |
| `rol` | `r8,imm8` only | `:1063` |
| `ror` | `r32,imm8`; `r8,imm8`; 16-bit via `0x66` | `:1079` |
| `rcr` | `r32,1` only | `:1105` |
| `rcl` | `r32,1` only | `:1134` |
| `xchg` | `eax,r32` only | `:1120` |
| `movzx` | `r32, r8/m8` only | `:1149` |
| `push` | `r32`; `m32`; `m16`; `imm8`; `imm32`; `imm16` | `:1164` |
| `pop` | `r32` only | `:1195` |
| `inc` / `dec` | `r32`; `r8/m8`; `dec` also `m32` | `:1271` / `:1254` |
| `neg` / `not` | `r32` only | `:1284` / `:1294` |
| `mul` | `r32/m` | `:1204` |
| `imul` | `r32,imm8` only (encodes `imul r,r,imm8`) | `:1214` |
| `div` | `r32/m`; `r8/m8` | `:1240` |
| `idiv` | `r32/m` | `:1230` |
| `ret` | no-operand only (`0xC3`; there is **no** `ret imm16`) | `:1304` |
| `nop`, `cdq`, `stc`, `sahf`, `pushfd`, `popfd` | no operands | `:1332`, `:1311`, `:1318`, `:1325`, `:1411`, `:1418` |
| `rep`, `repz` | both emit `0xF3` — correct, since `REP` and `REPE/REPZ` share that prefix byte. **`repnz`/`repne` (`0xF2`) does not exist** | `:1339`, `:1346` |
| `lodsd/lodsw/lodsb`, `stosd/stosw/stosb`, `movsb`, `cmpsb` | no operands | `:1353`–`:1409` |

**Absent from the entire encoder:** `sete`/`setcc`, `cmov`, `bt`/`bts`, `bswap`, `xadd`,
`cmpxchg`, `lock`, `int`, `enter`/`leave`, `movsx`, `mul r,imm`, any SSE/MMX instruction,
and any instruction with a 16-bit *addressing* mode. Note in particular: **there are no
atomic instructions at all** — which is one independent reason the current runtime cannot
be made thread-safe without new codegen.

### 5.2 x87 FPU instructions

| Mnemonic | Supported forms | Encoder |
|---|---|---|
| `fld` | `st(i)`; `qword ptr m` | `:1616` |
| `fild` | `dword ptr m`; `qword ptr m` | `:1571` |
| `fbld` | `m` | `:1561` |
| `fst`-family: `fistp`, `fist` | `m` | `:1606`, `:1596` |
| `fstp` | `st(i)`; `qword ptr m` | `:1807` |
| `fbstp` | `tbyte ptr m` | `:1826` |
| `fadd`/`fsub`/`fmul` | `st(0),st(i)`; `st(i),st(0)`; `qword ptr m` | `:1689`, `:1653`, `:1725` |
| `fdiv` | `qword ptr m` only | `:1761` |
| `faddp`, `fmulp` | no operands (implicit `st(1),st(0)`) | `:1900`, `:1949` |
| `fxch` | `st(i)` or bare | `:1636` |
| `ffree` | `st(i)` | `:2033` |
| `fcomip` | `st, st(i)` | `:1773` |
| `fcomp` | `st, st(i)` | `:1790` |
| `fstsw ax` / `fnstsw ax` | fixed | `:1838` / `:1850` |
| `fstcw` / `fldcw` | `word ptr m` | `:1861` / `:1874` |
| Constant loads: `fldz`, `fld1`, `fldpi`, `fldl2t`, `fldl2e`, `fldlg2`, `fldln2` | no operands | `:1886`–`:2031` |
| Transcendental: `f2xm1`, `fyl2x`, `fpatan`, `fsin`, `fcos`, `fsqrt`, `fabs`, `frndint`, `fscale`, `fprem`, `fxam`, `ftst` | no operands | `:1907`–`:2017` |

### 5.3 Control flow

| Mnemonic(s) | Jump type | Encoder |
|---|---|---|
| `jb`, `jc` | `JUMP_TYPE_JB` (0x02) | `x86assembler.cpp:2325` |
| `jnb`, `jnc`, `jae` | `JUMP_TYPE_JAE` (0x03) | `:2329` |
| `jz`, `je` | `JUMP_TYPE_JZ` (0x04) | `:2333` |
| `jnz` | `JUMP_TYPE_JNZ` (0x05) | `:2337` |
| `jbe` | `0x06` | `:2341` |
| `ja`, `jnbe` | `0x07` | `:2345` |
| `js` / `jns` | `0x08` / `0x09` | `:2349` / `:2353` |
| `jpe`, `jp` / `jpo`, `jnp` | `0x0A` / `0x0B` | `:2357` / `:2361` |
| `jl` / `jge` / `jle` / `jg` | `0x0C` / `0x0D` / `0x0E` / `0x0F` | `:2365`–`:2377` |
| `jmp` | unconditional | `:2381` → `:1448` |
| `loop` | `0xE2`, short only | `:2397` → `:1471` |
| `call` | four distinct forms — see below | `:2105` → `:1485` |

There is **no `jne`** alias (only `jnz`), no `jo`/`jno`, and no `jcxz`.

A `short` prefix may precede the target (`compileJxx`, `:1430-1433`). Without it, forward
jumps are emitted as near (rel32) and back jumps are auto-sized by
`x86LabelHelper::writeJxxBack` (`x86helper.cpp:43-50`) — short if the displacement fits in
`±0x82`, otherwise near.

`call` forms (`x86assembler.cpp:1485-1547`):

| Syntax | Emitted | Meaning |
|---|---|---|
| `call 'dlls'lib.Func` | `FF 15 <rel>` (indirect through IAT) | Win32 import |
| `call [operand]` | `FF /2` | indirect call |
| `call @symbol` / `call @"native'name"` | `E8 <rel32>` | PC-relative call to another section |
| `call __arg1fun` / `__arg2fun` | `E8 <rel32>` (placeholder) | JIT-substituted target |
| `call label` | `E8 <rel32>` via `x86JumpHelper` | intra-section call |

### 5.4 Addressing-mode encoding

Operand types are the `x86Helper::OperandType` bitfield (`x86helper.h:19-70`): bits
0x00100000/0x00200000/0x00400000 select 32/8/16-bit width, `0x300`-family bits mark a
register, `0x10000` marks memory, `0x100`/`0x200` mark disp8/disp32.

`readOffset` (`x86assembler.cpp:106-185`) handles at most **two** displacement terms.
A second *register* term is folded into a SIB byte by packing it into bits 26+ of the type
(`:156`, `:170`) — which is why `[edx+ebx]` works but `[edx+ebx*4]` does not: **there is no
scale-factor syntax at all.**

`writeModRM` (`x86helper.h:183-212`) writes the ModRM byte, an optional SIB
(defaulting to `0x24` when the packed SIB field is zero, `:192-194`), then disp8 / disp32 /
relocation. A dedicated `ebpReg` flag on `Operand` (`x86helper.h:80`) exists solely to
disambiguate `[ebp]` (which needs a disp8 of 0) from a bare `disp32`.

### 5.5 Label resolution

Labels are function-local (a fresh `x86JumpHelper` per `compileProcedure`,
`x86assembler.cpp:2716`). `x86JumpHelper` (`x86jumphelper.cpp`) maps *names* to small
integer ids and forwards to `x86LabelHelper` (`x86helper.cpp:15-300`), which:

1. records every forward jump's patch position in a `jumps` multimap (`x86helper.cpp:26-40`);
2. on `setLabel` (`:181`) backpatches all pending jumps for that id (`fixLabel`, `:212`);
3. if a short forward jump turns out to exceed ±0x80, **promotes it to near in place**
   (`fixShortLabel` → `convertShortToNear`, `:185-200`) and then calls `fixJumps`
   (`:230-300`) to shift every other recorded jump by the inserted bytes.

This in-place promotion is the single most delicate part of the tool. It is also why
`compileProcedure` can be single-pass.

Declaring the same label twice raises `"Label with such a name already exists"`
(`x86assembler.cpp:1551-1553`).

---

## 6. Output format

`compile()` (`x86assembler.cpp:2733-2772`) builds an in-memory `Module` named `$binary` and
writes it verbatim with `Module::save` (`module.cpp:146-170`):

```
"EN!10"                      MODULE_SIGNATURE, elenaconst.h:149
"$binary\0"                  module name
<reference map>              name -> ref_t
<message map>                message name -> id
<constant map>               (empty for .bin)
<section map>                (ref_t | mask) -> Section{ bytes, RelocationMap }
```

This is **the same container format as a compiled ELENA module (`.nl`)** — `.bin` is not a
special format, it is a module that happens to contain only native sections.

Section keys:

| Directive | Section key | Value |
|---|---|---|
| `procedure X` / `inline X` | `mapReference("$package'X") \| mskNativeCodeRef` | `0x48000000` |
| `structure X` | `mapReference("$package'X") \| mskNativeDataRef` | `0x28000000` |

The `$package'` prefix comes from `LocalReferenceName(PACKAGE_MODULE, name)`
(`x86assembler.cpp:2705`, `:2050`; `PACKAGE_MODULE` = `"$package"`, `elenaconst.h:24`) —
which is exactly how `elenaconst.h:43-69` names the core functions
(`ALLOC_FUNCTION = "$package'elena'1"` and friends).

Each section carries a `RelocationMap` mapping *reference|mask* → *byte offset*. Three
mask values get special treatment downstream in
`ReferenceLoader::loadNativeSection` (`jitlinker.cpp:198-217`):

| Mask | Handling |
|---|---|
| `mskLinkerConstant` (`0x0D000000`) | replaced by `getLinkerConstant()` — e.g. `gcsize` |
| `0` (no mask) | replaced by `resolveMessageID()` — the `#message` form |
| anything else | recursive `load()` then normal relocation |

### 6.1 How the build consumes `.bin`

1. `bin/elc.cfg` `[primitives]` maps a logical name to a file: `elena=elena.bin`,
   `win32=win32.bin`, `standard=standard.bin`, `extended=extended.bin`, `winsock=winsock.bin`.
2. `Project::resolvePrimitive` (`project.cpp:141-163`) loads and caches each `Module`.
3. `x86JITCompiler`'s constructor (`x86jitcompiler.cpp:466-491`) pulls **22 named sections**
   out of `elena.bin` and keeps them as inline templates:

   | Template | `.asm` symbol | Template | `.asm` symbol |
   |---|---|---|---|
   | `cmdPrepare` | `elena'4` | `cmdOCreate6` | `elena'14` |
   | `cmdSPrepare` | `elena'5` | `cmdOCreate0` | `elena'15` |
   | `cmdReturn` | `elena'6` | `cmdCallExt` | `elena'16` |
   | `cmdIOCallN` | `elena'7` | `cmdPrepRedir` | `elena'17` |
   | `cmdSExit` | `elena'8` | `cmdExitRedir` | `elena'18` |
   | `cmdSReturn` | `elena'9` | `cmdRedirect` | `elena'19` |
   | `cmdRReturnIf` | `elena'10` | `cmdRRedirect` | `elena'20` |
   | `cmdOCreate` | `elena'11` | `cmdIOSWAP` | `elena'25` |
   | `cmdOCreate2` | `elena'12` | `cmdIOSet` | `elena'26` |
   | `cmdOCreate4` | `elena'13` | `cmdShift` | `elena'28` |
   | | | `cmdUnShift` | `elena'29` |
   | | | `cmdIRCallN` | `elena'30` |

4. `ReferenceLoader::preloadCoreCode` (`jitlinker.cpp:494-533`) allocates the 0x30-byte GC
   table and the static-root block, then force-loads `elena'1` (allocator), `$elena'$nil`,
   `elena'21` (group) and `elena'36` (cast).
5. `Linker::createImage` (`linker.cpp:292-313`) loads the entry symbol —
   `$package'elena'3` for console (`bin/templates/console.cfg`) or `$package'elena'35` for
   GUI (`bin/templates/gui.cfg`).
6. Everything else (`standard'*`, `win32'*`, `winsock'*`, `extended'*`) is pulled on demand
   when ELENA source says `#inline standard'8(...)` or `#external win32'2(...)`.

---

## 7. Deletion analysis

### 7.1 What breaks if `asm2bin` is deleted today

| Consumer | Breakage | Severity |
|---|---|---|
| `x86JITCompiler` constructor | Cannot find its 22 inline templates → **no code generation at all** | fatal |
| `ReferenceLoader::preloadCoreCode` | No GC allocator, no `$nil`, no group/cast pseudo-VMTs | fatal |
| `Linker::createImage` | No entry point symbol | fatal |
| ~165 distinct `#inline` / `#external` sites in `src/*.l` | Unresolved link errors | fatal |
| `bin/elc.cfg [primitives]` | Five dangling paths | cosmetic |

In short: **deleting `asm2bin` without a replacement runtime deletes the language.** The
tool itself is trivial to replace; the 9 481 lines of `.asm` it consumes are not.

### 7.2 Is the tool worth keeping on its own merits?

No. Weighed as an assembler it is strictly worse than any off-the-shelf option:

| Property | `asm2bin` | NASM/YASM | LLVM MC |
|---|---|---|---|
| Instruction coverage | ~90 mnemonics, x87 only | full x86/x86-64 | full |
| 64-bit | no | yes | yes |
| Atomics (`lock`, `cmpxchg`) | **none** | yes | yes |
| SSE/AVX | none | yes | yes |
| Expressions in operands | none (two-term displacement max) | full | full |
| Macros | none (`define` = integer alias) | full | full |
| Object-file output | proprietary ELENA module | ELF/COFF/Mach-O | ELF/COFF/Mach-O |
| Error reporting | row number, no column, exit code always 0 | good | good |
| Silent-failure modes | unknown mnemonics starting with `b,e,g,h,k,q,u,v,w,y,z` become labels | none | none |
| Lines to maintain | 3 335 | 0 (external) | 0 (external) |

The only thing it does that a stock assembler cannot is emit **ELENA relocation records**
— specifically the `__arg1..__arg2fun` JIT placeholders and the `#message` / `'gc_*` /
`'nil` symbolic references. That capability is worth ~200 lines, not 3 335.

### 7.3 Replacement strategy

The right question is not "what replaces `asm2bin`" but "what replaces the *template
mechanism*". Three tiers, in increasing order of desirability:

**Tier 1 — mechanical port (keeps the current architecture).**
Rewrite the five `.asm` files in NASM syntax, assemble to ELF/COFF, and write a ~300-line
`obj2bin` that reads the object file's relocation table and re-emits it as an ELENA
`RelocationMap`. The `__argN` placeholders become named external symbols
(`extern __elena_arg1`) that `obj2bin` maps back to ids −1…−8. Keeps the JIT-splicing
design intact. **Effort: 2–4 weeks. Value: low** — you still own hand-written x86.

**Tier 2 — portable C runtime (recommended).**
Rewrite the runtime as C compiled to a static library, and let LLVM generate calls into it
rather than splicing byte templates:

| `.asm` content | Becomes |
|---|---|
| `elena'1`, `'2`, `'31`–`'33` (GC) | C (`elena_alloc`, `elena_collect`, `elena_write_barrier`) |
| `elena'3`, `'35` (entry) | C `main` / `WinMain` |
| `elena'7`, `'19`, `'20`, `'21`, `'23`, `'30`, `'36` (dispatch) | LLVM IR emitted by the codegen, or a C helper `elena_send` |
| `elena'4`–`'6`, `'8`, `'9`, `'17`, `'18` (frames) | disappear — LLVM's own prologue/epilogue |
| `elena'11`–`'15` (create) | LLVM IR inline (`alloc` + `store` VMT + zero fields) |
| `standard'*` (arith/string) | C, or LLVM intrinsics (`llvm.sqrt`, `__udivti3`, etc.) |
| `win32'*`, `winsock'*`, `extended'*` | C wrappers over POSIX / Win32 / Cocoa |
| `elena'37` (WndProc) | platform-specific C |

`asm2bin` is deleted in full. **Effort: the real work is 3–6 months of runtime
reimplementation** (see [`07-runtime-core-asm.md` §9](07-runtime-core-asm.md)).

**Tier 3 — LLVM MC for the last irreducible fragments.**
A handful of things genuinely cannot be written in C: reading the current stack pointer for
root scanning, the `fs:[4]/fs:[8]` stack-bounds probe (`elena.asm:1052-1053`), and any
future safepoint poll. Those become 5–20 line inline-asm blocks or LLVM intrinsics
(`llvm.frameaddress`, `llvm.stacksave`), assembled by the compiler you already ship.

### 7.4 Recommendation

**Delete `asm2bin` — but only as the *last* step of the runtime rewrite, not the first.**
Sequence:

1. Port the runtime subsystem-by-subsystem to C, keeping the `.bin` mechanism alive so both
   can coexist (a C function can be called via the existing `#external` path).
2. Replace the JIT inline templates with LLVM IR emission.
3. When no `.asm` file is referenced by any `.l` file or by `x86jitcompiler.cpp`, delete
   `elenasrc/asm2bin/`, `src/asm/`, `elenasrc/engine/win32/x86helper.*`, the
   `[primitives]` section of `elc.cfg`, and `Project::resolvePrimitive`.

Deleting it earlier means hand-porting 9 481 lines of assembly in one atomic change, with
no way to bisect a regression.
