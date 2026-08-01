# The Runtime in C — Design Revision

> **Supersedes parts of [`16-syntax-evolution.md`](16-syntax-evolution.md) §S2 and
> [`17-llvm-backend-and-targets.md`](17-llvm-backend-and-targets.md) §8.**
>
> Decision: **`src/asm/` is replaced by a single C runtime library, not by
> compiler-emitted LLVM IR.** This document explains why that is better than the earlier
> hybrid proposal, and what changes as a result.

---

## 1. The tension that LTO removes

The runtime audit warned, correctly, that:

> *"a naive C `elena_send()` will be **slower** than today, because the scan is currently
> inlined with zero call overhead."*

The same is true of the allocator: today it is a 7-instruction inline sequence
(`elena.asm:15-23`). Turning it into a C function call would be a regression on the single
hottest path in the system.

That argument assumes a C function stays a function call. It does not, if the runtime is
compiled to **LLVM bitcode** and linked with LTO:

```
libelena.c  ──clang -flto──▶  libelena.bc  ┐
                                            ├──▶ lld -flto ──▶ optimized native
elc → LLVM IR → .bc                        ┘
```

At link time LLVM sees both sides as one module and inlines `elena_alloc()` straight into
every allocation site. **You write maintainable C and get the hand-written assembly.**

For the handful of paths where this must not be left to a heuristic:

```c
__attribute__((always_inline))
static inline void *elena_alloc(size_t slots);
```

---

## 2. C, not C++, for the runtime

| | Runtime (`libelena`) | Compiler (`elc`) |
|---|---|---|
| Language | **C11** | **Modern C++** |

The compiler is already C++ and should modernize as C++. The runtime should be C, for
reasons that get stronger the further the project goes:

1. **Freestanding.** The stated end goal is an operating system written in ELENA. A C11
   runtime compiles with `-ffreestanding -nostdlib` and needs nothing. C++ drags in
   exceptions, RTTI, static-initialization order, `__cxa_atexit` and a personality routine —
   all of which have to be stubbed or disabled to run without an OS underneath.
2. **ABI stability.** No name mangling, no vague linkage, no inline-function ODR subtleties
   across six targets.
3. **Predictable codegen.** Nothing is generated behind your back — important for a GC,
   where an unexpected temporary holding a reference is a bug.
4. **Any toolchain.** GCC, Clang, MSVC, or a cross-compiler for a board you do not control.

The one place C++ would help — generic containers — is not something a GC and a dispatcher
need.

---

## 3. What this simplifies

My earlier proposal had a large compiler-owned intrinsic table lowering to hand-written IR
sequences. With a C runtime, most of that table **disappears**, because C already expresses
it portably:

| Was going to be | Becomes | Portable? |
|---|---|---|
| `core'i32'add` → hand-written IR | `a + b` in C | Yes — LLVM optimizes it identically |
| `core'f64'sqrt` → `llvm.sqrt.f64` | `sqrt()` from `<math.h>` | Yes — Clang lowers to the intrinsic anyway |
| `core'atomic'cmpxchg` → per-arch asm | `atomic_compare_exchange_strong` from **C11 `<stdatomic.h>`** | **Yes — fully portable, all six targets** |
| `core'mem'copy` | `memcpy` | Yes |
| int64 arithmetic (480 lines of asm) | `int64_t` operators | Yes |
| Thread-local state | C11 `_Thread_local` | Yes |

The atomics line is worth pausing on. `asm2bin` **cannot encode `lock`, `cmpxchg` or `xadd`
at all** — atomics do not exist anywhere in the current system, at any level. In C they are
`<stdatomic.h>`, standard since C11, correct on x86, arm64, ppc32, ppc64 and s390x,
including the acquire/release semantics that the weakly-ordered targets require.

**The hardest prerequisite for MTA is solved by choosing C.**

Likewise, memory ordering stops being something we hand-implement per architecture and
becomes something we *declare*: `memory_order_acquire`, `memory_order_release`. LLVM emits
the right fences for each target.

---

## 3.1 Numbered runtime routines must become named symbols

The `#inline pkg'N` problem has a second, less visible face: the runtime routines
are referenced **by number** from three different places, and only one of them is
even halfway fixed.

| Surface | Today | Count |
|---|---|---|
| C++ compiler | `#define ALLOC_FUNCTION _T("$package'elena'1")` — named at the call site, numbered at the binding | 25 constants (`elenaconst.h:43-69`) |
| Project config | `start = $package'elena'3` in `templates/console.cfg`, `'elena'35` in `gui.cfg` | 2 |
| Library sources | `#inline elena'8` | ~109 |

A flat numeric namespace over ~37 routines, with gaps — `elena'27` does not exist
at all. Renumber `elena.asm` and every one of these silently rebinds to the wrong
routine. Nothing detects it.

### What `elena'3` actually is

Worth spelling out, because the number hides it. `elena.asm:490`:

```asm
mov  ecx, ['gc_static_size]        ; zero the static root array
...
call 'dlls'kernel32.GetProcessHeap
call 'dlls'kernel32.HeapAlloc      ; allocate the entire GC heap
mov  ['gc_heap_start], eax         ; initialise the collector's pointers
```

It is not an entry point. It is **runtime initialization, with the heap coming
from `HeapAlloc`** — which is OS-development blocker 3
([`20-os-development.md`](20-os-development.md) §7) hiding behind a numeral. A
kernel has no `HeapAlloc`; it *is* the thing that provides memory.

### Where it goes

Under a C runtime these stop being numbers and become **ordinary linker symbols**:

```c
void  selene_runtime_init(const selene_region_source* heap);
void *selene_alloc(size_t slots);
void *selene_send(void *receiver, uint32_t message);
```

The payoff is not aesthetic. A named symbol that does not exist is a **link
error**; a wrong number is a silent call to the wrong routine. That single
property is worth more than the readability.

`start = $package'elena'3` disappears entirely: under LLVM and a system linker,
the C runtime's `main` (or `_start`, freestanding) initializes the runtime and
calls the program's entry symbol. The template stops naming a runtime routine and
names only the *program shape*, which is what a template should have described in
the first place.

### Sequencing

This is not a separate task. It falls out of the migration order in §7: as each
subsystem is ported to C, its callers move from `$package'elena'N` to the C
symbol. The three surfaces converge because they all end up naming the same
thing.

## 4. What the compiler must still emit as IR

A short list — these are per-call-site or per-frame, so they cannot live in a library:

| Emitted by the compiler | Why it cannot be a C function |
|---|---|
| The `{ptr, i1}` failure ABI | It is the calling convention itself |
| Inline-cache guard at each send site | Per-call-site state; the guard must be at the site |
| Shadow-stack frame push/pop | Per-frame; `always_inline` C is acceptable here |
| GC safepoint polls at back edges | Placement is a codegen decision |
| Field access | `GEP` on target slot size — trivially compiler-side |

Everything else — allocation, collection, dispatch slow path, string and array primitives,
conversions, I/O — is ordinary C in `libelena`.

### The one rule that must survive

> The VMT-pointer load must **never** be marked `!invariant.load` and must never be hoisted
> out of a loop, because `#shift` mutates a live object's VMT pointer by design.

In C this means the VMT slot is read through a qualified pointer, not cached in a local
across anything that could shift.

---

## 5. What still needs assembly

Close to nothing:

| Candidate | Verdict |
|---|---|
| Atomics | **No** — C11 |
| Math | **No** — libm / LLVM intrinsics |
| Calling conventions | **No** — LLVM's job |
| Stack scanning | **No** — the shadow stack removes the need to scan the machine stack |
| TLS | **No** — `_Thread_local` |
| Startup | **No** — a C `main` (or `_start` when freestanding) |
| Context switching | Only if green threads are ever wanted. Not needed for MTA on OS threads |
| Interrupt entry stubs | Only for the OS goal, and only the few instructions before C can take over |

So the honest answer is: **the runtime becomes 100% C for every hosted target**, and a small
amount of per-architecture assembly reappears only if and when a bare-metal kernel target
is attempted.

---

## 6. The practical payoff nobody would guess

A C runtime can be tested with tooling that hand-written assembly categorically cannot use:

| Tool | What it finds |
|---|---|
| **ThreadSanitizer** | Data races in the GC and allocator — *exactly* the class of bug that makes MTA hard |
| **AddressSanitizer** | The heap corruption class behind `knownbugs.txt #00024` |
| **UndefinedBehaviorSanitizer** | Alignment violations — which become real faults on ppc32 and s390x |
| **Unit tests** | The GC becomes testable in isolation, without compiling an ELENA program |
| **Fuzzing** | The module reader and the dispatcher |
| **Coverage** | Which of 198 runtime routines are actually exercised |

The GC bug open since 2009 was never fixed partly because there was no way to look for it.
TSan on a C GC is a weekend; TSan on `elena.asm` is not possible at all.

---

## 7. Revised migration order

Unchanged in shape, but each step now produces C rather than a mix:

| # | Subsystem | asm lines | Becomes | Notes |
|---|---|---:|---|---|
| 1 | int32 / int64 / real64 arithmetic | 1,130 | C operators, `<math.h>` | Fixes 3 latent int64 bugs for free. Switches x87 → SSE/target FP |
| 2 | `ftoa` / `atof` | 646 | `snprintf` / `strtod` | 10.5% of the runtime, deleted |
| 3 | Strings, arrays, byte dumps | 900 | C | UTF-8, decided — see §8.1 |
| 4 | **Win32 + Winsock syscall layer** | **1,785** | **`#external` declarations** | Becomes metadata, not code |
| 5 | Object creation, roles, identity, frames | 267 | C, `always_inline` | |
| 6 | Dispatch | 300 | C slow path + compiler-emitted inline cache | Binary search over the already-sorted VMT is a free win |
| 7 | **GC** | 600 | C on a shadow stack | Last, because everything else must be stable first |
| 8 | Delete `asm2bin`, `src/asm/`, `x86helper`, `[primitives]` | 3,335 + 6,146 | — | **DONE** — with the PE linker and the x86 JIT; `elc -c<prj>` links through LLVM on every platform |

Steps 1-3 are leaf work: no GC interaction, so each routine can be swapped and verified
independently against the legacy path. Step 7 is last for the same reason.

Throughout, both mechanisms coexist via `#external`, and `#inline pkg'N` becomes a **hard
error on any non-x86 target** so nothing silently architecture-locks.

---

## 8. What changes as a consequence

The user's observation that *"algumas coisas podem mudar"* is right. Concretely:

| Changes | From | To |
|---|---|---|
| Object header | 8 bytes, 32-bit fields | Target-word-sized; alignment a `TargetInfo` property |
| References | `address >> 3` + 8-bit tag, 128 MiB ceiling | Full pointers |
| Field access in bytecode | Byte offsets | **Slot indices** (`doc/todo.txt:334`) |
| Method addresses in VMTs | Integers | Typed function pointers (mandatory on ppc64 ELFv1) |
| Failure channel | `EAX == 0` | `{ptr, i1}`, optionally carrying a reason |
| Floating point | x87 only | Target FP — SSE2, VSX, NEON, z |
| Heap | `HeapAlloc`, fixed 64 KiB | Portable allocator, growable, pluggable region source |
| Strings | UTF-16 | UTF-8 (removes byte-order concerns entirely) |
| Atomics | Do not exist | C11 `<stdatomic.h>` |
| Memory ordering | Implicit x86 TSO | Explicit acquire/release |

None of these are optional under a multi-architecture, big-endian-capable target set. They
are the price of the six targets, and choosing C pays most of it automatically.

## 8.1 The text contract: UTF-8, decided

The "Strings: UTF-16 → UTF-8" row above is now a contract, not an intention.
**All runtime text is UTF-8.** Concretely:

- A literal object's payload is `[u32 length in BYTES][UTF-8 bytes][NUL]`.
  The length counts bytes, not characters. This is the layout the code
  generator materialises for constants (`llvmgen.cpp`, `valueConstant`) and
  the layout `runtime/natives.h` reads — the two must not drift.
- A character value is a Unicode code point in 32 bits.
- **No platform converts by default -- including Windows.** A POSIX
  descriptor takes the bytes as they are (`runtime/posix/io.c` converts
  nothing). On Windows the console renders UTF-8 directly once the code
  pages say so -- `selene_platform_init` calls `SetConsoleOutputCP(CP_UTF8)`
  / `SetConsoleCP(CP_UTF8)` exactly once -- and `WriteFile` then takes the
  runtime's bytes unchanged; redirected output gets clean UTF-8 with no BOM.
  This sets Selene's **Windows baseline at Windows 10 1903**. Converting to
  UTF-16 is a per-call exception reserved for APIs that leave no choice (the
  known case is console INPUT on builds where the -A read path is broken:
  read through `ReadConsoleW` and convert -- decided when real line input
  lands), and it lives inside `runtime/win32/`, nowhere else. No other
  runtime code may mention an encoding.

Why UTF-8 and not a fixed-width form: it is byte-oriented, so the layout is
identical on the big-endian targets (s390x, ppc) — no byte-order rule, no
BOM, nothing for module portability to record; it matches the compiler
build's own character model (`ELENA_UTF8=ON`); and the C implementations of
the string natives become plain byte code with no `wchar_t` width trap.

**Deliberately still open:** the indexing semantics of the `standard'`
string natives (`strgetchar` and friends). The 2009 code indexed fixed-width
units; under UTF-8 the natural native is "code point at byte offset, plus
the next offset", but whether the library's byte code does index arithmetic
that assumes one-unit-per-character has to be read from the library sources,
not guessed. Decide it in step 3 of §7, with the call sites in front of you.

---

## 9. Risks

1. **LTO must actually work end to end.** Both the ELENA-generated IR and `libelena.bc` must
   reach the linker as bitcode. Verify early with a benchmark — an allocation loop that must
   compile to the same instruction count as the 2009 assembly. If it does not inline, the
   whole performance argument collapses and the design must be revisited.
2. **`always_inline` on the wrong function** bloats code size on ppc32, where I-cache is
   scarce. Measure rather than annotate by intuition.
3. **A C compiler may keep references in registers** where a conservative collector could
   not see them — which is precisely why the shadow stack (explicit, precise roots) must land
   *before* the GC is rewritten in C, not after.

Risk 3 is the one that could actually bite. It is the reason the migration order puts the
GC last.
