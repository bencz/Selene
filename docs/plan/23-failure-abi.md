# Decision: the Failure ABI

> **Status: decided.** `{ptr, i1}` aggregate returns.
>
> This is the calling convention of every generated call. It cannot be changed
> later without regenerating everything, so it is settled before the backend is
> written.

---

## 1. What failure is in Selene

A message send either succeeds with a value or **fails**. Failure is not an error
mechanism — it is *the* conditional mechanism. `#if`, `#loop` and the alternative
operator `|` are all built on it. A comparison that is false does not return
`false`; it fails.

Today that is expressed as `EAX == 0`, and every call site is followed by
`test eax,eax; jz <target>`. The bytecode already carries this: every call opcode
has a second argument naming the branch to take on failure.

```
000017  C7  iocall1      8           41      <- message 8, on failure jump to 41
000026  43  rcall        1107296261  26
```

## 2. Measurement

Taken from the compiled standard library, 28 modules:

| | |
|---|---:|
| Methods containing calls | 486 |
| Call sites | 955 |
| Calls per method, mean | 2.0 |
| Distinct failure targets per method, mean | 1.94 |
| **Methods with exactly one failure target** | **307 (63%)** |
| Longest tail | one method with 20 distinct targets |

Call sites by opcode:

| Opcode | Count |
|---|---:|
| `iocall1` | 384 |
| `rcall` | 270 |
| `iocall0` | 173 |
| **`rcallemb`** | **99** |
| `rcallext` | 19 |
| `ircall0` / `ircall1` | 10 |

## 3. The options

### 3.1 `invoke` + landing pads — rejected

The natural reading of "every call has a failure edge" is LLVM's `invoke`. It is
the wrong instrument, for one decisive reason:

> **Failure is not exceptional in Selene. It is the primary conditional.**

`#if x > 5` compiles to a call that fails when the comparison does not hold. An
unwind per false comparison is not a performance concern, it is a category error:
unwinding costs orders of magnitude more than a branch, and 63% of methods in the
library would pay it on their ordinary path.

It also drags in the personality routine, EH tables and the unwinder itself —
across six targets, three of them (ppc64 ELFv1, s390x, ppc32) with their own
unwind ABIs — for a language that has no exceptions.

### 3.2 Sentinel pointer — rejected

Keep the current scheme: a reserved pointer value means failure. Cheapest
possible representation, and it is what exists.

Rejected because it cannot carry a reason. Widening it later would change the ABI
of every call in the system at once, which is exactly what this decision exists to
avoid. It also permanently reserves one pointer value at the language level for a
purpose the language does not otherwise need.

### 3.3 `{ptr, i1}` aggregate — chosen

```llvm
%selene.result = type { ptr, i1 }
```

- Register-allocated into two registers by every target ABI under consideration.
  No memory traffic, no unwinder, no EH tables.
- The branch is `extractvalue` + `br i1`, which is what the x86 back end already
  emits by hand today. Cost parity with the 2009 code.
- With 63% of methods having a single failure target, LLVM's own `simplifycfg`
  merges the branches without help.
- **It accommodates the language change without a second ABI break** — see §5.

## 4. The convention

Every Selene method compiles to:

```llvm
define %selene.result @method(ptr %self, ptr %arg) {
entry:
  %r  = call %selene.result @callee(ptr %recv, ptr %param)
  %ok = extractvalue %selene.result %r, 1
  br i1 %ok, label %continue, label %onfail

continue:
  %v  = extractvalue %selene.result %r, 0
  ...
}
```

| Field | On success | On failure |
|---|---|---|
| `ptr` (field 0) | the result object | `null`, or the reason object once §5 lands |
| `i1` (field 1) | `true` | `false` |

Arity stays at one argument, matching the language. `self` is an explicit first
parameter rather than a pinned register, so LLVM allocates it per target ABI
instead of the code generator hard-coding EDI.

## 5. Why this survives the language change

[`16-syntax-evolution.md`](16-syntax-evolution.md) §S1 proposes letting `fail:`
carry a reason and `|` bind it:

```elena
aStream read | (e)[ console writeLine:(e describe). ]
```

Under `{ptr, i1}` that needs **no representation change at all**. The pointer
already exists and is unused on the failure path; it simply starts carrying the
reason instead of `null`. Existing `| [ ... ]` without a binder ignores it, as it
does today.

So the ABI decision and the language decision are genuinely independent, and can
land in either order. That property is most of why this option was chosen.

## 6. What still has to be decided elsewhere

- **`nil` stays a real heap object.** It has a VMT and answers messages
  (`$elena'$nil` in the root module has symbol code, class code, a VMT and meta
  data). `{ptr, i1}` frees the null pointer from meaning "failure", but it does
  not make `nil` representable as null.
- **`rcallemb` is unaffected by this decision but blocks the backend.** 99 sites
  paste raw assembly mid-function; LLVM has no equivalent. They become named
  intrinsics ([`16-syntax-evolution.md`](16-syntax-evolution.md) §S2). That work
  is independent of the failure ABI and larger.
- **`[nogc]` interacts with §5.** A failure carrying a reason object allocates.
  Code marked `[nogc]` — interrupt handlers, the allocator, anything holding a
  spinlock — is therefore restricted to plain failures.
  See [`20-os-development.md`](20-os-development.md) §3.

## 7. Consequence for the existing bytecode

None. The bytecode already encodes a failure target per call, which is precisely
the information the translator needs to emit the `br i1`. Nothing about the
format changes; only its interpretation by the new backend.

That is the second reason this option was chosen: it is what the 2009 design was
already describing, expressed in a form LLVM can consume.
