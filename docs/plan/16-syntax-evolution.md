# Syntax Evolution — Proposals

> **Status: strawman for discussion.** Every syntax shown here is a sketch chosen to fit
> ELENA's existing grammar (`#directive`, `'` namespaces, `.` terminators, `[ ]` blocks,
> `{ }` record literals, `[hint]` suffixes). Names and punctuation are negotiable; the
> *semantics* are the proposal.
>
> Baseline: [`../language/10-elena-language-reference.md`](../language/10-elena-language-reference.md).

Proposals are ordered by **value ÷ risk**, best first.

---

## S1 — Failures that carry information ★ highest value

### The problem

`EAX == 0` means, indistinguishably: *message not understood*, *index out of range*,
*parse failure*, *boolean false*, *end of iteration*. A `#loop` cannot tell normal
termination from a real error — the original author documented this himself
(`doc/todo.txt:23-25`). Nothing in the language can report *why* anything failed.

For OS work this is disqualifying: a driver that fails cannot say what went wrong.

### The proposal

Let `fail:` take an argument, and let `|` bind it.

```elena
// today
#method get [ self fail. ]

// proposed
#method get [ self fail: 'io'EndOfStream. ]

// today — you learn only that something failed
aStream read | [ console writeLine:"failed". ]

// proposed — the alternative branch can bind the reason
aStream read | (e)[ console writeLine:(e describe). ]
```

`| [ ... ]` without a binder stays **exactly** as it is today. That is what makes this
nearly source-compatible: every existing `|` keeps working unchanged.

### Representation

At the ABI level, the failure channel widens from `EAX` to `{ptr, i1}`:

| Result | Meaning |
|---|---|
| `{value, true}` | success |
| `{null, false}` | plain failure — what `self fail.` produces today |
| `{reason, false}` | failure carrying a reason object |

`nil` no longer has to be a real heap object purely because the null pointer is reserved
(see [`17-llvm-backend-and-targets.md`](17-llvm-backend-and-targets.md) §4).

### Cost

Library sites that want to report a reason must be edited — but only those. Everything
else recompiles untouched. **This is the single highest value-per-effort change available
in the project.**

---

## S2 — Kill `#inline pkg'N`, introduce named typed intrinsics ★

### The problem

```elena
#method ifSame : anObject = #inline elena'8 (self, anObject).
```

`#inline pkg'N` splices assembly block number `N` from a package **verbatim** into the
caller. There are ~109 such sites. Every property of this mechanism is bad:

| Problem | Consequence |
|---|---|
| Identified by **number**, not name | `elena'27` does not exist — a gap in the numbering. Nothing detects that |
| No type information | Wrong argument types produce corruption, not a diagnostic |
| No arity checking | Same |
| Splices raw **x86** | Architecture-locked by construction. Cannot exist on ppc64/s390x/arm64 |
| Position-dependent | Renumbering the `.asm` file silently rebinds every call site |
| Invisible to the compiler | It cannot reason about, optimize or verify any of it |

`doc/knownbugs.txt` even carries an open defect against one:
*"optimization bug: `#method ifSame : anObject = #inline elena'8 (self, anObject).`
works incorrectly"*.

### The proposal

Replace with a **closed, named, typed intrinsic set** that the compiler knows about:

```elena
#method ifSame : anObject = #intrinsic core'obj'identical (self, anObject).
#method + : aValue       = #intrinsic core'i32'add       (self, aValue).
#method sqrt             = #intrinsic core'f64'sqrt      (self).
```

Each intrinsic has a fixed signature in a compiler-owned table, and lowers to exactly one
of:

| Lowering | Example | Backend |
|---|---|---|
| LLVM instruction | `core'i32'add` → `add nsw i32` | direct |
| LLVM intrinsic | `core'f64'sqrt` → `llvm.sqrt.f64` | direct |
| Runtime call | `core'gc'alloc` → `call @elena_gc_alloc` | linked |
| IR sequence | `core'obj'vmt` → typed GEP + load | direct |

**Unknown intrinsic name = compile error.** Today, a wrong `#inline` number is a silent
miscompile.

### Why this is the key enabler

> **The intrinsic table becomes the only place in the compiler that knows anything about
> the target architecture.**

That is the whole point. Everything else — frontend, bytecode, library — becomes
architecture-neutral. Adding s390x means reviewing one table, not auditing 6,146 lines of
assembly.

### Namespace sketch

| Namespace | Contents |
|---|---|
| `core'i8/i16/i32/i64/u*` | integer arithmetic, comparison, bit ops, conversions |
| `core'f32'/f64'` | float arithmetic, comparison, `sqrt/sin/cos/exp/log/fabs/trunc` |
| `core'obj'` | `vmt`, `setvmt` (for `#shift`), `identical`, `size`, `isbinary` |
| `core'gc'` | `alloc`, `allocbinary`, `writebarrier`, `safepoint` |
| `core'mem'` | `load8/16/32/64`, `store*`, `copy`, `fill` — endian-explicit variants |
| `core'ptr'` | address arithmetic (gated — see S5) |
| `core'atomic'` | `load`, `store`, `cmpxchg`, `fetchadd`, `fence` — **new; MTA depends on these** |

`core'atomic'` deserves emphasis: `asm2bin` **cannot encode `lock`, `cmpxchg` or `xadd`
at all**. Atomics do not exist in the current system at any level. They enter the language
here, for the first time.

### Gating

`#intrinsic` should be usable **only in library code** (like Rust's `core::intrinsics`),
gated by a project flag. User code reaches it through the standard library.

### Migration

Mechanical. The runtime audit produced a complete catalogue of all 198 asm sections and
109 inline sites, so `pkg'N → core'ns'name` is a table we already have the data to write.
Both mechanisms can coexist during migration: keep `#inline` working for not-yet-ported
routines, and make it a **hard error on non-x86 targets** so nothing silently
architecture-locks.

---

## S3 — Named, typed, layout-explicit structs ★

### The problem

```elena
#class MSG
[dbg:...]
{
   #field(28).       // 28 bytes. Which field is where? Read the Win32 docs.
}
```

Fields are anonymous byte ranges accessed by hand-computed offsets
(`standard'61(x, $self, 4)`). Acceptable for the 12 Win32 structs the library defines. A
memory-corruption generator for kernel structs, and **impossible to get right across four
target ABIs** where field offsets differ.

### The proposal

```elena
#struct 'posix'iovec
{
   #field iov_base : ptr.
   #field iov_len  : usize.
}

#struct 'net'header [packed, endian=big]
{
   #field version : u8.
   #field flags   : u8.
   #field length  : u16.
}
```

Layout attributes: `[packed]`, `[align=N]`, `[endian=big|little|native]`, `[c]` (follow
the target C ABI — the default for FFI).

The compiler computes offsets **per target**. `[endian=big]` makes wire formats explicit
and correct on every host — which matters directly once s390x and ppc are targets.

### Relationship to S5

`#struct` describes *layout*. It does not by itself grant the ability to place a struct at
an arbitrary address — that is S5.

---

## S4 — A real `#external` declaration

Covered in full in [`18-ffi-design.md`](18-ffi-design.md). Summary of the change:

```elena
// today — a magic string parsed by the linker
$dlls'kernel32.WriteFile

// proposed — typed, with an explicit convention
#external 'posix'write =
   { lib = "c". sym = "write". conv = cdecl.
     args = (i32, ptr, usize). ret = isize. }.
```

Platform selection needs **no new mechanism** — it reuses the existing *forwards* system,
which is the best architecture in the tree.

---

## S5 — Raw pointers, gated

### The problem

`ByteArray` can only reference memory the GC allocated. There is no address arithmetic and
no way to construct an object *at* a given address. You cannot touch a page table, a
framebuffer or an MMIO register. **This is the single hard blocker for OS work.**

### The proposal

A distinct `ptr` type that is *not* an object and is *not* traced by the GC, usable only
inside a module compiled with an `[unsafe]` attribute:

```elena
#module 'kernel'vga [unsafe]

#symbol Framebuffer = #ptr 0B8000h.

#method writeCell : index : value
[
   #intrinsic core'mem'store16 (Framebuffer + (index * 2), value).
]
```

Rules:
- `ptr` never enters a GC-traced field. The compiler enforces this.
- `ptr → object` requires an explicit, gated intrinsic.
- `[unsafe]` is per-module and recorded in the `.nl` metadata, so a build can refuse to
  link unsafe modules it did not expect.

### Why gated rather than free

ELENA's entire value proposition is a uniform object model. Raw pointers everywhere would
dissolve it. Gating keeps `std`/`sys`/`ext` provably pointer-free while letting a kernel
package exist.

---

## S6 — UTF-8 source files ★ low risk, removes a whole problem class

Sources are UTF-16LE-with-BOM or ANSI. **UTF-8 is not supported at all** — `feUTF8` is
declared at `files.h:200` and never implemented; passing it makes every read and write
return `false`.

Move to UTF-8 as the source encoding *and* the internal string representation.

Beyond the obvious tooling benefits, this matters specifically for the new targets:
**UTF-8 has no byte order.** UTF-16 does. Supporting `{32,64} × {LE,BE}` with UTF-16
internals means BOM handling and byte-swapping in four combinations; with UTF-8 the
problem does not exist. This is an independent and strong argument for the `TCHAR` → UTF-8
migration already planned as P1.

Keep reading UTF-16 sources for compatibility; stop writing them.

---

## S7 — Case sensitivity (decision required)

ELENA is case-insensitive **by accident**: the lexer lowercases every token
(`source.cpp:108`), and the config reader lowercases every key and value
(`elc.cpp:96-98`). This was never a design decision.

Consequences: `Foo` and `foo` cannot coexist; the debugger and any future LSP cannot show
the identifier as written; FFI symbol names — which **are** case-sensitive on every
platform — must bypass the lexer entirely.

| Option | Risk |
|---|---|
| Keep as-is | FFI needs an escape hatch anyway; LSP quality stays poor |
| **Preserve case, compare case-sensitively** | Breaks source that spells one identifier inconsistently |
| Preserve case, compare insensitively, warn on mismatch | Zero breakage; migration path to full sensitivity |

**Recommendation:** the third, then the second one release later. It costs nothing and
surfaces exactly how much code is actually affected.

---

## S8 — Arity greater than one (open question)

Message arity is **always exactly 1**. Multiple arguments become chains
(`w $writeAsInt32:n $writeAsLiteral:s`) or record literals (`{ from = 0. till = 9. }`).

This is elegant and I would not change it for ELENA-level code. But FFI needs to call
6-argument C functions, and forcing those through records means building a heap object per
syscall.

**Recommendation:** keep arity 1 for message sends; allow true multi-argument signatures
**only** for `#external` and `#intrinsic`, which are already not message sends. This
contains the change entirely within the FFI/intrinsic layer and leaves the language's
character intact.

---

## Summary

| # | Proposal | Value | Risk | Source-compatible? |
|---|---|---|---|---|
| S1 | Failures carry information | ★★★ | Low | Nearly — `\|[...]` unchanged |
| S2 | Named typed intrinsics replace `#inline pkg'N` | ★★★ | Low | Library-only; both coexist |
| S3 | Named typed structs | ★★ | Low | Additive |
| S4 | Real `#external` | ★★★ | Low | Additive, reuses forwards |
| S5 | Gated raw pointers | ★★ (★★★ for OS) | Medium | Additive |
| S6 | UTF-8 sources | ★★ | Low | Additive |
| S7 | Case sensitivity | ★ | Medium | Phased |
| S8 | Arity >1 for FFI only | ★ | Low | Additive |

**S1 + S2 + S4 are the ones that unblock everything else.** S2 in particular is what makes
`ppc64`/`s390x`/`ppc32` reachable at all, because it removes architecture knowledge from
the library and concentrates it in one compiler-owned table.
