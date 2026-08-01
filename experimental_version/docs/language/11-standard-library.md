# ELENA 1.5 — Standard Library Reference

> **Status of this document:** a catalogue of the ELENA 1.5.0 standard library as it exists in
> `src/`, written from the sources. The original API documentation (`dat/api2html/*.txt`, rendered
> to HTML by `api2html`) was already out of date in 2009 — `whatsnew.txt:199` and `:271` both say
> *"ELENA API and documentation are out of date"* — and it contains a dozen typos that silently
> drop entries. Where this document and `dat/api2html` disagree, **the `.l` source wins**;
> notable divergences are flagged.

**Companion document:** [`10-elena-language-reference.md`](10-elena-language-reference.md) for
the language itself.

---

## Table of contents

1. [Package map](#1-package-map)
2. [Per-module catalogue](#2-per-module-catalogue)
3. [Core protocols](#3-core-protocols)
4. [Dependency graph](#4-dependency-graph)
5. [Platform coupling](#5-platform-coupling)
6. [Modernization notes](#6-modernization-notes)

---

## 1. Package map

Seven compilation units, 7,942 lines of ELENA, plus 6,146 lines of x86 assembly that the library
calls into.

| Package | `.prj` | Output | Modules | LOC | Purpose |
|---|---|---|---|---|---|
| **`$elena`** (root) | *none* — compiled into every program | — | 1 | 45 | `Object`, `$Nil`, `$TypeInstance`, `Type`. The implicit root of every class hierarchy. |
| **`std`** | `src/std/std.prj` | `lib/std` | 6 | 3,333 | Data types, collections, iteration, I/O adapters, control patterns. |
| **`sys`** | `src/sys/sys.prj` | `lib/sys` | 2 | 107 | Program startup templates and the event/handler mechanism. |
| **`ext`** | `src/ext/ext.prj` | `lib/ext` | 4 | 318 | Convenience layer: console, text files, word parsing, date, RNG. |
| **`gui`** | `src/gui/gui.prj` | `lib/gui` | 4 | 1,252 | Platform-**independent** control and graphics abstraction. |
| **`win32`** | `src/win32/win32.prj` | `lib/win32` | 9 | 2,421 | Win32 binding: handles, GDI, console, files, window classes, styles. |
| **`win32'socket`** | `src/win32/socket/win32socket.prj` | `lib/win32/socket` | 2 | 466 | Winsock TCP sockets, event-driven. |

Module-level detail:

| Module | File | LOC | Contents |
|---|---|---|---|
| *(root)* | `src/elena.l` | 45 | `Object`, `$Nil`, `$TypeInstance`, `Type` |
| `std'basic` | `src/std/basic.l` | 1,623 | numbers, literals, strings, chars, booleans, arrays, indexers, extensions, type symbols |
| `std'basic'math` | `src/std/basic/math.l` | 57 | 11 float functions |
| `std'basic'memory` | `src/std/basic/memory.l` | 325 | `ByteArray`, `Buffer`, binary readers/writers |
| `std'properties` | `src/std/properties.l` | 617 | enumerator/indexer protocol, reader/writer adapters, content & key properties |
| `std'patterns` | `src/std/patterns.l` | 356 | actions, agents, loops, `Break`, `Control`, dynamic group/cast |
| `std'collections` | `src/std/collections.l` | 355 | `List`, `Circle`, their enumerators, read-only façades |
| `sys'templates` | `src/sys/templates.l` | 45 | `Simple`, `System`, `CommandCycle`, `CommandLineArguments` |
| `sys'events` | `src/sys/events.l` | 62 | `Controller`, `Handlers`, `GroupAdapter` |
| `ext'io` | `src/ext/io.l` | 108 | `Console`, `TextFile`, `ListPrinter` |
| `ext'text` | `src/ext/text.l` | 143 | `WordParsing`, `Words`, `PrintValue` |
| `ext'patterns` | `src/ext/patterns.l` | 6 | `Printing` (one symbol) |
| `ext'utilities` | `src/ext/utilities.l` | 61 | `RandomValue`, `Date`, `Now` |
| `gui'graphics` | `src/gui/graphics.l` | 335 | `Point`, `Size`, `Rectangle`, `Pen`, `Brush`, `Image`, `Drawing`, `Plotter` |
| `gui'controls` | `src/gui/controls.l` | 599 | 18 control classes + 9 event-action classes |
| `gui'controls'properties` | `src/gui/controls/properties.l` | 204 | `Caption`, `Size`, `Location`, `Enabled`, `Visible`, `Checked`, `Items`, … |
| `gui'forms` | `src/gui/forms.l` | 114 | `Custom`, `SDI`, `Dialog`, `SDIDialog` |
| `win32'api` | `src/win32/api.l` | 661 | `DWORD`, `Handle`, `HWND`, `HDC`, `HPEN`, `HBRUSH`, `HBITMAP`, `MSG`, `Message`, factories |
| `win32'api'constants` | `src/win32/api/constants.l` | 137 | 88 Win32 constants |
| `win32'api'controls` | `src/win32/api/controls.l` | 491 | native control wrappers and the window procedure handlers |
| `win32'api'styles` | `src/win32/api/styles.l` | 458 | 31 style descriptor symbols |
| `win32'api'factories` | `src/win32/api/factories.l` | 50 | `HandlerType`, `ClassType`, 4 registered window classes |
| `win32'api'graphics` | `src/win32/api/graphics.l` | 136 | `Canvas`, `Bitmap`, pen/brush/image factories |
| `win32'io` | `src/win32/io.l` | 338 | console reader/writer, file reader/writer, `TextFileRO/New/Append` |
| `win32'system` | `src/win32/system.l` | 75 | `$GUI` — the GUI object pool and event root |
| `win32'applications` | `src/win32/applications.l` | 75 | `$SDIApp` — the SDI message loop |
| `win32'socket'primitives` | `src/win32/socket/primitives.l` | 163 | `SOCKET`, readers/writers, socket factories |
| `win32'socket'controls` | `src/win32/socket/controls.l` | 303 | `ServerSocket`, `ClientSocket`, `RemoteSocket` |

The assembly the library binds to:

| Runtime package | File | LOC | Routines |
|---|---|---|---|
| `standard` | `src/asm/standard.asm` | 2,804 | 82 |
| `elena` | `src/asm/elena.asm` | 1,481 | 38 (compiler-emitted) |
| `win32` | `src/asm/win32.asm` | 1,423 | 63 |
| `winsock` | `src/asm/winsock.asm` | 362 | 13 |
| `extended` | `src/asm/extended.asm` | 76 | 3 |

---

## 2. Per-module catalogue

Notation: `msg` = unary message, `msg:` = takes one argument, `$msg` = private/protected.
Roles are listed in *italics*.

### 2.1 Root — `src/elena.l`

| Entity | Kind | Protocol | Description |
|---|---|---|---|
| `Object` | class | `new`, `ifNotNil`, `back:`, `ifSame:` | Root of all classes (`$elena'object`). Four methods, three of them empty. `back:` returns its argument — the basis of the `back:T \| back:F` ternary. |
| `$Nil` | class | `ifNotNil` (fails) | The nil object. Its only method fails, which is how nil checks work. |
| `$TypeInstance` | class | `$getClassName:`, `$typecast:`, `$ifSameType:` | `#field(4)` — holds a reified VMT pointer. Produced by `#type X`. |
| `Type` | symbol | `Name`, `ifSame:`, `of:` | The public reflection façade over `$TypeInstance`. The entirety of ELENA reflection. |

```elena
#class Object
{
    #method new      []
    #method ifNotNil []
    #method back : aParam = aParam.
    #method ifSame : anObject = #inline elena'22 (self, anObject).
}
```
— `src/elena.l:3-12`

### 2.2 `std'basic` — `src/std/basic.l`

**Abstract bases**

| Class | Line | Protocol | Description |
|---|---|---|---|
| `Indexer` | 10 | `prop'$getIndexer`, `prop'$getIndex`, `prop'$goto:`, `prop'$next`, `prop'$previous` | Base for all random-access cursors. `$next` / `$previous` fail at the bounds. |
| `Magnitude` | 55 | `==` `!=` `>` `<` `>=` `<=` | Comparison implemented once, in terms of two abstract private messages `$equal:` and `$less:`. Every ordered type inherits it. |

**Value types** (all have a binary body)

| Class | Line | Body | Protocol | Notes |
|---|---|---|---|---|
| `IntNumber` | 102 | `#field(4)` | `+ - * /`, `and: or: xor: not shift: anyMask: allMask:`, `clone`, `new:` | `[dbg:int]`. The compiler's `integerclass`. |
| `Integer` | 674 | inherits | + `<<` `+=` `-=` `*=` `/=` `next` `previous` | The **mutable** variant. `IntNumber` is the value; `Integer` is the variable. |
| `LongNumber` | 202 | `#field(8)` | same shape as `IntNumber`, 64-bit | `[dbg:long]`. `$getInteger` *fails* on overflow. |
| `LongInteger` | 720 | inherits | + assignment operators | |
| `RealNumber` | 312 | `#field(8)` | `+ - * /`, `clone`, `new:` | `[dbg:real]`. The compiler's `realclass`. |
| `Real` | 766 | inherits | + assignment operators | |
| `CharValue` | 435 | `#field(2)` | comparison, `clone`, `new:` | UTF-16 code unit. |
| `Char` | 903 | inherits | + assignment operators | |
| `Literal` | 376 | `#field.` (dynamic) | `+`, `Length`, `clone`, comparison, `prop'$getIndexer` | Immutable UTF-16 string. `new` **fails** — you cannot construct one directly, only via `LiteralType create:`. The compiler's `literalclass`. |
| `String` | 804 | `theLiteral` | `Length`, `+`, `+=`, `<<`, `clone`, `clear`, `new:` | Mutable string. Role *`Empty`*. |
| `Array` | 490 | `#field.` (dynamic) | `Length`, `prop'$getIndexer` | Fixed-size object array. `new` fails. The compiler's `arrayclass`. |
| `Boolean` | 935 | — | `? ! == != << and: or: xor: if: new:` | A mutable boolean **variable**. Roles *`TrueValue`* / *`FalseValue`*. |
| `VariantValue` | 629 | `theValue` | `$getLiteral/$getInteger/$getReal/$getLong/$getBool`, `new:` | Auto-converting box; `#annex theValue.` |

**Boolean singletons**

| Symbol | Line | Protocol |
|---|---|---|
| `True` | 518 | `?` (succeeds), `if:`, `==`, `!=`, `and:`, `or:`, `xor:`, `not`, `$getBool` |
| `False` | 572 | `!` (succeeds), same rest |
| `NilValue` | 655 | `ifNotNil` (fails), `==`, `!=` |

**Adapters**

| Class | Line | Description |
|---|---|---|
| `LiteralIndexer` | 1022 | Character cursor over a `Literal`; `#annex theChar.` so the cursor *is* the character. |
| `StringIndexer` | 1062 | Adds `$seek:`, `$insert:`, `$delete:` over a mutable `String`. Role *`EOF`*. |
| `ArrayIndexer` | 1170 | Element cursor over an `Array`; `#annex theItem.` |
| `StringWriter` | 1222 | Collects `$writeAsLiteral:` into a `String`. |

**Property symbols**

| Symbol | Line | Meaning |
|---|---|---|
| `HiWord : n` / `LoWord : n` | 1245 / 1250 | high / low 16 bits of a 32-bit int |
| `HiDWord : n` / `LoDWord : n` | 1255 / 1260 | high / low 32 bits of a 64-bit int |
| `SubString : range` | 1265 | substring from a `{ for = s. from = i. till = j. }` record |

**Extensions** — stateless objects annexed into the value types (see
[§7.5 of the language reference](10-elena-language-reference.md#75-extensions)):

| Symbol/class | Line | Adds |
|---|---|---|
| `BasicExtension` | 1272 | `prop'$input:`, `prop'$printOn:` |
| `IntegerExtension` | 1289 | `$getInteger`, `$getLong`, `$getReal`, `$toLiteral`, `$toBool`, `prop'$readFrom:`, `prop'$writeTo:` |
| `LongExtension` | 1324 | `$getLong`, `$toBool`, `$toLiteral` |
| `RealExtension` | 1341 | `$getReal`, `$toLiteral`, read/write |
| `LiteralExtension` | 1364 | `$getLiteral`, `$toInteger`, `$toLong`, `$toReal`, `$toBool`, `Length`, `prop'$getEnumerator`, `prop'$printOn:`, `prop'$writeTo:` |
| `StringExtension` | 1407 | `prop'$getIndexer`, `prop'$input:`, `prop'$readFrom:`, `prop'$getWriter`, `+=` |
| `ArrayExtension` | 1447 | `prop'$getEnumerator` |
| `BooleanExtension` | 1454 | `$toInteger`, `$toLong`, `$toLiteral`, print/read/write |

**Type symbols** — the factory + converter for each type. Every one answers `of : x` (convert)
and, where allocation is possible, `create : profile`.

| Symbol | Line | `of:` | `create:` | `$getGroup` |
|---|---|---|---|---|
| `IntegerType` | 1504 | `$getInteger` | — | Basic + Integer |
| `LongType` | 1516 | `$getLong` | — | Basic + Long |
| `RealType` | 1530 | `$getReal` | — | Basic + Real |
| `LiteralType` | 1544 | `$getLiteral` | `standard'38` alloc by `Length` | Literal + Indexer |
| `StringType` | 1567 | — | — | Literal + Indexer + String |
| `ArrayType` | 1574 | `$getArray` | `standard'39` alloc + per-item init | Array + Indexer |
| `BooleanType` | 1605 | `$getBool` | — | BooleanExtension |
| `CharType` | 1620 | `$getChar` | — | — |

### 2.3 `std'basic'math` — `src/std/basic/math.l`

Eleven parameterised symbols, all thin wrappers over `standard'65`–`standard'74`:

| Symbol | Primitive | | Symbol | Primitive |
|---|---|---|---|---|
| `Abs : n` | `standard'65` | | `Exp : n` | `standard'70` |
| `Rounded : n` | `standard'66` | | `Frac : n` | *(computed: `n - Int::n`)* |
| `Int : n` | `standard'67` | | `Ln : n` | `standard'71` |
| `ArcTan : n` | `standard'68` | | `Sin : n` | `standard'72` |
| `Cos : n` | `standard'69` | | `Sqrt : n` | `standard'73` |
| | | | `Pi` | `standard'74` |

All take and return `RealNumber`. **Missing:** `Tan`, `power`, `modulus`, `Log10`, `Min`, `Max`
— `doc/todo.txt:136-137` records the gap. `examples/pi/pi.l:6-17` hand-writes `Power`.

### 2.4 `std'basic'memory` — `src/std/basic/memory.l`

| Entity | Line | Protocol | Description |
|---|---|---|---|
| `ByteArray` | 10 | `Length`, `$getByteArray`, `$getBOF`, `prop'$getReader/$getWriter` | Raw byte block, `#field.`. `new` fails. |
| `Buffer` | 44 | `Length`, `<<`, `clear`, `new:`, `$getSize`, `$getDump` | Growable byte buffer: a `ByteArray` with a 4-byte length prefix (`$getBOF = 4`). |
| `ByteArrayReader` | 98 | `seek:`, `Position`, `$readAsInt32:`, `$readAsLiteral:`, `$readAsBuffer:` | Little-endian binary reader. |
| `ByteArrayWriter` | 146 | `seek:`, `Position`, `$writeAsInt32:`, `$writeAsLiteral:`, `$writeAsBuffer:` | Binary writer. |
| `BufferReader` / `BufferWriter` | 190 / 241 | same, bounds-checked | Wrap the byte-array pair, tracking the buffer's logical length. |
| `BufferExtension` | 289 | `prop'$readFrom:`, `prop'$writeTo:` | Makes a buffer serialisable. |
| `ByteArrayType` | 304 | `of:`, `create:` | Allocator (`standard'82`). |
| `BufferType` | 316 | `of:`, `create:`, `$getGroup` | |

This is the closest ELENA 1.5 gets to raw memory. **There is no way to point a `ByteArray` at an
address you did not get from the allocator.**

Note a real bug: `ByteArrayWriter::new` (`memory.l:157`) sends `$GetByteArray` where every other
site sends `$getByteArray`. Harmless only because the language is case-insensitive.

### 2.5 `std'properties` — `src/std/properties.l`

The iteration and stream-adapter layer. This module defines the protocol names that the rest of
the library implements.

| Entity | Line | Protocol | Description |
|---|---|---|---|
| `EnumIndexerProxy` | 9 | `$getIndex`, `$getContent`, `$setContent:`, `$goto:`, `$next`, `$previous`, `new:` | Presents an *enumerator* as an *indexer* by walking. Roles *`BOF`* / *`EOF`*. O(n) `$goto:`. |
| `PositionCriteria` | 161 | `$seekin:` | Seek by index. Subclass supplies `Position`. |
| `ValueCriteria` | 171 | `$seekin:` | Seek by value. Subclass supplies `Value`. |
| `Enumerator` | 211 | `new:`, `next`, `run:`, `start`, `get`, `end` (via annex) | The `foreach` driver. `#annex theProxy.` Role *`BOF`*. |
| `IndexEnumerator` | 497 | `new:`, `start`, `next`, `seek:`, `insert:`, `delete:`, `run:`, `clone` | `foreach` that also exposes `prop'Index::item`. |
| `Indexer` | 342 | `new:`, `Position`, `get`, `set:`, `seek:`, `insert:`, `delete:`, `next`, `previous`, `start` | Public random-access façade. |
| `Content` | 394 | `new:`, `get`, `set:`, `exchange:` | Read/write handle on a cursor's current element. |
| `ContentProxy` | 457 | `new:`, `$getContent`, `$setContent:` | Adapter in the other direction. |
| `KeyValue` | 437 | `new:`, `$getKey`, `$getContent` | Key/value pair; `#annex (theValue).` |
| `Range` | 478 | `new:`, `isValid:` | Bounds object over `{ from. till. }`. |
| `TextWriter` | 254 | `new:`, `<<` | Sends `$printOn:` to whatever you write. |
| `TextReader` | 276 | `new:`, `>>` | Sends `$input:`. |
| `Writer` | 320 | `new:`, `<<` | Binary — sends `$writeTo:`. |
| `Reader` | 298 | `new:`, `>>` | Binary — sends `$readFrom:`. |

Property symbols and extensions:

| Symbol | Line | Meaning |
|---|---|---|
| `Index : indexer` | 422 | the cursor's current position (a copy) |
| `ContentValue : obj` | 427 | `obj $getContent` |
| `ContentKey : obj` | 432 | `obj $getKey` |
| `ArrayIterator` / `ArrayBIterator` | 181 / 195 | forward / backward iteration strategies for `#group` |
| `EEnumerator` | 571 | makes an enumerable usable with `EIteration` |
| `EContent` | 582 | adds `get` / `set:` / `exchange:` |
| `IndexerExtension` | 603 | **adds the `@` operator** — three lines, used everywhere |
| `ERange` | 611 | adds `in : value` |

### 2.6 `std'patterns` — `src/std/patterns.l`

Reusable actions (objects with `proceed`) and agents (objects that drive an enumeration).

| Entity | Line | Protocol | Description |
|---|---|---|---|
| `CountAction` | 11 | `new`, `proceed`, `$getResult` | Counts elements. |
| `SearchAction` | 30 | `new:`, `proceed:`, `$getResult` | Finds the first equal element; role *`Found`*. |
| `ValidateAction` | 61 | `new:`, `proceed:`, `$ifPositive` | All-elements-satisfy; role *`Invalid`*. |
| `GetMaxAction` | 112 | `new:`, `proceed:`, `$getResult` | Maximum. |
| `CopyAction` | 134 | `new:`, `proceed:`, `$getBufferSize` | Stream-to-stream copy through a 64-byte buffer. |
| `KeyComparator` | 95 | `new:`, `==` | Compares by `prop'ContentKey`. |
| `SearchAgent` | 189 | `new:`, `proceed:`, `==`, `!=` | `SearchAgent::coll == x` is "contains". |
| `ValidateAgent` | 216 | `new:`, `proceed:` | `agent proceed:validator` → boolean. |
| `Agent` | 233 | `new:`, `proceed:`, `prop'$Owner` | Runs an arbitrary action over a collection. |
| `DecrimentLoop` | 167 | `new:`, `run:` | Counted loop, n down to 1. **Spelled "Decriment" in the API** — reproduce verbatim. |
| `Variable` | 283 | `new:` | A mutable cell; `#annex theValue.` |
| `Counter : coll` | 162 | symbol | element count |
| `Break` | 252 | `$ifBreak`, `==`, `!=` | The loop-break sentinel; `^ ctrl'Break.` from an action stops `run:`. |
| `PositiveResult` | 269 | `==` | `PositiveResult == action` tests `$ifPositive`. |
| `Control` | 274 | `run:`, `if` | Always-succeeds host for an action. `ctrl'Control run: => [ ... ]` is how you get `^` inside a symbol. |
| `EIteration` | 302 | `run:` | Generic iteration over `$getIterator` / `$getCurrent:` / `$step:`. |
| `DynamicGroup` | 321 | `+=` | Grows a `#group` at run time (reallocating via `standard'39/44/45`). |
| `DynamicCast` | 340 | `+=` | Grows a `#cast` at run time — this is the event-handler list. |

### 2.7 `std'collections` — `src/std/collections.l`

| Entity | Line | Protocol | Description |
|---|---|---|---|
| `Item` | 14 | `new:`, `$getObject`, `$getNext`, `$setNext:`, `prop'$getContent/$setContent:` | Singly-linked list node; `#annex theObject.` |
| `List` | 50 | `new`, `+=`, `$getList`, `$getTop`, `$getTale`, `$setTop:`, `prop'$getEnumerator` | Singly-linked list with a tail pointer. Role *`Empty`*. `#annex ListGroup.` |
| `Circle` | 112 | as `List` + `shift:` | Circular list. Role *`Empty`*. |
| `Indexer` | 170 | `prop'$insert:`, `prop'$delete:` | `EnumIndexerProxy` specialised for lists. |
| `Enumerator` | 185 | `new:`, `start`, `next`, `end`, `get`, `set:`, `insert:`, `delete:` | List cursor. Roles *`BOF`* / *`EOF`*. |
| `CircleEnumerator` | 302 | as above, wraps at the top | |
| `ListExtension` | 325 | `Count`, `prop'$getIndexer`, `prop'$printOn:` | |
| `ReadOnly : list` | 350 | symbol | `#annex(aList)` façade whose `+=` fails |
| `ReadOnlyEnumerator : e` | 339 | symbol | façade whose `set` / `delete` / `insert` fail |

**`List` is the only container in the library.** There is no dictionary, no set, no growable
array, no stack, no queue, no tree. `Count` is O(n) — it runs a `CountAction` over the whole
list (`collections.l:327`).

*(`whatsnew.txt:541` mentions `std'matrix` and `std'arrays` from 1.0.5; both were removed.)*

### 2.8 `sys` — `src/sys/templates.l`, `src/sys/events.l`

**`sys'templates`** — the startup templates. `'entry` is forwarded to one of these.

| Symbol | Line | Behaviour |
|---|---|---|
| `Simple` | 25 | `'Program proceed:CommandLineArguments.` — console default (`console.cfg`). |
| `System` | 29 | `'system run:('program ifNotNil:'program'Modules).` — GUI default (`gui.cfg`). |
| `CommandCycle` | 34 | REPL: loop reading into and writing from a controller until it answers `Break`. |
| `CommandLineArguments` | 8 | splits `'program'CommandLine` with `ext'text'WordParsing` into a read-only `List`. |
| `IdleModules` | 45 | `{ ifNotNil [] }` — the do-nothing module list. |

**`sys'events`** — the event mechanism, 62 lines that the whole GUI rests on.

| Entity | Line | Protocol | Description |
|---|---|---|---|
| `Controller` | 6 | `new`, `$getEvents`, `$setEvents:` | Holds a `#cast` of handler objects; `#annex theEvents.` so sending an event to the controller broadcasts it. Role *`EOF`* for the "no handlers yet" state. |
| `GroupAdapter` | 41 | `ctrl'$getGroup`, `ctrl'$setGroup:` | Bridges `Controller` to `DynamicCast`. |
| `Handlers : obj` | 50 | `+=` | The public API: `events'Handlers::obj += { $onClick = ... }`. |
| `ControllerType` | 60 | `of:` | |

```elena
#symbol Handlers : anObject =
{
    += aHandler
    [
        #group(GroupAdapter, ctrl'DynamicCast, anObject) += aHandler.
    ]
}.
```
— `src/sys/events.l:50-56`. Three composed objects and one `+=` is the entire event system.

**A control publishes itself as an event source** by answering one message:

```elena
#method events'$getController = theEvents.
```
— `src/gui/controls.l:47`

### 2.9 `ext` — convenience layer

| Module | Entity | Line | Description |
|---|---|---|---|
| `ext'io` | `Console` | 53 | `<<` / `>>` forwarding to `'program'Output` / `'program'Input`. |
| | `ListPrinter : list` | 42 | Comma-separated printing of any enumerable. |
| | `ListPrintAction` | 7 | The per-item action; role *`First`* suppresses the leading comma. |
| | `TextFile : name` | 104 | An enumerable over the lines of a file. **Delegates to the forward `'handlers'TextFileRO`** — this is what keeps `ext` off Win32. |
| | `TextFileEnumerator` | 68 | Line cursor; role *`BOF`*. |
| `ext'text` | `WordParsing` | 124 | `new:`, `run:` — tokenise text on spaces. |
| | `Words : text` | 117 | The enumerable behind it. |
| | `WordEnumerator` | 33 | The tokeniser proper; roles *`Space`* / *`Token`* are a two-state machine. |
| | `PrintValue` | 13 | Renders any object to a literal via `$printOn:`. |
| `ext'patterns` | `Printing` | 5 | `aValue => ('program'Output << aValue)` — the whole module. |
| `ext'utilities` | `RandomValue : max` | 21 | `#external extended'2` |
| | `RandomGenerator` | 17 | `#static`, `#field(8)`, seeded from `GetSystemTime` |
| | `Date`, `Timespan`, `DateType`, `Now` | 26–61 | `#field(8)` FILETIME. **No formatting, no arithmetic, no component accessors** — the class is a stub. Added in 1.5.0 (`whatsnew.txt:14`). |

### 2.10 `gui` — platform-independent controls

The `gui` package **never names Win32**. Every platform touch goes through a forward.

**`gui'controls`** — `src/gui/controls.l`

| Class | Line | Base | Notes |
|---|---|---|---|
| `Control` | 21 | — | `new:`, `$getControl`, `$ifEmbeddedControl`, `events'$getController`; `#annex(theHandle).` |
| `Container` | 54 | `Control` | `$getControls`, `$addControl:` |
| `StaticFrame`, `StaticLabel`, `GroupBox` | 78–95 | `Control` | each is a `$createHandle` one-liner |
| `InnerStaticFrame/Label/GroupBox` | 99–116 | above | `$ifEmbeddedControl` fails ⇒ not added to the parent's control list |
| `Edit` | 122 | `Control` | `<<`, `basic'$toLiteral`; annexes literal+string+indexer |
| `UpDown` | 138 | `Control` | |
| `Button` | 147 | `Control` | |
| `RadioButton`, `InnerRadioButton` | 154, 161 | `Button` | |
| `Combobox` | 175 | `Control` | `$getItemAt:`, `$insertItem:`, `$deleteItem:`, `$getObjects` |
| `Listbox` | 214 | `Control` | same protocol |
| `StatusBar` | 253 | `Control` | `$getItemAt:` |
| `Panel` | 270 | `Container` | owns an `InnerGroupBox` frame; handles `$onResize` / `$onInit` |
| `RadioButtonGroup` | 317 | `Panel` | auto-lays-out its buttons; `basic'$toInteger` = selected index |
| `Paintbox` | 411 | `Control` | raises `$onRender:` with a `Canvas` |
| `ImageBox` | 440 | `Paintbox` | |
| `Indexer` | 449 | — | cursor over a control's items |

Nine **event action** classes (`PaintAction`, `ClickAction`, `ChangeAction`, `CommandAction`,
`KeyPressedAction`, `LButtonAction`, `CloseAction`, `InitAction`, `ResizeAction`,
lines 529–599), each a single `proceed:` that forwards to the target's controller:

```elena
#class ClickAction
{
    #method proceed : aParam
    [
        ^ events'ControllerType of:(self for) $onClick:aParam.
    ]
}
```

**`gui'controls'properties`** — `src/gui/controls/properties.l`. Twelve parameterised property
symbols plus three classes:

| Property | Line | Write | Read |
|---|---|---|---|
| `Caption : ctl` | 23 | `<< literal` | `basic'$toLiteral` |
| `Range : ctl` | 35 | `<< range` | `prop'$getRange` |
| `Size : ctl` | 47 | `<< size` | `graph'$getSize` |
| `Location : ctl` | 59 | `<< point` | `graph'$getCoord` |
| `Checked : ctl` | 71 | `<< bool` | `basic'$getBool` |
| `Enabled : ctl` | 86 | `<< bool` | `basic'$getBool` |
| `Visible : ctl` | 101 | `<< bool` | `basic'$getBool` |
| `Items : ctl` | 116 | `+= value` | `Count`, `prop'$getIndexer`, `prop'$getRange` |
| `SelectedIndex : ctl` | 175 | `<< n` | `basic'$toInteger` |
| `SelectedItem : ctl` | 187 | — | the item |
| `DialogResult : win` | 168 | `<< result` | — |
| `Item` / `Tag` / `Controls` | 136/147/192 | classes | list item, user data, child list |

**`gui'graphics`** — `src/gui/graphics.l`

| Entity | Line | Protocol |
|---|---|---|
| `Point` / `PointVar` | 16 / 44 | `X`, `Y`, `+`, `-`, `<<`, `+=`, `-=` |
| `Size` / `SizeVar` | 37 / 134 | `Width`, `Height`, `<<` |
| `Rectangle` | 94 | `X`, `Y`, `Width`, `Height`, `From`, `Till`, `isValid:` |
| `PointType` / `SizeType` | 168 / 175 | `of:` |
| `Image` | 184 | `new:`; `#annex theImage.` |
| `Pen` / `Brush` | 198 / 215 | `new`; delegate to `'gui'graphics'penType` / `'brushType` |
| `ImageList` | 235 | `new:`, `+=`, `prop'$getIndexer` |
| `Drawing` | 260 | `new:`, `<<`, `+=`, `-=`, `proceed:` — a positioned draw cursor; dispatches by `identify:` |
| `Plotter` | 317 | `new:`, `<<`, `proceed:` — moveTo / lineTo |

**`gui'forms`** — `src/gui/forms.l`, 114 lines, four classes:

| Class | Line | Description |
|---|---|---|
| `Custom` | 9 | `Container` + `$initialize` (raises `InitAction`) |
| `SDI` | 26 | Main window; `$onClose` calls `'program exit.` |
| `Dialog` | 59 | Modal; role *`Modal`*; drives `'gui'DialogLoopAction`, disables the parent |
| `SDIDialog` | 108 | `SDI` with dialog styling |

### 2.11 `win32` — the platform layer

**`win32'api`** — `src/win32/api.l`. Value wrappers, all `#field(4)` or larger:

| Class | Line | Body | Key messages |
|---|---|---|---|
| `DWORD` | 21 | `#field(4)` | `basic'$equal:`, `basic'$toInteger` |
| `LPSTR` | 38 | inherits | `new:` — address of a literal (`standard'40`) |
| `LPVOID` | 48 | inherits | `new:` — address of an object (`win32'15`) |
| `Handle` | 58 | inherits | `$getHandle` |
| `HWND` | 65 | inherits | `refresh`, `free`, `$getHDC`, `$show:`, `$enable:`, `$getText`, `$setText:`, `$getSize`, `$setSize:`, `$getLocation`, `$setLocation:` |
| `HINSTANCE` | 152 | inherits | `$getHINSTANCE`, `$setHandle:` |
| `FileHandle` | 164 | inherits | `basic'$readAsLiteral:`, `basic'$writeAsLiteral:`, `close` |
| `HDC` | 275 | `#field(4)` | `$setTextColor:`, `$setBkMode:`, `$setBkColor:`, `close` |
| `HPEN` / `HBRUSH` | 312 / 333 | `#field(4)` | `$getColor`, `close`, `$select:` |
| `HBITMAP` | 375 | inherits | `$getSize`, `$getHDC:`, `close` |
| `PAINTSTRUCT` | 354 | `#field(64)` | `open:`, `close:` |
| `MSG` | 209 | `#field(28)` | `$peek:`, `$dispatch:`, `$wait` |
| `Message` | 233 | `#field(12)` | `new:`, `$getID`, `$getWParam`, `$getLParam`, `$return:`, `$send:`, `$post:` |
| `Cursor` | 186 | `theCursor` | `new:`, `$set:` |

Factories and symbols: `HandleType`, `HWNDType` (`create:` calls `CreateWindowExW`),
`FileHandleType` (`CreateFileW`), `ClassType` (`RegisterClassW` — builds the `WNDCLASS` by
writing 10 fields into a 40-byte `ByteArray`), `BitmapType`, `PenFactory`, `BrushFactory`,
`Instance`, `StdOutput`, `StdInput`, `WindProcPtr`, `SystemColor`, `SystemColorBrush`,
`CommandLine`, plus `SDILoopAction` and `DialogLoopAction`.

**`win32'api'controls`** — `src/win32/api/controls.l`. Native control wrappers that implement
the `gui'` protocol and, crucially, **the window-procedure handlers**:
`$onDestroy`, `$onSetFocus`, `$onPaint`, `$onClose`, `$onSetCursor`, `$onKeyDown`, `$onChar`,
`$onCommand`, `$onSetColorButton`, `$onSetColorStatic`, `$onLButton`, `$onUserEvent`. These are
the message names hard-coded in `src/asm/elena.asm:1443-1478`.

**`win32'api'styles`** — 31 message-less descriptor symbols (`SDIWindow`, `Edit3D`, `Button3D`,
…) each answering `$getStyle`, `$getExStyle`, `$getClass`, `$getWindowHandle`, `$createHandler`,
plus the `Parent`/`Owner`/`Width`/`Color`/`Destination`/`Source`/`Text`/`FileName`/`ImageCopy`
parameter symbols. These are what `'gui'styles'*` forwards resolve to.

**`win32'api'factories`** — `HandlerType`, `ClassType` and the four registered window classes
(`ELENA.SDIWINDOW.3.0`, `ELENA.PANEL.3.0`, `ELENA.Paintbox.3.0`, `ELENA.NONVISIBLE.3.0`).

**`win32'api'graphics`** — `Canvas` (BitBlt, MoveTo, LineTo, TextOut, FillRect), `Bitmap`,
and the pen/brush/image type symbols.

**`win32'io`** — `ConsoleWriter`/`ConsoleReader`/`Console`, `FileWriter`/`FileReader`,
`TextFileWriter`/`TextFileReader`/`TextFileBuffer`, and the three file-mode classes
`TextFileRO` / `TextFileNew` / `TextFileAppend`. `StdConsole`, `StdOutput`, `StdInput` are
`#static`.

**`win32'system`** — `$GUI`: an object pool mapping window handles to ELENA objects (`+=` / `-=`
using `win32'55` / `win32'56`), an `events'Controller`, and `run:` which starts the program and
calls `$onError` on failure. Published as `#static GUI`.

**`win32'applications`** — `$SDIApp`: obtains `'MainWindow`, opens it, and runs
`#loop theLoopAction proceed:aHWND.` — the message pump. Role *`Stopped`*.

### 2.12 `win32'socket`

| Entity | Line | Description |
|---|---|---|
| `$SocketInfo` / `SocketInfo` | 15 / 36 | WSAStartup/WSACleanup lifetime, hooked to `'system`'s `$onStop`. Role *`Released`*. |
| `SOCKET` | 40 | `win32'api'DWORD` subclass; `close`, `prop'$getReader/$getWriter` |
| `ListenerSOCKET` | 56 | + `open` (listen) |
| `SOCKETReader` / `SOCKETWriter` | 66 / 95 | `basic'$readAsBuffer:`, `$writeAsInt32:`, `$writeAsLiteral:`, `$writeAsBuffer:` |
| `ListenerSocketType`, `RemoteSocketType`, `ClientSocketType`, `SocketType` | 122–159 | factories |
| `CustomSocket` | 49 | base: parent, handle, events, port, socket |
| `ServerSocket` | 91 | role *`Listening`*; `$accept`, `api'$onUserEvent:` |
| `ClientSocket` | 172 | roles *`Connecting`* / *`Connected`* |
| `RemoteSocket` | 268 | an accepted connection, with a `gui'$getTag`/`$setTag:` slot |
| `Port`, `IPAddress` | 297 / 302 | property symbols |

Sockets are **asynchronous and GUI-coupled**: `winsock'1` is `WSAAsyncSelect`, and socket
readiness arrives as a Win32 window message (`WSM_NOTIFY = WM_USER + 1`) delivered through a
hidden window. A console program cannot use this package.

---

## 3. Core protocols

These are the message names that make an object "work" with the rest of the library. There is no
`interface` construct — a protocol is just a set of messages, and the namespace prefix is what
prevents collisions.

Read the `ns'$name` spellings below as **declarations**, not sends. A message written `$foo`
inside module `std'properties` compiles to the id `std'properties'$foo`; a class in `std'basic`
that wants to answer it must *declare* `#method prop'$foo` (with
`#define prop'* = std'properties'*` in scope) to land on the same id. You never write
`x prop'$foo` at a call site — the grammar does not allow it. See
[§4.4 of the language reference](10-elena-language-reference.md#44-method).

Note also that private names are namespaced by the **module truncated to two levels**, so `$foo`
in `std'basic` and `$foo` in `std'basic'memory` are the *same* message.

### 3.1 Universal

| Message | Contract |
|---|---|
| `new` / `new : x` | constructor; sent automatically when a class reference is used as an object |
| `ifNotNil` | succeeds unless the receiver is nil |
| `ifSame : x` | succeeds iff pointer-identical |
| `back : x` | returns `x`, discarding the receiver |
| `fail` | never in any VMT ⇒ always fails |
| `$invoke` / `$invoke : x` | re-send the *current* message to the receiver (used for `super`) |
| `proceed` / `proceed : x` | invoke an action |
| `=>` | declares the `proceed` method |

### 3.2 Conversion (`std'basic`)

Two layers. `$getX` is *"give me your X, or fail"*; `$toX` is *"convert yourself to X"*.

| Message | Meaning |
|---|---|
| `$getInteger` `$getLong` `$getReal` `$getLiteral` `$getString` `$getChar` `$getBool` `$getArray` `$getByteArray` `$getDump` | direct accessor; fails if the receiver is not that thing |
| `$toInteger` `$toLong` `$toReal` `$toLiteral` `$toBool` | conversion; supplied by the extensions |
| `TypeSymbol of : x` | the public form — `basic'IntegerType of:x` |
| `$typeCast : x` | the `[def]` class-level cast hook |
| `$getClassName` | `[def]`, returns the fully qualified name as a literal |

The chaining idiom `theValue $getLiteral | $toLiteral $getLiteral` (`basic.l:640`) reads "give me
your literal, or failing that convert yourself and then give it to me".

### 3.3 Comparison and boolean

| Message | Defined by |
|---|---|
| `$equal : x`, `$less : x` | **you** — the two abstract methods of `Magnitude` |
| `==` `!=` `>` `<` `>=` `<=` | `Magnitude`, in terms of the above |
| `?` `!` | succeed iff true / false |
| `if : b` | `True` / `False`; succeeds iff `b` matches the receiver |
| `and: or: xor: not` | `True` / `False` / `Boolean` |
| `$getBool` | anything boolean-ish |

### 3.4 Iteration

| Protocol | Messages | Implement to get… |
|---|---|---|
| **Enumerable** | `prop'$getEnumerator` | `prop'Enumerator::x run: item => [...]` |
| **Enumerator** | `start`, `next`, `get`, `set:`, `end`, `insert:`, `delete:` | — |
| **Indexable** | `prop'$getIndexer` | `x@n`, `prop'Indexer::x` |
| **Indexer** | `$goto:`, `$next`, `$previous`, `$getIndex`, `$getContent`, `$setContent:`, `$seek:`, `$insert:`, `$delete:` | — |
| **Iterator** (`ctrl'`) | `$getIterator`, `$getCurrent:`, `$step:` | `#group(ctrl'EIteration, …) run:` |
| **Collection** | `+=`, `Count` | |
| **Content** | `$getContent`, `$setContent:` | `prop'Content::x get` / `set:` |
| **Key/value** | `$getKey` | `prop'ContentKey::x` |

`next` **failing** is the end-of-sequence signal — the whole of iteration rests on that.

### 3.5 Streams and serialisation

| Protocol | Messages | Adapter |
|---|---|---|
| Stream | `prop'$getTextWriter`, `$getTextReader`, `$getWriter`, `$getReader` | — |
| Text writing | `basic'$writeAsLiteral:` | `prop'TextWriter` (`<<`) |
| Text reading | `basic'$readAsLiteral:` | `prop'TextReader` (`>>`) |
| Binary writing | `basic'$writeAsInt32:`, `$writeAsLiteral:`, `$writeAsBuffer:`, `$writeAsReal64:` | `prop'Writer` (`<<`) |
| Binary reading | `basic'$readAsInt32:`, `$readAsLiteral:`, `$readAsBuffer:`, `$readAsReal64:` | `prop'Reader` (`>>`) |
| Printable | `prop'$printOn : writer` | what `<<` sends |
| Readable | `prop'$input : reader`, `prop'$readFrom : reader` | what `>>` sends |
| Writable | `prop'$writeTo : writer` | |

To make your class printable you write one method:
```elena
#method prop'$printOn : aWriter
[
    self $getName $invoke:aWriter.
]
```

### 3.6 Events (`sys'events`)

| Message | Contract |
|---|---|
| `events'$getController` | return your `events'Controller` — this is all a class must do to be an event source |
| `events'Handlers::obj += { ... }` | register a handler object |
| `$onXxx` / `$onXxx : arg` | the handler messages themselves; broadcast via `#cast` |

### 3.7 Patterns (`std'patterns`)

| Message | Contract |
|---|---|
| `proceed` / `proceed : x` | an *action*: called once per element |
| `run : action` | a *pattern*: drives an action over something |
| `$getResult` | an action's accumulated answer |
| `$ifPositive` | succeeds iff the action's outcome was positive |
| `^ ctrl'Break.` | returned from an action to stop the iteration |
| `validate : x` | a validator |
| `create : profile` | a *factory* — takes a record of named parameters |
| `of : x` | a *type* — converts |

---

## 4. Dependency graph

### 4.1 Package level

```
                        ┌─────────┐
                        │ $elena  │   (implicit, in every program)
                        └────┬────┘
                             │
                        ┌────▼────┐
                   ┌────┤   std   ├────┐
                   │    └────┬────┘    │
                   │         │         │
              ┌────▼───┐ ┌───▼───┐ ┌───▼────┐
              │  sys   │ │  ext  │ │  gui   │
              └────┬───┘ └───┬───┘ └───┬────┘
                   │         │         │
                   └────┬────┴─────────┘
                        │      (all upward edges are FORWARDS, not links)
                   ┌────▼────┐
                   │  win32  │────────► gui, std, sys   (win32 depends downward too)
                   └────┬────┘
                        │
              ┌─────────▼─────────┐
              │   win32'socket    │
              └───────────────────┘
```

The clean part: `std` → `sys`/`ext`/`gui` is a proper layering. The dirty part: **`win32`
depends on `gui`**, and `gui` depends on `win32` *through forwards*. That is a genuine cycle,
broken only by the linker.

### 4.2 Module level, from the `#define` masks

| Module | Depends on |
|---|---|
| `std'basic` | `std'properties`, `std'patterns` |
| `std'properties` | `std'basic`, `std'patterns` |
| `std'patterns` | `std'basic`, `std'properties`, `std'basic'memory` |
| `std'collections` | `std'basic`, `std'properties`, `std'patterns` |
| `std'basic'memory` | `std'basic`, `std'properties` |
| `std'basic'math` | `std'basic` |
| `sys'events` | `std'patterns` |
| `sys'templates` | `std'basic`, `std'patterns`, `std'collections`, `ext'text` |
| `ext'io` | `std'basic`, `std'properties` |
| `ext'text` | `std'basic`, `std'properties`, `std'collections` |
| `gui'graphics` | `std'basic`, `std'properties`, `std'collections` |
| `gui'controls` | `std'basic`, `std'properties`, `std'patterns`, `std'collections`, `sys'events`, `gui'graphics`, `gui'controls'properties` |
| `gui'controls'properties` | `std'*`, `gui'controls`, `gui'graphics` |
| `gui'forms` | `std'basic`, `sys'events`, `gui'controls`, `gui'controls'properties` |
| `win32'api` | `std'basic`, `std'basic'memory`, `std'patterns`, `std'properties`, **`gui'graphics`** |
| `win32'api'controls` | `std'*`, **`gui'controls`, `gui'controls'properties`, `gui'graphics`**, `win32'api*` |
| `win32'api'styles` | `std'basic`, `win32'api`, `win32'api'controls`, **`win32'api'factories`** |
| `win32'api'factories` | `std'*`, **`win32'api'styles`** |
| `win32'io` | `std'basic`, `std'properties`, `std'basic'memory`, `win32'api*` |
| `win32'system` | `std'basic`, `std'properties`, `sys'events` |
| `win32'applications` | `std'basic`, `win32'system`, `win32'api`, `sys'events` |
| `win32'socket'*` | `std'*`, `sys'events`, `win32'*`, `gui'controls` |

### 4.3 The cycles the author complained about

`doc/todo.txt:130-132`, verbatim:

> ```
> - there should not be a mutual links from win32'api'factories and win32'api'styles,
>   std'basic and str'patterns and so on
> ```

Confirmed by the sources. Four genuine cycles exist:

| # | Cycle | Evidence |
|---|---|---|
| 1 | `std'basic` ⇄ `std'properties` ⇄ `std'patterns` | `basic.l:3-4` masks `prop'` and `ctrl'`; `properties.l:3` masks `ctrl'`; `patterns.l:2-3` masks `basic'` and `prop'`. `basic.l:1586` calls `prop'Enumerator` and `ctrl'Break`. |
| 2 | `win32'api'factories` ⇄ `win32'api'styles` | `factories.l:6` masks `styles'`; `styles.l:6` masks `factories'`. Both define `SDIWindowClass`, `PanelClass`, `PaintboxClass`, `NonvisibleWindowClass`. |
| 3 | `win32'api'controls` ⇄ `win32'api'styles` | `styles.l:5` masks `controls'`; `controls.l` uses `api'*` which re-enters. |
| 4 | `gui` ⇄ `win32` (via forwards) | `gui/controls.l:80` calls `'gui'handlerType`; `gui.cfg` binds it to `win32'api'factories'handlertype`; `win32/api.l:9` masks `graph'* = gui'graphics'*`. |

Cycle 1 is inherent to the design: `Magnitude ==` needs `True`/`False`, `True` is in
`std'basic`, and `std'basic` needs `std'patterns` for `Break` and `std'properties` for `@`.
Files are compiled in the order listed in `std.prj` (`properties.l` **before** `basic.l`), and
the resolution works because ELENA resolves references lazily at link time, not at compile time.

Two further coupling defects, recorded in `doc/knownbugs.txt`:

* `#00030` — *"`std'prototypes'magnitude#const` is included practically in every system though
  the object never used itself"* — a dead constant that every executable drags in, and it names
  a namespace (`std'prototypes'`) that no longer exists in 1.5.
* `#00034` — *"not possible to declare `#annex` in role"* — which forces workarounds in the
  collection classes.

`doc/todo.txt:380` also asks to *"remove obsolete classes `std'properties'indexenumerator`,
`std'properties'range`"* — both are still present and both are still used by `gui`.

---

## 5. Platform coupling

### 5.1 The layering, and where it holds

| Layer | Win32-bound? | Notes |
|---|---|---|
| `$elena` (`src/elena.l`) | **no** | uses `elena'22/23/24`, which are pure |
| `std'basic`, `std'basic'math`, `std'basic'memory`, `std'properties`, `std'patterns`, `std'collections` | **no** | 3,333 lines, entirely `#inline standard'N`. The only OS dependency is the allocator behind `ocreate`. |
| `sys'events` | **no** | pure |
| `sys'templates` | **partial** | references `'program'CommandLine` — a forward, so portable in principle |
| `ext'io`, `ext'text`, `ext'patterns` | **no** | `ext'io'TextFile` deliberately goes through the forward `'handlers'TextFileRO` |
| `ext'utilities` | **yes** | `extended'1/2/3` call `GetSystemTime` + `SystemTimeToFileTime` |
| `gui'*` (1,252 lines) | **no** — by design | every platform touch is a forward: `'gui'handlerType`, `'gui'styles'*`, `'gui'graphics'canvas`, `'gui'imageType`, `'gui'DialogLoopAction` |
| `win32'*` (2,421 lines) | **yes, totally** | |
| `win32'socket'*` (466 lines) | **yes, totally** | |

**This is much better than it looks.** 5,055 of 7,942 library lines (64%) are portable ELENA
that calls only `standard'N` primitives. The Win32 dependency is 2,887 lines, and it is
concentrated behind a forward-based seam that already works.

### 5.2 What the seam actually looks like

`src/gui/controls.l:78-81` — a control with zero platform knowledge:

```elena
#class StaticFrame (Control)
{
    #method $createHandle = 'gui'handlerType create:#group('gui'styles'StaticFrame, 'gui'styles'Parent::theParent).
}
```

`bin/templates/gui.cfg` closes it:

```ini
'gui'handlertype=win32'api'factories'handlertype
'gui'styles'staticframe=win32'api'styles'staticframe
'gui'styles'parent=win32'api'styles'parent
```

A GTK or Cocoa backend needs to supply the same 38 forwards. **No `gui` source changes.**

The seam is not perfect. `src/win32/api.l:9` masks `graph'* = gui'graphics'*` and
`src/win32/api/controls.l:9-11` masks three `gui'*` namespaces, so the platform layer *does*
depend on `gui` types (`gui'graphics'Point`, `gui'graphics'Size`). That direction is fine — a
backend is allowed to know the front-end's value types — but it means `gui` and `win32` must be
compiled together or with forward declarations.

### 5.3 What a POSIX port needs

Working outward from the assembly:

**Tier 0 — the runtime (mandatory, blocks everything).**

| `src/asm/` routine | Win32 call | POSIX equivalent |
|---|---|---|
| `elena'1` (`STD_ALLOC`) | — (bump pointer in the GC heap) | unchanged |
| `elena'3` (`STD_ENTRY`) | `GetProcessHeap`, `HeapAlloc`, `ExitProcess` | `mmap` / `malloc`, `exit` |
| `elena'35` (`STD_WIN32ENTRY`) | same + `WinMain` plumbing | n/a for console; needs a GUI equivalent |
| `elena'37` (`STD_WINDPROC`) | `GetWindowLongW`, `SetWindowLongW`, `DefWindowProcW` | a generic native-callback thunk |
| `elena'38` | Win32 message-id table | backend-specific |
| all of `standard.asm` (82 routines) | **none** | portable as-is; better, replace with LLVM intrinsics |

**Tier 1 — console + files (unblocks `std`, `sys`, `ext` and every console example).**

| Module | Win32 routines used | POSIX |
|---|---|---|
| `win32'io` | `win32'1-5` (std handles, console read/write, getchar), `win32'6-8` (line splitting — pure, portable), `win32'9-12` (`CreateFileW`, `ReadFile`, `WriteFile`, `CloseHandle`), `win32'53` (clear), `win32'54` (`WideCharToMultiByte`) | `read`/`write` on fd 0/1/2, `open`/`read`/`write`/`close`, `iconv` or a hand-rolled UTF-16↔UTF-8 |
| `win32'api` (partial) | `win32'13-14` (command line) | `argv` |
| `ext'utilities` | `extended'1/3` (`GetSystemTime`) | `clock_gettime` |

Estimated: about **12 assembly routines** and a new `posix'io` module (~300 lines of ELENA
mirroring `win32/io.l`) gets you a working console ELENA on Linux. That is a realistic first
milestone.

**Tier 2 — GUI (`win32'api*`, `win32'system`, `win32'applications`, ~2,000 lines).**
Requires reimplementing 45 of the 63 `win32.asm` routines against a toolkit, plus a new
`<toolkit>'api'styles` module supplying the 38 `'gui'styles'*` forwards, plus a native-callback
mechanism to replace `elena'37`. The `gui` package itself needs no changes.

**Tier 3 — sockets (`win32'socket'*`, 466 lines).** All 13 `winsock.asm` routines map onto BSD
sockets almost one-for-one (`WSAStartup`/`WSACleanup` become no-ops). The one architectural
problem is `winsock'1` = `WSAAsyncSelect`: readiness is delivered as a *window message*. On
POSIX that becomes `epoll`/`kqueue`, and the socket classes stop being GUI-coupled — which is an
improvement, but it changes `win32'socket'controls`.

### 5.4 The 32-bit assumption

Independent of the OS. Baked into:

* `elenaconst.h:253-254` — `elEmptyObject = 8`, `elVMTOffset = 12`
* every `#field(N)` in the library: `DWORD` is 4, `Handle` is 4, `MSG` is 28, `PAINTSTRUCT` is 64
* every `#inline standard'61(x, obj, 4)` — hand-written byte offsets
* `HiDWord` / `LoDWord`, which assume a 64-bit long is two 32-bit halves

`doc/todo.txt:340-341` — *"do not use processor specified offsets in byte code"* — and `:355`
*"start to support 64bit platform"*.

---

## 6. Modernization notes

### 6.1 What is missing, ranked

| Gap | Severity | Notes |
|---|---|---|
| **No exceptions / no error information** | critical | A failure carries nothing. You know *that* the file open failed, never *why*. `doc/todo.txt:153-155, 176-177, 239` all circle this. Every design decision below is easier once failures carry a value. |
| **No concurrency at all** | critical | No threads, no locks, no atomics, no memory model. The GC has no notion of more than one stack (`doc/todo.txt:482-483`). |
| **One collection type** | high | `List` — singly linked, O(n) `Count`, O(n) index. No hash map, no dynamic array, no set, no sorted container. Every program in `examples/` reimplements what it needs. |
| **No string builder worth the name** | high | `String +=` reallocates and copies the whole literal each time (`basic.l:888-891`). `StringWriter` helps but is not used by `+=`. |
| **No date/time** | medium | `ext'utilities'Date` is `#field(8)` with a single method. No formatting, no arithmetic, no components. |
| **No formatting** | medium | No `printf`, no number formatting options, no padding. `$toLiteral` is all there is. |
| **No file system operations** | medium | Open/read/write/close only. No directory listing, no stat, no delete, no rename, no path manipulation. |
| **No regular expressions, no text search beyond `$seek:`** | medium | |
| **No math beyond 11 functions** | low | `Tan`, `power`, `modulus`, `min`, `max` all absent. |
| **`Real` cannot convert to `Long`/`Int` directly** | low | `doc/todo.txt:159` |
| **No unit test facility** | medium | `doc/todo.txt:646-647` asks for GC test cases; there is no framework. |

### 6.2 What the library would look like after cross-platform + threading

**Repackaging.** The current `std` conflates value types, iteration, patterns and I/O adapters
in one 3,333-line compilation unit with internal cycles. A natural split:

| Proposed package | From | Adds |
|---|---|---|
| `core` | `src/elena.l` + `std'basic` value types | — |
| `core'text` | `Literal`, `String`, `Char`, indexers | encoding, formatting, a real builder |
| `core'memory` | `std'basic'memory` | **a region/pointer type** (see below) |
| `collections` | `std'collections` + `std'properties` iteration | `Array` (growable), `Dictionary`, `Set`, `Deque` |
| `patterns` | `std'patterns` | — |
| `io` | `std'properties` stream adapters + a **portable** `io` | paths, directories, stat |
| `sys` | `sys'*` | **`sys'threads`, `sys'sync`, `sys'atomic`** |
| `platform'<os>` | `win32'*` / `posix'*` | one per OS, behind the existing forward seam |
| `ui` | `gui'*` | unchanged in shape |

**Threading.** The forward mechanism already gives you a clean place to put an STA/MTA choice:
`'sys'scheduler` forwarded to either a single-threaded pump or a real scheduler. The hard part is
not the library, it is the GC — `src/asm/elena.asm:1104-1180` (`STD_ADDYGPTR`) maintains
inter-generation pointer lists in global variables with no synchronisation, and
`['gs_current_frame]` is a single global stack-frame pointer. Both must become thread-local
before any concurrency is possible.

**Concurrency primitives that would fit the language.** ELENA's failure protocol maps naturally
onto non-blocking operations:

```elena
#if aChannel receive:aMessage
    [ ... handle it ... ]
    | [ ... nothing available ... ].

#loop aQueue take:anItem [ ... ].       // exits when the queue closes
```

`try_lock`, `try_recv`, `try_pop` are all "message that may fail". A channel/actor library would
read very idiomatically. Blocking operations, by contrast, have no natural spelling.

**Exceptions without breaking the failure protocol.** The minimal change: let `fail` take an
argument and let `|` bind it.

```elena
self fail:(io'Error { $getCode = errno. $getPath = aName. }).

#if aFile open
    | anError [ 'program'output << "cannot open: " << anError. ].
```

At the machine level, `EAX = 0` becomes `EAX = 0` (bare failure) or `EAX = <error object>` with
a tag. Existing code that ignores the value keeps working; new code can diagnose. This is the
single highest-value change available and it is nearly source-compatible.

**What LLVM removes from the library's problem list.** All 82 `standard'N` primitives become
intrinsics or short generated sequences: `IntNumber +` is `add i32`, `Sqrt` is `llvm.sqrt.f64`,
`WSTR_CPY` is `llvm.memcpy`. That deletes `src/asm/standard.asm` (2,804 lines) outright and
removes the `#inline`/`#external` distinction from the library's surface. The remaining
assembly is the GC, dispatch and the entry points — which is where the real work is.

**What LLVM does not remove.** The `win32.asm` DLL stubs are not arithmetic; they are FFI. A
proper `#external` declaration syntax (name, library, calling convention, argument marshalling)
is needed regardless of backend, and it is what would let `win32'io` be rewritten as
`posix'io` in ELENA rather than in assembly. Designing that syntax should come **before** the
backend work, because it determines what the IR has to express.

### 6.3 Suggested order of work

1. **`#external` declaration syntax** — replaces 63 hand-written Win32 stubs with declarations.
   Unblocks everything else and is pure front-end work.
2. **A failure value** — `fail:` and `| binding`. Small language change, enormous library payoff.
3. **Console tier POSIX port** — 12 runtime routines + a `posix'io` module. First running
   ELENA on Linux.
4. **LLVM backend for `standard'N`** — delete `standard.asm`.
5. **Collections rewrite** — `Array`, `Dictionary`, `Set` on top of the existing iteration
   protocols, which are good and worth keeping.
6. **Thread-local GC state** — prerequisite for anything concurrent.
7. **`sys'threads` / `sys'sync` / channels** — designed around the failure protocol.
8. **GUI backend behind the existing forward seam** — last, because it is the largest and the
   least architecturally interesting.

---

*See also:* [`10-elena-language-reference.md`](10-elena-language-reference.md) for the language,
and `docs/00-project-overview.md` for the toolchain and repository layout.
