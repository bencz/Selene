# FFI Design — Foreign Function Interface

> **Status: design proposal for discussion.**
>
> Must support Linux / Windows / macOS across x86-64, arm64, ppc64 (BE), ppc64le, ppc32
> and s390x. Every one of those has a different C calling convention.

---

## 1. What exists today

```
$dlls'kernel32.WriteFile
```

A magic string in the pseudo-namespace `$dlls`, split at the last `.` by the linker
(`linker.cpp:270`) into DLL name and function name, then emitted into a hand-built PE
import table. Calls go through a GC-safe frame provided by `#external`.

| Property | Status |
|---|---|
| Type information | **None.** Arguments are whatever happens to be on the stack |
| Calling convention | Implicitly `stdcall`; not expressible |
| Return type | Untyped machine word |
| Platform selection | None — the string names a Windows DLL directly |
| Struct arguments | Not expressible |
| Callbacks | One hand-written `WndProc` thunk (`elena.asm:1335`), in the runtime core |
| Error reporting | None |

It is not an FFI. It is a hard-coded Win32 import mechanism.

---

## 2. Design goals

1. **Typed** — the compiler knows argument and return types, and checks them.
2. **Per-target correct** — LLVM emits the right convention for each of six ABIs.
3. **Platform-selected without new machinery** — reuse the existing *forwards* system.
4. **GC-safe** — a foreign call must be a safepoint, which MTA requires.
5. **Bidirectional** — native code must be able to call *into* ELENA. This is the
   prerequisite for threads, signal handlers, ISRs and callbacks alike.
6. **No assembly.** Every thunk generated as LLVM IR.

---

## 3. Declaration syntax

**Phase 1 — implemented.** Declarations live in the language, riding the hint
grammar that already exists (`[identifier:value, ...]` on `#define`), so no
parser change was needed:

```elena
#define[external, lib:c, sym:write, args:i32'str'usize, ret:isize] write = c'write.
#define[external, lib:c, sym:exit, args:i32]                       exit  = c'exit.
```

The apostrophes of the `args` reference stand in for commas, which the hint
grammar reserves as its separator. The declaration compiles to MODULE
METADATA — a native data section under the same `$package` name the
`#external` call sites map — and the link step turns every call into a
typed, marshalled call to the C symbol (`compileExternalDeclaration` in
`compiler.cpp`; `collectForeignDeclarations` + `foreignWrapper` in the link
pipeline). Types supported today: the integer lattice, `isize`/`usize`
(widened from the 32-bit payload), `f64`, `ptr` (the object's payload
address), `str` (UTF-8 bytes past the literal's length word). Return values
are discarded until fail: carries an argument (§8); `conv` is the target's C
convention; `lib` is informational under static linking.

**Eventually** — once the syntax evolution lands a block form — the richer
spelling below can replace the hints; the metadata and the wrapper machinery
stay the same:

```elena
#external 'posix'write =
   { lib = "c". sym = "write". conv = cdecl.
     args = (i32, ptr, usize). ret = isize. }.

#external 'win32'WriteFile =
   { lib = "kernel32". sym = "WriteFile". conv = stdcall.
     args = (ptr, ptr, u32, ptr, ptr). ret = i32. errno = lasterror. }.
```

| Key | Meaning |
|---|---|
| `lib` | Logical library name — resolved per platform (§6) |
| `sym` | Exact, **case-sensitive** symbol name (see `16-syntax-evolution.md` §S7) |
| `conv` | `cdecl` `stdcall` `fastcall` `sysv` `win64` `aapcs64` — or `default`, meaning the target's C convention |
| `args` | Native types, positional. Arity > 1 is allowed **here only** (§S8) |
| `ret` | Native return type, or `void` |
| `errno` | Error retrieval strategy: `none` `errno` `lasterror` |
| `variadic` | Optional; fixed-argument prefix count |

`conv = default` should be the norm. Explicit conventions exist for Win32 x86, where
`stdcall` is not the default, and for anything unusual.

---

## 4. Native type system

FFI types are **not ELENA objects**. They are a separate, explicitly-sized lattice:

| Category | Types |
|---|---|
| Integers | `i8 i16 i32 i64 u8 u16 u32 u64` |
| Target-width | `isize usize ptr` |
| Floats | `f32 f64` |
| Aggregate | `#struct` names (see `16-syntax-evolution.md` §S3) |
| Nothing | `void` |

> **Never `int`, never `long`, never `char`.** Those vary by platform and are exactly how
> FFI bugs happen. `long` is 64-bit on Linux/LP64 and 32-bit on Windows/LLP64 — a
> distinction the current codebase already gets wrong internally.

### Marshalling

| ELENA | Native | Notes |
|---|---|---|
| `Integer` | `i32` | Unboxed at the boundary |
| `LongInteger` | `i64` | |
| `RealNumber` | `f64` | |
| `Literal` | `ptr` | **Requires an explicit encoding**: `utf8`, `utf16`, or `ansi`. No implicit conversion |
| `ByteArray` | `ptr` | Pinned for the duration of the call (§5) |
| `#struct` | by value or by pointer | Per the target C ABI; LLVM decides |
| `nil` | null `ptr` | |

String encoding must be explicit at the declaration. Guessing is how the current code ends
up locale-dependent (`CP_ACP` in `win32/unicode.h`).

### Struct support, staged

| Case | Status |
|---|---|
| Struct **by pointer**, in or out (`GetSystemTime(&st)`, `uname(&u)`, `sockaddr`) | **Works today**: `ptr` passes the object's payload address, `str` a literal's bytes — the callee reads or fills it in place (`examples/ffi-struct`) |
| Struct **by value** as an argument | Needs `#struct` descriptions. NOT automatic in LLVM IR: the C ABI classification of aggregates (SysV register classes, ppc64/s390x rules) is a frontend duty, so the wrapper must lower fields per target exactly as clang would — this is §11.2's conformance-test work |
| Function **returns struct by value** (sret) | Same machinery as by-value arguments — the hidden-pointer protocol per target |
| Function **returns a pointer** (to a struct or anything) | Needs return-value BINDING at the source level, which arrives with fail: carrying a value (§8, plan 16 S1); the pointer then travels boxed as a handle object |

---

## 5. GC safety — the part that matters for MTA

A foreign call may block for an arbitrary time. Under MTA, other threads must be able to
collect while it does. Every external call therefore becomes a **GC state transition**:

```
  publish frame (shadow stack)      ; roots become visible
  pin any buffers passed by pointer
  transition: managed → native      ; this thread no longer participates in GC
  call @foreign(...)
  transition: native → managed      ; blocks if a collection is in progress
  unpin
```

Consequences worth stating plainly:

- A foreign call is a **safepoint**. Today the system has **exactly one** safepoint, inside
  the allocator — so this alone multiplies the number of places a collection can happen.
- **Buffers passed by pointer must be pinned**, because the collector *compacts*. Handing
  `write(2)` a pointer into a movable heap and then collecting is a data-corruption bug
  waiting to happen. The current runtime gets away with it only because collection cannot
  occur during a foreign call in a single-threaded program.
- The transition is ~2 atomic stores per call. Negligible against a syscall; measurable
  against a trivial function. Offer `conv = leaf` for short, non-blocking, non-callback
  foreign functions that skip the transition.

---

## 6. Platform selection — reuse the forwards mechanism

**No new machinery is required.** ELENA already resolves names beginning with `'` from the
project's `[forwards]` table at link time. That mechanism — link-time dependency injection
written in 2009 — is the best architecture in the tree, and the entire `gui` package (1,252
lines) already reaches the platform through 38 of them.

```ini
# posix.cfg
'io'write      = posix'write
'io'open       = posix'open

# win32.cfg
'io'write      = win32'WriteFile
'io'open       = win32'CreateFileW
```

Library code calls `'io'write`. The `.prj` decides which platform module it resolves to.
Portable code stays portable, and platform code lives in named, separate modules —
`posix'io` alongside `win32'io`.

### Logical library names

| Logical | Linux | macOS | Windows |
|---|---|---|---|
| `"c"` | `libc.so.6` | `libSystem.B.dylib` | `msvcrt.dll` / UCRT |
| `"m"` | `libm.so.6` | (in libSystem) | (in CRT) |
| `"pthread"` | `libpthread.so.0` | (in libSystem) | — |

A small target-owned table maps logical → concrete. Anything platform-specific
(`kernel32`) is named directly in a platform module and never appears in portable code.

---

## 7. Callbacks — one mechanism, three payoffs

Native code calling **into** ELENA currently has exactly one path: a hand-written `WndProc`
thunk living in the runtime core (`elena.asm:1335`). Generalizing it is, per the language
audit, blocker 4 for OS work — and it is also a prerequisite for MTA.

```elena
#external'callback 'gui'WindowProc =
   { conv = stdcall. args = (ptr, u32, usize, isize). ret = isize. }.

#symbol MyHandler = 'gui'WindowProc :: [ ... ELENA code ... ].
```

The compiler generates, **as LLVM IR**, a trampoline that:

1. **Attaches the calling thread** to the ELENA runtime if it is not already attached —
   this is what makes callbacks from OS-created threads work at all, and it is the same
   machinery MTA needs.
2. Transitions native → managed (blocking if a collection is in progress).
3. Establishes a fresh GC frame on that thread's shadow stack.
4. Marshals native arguments to ELENA objects.
5. Sends the message.
6. Marshals the result back; converts an ELENA *failure* into the convention's error value.
7. Transitions managed → native.

> Build this once and you get **GUI callbacks, thread entry points, signal handlers,
> syscall handlers and interrupt service routines** — all of them are "native code enters
> ELENA on a thread the runtime may not know about".

That is why blocker 4 is worth more than its size suggests.

---

## 8. Errors

Foreign functions report errors three different ways, and the declaration must say which:

| `errno =` | Behaviour |
|---|---|
| `none` | Return value only |
| `errno` | Read thread-local `errno` after the call, **before** anything else can clobber it |
| `lasterror` | `GetLastError()` on Windows, same timing constraint |

The read must happen inside the generated wrapper, immediately after the call and before
the managed transition — otherwise the runtime's own activity destroys it. This is a
classic FFI bug and worth getting right by construction.

Surface it through the tagged-failure mechanism (S1):

```elena
'io'write:fd :buffer :count | (e)[ console writeLine:(e describe). ]
```

This is a concrete example of why S1 and the FFI design are coupled: without failures that
carry information, an FFI layer cannot report `EAGAIN` versus `EBADF`.

---

## 9. Variadics

`printf`-style variadic calls have target-specific rules (arm64 on macOS differs from arm64
on Linux; ppc64 differs again). LLVM handles them **if** the fixed-argument count is
declared:

```elena
#external 'c'printf =
   { lib = "c". sym = "printf". conv = cdecl.
     args = (ptr). variadic = 1. ret = i32. }.
```

**Recommendation:** support them, but keep them out of the standard library. Variadic FFI
is the least portable construct available and there is almost always a non-variadic
alternative.

---

## 10. What this replaces

| Today | Becomes |
|---|---|
| `$dlls'name.func` magic strings | Typed `#external` declarations |
| PE import table built by hand (`linker.cpp:326`) | Normal undefined symbols resolved by `lld` |
| `win32.asm` + `winsock.asm` (1,785 lines of syscall shims) | ELENA `#external` declarations in `win32'*` modules |
| The `WndProc` thunk welded into `elena.bin` | Compiler-generated trampolines, in a library |
| Implicit `stdcall` | Explicit per-declaration convention |
| Locale-dependent `CP_ACP` string conversion | Explicit `utf8`/`utf16`/`ansi` marshalling |

**~1,785 lines of hand-written assembly become ELENA declarations.** This is the single
largest chunk of `src/asm/` that disappears without needing a C reimplementation — the
syscall layer stops being code and becomes metadata.

That is also what makes the "cheap first win" from
[`15-modernization-roadmap.md`](15-modernization-roadmap.md) §4 real: a `posix'io` module
is mostly `#external` declarations, not implementation.

---

## 11. Open questions

1. **Should `#external` be callable from user code, or library-only?** Gating it (like
   `#intrinsic`) keeps unsafe surface contained, but makes ELENA less useful as a general
   language. My inclination: allow it, but require the module to be marked `[unsafe]`.
2. **Struct-by-value on ppc64 ELFv1 and s390x** is genuinely intricate. LLVM implements it
   correctly, but only if the `#struct` type is described precisely. Worth a dedicated
   conformance test suite per target early.
3. **Do we need `dlopen`-style dynamic loading**, or is link-time binding sufficient?
   Dynamic loading would serve the "modifiable at run time" goal in `doc/roadmap.txt`, but
   it is a larger surface.
4. **Pinning granularity** — per-call pinning is simple but limits the collector. An
   explicit `#pinned` region would be faster for I/O-heavy code, at the cost of a new
   language construct.
