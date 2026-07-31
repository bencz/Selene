# ELENA 1.5 — Language Reference

> **Status of this document:** a complete reference for the ELENA **1.5.0 dialect** (December
> 2009), reconstructed from the sources in this tree. There was never an official reference for
> this dialect — `doc/lang/elena.txt` is 30 lines of prose. Everything below is derived from
> `dat/sg/syntax.txt` (the grammar actually fed to the parser generator),
> `elenasrc/elc/` (the compiler), `elenasrc/engine/elenaconst.h` (the ABI constants),
> `src/asm/` (the runtime) and the ~13,500 lines of real ELENA code in `src/` and `examples/`.
>
> Where the 2009 sources contradict each other, the **compiler** is treated as authoritative
> and the discrepancy is noted.

**Companion document:** [`11-standard-library.md`](11-standard-library.md).

---

## Table of contents

1. [Philosophy and execution model](#1-philosophy-and-execution-model)
2. [Lexical structure](#2-lexical-structure)
3. [Grammar](#3-grammar)
4. [Declarations — every `#` directive](#4-declarations--every--directive)
5. [Expressions and message sends](#5-expressions-and-message-sends)
6. [Control flow](#6-control-flow)
7. [The distinctive features](#7-the-distinctive-features)
8. [Modules, projects and linkage](#8-modules-projects-and-linkage)
9. [External and native interop](#9-external-and-native-interop)
10. [Idiom cookbook](#10-idiom-cookbook)
11. [Comparison, and viability for OS development](#11-comparison-and-viability-for-os-development)

---

## 1. Philosophy and execution model

### 1.1 The three axioms

ELENA 1.5 rests on three rules. Everything else in the language is a consequence.

| # | Axiom | Consequence |
|---|---|---|
| 1 | **Everything is an object.** | There are no primitive types in the language. `5` is an object; `"abc"` is an object; `true` is an object (`std'basic'True`, a singleton). Integers are objects with a 4-byte binary body (`#field(4)`). |
| 2 | **Everything is a message send.** | There are no built-in operators. `a + b` sends the message `+` with argument `b` to `a`. `#if`/`#loop` are not statements that evaluate a boolean — they are statements that *send a message and look at whether it worked*. |
| 3 | **A message send can fail.** | Failure is not an error, it is the **only** control-flow primitive. There is no `if`, no `while`, no exception, no `null` check. There is only "did that message succeed?". |

### 1.2 The failure protocol — the heart of the language

At the machine level a method returns a value in `EAX`. **A zero (`nil`) return means the message
failed.** Two distinct events produce the same failure:

* the receiver's VMT contains no entry for the message id, **or**
* the method ran and returned zero.

`src/asm/elena.asm:580-604` — the inline message-send primitive `elena'7` (`iocall`):

```asm
 labStart:
   mov  eax, [eax-4]           // ; get vmt
   test eax, eax
   jz   short labEnd           // ; go to the end if no VMT pointer  <-- "not understood"
   ...
 labCall:
   mov  eax, [esp]             // ; load self pointer
   call [esi-4]
 labEnd:
```

and `doc/tech/bytecode.txt` states the contract explicitly:

> ```
> iocall(n) msg, else      - scan in [sp] vmt for the msg mapped method starting with n position;
>                            if no mapping found goes to else label
>                            if mapping found calls the method;
>                            if the method result is zero goes to else label
> ```

A method that *succeeds* exits through `sexit` — *"the procedure result is always non zero"*. A
method that *fails* exits through `exitredir` / `ipush 0; sreturn` — *"the procedure result is
always zero"*.

The three practical corollaries:

1. **A failed message aborts the rest of the statement.** In `a foo bar baz.` if `foo` fails,
   `bar` and `baz` are never sent and the whole *statement* is abandoned.
2. **A failed statement aborts the enclosing block**, which makes the enclosing method fail,
   which propagates outward — unless caught by `|`.
3. **`|` (the alternative operator) is the only catch.** `expr | fallback` runs `fallback`
   if any message in `expr` failed.

`self fail.` is the way to fail deliberately. `fail` is a predefined message id
(`FAIL_MESSAGE_ID 0x80000000`, `elenaconst.h:112`) that is never in any VMT, so sending it always
takes the "not understood" path.

### 1.3 Objects that exist only to succeed or fail

Because failure is the control primitive, the standard library is full of objects whose entire
job is to succeed for one input and fail for another. They read as predicates:

| Object / message | Succeeds when | Fails when |
|---|---|---|
| `x ifNotNil` | `x` is not nil | `x` is nil (`elena.l:16-19`) |
| `b ?` | `b` is true | `b` is false |
| `b !` | `b` is false | `b` is true |
| `basic'True if:c` | `c` is true | `c` is false (`basic.l:524-528`) |
| `basic'False if:c` | `c` is false | `c` is true (`basic.l:578-582`) |
| `a == b` | *always* — returns `True` or `False` | — |
| `enumerator next` | there is a next element | at the end |
| `indexer seek:c` | a match was found | no more matches |

`std'basic'True` and `std'basic'False` differ by exactly one method: `True` defines `?` (empty
body ⇒ succeeds) and `False` defines `!`. Neither defines the other. That is the whole of
boolean logic in ELENA.

### 1.4 Execution model

There is **no virtual machine** in 1.5. `elc` emits ELENA bytecode into `.nl` module files, and
the JIT linker translates that bytecode into x86 machine code at link time, before the program
runs. The runtime (GC, dispatch, arithmetic, all Win32 calls) is hand-written x86 assembly in
`src/asm/`.

Object layout (from `elenasrc/engine/elenaconst.h:253-273`):

```
        ┌──────────────┬──────────────┬──────────────┬─────────
  ptr-8 │  size/flags  │  VMT pointer │  field 0     │  field 1 ...
        └──────────────┴──────────────┴──────────────┴─────────
                        ptr-4          ptr+0          ptr+4
```

* `elEmptyObject = 8` — an object header is 8 bytes (size word + VMT pointer).
* `elVMTOffset = 12` — a VMT has a 12-byte header, then an array of 8-byte
  `VMTEntry { int messageID; int address; }` **sorted ascending by *signed* message id** and
  terminated by `0x7FFFFFFF` (`TERMINAL_MESSAGE_ID`). Because the 25 predefined ids have bit 31
  set, they sort *first*. Dispatch is a **linear scan** of that array
  (`elenasrc/engine/jitcompiler.cpp:75-167`).
* The VMT header holds, at `[vmt-12]` the role-table pointer, at `[vmt-8]` the flags, and at
  `[vmt-4]` the **fallback**: the any-handler entry if `elVMTAnyHandler`, the owner class VMT if
  `elRoleVMT`, otherwise 0. A scan that overshoots follows `[vmt-4]`; when that is 0 the message
  **fails**.
* `gcBinary = 0x80000000` in the size word marks a *binary* (structure) object whose body is raw
  bytes, not object references — this is what `#field(4)` produces. `gcCollected = 0x40000000` is
  the GC mark bit.
* Allocation granularity is 16 bytes (`gcPageSize = 0x10`).
* The any-handler (`#annex`) is stored as **two identical entries with message id 0, placed after
  the terminator**. Ordinary sends use `iocall(1)`, which starts the scan at byte offset 8 —
  entry 0 is skipped, which is why every role VMT reserves it with a dummy. Only `new` uses
  `iocall(0)`.

VMT flags, all in `elenaconst.h:258-266`:

| Flag | Value | Meaning |
|---|---|---|
| `elStandartVMT` | `0x0001` | a normal class |
| `elInlineClass` | `0x0002` | anonymous inline class (`{ ... }`) |
| `elDynamicRole` | `0x0004` | variable-length binary body (`#field.` with no name) |
| `elStructureRole` | `0x0008` | binary/structure object (`#field(N)`) |
| `elRoleVMT` | `0x0010` | this VMT is a **role**, not a class |
| `elVMTWithRoles` | `0x0020` | the class owns a role table |
| `elVMTAnyHandler` | `0x0040` | the class has an `#annex` (any-message handler) |
| `elStateless` | `0x0080` | no fields and no roles ⇒ the compiler makes it a **singleton constant** |

`elStateless` matters: a class or inline object with no fields and no roles is allocated **once**
and every reference to it yields the same object (`compiler.cpp:1566-1575`). All the `#symbol`
"type" objects in the library rely on this.

---

## 2. Lexical structure

The lexer is a table-driven DFA (`elenasrc/elc/source.cpp:18-46`, states named in
`elenasrc/elc/source.h:17-38`). The table below is decoded from that DFA, not guessed.

### 2.1 Source encoding

`elc` opens source files with `feAutodetect` (`compiler.cpp:1701` → `common/files.cpp:25-33`):

* file begins with the byte pair `FF FE` (BOM `0xFEFF`) ⇒ **UTF-16LE**;
* otherwise ⇒ **ANSI**, decoded with the *current Windows codepage*.

**UTF-8 is not supported.** `doc/todo.txt:491` records the wish: *"support the following types of
unicode source files: little endian, UTF-8?"*. `examples/helloworld/u_helloworld.l` is the only
UTF-16 source in the tree (hence the `u_` prefix); `examples/c_a_g/calc_area_gui.l` and
`examples/kx.l` contain Portuguese text in a single-byte codepage and read as mojibake in any
UTF-8 tool.

Hard limits: **4096 characters per line** (`LINE_LEN`, throws `LineTooLong`), **256 characters per
identifier** (`IDENTIFIER_LEN`).

### 2.2 Case sensitivity

**ELENA 1.5 is case-insensitive.** `SourceReader::read` takes `bool lowerCase = true` and calls
`_tcslwr(token)` on every token except a string literal (`source.cpp:100-107`). So `basic'True`
and `basic'true` are the same symbol, `'program'Output` and `'program'output` are the same
forward, and `#symbol Program` is reached by the project forward `'program=words'program`. Real
code exploits this constantly and inconsistently — see `examples/words/words.l:39` vs `:58`.

String literals preserve case.

### 2.3 Token classes

| Class | Grammar terminal | Syntax | Examples |
|---|---|---|---|
| Identifier | `identifier` | `[A-Za-z_][A-Za-z0-9_]*` | `theIndex`, `Program`, `aValue` |
| Reference | `reference` | identifiers joined by `'`, optionally leading `'` | `std'basic'Integer`, `'program'output`, `win32'api'HWND` |
| Private | `private` | `$` + identifier chars | `$self`, `$getLiteral`, `$Nil` |
| Protected | `protected` | *namespace* `'$` + identifier chars | `prop'$getIndexer`, `basic'$toLiteral` |
| Wildcard | `wildcard` | *namespace* `'*` | `std'basic'*`, `basic'*` |
| Integer | `integer` | `[0-9]+`, optional leading `-` | `0`, `42`, `-1` |
| Hex | `hex` | `[0-9][0-9A-Fa-f]*h` | `20h`, `0012h`, `0FFFFFFh` |
| Real | `real` | `[0-9]+.[0-9]+r` | `0.005r`, `3.0r`, `2.0r` |
| Literal | `literal` | `"..."` | `"Hello World!!%n"` |
| Keyword | — (matched by text) | `#` + `[A-Za-z0-9$']*` | `#class`, `#if`, `#annex` |
| Operator | — (matched by text) | see below | `+`, `:=`, `::`, `=>` |
| Bracket | — (matched by text) | `( ) , . ; [ ] ^ { }` | |

Notes forced by the DFA:

* Characters above U+007F are **clamped to 127** before the table lookup (`dfa.h:30-31`), and 127
  is in the identifier class — so **any non-ASCII character is a legal identifier character**.
* **A leading `-` is part of the integer token.** `-5` is one token. Consequently `a-5` lexes as
  `a` followed by the integer `-5`, which is a syntax error: subtraction must be written
  `a - 5` with a space. (State `a` → `o`(Minus) → `p`(Integer).)
* **Hex must start with a decimal digit and end with `h`** (assembler style). `FFh` is an
  *identifier*, not a number; write `0FFh`.
* **Real numbers require the `r` suffix and a fractional part.** `2.0r` is real; `2.0` is a
  syntax error; `2r` is not a real. Introduced in 1.4.5 (`whatsnew.txt:81`).
* A `.` immediately after digits is *backed out* by an explicit hack in `dfa.h:34-39` ("HOT FIX:
  to deal with tailing dot after digit"), so `#var x := 5.` parses as `5` then statement-dot.
* `$$name` is a **lexical error** in 1.5 (state `dfaPrivate` on `$` → error). 1.4.1 introduced
  `$$` for protected messages and 1.4.5 removed it (`whatsnew.txt:77-80`); protected is now
  written `namespace'$name`. The only `$$` in the tree,
  `src/win32/socket/primitives.l:75-85`, is inside a `/* */` comment.

### 2.4 String literals and escapes

Decoded by `elenasrc/common/altstrings.h:341-390`:

| Escape | Result |
|---|---|
| `%n` | LF (0x0A) |
| `%r` | CR (0x0D) |
| `%t` | tab |
| `%a` | bell |
| `%b` | backspace |
| `%%` | a literal `%` |
| `%<decimal digits>` | the character with that decimal code |
| `""` | a literal `"` — this is the only way to embed a quote |

There is no character literal syntax. A single character is a one-element literal converted with
`basic'CharType of: ...`, or obtained from a `LiteralIndexer`.

### 2.5 Comments

```
// line comment, to end of line
/* block comment, spans lines, does not nest */
```

An unterminated `/*` at end of file is reported as an invalid character, not as an unterminated
comment.

### 2.6 Operators

Single-character operator start characters: `! & * + - / : < = > ? @ |`.
Two-character operators (`dfaDblOperator`, formed when an operator is followed by `: < = >`):

```
:=   ==   !=   <=   >=   <<   >>   ::   =>   +=   -=   *=   /=
```

`|` alone is the **alternative** operator. (In `dat/sg/syntax.txt` it is written `||` because
`|` is the grammar file's own rule separator; `elenasrc/sg/sg.cpp:41-42` strips the first
character: `if (compstr(symbol, _T("||"))) symbol++;`. The same escape applies to `-->` for the
`->` terminal, which is obsolete in 1.5.)

`.` terminates every statement, field declaration, symbol declaration and `#define`. Method
bodies written with `[ ... ]` are **not** followed by a dot; method bodies written `= expr.`
**are**.

---

## 3. Grammar

Below is `dat/sg/syntax.txt` rewritten as readable EBNF. It is an LL grammar; `sg.exe` turns it
into the parser table `dat/sg/syntax.dat`. Names in `SMALL CAPS` are the original non-terminals so
you can cross-reference.

### 3.1 Module

```ebnf
module      = { directive } , { declaration } , EOF ;

directive   = "#define" , [ hints ] , shortcut ;
shortcut    = identifier , "=" , reference , "."      (* named shortcut  *)
            | wildcard  , "=" , wildcard  , "." ;     (* namespace mask  *)

declaration = "#class"  , [ hints ] , class
            | "#symbol" , [ hints ] , symbol
            | "#static" , [ hints ] , static ;

hints       = "[" , hint , { "," , hint } , "]" ;
hint        = identifier , [ ":" , ( identifier | reference ) ] ;
```

A module is: an optional block of `#define`s, then a sequence of `#class` / `#symbol` /
`#static` declarations. **Every `#define` must be at the very top of the file** — the grammar
allows directives only at `START`, and `compileDirectives` (`compiler.cpp:1623-1652`) stops at
the first non-`nsShortcut` node. A `#define` after the first declaration is a syntax error.

### 3.2 Classes

```ebnf
class       = name , [ "(" , baseclass , ")" ] , "{" ,
                  { field } , { role } , { method } , [ annex ] ,
              "}" ;

name        = identifier | private ;
baseclass   = identifier | private | reference ;

field       = "#field" , ( identifier | private | "(" , size , ")" | (* empty *) ) , "." ;
size        = integer | hex ;

role        = "#role" , name , "{" , { method } , "}" ;

annex       = "#annex" , object , "." ;
```

Three kinds of `#field`:

| Form | Meaning | Sets flag |
|---|---|---|
| `#field theName.` | a normal reference-holding field | — |
| `#field(N).` | the object has an **N-byte binary body** and no reference fields | `elStructureRole` |
| `#field.` | the object has a **variable-length binary body** | `elDynamicRole` \| `elStructureRole` |

They are mutually exclusive: `compileFieldDeclarations` (`compiler.cpp:1509-1547`) raises
`errIllegalField` (error 111) if you mix them or declare any field after a sized one. A binary
class's contents can then only be manipulated through `#inline standard'N` primitives — there is
no field-name-to-offset mapping, so offsets are written by hand
(`#inline standard'61(aValue, $self, 4)`).

### 3.3 Methods

```ebnf
method      = "#method" , [ hints ] , methodhead , body ;

methodhead  = name                                  (* unary   *)
            | name , ":" , parameter                (* keyword *)
            | operator , parameter                  (* binary operator *)
            | ( "=>" | "?" | "!" ) ;                (* special *)

name        = identifier | private | protected ;
parameter   = identifier | private ;

operator    = "==" | "!=" | "<" | ">" | "<=" | ">=" | "+" | "-" | "*" | "/"
            | "+=" | "-=" | "*=" | "/=" | "<<" | ">>" | "@" ;

body        = "[" , statements                      (* code block, closed by "]" *)
            | "=" , expression , "." ;              (* returning expression      *)
```

`=>` as a method head declares the **`proceed` method**: `=>` and `proceed` are mapped to the
*same* id, `PROCEED_MESSAGE_ID = 0x80000003` (`compiler.cpp:494-495`). (`OPROCEED_MESSAGE_ID
0x8000000E` is declared in `elenaconst.h` but never used.) `?` and `!` are ordinary postfix unary
methods with the predefined ids `IF_MESSAGE_ID` / `IFNOT_MESSAGE_ID`; they receive `nil` as their
argument.

### 3.4 Symbols

```ebnf
symbol      = name , [ ":" , parameter ] , symbolbody ;
static      = name , symbolbody ;

symbolbody  = "=" , expression , "."
            | "=>" , actionbody , "." ;

actionbody  = "[" , statements
            | "(" , expression , ")" ;
```

### 3.5 Statements

```ebnf
statements  = statement , { "." , statement } , "]" ;

statement   = expression
            | "#var" , name , [ ":=" , expression ]
            | "#if" , caseexpr
            | "#loop" , loopexpr
            | "#shift" , [ rolename ]
            | "^" , expression , "." ;              (* return *)

caseexpr    = object , operation , caseoptions ;
caseoptions = block , [ elseoption ]
            | "|" , block ;
elseoption  = "|" , ( operation , block , [ elseoption ] | block ) ;

loopexpr    = object , operation , [ block ] ;

block       = "[" , statements ;
```

### 3.6 Expressions

```ebnf
expression  = object , operations
            | "#external" , reference , "(" , arglist , ")"
            | "#inline"   , reference , "(" , arglist , ")" ;

operations  = ":=" , expression                     (* assignment *)
            | operation , { "|" , operation }       (* message chain + alternatives *)
            | (* empty *) ;

operation   = L0op { L1op } { L2op } { L3op } { L4op }
            | L1op { L1op } { L2op } { L3op } { L4op }
            | L2op { L2op } { L3op } { L4op }
            | L3op { L3op } { L4op }
            | L4op { L4op } ;

L0op        = "::" , L0expr ;                       (* property / construction *)
L1op        = name , [ ":" , L0expr ]               (* ordinary message send   *)
            | "@" , L0expr ;                        (* index                   *)
L2op        = ( "*" | "/" | "*=" | "/=" ) , L1expr ;
L3op        = ( "+" | "-" | "+=" | "-=" ) , L2expr ;
L4op        = ( "==" | "!=" | ">" | "<" | ">=" | "<=" | "<<" | ">>" ) , L3expr
            | "?" | "!" ;

object      = identifier , [ action | inlineobject ]
            | reference  , [ inlineobject ]
            | private    , [ action | inlineobject ]
            | literal | integer | hex | real
            | inlineobject                          (* "{" method... "}"      *)
            | "(" , expression , { "," , expression } , ")"
            | "#group" , "(" , exprlist , ")"
            | "#cast"  , "(" , exprlist , ")"
            | "#type"  , ( reference | identifier )
            | "#annex" , "(" , object , ")" , "{" , { method } , "}"
            | action ;                              (* "=>" ...               *)

inlineobject = "{" , method , { method } , "}" ;
action       = "=>" , actionbody ;
```

### 3.7 Operator precedence

Five levels, introduced in 1.4.6 (`whatsnew.txt:37-38`). Lowest binds last:

| Level | Operators | Example |
|---|---|---|
| **L0** (tightest) | `::` | `basic'Integer::0` |
| L1 | *message* `name`, `name : arg`, `@` | `a foo:b`, `arr@3` |
| L2 | `*` `/` `*=` `/=` | |
| L3 | `+` `-` `+=` `-=` | |
| **L4** (loosest) | `==` `!=` `>` `<` `>=` `<=` `<<` `>>` `?` `!` | |

So `i + j * k` needs no brackets, and `x foo + y bar` means `(x foo) + (y bar)` — **a message
binds tighter than arithmetic**. This surprises everyone once.

`:=` (assignment) is not in the precedence ladder at all; it is a separate `OPERATIONS`
alternative, so `a := b + c` is fine but `a := b := c` is not.

---

## 4. Declarations — every `#` directive

The complete list. There are **seventeen**; the grammar admits no others, and every one is used
somewhere in `src/` or `examples/`.

| Directive | Position | Purpose |
|---|---|---|
| `#define` | module head | reference shortcut or namespace mask |
| `#class` | module | class declaration |
| `#symbol` | module | symbol (re-evaluated each reference) |
| `#static` | module | symbol evaluated once, cached |
| `#field` | class body | instance field, or binary body size |
| `#role` | class body | a role (alternative VMT) for `#shift` |
| `#method` | class / role body | method |
| `#annex` | class body **or** expression | any-message handler / object extension |
| `#var` | statement | local variable |
| `#if` | statement | conditional (failure-driven) |
| `#loop` | statement | loop (failure-driven) |
| `#shift` | statement | enter/leave a role |
| `#group` | expression | first-succeeding-member dispatch object |
| `#cast` | expression | broadcast-to-all-members dispatch object |
| `#type` | expression | reified class reference (for allocators) |
| `#external` | expression | call a runtime `procedure` |
| `#inline` | expression | splice a runtime `inline` block |

Directives that appear in `whatsnew.txt` but **do not exist in 1.5**: `#hint` (removed 1.4.3,
replaced by the `[...]` postfix), `#case` (removed 1.4.3, replaced by `#if`), `#switch`
(renamed `#cast` in 1.4.2), `#redirect` (1.2.5, folded into `#annex`), `#pattern` (obsolete
since 0.9.9).

**Hints.** `#class`, `#symbol`, `#static`, `#method` and `#define` all accept a bracketed hint
list (`[a, b:c]`) syntactically, but the compiler recognises exactly **three** hint names in the
whole of 1.5. Anything else is `warning 404: Unknown hint`.

| Hint | Legal on | Meaning |
|---|---|---|
| `[const]` | `#define` only | the target is a link-time constant singleton |
| `[dbg:<kind>]` | `#class` only | debugger display format for a binary body |
| `[def]` | `#method` only | route the message into the `$elena` system namespace |

`#symbol` and `#static` accept **no** hints — `SymbolScope::compileHints`
(`compiler.cpp:245-252`) warns unconditionally. Names such as `sealed`, `extension`, `limited`
or `stacksafe` belong to ELENA 2.x/3.x and do nothing here.

### 4.1 `#define` — shortcuts and namespace masks

```elena
#define basic'* = std'basic'*.          // namespace mask
#define[const] ListExtension = list'ListExtension.   // named constant shortcut
```

Two forms, both handled in `compileDirectives`, `compiler.cpp:1623-1652`:

* **Mask** — `wildcard = wildcard .` calls `defineMask`. Thereafter any reference whose
  namespace matches the left mask is rewritten with the right one. This is ELENA's `import`.
  Every non-trivial source file in the tree opens with a block of these. Masks are **not
  recursive** and compare only the last namespace boundary, so `basic'*` matches `basic'Literal`
  but **not** `basic'sub'X` — that needs its own mask. Masks also drive protected-message
  resolution ([§4.4](#44-method)).
* **Named shortcut** — `identifier = reference .` calls `defineForward`. `MyName` now resolves
  to that reference in this module only.

The only hint accepted here is `[const]` (`HINT_CONSTANT`, `compileForwardHints`,
`compiler.cpp:211-221`). It marks the target as a **compile-time constant**, which is required
when the symbol is used inside `#annex` or `#group` — those need the value at compile time, not
a runtime symbol call. `src/gui/controls.l:11-17` is the canonical block:

```elena
// --- compiler hints ---
#define[const] IntegerExtension = basic'IntegerExtension.
#define[const] LiteralExtension = basic'LiteralExtension.
#define[const] StringExtension  = basic'StringExtension.
#define[const] ArrayExtension   = basic'ArrayExtension.
#define[const] ListExtension    = list'ListExtension.
#define[const] IndexerExtension = prop'IndexerExtension.
```

### 4.2 `#class`

```elena
#class[dbg:int] IntNumber (Magnitude)
{
    #field(4).

    #method[def] $getClassName = "std'basic'IntNumber".
    #method + aValue [ ^ #inline standard'8($self clone, aValue $getInteger). ]

    #annex IntegerGroup.
}
```
— `src/std/basic.l:102-196`

Ordering inside the body is fixed by the grammar: **fields, then roles, then methods, then at
most one `#annex`**. Base class in parentheses; if omitted, the class inherits
`$elena'object` (`SUPER_CLASS`, `elenaconst.h:30`), i.e. `src/elena.l:3`.

A class name beginning with `$` (`#class $GUI`, `src/win32/system.l:8`) is a *private* class —
not exported outside the module unless re-published via `#static`.

**Class hints:**

| Hint | Effect |
|---|---|
| `[dbg:int]` | debugger shows the binary body as a 32-bit int (`elDebugDWORD`) |
| `[dbg:long]` | …as a 64-bit int (`elDebugQWORD`) |
| `[dbg:real]` | …as a 64-bit float (`elDebugReal64`) |
| `[dbg:literal]` | …as a UTF-16 string (`elDebugLiteral`) |
| `[dbg:array]` | …as an object array (`elDebugArray`) |

That is the complete set (`ClassScope::compileHints` / `setDebugWatchHints`, `compiler.cpp:310-347`; names in
`elenaconst.h:159-166`). Any other
hint on a class produces warning 404.

### 4.3 `#symbol` and `#static`

```elena
#symbol Pi        = #inline standard'74(basic'RealNumber).      // computed on each use
#symbol HiWord : anInteger
    = #inline standard'24(basic'IntNumber, anInteger $getInteger).  // parameterised
#static  GUI      = $GUI.                                        // computed once
```

`#symbol` compiles to a small procedure that is **re-executed at every reference**
(`compileSymbolDeclaration`, `compiler.cpp:1589-1620`); a reference emits `rpush nil; rcall
<symbol>` — the `nil` being the default parameter.

`#static` wraps the same body with `newStaticSymbol` / `endStaticSymbol`
(`bccompiler.cpp:189-202, 593-608`), which prepends a memoisation check:

```
rpushptr <static slot>     ; load the cached value
rreturnif nil              ; if it is NOT nil, return it now
pop / prep / <body> / rmoveptr <static slot>
```

So a `#static` is a **lazily-initialised, memoised singleton** backed by a `.bss` slot that is a
GC root. `#static` takes no parameter (grammar `STATIC -> identifier SYMBOL_BODY`).

The difference matters a lot in the library: `#symbol Now = DateType $getCurrent.` reads the
clock every time; `#static RandomGenerator = $RandomGenerator.` seeds once.

**Parameterised symbols** (`#symbol Name : param = expr.`) are applied with `::` —
`HiWord::x`. They are ELENA's substitute for free functions, and (with `#annex`) for generic
type constructors. Only **one** parameter is allowed (`whatsnew.txt:159`).

`#static` takes no parameter (grammar `STATIC -> identifier SYMBOL_BODY`).

### 4.4 `#method`

Four head shapes and two body shapes:

```elena
#method Length = theLiteral Length.          // unary,   expression body
#method new : aValue [ ... ]                 // keyword, code body
#method + aValue [ ... ]                     // operator
#method ? []                                 // predicate; empty body ⇒ always succeeds
#method => ...                               // default / "proceed" method
```

* `= expr.` compiles as `^ expr.` — a returning expression. Note the terminating dot.
* `[ ... ]` is a statement block; its value is "succeeded" unless an explicit `^` returns
  something or a message inside fails.
* **`#method foo []`** — an empty body — is an extremely common idiom: it declares that the
  object *understands* `foo` and always succeeds. That is how `True` says it is true
  (`basic.l:522`) and how marker protocols work.

**Method hints:** only `[def]` (`HINT_DEFAULT`, `compiler.cpp:380-390`). It routes the message
into the `$elena` standard module namespace (`mapDefaultMessage`), making it a *system* message
that the compiler/debugger knows about. Used for `$getClassName`, `$typeCast`:

```elena
#method[def] $getClassName = "std'basic'Literal".
#method[def] $typeCast : anObject = anObject $getLiteral.
```

**Message name spaces.** A method name may be:

| Form | Kind | Message id derived from | Where it may appear |
|---|---|---|---|
| `foo` | public | the bare name, lower-cased — global across all modules | declaration **and** send |
| `$foo` | private | **`<module name, truncated to 2 namespace levels>'$foo`** | declaration **and** send |
| `ns'$foo` | protected | `ns` expanded through the module's `#define` masks, truncated to 2 levels, + `'$foo` | **declaration only** |

Three things follow, and together they are the whole encapsulation story.

**(a) `$foo` is namespaced by the *module*, not the class.** `mapMessage` builds
`LocalPrivateMessage(module->Name(), "$foo")` and truncates the module name to at most two
namespace levels (`compiler.cpp:540-557`). So `$getLiteral` written anywhere in `std'basic` *or*
in `std'basic'memory` is the same message, `std'basic'$getliteral`. That is the feature
`whatsnew.txt:86-87` describes as *"message private namespace could be shared between the modules
belonging to the same subbranch"*.

**(b) A protected name can only be *declared*, never *sent*.** The grammar's message-send rule is
`L1_OPERATION -> identifier ... | private ...` — `protected` is not an alternative
(`syntax.txt:386-389`), while `METHOD -> ... | protected METHOD_BODY` is (`syntax.txt:157`).
You never write `x prop'$getIndexer`.

**(c) Therefore protected means "I implement another module's private message".** `std'properties`
writes `anArray $getIndexer`, which compiles to `std'properties'$getindexer`. `std'basic` cannot
spell that with a bare `$`, so it declares

```elena
#method prop'$getIndexer = basic'LiteralIndexer::self.
```

and `mapProtectedMessage` (`compiler.cpp:520-531`) expands `prop'` through
`#define prop'* = std'properties'*` to produce exactly the same id. `src/std/basic.l:406` and
`src/gui/controls/properties.l:131` collide on purpose. **This is ELENA's interface mechanism**,
and it is a naming convention enforced by the compiler, not an access check.

### 4.5 `#var`

```elena
#var aCopy := basic'LiteralType $create:(self Length).
#var aMaxChild := nil.
#var aRetVal := IntNumber.          // declares AND constructs
```

A local. **The `:=` initialiser is mandatory** — the grammar is `VARIABLE -> identifier
ASSIGNING` and `ASSIGNING -> ":=" EXPRESSION` has no empty alternative. A duplicate name is
`error 105`. Scope is the enclosing block.

`:=` is compiler-level assignment to a local or field only (`compileAssignment`,
`compiler.cpp:1173-1190`), and every field assignment additionally emits the GC write barrier
`$package'elena'34`. `<<` is an ordinary message (`COPY_MESSAGE_ID`) — the two are not
interchangeable.

**`#var x := SomeClass.`** does not merely name a class — a class reference used as an object
*allocates an instance and sends `new` with `nil`* (`compileSymbolCode`,
`compiler.cpp:1349-1383`). This is the single most confusing thing about reading ELENA: **class names
are constructors.**

### 4.6 `#if`, `#loop`, `#shift`

See [§6](#6-control-flow) and [§7.1](#71-roles-and-shift-technology).

### 4.7 `#annex`, `#group`, `#cast`, `#type`

See [§7](#7-the-distinctive-features).

### 4.8 `#external`, `#inline`

See [§9](#9-external-and-native-interop).

---

## 5. Expressions and message sends

### 5.1 Anatomy of a send

```
    receiver   verb   :   argument
       a       foo    :      b
```

There is exactly **zero or one argument**. Multiple arguments are expressed by chaining
(each send returns something the next is sent to) or by passing a single inline object that
carries several named getters:

```elena
theStatusbar create:(104,284,464,690).                   // collection argument
basic'SubString::{ for = aLiteral. from = anIndex. till = anIndex + aLen. }.  // record argument
aWriter $writeAsInt32:(aLiteral Length) $writeAsLiteral:aLiteral.             // chaining
```

The message id is derived from the verb text plus whether there is an argument
(`Compiler::mapMessage`, `compiler.cpp:540-557`). **25 message names are predefined**
(`elenaconst.h:72-97`, ids at `:112-140`) and get fixed ids in the `0x8000000n` range so the
compiler can special-case them:

```
fail  new  proceed  <<  >>  =>  ifnotnil  of  ?  !
+  -  *  /  >=  <=  >  <  ==  !=  +=  -=  back  run  ifsame
```

Note what is *not* on that list: `*=`, `/=`, `@` and `::` are ordinary messages with hashed ids.
There is also `#any` (`ANY_MESSAGE`), the reserved id used for `#annex` handlers.

### 5.2 `::` — the property / construction operator

`::` is the tightest-binding operator and reads *right to left*: `A::B` evaluates `B`, then
applies `A` to it. What that means depends on `A`:

| `A` is… | `A::B` means |
|---|---|
| a **class** | allocate an instance of `A` and send it `new : B` |
| a **parameterised symbol** (`#symbol A : p = ...`) | evaluate `A` with `p` bound to `B` |
| an **inline object** | same as a class |

```elena
basic'IntNumber::self                 // construct an IntNumber from self
prop'Enumerator::aCollection          // construct an Enumerator over aCollection
basic'HiWord::WParam                  // apply the HiWord symbol to WParam
list'ReadOnly::aCollection            // wrap aCollection in a read-only façade
```

Mechanically: the compiler evaluates the right operand first and leaves it on the stack, then
calls the left symbol *without* pushing `nil` (`pushProperty`, `bccompiler.cpp:392-396`). The
parameter is read as `ifpush -1` — the top of the caller's frame. Consequently **the left operand
of `::` must be a real (non-constant) symbol**; anything else is `error 121`
(`compiler.cpp:810-812`).

Before 1.4.6 this was written `->` and messages needed explicit arrows; `whatsnew.txt:39-42`
records the change. Nothing in 1.5 uses `->`.

`basic'Integer::0` and `basic'Integer << 0` are both used in real code and are close to
equivalent — the first constructs-with, the second constructs-then-assigns.

### 5.3 `@` — indexing

`@` is an ordinary L1 operator method, declared as `#method @ anIndex`. The standard
implementation is one line (`src/std/properties.l:603-609`):

```elena
#symbol IndexerExtension =
{
    @ anIndex
    [
        ^ self $getIndexer $goto:anIndex.
    ]
}.
```

`arr@3` therefore returns an **indexer positioned at 3**, not the element. To get the element you
send `prop'ContentValue::(arr@3)` or rely on the indexer's `#annex` to forward the message to the
element (which is exactly what `ArrayIndexer` does — `basic.l:1217`, `#annex theItem.`). Chained
indexing works: `(theTable $getHands)@ prop'Index::aPlayer @ aCard << basic'True.`

### 5.4 `<<` and `>>`

By convention throughout the library:

* `<<` — write / assign into. `aWriter << value`, `anInteger << 5`, `prop'Caption::ctl << "text"`.
* `>>` — read into. `'program'input >> aVariable`. **Fails** on a parse error, which is why
  input handling is `#if input >> x | [ error ]`.

Both are ordinary operator methods with predefined ids `COPY_MESSAGE_ID` / `COPYTO_MESSAGE_ID`.

### 5.5 `self`, `$self`, `super`

Only three built-in variables exist in 1.5 (`elenaconst.h:143-145`):

| Variable | Meaning |
|---|---|
| `self` | *the current object group* — the outermost receiver, i.e. what the sender sees, including any currently active role and any annex wrapper |
| `$self` | *the current class instance* — the raw object, bypassing roles/extensions |
| `super` | the parent class VMT |

`$this`, `this`, `$vself`, `$param`, `$super` all existed in earlier versions and are **all
removed by 1.5** (`whatsnew.txt:52, 82, 131, 165, 239`).

The `self` / `$self` distinction is what makes roles work. Inside a role method, `self fail.`
fails the whole object; `$self $reset.` reaches past the role to the base class. `examples/upndown/players/net.l:288` and `:318` use both in the same class.

**`super` may only receive the *first* message of a chain.** `compileOperations` resets the
receiver's kind to `otExpression` after the first send (`compiler.cpp:720-721`), so
`super foo bar` sends `foo` to the parent and `bar` to the *result*. A `super` send compiles to
`ircall` (`elena'30`), which starts the VMT scan in the named parent VMT rather than in the
object's own.

`super $invoke` / `super $invoke:x` re-sends **the current message** to the parent — the ELENA
equivalent of `super.sameMethod(...)`. `$invoke` is not a real message: `compileMessage`
(`compiler.cpp:690-694`) substitutes the id of *the method currently being compiled*, and raises
`error 127` if used outside a method. `doc/todo.txt:32` asks whether it should be replaced with
something less ambiguous.

### 5.6 Inline objects

```elena
{ X = anX. Y = anY. }                       // a record: two unary methods
{ prop'$printOn : aWriter [ ... ] }         // a one-method protocol implementation
gui'prop'Item { $getCaption = ... . }       // with an explicit base class
```

An inline object is an anonymous class declared at the point of use. **There is no `#method`
keyword inside the braces** — the method heads stand alone (`SYMBOL_EXPRESSION -> { METHOD
INLINE_METHODS }`, `syntax.txt:240-241`).

Its "fields" are the enclosing scope's variables, captured by copy into hidden fields
(`compileSymbolExpression`, `compiler.cpp:832-901` — the `scope.outers` loop). Inside an inline
object, an identifier is *first* resolved as a capture from the enclosing scope; only if that
fails does `self` fall back to meaning the inline object
(`InlineClassScope::mapObject`, `compiler.cpp:444-479`). If it captures nothing and has no roles,
the compiler marks it `elStateless` and it becomes a **shared singleton constant**.

`Parent { ... }` both sets the base class *and* sends `new` to the result.

`Identifier{ ... }` gives the inline object a base class. `=> [...]` / `=>( expr )` is the
degenerate case: an object with exactly one method, `proceed`.

---

## 6. Control flow

### 6.1 The model in eight lines

1. Every message send yields **success + value** or **failure** (`EAX = 0`).
2. A send inside a `|` chain, an `#if` condition or a `#loop` condition jumps to **that
   construct's** failure label.
3. A send **anywhere else — including inside an `#if` body — jumps straight to the enclosing
   *method's* failure exit**, which returns 0 and so fails the caller's send in turn.
4. `|` is the only catch: `A | B` runs `B` iff anything in `A` failed.
5. `#if X op [then] | [else]` = "send `op` to `X`; run `[then]` on success, `[else]` on failure".
6. `#loop X op [body]` = "while sending `op` to `X` succeeds, run `[body]`".
7. `self fail.` fails deliberately; `back:v` supplies a value for the successful path.
8. There are no exceptions. A failure that reaches the program entry point ends the program
   ("Program broken", `whatsnew.txt:492-493`).

Point 3 is the one people get wrong. `#if` does **not** wrap its body in a try/catch: only the
*condition* is compiled with `CTRL_BRANCHING`; the body is compiled with plain `CTRL_ROOT`
(`compiler.cpp:1214`). A message that fails on the third line of an `#if` body does not take the
`|` branch — it abandons the whole method.

### 6.2 `#if`

```ebnf
"#if" object operation ( block [ "|" [ operation ] block ... ] | "|" block )
```

The subject is evaluated **once**; each `|` clause may supply its own operation to re-test it.

```elena
// simple two-way
#if (aLiteral Length != 0)?
    [ aText += aValue. ]
    | [ aFlag << basic'False. ].

// no condition operator at all — ifNotNil fails on nil
#if aValue ifNotNil
[
    #inline standard'3 ($self, aValue $getInteger).
].

// failure-only: no "then" branch, just a fallback
#if 'program'input >> aNumber
    | [ 'program'output << "Input error. Aborting%n". $self fail. ].

// three-way with explicit both-polarities
#if(aValue $getBool)
    ? [ self $setTrue. ]
    | ! [ self $setFalse. ]
    | [ self fail. ].
```

The last form is the library's standard "true / false / neither" dispatch (`src/std/basic.l:1001-1005`).

**Multi-way dispatch** ("switch") is a chain of `| op ?`:

```elena
#if (basic'LiteralType of:aChar)
    == ">"   ? [ theTapeIterator $next. ]
    | == "<" ? [ theTapeIterator $previous. ]
    | == "+" ? [ theTapeIterator += 1. ]
    | == "-" ? [ theTapeIterator -= 1. ]
    | == "." ? [ 'program'Output << theTapeIterator. ]
    | == "," ? [ 'program'Input get. ]
    | == "[" ? [ #shift Looping. self $resetLoop. ].
```
— `examples/interpreter/interpreter.l:79-86`. Multiple cases in `#if` arrived in 1.4.6
(`whatsnew.txt:43`).

### 6.3 `#loop`

```ebnf
"#loop" object operation [ block ]
```

Loops while the head expression succeeds. The body is **optional**:

```elena
#loop(i < MAX)? [ ... ].                        // classic counted loop
#loop self next [ ... ].                        // enumerator: next fails at the end
#loop 'program'Input >> aText $ifNotStopped.    // no body at all
#loop anIndexer seek:aCriteria delete:{ Count = n. } insert:aReplaceText.  // search & replace all
```

The last one (`examples/replace/replace.l:65-69`) is the whole of a replace-all: `seek:` fails
when there are no more matches.

`doc/todo.txt:23-25` records the author's own doubt about this design: *"loop implementation,
could it be improved (if in the statement `#loop i > 5 ? [ ... ]`, message `>` fails, it is
considered as end of loop, is it ok?)"* — i.e. a genuine error inside the condition is
indistinguishable from normal termination.

### 6.4 `|` outside `#if`

`|` is an expression-level operator, not an `#if` keyword. It works in any expression:

```elena
^ self $equal:aValue back:basic'True | back:basic'False.       // ternary
theValue $getLiteral | $toLiteral $getLiteral.                 // try, else convert
#var anOwner := aProfile $getOwner | back:aHandle.             // default value
theKey := aKeyValue $getKey | back:'nil.
theProxy $seek:(aValue Value) | $goto:(aValue Position).       // try seek, else goto
```

`back : x` is a method on `Object` (`src/elena.l:9`): `#method back : aParam = aParam.` — it
throws away the receiver and returns its argument. `expr back:T | back:F` is therefore exactly
`expr ? T : F`.

### 6.5 `^` — return

`^ expr.` returns from the enclosing **method or action**, not the block. Because a symbol body
is an expression, returning from inside a symbol requires wrapping the code in an action:

```elena
#symbol Function : t = ctrl'Control run: =>
[
    ...
    ^ { X = anX. Y = anY. }.
].
```
— `examples/graphs/graphs.l:19-27`. `ctrl'Control run: anAction = anAction proceed.`
(`src/std/patterns.l:274-279`) exists purely to give an action something to be invoked by.

---

### 6.6 What it compiles to

Method prologue/epilogue (`ByteCodeCompiler::endMethod`, `bccompiler.cpp:555-568`):

```
  sprep                   ; open frame, set self
  ...body...
  sexit                   ; normal exit: result is ALWAYS non-zero
proc-failure:
  ipush 0
  sreturn                 ; returns 0 -> the caller's send sees failure
```

`sexit` is `elena'8` and its contract is literally *"the procedure result is always non zero"*;
the failure path pushes a zero and returns it. That is the whole protocol.

An alternative chain (`newBranch` / `newAlternativeBranch` / `endBranch`,
`bccompiler.cpp:282-297, 511-526`):

```
   iopush 0                    ; save the receiver and the stack level
   <first operation, CTRL_BRANCHING>
   iomove 1 / ifset branch-level / jump branch-end
branch-failing:
   ifset branch-level          ; restore sp to the branch entry level
   iopush 0                    ; re-push the ORIGINAL receiver
   <second operation>
   ...
branch-failing:
   jump proc-failing           ; (or the enclosing branch's failing label, if nested)
branch-end:
```

`#loop` (`compileLoop`, `compiler.cpp:1130-1140`):

```
loop-start:                    ; a nop that serves as the back-jump target
   <condition, CTRL_BRANCHING>
   <body, CTRL_ROOT>
   ifset loop-level
   ijump loop-start
branch-failing:
   ifset loop-level
```

`doc/tech/bytecode.txt:160-189` works a full example through by hand.

---

## 7. The distinctive features

### 7.1 Roles and "shift technology"

A **role** is a second VMT attached to a class. `#shift RoleName.` swaps the object's VMT
pointer to the role's; bare `#shift.` swaps it back. The object's identity, address and fields
are unchanged — **only its behaviour changes**.

The runtime is a handful of instructions (`src/asm/elena.asm:993-1015`):

```asm
// shift
inline elena'28
  mov edx, [edi-4]
  test [edx-8], elRoleVMT  // ; skip if it is not a role
  jz   short labShift
  mov edx, [edx-4]         // ; get a role owner vmt
labShift:
  mov edx, [edx-0Ch]
  mov ecx, [edx+__arg1]
  mov [edi-4], ecx         // ; overwrite the object's VMT pointer
end

// unshift
inline elena'29
  mov edx, [edi-4]
  mov edx, [edx-4]
  mov [edi-4], edx
end
```

A role VMT has `elRoleVMT` set and its "parent" slot points back at the owning class, so
`#shift.` restores the class. Roles are declared before methods, and a subclass may *override* a
parent's role of the same name (`compileRoleDeclarations`, `compiler.cpp:1445-1507`).

**Canonical example** — a string that is empty behaves differently from one that is not
(`src/std/basic.l:804-899`):

```elena
#class String (Magnitude)
{
    #field theLiteral.

    #role Empty
    {
        #method $getLiteral = "".
        #method Length = 0.
        #method + aValue [ ^ aValue $getLiteral. ]

        #method << aValue
        [
            #shift.               // leave the Empty role...
            self << aValue.       // ...and re-send to the real implementation
        ]
    }

    #method new : aValue
    [
        #if aValue ifNotNil
        [ theLiteral := aValue $getLiteral. ]
        | [ #shift Empty. ].
    ]

    #method clear [ #shift Empty. ]
}
```

Three idioms recur:

| Idiom | Shape | Example |
|---|---|---|
| **State machine** | `#shift OtherState.` inside a method | `WordEnumerator` `Space`/`Token` (`src/ext/text.l:38-58`) |
| **Sentinel** | a role whose methods `self fail.` | `Indexer` `EOF`, `Enumerator` `BOF` (`src/std/properties.l:15-87`) |
| **Bootstrap** | a role that shifts out then re-sends | `String Empty <<` above; `Enumerator BOF next` (`src/std/properties.l:216-228`) |

Roles are how ELENA does what other languages do with `null` checks, state enums, and the State
pattern — with zero allocation and zero branching.

**Known limitation:** a role may not contain `#annex`. This is enforced by the grammar itself
(`ROLE_BODY -> { METHODS EXTENSION_ERROR }` with `EXTENSION_NOTEXPECTED -> #annex ERROR`,
`syntax.txt:143-148, 609-610`) and reported as *error 012: role cannot have an extension*.
`doc/knownbugs.txt` `#00034` records it as a defect with a repro.

### 7.2 `#annex` — class mutation / any-message handler

`#annex X.` installs an **any-message handler**: every message the class VMT does not answer is
forwarded to `X`. `compileVMT` `case nsExtend` (`compiler.cpp:1418-1435`) compiles it to a redirect method with
message id 0 in a special "ANY VMT", setting `elVMTAnyHandler`; the expression form goes through
`compileExtend` (`compiler.cpp:1019-1042`). If the parent also has one, they chain.

Two syntactic positions:

**(a) Class-body position** — delegation:

```elena
#class WordList
{
    #field theList.
    #field theTotal.
    ...
    #annex (theList).          // anything I don't answer, ask theList
}
```
— `examples/words/words.l:32`. This is why `'program'output << aList` prints the underlying
list: `$printOn` is not in `WordList`, so it goes to `theList`.

The annexed expression is evaluated **each time a message misses**, so it can be dynamic:
`#annex (theEnumerator get).` (`src/std/properties.l:156`) forwards to whatever the enumerator
currently points at.

**(b) Expression position** — a decorator literal:

```elena
anArray := #annex(anArray)
{
    $sort = BSort $sort:anArray.
};
```
— `examples/bsort/bsort.l:78-81`. This creates a *new* anonymous object with one method `$sort`
and an any-handler pointing at the original array. The result is an array that also knows how to
sort itself, without touching the `Array` class.

The library uses it for read-only façades (`src/std/collections.l:339-355`):

```elena
#symbol ReadOnly : aList = #annex(aList)
{
    += aParam = $self fail.
    prop'$getEnumerator = ReadOnlyEnumerator::(aList $invoke).
}.
```

Note `$self fail.` — inside the annex literal, `$self` is the wrapper, so failing it makes `+=`
fail while everything else still reaches `aList`.

**Mixin form.** `#annex` accepts a collection or a `#group`, which is how the library composes
protocols without inheritance:

```elena
#static ListGroup = #group(ListExtension, prop'IndexerExtension).
#class List { ... #annex ListGroup. }
```
— `src/std/collections.l:48, 107`. Implement one hook (`prop'$getEnumerator`), annex the
extension objects, and you inherit `Count`, `@`, `$printOn`, the whole indexer protocol.

`doc/todo.txt:527` warns: *"warn if you dynamically override itself (like `#symbol my =
#annex(my){...}`)"* — the construct is easy to make circular.

### 7.3 `#group` and `#cast`

Both build an object out of several members. They differ only in dispatch policy:

| | `#group(a, b, c)` | `#cast(a, b, c)` |
|---|---|---|
| Class | `$elena'$group` | `$elena'$cast` |
| Runtime | `elena'21` | `elena'36` |
| Policy | send to each member **until one succeeds**; return its result | send to **every** member; return the group |
| Use | protocol composition, "mixin" | event multicast |

Both are ordinary object arrays (`ocreate` + `omoveptr` per element, `compiler.cpp:995-1017`)
given a **pseudo-VMT** whose single any-handler is a runtime routine
(`loadPseudoVMT`, `jitlinker.cpp:524-532`). `src/asm/elena.asm:839-919` (`elena'21`, group) tests
`eax` after each member call and exits on the first non-zero, returning 0 if none succeeded;
`elena'36` (`elena.asm:1276-1350`, cast) never tests, ignores every result and returns `edi` —
so **a `#cast` send always succeeds**.

A bare `( a, b, c )` with no keyword is neither: it is an ordinary array literal of the class
named by `[compiler] arrayclass` (default `std'basic'Array`).

```elena
// group: try EIteration first, then the indexer, then the iterator strategy
#group(ctrl'EIteration, anArray @ aLastIndex, prop'ArrayBIterator) run: anItem => [ ... ].
```
— `examples/bsort/bsort.l:54`

```elena
// cast: an event list — every handler must run
#method new [ #shift EOF. theEvents := #cast(nil). ]
```
— `src/sys/events.l:22-27`. `sys'events'Handlers::obj += handler` grows that cast dynamically
via `ctrl'DynamicCast` (`src/std/patterns.l:340-357`), which reallocates the member array with
`#inline standard'39` / `standard'44` / `standard'45`.

### 7.4 `#type`

`#type X` pushes a **reified class reference** — a `$elena'$typeinstance` object holding a VMT
pointer (`compileType`, `compiler.cpp:1044-1064`). It exists so that allocation primitives can be told what
class to stamp on a new object:

```elena
create : aParameter
[
    ^ #inline standard'38 (#type basic'Literal, aParameter Length).
]
```
— `src/std/basic.l:1557-1560`. `standard'38` is `WSTR_ALLOC(aType, aLen)`.

Two magic names are recognised: `#type group` and `#type cast` map to the built-in group/cast
classes (`elenaconst.h:39-40`), used by `DynamicGroup` / `DynamicCast`.

`src/elena.l:23-46` shows the reflection layer built on it:

```elena
#class $TypeInstance
{
    #field(4).
    #method $getClassName : aParameter = #inline elena'23 (self, aParameter).
    #method $typecast : aParameter = #inline elena'23 (self, aParameter).
    #method $ifSameType : anObject [ #inline elena'24 (self, anObject). ]
}

#symbol Type =
{
    Name = self $getClassName.
    ifSame : anObject = self $ifSameType:anObject.
    of : anObject = self $typecast:anObject.
}.
```

`elena'23` is `CLASS_REDIRECT` — it dispatches the current message against the *reified* VMT,
i.e. it calls a class method. `elena'24` is `IS_TYPESAME`.

### 7.5 Extensions

An "extension" is not a language construct — it is a naming convention for a **stateless object
that is annexed into other classes** to add protocol. `src/std/basic.l` defines eight of them
(`BasicExtension`, `IntegerExtension`, `LongExtension`, `RealExtension`, `LiteralExtension`,
`StringExtension`, `ArrayExtension`, `BooleanExtension`).

The pattern is:

```elena
#symbol LiteralExtension =
{
    $getLiteral = self $toLiteral $getLiteral.
    $toInteger  = #inline standard'26(basic'IntNumber, self $getLiteral).
    Length      = self $getLiteral Length.
    prop'$getEnumerator = prop'Indexer::self.
    prop'$printOn : aWriter [ aWriter $writeAsLiteral:(self $getLiteral). ]
}.

#symbol LiteralType = { $getGroup = #group(basic'LiteralExtension, prop'IndexerExtension). ... }.
#static LiteralGroup = basic'LiteralType $getGroup.
#class Literal (Magnitude) { ... #annex LiteralGroup. }
```

Note the key trick: inside the extension `self` refers to **whatever object annexed it**, not to
the extension. So `LiteralExtension` can call `self $toLiteral` and get the host's conversion.
This is duck-typed mixin composition, and it is the mechanism `doc/lang/elena.txt:24-25` calls
*"Extension — could we implement the mechanism which will help to integrate classes described in
other packages"*.

### 7.6 Properties, enumerators, indexers

There are no property declarations. A "property" is a **parameterised symbol** that wraps a
private accessor pair:

```elena
#symbol Index : anIndexer = anIndexer $getIndex clone.
#symbol ContentValue : anObject = anObject $getContent.
#symbol Caption : aControl = #annex(...)
{
    << aLiteral [ aControl $setCaption:aLiteral. ]
    basic'$toLiteral = aControl $getCaption.
}.
```
— `src/std/properties.l:422-433`, `src/gui/controls/properties.l:23-31`

Read it as `gui'prop'Caption::myButton << "OK"` (write) and
`basic'LiteralType of:(gui'prop'Caption::myButton)` (read). The property object is created on
demand and is usually stateless, hence free.

**Two iteration protocols exist**, and the library uses both:

| Protocol | Contract | Direction |
|---|---|---|
| **Enumerator** | `start`, `next`, `get`, `set`, `end`; `next` fails at the end | forward, sequential |
| **Indexer** | `$goto:n`, `$next`, `$previous`, `$getIndex`, `$getContent`, `$setContent`, `$seek:`, `$insert:`, `$delete:` | random access |

Client code uses the `prop'Enumerator` / `prop'Indexer` / `prop'IndexEnumerator` adapters:

```elena
prop'Enumerator::theControls run: aControl => [ ... ].              // foreach
prop'IndexEnumerator::theHand run: aCard => [ ... prop'Index::aCard ... ].  // foreach with index
#var anIndexer := prop'Indexer::aText.                              // random access
```

A collection joins in by implementing one hook — `prop'$getEnumerator` or `prop'$getIndexer` —
and annexing `prop'IndexerExtension`.

`doc/todo.txt:121-124` records that the author considered these obsolete and wanted to unify
them in 1.5.1.

### 7.7 Templates and generics

**There are none.** ELENA 1.5 has no type parameters and no compile-time templates. What
`whatsnew.txt:281-282` calls "symbol templates" is just parameterised symbols
(`#symbol X : p = ...`).

Genericity is achieved two ways:

1. **Duck typing** — everything is dynamic, so an algorithm written against `$getInteger` works
   for anything that answers it.
2. **Link-time substitution via project forwards** — `examples/sum/` compiles *one source* into
   two executables:

   | `intsum.prj` | `realsum.prj` |
   |---|---|
   | `'sumsample'number=std'basic'integer` | `'sumsample'number=std'basic'real` |
   | `'sumsample'prompt=sumsample'intsampleprompt` | `'sumsample'prompt=sumsample'realsampleprompt` |

   and `sum.l:23-24` reads `#var a := 'SumSample'Number.` The unresolved forward *is* the type
   parameter. This is genuinely the closest thing ELENA 1.5 has to generics.

---

## 8. Modules, projects and linkage

### 8.1 Namespaces and the `'` separator

A **reference** is a namespace path: `std'basic'Integer` = symbol `Integer` in namespace
`std'basic`. A leading `'` means the *root* namespace: `'program`, `'system`, `'mainwindow`.

Namespace ≠ file. The namespace of a declaration is `<package>'<relative path>`:

| Project setting | File | Namespace |
|---|---|---|
| `package=std` | `basic.l` | `std'basic` |
| `package=std` | `basic\memory.l` | `std'basic'memory` |
| `package=win32` | `api\controls.l` | `win32'api'controls` |
| `package=win32'socket` | `primitives.l` | `win32'socket'primitives` |
| *(no package)* | `helloworld.l` | `helloworld` |

So `src/std/basic.l` declaring `#class Integer` produces `std'basic'Integer`.

### 8.2 `.prj` structure

```ini
[project]
executable=bin\upndown.exe     ; output binary (executable projects)
output=tmp                     ; intermediate/module output directory
entry='entry                   ; startup symbol (a forward)
package=cardgame               ; namespace prefix for all files
template=gui                   ; bin/templates/<name>.prj to merge in
projecttype=2                  ; 0=library 1=console 2=GUI  (ProjectType, elenaconst.h:240)
debuginfo=-1                   ; emit debug module
warn:unresolved=0              ; warn about unresolved references

[files]
configs.l                      ; compilation order matters
game.l
game\upndown.l
...

[forwards]
'mainwindow=cardgame'gui'forms'playground
'program'modules=cardgame'configs'modules
'rulebook=cardgame'game'upndown'rulebook
```

Three subtleties that bite:

* **`entry=` does not define the forward `'entry`.** It calls `addForward(STARTUP_CLASS, value)`
  where `STARTUP_CLASS` is `'starter` (`elenaconst.h:31`). So `entry='entry` means *"bind
  `'starter` to `'entry`"*, and the template then binds `'entry` to `sys'templates'simple`. The
  assembly entry point calls `call @'starter` (`src/asm/elena.asm:540`).
* **`projecttype` is read by the IDE, not by `elc`.** `elc` takes the subsystem from
  **`[linker] type`**, which the *template* supplies (`console.cfg` → `type=1`, `gui.cfg` →
  `type=2`). `projecttype` lives in `[project]` and only `elide` looks at it. Values are
  `elenaconst.h:240-244` — `ptLibrary=0`, `ptConsole=1`, `ptGUI=2`.
* An **obsolete `type=` key in `[project]`** used a *different* mapping (0=console, 1=GUI,
  2=library), which is why some examples carry both and they appear to disagree —
  `examples/graphs/graphs.prj` has `type=1` *and* `projecttype=2`.

Library projects (`std`, `sys`, `ext`, `gui`, `win32`) use `projecttype=0` and
`output=..\..\lib\<name>`; linking is skipped entirely for them.

### 8.3 Forwards — whole-program configuration

A reference beginning with `'` and not resolvable in any compiled module is a **forward**: a hole
in the program that the `.prj` fills at link time. This is ELENA's dependency injection, and it
is used pervasively:

| Forward | Bound to | Meaning |
|---|---|---|
| `'program` | your entry symbol | the application object |
| `'program'output` / `'program'input` | `win32'io'StdOutput` / `StdInput` | the console streams |
| `'entry` | `sys'templates'Simple` etc. | the startup template |
| `'mainwindow` | your main form class | GUI entry |
| `'system` | `win32'system'GUI` | the GUI event system |
| `'gui'handlerType` | `win32'api'factories'HandlerType` | the platform's window factory |
| `'gui'styles'*` | `win32'api'styles'*` | the platform's control styles |
| `'collection'ListPrinter` | `ext'io'ListPrinter` | list formatting |
| `'nil` | `$elena'$nil` | the nil object |

**This is the seam that makes the `gui` package platform-independent.** `src/gui/controls.l:80`
writes `'gui'handlerType create:#group('gui'styles'StaticFrame, ...)` — no mention of Win32. The
Win32 binding lives entirely in the templates in `bin/templates/`.

Forwards come from three places, merged in this order (later wins):

1. **`bin/elc.cfg` `[forwards]`** — global defaults for every project:
   ```ini
   'collection'listprinter=ext'io'listprinter
   'handlers'textfilero=win32'io'textfilero
   'program'modules=sys'templates'idlemodules
   ```
2. **`bin/templates/<name>.cfg`**, selected by `template=` in the project. `console.cfg` binds
   5 forwards; `gui.cfg` binds 38 — the entire `'gui'styles'*` and `'gui'graphics'*` families
   plus `'system`, `'program` and `'entry`.
3. **The project's own `[forwards]` section.**

`bin/elc.cfg` also fixes the classes the compiler uses for literals — these are not built into
the language:

```ini
[compiler]
literalclass=std'basic'literal
integerclass=std'basic'intnumber
realclass=std'basic'realnumber
arrayclass=std'basic'array
```

so `"abc"` becomes a `std'basic'Literal`, `5` a `std'basic'IntNumber`, and `( a, b, c )` a
`std'basic'Array`. Point them elsewhere and the whole numeric tower changes.

The `[primitives]` section names the assembled runtime packages consumed by
`#inline` / `#external`:

```ini
[primitives]
elena=elena.bin
win32=win32.bin
standard=standard.bin
extended=extended.bin
winsock=winsock.bin
```

and `[linker] type=` (1 = console, 2 = GUI) plus `start=` (the assembly entry symbol) come from
the template.

### 8.4 Symbol resolution order

`Compiler::ModuleScope::mapReference` (`compiler.cpp:147-166`):

1. If the reference is **weak** (starts with `'`) → look it up directly / register as a forward.
2. Otherwise, try the `#define` **namespace masks** — first matching mask rewrites the prefix.
3. Otherwise, try the `#define` **named shortcuts** (`forwards` map).
4. Otherwise, use the reference verbatim.

Unqualified identifiers (`Magnitude`, `Program`) resolve inside the current module's namespace.

### 8.5 Compilation output

`elc` emits a `.nl` bytecode module per project plus a `.dnl` debug module
(`MODULE_SIGNATURE "EN!10"`, `DEBUG_MODULE_SIGNATURE "EN.D10!"`, version string `"ELENA.150"`).
For executable projects the JIT linker then materialises native x86 and the PE linker writes the
`.exe`.

There is **no separate compilation of a single file** and no DLL support — `doc/roadmap.txt`
lists *"DLL support; any package must be compiled as DLL"* as an unshipped 1.5.x goal.

---

## 9. External and native interop

### 9.1 The two forms

```elena
#external <package>'<N> ( arg, arg, ... )     // call a runtime procedure
#inline   <package>'<N> ( arg, arg, ... )     // splice a runtime code block
```

Both name a **numbered routine in an assembly package** under `src/asm/`. There is no FFI
declaration syntax, no type signature, no calling-convention annotation — the number *is* the
signature, and it is enforced only by the assembly source agreeing with the ELENA source.

| | `#external` | `#inline` |
|---|---|---|
| Assembly keyword | `procedure name (params)` | `inline name` |
| Compiled as | `callext` (`elena'16`) — a real call with a saved GC frame | the routine's machine code copied into the caller |
| Compiler function | `compileExternalFunction` (`compiler.cpp:940-966`) | `compileEmbeddedExpression` (`compiler.cpp:968-988`) |
| Stack cleanup | caller pops (`count-1` pops) | the embedded code pops |
| Limit | 127 arguments (`errExtTooManyParameters`) | — |
| Use for | anything that calls a DLL, anything long | 2–20 instruction primitives |

Arguments are pushed left to right; the first argument is conventionally the destination /
return object. Objects are passed as raw pointers, so a `#field(4)` object *is* a pointer to a
32-bit integer as far as the assembly is concerned.

**Reference resolution.** Both forms build `ReferenceName(PACKAGE_MODULE, terminal)` where
`PACKAGE_MODULE` is `"$package"` (`elenaconst.h:24`). `Project::resolveModule`
(`elenasrc/elc/project.cpp:201-208`) strips that prefix, takes the next namespace component, and
looks it up in the **`[primitives]`** section of `bin/elc.cfg`:

```
elena=elena.bin   standard=standard.bin   win32=win32.bin   winsock=winsock.bin   extended=extended.bin
```

Those `.bin` files are produced by `asm2binx` from `src/asm/*.asm`; each `procedure <name>` or
`inline <name>` directive emits a section called `$package'<name>`. So `#external win32'2(...)`
resolves to `$package'win32'2` = `procedure win32'2` in `src/asm/win32.asm:23`.

Inside the assembly, a declared parameter resolves to `[ebp + (count - index)*4]`, so parameter 0
is the leftmost ELENA argument.

### 9.2 The runtime packages

| Package | File | Routines | Content |
|---|---|---|---|
| `elena` | `src/asm/elena.asm` (1481) | 1–38 | GC, allocation, dispatch, roles, groups, entry points. **Emitted by the compiler, not called from ELENA source** — except 22/23/24, used by `src/elena.l`. |
| `standard` | `src/asm/standard.asm` (2804) | 1–82 | all arithmetic, string, array, buffer primitives |
| `win32` | `src/asm/win32.asm` (1423) | 1–63 | every Win32 API call |
| `winsock` | `src/asm/winsock.asm` (362) | 1–13 | Winsock TCP |
| `extended` | `src/asm/extended.asm` (76) | 1–3 | randomiser seed, random value, current date |

### 9.3 `elena'N` — the compiler's own primitives

These are what the compiler emits for language constructs. Names from `elenaconst.h:43-69`,
bodies in `src/asm/elena.asm`.

| N | Symbol | Kind | What it is |
|---|---|---|---|
| 1 | `ALLOC_FUNCTION` | proc | `STD_ALLOC` — bump-allocate, trigger GC |
| 2 | — | proc | GC mark-and-sweep + compact |
| 3 | — | proc | `STD_ENTRY` — **console program entry point** (`bin/templates/console.cfg`: `start=$package'elena'3`) |
| 4 | `PREP_FUNCTION` | inline | open a stack frame |
| 5 | `SPREP_FUNCTION` | inline | open a frame and set `self` |
| 6 | `RETURN_FUNCTION` | inline | return a value |
| 7 | `IOCALLN_FUNCTION` | inline | **send a message** (VMT scan from index n) |
| 8 | `SEXIT_FUNCTION` | inline | exit succeeding |
| 9 | `SRETURN_FUNCTION` | inline | return from a method |
| 10 | `RRETURNIF_FUNCTION` | inline | early return if equal to a constant |
| 11–15 | `OCREATE*_FUNCTION` | inline | allocate an object of 0/2/4/6/n fields |
| 16 | `CALLEXT_FUNCTION` | inline | call an external procedure with a GC frame |
| 17 | `PREPREDIR_FUNCTION` | inline | open a redirect frame |
| 18 | `EXITREDIR_FUNCTION` | inline | **exit failing** (`xor eax,eax`) |
| 19 | `REDIRECT_FUNCTION` | inline | forward the current message to an object (`#annex`) |
| 20 | `RREDIRECT_FUNCTION` | inline | forward the current message to a parent VMT |
| 21 | `GROUP_FUNCTION` | proc | `#group` dispatch — first member that succeeds |
| 22 | — | inline | `IS_SAME` — pointer identity (`Object ifSame:`) |
| 23 | — | inline | `CLASS_REDIRECT` — dispatch against a reified class |
| 24 | — | inline | `IS_TYPESAME` |
| 25 | `IOSWAP_FUNCTION` | inline | swap two stack items |
| 26 | `IOSET_FUNCTION` | inline | set an object's VMT |
| 28 | `SHIFT_FUNCTION` | inline | **`#shift RoleName`** |
| 29 | `UNSHIFT_FUNCTION` | inline | **`#shift.`** |
| 30 | `IRCALL_FUNCTION` | inline | send a message against a named VMT (`super`) |
| 31–33 | — | proc | GC write barriers: assign, alloc-temporary, add young-gen pointer |
| 34 | `ASSIGN_FUNCTION` | inline | inline write barrier |
| 35 | — | proc | `STD_WIN32ENTRY` — **GUI program entry point** (`bin/templates/gui.cfg`: `start=$package'elena'35`) |
| 36 | `CAST_FUNCTION` | proc | `#cast` dispatch — broadcast to all members |
| 37 | `WIND32PROC` | proc | the Win32 `WndProc` trampoline |
| 38 | — | data | table mapping Win32 message ids → ELENA message ids |

`elena'38` deserves a look — it is the entire Win32 event vocabulary, hard-coded in assembly
(`src/asm/elena.asm:1453-1478`):

```asm
structure elena'38
 dd 00002h
 dd #win32'api'$ondestroy
 dd 0000Fh
 dd #win32'api'$onpaint
 dd 00111h
 dd #win32'api'$oncommand
 ...
 dd 0FFFFh
 dd 0
end
```

### 9.4 `standard'N` — the primitive table

The complete table, from the comment headers in `src/asm/standard.asm`. `i` = `#inline`,
`p` = `#external`.

| N | | Operation | N | | Operation |
|---|---|---|---|---|---|
| 1 | i | `WSTR_EQUAL(s1,s2)` | 42 | p | `ARR_SET(ptr,offs,obj)` |
| 2 | i | `WSTR_LESS(s1,s2)` | 43 | i | `ARRAY_LEN(ret,obj)` |
| 3 | i | `INT_COPY(dest,sour)` | 44 | i | `GROUP_ADD(array,nil,obj)` |
| 4 | i | `WSTR_CPY(dest,sour)` | 45 | i | `GROUP_CPY(dest,sour)` |
| 5 | i | `WSTR_ADD(dest,sour)` | 46 | i | `LONG_COPYINT(dest,sour)` |
| 6 | i | `INT_EQUAL(n1,n2)` | 47 | i | `FLOAT_COPYLONG(dest,sour)` |
| 7 | i | `INT_LESS(n1,n2)` | 48 | i | `LONG_EQUAL(n1,n2)` |
| 8 | i | `INT_ADD(dest,sour)` | 49 | i | `LONG_LESS(n1,n2)` |
| 9 | i | `INT_SUB(dest,sour)` | 50 | i | `LONG_ADD(dest,sour)` |
| 10 | i | `INT_MUL(dest,sour)` | 51 | i | `LONG_SUB(dest,sour)` |
| 11 | i | `INT_DIV(dest,sour)` | 52 | i | `LONG_MUL(dest,sour)` |
| 12 | i | `INT_BAND(dest,sour)` | 53 | i | `LONG_DIV(dest,sour)` |
| 13 | i | `INT_BOR(dest,sour)` | 54 | i | `LONG_TEST(n1,n2)` |
| 14 | i | `INT_BXOR(dest,sour)` | 55 | i | `LONG_TEST2(n1,n2)` |
| 15 | i | `INT_SHIFT(dest,offset)` | 56 | i | `LONG_SHIFT(dest,offset)` |
| 16 | i | `INT_TEST(n1,n2)` — `anyMask:` | 57 | i | `LONG_NOT` |
| 17 | i | `INT_TEST2(n1,n2)` — `allMask:` | 58 | i | `LONG_BOR(dest,sour)` |
| 18 | p | `INT_NOT` | 59 | i | `LONG_BXOR(dest,sour)` |
| 19 | i | `WCHAR_CPYPTR(ch,ptr,ofs)` | 60 | i | `LONG_BAND(dest,sour)` |
| 20 | i | `WSTR_INDEXOF(ret,s,ofs,subs)` | 61 | i | `INT_COPYPTR(n,ptr,ofs)` |
| 21 | i | `WSTR_INSERT(ret,s,index,subs)` | 62 | i | `STR_COPYLONG(s,n)` |
| 22 | i | `WSTR_ERASE(ret,s,index,len)` | 63 | i | `LONG_CPYSTR(n,s)` |
| 23 | i | `WSTR_CPYWCHR(s,ch)` | 64 | i | `WSTR_SUBSTR(ret,s,index)` |
| 24 | i | `INT_COPYHWORD(dest,sour)` | 65 | i | `FLOAT_ABS(dest,sour)` |
| 25 | i | `INT_COPYLWORD(dest,sour)` | 66 | i | `INT_ROUNDREAL(n,f)` |
| 26 | i | `INT_CPYSTR(n,s)` | 67 | i | `FLOAT_TRUNC(dest,sour)` |
| 27 | p | `STR_COPYINT32(s,n)` | 68 | i | `FLOAT_ARCTAN(dest,sour)` |
| 28 | i | `FLOAT_COPYINT(dest,sour)` | 69 | i | `FLOAT_COS(dest,sour)` |
| 29 | i | `FLOAT_EQUAL(f1,f2)` | 70 | i | `FLOAT_EXP(dest,sour)` |
| 30 | i | `FLOAT_LESS(f1,f2)` | 71 | i | `FLOAT_LN(dest,sour)` |
| 31 | i | `LONG_COPY(dest,sour)` | 72 | i | `FLOAT_SIN(dest,sour)` |
| 32 | i | `FLOAT_ADD(dest,sour)` | 73 | i | `FLOAT_SQRT(dest,sour)` |
| 33 | i | `FLOAT_SUB(dest,sour)` | 74 | i | `FLOAT_PI(dest)` |
| 34 | i | `FLOAT_MUL(dest,sour)` | 75 | i | `DUMP_LEN(ret,obj)` |
| 35 | i | `FLOAT_DIV(dest,sour)` | 76 | i | `DUMP_CPY(dest,sour)` |
| 36 | i | `STR_COPYFLOAT(s,f)` | 77 | i | `WSTR_CPYPTR(str,ptr,offs)` |
| 37 | i | `FLOAT_CPYSTR(f,s)` | 78 | i | `DUMP_READ2BUF(buf,dump,offs)` |
| 38 | i | `WSTR_ALLOC(type,len)` | 79 | i | `PTR_ADDW(dest,offs,sour)` |
| 39 | i | `OBJ_ALLOC(type,pattern,len)` | 80 | i | `PTR_COPYINT32(ptr,ofs,n)` |
| 40 | i | `INT_LOADSTRADDR(int,s)` | 81 | i | `PTR_ADDBUF(buf,dump,offs)` |
| 41 | i | `ARR_GET(ptr,offs)` | 82 | i | `DUMP_ALLOC(type,len)` |

Typical use — every arithmetic method in the library is a one-line wrapper:

```elena
#method + aValue
[
    ^ #inline standard'8($self clone, aValue $getInteger).
]
```
— `src/std/basic.l:142-145`

And note how the primitive signals failure: `standard'1` (`WSTR_EQUAL`) ends with
`xor eax, eax` on mismatch (`src/asm/standard.asm:5-31`), so `$equal:` *fails* when the strings
differ. `Magnitude ==` then converts that into a boolean:

```elena
#method == aValue
[
    ^ self $equal:aValue
        back:basic'True | back:basic'False.
]
```

### 9.5 Calling a DLL

There is no `#external "kernel32" GetStdHandle(...)` syntax. Every Win32 call goes through a
hand-written assembly stub. `src/asm/win32.asm:5-19`:

```asm
procedure win32'1 (handle)
  push 0FFFFFFF5h
  call 'dlls'kernel32.GetStdHandle
  mov  ebx, handle
  mov  [ebx], eax
  ret
end
```

called from `src/win32/api.l:583-584`:

```elena
#symbol StdOutput
    = #external win32'1(api'Handle).
```

The `'dlls'kernel32.GetStdHandle` form is resolved by the assembler/linker via the `$dlls`
namespace (`DLL_NAMESPACE`, `elenaconst.h:23`). **Adding one Win32 API to the library therefore
requires writing x86 assembly.** This is the single largest obstacle to porting.

---

## 10. Idiom cookbook

Twenty-six annotated snippets, all verbatim from this tree.

**1 — Hello, world.** The entry object answers `proceed`.
```elena
#symbol Program =
{
    proceed
    [
        'program'output << "Hello World!!%n".
        'program'input get. // wait for any key
    ]
}.
```
`examples/helloworld/helloworld.l:3-11`. The project supplies `'program=helloworld'program`.

**2 — The short entry form.** `=> [...]` makes the symbol itself an action.
```elena
#symbol Program =>
[
    'program'output << "Enter the text(to stop press enter two times):%n".
```
`examples/words/words.l:37-39`

**3 — Prompt and read in one expression.** `<<` and `>>` both return the stream.
```elena
#var aSize := ext'io'console << "Enter the array size:" >> basic'Integer.
```
`examples/bsort/bsort.l:75`

**4 — Input validation is a failure alternative, not a check.**
```elena
#if 'program'input >> aNumber
    | [ 'program'output << "Input error. Aborting%n". $self fail. ].
```
`examples/sum/sum.l:13-14`

**5 — The ternary.** `back:` names the value of the succeeding path.
```elena
#method == aValue
[
    ^ self $equal:aValue
        back:basic'True | back:basic'False.
]
```
`src/std/basic.l:57-61`

**6 — Boolean AND / OR without booleans.** Chained `if:` = AND; `| if:` = OR.
```elena
validate : aChar
[
    basic'True if:(43 == aChar) | if:(45 == aChar) | if:(42 == aChar) | if:(47 == aChar).
]
```
`examples/calculator/parser.l:52-55`. The method has no `^` — its *success* is the answer.

**7 — Bounds checking by failing.**
```elena
#method prop'$goto : aCard
[
    basic'True if:(aCard < 37) if:(aCard > 0).

    theCard << aCard.
]
```
`examples/upndown/engine/cards.l:200-205`. If the guard fails, the assignment never runs and
the caller sees a failed `$goto`.

**8 — Multi-way dispatch.** Subject evaluated once, `| op ?` per case.
```elena
#if (basic'LiteralType of:aChar)
    == ">"   ? [ theTapeIterator $next. ]
    | == "<" ? [ theTapeIterator $previous. ]
    | == "+" ? [ theTapeIterator += 1. ]
    | == "[" ? [ #shift Looping. self $resetLoop. ].
```
`examples/interpreter/interpreter.l:79-86`

**9 — A role that turns a method into a failure — this is how loops end.**
```elena
#role Stopped
{
    #method $ifNotStopped [ self fail. ]
}
```
…and the loop that consumes it, with no body at all:
```elena
#loop 'program'Input >> aText $ifNotStopped.
```
`examples/replace/replace.l:15-18, 60`

**10 — Bootstrap role: shift out, then re-send.**
```elena
#role Empty
{
    #method << aValue
    [
        #shift.
        self << aValue.
    ]
}
```
`src/std/basic.l:808-827`

**11 — Sentinel role guarding an enumerator.**
```elena
#role EOF
{
    #method get = self fail.
    #method next [ self fail. ]
    #method insert : anObject
    [
        theList += anObject.
        theCurrent := theList $getTale.
        #shift.
    ]
}
```
`src/std/collections.l:191-215`

**12 — Delegation with `#annex`.**
```elena
#class WordList
{
    #field theList.
    #field theTotal.
    ...
    #method $getUnique = theList Count.

    #annex (theList).
}
```
`examples/words/words.l:9-33`

**13 — Decorator literal.** Add one method to an existing object, in place.
```elena
anArray := #annex(anArray)
{
    $sort = BSort $sort:anArray.
}.
```
`examples/bsort/bsort.l:78-81`

**14 — Read-only façade.**
```elena
#symbol ReadOnly : aList = #annex(aList)
{
    += aParam = $self fail.
    prop'$getEnumerator = ReadOnlyEnumerator::(aList $invoke).
}.
```
`src/std/collections.l:350-355`

**15 — Protocol composition by annexed group.**
```elena
#static ListGroup = #group(ListExtension, prop'IndexerExtension).

#class List
{
    ...
    #method prop'$getEnumerator = list'Enumerator::self.

    #annex ListGroup.
}
```
`src/std/collections.l:48-107`

**16 — `#group` chosen at the call site.**
```elena
#group(ctrl'EIteration, anArray @ aLastIndex, prop'ArrayBIterator) run:
    anItem => [
        #var aCurrent := prop'Index::anItem.
        prop'Content::anItem exchange:(anArray@0).
        ...
    ].
```
`examples/bsort/bsort.l:54-65`

**17 — `#cast` as a dynamic event list.**
```elena
#method new
[
    #shift EOF.
    theEvents := #cast(nil).
]
```
`src/sys/events.l:22-27`; grown by `events'Handlers::obj += { ... }`.

**18 — Registering event handlers as an inline object.**
```elena
events'Handlers::theClientSocket +=
{
    sockets'$OnSocketDisconnect = self $getListener $onDisconnect.
    sockets'$OnConnectRefuse    = self $getListener $onServerRefuse.
    sockets'$onRead : aSocket   = self $getListener $onClientRead:aSocket.
}.
```
`examples/upndown/gui/forms.l:973-983`

**19 — A record literal used as a named-argument bundle.**
```elena
BSort $sortDown:
{
    for  = anArray.
    from = aCurrent.
    till = aLastIndex.
}.
```
`examples/bsort/bsort.l:46-50`; unpacked with `aRange for`, `aRange from`, `aRange till`.

**20 — A marker object.** An empty method used purely as a type test.
```elena
#symbol Current =
{
    $isCurrent []
    == anIndex
    [
        ^ anIndex $isCurrent
            back:basic'True
            | back:basic'False.
    ]
}.
```
`examples/upndown/game.l:143-153`

**21 — foreach, with and without an index.**
```elena
prop'Enumerator::theControls run: aControl => [ ... ].

prop'IndexEnumerator::theHand run: aCard =>
[
    ... prop'Index::aCard ...
].
```
`src/gui/controls.l:386`, `examples/upndown/gui/forms.l:1060-1068`

**22 — Search and replace all, in three lines.**
```elena
#var aCriteria := prop'ValueCriteria { Value = aSearchText. }.
#var anIndexer := prop'Indexer::aText.
#loop anIndexer seek:aCriteria delete:{ Count = aSearchText Length. } insert:aReplaceText.
```
`examples/replace/replace.l:65-69`

**23 — Namespace-qualified messages as an interface.**
```elena
#method engine'$onPrepare : Players
[
    self $doPrepare:Players.
]
```
`examples/upndown/players.l:80-83`. The `cardgame'engine` package declares the message; any
package can implement it without name collisions.

**24 — Wrapping a primitive.**
```elena
#method Length = #inline standard'43(basic'IntNumber, $self).

#method $set : anObject
[
    #external standard'42(theArray, theIndex, anObject).
    theItem := anObject.
]
```
`src/std/basic.l:508, 1210-1215`

**25 — Link-time genericity.** One source, two programs.
```elena
'program'output << 'SumSample'Prompt.

#var a := 'SumSample'Number.
#var b := 'SumSample'Number.
```
`examples/sum/sum.l:21-24`, with `intsum.prj` / `realsum.prj` binding
`'sumsample'number` to `std'basic'integer` or `std'basic'real`.

**26 — Custom control flow as a class.** ELENA has no `while` keyword, so write one.
```elena
#method run : anAction
[
    anAction proceed.
    #loop theCondition?
    [
        anAction proceed.
    ]
]
```
`examples/TEST/class.l:9-16`, used as `class'while::a run: => [ ... ]`.

---

## 11. Comparison, and viability for OS development

### 11.1 Where ELENA sits

| Feature | Smalltalk-80 | Self | ELENA 1.5 | Modern (Rust/Swift) |
|---|---|---|---|---|
| Uniform object model | yes | yes | **yes** | no (primitives) |
| Message-based control flow | yes (blocks + `ifTrue:`) | yes | **yes, but failure-based** | no |
| Failure as control flow | `doesNotUnderstand:` → exception | same | **the primary mechanism** | `Option`/`Result`, but explicit |
| Exceptions | yes | yes | **none** | yes |
| Classes | yes | prototypes | yes, plus roles | yes |
| Behaviour change at run time | `become:` | delegation slots | **`#shift` — VMT swap, O(1)** | no |
| Dynamic delegation | `doesNotUnderstand:` forwarding | parent slots | **`#annex` — first-class** | protocol witnesses (static) |
| Multiple dispatch targets | no | no | **`#group` / `#cast`** | no |
| Static types | no | no | **no** | yes |
| Generics | no | no | **no** (link-time forwards only) | yes |
| Reflection | full | full | **minimal** (`Type Name`, `ifSame`) | partial |
| Multi-argument messages | keyword messages, n-ary | n-ary | **1 argument maximum** | n-ary |
| Blocks/closures | first-class | first-class | **`=> [...]` — captures by copy** | first-class |
| Concurrency | processes | threads | **none** | yes |

The two genuinely original ideas are **`#shift`** (an object changing class in O(1) with no
allocation and no identity change) and **failure-as-control-flow** (removing `if`, `while`,
`null` and `throw` in one stroke). Both are cheap at runtime — that is the interesting part.

The two most costly omissions are **no exceptions** (a failure carries no information: you know
*that* something failed, never *what* or *why* — `doc/todo.txt:176-177, 239` agonise over this)
and **one argument per message** (which forces record literals and chaining everywhere).

### 11.2 What OS development requires, and what exists

| Requirement | Status in 1.5 | Evidence / gap |
|---|---|---|
| **Raw memory access** | ⚠️ partial | `#field(N)` gives a fixed binary body; `#field.` gives a variable one; `standard'61/79/80` read/write 32-bit words at an offset. But there is **no pointer type**, no address arithmetic, no way to construct an object *at* an address. `win32'api'LPVOID` gets an address of an object (`#inline win32'15`) but cannot dereference an arbitrary one. |
| **Fixed-layout structs** | ✅ yes | `#field(N)` + `elStructureRole` is exactly a struct. `win32'api'MSG` is `#field(28)`, `PAINTSTRUCT` is `#field(64)` — real Win32 structs, byte-accurate. Fields are accessed by explicit offset (`standard'61(x, $self, 4)`), so **there is no field-name-to-offset mapping** — you write the offsets by hand. |
| **Bit manipulation** | ✅ yes | `and: or: xor: not shift: anyMask: allMask:` on `IntNumber` and `LongNumber` (`standard'12-18`, `54-60`). Complete and efficient. |
| **Inline assembly** | ⚠️ indirect | `#inline pkg'N` splices assembly, but the assembly must live in a separate `.asm` file compiled by `asm2bin`. There is no `asm { }` block in ELENA source. Practically this *is* inline asm with a rename step. |
| **No-GC regions / manual allocation** | ❌ none | Every object comes from `elena'1` (`STD_ALLOC`) which may trigger a collection. There is no stack allocation (`doc/todo.txt:486-487, 528` — "temporal objects" were being designed), no arena, no `free`. |
| **Deterministic destruction** | ❌ none | No destructors, no finalizers, no RAII. Handles are closed by explicit `close` messages and leak when you forget (`knownbugs #00043`). |
| **Interrupts / ISRs** | ❌ none | The only callback mechanism is `elena'37`, a hand-written x86 `WndProc` trampoline that opens a GC frame. A generic "call ELENA from arbitrary native context" ABI does not exist — it would have to be generalised from `elena'16` + `elena'37`. |
| **Freestanding runtime** | ❌ none | `elena'35` (`STD_WIN32ENTRY`) calls `GetProcessHeap` / `HeapAlloc` / `ExitProcess`. The GC's *entire heap* comes from the Win32 heap. There is no way to hand it a memory region. |
| **Concurrency / atomics** | ❌ none | No threads, no locks, no atomics, no memory model. `doc/todo.txt:27-28, 482-483` list it as unsolved: *"how could GC algorithm modified to deal with multi-thread applications (thread stacks, synchronization)"*. |
| **Volatile / MMIO** | ❌ none | No volatile qualifier, and the JIT is free to reorder. MMIO would have to go through `#external` stubs. |
| **Compile-time constants / `const`** | ⚠️ partial | `#define[const]` marks a *reference* as constant so it can be used in `#annex`/`#group`. There is no constant folding of arithmetic and no `constexpr`. |
| **Zero-cost abstraction** | ❌ no | Every message is a linear VMT scan (`iocall`). Roles, groups and annexes each add a dispatch layer. `doc/todo.txt:531` — *"due to gc every integer takes 12 bytes"*. |
| **Static linking / no runtime** | ❌ no | The 6,150-line assembly runtime is mandatory and Win32-specific. |
| **Error propagation with information** | ❌ no | The single most serious gap for systems work: a driver that fails cannot say *why*. |

### 11.3 The five changes that would make it plausible

Ordered by how much they unblock:

1. **A pointer / memory-region type.** Something like `#field(N)` but backed by an address the
   program supplies, plus load/store at computed offsets. Today `standard'61/79/80` almost do
   this but only against an object the GC owns. Without it you cannot touch a page table, a
   framebuffer, or an MMIO register.
2. **A failure value.** Keep the failure protocol — it is good — but let a failing method carry
   an object. `EAX = 0` becomes `EAX = 0` *or* a tagged error object; `|` binds it. This is
   almost source-compatible and turns the language from "unusable for diagnosis" into "usable".
3. **A pluggable allocator + no-GC regions.** `elena'1` takes its heap from `HeapAlloc`. Making
   the heap a parameter, and adding a region in which `ocreate` is a bump pointer with no
   collection, would let kernel code allocate.
4. **A general native-callback ABI.** Generalise `elena'37` into "given a function pointer
   signature, produce a native thunk that enters ELENA with a fresh GC frame". That gives you
   ISRs, syscall handlers and threads.
5. **Named fields on structure objects.** `#field(4) theWidth. #field(4) theHeight.` with
   compiler-computed offsets, instead of `#field(8)` plus hand-written
   `standard'61(x, $self, 4)`. Purely ergonomic, but hand-written offsets in kernel structs are
   how you get memory corruption.

Notably **absent from that list**: object model changes. Message-passing with a linear VMT scan
is slow but not *wrong* for an OS — it is roughly what a microkernel IPC layer does anyway, and
`#shift` maps neatly onto "this device is now in state X". The blockers are all about memory and
error reporting, not about polymorphism.

### 11.4 What the LLVM migration changes

Three things in this document become negotiable once the backend is LLVM:

* **§9 disappears.** `#inline standard'N` / `#external win32'N` exist because the compiler could
  not generate arithmetic. With LLVM, `IntNumber +` becomes an `add` intrinsic and 2,800 lines of
  `standard.asm` become an intrinsic table. This is the largest single simplification available.
* **§2.1 (encoding) and §2.2 (case-insensitivity)** are free to fix — nothing depends on them.
* **The 32-bit assumption** in §1.4 (object header, VMT layout, `elena'38`) is pervasive but
  mechanical. `doc/todo.txt:340` already asks for *"do not use processor specified offsets in
  byte code"*.

What LLVM does **not** give you: the pointer type, the failure value, the allocator hook, or
the callback ABI. Those are language design decisions and should be made before the backend
work locks in an IR shape.

---

*See also:* [`11-standard-library.md`](11-standard-library.md) for the library catalogue,
dependency graph and platform-coupling inventory.
