# The ELENA Engine — Bytecode, Module Format, JIT Compiler, JIT Linker

> **Scope.** `elenasrc/engine/` (platform-independent) + `elenasrc/engine/win32/` (x86 backend),
> plus the hand-written runtime core in `src/asm/elena.asm` that the JIT splices into
> every generated executable.
>
> **Status.** Written from the 2009 source tree as found. Everything here was verified
> against the code; where `doc/tech/bytecode.txt` and `doc/tech/jitlinker.txt` disagree
> with the implementation, the disagreement is called out explicitly (§10).
>
> **Why this document matters for the LLVM migration.** The engine is where the language
> semantics stop being abstract and become machine state: which register holds `self`,
> what "message failure" compiles to, what an object header looks like, where the GC
> roots live. An LLVM backend has to reproduce *these* invariants, not the source
> language. §11 maps each of them to an LLVM concept — or explains why there isn't one.

Related: [`04-pe-linker.md`](04-pe-linker.md) (what consumes the image this produces),
[`../00-project-overview.md`](../00-project-overview.md).

---

## Table of contents

1. [Overview — the "no VM" execution model](#1-overview--the-no-vm-execution-model)
2. [Complete bytecode reference](#2-complete-bytecode-reference)
3. [Object model & memory layout](#3-object-model--memory-layout)
4. [Module (`.nl`) file format](#4-module-nl-file-format)
5. [The JIT linker](#5-the-jit-linker)
6. [x86 code generation](#6-x86-code-generation)
7. [Garbage collection interface](#7-garbage-collection-interface)
8. [Debug info format](#8-debug-info-format)
9. [Core constants reference](#9-core-constants-reference)
10. [Where the reference docs disagree with the code](#10-where-the-reference-docs-disagree-with-the-code)
11. [Modernization notes — mapping to LLVM](#11-modernization-notes--mapping-to-llvm)

---

## 1. Overview — the "no VM" execution model

### 1.1 There is no virtual machine and there is no runtime JIT

Despite the class names (`_JITCompiler`, `x86JITCompiler`, `ReferenceLoader` a.k.a. "JIT
linker"), **nothing in ELENA 1.5.0 compiles code at run time.** The word "JIT" here means
*"translated lazily, on first reference, during linking"* — what today would be called
**AOT compilation with lazy, demand-driven reachability**.

```
   .l source
      │  elc: parser + Compiler (elenasrc/elc/compiler.cpp)
      ▼
   ELENA bytecode, stored in .nl module files          ← §2, §4
      │
      │  ═══════ still inside the same elc process ═══════
      ▼
   ReferenceLoader::load()  (engine/jitlinker.cpp:439)  ← §5
      │  demand-driven: pulls in only what the entry point transitively reaches
      ▼
   x86JITCompiler::compileMethod() (win32/x86jitcompiler.cpp:521)  ← §6
      │  bytecode → native x86, one method at a time, into in-memory Sections
      ▼
   Linker::run() (elc/win32/linker.cpp:619)             ← doc 04
      │  assign RVAs, apply relocations, emit PE headers
      ▼
   .exe  (+ optional .dn debug sidecar)                 ← §8
```

The produced `.exe` contains **only native x86**. There is no interpreter, no bytecode in
the image, no runtime code generator, no metadata reader. The entire `engine/` directory
is *compile-time-only* code: it ships inside `elc.exe`, never inside a user program.

### 1.2 What actually is in the produced executable

| Component | Origin | Where it lands |
|---|---|---|
| Native code for every reachable method/symbol | `x86JITCompiler::compileMethod` translating bytecode | `.text` |
| Inline expansions of ~22 hand-written asm routines | `src/asm/elena.asm`, precompiled to `elena.bin` by `asm2bin` | `.text` (copied per use site) |
| Whole hand-written asm procedures (GC, entry point, `group`, `cast`) | same | `.text` (copied once) |
| Whole hand-written asm procedures from other packages (`standard.bin`, `win32.bin`, …) | `src/asm/*.asm` | `.text` (copied once, or inlined per use) |
| VMTs, constant objects, the GC control table | built by `_JITCompiler::compileVMT`, `ReferenceLoader::loadConstant`, `preloadCoreCode` | `.data` |
| Static symbol slots (GC roots) | `ReferenceLoader::loadStaticVariable` | `.bss` |
| PE import directory | `Linker::createImportTable` | `.import` |

So the "runtime" is: the `.asm` core routines (allocator, collector, dispatcher fragments,
process entry point) plus whatever `kernel32`/`user32` imports the program uses. There is
no `libelena.so` equivalent — the runtime is *statically inlined into every executable*.

### 1.3 The three-stage translation, precisely

**Stage 1 — bytecode emission (`elc`).** `Compiler` drives `ByteCodeCompiler`
(`engine/bccompiler.cpp`) which appends `ByteCommand` records to a `CommandTape`
(a linked list, `bytecode.h:179`). At the end of a class or symbol,
`ByteCodeCompiler::save` (`bccompiler.cpp:858`) walks the tape and serialises it into
`Section` objects inside a `Module`, resolving *intra-procedure* label offsets on the way
(`saveProcedure`, `bccompiler.cpp:621`). The result is written to a `.nl` file.

**Stage 2 — JIT linking (`elc`, link phase).** `ReferenceLoader::load(name, mask, silent)`
(`jitlinker.cpp:439`) is the single entry point. Given a *reference name* and a *mask*
saying what kind of thing it is, it:

* checks whether it is already loaded (`_helper->resolveReference`);
* if not, dispatches to `loadNativeSection` / `loadBytecodeSection` /
  `loadBytecodeVMTSection` / `loadConstant` / `loadStaticVariable`;
* those recursively call `load()` for anything they reference.

Loading starts from a single root: the project's `start` symbol
(`Linker::createImage`, `linker.cpp:303`), which for a console app is
`$package'elena'3` — the hand-written process entry point (`bin/templates/console.cfg`).
Everything unreachable from it is never translated and never appears in the executable.
This is **treeshaking by construction**.

**Stage 3 — image fixup and PE emission.** Covered in [`04-pe-linker.md`](04-pe-linker.md).

### 1.4 Two addressing modes: "virtual" and "real"

`ReferenceLoader` is constructed with a `_virtualMode` flag (`jitlinker.h:142`). The
`elc` linker always passes `true` (`linker.h:108`). In virtual mode:

* an object's *virtual address* is `mask | (sectionRelativeByteOffset >> 3)`
  (`calculateVAddress`, `jitlinker.cpp:121`) — hence the global 8-byte alignment
  (`VA_ALIGNMENT = 0x08`, `elena.h:225`);
* references are not patched immediately; they are recorded in the target `Section`'s
  relocation map and fixed up later against the final image bases
  (`resolveReference`, `jitlinker.cpp:17`).

The non-virtual path (`_virtualMode == false`) patches absolute addresses directly and
exists for a *runtime* loader that was never built in 1.5.0. It is dead code in `elc`,
but it is the seam that a future in-process VM would have used.

---

## 2. Complete bytecode reference

### 2.1 Instruction encoding

A bytecode instruction is:

```
 byte 0        : opcode
 bytes 1..4    : argument1   (present iff (opcode & 0x03) != 0)
 bytes 5..8    : argument2   (present iff (opcode & 0x03) == 0x03)
 bytes 9..12   : extra dword (only for bcIRCall0 / bcIRCall1 — the parent VMT reference)
```

Decoding is done in exactly one place, `x86JITCompiler::compileMethod`
(`x86jitcompiler.cpp:521-543`):

```cpp
code = tapeReader.getByte();
if ((code & 0x3) != 0) {
   scope.argument1 = tapeReader.getDWord();
   if (test(code, 0x3))                 // test() means (code & 3) == 3
      scope.argument2 = tapeReader.getDWord();
}
_commands[code >> 0x4](code & ~bcCommand, scope);
```

Two independent fields are packed into the opcode byte:

| Field | Bits | Meaning |
|---|---|---|
| **command group** | `opcode >> 4` (high nibble) | index into `_commands[0x10]`, the 16-entry handler table built at `x86jitcompiler.cpp:493-508` |
| **operand form** | `opcode & 0x0F` (low nibble) | passed to the handler as its `opcode` parameter; selects the addressing mode |
| **argument count** | `opcode & 0x03` (low 2 bits) | 0 → none, 1 or 2 → one dword, 3 → two dwords |

`bcCommand = 0xFF0` (`bytecode.h:108`) and `code & ~bcCommand` is just `code & 0x0F`
for a byte-sized `code`.

The operand-form nibbles are named constants (`bytecode.h:67-89`):

| Constant | Value | Meaning |
|---|---|---|
| `bcROperand` | `0x01` | reference operand (object address, `+elEmptyObject`) |
| `bcIOperand` | `0x02` | raw integer operand |
| `bcREOperand` | `0x03` | reference + branch-on-fail offset |
| `bcRedirOperand` | `0x04` | redirect form |
| `bcIOOperand` | `0x06` | `[esp + n*4]` — evaluation-stack-relative |
| `bcExtOperand` | `0x07` | external (native procedure) call form |
| `bcSOperand` | `0x08` | `self` (EDI) form |
| `bcRPtrOperand` | `0x09` | reference-to-pointer (`[ref]`) |
| `bcIFOperand` | `0x0A` | `[ebp ± n*4]` — frame-relative |
| `bcSwapOperand` | `0x0A` | (group `0xF` re-use) swap form |
| `bcSPrmOperand` | `0x0C` | `self`-prologue with parameter |
| `bcISOperand` | `0x0E` | `[edi + (n-1)*4]` — field of `self` |
| `bcRoleOperand` | `0x0E` | (group `0xF` re-use) role-shift form |
| `bcOPtrOperand` | `0x0F` | `[[esp+n*4] + m]` — field of a stacked object |
| `bcEmbOperand` | `0x0F` | (group `4` re-use) embedded-code form |
| `bcION0`/`bcION1` | `0x03`/`0x07` | `iocall` with VMT start offset 0 / 8 |
| `bcIRN0`/`bcIRN1` | `0x0B`/`0x0F` | `ircall` with VMT start offset 0 / 8 |

### 2.2 The dispatch table

| Group | Handler | `x86jitcompiler.cpp` | Opcodes in group |
|---|---|---|---|
| `0x0` | `compileNop` | :113 | `bcNop` |
| `0x1` | `compilePrep` | :124 | `bcPrep`, `bcPrepRedir`, `bcSPrep`, `bcSPrepParam` |
| `0x2` | `compilePush` | :152 | `bcPush`, `bcRPush`, `bcIPush`, `bcIOPush`, `bcSPush`, `bcRPushPtr`, `bcIFPush`, `bcISPush` |
| `0x3` | `compileReturn` | :199 | `bcReturn`, `bcSReturn` |
| `0x4` | `compileCall` | :220 | `bcRCall`, `bcRCallExt`, `bcRCallEmb` |
| `0x5` | `compilePop` | :242 | `bcPop` |
| `0x6` | `compileMove` | :248 | `bcIOMove`, `bcRMovePtr`, `bcIFMove`, `bcISMove`, `bcOMovePtr` |
| `0x7` | `compileExit` | :303 | `bcExitRedir`, `bcSExit` |
| `0x8` | `compileRedirect` | :314 | `bcRedirect`, `bcRRedirect` |
| `0x9` | `compileSet` | :324 | `bcIOSet`, `bcIFSet` |
| `0xA` | `compileNop` | :113 | *(unused)* |
| `0xB` | `compileReturnIf` | :339 | `bcRReturnIf` |
| `0xC` | `compileIOCallN` | :346 | `bcIOCall0`, `bcIOCall1`, `bcIRCall0`, `bcIRCall1` |
| `0xD` | `compileNop` | :113 | *(unused)* |
| `0xE` | `compileNop` | :113 | *(unused)* |
| `0xF` | `compileOthers` | :424 | `bcUnShift`, `bcIJump`, `bcOCreate`, `bcIOSwap`, `bcDebug`, `bcShift` |

**41 opcodes are defined** (`bytecode.h:19-65`); one of them (`bcPush = 0x20`) is dead —
its operand nibble is 0, which no branch of `compilePush` handles, and `bccompiler.cpp`
never emits it. **4 further opcodes are commented out** (`bcCall = 0x40`,
`bcCallRedir = 0x46`, `bcMove = 0x60`, `bcExit = 0x70`) — remnants of a pre-1.5 encoding.
**10 pseudo-opcodes** (values > `0xFF`) exist only inside the in-memory `CommandTape` and
are consumed by `saveProcedure`; they never reach a `.nl` file.

### 2.3 Master opcode table

Stack effect is expressed in 4-byte slots of the machine stack (which *is* the ELENA
evaluation stack). `S` = evaluation stack, top on the right.

| # | Mnemonic | Value | Args in tape | Stack effect | Semantics |
|---:|---|---|---|---|---|
| 1 | `nop` | `0x00` | — | 0 | Label mark. Not an instruction: tells the JIT "a branch may target this tape offset". |
| 2 | `prep` | `0x10` | — | 0 | Symbol prologue: open a frame. |
| 3 | `prepredir` | `0x14` | — | 0 | Redirect-method prologue: save message id + self, open frame. |
| 4 | `sprep` | `0x18` | — | 0 | Method prologue: save self, adopt receiver as new self, open frame. |
| 5 | `sprepparam` | `0x1C` | — | +1 | `sprep` + push the message parameter as local #1. |
| 6 | `push` | `0x20` | — | — | **Dead opcode.** No handler branch, never emitted. |
| 7 | `rpush ref` | `0x21` | ref | +1 | Push a static object address (`ref` + 8). |
| 8 | `ipush n` | `0x22` | int | +1 | Push a raw integer. |
| 9 | `iopush` | `0x26` | int (always 0) | +1 | Duplicate the top of stack. |
| 10 | `spush` | `0x28` | — | +1 | Push `self` (EDI). |
| 11 | `rpushptr ref` | `0x29` | ref | +1 | Push the *contents* of a static slot (`[ref]`). |
| 12 | `ifpush n` | `0x2A` | int | +1 | Push frame slot `n` (n>0 = local, n<0 = caller's slot). |
| 13 | `ispush n` | `0x2E` | int | +1 | Push field `n` of `self` (1-based). |
| 14 | `return` | `0x30` | — | −1 (frame) | Symbol epilogue: pop result, restore frame, store result into caller's slot. |
| 15 | `sreturn` | `0x38` | — | −1 (frame) | Method epilogue with explicit result. Also restores `self`. |
| 16 | `rcall ref, else` | `0x43` | ref, i32 | ±0 | Call a symbol; if it returns 0, branch to `else`. |
| 17 | `rcallext ref, else` | `0x47` | ref, i32 | ±0 | Call a native procedure through the `callext` frame-registering wrapper. |
| 18 | `rcallemb ref, else` | `0x4F` | ref, i32 | varies | **Inline** the referenced native section verbatim at this point. |
| 19 | `pop` | `0x50` | — | −1 | Discard the top of stack (into EDX). |
| 20 | `iomove n` | `0x66` | int | 0 | `S[top-n] := S[top]`. |
| 21 | `rmoveptr ref` | `0x69` | ref | 0 | `[ref] := S[top]` (write a static slot). |
| 22 | `ifmove n` | `0x6A` | int | 0 | Frame slot `n` := `S[top]`. |
| 23 | `ismove n` | `0x6E` | int | 0 | Field `n` of `self` := `S[top]`. |
| 24 | `omoveptr n, m` | `0x6F` | int, int | 0 | `field m of S[top-n] := S[top]`. |
| 25 | `exitredir` | `0x74` | — | frame | Redirect-method epilogue; **result is always 0 (= failure)**. |
| 26 | `sexit` | `0x78` | — | frame | Method epilogue without explicit result; result is the receiver, always non-zero. |
| 27 | `redirect` | `0x80` | — | ±0 | Re-send the current message to `S[top]`; on success, return from the method. |
| 28 | `rredirect ref` | `0x81` | ref | ±0 | Same, but start the search in the VMT named by `ref` (super-send). |
| 29 | `ioset n, vmt` | `0x93` | int, ref | 0 | `*(S[top] + n) := vmt` — store a VMT pointer into a field. |
| 30 | `ifset n` | `0x9A` | int | resets | `esp := ebp − n*4` — unwind the evaluation stack to a scope level. |
| 31 | `rreturnif ref` | `0xB1` | ref | −1/0 | If `S[top]` is **not** `ref`, return it immediately; if it still is, fall through with the stack unchanged. (The asm template in §2.5.31 is the authority: memoisation returns the cache once it is no longer nil.) |
| 32 | `iocall(0) msg, else` | `0xC3` | msg, i32 | −1 | Message send, VMT scan starts at entry **0**. Used only for `new`. |
| 33 | `iocall(1) msg, else` | `0xC7` | msg, i32 | −1 | Message send, VMT scan starts at entry **1**. All other messages. |
| 34 | `ircall(0) msg, else, vmt` | `0xCB` | msg, i32, +ref | −1 | Like `iocall(0)` but scans the VMT given by `vmt` (super-send of `new`). |
| 35 | `ircall(1) msg, else, vmt` | `0xCF` | msg, i32, +ref | −1 | Like `iocall(1)` but scans the VMT given by `vmt`. |
| 36 | `unshift` | `0xF0` | — | 0 | Leave the current role: restore `self`'s VMT from the role's owner field. |
| 37 | `ijump off` | `0xF2` | i32 | 0 | Unconditional jump, tape-relative. |
| 38 | `ocreate size, vmt` | `0xF3` | int, ref | +1 | Allocate an object, set its VMT, nil-fill it, push it. |
| 39 | `ioswap n` | `0xFA` | int | 0 | *Intended* `S[top] ↔ S[top−n]`. **Implemented as a no-op — see §2.5.39.** |
| 40 | `dbgbreak` | `0xFC` | — | 0 | Emit nothing; record a breakpoint address for the debugger. |
| 41 | `shift n` | `0xFE` | int | 0 | Enter role `n`: replace `self`'s VMT pointer with role VMT `n`. |

### 2.4 Pseudo-opcodes (tape-only in 2009; two are now serialised)

| Mnemonic | Value | Handled at | Effect |
|---|---|---|---|
| `bcExtraParam` | `0x100` | `bccompiler.cpp:700` | Writes a bare dword (no opcode byte) — the trailing VMT reference of `ircall`. |
| `bcAllocStack` | `0x101` | `bccompiler.cpp:640` | `stackLevel += n`, **and serialised** as byte `0x01` + count. The x86 JIT skips it (family 0 → `compileNop`, argument consumed by the tape reader); the LLVM stack-to-SSA translation reads it as depth metadata. |
| `bcFreeStack` | `0x102` | `bccompiler.cpp:643` | `stackLevel −= n`, **and serialised** as byte `0x02` + count. Directly after `rcallemb` it is load-bearing: the only record of how many slots an embedded blob consumes. |
| `blBegin` | `0x110` | `bccompiler.cpp:646` | Open a scope; for `bltLoop` also emits a `nop` label. |
| `blEnd` | `0x111` | `bccompiler.cpp:665` | Close a scope; for `bltBranch` resolves forward jumps and emits a `nop`. |
| `blFailure` | `0x112` | `bccompiler.cpp:654` | Failure label; resolves pending branch/proc failure jumps, emits a `nop`. |
| `blDeclare` | `0x113` | `bccompiler.cpp:789` | Only inside a role table: declares one role VMT entry. |
| `bdBreakpoint` | `0x201` | `bccompiler.cpp:682` | Emits a `DebugLineInfo` record into the debug module. |
| `bdBreakCoord` | `0x202` | `bccompiler.cpp:155` | Column/length continuation of the preceding `bdBreakpoint`. |
| `bdLocal` | `0x203` | `bccompiler.cpp:678` | Emits a local-variable `DebugLineInfo` record. |

`LabelType` values (`bytecode.h:111`) qualify `blBegin`/`blEnd`/`blFailure`:
`bltNone 0`, `bltSymbol 1`, `bltClass 2`, `bltMethod 3`, `bltProc 4`, `bltBranch 5`,
`bltRole 6`, `bltLoop 7`.

### 2.5 Per-opcode detail: the exact x86 the JIT emits

Notation: `→` shows the emitted machine bytes; `〈inline N〉` means "copy the whole body of
`$package'elena'N` from `elena.bin` here, patching its `__argN` placeholders".
The 22 inline templates are loaded once in the `x86JITCompiler` constructor
(`x86jitcompiler.cpp:466-490`) and spliced by `copySection` (`x86jitcompiler.cpp:18`).

---

#### 1. `nop` — `0x00` — group 0 → `compileNop` (`x86jitcompiler.cpp:113`)

Emits **nothing**. Registers the current *tape* position as a jump label:

```cpp
if (scope.lh.checkLabel(scope.tape->Position() - 1))
   scope.lh.fixLabel(scope.tape->Position() - 1);     // back-patch pending forward jumps
else scope.lh.setLabel(scope.tape->Position() - 1);
```

Labels are keyed by **bytecode tape offset**, not code offset — that is how the two
address spaces are kept in sync (`x86LabelHelper`, `x86helper.h:253`).

---

#### 2. `prep` — `0x10` — `compilePrep(0)` (`x86jitcompiler.cpp:145`)

```
→ 〈inline elena'4〉   (elena.asm:553)
     55                 push ebp
     8B EC              mov  ebp, esp
scope.prevFSPOffs = 4
```

---

#### 3. `prepredir` — `0x14` (`bcRedirOperand`) — `x86jitcompiler.cpp:139`

```
→ 〈inline elena'17〉  (elena.asm:724)
     52                 push edx        ; the message id must survive the method body
     57                 push edi        ; save caller's self
     8B F8              mov  edi, eax   ; self := receiver
     55                 push ebp
     8B EC              mov  ebp, esp
     53                 push ebx        ; (written explicitly at x86jitcompiler.cpp:141)
scope.prevFSPOffs = 0x0C
```

---

#### 4. `sprep` — `0x18` (`bcSOperand`) — `x86jitcompiler.cpp:126`

```
→ 〈inline elena'5〉   (elena.asm:561)
     57                 push edi
     8B F8              mov  edi, eax
     55                 push ebp
     8B EC              mov  ebp, esp
scope.prevFSPOffs = 8
```

---

#### 5. `sprepparam` — `0x1C` (`bcSPrmOperand`) — `x86jitcompiler.cpp:131`

```
→ 〈inline elena'5〉 followed by
     53                 push ebx        ; the message parameter becomes local #1 at [ebp-4]
scope.prevFSPOffs = 8
```

---

#### 7. `rpush ref` — `0x21` (`bcROperand`) — `x86jitcompiler.cpp:154`

```
→ 68 <imm32>            push imm32
```
The imm32 is a relocation: `writeReference(code, argument1, elEmptyObject)` — the final
value is `objectBase + 8`, i.e. it skips the object header (§3.1).

---

#### 8. `ipush n` — `0x22` (`bcIOperand`) — `x86jitcompiler.cpp:159`

```
→ 68 <n>                push n          ; raw integer, no relocation
```
Emitted by `newLocal` (`bccompiler.cpp:257`, `ipush 0` reserves a local slot) and by the
failure epilogues (`ipush 0` = "the result is 0 = failure", `bccompiler.cpp:565`).

---

#### 9. `iopush` — `0x26` (`bcIOOperand`) — `x86jitcompiler.cpp:186`

```
→ FF B4 24 <-(n<<2)>    push dword [esp + disp32]
```
`n` is always 0 when emitted by `pushCurrent` (`bccompiler.cpp:398`), so this is
`push dword [esp]` — duplicate the current object. Note the JIT negates the displacement
(`-(argument1 << 2)`), which for `n = 0` is 0.

---

#### 10. `spush` — `0x28` (`bcSOperand`) — `x86jitcompiler.cpp:164`

```
→ 57                    push edi
```

---

#### 11. `rpushptr ref` — `0x29` (`bcRPtrOperand`) — `x86jitcompiler.cpp:168`

```
→ BA <imm32>            mov  edx, imm32   ; relocation, disp 0 (raw slot address)
  FF 32                 push dword [edx]
```
Used by `newStaticSymbol` (`bccompiler.cpp:198`) to read a static symbol's cached value.

---

#### 12. `ifpush n` — `0x2A` (`bcIFOperand`) — `x86jitcompiler.cpp:175`

```
n > 0 → FF B5 <-(n<<2)>                 push dword [ebp - n*4]      ; local variable n
n < 0 → FF B5 <prevFSPOffs - (n<<2)>    push dword [ebp + prevFSPOffs + |n|*4]
```
Negative `n` reaches into the *caller's* frame. With `sprep` (`prevFSPOffs = 8`),
`ifpush -1` is `[ebp+12]` — the receiver slot, i.e. `self` as seen by the caller
(`otVSelf`, `bccompiler.cpp:361`). With `prep` (`prevFSPOffs = 4`), `ifpush -1` is
`[ebp+8]` — the symbol's result slot (`VSELF_PTR_OFFSET`, `compiler.cpp:25`).

---

#### 13. `ispush n` — `0x2E` (`bcISOperand`) — `x86jitcompiler.cpp:192`

```
→ FF B7 <(n-1)<<2>      push dword [edi + (n-1)*4]
```
Fields are numbered from 1 (`compiler.cpp:1540`: `fields.add(name, Count()+1)`), so field 1
is at offset 0 from the object pointer.

---

#### 14. `return` — `0x30` (`opcode == 0`) — `x86jitcompiler.cpp:204`

```
→ 〈inline elena'6〉   (elena.asm:571)
     58                 pop  eax          ; the result
     8B E5              mov  esp, ebp
     5D                 pop  ebp
     89 44 24 04        mov  [esp+4], eax ; overwrite the caller's placeholder slot
     C3                 ret
```

---

#### 15. `sreturn` — `0x38` (`bcSOperand`) — `x86jitcompiler.cpp:201`

```
→ 〈inline elena'9〉   (elena.asm:621)
     58                 pop  eax
     8B E5              mov  esp, ebp
     5D                 pop  ebp
     5F                 pop  edi          ; restore caller's self
     89 44 24 04        mov  [esp+4], eax ; result replaces the receiver slot
     C3                 ret
```
EAX is left holding the result — that is the success/failure signal (§2.6).

---

#### 16. `rcall ref, else` — `0x43` (`bcREOperand`) — `x86jitcompiler.cpp:223`

```
→ E8 <rel32>            call ref          ; relocation: argument1 | mskRelativeRef
  85 C0                 test eax, eax
  74 <rel8> | 0F 84 <rel32>   jz else
```
The `test/jz` pair is emitted by `compileJumpIfNot` (`x86jitcompiler.cpp:81`); the short
form is chosen when the *bytecode* jump offset is `< 0x10` (`x86jitcompiler.cpp:239`) —
a heuristic, corrected later by `convertShortToNear` if it turns out too small
(`x86helper.cpp:302`).

---

#### 17. `rcallext ref, else` — `0x47` (`bcExtOperand`) — `x86jitcompiler.cpp:228`

```
argument1 |= mskRelativeRef
→ 〈inline elena'16〉  (elena.asm:707)
     57                 push edi
     A1 <gc_table+0>    mov  eax, [gs_current_frame]
     50                 push eax                    ; link the previous GC frame
     8B C8               mov  ecx, eax
     2B CC               sub  ecx, esp
     89 08               mov  [eax], ecx             ; record the previous frame's extent
     55                 push ebp
     8D 6C 24 08         lea  ebp, [esp+8]
     E8 <rel32>          call __arg1fun              ; ← argument1
     5D                  pop  ebp
     5A                  pop  edx
     89 15 <gc_table+0>  mov  [gs_current_frame], edx
     5F                  pop  edi
  85 C0 / jz else
```
This is the **native-call boundary**: it publishes the ELENA stack extent into the GC
frame chain so that a GC triggered inside (or by a callback from) the native procedure can
still find the roots. The callee's parameters are the values already on the evaluation
stack: `[ebp+4]` is the last pushed, `[ebp+8]` the one before, etc. — matching
`x86Assembler::readParameterList` (`asm2bin/x86assembler.cpp:78`), which maps parameter
`i` (0-based, declaration order) to `[ebp + (count − i)*4]`.

---

#### 18. `rcallemb ref, else` — `0x4F` (`bcEmbOperand`) — `x86jitcompiler.cpp:234`

```
→ embed(helper.getSection(argument1))     ; the ENTIRE native section, copied verbatim,
                                          ; with its own relocations re-resolved
  85 C0 / jz else
```
`embed` (`x86jitcompiler.cpp:209`) fetches the section with mask `mskNativeCodeRef`
(`ReferenceHelper::getSection`, `jitlinker.cpp:37`) and hands it to `copySection` with a
non-NULL module, so its symbolic references are resolved through
`helper.writeReference(writer, module, key, value)`. This is how `#inline standard'6(...)`
in ELENA source becomes straight-line machine code. Embedded snippets must clean the
evaluation stack themselves (`bccompiler.cpp:466-472` only records the meta-level
`bcFreeStack`).

---

#### 19. `pop` — `0x50` — `compilePop` (`x86jitcompiler.cpp:242`)

```
→ 5A                    pop edx
```
Unconditional, regardless of operand nibble.

---

#### 20. `iomove n` — `0x66` (`bcIOOperand`) — `x86jitcompiler.cpp:272`

```
→ 8B 14 24              mov edx, [esp]
  89 54 24 <n*4> | 89 94 24 <n*4>   mov [esp + n*4], edx      (via x86Helper::movMR32disp)
```

---

#### 21. `rmoveptr ref` — `0x69` (`bcRPtrOperand`) — `x86jitcompiler.cpp:250`

```
→ 8B 14 24              mov edx, [esp]
  89 15 <imm32>         mov [imm32], edx     ; relocation, disp 0
```

---

#### 22. `ifmove n` — `0x6A` (`bcIFOperand`) — `x86jitcompiler.cpp:258`

```
→ 8B 14 24              mov edx, [esp]
  n > 0 → mov [ebp - n*4], edx
  n < 0 → mov [ebp + prevFSPOffs + |n|*4], edx
```
(displacement chosen 8- or 32-bit by `x86Helper::getOperandType`, `x86helper.h:144`)

---

#### 23. `ismove n` — `0x6E` (`bcISOperand`) — `x86jitcompiler.cpp:293`

```
→ 8B 14 24              mov edx, [esp]
  89 97 <(n-1)<<2>      mov [edi + (n-1)*4], edx
```
**Always followed by the write barrier** when emitted from an assignment:
`compileAssignment` (`compiler.cpp:1184`) appends
`rcallemb $package'elena'34` (see §7.4).

---

#### 24. `omoveptr n, m` — `0x6F` (`bcOPtrOperand`) — `x86jitcompiler.cpp:280`

```
→ 8B 14 24              mov edx, [esp]
  8B 9C 24 <n*4>        mov ebx, [esp + n*4]
  89 93 <m>             mov [ebx + m], edx
```
Used to fill collection elements and inline-class captured outers
(`compiler.cpp:872`, `compiler.cpp:1004`). `m` is a **byte** offset (already shifted by
the caller), unlike `n` which the JIT shifts.

---

#### 25. `exitredir` — `0x74` (`bcRedirOperand`) — `x86jitcompiler.cpp:308`

```
→ 〈inline elena'18〉  (elena.asm:735)
     8B E5              mov  esp, ebp
     5D                 pop  ebp
     5F                 pop  edi
     33 C0              xor  eax, eax     ; ← result 0 == FAILURE
     5A                 pop  edx          ; discard the saved message id
     C3                 ret
```

---

#### 26. `sexit` — `0x78` (`bcSOperand`) — `x86jitcompiler.cpp:305`

```
→ 〈inline elena'8〉   (elena.asm:610)
     8B E5              mov  esp, ebp
     5D                 pop  ebp
     5F                 pop  edi
     8B C7              mov  eax, edi     ; non-zero == SUCCESS
     C3                 ret
```
The receiver slot on the caller's stack is left untouched, so the "result" of a method
that falls off its end is the receiver itself.

---

#### 27. `redirect` — `0x80` (`opcode == 0`) — `x86jitcompiler.cpp:319`

```
→ 〈inline elena'19〉  (elena.asm:747)   — full VMT search on S[top] using the
                                          saved message id at [ebp+8] and the saved
                                          parameter at [ebp-4]; on success performs the
                                          whole sreturn sequence inline and RETs.
```

---

#### 28. `rredirect ref` — `0x81` (`bcROperand`) — `x86jitcompiler.cpp:316`

```
→ 〈inline elena'20〉  (elena.asm:793)   — identical, except the search starts at
                                          __arg1vmt (relocation with disp elVMTOffset)
                                          instead of S[top]'s own VMT.
```
This is how `super`-dispatch and `#annex` delegation are implemented.

---

#### 29. `ioset n, vmt` — `0x93` (`bcREOperand`) — `x86jitcompiler.cpp:326`

```
argument2 |= mskVMTRef
→ 〈inline elena'26〉  (elena.asm:983)
     8B 1C 24           mov ebx, [esp]
     B9 <vmt+12>        mov ecx, __arg2vmt
     89 8B <n>          mov [ebx + n], ecx
```
Used only by `pushNewObject(…, constantRef)` (`bccompiler.cpp:330`) to stamp a class VMT
into a `$typeinstance` object (`compiler.cpp:1063`).

---

#### 30. `ifset n` — `0x9A` (`bcIFOperand`) — `x86jitcompiler.cpp:331`

```
→ 8D 65 <-(n<<2)> | 8D A5 <-(n<<2)>     lea esp, [ebp - n*4]
```
This is the **scope unwinder**. `n` is the evaluation-stack depth at the start of the
enclosing scope, computed at bytecode-save time by `saveProcedure`'s `stackLevels` stack
(`bccompiler.cpp:691-699`). After a branch fails midway through an expression, the
evaluation stack can be at an arbitrary depth; `ifset` snaps it back in one instruction.

---

#### 31. `rreturnif ref` — `0xB1` (`bcROperand`) — `x86jitcompiler.cpp:341`

```
→ 〈inline elena'10〉  (elena.asm:631)
     58                 pop  eax
     3D <ref+8>         cmp  eax, __arg1obj
     74 xx              jz   lbContinue
     89 44 24 04        mov  [esp+4], eax
     C3                 ret
  lbContinue:
     50                 push eax
```
Static-symbol memoisation: "if the cached value is still `nil`, fall through and compute;
otherwise return the cache immediately" (`newStaticSymbol`, `bccompiler.cpp:189`).

---

#### 32/33. `iocall(0|1) msg, else` — `0xC3` / `0xC7` — `x86jitcompiler.cpp:349,355`

```
argument2 (the VMT start offset) := 0 for iocall(0), 8 for iocall(1)
if !(msg & PREDEFINED_REF): argument1 := helper.resolveMessageID(msg)   ; :359
→ 〈inline elena'7〉   (elena.asm:580)
     5B                 pop  ebx                ; the message PARAMETER
     8B 04 24           mov  eax, [esp]         ; the RECEIVER
     BA <msgid>         mov  edx, __arg1
   labStart:
     8B 40 FC           mov  eax, [eax-4]       ; follow the VMT chain
     85 C0              test eax, eax
     74 ..              jz   labEnd             ; nothing left → failure
     8D B0 <n>          lea  esi, [eax + __arg2]
     8B 0E              mov  ecx, [esi]
     85 C9              test ecx, ecx
     74 ..              jz   labCall            ; messageID 0 == "any" handler
   labNext:
     3B CA              cmp  ecx, edx
     8D 76 08           lea  esi, [esi+8]
     7F ..              jg   labStart           ; overshot → walk to parent/any-handler
     8B 0E              mov  ecx, [esi]
     7C ..              jl   labNext
   labCall:
     8B 04 24           mov  eax, [esp]         ; self := receiver
     FF 56 FC           call [esi-4]            ; esi already advanced past the match
   labEnd:
  <breakpoint record>                           ; compileOthers(0x0C), x86jitcompiler.cpp:382
  85 C0 / jz else                               ; x86jitcompiler.cpp:386
```
Everything about dynamic dispatch is in these 20 instructions. See §3.4 for why the VMT
start offset can be 8, and §2.6 for the failure protocol.

---

#### 34/35. `ircall(0|1) msg, else, vmt` — `0xCB` / `0xCF` — `x86jitcompiler.cpp:365`

```
→ B8 <vmt+12>           mov eax, imm32          ; ← the EXTRA tape dword, read at :370
  argument2 := 0 or 8
→ 〈inline elena'30〉  (elena.asm:1018)
     5B                 pop  ebx
     BA <msgid>         mov  edx, __arg1
     EB ..              jmp  labStart2          ; skip the first [eax-4] indirection
   labStart:
     8B 40 FC           mov  eax, [eax-4]
     85 C0 / 74 ..
   labStart2:
     8D B0 <n>          lea  esi, [eax + __arg2]
     ... identical scan loop ...
  <breakpoint record>
  85 C0 / jz else
```
The only difference from `iocall` is the entry point: EAX already holds a VMT, so the
first dereference is skipped.

---

#### 36. `unshift` — `0xF0` (`opcode == 0`) — `x86jitcompiler.cpp:427`

```
→ 〈inline elena'29〉  (elena.asm:1009)
     8B 57 FC           mov edx, [edi-4]     ; current (role) VMT
     8B 52 FC           mov edx, [edx-4]     ; the role's owner class VMT
     89 57 FC           mov [edi-4], edx     ; write it back into the object header
```
**Mutates the object header in place.**

---

#### 37. `ijump off` — `0xF2` (`opcode == 2`) — `x86jitcompiler.cpp:431`

```
off > 0 → EB <rel8> or E9 <rel32>    jmp forward
off < 0 → EB/E9                      jmp backward (offset known, encoded directly)
```
Target label = `tapePosition + off` (`compileJump`, `x86jitcompiler.cpp:98`).

---

#### 38. `ocreate size, vmt` — `0xF3` (`opcode == 3`) — `compileCreate` (`x86jitcompiler.cpp:389`)

```cpp
int fieldCount = argument1;  int size = fieldCount << 2;
if (fieldCount < 0) {                       // binary object: argument1 carries gcBinary|rawSize
   size = fieldCount & ~gcBinary;
   fieldCount = gcBinary | ((size + 3) >> 2);
}
argument1 = fieldCount;  argument2 |= mskVMTRef;
```
```
→ B9 <align(size + 8, 16)>   mov ecx, imm32        ; total allocation, gcPageSize-aligned
  then one of, by field count (x86jitcompiler.cpp:408-421):
     0        → 〈inline elena'15〉  ocreate0   (elena.asm:697)
     1..2     → 〈inline elena'12〉  ocreate2   (elena.asm:658)
     3..4     → 〈inline elena'13〉  ocreate4   (elena.asm:669)
     5..6     → 〈inline elena'14〉  ocreate6   (elena.asm:682)
     >6       → 〈inline elena'11〉  ocreate    (elena.asm:642, loop-based nil fill)
```
Each variant is:
```
     BB <fieldCount>    mov  ebx, __arg1        ; the header size/flags word
     E8 <rel32>         call $package'elena'1   ; the allocator (§7.2)
     C7 40 FC <vmt+12>  mov  [eax-4], __arg2vmt
     ...nil fill...
     50                 push eax
```
So *object creation is a direct call to the bump allocator with a fully unrolled
initialiser* — no runtime type metadata is consulted.

---

#### 39. `ioswap n` — `0xFA` (`bcSwapOperand`) — `x86jitcompiler.cpp:438`

```
argument1 <<= 2
→ 〈inline elena'25〉  (elena.asm:972)
     8B 1C 24           mov ebx, [esp]
     8B 8C 24 <n*4>     mov ecx, [esp - __arg1]     ; ← see note
     89 1C 24           mov [esp], ebx
     89 8C 24 <n*4>     mov [esp - __arg1], ecx
```
**This is a no-op, and it looks like a bug.** The template reads two slots into EBX/ECX
and then writes each value *back where it came from*; a real swap would need
`mov [esp], ecx` / `mov [esp-n], ebx`. Separately, the `-__arg1` in the source has no
effect: `x86Assembler::readOffset` (`asm2bin/x86assembler.cpp:114-118`) negates only
`disp.offset`, and for a reference-typed operand the displacement is patched from
`scope.argument1` at JIT time, so the encoded addressing is actually `[esp + n*4]` (a
valid, deeper stack slot). `ioswap` is emitted only from `compileInlineClass`'s property
path (`compiler.cpp:888`).

---

#### 40. `dbgbreak` — `0xFC` (`opcode == 0x0C`) — `x86jitcompiler.cpp:443`

```
→ (no machine code)
  helper.addBreakpoint(code->Position())    ; appends the current code address to the
                                            ; .debug section (jitlinker.cpp:49)
```
Emitted into the tape only when `-d` (debug info) is on (`bccompiler.cpp:686`), so debug
and release bytecode streams differ.

---

#### 41. `shift n` — `0xFE` (`bcRoleOperand`) — `x86jitcompiler.cpp:447`

```
argument1 <<= 2
→ 〈inline elena'28〉  (elena.asm:993)
     8B 57 FC           mov  edx, [edi-4]           ; self's current VMT
     F7 42 F8 10        test [edx-8], elRoleVMT     ; already inside a role?
     74 ..              jz   labShift
     8B 52 FC           mov  edx, [edx-4]           ; yes → get the owner class VMT first
   labShift:
     8B 52 F4           mov  edx, [edx-0Ch]         ; the class's role table
     8B 8A <n*4>        mov  ecx, [edx + __arg1]    ; role n's VMT
     89 4F FC           mov  [edi-4], ecx           ; install it
```

### 2.6 The failure protocol — the single most important semantic

ELENA has no exceptions in 1.5.0. Every call-like operation carries a **branch-on-failure
target** encoded as the second tape dword, and every callee returns a
success/failure flag in EAX:

| Producer | EAX on success | EAX on failure |
|---|---|---|
| `sreturn` (`elena'9`) | the returned object (assumed non-zero) | — |
| `sexit` (`elena'8`) | the receiver (non-zero) | — |
| `exitredir` (`elena'18`) | — | always 0 |
| method-not-found (`iocall` falls out at `labEnd`) | — | EAX is whatever the chain walk left (0, because `test eax,eax; jz labEnd`) |
| explicit failure epilogue | — | `ipush 0; sreturn` (`bccompiler.cpp:565`) |
| an `#inline` asm snippet | non-zero | `xor eax,eax` (e.g. `standard'1`, `standard.asm:27`) |

The JIT unconditionally appends `test eax, eax; jz <else>` after `rcall`, `rcallext`,
`rcallemb`, `iocall(n)` and `ircall(n)` (`compileJumpIfNot`, `x86jitcompiler.cpp:81`).
`<else>` is either the enclosing branch's failure label (`bltBranch`) or the procedure's
failure epilogue (`bltProc`) — chosen at bytecode-emission time by `ByteCodeCompiler`
(`bccompiler.cpp:388`, `:426`, `:434`).

**Consequence: `0` is not a valid object reference anywhere in the system.** This is why
`nil` is a real allocated object (`$elena'$nil`) rather than a null pointer.

### 2.7 How branches, loops and labels are resolved

Label resolution happens **twice**, in two different address spaces:

**(a) Bytecode-space, in `elc`, at `saveProcedure` time** (`bccompiler.cpp:621-759`).
Three stacks of pending jumps (`jumpsToFail`, `jumpsToProcFail`, `jumpsToEnd`) plus a map
for loops. When `blFailure`/`blEnd` is reached, `fixJumps` (`bccompiler.cpp:43`) patches
each placeholder with `labelPosition − jump.position − (withExtraParam ? 8 : 4)`.
The `withExtraParam` case is exactly `ircall`, whose trailing VMT dword sits between the
offset field and the next instruction.

**(b) Machine-code-space, in the JIT, via `x86LabelHelper`** (`x86helper.h:253`,
`x86helper.cpp`). A `nop` in the tape calls `setLabel(tapeOffset)`; a jump calls
`writeJxxForward(tapeOffset + delta, …)` and records a placeholder. `fixLabel` patches
them once the label appears.

Short/near jump selection is a *guess* (`jumpOffset < 0x10`) that can be wrong. When it
is, `fixShortLabel` (`x86helper.cpp:190`) detects overflow and calls `convertShortToNear`
(`x86helper.cpp:302`), which **inserts bytes into the already-emitted code section** and
then re-fixes every other jump that spans the insertion point (`fixJumps`,
`x86helper.cpp:230`) and shifts every recorded label (`shiftLabels`, `x86helper.h:315`).
This is a genuinely fragile piece of machinery and it disappears entirely under LLVM.

---

## 3. Object model & memory layout

### 3.1 Object header

```
        ┌──────────────┬──────────────┬────────────┬────────────┬─────
offset  │    obj-8     │    obj-4     │   obj+0    │   obj+4    │ ...
        │  size/flags  │   VMT ptr    │  field 1   │  field 2   │
        └──────────────┴──────────────┴────────────┴────────────┴─────
                                      ▲
                                      └── the object *pointer* points here
```

`elEmptyObject = 0x08` (`elenaconst.h:253`) is the header size. Every reference written by
the JIT uses `disp = elEmptyObject` so it names the *body*, not the header
(`copySection` cases −3/−4, `x86jitcompiler.cpp:39-44`).

The size/flags word at `[obj-8]`:

| Bits | Meaning |
|---|---|
| 0..29 | number of **dword** slots in the object (field count, or `(byteSize+3)/4` for binary objects) |
| 30 (`gcCollected = 0x40000000`) | GC mark bit, set during the mark phase (`elena.asm:463`) |
| 31 (`gcBinary = 0x80000000`) | the object holds raw bytes, **not** references — the GC must not scan it |

The `gcBinary` bit being the sign bit is load-bearing: the collector tests
`test eax,eax; jle skip` (`elena.asm:464`) — a binary object's size word is negative, so
one instruction handles both "empty" and "don't scan".

### 3.2 VMT layout

`elVMTOffset = 0x0C` (`elenaconst.h:254`) — VMTs also have a **negative-offset header**:

```
        ┌───────────────┬────────────┬──────────────────┬──────────┬──────────┬─────
offset  │    vmt-12     │   vmt-8    │      vmt-4       │  vmt+0   │  vmt+8   │ ...
        │  role table   │   flags    │ parent / any-hdl │ entry[0] │ entry[1] │
        └───────────────┴────────────┴──────────────────┴──────────┴──────────┴─────
                                                        ▲
                                          objects' [obj-4] points here
```

Built by `_JITCompiler::compileVMT` (`jitcompiler.cpp:105`):

| Slot | Contents | Written at |
|---|---|---|
| `vmt-12` | pointer to the role table (`elVMTWithRoles`), else 0 | `jitcompiler.cpp:113-116` |
| `vmt-8` | `ClassHeader.flags` | `jitcompiler.cpp:118` |
| `vmt-4` | if `elVMTAnyHandler`: pointer to the any-handler entry pair; if `elRoleVMT`: pointer to the owner class's VMT; else 0 | `jitcompiler.cpp:121-151` |
| `vmt+8k` | `VMTEntry { int messageID; int address; }` | `jitcompiler.cpp:154-161` |

Entries are **sorted ascending by `messageID` as a signed int** (`addVMTEntry`,
`jitcompiler.cpp:90`), and terminated by an entry with
`messageID = TERMINAL_MESSAGE_ID = 0x7FFFFFFF` (`jitcompiler.cpp:164-166`) — the largest
possible signed value, so the linear scan always overshoots and exits.

### 3.3 Message IDs and the sort order that makes dispatch work

| Kind | Value range | Signed order |
|---|---|---|
| Predefined (`fail`, `new`, `+`, `?`, …) | `0x80000000` … `0x80000019` (`elenaconst.h:112-140`) | **most negative** → sorted first |
| Ordinary messages | 1, 2, 3, … assigned in link order by `Linker::resolveMessage` (`linker.cpp:100`) | positive, sorted after |
| VMT terminator | `0x7FFFFFFF` | last |

Ordinary message IDs are **global to the executable** and assigned lazily by the linker as
each message name is first encountered. Inside a `.nl` module a message is a
module-local index into the module's message table; `ReferenceLoader::resolveMessageID`
(`jitlinker.cpp:111`) translates module-local → global by resolving the *name*.

Message names are built by the compiler as `namespace'verb` for private messages, or the
bare verb otherwise (`compiler.cpp:526-555`). There is **no arity or signature encoding**
in a message id — every method takes exactly one parameter (in EBX) plus the receiver.

### 3.4 Why the VMT scan can start at entry 1 (`iocall(1)`)

`sendMessage` (`bccompiler.cpp:419`) emits `iocall(0)` **only** for `NEW_MESSAGE_ID`, and
`iocall(1)` for everything else. `iocall(1)` starts the scan at `entry[1]`, skipping
`entry[0]`. That is safe because:

* `$elena'object` — the root class every class inherits from — defines `new`
  (`src/elena.l:6`), and `NEW_MESSAGE_ID = 0x80000001` is the smallest message id
  actually used (`FAIL_MESSAGE_ID = 0x80000000` is never installed as a method; "fail" is
  what happens when *no* method matches);
* therefore `entry[0]` is always the `new` entry, and `new` is the one message that uses
  `iocall(0)`.

Role VMTs have `parentRef = 0` (`compiler.cpp:361`) and so do not inherit `new`. The
compiler therefore *manually reserves* entry 0 with a dummy method:

```cpp
// reserve the first entry for role vmt, due to iocall(1) command   <- compiler.cpp:1387
if (test(scope.info.header.flags, elRoleVMT))
   compileIdleMethod(scope);                                        //  compiler.cpp:1388-1389
```

`DUMMY_MESSAGE_ID = 0x80000002` (`elenaconst.h:115`) is chosen to sit immediately above
`new`, guaranteeing index 0.

> **Latent hazard.** If user code ever defines a method named `fail`
> (`FAIL_MESSAGE = "fail"` is in the predefined table, `compiler.cpp:492`), it takes
> entry 0 and becomes permanently unreachable via `iocall(1)`.

### 3.5 The "any" handler (`#extend` / inline classes)

A class flagged `elVMTAnyHandler` gets a method with `messageID == 0` that catches every
unmatched message. `compileVMT` (`jitcompiler.cpp:121-145`) arranges it as **two identical
entries placed after the terminator**:

```
   entry[0..k-2]   real methods (sorted)
   entry[k-1]      { TERMINAL_MESSAGE_ID, 0 }        the terminator
   entry[k]        { 0, anyHandlerAddress }          ← vmt-4 points here
   entry[k+1]      { 0, anyHandlerAddress }          duplicate
```

The duplication exists because `iocall(0)` and `iocall(1)` add 0 or 8 to whatever `vmt-4`
yields, and both must land on a `messageID == 0` cell so that the
`test ecx,ecx; jz labCall` fast path fires. The `call [esi-4]` then picks up the
*preceding* entry's `address` field — which is why the pair is needed and why
`compilePseudoVMT` (`jitcompiler.cpp:29`) lays out `$group`/`$cast` the same way.

> Reading the arithmetic literally, `iocall(0)` reaching an any-handler chain lands on
> `entry[k]` and calls `entry[k-1].address` — the terminator's zero address. In practice
> `new` is always found in the real entry list before the chain walk reaches the
> any-handler, so the path is not exercised; but it is not defensively coded.

### 3.6 `self`, the receiver, and roles

| Concept | Where it lives |
|---|---|
| **`self` / `$self`** (the class instance whose method is running) | register **EDI** |
| **the receiver of the message being sent** | the evaluation-stack slot *below* the parameter — `[esp]` at the moment `iocall` runs |
| **`self` as the source language sees it in a class body** (`otVSelf`) | `[ebp + prevFSPOffs + 4]`, i.e. the caller's receiver slot — `ifpush -1` |
| **the message parameter** | register **EBX**, pushed to `[ebp-4]` by `sprepparam` |
| **`super`** | not a value: compiled to `ircall`/`rredirect` against the parent VMT reference |

EDI is saved and restored by every method prologue/epilogue pair
(`sprep`/`sexit`/`sreturn`), making it a callee-saved "self register".

**Roles ("shift technology")** are implemented by *mutating the live object's VMT
pointer* (§2.5.41 and §2.5.36). A role VMT is a full VMT with `elRoleVMT` set and
`vmt-4` pointing back at the owner class's VMT, so `unshift` can restore the original and
so the ordinary dispatch chain walk (`mov eax,[eax-4]`) naturally falls through from the
role to the owner class. The class's role table is a flat array of VMT pointers in a
`mskNativeDataRef` section (`saveRoleTable`, `bccompiler.cpp:782`).

This has two immediate consequences that matter for modernization:

1. **VMT pointers are not constant for the lifetime of an object.** Any devirtualization
   or type-based alias analysis must account for `shift`/`unshift`.
2. **It is not thread-safe by construction** — two threads inside the same object would
   race on `[obj-4]`.

### 3.7 Field layout

| Class kind | Flag | `[obj-8]` size word | Body |
|---|---|---|---|
| Normal | — | field count | one 4-byte reference per field, 1-based (`ispush 1` → `[edi+0]`) |
| Structure (`#field(N)`) | `elStructureRole` | `gcBinary \| ((N+3)/4)` | N raw bytes; GC does not scan |
| Dynamic (`#field.`) | `elDynamicRole \| elStructureRole` | set at creation | variable-size raw data |
| Stateless | `elStateless` | 0 | no body; the class is represented by a single shared constant object |

A class is `elStateless` iff it has no fields and no roles (`compiler.cpp:1568-1576`), in
which case `ReferenceLoader::loadConstant` will materialise exactly one instance of it in
`.data` and every mention of the class becomes that pointer (`jitlinker.cpp:384-391`
refuses to make a constant out of a non-stateless class).

### 3.8 Alignment and size constants

| Constant | Value | File | Meaning |
|---|---|---|---|
| `elEmptyObject` | 8 | `elenaconst.h:253` | object header size |
| `elVMTOffset` | 12 | `elenaconst.h:254` | VMT header size |
| `elAnyHandlerSize` | 16 | `elenaconst.h:256` | space for the two duplicated any-handler entries |
| `VMT_INDEX_SIZE` | 4 | `elenaconst.h:110` | (declared, unused in 1.5) |
| `sizeof(VMTEntry)` | 8 | `elena.h:76` | `{int messageID; int address;}` |
| `sizeof(ClassHeader)` | 12 | `elena.h:84` | `{ref_t roleRef; size_t flags; ref_t parentRef;}` |
| `VA_ALIGNMENT` | 8 | `elena.h:225` | every JIT-emitted item is 8-byte aligned |
| `VA_ALIGNMENT_POWER` | 3 | `elena.h:226` | virtual addresses are byte offsets `>> 3` |
| `gcPageSize` | 16 | `elenaconst.h:276` | heap allocation granularity |
| `MAXIMAL_MESSAGE_REF` | `0xFFFF` | `elenaconst.h:105` | (declared, not enforced in the engine) |
| `PREDEFINED_REF` | `0x80000000` | `elenaconst.h:107` | tests "is this a predefined message id" |
| `TERMINAL_MESSAGE_ID` | `0x7FFFFFFF` | `elenaconst.h:108` | VMT terminator |

### 3.9 VMT flags (`elenaconst.h:258-273`)

| Flag | Value | Meaning |
|---|---|---|
| `elStandartVMT` | `0x00000001` | ordinary class (set by default, `compiler.cpp:268`) |
| `elInlineClass` | `0x00000002` | anonymous/inline class created from an expression |
| `elDynamicRole` | `0x00000004` | variable-length raw body |
| `elStructureRole` | `0x00000008` | raw (binary) body — GC does not trace fields |
| `elRoleVMT` | `0x00000010` | this VMT is a role; `vmt-4` names the owner class |
| `elVMTWithRoles` | `0x00000020` | this class has a role table at `vmt-12` |
| `elVMTAnyHandler` | `0x00000040` | this class has a catch-all method |
| `elStateless` | `0x00000080` | one shared immutable instance; eligible to be a constant |
| `elDebugMask` | `0x000F0000` | selector for the debugger's value-rendering hint |
| `elDebugDWORD` | `0x00010000` | render the body as a 32-bit integer |
| `elDebugReal64` | `0x00020000` | render as a double |
| `elDebugLiteral` | `0x00030000` | render as a length-prefixed UTF-16 string |
| `elDebugArray` | `0x00050000` | render as an array of references |
| `elDebugQWORD` | `0x00060000` | render as a 64-bit integer |

---

## 4. Module (`.nl`) file format

A `.nl` file is a serialised `_ELENA_::Module` (`engine/module.cpp:146`). All integers
are little-endian 32-bit. All strings are `TCHAR` — **UTF-16LE in the shipped Unicode
build** (`common/common.h:14`, `_UNICODE`), so "one character" is 2 bytes.

### 4.1 Top-level layout

| Offset | Size | Field |
|---|---|---|
| 0 | 5 | ASCII signature `"EN!10"` (`MODULE_SIGNATURE`, `elenaconst.h:149`) — note: **not** NUL-terminated, exactly `strlen` bytes |
| 5 | (len+1)×2 | module name, NUL-terminated UTF-16 |
| … | var | **reference table** — `MemoryHashTable<TCHAR*, ref_t, mapReferenceKey, 29>` |
| … | var | **message table** — same type |
| … | var | **constant table** — `MemoryHashTable<TCHAR*, ref_t, mapLiteralKey, 29>` |
| … | var | **section map** — `Map<ref_t, Section*>` |

Version checking (`module.cpp:124`): the first 5 bytes must equal `"EN!10"`; if only the
first 2 bytes (`"EN"`) match, the result is `lrWrongVersion`, otherwise
`lrWrongStructure`.

### 4.2 String→id hash table (`MemoryHashTable`, `common/lists.h:2361`)

```
 DWORD bufferLength
 DWORD count
 byte[bufferLength] buffer
```

The buffer is a self-contained arena:

```
 offset 0   : DWORD bucketHead[29]        (116 bytes; byte offset of the first item, 0 = empty)
 offset 116 : items, appended in insertion order
```

Each item is 12 bytes followed (immediately, in practice) by its key string:

| Offset in item | Size | Field |
|---|---|---|
| +0 | 4 | `next` — byte offset of the next item in this bucket, 0 = end |
| +4 | 4 | `key` — a **signed relative offset**: `keyStringPos − itemPos` (`lists.h:2298-2308`) |
| +8 | 4 | `item` — the `ref_t` value |
| +12 | (len+1)×2 | the key string (written by `MemoryDump::write(pos, const wchar_t*)`) |

Bucket selection: `mapReferenceKey` (`common/tools.h:240`) takes the first character after
the last `'` and maps `a..z` → 0..25, anything else → 26. `mapLiteralKey`
(`tools.h:252`) uses the first character. Buckets are kept sorted by key so that
`getIt` can stop early.

**Reference id encoding.** A reference id is a plain sequence number
(`Module::mapReference`, `module.cpp:60`) limited to 24 bits — `mapReference` throws if
`id > ~mskAnyRef` (`0x00FFFFFF`). The top byte is always a *mask* (§5.1), never part of
the id.

### 4.3 Section map (`Map<ref_t, Section*>`, serialised by `section.h:66`)

```
 DWORD count
 repeat count times:
    DWORD key                 = referenceId | mask
    DWORD length
    byte[length] data
    relocation map:           MemoryMap<ref_t, ref_t>
       DWORD bufferLength
       DWORD count
       DWORD tale             (byte offset of the last item; 0 if empty)
       byte[bufferLength] buffer
```

The `MemoryMap` buffer (`lists.h:1445`) is:

```
 offset 0 : DWORD headOffset   (always 4 once non-empty)
 offset 4 : items, 12 bytes each: { DWORD next; DWORD key; DWORD item; }
```

For a relocation map, `key` is the reference being pointed at (id + mask) and `item` is
the **byte offset within this section** of the 32-bit slot to patch.

### 4.4 Section kinds found in a `.nl` file

| Mask | Value | Section content |
|---|---|---|
| `mskSymbolRef` | `0x42000000` | bytecode of a symbol (one procedure) |
| `mskClassRef` | `0x41000000` | bytecode of **all** methods of a class, concatenated |
| `mskVMTRef` | `0x21000000` | the class's VMT *tape* (see §4.6) |
| `mskMetaDataRef` | `0x24000000` | serialised `ClassInfo` (see §4.7) |
| `mskNativeDataRef` | `0x28000000` | a role table: an array of DWORD references to role VMTs |

In a debug module (`.dnl`) the section keys are the two magic ids
`DEBUG_LINEINFO_ID = (size_t)-1` and `DEBUG_STRINGS_ID = (size_t)-2`
(`elenaconst.h:226-227`).

### 4.5 Procedure (bytecode) format

Written by `ByteCodeCompiler::saveProcedure` (`bccompiler.cpp:621`):

```
 DWORD size          ; byte length of the bytecode that follows (patched at :754)
 byte[size] bytecode ; §2.1 encoding
```

For a class code section, methods are concatenated; each VMT tape entry's `address` field
is the byte offset of that method's `size` dword.

### 4.6 VMT tape format

Written by `ByteCodeCompiler::saveClass`/`saveVMT` (`bccompiler.cpp:817`, `:796`):

```
 DWORD  size            ; byte length of everything after this field (patched at :814)
 struct ClassHeader     ; 12 bytes: roleRef, flags, parentRef
 DWORD  classSize       ; VMT byte size = elVMTOffset + 8 * methodCount
 repeat: VMTEntry { DWORD messageID; DWORD codeOffset; }
```

The JIT linker derives the entry count as `(size − sizeof(ClassHeader) − 4) >> 3`
(`jitlinker.cpp:313`).

### 4.7 Class metadata (`mskMetaDataRef`) — `ClassInfo::save` (`elena.h:105`)

```
 struct ClassHeader     ; 12 bytes
 DWORD classSize        ; VMT byte size
 DWORD size             ; object body size in bytes (structure classes) or 0
 MemoryMap methods      ; MemoryMap<ref_t, bool, KeyStored=false>
 MemoryMap fields       ; MemoryMap<const TCHAR*, int, KeyStored=true>
 MemoryMap roles        ; MemoryMap<const TCHAR*, int, KeyStored=true>
```

This section is **compile-time only**: it is read when a class is inherited
(`compiler.cpp:618`) and by `saveClass` to emit the VMT header, but the JIT linker never
touches it. It does not contribute to the executable.

> **Portability hazard.** `MemoryMap` items are written with
> `_buffer.write(position, &item, sizeof(item))` — raw `struct` dumps including compiler
> padding. `_MemoryMapItem<ref_t, bool, false>` is `{size_t; ref_t; bool;}` = 9 bytes of
> data occupying 12 bytes. The on-disk format is therefore ABI-dependent (32-bit,
> 4-byte-aligned, LE). Any reimplementation must hard-code 12 and skip the padding.

---

## 5. The JIT linker

`ReferenceLoader` (`jitlinker.h:57`, `jitlinker.cpp`) is 533 lines and is the heart of the
build. It is platform-independent; everything platform-specific goes through two
interfaces:

* `_LoaderHelper` (`jitlinker.h:32`) — implemented by `Linker` (`elc/win32/linker.h:20`);
  answers "where do I put this?", "what is this name?", "what section holds that?".
* `_JITCompiler` (`jitcompiler.h:35`) — implemented by `x86JITCompiler`; answers
  "turn this bytecode into machine code".

**This is a genuinely clean seam** and is the right place to plug an LLVM backend in.

### 5.1 Reference masks

A `ref_t` in ELENA is `id | mask` where the mask occupies the top byte
(`ReferenceType`, `elenaconst.h:169-200`):

| Mask | Value | Meaning |
|---|---|---|
| `mskAnyRef` | `0xFF000000` | mask-extraction mask (so ids are 24-bit) |
| `mskImageMask` | `0xF0000000` | which image section (top nibble) |
| `mskSectionMask` | `0x70000000` | section without the relative bit |
| `mskNativeMask` | `0x08000000` | "this came from a precompiled `.bin`, not from bytecode" |
| `mskRelativeRef` | `0x80000000` | the slot holds a **PC-relative** displacement |
| `mskExternalRef` | `0x10000000` | a DLL import thunk |
| `mskDataRef` | `0x20000000` | `.data` |
| `mskCodeRef` | `0x40000000` | `.text` |
| `mskStaticRef` | `0x60000000` | `.bss` |
| `mskNativeDataRef` | `0x28000000` | `.data`, from a `.bin` (role tables, the GC table) |
| `mskNativeCodeRef` | `0x48000000` | `.text`, from a `.bin` (asm procedures) |
| `mskNativeStaticRef` | `0x68000000` | `.bss`, from a `.bin` (the GC root array) |
| `mskSymbolRef` | `0x42000000` | symbol code |
| `mskVMTRef` | `0x21000000` | class VMT |
| `mskClassRef` | `0x41000000` | class method code |
| `mskMetaDataRef` | `0x24000000` | class metadata (compile-time only) |
| `mskStaticConstRef` | `0x01000000` | a static symbol's storage slot |
| `mskConstantRef` | `0x08000000` | a stateless class's shared instance |
| `mskLiteralRef` | `0x09000000` | a string literal object |
| `mskInt32Ref` | `0x0A000000` | an integer literal object |
| `mskRealRef` | `0x0C000000` | a double literal object |
| `mskLinkerConstant` | `0x0D000000` | patch this slot with a *linker setting* (only `lnGCSize`) |
| *(mask 0)* | `0x00000000` | patch this slot with a resolved **message id** |

### 5.2 The load dispatcher

`ReferenceLoader::load` (`jitlinker.cpp:439`):

```cpp
void* vaddress = _helper->resolveReference(reference, mask);       // already loaded?
if (vaddress == LOADER_NOTLOADED) {
   if (mask == mskNativeCodeRef || mask == mskNativeDataRef)  loadNativeSection(...)
   else if (mask == mskSymbolRef)                             loadBytecodeSection(...)
   else if (mask == mskVMTRef)                                loadBytecodeVMTSection(...)
   else if (mask == mskConstantRef || mskLiteralRef
            || mskInt32Ref || mskRealRef)                     loadConstant(...)
   else if (mask == mskStaticConstRef)                        loadStaticVariable(...)
}
if (!silentMode && vaddress == LOADER_NOTLOADED)
   throw JITUnresolvedException(reference);
```

`LOADER_NOTLOADED` is `(void*)-1` (`jitlinker.h:18`).

| Loader | Line | What it does |
|---|---|---|
| `loadNativeSection` | :179 | Copies a precompiled `.bin` section verbatim into `.text`/`.data` and re-resolves its relocations. Handles the three special mask values (`mskLinkerConstant`, mask 0 = message, everything else = a nested `load`). |
| `loadBytecodeSection` | :221 | Symbols: emits a debug header, runs `compileSymbol` (which is `compileMethod`), then `fixReferences`. |
| `loadBytecodeVMTSection` | :251 | Classes: the most involved path — see §5.3. |
| `loadConstant` | :334 | Builds a literal/int/real/stateless object directly in `.data`. |
| `loadStaticVariable` | :399 | Reserves one dword in `.bss`; its vaddress is `offset \| mskStaticRef` (**not** shifted by 3). |
| `loadNativeData` | :465 | Reserves N zero bytes in `.data` (used only for the GC table). |
| `loadPseudoVMT` | :479 | Builds a `$group`/`$cast` any-handler-only VMT pointing at a hand-written asm routine. |

### 5.3 Class loading, step by step (`jitlinker.cpp:251-332`)

1. Read the VMT tape header: `size`, `ClassHeader`, `classSize` (:269-278).
2. Compute the VMT byte size = `classSize + sizeof(VMTEntry)` (+ `elAnyHandlerSize` if the
   class has an any handler) and **reserve that many zero bytes in `.data`** (:284-287).
   The VMT's virtual address is fixed *before* any method is compiled, which is what makes
   recursive class references work.
3. `getVMTReference(parentRef)` → recursively `load()` the parent class and
   **copy its entire VMT entry array** (`copyParentVMT`, `jitcompiler.cpp:44`). Parent
   any-handler entries are copied too, with `messageID` rewritten to `TERMINAL_MESSAGE_ID`
   so they sort last (:66-68).
4. Load the role table (`mskNativeDataRef`) or, for a role VMT, the owner class VMT
   (:294-301).
5. Emit the class debug header (:304).
6. For each VMT tape entry: seek the code section to `entry.address`, JIT-compile the
   method into `.text`, and insert `{messageID, codeOffset}` into the entry array with
   `addVMTEntry` — an **insertion sort** that also overwrites an inherited entry with the
   same id (`jitcompiler.cpp:75`) (:316-323).
7. `compileVMT` writes the VMT header, marks every `address` field as a `mskCodeRef`
   relocation, and appends the terminator (:326).
8. `fixReferences` resolves everything the methods referred to but that was not yet loaded
   (:329).

Note **step 6 defines method overriding**: it is not a per-slot override table, it is an
insertion into a sorted array keyed by global message id.

### 5.4 Reference resolution and relocation

`ReferenceHelper::writeReference` (`jitlinker.cpp:72`) is called for every relocatable
slot the JIT emits:

```cpp
ref_t position = writer.Position();
writer.writeDWord(disp);                      // the addend (0, 8 or 12)
void* vaddress = resolveReference(retrieveReference(module, refID, mask), mask);
if (vaddress != LOADER_NOTLOADED)
   resolveReference(section, position, vaddress, mask, virtualMode);   // patch now
else _references->add(position, RefInfo(reference, module));           // patch later
```

`resolveReference` (`jitlinker.cpp:17`) in **virtual mode** does not compute an address at
all — it records `section->addReference(vaddress | maybeRelativeBit, position)`. The
actual arithmetic happens later in `Section::fixupReferences` (`section.cpp:40`),
driven by `Linker::fixImage` (`linker.cpp:377`):

| Relocation kind | Formula | Where |
|---|---|---|
| absolute, code | `slot += imageBase + codeBase + (vaddr << 3)` | `section.cpp:52`, `reallocateReference` |
| absolute, data | `slot += imageBase + dataBase + (vaddr << 3)` | same |
| absolute, static (`.bss`) | `slot += imageBase + bssBase + (vaddr & ~mskImageMask)` | `returnReference` (`elena.h:233`) — **not** shifted |
| PC-relative | `slot = (vaddr << 3) − slotPosition − 4` | `section.cpp:50` |
| import thunk | `slot += importBase + importFixTableEntry` | `Section::fixupReferences(RelocationFixMap&,…)`, `section.cpp:24` |

The `<< 3` is `reallocateReference` (`elena.h:228`). Because the mask lives in the top
nibble, `key << 3` shifts it out — the mask bits are discarded "for free" by the shift.
That is intentional but extremely non-obvious.

The **addend** (`disp`) written before the reference is what turns a section-base pointer
into a usable pointer:

| `disp` | Used for | Result |
|---|---|---|
| `0` | raw slot address, message id patches, `.bss` slots | the base itself |
| `elEmptyObject` (8) | any object reference | skips the object header |
| `elVMTOffset` (12) | any VMT reference | points at `entry[0]` |

### 5.5 How the hand-written `.asm` core is pulled in

`src/asm/*.asm` are compiled **ahead of time** by the `asm2bin` tool
(`elenasrc/asm2bin/`) into `Module` files (`elena.bin`, `standard.bin`, `win32.bin`,
`extended.bin`, `winsock.bin` — registered in `bin/elc.cfg` under `[primitives]`). Each
`procedure`/`inline` becomes a `mskNativeCodeRef` section named
`$package'<file>'<n>`; each `structure` becomes a `mskNativeDataRef` section
(`x86assembler.cpp:2701-2764`).

Three different mechanisms bring that code into the executable:

**(a) Inline templates — the JIT's instruction bodies.** The `x86JITCompiler` constructor
(`x86jitcompiler.cpp:466-490`) loads 22 sections into `_inlines[]` and splices them at
every use site:

| `_inlines[]` slot | Reference | `elenaconst.h` | asm | Used for |
|---|---|---|---|---|
| `cmdPrepare` | `$package'elena'4` | :44 | :553 | `prep` |
| `cmdSPrepare` | `$package'elena'5` | :45 | :561 | `sprep`, `sprepparam` |
| `cmdReturn` | `$package'elena'6` | :46 | :571 | `return` |
| `cmdIOCallN` | `$package'elena'7` | :47 | :580 | `iocall(n)` |
| `cmdSExit` | `$package'elena'8` | :48 | :610 | `sexit` |
| `cmdSReturn` | `$package'elena'9` | :49 | :621 | `sreturn` |
| `cmdRReturnIf` | `$package'elena'10` | :50 | :631 | `rreturnif` |
| `cmdOCreate` | `$package'elena'11` | :51 | :642 | `ocreate` (>6 fields) |
| `cmdOCreate2` | `$package'elena'12` | :52 | :658 | `ocreate` (1–2) |
| `cmdOCreate4` | `$package'elena'13` | :53 | :669 | `ocreate` (3–4) |
| `cmdOCreate6` | `$package'elena'14` | :54 | :682 | `ocreate` (5–6) |
| `cmdOCreate0` | `$package'elena'15` | :55 | :697 | `ocreate` (0) |
| `cmdCallExt` | `$package'elena'16` | :56 | :707 | `rcallext` |
| `cmdPrepRedir` | `$package'elena'17` | :57 | :724 | `prepredir` |
| `cmdExitRedir` | `$package'elena'18` | :58 | :735 | `exitredir` |
| `cmdRedirect` | `$package'elena'19` | :59 | :747 | `redirect` |
| `cmdRRedirect` | `$package'elena'20` | :60 | :793 | `rredirect` |
| `cmdIOSWAP` | `$package'elena'25` | :62 | :972 | `ioswap` |
| `cmdIOSet` | `$package'elena'26` | :63 | :983 | `ioset` |
| `cmdShift` | `$package'elena'28` | :64 | :993 | `shift` |
| `cmdUnShift` | `$package'elena'29` | :65 | :1009 | `unshift` |
| `cmdIRCallN` | `$package'elena'30` | :66 | :1018 | `ircall(n)` |

**(b) Preloaded core symbols — `preloadCoreCode`** (`jitlinker.cpp:494-533`), called once
before anything else (`linker.cpp:300`):

| Symbol | Action | Line |
|---|---|---|
| `$elena'@gctable` (`GC_TABLE`) | `loadNativeData(…, 0x30)` — reserve the 48-byte GC control block in `.data` | :510 |
| `$elena'@gcroot` (`GC_ROOT`) | `loadStaticVariable` — the first slot of the `.bss` static-root array | :514 |
| `$package'elena'1` (`ALLOC_FUNCTION`) | `load(mskNativeCodeRef)` — the allocator/collector trigger | :518 |
| `$elena'$nil` (`NIL_CLASS`) | `load(mskConstantRef)` — the `nil` singleton | :521 |
| `$package'elena'21` (`GROUP_FUNCTION`) + `$elena'$group` | `loadPseudoVMT` | :525-527 |
| `$package'elena'36` (`CAST_FUNCTION`) + `$elena'$cast` | `loadPseudoVMT` | :530-532 |

Each is registered with `_compiler->addPreloadedReference(...)` so that inline templates
referring to them (via `'gc_yg_heap`, `'nil`, `@"$package'elena'1"`, …) can be patched
without another lookup — that is the `_preloaded` cache
(`x86jitcompiler.h:112`, used at `x86jitcompiler.cpp:64-69`).

**(c) Ordinary reachability.** `$package'elena'3` (the console entry point) is the link
root; `$package'elena'2` (mark-and-sweep), `'31`–`'33` (assign/alloctemp/addygptr),
`'35`/`'37` (GUI entry / window proc) and every `win32'N`, `standard'N` procedure are
pulled in by normal `load()` recursion from `#external`/`#inline` references.

### 5.6 The `copySection` placeholder protocol

`copySection` (`x86jitcompiler.cpp:18`) copies an inline template and then walks its
relocation map. Negative keys are **argument placeholders**, assigned by the assembler
(`x86assembler.cpp:274-302`, `:1531-1537`):

| Key | Assembler token | Patched with |
|---|---|---|
| −1 | `__arg1` | `scope.argument1` as a raw dword |
| −2 | `__arg2` | `scope.argument2` as a raw dword |
| −3 | `__arg1obj` | `argument1` as an **object** reference (disp 8) |
| −4 | `__arg2obj` | `argument2` as an object reference (disp 8) |
| −5 | `__arg1vmt` | `argument1` as a **VMT** reference (disp 12) |
| −6 | `__arg2vmt` | `argument2` as a VMT reference (disp 12) |
| −7 | `__arg1fun` | `argument1` as a raw reference (disp 0) — used by `call __arg1fun` |
| −8 | `__arg2fun` | `argument2` as a raw reference (disp 0) |
| ≥0 | a real symbol | if embedding (`module != NULL`): resolved through that module; otherwise looked up in the `_preloaded` cache |

The `_preloaded` path is why an inline template can say `mov eax, ['gc_yg_heap]` and get a
correct absolute address with no per-use bookkeeping.

### 5.7 Entry point construction

There is no synthesized entry stub. The linker simply loads the project's `start` setting
as a native code reference and takes its address:

```cpp
_symbolEntryPoint = _loader.load(entry, mskNativeCodeRef, true);     // linker.cpp:303
_entryPoint = reallocateReference((ref_t)_symbolEntryPoint);         // linker.cpp:307
...
header.AddressOfEntryPoint = _codeBase + _entryPoint;                // linker.cpp:454
```

`entry` comes from `opEntry` (`-e` / the project template): `$package'elena'3` for console
(`bin/templates/console.cfg`), `$package'elena'35` for GUI. That asm procedure ends with
`call @'starter` (`elena.asm:541`) — `'starter` is the weak reference `STARTUP_CLASS`
(`elenaconst.h:31`), resolved through the forwards table to `sys'templates'simple` or
similar, and pulled in as a `mskSymbolRef`.

So the boot chain is:

```
PE AddressOfEntryPoint
  → $package'elena'3           (asm: zero the static roots, HeapAlloc the GC heap,
                                establish the root GC frame)
    → 'starter → 'entry        (ELENA symbols, JIT-compiled)
      → the program
  → kernel32.ExitProcess
```

---

## 6. x86 code generation

### 6.1 Strategy: table-dispatched inline-template splicing

The x86 backend is not a code generator in the usual sense. It is:

* a **16-entry function-pointer table** indexed by the opcode's high nibble
  (`x86jitcompiler.cpp:493-508`);
* per handler, an `if`/`else` chain on the low nibble;
* per case, either **~2–10 hand-written bytes** written with
  `writeByte`/`writeWord`/`writeDWord`, or a **`memcpy` of a precompiled asm template**
  with placeholder patching.

There is no IR, no register allocation, no instruction selection, no scheduling, no
peephole pass. Register assignment is *fixed by convention* (§6.2). The only
"optimisation" in the whole backend is:

* `ocreate` picking one of five unrolled initialisers by field count
  (`x86jitcompiler.cpp:408-421`);
* short-vs-near jump guessing (`x86jitcompiler.cpp:239`).

`x86Helper` (`x86helper.h:16`) provides a minimal ModRM encoder
(`writeModRM`, `x86helper.h:183`) used by exactly two emitters — `movMR32disp` and
`leaRM32disp` — because those need a displacement whose size is not known statically.
Everything else is a hard-coded byte sequence.

### 6.2 Register conventions

| Register | Role | Preserved across an ELENA call? |
|---|---|---|
| **EAX** | (1) the receiver, passed to the method; (2) the method **result and success flag** — 0 means failure; (3) the VMT cursor during dispatch; (4) the freshly allocated object out of the allocator | No — it *is* the return value |
| **EBX** | the **message parameter** (the single argument of a message send) | No |
| **ECX** | allocation byte count; scan counters in dispatch and GC | No |
| **EDX** | the **message id** during dispatch; scratch for all `*move` opcodes; the sink of `pop` | No |
| **ESI** | the VMT entry cursor during dispatch; the object cursor in the GC | No |
| **EDI** | **`self`** — the current class instance | **Yes** — saved by `sprep`/`prepredir`, restored by `sexit`/`sreturn`/`exitredir` |
| **EBP** | frame pointer of the current ELENA procedure | **Yes** |
| **ESP** | the ELENA **evaluation stack** *is* the machine stack | by construction |

`doc/tech/bytecode.txt:5-10` calls these `sp`, `fsp`, `self`; the mapping is
`sp = ESP`, `fsp = EBP`, `self = EDI`.

### 6.3 Calling convention for a message send

```
   caller:   push <receiver>          ; the object
             push <parameter>         ; exactly one, always
             iocall(1) msg, else
                 ↓
   iocall:   pop ebx                  ; parameter → EBX
             mov eax, [esp]           ; receiver → EAX
             ...VMT search...
             call [esi-4]
                 ↓
   callee:   sprep       →  push edi / mov edi,eax / push ebp / mov ebp,esp
             ...body...
             sreturn     →  pop eax / mov esp,ebp / pop ebp / pop edi
                            mov [esp+4], eax        ; result overwrites the receiver slot
                            ret
                 ↓
   caller:   test eax, eax / jz else  ; the failure check
             ; the evaluation stack is now one slot shallower and its top is the result
```

Net stack effect of a send: **−1** (`bccompiler.cpp:436`). The receiver slot is reused as
the result slot — this is why the receiver has to be *below* the parameter.

For a **symbol call** (`rcall`) the caller first pushes a `nil` placeholder
(`pushSymbol`, `bccompiler.cpp:387`); `return` (`elena'6`) overwrites it, giving a net
effect of **+1**.

### 6.4 Stack frames

| Prologue | Pushes | `prevFSPOffs` | Frame contents (from EBP) |
|---|---|---|---|
| `prep` (symbol) | `ebp` | 4 | `[ebp]`=saved ebp, `[ebp+4]`=ret, `[ebp+8]`=result slot |
| `sprep` (method) | `edi`, `ebp` | 8 | `[ebp]`=saved ebp, `[ebp+4]`=saved edi, `[ebp+8]`=ret, `[ebp+12]`=receiver |
| `sprepparam` | `edi`, `ebp`, then `ebx` | 8 | as `sprep`, plus `[ebp-4]` = the parameter (local #1) |
| `prepredir` | `edx`, `edi`, `ebp` | 12 | `[ebp+8]`=ret, `[ebp+12]`=saved edi, `[ebp+16]`=saved msg id… (the redirect templates read the msg id at `[ebp+8]`) |

Locals are at `[ebp - n*4]`, n ≥ 1 (`ifpush n` / `ifmove n`). `prevFSPOffs` is a JIT-scope
variable (`x86jitcompiler.h:28`) recomputed by every prologue; it is what makes
`ifpush -1` mean "the caller's top slot" independently of which prologue was used.

### 6.5 Native (`#external`) calls

`rcallext` wraps the call in `elena'16` (§2.5.17), which:

1. saves EDI (self);
2. links a GC frame record and records the current stack extent;
3. sets EBP so the callee's declared parameters resolve to `[ebp+4]`, `[ebp+8]`, …;
4. `call`s the native procedure with a plain `E8 rel32`;
5. restores the GC frame head and EDI.

The native procedure itself calls into Win32 with
`FF 15 <import thunk>` (`x86assembler.cpp:1497-1500`) — `call dword [imm32]`, patched
against the PE import table.

Note: **arguments are not popped by the wrapper.** The ELENA side pops `count − 1` of them
with `endStatement` (`compiler.cpp:961-965`), leaving one slot as the result slot.

### 6.6 The GC write barrier in generated code

Emitted only for **field assignment** (`compileAssignment`, `compiler.cpp:1181-1186`):

```
   ismove n                       ; the store itself
   rcallemb $package'elena'34     ; the barrier, inlined
   test eax,eax / jz proc-fail
```

`elena'34` (`elena.asm:1164`) does two things:

```
  mov  ecx, fs:[4]                ; TIB StackBase
  mov  edx, fs:[8]                ; TIB StackLimit
  cmp  eax, ecx / ja  labSkip
  cmp  eax, edx / jb  labSkip
  call $package'elena'32          ; the value lives on the MACHINE STACK → copy to heap
labSkip:
  cmp  edi, ['gc_mg_heap]
  ja   short labSkip2
  call $package'elena'33          ; the target object is older than yg → remember it
labSkip2:
```

Two observations for the migration:

* `fs:[4]` / `fs:[8]` is a **hard-coded Win32 TIB access**. On Linux/macOS this is the
  wrong segment and the wrong layout.
* `elena'34` differs from its out-of-line twin `elena'31` (`elena.asm:1050`) in that it
  **does not write the relocated copy back** (`elena'31` has `mov [esi], eax` at
  `elena.asm:1062`; `elena'34` has no equivalent, and ESI is never set up by the preceding
  `ismove`). Assigning a stack-resident object into a field therefore appears to leave the
  stack pointer in the field. Treat this as a bug to be reproduced-or-fixed deliberately,
  not silently.

### 6.7 Alignment

`x86JITCompiler::alignCode` (`x86jitcompiler.cpp:516`) always aligns to `VA_ALIGNMENT`
(8), padding code with `0x90` (`nop`) and data with `0x00`. It ignores its own
`alignment` parameter. Called from `calculateVAddress` (`jitlinker.cpp:124`) before every
item is emitted, which is what guarantees the `>> 3` virtual-address encoding is lossless.

---

## 7. Garbage collection interface

The engine knows the GC only through: (a) the 48-byte GC table, (b) three reference names,
(c) two constants in the object header. Everything else is in `src/asm/elena.asm`.

### 7.1 The GC control table (`$elena'@gctable`)

48 bytes in `.data`, reserved by `preloadCoreCode` (`jitlinker.cpp:510`), field offsets
fixed by `asm2bin` (`x86assembler.cpp:303-365`):

| Offset | `.asm` name | Meaning |
|---|---|---|
| `+0x00` | `'gs_current_frame` | head of the GC stack-frame chain |
| `+0x04` | `'gc_heap_start` | base of the object heap (after the forwarding table) |
| `+0x08` | `'gc_yg_heap` | young-generation bump pointer (= allocation point) |
| `+0x0C` | `'gc_mg_heap` | mid-generation boundary |
| `+0x10` | `'gc_og_heap` | old-generation boundary |
| `+0x14` | `'gc_static_size` | number of dwords in the `.bss` static-root array — **patched by the linker** at `linker.cpp:391` |
| `+0x18` | `'gc_heap_end` | end of the heap |
| `+0x1C` | `'gc_mgptr2` | base of the mid→young remembered set |
| `+0x20` | `'gc_mgptr2_end` | current top of that set (grows **downward**) |
| `+0x24` | `'gc_ogptr2` | base of the old→young remembered set |
| `+0x28` | `'gc_ogptr2_end` | current top of that set |
| `+0x2C` | `'gc_flag` | 0 = collect young, 1 = `GC_MG_COLLECT`, 2 = `GC_FULL_COLLECT` |

### 7.2 Heap layout and initialisation (`elena.asm:490-550`)

```
  N = 'gc_heapsize                      ← the linker constant lnGCSize (elc.cfg gcsize, default 4096)
  total = N*16 + N*4
  base  = HeapAlloc(GetProcessHeap(), 0x0D, total)

  ┌───────────────────────────┬──────────────────────────────────────────────┐
  │  forwarding table  N*4    │       object heap  N*16 bytes                │
  └───────────────────────────┴──────────────────────────────────────────────┘
  ▲                           ▲                                              ▲
  base                    heap_start                                     heap_end
                          = yg = mg = og                                (initially)
```

The forwarding table is one dword per `gcPageSize` (16-byte) heap page, used by the
compactor to record each page's displacement (`elena.asm:416-424`) and by the fixup pass
to translate old pointers to new ones (`elena.asm:290-301`). The remembered sets
(`mgptr2`, `ogptr2`) also live in that region, growing downward from `heap_start - 4`
(`elena.asm:523-527`).

`0x0D` = `HEAP_NO_SERIALIZE | HEAP_GENERATE_EXCEPTIONS | HEAP_ZERO_MEMORY`.
`HEAP_NO_SERIALIZE` is an explicit **single-threaded** declaration.

### 7.3 Allocation and the collection trigger — `$package'elena'1` (`elena.asm:11`)

```
  eax = [gc_yg_heap]
  esi = eax + ecx                       ; ecx = requested total size (already 16-aligned)
  if esi > [gc_heap_end]  → collect
  [eax] = ebx                           ; the header size/flags word
  [gc_yg_heap] = esi
  return eax + 8                        ; the object pointer
```

That is the entire fast path: **a 5-instruction bump allocator**, inlined at every
`ocreate`. On overflow it publishes the stack extent into the current GC frame
(`elena.asm:28-31`), then runs one of three collections chosen by `gc_flag`:

| `gc_flag` | Collection | Range collected | Promotion |
|---|---|---|---|
| 0 | young | `[gc_mg_heap, gc_heap_end)` | survivors become mid-gen |
| 1 (`GC_MG_COLLECT`) | mid | `[gc_og_heap, gc_heap_end)` | survivors become old-gen |
| 2 (`GC_FULL_COLLECT`) | full | `[gc_heap_start, gc_heap_end)` | — |

Escalation is automatic: after a collection, if the free space is below
`'gc_heap_minimal` (= `gcPageSize * 0x10` = 256 bytes, `x86assembler.cpp:69`), `gc_flag`
is bumped (`elena.asm:57-59`, `:116-118`). If a full collection still cannot satisfy the
request, `kernel32.RaiseException(0x190, …)` is raised
(`elena.asm:193-202`; `ELENA_ERR_OUTOF_MEMORY = 0x190`, `elenaconst.h:284`).

### 7.4 The algorithm: mark → compact → fixup (`$package'elena'2`, `elena.asm:337`)

1. **Mark** (`collect`, `elena.asm:448`) from three root sets:
   * the remembered sets `mgptr2` and `ogptr2` (old→young pointers);
   * the **stack frame chain** starting at `gs_current_frame`;
   * the `.bss` static-root array `'statroots`, `gc_static_size` dwords long.
   Marking sets `gcCollected` in `[obj-8]`; `test eax,eax; jle` skips binary/empty objects.
2. **Compact** (`elena.asm:399-444`): sliding compaction. For each surviving object the
   displacement is recorded in the forwarding table slot for its page.
3. **Fixup** (`fixupHeap`, `elena.asm:206`): rescan the same three root sets, translate
   every in-range pointer through the forwarding table, and clear `gcCollected`.

### 7.5 Root discovery — the stack frame chain

There is no stack map. A GC frame record is two dwords:

```
   [frame + 0] = byte distance from `frame` down to the current ESP
   [frame + 4] = previous frame record
```

`gs_current_frame` points to the innermost record. The collector walks the chain and
treats **every dword in `[frame − size, frame)` as a potential root**, filtered by
`cmp esi, ebx` / `cmp esi, edx` against the collected heap range
(`elena.asm:454-458`). So it is **precise about frame extents but conservative about slot
contents** — an integer that happens to look like a heap pointer will be traced and, worse,
*updated by the compactor* (`elena.asm:300-301`). In practice this is safe only because
almost everything on the ELENA stack really is a reference (raw integers live inside
boxed `IntNumber` objects).

Frame records are pushed at exactly three places:
* the process entry point (`elena.asm:535-537`, `:1230-1233`) — the chain root, `{0, 0}`;
* `elena'16` (`callext`) — around every `#external` call (`elena.asm:709-713`);
* `elena'37` (`STD_WINDPROC`) — around every window-message callback
  (`elena.asm:1342-1345`).

### 7.6 Safe points

A GC can only happen **inside `$package'elena'1`**, i.e. at an `ocreate` or a
`$package'elena'32` (alloctemp) call. There are no other safe points: no poll at back
edges, no poll at calls, no interruption. This is a direct consequence of the
frame-chain root discovery — the stack extent is only published when the allocator
records it.

### 7.7 Why it is single-threaded-only

| Reason | Evidence |
|---|---|
| The heap is a **process-global bump pointer** with no locking | `elena.asm:15-21` |
| The heap block itself is `HEAP_NO_SERIALIZE` | `elena.asm:510` |
| `gs_current_frame` is a **single global**, not thread-local | GC table `+0x00` |
| The remembered sets are global and appended non-atomically | `elena.asm:1124-1155` |
| Roles mutate `[obj-4]` of a live shared object | `elena.asm:1003` |
| Static roots are one global array with a global count | `'statroots`, `gc_static_size` |
| The `gc_flag` escalation state machine is global | GC table `+0x2C` |
| No stop-the-world mechanism exists at all | there is no suspend/resume anywhere |

Adding threads requires, at minimum: per-thread allocation buffers, a per-thread frame
chain (TLS-resident), a stop-the-world protocol (hence real safe points), an atomic or
per-thread remembered set, and rethinking `shift`/`unshift`.

---

## 8. Debug info format

Debug information is split across **two files** because it is produced by two different
stages.

### 8.1 `.dnl` — the debug module (produced by `elc`'s compile phase)

A normal ELENA `Module` (§4.1) saved with extension `dnl`
(`Project::saveDebugModule`, `elc/project.cpp:121`), containing exactly two sections:

| Section key | Content |
|---|---|
| `DEBUG_LINEINFO_ID` = `(size_t)-1` | an array of 24-byte `DebugLineInfo` records |
| `DEBUG_STRINGS_ID` = `(size_t)-2` | concatenated NUL-terminated UTF-16 strings |

Its `_references` table maps **class name → byte offset** of that class's `dsClass` record,
and **`"#" + symbol name` → offset** of the `dsSymbol` record
(`mapPredefinedReference`, `bccompiler.cpp:66`, `:89`). The line-info section begins with a
zero dword placeholder so that offset 0 can never be a valid bookmark
(`bccompiler.cpp:60-63`).

### 8.2 `DebugLineInfo` (`elena.h:133`) — 24 bytes

| Offset | Size | Field |
|---|---|---|
| +0 | 4 | `symbol` — a `DebugSymbol` enum value |
| +4 | 4 | `col` (byte offset from line start, tab = 1 char) |
| +8 | 4 | `row` (0-based; `bccompiler.cpp:148` stores `argument − 1`) |
| +12 | 4 | `length` |
| +16 | 8 | union: `{nameRef, flags}` \| `{address}` \| `{nameRef, level}` |

`DebugSymbol` values (`elenaconst.h:203-223`):

| Symbol | Value | Meaning | Union field |
|---|---|---|---|
| `dsNone` | `0x00` | — | — |
| `dsStep` | `0x10` | an ordinary statement step | `step.address` |
| `dsEOP` | `0x11` | end of procedure | `step.address` |
| `dsVirtualStep` | `0x12` | synthesised step | `step.address` |
| `dsVirtualEnd` | `0x13` | virtual end of statement; the debugger skips it | `step.address` |
| `dsProcedureStep` | `0x14` | a step whose result must be checked | `step.address` |
| `dsAtomicStep` | `0x18` | "step into" behaves as "step over" (external/inline code) | `step.address` |
| `dsSymbol` | `0x20` | opens a symbol | `symbol.nameRef` |
| `dsClass` | `0x30` | opens a class | `symbol.nameRef`, `symbol.flags` (the VMT flags) |
| `dsBase` | `0x40` | the `self` local | `local.level` |
| `dsField` | `0x50` | a field declaration | `symbol.nameRef` |
| `dsLocal` | `0x60` | a local variable | `local.nameRef`, `local.level` |
| `dsProcedure` | `0x70` | opens a method | — |
| `dsEnd` | `0x80` | closes a class/symbol/procedure | — |
| `dsDebugMask` | `0xF0` | `(symbol & dsDebugMask) == dsStep` tests "is this a step" | — |

Record order for a class (`bccompiler.cpp:817-856`):

```
   dsClass                 (name, flags)
   dsField × fieldCount
   for each method:
       dsProcedure
       (dsLocal | dsBase | dsStep…)*
       dsEnd
   dsEnd
```

`nameRef` is a **byte offset into the strings section** while on disk; the IDE rewrites it
in memory to a real pointer when loading (`debugcontroller.cpp:299-304`).

### 8.3 `.dn` — the address sidecar (produced by the linker)

Written by `Linker::createDebugFile` (`elc/win32/linker.cpp:589`):

```
  "EN.D10!"                     7 ASCII bytes (DEBUG_MODULE_SIGNATURE, elenaconst.h:150)
  DWORD entryPointVA
  repeat, in JIT-load order:
     <reference name>           NUL-terminated UTF-16; symbols are prefixed with '#'
     [DWORD vmtAddress]         classes only
     DWORD stepAddress          one per breakpoint, in emission order
```

The name and (for classes) the VMT address are appended by
`createNativeSymbolDebugInfo` / `createNativeClassDebugInfo`
(`jitlinker.cpp:413`, `:423`); each step address is appended by
`ReferenceHelper::addBreakpoint` (`jitlinker.cpp:49`) at the moment the JIT reaches a
`bcDebug` opcode. All of these are written as **relocations**, so `fixImage`
(`linker.cpp:408-411`) turns them into final virtual addresses.

### 8.4 How the IDE joins the two

`DebugController::loadSymbolDebugInfo` (`ide/debugcontroller.cpp:257`):

1. read a name from the `.dn` stream;
2. look up that name in the corresponding `.dnl` module's reference table → a byte offset
   into the line-info section;
3. if it is a class, consume one dword from `.dn` and register
   `vmtAddress → lineInfoPosition` in `_classes` (that is how "given an object, show its
   class and fields" works — `seekClassInfo`, `debugcontroller.cpp:140`);
4. walk the line-info records until the matching `dsEnd`, and for every step-family record
   consume the next dword from `.dn` and patch it into `addresses.step.address`, then
   register `stepAddress → record` with the debugger.

The debugger then sets an INT3 at each step address and, on hit, looks the address up to
find the source row/column. `dsVirtualEnd` records get an address but are deliberately not
registered as stoppable (`debugcontroller.cpp:309-310`).

Note the important asymmetry: **enabling debug info changes the bytecode**
(`bcDebug` opcodes are only emitted when a debug module exists, `bccompiler.cpp:686`), so
a debug build and a release build are not the same program.

---

## 9. Core constants reference

Every constant in `elenaconst.h`, with its meaning.

### 9.1 Versions and limits

| Constant | Value | Line | Meaning |
|---|---|---|---|
| `ENGINE_MAJOR_VERSION` | `0x0005` | :15 | engine version (unused at runtime) |
| `ENGINE_MINOR_VERSION` | `0x0000` | :16 | |
| `LINE_LEN` | `0x1000` | :18 | max source line length |
| `IDENTIFIER_LEN` | `0x0100` | :19 | max identifier length; also the bookmark buffer size |
| `ELENA_SIGNITURE` | `"ELENA.150"` | :148 | stamped at the start of every `.data` section |
| `MODULE_SIGNATURE` | `"EN!10"` | :149 | `.nl` magic |
| `DEBUG_MODULE_SIGNATURE` | `"EN.D10!"` | :150 | `.dn` magic |

### 9.2 Well-known module and class names

| Constant | Value | Line | Role |
|---|---|---|---|
| `STANDARD_MODULE` | `$elena` | :22 | the standard library module name |
| `DLL_NAMESPACE` | `$dlls` | :23 | prefix for imported DLL functions |
| `PACKAGE_MODULE` | `$package` | :24 | prefix for everything in a `.bin` primitive |
| `CORE_BINARY_MODULE` | `elena` | :25 | the primitive holding the runtime core (`elena.bin`) |
| `GC_TABLE` | `$elena'@gctable` | :27 | the 48-byte GC control block |
| `GC_ROOT` | `$elena'@gcroot` | :28 | the `.bss` static-root array |
| `SUPER_CLASS` | `$elena'object` | :30 | root of the class hierarchy |
| `STARTUP_CLASS` | `'starter` | :31 | weak ref called by the asm entry point |
| `NIL_CLASS` | `$elena'$nil` | :32 | the `nil` singleton |
| `GROUP_CLASS` | `$elena'$group` | :33 | pseudo-VMT for message broadcasting |
| `CAST_CLASS` | `$elena'$cast` | :34 | pseudo-VMT for dynamic casts |
| `TYPEINSTANCE_CLASS` | `$elena'$typeinstance` | :35 | boxed class reference |
| `TYPE_CLASS` | `$elena'type` | :36 | the `type` property |
| `GUI_CLASS` | `win32'system'gui` | :37 | the GUI singleton (referenced by `'system` in asm) |

### 9.3 Core asm function references (`elenaconst.h:42-69`)

See the table in §5.5(a) for the inline templates. The remainder:

| Constant | Reference | asm | Role |
|---|---|---|---|
| `ALLOC_FUNCTION` | `$package'elena'1` | :11 | allocator + GC trigger |
| — | `$package'elena'2` | :337 | mark & compact |
| — | `$package'elena'3` | :490 | console process entry point |
| `GROUP_FUNCTION` | `$package'elena'21` | :839 | broadcast a message to every group member |
| — | `$package'elena'22` | :913 | `ifSame` (identity test) |
| — | `$package'elena'23` | :926 | class-redirect (`$getClassName`, `$typecast`) |
| — | `$package'elena'24` | :957 | `$ifSameType` |
| `IRCALL_FUNCTION` | `$package'elena'30` | :1018 | `ircall(n)` |
| — | `$package'elena'31` | :1050 | out-of-line assignment barrier |
| — | `$package'elena'32` | :1074 | copy a stack object into the heap |
| — | `$package'elena'33` | :1104 | add to the remembered set |
| `ASSIGN_FUNCTION` | `$package'elena'34` | :1164 | inline assignment barrier |
| — | `$package'elena'35` | :1185 | GUI process entry point |
| `CAST_FUNCTION` | `$package'elena'36` | :1276 | dynamic cast over a group |
| `WIND32PROC` | `$package'elena'37` | :1335 | the Win32 window procedure trampoline |
| — | `$package'elena'38` | :1453 | Win32 message → ELENA message id table |

`$package'elena'27` does not exist (a gap in the numbering).

### 9.4 Predefined messages (`elenaconst.h:71-140`)

| Name | Constant | ID |
|---|---|---|
| `fail` | `FAIL_MESSAGE_ID` | `0x80000000` |
| `new` | `NEW_MESSAGE_ID` | `0x80000001` |
| *(placeholder)* | `DUMMY_MESSAGE_ID` | `0x80000002` |
| `proceed` / `=>` | `PROCEED_MESSAGE_ID` / `OPROCEED_MESSAGE_ID` | `0x80000003` / `0x8000000E` |
| `<<` | `COPY_MESSAGE_ID` | `0x80000004` |
| `>>` | `COPYTO_MESSAGE_ID` | `0x80000005` |
| `ifnotnil` | `NOTNIL_MESSAGE_ID` | `0x80000006` |
| `of` | `OF_MESSAGE_ID` | `0x80000007` |
| `?` | `IF_MESSAGE_ID` | `0x80000008` |
| `!` | `IFNOT_MESSAGE_ID` | `0x80000009` |
| `+` `-` `*` `/` | `ADD`/`SUB`/`MUL`/`DIV_MESSAGE_ID` | `0x8000000A`–`0x8000000D` |
| `>=` `<=` `>` `<` `==` `!=` | … | `0x8000000F`–`0x80000014` |
| `+=` `-=` | `ADD2`/`SUB2_MESSAGE_ID` | `0x80000015` / `0x80000016` |
| `back` | `BACK_MESSAGE_ID` | `0x80000017` |
| `run` | `RUN_MESSAGE_ID` | `0x80000018` |
| `ifsame` | `SAME_MESSAGE_ID` | `0x80000019` |
| `#any` | `ANY_MESSAGE` | encoded as message id **0** in a VMT |
| `$invoke` | `REDIRECT_MESSAGE` | the explicit redirect message |

Because these are negative as signed ints, they always sort before ordinary messages in a
VMT — which is exactly what §3.4 depends on.

### 9.5 Compiler hints, prefixes and project types

| Constant | Value | Line | Meaning |
|---|---|---|---|
| `INLINE_POSTFIX` | `#inline` | :153 | name suffix for anonymous inline classes |
| `ROLE_POSTFIX` | `#role` | :154 | name suffix for role VMTs |
| `ROLETABLE_POSTFIX` | `#roles` | :155 | name suffix for a class's role table |
| `HINT_CONSTANT` | `const` | :158 | mark a symbol as static/memoised |
| `HINT_DEBUG` | `dbg` | :159 | debugger rendering hint |
| `HINT_DEFAULT` | `def` | :160 | `$elena` method hint |
| `HINT_DEBUG_INT`/`LITERAL`/`ARRAY`/`REAL`/`LONG` | … | :162-166 | select `elDebugDWORD`/`Literal`/`Array`/`Real64`/`QWORD` |
| `SELF_VAR` / `THIS_VAR` / `SUPER_VAR` | `self` / `$self` / `super` | :143-145 | |
| `ptLibrary` / `ptConsole` / `ptGUI` | 0 / 1 / 2 | :241-243 | project type (drives PE subsystem and entry point) |
| `lnGCSize` | 1 | :281 | the only linker constant patchable into asm (`'gc_heapsize`) |
| `ELENA_ERR_OUTOF_MEMORY` | `0x190` | :284 | the SEH code raised on heap exhaustion |
| `lrSuccessful` … `lrDuplicate` | 0..4 | :230-237 | module load results |
| `cnHashSize` | `0x0100` | :247 | parse-table hash size (parser, not engine) |
| `cnTablePower` / `cnTableKeyPower` / `cnSyntaxPower` | `0x10`/`0x11`/`0x08` | :248-250 | parser table key scaling |

---

## 10. Where the reference docs disagree with the code

`doc/tech/bytecode.txt` is a **design sketch that predates the 1.5 implementation**. It is
useful for intent but must not be trusted for detail.

| `bytecode.txt` says | The code says |
|---|---|
| Title: "ELENA **Virtual Machine** Byte code commands" (:2) | There is no VM. Bytecode never exists at run time. |
| Lists `ocast` (:41) and `oswitch` (:44) | No such opcodes exist in `bytecode.h`. The functionality became `$group`/`$cast` pseudo-VMTs (`jitlinker.cpp:525-532`). |
| `iocall(n) msg, else` — "scan … starting with n position" (:48) | Correct in spirit, but `n` is a **byte offset** (0 or 8), not an index, and it is chosen solely by `msg == NEW_MESSAGE_ID` (`bccompiler.cpp:424-430`). |
| `ircall(n) msg, else, ref` (:59) | Correct, but the doc omits that `ref` is a *trailing* dword after the branch offset, and that this changes the branch-offset arithmetic (`+8` instead of `+4`, `bccompiler.cpp:49`). |
| `oprepparam` / `oprep` (bccompiler comment, `bccompiler.cpp:231`) | The opcodes are `sprepparam` / `sprep`. |
| `iomoveptr offs1, offs2` (:133) | The opcode is `omoveptr` (`bcOMovePtr`); `offs2` is a **byte** offset, `offs1` a slot index. |
| `ioswap n` — "`[sp] <-> [sp-n]`" (:158) | The template is a no-op and the displacement is `+n`, not `−n` (§2.5.39). |
| `iomove offset` documented with no body (:121) | It is `S[top−n] := S[top]` (`x86jitcompiler.cpp:272`). |
| Does not mention | `bcIOSet`, `bcIFSet`, `bcShift`, `bcUnShift`, `bcDebug`, `bcRCallEmb`, `bcRCallExt`, `bcIJump`, `bcIOPush`, `bcISPush`, `bcRPushPtr`, `bcRMovePtr` — i.e. 12 of the 40 live opcodes. |
| Example 1 (:160-189) uses `iocall message, branch-failing` | Real tapes always carry the `(n)` suffix and an explicit `ifset` before each alternative. |

`doc/tech/jitlinker.txt` is three lines and is **accurate but nearly empty**: "in general a
reference points only to a unique section / message / external function, with the
exception of a symbol (which can point to code, vmt and static sections)". That is exactly
what `Linker::resolveReference` (`linker.cpp:120`) implements — note the three separate
maps `_nativeReferences`, `_symbolReferences`, `_constReferences` and the
`retrieveMappedReference` mask-disambiguation loop (`linker.cpp:64`).

---

## 11. Modernization notes — mapping to LLVM

### 11.1 What maps cleanly

| Bytecode | LLVM IR |
|---|---|
| `prep`, `sprep`, `sprepparam`, `prepredir` | function prologue — becomes implicit; `self` becomes an explicit first parameter, the message parameter the second |
| `ifpush n`, `ifmove n` (n > 0) | `alloca` + `load`/`store`, or SSA values outright after mem2reg |
| `ispush n`, `ismove n` | `getelementptr` + `load`/`store` on the object struct |
| `omoveptr n, m` | `getelementptr` + `store` |
| `rpush`, `ipush` | constants / global references |
| `pop`, `iopush`, `iomove`, `ifset` | disappear — they are an artefact of the stack machine; SSA replaces them entirely |
| `ijump` | `br` |
| `rcall ref, else` | `call` + `icmp ne`/`br` |
| `ocreate size, vmt` | `call @elena_alloc(i32 size)` + `store` of the VMT + `memset`/stores |
| `return`, `sreturn`, `sexit` | `ret` |
| `rcallext` | `call` with the platform C calling convention |

Roughly **25 of the 40 live opcodes are pure stack plumbing or straightforward memory
access** and vanish or become one IR instruction.

### 11.2 What does not map cleanly

**(a) Dynamic dispatch — `iocall(n)` / `ircall(n)`.** The dispatch is a *linear scan of a
sorted array along a parent chain*, with a fallback to an "any" handler that lives after
the terminator, plus an optional starting offset of 8 to skip entry 0
(`elena.asm:580-607`). None of that is expressible as an LLVM construct. The realistic
options:

1. Keep it as a **runtime helper**: `i8* @elena_lookup(i8* obj, i32 msgid)` returning a
   function pointer, then an indirect `call`. Simple, portable, slow (that is roughly what
   the current code does anyway, just inlined).
2. **Inline caching**: emit a monomorphic guard (`cmp [obj-4], cachedVMT`) plus a direct
   call, falling back to the helper. Needs a patchable code path or a per-site global.
3. **Perfect-hash or sorted-binary-search VMTs** built by the new linker — the same VMT
   layout but O(log n) or O(1) lookup, still via a helper.
4. `llvm.type.checked.load` / whole-program devirtualization only works for closed-world
   vtables with fixed slot indices, which ELENA deliberately does not have.

Note that option 2 is unsound as long as `shift`/`unshift` can mutate `[obj-4]` at any
time (§3.6) — an inline cache would have to key on the *current* VMT, not the static type.

**(b) The failure/branch-on-fail protocol.** Every call site is `call; test eax,eax; jz L`.
In LLVM this is easy to *express* (`%ok = icmp ne i8* %r, null; br i1 %ok, ...`), but it
has three properties that hurt:

* it makes every method a two-valued return (`{result, success}` fused into one pointer),
  so **the null pointer is reserved** and `nil` must stay a real object;
* it turns every expression into a CFG with a failure edge, so basic blocks are tiny and
  the IR is branch-dense — LLVM will handle it, but naive lowering will produce poor code;
* `sexit` returns the *receiver* as its success value, so "success" and "result" are the
  same register. Any refactor to `{i8*, i1}` or an `sret` out-parameter changes the ABI of
  every hand-written asm routine simultaneously.

A cleaner target model: return `{ptr result, i1 ok}` (LLVM handles small aggregate returns
natively in registers) and let the null-pointer overload go. That decouples "returned nil"
from "failed".

**(c) `iocall`'s "returns zero = fail" applied to `#inline` snippets.** `rcallemb`
(`x86jitcompiler.cpp:234`) splices an *arbitrary hand-written asm blob* into the middle of
a function and then tests EAX. The blob may pop the stack, may clobber anything, and its
only contract is "EAX non-zero = success". There is no LLVM equivalent for
"paste these bytes here"; each of the ~100 `standard'N`/`win32'N` snippets must become
either an LLVM intrinsic, a real function with a defined signature, or IR built by the
compiler. **This is the single largest mechanical task in the migration** — see
`src/asm/standard.asm` (2804 lines) and `src/asm/win32.asm` (1423 lines).

**(d) `redirect` / `rredirect`.** These do a VMT search *and*, on success, execute the
callee's epilogue inline and `ret` from the enclosing method (`elena.asm:779-786`). That
is a tail-call-with-a-conditional-return-from-two-frames. In LLVM: a helper returning
`{result, handled}` plus an explicit `br` to the caller's return block. `musttail` does
not apply because the return value must still be inspected.

**(e) `shift` / `unshift`.** Mutating `[obj-4]` invalidates every assumption LLVM would
otherwise make about a type field. It must be modelled as an opaque store through a
pointer LLVM cannot reason about (or the VMT field must be marked as escaping/volatile),
which will suppress useful optimisations. Consider redesigning roles as an explicit
delegation field rather than header mutation.

**(f) `ifset n`.** Unwinding the evaluation stack with `lea esp, [ebp-n*4]` has no meaning
once the evaluation stack is gone; it simply disappears. But it currently doubles as the
*only* mechanism keeping the stack balanced across failure edges — the new front end must
reconstruct the same scoping information (`saveProcedure`'s `stackLevels`,
`bccompiler.cpp:646-699`) to know which SSA values are live on a failure edge.

### 11.3 What is hard-coded to 32-bit / x86 / Win32

| Assumption | Where | Consequence |
|---|---|---|
| Pointers and all header fields are 32-bit | everywhere; `ref_t` is `size_t` but every layout constant assumes 4 | 64-bit needs `elEmptyObject = 16`, `elVMTOffset = 24`, `VMTEntry` = 16 bytes, and a new virtual-address encoding |
| `VA_ALIGNMENT_POWER = 3` packs a 4-bit mask into a 32-bit vaddress | `elena.h:225-231`, `jitlinker.cpp:130` | fundamentally a 32-bit scheme; 64-bit needs a separate `{section, offset}` representation |
| Object body is a dword array; header size word counts dwords | `elena.asm:410`, `x86jitcompiler.cpp:392` | field offsets, GC scanning, `align(size,16)` all assume 4-byte slots |
| `gcPageSize = 16` and the forwarding table is 1 dword per page | `elenaconst.h:276`, `elena.asm:503-518` | fine, but the ratio changes on 64-bit |
| `fs:[4]` / `fs:[8]` = Win32 TIB stack bounds | `elena.asm:1052-1053`, `:1109`, `:1166-1167` | must become a portable "is this address on the stack" test (or the whole stack-object mechanism must be removed) |
| `HeapAlloc`/`GetProcessHeap`/`ExitProcess`/`RaiseException` | `elena.asm:511-513`, `:201`, `:546` | `mmap`/`VirtualAlloc` abstraction needed |
| `GetWindowLongW`/`SetWindowLongW`/`DefWindowProcW` in the *core* | `elena.asm:1349-1439` | the window procedure trampoline is in `elena.bin`, i.e. the GUI is welded into the runtime core |
| PE-only image construction | `elc/win32/linker.cpp` | see [`04-pe-linker.md`](04-pe-linker.md) |
| `TCHAR`/`_UNICODE`/`<tchar.h>`/`<io.h>` | `common/common.h:14-25` | the whole toolchain is MSVC-flavoured; module files are UTF-16LE |
| `IMAGE_FILE_MACHINE_I386` | `linker.cpp:429` | with the comment `// !! machine type may be different` |
| x86 opcode bytes written literally | `x86jitcompiler.cpp` throughout | the entire backend |
| Structure serialisation via `sizeof`/`memcpy` | `elena.h:107`, `lists.h:1585` | `.nl` format is ABI-dependent |

### 11.4 What an LLVM backend would need

**Calling convention.** Two viable designs:

| | Design A: keep it close | Design B: normalise |
|---|---|---|
| `self` | explicit first parameter | first parameter |
| message parameter | second parameter | second parameter |
| result | return value | `{ptr, i1}` return |
| failure | `ret null` | the `i1` |
| `self` register pinning | drop it — LLVM allocates | drop it |
| hand-written asm compatibility | high (EDI/EBX/EAX still map) | requires rewriting all asm |

Design B is the right long-term target, but it forces (c) above to be done first.

**GC integration.** The current scheme (conservative scan of published frame extents) does
not survive an optimising backend: LLVM will keep references in callee-saved registers and
in spill slots the runtime cannot find. The options:

1. **`gc "statepoint-example"` + `llvm.experimental.gc.statepoint`** — precise stack maps,
   relocation of live pointers across safepoints. This is the correct answer for a moving
   (compacting) collector, which ELENA's is. It requires every allocation and every call
   that can trigger GC to be a statepoint, and the collector to consume LLVM's
   `StackMap` section.
2. **Shadow stack** (`gc "shadow-stack"`) — closest to today's frame chain, much slower,
   but a viable first milestone that keeps the existing collector almost unchanged.
3. Make the collector **non-moving** (mark-sweep with free lists) so that a conservative
   scan is merely imprecise rather than incorrect. This is the cheapest path but abandons
   compaction, which the current design relies on for allocation speed.

Recommendation: ship on (2) to get correctness, then move to (1) — and note that the write
barrier (§6.6) and the remembered sets survive either way.

**Exception model.** ELENA 1.5 has *no* exceptions; failure is a return value. Three
things nevertheless need an exception decision:
* out-of-memory currently raises a Win32 SEH exception (`elena.asm:193-202`) that nothing
  catches;
* `#external` calls into C can longjmp/throw;
* the language roadmap wants real exceptions.
The pragmatic answer is `nounwind` on all ELENA functions initially, with a plan to move to
`invoke`/landing pads when the language gains `try`.

**Debug info.** The `DebugLineInfo` + `.dn` scheme should be replaced wholesale by
`llvm.dbg.*` metadata and DWARF/CodeView; the IDE debugger then talks to LLDB/GDB instead
of `ide/win32/debugger.cpp`. The one thing to preserve is the *class → VMT address* map
(§8.4) that powers "show me this object's fields" — that becomes a DWARF type description
plus a synthetic `vtable_pointer` member.

**Linker.** `ReferenceLoader`'s demand-driven reachability is worth keeping as a
*front-end* concept (it is effectively `--gc-sections` with better information), but the
relocation machinery (§5.4), `x86LabelHelper` (§2.7), the PE writer and the
short/near-jump patcher all disappear. Emit LLVM modules, let `lld` link.

### 11.5 What must stay hand-written

| Component | Why | Size |
|---|---|---|
| The allocator fast path | must be inlined at every `ocreate`; an LLVM intrinsic + `alwaysinline` function works, but the bump-pointer/TLAB logic itself stays hand-tuned | `elena.asm:11-24` |
| The collector (mark/compact/fixup) | pointer arithmetic over raw memory; nothing LLVM adds | `elena.asm:206-486` |
| The dispatch helper | see §11.2(a) — either asm or a tight C function, but it is the hottest code in the system | `elena.asm:580-607` |
| Process entry / runtime bootstrap | must run before any managed code exists | `elena.asm:490-550` |
| Stack-map consumption / safepoint machinery | inherently target-specific | new |
| Write barrier | can be C, but must be inlined and branch-free-ish | `elena.asm:1104-1181` |

Everything else in `src/asm/*.asm` — the ~6100 lines of `standard`, `win32`, `winsock`,
`extended` — is *library* code that happens to be written in assembly because there was no
other way to reach the machine. It should be rewritten in ELENA (with intrinsics) or in C,
not ported.

### 11.6 Recommended order of attack

1. **Freeze the semantics** by writing an executable specification of the 40 opcodes
   (this document is the input; a bytecode interpreter is the artefact). Without it there
   is no way to know the LLVM backend is correct.
2. **Replace `#inline` asm snippets** with typed intrinsics — this unblocks everything
   else and is mechanical.
3. **Choose the calling convention and the failure representation** (§11.4). Do this
   before writing any IR.
4. **Build an LLVM backend for the existing bytecode**, keeping the existing object layout,
   VMT format and GC. Target the shadow-stack GC. Prove parity on the examples.
5. **Widen to 64-bit** — layout constants, `VA_ALIGNMENT` encoding, `.nl` format.
6. **Replace the module format** with something version-tolerant and ABI-independent
   (§4 shows why the current one cannot cross a compiler).
7. **Move to statepoints** and re-enable compaction.
8. **Only then** add threads — the GC redesign (§7.7) is a prerequisite, not a follow-up.
