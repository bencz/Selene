# Modernization Roadmap — Synthesis

> This document synthesizes the findings of the eight subsystem audits into a single
> ordered plan. It is the only document here that is *opinionated*; the subsystem
> documents describe the code as it is.
>
> Effort figures are estimates and should be treated as relative magnitudes, not
> commitments. Everything marked **measured** was empirically verified.

---

## 1. The five decisions that must be made before writing code

These are not implementation tasks. Each one, if decided late, causes rework of work
already done. They are ordered by how early they bind.

### D1 — The failure protocol becomes what, exactly?

Flagged independently by the frontend, engine and runtime audits.

Today every call ends `test eax,eax; jz <fail>`. `EAX == 0` simultaneously means
*message not understood*, *index out of range*, *parse failure* and *boolean false*.
The standard library depends on this conflation, and `nil` must be a real heap object
because the null pointer is reserved for failure.

| Option | Cost | Consequence |
|---|---|---|
| `{ptr, i1}` pair returns in LLVM IR | Low, mechanical | Preserves current semantics exactly; keeps ELENA's identity |
| LLVM `invoke` + landing pads | Medium | Natural fit for the "failure edge" already in the bytecode |
| Real exceptions | High — rewrites every call site | Gains diagnostics; changes the language |
| **Tagged failure object** (`fail:` takes an argument, `\|` binds it) | Low-medium, *nearly source-compatible* | Failures carry information. **Required for OS work** (see D5) |

**Recommendation:** `{ptr, i1}` in the IR *plus* the tagged-failure language change. They
are orthogonal — one is representation, the other is semantics — and the second is the
highest value-per-effort change available anywhere in this project.

### D2 — Module format v2 (`EN!20`) — this is the critical path

The `.nl` format is a **raw dump of 32-bit process memory**: hash maps serialized with
`sizeof`/`memcpy` including compiler padding (`lists.h:1594,1599`; `elena.h:107`).
`_MemoryMapItem` goes 12 → 24 bytes at LP64, `ClassHeader` 12 → 24, `DebugLineInfo`
20 → 32. The magic `"EN!10"` records neither word size nor character width, so
mismatches are **undetected** — they produce garbage, silently.

This blocks everything downstream: you cannot verify a 64-bit codegen against a format
that cannot be read by a 64-bit process. **Format v2 must land before the backend work,
not alongside it.**

Same defect class in `syntax.dat` (`parsertable.cpp:208-215`): no magic, no version,
embeds `sizeof(size_t)` and `sizeof(TCHAR)`. Fixing that is ~10 lines and should happen
on day one.

### D3 — Do the OS-enabling language features land before or after the IR shape?

The language audit is blunt: **LLVM removes less than you'd hope.** It deletes
`standard.asm` and the `#inline`/`#external` split, but delivers none of the five
things ELENA needs to be an OS language:

| # | Blocker | Why LLVM doesn't fix it |
|---|---|---|
| 1 | **No pointer type** — `ByteArray` can only reference GC-allocated memory | Language design. No page tables, no framebuffer, no MMIO |
| 2 | **Failures carry no information** | See D1 |
| 3 | **Heap is `HeapAlloc`** — no arenas, no `free`, no handing the runtime a region | Runtime design |
| 4 | **No callback ABI** — the only native entry is a hand-written `WndProc` thunk | Generalizing this yields ISRs, syscall handlers *and* threads at once |
| 5 | **Struct fields are unnamed** — `#field(8)` + manual offsets | Fine for 12 Win32 structs; a corruption generator for kernel structs |

**Recommendation:** settle at minimum a real `#external` declaration syntax and the
pointer/raw-memory story **before** the IR shape locks in. Blocker 4 in particular is
a shared prerequisite of both MTA and OS work — build it once.

### D4 — Object formats: write them or delegate them?

Hand-writing PE + ELF + Mach-O ≈ 20-29 weeks plus permanent maintenance of
code-signing, dyld and `gnu.hash` churn. An LLVM IR emitter ≈ 10-14 weeks and **deletes**
`asm2bin` (3,335), `x86helper` + `x86jitcompiler` (1,366) and `linker.cpp` (637) while
delivering arm64, DWARF, PIE and ORCv2 JIT for free.

**Recommendation: delegate.** Two independent audits reached this conclusion.

### D5 — The IDE: port, rewrite, or replace?

Three audits converged on the same answer.

| | Port to Qt/GTK | Rewrite | **LSP + DAP** |
|---|---|---|---|
| Effort | 9-15 mo | 18-30 mo | **3-6 mo** |
| Code that survives | ~4,600 | ~1,700 | **~1,700** |
| Risk | High — *no compiling intermediate state* | Very high | **Low, incremental** |

The decisive evidence is in the repository itself: `ide/gtk/main.cpp` is a **49-line GTK
hello-world referencing nothing**. The author tried to port and stopped, because
`ideconst.h` is the only file in `ide/` that compiles without `windows.h`.

**Recommendation:** LSP server + DAP adapter. Promote `debugcontroller.*` (855 lines,
already platform-independent by design) to `elenasrc/debug/` behind a `_DebugTarget`
interface. **Write the Linux `ptrace` backend before the DAP adapter** so Win32 semantics
are not baked into the interface. If the LLVM backend emits DWARF, `gdb`/`lldb` debug
ELENA natively and the DAP adapter may become unnecessary — decide that first.

---

## 2. What is actually hard

The audits corrected my initial assumption. There are **three** hard problems, not two,
and the largest by volume was invisible from the outside:

| Problem | Size | Why it is hard |
|---|---:|---|
| **`rcallemb` asm blobs** | ~100 sites | Pastes arbitrary assembly mid-function (`x86jitcompiler.cpp:234`). **No LLVM equivalent exists.** Converting to typed intrinsics is the largest mechanical task and blocks everything else |
| **GC root scanning** | 600 lines | Region-precise but **slot-conservative** — and it *compacts*. A false positive corrupts data. An optimizing backend keeps refs in registers and spill slots the collector cannot see |
| **Dispatch** | 300 lines | Linear VMT scan, currently *inlined with zero call overhead*. A naive C rewrite is **slower**. Needs inline caches that cannot be hoisted, because `#shift` mutates the VMT pointer at runtime by design |

### One piece of unexpected good news

The runtime already maintains an explicit frame chain (`gs_current_frame`, `{size,
prevLink}` records on the machine stack). That is a **direct generalization of LLVM's
shadow stack** (`gc "shadow-stack"`). The 2009 design already has the right structure for
precise roots — it is just hand-written and single-threaded. Migrating makes per-thread
roots straightforward rather than the hardest part of MTA.

Recommended path: shadow stack first, `gc.statepoint` later if a large moving heap is
needed.

---

## 3. Why MTA requires the rewrite

Multithreading is not a feature to add to this runtime. The blockers are structural, and
the most fundamental one is in the *tooling*:

**`asm2bin` cannot encode `lock`, `cmpxchg` or `xadd`.** No atomic instruction exists in
the dialect. The assembler that builds the runtime is physically incapable of expressing
synchronization.

Everything else follows: `gc_yg_heap` is a global bump pointer with non-atomic RMW; the
heap is `HEAP_NO_SERIALIZE`; there is a **single global root chain**, so other threads'
stacks are invisible; there is no stop-the-world and no safepoint mechanism — **exactly
one safe point exists in the entire system**, inside the allocator; `fs:[4]`/`fs:[8]`
reads only the current thread's TIB; the write barrier is a racy read-modify-append that
silently drops cross-generation references.

Additionally the debugger is *actively* incompatible: **any `EXIT_THREAD` event ends the
debug session** (`debugger.cpp:396`).

---

## 4. Phased plan

Phases P0-P7 with dependencies `P0 → P1 → P2 → {P3 → P4, P5} → P6 → P7`.

| Phase | Scope | Effort | Definition of done |
|---|---|---|---|
| **P0** | CMake; backslash includes; MSVC-isms; `syntax.dat` magic+version | 2-3 wk | CMake builds `elc` on Linux `-m32`, byte-identical `.nl` output |
| **P1** | `TCHAR` → UTF-8 (1,066 lines, 15 `<<1` sites) | 4-6 wk | Identical `.nl` on Linux and Windows; non-ASCII identifiers round-trip |
| **P2** | **Module format `EN!20`** — critical path | 5-7 wk | Same `.nl` from 32/64-bit × Linux/Windows; `EN!10` converter passes `src/std` |
| **P3** | LLVM IR backend (deletes ~4,640 LOC) | 10-14 wk | Native binaries on 3 OS × 2 arch; `lldb` line breakpoints work |
| **P4** | Runtime rewrite, replacing `src/asm/` | 8-12 wk | Full stdlib + non-GUI examples on all 6 targets |
| **P5** | Threading & GC (parallel with P3/P4) | 6-8 wk | N-thread alloc stress clean under TSan for 10 min |
| **P6** | LSP/DAP, CI matrix, `sg`/`api2html` | 4-6 wk | Green CI on 6 targets |
| **P7** | Native GUI on Qt 6 — **optional** | 16-24 wk | IDE edits/builds/debugs on all 3 OSes |

**MVP (P0-P4 + P6) ≈ 48 weeks. With the IDE ≈ 72.**

### The independent quick win

Sequenced outside this chain, because it depends on none of it:

> **~12 assembly routines + a ~300-line `posix'io` module gives a working console ELENA
> on Linux.** `std`, `sys`, `ext` and `gui` need **no changes at all**.

This works because **64% of the standard library is already portable**, and because the
*forward* mechanism — names beginning with `'`, resolved from the `.prj` at link time —
is already exactly the platform seam required. The entire `gui` package (1,252 lines)
touches the platform only through 38 forwards declared in `bin/templates/gui.cfg`.

That mechanism is the best piece of architecture in the tree: link-time dependency
injection, written in 2009. **Reuse it rather than inventing a new abstraction layer.**

### Deletion order for `asm2bin`

`asm2bin` cannot be deleted first, despite being a poor tool (x87 only, no 64-bit, no
SSE, no atomics, exit code always 0, and unknown mnemonics starting with
`b,e,g,h,k,q,u,v,w,y,z` are **silently reinterpreted as label declarations**).

Deleting it today kills the language: the JIT constructor loads 22 inline templates from
`elena.bin`, `preloadCoreCode` needs the allocator and `$nil`, the linker needs the entry
symbol, and ~165 `#inline`/`#external` sites in `src/*.l` would dangle.

**Order:** port subsystem-by-subsystem to C (both mechanisms coexist via `#external`) →
replace the JIT's inline templates with LLVM IR → *then* remove `elenasrc/asm2bin/`,
`src/asm/`, `engine/win32/x86helper.*` and the `[primitives]` section of `elc.cfg`.

The one capability NASM/LLVM MC lack is emitting ELENA relocation records — worth ~200
lines, not 3,335.

---

## 5. Measured facts worth keeping in view

**Bootstrap is not circular.** ELENA 1.5 is **not self-hosting**; no prebuilt binary is
needed. `sg` and `asm2binx` are plain C++; the runtime is assembly *precisely so* that it
needs no ELENA; `-lstd` breaks the `elena.nl` cycle. The tree is self-sufficient.

**Modern C++ is not a blocker.** Measured with GCC 16.1.1 and Clang 22.1.8 over 16
translation units: **25 errors at `-m64`, 3 at `-m32`** — and `-std=c++17` adds **zero**
new rejections. No `register`, no throw specs, no `auto_ptr`, no `>>` ambiguity anywhere.
The blockers are platform and word size, not language version. Go straight to a modern
standard.

**A 32-bit Linux build is very close.** Three errors, all in the audit's own shim rather
than the codebase.

**`engine/win32/` contains no Windows API.** It is *architecture*-specific, not
OS-specific. Rename it `engine/x86/` before the CMake work so the `arch × os` split is
correct from the start.

---

## 6. Live defects found during the audit

Not a complete bug hunt — these surfaced incidentally and are worth recording.

| Severity | Defect | Location |
|---|---|---|
| High | **`elc` deletes successfully-compiled `.nl` files** when any later file fails — `raiseError` always throws, `cleanUp()` removes every project output | `elc.cpp:332` |
| High | **`ioswap` is a no-op** — loads two stack slots and writes each back where it came from | `elena.asm:972`, used from `compiler.cpp:888` |
| High | **Inline write barrier `elena'34` appears to drop its result** — the out-of-line twin does `mov [esi], eax`; this one does not, and ESI is never set up. Assigning a stack-resident object to a field may leave a stack pointer in the field. **Strong candidate for `knownbugs.txt #00024`**, the intermittent GC bug open since 2009 | `elena.asm:1062` vs. inline variant |
| High | **Fixup hash always returns 0** — `(reference && ~mskAnyRef) >> 2` uses logical `&&` where `&` was meant. Clang catches it; MSVC never did | `engine/section.h:19` |
| Medium | **The last production in `syntax.txt` is silently discarded** — the final `registerRule()` is commented out | `sg.cpp:121` |
| Medium | `text.cpp` `erase` copies `used-offset` instead of `used-offset-size` — overread with overlapping `_tcsncpy`. Likely `knownbugs.txt #00022` | `ide/text.cpp:440` |
| Medium | Numeric overflow checks are dead code — `errno` tested without being cleared; out-of-range literals compile to wrong values silently | `compiler.cpp:776-802` |
| Medium | Short/near jump selection is a guess (`jumpOffset < 0x10`) that, when wrong, **inserts bytes into already-emitted machine code** | `x86helper.cpp:302` |
| Low | `-xunicode` sets the output path to the literal string `"unicode"` | `elc.cpp:197-199` |
| Low | Missing `return` in non-void functions — undefined behaviour | `dump.h:112,175`, `lists.h:1791`, `streams.h:322` |

---

## 7. Traps for the modernization work itself

Things that will silently break if approached naively:

- **`Map` is not `std::map`.** O(n) lookup, insertion-ordered, duplicates allowed,
  `exclude()` ≠ `erase()`. Substituting `std::map` "because it's equivalent" silently
  reorders config output and drops duplicates.
- **`.prj` paths are parsed by `pathToName` to build module names**
  (`altstrings.h:316-336`). You cannot simply rewrite `\` to `/` in the data files.
- **`examples/helloworld/u_helloworld.prj` is UTF-16LE** and will be destroyed by any
  repo-wide `sed`.
- **VS Release is MBCS while Debug and MinGW are Unicode** — the two `elc` builds emit
  **mutually unreadable `.nl` files**. This is presumably the source of historical
  "corrupted module" reports.
- **`-d` changes the bytecode.** `bcDebug` opcodes are only emitted when a debug module
  exists (`bccompiler.cpp:686`), so debug and release are **not the same program**.
- **`iocall(1)` skips VMT entry 0**, safe only because `$elena'object` defines `new` and
  `NEW_MESSAGE_ID = 0x80000001` is the smallest ID ever installed. Role VMTs have no
  parent, so the compiler manually reserves entry 0 with `DUMMY_MESSAGE_ID`
  (`compiler.cpp:1387`). Undocumented anywhere; breaks silently on any VMT redesign.
- **The Win32 window procedure lives in the runtime core** (`elena.asm:1335`), not in a
  library. The GUI is welded into `elena.bin`; separating it is a prerequisite for any
  headless or non-Windows build.
- **`common/` calls *up* into `engine/`** — `lists.h` uses `_writeIterator`/`_readToMap`
  from `engine/section.h:49,66`. Must be untangled before clean CMake module separation.
- **`doc/tech/bytecode.txt` is wrong.** It is a pre-1.5 design sketch: omits 12 of the 40
  live opcodes, lists two that do not exist, and misdescribes `ioswap` and `iocall`.
  Use [`../architecture/03-engine-bytecode-jit.md`](../architecture/03-engine-bytecode-jit.md)
  instead.

---

## 8. Free deletions

Work that removes code with no analysis required:

| Item | Lines |
|---|---:|
| `Queue`, `CList`, `_BList::circle/shift*` — **zero instantiations tree-wide** | ~250 |
| `ide/gtk/main.cpp` — a GTK hello-world referencing nothing | 49 |
| `bcPush 0x20` — dead opcode; plus 4 commented-out remnants | — |
| `elena'27` — does not exist (numbering gap) | — |
| Frame prologue/epilogue asm templates — LLVM emits these | 60 |

And the largest single mechanical simplification available: `ftoa`/`atof`
(`standard'36`/`'37`) are **646 lines — 10.5% of the entire runtime** — and are a direct
substitute for `snprintf`/`strtod`.

---

*Synthesized from the eight subsystem audits in this documentation set. See
[`../README.md`](../README.md) for the index.*
