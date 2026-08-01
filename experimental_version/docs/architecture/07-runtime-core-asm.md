# The ELENA Runtime Core (`src/asm/*.asm`)

> 6 146 lines of hand-written x86 assembly across five files. **This is the entire ELENA
> runtime.** There is no C runtime, no libc, no `crt0`. Everything between the PE entry
> point and user code — heap creation, allocation, a three-generation copying garbage
> collector, message dispatch, 32/64-bit integer and 64-bit float arithmetic, UTF-16 string
> handling, the Win32 syscall layer and the window procedure — is here.
>
> **This document is the behavioural specification that a future LLVM/C runtime must
> satisfy.** Everything below was read out of the assembly, not inferred from
> documentation.

Related: [`06-assembler-asm2bin.md`](06-assembler-asm2bin.md) (how these files become
`.bin`), [`04-pe-linker.md`](04-pe-linker.md) (how `.bin` becomes an `.exe`).

---

## 1. File inventory and subsystem breakdown

| File | Lines | Sections | Content |
|---|---:|---:|---|
| `src/asm/elena.asm` | 1 481 | 37 | GC, dispatch, object creation, frames, entry points, WndProc |
| `src/asm/standard.asm` | 2 804 | 82 | int32/int64/real64 arithmetic, strings, arrays, byte dumps |
| `src/asm/win32.asm` | 1 423 | 63 | console, files, windows, GDI |
| `src/asm/winsock.asm` | 362 | 13 | BSD-style sockets over Winsock2 |
| `src/asm/extended.asm` | 76 | 3 | RNG, system time |
| **Total** | **6 146** | **198** | |

`elena.asm` numbers its sections `elena'1`…`elena'38` but **`elena'27` does not exist** —
the gap is historical, not an omission in this document.

Approximate line budget by subsystem (a section may span several concerns):

| Subsystem | ≈ lines | Where |
|---|---:|---|
| Garbage collector (alloc, mark, compact, fixup, barriers, temporaries) | **600** | `elena.asm:11-486`, `:1050-1181` |
| Dynamic dispatch (`send`, redirect, group, cast, super-send) | **300** | `elena.asm:580-607`, `:747-953`, `:1018-1046`, `:1276-1331` |
| Startup / shutdown (console + GUI entry) | **150** | `elena.asm:490-550`, `:1185-1273` |
| Win32 GUI window procedure + message table | **146** | `elena.asm:1335-1481` |
| Object creation (`ocreate` ×5) | **65** | `elena.asm:642-704` |
| Frame prologue/epilogue/return templates | **60** | `elena.asm:553-639`, `:724-744` |
| Roles ("shift"), identity, type tests, stack ops | **72** | `elena.asm:707-721`, `:913-1015` |
| real64 arithmetic + `ftoa`/`atof` (x87) | **1 050** | `standard.asm:672-1438`, `:2250-2510` |
| int64 arithmetic + conversions | **480** | `standard.asm:724-733`, `:1630-2216` |
| UTF-16 string operations | **480** | `standard.asm:5-134`, `:314-668`, `:2221-2246`, `:2549-2594` |
| Arrays / byte dumps / heap-object allocation helpers | **420** | `standard.asm:1443-1626`, `:2514-2804` |
| int32 arithmetic | **250** | `standard.asm:73-311`, `:553-615` |
| Console I/O | **190** | `win32.asm:5-215`, `:1125-1156` |
| File I/O | **160** | `win32.asm:219-370` |
| Windowing (create/show/message loop/text) | **480** | `win32.asm:375-1000`, `:1091-1122`, `:1256-1279` |
| GDI (DC, bitmap, pen, brush, drawing) | **420** | `win32.asm:714-890`, `:1002-1088`, `:1282-1422` |
| Sockets | **362** | `winsock.asm` (all) |
| RNG / clock | **76** | `extended.asm` (all) |

---

## 2. Symbol catalogue

Every section in the five files. Names in the "Symbol" column are the ELENA reference
(`$package'` prefix is implicit). The **Kind** column: `P` = declared `procedure`
(4-byte aligned), `I` = declared `inline`, `S` = `structure`. Recall from
[`06-assembler-asm2bin.md` §3.1](06-assembler-asm2bin.md) that this distinction is *only*
alignment — whether a section is called or spliced is decided by the ELENA source
(`#external` vs `#inline`) or by the JIT.

Register conventions used throughout the table are defined in §9.

### 2.1 Garbage collector — `elena.asm`

| Symbol | Kind | Line | In | Out | Clobbers | Description |
|---|---|---|---|---|---|---|
| `elena'1` | P | `elena.asm:11` | `ebx` = header word, `ecx` = total byte size (16-aligned) | `eax` = object ptr | `esi`, `eax`; slow path also `ebx/ecx/edx/ebp` (restored) | **STD_ALLOC** — bump-pointer allocate from the young gen; on exhaustion runs a young / mid / full collection and retries |
| — `fixupHeap` | (local) | `elena.asm:206` | `ebp` = heap_start−4, `ebx`/`edx` = collected range | — | `esi,ecx,edi,eax` | Second GC pass: relocate every pointer through the fixup table and clear mark bits |
| — `fixup` | (local) | `elena.asm:278` | `edi` = slot array, `ecx` = count, `ebx`/`edx` = range, `ebp` | — | `eax,esi` | Recursive pointer relocation over one object/root block |
| `elena'2` | P | `elena.asm:337` | `ebx` = lowest collected address, `edx` = heap end, `ebp` = heap_start−4 | `edi` = new heap top | `eax,ecx,esi` | **Mark & compact** — marks from all root sets, then slides survivors down and records displacements |
| — `collect` | (local) | `elena.asm:448` | `edi` = slot array, `ecx` = count, `ebx`/`edx` = range | — | `eax,esi` | Recursive depth-first mark |
| `elena'31` | P | `elena.asm:1050` | `eax` = value, `esi` = destination slot, `edi` = destination object | `eax` | `ecx,edx` | **STD_ASSIGN** — stack-escape check + store + write barrier |
| `elena'32` | P | `elena.asm:1074` | `eax` = stack-resident object | `eax` = heap copy | `ebx,ecx,edx,esi` | **STD_ALLOCTEMP** — copy a stack temporary into the heap |
| `elena'33` | P | `elena.asm:1104` | `edi` = referring object, `eax` = referenced object | — | `ecx,edx,esi` | **STD_ADDYGPTR** — write barrier: append `edi` to the mid or old remembered set |
| `elena'34` | I | `elena.asm:1164` | same as `elena'31` | — | `ecx,edx` | Inline form of assign (no explicit store; the JIT emits the store) |

### 2.2 Dispatch — `elena.asm`

| Symbol | Kind | Line | In | Out | Clobbers | Description |
|---|---|---|---|---|---|---|
| `elena'7` | I | `elena.asm:580` | `[esp]` = receiver, `__arg1` = message id, `__arg2` = start offset (0 or 8) | `eax` = result (0 = failed) | `ebx,ecx,edx,esi` | **iocall** — the primary `send`. Linear VMT scan + parent/any-handler chain |
| `elena'19` | I | `elena.asm:747` | `[ebp+8]` = message id, `[esp]` = receiver | `eax` | `ebx,ecx,edx,esi` | **redirect** — dispatch the *current* method's message to another object; returns from the enclosing method on success |
| `elena'20` | I | `elena.asm:793` | `[ebp+8]` = message id, `__arg1vmt` = parent VMT | `eax` | `ebx,ecx,edx,esi` | **rredirect** — same, starting the search at a named parent class |
| `elena'21` | P | `elena.asm:839` | `eax` = group array, `edx` = message id, `ebx` = parameter | `eax` = first non-zero result, else 0 | `ecx,esi,edi` (edi restored) | **group** — try each element until one handles the message |
| `elena'23` | I | `elena.asm:926` | `[esp]` = `$TypeInstance`, `edx` = message id | `eax` | `ebx,ecx,esi` | **class redirect** — dispatch against the VMT stored in field 0 (used by `Type`) |
| `elena'30` | I | `elena.asm:1018` | `eax` = VMT (preloaded by the JIT), `__arg1` = message id, `__arg2` = offset | `eax` | `ebx,ecx,edx,esi` | **ircall** — statically-typed send / "super" send |
| `elena'36` | P | `elena.asm:1276` | `eax` = array, `edx` = message id, `ebx` = parameter | `eax` = the array | `ecx,esi,edi` (edi restored) | **cast** — broadcast the message to *every* element |

### 2.3 Frames, returns and stack discipline — `elena.asm`

| Symbol | Kind | Line | Effect |
|---|---|---|---|
| `elena'4` | I | `elena.asm:553` | **prep** — `push ebp; mov ebp,esp` |
| `elena'5` | I | `elena.asm:561` | **sprep** — `push edi; mov edi,eax; push ebp; mov ebp,esp` (establishes `$self` in `edi`) |
| `elena'6` | I | `elena.asm:571` | **return** — pop result, unwind to `ebp`, overwrite the receiver slot with the result, `ret` |
| `elena'8` | I | `elena.asm:610` | **sexit** — unwind, restore `edi`, return `$self` in `eax` |
| `elena'9` | I | `elena.asm:621` | **sreturn** — pop result, unwind, restore `edi`, return result |
| `elena'10` | I | `elena.asm:631` | **rreturnif** — return early if the popped value equals `__arg1obj` (a constant such as `nil`) |
| `elena'16` | I | `elena.asm:707` | **callext** — save `edi`, publish the current stack extent to the GC frame record, set up `ebp`, `call __arg1fun`, restore |
| `elena'17` | I | `elena.asm:724` | **prepredir** — `push edx; push edi; mov edi,eax; push ebp; mov ebp,esp` (keeps the message id live) |
| `elena'18` | I | `elena.asm:735` | **exitredir** — unwind, restore `edi`/`edx`, return **0** (dispatch failure) |
| `elena'25` | I | `elena.asm:972` | **ioswap** — exchange `[esp]` with `[esp − __arg1]` |
| `elena'26` | I | `elena.asm:983` | **ioset** — store `__arg2vmt` into `[[esp] + __arg1]` (initialise a `$TypeInstance` field) |

### 2.4 Object creation — `elena.asm`

| Symbol | Kind | Line | Fields initialised | Notes |
|---|---|---|---|---|
| `elena'15` | I | `elena.asm:697` | 0 | `ocreate0` — header + VMT only |
| `elena'12` | I | `elena.asm:658` | 2 | `ocreate2` — unrolled |
| `elena'13` | I | `elena.asm:669` | 4 | `ocreate4` — unrolled |
| `elena'14` | I | `elena.asm:682` | 6 | `ocreate6` — unrolled |
| `elena'11` | I | `elena.asm:642` | `ecx` (loop) | `ocreate` — general case, >6 fields |

All five take `ecx` = total aligned byte size, `__arg1` = header word, `__arg2vmt` = class
VMT; all `call @"$package'elena'1"`, store the VMT at `[eax−4]`, fill fields with `'nil`
and `push eax`. Selection is made by `compileCreate` (`x86jitcompiler.cpp:389-422`).

### 2.5 Identity, types and roles — `elena.asm`

| Symbol | Kind | Line | Description |
|---|---|---|---|
| `elena'22` | I | `elena.asm:913` | **ifSame** — pointer identity; `eax` = 0 if different |
| `elena'24` | I | `elena.asm:957` | **ifSameType** — compare `[type+0]` with `[other−4]` |
| `elena'28` | I | `elena.asm:993` | **shift** — replace `$self`'s VMT with role table entry `__arg1`; follows `[V−4]` first if `V` is already a role VMT |
| `elena'29` | I | `elena.asm:1009` | **unshift** — restore the owning class VMT from `[roleVMT−4]` |

### 2.6 Entry points and the Win32 window procedure — `elena.asm`

| Symbol | Kind | Line | Description |
|---|---|---|---|
| `elena'3` | P | `elena.asm:490` | **STD_ENTRY** — console `main`. Clears statics, creates the heap, seeds the GC frame chain, calls `@'starter`, `ExitProcess(0)` |
| `elena'35` | P | `elena.asm:1185` | **STD_WIN32ENTRY** — GUI `WinMain`. Same as `elena'3` plus `win32'api'instance` init and an `$sethandle` send |
| `elena'37` | P | `elena.asm:1335` | **STD_WNDPROC** — the `WNDPROC` trampoline: establishes a GC frame, translates `WM_*` to an ELENA message, dispatches, falls back to `DefWindowProcW` |
| `elena'38` | S | `elena.asm:1453` | `WM_*` → ELENA message-id table (11 entries + `0FFFFh` sentinel) |

### 2.7 int32 primitives — `standard.asm`

All take the destination object in `[esp]` and the source popped into `eax`; all return the
destination in `eax` (or 0 on a failed predicate). Clobbers are uniformly `eax,ebx,ecx,edx,esi`.

| Symbol | Line | Operation | Symbol | Line | Operation |
|---|---|---|---|---|---|
| `standard'3` | 73 | `INT_COPY` | `standard'16` | 272 | `INT_TEST` (any bit) |
| `standard'6` | 137 | `INT_EQUAL` | `standard'17` | 286 | `INT_TEST2` (all bits) |
| `standard'7` | 151 | `INT_LESS` (signed) | `standard'18` | 302 | `INT_NOT` (actually two's-complement `neg`) |
| `standard'8` | 165 | `INT_ADD` | `standard'24` | 553 | `INT_COPYHWORD` (`>>16`) |
| `standard'9` | 176 | `INT_SUB` | `standard'25` | 566 | `INT_COPYLWORD` (`&0FFFF`) |
| `standard'10` | 187 | `INT_MUL` (unsigned `mul`) | `standard'26` | 578 | `INT_CPYSTR` — decimal string → int32 |
| `standard'11` | 202 | `INT_DIV` (`cdq`/`idiv`) | `standard'27` | 619 | `STR_COPYINT32` — int32 → UTF-16 decimal |
| `standard'12` | 218 | `INT_BAND` | `standard'40` | 1495 | `INT_LOADSTRADDR` — take address of a literal body |
| `standard'13` | 229 | `INT_BOR` | `standard'61` | 2045 | `INT_COPYPTR` — read dword from a byte dump at an offset |
| `standard'14` | 240 | `INT_BXOR` | `standard'66` | 2262 | `INT_ROUNDREAL` — real64 → int32 with FPU rounding mode save/restore |
| `standard'15` | 252 | `INT_SHIFT` (sign of the operand selects left/right) | | | |

### 2.8 int64 primitives — `standard.asm`

| Symbol | Line | Operation | Symbol | Line | Operation |
|---|---|---|---|---|---|
| `standard'31` | 724 | `LONG_COPY` | `standard'55` | 1904 | `LONG_TEST2` |
| `standard'46` | 1630 | `LONG_COPYINT` (`cdq` sign-extend) | `standard'56` | 1926 | `LONG_SHIFT` (`shld`/`shrd`, ≥32 special-cased) |
| `standard'48` | 1656 | `LONG_EQUAL` | `standard'57` | 1988 | `LONG_NOT` (negate) |
| `standard'49` | 1674 | `LONG_LESS` | `standard'58` | 2006 | `LONG_BOR` |
| `standard'50` | 1694 | `LONG_ADD` (`add`/`adc`) | `standard'59` | 2019 | `LONG_BXOR` |
| `standard'51` | 1717 | `LONG_SUB` (`sub`/`sbb`) | `standard'60` | 2032 | `LONG_BAND` |
| `standard'52` | 1739 | `LONG_MUL` — 32×32 fast path, three-partial-product slow path | `standard'62` | 2067 | `STR_COPYLONG` — int64 → UTF-16 decimal |
| `standard'53` | 1776 | `LONG_DIV` — full signed 64/64 with shift-normalise and quotient correction | `standard'63` | 2145 | `LONG_CPYSTR` — decimal string → int64 |
| `standard'54` | 1884 | `LONG_TEST` | | | |

> **Known defects to preserve or fix deliberately.** `standard'51` (`LONG_SUB`) computes
> `sub edx,ebx` / `sbb ecx,eax` but then stores the *unmodified* `ebx`/`eax`
> (`standard.asm:1731-1732`) — it stores the operand, not the difference.
> `standard'56` (`LONG_SHIFT`) ends with `mov ebx,[eax]` where `eax` was already reloaded to
> the destination object (`standard.asm:1979`), producing a wild store. `standard'54`
> (`LONG_TEST`) tests `[eax]` against `edx` rather than `ebx` (`standard.asm:1890`).
> These are latent bugs in the 2009 code — a reimplementation should implement the
> *documented intent*, not the emitted behaviour.

### 2.9 real64 primitives — `standard.asm` (all x87)

| Symbol | Line | Operation | Symbol | Line | Operation |
|---|---|---|---|---|---|
| `standard'28` | 672 | `FLOAT_COPYINT` (`fild`/`fstp`) | `standard'67` | 2298 | `FLOAT_TRUNC` (CW truncate mode) |
| `standard'29` | 684 | `FLOAT_EQUAL` (`fcomp`+`fnstsw`) | `standard'68` | 2333 | `FLOAT_ARCTAN` (`fpatan`) |
| `standard'30` | 704 | `FLOAT_LESS` | `standard'69` | 2357 | `FLOAT_COS` (with `fprem` range reduction) |
| `standard'32` | 737 | `FLOAT_ADD` | `standard'70` | 2388 | `FLOAT_EXP` (`fldl2e`/`f2xm1`/`fscale`) |
| `standard'33` | 750 | `FLOAT_SUB` | `standard'71` | 2433 | `FLOAT_LN` (`fldln2`/`fyl2x`) |
| `standard'34` | 763 | `FLOAT_MUL` | `standard'72` | 2459 | `FLOAT_SIN` |
| `standard'35` | 776 | `FLOAT_DIV` | `standard'73` | 2490 | `FLOAT_SQRT` |
| `standard'36` | 789 | `STR_COPYFLOAT` — **355 lines** of `ftoa`: BCD via `fbstp`, regular and scientific notation | `standard'74` | 2503 | `FLOAT_PI` |
| `standard'37` | 1148 | `FLOAT_CPYSTR` — **291 lines** of `atof`: packed-BCD accumulation + `fbld` + `f2xm1`/`fscale` exponent | `standard'47` | 1645 | `FLOAT_COPYLONG` (`fild qword`) |
| `standard'65` | 2250 | `FLOAT_ABS` | | | |

`standard'36` and `standard'37` are together **646 lines — 10.5 % of the whole runtime** and
are the single largest replaceable block: both are direct substitutes for `snprintf("%g")`
and `strtod`.

### 2.10 String, array and dump primitives — `standard.asm`

Strings are UTF-16, stored as `[length:dword][chars…][0:word]`, length in *characters*.

| Symbol | Line | Operation |
|---|---|---|
| `standard'1` | 5 | `WSTR_EQUAL` |
| `standard'2` | 35 | `WSTR_LESS` |
| `standard'4` | 84 | `WSTR_CPY` |
| `standard'5` | 108 | `WSTR_ADD` (append) |
| `standard'19` | 314 | `WCHAR_CPYPTR` — indexed character read with bounds check |
| `standard'20` | 344 | `WSTR_INDEXOF` — substring search, −1 when absent |
| `standard'21` | 405 | `WSTR_INSERT` |
| `standard'22` | 471 | `WSTR_ERASE` |
| `standard'23` | 538 | `WSTR_CPYWCHR` — 1-char string from a char |
| `standard'64` | 2221 | `WSTR_SUBSTR` |
| `standard'77` | 2549 | `WSTR_CPYPTR` — build a string from a byte dump |
| `standard'38` | 1443 | `WSTR_ALLOC` — allocate a binary string object (sets `gcBinary`) |
| `standard'82` | 2785 | `DUMP_ALLOC` — allocate a raw byte dump |
| `standard'39` | 1468 | `OBJ_ALLOC` — allocate an object array filled with a pattern |
| `standard'41` | 1507 | `ARR_GET` — bounds-checked element read |
| `standard'42` | 1532 | `ARR_SET` — bounds-checked element write **via `elena'31` (write barrier)** |
| `standard'43` | 1566 | `ARRAY_LEN` |
| `standard'44` | 1578 | `GROUP_ADD` — find a free slot and assign (barriered) |
| `standard'45` | 1609 | `GROUP_CPY` |
| `standard'75` | 2514 | `DUMP_LEN` (bytes) |
| `standard'76` | 2526 | `DUMP_CPY` |
| `standard'78` | 2598 | `DUMP_READ2BUF` |
| `standard'79` | 2659 | `PTR_ADDW` — append a string into a dump at an offset |
| `standard'80` | 2700 | `PTR_COPYINT32` — write a dword into a dump |
| `standard'81` | 2723 | `PTR_ADDBUF` |

### 2.11 Win32 layer — `win32.asm`

All are `procedure`s with named parameters and end with `ret`; they are reached through
`#external` → `elena'16`. Return convention: the receiver object in `eax` on success,
`xor eax,eax` on failure.

**Console** — `win32'1` (`:5` `GetStdHandle(STD_OUTPUT)`), `win32'2` (`:23` `WriteConsoleW`),
`win32'3` (`:46` `ReadConsoleW` + CRLF trim), `win32'4` (`:82` `GetStdHandle(STD_INPUT)`),
`win32'5` (`:100` `ReadConsoleInputW`, blocking key read), `win32'6` (`:134` seek to EOL in
a buffer), `win32'7` (`:161` copy an ANSI line to UTF-16), `win32'8` (`:199` EOL test),
`win32'53` (`:1125` flush the console input queue), `win32'54` (`:1160`
`WideCharToMultiByte`).

**Files** — `win32'9` (`:219` `CreateFileW`), `win32'10` (`:251` `ReadFile`),
`win32'11` (`:283` `WriteFile`), `win32'12` (`:311` `CloseHandle`),
`win32'13`/`win32'14` (`:328`/`:349` `GetCommandLineW` length / copy).

**Windows** — `win32'15` (`:375` pointer store), `win32'16` (`:386` invalidate+update),
`win32'17` (`:402` `DestroyWindow`), `win32'19` (`:432` `ShowWindow`),
`win32'20` (`:447` `EnableWindow`), `win32'21`/`win32'22` (`:461`/`:482` `WM_GETTEXT`/`WM_SETTEXT`),
`win32'23` (`:504` `GetClientRect`), `win32'24`/`win32'25` (`:531`/`:555` `SetWindowPos`),
`win32'26` (`:579` `LoadCursorW`+`SetCursor`), `win32'27` (`:607` `PeekMessageW`),
`win32'28` (`:628` `IsDialogMessage`/`TranslateMessage`/`DispatchMessageW`),
`win32'29` (`:650` `WaitMessage`), `win32'30`–`win32'32` (`:661`,`:679`,`:695`
`SendMessageW`/`PostMessageW`), `win32'42` (`:894` install `'windproc`),
`win32'43`/`win32'44` (`:905`/`:932` system colours), `win32'45` (`:948` `CreateWindowExW`),
`win32'46` (`:989` `RegisterClassW`), `win32'51` (`:1091` `GetMessageW`),
`win32'52` (`:1110` `IsWindowVisible`), `win32'55`/`win32'56` (`:1198`/`:1235` object-pool
add/remove), `win32'57` (`:1256` `IsWindowEnabled`), `win32'58` (`:1271` `PostQuitMessage`).

**GDI** — `win32'18` (`:414` `GetDC`), `win32'33`–`win32'35` (`:714`,`:735`,`:754` text/bk
colour and mode), `win32'36` (`:775` `DeleteDC`), `win32'37` (`:790` `DeleteObject`),
`win32'38`/`win32'39` (`:805`/`:824` `BeginPaint`/`EndPaint`),
`win32'40` (`:839` `GetObjectA`), `win32'41` (`:868` `CreateCompatibleDC`+`SelectObject`),
`win32'47` (`:1004` `CreateCompatibleBitmap`), `win32'48` (`:1022` `LoadImageW`),
`win32'49` (`:1052` `CreatePen`), `win32'50` (`:1074` `CreateSolidBrush`),
`win32'59` (`:1284` `BitBlt`), `win32'60`/`win32'61` (`:1315`/`:1336` `MoveToEx`/`LineTo`),
`win32'62` (`:1371` `TextOutW`), `win32'63` (`:1395` `FillRect`).

### 2.12 Sockets — `winsock.asm`

| Symbol | Line | Win32 call | Symbol | Line | Win32 call |
|---|---|---|---|---|---|
| `winsock'1` | 12 | `WSAAsyncSelect` | `winsock'8` | 153 | `send` (UTF-16 string) |
| `winsock'2` | 38 | `WSACleanup` | `winsock'9` | 179 | `send` (byte dump) |
| `winsock'3` | 48 | `WSAStartup` | `winsock'10` | 204 | `gethostname`+`gethostbyname`+`socket`+`bind` |
| `winsock'4` | 66 | `closesocket` | `winsock'11` | 264 | `accept` |
| `winsock'5` | 78 | `listen(SOMAXCONN)` | `winsock'12` | 285 | `socket(AF_INET,SOCK_STREAM,0)` |
| `winsock'6` | 96 | `recv` | `winsock'13` | 309 | `inet_addr`+`connect` |
| `winsock'7` | 129 | `send` (4-byte int32) | | | |

### 2.13 Extended — `extended.asm`

| Symbol | Line | Description |
|---|---|---|
| `extended'1` | 6 | `GetSystemTime`+`SystemTimeToFileTime` → RNG seed |
| `extended'2` | 26 | 64-bit LCG (`×0x4E35 + 0x15A`, then `idiv maxn`) |
| `extended'3` | 63 | Current time as a `FILETIME` |

---

## 3. Object model and memory layout

Constants from `elenasrc/engine/elenaconst.h:253-278`.

### 3.1 Object header

An **object pointer** always points at field 0. Two dwords precede it:

```
     ┌──────────────┬──────────────┬─────────┬─────────┬─────┐
     │  size/flags  │   VMT ptr    │ field 0 │ field 1 │ ... │
     └──────────────┴──────────────┴─────────┴─────────┴─────┘
      obj−8          obj−4          obj+0     obj+4
```

| Field | Offset | Contents |
|---|---|---|
| size / flags | `−8` | field count (dwords). Bit 31 `gcBinary` = 0x80000000 → the body contains **no pointers** and the value is a dword count of raw storage. Bit 30 `gcCollected` = 0x40000000 → **GC mark bit**, set during marking, cleared during fixup |
| VMT pointer | `−4` | points at `entries[0]` of the class VMT (see §3.2) |

Header size `elEmptyObject` = **8** (`elenaconst.h:253`). Every object is padded to a
16-byte boundary: `gcPageSize` = 0x10 (`elenaconst.h:276`).

```
totalBytes = (fieldCount*4 + 8 + 15) & ~15
```
which the assembler writes as `(fields<<2 + 'gc_empty_object_aligned) & 'gc_page_mask`
(`standard.asm:1472-1475`, `elena.asm:1078-1082`).

Because the mark bit is bit 30 and the binary bit is bit 31, a binary object's size word is
**negative**, and the mark loop uses `jle` (`elena.asm:465`, `:312`) to skip tracing its
body. Zero-field objects are skipped the same way. **This is the entire type information
the GC has** — there are no maps, no bitmaps, no stack maps.

### 3.2 VMT layout

Built by `_JITCompiler::compileVMT` (`jitcompiler.cpp:105-192`). A VMT reference points 12
bytes (`elVMTOffset`, `elenaconst.h:254`) into the block:

```
  V−12 : role table pointer   (if elVMTWithRoles) else 0
  V−8  : class flags          (elStandartVMT, elRoleVMT, elVMTAnyHandler, elStateless …)
  V−4  : if elVMTAnyHandler → pointer to the any-handler entry pair
         if elRoleVMT       → the owning class's VMT
         else               → 0
  V+0  : VMTEntry[0]  { messageID:dword, address:dword }
  V+8  : VMTEntry[1]
  ...
         VMTEntry[n−1] = { TERMINAL_MESSAGE_ID (0x7FFFFFFF), 0 }   ← terminator
         VMTEntry[n]   = { 0, anyHandlerAddr }                     ← any handler
         VMTEntry[n+1] = { 0, anyHandlerAddr }                     ← duplicate (see §5.2)
```

Entries are **sorted ascending by `messageID` as a signed 32-bit value**
(`jitcompiler.cpp:90-91`). Predefined messages have ids `0x80000000`–`0x80000019`
(`elenaconst.h:112-140`), i.e. large negative, so they sort *before* all user messages,
which are small positive ints assigned by the linker. The terminator `0x7FFFFFFF` sorts last.

### 3.3 Heap layout

Created once, in the entry point (`elena.asm:503-532`):

```
        base                                heap_start                              heap_end
          │                                       │                                      │
          ▼                                       ▼                                      ▼
          ├──────── fixup table ──────────────────┼──── old gen ──┬─ mid gen ─┬─ young ─┬─ free ─┤
          │       gcsize × 4 bytes                │               │           │         │        │
          │  (also hosts the two remembered sets, │           og_heap     mg_heap   yg_heap      │
          │   which grow *downward* from          │                                             │
          │   heap_start − 4)                     │◄────────── gcsize × 16 bytes ───────────────►│
```

| Symbol | GC table offset | Meaning |
|---|---|---|
| `gc_heap_start` | `0x04` | base of the object area; also the base of the old generation |
| `gc_og_heap` | `0x10` | top of the old generation / base of the mid generation |
| `gc_mg_heap` | `0x0C` | top of the mid generation / base of the young generation |
| `gc_yg_heap` | `0x08` | **allocation bump pointer** |
| `gc_heap_end` | `0x18` | end of the object area |

With the shipped `bin/elc.cfg` `[linker] gcsize=4096`: object heap = **64 KiB**, fixup
table = **16 KiB**. The heap **never grows** — `HeapAlloc` is called exactly once.

The fixup table is one dword per 16-byte page, indexed *downward* from `ebp = heap_start−4`:

```
slot(addr) = ebp − ((addr − ebp) & ~15) >> 4 << 2
```
which the code writes as `((addr − ebp) & mask) >> (log−2)`, negated, plus `ebp`
(`elena.asm:102-107`, `:290-295`, `:416-421`).

The two remembered sets (`gc_mgptr2`, `gc_ogptr2`) live in the *same* region below
`heap_start`, both initialised to `heap_start − 4` (`elena.asm:523-527`) and growing
downward. After a mid-generation collection `gc_mgptr2` is repositioned to the fixup slot
corresponding to the new mid/old boundary (`elena.asm:102-109`), which partitions the space
between the two sets. **The fixup table and the remembered sets share one arena with no
bounds checking.**

---

## 4. Garbage collector

### 4.1 Allocation fast path — `elena.asm:11-23`

```
eax  = [gc_yg_heap]
esi  = eax + ecx                 ; ecx = 16-aligned total size
if esi > [gc_heap_end] → slow path
[eax] = ebx                      ; header word (field count | gcBinary)
[gc_yg_heap] = esi
return eax + 8                   ; object pointer
```

Seven instructions, no locking, no zeroing (fields are written by the `ocreate` caller).
The VMT slot at `[eax+4]` is left uninitialised — the caller must store it *immediately*.

### 4.2 Collection trigger and generation escalation

The slow path (`elena.asm:25-202`) is entered only on exhaustion. It reads `gc_flag`
(GC table `+0x2C`) to decide the scope:

| `gc_flag` | Scope | `ebx` (lowest collected addr) | `elena.asm` |
|---|---|---|---|
| 0 | young | `[gc_mg_heap]` | `:43-83` |
| 1 (`GC_MG_COLLECT`) | young + mid | `[gc_og_heap]` | `:85-139` |
| 2 (`GC_FULL_COLLECT`) | everything | `heap_start` | `:141-191` |

After each pass, if the surviving free space (`heap_end − newTop`) is **not greater than**
`'gc_heap_minimal` (256 bytes) the flag is escalated for next time (`:57-59`, `:116-118`).
If the requested size still does not fit, the pass falls through directly into the next
wider scope (`:62-64`, `:121-123`). A full collection that still cannot satisfy the request
jumps to `gc_Error` (`:193-202`), which raises Win32 exception `0x190`
(`ELENA_ERR_OUTOF_MEMORY`, `elenaconst.h:284`) via `RaiseException`.

**Survivors are promoted by construction, not by copying twice.** A young collection
compacts survivors down to `[gc_mg_heap]` and then sets `mg_heap = yg_heap = newTop`
(`:52-53`) — so everything that survived is now *inside* the mid generation. A mid
collection compacts to `[gc_og_heap]` and sets `og = mg = yg = newTop` (`:98-100`).

### 4.3 Root scanning — **region-precise, slot-conservative**

Three root sets, walked identically by `collect` (mark) and `fixup` (relocate):

**(a) The GC frame chain.** `gs_current_frame` (GC table `+0x00`) points at a two-dword
record on the machine stack:

```
[frame + 0] = byte size of the live region below this record
[frame + 4] = pointer to the previous record (0 terminates)
```

Roots are the dwords in `[frame − size, frame)`. The size field is **refreshed at every
point that can trigger a collection or leave managed code**:

| Site | Code | `elena.asm` |
|---|---|---|
| allocation slow path | `edx=[gs_current_frame]; [edx] = edx − esp` | `:28-31` |
| external call (`callext`) | same pattern before `call __arg1fun` | `:709-713` |
| WndProc entry | pushes a *new* record and sets `gs_current_frame = esp` | `:1342-1345` |
| program start | pushes `{0, 0}` and sets `gs_current_frame = esp` | `:534-537` |

Scanning code: `elena.asm:377-388` (mark), `:246-261` (fixup).

**(b) Static roots.** `'statroots` (the `.bss` section) with `gc_static_size` entries; the
count is patched into GC table `+0x14` by the PE linker (`linker.cpp:391` =
`sizeof(.bss) >> 2`). Cleared to `'nil` at startup (`elena.asm:493-501`).

**(c) Remembered sets.** `gc_mgptr2`/`gc_ogptr2` list *objects* in the mid / old generation
that hold a reference into a younger generation. Each entry is scanned as a full object:
`ecx = [edi−8]` gives the field count (`elena.asm:349-351`, `:366-369`).

Inside a scanned region, `collect` (`elena.asm:448-484`) treats **every dword** as a
candidate:

```
esi = [edi]
if esi <  ebx  → not a root      ; below the collected range
if esi >  edx  → not a root      ; above the heap
eax = [esi−8]                    ; read what would be the header
if eax & gcCollected → already marked
[esi−8] |= gcCollected
if eax <= 0 → do not trace body  ; binary object or zero fields
recurse into esi with ecx = eax  ; depth-first, using the CPU stack
```

So: the collector knows exactly *which memory ranges* may contain roots (precise), but
inside a range it cannot distinguish a pointer from an integer that happens to fall inside
the 64 KiB heap (conservative). **Because the collector then *moves* the object and
rewrites the slot, a false positive is not merely a leak — it corrupts data.** In practice
this is survivable only because the heap is a single small, fixed range and ELENA boxes
every integer, so raw integers rarely appear in scanned slots. Any reimplementation that
enlarges the heap makes this dramatically more dangerous.

Marking recursion uses the machine stack (`push edi; push ecx` at `:467-468`) with **no
depth limit** — a sufficiently deep object graph overflows the stack.

### 4.4 The mark / compact / fixup algorithm

`elena'2` (`elena.asm:337-444`) and `fixupHeap` (`:206-274`) implement a two-pass
mark-compact with a side table:

```
PASS 1 — MARK  (elena'2, elena.asm:341-397)
  for each remembered-set entry, each stack frame region, and the static root array:
      collect()   → set gcCollected on every reachable object in [ebx, edx]

PASS 2 — COMPACT  (elena.asm:399-444)
  src = dst = ebx
  while src < edx:
      size = align(([src] << 2) + 8, 16)
      if [src] has gcCollected:
          fixupSlot(src) = dst − src          ; record the displacement
          if src != dst: copy `size` bytes src → dst
          dst += size
      src += size
  return dst                                   ; the new heap top

PASS 3 — FIXUP  (fixupHeap, elena.asm:206-331)
  for the same three root sets:
      for each slot in range:
          p = [slot]
          if p in [ebx, edx]:
              d = fixupSlot(p);  if d != 0: [slot] = p + d
              if [p−8] has gcCollected:
                  clear gcCollected
                  if size > 0: recurse into p's fields
```

Notes that matter for a reimplementation:

* Compaction is a **sliding** compaction, not a semispace copy — object order is preserved
  and there is no to-space. That is what makes the fixup table necessary.
* The fixup table is indexed by *page*, so it can only record one displacement per 16-byte
  page. This is safe because every object is 16-byte aligned and at least 16 bytes.
* Pass 3 doubles as the mark-bit clearing pass. If a marked object is *not* reachable from
  any root during pass 3, its mark bit stays set — which would corrupt the next cycle.
  In practice pass 1 and pass 3 walk identical root sets, so this holds.
* `[gc_mgptr2_end]` is reset to `[gc_mgptr2]` after a young collection (`elena.asm:68-69`)
  because everything the mid generation pointed at has been promoted into it.

### 4.5 Write barriers and stack escape

**`elena'31` / `elena'34` (STD_ASSIGN, `elena.asm:1050-1070`, `:1164-1181`)** run on every
reference store into an object field:

```
ecx = fs:[4]          ; Win32 TIB StackBase  (high address)
edx = fs:[8]          ; Win32 TIB StackLimit (low address)
if eax > ecx or eax < edx:  goto store        ; value is not on the stack
call elena'32                                 ; copy the stack temporary into the heap
store:
[esi] = eax
if edi <= [gc_mg_heap]:  call elena'33        ; destination is mid or old → write barrier
```

The stack-range probe via `FS:[4]` / `FS:[8]` is a **hard Win32 dependency** and the single
most platform-specific instruction pair in the runtime (POSIX equivalent: cache
`pthread_attr_getstack` results in TLS, or on Linux read `__libc_stack_end`).

**`elena'32` (STD_ALLOCTEMP, `elena.asm:1074-1100`)** is the escape hatch for *temporal
objects*: objects the compiler allocated in the stack frame and which are now being stored
into a longer-lived object. It reads the size from `[eax−8]`, masks off `gcBinary`
(`and ecx, 7FFFFFFFh`, `:1086`), allocates, copies the VMT and body, and returns the heap
copy.

**`elena'33` (STD_ADDYGPTR, `elena.asm:1104-1160`)** is the generational write barrier:

```
if edi >= eax: return              ; referrer is younger than referent → nothing to record
if edi in [esp, fs:[4]]: return    ; referrer is on the stack → covered by frame scanning
if edi < [gc_og_heap]:  target = the OG set
else:                   target = the MG set (and only if eax >= [gc_mg_heap])
linear scan the set for edi; if absent, append and bump the *_end pointer
```

The duplicate check is a **linear scan of the whole set on every barrier**. There is no
capacity check on the append — the set grows down into the fixup table.

### 4.6 Finalization

**There is none.** No finalizer queue, no destructor call, no `WeakReference`. Handles
(files, DCs, bitmaps, sockets) are closed only by explicit ELENA-level calls
(`win32'12`, `win32'36`, `win32'37`, `winsock'4`).

### 4.7 Why this GC cannot work with multiple threads

Ten independent reasons, each of which alone is fatal:

| # | Problem | Evidence |
|---|---|---|
| 1 | **The allocation fast path is a non-atomic read-modify-write** of the single global `gc_yg_heap`. Two threads racing produce overlapping objects. | `elena.asm:15-21` |
| 2 | **There are no atomic instructions in the assembler at all** — no `lock`, `cmpxchg` or `xadd` is encodable, so the fast path cannot even be made atomic without extending `asm2bin`. | `x86assembler.cpp` §5.1 has no such mnemonics |
| 3 | **The heap is allocated `HEAP_NO_SERIALIZE`** (`GC_HEAP_ATTRIBUTE = 0x0D` = `NO_SERIALIZE\|GENERATE_EXCEPTIONS\|ZERO_MEMORY`). | `elena.asm:3`, `:510`, `:1205` |
| 4 | **`gs_current_frame` is one global root-chain head.** With N threads there are N stacks but only one chain; the other N−1 stacks are invisible to the collector and their objects are freed while live. | `elena.asm:28`, `:250`, `:378`, `:537` |
| 5 | **There is no stop-the-world, no safepoint and no poll.** A collection runs inline on whichever thread hit exhaustion and *moves objects*. Every other thread's registers and stack slots become stale pointers instantly. | `elena.asm:25-202` |
| 6 | **Root scanning is conservative over the collecting thread's own frame region while the collector is running on that same stack.** With another thread mutating the heap concurrently there is no consistent snapshot to scan. | `elena.asm:246-261` |
| 7 | **The stack-escape probe reads `FS:[4]`/`FS:[8]` — the *current* thread's TIB.** An object legitimately on thread B's stack is classified as a heap object by thread A, so it is neither copied out nor tracked. | `elena.asm:1052-1053`, `:1109` |
| 8 | **The write barrier is a racy linear scan + append.** Two concurrent barriers can both read `*_end`, both write the same slot, and lose an entry — silently dropping a cross-generation reference. | `elena.asm:1124-1136`, `:1141-1154` |
| 9 | **`gc_flag`, the generation pointers and both remembered-set cursors are unsynchronised shared mutable state** in one 0x30-byte block. | GC table, `jitlinker.cpp:496-509` |
| 10 | **Mark and fixup recurse on the mutator's own CPU stack**, so a collection cannot be suspended, resumed, or moved to a dedicated GC thread. | `elena.asm:314-318`, `:467-471` |

A multithreaded successor cannot be an incremental patch. It needs, at minimum:
thread-local allocation buffers, an atomic or per-thread bump pointer, a per-thread root
chain (or precise stack maps), a real safepoint protocol, and a card table or SATB barrier
using atomic operations.

---

## 5. Dynamic dispatch

### 5.1 The message-send protocol

A send has three inputs and one output:

| | Location |
|---|---|
| receiver | `[esp]` (pushed by the caller) |
| message id | `edx` (from `__arg1`, or `[ebp+8]` for redirects) |
| parameter | `ebx` |
| **result** | `eax` — **`0` means "the message was not handled"** |

The JIT always follows a send with `test eax,eax; jz <alternative>`
(`compileJumpIfNot`, `x86jitcompiler.cpp:81-96`). This is the **entire failure protocol**:
there are no exceptions, no error codes and no unwinding. "Message not understood" is a
zero in `eax` and a branch.

### 5.2 The scan loop — `elena'7` (`elena.asm:580-607`)

```
edx = __arg1                       ; message id
eax = [esp]                        ; receiver
labStart:
    eax = [eax − 4]                ; first pass: the object's VMT
                                   ; later passes: [VMT−4] = any-handler / role-owner chain
    if eax == 0: goto labEnd       ; eax stays 0 → dispatch failed
    esi = eax + __arg2             ; __arg2 = 0 or 8 (start at entry 0 or entry 1)
    ecx = [esi]                    ; entries[k].messageID
    if ecx == 0: goto labCall      ; 0 marks the any-handler entry
labNext:
    cmp ecx, edx                   ; SIGNED comparison
    esi += 8
    if ecx >  edx: goto labStart   ; passed the target → follow the chain
    ecx = [esi]
    if ecx <  edx: goto labNext
    ; fallthrough: exact match
labCall:
    eax = [esp]                    ; reload self
    call [esi − 4]                 ; esi was already advanced → esi−4 = entries[k].address
labEnd:
```

Properties:

* **Linear scan over a sorted array.** Not a hash table, not a vtable index, not an inline
  cache. Complexity is O(number of methods in the class) per send.
* **`__arg2` is a start offset, not a hash bucket.** `compileIOCallN`
  (`x86jitcompiler.cpp:349-364`) emits `0` only for the `new` message
  (`bccompiler.cpp:419-427`) and `8` for everything else — i.e. every ordinary send *skips
  VMT entry 0*.
* **The "chain" is the `[VMT−4]` slot**, which is the any-handler pointer for a class with
  `#any` and the owner VMT for a role VMT. When the scan runs past the target, or when the
  first inspected entry is the any-handler marker, the loop restarts one link along.
* **The duplicated any-handler entry** (`jitcompiler.cpp:130-131`, and the pseudo-VMT at
  `:36-41`) exists so that the `__arg2 = 8` form lands on `entries[n].address` after
  `esi += 8` and `[esi−4]`.

> **Latent hazard.** With `__arg2 = 0` (the `new` message) on a class that has an
> any-handler and no matching entry, `[esi−4]` resolves to the *terminator* entry's address
> field, which is `0` — i.e. `call 0`. The current library evidently never reaches this
> path. A reimplementation must define the behaviour explicitly (either route `new` to the
> any-handler, or raise "not understood").

### 5.3 Dispatch variants

| Variant | Section | Difference from `elena'7` |
|---|---|---|
| **redirect** | `elena'19`, `elena.asm:747` | Message id comes from `[ebp+8]` (the *current* method's id). On success, unwinds the enclosing method and returns the result directly to *its* caller (`:779-786`). On failure, continues only if `[esp]` is non-zero |
| **rredirect** | `elena'20`, `elena.asm:793` | Same, but starts at `__arg1vmt` (a named parent class) instead of the receiver's own VMT — this is `super` |
| **ircall** | `elena'30`, `elena.asm:1018` | The JIT preloads `eax` with a VMT reference (`x86jitcompiler.cpp:369-370`) and the loop jumps straight to `labStart2`, skipping the `[eax−4]` fetch. Statically-resolved send |
| **class redirect** | `elena'23`, `elena.asm:926` | `lea eax,[eax+4]` so the next `mov eax,[eax−4]` reads **field 0** of a `$TypeInstance`, which holds a VMT pointer. Implements `Type Name` / `Type of:` (`src/elena.l:27-29`) |
| **group** | `elena'21`, `elena.asm:839` | Iterates the elements of an object array, running the full scan against each. Returns the first non-zero result; a zero result continues only while the saved receiver slot is non-zero; exhausting the array returns 0 |
| **cast** | `elena'36`, `elena.asm:1276` | Same iteration, but does **not** stop on success — broadcasts to every element and returns the array |

`group` and `cast` are installed as *pseudo-VMTs* by `loadPseudoVMT`
(`jitlinker.cpp:479-492`, `:524-532`): a 3-dword header plus two identical any-handler
entries pointing at the routine, so that a normal send to a `$group`/`$cast` object lands
in them.

### 5.4 Roles ("shift technology")

ELENA's headline feature. `elena'28` (`elena.asm:993-1005`) mutates the *instance's* VMT
pointer in place:

```
edx = [edi − 4]                     ; $self's current VMT
if [edx − 8] & elRoleVMT:           ; already shifted?
    edx = [edx − 4]                 ; recover the owning class VMT
edx = [edx − 0Ch]                   ; role table
ecx = [edx + __arg1]                ; role table entry (__arg1 = index*4)
[edi − 4] = ecx                     ; install the role VMT
```

`elena'29` (`:1009-1015`) reverses it via `[roleVMT−4]`. **An object's class is mutable at
runtime by design** — a property any reimplementation must preserve, and one that rules out
LLVM's `invariant.group`-based devirtualisation on the VMT slot.

---

## 6. Object creation

Emitted by `compileCreate` (`x86jitcompiler.cpp:389-422`):

```
mov ecx, align(fieldCount*4 + 8, 16)      ; total byte size, computed at JIT time
<ocreateN template>
```

The template (e.g. `elena'11`, `elena.asm:642-655`):

```
ebx = __arg1                              ; header word: fieldCount, or gcBinary|dwordCount
call $package'elena'1                     ; allocate
[eax − 4] = __arg2vmt                     ; install the class VMT
esi = eax
repeat ecx times: [esi] = 'nil; esi += 4  ; initialise every field to nil
push eax                                  ; leave the new object on the ELENA stack
```

Five specialisations exist purely to unroll the initialisation loop:

| Fields | Template | `elena.asm` |
|---|---|---|
| 0 | `elena'15` | `:697` |
| 1–2 | `elena'12` | `:658` |
| 3–4 | `elena'13` | `:669` |
| 5–6 | `elena'14` | `:682` |
| >6 | `elena'11` (loop) | `:642` |

Binary objects (strings, byte dumps) take a different path: the field count is negative,
`compileCreate` converts it to `gcBinary | ((size+3)>>2)` (`x86jitcompiler.cpp:394-398`),
or the library calls `standard'38` / `standard'82` / `standard'39` directly
(`standard.asm:1443`, `:2785`, `:1468`), which `or [eax−8], 'gc_binary` after allocating.

**Temporal (stack-allocated) objects** have no creation routine — the compiler simply
reserves stack space and writes a header. They become heap objects lazily, the first time
they are stored into a field, via `elena'32` (§4.5).

---

## 7. Arithmetic and primitives

### 7.1 Representation

| ELENA type | Storage | Header |
|---|---|---|
| `intnumber` | 1 dword | field count 1 |
| `longnumber` | 2 dwords (lo, hi) | field count 2 |
| `realnumber` | IEEE-754 binary64, 2 dwords | field count 2 |
| `literal` | `[len:dword][UTF-16 chars][0:word]` | `gcBinary` set |
| byte dump | raw bytes | `gcBinary` set |
| array | N object pointers | field count N |

`len` is in **characters**, not bytes (`standard.asm:10`, `:90`).

### 7.2 Calling shape of the arithmetic primitives

Nearly every `standard'N` is an `#inline` template with the shape:

```
pop eax            ; source operand object
mov edx, [eax]     ; its value
mov eax, [esp]     ; destination operand object (left on the stack)
<operate on [eax]>
                   ; eax is the result → non-zero = success, or xor eax,eax = failure
```

Predicates (`INT_EQUAL`, `WSTR_LESS`, `FLOAT_LESS`, …) return the destination object on
true and `0` on false — reusing the dispatch failure protocol as the boolean protocol.

### 7.3 int32 / int64

int32 is plain x86 (`add`, `sub`, `imul`/`mul`, `cdq`+`idiv`, `and`/`or`/`xor`,
`shl`/`shr` with the sign of the shift count selecting direction — `standard.asm:258-265`).

int64 is emulated: `add`/`adc` and `sub`/`sbb` for ±; `standard'52` does a three-partial-
product multiply with a 32×32 fast path (`standard.asm:1739-1772`); `standard'53` is a full
signed 64/64 division using shift-normalisation plus a multiply-back correction step
(`standard.asm:1776-1879`). `standard'56` uses `shld`/`shrd` with a ≥32 special case.

**Every one of these maps directly onto an LLVM `i64` operation** — `add`, `sub`, `mul`,
`sdiv`, `shl`, `ashr` — and 200+ lines disappear.

### 7.4 real64 — x87 only

The runtime uses the **x87 FPU exclusively**; there is no SSE anywhere. Notable consequences:

* Comparisons go through `fcomp` + `fnstsw ax` + `test ah, 44h` / `41h`
  (`standard.asm:690-696`, `:711-717`) — the pre-`fcomi` idiom.
* Rounding and truncation save and restore the x87 **control word** by hand
  (`standard'66` at `:2268-2276`, `standard'67` at `:2304-2312`).
* `sin`/`cos` do their own range reduction with `fprem` in a loop (`:2365-2373`,
  `:2467-2475`), because `fsin`/`fcos` are undefined for |x| ≥ 2⁶³.
* `exp` and `ln` are built from `f2xm1`/`fscale`/`fyl2x` (`:2393-2418`, `:2438-2440`).
* `standard'36` (`ftoa`, 355 lines) converts via `fbstp` to an 80-bit packed BCD, unpacks it
  nibble by nibble, and formats either regular or scientific notation with manual
  trailing-zero trimming (`:1101-1111`).
* `standard'37` (`atof`, 291 lines) does the reverse: builds a packed-BCD string, `fbld`s
  it, then applies the decimal exponent through `fldl2t`/`f2xm1`/`fscale`.

**The x87 stack is a hidden part of the ABI.** Several templates leave a value on `st(0)`
across a `call` (e.g. `standard'66` at `:2264-2284`). Any reimplementation that mixes SSE
and x87, or that lets LLVM allocate x87 registers, must ensure the stack is balanced at
every boundary.

### 7.5 Strings and arrays

String routines are straight loops over 16-bit units. Two patterns recur and are worth
noting as porting hazards:

* `mov ebx,[esi]` followed by `cmp bx, word ptr [edx]` (`standard.asm:18-19`) reads **four**
  bytes to compare two — this reads one word past the end of a string at the last position.
  It is safe only because every allocation is 16-byte padded.
* Copy loops move dwords (`standard.asm:96-100`), relying on the same padding.

Array element writes go through the write barrier: `standard'42` (`:1532-1562`) and
`standard'44` (`:1578-1604`) both `call @"$package'elena'31"`. Element *reads*
(`standard'41`, `:1507`) are bounds-checked against `[obj−8]` and return `0` out of range.

---

## 8. Win32 dependency layer

Complete list of external symbols, with the POSIX / macOS replacement for each.

### 8.1 kernel32 — memory, process, console, files

| API | Used by | POSIX / macOS equivalent |
|---|---|---|
| `GetProcessHeap` | `elena.asm:511`, `:1206` | — (fold into the next row) |
| `HeapAlloc` | `elena.asm:513`, `:1208` | `mmap(PROT_READ\|PROT_WRITE, MAP_PRIVATE\|MAP_ANONYMOUS)`; `VirtualAlloc` on Windows |
| `ExitProcess` | `elena.asm:546`, `:1269` | `_exit(2)` / `exit(3)` |
| `RaiseException` | `elena.asm:201` | `abort(3)`, or a `longjmp`/C++ throw for a recoverable OOM |
| `GetStdHandle` | `win32.asm:8`, `:85` | fds `0`/`1`/`2` — no call needed |
| `WriteConsoleW` | `win32.asm:35` | `write(1, …)` after `wcstombs`/`iconv` to UTF-8 |
| `ReadConsoleW` | `win32.asm:59` | `read(0, …)` + UTF-8 → UTF-16 |
| `ReadConsoleInputW` | `win32.asm:111`, `:1146` | `tcsetattr` raw mode + `read`; `ncurses` `getch` |
| `GetNumberOfConsoleInputEvents` | `win32.asm:1131` | `ioctl(FIONREAD)` |
| `WideCharToMultiByte` | `win32.asm:1185` | `iconv` / `wcstombs` / ICU |
| `CreateFileW` | `win32.asm:235` | `open(2)` |
| `ReadFile` / `WriteFile` | `win32.asm:265` / `:297` | `read(2)` / `write(2)` |
| `CloseHandle` | `win32.asm:316` | `close(2)` |
| `GetCommandLineW` | `win32.asm:330`, `:351` | `argv` from `main`; `/proc/self/cmdline`; `_NSGetArgv` |
| `GetSystemTime` + `SystemTimeToFileTime` | `extended.asm:15-16`, `:71-72` | `clock_gettime(CLOCK_REALTIME)` |

**`VirtualAlloc` is not used** — the heap comes from `HeapAlloc` on the process heap. A
port should use `mmap`/`VirtualAlloc` directly, which also gives page-granularity control
needed for a card table.

### 8.2 user32 — windowing

| API | Used by | Cross-platform replacement |
|---|---|---|
| `RegisterClassW`, `CreateWindowExW`, `DestroyWindow` | `win32.asm:993`, `:976`, `:406` | GTK / Qt / SDL / Cocoa `NSWindow` |
| `DefWindowProcW` | `elena.asm:1439` | toolkit default handler |
| `GetWindowLongW`, `SetWindowLongW` | `elena.asm:1349`, `:1360` | per-window user-data pointer |
| `GetMessageW`, `PeekMessageW`, `WaitMessage` | `win32.asm:1099`, `:617`, `:652` | `g_main_loop_run`, `QApplication::exec`, `NSApplication run` |
| `TranslateMessage`, `DispatchMessageW`, `IsDialogMessage` | `win32.asm:640`, `:641`, `:634` | toolkit event dispatch |
| `SendMessageW`, `PostMessageW`, `PostQuitMessage` | `win32.asm:472`,`:669`,`:687`,`:703`,`:1275` | toolkit signals / `quit()` |
| `ShowWindow`, `EnableWindow`, `IsWindowVisible`, `IsWindowEnabled` | `:438`,`:453`,`:1114`,`:1260` | widget show/hide/sensitive |
| `SetWindowPos`, `GetClientRect` | `:544`, `:568`, `:511` | widget geometry |
| `InvalidateRect`, `UpdateWindow`, `BeginPaint`, `EndPaint` | `:392`,`:395`,`:812`,`:831` | `queue_draw` / `NSView setNeedsDisplay` |
| `LoadCursorW`, `SetCursor` | `:594`, `:598` | `gdk_cursor_new`, `NSCursor` |
| `LoadImageW` | `:1037` | image-loading library |
| `GetSysColor`, `GetSysColorBrush` | `:920`, `:937`, `:910` | theme API |
| `FillRect` | `:1412` | canvas fill |

### 8.3 gdi32 — drawing

| API | Used by | Replacement |
|---|---|---|
| `GetDC`, `CreateCompatibleDC`, `DeleteDC` | `win32.asm:418`, `:873`, `:779` | Cairo `cairo_t`, Core Graphics `CGContextRef`, Skia `SkCanvas` |
| `CreateCompatibleBitmap`, `SelectObject`, `DeleteObject`, `GetObjectA` | `:1012`,`:880`,`:794`,`:847` | surface / image objects |
| `CreatePen`, `CreateSolidBrush` | `:1060`, `:1078` | stroke / fill styles |
| `MoveToEx`, `LineTo`, `BitBlt`, `TextOutW` | `:1325`,`:1351`,`:1304`,`:1383` | `cairo_move_to`/`line_to`/`set_source_surface`/`show_text` |
| `SetTextColor`, `SetBkColor`, `SetBkMode` | `:721`, `:761`, `:742` | source colour + operator |

### 8.4 Ws2_32 — sockets

`WSAStartup` (`winsock.asm:56`) and `WSACleanup` (`:40`) have **no POSIX counterpart** —
they simply disappear. Everything else is BSD sockets under a different name:

| Winsock | POSIX |
|---|---|
| `socket`, `bind`, `listen`, `accept`, `connect`, `send`, `recv` | identical names in `<sys/socket.h>` |
| `closesocket` (`:70`) | `close(2)` |
| `gethostname`, `gethostbyname`, `inet_addr` (`:210`,`:214`,`:332`) | `gethostname`, **`getaddrinfo`** (`gethostbyname` is deprecated), `inet_pton` |
| `WSAAsyncSelect` (`:23`) | `epoll`/`kqueue`/`poll` + an event-loop integration |

`SOMAXCONN` is defined as `7fffffffh` (`winsock.asm:7`) — an absurd backlog that Linux
silently clamps to `net.core.somaxconn`.

### 8.5 Non-API platform dependencies

These do not appear in the import table but are just as platform-specific:

| Dependency | Location | Replacement |
|---|---|---|
| `FS:[4]` / `FS:[8]` — TIB StackBase/StackLimit | `elena.asm:1052-1053`, `:1109`, `:1166-1167` | `pthread_attr_getstack` cached in TLS; `pthread_get_stackaddr_np` on macOS |
| `WNDPROC` ABI (`stdcall`, 4 args at `[ebp+8..14h]`) | `elena.asm:1337-1441` | toolkit callback signature |
| `GWL_USERDATA` = `0FFFFFFEBh` (−21) | `elena.asm:1347`, `:1358` | per-widget user data |
| `stdcall` for every import (callee pops) | all `call 'dlls'…` | `cdecl` on POSIX — argument cleanup moves to the caller |
| UTF-16 as the native string encoding | throughout `standard.asm` | UTF-8 on POSIX; a conversion layer or a representation change |

---

## 9. Startup, entry point and shutdown

### 9.1 Console — `elena'3` (`elena.asm:490-550`)

```
 1. for i in 0..gc_static_size:  statroots[i] = 'nil          ; :493-501
 2. total = gcsize*16 + gcsize*4                              ; :503-507
 3. HeapAlloc(GetProcessHeap(), 0x0D, total)                  ; :509-513
 4. heap_start = base + gcsize*4                              ; skip the fixup table, :515-518
    yg = mg = og = heap_start                                 ; :519-521
    mgptr2 = ogptr2 = mgptr2_end = ogptr2_end = heap_start−4   ; :523-527
    heap_end = heap_start + gcsize*16                          ; :529-532
 5. push 0; push 0;  gs_current_frame = esp                    ; seed the root chain, :534-537
 6. edi = 1;  push edi;  call @'starter                        ; :539-541
 7. lea esp,[esp+12];  ExitProcess(0)                          ; :542-546
```

`'starter` is `STARTUP_CLASS` (`elenaconst.h:31`), resolved through the project's
`'entry` forward (`bin/templates/console.cfg` → `sys'templates'simple`).

Selected by `bin/templates/console.cfg` `[project] start=$package'elena'3`, loaded by
`Linker::createImage` (`linker.cpp:302-307`) and written to
`IMAGE_OPTIONAL_HEADER.AddressOfEntryPoint` (`linker.cpp:454`).

### 9.2 GUI — `elena'35` (`elena.asm:1185-1273`)

Identical steps 1–5, then:

```
 6. call @win32'api'instance                                   ; :1236
 7. call @std'basic'intnumber; store [ebp+8] (hInstance)        ; :1238-1242
 8. hand-inlined dispatch of #win32'api'$sethandle              ; :1244-1259
 9. call @'starter; ExitProcess(0)                              ; :1262-1269
```

Step 8 is a **fourth copy of the VMT scan loop**, written out longhand rather than reusing
`elena'7`. Selected by `bin/templates/gui.cfg` `start=$package'elena'35`.

### 9.3 The window procedure — `elena'37` (`elena.asm:1335-1451`)

The only place the runtime is re-entered from foreign code:

```
push ebp; mov ebp,esp
push [gs_current_frame]; push 0; gs_current_frame = esp     ; NEW GC frame record, :1342-1345
eax = GetWindowLongW(hwnd, GWL_USERDATA)                     ; :1347-1349
if eax == 0:
    if msg == WM_CREATE(1): SetWindowLongW(hwnd, GWL_USERDATA, [[lparam]])   ; :1352-1360
    goto default
ebx = [['system]] + eax                                      ; handler slot, :1365-1367
edx = #win32'api'$onuserevent
if msg < 0x400:  linear-search elena'38 for msg → edx        ; :1372-1382
push lparam, wparam, msg;  eax = esp
push "win32'api'message"                                     ; VMT for the message object
push 80000003h                                               ; PROCEED_MESSAGE_ID
push [ebx]                                                   ; the handler object
<inline VMT scan>;  call [esi−4]                             ; :1400-1418
if eax != 0: result = [eax]; goto exit
default: restore gs_current_frame; DefWindowProcW(...)       ; :1429-1441
```

Note that a **synthetic `win32'api'message` object is built on the machine stack**
(`:1385-1392`) — a temporal object whose VMT is a class reference. It is visible to the GC
only because `gs_current_frame` was set to `esp` on entry.

`elena'38` (`:1453-1481`) is the `WM_*` table:

| `WM_` | Value | ELENA message |
|---|---|---|
| `WM_DESTROY` | 0x0002 | `$ondestroy` |
| `WM_SETFOCUS` | 0x0007 | `$onsetfocus` |
| `WM_PAINT` | 0x000F | `$onpaint` |
| `WM_CLOSE` | 0x0010 | `$onclose` |
| `WM_SETCURSOR` | 0x0020 | `$onsetcursor` |
| `WM_KEYDOWN` | 0x0100 | `$onkeydown` |
| `WM_CHAR` | 0x0102 | `$onchar` |
| `WM_COMMAND` | 0x0111 | `$oncommand` |
| `WM_CTLCOLORBTN` | 0x0135 | `$onsetcolorbutton` |
| `WM_CTLCOLORSTATIC` | 0x0138 | `$onsetcolorstatic` |
| `WM_LBUTTONDOWN` | 0x0201 | `$onlbutton` |
| sentinel | 0xFFFF | 0 |

### 9.4 Shutdown

There is **no shutdown path**. `ExitProcess(0)` is called unconditionally after `'starter`
returns (`elena.asm:544-546`, `:1267-1269`). The heap is never freed, no finalizers run, no
handles are closed, and the exit code is hard-coded to 0. `winsock'2` (`WSACleanup`) exists
but is only invoked if ELENA code calls it explicitly.

---

## 10. Calling conventions and register discipline

### 10.1 The internal ABI

| Register | Role |
|---|---|
| `eax` | **self / receiver** on entry to a method; **result** on exit. `0` = message failed. Also the universal scratch register |
| `ebx` | **message parameter** (the single argument of a send). Also scratch in primitives |
| `ecx` | Counters, sizes. Always scratch |
| `edx` | **message id** during dispatch. Otherwise scratch |
| `esi` | VMT entry cursor during dispatch; source pointer in copy loops. Scratch |
| `edi` | **`$self`** — the current class instance, established by `sprep` (`elena.asm:563-564`). **Callee-saved**: every routine that uses it as scratch pushes and pops it |
| `ebp` | Frame pointer. Parameters of `#external` procedures at `[ebp+4]`, `[ebp+8]`, … |
| `esp` | The **ELENA object stack** — arguments and intermediate objects are pushed here. `[esp]` is the current top object |

Two distinct stacks are overlaid on `esp`: the machine call stack *and* the ELENA operand
stack. `bcPush`/`bcMove` in the JIT (`x86jitcompiler.cpp:152-197`, `:248-301`) address
locals as `[ebp − level*4]` and operands as `[esp − level*4]`.

### 10.2 Frame shapes

**Plain method (`prep`/`return`)** — `elena.asm:553`, `:571`:
```
    [ebp+8]  ...  caller's pushed operands
    [ebp+4]  return address
    [ebp+0]  saved ebp        ← ebp
    [ebp-4]  local 0
```

**Instance method (`sprep`/`sexit`/`sreturn`)** — `elena.asm:561`, `:610`, `:621`:
```
    [ebp+8]  return address
    [ebp+4]  saved edi        ($self of the caller)
    [ebp+0]  saved ebp        ← ebp,  edi = this instance
    [ebp-4]  local 0
```

**Redirect method (`prepredir`/`exitredir`)** — `elena.asm:724`, `:735`:
```
    [ebp+12] return address
    [ebp+8]  saved edx        (the message id — kept live for elena'19/'20)
    [ebp+4]  saved edi
    [ebp+0]  saved ebp        ← ebp
```

The JIT tracks the difference in `scope.prevFSPOffs` (4 / 8 / 0xC,
`x86jitcompiler.cpp:124-150`).

**Return protocol.** `return` (`elena.asm:571-576`) pops the result, restores `esp`/`ebp`,
then does `mov [esp+4], eax` before `ret` — i.e. the result **overwrites the receiver slot**
the caller had pushed. The caller therefore never pops the receiver; the value it finds at
`[esp]` after the call *is* the result. This is the ELENA operand-stack calling convention
and is the reason every primitive begins with `pop eax` / `mov eax,[esp]`.

### 10.3 External calls

`#external` routines are wrapped by `elena'16` (`elena.asm:707-721`), which:

1. saves `edi`;
2. saves the current `gs_current_frame` on the stack;
3. **publishes the live stack extent** into the frame record (`[eax] = eax − esp`) so a GC
   triggered inside the callee can see every operand pushed so far;
4. saves `ebp`, sets `ebp = esp + 8`;
5. `call __arg1fun`;
6. restores `ebp`, `gs_current_frame`, `edi`.

Parameters are pushed by the ELENA caller in declaration order, so the *first* declared
parameter is deepest — `[ebp + count*4]` — matching
`x86assembler.cpp:390-393`.

Imports use **stdcall** (`call 'dlls'lib.Func` → `FF 15 <IAT slot>`), so the callee pops
its own arguments. Several routines nonetheless adjust `esp` manually around scratch
buffers allocated with `lea esp,[esp-N]` (`win32.asm:102`, `:506`, `:1137`;
`winsock.asm:50`, `:206`, `:318`) — a pattern that would break under any calling convention
requiring 16-byte stack alignment (i.e. every 64-bit ABI, and macOS 32-bit).

---

## 11. Reimplementation plan

### 11.1 Target form for each subsystem

| # | Subsystem | ≈ lines | Target form | Difficulty | Notes |
|---|---|---:|---|---|---|
| 1 | Frame prologue/epilogue (`elena'4`–`'6`, `'8`, `'9`, `'17`, `'18`) | 60 | **Deleted** | trivial | LLVM emits prologues; the operand-stack return protocol becomes an ordinary SSA return value |
| 2 | int32 arithmetic (`standard'3`–`'18`, `'24`–`'26`) | 250 | **LLVM IR intrinsics** | trivial | Direct `add`/`sub`/`mul`/`sdiv`/`and`/`or`/`xor`/`shl`/`ashr` on `i32` |
| 3 | int64 arithmetic (`standard'31`, `'46`–`'63`) | 480 | **LLVM IR on `i64`** | trivial | LLVM's own `__udivti3`-style lowering replaces `standard'52`/`'53` entirely. **Fixes the three known bugs in §2.8 for free** |
| 4 | real64 arithmetic (`standard'28`–`'35`, `'65`, `'67`–`'74`) | 400 | **LLVM IR + `llvm.*` intrinsics** | easy | `fadd`/`fsub`/`fmul`/`fdiv`, `llvm.sqrt`, `llvm.sin`, `llvm.cos`, `llvm.exp`, `llvm.log`, `llvm.fabs`, `llvm.trunc`, `llvm.rint`. **Switch to SSE2** — the x87 stack disappears |
| 5 | `ftoa`/`atof` (`standard'36`, `'37`) | 646 | **Portable C** | easy | `snprintf("%.17g")` / `strtod`, or Ryu/Grisu for exact round-trips. Biggest single line-count win |
| 6 | Strings (`standard'1`–`'5`, `'19`–`'23`, `'64`, `'77`) | 480 | **Portable C** | easy | Also the moment to decide UTF-16 vs UTF-8. Keeping UTF-16 keeps the object layout; switching costs a library rewrite but removes `WideCharToMultiByte` |
| 7 | Arrays / dumps (`standard'38`–`'45`, `'75`–`'82`) | 420 | **Portable C + IR** | easy | Bounds checks become IR; the barriered store calls into the GC |
| 8 | Object creation (`elena'11`–`'15`) | 65 | **LLVM IR inline** | easy | `alloc` call + `store` VMT + `memset`/unrolled `store` of nil |
| 9 | Allocation fast path (`elena'1` fast path) | 15 | **LLVM IR inline** (TLAB bump) | medium | Must become thread-local for MT; the slow path calls into C |
| 10 | Sockets (`winsock'*`) | 362 | **Portable C over BSD sockets** | easy | Only `WSAStartup`/`WSACleanup`/`WSAAsyncSelect` need `#ifdef` |
| 11 | Console + file I/O (`win32'1`–`'14`, `'53`, `'54`) | 350 | **Portable C + thin `#ifdef`** | easy | `open`/`read`/`write`/`close`; termios for raw key input |
| 12 | Clock / RNG (`extended'*`) | 76 | **Portable C** | trivial | `clock_gettime`, PCG or xoshiro |
| 13 | Entry / shutdown (`elena'3`, `'35`) | 150 | **Portable C `main`** | easy | Add a real shutdown path: finalizers, handle close, exit code |
| 14 | Roles (`elena'28`, `'29`), identity (`'22`, `'24`), stack ops (`'25`, `'26`) | 72 | **LLVM IR inline** | easy | A VMT-pointer store; blocks devirtualisation, so mark the slot non-invariant |
| 15 | **Dynamic dispatch** (`elena'7`, `'19`–`'21`, `'23`, `'30`, `'36`) | 300 | **IR + a C slow path** | **hard** | See §11.2 |
| 16 | **Garbage collector** (`elena'1` slow path, `'2`, `'31`–`'34`) | 600 | **Portable C**, with 2 platform shims | **hard** | See §11.3 |
| 17 | GUI (`elena'37`, `'38`, `win32'15`–`'52`, `'55`–`'63`) | 1 100 | **Replaced by a toolkit binding** | medium | Not a port — a rewrite against GTK/Qt/Cocoa. Consider dropping from v2 scope entirely |

### 11.2 Hard part 1 — dispatch performance

The current send is a linear scan over a sorted array, one to three cache lines per call,
with a chain walk on miss. Naively replacing it with a C function call
(`elena_send(obj, msgid, param)`) will be **slower**, because today the scan is inlined into
the caller with zero call overhead.

Recommended shape:

| Layer | Mechanism |
|---|---|
| Monomorphic sites | **Inline cache** in IR: compare the receiver's VMT against a cached VMT global; on hit, direct `call`; on miss, fall to the next layer. This is the single largest win and did not exist in 2009 |
| Static sends (`ircall`) | Direct `call` — the VMT is known at compile time |
| Polymorphic sites | Emit the scan as IR (binary search over the sorted entry array — O(log n) instead of O(n)) |
| Megamorphic / any-handler | C helper `elena_send_slow()` |

Constraints the design must respect:

* **The VMT pointer is mutable** (`elena'28`, roles). Inline caches must be invalidated or
  keyed on the current VMT value read fresh each time — never hoisted out of a loop.
* **Message ids are signed and predefined ones are negative** (`0x8000000x`). A binary
  search must use signed comparison, matching `jitcompiler.cpp:90`.
* **The failure protocol is `eax == 0` plus a branch.** Every send site has a
  "not handled" successor. Preserve this as an explicit second return value or a branch on a
  sentinel; do not replace it with exceptions without auditing every `#inline` predicate
  (§7.2), which reuses the same protocol for booleans.
* The `+0` / `+8` start-offset hack (§5.2) and its `call 0` hazard must be resolved
  explicitly, not carried forward.

### 11.3 Hard part 2 — GC root scanning

This is the highest-risk item in the whole modernization.

**The current design cannot be kept.** Region-precise/slot-conservative scanning
(§4.3) works only because the heap is 64 KiB and every integer is boxed. Under LLVM, values
live in registers and spill slots the runtime knows nothing about, and a larger heap makes
false positives both more likely and more destructive (the collector *moves* objects).

Two viable strategies:

| Strategy | Mechanism | Cost | Notes |
|---|---|---|---|
| **A. Precise via LLVM statepoints** | `gc "statepoint-example"`, `llvm.experimental.gc.statepoint` / `.relocate`, `StackMaps` section | Highest engineering cost; needs the whole codegen to be statepoint-aware | The only option that allows a *moving* collector with a large heap. This is what Java/Go/CLR do |
| **B. Shadow stack** | `gc "shadow-stack"` — LLVM's built-in, each frame links a root array | Low cost, ~5–15 % slower | Conceptually **identical to the existing `gs_current_frame` chain**, but precise instead of conservative. Natural migration path |

**Recommendation: start with B, keep the door open for A.** The shadow stack is a direct
generalisation of what `elena.asm:28-31` already does — the difference is that the compiler
enumerates *which* slots are roots instead of the collector guessing. That single change
makes the collector precise, removes the `FS:[4]/FS:[8]` probe's role in correctness, and
makes a per-thread root chain trivial (one shadow-stack head per thread in TLS).

Other GC decisions to make deliberately:

| Decision | Current | Recommendation |
|---|---|---|
| Heap sizing | fixed 64 KiB, never grows | `mmap` a large reservation, commit on demand |
| Collector shape | sliding mark-compact with a page-indexed fixup table | Copying young gen + mark-compact old gen, or an off-the-shelf design |
| Barrier | linear-scan remembered set with no capacity bound | **Card table** — O(1) barrier, page-granular, MT-safe with a plain byte store |
| Temporal objects (`elena'32`) | copy-on-store detected by stack-range probe | Escape analysis in the compiler, or always heap-allocate and let the GC handle it |
| Finalization | none | Add it — file handles, DCs and sockets currently leak by design |
| Multithreading | impossible (§4.7) | TLABs + safepoints + per-thread shadow stacks, designed in from day one |

### 11.4 Hard part 3 — failure/exception semantics

The runtime has **no exception mechanism**. "Message not understood", out-of-range array
access, `atof` parse failure and arithmetic overflow all produce the same signal: `eax = 0`
and a branch to an alternative path (§5.1, §7.2). Only one condition is fatal — heap
exhaustion, which raises Win32 exception `0x190` (`elena.asm:193-202`).

This is not a bug to fix in passing. It is a **language semantic**: `#inline standard'6`
returns the object or zero, and ELENA source relies on it. Decide explicitly:

* **Keep it** — model every primitive as returning `{ptr, i1}` in IR and branch on the flag.
  Lowest risk, preserves all existing library code.
* **Replace it** — introduce real exceptions (`invoke`/`landingpad`, or a Result type) and
  rewrite every `#inline`/`#external` call site in `src/*.l`. Higher risk, better long-term.

Whichever is chosen, the OOM path must become recoverable rather than
`RaiseException(0x190)`.

### 11.5 Suggested sequencing

| Phase | Work | Deletes |
|---|---|---|
| 0 | Build a C runtime library skeleton; make `#external` able to call it | — |
| 1 | Port `extended.asm`, `winsock.asm`, `win32.asm` console + file sections to C | 800 lines |
| 2 | Port `standard'36`/`'37` (ftoa/atof) and the string routines to C | 1 100 lines |
| 3 | Move int32/int64/real64 to LLVM IR; switch real64 from x87 to SSE2 | 1 100 lines |
| 4 | Replace the frame templates and `ocreate` with LLVM-emitted code | 125 lines |
| 5 | **Introduce the shadow stack**; rewrite the GC in C against it | 600 lines |
| 6 | **Rewrite dispatch** with inline caches + binary search | 300 lines |
| 7 | GUI: replace `elena'37` and the `win32` windowing/GDI sections with a toolkit binding | 1 100 lines |
| 8 | Delete `elenasrc/asm2bin`, `src/asm`, `elenasrc/engine/win32/x86helper.*`, `[primitives]` | 3 335 + 6 146 lines |
| 9 | Add multithreading: TLABs, safepoints, per-thread roots, atomic barriers | — |

Phases 1–4 are mechanical and independently testable. Phase 5 and 6 are where the real
design work is. Phase 9 is only possible *after* 5 and 6 — attempting it earlier is what
would make the project fail.
