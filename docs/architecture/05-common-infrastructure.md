# 05 — Shared C++ Infrastructure (`elenasrc/common/`)

**Subject:** ELENA Language v1.5.0.0 (2009) — the shared C++ utility layer.
**Audience:** engineers planning the cross-platform / LLVM / multithreading / new-GC modernization.
**Status of the code:** C++98, Windows-only, built with MinGW (CodeBlocks `.cbp`) and Visual Studio 7/8/9 (`.vcproj`).

---

## Table of contents

1. [Overview & dependency graph](#1-overview--dependency-graph)
2. [Container library (`lists.h`)](#2-container-library-listsh)
3. [String handling (`altstrings.h`)](#3-string-handling-altstringsh)
4. [Streams & memory (`streams.h`, `dump.h`)](#4-streams--memory-streamsh-dumph)
5. [File system (`files.h`)](#5-file-system-filesh)
6. [Config parsing (`config.h`)](#6-config-parsing-configh)
7. [Type & macro conventions (`common.h`, `tools.h`)](#7-type--macro-conventions-commonh-toolsh)
8. [Portability audit](#8-portability-audit)
9. [Modernization recommendations](#9-modernization-recommendations)
10. [Appendix A — latent bugs found while reading](#appendix-a--latent-bugs-found-while-reading)

---

## 1. Overview & dependency graph

### 1.1 What the layer is

`elenasrc/common/` is a **self-contained, dependency-free replacement for the C++ standard library**, written between 2005 and 2009. It deliberately avoids `<string>`, `<vector>`, `<map>`, `<fstream>` and `<iostream>` entirely. Nothing in the tree includes an STL header. The author instead built:

| Concern | STL equivalent avoided | Local replacement |
|---|---|---|
| Containers | `<list> <map> <vector> <stack> <queue> <unordered_map>` | `lists.h` |
| Strings | `<string>` | `altstrings.h` / `altstrings.cpp` |
| I/O | `<iostream> <fstream> <sstream>` | `streams.h`, `files.h` / `files.cpp` |
| Byte buffers | `std::vector<char>` | `dump.h` / `dump.cpp` |
| Config | (none) | `config.h` / `config.cpp` |
| Utilities | `<algorithm> <cstring>` wrappers | `tools.h` |
| Encoding | `<codecvt>` (didn't exist) | `win32/unicode.h` |

Everything lives in `namespace _ELENA_` **except** `win32/unicode.h`, whose two functions are at global scope (`win32/unicode.h:17`, `win32/unicode.h:22`).

### 1.2 The umbrella header

`common.h` is a **single umbrella header** that pulls in the whole layer in a fixed order. There are no per-file include guards protecting against reordering — each header assumes its predecessors are already included.

```
common.h:14-18   <tchar.h> <stdio.h> <stdlib.h> <string.h> <io.h>
common.h:21      #define ref_t       size_t
common.h:22      #define DEFAULT_STR (const TCHAR*)NULL
common.h:25      #include "win32\unicode.h"     <-- backslash separator!
common.h:26      #include "tools.h"
common.h:27      #include "altstrings.h"
common.h:28      #include "streams.h"
common.h:29      #include "dump.h"
common.h:30      #include "lists.h"
common.h:31      #include "files.h"
```

**This ordering is load-bearing.** For example `lists.h` uses `MemoryDump` (`lists.h:1450`) and `StreamWriter` (`lists.h:1371`) without declaring them, and `altstrings.h` uses `emptystr` / `getlength` / `strdup` from `tools.h` (`altstrings.h:32-34`, `altstrings.h:88`). Individual headers cannot be compiled standalone.

`config.h` is the only header **not** in the umbrella; it includes `common.h` itself (`config.h:11`).

### 1.3 Internal dependency graph

```
                       <tchar.h>, <io.h>, <windows.h>   (Win32 CRT + API)
                                   |
                        win32/unicode.h  (MultiByteToWideChar / WideCharToMultiByte)
                                   |
                              tools.h  (strdup, compstr, getlength, align, freestr)
                                   |
                  +----------------+----------------+
                  |                                 |
            altstrings.h                        (used by all below)
          (_String, String,                          |
           LocalString<N>, Quote<>,                  |
           ReferenceNameTemplate<>)                  |
                  |                                  |
              streams.h  (StreamReader/Writer, TextReader/Writer,
                  |       LiteralReader/Writer)
                  |
               dump.h  (MemoryDump, DumpReader, DumpWriter)
                  |
              lists.h  (List, Map, MemoryMap, HashTable, MemoryHashTable,
                  |     Stack, Queue, BList, CList, Cache, CachedMemoryMap,
                  |     Dictionary2D)
                  |
               files.h  (PathTemplate<>, File, FileReader/Writer,
                  |      TextFileReader/Writer)
                  |
              common.h  (_ConfigFile abstract base, ConfigSettings typedef)
                  |
               config.h  (IniConfigFile)
```

Key structural observations:

- **`lists.h` depends on `dump.h` and `streams.h`.** `MemoryMap`, `CachedMemoryMap` and `MemoryHashTable` are all backed by a `MemoryDump` and can serialize themselves through `StreamWriter`/`StreamReader`. This is a hard, circular-ish coupling: the container library knows about the serialization format.
- **`lists.h` depends on `tools.h`** for key comparison — `_MapItem::operator==` calls `compstr` (`lists.h:129`) and `grtstr` (`lists.h:139`).
- **`lists.h` has an unresolved forward dependency on the engine.** `Map::write` (`lists.h:1376`) and `HashTable::write` (`lists.h:2080`) call `_writeIterator(...)`, and `Map::read` (`lists.h:1389`) / `HashTable::read` (`lists.h:2093`) call `_readToMap(...)`. **Neither is defined in `common/`.** They are defined as *friend* functions in the engine, at `elenasrc/engine/section.h:49` and `elenasrc/engine/section.h:66`. This only compiles because the templates are never instantiated with `write`/`read` outside the engine. **This is a layering violation: the common library calls up into the engine.**

### 1.4 Which subproject uses what

**There is not a single path-qualified include anywhere in `elenasrc/`.** No file writes `#include "../common/lists.h"`. Every subproject relies entirely on the build system adding `../../common` to the include path (`<Add directory="..\..\common" />` in the `.cbp` files, `AdditionalIncludeDirectories` in the `.vcproj`/`.vcxproj` files). Consequently the question "which headers does subproject X use?" collapses: there are only **three entry points** into the layer —

| Entry point | Location | Pulls in |
|---|---|---|
| `common.h` | included directly by ide, api2html, plugins | all 7 headers |
| `config.h` | includes `common.h` at `config.h:11` | all 7 |
| `engine/elena.h` | includes `common.h` at `elenasrc/engine/elena.h:12` | all 7 |

— and anyone reaching any of them gets **all seven headers transitively**. What actually differs between subprojects is (a) which `common/*.cpp` translation units are linked in, and (b) which templates are instantiated.

| Subproject | Output | Entry point | `common/*.cpp` compiled in | Heaviest use of |
|---|---|---|---|---|
| **elc** (compiler) | `bin/elc.exe` | `elc/elc.h:14` → `config.h`; `elc/*.cpp:9` → `elena.h` | **all 4** — `elc.cbp:40,51,57,59`; `elc9.vcproj:183,199,207,215` | Everything. Sole user of `HashTable`; `Dictionary2D`, `Map`×6, `Stack`, `List`, `MemoryDump`, `Path`, `IniConfigFile` |
| **engine** (no build file of its own — compiled into elc/ide/asm2bin) | — | `engine/elena.h:12` | none | Heaviest `lists.h` consumer. **Sole user of `Cache`, `CachedMemoryMap`**; `MemoryMap`, `MemoryHashTable`, `Map`, `Stack`, `BList` |
| **ide** | `bin/elide.exe` | `ide/win32/idecommon.h:23,24` → `common.h`+`config.h` | **all 4** — `elide_win32.cbp:55,66,72,74`; `elide9.vcproj:186,198,218,226` | `List`, `Map`, `MemoryMap`, `BList`. **No hash tables.** |
| **asm2bin** | `bin/asm2bin.exe` | `asm2bin/x86assembler.h:12` → `elena.h` | **3** in `.cbp` (`asm2bin.cbp:59,70,74` — *no* `config.cpp`); **4** in VS | `Map`×3, plus `MemoryHashTable`/`Cache` indirectly via `engine/section.h`; `Path`, `FileName`, `TextFileReader` |
| **sg** (syntax generator) | `bin/sg.exe` | `sg/sg.cpp:7` → `elena.h` | **all 4** — `sg.cbp:42,53,59,61` | All *indirect*, via `elc/parsertable.h:25-27`: `MemoryMap`, `MemoryHashTable`×2, `Stack`. Compiles **no** engine `.cpp` at all. |
| **api2html** | `bin/api2html.exe` | `api2html.cpp:7,8` → `common.h`+`config.h` | **3** — `api2html.cbp:36,47,53` (*no* `dump.cpp`) | `Dictionary2D` via `ConfigSettings`, `IniConfigFile`, `TextFileWriter`, `FileName` |
| **idecommon** | header-only (`plugins.h`) | **none** — zero `#include` lines | none | Nothing. Deliberately independent of `common/`. |
| **plugins/autoform** | `autoform.dll` | `plugins/autoform/autoform.h:15` → `common.h` | **none** | **Nothing** — `common.h` is included but entirely unused. |

Build-configuration anomalies worth fixing during the CMake migration:

| Finding | Location | Impact |
|---|---|---|
| `config.cpp` is compiled into **sg** but nothing in sg's compiled set references `config.h` | `sg.cbp:53` | dead weight |
| `config.cpp` is compiled into **asm2bin** by the VS projects but correctly omitted by the `.cbp` — asm2bin never touches `config.h` | `asm2binx9.vcproj:189` | dead weight; the `.cbp` is the correct one |
| `api2html` omits `dump.cpp` — **legitimate**, since `Dictionary2D`/`Map` are pure heap lists and never touch `MemoryDump` | `api2html.cbp` | none |
| `elc.cbp` omits `engine/win32/imagesection.cpp` although `elc9.vcproj:219` includes it | `elc.cbp` | **MinGW build is out of sync with the VS build** |
| `asm2binx.vcproj:132,158`, `asm2binx7.vcproj:132,158`, `sg/vs/sg.vcproj:201,231` reference `..\..\common\section.cpp` / `section.h`, **which do not exist** (moved to `engine/` years earlier) | those three files | stale/broken projects |
| `plugins/autoform` includes `common.h` but links **no** `common/*.cpp` | `autoform.vcproj:176,188` | any use of `String`/`MemoryDump`/`File` would fail to link |

### 1.4b `ide/gtk` is not a port

The presence of `elenasrc/ide/gtk/` and `elenasrc/ide/codeblocks/elide_gtk.cbp` might suggest a prior cross-platform attempt. **It is not one.**

- `elenasrc/ide/gtk/main.cpp` is **49 lines**: the stock GTK+ 2.0 "Hello World" sample (one window, a `GTK_STOCK_DIALOG_INFO` button, a message dialog, a Close button). It includes only `<stdlib.h>` and `<gtk/gtk.h>` and references **nothing** from `common/`, `engine/`, `ide/`, or `idecommon/`.
- `elenasrc/ide/codeblocks/elide_gtk.cbp` is 46 lines whose entire unit list is one line — `elide_gtk.cbp:38`, `<Unit filename="../gtk/main.cpp" />` — with `pkg-config gtk+-2.0` flags. It adds **no include directories at all** (not even `..\..\common`) and lists no common/engine/ide sources.

It is a build-system spike, not a port. Treat it as zero prior art. Note also that a real GTK port is blocked at the very first header anyway: `common.h:14,18` include `<tchar.h>` and `<io.h>`, and `common.h:25` includes `win32\unicode.h` which includes `<windows.h>`.

### 1.4c The `win32/` subdirectories

| Directory | Contents | Role |
|---|---|---|
| `elenasrc/common/win32/` | `unicode.h` only | The **single OS-abstraction point** of `common/` — but it is hard-wired into `common.h:25`, so it is not optional |
| `elenasrc/engine/win32/` | `imagesection.{cpp,h}`, `x86helper.{cpp,h}`, `x86jitcompiler.{cpp,h}` | Mixes two concerns: PE/COFF image sections (OS-specific) and x86 codegen (CPU-specific, *not* OS-specific) — worth separating before the LLVM work |
| `elenasrc/elc/win32/` | `elc.cpp` (the `main`), `linker.{cpp,h}` | PE linker + compiler entry point |
| `elenasrc/ide/win32/` | 33 files: full Win32 GUI + debugger | `ide/win32/idecommon.h` is the umbrella every `ide/*.cpp` includes (17 sites) — the reason the "portable-looking" `ide/*.cpp` files are not portable |
| `elenasrc/plugins/autoform/win32/` | `dllmain.cpp` | DLL entry stub |

### 1.5 Real instantiations found in the tree

These are the concrete template instantiations that any rewrite must keep working:

| Instantiation | Location |
|---|---|
| `MemoryHashTable<ref_t, int, indexReference, cnHashSize>` (`RelocationFixMap`) | `elenasrc/engine/section.h:23` |
| `Map<ref_t, Section*>` (`SectionMap`) | `elenasrc/engine/section.h:27` |
| `MemoryMap<ref_t, ref_t>` (`RelocationMap`) | `elenasrc/engine/section.h:30` |
| `MemoryMap<ref_t, bool, false>` (`MethodMap`) | `elenasrc/engine/elena.h:95` |
| `MemoryMap<const TCHAR*, int, true>` (`FieldMap`) | `elenasrc/engine/elena.h:96` |
| `Map<const TCHAR*, _Module*>` (`ModuleMap`) | `elenasrc/engine/elena.h:193` |
| `MemoryHashTable<const TCHAR*, ref_t, mapReferenceKey, 29>` (`ReferenceMap`) | `elenasrc/engine/elena.h:206` |
| `MemoryHashTable<const TCHAR*, ref_t, mapLiteralKey, 29>` (`ConstantMap`) | `elenasrc/engine/elena.h:207` |
| `Map<const TCHAR*, ref_t, false>` (`MessageMap`) | `elenasrc/engine/elena.h:210` |
| `Stack<int>` (`ParserStack`) | `elenasrc/engine/elena.h:213` |
| `MemoryMap<const TCHAR*, int>` (`SymbolMap`) | `elenasrc/engine/elena.h:214` |
| `MemoryHashTable<size_t, int, syntaxRule, cnHashSize>` (`SyntaxHash`) | `elenasrc/engine/elena.h:215` |
| `MemoryHashTable<size_t, int, tableRule, cnHashSize>` (`TableHash`) | `elenasrc/engine/elena.h:216` |
| `Map<const TCHAR*, const TCHAR*, false>` (`NamespaceMaskMap`) | `elenasrc/elc/compiler.h:22` |
| `Map<const TCHAR*, ref_t, false>` (`ForwardMap`, `LocalMap`) | `elenasrc/elc/compiler.h:23-24` |
| `Map<ref_t, RoleScope*, false>` (`RoleMap`) | `elenasrc/elc/compiler.h:351` |
| `Dictionary2D<int, const TCHAR*>` (`ProjectSettings`) | `elenasrc/elc/project.h:16` |
| `MemoryMap<ref_t, size_t, false>` (`ClassInfoMap`) | `elenasrc/ide/debugcontroller.h:60` |
| `List<_TextWatcher*>` (`TextWatchers`) | `elenasrc/ide/text.h:31` |
| `BList<Page>` (`Pages`) | `elenasrc/ide/text.h:59` |
| `BList<ByteCommand>` | `elenasrc/engine/bytecode.h:177,181` |
| `CachedMemoryMap<…>` | `elenasrc/engine/jitlinker.h:75` |
| `Cache<…>` | `elenasrc/engine/module.h:19`, `elenasrc/engine/win32/x86jitcompiler.h:112` |
| `MemoryMap<int, x86JumpInfo>` | `elenasrc/engine/win32/x86helper.h:278,279` |
| `HashTable<…>` (the **only** instantiation in the tree) | `elenasrc/elc/parsertable.cpp:15` |
| `Stack<int>` | `elenasrc/elc/parsertable.cpp:31,95`, `elenasrc/engine/bccompiler.cpp:629-632` |
| `List<Unresolved>` | `elenasrc/elc/compiler.h:71` |
| `Map<const TCHAR*, size_t>` | `elenasrc/asm2bin/x86assembler.h:29`, `elenasrc/asm2bin/x86jumphelper.h:20,21` |
| `List<TCHAR*>` | `elenasrc/ide/idesettings.h:65,66` |
| `MemoryMap<int, void*>` | `elenasrc/ide/win32/debugger.h:159` |
| `Dictionary2D<const TCHAR*, const TCHAR*>` (`ConfigSettings`) | `elenasrc/common/common.h:36` |
| `Map<const TCHAR*, TCHAR*>` (`CategoryMap`) | `elenasrc/common/common.h:39` |

### 1.6 Dead code: containers that are never instantiated

A full sweep of `elenasrc/` finds **five templates in `lists.h` with zero instantiations anywhere in the tree**:

| Template | Lines | Status |
|---|---|---|
| `Queue<T>` | `lists.h:950-1018` | **Never instantiated.** Also has an uninitialized-member bug (Appendix A #12). |
| `CList<T>` | `lists.h:1083-1155` | **Never instantiated.** The circular-list machinery in `_BList` (`circle`, `shiftNext`, `shiftPrevious`) exists solely to serve it. |
| `_List<T>` / `_BList<T>` | `lists.h:456-797` | Only ever used *through* the public facades — never directly. |
| `_MemoryIterator<>` | `lists.h:384-451` | Only appears inside `MemoryMap`/`CachedMemoryMap`/`MemoryHashTable` typedefs. |

Roughly **250 lines of `lists.h` (9%) can be deleted outright** rather than ported. This materially changes the recommendation for `CList` and `Queue` — see §9.2.

---

## 2. Container library (`lists.h`)

`lists.h` is 2763 lines and contains **19 class templates plus 5 free function templates**. There is a consistent design: an internal `_`-prefixed engine class (`_List`, `_BList`) wrapped by thin public facades (`List`, `Stack`, `Queue`, `BList`, `CList`), and a parallel "memory-backed" family that stores nodes inside a flat `MemoryDump` byte buffer instead of individual heap nodes.

### 2.1 Design axes

Every container in this file sits on three axes:

1. **Node storage** — individually `new`-ed nodes (`_Item`, `_BItem`, `_MapItem`) vs. packed into a relocatable byte buffer (`_MemoryMapItem` inside a `MemoryDump`).
2. **Key ownership** — the `bool KeyStored` template parameter. When `true`, string keys are `strdup`-ed on insert and `free`-d on destruction. When `false`, the container stores the caller's pointer verbatim and never frees it.
3. **Value ownership** — a `void (*_freeT)(T)` function pointer passed to the constructor. If non-`NULL`, it is invoked on every value at `erase`/`clear`/destruction. If `NULL` (the default), values are **never** freed. There is no RAII and no smart pointers anywhere.

> **This dual-ownership model is the single biggest semantic hazard in the codebase.** Whether a `Map` frees its keys and its values is decided by a template non-type parameter and a raw function pointer, not by the type system. `Map<const TCHAR*, Section*>` leaks every `Section*` unless the caller remembered to pass `freeobj`.

### 2.2 Node/item structures

| Struct | Lines | Layout | Notes |
|---|---|---|---|
| `_Item<T>` | `lists.h:29-40` | `T item; _Item* next;` | Singly-linked node. |
| `_BItem<T>` | `lists.h:44-57` | `T item; _BItem* previous; _BItem* next;` | Doubly-linked node. |
| `_MapItem<Key,T,KeyStored=true>` | `lists.h:61-203` | `Key key; T item; _MapItem* next;` | Carries ~14 comparison operator overloads for `int`, `size_t`, `const char*`, `const wchar_t*` (`lists.h:67-165`). Constructor `strdup`s the key if `KeyStored` (`lists.h:179-181`, `lists.h:189-191`); destructor `free`s it (`lists.h:197-202`). |
| `_MemoryMapItem<Key,T,KeyStored>` | `lists.h:207-303` | `size_t next; Key key; T item;` | `next` is a **byte offset into the owning `MemoryDump`**, not a pointer, precisely so the buffer can be `realloc`-ed (see comment at `lists.h:490`). When `KeyStored`, `key` is a *self-relative* offset: `(TCHAR*)((int)this + (int)this->key)` (`lists.h:216`, `lists.h:234`, `lists.h:266`, `lists.h:281`). |

### 2.3 Iterators

| Iterator | Lines | Category | Notes |
|---|---|---|---|
| `_Iterator<T, Item, Key=int, KeyStored=true>` | `lists.h:307-380` | Bidirectional-ish | `operator++`, `operator--`, `operator*`, `key()`, `Eof()`, `First()`, `Last()`. **Not STL-compatible**: uses `Eof()` instead of comparing against `end()`, and `operator--` is only valid when `Item` has a `previous` member (compiles only for `_BItem`). Private ctor + `friend` declarations for `_List`, `_BList`, `Map`. |
| `_MemoryIterator<T, Item, Key, KeyStored>` | `lists.h:384-451` | Forward | Holds `(map, byte-offset, cached Item*)`. Recomputes `Item*` from `_map->_buffer.getArray() + _position` on each `++` (`lists.h:421`) because the buffer may have been reallocated. |
| `HashTable::HashTableIterator` | `lists.h:1919-1984` | Forward | Walks bucket chains then advances the bucket index. |
| `MemoryHashTable::MemoryHashTableIterator` | `lists.h:2135-2221` | Forward | Same, over the flat buffer. |
| `CachedMemoryMap::CachedMemoryMapIterator` | `lists.h:1691-1767` | Forward | Dispatches at runtime between the small fixed array and the spilled `MemoryMap`. |

**None of these model an STL iterator concept.** There is no `value_type`, no `iterator_category`, no `operator->`, no `begin()`/`end()` free functions, and the termination test is `it.Eof()` rather than `it != container.end()`. Range-based `for` is impossible. Every `<algorithm>` function is unusable.

### 2.4 The complete catalogue

| # | Template | Lines | Purpose | Public API | Allocation strategy | Key ownership | Value ownership | Nearest STL | Clean STL match? |
|---|---|---|---|---|---|---|---|---|---|
| 1 | `_List<T>` | `456-625` | Internal singly-linked list engine with head+tail pointers | `Eof, Count, start, end, set, insertAfter, addToTop, addToTale, peek, cutTop, cut(T), cut(Iterator), clear` | One `new _Item<T>` per element | n/a | `_freeT` fn-ptr, else none | `std::forward_list` + tail pointer | ⚠️ Partial — `std::forward_list` has no tail |
| 2 | `_BList<T>` | `629-797` | Internal doubly-linked list engine; also supports a **circular** mode | `Count, start, end, set, insertAfter, insertBefore, addToTale, circle, shiftNext, shiftPrevious, cut(T), cut(Iterator), clear` | One `new _BItem<T>` per element | n/a | `_freeT` fn-ptr | `std::list` | ⚠️ Circular mode has no STL analogue |
| 3 | `List<T>` | `801-871` | Public sequence container | `Count, start, end, set, insertAfter, add, cut(T), cut(Iterator), get(index), clear` | Delegates to `_List<T>` | n/a | via ctor `List(T, void(*freeT)(T))` | `std::list<T>` / `std::vector<T>` | ✅ Yes |
| 4 | `Stack<T>` | `875-946` | LIFO with a *default item* returned on underflow | `start, end, Count, peek, push, pop, cut, get(index), clear` | `_List<T>`, push at head | n/a | none (always `_list(NULL)`) | `std::stack<T, std::list<T>>` | ⚠️ `Stack` is **iterable**; `std::stack` is not |
| 5 | `Queue<T>` | `950-1018` | FIFO with a default item | `start, end, Count, push, pop, get(index), cut, clear` | `_List<T>`, push at tail, pop at head | n/a | via 3-arg ctor | `std::queue<T, std::list<T>>` | ⚠️ Iterable, unlike `std::queue` |
| 6 | `BList<T>` | `1022-1079` | Public doubly-linked list | `Count, start, end, set, insertAfter, insertBefore, cut, add, clear` | `_BList<T>` | n/a | via 2-arg ctor | `std::list<T>` | ✅ Yes |
| 7 | `CList<T>` | `1083-1155` | **Circular** doubly-linked list (ring) | `Count, start, end, shiftNext, shiftPrevious, set, insertAfter, insertBefore, add, cut, clear` | `_BList<T>` in circular mode | n/a | via 2-arg ctor | — | ❌ **No STL equivalent** (see §2.5) |
| 8 | `Map<Key,T,KeyStored=true>` | `1159-1440` | **Ordered-by-insertion association list**. Lookup is **O(n) linear scan**, not a tree or hash | `DefaultValue, Count, start, end, getIt, get, exist(k), exist(k,v), addToTop, add, add(unique), add(Map&), exclude, erase(Key), erase(Iterator), write, read, shiftKeys, clear` | One `new _MapItem` per entry, singly linked | `strdup`/`free` iff `KeyStored` | `_freeT` fn-ptr | `std::vector<std::pair<K,V>>` (assoc-list) — *not* `std::map` | ❌ **No clean match** (see §2.5) |
| 9 | `MemoryMap<Key,T,KeyStored=true>` | `1444-1680` | Same semantics as `Map` but all nodes packed into one `MemoryDump`; **serializable as a single blob** | `DefaultValue, Count, start, getIt, getNextIt, get, exist, storeKey, add, add(unique), add(MemoryMap&), write, read, clear` | Single `MemoryDump`, grown by `realloc`; nodes appended, linked by **byte offsets** | Key `strdup`-ed *into the same buffer* as a self-relative offset iff `KeyStored` | none — values are POD, never freed | — | ❌ **No STL equivalent** (see §2.5) |
| 10 | `CachedMemoryMap<Key,T,cacheSize>` | `1684-1911` | Small-size-optimized `MemoryMap`: first `cacheSize` entries live in a fixed inline array; **spills** to a real `MemoryMap` on overflow | `DefaultValue, Count, start, getIt, get, exist, add, add(unique), write, read, clear` | Inline `Item _cache[cacheSize]` (`lists.h:1773`) then `MemoryMap` | forced `KeyStored=false` (`lists.h:1686`) | none | `boost::container::small_vector` + linear find | ❌ **No STL equivalent** |
| 11 | `HashTable<Key,T,_scaleKey,hashSize>` | `1915-2127` | Chained hash table with a **compile-time bucket count** and a **compile-time hash function pointer**; chains are kept **sorted** by key | `DefaultValue, Count, start, get, getIt, exist(k), exist(k,v), add, add(unique), write, read, clear` | `Item* _table[hashSize]` (`lists.h:1988`) + one `new _MapItem` per entry | `KeyStored` defaulted to `true` via `_MapItem<Key,T>` (`lists.h:1917`) | `_freeT` fn-ptr (always `NULL`; ctor sets it at `lists.h:2116`) | `std::unordered_multimap` | ⚠️ Partial — fixed buckets, no rehash, sorted chains |
| 12 | `MemoryHashTable<Key,T,_scaleKey,hashSize,KeyStored=true>` | `2131-2400` | Hash table entirely inside one `MemoryDump`; bucket array is the first `hashSize*4` bytes of the buffer | Same as `HashTable` + `storeKey` | Single `MemoryDump`; bucket heads at byte offset `index<<2` (`lists.h:2249`) | Self-relative offset into the same buffer iff `KeyStored` | none | — | ❌ **No STL equivalent** |
| 13 | `Cache<Key,T,cacheSize>` | `2404-2505` | Fixed-capacity **ring-buffer MRU cache**; overwrites oldest on overflow, no eviction callback | `get, add, clear` | Fixed `Item _items[cacheSize]` (`lists.h:2452`), `_top`/`_tale` indices | none — raw pointer copy | none | — | ❌ **No STL equivalent** |
| 14 | `Dictionary2D<Key,SubKey>` | `2526-2670` | Two-level `category → key → value` dictionary with a **tagged-union value type** (`VItem`) | `add(k,v)`, `add(k,sk,v)`, `start`, `getIt(k)`, `get(k,def)`, `get(k,sk,def)`, `clear(k,sk)`, `clear(k)`, `clear()` | `Map<Key,VItem>` of `Map<SubKey,VItem>*` | inherited from `Map` | `freeVItem` (`lists.h:2516-2524`) frees by union tag | `std::map<K, std::map<SK, std::variant<...>>>` | ⚠️ Needs C++17 `std::variant` |
| 15 | `Dictionary2D<>::VItem` | `2529-2581` | Tagged union: `stDWORD` / `stString` / `stMap` | implicit `operator int`, `operator TCHAR*`, `operator const TCHAR*`, `operator Map<SubKey,VItem>*` | union | — | see §6.2 | `std::variant<int, std::string, Map*>` | ⚠️ |
| 16 | `_Iterator<>` | `307-380` | see §2.3 | — | — | — | — | STL bidirectional iterator | ❌ Not conforming |
| 17 | `_MemoryIterator<>` | `384-451` | see §2.3 | — | — | — | — | STL forward iterator | ❌ Not conforming |
| 18 | `_Item<T>` / `_BItem<T>` / `_MapItem<>` / `_MemoryMapItem<>` | `29-303` | node structs | — | — | — | — | internal | n/a |
| 19 | `ValueType` enum | `2509-2514` | `stDWORD`, `stString`, `stMap` | — | — | — | — | `std::variant` index | n/a |

#### Free function templates

| Function | Lines | Purpose | STL equivalent |
|---|---|---|---|
| `freeVItem<VItem>(VItem)` | `2516-2524` | Tag-dispatched destructor for `Dictionary2D::VItem` | destructor of `std::variant` |
| `simpleRule(int)` | `2674-2677` | Identity hash function for `HashTable` | `std::identity` / `std::hash` |
| `mapKey<Map,Key,T>(Map&, Key, T)` | `2681-2690` | Get-or-insert; returns existing value if present, else inserts `newValue` | `std::map::try_emplace` / `insert().first->second` |
| `shift<Iterator,T>(Iterator, T minValue, int)` | `2694-2702` | Add `displacement` to every element `>= minValue` | `std::for_each` / `std::transform` |
| `retrieveKey<Key,T,Iterator>(Iterator, T, Key)` | `2706-2715` | Reverse lookup: find the key whose value equals `value` | `std::find_if` + `->first` |
| `searchInList(List<const TCHAR*>&, const TCHAR*)` | `2719-2731` | Linear index-of by string compare | `std::find` + `std::distance` |
| `searchInList(List<TCHAR*>&, const TCHAR*)` | `2733-2745` | overload | idem |
| `searchInList(List<ref_t>&, ref_t)` | `2747-2759` | overload | idem |

### 2.5 Containers with **no clean STL equivalent**, and why

These four are the ones that will actually cost engineering time. They are not "a `std::map` with a funny name" — they encode format and performance decisions that the rest of the compiler depends on.

#### (a) `MemoryMap` / `MemoryHashTable` — relocatable, serializable, offset-linked

**Why there is no STL analogue.** These are not containers in the STL sense; they are **on-disk data structure builders**. Every node lives at a byte offset inside a single `MemoryDump`. Links are `size_t` byte offsets, never pointers (`lists.h:209`, and the explicit comment "offset is used instead of pointer due to possible buffer relocation" at `lists.h:490`, `lists.h:1513`, `lists.h:1607`). Consequently the whole map can be written to a `.nl` module file with a single `memcpy`-style blob copy:

```cpp
// lists.h:1635-1643
void write(StreamWriter* writer) {
   writer->writeDWord(_buffer.Length());
   writer->writeDWord(_count);
   writer->writeDWord(_tale);
   DumpReader reader(&_buffer);
   writer->read(&reader, _buffer.Length());   // raw blob
}
```

An `std::unordered_map` cannot do this: its nodes are scattered pointers. Replacing these with STL would mean **changing the ELENA module file format**, which is exactly what the LLVM-backend work will want to do anyway — but it is a coupled change, not a drop-in.

Additional consequences that make them hostile to portability:
- Node layout is written raw: `_buffer.write(position, &item, sizeof(item))` (`lists.h:1594`, `lists.h:2322`) — **struct padding and pointer size become part of the file format**.
- The key offset is patched at a **hardcoded byte offset 4**: `_buffer.writeDWord(position + 4, storedKey)` (`lists.h:1599`, `lists.h:2326`). This assumes `sizeof(size_t next) == 4`. On a 64-bit build it silently corrupts the `key` field.
- Bucket heads are read via `_buffer[index << 2]` where `MemoryDump::operator[]` returns `int&` at an arbitrary byte offset (`dump.h:32-35`) — an **unaligned 32-bit access**, which traps on strict-alignment targets.

#### (b) `Map` — an insertion-ordered association *list* with O(n) lookup

Despite the name, `Map` is **not** a tree and **not** hashed. `get`, `exist` and `getIt` all walk the singly-linked chain from `_top` (`lists.h:1197-1207`, `lists.h:1185-1195`). It also permits **duplicate keys** (`add` has an explicit `unique` overload at `lists.h:1254`, implying the default allows dupes), and `Dictionary2D` relies on this via its `_allowDuplicates` flag (`lists.h:2585`, `lists.h:2590`).

So `Map` is simultaneously:
- a `std::multimap` (duplicates allowed),
- with **insertion order** preserved (not key order),
- with O(n) lookup,
- and an `exclude()` method that *detaches without freeing* (`lists.h:1274-1307`) versus `erase()` which *detaches and frees* (`lists.h:1309-1341`).

`std::map` gives you none of insertion-ordering, duplicate keys, or the exclude/erase distinction. The honest replacement is `std::vector<std::pair<K,V>>` plus explicit helpers — which changes complexity characteristics — or a proper `std::unordered_multimap` if insertion order turns out not to matter (it does matter for `ConfigSettings`, since `IniConfigFile::save` writes categories back in iteration order, `config.cpp:72-93`).

#### (c) `CList` — a ring with a movable head (**dead code**)

`CList` builds a genuine circular doubly-linked list via `_BList::circle` (`lists.h:694-707`) and exposes `shiftNext`/`shiftPrevious` (`lists.h:709-723`) to rotate the notion of "first". `std::list` has `splice` but no O(1) rotate-head and no circular topology (its `end()` sentinel breaks the ring), so there is genuinely no STL equivalent.

> **However — `CList` is never instantiated anywhere in the tree (§1.6).** The correct action is to delete it, not port it. The same applies to the `circle`/`shiftNext`/`shiftPrevious` machinery in `_BList` that exists only to serve it.

#### (d) `Cache` — a fixed ring MRU with silent overwrite

`Cache` (`lists.h:2404`) is a bounded ring buffer used as a memoization cache: `add` overwrites the oldest entry and advances `_top` when full (`lists.h:2479-2488`). Lookup is a linear scan from `_top` to `_tale` (`lists.h:2466-2477`). It never frees anything and never notifies on eviction. There is no STL container with eviction semantics; the closest is a hand-rolled `std::array` + indices, i.e. exactly what this is.

It **is** live code, with two consumers, both in the engine: `elenasrc/engine/module.h:19` (module-name resolution cache) and `elenasrc/engine/win32/x86jitcompiler.h:112` (JIT label cache). Both are on hot paths, so its performance characteristics matter.

#### (e) Honourable mention: `CachedMemoryMap` — small-buffer optimization

`CachedMemoryMap` (`lists.h:1684`) is a small-size optimization that *changes its own serialization format* depending on whether it spilled: `write` emits a `-1` marker and a raw array dump when cached, or delegates to `MemoryMap::write` when spilled (`lists.h:1867-1876`). `boost::container::small_vector` covers the storage idea but not the dual serialization format.

### 2.6 Allocation strategy summary

| Family | Allocator | Growth | Reallocation hazard |
|---|---|---|---|
| `_List`, `_BList`, `List`, `Stack`, `Queue`, `BList`, `CList`, `Map`, `HashTable` | `new` / `delete` per node | one node at a time | none (stable addresses) |
| `MemoryMap`, `MemoryHashTable`, `CachedMemoryMap` (spilled) | `MemoryDump` → `malloc`/`realloc` (`dump.cpp:22`, `dump.cpp:38`) | `align(size, SECTION_PAGE_SIZE=0x40)` (`dump.cpp:36`) | **Yes — all raw `Item*` are invalidated on any `add()`.** This is why iterators re-derive pointers from offsets. Any code holding an `Item*` across an `add()` is UB. |
| `Cache`, `CachedMemoryMap` (cached) | inline fixed C array | none — fixed capacity | none |
| `String` | `malloc`/`realloc` via `tools.h` | rounded up to `STR_PAGE_SIZE = 0x20` (`altstrings.h:16`, `altstrings.h:119`) | pointers into the string invalidated on append |
| `LocalString<N>` | inline `TCHAR _string[N+1]` (`altstrings.h:171`) | **none — silently fails** (`_copy` returns `false` at `altstrings.h:183`) | none |

---

## 3. String handling (`altstrings.h`)

### 3.1 The model in one paragraph

ELENA 1.5 uses the **Windows `TCHAR` model**: a single source tree compiles to either an "ANSI" (`char`) or a "Unicode" (`wchar_t`, i.e. UTF-16LE on Windows) build, selected by the `_UNICODE` / `UNICODE` preprocessor macros. There is **no runtime encoding abstraction and no UTF-8 support at all**. `TCHAR` comes from Microsoft's `<tchar.h>` (`common.h:14`), and every string function in the layer is the `_t`-prefixed Microsoft macro (`_tcslen`, `_tcscpy`, `_tcsrchr`, …) which expands to the `str*` or `wcs*` family depending on `_UNICODE`.

### 3.2 The class hierarchy

```
_String                      (altstrings.h:20)  abstract base
 ├── String                  (altstrings.h:102) heap-allocated, growable
 ├── LocalString<size>       (altstrings.h:168) fixed inline buffer, no heap
 └── ReferenceNameTemplate<String>  (altstrings.h:250) ELENA "module'class'method" names

  (not derived from _String — they only expose operator const TCHAR*:)
 ── Quote<String>                    (altstrings.h:341)
 ── NamespaceTemplate<String>        (altstrings.h:394)
 ── IdentifierTemplate<String>       (altstrings.h:420)
 ── PrivateMessageTemplate<String>   (altstrings.h:435)
```

| Class | Lines | Storage | Overflow behaviour | Notes |
|---|---|---|---|---|
| `_String` | `20-98` | abstract | — | Three pure virtuals: `_append`, `_copy`, `getBody`. Public: `operator const TCHAR*`, `isEmpty`, `Length`, `asString`, `reserve`, `asInt`, `lower`, `upper`, `copy`×2, `append`×5, `appendHex`, `appendInt`, `appendLong`, `appendInt64`, `appendHex64`, `appendDouble`, `Clone`, `get(size)`, `clear`. |
| `String` | `102-164` | `TCHAR* _string` from `malloc` | grows via `realloc` rounded to `STR_PAGE_SIZE` (0x20) | `allocate` (`altstrings.h:108`) frees then re-mallocs; `reallocate` (`altstrings.h:116`) only grows. |
| `LocalString<size>` | `168-246` | `TCHAR _string[size+1]` on the stack | **silently returns `false`, string truncated/unchanged** (`altstrings.h:183`, `altstrings.h:193`) | `reserve` is a no-op (`altstrings.h:206`). Callers routinely ignore the `bool`. |
| `ReferenceNameTemplate` | `250-337` | wraps a `String` | delegates | Builds `module'proper'sub` ELENA references; `pathToName` converts a **backslash path** into an apostrophe-separated name (`altstrings.h:322`). |
| `Quote` | `341-390` | wraps a `String` | delegates | Un-escapes ELENA literal syntax: `%n %r %t %a %b %%`, `%<decimal>` numeric escapes, and `""` → `"` (`altstrings.h:351-388`). |
| `NamespaceTemplate` | `394-416` | wraps a `String` | delegates | Takes everything up to the last `'`. |
| `IdentifierTemplate` | `420-431` | wraps a `String` | delegates | Takes everything after the last `'`. |
| `PrivateMessageTemplate` | `435-456` | wraps a `String` | delegates | Truncates a module name to at most two namespace levels (`altstrings.h:442` comment). |

### 3.3 The `TCHAR` abstraction and how the two builds diverge

| Build | Macro | `TCHAR` | `sizeof(TCHAR)` on Win32 | Where set |
|---|---|---|---|---|
| MinGW / CodeBlocks | `-D_UNICODE -DUNICODE` | `wchar_t` | 2 | `elenasrc/elc/codeblocks/elc.cbp` (`-D_UNICODE`, `-DUNICODE`) |
| VS2008 Debug | `CharacterSet="1"`, `UNICODE` | `wchar_t` | 2 | `elenasrc/elc/vs/elc9.vcproj:24`, `:45` |
| VS2008 **Release** | `CharacterSet="2"` (MBCS), **no `UNICODE`** | `char` | 1 | `elenasrc/elc/vs/elc9.vcproj:104`, `:123` |

> ⚠️ **The Visual Studio Release configuration builds an ANSI `elc`, while Debug and the MinGW build produce a Unicode `elc`.** Because `MemoryMap`/`MemoryHashTable` write string keys raw into the module file, **`.nl` files produced by the two configurations are byte-incompatible**. `DumpReader::getLiteral` even branches on `_UNICODE` to decide the stride (`dump.cpp:185-189`). This is a pre-existing correctness defect, not just a portability one.

### 3.4 Encoding conversion — the entire surface

There are exactly **two** conversion functions, both thin wrappers over Win32, both at global scope:

```cpp
// win32/unicode.h:17-20
inline void ansiToUnicode(const char* sour, wchar_t* dest, size_t length)
{
   MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, sour, length, dest, length);
}

// win32/unicode.h:22-29
inline bool unicodeToAnsi(const wchar_t* sour, char* dest, size_t length)
{
   BOOL flag = false;
   WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, sour, length, dest, length, "?", &flag);
   return !flag;
}
```

| Property | Value | Consequence |
|---|---|---|
| Code page | **`CP_ACP`** — the process's *active ANSI code page* | Conversion result depends on the **machine's locale**, not on the file. A `.l` source file with Cyrillic identifiers compiles differently on a CP1251 machine than on a CP1252 machine. Not reproducible across machines. |
| Flags | `MB_PRECOMPOSED`, `WC_NO_BEST_FIT_CHARS` | No composition control on the way back; unmappable chars become `"?"`. |
| Length model | `length` used as **both** input and output length | Correct only for single-byte↔single-`wchar_t` mappings. Breaks for DBCS code pages (932/936/949/950) and for any character outside the BMP. |
| Error reporting | `unicodeToAnsi` returns `!usedDefaultChar`; `ansiToUnicode` returns `void` | Silent data loss on the ANSI→Unicode path. |
| Surrogate pairs | not handled | Any character above U+FFFF is two `wchar_t` on Windows; `Length()`/`_tcslen` count code units, not code points. |

**Callers of these two functions:**

| Call site | Purpose |
|---|---|
| `tools.h:194` (`doubleToStr` wide overload) | converts `_gcvt` ANSI output to wide |
| `altstrings.h:225` (`LocalString::convert`) | `#ifdef _UNICODE` only |
| `altstrings.cpp:180` (`String::convert`) | `#ifdef _UNICODE` only |
| `files.cpp:115` (`File::readLiteral` UTF-16→char) | file decoding |
| `files.cpp:163` (`File::readLine` UTF-16→char) | file decoding |
| `files.cpp:213` (`File::writeLiteral` char→UTF-16) | file encoding |
| `files.cpp:234` (`File::writeLiteral` wide→ANSI) | file encoding |

### 3.5 UTF-8: declared but not implemented

`FileEncoding` declares a `feUTF8 = 3` member:

```cpp
// files.h:200
enum FileEncoding { feAutodetect = 0, feRaw = 1, feAnsi = 2, feUTF8 = 3, feUTF16 = 4};
```

A repository-wide grep for `feUTF8` finds **exactly one hit — the declaration itself**. There is:
- no UTF-8 encoder,
- no UTF-8 decoder,
- no UTF-8 BOM detection (`File::File` only sniffs `0xFEFF` for UTF-16, `files.cpp:25-35`),
- no `else if (_encoding == feUTF8)` branch in any of the six encoding switches in `files.cpp` (lines 100, 105, 128, 151, 154, 179, 205, 226).

**Passing `feUTF8` to `File` makes every `readLiteral`/`readLine` return `false` immediately** (the terminal `else return false;` at `files.cpp:123`, `files.cpp:174`). UTF-8 is dead code. Any claim that ELENA 1.5 "supports UTF-8" is false.

### 3.6 String utility routines in `tools.h`

| Function | Lines | Notes |
|---|---|---|
| `freestr(char*)`, `freestr(wchar_t*)` | `16-28` | `free()` with NULL guard. Pairs with `strdup`'s `malloc`. |
| `compstr(const char*/wchar_t*, …)` ×4 | `103-125` | NULL-safe `strcmp`/`wcscmp`/`strncmp`/`wcsncmp`. Returns `false` if either is NULL — **so `compstr(NULL, NULL)` is `false`**, which is arguably wrong. |
| `grtstr` | `97-101` | `_tcscmp(s1,s2) > 0`; uses `TCHAR`, so no `char`/`wchar_t` overload pair. |
| `emptystr` ×2 | `127-135` | NULL or `[0]==0`. |
| `strdup(const char*)`, `strdup(const wchar_t*)` | `137-151` | **Shadows POSIX `strdup`** inside `namespace _ELENA_`. Returns `NULL` for empty input — so a round-trip of `""` yields `NULL`, not `""`. The wide version computes `(wcslen(s) << 1) + 2`, **hardcoding `sizeof(wchar_t)==2`**. |
| `createstr` / `recreatestr` ×4 | `153-171` | `malloc`/`realloc`; the wide versions use `length << 1`, again assuming 2-byte `wchar_t`. |
| `getlength` ×2 | `173-181` | NULL-safe length. |
| `doubleToStr` ×2 | `183-196` | Uses MSVC-only `_gcvt`; the wide version goes through `ansiToUnicode`. |
| `insertstr` | `198-206` | In-place insert; **no bounds check** — writes past the end if the caller under-allocated. |
| `movestr` ×2 | `208-216` | `memmove`; wide version `length << 1`. |
| `_tchlwr` | `218-226` | Lowercases one char by building a 2-char buffer and calling `_tcslwr`. Locale-dependent, ASCII-biased. |
| `lastchrpos` ×2, `chrpos` | `70-95` | `_tcsrchr`/`_tcschr` wrappers returning an `int` index or `-1`. |
| `calcTabShift` | `61-66` | Tab-stop arithmetic for the IDE. |
| `test(int, int)` | `39-42` | Bit-mask test `(n & mask) == mask`. |
| `isbetween` | `44-47` | Range test, **exclusive on both ends**. |
| `align(unsigned int, unsigned int)` | `230-236` | Power-of-two round-up. **Takes `unsigned int`, not `size_t`** — truncates on 64-bit. |
| `mapReferenceKey` / `mapLiteralKey` | `240-257` | Hash functions for `ReferenceMap` / `ConstantMap`: bucket = first letter after the last `'`, `'a'`..`'z'` → 0..25, everything else → 26. Case-sensitive, ASCII-only, and **`position > 26` should be `>= 26`** (`tools.h:246`). |
| `removeFile` ×2 | `49-57` | `remove` / MSVC `_wremove`. |
| `freeobj<T>` | `30-35` | `delete` with NULL guard. |

---

## 4. Streams & memory (`streams.h`, `dump.h`)

### 4.1 The reader/writer hierarchy

```
StreamReader (abstract)                     streams.h:20
 ├── LiteralReader                          streams.h:354   reads from an in-memory TCHAR* text
 ├── DumpReader                             dump.h:159      reads from a MemoryDump
 └── FileReader                             files.h:243     reads from a File

StreamWriter (abstract)                     streams.h:117
 ├── LiteralWriter                          streams.h:259   writes into a caller-supplied TCHAR buffer
 ├── DumpWriter                             dump.h:101      appends to a MemoryDump
 └── FileWriter                             files.h:279     writes to a File

TextReader (abstract)                       streams.h:218
 └── TextFileReader                         files.h:315     line-oriented file reads

TextWriter (abstract)                       streams.h:240
 └── TextFileWriter                         files.h:331     line-oriented file writes
```

Two *separate*, unrelated hierarchies: `Stream*` is **binary/positioned**, `Text*` is **line-oriented and unpositioned**. There is no adapter between them.

### 4.2 `StreamReader` API (`streams.h:20-113`)

| Member | Line | Kind | Notes |
|---|---|---|---|
| `Eof()` | 23 | pure virtual | |
| `Position()` | 24 | pure virtual | returns `size_t` |
| `seek(size_t)` | 26 | pure virtual | |
| `read(void*, size_t)` | 28 | pure virtual | the single primitive |
| `getLiteral()` | 30 | pure virtual | returns an **interior pointer** into the underlying storage — dangling if the buffer is reallocated |
| `getDWord()` | 32 | concrete | |
| `getByte()` | 40 | concrete | |
| `readDWord(int&)` | 48 | concrete | `read(&dword, 4)` |
| `readDWord(size_t&)` | 53 | concrete | ⚠️ **`read((void*)&dword, 4)` into a `size_t`** — on a 64-bit build this leaves the top 4 bytes uninitialized |
| `readByte(char&)` | 58 | concrete | |
| `readChar(TCHAR&)` | 63 | concrete | `sizeof(TCHAR)`-sized |
| `readLiteral(wchar_t*, size_t)` | 68 | concrete | `length << 1` — assumes 2-byte `wchar_t` |
| `readLiteral(char*, size_t)` | 73 | concrete | |
| `readString(_String&, size_t)` | 78 | concrete | chunked via a `TCHAR buffer[BLOCK_SIZE]` |
| `readString(_String&)` | 98 | concrete | NUL-terminated read |

### 4.3 `StreamWriter` API (`streams.h:117-214`)

| Member | Line | Notes |
|---|---|---|
| `isOpened()` | 120 | pure virtual |
| `Position() const` | 122 | pure virtual — note the `const`, which `StreamReader::Position` lacks |
| `write(const void*, size_t)` | 124 | pure virtual — the single primitive |
| `writeLiteral(const TCHAR*)` | 126 | writes **including** the NUL terminator (`getlength(s) + 1`) |
| `writeLiteral(const char*, size_t)` | 131 | |
| `writeLiteral(const wchar_t*, size_t)` | 136 | `length << 1` |
| `writeChar(TCHAR)` | 141 | |
| `writeDWord(int)` | 146 | **always 4 bytes**, `void` return — errors are silently dropped |
| `writeWord(unsigned short)` | 151 | 2 bytes |
| `writeByte(unsigned char)` | 156 | |
| `writeBytes(unsigned char, size_t)` | 161 | virtual; naive per-byte loop, overridden by `DumpWriter` with a `memset` (`dump.cpp:149`) |
| `writeChars(TCHAR, size_t)` | 171 | virtual, per-char loop |
| `writeAsciiLiteral(const char*, size_t)` | 181 | |
| `writeAsciiLiteral(const wchar_t*, size_t)` | 186 | ⚠️ **narrows by writing only the low byte of each `wchar_t`** (`write((void*)&s[i], 1)`) — a little-endian-only, silently-lossy "conversion" |
| `writeAsciiLiteral(const TCHAR*)` | 193 | |
| `read(StreamReader*, size_t)` | 198 | pump: copies `length` bytes reader→writer through a `char buffer[BLOCK_SIZE]` |

`BLOCK_SIZE` is `0x200` (`streams.h:16`). Note that `TCHAR buffer[BLOCK_SIZE]` (`streams.h:82`, `streams.h:227`) is 1 KiB in a Unicode build and `char buffer[BLOCK_SIZE]` (`streams.h:200`) is 512 B — the same constant means different byte counts in different places.

### 4.4 `MemoryDump` (`dump.h:20-97`, `dump.cpp:17-106`)

The single growable byte buffer that underpins `MemoryMap`, `MemoryHashTable`, `CachedMemoryMap`, and every module `Section`.

```cpp
char*  _buffer;   // dump.h:23
size_t _total;    // capacity
size_t _used;     // logical length
```

| Member | Line | Behaviour |
|---|---|---|
| `getArray()` | `dump.h:30` | raw `void*` — **escapes the abstraction**; callers do pointer arithmetic on it (`lists.h:395`, `lists.h:1478`, `lists.h:1602`, `lists.h:2243`, `lists.h:2329`) |
| `operator[](size_t) const` | `dump.h:32-35` | **returns `int&` at a byte offset**: `*(int*)(_buffer + position)`. This is the aliasing/alignment landmine — see §8. Also `const` but returns a mutable reference. |
| `Length()` / `Size()` | `dump.h:37-38` | used vs. total |
| `reserve(size_t)` | `dump.cpp:33-40` | `realloc` to `align(size, SECTION_PAGE_SIZE)`; **`realloc` return value is not checked for NULL** |
| `allocate(size_t)` | `dump.cpp:42-45` | grow logical length by `size` |
| `resize(size_t)` | `dump.cpp:47-54` | protected; grow-only, never shrinks |
| `read(pos, void*, len)` | `dump.cpp:90-98` | bounds-checked |
| `write(pos, const void*, len)` | `dump.cpp:56-66` | grows on demand; rejects `position > _used` (no sparse writes) |
| `writeBytes(pos, char, len)` | `dump.cpp:68-78` | `memset` |
| `writeDWord(pos, int)` | `dump.h:49-52` | 4 bytes |
| `write(pos, const wchar_t*)` | `dump.h:54-63` | writes `(wcslen(s)+1) << 1` bytes — **assumes 2-byte `wchar_t`** |
| `write(pos, const char*)` | `dump.h:64-73` | writes `strlen(s)+1` bytes |
| `insert(pos, const char*, len)` | `dump.cpp:80-88` | `memmove` + `memcpy` (see Appendix A — the length arithmetic is wrong) |
| `insertDWord` / `insertWord` / `insertByte` | `dump.h:77-88` | |
| `get(size_t)` | `dump.cpp:100-106` | interior pointer or NULL |
| `clear()` | `dump.h:92` | sets `_used = 0`; **does not free `_buffer`** |
| dtor | `dump.h:96` | `freestr(_buffer)` — i.e. `free()`; correct pairing with `malloc`, but semantically odd (a `char*` buffer freed by a *string* helper) |

`SECTION_PAGE_SIZE` is `0x40` (`dump.h:16`) — a 64-byte growth quantum, which for a compiler building megabyte-sized code sections means a very large number of `realloc` calls.

### 4.5 Byte-order and word-size assumptions

| Assumption | Where | Impact |
|---|---|---|
| **Little-endian on disk** | Every `writeDWord` (`streams.h:146`) / `readDWord` (`streams.h:48`) does a raw `memcpy` of the host representation. `MemoryMap::write` (`lists.h:1635`) dumps whole structs. | Module `.nl` files and the `.nl`/`.dnl` on-disk format are **implicitly little-endian**. On a big-endian target (or when cross-compiling) they are unreadable. There is no byte-swap layer. |
| **32-bit `int` for all serialized integers** | `writeDWord(int)` hardcodes 4 (`streams.h:148`) | Fine, `int` is 32-bit on all targets of interest. |
| **`size_t` serialized as 4 bytes** | `readDWord(size_t&)` (`streams.h:53`), `writeDWord(_buffer.Length())` (`lists.h:1637`), `writeDWord(_count)` (`lists.h:1373`) | On a 64-bit build, **reads leave 4 bytes uninitialized and writes silently truncate**. This is the single most mechanical 64-bit blocker. |
| **`sizeof(wchar_t) == 2`** | `streams.h:70`, `streams.h:138`, `dump.h:57`, `tools.h:149`, `tools.h:160`, `tools.h:170`, `tools.h:215`, `files.cpp:111`, `files.cpp:129`, `files.cpp:141-143`, `dump.cpp:186` | **On Linux/macOS `wchar_t` is 4 bytes.** Every `<< 1` becomes a factor-of-2 under-count: buffers half the needed size, strings read at the wrong stride, module files with the wrong layout. See §8. |
| **UTF-16 BOM `0xFEFF` written/read as a raw `unsigned short`** | `files.cpp:27-28`, `files.cpp:290-291`, `files.cpp:306-307` | Encodes host endianness into the BOM. A big-endian writer would emit a BE BOM and then LE-assume on read. |
| **Struct padding is part of the file format** | `lists.h:1594`, `lists.h:2322` write `sizeof(_MemoryMapItem)` raw | Any change in compiler, ABI, or pointer width changes the on-disk layout. No `#pragma pack` is used, so the layout is whatever the ABI says. |
| **Hardcoded field offset 4** | `lists.h:1599`, `lists.h:2326` (`writeDWord(position + 4, storedKey)`) | Assumes `offsetof(Item, key) == 4`, i.e. `sizeof(size_t) == 4`. Breaks on LP64 and LLP64. |

---

## 5. File system (`files.h`, `files.cpp`)

### 5.1 Path handling

Paths are **string templates**, not a dedicated type:

```cpp
typedef PathTemplate<String>            Path;              // files.h:193
typedef FileNameTemplate<String>        FileName;          // files.h:194
typedef LocalString<LOCAL_PATH_LENGTH>  LocalPathString;   // files.h:195  (0x200 = 512)
typedef PathTemplate<LocalPathString>   LocalPath;         // files.h:196
```

`LOCAL_PATH_LENGTH` is `0x200` = 512 (`files.h:15`) — chosen to sit under the Windows `MAX_PATH` era limit. On Linux `PATH_MAX` is 4096; on macOS 1024. **A 512-char stack buffer with a silently-failing `_copy` (`altstrings.h:183`) will truncate deep paths without any diagnostic.**

| `PathTemplate` member | Line | Behaviour | Portability |
|---|---|---|---|
| `checkExtension(path, ext)` | `24-33` | splits on the **last `'\\'`** then the last `'.'` | ❌ backslash |
| `operator const TCHAR*` / `asString` | `35-37` | | |
| `isEmpty` | `39` | | |
| `appendExtension(ext)` | `41-45` | appends `'.' + ext` | ok |
| `nameToPath(name, ext)` | `47-61` | converts an ELENA reference `a'b'c` into a path by splitting on `'` and calling `combine` | ok |
| `copyPath(path)` | `63-73` | keeps everything before the **last `'\\'`** (i.e. the directory) | ❌ backslash |
| `copy(path)` | `75-78` | | |
| `combine(path, length)` | `80-90` | joins with `'\\'` **hardcoded** at line 84-86 | ❌ backslash |
| `combine(path)` | `92-95` | | |
| `lower()` | `97-100` | lowercases the whole path — **assumes a case-insensitive filesystem** | ❌ see §5.4 |
| `changeExtension(ext)` | `102-113` | splits on last `'\\'` then last `'.'` | ❌ backslash |
| `clear()` | `115-118` | | |
| `exists()` | `120-128` | `struct _stat` + `_tstat` | ❌ MSVC-only names |

`FileNameTemplate::copyName` (`files.h:177-188`) extracts the stem by scanning back to `'\\'` (`_tcsrchr(path, '\\')`, `files.h:181`) then forward to the first `'.'`.

**Summary of separator handling:** the character `'\\'` appears as a hardcoded path separator at `files.h:27`, `files.h:65`, `files.h:84`, `files.h:86`, `files.h:105`, `files.h:181`, and in `altstrings.h:322` (`ReferenceNameTemplate::pathToName`). There is **no** separator constant, no `'/'` acceptance, and no normalization. Even on Windows this rejects the forward-slash paths that the Win32 API itself accepts.

Additionally `common.h:25` uses a **backslash inside an `#include` directive**: `#include "win32\unicode.h"`. GCC and Clang treat `\u` as an (invalid) universal-character-name escape or simply fail to find the file; this line alone stops the tree from compiling on any non-MSVC toolchain.

### 5.2 The `File` class and the C stdio layer

`File` (`files.h:202-239`, `files.cpp:19-253`) wraps a `FILE*` — **not** Win32 `HANDLE`s. That is fortunate: the raw I/O is already largely portable C89. What is not portable is the *naming*:

| Construct | Location | Replacement |
|---|---|---|
| `_tfopen(path, mode)` | `files.cpp:21` | `fopen` (+ UTF-8 path conversion on Windows) |
| `struct _stat` / `_tstat` | `files.h:122`, `files.h:124` | `struct stat` / `stat` |
| `_mkdir` / `_wmkdir` | `files.cpp:335`, `files.cpp:340` | `mkdir(path, mode)` — **note the POSIX version takes a mode argument** |
| `_access` / `_waccess` | `files.cpp:345`, `files.cpp:350` | `access(path, mode)` |
| `_wremove` | `tools.h:56` | `remove` |
| `<direct.h>` | `files.cpp:13` | `<sys/stat.h>`, `<unistd.h>` |
| `<io.h>` | `common.h:18` | `<unistd.h>` |

`File::Position()` and `File::Length()` return **`long`** (`files.h:213-214`, `files.cpp:47`, `files.cpp:52`) via `ftell`. `long` is 32-bit on Windows (LLP64) *and* on 32-bit Linux, but 64-bit on 64-bit Linux/macOS (LP64) — so the type silently changes width across platforms, and on Windows it caps files at 2 GB. `FileReader::Position()` then narrows it to `size_t` (`files.h:254`), and `FileWriter::align` does `::align(_file.Position(), alignment)` (`files.cpp:327`) which further narrows `long` → `unsigned int`.

### 5.3 Encodings in `File`

`File` performs transcoding inline in every read/write method, switching on `_encoding`:

| Method | Line | `feAnsi`/`feRaw` | `feUTF16` | `feUTF8` |
|---|---|---|---|---|
| `readLiteral(char*, …)` | `98-124` | direct `fread` | `fread` 2-byte units + `unicodeToAnsi` | ❌ `return false` |
| `readLiteral(wchar_t*, …)` | `126-147` | reads bytes then **widens in place by inserting zero bytes** (`files.cpp:139-144`) — a raw Latin-1 widening, *not* a code-page conversion, inconsistent with `ansiToUnicode` | `fread(…, 2, …)` | falls into the ANSI branch |
| `readLine(char*, …)` | `149-175` | `fgets` | `fgetws` + `unicodeToAnsi` | ❌ `return false` |
| `readLine(wchar_t*, …)` | `177-196` | in-place widening | `fgetws` | falls through |
| `writeLiteral(const char*, …)` | `203-222` | direct `fwrite` | converts to wide… **then writes the original `s` instead of `temp`** (`files.cpp:214`) — a real bug | ❌ |
| `writeLiteral(const wchar_t*, …)` | `224-243` | `unicodeToAnsi` + `fwrite` | `fwrite(…, 2, …)` | ❌ |

Autodetection (`files.cpp:25-35`) reads 2 bytes and checks for `0xFEFF`; anything else is assumed `feAnsi`. **No UTF-8 BOM (`EF BB BF`) detection, no heuristic.**

`File::writeNewLine()` hardcodes CRLF: `writeLiteral(_T("\r\n"), 2)` (`files.cpp:247`). Every text file the toolchain emits — including IDE-saved sources and `api2html` output — gets Windows line endings unconditionally.

### 5.4 Case-sensitivity assumptions

`PathTemplate::lower()` (`files.h:97-100`) exists specifically to normalize paths for comparison, and `createPath` compares directory strings with `compstr(dirPath, root)` (`files.cpp:359`) — an exact, case-**sensitive** `wcscmp`. The combination is contradictory and only works because NTFS is case-insensitive:

- Callers lowercase paths to make them comparable → **on Linux/macOS-with-case-sensitive-APFS this actively breaks**, because `/Home/User/Src` and `/home/user/src` are different files.
- `Map<const TCHAR*, _Module*> ModuleMap` (`engine/elena.h:193`) keys modules by name using `compstr` — exact binary comparison. Combined with lowercasing elsewhere, module identity depends on which code path produced the key.

`createPath` (`files.cpp:353-367`) recursively creates parent directories. Note it calls `createDir(dirPath)` with no mode argument (`files.cpp:362`) — the POSIX `mkdir` requires one.

---

## 6. Config parsing (`config.h`, `config.cpp`)

### 6.1 The format

`IniConfigFile` parses a classic **INI** file. This is the format of `.prj` project files (consumed by `elc`, see `elenasrc/elc/project.h:16`), IDE `.cfg` settings, and the `sg`/`api2html` data files under `dat/`.

Grammar, as implemented by `IniConfigFile::load` (`config.cpp:24-62`):

```
file      := line*
line      := section | entry | flag | blank
section   := '[' name ']'          ; name must be >= 1 char (config.cpp:43)
entry     := key '=' value         ; split at the FIRST '=' (config.cpp:52-53)
flag      := key                   ; no '=' -> value is DEFAULT_STR (NULL)
blank     := ''                    ; skipped (config.cpp:39)
```

Concrete behaviour:

| Aspect | Behaviour | Line |
|---|---|---|
| Encoding | `TextFileReader(path, feAutodetect)` — ANSI or UTF-16 | `config.cpp:29` |
| Line trimming | trailing `'\n'`, `'\r'`, `' '` stripped, **in that order**, via `String::trim` | `config.cpp:35-37` |
| Leading whitespace | **not** stripped — ` key=value` yields the key `" key"` | — |
| Comments | **not supported at all** — a `;` or `#` line becomes a key | — |
| Section detection | first char `'['` **and** last char `']'` | `config.cpp:42` |
| Empty section `[]` | rejected: `load` returns `false` | `config.cpp:43-45` |
| Entry before any section | rejected: `load` returns `false` | `config.cpp:49-51` |
| Key/value split | first `'='`; value is everything after, **including further `=`** | `config.cpp:52-56` |
| Value whitespace | **not** trimmed (the line-level trim already removed trailing spaces) | — |
| Quoted values | not supported; quotes are literal characters | — |
| Duplicate keys | controlled by `IniConfigFile(bool allowDuplicates)` → `Dictionary2D::_allowDuplicates`. When `false`, `add` erases the previous entry first (`lists.h:2603-2605`) | `config.cpp:19-22` |
| Line length | `String line(256)` — but `String` grows, so this is only an initial capacity | `config.cpp:26` |
| Buffer per read | `TextReader::readString` uses `TCHAR buffer[BLOCK_SIZE]` = 512 chars, looping until `'\n'` | `streams.h:227-235` |

Writing back (`IniConfigFile::save`, `config.cpp:64-95`) emits `[category]`, then `key=value` (or bare `key` when the value is empty), then a blank line between categories. **Round-tripping is lossy**: comments do not exist, ordering follows `Map` insertion order, and originally-blank lines are not preserved.

### 6.2 The storage model and its ownership trap

`ConfigSettings` is `Dictionary2D<const TCHAR*, const TCHAR*>` (`common.h:36`). Values are stored as `Dictionary2D::VItem`, a tagged union (`lists.h:2529-2581`). **Which tag a value gets is decided by C++ overload resolution on the *constness* of the pointer**:

```cpp
VItem(TCHAR* literal)       { value.literal = literal; type = stString; }  // lists.h:2566  -> WILL be free()d
VItem(const TCHAR* literal) { value.literal = literal; type = stDWORD;  }  // lists.h:2571  -> will NOT be freed
```

and `freeVItem` (`lists.h:2516-2524`) frees only `stString` entries.

So **constness *is* the ownership annotation**:

| Call site | Argument | Deduced type | Tag | Freed? | Correct? |
|---|---|---|---|---|---|
| `config.cpp:56` | `strdup(value + pos + 1)` | `TCHAR*` | `stString` | ✅ yes | ✅ heap string, owned |
| `config.cpp:58` | `DEFAULT_STR` = `(const TCHAR*)NULL` | `const TCHAR*` | `stDWORD` | no | ✅ NULL, nothing to free |
| `config.cpp:104` | `_ELENA_::strdup(value)` | `TCHAR*` | `stString` | ✅ yes | ✅ |
| `config.cpp:112`, `:120` | `string.Clone()` | `TCHAR*` | `stString` | ✅ yes | ✅ (`Clone` is `strdup`, `altstrings.h:88`) |
| `config.cpp:125` | `_T("-1")` / `_T("0")` | `const TCHAR*` (array-to-pointer decay preserves `const`) | `stDWORD` | no | ✅ — but only by luck of the deduction rules |

This works, but it is **the most fragile construct in the whole layer**. Dropping a single `const` at any call site turns a string literal into a `free()` target. A modern rewrite must replace this with an explicit owning type (`std::string`) or a `std::variant`.

### 6.3 The `_ConfigFile` interface

`common.h:42-69` declares the abstract base:

| Member | Line | Notes |
|---|---|---|
| `load(const TCHAR*)` | 45 | pure virtual |
| `getCategoryIt(const TCHAR*)` | 47 | returns `ConfigCategoryIterator` (`common.h:37`) |
| `getSetting(cat, key, def=NULL)` | 49 | pure virtual |
| `getIntSetting(cat, key, def=0)` | 50-57 | `_ttoi` — MSVC-only, **no error detection** (returns 0 for garbage) |
| `getBoolSetting(cat, key, def=false)` | 59-66 | ⚠️ **`true` is spelled `"-1"`**, a Visual-Basic-ism; `compstr(value, _T("-1"))`. Any other value, including `"1"` or `"true"`, reads as `false`. |

`IniConfigFile` adds `setSetting` in four overloads (`const TCHAR*`, `int`, `size_t`, `bool`) and three `clear` overloads (`config.h:30-37`). Note `setSetting(…, int)` and `setSetting(…, size_t)` have **identical bodies** (`config.cpp:107-121`), and the `size_t` version calls `appendInt` — truncating on 64-bit.

---

## 7. Type & macro conventions (`common.h`, `tools.h`)

### 7.1 The fundamental types

| Name | Definition | Location | Assessment |
|---|---|---|---|
| `ref_t` | `#define ref_t size_t` | `common.h:21` | ⚠️ **A macro, not a typedef.** It cannot be forward-declared, cannot participate in overload resolution as a distinct type, cannot be put in a namespace, and textually replaces any identifier named `ref_t` anywhere downstream. Semantically it is an ELENA **object/message/constant reference index**, which is a 32-bit VM concept — tying it to `size_t` means it silently becomes 64-bit on LP64/LLP64 and desynchronizes from the on-disk format. |
| `DEFAULT_STR` | `#define DEFAULT_STR (const TCHAR*)NULL` | `common.h:22` | Also a macro; exists purely to force the `const` overload of `VItem` (§6.2). |
| `TCHAR` | from `<tchar.h>` | `common.h:14` | `char` or `wchar_t` per `_UNICODE`. |
| `ConfigSettings` | `Dictionary2D<const TCHAR*, const TCHAR*>` | `common.h:36` | |
| `ConfigCategoryIterator` | `_Iterator<ConfigSettings::VItem, _MapItem<const TCHAR*, ConfigSettings::VItem>, const TCHAR*>` | `common.h:37` | The raw template spelling leaks into public API. |
| `CategoryMap` | `Map<const TCHAR*, TCHAR*>` | `common.h:39` | |

**On `size_t` usage.** The codebase uses `size_t` for three logically distinct things, which is why 64-bit portability is hard:
1. genuine sizes/lengths (`MemoryDump::_used`, `String::_size`) — correct;
2. byte offsets into a `MemoryDump` (`_MemoryMapItem::next`, `lists.h:209`) — **serialized as 4 bytes**, so must be `uint32_t`;
3. ELENA VM references (`ref_t`) — a 32-bit domain concept, **also serialized as 4 bytes**.

Categories 2 and 3 must become explicit fixed-width types before any 64-bit build is attempted.

### 7.2 Macros and constants

| Macro / constant | Value | Location | Purpose |
|---|---|---|---|
| `#pragma warning(disable : 4996)` | — | `common.h:12` | MSVC-only; silences the "this function is deprecated / use `_s` variants" warnings for `strcpy`, `wcscpy`, `_gcvt` etc. Unknown-pragma warning on GCC/Clang. |
| `WINVER` | `0x0500` (Win2000) | `win32/unicode.h:11-13` | |
| `STR_PAGE_SIZE` | `0x0020` (32) | `altstrings.h:16` | `String` growth quantum |
| `BLOCK_SIZE` | `0x0200` (512) | `streams.h:16` | stream exchange buffer — **units are ambiguous**: `char` in one place, `TCHAR` in another |
| `SECTION_PAGE_SIZE` | `0x0040` (64) | `dump.h:16` | `MemoryDump` growth quantum; comment requires power of two |
| `LOCAL_PATH_LENGTH` | `0x0200` (512) | `files.h:15` | `LocalPath` capacity |
| include guards | `commonH`, `toolsH`, `listsH`, `streamsH`, `DumpH`, `filesH`, `configH`, `altstringsH`, `unicodeHdr` | each header | inconsistent casing; no `#pragma once` |

There are **no** `#pragma pack` directives anywhere in `common/` — which is itself a hazard, because raw structs *are* written to disk (§4.5) with whatever padding the ABI chooses.

### 7.3 Alignment helpers

Exactly one:

```cpp
// tools.h:230-236
inline unsigned int align(unsigned int number, const unsigned int alignment)
{
   if (number & (alignment - 1)) {
      return (number & ~(alignment - 1)) + alignment;
   }
   else return number & ~(alignment - 1);
}
```

Round up to a power-of-two boundary. Notes:
- **Silently wrong for non-power-of-two `alignment`** (no assert, no documentation beyond the `dump.h:16` comment).
- **Takes and returns `unsigned int`, not `size_t`.** Every 64-bit call site truncates: `dump.cpp:36` (`align(size, SECTION_PAGE_SIZE)` where `size` is `size_t`), `dump.cpp:144` (`::align(_position, alignment)`), `files.cpp:327` (`::align(_file.Position(), alignment)` where the argument is a `long`).
- `DumpWriter::align(size_t, unsigned char)` (`dump.h:151`, `dump.cpp:142-147`) pads with a fill byte; `FileWriter::align(int)` (`files.h:308`, `files.cpp:325-330`) pads with `'\0'`. Two different signatures for the same idea.
- `::align` resolves to `_ELENA_::align` only because of the file-scope `using namespace _ELENA_;`. In C++17 this collides conceptually with `std::align`; it is fragile qualified lookup.

### 7.4 Endianness

There is **no endianness handling of any kind** — no macro, no byte-swap function, no `htonl`-style layer. Every serialized integer is a raw host-order `memcpy`. The format is therefore *de facto* little-endian x86. See §4.5.

---

## 8. Portability audit

Severity legend: 🔴 blocker (does not compile / silently corrupts data) · 🟠 major (compiles after mechanical fixes but changes behaviour) · 🟡 minor (cosmetic or easily contained).

### 8.1 Headers and toolchain

| # | `file:line` | Construct | Why it breaks | Recommended replacement | Sev |
|---|---|---|---|---|---|
| 1 | `common.h:25` | `#include "win32\unicode.h"` | **Backslash in an `#include`.** GCC/Clang interpret `\u` as the start of a universal-character-name; the file is not found. Stops compilation at the very first header. | `#include "win32/unicode.h"`, then move behind a platform dispatch header | 🔴 |
| 2 | `win32/unicode.h:15` | `#include <windows.h>` | No such header on Linux/macOS. Also drags in ~1 MB of macros (`min`, `max`, `near`, `far`, `IN`, `OUT`) that collide with normal C++ identifiers. | Delete; replace conversions with ICU or a hand-rolled UTF-8/UTF-16 codec | 🔴 |
| 3 | `common.h:14` | `#include <tchar.h>` | MSVC/MinGW-only. Defines `TCHAR` and all `_tcs*` macros. | Delete; commit to UTF-8 `char` (see §9) | 🔴 |
| 4 | `common.h:18` | `#include <io.h>` | MSVC-only (`_access`, `_open`). | `<unistd.h>` | 🔴 |
| 5 | `files.cpp:13` | `#include <direct.h>` | MSVC-only (`_mkdir`, `_chdir`). | `<sys/stat.h>` + `<unistd.h>` | 🔴 |
| 6 | `common.h:12` | `#pragma warning(disable : 4996)` | MSVC-only pragma; GCC/Clang emit `-Wunknown-pragmas`. | `#if defined(_MSC_VER)` guard, or remove once the deprecated CRT calls are gone | 🟡 |
| 7 | `win32/unicode.h:11-13` | `#ifndef WINVER / #define WINVER 0x0500` | Win32 SDK version gate; meaningless elsewhere. | Delete | 🟡 |

### 8.2 `TCHAR` / `_T()` / `_UNICODE`

| # | `file:line` | Construct | Why it breaks | Recommended replacement | Sev |
|---|---|---|---|---|---|
| 8 | *pervasive* — `common.h:22,36-39,45-63`; all of `altstrings.h`; `streams.h:30,63,65,82,102,126,141,171,193,221,227,243-253,261,283,326,356,380,394,419,426,431`; `dump.h:173`; `dump.cpp:183`; `files.h` (~60 sites); `files.cpp:21,247,258,270,287,303,311,316`; `config.h:23-41`; `config.cpp:41,72,78,97,102-125`; `lists.h:213,216,231,234,239,264,266,279,281,1572,1579,2298,2305,2519,2533,2545-2547,2566-2574,2719-2743`; `tools.h:70,72,79,81,88,90,97,99,198-206,218-226` | `TCHAR` | Undefined without `<tchar.h>`. Even if `typedef`-ed to `wchar_t`, **`wchar_t` is 4 bytes on Linux/macOS and 2 on Windows** — see #13. | Replace the entire `TCHAR` model with `char` holding UTF-8 (recommended) or with `char16_t`/`std::u16string` if the UTF-16 module format must be preserved bit-for-bit | 🔴 |
| 9 | `altstrings.cpp:81`; `files.cpp:247,258,270,287,303`; `config.cpp:78,125`; `common.h:63` | `_T("…")` | MSVC macro; expands to `L"…"` or `"…"`. | Plain `"…"` (UTF-8) | 🔴 |
| 10 | `altstrings.h:149,219`; `altstrings.cpp:173`; `streams.h:291,388`; `dump.cpp:185` | `#ifdef _UNICODE` | Two divergent compilations of the same source. `dump.cpp:185-189` even makes the **on-disk literal stride** depend on it. | Delete the branch entirely; one encoding | 🔴 |
| 11 | `tools.h:70,72,79,81,88,90,99`, `altstrings.h:42-43,179,189,283,382,419`, `altstrings.cpp:25,37,49`, `files.h:181`, `streams.h:283,382,394,419`, `tools.h:200,205,224` | `_tcslen _tcscpy _tcsncpy _tcsncat _tcsrchr _tcschr _tcscmp _tcslwr _tcsupr` | All are `<tchar.h>` macros, absent everywhere else. `_tcslwr`/`_tcsupr` have **no POSIX equivalent at all** (there is no `wcslwr`/`strlwr` in standard C). | `std::string` member functions; for case folding use ICU (`u_strFoldCase`) or a documented ASCII-only helper | 🔴 |
| 12 | `common.h:54`, `altstrings.h:40` | `_ttoi` | MSVC macro for `atoi`/`_wtoi`. | `std::from_chars` (and actually detect errors) | 🟠 |
| 12b | `altstrings.cpp:23,35,47,59,69` | `_itot _i64tot _ltot` | MSVC-only integer→string. | `std::to_chars` / `std::format` | 🟠 |
| 12c | `tools.h:185,192` | `_gcvt` | MSVC-only float→string. | `std::to_chars` (round-trip correct, locale-independent) | 🟠 |

### 8.3 `wchar_t` width — the deepest problem

> **On Windows `sizeof(wchar_t) == 2` (UTF-16). On Linux and macOS `sizeof(wchar_t) == 4` (UTF-32).** The code encodes the number 2 as the literal shift `<< 1` in fifteen places. None of them is a `sizeof`. Every one becomes a factor-of-two under-allocation or wrong-stride read on a Unix build — the sort of bug that does not crash, it just produces corrupt module files.

| # | `file:line` | Construct | Effect on Linux/macOS | Replacement | Sev |
|---|---|---|---|---|---|
| 13 | `tools.h:149` | `malloc((wcslen(s) << 1) + 2)` in `strdup(const wchar_t*)` | Allocates **half** the needed bytes → heap overflow on every wide string duplication | `std::string` / `std::u16string` | 🔴 |
| 14 | `tools.h:160` | `createstr(wchar_t*&, len)`: `malloc(length << 1)` | half-size buffer | idem | 🔴 |
| 15 | `tools.h:170` | `recreatestr(wchar_t*&, len)`: `realloc(s, length << 1)` | half-size buffer | idem | 🔴 |
| 16 | `tools.h:215` | `movestr(wchar_t*, …)`: `memmove(s1, s2, length << 1)` | copies half the data | idem | 🔴 |
| 17 | `streams.h:70` | `readLiteral(wchar_t* s, len)`: `read(s, length << 1)` | reads half the characters, leaves the rest uninitialized | explicit `sizeof(char16_t)` | 🔴 |
| 18 | `streams.h:138` | `writeLiteral(const wchar_t*, len)`: `write(s, length << 1)` | writes half the string to the module file | idem | 🔴 |
| 19 | `dump.h:57` | `write(position, s, (wcslen(s) + 1) << 1)` | truncates every stored map key | idem | 🔴 |
| 20 | `dump.cpp:186` | `_position += ((getlength(s) + 1) << 1)` in `getLiteral` | reader desynchronizes from the stream after the first literal | idem | 🔴 |
| 21 | `files.cpp:111,129` | `fread((char*)temp, 2, count, _file)` | element size hardcoded to 2 while the destination is 4-byte `wchar_t` | `char16_t` buffer | 🔴 |
| 22 | `files.cpp:227,214` | `fwrite((const char*)s, 2, length, _file)` | idem, writing | idem | 🔴 |
| 23 | `files.cpp:140-143`, `files.cpp:188-192` | in-place widening loop `j = i << 1; buf[j] = buf[i]; buf[j+1] = 0;` | produces UTF-16 bytes into a buffer the caller believes holds 4-byte `wchar_t` | proper decoder | 🔴 |
| 24 | `streams.h:300,392,394,410` | `length >> 1`, `size >> 1` in `LiteralWriter::write` / `LiteralReader::read` | byte↔character conversions assuming 2 | idem | 🔴 |
| 25 | `altstrings.h:84-85`, `altstrings.cpp:28,40` | `__int64` | MSVC extension; unknown to GCC/Clang. | `int64_t` from `<cstdint>` | 🟠 |

### 8.4 Path separators and filesystem

| # | `file:line` | Construct | Why it breaks | Replacement | Sev |
|---|---|---|---|---|---|
| 26 | `files.h:27` | `lastchrpos(path, '\\')` in `checkExtension` | On POSIX `\` is a legal filename character, `/` is the separator → extension checks silently misfire | `std::filesystem::path::extension()` | 🔴 |
| 27 | `files.h:65` | `lastchrpos(path, '\\')` in `copyPath` | directory extraction returns the whole path | `path.parent_path()` | 🔴 |
| 28 | `files.h:84,86` | `if ((*this)[strLength-1] != '\\') _string.append('\\');` in `combine` | **builds Windows paths on every platform** | `path::operator/=` | 🔴 |
| 29 | `files.h:105` | `lastchrpos(path, '\\')` in `changeExtension` | idem | `path::replace_extension()` | 🔴 |
| 30 | `files.h:181` | `_tcsrchr(path, '\\')` in `FileNameTemplate::copyName` | idem | `path::stem()` | 🔴 |
| 31 | `altstrings.h:322` | `chrpos(path, '\\')` in `ReferenceNameTemplate::pathToName` | ELENA module names derived from paths come out wrong | `path` iteration | 🔴 |
| 32 | `files.h:15`, `files.h:195` | `LOCAL_PATH_LENGTH 0x200` + `LocalString<512>` with silently-failing copy | Paths >511 chars are truncated with **no error**; Linux `PATH_MAX` is 4096 | `std::filesystem::path` (unbounded) | 🟠 |
| 33 | `files.h:97-100`, and callers | `PathTemplate::lower()` | Assumes a case-insensitive filesystem. On Linux/case-sensitive APFS this maps distinct files onto one another and breaks module lookup | Remove; compare paths via `std::filesystem::equivalent` or canonical form | 🟠 |
| 34 | `files.cpp:247` | `writeNewLine()` → `_T("\r\n")` | Emits CRLF on Unix unconditionally | `'\n'`, or a configurable policy | 🟠 |
| 35 | `files.h:122,124` | `struct _stat` / `_tstat` | MSVC-only names | `std::filesystem::exists()` | 🔴 |
| 36 | `files.cpp:335,340` | `_mkdir(name)` / `_wmkdir(name)` | MSVC-only; POSIX `mkdir` needs a **mode** argument | `std::filesystem::create_directories()` | 🔴 |
| 37 | `files.cpp:345,350` | `_access` / `_waccess` | MSVC-only names | `std::filesystem::exists()` / `access()` | 🔴 |
| 38 | `files.cpp:21` | `_tfopen(path, mode)` | MSVC-only | `fopen`, or `std::ofstream` | 🔴 |
| 39 | `tools.h:56` | `_wremove(name)` | MSVC-only | `std::filesystem::remove()` | 🔴 |

### 8.5 Win32 API surface

| # | `file:line` | Construct | Why it breaks | Replacement | Sev |
|---|---|---|---|---|---|
| 40 | `win32/unicode.h:19` | `MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, …)` | Win32-only. `CP_ACP` also makes results **machine-locale-dependent**, so compilation is not reproducible. | ICU `ucnv_toUChars`, or a fixed UTF-8 decoder | 🔴 |
| 41 | `win32/unicode.h:26` | `WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, …, "?", &flag)` | idem | ICU, or a fixed UTF-8 encoder | 🔴 |
| 42 | `win32/unicode.h:24` | `BOOL flag` | Win32 typedef | `bool` | 🟡 |
| 43 | `win32/unicode.h:17,22` | Both functions are at **global scope**, outside `namespace _ELENA_` | Namespace pollution; collides with any other `ansiToUnicode` | Move into `_ELENA_` | 🟡 |

### 8.6 Pointer-size and 64-bit assumptions

| # | `file:line` | Construct | Why it breaks | Replacement | Sev |
|---|---|---|---|---|---|
| 44 | `lists.h:216` | `return (TCHAR*)((int)this + (int)this->key);` | **Casts `this` to `int`.** On LP64/LLP64 this truncates a 64-bit pointer to 32 bits → immediate segfault. | `reinterpret_cast<char*>(this) + key` with `key` as `uint32_t` | 🔴 |
| 45 | `lists.h:234` | `compstr((TCHAR*)((int)this + (int)this->key), key)` | idem | idem | 🔴 |
| 46 | `lists.h:266` | `grtstr(key, (TCHAR*)((int)this + (int)this->key))` | idem | idem | 🔴 |
| 47 | `lists.h:281` | `!grtstr((TCHAR*)((int)this + (int)this->key), key)` | idem | idem | 🔴 |
| 48 | `lists.h:395` | `return (Item*)((int)_map->_buffer.getArray() + _position);` | **Casts a heap pointer to `int`.** | `static_cast<char*>(buf) + pos` | 🔴 |
| 49 | `lists.h:421` | `_current = (Item*)((int)_map->_buffer.getArray() + _position);` | idem | idem | 🔴 |
| 50 | `lists.h:1579`, `lists.h:2305` | `return (const TCHAR*)(keyPos - position);` | Stores a **numeric offset inside a pointer-typed variable**, then later re-adds it to `this`. Works only because both are 32-bit. | dedicated `uint32_t` offset field | 🔴 |
| 51 | `lists.h:1593` | `int position = _buffer.Length();` | `size_t` → `int`; buffers > 2 GB wrap negative | `uint32_t` (documented cap) or `size_t` | 🟠 |
| 52 | `lists.h:1598`, `lists.h:2325` | `ref_t storedKey = (ref_t)storeKey(position, key);` | Casts a `const TCHAR*` to `size_t` and back | explicit offset type | 🔴 |
| 53 | `lists.h:1599`, `lists.h:2326` | `_buffer.writeDWord(position + 4, storedKey)` | **Hardcodes `offsetof(Item, key) == 4`**, i.e. `sizeof(size_t)==4`. On 64-bit the key is written into the middle of `next`. | `offsetof(Item, key)` | 🔴 |
| 54 | `lists.h:1594`, `lists.h:2322` | `_buffer.write(position, &item, sizeof(item))` | Raw struct write: **padding + pointer size become the on-disk format** | explicit field-by-field serialization | 🔴 |
| 55 | `lists.h:1873`, `lists.h:1884` | `write(&_cache, sizeof(Item) * _count)` / `read(&_cache, …)` | idem, for the cached array | idem | 🔴 |
| 56 | `dump.h:32-35` | `int& operator[](size_t position) { return *(int*)(_buffer + position); }` | **Unaligned 32-bit access at an arbitrary byte offset** + a strict-aliasing violation (`char*` → `int*`). Traps on ARMv5/SPARC, and GCC ≥ 6 with `-O2` may miscompile it. | `memcpy`-based `readU32/writeU32` helpers | 🔴 |
| 57 | `lists.h:2157,2159,2190,2249,2318,2337,2344,2345` | `_buffer[index << 2]`, `_buffer[tale]` | Uses #56 with byte offsets — unaligned by construction | idem | 🔴 |
| 58 | `streams.h:53-56` | `bool readDWord(size_t& dword) { return read((void*)&dword, 4); }` | On 64-bit, reads 4 bytes into an 8-byte object; **top half is uninitialized garbage** | read into `uint32_t`, then widen | 🔴 |
| 59 | `lists.h:1373,1637-1639,2077,2363-2364` | `writer->writeDWord(_count)` / `(_buffer.Length())` / `(_tale)` where all are `size_t` | Silently narrows on 64-bit | explicit `uint32_t` cast + range check | 🟠 |
| 60 | `lists.h:1647-1651`, `lists.h:2374-2377` | `int length = reader->getDWord();` then `_buffer.reserve(length)` | signed `int` for a size | `uint32_t` | 🟠 |
| 61 | `tools.h:230` | `unsigned int align(unsigned int, const unsigned int)` | All `size_t` call sites truncate at 4 GB | `size_t align(size_t, size_t)` | 🟠 |
| 62 | `dump.cpp:144` | `size_t aligned = ::align(_position, alignment);` | `size_t` → `unsigned int` → `size_t` round trip | see #61 | 🟠 |
| 63 | `lists.h:1478,1501,1522,1548,1602,2243,2329` | `size_t beginning = (size_t)_buffer.getArray();` | Pointer-as-integer arithmetic; UB-adjacent but at least width-correct | `char*` arithmetic | 🟡 |
| 64 | `common.h:21` | `#define ref_t size_t` | Ties a 32-bit VM concept to the platform word size **and** to the 4-byte on-disk encoding | `typedef uint32_t ref_t;` | 🔴 |

### 8.7 `long`, endianness, and misc.

| # | `file:line` | Construct | Why it breaks | Replacement | Sev |
|---|---|---|---|---|---|
| 65 | `files.h:213-216`, `files.cpp:47,52,54,57,76` | `long Position()`, `long Length()`, `bool seek(long)` | `long` is 32-bit on Windows/ILP32 but **64-bit on LP64** — the type silently changes width. On Windows it caps files at 2 GB. | `int64_t` + `fseeko`/`ftello` or `std::filesystem::file_size` | 🟠 |
| 66 | `altstrings.h:83`, `altstrings.cpp:62` | `appendLong(long n)` / `_ltot` | idem | `int64_t` | 🟡 |
| 67 | `files.cpp:27-28,290-291,306-307` | `unsigned short signature = 0xFEFF;` written/read raw | **Host-endian BOM.** A big-endian build writes a BE BOM then LE-decodes it. | explicit byte pair `{0xFF, 0xFE}` | 🟠 |
| 68 | *all* `writeDWord`/`readDWord`/`writeWord` | raw host-order `memcpy` of multi-byte integers | **The `.nl` module format is implicitly little-endian**, with no marker and no swap layer | explicit LE serialization helpers | 🟠 |
| 69 | `streams.h:186-191` | `writeAsciiLiteral(const wchar_t* s, len)` writes `s[i]`'s **low byte only** | Little-endian-dependent and silently lossy above U+00FF | real transcoding | 🟠 |
| 70 | `tools.h:137,145` | `_ELENA_::strdup` | Shadows POSIX `strdup`; with `using namespace _ELENA_` the call is ambiguous under glibc unless `_ELENA_::` is spelled out (as `config.cpp:104` and `altstrings.h:88` in fact do) | rename to `copystr`, or drop for `std::string` | 🟠 |
| 71 | `elc/codeblocks/elc.cbp` (and all `.cbp`) | `-march=pentium2`, `-march=i686` | **x86-32-only build flags**; fail on ARM/AArch64, and pin the build to a 1997 microarchitecture | remove; let CMake choose | 🟠 |
| 72 | `elc/vs/elc9.vcproj:24,45` vs `:104,:123` | Debug is `CharacterSet="1"`+`UNICODE`; Release is `CharacterSet="2"` and **no `UNICODE`** | Debug and Release builds of `elc` produce **mutually unreadable `.nl` module files** | single encoding, no `TCHAR` | 🔴 |
| 73 | `lists.h:1917` | `typedef _MapItem<Key, T> Item;` inside `HashTable` — `KeyStored` defaults to `true` | Every `HashTable` with a pointer key silently `strdup`s and `free`s keys, even where the caller expected borrowing | make `KeyStored` explicit | 🟠 |
| 74 | `lists.h:1773` | `Item _cache[cacheSize];` where `Item` has non-trivial members | Default-initialized POD array; `_cache` is written to disk raw (#55) including padding | explicit init + field serialization | 🟠 |
| 75 | `common.h:12` and everywhere | no `#pragma once`, inconsistent guard naming (`DumpH` vs `dumpH` vs `filesH`) | Cosmetic, but `DumpH` vs a future `dumpH` would collide | `#pragma once` | 🟡 |
| 76 | `win32/unicode.h:19`, `:26` | `size_t length` passed to Win32's `int cbMultiByte` / `cchWideChar` parameters | Silent 64→32 narrowing **on Windows itself** (LLP64) for strings > 2 GB; also a signed/unsigned conversion warning | explicit bounded `int` cast with a range check | 🟠 |
| 77 | `tools.h:76`, `:85`, `:94` | `return p - s;` from an `int`-returning function | `ptrdiff_t` → `int` narrowing in `chrpos`/`lastchrpos` | return `ptrdiff_t` or `size_t` with an explicit sentinel | 🟡 |
| 78 | `tools.h:202` | `for (int i = _tcslen(s) ; i >= pos ; i--)` | `size_t` → `int` narrowing in `insertstr` | `size_t` loop with a guarded condition | 🟡 |
| 79 | `config.cpp:118` | `string.appendInt(value)` where `value` is `size_t` | `setSetting(…, size_t)` silently truncates to `int` on 64-bit | add `appendSize`/`appendUInt64` | 🟠 |
| 80 | `lists.h:2531-2536` | `union Value { const TCHAR* literal; int number; bool flag; size_t size; Map<…>* map; }` | Writing `.size` (8 bytes on LP64) and reading `.number` (4 bytes) truncates — and `VItem(size_t)` at `lists.h:2561` does exactly that, tagging it `stDWORD` so `operator int()` reads `.number` | `std::variant` with distinct alternatives | 🟠 |
| 81 | `files.cpp:66` | `int pos = ftell(_file);` inside `File::Eof()` | `long` → `int` narrowing; breaks for files > 2 GB even on Windows | `int64_t` + `ftello` | 🟠 |
| 82 | `streams.h:65` | `return read(&ch, sizeof(TCHAR));` | `sizeof(TCHAR)` is 1, 2 or 4 depending on platform **and** build flags — the number of bytes consumed per character is not stable across builds of the same source | fixed-width character type | 🔴 |
| 83 | `dump.h:61` | `write(position, &ch, 2);` writing a single `wchar_t ch = 0` | Writes 2 bytes of a 4-byte object on Unix — leaves the rest of the terminator unwritten | `sizeof` or a fixed-width type | 🔴 |
| 84 | `lists.h:1590` | `_buffer.writeDWord(0, 4);` | Reserves the first 4 bytes as the "top" pointer slot — another hardcoded 4 tied to `sizeof(size_t)==4` | named constant / explicit `uint32_t` | 🟠 |
| 85 | `tools.h:218-226` | `_tchlwr` — identifier begins with `_t` | Names beginning with `_` followed by a lowercase letter are reserved at file scope; `_t*` specifically collides with the entire `<tchar.h>` macro family | rename (e.g. `lowerChar`) | 🟡 |

### 8.7b Raw occurrence counts across the layer

The tables above list *distinct issues*. For sizing the mechanical work, these are the raw occurrence counts from an exhaustive grep of all 13 files (5,513 lines):

| Category | Count | Concentrated in |
|---|---|---|
| Windows-only headers | 4 (`<tchar.h>`, `<io.h>`, `<windows.h>`, `<direct.h>`) | `common.h:14,18`, `win32/unicode.h:15`, `files.cpp:13` |
| MSVC/Win32-only functions | ~60 call sites | `tools.h`, `altstrings.*`, `files.*` |
| `TCHAR` occurrences | **216** | `altstrings.h` (58), `files.h` (33), `streams.h` (29), `lists.h` (24) |
| `_T(` occurrences | 9 | `files.cpp` (6), `config.cpp` (2), `altstrings.cpp` (1), `common.h` (1) |
| `#ifdef _UNICODE` blocks | 6 | `altstrings.h:149,219`, `altstrings.cpp:173`, `streams.h:291,388`, `dump.cpp:185` |
| `__int64` | 4 | `altstrings.h:84,85`, `altstrings.cpp:28,40` |
| `#pragma` (MSVC-only) | 1 | `common.h:12` |
| Backslash path logic | 8 + 1 CRLF | `files.h:26,65,84,85,105,181`, `altstrings.h:322`, `common.h:25`; `files.cpp:247` |
| Pointer-size / 32-bit assumptions | **~75** | **`lists.h` (~55)**, `streams.h`, `dump.h` |
| `long` assumed 32-bit | 13 | `files.h:213,214,216,254,255,257,286,287`, `files.cpp:47,52,54,57,76` |
| `wchar_t` assumed 2 bytes | 19 | `tools.h`, `streams.h`, `dump.h/.cpp`, `files.cpp` |
| Endianness / raw POD writes | ~25 | `streams.h`, `dump.h/.cpp`, `files.cpp`, `lists.h` |

### 8.8 Blocker count by file

Counting *distinct audit entries* from §8.1–§8.7 (not raw occurrence counts, which are in §8.7b):

| File | 🔴 | 🟠 | 🟡 | Verdict |
|---|---|---|---|---|
| `lists.h` | 15 | 7 | 1 | **Worst file.** All 6 `(int)this`/`(int)ptr` casts, all raw struct blits, the hardcoded `+4` offsets |
| `files.h` / `files.cpp` | 14 | 6 | 0 | Most 🔴 *by category breadth* — paths, MSVC CRT, `long`, BOM, CRLF |
| `tools.h` | 5 | 4 | 3 | Every 2-byte-`wchar_t` allocation bug lives here |
| `streams.h` | 5 | 2 | 0 | `readDWord(size_t&)` is the highest-impact single line |
| `common.h` | 4 | 0 | 2 | Small file, but it is the root — fixing it is a prerequisite for everything |
| `dump.h` / `dump.cpp` | 4 | 1 | 0 | `operator[] → int&` is the aliasing/alignment landmine |
| `altstrings.h` / `.cpp` | 3 | 2 | 0 | Mostly mechanical once `TCHAR` is gone |
| `win32/unicode.h` | 3 | 1 | 3 | 32 lines, 100% Win32 — delete entirely |
| `config.h` / `config.cpp` | 0 | 2 | 0 | **Least portable-hostile file in the layer** |
| build files (`.cbp`/`.vcproj`) | 1 | 1 | 0 | `CharacterSet` mismatch, `-march=pentium2` |

---

## 9. Modernization recommendations

### 9.1 Guiding decisions to make first

Three decisions gate everything else. Make them explicitly, before writing code.

**Decision 1 — the string encoding. Recommendation: UTF-8 in `std::string`, everywhere.**
The `TCHAR` model must die; it is the root of blockers #8–#25 and #72. The only question is what replaces it. Choosing `char16_t`/`std::u16string` would preserve the `.nl` module format byte-for-byte, minimizing risk to the engine — but it permanently keeps ELENA on a Windows-shaped encoding and makes every interaction with POSIX APIs, LLVM (which is UTF-8/`StringRef` throughout), and modern toolchains a conversion. Choose **UTF-8 `std::string`**, convert to UTF-16 only at the Win32 boundary (`_wfopen` etc.), and accept a **module format version bump**. Since the LLVM backend will change the code sections anyway, do the format break once.

**Decision 2 — `ref_t`. Recommendation: `typedef uint32_t ref_t;`**
`#define ref_t size_t` (`common.h:21`) is wrong on two counts: it is a macro, and it couples a 32-bit VM concept to the host word size *and* to a 4-byte on-disk encoding. Making it `uint32_t` is a one-line change that resolves blockers #58, #59, #64 and makes the serialization honest. If the new VM needs a 64-bit address space, introduce a *separate* `addr_t`.

**Decision 3 — the module file format. Recommendation: define it explicitly, little-endian, fixed-width.**
Today the format is "whatever `memcpy` of a struct produced on 32-bit MSVC x86". Replace `write(&item, sizeof(item))` (`lists.h:1594`, `lists.h:2322`) with explicit field-by-field LE serialization. This kills blockers #53–#57, #67, #68 in one pass and is a prerequisite for cross-compilation.

### 9.2 Per-subsystem verdict

| Subsystem | Files | Verdict | Replacement | Effort | Risk | Reasoning |
|---|---|---|---|---|---|---|
| **`win32/unicode.h`** | 33 lines | 🔴 **DELETE** | ICU (`icu::UnicodeString`) if full Unicode is needed; otherwise a ~200-line hand-rolled UTF-8↔UTF-16 codec | **S** (1–2 days) | **Low** | Two functions, seven call sites. `CP_ACP` makes current behaviour locale-dependent and therefore already broken; nothing worth preserving. Do this **first** — it unblocks everything. |
| **`tools.h`** | 262 lines | 🟠 **MOSTLY DELETE** | `std::string` methods, `<algorithm>`, `std::from_chars`/`std::to_chars`, `<cstdint>` | **S** (2–3 days) | **Low** | ~90% is `str*`/`wcs*` wrappers made redundant by `std::string`. **Keep and fix**: `align()` (widen to `size_t`), `test()`, `calcTabShift()`, `mapReferenceKey`/`mapLiteralKey` (they define the module hash bucket layout — changing them changes the file format). **Delete**: `strdup`, `createstr`, `recreatestr`, `movestr`, `freestr`, `compstr`, `grtstr`, `emptystr`, `getlength`, `insertstr`, `doubleToStr`, `_tchlwr`, `chrpos`, `lastchrpos`. |
| **`altstrings.h/.cpp`** | 645 lines | 🟠 **REPLACE core, KEEP semantics** | `std::string` + free functions | **M** (1–2 weeks) | **Medium** | `_String`/`String`/`LocalString` are a worse `std::string` — delete outright. **But `Quote`, `ReferenceNameTemplate`, `NamespaceTemplate`, `IdentifierTemplate`, `PrivateMessageTemplate` encode ELENA *language semantics*** (the `'` reference separator, the `%n` escape syntax, the two-level private-message rule at `altstrings.h:442`). Port these as free functions over `std::string_view` — **do not** try to reimplement from scratch, port them line by line. Risk is in `LocalString`'s silent truncation: code that relied on `_copy` returning `false` (`altstrings.h:183`) will behave differently once strings grow freely. Audit those call sites. |
| **`streams.h`** | 443 lines | 🟢 **KEEP the interfaces, REWRITE the implementations** | Keep `StreamReader`/`StreamWriter` as abstract interfaces; reimplement concrete classes | **M** (1 week) | **Low** | The `StreamReader`/`StreamWriter` **abstraction is genuinely good** and is the seam through which the whole compiler serializes. `std::iostream` is a poor replacement (slow, locale-infected, awkward binary handling). Keep the vtable, but: make `readDWord`/`writeDWord` take `uint32_t`, add explicit LE helpers, delete `writeAsciiLiteral(const wchar_t*)` (#69), fix `LiteralWriter::write` (Appendix A #3). This is the **highest value-per-effort** item in the layer. |
| **`dump.h/.cpp`** | 287 lines | 🟠 **REPLACE storage, KEEP interface** | `std::vector<uint8_t>` inside, same public API | **S** (3–4 days) | **Low-Medium** | `MemoryDump` is `std::vector<char>` with `realloc` and a growth quantum of 64 bytes (`dump.h:16` — raise it to ≥4 KB, this alone is a measurable compile-speed win). **Must fix `operator[]` returning `int&`** (#56) — replace with `readU32(pos)`/`writeU32(pos, v)` using `memcpy`; this is a mechanical but wide-reaching change because `MemoryHashTable` uses it as its bucket array. **Keep `getArray()`** only if the JIT genuinely needs a raw pointer; otherwise remove it — it is the abstraction leak that forces every `(int)ptr` cast in `lists.h`. |
| **`files.h/.cpp`** | 717 lines | 🔴 **REPLACE wholesale** | `std::filesystem` (C++17) + `std::fstream` or retained `FILE*` | **M** (1–2 weeks) | **Medium** | Path handling (`PathTemplate`, `FileNameTemplate`, `LOCAL_PATH_LENGTH`) maps **1:1** onto `std::filesystem::path` — `combine`→`operator/=`, `changeExtension`→`replace_extension`, `copyPath`→`parent_path`, `copyName`→`stem`, `exists`→`std::filesystem::exists`, `createPath`→`create_directories`. This is the single largest source of 🔴 blockers (13) and also the easiest to fix correctly. **The risk is behavioural**: removing `lower()` (#33) changes module resolution on case-sensitive filesystems, and removing the 512-char cap changes what previously silently truncated. Keep `File`'s `FILE*` (portable already), but **implement `feUTF8`** — it is currently a lie (§3.5) — and make CRLF a policy, not a constant (#34). |
| **`config.h/.cpp`** | 196 lines | 🟡 **KEEP the format, REWRITE the storage** | `std::map<std::string, std::map<std::string, std::string>>`, or a small INI library (inih, ~300 lines) | **S** (2–3 days) | **Low** | The INI format is **public API** — it is the `.prj` and `.cfg` file format users write by hand. Do not change it. Do fix: add comment support (`;`/`#`), trim leading whitespace, and **replace the `-1`-means-true convention** (`common.h:63`) with something that also accepts `true`/`1`/`yes` (keep `-1` for compatibility). The `const`-decides-ownership trick (§6.2) evaporates the moment values are `std::string`; that removal alone justifies the work. |
| **`lists.h` — `List`, `BList`, `Stack`, `_List`, `_BList`** | ~545 lines | 🟢 **DELETE, replace with STL** | `std::vector` / `std::list` / `std::stack` | **S** (3–5 days) | **Low** | Straight substitution. The only friction is the `Eof()`-vs-`end()` iteration idiom, which is a mechanical rewrite, and the `_freeT` function pointer, which becomes `std::unique_ptr` in the element type. **Prefer `std::vector` over `std::list`** where the code only appends and iterates (which is most places) — a real performance win. |
| **`lists.h` — `Queue`, `CList`** | ~145 lines | 🟢 **DELETE — do not port** | none | **XS** (hours) | **None** | ⚠️ Both are **dead code — zero instantiations anywhere in `elenasrc/`** (§1.6). Deleting `CList` also lets you delete `_BList::circle`/`shiftNext`/`shiftPrevious`. Do not spend a single hour designing replacements for these; earlier analysis that assumed the IDE used `CList` was wrong. |
| **`lists.h` — `Cache`** | ~100 lines | 🟠 **KEEP (port as-is)** | none | **S** | **Low** | A 100-line fixed ring cache (§2.5d). No STL equivalent, no dependencies, no portability issues. Port unchanged onto `std::array`. **Live, and on hot paths** — two engine consumers (`engine/module.h:19`, `engine/win32/x86jitcompiler.h:112`); benchmark before changing its eviction policy. |
| **`lists.h` — `Map`** | ~280 lines | 🔴 **REPLACE — but carefully** | `std::vector<std::pair<K,V>>` where insertion order matters; `std::unordered_multimap` where it doesn't | **M** (1 week) | **HIGH** | ⚠️ `Map` is **not** `std::map` (§2.5b): O(n) lookup, insertion-ordered, duplicates allowed, and `exclude()`≠`erase()`. Naively substituting `std::map` will **silently reorder config output** (`config.cpp:72-93` writes in iteration order) and **silently drop duplicate keys** that `Dictionary2D(allowDuplicates=true)` depends on. Audit each of the ~10 instantiations in §1.5 individually and pick the right replacement per site. This is where a mechanical port will introduce bugs. |
| **`lists.h` — `HashTable`** | ~215 lines | 🟠 **REPLACE** | `std::unordered_multimap` with a custom hasher | **S** (2–3 days) | **Low-Medium** | Close enough to `unordered_multimap` to substitute, **except** that chains are kept sorted (`lists.h:2053-2059`) and the bucket count is a compile-time constant with no rehashing. Risk is contained: **there is exactly one instantiation in the whole tree**, `elenasrc/elc/parsertable.cpp:15`. Verify that the parser table does not rely on within-bucket key ordering, then substitute. Also fix the `index > hashSize` off-by-one (`lists.h:2015`, `:2050`). |
| **`lists.h` — `MemoryMap`, `MemoryHashTable`, `CachedMemoryMap`** | ~700 lines | 🔴 **REWRITE as an explicit serialization layer** | a hand-written module-format reader/writer | **L** (3–5 weeks) | **HIGH** | ⚠️ **This is the hard part of the whole modernization.** These are not containers, they are the **on-disk module format** (§2.5a). They contain 14 of the 🔴 blockers, including every `(int)this` cast (#44–#50) and the hardcoded offset-4 patch (#53). Do **not** try to make them 64-bit clean in place — the offset-linked, raw-struct-write design is fundamentally incompatible with pointer-size independence. Instead: **(a)** define the `.nl` format explicitly (Decision 3), **(b)** build in-memory representations on ordinary STL containers, **(c)** write dedicated `serialize()`/`deserialize()` functions that emit the explicit format. Effort is dominated by the engine/JIT code that reads these buffers directly (`engine/section.h`, `engine/jitlinker.cpp`), not by `lists.h` itself. Sequence this **after** the LLVM backend decision, since that may change what needs serializing. |
| **`lists.h` — `Dictionary2D`** | ~145 lines | 🟠 **REPLACE** | `std::map<K, std::map<SK, std::variant<int, std::string, …>>>` | **S-M** (3–5 days) | **Medium** | The tagged union maps directly onto `std::variant` (C++17). The **entire `const`-as-ownership hazard (§6.2) disappears** with `std::string`. Two consumers: `ConfigSettings` (`common.h:36`) and `ProjectSettings` (`elc/project.h:16`). Watch the insertion-order dependency inherited from `Map`. |
| **`lists.h` — iterators** | ~170 lines | 🟢 **DELETE** | STL iterators | **S** | **Low** | Falls out of the container replacements. Adopting real iterator concepts unlocks `<algorithm>` and range-based `for` across the whole codebase — a large readability dividend. |
| **`common.h`** | 73 lines | 🟠 **SPLIT** | `types.h` (fixed-width typedefs) + `config_iface.h` (`_ConfigFile`) | **S** (1 day) | **Low** | The umbrella-include pattern (§1.2) creates a fragile fixed ordering and murders compile times. Split into real headers with `#pragma once` and explicit includes. Convert both macros (`ref_t`, `DEFAULT_STR`) to typedefs/constants. |
| **Layering violation** (`_writeIterator`/`_readToMap`) | `lists.h:1376,1389,2080,2093` ↔ `engine/section.h:49,66` | 🔴 **FIX** | dependency inversion | **S** (1 day) | **Low** | The common library calls functions defined in the engine (§1.3). Whatever else happens, this must be inverted — pass a serialization callback/policy in, or move `write`/`read` out of the container templates entirely (which the `MemoryMap` rewrite does anyway). |

### 9.3 Recommended sequencing

The dependency graph (§1.3) dictates a bottom-up order. Each phase leaves the tree compiling and testable.

| Phase | Work | Gate |
|---|---|---|
| **0** | Build infrastructure: replace `.cbp`/`.vcproj` with CMake; drop `-march=pentium2`; get a `-Wall -Wextra` baseline on MSVC **and** GCC/Clang; **write module-format round-trip golden tests** (compile a corpus, hash the `.nl` output) | Golden tests green on the *existing* code |
| **1** | Kill `TCHAR`: delete `win32/unicode.h`, `tools.h` string wrappers, `altstrings.h` core → `std::string` UTF-8. Fix `common.h:25` backslash include. Convert `ref_t` to `uint32_t`. | Windows build still bit-identical `.nl` output, or a deliberate one-time format bump |
| **2** | `files.h` → `std::filesystem`. Implement `feUTF8`. Fix `lower()`/case-sensitivity. | Toolchain runs end-to-end on Linux for a project with ASCII paths |
| **3** | `dump.h` → `std::vector<uint8_t>`, kill `operator[] → int&`. `streams.h` interface fixes (`uint32_t`, explicit LE). `config.h` → STL + comment support. | 64-bit build produces correct `.nl` files |
| **4** | **Delete dead code first** (`Queue`, `CList`, `_BList::circle`/`shift*` — §1.6, ~250 lines, zero risk). Then simple containers (`List`/`BList`/`Stack`/`HashTable`/`Dictionary2D`) → STL. Port `Cache` verbatim. Invert the `_writeIterator` layering violation. | Full STL iteration idiom; `<algorithm>` available |
| **5** | `Map` audit — per-instantiation replacement decisions (§9.2, HIGH risk row) | No behavioural diffs in golden tests |
| **6** | `MemoryMap`/`MemoryHashTable` → explicit serialization layer. Sequence **with** the LLVM backend work, not before it. | Cross-compilation and big-endian targets viable |

### 9.4 Effort summary

| Bucket | Estimate |
|---|---|
| Phases 0–2 (compiles and runs on Linux, 32-bit-equivalent semantics) | **4–6 weeks** |
| Phases 3–5 (64-bit clean, STL containers, modern types) | **5–8 weeks** |
| Phase 6 (module format rewrite; couples to LLVM backend) | **3–5 weeks**, best folded into the backend project |

**The single highest-leverage action** is Phase 1. `TCHAR` accounts for the majority of the 🔴 blockers, and its removal makes every other subsystem's port mechanical rather than exploratory.

---

## Appendix A — latent bugs found while reading

These are pre-existing defects, independent of portability. Several will change observable behaviour when fixed, so they should be triaged before the port rather than during it.

| # | `file:line` | Bug | Impact |
|---|---|---|---|
| 1 | `dump.h:112`, `dump.h:175` | `void* getArray() const { _dump->getArray(); }` — **missing `return`** | UB. Returns whatever is in the return register. Both `DumpWriter::getArray` and `DumpReader::getArray`. |
| 2 | `lists.h:1180-1183` | `Map::end()` returns `Iterator(_top)`, not `Iterator(_tale)` | `end()` is an alias for `start()`. Any caller using it as a terminator loops forever or terminates immediately. |
| 3 | `streams.h:314-322` | Non-`_UNICODE` `LiteralWriter::write(void* s, size_t)` — wrong signature (`void*` vs. the base's `const void*`, so it **does not override**) *and* **no `return` statement** | The ANSI build of `LiteralWriter` is fundamentally broken: `StreamWriter::write` stays pure-virtual-dispatched to nothing sensible. |
| 4 | `streams.h:392-395` | `size_t size = length >> 1; _tcsncpy((TCHAR*)s, (_text + _offset), size >> 1);` — **double shift** | `LiteralReader::read` copies a quarter of the requested characters, then advances `_offset` by half. Desynchronizes the reader. |
| 5 | `lists.h:1791-1800` | `CachedMemoryMap::getIt` — when `_cached` is true and the key is **not** found, control falls off the end of the function with no `return` | UB on every cache miss in the cached state. |
| 6 | `lists.h:725-734` | `_BList::cut(T item)` — if the item is not found, falls off the end without returning | UB. |
| 7 | `lists.h:2015`, `:2050`, `:2246`, `:2315` | `if (index > hashSize) index = hashSize - 1;` — should be `>=` | A hash function returning exactly `hashSize` indexes one past the end of `_table[hashSize]` / the bucket array. `mapReferenceKey` (`tools.h:240-250`) can return 26 with `hashSize` 29, so this is latent rather than live — but `tools.h:246` has the same `> 26` / `>= 26` off-by-one. |
| 8 | `lists.h:1939` | `for(_hashIndex = 0; !_hashTable->_table[_hashIndex] && _hashIndex < hashSize; _hashIndex++);` — **dereferences before the bounds check** | Reads `_table[hashSize]` (one past the end) when all buckets are empty. Same pattern at `lists.h:2157`. |
| 9 | `dump.cpp:80-88` | `MemoryDump::insert`: `resize(_used + length)` then `memmove(_buffer + position + length, _buffer + position, _used - position - length)` — `_used` has **already been increased** by `resize`, so the length is short by `length` | The last `length` bytes of the moved region are not copied; `insert` corrupts the tail of the buffer. |
| 10 | `files.cpp:213-214` | `ansiToUnicode(s, temp, count); if (fwrite((const char*)s, 2, length, _file) <= 0)` — converts into `temp` then **writes `s`**, not `temp`; also writes `length` elements rather than `count` | `File::writeLiteral(const char*, …)` in UTF-16 mode writes raw ANSI bytes reinterpreted as UTF-16, and over-runs the source buffer. |
| 11 | `files.cpp:163-165` | `unicodeToAnsi(temp, s, wcslen(temp)); s[count] = 0;` — NUL is written at `count`, not at the actual converted length | Leaves uninitialized bytes between the converted text and the terminator. |
| 12 | `lists.h:1016` | `Queue(T defaultItem, void(*freeT)(T)) : _list(freeT) { defaultItem = defaultItem; }` — **self-assignment to the parameter**; `_defaultItem` is never initialized | `Queue::pop()` on an empty queue returns garbage. |
| 13 | `lists.h:1825-1826` | `if (_cache[i].key == key) if (_cache[i].key == key && _cache[i].item == item) return true;` — duplicated condition, first `if` has no body | Harmless but indicates copy-paste; the outer `if` makes the function's `_cached` branch effectively unreachable for a miss (see #5). |
| 14 | `lists.h:1802-1813` | `CachedMemoryMap::get` — the non-cached branch calls `_map.get(key)` and **discards the result**, then returns `DefaultValue()` | Lookups always fail once the cache has spilled. |
| 15 | `lists.h:1829-1831` | `CachedMemoryMap::exist(Key, T)` — same pattern: `_map.exist(key, item)` result discarded, always returns `false` | idem. |
| 16 | `dump.cpp:38` | `_buffer = (char*)realloc(_buffer, _total);` — **return value not checked for NULL** | On allocation failure the old pointer is leaked and `_buffer` becomes NULL; next write segfaults. Same for `malloc` at `dump.cpp:22`, `dump.cpp:30`, and in `tools.h:141,149,155,160,165,170`. |
| 17 | `tools.h:198-206` | `insertstr` — no bounds checking; writes `len` bytes past the original end of `s` | Buffer overflow if the caller under-allocated. |
| 18 | `tools.h:137-151` | `strdup("")` returns `NULL` (via the `emptystr` guard), not an empty string | Round-tripping an empty config value yields NULL, changing its `VItem` tag and thus its ownership (§6.2). |
| 19 | `lists.h:728` | `for (int i = 0 ; i < _count ; i++)` — signed/unsigned comparison against `size_t _count` | Warning; wrong for counts > `INT_MAX`. Same at `lists.h:1794`, `:1805`, `:1823`, `:1840`. |
| 20 | `lists.h:1277`, `:1312` | `if (!_top);` — empty statement then `else if` | Works as written (an intentional "do nothing if empty") but is a classic reading hazard. |

---

*Document generated from a complete reading of `elenasrc/common/` at ELENA 1.5.0.0 (2009). Line references are to the files as committed in this tree.*
