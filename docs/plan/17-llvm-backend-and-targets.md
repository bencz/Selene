# LLVM Backend & Multi-Target Design

> **Status: design proposal for discussion.**
>
> Target set requested: **x86-64, arm64, ppc64 (BE), ppc64le, ppc32 (BE), s390x (BE)** —
> across Linux, Windows and macOS. That is `{32,64} × {LE,BE}`, which changes several
> decisions that would be obvious for x86-only.

---

## 1. The target matrix and what each one costs

| Target | Bits | Endian | ABI notes that matter here |
|---|---|---|---|
| x86-64 | 64 | LE | SysV / Win64 differ; baseline |
| arm64 | 64 | LE | **Weak memory ordering**; AAPCS64 |
| ppc64le | 64 | LE | ELFv2 — direct entry points |
| **ppc64 (BE)** | 64 | **BE** | **ELFv1 — function pointers are *descriptors*, not code addresses** |
| **ppc32** | 32 | **BE** | **Weak memory ordering**; strict alignment |
| **s390x** | 64 | **BE** | 160-byte linkage area; strong ordering |

Three properties partition this set, and each one breaks something different in the
current code:

1. **Word size** — breaks the object model and the module format
2. **Byte order** — breaks the module format and every raw memory view
3. **Memory ordering** — breaks the (future) MTA runtime

### 1.1 The alignment landmine

`dump.h:32-34` exposes `int& operator[](size_t)` at **arbitrary byte offsets**. It is the
sole accessor for every hash-bucket link in the module format. On x86 this "works" because
x86 tolerates unaligned access. On **ppc32 and s390x it traps or is drastically slow**,
and it is undefined behaviour everywhere.

This was already flagged as a portability defect. With BE targets in scope its severity
goes from *cleanup* to *hard blocker*.

### 1.2 The memory-ordering landmine

The current runtime contains **no memory barriers of any kind**. It is implicitly written
for x86's strong ordering (TSO). s390x is also strongly ordered, so both survive — but
**ppc32, ppc64 and arm64 are weakly ordered**.

This costs nothing today (single-threaded), but the moment MTA lands, every publication of
an object, every VMT mutation via `#shift`, and every GC handshake needs explicit
acquire/release semantics on those targets.

**Consequence:** ELENA needs a defined memory model before MTA, not after. Use the C11/LLVM
model — `monotonic / acquire / release / acq_rel / seq_cst` — and express it through the
`core'atomic'` intrinsics proposed in
[`16-syntax-evolution.md`](16-syntax-evolution.md) §S2. Getting this right is free on x86
and s390x and mandatory on ppc and arm64.

### 1.3 ppc64 ELFv1 function descriptors — the subtle one

On **big-endian ppc64 (ELFv1)** a function pointer is **not a code address**. It points to
a 3-word descriptor `{entry, TOC, environment}`. ppc64le uses ELFv2 and has no descriptors.

The current design would break here in two ways:

- The VMT stores method addresses as **integers** and calls `call [esi-4]` after pointer
  arithmetic.
- References are stored **scaled**, as `address >> 3` with an 8-bit tag
  (`jitlinker.cpp:130`, `elena.h:228`). Descriptors and code have different alignment, so
  the scaling assumption does not survive.

**Design rules that follow:**

> **R1.** VMT slots hold **typed LLVM function pointers**. Calls go through IR
> `call ptr %fn(...)`. LLVM then emits whatever the target ABI requires — descriptors on
> ELFv1, direct branches elsewhere.
>
> **R2.** Never `ptrtoint` a function pointer and do arithmetic on it. Never assume a
> method address points into `.text`.
>
> **R3.** Drop the scaled-reference scheme entirely. Store full pointers.

R3 was already recommended for 64-bit reasons; ppc64 BE makes it non-negotiable.

---

## 2. The cross-compilation principle

The single most important structural rule, and the one currently violated everywhere:

> **Target properties must never be derived from host properties.**

Today `ref_t` is `#define ref_t size_t` (`common.h:21`) — a *target* concept defined as a
*host* type. `sizeof(wchar_t)` is assumed to be 2. `sizeof()` of host structs defines the
on-disk format.

Introduce an explicit target description consulted for every layout decision:

```cpp
struct TargetInfo {
   unsigned  pointerBits;      // 32 | 64
   Endianness endianness;      // Little | Big
   unsigned  objectAlign;      // 8 | 16
   unsigned  slotBytes;        // pointerBits / 8
   bool      functionDescriptors;   // ppc64 ELFv1
   MemoryModel ordering;       // TSO | Weak
};
```

`elc --target=s390x-unknown-linux-gnu` must produce byte-identical output regardless of
the machine it runs on. **Cross-compilation is not a feature to add later — it is the
only way to test six targets.**

Distinguish clearly in the source: `host_size_t` (the compiler's own memory) vs
`target_word_t` (what it is generating for). They are unrelated and currently conflated.

---

## 3. Module format v2 (`EN!20`)

The current `.nl` is a **raw dump of 32-bit little-endian process memory** — hash maps
written with `sizeof`/`memcpy` including compiler padding. It records neither word size nor
character width nor byte order, so mismatches produce garbage **silently**.

With BE targets this is not portable at all. Format v2 requirements:

| Rule | Rationale |
|---|---|
| **Canonical little-endian on disk**, always | A module compiled on x86-64 must be consumable by a codegen targeting s390x. Pick one order and byte-swap on BE hosts |
| **Explicit field-by-field serialization** | Never `memcpy` a struct. Padding must not be part of the format |
| **Magic + version + flags header** | `EN!10` has no version field at all |
| **Slot *indices*, not byte offsets** | See below — the original author asked for exactly this |
| **UTF-8 string table** | No byte order, no BOM, no surrogate handling |
| **Explicitly-sized scalars** (`u8/u16/u32/u64`) | Never `int`, never `long`, never `size_t` |
| **Target-neutral** | The same `.nl` feeds every backend. Word size belongs to codegen, not to the module |

### 3.1 Slot indices, not byte offsets

`doc/todo.txt:334`, written by the original author in 2009:

> *"do not use processor specified offsets in byte code (e.g. for `bcOMovePtr` we should
> use index instead of real offset)"*

He was right, and it is now load-bearing. If field access is encoded as "byte offset 12",
the bytecode is 32-bit-only. If it is encoded as "field index 3", the same bytecode works
on every target and the backend multiplies by the target slot size.

**This one change is what makes a single `.nl` portable across all six targets.** It should
be treated as the defining feature of format v2.

### 3.2 Also fix `syntax.dat`

Same defect class: no magic, no version, embeds `sizeof(size_t)` and `sizeof(TCHAR)`
(`parsertable.cpp:208-215`). A 64-bit `sg` feeding a 32-bit `elc` loads garbage in silence.
~10 lines to fix. Do it on day one.

---

## 4. Pipeline architecture

```
     .l source
        │
        ▼
   elc frontend                       (unchanged initially)
        │  derivation stream
        ▼
   _CodeEmitter  ◄── interface extracted from ByteCodeCompiler
        │
        ▼
   bytecode  ──────►  .nl module v2   (canonical LE, slot-indexed, target-neutral)
        │
        ▼
   BytecodeReader
        │
        ▼
   LLVMEmitter  ──►  LLVM IR Module
        │
        ▼
   LLVM opt / TargetMachine  ──►  .o   (ELF / COFF / Mach-O, any arch)
        │
        ▼
   lld  ──►  executable
```

### 4.1 Why keep bytecode instead of going straight to IR

1. **Differential testing.** The same `.nl` can be run through the legacy x86 JIT *and* the
   LLVM backend, and the results compared. Without this, the new backend is written against
   a specification we reverse-engineered from assembly, with nothing to check it against.
2. **`.nl` stays the distribution artifact**, which the project already assumes.
3. **Cross-compilation falls out for free**: compile once to bytecode, run codegen per
   target.
4. **The frontend does not have to change on day one.** The first prerequisite —
   extracting `_CodeEmitter` as an interface from the concrete `ByteCodeCompiler` held *by
   value* at `compiler.h:353` — is the highest-leverage refactor identified anywhere in the
   audits.

A direct frontend → IR path can be added later for optimization. It should not be the first
thing built.

### 4.2 Stack-to-SSA

ELENA bytecode is stack-based; LLVM IR is SSA. This is the well-trodden JVM/CIL → LLVM
problem. Approach: emit `alloca` per stack slot and per frame local, then let LLVM's `mem2reg`
promote them. Do **not** hand-roll SSA construction — `mem2reg` is free, correct, and better.

The exception is GC roots: slots holding traced references must *stay* in memory (see §6).

---

## 5. Dispatch

### 5.1 A free win available immediately

VMT entries are **already sorted by signed message ID**, terminated by `0x7FFFFFFF`. The
current runtime does a **linear scan** anyway. Binary search is available with zero layout
change: **O(log n) instead of O(n)**.

### 5.2 Inline caches, and the rule that must not be broken

Per-call-site monomorphic inline cache:

```llvm
%vmt   = load ptr, ptr %obj.vmtslot        ; NO !invariant.load
%hit   = icmp eq ptr %vmt, %cached.vmt
br i1 %hit, label %fast, label %slow
```

> **R4.** The VMT-pointer load must **never** carry `!invariant.load`, and must never be
> hoisted out of a loop.

`#shift` mutates the live object's VMT pointer in place (`elena.asm:993`) — that *is* the
"shift technology" the language is built on. Any optimization keyed on "an object's class
does not change" is unsound in ELENA. This single rule is easy to violate accidentally and
would produce miscompiles that appear only under optimization.

### 5.3 The performance warning

The current linear scan is **inlined with zero call overhead**. A naive C `elena_send()`
will be **slower than the 2009 assembly**. Inline caches are not an optimization here —
they are what makes the rewrite not a regression.

---

## 6. Garbage collection

### 6.1 Start with the shadow stack

The runtime already maintains an explicit frame chain: `{size, prevLink}` records on the
machine stack, rooted at `gs_current_frame`. **That is a hand-written shadow stack.**
LLVM's `gc "shadow-stack"` is a direct generalization of it.

This is the most fortunate finding in the audit: the 2009 design already has the right
structure for precise roots. Migrating makes **per-thread roots trivial** — each thread
gets its own chain head — which removes what looked like the hardest part of MTA.

`gc.statepoint` remains available later if a large moving heap needs it. Do not start
there.

### 6.2 What must change regardless

| Current | Problem | Fix |
|---|---|---|
| Slot-**conservative** scanning, and the collector **compacts** | A false positive corrupts data — this is a correctness bug, not imprecision | Precise roots via shadow stack |
| **One** safepoint, inside the allocator | A non-allocating loop never yields | Poll at back edges and calls |
| Global bump pointer, unlocked | Any second thread corrupts the heap | Per-thread TLAB |
| `fs:[4]`/`fs:[8]` (Win32 TIB) for stack bounds | Windows-x86-only | Remove stack-allocated objects, or use a portable thread-context record |
| Heap from `HeapAlloc`, never grows | 64 KiB ceiling; no way to hand the runtime a region | Portable allocator with a pluggable region source (also unblocks OS work) |
| No barriers | Fine on x86/s390x, **broken on ppc/arm64 under MTA** | Explicit acquire/release via `core'atomic'` |

---

## 7. The failure ABI

Every call currently ends `test eax,eax; jz <fail>`; `0` means failure, so `nil` must be a
real heap object because the null pointer is reserved.

**Recommendation:** lower to LLVM aggregate returns `{ptr, i1}`.

| Option | Verdict |
|---|---|
| `{ptr, i1}` | **Chosen.** Cheap, register-allocated on every target, preserves semantics exactly, no unwinder involvement |
| `invoke` + landing pads | Natural fit for the "failure edge" already in the bytecode, but drags in the unwinder and per-target EH tables. Costly on six targets |
| Real exceptions | Rewrites every call site. Do not do this to enable the backend |

Combine with the **tagged failure object** language change (S1): `{null, false}` is a plain
failure, `{reason, false}` carries information. The representation and the language change
are orthogonal and can land independently.

---

## 8. `rcallemb` — the largest mechanical task

`rcallemb` pastes **arbitrary assembly blobs mid-function** (`x86jitcompiler.cpp:234`),
~100 sites across `standard.asm` and `win32.asm`. **LLVM has no equivalent** and cannot
acquire one — the construct is meaningless on a target-independent IR.

Every one becomes a named intrinsic (S2) or a runtime call. This is:
- the **largest** mechanical task in the project by volume,
- a **hard blocker** — it gates everything else,
- and **fully enumerable today**, since the runtime audit catalogued all 198 asm sections.

Treat it as the first real work item after the interface extraction.

---

## 9. What gets deleted

| Component | Lines | Replaced by |
|---|---:|---|
| `asm2bin/` | 3,335 | LLVM MC (and ~200 lines of relocation emission) |
| `engine/win32/x86jitcompiler.*` + `x86helper.*` | 1,366 | LLVM codegen |
| `elc/win32/linker.cpp` | 637 | `lld` |
| `src/asm/*.asm` | 6,146 | intrinsics + portable C |
| Frame prologue/epilogue templates | 60 | LLVM emits them |
| **Total** | **~11,500** | |

Gained for free: arm64, ppc, s390x, DWARF, PIE, ASLR, ORCv2 JIT, LTO, sanitizers.

---

## 10. Sequencing

| # | Step | Gate |
|---|---|---|
| 0 | `TargetInfo`; `host_size_t` vs `target_word_t` split; `syntax.dat` magic+version | Nothing else is verifiable without it |
| 1 | Extract `_CodeEmitter` interface from `ByteCodeCompiler` | Highest-leverage refactor in the project |
| 2 | **Module format v2** — canonical LE, explicit serialization, **slot indices** | **Critical path.** Blocks all codegen verification |
| 3 | Fix unaligned access (`dump.h:32-34`) | Hard blocker for ppc32/s390x |
| 4 | Intrinsic table + `#intrinsic`; port `rcallemb` sites | Largest mechanical task; gates the backend |
| 5 | `BytecodeReader` + `LLVMEmitter`, x86-64 only, differential-tested vs. legacy JIT | Correctness oracle available exactly here |
| 6 | Shadow-stack GC; safepoints at back edges | Required before any optimization is trustworthy |
| 7 | Second target: **arm64** (LE, weak ordering) | Proves the memory model without changing byte order |
| 8 | Third target: **s390x** (BE, strong ordering) | Proves byte order without changing the memory model |
| 9 | **ppc64 BE** (descriptors) and **ppc32** (BE + weak + strict alignment) | Hardest; both variables at once — do last |

Steps 7-9 are deliberately ordered to change **one variable at a time**. Going straight
from x86-64 to ppc32 would change word size, byte order, memory ordering and alignment
strictness simultaneously, and any failure would be unattributable.
