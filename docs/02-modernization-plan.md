# ELENA Modernization — Plan for the 1.9.23/2.0 base

*Round 2. Round 1 (the ELENA 1.5.0.0 experiment, archived in
`experimental_version/`) proved the whole approach end to end: LLVM backend
linked in-process, C11 runtime, one-command compile→translate→link, same
source running on Linux and Windows. This plan re-applies it to the far
richer 1.9.23/2.0 base described in `01-codebase-map.md`.*

## 1. Goals (restated)

1. **No VM.** Programs are native executables; `elenavm`, `elenasm`, `elt`
   and the VM tape/`lnVMAPI_*` surface are deleted.
2. **No assembly, no e-code assembly.** `src30/asm/x32/*.asm` (11.7k lines)
   and the `.esm` bytecode-assembly layer (5.3k lines) are replaced by a
   minimal C11 runtime (GC, dispatch, object model) plus LLVM lowering.
3. **Codegen through the LLVM C++ API, in-process** — IRBuilder/Module/
   TargetMachine → `addPassesToEmitFile` → `.o` → system linker. No textual
   IR in the compile path (a `.ll` dump stays as a debug flag only).
4. **Keep the ELENA object-module pipeline**: elc continues to emit `.nl`
   (code) and `.dnl` (debug) modules per namespace; the translator consumes
   the module closure — exactly the architecture validated in round 1.
5. **Targets**: Linux/Windows/macOS × x86, x86-64, ppc32, ppc64, ppc64le,
   s390x — big- and little-endian, 32- and 64-bit, from any host
   (cross-compilation is a first-class requirement).
6. **MTA from the design up**: the 2.0 base already has `corex` (threads,
   TLS, spinlocked alloc, `snop`/`trylock`/`freelock`), so the C runtime is
   designed multithread-first even if phase 1 runs single-threaded.

## 2. Decisions carried over from round 1 (already argued, not reopened)

| Decision | Where argued (in `experimental_version/`) |
|---|---|
| LLVM linked in-process; system linker produces executables | `docs/plan/17-llvm-backend-and-targets.md`, `elenasrc/llvmgen/llvmgen.h` header |
| Runtime in C11, freestanding-capable, **static** lib, LTO-friendly | `docs/plan/19-runtime-in-c.md` |
| Failure ABI: ONE packed word, ok flag in bit 0, never unwind | `docs/plan/23-failure-abi.md` (the MS x64 ABI post-mortem) |
| Name-based linking `elena.<tag>.<name>`; `'` `:` `#` → `.` (round 1 spelled the prefix `selene.`; the scheme is identical) | `llvmgen.h:27-50` |
| Named symbols for runtime entry points, never numbers | plan 19 §3.1 |
| Message interning: one dense global id space per link unit | `elc/posix/elc.cpp:148-168` |
| UTF-8 end to end; conversion only at OS boundaries, per-platform dir | plan 19 §8.1 |
| `TargetInfo`: target properties never derive from the host | `targetinfo.{h,cpp}` — copy nearly verbatim |
| Platform selection by forwards: `targets/<os>.cfg` axis ≠ template axis | plan 17 §1.4, plan 20 |
| No `#ifdef` in shared code; per-platform source directories | `docs/architecture/22-platform-layer.md` |
| VMT load at send sites never `!invariant.load`, never hoisted | plan 17 §5.2 (2.0 keeps dynamic class mutation) |
| Pluggable heap region source (OS-development blocker) | plan 20 §7 |
| Stubs object from the runtime archive's symbol index, linked after it | `llvmgen.cpp:622-732` |
| Stack→SSA via one alloca per slot + mem2reg; exact CFG depth analysis | plan 17 §4.2, `llvmgen.cpp:1558-1601` |
| IDE not ported; LSP + DAP later | `docs/architecture/08-ide-debugger.md` |
| Explicit per-target `LLVMInitialize*` (X86, PowerPC, SystemZ, AArch64) | `llvmgen.cpp:46-73` |

Explicitly **not** carried over: the `ELENA_32BIT` build option (1.5-specific;
v2 builds 64-bit clean from day one), the 1.5 opcode translation switch, the
`.sem` extension experiment (v2 keeps `.nl`/`.dnl`), `rcallemb` handling.

## 3. What changes relative to round 1

The 2.0 base is richer in exactly the places round 1 was thin:

1. **Register machine.** e-code has A/B/D/E registers plus stack. In the
   translator these become four mutable SSA slots (allocas, mem2reg'd) next
   to the per-slot stack array; the depth analysis from round 1 still
   governs stack merges, registers merge trivially.
2. **~150 opcodes** (vs 64), including int/long/real arithmetic, typed
   array access, and frame/exception/threading ops. Bigger translation
   switch; same decode-once discipline. The operand-order reversal quirk
   for `ifr`/`ifn`/`ifm`/`elsem`/`elser`/`elsen`/`lessn` must live in ONE
   decoder.
3. **Exceptions exist** (`hook`/`throw`/`unhook` + `coreapi` unwinding).
   Lowering: setjmp-style runtime chain in C (no native unwind tables) —
   consistent with the never-unwind failure decision; `hook` pushes a
   handler frame, `throw` longjmps to it. Design note needed before P2.
4. **The primitive layer is e-code, not magic.** 1.5 hid primitives in
   `rcallemb` blobs; 2.0's are honest `.esm` procedures spliced via
   `system'internal'*`. Migration: each of the 252 `core_routines`
   procedures becomes either (a) a C function in the runtime with its
   mangled name (round 1's `SELENE_NATIVE` `__asm__`-symbol trick, renamed
   `ELENA_NATIVE`), or (b) a translator intrinsic
   (the hot arithmetic/copy ones). The `'$import` splice mechanism in the
   compiler is retired; `=> system'internal'X` compiles to a direct call.
5. **FFI is half-done already**: `system'external'ALIAS Func &subject:arg`
   has typed arguments by subject. v2 keeps the surface syntax initially,
   adds typed returns + per-target marshalling through LLVM (plan 18's
   lattice), and replaces `[winapi]`-implies-stdcall with per-declaration
   convention data.
6. **MTA is designed in**: `corex.asm` semantics (TLS state, safepoint
   `snop`, object locks) map to C11 `<stdatomic.h>` + per-thread allocation
   buffers + the shadow-stack plan.
7. **GC has a real spec now**: the `%GC_ALLOC` assembly is a complete
   generational collector (card table, shadow YG, mark-compact MG). The C
   collector reimplements *that design* precisely — round 1 never got past
   the bump allocator.

## 4. Module format v2

The 1.9 container is kept structurally (stamp, name, 3 hash maps, sections
with relocation tables) but re-serialized:

- New magic with an explicit version and property record: byte order,
  reference width, char encoding. A mismatch is a *rejection*, never a
  reinterpretation.
- All scalars explicitly sized and **little-endian on disk** regardless of
  host/target; `ClassHeader`/`VMTEntry`/`DebugLineInfo` written
  field-by-field, never `sizeof()`-dumped.
- `ref_t` stays a 32-bit *file* quantity (24-bit id + 8-bit mask is plenty)
  even when the in-memory type is wider — the mask stops living in
  pointer-sized words.
- `ecv` is ported first and used as the differential oracle
  (old reader vs new writer on the same logical module).

## 5. Phases

- **P0 — Toolchain builds on Linux x86-64** (CMake; this session):
  `common` + `engine` (minus JIT) + `elc` front end + `sg` + `og` + `ecv` +
  `asm2bin` compile as C++17 on GCC/Clang 64-bit. Mechanical fixes only
  (headers, casts, `ident_t`-in-int smuggling gets a proper fix); the
  hand-written PE/ELF writers and the JIT are excluded from the build
  rather than ported. IDE/GUI/elenavm/elenasm/elt are not built.
- **P1 — Module format v2 + 64-bit-clean serialization.** Fix hazard
  classes 1–4 of `01-codebase-map.md` §10. `elc -c<prj>` produces `.nl`
  modules on Linux 64-bit; `ecv` round-trips them.
- **P2 — llvmgen + runtime.** New `elenasrc2/llvmgen/` (same opaque-impl
  architecture as round 1) translating the module closure; new `runtime/`
  in C11 (object model, dispatch, allocator, exception chain, natives);
  `core_routines` migration (§3.4); helloworld runs natively on x64 Linux.
- **P3 — Standard library bring-up.** `system` + `extensions` compile from
  the `.project` (portable) subsets; POSIX console/file bindings written
  properly (no dlsym-stdout, no putchar loops); Windows via mingw cross +
  Wine validation, as in round 1.
- **P4 — Typed FFI** (plan 18 adapted): typed returns, struct marshalling,
  callback trampolines; syscall layer becomes declarations.
- **P5 — MTA + real GC**: shadow stack, safepoints, per-thread nurseries,
  the generational design from `core.asm` in C; `#lock`/`trylock` on
  `<stdatomic.h>`.
- **P6 — Target matrix**: ppc64le first (LE 64-bit sanity), then s390x
  (big-endian proof), ppc32/ppc64 (ELFv1 function descriptors), x86-32.

## 6. Deletion list (with ordering constraints)

Delete together, in this order:
1. `tools/elt` → 2. `elenasrc2/elenasm` (link-depends on elenavm) →
3. `elenasrc2/elenavm` + VM tape constants + `lnVMAPI_*` + `core_vm.asm` +
   `vm_console` templates + `#config vm_console` in `bin/scripts` →
4. `engine/x86jitcompiler.*`, `engine/x86helper.*`, asm2bin's x86 half,
   `bin/x32/*.bin`, `src30/asm/x32/*.asm` — **simultaneously with** the
   llvmgen replacement landing in elc (they share `JITLinker`) →
5. `elc/win32/linker.cpp`, `elc/linux32/linker.cpp` (PE/ELF writers),
   `winstub.ex_` →
6. `.esm` files + asm2bin entirely + the `'$import` splice path, once the
   runtime-in-C migration (P2) completes →
7. `elenart` DLL (its reflection contract re-emerges as emitted tables +
   runtime C), `tools/og` + `rules.dat` (once LLVM owns optimization),
   `ide/` + `gui/`.

`ext_routines.esm`'s `evaluate`/`load` (elenasm bridge) and
`extensions/dynamic/interpreter.l` are orphaned by step 2 and removed with
it.

## 7. Risks / open questions

- **Exception lowering** (§3.3) needs a short design doc before P2 —
  interaction with the failure ABI and with GC frames.
- **`bsredirect`/generic handlers**: 2.0's dispatch fallback chain
  (`handle_generic`, `handle_extension`, `handle_group`…) is richer than
  1.5's; the C dispatcher must reproduce it exactly. The `.esm` bodies are
  the spec.
- **`syntax.dat`/`rules.dat` portability**: raw memory images with no
  magic/version (inherited defect). Add a header when `sg` is ported (P0).
- **Front-end 64-bit smuggling** (`ident_t` through 32-bit tree args) is a
  correctness blocker for P0/P1, not a warning.
- **Windows console**: keep round 1's decision (UTF-8 code pages, bytes
  unchanged, baseline Win10 1903) and drop the `WriteConsoleW`/UTF-16
  paths of the 1.9 library during P3.
