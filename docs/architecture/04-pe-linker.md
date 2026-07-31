# The PE Linker (`elenasrc/elc/win32/linker.cpp`)

> 637 lines + 128-line header. The final stage of `elc`: takes the in-memory image
> produced by the JIT linker and writes a Win32 PE executable.
>
> **This is the single most replaceable component in the toolchain.** Under an LLVM
> backend it disappears entirely, replaced by the system linker (`lld`, `ld`, `link.exe`).

Related: [`03-engine-bytecode-jit.md`](03-engine-bytecode-jit.md) (the JIT linker that
feeds this), [`../porting/14-platform-dependency-audit.md`](../porting/14-platform-dependency-audit.md).

---

## 1. Position in the pipeline

```
elc → bytecode (.nl modules)
        │
        ▼
  JITLinker + x86JITCompiler        ← engine/, produces native code into Section objects
        │
        ▼
  Linker::run()                     ← linker.cpp:619, THIS document
        │
        ├─ createImage()            resolve everything, JIT-compile reachable code
        ├─ createImportTable()      build the PE import directory
        ├─ mapImage()               assign RVAs to sections
        ├─ fixImage()               apply all relocations
        ├─ createExecutable()       write the .exe
        └─ createDebugFile()        write the .dn debug sidecar (optional)
```

`Linker` implements the `_LoaderHelper` interface (`linker.h:20`), which is how the
platform-independent JIT linker in `engine/` calls back into platform-specific code.
**That interface is the seam along which a future ELF/Mach-O backend would be added** —
and it is genuinely a clean seam, one of the better design decisions in the codebase.

## 2. Image layout

Four PE sections, created on demand (`linker.cpp:25-29`):

| Section | Const | Contents | Characteristics | Source mask |
|---|---|---|---|---|
| `.text` | `TEXT_SECTION` | JIT-compiled native x86 code | `CNT_CODE \| MEM_EXECUTE \| MEM_READ` | `mskCodeRef` |
| `.data` | `DATA_SECTION` | VMTs, constants, GC table | `CNT_INITIALIZED_DATA \| MEM_READ \| MEM_WRITE` | `mskDataRef` |
| `.bss` | `BSS_SECTION` | Static (uninitialized) symbol storage | `CNT_UNINITIALIZED_DATA \| MEM_READ \| MEM_WRITE` | `mskStaticRef` |
| `.import` | `IMPORT_SECTION` | PE import directory + thunks | `CNT_INITIALIZED_DATA \| MEM_SHARED \| MEM_READ \| MEM_WRITE` | — |

A fifth pseudo-section, `.debug`, is **not** written into the executable — it goes to a
separate `.dn` file (`linker.cpp:589`).

The `.data` section always begins with the literal signature `"ELENA.150"`
(`ELENA_SIGNITURE`, `elenaconst.h:148`), 4-byte aligned — written at `linker.cpp:55`.
This is a version stamp embedded in every produced executable.

### Address assignment — `mapImage()` (`linker.cpp:315`)

```cpp
_codeBase   = 0x1000;                                              // hard-coded
_dataBase   = align(_codeBase   + sizeof(.text),   alignment);
_bssBase    = align(_dataBase   + sizeof(.data),   alignment);
_importBase = align(_bssBase    + sizeof(.bss),    alignment);
_imageSize  = align(_importBase + sizeof(.import), alignment);
```

`_codeBase` is a literal `0x1000` with the author's own comment
`// !! code section should always be first?` (`linker.cpp:319`). Sections are laid out
strictly sequentially with no reordering and no gaps beyond alignment.

| Constant | Value | Configurable via |
|---|---|---|
| `IMAGE_BASE` | `0x00400000` | `opImageBase` |
| `SECTION_ALIGNMENT` | `0x1000` | `opSectionAlignment` |
| `FILE_ALIGNMENT` | `0x200` | `opFileAlignment` |
| Code base RVA | `0x1000` | **not configurable** |
| `MAJOR_OS` / `MINOR_OS` | `4.0` | not configurable |

## 3. Reference resolution

The linker maintains six separate reference maps (`linker.h:30-35`):

| Map | Holds |
|---|---|
| `_nativeReferences` | Addresses of native code/data emitted by the JIT |
| `_symbolReferences` | ELENA symbols and VMTs (`mskSymbolRef`, `mskVMTRef`) |
| `_constReferences` | Constant symbols (`mskConstantRef`) |
| `_messages` | Message name → sequential message ID |
| `_numbers` | Int32 and Real constants (`mskInt32Ref`, `mskRealRef`) |
| `_literals` | String constants (`mskLiteralRef`) |

`resolveReference()` (`linker.cpp:120`) dispatches on the reference mask. Note the
subtlety at `linker.cpp:69`: a reference map may contain several entries under the same
name distinguished only by mask bits, so lookup walks the collision chain comparing
`(*it) & mskImageMask` against the requested mask.

### Message IDs are assigned by the linker, not the compiler

```cpp
ref_t Linker :: resolveMessage(const TCHAR* reference)
{
   return mapKey(_messages, reference, _messages.Count() + 1);   // linker.cpp:100
}
```

Message IDs are **small sequential integers assigned at link time**, in first-encounter
order. They are therefore not stable across builds and are not embedded in `.nl`
modules — modules carry message *names*. This matters: `doc/todo.txt:504` worries about
message IDs colliding with heap addresses, because dispatch compares them numerically.

### Weak references and forwards

`retrieveReference()` (`linker.cpp:216`) resolves *weak* references by repeatedly
consulting the project's `[forwards]` table until a strong name is produced, raising
`errUnresovableLink` if the chain dead-ends. This is how `console.cfg` can redirect
`'program'output` → `win32'io'stdoutput` without the library naming Win32 directly —
the one genuine platform-abstraction mechanism the 2009 design provides, and the hook a
future POSIX target should reuse.

## 4. External functions and the import table

External references use the pseudo-namespace `$dlls` (`DLL_NAMESPACE`,
`elenaconst.h:23`). `resolveExternal()` (`linker.cpp:270`) splits
`$dlls'kernel32.WriteFile` into DLL name and function name at the last `.`, then
allocates a sequential import ordinal.

`createImportTable()` (`linker.cpp:326`) hand-builds the PE import directory: a
20-byte `IMAGE_IMPORT_DESCRIPTOR` per DLL, an OriginalFirstThunk list, a FirstThunk
list, and hint/name entries. `.dll` is appended if absent (`linker.cpp:350`).

**Import by name only** — no ordinal imports, no delay loading, no bound imports.

## 5. Relocation — `fixImage()` (`linker.cpp:377`)

All fixups are applied statically at link time; **the executable contains no `.reloc`
section**. Consequences:

- The image *must* load at its preferred base. It is not ASLR-compatible.
- Modern Windows will still load it (no `DYNAMIC_BASE` flag is set), but it forfeits
  ASLR and will fail if the base is unavailable.

Four fixup passes, each over a reference mask:

| Pass | Sections | Base | Transform |
|---|---|---|---|
| Code refs | `.text`, `.data` | `_codeBase + imageBase` | `reallocateReference` |
| Data refs | `.text`, `.data` | `_dataBase + imageBase` | `reallocateReference` |
| Static refs | `.text` | `_bssBase + imageBase` | `returnReference` |
| Import refs | `.text` | `_importBase + imageBase` | direct map |

`reallocateReference` applies the `VA_ALIGNMENT_POWER` shift — addresses are stored in
sections in *scaled* form and expanded on fixup. This is a 32-bit space-saving trick
that constrains object alignment and will need re-examination for 64-bit.

### The GC table patch

```cpp
void* table = resolveNativeReference(GC_TABLE, mskNativeDataRef);
ref_t offset = reallocateReference((size_t)table);
(*data)[offset + 0x14] = (getSectionSize(BSS_SECTION) >> 2);      // linker.cpp:389-391
```

The linker reaches into the runtime's `$elena'@gctable` structure at **hard-coded byte
offset `0x14`** and writes the size of the `.bss` section in dwords. This is how the
assembly GC learns how many static roots to scan.

**This is the tightest coupling in the whole system**: a magic offset in the C++ linker
into a structure defined in `src/asm/elena.asm`. Change the GC table layout in assembly
and the linker silently corrupts it. Any GC replacement must eliminate this.

## 6. Writing the executable

`createExecutable()` (`linker.cpp:566`) writes, in order:

1. **DOS stub** — read verbatim from an external file `winstub.ex_` located next to the
   compiler (`linker.cpp:414`). It was missing from the original checkout and has been
   re-created at `bin/winstub.ex_` (160 bytes; `e_lfanew` = `0xA0` = the file length, so
   the PE signature lands immediately after it, as `createExecutable()` assumes at
   `linker.cpp:577`). Reading the stub from an external file at run time is itself a
   design wart — it should be a compiled-in byte array — but see §10: the component is
   slated for deletion, so this is not worth fixing.
2. `IMAGE_NT_SIGNATURE` (`"PE\0\0"`).
3. `IMAGE_FILE_HEADER` — machine type hard-coded to `IMAGE_FILE_MACHINE_I386`, with the
   author's comment `// !! machine type may be different` (`linker.cpp:429`). Flags:
   `32BIT_MACHINE`, `LOCAL_SYMS_STRIPPED`, `LINE_NUMS_STRIPPED`.
4. `IMAGE_OPTIONAL_HEADER` — `IMAGE_NT_OPTIONAL_HDR32_MAGIC`. Subsystem is chosen from
   `opSystemType`: `ptGUI` → `WINDOWS_GUI`, otherwise `WINDOWS_CUI` (`linker.cpp:475`).
   Only data directory entry **1 (import)** is populated; all others are zeroed. No
   exports, no resources, no relocations, no TLS, no exception directory.
5. Section headers, then section bodies (`.bss` skipped — it has no raw data).

The structures written are the actual Win32 `IMAGE_*` structs from `<windows.h>`
(`linker.cpp:14`), copied to the file with `memcpy` semantics. So the linker does not
merely target PE — **it cannot compile without the Windows SDK headers**.

### The `mingw49` workaround

```cpp
#ifndef mingw49
header.Win32VersionValue = 0;    // ??
#endif
```
(`linker.cpp:468-470`) — evidence of a later patch for a newer MinGW whose
`IMAGE_OPTIONAL_HEADER` differed. A small sign this tree was touched after 2009.

## 7. Debug file (`.dn`)

`createDebugFile()` (`linker.cpp:589`) writes a sidecar consumed by the IDE debugger:

| Offset | Content |
|---|---|
| 0 | `"EN.D10!"` signature (`DEBUG_MODULE_SIGNATURE`, `elenaconst.h:150`) |
| 7 | DWORD: entry point VA of `'starter` (`STARTUP_CLASS`) |
| 11.. | The raw `.debug` section |

Debug info is emitted only when `opWithDebugInfo` is set.

## 8. What the linker does *not* support

| Missing | Consequence |
|---|---|
| DLL / shared library output | `doc/roadmap.txt:15-16` wanted "every package as a DLL" — never implemented |
| Export tables | Nothing can link *against* an ELENA binary |
| `.reloc` section | No ASLR; fixed load address |
| Resources | `doc/todo.txt:468` — icons, version info impossible |
| Object file output | Cannot link with C/C++ objects (`doc/todo.txt:24`) |
| Any non-x86 machine type | Hard-coded I386 |
| 64-bit PE (PE32+) | `IMAGE_NT_OPTIONAL_HDR32_MAGIC` only |
| Incremental / separate compilation | The whole program is relinked every time |

## 9. Missing artifacts this component needs

| Needed | Referenced at | Status |
|---|---|---|
| `winstub.ex_` | `linker.cpp:416` | **Supplied** — re-created at `bin/winstub.ex_` |
| `elena.bin`, `standard.bin`, `win32.bin`, `extended.bin`, `winsock.bin` | `bin/elc.cfg` `[primitives]` | Missing. Assembled runtime, loaded by `preloadCoreCode()` (`linker.cpp:300`) |
| `../lib/*.nl` | `bin/elc.cfg` `libpath` | Missing. Compiled standard library modules |

The `.bin` files are the output of `asm2bin` over `src/asm/*.asm`; the `.nl` files are
the output of `elc` over `src/**/*.l`. Both are reproducible from source once the
toolchain builds, so no artifact is permanently lost.

## 10. Modernization notes

### Verdict: **delete this component.**

It is ~640 lines implementing badly what `lld` implements well. Under an LLVM backend
the flow becomes:

```
elc → LLVM IR → llc/LLVM MC → .o (ELF/COFF/Mach-O) → lld → executable
```

and PE generation, import tables, relocations, section layout and debug-info emission
(DWARF/CodeView) all become someone else's solved problem.

### What must be preserved when it goes

| Concern | Currently in linker | Where it must move |
|---|---|---|
| `_LoaderHelper` interface | `linker.h:86-104` | **Keep.** This is the platform seam; reuse it as the LLVM backend's interface. |
| Message ID assignment | `linker.cpp:100` | Must move to a whole-program step, or change to name-based dispatch. |
| Weak reference / forwards resolution | `linker.cpp:216` | Keep — this is the platform-abstraction mechanism; extend it for POSIX targets. |
| GC table `.bss` size patch | `linker.cpp:389` | **Must die.** Replace with a proper symbol the runtime reads, or an LLVM-generated static-root table. |
| `VA_ALIGNMENT_POWER` address scaling | `fixImage()` | Re-evaluate for 64-bit; likely drop entirely. |
| Literal/int/real constant pooling | `_literals`, `_numbers` | Becomes LLVM global constants with automatic deduplication. |
| Debug info (`.dn`) | `linker.cpp:589` | Replace with DWARF/CodeView so any debugger works. See the DAP discussion in [`08-ide-debugger.md`](08-ide-debugger.md). |

### Ordered blockers

1. **`.bss` size patch at offset `0x14`** — couples the linker to the asm GC layout.
   Must be resolved before either side can be replaced independently.
2. **Link-time message ID assignment** — whole-program, order-dependent. Incompatible
   with separate compilation and with any DLL/shared-library plan.
3. **`VA_ALIGNMENT_POWER` scaled references** — a 32-bit assumption baked into the
   section fixup format.
4. **`<windows.h>` `IMAGE_*` structs** — makes the file uncompilable off Windows even
   though the surrounding logic is portable.
5. **No relocations** — must be fixed for any modern OS target regardless of LLVM.

### Interim step, if LLVM is not yet ready

If a Linux build is wanted *before* the LLVM backend lands, the cheapest path is to
write a sibling `elf/linker.cpp` implementing the same `_LoaderHelper` interface and
emitting a static ELF executable. The interface is clean enough that this is on the
order of 700 lines. It would let the existing x86 JIT run on Linux and give a working
cross-platform compiler well before the LLVM work is finished — at the cost of code
thrown away later.

---

*Sources read in full: `elenasrc/elc/win32/linker.h`, `elenasrc/elc/win32/linker.cpp`,
`bin/elc.cfg`, `bin/templates/console.cfg`, plus `elenasrc/engine/elenaconst.h`
constants.*
