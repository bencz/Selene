# ELENA (modernization)

A modernization of **ELENA Language 1.9.23 / 2.0** (2015, Alex Rakov,
Apache 2.0) — a pure message-passing object-oriented language in the
Smalltalk lineage, at its most mature pre-rewrite state: register-based
e-code, generational GC, threading variant, script engine, ~150 opcodes.
The project keeps the name **ELENA**; "Selene" was only the codename of
round 1 (and survives in the repo directory name).

**Round 1** — the same modernization applied to ELENA 1.5.0.0 (2009) —
was completed as an experiment and lives whole in `experimental_version/`
(LLVM backend, C11 runtime, CMake, Linux+Windows, FFI). It is reference
material: its `docs/plan/*` argue the design decisions round 2 inherits.

Round 2 rebuilds the 1.9.23 implementation the same way: the VM
(`elenavm`), the x86 JIT, the hand-written PE32/ELF32 writers, the x86
assembly runtime (`src30/asm/x32/`) and the e-code assembly layer (`.esm`)
are all **deleted**; codegen goes through the **LLVM C++ API in-process**
(no textual IR in the compile path) and the system linker; the runtime
(GC, dispatch, object model — MTA-first) is **C11**. elc keeps emitting
ELENA object modules (`.nl`/`.dnl`) per namespace; the translator consumes
the module closure.

> The upstream project (`elena-lang`) is still actively developed by its
> author. This is a divergent rewrite of the implementation, not a
> continuation. Original copyright headers must be preserved.

---

## Where things are

| Path | Contents |
|---|---|
| `docs/` | **Read first.** `01-codebase-map.md` (the system as it is), `02-modernization-plan.md` (goals, phases P0–P6, deletion order) |
| `elenasrc2/` | The 2015 C++ toolchain: `common/`, `engine/`, `elc/`, `elenavm/`, `elenasm/`, `elenart/`, `tools/{asm2bin,ecv,elt,og,sg}`, `ide/`+`gui/` (not ported) |
| `src30/` | Standard library in ELENA (`system`, `extensions`, `forms`, `net`, `sqlite`), `asm/` (esm + x86 — dies), `cpuvm/` (sample program) |
| `dat/sg/`, `dat/og/` | Grammar (→ `syntax.dat`) and peephole rules (→ `rules.dat`) |
| `bin/` | `elc.cfg`/`elc.config`, `templates/*.cfg` link templates, `scripts/*.es`, prebuilt `x32/*.bin` |
| `doc/` | Original author docs (partly stale; `docs/` supersedes) |
| `experimental_version/` | Round 1, complete and frozen. Pull code/docs from here when useful |

## Build (current state — P0+P1 done)

```bash
cmake -S . -B build-64
cmake --build build-64 -j        # elc, sg, og, ecv, asm2bin + staged
                                 # syntax.dat/rules.dat/elc.config

mkdir -p build-64/lib30/system   # the e-code primitive modules (die in P2)
build-64/bin/asm2bin src30/asm/core_routines.esm build-64/lib30/system
build-64/bin/asm2bin src30/asm/ext_routines.esm  build-64/lib30/system

build-64/bin/elc -csrc30/system/system.project \
   -o$PWD/build-64/lib30 -p$PWD/build-64/lib30
build-64/bin/elc -csrc30/extensions/extensions.project \
   -o$PWD/build-64/lib30 -p$PWD/build-64/lib30
```

Modules carry the signature `ELENA.2.001` (64-bit-self-consistent container;
2015 modules are rejected as wrong version). The VM, script engine and IDE
are not built. **LLVM (llvm-devel) is REQUIRED** — llvmgen is linked into
elc (`elc --llvm-selftest` emits an object for all 12 targets, including
big-endian).

**The first native program runs.** The e-code→LLVM translator emits and
verifies 99.3% of the 2,629-procedure library corpus
(`elc --llvm-translate <mod.nl>` measures it), and:

```bash
build-64/bin/elc -cexamples/helloworld/helloworld.prj -p$PWD/build-64/lib30
build-64/bin/elc --llvm-build examples/helloworld/helloworld.nl \
   "helloworld'program" /tmp/hello
/tmp/hello        # -> Hello World from ELENA!
```

`--llvm-build` translates the module, materializes value constants
([size][vmt][payload] images), emits the `elena_program` entry thunk, turns
undefined `elena.*` into loud stubs, and links with `cc` against
`build-64/libelena_rt.a` (runtime/elena_runtime.c — bump allocator, hook
chain, harness). Pending, in order: VMT data emission + real dispatch in C,
forward resolution at translate time + `targets/<os>.cfg` (round-1 seam),
message interning across the closure, coreapi surface, library console via
typed FFI (P4).

`.prj` is THE project format on both platforms (the `.project` duplicates
were the experimental Linux variant and will be absorbed when the stdlib is
restructured in P3); path separators are canonized at ingestion
(`Project::loadSourceCategory`), so `src30/system/system.prj` compiles on
Linux, matching the 2015 elc.exe warning-for-warning (validated under Wine).

## Invariants — do not violate these

**1. No platform conditionals in shared code.** Use per-platform source
directories; the build system selects the file. See
`experimental_version/docs/architecture/22-platform-layer.md`.

**2. UTF-8 is the character model — compiler AND runtime.** Literal payload
`[u32 byte length][UTF-8 bytes][NUL]`. Conversion only at an OS boundary,
inside that platform's directory. Never assume a character width.

**3. Target properties never derive from the host.** Widths, byte order,
alignment come from `TargetInfo` (port from
`experimental_version/elenasrc/elc/targetinfo.*`). Cross-compilation must
be byte-identical from any host. Sole exception: `getDefaultTarget()`.

**4. Serialization is explicit.** Module format v2: fixed-width scalars,
little-endian on disk, versioned magic that records its properties; never
`sizeof()`-dump a struct, never read 4 bytes into a `size_t`. A format
mismatch is a rejection, not a reinterpretation.

**5. The failure result is ONE packed word** — value with the ok flag in
bit 0. Never a two-field struct (Microsoft x64 ABI breaks it silently),
never unwinding. See `experimental_version/docs/plan/23-failure-abi.md`.

**6. The VMT load at a send site must never be `!invariant.load` or
hoisted.** ELENA mutates a live object's class by design.

**7. Runtime entry points are named symbols, never numbers.** A missing
name is a link error; a wrong number is a silent wrong call.

**8. `.cfg`/`.prj`/`.project` path strings feed `pathToName()` to derive
module names.** Separators are normalized only where a path becomes a
syscall.

## Design decisions already taken (do not reopen)

LLVM in-process + system linker; runtime in C11 (static, LTO,
freestanding-capable); name-based linking `elena.<tag>.<name>` with
`'` `:` `#` → `.`; message interning into one global id space per link
unit; forwards (`targets/<os>.cfg`) as the platform seam; IDE replaced by
LSP+DAP later. Argued in `experimental_version/docs/plan/{17,18,19,20,23}`.

## Where the project is going (phases, see docs/02 §5)

- **P0** toolchain compiles on Linux x86-64 (CMake) — in progress
- **P1** module format v2; 64-bit-clean `.nl` emission; `ecv` as oracle
- **P2** `llvmgen` (LLVM C++ API) + C11 runtime; the 252 `core_routines`
  esm procedures become C functions / intrinsics; helloworld runs native
- **P3** stdlib bring-up (POSIX first, then mingw+Wine for Windows)
- **P4** typed FFI (returns, structs, callbacks; plan 18 adapted)
- **P5** MTA + the real generational GC (the `core.asm` design, in C)
- **P6** target matrix: ppc64le, s390x (big-endian), ppc32/ppc64, x86-32

## Working notes

- Reply to the user in **Portuguese**; write code, comments and
  documentation in **English**.
- `src30/cpuvm` is a sample program, not toolchain.
- The two READMEs at the root describe the same package (1.9.23 ≈ "2.0").
- Known 2015-era latent bugs are listed at the end of
  `docs/01-codebase-map.md` §10.
