# Writing an Operating System in ELENA — Design Implications

> **Status: design proposal for discussion.**
>
> The stated end goal is a kernel written in ELENA. This document collects what that
> requires, and — more usefully — what it changes about decisions being made *now*, before
> any kernel work starts.
>
> The single most important claim here: **most of the cost is paid by choices made during
> the LLVM/runtime migration.** Deferring them makes the OS goal dramatically more
> expensive later.

---

## 1. Why this validates the C runtime decision

[`19-runtime-in-c.md`](19-runtime-in-c.md) chose C11 over C++ for `libelena`, with
*freestanding* as the first argument. With the OS goal confirmed, that stops being a
preference and becomes a requirement.

A kernel written in ELENA must **link the ELENA runtime into the kernel image**. That means
`libelena` must compile with `-ffreestanding -nostdlib` and depend on nothing:

| C++ feature | What it drags in | Bare-metal cost |
|---|---|---|
| Exceptions | Unwinder, personality routine, `.eh_frame` | Must be disabled entirely |
| RTTI | Type-info tables | Disable |
| Static initialization | `__cxa_atexit`, init-array ordering | Hand-written init |
| `operator new` | An allocator that must already exist | Circular at boot |

C11 has none of these. It also has `<stdatomic.h>`, which works bare metal on every target
because atomics are compiler intrinsics, not library calls.

**Conclusion: the runtime language choice is settled by this goal, not merely informed by
it.**

---

## 2. A tiered runtime

The runtime cannot be one monolithic thing. An interrupt handler and a file-reading library
routine have incompatible requirements. Propose three tiers, enforced by the build:

| Tier | Name | May use | Cannot use | Used by |
|---|---|---|---|---|
| **0** | `core` | Raw memory, atomics, bit ops, arithmetic, MMIO | GC, allocation, dispatch that may fail into allocation | ISRs, early boot, the allocator itself, spinlocks |
| **1** | `managed` | Everything in 0, plus objects, GC, dispatch | OS services — files, threads-from-OS, sockets | Kernel proper, drivers |
| **2** | `hosted` | Everything | — | Normal ELENA programs |

Package mapping follows directly:

| Tier | Packages |
|---|---|
| 0 | new `core` package |
| 1 | `$elena`, most of `std` |
| 2 | `sys`, `ext`, `gui`, platform packages |

The tier is a module attribute recorded in the `.nl` metadata, and **the linker refuses to
link a lower tier against a higher one**. This is the mechanism that keeps "the kernel
accidentally called `printf`" from being a runtime discovery.

Note this costs nothing for hosted programs — it is a build-time constraint, not a runtime
one.

---

## 3. `[nogc]` — a verified language attribute ★

The most important *language* addition for OS work, and one that is cheap now and expensive
later.

```elena
#method handleInterrupt [nogc]
[
   // compiler PROVES no allocation can occur in here
]
```

The compiler verifies, transitively:

- no object creation
- no boxing of integers or reals
- no string concatenation
- no call to any method not itself `[nogc]`
- no safepoint poll inserted

Violations are **compile errors**, not runtime surprises.

### Why this matters more than it looks

Three unrelated things need exactly this guarantee:

1. **Interrupt handlers** — cannot allocate, because the allocator may be mid-collection on
   the interrupted CPU.
2. **The allocator and collector themselves** — obviously cannot allocate.
3. **Any code holding a spinlock** — because allocation can trigger a collection, which can
   block, which deadlocks while holding a lock.

Item 3 applies to ordinary MTA work, not just kernels. So `[nogc]` earns its cost during the
threading phase regardless of whether a kernel is ever written.

### Implementation note

Dynamic dispatch itself does *not* allocate — it is a VMT scan — so `[nogc]` code may still
send messages, which preserves ELENA's character. What it cannot do is take a path that
boxes or constructs. The failure path is the subtle case: `fail:` carrying a reason object
would allocate, so `[nogc]` code is restricted to plain failures.

---

## 4. Raw memory, and why MMIO is *not* just a pointer

[`16-syntax-evolution.md`](16-syntax-evolution.md) §S5 proposed gated raw pointers. The OS
goal makes that mandatory rather than optional, and adds a distinction that is easy to miss:

> **A normal load through a pointer and an MMIO read are different operations, and LLVM will
> destroy the second one if you express it as the first.**

```elena
#intrinsic core'mem'load32   (p)        // may be cached, reordered, elided
#intrinsic core'mmio'read32  (p)        // volatile: exactly one access, never elided
#intrinsic core'mmio'write32 (p, v)
```

An optimizer is entitled to delete a load whose result is unused, hoist it out of a loop, or
merge two loads of the same address. For a device status register, every one of those is a
bug. The distinction must exist at the intrinsic level, lowering to `volatile` loads and
stores in LLVM IR.

### Device barriers ≠ thread barriers

Equally easy to get wrong, and target-specific:

| Purpose | ARM64 | ppc | x86 |
|---|---|---|---|
| Thread ordering | `dmb ish` | `lwsync` | usually nothing (TSO) |
| Device ordering | `dsb sy` | `eieio` / `sync` | often nothing, but `mfence` for WC memory |

C11's `memory_order_*` covers the first. The second needs explicit intrinsics —
`core'mmio'barrier` — because the C memory model has nothing to say about device access.

---

## 5. Per-CPU data — the `_Thread_local` trap

[`19-runtime-in-c.md`](19-runtime-in-c.md) listed C11 `_Thread_local` as the portable answer
for thread-local state. **That is true only on hosted targets.**

On bare metal there is no dynamic loader, no `__tls_get_addr`, and no TLS setup. The kernel
must provide per-CPU data itself, conventionally through a dedicated register:

| Target | Convention |
|---|---|
| x86-64 | `GS` base, via `swapgs` on kernel entry |
| arm64 | `TPIDR_EL1` |
| ppc64 | `r13` |
| s390x | Access registers / prefix page |

**Design consequence:** the runtime must access per-CPU state through **one internal
abstraction** — `elena_current_cpu()` — with two implementations: `_Thread_local` on hosted
targets, and a register read on bare metal. If per-thread state is scattered across the
runtime as bare `_Thread_local` variables, retrofitting this is invasive.

This is a concrete example of the general point: **cheap now, expensive later.**

---

## 6. Two trampoline flavours

[`18-ffi-design.md`](18-ffi-design.md) §7 proposed generalizing the `WndProc` thunk into a
compiler-generated callback trampoline, noting it would serve GUI callbacks, thread entry
points, signal handlers and ISRs alike.

With the OS goal explicit, that needs splitting. An ISR cannot do what a hosted callback
does:

| Step | Hosted callback | **ISR trampoline** |
|---|---|---|
| Attach calling thread to runtime | Yes | **No** — the CPU is not a thread |
| Transition native → managed, may block for GC | Yes | **No** — cannot block in an interrupt |
| Establish GC frame | Yes | **No** — target is `[nogc]` |
| Marshal arguments | Yes | Minimal — raw values only |
| Switch stack | No | **Maybe** — per-CPU interrupt stack |
| Save/restore full CPU state | No | **Yes** — including SIMD if the handler touches it |

So: **`#external'callback` (hosted) and `#external'interrupt` (bare metal)**, the latter
requiring a `[nogc]` target method. The few instructions of entry/exit assembly are the only
place assembly legitimately returns — and only for a bare-metal target.

---

## 7. The heap must be given a region

Today the entire GC heap comes from `HeapAlloc` once, never grows, and is 64 KiB by default
(`elena.asm`, `gcsize=4096`). A kernel has no `HeapAlloc`; it *is* the thing that provides
memory.

The allocator needs a pluggable region source:

```c
typedef struct {
   void *(*acquire)(size_t bytes);   // mmap / VirtualAlloc / kernel physical allocator
   void  (*release)(void *p, size_t bytes);
} elena_region_source;
```

Hosted builds supply an mmap-based one; a kernel supplies its own physical page allocator.
This also fixes the fixed-64-KiB-heap limitation for ordinary programs, so it is not
kernel-only work — another case where the OS goal and the general modernization want the
same thing.

---

## 8. Linking a kernel image

Kernels are linked differently from programs: a fixed high virtual base, no PIE, custom
section layout, an explicit entry symbol, often a separate physical load address.

`lld` handles all of this through a linker script. This is a further argument for
[`17-llvm-backend-and-targets.md`](17-llvm-backend-and-targets.md) §D4 — delegating object
formats to LLVM rather than hand-writing emitters. The current PE linker has a **hard-coded**
`_codeBase = 0x1000` and image base `0x400000` (`linker.cpp:319`, `:23`); it could never
produce a kernel image.

---

## 9. What the GC can and cannot do in a kernel

The honest position: a moving, stop-the-world collector is a poor fit for interrupt-driven
kernel code, and pretending otherwise would be a design error.

Workable split:

| Context | Allocation | Collection |
|---|---|---|
| Boot, ISRs, scheduler, allocator, lock-holding code | **`[nogc]`** — none | Never |
| Drivers, filesystems, network stack, most kernel logic | Allowed | At explicit yield points |
| User-space ELENA programs | Allowed | Normal |

This is not a compromise forced by ELENA — it is roughly what managed-language kernel
projects (Singularity, Redox's allocator boundaries, various Java/C# kernel research
systems) converge on. The `[nogc]` attribute is what makes the boundary checkable rather
than conventional.

---

## 10. What changes in the plan, starting now

The OS goal does not add a phase at the end. It **reorders and constrains current work**:

| Decision | Without OS goal | **With OS goal** |
|---|---|---|
| Runtime language | C preferred | **C required** — freestanding |
| Raw pointers (S5) | Nice to have, later | **Design into the IR now** — retrofitting is backend rework |
| `[nogc]` attribute | Not considered | **Add during the threading phase** — needed for spinlocks anyway |
| MMIO intrinsics | Not needed | **Must be distinct from normal loads** from day one |
| Per-CPU state | `_Thread_local` everywhere | **One `elena_current_cpu()` abstraction** |
| Heap source | mmap is fine | **Pluggable region source** |
| Tiering | Not needed | **Module tier in `.nl` metadata** — cheap now, invasive later |
| Callback trampolines | One flavour | **Two flavours** |
| Object formats | LLVM preferred | **LLVM required** — linker scripts, fixed base, non-PIE |

None of these are large. All of them are much larger if added after the backend and module
format have solidified — which is exactly the situation the original 2009 codebase found
itself in, and why it never got past Win32/x86.

---

## 11. What ELENA already has going for it

Worth stating, because the blocker list is long and the starting position is better than it
looks:

- **Fixed-layout structs already work** — `#field(N)` produces `elStructureRole`; `MSG` is
  `#field(28)` and `PAINTSTRUCT` is `#field(64)`, real byte-accurate OS structures.
- **Complete bit manipulation** — `and: or: xor: not shift: anyMask: allMask:`.
- **AOT compilation with no runtime codegen** — VMTs are laid out statically at link time,
  which is exactly what a kernel image needs. There is no JIT to bootstrap.
- **No hidden allocations in the object model** beyond object creation itself.
- **The dispatch model is not a liability** — a linear/binary VMT search is roughly what a
  microkernel IPC layer does anyway, and `#shift` maps cleanly onto device state machines.

The gap is not the object model. It is memory access, effects tracking (`[nogc]`), and the
runtime's assumption that an OS exists underneath it — all three of which are addressed by
decisions being made in this migration regardless.
