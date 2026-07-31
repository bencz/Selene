# 02 — The ELC Compiler Frontend (ELENA 1.5.0.0, 2009)

> Scope of this document: everything under `elenasrc/elc/` — the command-line driver, the
> project/config loader, the lexer, the table-driven parser, the derivation tree, and the
> semantic-analysis / byte-code-emission pass in `compiler.cpp`.
> The PE linker (`elenasrc/elc/win32/linker.cpp`) and the x86 JIT/engine
> (`elenasrc/engine/`) are only referenced where the frontend touches them; they are
> documented separately.
>
> All line anchors were verified against the 2009 sources in this tree.

---

## Table of contents

1. [Purpose & entry point](#1-purpose--entry-point)
2. [Compilation pipeline](#2-compilation-pipeline)
3. [Lexer / source reading](#3-lexer--source-reading)
4. [Parser](#4-parser)
5. [Semantic analysis & code generation](#5-semantic-analysis--code-generation-in-compilercpp)
6. [Message / reference encoding](#6-message--reference-encoding)
7. [Project (.prj) and config (.cfg) format](#7-project-prj-and-config-cfg-format)
8. [Error handling & diagnostics](#8-error-handling--diagnostics)
9. [Data structures used](#9-data-structures-used)
10. [Modernization notes](#10-modernization-notes)

---

## 1. Purpose & entry point

`elc.exe` is a single-pass, whole-program-ish compiler driver. It:

1. reads a configuration chain (`elc.cfg` → template `.cfg` → project `.prj`),
2. compiles every source file listed in the `[files]` category into a **`.nl` module**
   (an ELENA byte-code module — *not* an object file),
3. optionally emits a parallel **`.dnl` debug module**,
4. and — unless the project type is `ptLibrary` — invokes the in-process PE linker,
   which JIT-compiles all reachable byte code to x86 and writes a Windows `.exe`.

There is **no separate assembler/link step and no VM**: the compiler's output is byte
code, and `x86JITCompiler` + `Linker` turn it into a native image at link time.

### 1.1 `main()`

`elenasrc/elc/win32/elc.cpp:255` — the only entry point. It is a `main()` with *no*
parameters; the command line is obtained from Win32:

```cpp
int main()
{
   int argc;
   TCHAR **argv = CommandLineToArgvW(GetCommandLineW(), &argc);   // elc.cpp:258

   // switch to unicode command line output if requiered
   if (argc > 1 && compstr(argv[1] + 1, ELC_PRM_UNICODE))
      setmode(_fileno(stdout), _O_WTEXT);                          // elc.cpp:262
```

Sequence executed by `main`:

| Step | Line | What happens |
|---|---|---|
| Greeting | `elc.cpp:268` | `ELC_GREETING` with `ELC_MAJOR_VERSION`/`ELC_MINOR_VERSION` (`elc.h:22`, `elc.h:23` — hard-coded `2.0`, **not** 1.5) |
| No args → help | `elc.cpp:270-274` | prints `ELC_HELP_INFO` (`elc.h:80`), returns `-3` |
| Load global config | `elc.cpp:277` | `project.loadConfig(Path(project.appPath, DEFAULT_CONFIG), false)` — `elc.cfg` next to the exe, *optional* |
| Parse argv | `elc.cpp:280-285` | `-x…` → `Project::setOption`, anything else → `Project::addSource` |
| Load `$elena` | `elc.cpp:288-290` | unless `-lstd`, loads `elena.nl` (`ELC_STANDARD_MODULE`, `elc.h:19`) from `libpath` |
| Clean up | `elc.cpp:293-294` | **deletes** every output `.nl` that this project would produce (`Project::cleanUp`, `elc.cpp:62`) |
| Load parser table | `elc.cpp:299-302` | `syntax.dat` (`SYNTAX_FILE`, `elc.h:18`) from the exe directory; fatal `errInvalidFile` if missing |
| Compile | `elc.cpp:304-310` | `Compiler compiler(&syntaxFile); compiler.run(project)` |
| Link | `elc.cpp:313-320` | only if `IntSetting(opSystemType) != ptLibrary`; constructs `Linker(&project, new x86JITCompiler(project.resolvePrimitive(CORE_BINARY_MODULE, false)))` |
| Failure paths | `elc.cpp:322-333` | `InternalError` → `-2`, generic `_Exception` → `-2`; both call `cleanUp()` again |

Notes / gotchas:

* `Compiler::run` returns `false` **when there were warnings**, not errors
  (`compiler.cpp:1754`, `return !project.HasWarnings();`). `main` then sets
  `exitCode = -1` and prints `ELC_WARNING_COMPILATION` — **but still links**.
  There is no "errors" return path at all: an error always throws.
* `getAppPath` (`elc.cpp:26`) uses `::GetModuleFileName(NULL, …)` — Win32-only.
* `CommandLineToArgvW` returns `LPWSTR*`, assigned to `TCHAR**`; the frontend therefore
  only compiles in a `_UNICODE` build. The CodeBlocks project confirms this
  (`elenasrc/elc/codeblocks/elc.cbp`, `-D_UNICODE -DUNICODE`).

### 1.2 Command-line options

Defined as single characters in `elc.h:26-43`, dispatched in
`_ELC_::Project::setOption` (`elc.cpp:175`).

| Flag | Constant | `ProjectSetting` | Meaning |
|---|---|---|---|
| `-c<path>` | `ELC_PRM_CONFIG` | — | load a `.prj`/`.cfg`; also defaults `opOutputPath` to the file's directory (`elc.cpp:232-240`) |
| `-d` | `ELC_PRM_DEBUGINFO` | `opWithDebugInfo` | emit `.dnl` debug module |
| `-e<symbol>` | `ELC_PRM_ENTRY` | forward `'starter` | `addForward(STARTUP_CLASS, value+1)` |
| `-g<name>` | `ELC_PRM_PACKAGE` | `opPackage` | package (namespace) prefix for produced module names |
| `-lstd` | `ELC_PRM_STANDART_LIBRARY` | `opStandart` | compile *as* the `$elena` standard module; suppresses loading `elena.nl` |
| `-m<path>` | `ELC_PRM_MAP` | `opMapFile` | map file (consumed by the linker) |
| `-o<path>` | `ELC_PRM_OUTPUT_PATH` | `opOutputPath` | where `.nl`/`.dnl` are written |
| `-p<path>` | `ELC_PRM_LIB_PATH` | `opLibPath` | module search root |
| `-s<symbol>` | `ELC_PRM_START` | `opEntry` | linker entry symbol |
| `-t<path>` | `ELC_PRM_TARGET` | `opTarget` | output executable |
| `-wun` / `-wwun` | `ELC_W_UNRESOLVED` / `ELC_W_WEAKUNRESOLVED` | `opWarnOnUnresolved`, `opWarnOnWeakUnresolved` | enable unresolved-reference warnings |
| `-xtab<n>` | `ELC_PRM_TABSIZE` | (member `_tabSize`) | tab width used for column arithmetic |
| `-xunicode` | `ELC_PRM_UNICODE` | — | switch stdout to UTF-16 (only honoured as `argv[1]`) |
| `-xpath<dir>` | `ELC_PRM_PROJECTPATH` | `opProjectPath` (+`opOutputPath`) | base directory for command-line source paths |

**Bug — `-xunicode`** (`elc.cpp:197-199`): the handler does
`_settings.add(opOutputPath, value + 1)`, i.e. it sets the *output path* to the literal
string `"unicode"`. The flag only works because `main` intercepts it separately at
`elc.cpp:261`. Copy/paste defect.

**Doc mismatch** — `ELC_HELP_INFO` (`elc.h:80`) advertises `-xguit` for GUI applications;
no such option exists in `setOption`. GUI mode comes from the `gui` template's
`[linker] type=2`.

### 1.3 Config / project loading

`_ELC_::Project::loadConfig` (`elc.cpp:115`) is recursive and idempotent-by-overwrite:
later `loadConfig` calls overwrite earlier settings, but *absent* keys leave the previous
value untouched (`setOption`/`setPathOption` only add when the key exists).

Load order for `elc -cfoo.prj`:

```
1. <appdir>\elc.cfg                        (elc.cpp:277, optional)
2. foo.prj [templates]                     (elc.cpp:128)
3. the template named by [project] template (elc.cpp:131-138, recursive loadConfig)
4. the rest of foo.prj                     (elc.cpp:141-172)
```

Note the ordering quirk at `elc.cpp:131-138`: the template is resolved from the
`[templates]` category of the *file currently being loaded*, so a `.prj` that names
`template=console` relies on `elc.cfg` having already contributed `console=templates\console.cfg`
into `opTemplates` — the `_settings` dictionary is shared, so this works, but the
dependency is implicit.

`loadCategory` (`elc.cpp:77`) is the generic multi-value loader used for
`[templates]`, `[primitives]`, `[files]` and `[forwards]`. It lowercases keys and values
and, when a base path is supplied, makes values absolute relative to the config file.
Consequences: **all forwards, file names and reference names are case-insensitive and
stored lowercase**; ELENA source is therefore effectively case-insensitive too (the lexer
lowercases every token, see §3).

---

## 2. Compilation pipeline

End-to-end, source text → `.nl` module:

```
elc.exe main()                                        win32/elc.cpp:255
 └─ Project::loadConfig / setOption / addSource       win32/elc.cpp:115 / 175 / 44
 └─ Compiler::Compiler(StreamReader* syntax)          compiler.cpp:483
      ├─ Parser::Parser  → ParserTable::load          parser.cpp:113 → parsertable.cpp:199
      └─ Compiler::loadPredefinedMessages             compiler.cpp:489
 └─ Compiler::run(project)                            compiler.cpp:1717
      for each entry of project [files]:
        ├─ Project::createModule   (.nl in memory)    project.cpp:77
        ├─ Project::createDebugModule (.dnl)          project.cpp:95
        ├─ ModuleScope scope(...)                     compiler.cpp:101
        ├─ Compiler::compile(source, buffer, scope)   compiler.cpp:1698
        │    ├─ TextFileReader(source, feAutodetect)  compiler.cpp:1701
        │    ├─ Parser::parse(reader, writer, tab)    parser.cpp:141
        │    │    ├─ SourceReader::read  (DFA lexer)  source.cpp:73
        │    │    ├─ Parser::derive  (LL(1) driver)   parser.cpp:118
        │    │    └─ DerivationWriter::writeSymbol /
        │    │       writeTerminal → MemoryDump       derivation.cpp:19 / 24
        │    └─ Compiler::compileModule(root, scope)  compiler.cpp:1654
        │         ├─ compileDirectives (#define)      compiler.cpp:1623
        │         ├─ compileClassDeclaration          compiler.cpp:1549
        │         ├─ compileSymbolDeclaration         compiler.cpp:1589
        │         └─ … → ByteCodeCompiler (CommandTape)
        ├─ Project::saveModule    → <output>\x.nl     project.cpp:106
        └─ Project::saveDebugModule → <output>\x.dnl  project.cpp:121
 └─ Linker::run()                                     win32/linker.cpp (separate doc)
```

### 2.1 Stage table

| # | Stage | Implementation | Input | Output |
|---|---|---|---|---|
| 0 | Parser table load | `ParserTable::load` `parsertable.cpp:199` | `syntax.dat` | in-memory `SymbolMap` + `TableHash` |
| 1 | Encoding detect + line read | `File::File` `common/files.cpp:19`, `SourceReader::cacheLine` `source.cpp:61` | file bytes | one `TCHAR` line (max `LINE_LEN`=0x1000) |
| 2 | Tokenize | `SourceReader::read` `source.cpp:73` + `DFA::makeStep` `dfa.h:27` | line buffer | `LineInfo{type,row,col,position,length}` |
| 3 | Terminal classification | `getTerminalInfo` `parser.cpp:65` | `LineInfo` | `TerminalInfo{Symbol, value, row, col, disp, length}` |
| 4 | LL(1) derivation | `Parser::derive` `parser.cpp:118` | terminal + stack | derivation events |
| 5 | Serialize derivation tree | `DerivationWriter` `derivation.cpp:19,24` | events | flat DWORD stream in a `MemoryDump` |
| 6 | Tree navigation | `DerivationReader` `derivation.cpp:50-155` | `MemoryDump` | `DNode` cursor API |
| 7 | Semantic analysis + lowering | `Compiler::compileModule` `compiler.cpp:1654` and descendants | `DNode` tree | `CommandTape` (byte-code IR) + `ClassInfo` metadata |
| 8 | Byte-code serialization | `ByteCodeCompiler::save` `engine/bccompiler.cpp:858` | `CommandTape` | module sections (`mskSymbolRef`, `mskClassRef`, `mskVMTRef`, `mskMetaDataRef`) |
| 9 | Module write | `Module::save` `engine/module.cpp:146` | sections + name/reference/message/constant tables | `.nl` file |

**Important architectural property:** the derivation tree is *not* an in-memory AST of
objects. It is a **flat, forward-only DWORD stream** in a `MemoryDump`; `DNode` is a
`(reader, position, symbol)` cursor that re-parses the stream on every `firstChild()`,
`nextNode()` or `select()` call (`derivation.cpp:55`, `:76`, `:102`). Navigation is
therefore O(n) per step and the tree is single-pass-friendly but re-traversal-hostile.
The same `MemoryDump buffer` is reused for every source file (`compiler.cpp:1705`,
`compiler.cpp:1721`).

---

## 3. Lexer / source reading

### 3.1 Encoding

Encoding handling lives in `common/files.cpp`, not in `elc/`. `Compiler::compile`
(`compiler.cpp:1701`) opens sources with `feAutodetect`:

```cpp
if (encoding==feAutodetect) {                     // common/files.cpp:24
   unsigned short signature = 0;
   fread(&signature, 1, 2, _file);
   if (signature==0xFEFF) { _encoding = feUTF16; }
   else { _encoding = feAnsi; rewind(); }
}
```

* Only a **UTF-16LE BOM** is detected. UTF-8 (with or without BOM) is silently treated as
  ANSI, so a UTF-8 BOM (`EF BB BF`) is fed to the lexer as three garbage characters.
* ANSI→wide conversion goes through `MultiByteToWideChar(CP_ACP, …)`
  (`common/win32/unicode.h:17`) — the result depends on the machine's ANSI code page.
* `FileEncoding` enum: `common/files.h:200`.

### 3.2 Line buffering

`SourceReader` (`source.h:94`) keeps exactly **one line** at a time in `_line`
(allocated `LINE_LEN + 1` = 0x1001 `TCHAR`, `source.cpp:56`):

```cpp
void SourceReader :: cacheLine()                 // source.cpp:61
{
   if (!_source->read(_line, LINE_LEN)) _line[0] = 0;
   _position = 0;  _row++;  _column = 1;
   if (getlength(_line)==LINE_LEN) throw LineTooLong(_row);
}
```

Consequences:

* **A token cannot span a line.** Literals, comments (`/* … */`) and identifiers must be
  on one line. (The DFA has `dfaComment` states, but a multi-line comment simply causes
  `cacheLine()` mid-token, which works only because the DFA state survives the refill.)
* Lines ≥ 4096 characters throw `LineTooLong` → error 001.
* `EOF` is represented by an empty `_line` — reading past EOF returns `dfaEOF` forever.

Column tracking honours tabs (`SourceReader::nextColumn`, `source.h:104`) via
`calcTabShift(col-1, tabSize)` (`common/tools.h:61`), with `tabSize` from
`Project::getTabSize()` (`elc.h:176`, default 4 at `elc.cpp:41`). Note that `TerminalInfo`
carries **both** `col` (tab-expanded, for error messages) and `disp` (raw character offset
in the line, for debug info) — see the comment at `engine/bccompiler.h:78`.

### 3.3 The DFA

`dfa.h:20` declares a template parameterised on the table and five state constants:

```cpp
template <const TCHAR* DFA_table[23], TCHAR start, TCHAR whitespace,
          TCHAR backState, TCHAR lineComment, TCHAR comment> struct DFA
```

instantiated at `source.cpp:77` as
`DFA<DFA_table, dfaStart, dfaWhitespace, dfaBack, dfaLineComment, dfaComment>`.

The table itself is `const TCHAR* DFA_table[27]` (`source.cpp:18-47`): **27 rows of
exactly 128 characters**. Row index = `state - 'a'`; column = the input character code.
The template's declared bound of `23` is wrong but harmless (the parameter decays to a
pointer). States are encoded as ASCII letters (`source.h:18-39`):

| Const | Char | Meaning | Terminal symbol produced |
|---|---|---|---|
| `dfaStart` | `a` | start | — |
| `dfaSlashOperator` | `b` | `/` seen | (`h` or comment) |
| `dfaLineComment` | `c` | `//…` | skipped |
| `dfaWhitespace` | `d` | space/tab/CR/LF | skipped |
| `dfaKeyword` | `e` | `#class`, `#var`, … | looked up in symbol table |
| `dfaIdentifier` | `f` | `abc`, `MyClass` | `tsIdentifier` |
| `dfaBracket` | `g` | `( ) [ ] { } , . ;` | looked up |
| `dfaOperator` | `h` | `+ - * / < > = ! ? @ :` | looked up |
| `dfaDblOperator` | `i` | `== != >= <= += << :: →` | looked up |
| `dfaFullIdentifier` | `j` | `std'basic'literal`, `'entry` | `tsReference` |
| `dfaPrivate` | `k` | `$name` | `tsPrivate` |
| `dfaComment` | `n` | `/* … */` | skipped |
| `dfaMinus` | `o` | `-` | `h` / `p` / `i` |
| `dfaInteger` | `p` | `123`, `-5` | `tsInteger` |
| `dfaQuote` | `r` | `"…"` | `tsLiteral` |
| `dfaHexInteger` | `t` | `00CF0000h` | `tsHexInteger` |
| `dfaReal` | `{` | `0.005r` | `tsReal` |
| `dfaWildcard` | `x` | `'*` | `tsWildcard` |
| `dfaProtected` | `y` | `'$name` | `tsProtected` |
| `dfaEOF` | `.` | end of file | `tsEof` |
| `dfaError` | `?` | invalid char | throws `InvalidChar` |
| `dfaBack` | `!` | 1-char lookahead pushback | — |

Observations worth recording:

* **Numeric literals are suffixed**: hex ends in `h` (`00CF0000h`), reals end in `r`
  (`0.005r`). Verified in `src/win32/api/constants.l` and `examples/graphs/graphs.l`.
* **Non-ASCII characters are clamped**: `dfa.h:30-31` maps any `ch > 127` to `127`.
  Column 127 of the start row is `f` (identifier), so *every* character above U+007F is
  accepted as an identifier character. Identifiers can therefore contain arbitrary
  Unicode, but the DFA cannot distinguish between them.
* **The `back` hack** (`dfa.h:34-39`) is a hand-rolled 1-character pushback used, per the
  comment, "to deal with tailing dot after digit" (`5.` vs `5.0r`).
* `dfa.h:49` (the default constructor) assigns `state = dfaStart` — the *global* constant
  from `source.h`, not the `start` template parameter. `reset()` (`dfa.h:45`) correctly
  uses `start`. The template is therefore not reusable for a second DFA.

### 3.4 Tokenization

```cpp
LineInfo SourceReader :: read(TCHAR* token, size_t length, bool lowerCase = true)  // source.cpp:73
```

* Skip states (`whitespace`, `lineComment`, `comment`) restart the DFA and re-anchor the
  token start (`source.cpp:88-91`).
* `dfaError` → `throw InvalidChar(col, row, ch)` (`source.cpp:96-97`).
* For `dfaQuote` the token is **not copied** — `info.line` points directly into the line
  buffer (`source.cpp:101-102`), so string literals keep their case and their embedded
  `%n`/`%r` escapes; escape decoding happens later in `Quote<>` (`common/altstrings.h:341`)
  when the terminal is serialized (`derivation.cpp:32-44`).
* For every other token type the text is copied into the caller's buffer and
  **lowercased** (`source.cpp:106-109`) — `_tcslwr(token)`. This is where ELENA's
  case-insensitivity comes from. `IDENTIFIER_LEN` = 0x100 (`engine/elenaconst.h:19`);
  longer identifiers are silently truncated by `_tcsncpy` with no diagnostic.

`sg` (the syntax generator) reuses the exact same `SourceReader` with `lowerCase=false`
(`sg/sg.cpp:87`), which is why `syntax.txt` is written entirely in lowercase terminals.

---

## 4. Parser

### 4.1 Class

```cpp
class Parser {                                    // parser.h:34
   TCHAR       _buffer[IDENTIFIER_LEN + 1];
   ParserTable _table;
   bool derive(TerminalInfo&, ParserStack&, DerivationWriter*, bool& traceble);
public:
   void parse(TextReader* reader, DerivationWriter* writer, int tabSize);
   Parser(StreamReader* syntax);                  // parser.cpp:113 → _table.load(syntax)
};
```

It is a textbook **LL(1) table-driven predictive parser** with an explicit stack:

```cpp
void Parser :: parse(TextReader* reader, DerivationWriter* writer, int tabSize)  // parser.cpp:141
{
   SourceReader source(tabSize, reader);
   ParserStack  stack(tsEof);
   stack.push(nsStart);
   writer->writeSymbol(nsStart);
   do {
      terminal = getTerminalInfo(_table, source.read(_buffer, IDENTIFIER_LEN));
      bool traceble = false;
      if (!derive(terminal, stack, writer, traceble))
         throw SyntaxError(terminal.Col(), terminal.Row(), _buffer);
      if (traceble) writer->writeTerminal(terminal);
   } while (terminal != tsEof);
}
```

`derive` (`parser.cpp:118`) pops symbols until a terminal surfaces:

```cpp
Symbol current = (Symbol)stack.pop();
while (!test(current, mskTerminal)) {
   traceble = test(current, mskTraceble);
   if (current == nsNone) writer->writeSymbol(nsNone);           // close node
   else {
      if (traceble) { stack.push(nsNone); writer->writeSymbol(current); }   // open node
      if (!_table.read(current, terminal.symbol, stack)) return false;      // no production
      if (test(current, mskError))
         throw SyntaxError(..., getError(current));              // parser.cpp:134
   }
   current = (Symbol)stack.pop();
}
return (terminal == current);
```

### 4.2 Symbol encoding

`syntax.h:16-100`. Symbol IDs carry bit flags:

| Mask | Value | Meaning |
|---|---|---|
| `mskTerminal` | `0x10000` | this symbol is a terminal |
| `mskTraceble` | `0x00100` | emit a node for this non-terminal into the derivation tree |
| `mskError` | `0x00400` | this "non-terminal" is a synthetic error production |
| `mskAnySymbolMask` | `0x10500` | union of the above, used by `sg` when computing the next free id |

Fixed terminals are `tsEof`(0x10003) … `tsProtected`(0x1000D). Non-terminals that survive
into the tree all have the `0x100` bit set — e.g. `nsClass=0x114`, `nsMethod=0x136`,
`nsExpression=0x132`. Non-traceable helper non-terminals (`SYMBOLS`, `METHODS`,
`OPERATIONS`, `CLOSING_BRACE`, …) get ids assigned automatically by `sg` and never appear
in the tree — this is how the concrete syntax tree is pruned into a compact
abstract-ish tree at parse time, without a separate AST-building pass.

### 4.3 `ParserTable`

`parsertable.h:17`. Three containers (`parsertable.h:25-27`):

| Member | Type | Contents |
|---|---|---|
| `_symbols` | `SymbolMap` = `MemoryMap<const TCHAR*, int>` | terminal spelling → symbol id (`elena.h:214`) |
| `_syntax` | `SyntaxHash` = `MemoryHashTable<size_t,int,syntaxRule,0x100>` | rule id → RHS symbol sequence (build-time only) |
| `_table` | `TableHash` = `MemoryHashTable<size_t,int,tableRule,0x100>` | `(nonterminal, terminal)` → RHS, reversed onto the stack |

Key packing (`parsertable.cpp:19`, `:86`; powers from `engine/elenaconst.h:247-250`):

```
rule key   = (l_symbol << cnSyntaxPower /*8*/) + ordinal
table key  = (nonterminal << cnTablePower /*16*/) + terminal
```

Note that `tableKey` shifts the nonterminal left by 16 and adds the terminal, but terminals
themselves have the `0x10000` bit set — so the terminal contributes bit 16 into the
nonterminal field. It works only because it is consistently wrong on both sides
(`parsertable.cpp:100` vs `:174`/`:186`).

`ParserTable::read` (`parsertable.cpp:95`) is the runtime hot path: `nsEps` is a no-op
(epsilon move), otherwise the RHS is pushed onto the derivation stack in reverse via the
recursive `add2stack` helper (`parsertable.cpp:31`). Returning `false` means "no table
entry" → syntax error.

`ParserTable::generate` (`parsertable.cpp:113`) is the classic FIRST/FOLLOW fixed-point
LL(1) construction; it returns `false` on any conflict, which `sg` reports as
`"error:syntax ambigous"` (`sg/sg.cpp:126`). **`generate` is never called by `elc`** — the
compiler only ever calls `load` (`parsertable.cpp:199`), which reads `_symbols` and
`_table` from `syntax.dat`.

### 4.4 Relationship to `sg` and `dat/sg/syntax.txt`

```
dat/sg/syntax.txt  ──[ sg.exe ]──▶  dat/sg/syntax.dat  ──(copy)──▶  bin/syntax.dat  ──▶ elc
```

`elenasrc/sg/sg.cpp` is a ~150-line tool that:

1. lexes `syntax.txt` with the **same `SourceReader`** (`sg.cpp:76`, `tabSize=4`,
   `lowerCase=false`);
2. handles `__define <NAME> <number>` to bind explicit ids (`sg.cpp:91-98`);
3. treats a symbol starting with an uppercase `A`–`Z` as a non-terminal, everything else
   as a terminal (`sg.cpp:51-52`, `id |= mskTerminal`);
4. auto-assigns `last_id + 1` to any symbol not `__define`d (`sg.cpp:116`), where
   `last_id` is tracked as `id & ~mskAnySymbolMask` (`sg.cpp:56`);
5. accumulates `->` / `|` separated productions into `registerRule` (`sg.cpp:99-113`);
6. calls `generate()` and `save()` to `<input>.dat` (`sg.cpp:125-140`).

**`bin/syntax.dat` is not present in this tree.** `bin/` contains only `elc.cfg` and
`templates/`. Without regenerating it from `dat/sg/syntax.txt`, `elc.exe` fails at
`elc.cpp:301-302` with `errInvalidFile`. This is a build-reproducibility landmine.

Grammar highlights (`dat/sg/syntax.txt`):

| Production | Line | Notes |
|---|---|---|
| `START` | `:75` | top level: `#class`, `#symbol`, `#static`, `#define`, `eof` |
| `CLASS_BODY` | `:110` | `{ FIELDS ROLES METHODS REDIRECTION }` or `( BASE_CLASS ) { … }` |
| `METHOD` | `:154` | identifier / private / protected / 18 operator forms / `=>` / `?` / `!` |
| `STATEMENTS` | `:196` | expression, `#var`, `#if`, `#loop`, `#shift`, `^` (return), `]` |
| `OBJECT` | `:253` | identifier, reference, private, literal, integer, hex, real, inline symbol `{…}`, `( … )`, `#group`, `#cast`, `#type`, `#annex`, action `=> …` |
| `OPERATION` | `:326` | 5 precedence levels L0..L4 |
| `L4_OPERATION` | `:341` | `== != > < >= <= << >> ? !` |
| `L3_OPERATION` | `:360` | `+ - += -=` |
| `L2_OPERATION` | `:373` | `* / *= /=` |
| `L1_OPERATION` | `:386` | keyword message send `identifier [: L0_EXPRESSION]`, `@` |
| `L0_OPERATION` | `:402` | `::` (annex/cast operator) |
| error productions | `:500-610` | one production per plausible lookahead |

`__define`d ids map directly onto the `Symbol` enum: `CASE_EXPRESSION 354 = 0x162 =
nsControl`, `LOOP 336 = 0x150 = nsLoop`, `ROLE_NAME 342 = 0x156 = nsShiftParam`,
`BLOCK_END 340 = 0x154 = nsCodeEnd`, etc. **The two files must be kept in sync by hand.**
`nsCollectionSymbol` (0x11A) and `nsObjectExpression` (0x11C) are declared in `syntax.h`
but have no counterpart in `syntax.txt` — dead enum entries.

**Latent grammar defect:** `syntax.txt:212` and `:490` reference `DOT_EXPECTED`, which is
never `__define`d. `sg` therefore auto-assigns it an ordinary id *without* the
`mskError` (0x400) bit and *without* any production. `_table.read` then finds no entry and
`derive` returns `false`, so what should be error 005 (`'.' expected`) is always reported
as the generic error 004 (`Invalid syntax near …`). Only `FIELD_DOT_EXPECTED` (0x401,
`syntax.txt:62`) actually produces error 005.

### 4.5 Derivation tree format

`DerivationWriter` (`derivation.cpp:19`) emits a flat DWORD stream:

* non-terminal open: one DWORD = symbol id;
* node close: one DWORD = `nsNone` (0);
* terminal: `symbol, disp, row, col, length` (5 DWORDs) followed by a NUL-terminated
  literal (`derivation.cpp:24-46`). Quoted literals are run through `Quote<>` to decode
  `%n`, `%r`, `%t`, `%a`, `%b`, `%%` and `%<decimal>` escapes, with a `LocalString<0x100>`
  fast path.

`DerivationReader` provides the cursor API (`derivation.h:68`):
`Node::firstChild()`, `Node::nextNode()`, `Node::select(Symbol)`, `Node::Terminal()`.
`readNextNode` (`derivation.cpp:76`) walks the stream counting nesting levels; `select`
(`derivation.cpp:102`) does a level-1-only scan for a given symbol. There is **no random
access, no parent pointer, and no way to modify the tree** — which structurally forbids
any AST-rewriting optimization pass.

---

## 5. Semantic analysis & code generation in `compiler.cpp`

This is the heart of the frontend: a single recursive-descent walk over the derivation
tree that emits `CommandTape` byte code as it goes. There is **no separate semantic pass,
no type checker, no IR, and no optimizer.**

### 5.1 The mode bit-mask

Threading through nearly every `compile*` function is an `int mode`
(`compiler.cpp:18-23`):

| Flag | Value | Meaning |
|---|---|---|
| `CTRL_BRANCHING` | 0x0001 | the value is consumed by a branch — failure jumps to the branch label rather than the procedure failure label |
| `CTRL_PROPERTY` | 0x0002 | this object is the *target* of an `L0_OPERATION` (`::`), i.e. a "property" call |
| `CTRL_ROOT` | 0x0004 | top-level statement expression (allows naming an inline class after the enclosing symbol) |
| `CTRL_ACTION` | 0x0008 | `=> […]` action shorthand |
| `CTRL_INHERITED` | 0x0010 | inline class has an explicit parent, so `new` must be sent |
| `CTRL_SINGLE` | 0x0020 | expression consists of a single object with no operations |

`VSELF_PTR_OFFSET = -1` (`compiler.cpp:25`) is the frame-relative slot of the "virtual
self" pointer inherited from the caller's frame.

### 5.2 Scope hierarchy

All scopes derive from `Compiler::Scope` (`compiler.h:152`), which chains via `parent` and
resolves names by delegation (`Scope::mapObject`, `compiler.h:175`). `getScope(level)`
(`compiler.h:183`) walks up looking for `slClass` / `slSymbol` / `slMethod` / `slCode`.

| Scope | Declared | Constructed | Owns | `mapObject` resolves |
|---|---|---|---|---|
| `ModuleScope` | `compiler.h:36` | `compiler.cpp:101` | `_Module`, debug module, `masks`, `forwards`, `symbolHints`, `forwardsUnresolved` | references (`tsReference`) and top-level symbols (`compiler.cpp:122`) |
| `SourceScope` | `compiler.h:204` | `compiler.cpp:225,231` | a `CommandTape` + the target `reference` | — |
| `ClassScope` | `compiler.h:214` | `compiler.cpp:264` | `ClassInfo` (header/flags/fields/roles/methods) | `self` → `otVSelf`, `super` → `otSuper`, fields → `otField` (`compiler.cpp:274`) |
| `RoleScope` | `compiler.h:246` | `compiler.cpp:356` | a role VMT; `elRoleVMT`, `roleRef = owner` | delegates entirely to the owning class (`compiler.cpp:365`) |
| `SymbolScope` | `compiler.h:254` | `compiler.cpp:239` | optional `parameter` name | the symbol parameter → `otSymbolParam` (`compiler.cpp:254`) |
| `MethodScope` | `compiler.h:274` | `compiler.cpp:372` | `messageRef`, `param`, `isDefaultMethod` | `$self` → `otSelf` (`compiler.cpp:392`) |
| `CodeScope` | `compiler.h:297` | `compiler.cpp:402,410,418` | `LocalMap locals`, `level`, `methodRef`, `tape*` | locals → `otLocal` (`compiler.cpp:426`) |
| `InlineClassScope` | `compiler.h:326` | `compiler.cpp:437` | `outers` map (closure capture) | promotes captured outer variables to fields (`compiler.cpp:444`) |

`ClassScope::mapObject` uses the *sentinel* `-1` for "not found" because
`ClassInfo::fields` is a `FieldMap` constructed with default `-1` (`engine/elena.h:126`),
whereas `CodeScope::mapObject` uses `0` as "not found" — two different conventions
(`compiler.cpp:283-284` vs `compiler.cpp:428-429`).

#### Closure capture (`InlineClassScope::mapObject`, `compiler.cpp:444`)

This is the one genuinely clever piece of scoping. When an identifier inside an inline
class (`{ … }`) resolves in an *enclosing* scope to a `otLocal`, `otField`,
`otSymbolParam`, `otOuter`, `otVSelf` or `otSuper`, the compiler:

1. allocates a new field slot `info.fields.Count() + 1`,
2. records the mapping in `outers` and in `info.fields`,
3. returns `ObjectInfo(otOuter, slot)`.

At instantiation time `compileSymbolExpression` (`compiler.cpp:867-876`) walks `outers`
and stores each captured object into the freshly-created instance:

```cpp
_coder.pushObject(*ownerScope.tape, info);
_coder.moveToObjectPtr(*ownerScope.tape, ObjectInfo(otLocal, 1), (((*outer_it).reference - 1) << 2));
_coder.endStatement(*ownerScope.tape);
```

The `<< 2` is a hard-coded 4-byte field stride.

### 5.3 `ObjectInfo` — the frontend's only "type" information

`engine/bccompiler.h:18` defines `ObjectType`:
`otUnknown, otProperty, otSymbol, otConstant, otVSelf, otSelf, otSuper, otField, otLocal,
otLiteral, otInteger, otReal, otSymbolParam, otOuter, otRole, otExpression`.

This is a *storage class*, not a data type. The compiler never reasons about the class of
a value; every operation is a dynamic message send. `ObjectInfo::reference` doubles as a
reference id, a stack slot, a field index and a role index depending on `type`
(`engine/bccompiler.h:39`). `ByteCodeCompiler::pushObject`
(`engine/bccompiler.cpp:338`) switches on `ObjectType` to pick the push opcode.

### 5.4 Lowering, construct by construct

#### `#define` (namespace shortcut / forward) — `compileDirectives`, `compiler.cpp:1623`

Two forms, distinguished by whether the shortcut is a `tsWildcard`:

* `#define basic'* = std'basic'*.` → `ModuleScope::defineMask` (`compiler.h:81`), adding
  to `masks`. `matchMask` (`compiler.cpp:133`) later rewrites a reference's namespace when
  it is resolved.
* `#define 'entry = sys'templates'simple.` → `ModuleScope::defineForward`
  (`compiler.h:85`), mapping a name to a `ref_t` in `forwards`.
  `compileForwardHints` (`compiler.cpp:211`) processes `[const]`.

Directives must precede all declarations (`compileDirectives` returns on the first
non-`nsShortcut` node, `compiler.cpp:1647-1649`).

#### `#class` — `compileClassDeclaration`, `compiler.cpp:1549`

```
compileHints(hints)                     // [dbg:int] etc.       compiler.cpp:310
newClass(tape)                          // blBegin/bltClass
compileParentDeclaration(...)           //                       compiler.cpp:1318
compileFieldDeclarations(member, scope) //                       compiler.cpp:1509
compileRoleDeclarations(member, scope, roles) //                 compiler.cpp:1445
[ elStateless if no roles / no fields / not a structure ]        compiler.cpp:1569-1576
compileSymbolCode(scope)                // implicit constructor symbol  compiler.cpp:1349
compileVMT(member, scope)               //                       compiler.cpp:1385
endClass(tape); scope.save(_coder)      //                       compiler.h:232
```

* **Parent resolution** (`compiler.cpp:1318`): default parent is
  `$elena'object` (`SUPER_CLASS`, set in `ClassScope` ctor `compiler.cpp:267`).
  `$elena'object` itself is special-cased to have `parentRef = 0` (`compiler.cpp:1321-1326`).
* **Inheritance** (`inheritClass`, `compiler.cpp:603`) is *copy-based*: the parent's
  `ClassInfo` is loaded from its `mskMetaDataRef` section and copied wholesale into the
  child. When the parent lives in another module, all method ids and role references are
  re-mapped through `importMessage`/`importReference` (`compiler.cpp:34`, `:44`). All
  inherited methods are marked `false` ("inherited") so that overriding them does not grow
  the VMT (`compiler.cpp:1265-1270`).
* **Statelessness**: a class with no fields, no roles and no structure flag gets
  `elStateless` and is registered as a *constant* via `defineConstant`
  (`compiler.cpp:1572-1574`), which means references to it are emitted as `mskConstantRef`
  pushes rather than symbol calls — a singleton optimization.
* **Implicit symbol** (`compileSymbolCode`, `compiler.cpp:1349`): every `#class Foo`
  also produces a *symbol* named `Foo` whose body is "create an instance and send `new`":

  ```cpp
  _coder.newSymbol(symbolScope.tape);
  if (stateless) _coder.pushObject(tape, ObjectInfo(otConstant, scope.reference));
  else           _coder.pushNewObject(tape, size, scope.reference);
  _coder.pushObject(tape, ObjectInfo(otLocal, VSELF_PTR_OFFSET));   // ctor argument
  _coder.newDummyBreakpoint(tape, dsVirtualEnd);
  _coder.sendMessage(tape, NEW_MESSAGE_ID, false);
  _coder.endSymbol(tape);
  ```

  This is why `Foo::x` in ELENA reads as "instantiate `Foo` with argument `x`".

#### `#field` — `compileFieldDeclarations`, `compiler.cpp:1509`

Three mutually exclusive forms:

| Syntax | Effect | Line |
|---|---|---|
| `#field(4).` | sets `elStructureRole`, `info.size += 4` (raw bytes) | `:1515-1523` |
| `#field.` (no name) | sets `elDynamicRole \| elStructureRole` (variable-length binary object) | `:1525-1531` |
| `#field theName.` | ordinary reference field, index = `fields.Count()+1` | `:1533-1541` |

Mixing named fields with structure/dynamic fields raises `errIllegalField`
(`:1517`, `:1527`, `:1535`). Structure sizes are raw byte counts with no alignment logic.

#### `#role` — `compileRoleDeclarations`, `compiler.cpp:1445`

Roles are ELENA 1.5's *state machine* mechanism: a class can carry several named VMTs and
`#shift` between them at run time.

* Each role becomes a separate `RoleScope` with a **generated** reference name
  `<module>'#role<hex>` (`ROLE_POSTFIX`, `engine/elenaconst.h:154`;
  `findUninqueName`, `compiler.cpp:51`).
* The owning class gets `elVMTWithRoles`, and a role table section is emitted
  (`newRoleTable` / `newRole` / `endRoleTable`, `compiler.cpp:1492-1505`), referenced by
  `info.header.roleRef`.
* Roles are *inherited* by `inheritRoles` (`compiler.cpp:577`), which clones each parent
  role into a fresh `RoleScope` of the child. Overriding a role by name clears the cloned
  tape (`compiler.cpp:1461`).
* `compileIdleMethod` (`compiler.cpp:1250`) reserves VMT entry 0 of every role VMT with
  `DUMMY_MESSAGE_ID`, "due to `iocall(1)` command" (`compiler.cpp:1387`).
* `#shift Name.` → `compileShift` (`compiler.cpp:1066`), which resolves the role *index*
  by linear scan (`ClassScope::mapRole`, `compiler.cpp:291`) and emits `bcShift index`.
* `#shift.` (no name) → `compileUnShift` (`compiler.cpp:1074`), valid only inside a role
  (`errInvalidShift`).

#### `#symbol` / `#static` — `compileSymbolDeclaration`, `compiler.cpp:1589`

```cpp
if (isStatic) _coder.newStaticSymbol(tape, reference, nilRef);   // memoized
else          _coder.newSymbol(tape);
...
if (isStatic) _coder.endStaticSymbol(tape, reference);
else          _coder.endSymbol(tape);
```

`#static` symbols are lazily-initialized singletons: `newStaticSymbol`
(`engine/bccompiler.cpp:189`) emits `rpushptr static / rreturnif nil / pop / prep`, and
`endStaticSymbol` (`:593`) stores the computed value back with `rmoveptr`.

Symbols may have a parameter (`#symbol ListPrinter : aList = …`); the parameter is
registered on the `SymbolScope` (`compiler.cpp:1679-1681`) and resolves to
`otSymbolParam`, which pushes the caller's frame slot `-1`
(`engine/bccompiler.cpp:365-368`).

#### `#method` — `compileVMT` / `compileMethod`, `compiler.cpp:1385` / `:1260`

* The message id is computed by `mapMessage` (`compiler.cpp:1409`, see §6).
* Hint `[def]` (`HINT_DEFAULT`) re-maps the message into the `$elena` namespace via
  `mapDefaultMessage` (`compiler.cpp:1413-1415`, `:533`), which is how library-internal
  methods like `$getClassName` avoid colliding across modules.
* Duplicate detection: `scope.info.methods.exist(messageRef, true)` — the `true` means
  "already declared *in this class*" as opposed to inherited (`compiler.cpp:1421`).
* VMT growth: `classSize += sizeof(VMTEntry)` (8 bytes) only for genuinely new methods
  (`compiler.cpp:1267`).
* Prologue (`compiler.cpp:1275-1285`):

  ```cpp
  if (!emptystr(scope.param)) {
     int level = codeScope.newLocal(scope.param);
     _coder.newMethod(*tape, scope.messageRef, /*withParam*/true);
     _coder.newLocalInfo(*tape, SELF_VAR, -3);      // debug: self at frame slot -3
     _coder.newLocalInfo(*tape, scope.param, level);
  }
  else { _coder.newMethod(*tape, scope.messageRef, false);
         _coder.newLocalInfo(*tape, SELF_VAR, -3); }
  ```

  The `-3` is a magic frame offset shared with the JIT and the debugger.
* Two body forms: `[ statements ]` (`nsSubCode`) or `= expression.`; the latter is
  compiled by passing the method node itself to `compileCode`
  (`compiler.cpp:1287-1293`), which then sees the `nsRetStatement` child.

#### `#annex` (extension / any-message handler) — `compileExtend`, `compiler.cpp:1019` and `compileVMT` case `nsExtend`, `compiler.cpp:1427`

Two syntactic positions, one mechanism:

* Inside a class body, `#annex Expr.` (`REDIRECTION`, `syntax.txt:220`) adds an
  **any-message handler**: `elVMTAnyHandler` is set and a method with message id `0` is
  emitted via `compileRedirectMethod` (`compiler.cpp:1298`). If the parent already had
  one, `redirectMessageToParent` chains to it (`compiler.cpp:1311-1313`).
* As an expression, `#annex(obj) { methods }` (`EXTENSION_EXPRESSION`, `syntax.txt:415`)
  builds an anonymous inline class that both redirects unknown messages to `obj` and
  defines new methods — this is ELENA's extension-method mechanism.

`compileRedirectMethod` emits:

```
newRedirectMethod(messageRef)   → blBegin/bltMethod + bcPrepRedir
pushObject(otVSelf)
compileExpression(node, …, CTRL_ROOT)
redirectMessage()               → bcRedirect
[ redirectMessageToParent(parentRef) if overridden ]
endRedirectMethod()             → bcExitRedir
```

#### `#group` / `#cast` — `compileGroup`, `compiler.cpp:1012`

Both lower to `compileCollection` with a different VMT:

```cpp
compileCollection(objectNode.firstChild(), scope, mode,
   isBroadcasting ? module->mapReference(CAST_CLASS)      // $elena'$cast
                  : module->mapReference(GROUP_CLASS));   // $elena'$group
```

`compileCollection` (`compiler.cpp:995`) creates an array-like object of `countSymbols(node)`
slots and fills them:

```cpp
_coder.pushNewObject(*scope.tape, counter, vmtReference);
while (node != nsNone) {
   compileExpression(node, scope, mode);
   _coder.moveToObjectPtr(*scope.tape, ObjectInfo(otLocal, 1), (index << 2));
   _coder.endStatement(*scope.tape);
   node = node.nextNode(); index++;
}
```

A `#group` dispatches a message to the *first* member that handles it; a `#cast`
broadcasts to all. A bare parenthesized comma list `( a, b, c )` in object position is
detected by `isCollection` (`compiler.cpp:29`) and compiled with the project's
`arrayclass` (`compiler.cpp:990-993`, `opArrayClass` — `std'basic'array` by default).

#### `#type` — `compileType`, `compiler.cpp:1044`

```cpp
_coder.pushNewObject(*scope.tape, 4 | gcBinary,
   scope.moduleScope->module->mapReference(TYPEINSTANCE_CLASS), type.reference);
```

Creates a 4-byte **binary** object (`gcBinary` = 0x80000000, `elenaconst.h:278`) of class
`$elena'$typeinstance` whose payload is the target class's VMT pointer. `cast` and `group`
are recognised as pseudo-references (`compiler.cpp:1051-1056`). The `4` is a hard-coded
pointer size.

#### `::` (annex/cast operator, `L0_OPERATION`) — `compileExpression`, `compiler.cpp:1142`

```cpp
if (member==nsObject) {
   if (member.nextNode() == nsL0Operation) {
      recordStep(scope, member.Terminal(), dsVirtualStep);
      compileExpression(member.nextNode(), scope, mode & ~CTRL_ROOT);   // RHS first
      compileObject(member, scope, mode | CTRL_PROPERTY);               // then LHS as "property"
   }
   else objectInfo = compileObject(member, scope, mode);
}
```

Note the **reversed evaluation order**: `A::B` compiles `B` first, then `A` with
`CTRL_PROPERTY`. In `compileTerminal` (`compiler.cpp:810-818`), a `CTRL_PROPERTY` object
must be an `otSymbol` (else `errInvalidProperty`) and is emitted as `pushProperty`
(`bcRCall` *without* pushing `nil` first), so the already-computed RHS becomes the
symbol's argument. This is how `prop'TextWriter::anOutput` reads as "apply the
`TextWriter` adapter to `anOutput`".

#### Inline objects `{ … }` — `compileSymbolExpression`, `compiler.cpp:903` and `:832`

The two overloads split "decide the reference & parent" from "emit the class".

* Naming (`mapSymbolExpression`, `compiler.cpp:559`): if `CTRL_ROOT | CTRL_SINGLE` and we
  are directly inside a `#symbol`, the inline class **reuses the symbol's own reference**;
  otherwise it gets a generated name `<module>'#inline<hex>` (`INLINE_POSTFIX`).
* Three shapes handled at `compiler.cpp:912-937`:
  `CTRL_ACTION` (`=> [ … ]`), inherited (`Parent{ … }`), and plain (`{ … }`).
* Emission (`compiler.cpp:832`): compile the VMT, then decide between the **stateless**
  path (no fields, no roles → mark `elStateless`, register as a constant, push it directly
  — `compiler.cpp:852-861`) and the **stateful** path (`pushNewObject` + store captured
  outers — `compiler.cpp:862-878`).
* `needNew` (`compiler.cpp:860`, `:877`) decides whether a `new` message is sent, with a
  `swap` when the object is being used as a property (`compiler.cpp:887-890`).
* Finally the inline class's metadata and byte code are saved *immediately*, inline, in
  the middle of compiling the enclosing expression (`compiler.cpp:895-900`).

`=> [ … ]` action shorthand is compiled by `compileVMT`'s special case
(`compiler.cpp:1393-1401`): it synthesises a single method with `PROCEED_MESSAGE_ID`,
optionally taking the preceding identifier as its parameter name. This is ELENA's lambda.

#### `#var` — `compileVariable`, `compiler.cpp:1192`

```cpp
if (!scope.locals.exist(node.Terminal())) {
   int level = scope.newLocal(node.Terminal());       // compiler.h:304
   _coder.newLocal(*scope.tape);                      // bcIPush 0 + allocstack
   _coder.newLocalInfo(*scope.tape, node.Terminal(), level);
   compileAssignment(node.firstChild(), scope, node.Terminal());
}
else scope.raiseError(errDuplicatedLocal, node.Terminal());
```

Slot allocation is `level + locals.Count() + 1` (`compiler.h:304`) where a nested
`CodeScope` starts at `parent->level + parent->locals.Count()` (`compiler.cpp:422`).
**Slots are never reused** across sibling blocks.

#### `:=` — `compileAssignment`, `compiler.cpp:1173`

```cpp
if (object.type == otLocal)      _coder.moveObject(tape, object);            // bcIFMove
else if (object.type == otField) { _coder.moveObject(tape, object);          // bcISMove
   _coder.callEmbedded(tape, module->mapReference(ASSIGN_FUNCTION), false, -1); }
```

Assigning to a **field** additionally calls the runtime helper `$package'elena'34`
(`ASSIGN_FUNCTION`, `engine/elenaconst.h:67`) — the generational GC write barrier
("if the object is temporal recreate it, and check if the yg object refers to mg one",
`compiler.cpp:1183`). Note the `paramCount = -1`, which suppresses the stack cleanup in
`callEmbedded` (`engine/bccompiler.cpp:470`).

#### `#if` — `compileControlExpression` / `compileControlChain`, `compiler.cpp:1121` / `:736`

```cpp
_coder.newBranchStatement(tape);                       // blBegin/bltBranch + bcIOPush
while (node != nsNone) {
   if (node is an L1..L4 operation) compileMessage(node, scope, object, mode|CTRL_BRANCHING);
   else if (node == nsAlternative) { newBreakpoint(dsVirtualEnd); newAlternativeBranchStatement(); }
   else if (node == nsSubCode)     { newSubCode(tape); CodeScope sub(&scope); compileCode(node, sub); }
   node = node.nextNode();
}
_coder.endBranchStatement(tape, false);
```

Branching in ELENA is **exception-style, not comparison-style**: a message send in
`CTRL_BRANCHING` mode gets a second, "on-failure" jump target
(`engine/bccompiler.cpp:703-717` pushes the call site onto `jumpsToFail`). A boolean
`false` is expressed by the callee taking the failure path. `||` (`nsAlternative`) chains
alternative branches.

#### `#loop` — `compileLoop`, `compiler.cpp:1130`

```cpp
_coder.newLoop(*scope.tape);
compileExpression(node, scope, mode | CTRL_BRANCHING);
DNode code = node.select(nsSubCode);
compileCode(code, scope);
_coder.newBreakpoint(*scope.tape, 0, 0, 0, dsVirtualEnd);
_coder.endLoop(*scope.tape);
```

`endLoop` (`engine/bccompiler.cpp:539`) emits the back-jump; the loop label position is
recorded by `saveProcedure` when it sees `blBegin/bltLoop` (`engine/bccompiler.cpp:649-653`).
Note the loop body shares the *enclosing* `CodeScope` (no new scope) — locals declared in
a loop body leak into the enclosing block.

#### `^` (return) — `compileCode` case `nsRetStatement`, `compiler.cpp:1226-1230`

```cpp
compileExpression(statement.firstChild(), scope, CTRL_ROOT);
_coder.newBreakpoint(*scope.tape, 0, 0, 0, dsVirtualEnd);
_coder.returnObject(*scope.tape);                    // bcSReturn
```

#### `#external` / `#inline` — `compileExternalFunction` / `compileEmbeddedExpression`, `compiler.cpp:940` / `:968`

Both resolve the callee as `$package'<name>` (`PACKAGE_MODULE`,
`engine/elenaconst.h:24`), i.e. a **symbol exported from a hand-written `.bin` primitive
module** (`elena.bin`, `win32.bin`, `standard.bin`, `extended.bin`, `winsock.bin` — see
`bin/elc.cfg` `[primitives]`).

| | `#external` | `#inline` |
|---|---|---|
| Emits | `bcRCallExt` (`compiler.cpp:956`) | `bcRCallEmb` (`compiler.cpp:984`) |
| Stack cleanup | compiler emits `paramCount-1` `endStatement`s (`compiler.cpp:961-965`) | callee cleans up; `paramCount-1` passed to the coder |
| Limit | `> 127` args → `errExtTooManyParameters` (`compiler.cpp:953`) | none checked |

The `> 127` limit exists because the JIT encodes the arg count in one byte.
`errTooManyParameters` (error 113) is declared in `errors.h:36` but **never raised**.

#### Message sends — `compileMessage`, `compiler.cpp:676`

```cpp
DNode operand = node.firstChild();
if (operand != nsNone) compileExpression(operand, scope, mode);
else _coder.pushObject(*scope.tape, scope.moduleScope->mapReference(NIL_CLASS));  // implicit nil arg

recordStep(scope, node.Terminal(), dsProcedureStep);

ref_t messageRef = compstr(node.Terminal(), REDIRECT_MESSAGE)   // "$invoke"
   ? scope.methodRef                                            // re-send current message
   : mapMessage(node.Terminal(), scope.moduleScope);

_coder.newDummyBreakpoint(*scope.tape, dsVirtualEnd);
if (object.type == otSuper)
   _coder.sendMessage(*scope.tape, messageRef, object.reference, test(mode, CTRL_BRANCHING));
else
   _coder.sendMessage(*scope.tape, messageRef, test(mode, CTRL_BRANCHING));
```

Key facts: **every message takes exactly one argument** (implicit `nil` if omitted);
`super Foo` becomes a statically-bound `bcIRCall*` with the parent VMT as an extra
parameter (`engine/bccompiler.cpp:439`); `$invoke` re-sends the enclosing method's own
message id, erroring with `errInvalidRedirectMessage` outside a method.

`compileOperations` (`compiler.cpp:705`) chains messages left-to-right; only the *first*
message in a chain can target `super` (`compiler.cpp:721`, `currentObject.type = otExpression`).

#### Literals — `compileTerminal`, `compiler.cpp:765`

| Terminal | Handling | Line |
|---|---|---|
| `tsLiteral` | `otLiteral`, `module->mapConstant(text)` → later `mskLiteralRef` | `:773-775` |
| `tsInteger` | `otInteger`, constant is the **decimal spelling** | `:776-782` |
| `tsHexInteger` | `_tcstoul(…,16)`, converted back to a decimal string, then `mapConstant` | `:783-795` |
| `tsReal` | trailing `r` stripped, `_tcstod`, stored as a string | `:796-803` |
| other | `scope.mapObject(terminal)` | `:804-806` |

**Bug:** all three numeric paths test `if (errno == ERANGE)` *without* clearing `errno`
first, and `_ttoi`/`_tcstod` are not guaranteed to set it — overflow detection is
effectively dead code, and a stale `errno` from an unrelated call can produce a spurious
`errInvalidIntNumber`. Also, integers are keyed by spelling, so `5` and `05` become two
distinct module constants.

### 5.5 Debug information

`recordStep` (`compiler.h:360`) emits a `bdBreakpoint` + `bdBreakCoord` pair carrying
`(row, disp, length, stepType)`. Step kinds are in `engine/elenaconst.h:203-223`
(`dsStep`, `dsAtomicStep`, `dsProcedureStep`, `dsEOP`, `dsVirtualStep`, `dsVirtualEnd`).
`newDummyBreakpoint(dsVirtualEnd)` is emitted before *every* `sendMessage`
(`compiler.cpp:698`, `:882`, `:1375`) because — per the comment — the byte-code writer
requires a virtual end marker to cope with branching. Debug records are only materialised
when a debug module exists (`engine/bccompiler.cpp:678-690`), but the *byte-code IR always
carries them*, which perturbs the tape shape regardless of `-d`.

---

## 6. Message / reference encoding

> **Important:** ELENA 1.5 does **not** pack verb + signature + parameter count into a
> message id. (That scheme arrived in the 2.x rewrite.) Here a message is a *string*, and
> a message id is an index into a **per-module string table**.

### 6.1 Message ids

`Compiler::mapMessage` (`compiler.cpp:540`) is the single entry point:

```cpp
if (terminal==tsPrivate) {                                  // $foo
   LocalPrivateMessage name(moduleScope->module->Name(), terminal);
   return moduleScope->module->mapMessage(name);
}
else if (terminal==tsProtected) {                           // '$foo
   return mapProtectedMessage(terminal, moduleScope);
}
else {
   int index = _predefined.get(terminal);
   if (index != 0) return index;                            // predefined
   else return moduleScope->module->mapMessage(terminal);   // public
}
```

| Kind | Lexical form | Encoded as | Line |
|---|---|---|---|
| Predefined | `new`, `proceed`, `+`, `==`, `?`, … | fixed id with bit `PREDEFINED_REF` (0x80000000) | `compiler.cpp:489-518`, ids `elenaconst.h:112-140` |
| Public | `printOn`, `run`, `get` | `Module::mapMessage(text)` → sequential id 1,2,3,… | `engine/module.cpp:77` |
| Private | `$getClassName` | `<module,≤2 levels>'$getClassName`, then mapped | `compiler.cpp:542-546`, `common/altstrings.h:435` |
| Protected | `'$asHDC` | namespace-mask-qualified, then mapped | `compiler.cpp:520-531` |
| Default (`[def]`) | `#method[def] $getClassName` | `$elena'$getClassName` | `compiler.cpp:533-538` |
| Any-handler | `#annex` | literal id `0` | `compiler.cpp:1038`, `:1434` |
| Placeholder | reserved role VMT slot 0 | `DUMMY_MESSAGE_ID` = 0x80000002 | `compiler.cpp:1252-1256` |
| Redirect | `$invoke` | the enclosing method's own `messageRef` | `compiler.cpp:690-694` |

`PrivateMessageTemplate` (`common/altstrings.h:443`) truncates the module name to at most
two namespace levels — the comment explains: `win32'api'$asHDC` rather than
`win32'api'factories'$ashdc`. So private messages are private *to a two-level namespace*,
not to a class or a file.

Constraints and cross-module resolution:

* `MAXIMAL_MESSAGE_REF` = 0x0000FFFF (`elenaconst.h:105`) — nominally 65535 distinct
  messages, though `Module::mapMessage` never enforces it.
* `PREDEFINED_REF` = 0x80000000 distinguishes fixed ids from table indices; `sendMessage`
  branches on it to pick `bcIOCall0` for `new` vs `bcIOCall1` for everything else
  (`engine/bccompiler.cpp:419-437`).
* Because ids are per-module, a message id is **meaningless across modules**. Two
  translations exist:
  * compile time: `importMessage` (`compiler.cpp:34`) — resolve to string in the exporter,
    re-map in the importer, when copying an inherited VMT.
  * link time: `ReferenceLoader::resolveMessageID` (`engine/jitlinker.cpp:111`) — resolve
    to string and map into a single global id space.

### 6.2 Reference (symbol) ids

`Module::mapReference` (`engine/module.cpp:60`) assigns sequential ids from a per-module
reference table, throwing `InternalError(errReferenceOverflow)` above `~mskAnyRef`
(2^24 − 1). The **high byte is a section-kind tag**, `ReferenceType`
(`engine/elenaconst.h:169-200`):

| Mask | Value | Section |
|---|---|---|
| `mskSymbolRef` | 0x42000000 | symbol code |
| `mskClassRef` | 0x41000000 | class method code |
| `mskVMTRef` | 0x21000000 | class VMT |
| `mskMetaDataRef` | 0x24000000 | serialized `ClassInfo` |
| `mskConstantRef` | 0x08000000 | stateless-class / `nil` constant |
| `mskLiteralRef` | 0x09000000 | string constant |
| `mskInt32Ref` | 0x0A000000 | integer constant |
| `mskRealRef` | 0x0C000000 | real constant |
| `mskStaticConstRef` | 0x01000000 | `#static` cell |
| `mskNativeCodeRef` | 0x48000000 | `.bin` primitive code |
| `mskRelativeRef` | 0x80000000 | relative fixup |
| `mskAnyRef` | 0xFF000000 | the tag field itself |

So a reference is `(tag << 24) | index` — **24 bits of index, hard-capped at ~16.7M
references per module**, and, critically, the tag steals the sign bit
(`mskRelativeRef` = 0x80000000), which forces `ref_t` (`= size_t`,
`common/common.h:21`) to be treated as unsigned 32-bit everywhere.

### 6.3 Reference *name* resolution

`ModuleScope::mapReference` (`compiler.cpp:147`):

1. Weak references (leading `'`, `isWeakReference`, `engine/elena.h:220`) are mapped
   as-is; the linker resolves them later through `Project::resolveForward`
   (`project.cpp:136`).
2. Strong references first try the namespace masks: `matchMask` (`compiler.cpp:133`)
   linearly scans `masks` comparing the reference's namespace to each `#define` wildcard,
   and rewrites the namespace if it matches.
3. `getObjectInfo` (`compiler.h:131`) then returns `otConstant` if the reference was
   registered as a constant (stateless class / `[const]` hint), else `otSymbol`.

`ModuleScope::mapSymbol` (`compiler.h:117`) handles identifiers: check `forwards`, else
qualify with the current module name.

Module file naming (`Project::nameToPath`, `project.cpp:55`): apostrophes become path
separators after the package prefix is stripped (`getName`, `project.cpp:44`), so
`std'basic'memory` ↔ `<output>\basic\memory.nl` when `package=std`, and reverse lookup uses
the reference's namespace (`Project::resolveModule`, `project.cpp:192`). `$`-prefixed
names have the `$` stripped, which is how `$elena` maps to `elena.nl`.

---

## 7. Project (.prj) and config (.cfg) format

Both are the **same INI dialect**, parsed by `IniConfigFile::load`
(`common/config.cpp:24`): `[category]` headers, `key=value` lines, or bare `key` lines
(value = `NULL`). No comments, no escaping, no quoting. Duplicate keys in a
multi-value category are all retained (`Dictionary2D`).

### 7.1 Categories

| Category | Constant | Loaded at | Semantics |
|---|---|---|---|
| `[project]` | `PROJECT_CATEGORY` | `elc.cpp:141-153` | single-valued scalars |
| `[compiler]` | `COMPILER_CATEGORY` | `elc.cpp:156-159` | names of the built-in literal/int/real/array classes |
| `[linker]` | `LINKER_CATEGORY` | `elc.cpp:162-163` | GC size + subsystem type |
| `[templates]` | `TEMPLATE_CATEGORY` | `elc.cpp:128` | name → template `.cfg` path |
| `[primitives]` | `PRIMITIVE_CATEGORY` | `elc.cpp:166` | name → `.bin` native module |
| `[files]` | `SOURCE_CATEGORY` | `elc.cpp:169` | one relative source path per line |
| `[forwards]` | `FORWARD_CATEGORY` | `elc.cpp:172` | weak reference → strong reference |

### 7.2 Every key

| Category | Key | Constant | `ProjectSetting` | Default | Meaning |
|---|---|---|---|---|---|
| `project` | `template` | `ELC_PROJECT_TEMPLATE` | — | none | name of a `[templates]` entry to load first |
| `project` | `entry` | `ELC_PROJECT_ENTRY` | forward `'starter` | none | forward for `STARTUP_CLASS` (`elc.cpp:141-144`) |
| `project` | `start` | `ELC_PROJECT_START` | `opEntry` | none | linker entry symbol (e.g. `$package'elena'3`) |
| `project` | `package` | `ELC_PACKAGE` | `opPackage` | empty | namespace prefix for generated module names |
| `project` | `executable` | `ELC_TARGET` | `opTarget` | none | output `.exe`, resolved relative to the config file |
| `project` | `libpath` | `ELC_LIB_PATH` | `opLibPath` | empty | `.nl` search root |
| `project` | `output` | `ELC_OUTPUT_PATH` | `opOutputPath` | dir of the `-c` file | where `.nl`/`.dnl` go |
| `project` | `warn:unresolved` | `ELC_WARNON_UNRESOLVED` | `opWarnOnUnresolved` | off | enable warning 401 |
| `project` | `debuginfo` | `ELC_DEBUGINFO` | `opWithDebugInfo` | off | emit `.dnl` |
| `compiler` | `literalclass` | `ELC_COMPILER_LITERALCLASS` | `opLiteralClass` | — | `std'basic'literal` |
| `compiler` | `integerclass` | `ELC_COMPILER_INTEGERCLASS` | `opIntegerClass` | — | `std'basic'intnumber` |
| `compiler` | `realclass` | `ELC_COMPILER_REALCLASS` | `opRealClass` | — | `std'basic'realnumber` |
| `compiler` | `arrayclass` | `ELC_COMPILER_ARRAYCLASS` | `opArrayClass` | — | `std'basic'array`; **the only one the frontend actually reads** (`compiler.cpp:992`) |
| `linker` | `gcsize` | `ELC_GC_PAGESIZE` | `opGCHeapSize` | 0 (→ linker default) | GC page count |
| `linker` | `type` | `ELC_SYSTEMTYPE` | `opSystemType` | `0` = `ptLibrary` | 0=library (no link), 1=console, 2=GUI (`elenaconst.h:240`) |

Boolean values are `-1` for true (`setBoolOption`, `elc.h:168`; `getBoolSetting`,
`common/common.h:59`). `setIntOption` (`elc.h:160`) only stores the value when it
*differs* from the supplied default — a subtle trap: you cannot explicitly set a value
back to the default.

`opLiteralClass`, `opIntegerClass`, `opRealClass` are declared and loaded but **never
consulted by the frontend**. Literal/integer/real objects are emitted as
`mskLiteralRef`/`mskInt32Ref`/`mskRealRef` constants and given their class by the
*linker*, not the compiler.

### 7.3 Settings that are *not* read by `elc`

Real `.prj` files in `examples/` contain keys that `elc` ignores entirely — they belong to
the IDE (`elenasrc/ide/ideconst.h:310-322`):

| Key | Owner | Note |
|---|---|---|
| `projecttype` | IDE (`IDE_TYPE_SETTING`) | mirrors `[linker] type`; `elc` reads only the latter |
| `arguments` | IDE (`IDE_ARGUMENT_SETTING`) | debuggee command line |
| `debug` | IDE (`IDE_OLD_TYPE_DEBUG`) | obsolete |
| `type` under `[project]` | IDE (`IDE_OLD_TYPE_SETTING`) | obsolete; **not** the linker's `type` |
| `options` | IDE (`IDE_COMPILER_OPTIONS`) | extra `elc` flags |
| `[virtualmachine]` in `elc.cfg` | nobody in this tree | `codereserved`/`codecommit`/`datareserved`/`datacommit` are dead keys |

The IDE launches the compiler as `elc.exe [-xunicode] -c<project.prj>`
(`elenasrc/ide/win32/appwindow.cpp:1928-1931`).

### 7.4 Worked example

`examples/helloworld/helloworld.prj`:

```ini
[project]
executable=helloworld.exe
entry='entry
template=console
projecttype=1        ; IDE only
debuginfo=-1
warn:unresolved=0

[files]
helloworld.l

[forwards]
'program=helloworld'program
```

`bin/templates/console.cfg` supplies `[project] start=$package'elena'3`,
`[linker] type=1`, and the `'entry`/`'program'output`/… forwards.
`bin/elc.cfg` supplies `libpath=..\lib`, the `[compiler]` class names, `gcsize=4096`, and
the `[primitives]` map.

Library projects (`src/std/std.prj`) omit `executable` and `template`, so
`[linker] type` stays 0 = `ptLibrary` and `elc.cpp:313` skips linking entirely.

---

## 8. Error handling & diagnostics

### 8.1 Mechanism

Every diagnostic funnels through two virtuals on `Project`:

```cpp
virtual void raiseError(const TCHAR* msg, ...)  // elc.h:103 — printf, then THROW _Exception
virtual void printInfo (const TCHAR* msg, ...)  // elc.h:130 — printf only
```

`ModuleScope::raiseError` (`compiler.cpp:200`) supplies
`(sourcePath, terminal.Row(), terminal.Col(), terminal.value)` — matching the
`%s(%d:%d): … %s` shape of every message in `errors.h`.
`ModuleScope::raiseWarning` (`compiler.cpp:205`) prints and calls
`Project::indicateWarning()` (`project.h:108`).

Lexer/parser errors are C++ exceptions caught per-source in `Compiler::run`
(`compiler.cpp:1740-1751`):

```cpp
catch (LineTooLong& e)  { project.raiseError(errLineTooLong, it.key(), e.row); }
catch (InvalidChar& e)  { project.raiseError(errInvalidChar, it.key(), e.row, e.column, e.ch); }
catch (SyntaxError& e)  { project.raiseError(e.error, it.key(), e.row, e.column, e.token); }
```

**Critical limitation:** since `_ELC_::Project::raiseError` *always* throws
(`elc.h:113`), these handlers immediately re-throw, escaping `run()` to `main`'s catch
(`elc.cpp:328`). Therefore:

* **there is no error recovery of any kind** — the very first error aborts the whole
  compilation, including all remaining source files;
* the per-file `try` in `run()` is misleading: it cannot continue to the next file;
* `main` then calls `cleanUp()` (`elc.cpp:332`), **deleting every `.nl` this project would
  have produced**, including modules that compiled successfully before the failure.

Warnings, by contrast, accumulate: `run()` returns `!HasWarnings()`, `main` prints
"Compiled with warnings", sets `exitCode = -1`, and **still links**.

### 8.2 Catalogue (`elenasrc/elc/errors.h`)

Parser errors (`errors.h:15-25`) — thrown from `source.cpp` / `parser.cpp`:

| Code | Macro | Text | Raised at |
|---|---|---|---|
| 001 | `errLineTooLong` | Line too long | `source.cpp:70` → `compiler.cpp:1742` |
| 002 | `errInvalidChar` | Invalid char `%c` | `source.cpp:97` → `compiler.cpp:1746` |
| 004 | `errInvalidSyntax` | Invalid syntax near `'%s'` | `parser.cpp:20`, `:61`, `:154` |
| 005 | `errDotExpectedSyntax` | `'.'` expected | `parser.cpp:39-41` (`nsErrDotExpected`, `nsFieldErrDotExpected`) |
| 006 | `errCBrExpectedSyntax` | `')'` expected | `parser.cpp:42-44` |
| 007 | `errOBrExpectedSyntax` | `'('` expected | `parser.cpp:45-46` |
| 008 | `errOActionExpectedSyntax` | `'('` or `'['` expected | `parser.cpp:47-48` |
| 009 | `errCSBrExpectedSyntax` | `']'` expected | `parser.cpp:49-52` |
| 010 | `errCBraceExpectedSyntax` | `'}'` expected | `parser.cpp:53-54` |
| 011 | `errVarNameExpectedSyntax` | public or private identifier expected | `parser.cpp:55-57` |
| 012 | `errExtensionNotAllowed` | role cannot have an extension | `parser.cpp:58-59` |

Note codes **003** are unused, and the mapping is `Symbol → message` via the `getError`
switch (`parser.cpp:35`). As noted in §4.4, error 005 is only reachable through
`FIELD_DOT_EXPECTED`.

Compiler errors (`errors.h:28-43`):

| Code | Macro | Text | Raised at |
|---|---|---|---|
| 102 | `errDuplicatedSymbol` | Class `'%s'` already exists | `compiler.cpp:1667` |
| 103 | `errDuplicatedMethod` | Method already exists in class | `compiler.cpp:1422` |
| 104 | `errUnknownClass` | Class doesn't exist | `compiler.cpp:1337`, `:1345` |
| 105 | `errDuplicatedLocal` | Variable already exists | `compiler.cpp:1202` |
| 106 | `errUnknownObject` | Unknown object | `compiler.cpp:827`, `:1187` |
| 107 | `errInvalidOperation` | Invalid operation with `'%s'` | `compiler.cpp:1061`, `:1189` |
| 109 | `errDuplicatedField` | Field already exists | `compiler.cpp:1538` |
| 111 | `errIllegalField` | Illegal field declaration | `compiler.cpp:1517`, `:1527`, `:1535` |
| 113 | `errTooManyParameters` | Too many parameters for embedded function | **never raised** |
| 117 | `errUnknownRole` | Unknown role | `compiler.cpp:295` |
| 118 | `errInvalidShift` | `#shift<>` only in roles | `compiler.cpp:1079` |
| 119 | `errDuplicatedDefinition` | Duplicate `#define` | `compiler.cpp:1636`, `:1641` |
| 121 | `errInvalidProperty` | Invalid or non-existing property | `compiler.cpp:812`, `:906`, `:1022` |
| 124 | `errExtTooManyParameters` | Too many parameters for external function | `compiler.cpp:954` |
| 127 | `errInvalidRedirectMessage` | Cannot use redirect message here | `compiler.cpp:693` |
| 130 | `errInvalidIntNumber` | Invalid integer value | `compiler.cpp:779`, `:788`, `:800` (effectively dead, see §5.4) |

Codes 100, 101, 108, 110, 112, 114–116, 120, 122, 123, 125, 126, 128, 129 are unassigned —
the numbering reflects features removed over time.

Linker errors (`errors.h:46-52`, codes 201–210) are also used by the frontend:
`errInvalidFile` (205) for a missing `syntax.dat` (`elc.cpp:302`) or a missing source file
(`compiler.cpp:1703`); `errCannotCreate` (204) when a module cannot be written
(`project.cpp:118`, `:133`); `errDuplicatedModule` (208) at `project.cpp:90`; and the
`LoadResult → message` mapping in `Project::getLoadError` (`project.cpp:27`).

Internal error (`errors.h:55`): `errReferenceOverflow` (301), thrown as
`InternalError` from `Module::mapReference` (`engine/module.cpp:66`).

Warnings (`errors.h:58-61`):

| Code | Macro | Raised at |
|---|---|---|
| 401 | `wrnUnresovableLink` | `compiler.cpp:187` (cross-module), `:196` (deferred, same module) |
| 404 | `wrnUnknownHint` | `compiler.cpp:217`, `:248`, `:323`, `:386` |
| 405 | `wrnUnknownHintValue` | `compiler.cpp:351` |
| 406 | `wrnInvalidHint` | `compiler.cpp:319` |

The unresolved-link check (`ModuleScope::validateReference`, `compiler.cpp:168`) is
*optional* (gated on `warn:unresolved`) and works by actually loading the target module
and probing for the section. Same-module references are deferred into
`forwardsUnresolved` and rechecked at end-of-module by `validateForwards`
(`compiler.cpp:192`, called from `compileModule` at `:1695`).

CLI errors live in `elc.h:83-85` (401 invalid parameter, 402 invalid path, 404 invalid
template) — note the **numbering collision** with warning 401.

---

## 9. Data structures used

The frontend depends on `elenasrc/common/` (header-only except for four `.cpp` files) and
`elenasrc/engine/`. Nothing from the C++ standard library beyond `<stdio.h>`, `<stdlib.h>`,
`<string.h>`, `<tchar.h>`, `<io.h>`, `<stdarg.h>`, `<errno.h>`.

### 9.1 Containers (`common/lists.h`)

| Template | Decl | Used for |
|---|---|---|
| `List<T>` | `:801` | `ModuleScope::forwardsUnresolved` (`compiler.h:71`) |
| `Stack<T>` | `:875` | `ParserStack` (`engine/elena.h:213`) — the LL(1) derivation stack |
| `BList<T>` | `:1022` | `CommandTape::tape` (`engine/bytecode.h:181`) — the byte-code IR |
| `Map<Key,T,KeyStored>` | `:1159` | `ForwardMap`, `LocalMap`, `NamespaceMaskMap`, `RoleMap`, `MessageMap`, `ModuleMap` (`compiler.h:22-24`, `:351`) |
| `MemoryMap<Key,T,KeyStored>` | `:1444` | `ClassInfo::methods`, `ClassInfo::fields`, `ClassInfo::roles` (`engine/elena.h:95-96`), `SymbolMap` (`engine/elena.h:214`) |
| `HashTable<…>` | `:1915` | `SymbolHash` inside `ParserTable::generate` (`parsertable.cpp:15`) |
| `MemoryHashTable<…>` | `:2131` | `SyntaxHash`, `TableHash` (`engine/elena.h:215-216`), `ReferenceMap`, `ConstantMap` (`engine/elena.h:206-207`) |
| `Cache<Key,T,N>` | `:2404` | `Module::_resolvedReferences/_resolvedMessages` (20 entries, `engine/module.h:19`) |
| `Dictionary2D<Key,SubKey>` | `:2526` | `ProjectSettings` (`project.h:16`) and `ConfigSettings` (`common/common.h:36`) |

`MemoryMap`/`MemoryHashTable` store their entries **inside a `MemoryDump`**, which lets
them be `read`/`write`n to a stream verbatim — that is how `syntax.dat` and the `.nl`
reference/message/constant tables are serialized (`parsertable.cpp:199-215`,
`engine/module.cpp:116-170`). It also means iterator invalidation on growth, hence the
`_resolvedReferences.clear()` calls in `Module::mapReference` (`engine/module.cpp:71-72`).

`mapKey` (`lists.h:2681`) is the ubiquitous get-or-insert helper;
`retrieveKey` (`lists.h:2706`) is the **linear reverse lookup** used by
`Module::resolveReference`/`resolveMessage` — O(n) per call, mitigated only by the
20-entry `Cache`.

### 9.2 Strings and paths

| Type | Decl | Notes |
|---|---|---|
| `String` | `common/altstrings.h:102` | heap, `STR_PAGE_SIZE`=0x20 growth |
| `LocalString<N>` | `:168` | fixed stack buffer; **silently fails** on overflow (`_copy` returns `false`, callers ignore it) |
| `ReferenceNameTemplate<S>` | `:250` | joins `module'proper'sub`; `pathToName` converts `a\b.l` → `a'b` |
| `NamespaceTemplate<S>` | `:394` | everything before the last `'` |
| `IdentifierTemplate<S>` | `:420` | everything after the last `'` |
| `PrivateMessageTemplate<S>` | `:435` | 2-level-namespace-qualified private message |
| `Quote<S>` | `:341` | decodes `%n`, `%r`, `%t`, `%a`, `%b`, `%%`, `%<dec>` in string literals |
| `PathTemplate<S>` | `common/files.h:19` | `\`-separated; `nameToPath`, `copyPath`, `changeExtension` |
| `LocalPath` | `common/files.h:196` | `LocalString<0x200>` |

Fixed limits: `LINE_LEN`=0x1000, `IDENTIFIER_LEN`=0x100
(`engine/elenaconst.h:18-19`), `LOCAL_PATH_LENGTH`=0x200 (`common/files.h:15`).

### 9.3 Streams and dumps

`StreamReader` / `StreamWriter` / `TextReader` / `TextWriter` (`common/streams.h:20`,
`:117`, `:218`, `:240`); `MemoryDump` + `DumpWriter`/`DumpReader` (`common/dump.h:20`,
`:101`, `:159`); `FileReader`/`FileWriter`/`TextFileReader`/`TextFileWriter`
(`common/files.h:243`, `:279`, `:315`, `:331`); `Section` + `SectionWriter`
(`engine/section.h`).

The derivation tree, the parser table, the byte-code sections and the debug info all
travel through `MemoryDump`/`Section`, which are raw byte buffers with DWORD accessors and
a relocation list — no serialization schema, no versioning beyond the 5-byte module
signature `"EN!10"` (`engine/elenaconst.h:149`).

---

## 10. Modernization notes

Line references are to this tree. Items are grouped by how they affect the LLVM /
cross-platform / multithreading / new-GC effort.

### 10.1 Hard-coded to Win32

| What | Where | Impact |
|---|---|---|
| `main()` takes no args; uses `CommandLineToArgvW(GetCommandLineW(), …)` | `elc.cpp:255-258` | trivial to replace with `main(int, char**)`; also removes the implicit `_UNICODE`-only build |
| `setmode(_fileno(stdout), _O_WTEXT)` | `elc.cpp:262` | MSVCRT-only; delete in favour of UTF-8 stdout |
| `GetModuleFileName` for app path | `elc.cpp:30` | needs `/proc/self/exe`, `_NSGetExecutablePath`, `GetModuleFileName` shims |
| `MultiByteToWideChar` / `WideCharToMultiByte` with `CP_ACP` | `common/win32/unicode.h:19,26` | the *entire* string layer is `TCHAR` + ANSI-codepage; must become UTF-8 `char` |
| `#include <io.h>`, `<direct.h>`, `<tchar.h>` | `common/common.h:14-18`, `common/files.cpp:13` | MSVC-only headers |
| Backslash as the only path separator | `common/files.h:29,65,84,105,181`, `common/altstrings.h:322` | `nameToPath`/`pathToName`/`copyPath` all hard-code `'\\'` |
| `#include "win32\x86jitcompiler.h"`, `Linker`, `x86JITCompiler` | `elc.cpp:17,316` | the driver *directly* constructs the PE linker and the x86 JIT — no backend abstraction exists |
| `ProjectType { ptLibrary, ptConsole, ptGUI }` | `engine/elenaconst.h:240` | PE subsystem values, leaked into the frontend's "should I link?" decision (`elc.cpp:313`) |
| `GUI_CLASS = win32'system'gui` | `engine/elenaconst.h:37` | platform class name baked into engine constants |
| Windows `\r\n` written by `File::writeNewLine` | `common/files.cpp:245` | affects generated config/map files |

### 10.2 32-bit / pointer-size assumptions

These are the ones that will bite hardest, because they are spread across the frontend,
not confined to a backend.

| Assumption | Where |
|---|---|
| `ref_t` is `size_t` but reference **tags occupy the top 8 bits** including the sign bit (`mskRelativeRef` = 0x80000000) | `common/common.h:21`, `engine/elenaconst.h:169-200` — on a 64-bit build `size_t` is 64 bits and every mask test silently changes meaning |
| Field/array element stride hard-coded as `<< 2` (4 bytes) | `compiler.cpp:872` (outer capture), `compiler.cpp:1004` (collection fill) |
| Type instance is a 4-byte binary object | `compiler.cpp:1063` (`4 \| gcBinary`) |
| `VMTEntry` = two 32-bit ints; `sizeof(VMTEntry)` used as the VMT growth increment | `engine/elena.h:76-80`, `compiler.cpp:1253`, `:1267` |
| `elEmptyObject`=8, `elVMTOffset`=0x0C, `elAnyHandlerSize`=0x10 | `engine/elenaconst.h:253-256` |
| `VA_ALIGNMENT`=8, `VA_ALIGNMENT_POWER`=3; virtual addresses are `mask \| (addr >> 3)` | `engine/elena.h:225-231` — a 24-bit index × 8 caps the image at 128 MB |
| Everything serialized with `writeDWord` / `getDWord` (32-bit) — derivation tree, module tables, debug info | `derivation.cpp:19-45`, `engine/elena.h:105-123` |
| `PREDEFINED_REF` = 0x80000000 tested with `test(int, int)` | `common/tools.h:39`, `compiler.cpp:36`, `engine/bccompiler.cpp:421` — signed-int bit test on a value with the sign bit set |
| Frame slot constants `-1` (vself) and `-3` (self) | `compiler.cpp:25`, `:1279`, `:1284` |
| `MAXIMAL_MESSAGE_REF` = 0xFFFF; reference index capped at 2^24 | `engine/elenaconst.h:105`, `engine/module.cpp:65` |

### 10.3 What an LLVM backend would need

The good news: **the frontend never emits x86**. It emits a `CommandTape`
(`engine/bytecode.h:179`) of ~40 abstract opcodes. The seam is clean — but it is at the
*wrong level*, and it is not a seam the frontend can currently reach past.

1. **`ByteCodeCompiler` is a concrete class, not an interface.** `Compiler::_coder`
   (`compiler.h:353`) is a by-value member. Every lowering decision calls it directly
   (~90 call sites). Making it a virtual `IEmitter` is the single highest-leverage
   refactor: one could then plug an LLVM-IR emitter beside the byte-code one.
2. **The byte code is stack-machine + label-patching, not SSA.** Jump resolution happens
   in `ByteCodeCompiler::saveProcedure` via `fixJumps` over three `Stack<JumpInfo>`
   (`engine/bccompiler.cpp:43`, `:629-632`), writing raw byte offsets. Translating this to
   LLVM basic blocks requires reconstructing control flow from the `blBegin`/`blFailure`/
   `blEnd`/`bltBranch`/`bltLoop` markers — doable, but it would be far cleaner to have
   `compileLoop`/`compileControlChain` emit structured regions directly.
3. **The "failure edge" calling convention is unusual.** Every call
   (`bcRCall`, `bcIOCall1`, …) carries a second destination used when the callee
   "fails" (`engine/bccompiler.cpp:703-717`). In LLVM this maps naturally onto `invoke`
   + landing pads, or onto a two-result return; it does *not* map onto plain `call`.
   Deciding this early determines the whole ABI.
4. **Message dispatch is a runtime VMT search**, with the message id resolved to a global
   id at link time (`engine/jitlinker.cpp:111`). An LLVM backend needs a real dispatch
   ABI (inline cache / itable / perfect hash) and a global message-id assignment pass —
   which the current per-module string-table scheme (§6.1) does not provide.
5. **Constants are untyped strings.** Integers, hex and reals are stored as *spelling* in
   the module constant table (`compiler.cpp:774-802`) and are given their class by the
   linker. An LLVM frontend wants real `ConstantInt`/`ConstantFP` at compile time; this
   also removes the `errno`/`ERANGE` bug.
6. **Debug info is a bespoke `DebugLineInfo` stream** (`engine/elena.h:133`) with a
   `union` of three shapes and no line-table structure. It would need to be replaced by
   DWARF metadata; the frontend already carries the right raw data (row, `disp`, length),
   so the translation is mechanical.
7. **No IR to optimize.** There is no place to hang inlining, constant folding, escape
   analysis, or devirtualization. The `!!` comments in the code
   (`compiler.cpp:960`, `engine/bccompiler.cpp:630-633`, `:181`) mark exactly the spots
   the author knew were suboptimal.

### 10.4 What is architecturally sound and worth keeping

* **The pipeline shape** — lex → LL(1) → derivation stream → single-pass lowering — is
  clean, small (≈2 900 lines total for the whole frontend) and easy to reason about.
* **Grammar in a data file.** `dat/sg/syntax.txt` + `sg` is a genuinely good decision:
  the grammar is declarative, the table generator validates LL(1)-ness
  (`parsertable.cpp:113`), and the parser driver is 40 lines. Keep this; just move the
  generated table into a header so `syntax.dat` stops being a runtime dependency
  (see §10.5).
* **`mskTraceble`** — pruning the concrete syntax tree *during* parsing rather than
  building a full CST and simplifying afterwards is elegant and cheap.
* **The scope chain** (`Scope::mapObject` / `getScope`, `compiler.h:175`, `:183`) is a
  textbook lexical-scope resolver and should survive any rewrite essentially unchanged.
* **`InlineClassScope` closure conversion** (`compiler.cpp:444`) — automatically promoting
  captured outer variables to fields of a synthesized class is exactly what a modern
  closure-conversion pass does. Excellent design, keep the algorithm.
* **Namespace masks (`#define x'* = y'*`)** (`compiler.cpp:133`) are a nice, cheap
  aliasing mechanism.
* **Separation of `ClassInfo` metadata from code** into a distinct `mskMetaDataRef`
  section (`compiler.h:232-240`) makes separate compilation work without header files.
* **`ProjectSetting` as a single enum-keyed settings store** (`project.h:20`) is a
  reasonable, if untyped, configuration model.

### 10.5 Technical debt, ranked

| # | Item | Where | Why it matters |
|---|---|---|---|
| 1 | **No error recovery.** `raiseError` always throws; the first diagnostic kills the build and `cleanUp()` deletes already-good modules | `elc.h:103-114`, `compiler.cpp:1740-1751`, `elc.cpp:326,332` | unusable for a modern edit-compile loop; must become a diagnostic sink + recovery points |
| 2 | **`syntax.dat` is a missing build artifact.** `bin/` has no `syntax.dat`; `sg` must be run manually, and there is no build script anywhere in the tree | `elc.cpp:299-302`, `sg/sg.cpp:137`, `bin/` | the compiler cannot be built-and-run reproducibly today |
| 3 | **Grammar ↔ `syntax.h` are two hand-synced copies** of the same id table | `dat/sg/syntax.txt:1-73` vs `elenasrc/elc/syntax.h:16-100` | `DOT_EXPECTED` is already out of sync (§4.4); `nsCollectionSymbol`/`nsObjectExpression` are dead |
| 4 | **`TCHAR`/`_T()` everywhere** with ANSI-codepage conversion | all of `common/`, all of `elc/` | blocks UTF-8, blocks portability; ~every file changes |
| 5 | **`ByteCodeCompiler` is not an interface** | `compiler.h:353` + ~90 call sites | blocks any alternative backend |
| 6 | **Derivation tree is write-once and forward-only**; `firstChild`/`nextNode`/`select` re-scan the stream | `derivation.cpp:55-126` | forbids any AST pass; also O(n²) on wide nodes |
| 7 | **`errno`/`ERANGE` numeric-overflow checks are dead code**; `errno` never cleared, `_ttoi` never sets it, the parsed value is discarded | `compiler.cpp:776-802` | silent wrong-value compilation of out-of-range literals |
| 8 | **`-xunicode` sets the output path** | `elc.cpp:197-199` | plain bug |
| 9 | **`LocalString` overflow is silent** — `_copy` returns `false` and every caller ignores it | `common/altstrings.h:175-194`; e.g. `compiler.cpp:784`, `:797` | truncation instead of diagnostics; identifiers > 256 chars truncate in `SourceReader::read` too (`source.cpp:106`) |
| 10 | **`retrieveKey` is a linear scan** used for every `resolveReference`/`resolveMessage`, cached only 20 deep | `common/lists.h:2706`, `engine/module.cpp:30-48` | O(n²) on large modules; hit hard by `inheritClass`'s `importMessage` loop (`compiler.cpp:629-634`) |
| 11 | **`tableKey` overlaps the terminal bit with the nonterminal field** | `parsertable.cpp:19` vs `:174,:186` | works only by symmetric error; will break any table-format change |
| 12 | **`DFA` template's default ctor ignores its `start` parameter** and the declared bound `[23]` contradicts the 27-row table | `dfa.h:20,49` vs `source.cpp:18` | the template is not reusable; the bound is a lie |
| 13 | **No thread-safety anywhere.** `Compiler` holds a single `_coder`, `_parser`, `_predefined`; `Project` holds global `_modules`/`_binaries`; `Module` caches are mutable | `compiler.h:353-356`, `project.h:68-70`, `engine/module.h:29-30` | parallel per-file compilation (the obvious first speedup) requires making `ModuleScope` self-contained and `Project` thread-safe |
| 14 | **`Compiler::run` reuses one `MemoryDump buffer`** for all files | `compiler.cpp:1721`, `:1705` | another barrier to parallelism |
| 15 | **Roles and `#shift` are a whole-language feature with ~120 lines of support** and no test coverage in-tree | `compiler.cpp:291,577,1066,1445` | decide early whether the modernized language keeps roles; they interact with VMT layout, inheritance and the JIT (`bcShift`/`bcUnShift`) |
| 16 | **`opLiteralClass`/`opIntegerClass`/`opRealClass` are loaded but never used** by the frontend | `elc.cpp:156-158` vs `compiler.cpp` | dead configuration surface; the class of a literal is decided by the linker |
| 17 | **Warning code 401 collides with CLI error code 401** | `errors.h:58` vs `elc.h:83` | diagnostics numbering has no allocation discipline |
| 18 | **Version numbers disagree**: greeting says 2.0, engine says 5.0, module signature says `ELENA.150`/`EN!10` | `elc.h:22-23`, `engine/elenaconst.h:15-16`, `:148-150` | no single source of truth for versioning/compat checks |

### 10.6 Suggested order of attack (frontend only)

1. De-Windows the substrate: replace `TCHAR`/`_T`/ANSI with UTF-8 `char`, replace path
   handling, replace `main`. This is mechanical but touches everything, so do it first.
2. Introduce a `DiagnosticSink` and stop throwing on the first error; add recovery at
   statement (`.`) and member (`#method`/`#field`/`#class`) boundaries.
3. Generate `syntax.dat` at build time (or embed the table as a generated `.cpp`) and
   generate `syntax.h` from `syntax.txt` so the two can no longer drift.
4. Abstract the emitter (`IEmitter`) behind `Compiler::_coder`, keeping
   `ByteCodeCompiler` as the first implementation so the existing JIT keeps working while
   the LLVM emitter is written beside it.
5. Replace the flat derivation stream with a real arena-allocated AST, so a semantic pass
   (and later an optimizer) has something to walk twice.
6. Only then tackle 64-bit: widen `ref_t`, move the section tag out of the pointer's high
   byte into a side table, and replace `<< 2` strides with a target-parameterised
   `pointerSize`.
