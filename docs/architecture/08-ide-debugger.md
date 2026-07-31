# The ELENA IDE (`elide`) and its Debugger

> `elenasrc/ide/` — 14,378 lines of C++ (+351 lines of `.rc`, +52 lines of shared plugin
> header, +121 lines of sample plugin). A hand-rolled Win32 GUI application: text editor,
> project manager, compiler driver and a **full native x86 debugger** for ELENA programs.
>
> The critical finding of this document: **the debugger is worth far more than the IDE.**
> The debug-information model (`debugcontroller.cpp`) is a genuinely well-shaped,
> platform-independent design trapped inside a Win32-only application. The UI around it is
> 8,802 lines of raw `HWND` code with essentially zero abstraction value.

Related: [`04-pe-linker.md`](04-pe-linker.md) (writes the `.dn` file this debugger consumes),
[`03-engine-bytecode-jit.md`](03-engine-bytecode-jit.md) (emits the `DebugLineInfo` records).

---

## 1. Overview & module map

### 1.1 Build structure

`elide.exe` is built from three source roots (`ide/codeblocks/elide_win32.cbp`):

```
elenasrc/ide/            "platform-independent" core    5,527 LOC
elenasrc/ide/win32/      Win32 implementation           8,802 LOC
elenasrc/ide/gtk/        abandoned port                     49 LOC
elenasrc/idecommon/      plugin ABI (shared with DLLs)      52 LOC
elenasrc/plugins/        sample plugin                     121 LOC
                                                       ─────────
                                                        14,551 LOC
```

plus `ide/ide.rc` (351 lines: menus, accelerators, dialog templates, bitmaps) and four
Visual Studio project generations (`vc/elide7|8|9|10`) alongside the Code::Blocks/MinGW
project. The Win32 build is `-D_UNICODE -DUNICODE`, so **`TCHAR` is `wchar_t` throughout**.

### 1.2 Module map

Legend for **Platform**: `PI` = platform-independent in intent and in fact; `PI*` =
*declared* platform-independent (lives outside `win32/`) but **does not compile without
`windows.h`**; `W32` = explicitly Win32.

| File | LOC | Role | Platform |
|---|---:|---|---|
| **Core (`ide/`)** | | | |
| `text.h` | 264 | Paged text buffer, bookmark cursor, undo journal declarations | PI* |
| `text.cpp` | 1086 | Page list, bookmark navigation, `TextHistory` undo/redo ring | PI* |
| `document.h` | 200 | Document model: caret, selection, frame, markers, `LineInfo` | PI* |
| `document.cpp` | 901 | All editing commands, caret movement, styles for selection | PI* |
| `sourcedoc.h` | 74 | `LexicalStylist` + `SourceDoc` declarations | PI* |
| `sourcedoc.cpp` | 348 | 21-state lexical DFA table, styler, bracket matching | PI* |
| `debugcontroller.h` | 164 | `DebugController`, `Breakpoint`, `_DebuggerWatch` interface | PI* |
| `debugcontroller.cpp` | 691 | **Debug-info loading, source↔address mapping, step logic, object inspection** | PI* |
| `browser.h` | 108 | Watch-window model (`DebuggerWatch`, `DebuggerAutoWatch`) | W32 |
| `browser.cpp` | 310 | Watch tree population from `DebugController` callbacks | W32 |
| `ideproject.h` | 88 | `ProjectInfo` — `.prj` model | PI* |
| `ideproject.cpp` | 351 | `.prj` load/save, name↔path resolution, legacy migration | PI* |
| `idesettings.h` | 80 | `Paths` + `Settings` global statics | PI* |
| `idesettings.cpp` | 213 | `ide.cfg` load/save; **`shlwapi` path functions, `<direct.h>`** | W32 |
| `ideconst.h` | 324 | All IDs: styles, menu commands, notifications, `.prj` keys, messages | PI |
| `layout.h` / `.cpp` | 46 / 91 | Dock layout manager (top/bottom/left/right/client) | PI* |
| `messagelog.h` / `.cpp` | 55 / 48 | Compiler error list backed by a Win32 ListView | W32 |
| `pluginmanager.h` / `.cpp` | 65 / 20 | Plugin hook registry; `LoadLibrary` + `GetProcAddress` | W32 |
| `ide.rc` | 351 | Menus, accelerators, dialog templates, toolbar bitmaps | W32 |
| **Win32 layer (`ide/win32/`)** | | | |
| `appwindow.h` | 288 | `AppWindow`, `AppDebugger`, MRU/window menu lists | W32 |
| `appwindow.cpp` | 2167 | Main window: command dispatch, notification routing, debug session driving, compiler launch, breakpoint bookkeeping | W32 |
| `editframe.h` | 200 | `EditFrame` editor control + `ViewStyles` colour schemes | W32 |
| `editframe.cpp` | 1238 | Custom text rendering (GDI), caret, scrollbars, key handling | W32 |
| `debugger.h` | 225 | `Debugger`, `ProcessContext`, `BreakpointContext`, `DebugEventManager` | W32 |
| `debugger.cpp` | 607 | **`CreateProcess(DEBUG_PROCESS)`, `WaitForDebugEvent`, INT3 patching, DR0/DR7, `ReadProcessMemory`** | W32 |
| `dialogs.h` / `.cpp` | 253 / 670 | 7 modal dialogs over `DialogBoxParam` + `GetOpenFileName` | W32 |
| `idecommon.h` | 463 | `Point`, `Rectangle`, `Colour`, `Font`, `Style`, `Canvas`, `Clipboard`, `DateTime`, `NMHDR` extensions | W32 |
| `idecommon.cpp` | 455 | GDI canvas, font cache, clipboard, message box | W32 |
| `window.h` / `.cpp` | 94 / 227 | `Control`/`Window` base classes, `Window_Proc` thunk | W32 |
| `winmain.cpp` | 208 | `WinMain`, window-class registration, config load/save, cmdline | W32 |
| `menu.h` / `.cpp` | 73 / 130 | `HMENU` wrappers (main menu + popup) | W32 |
| `toolbar.h` / `.cpp` | 46 / 65 | `TOOLBARCLASSNAME` wrapper | W32 |
| `statusbar.h` / `.cpp` | 41 / 57 | `STATUSCLASSNAME` wrapper, 5 parts | W32 |
| `tabbar.h` / `.cpp` | 94 / 265 | `WC_TABCONTROL` wrapper + owner-draw tabs (`TabBarPlus` = tabbed container) | W32 |
| `treeview.h` / `.cpp` | 56 / 146 | `WC_TREEVIEW` wrapper (used by the watch browser) | W32 |
| `listview.h` / `.cpp` | 40 / 82 | `WC_LISTVIEW` wrapper (used by the message log) | W32 |
| `splitter.h` / `.cpp` | 52 / 198 | Draggable splitter, uses a **global low-level mouse hook** | W32 |
| `output.h` / `.cpp` | 54 / 256 | Read-only `edit` control + pipe-redirected `elc.exe` child process | W32 |
| `accelerator.h` / `.cpp` | 29 / 23 | `LoadAccelerators` / `TranslateAccelerator` | W32 |
| **Abandoned port** | | | |
| `gtk/main.cpp` | 49 | GTK+ 2 "Hello World" sample — **no IDE code at all** | GTK |
| **Plugins** | | | |
| `idecommon/plugins.h` | 52 | Plugin ABI: `RegisterPlugin`, hook typedefs, `PluginResult` | PI |
| `plugins/autoform/autoform.h` | 50 | Plugin skeleton class | W32 |
| `plugins/autoform/autoform.cpp` | 53 | Stub hooks that only forward to the previous handler | PI |
| `plugins/autoform/win32/dllmain.cpp` | 18 | Empty `DllMain` | W32 |

### 1.3 The `PI*` problem

Only **one file in the entire IDE (`ideconst.h`) compiles without Windows headers.**
Every other "core" file begins with `#include "idecommon.h"` (`text.cpp:7`,
`document.cpp:7`, `sourcedoc.cpp:7`, `ideproject.cpp:7`, `browser.cpp:7`,
`layout.cpp:7`, `messagelog.cpp:7`, `pluginmanager.cpp:7`), which pulls in
`<windows.h>`, `<commctrl.h>` and `<shlwapi.h>` (`idecommon.h:19-21`). Additional
concrete leaks:

| Leak | Where |
|---|---|
| `debugcontroller.h` includes `"win32\debugger.h"` — **with a backslash** | `debugcontroller.h:11` |
| `Document::scroll` takes Win32 `SB_VERT`/`SB_HORZ` | `document.cpp:449`, `document.h:134` |
| `Point`/`Rectangle` used by `text.h`/`document.h` are declared in `win32/idecommon.h` | `idecommon.h:34,89` |
| `Paths::init` uses `GetModuleFileName`, `PathRemoveFileSpec`, `_wgetcwd` | `idesettings.cpp:71-96` |
| `Paths::resolveRelativePath` uses `PathIsRelative` / `PathCanonicalize` | `idesettings.cpp:98-117` |
| `DebugController` calls `Debugger` (a concrete Win32 class) by value, not through an interface | `debugcontroller.h:62` |
| `browser.h` includes `treeview.h` and `create()` takes `HINSTANCE, HWND` | `browser.h:10,93` |

The `_Controller` abstract class (`debugger.h:49`) is the only real seam, and it points
the *wrong way*: the Win32 debugger calls into the controller, but the controller
hard-depends on the concrete Win32 debugger.

---

## 2. Text engine

### 2.1 Data structure — a paged list, **not** a gap buffer

`text.h:35`:

```cpp
#define PAGE_SIZE 0x100                 // 256 TCHARs

struct Page {
   size_t used;                         // chars actually used in this page
   size_t rows;                         // count of 0x0A characters in this page
   TCHAR  text[PAGE_SIZE];              // fixed inline array — no indirection
};
typedef BList<Page> Pages;              // doubly-linked list (common/lists.h:1022)
```

The document is a **doubly-linked list of fixed 512-byte character pages**, each carrying a
cached newline count. This is closer to Emacs' old buffer-of-lines than to a gap buffer:
there is no gap, no single contiguous array, and no rope-style balanced index.

| Property | Value |
|---|---|
| Page payload | 256 `TCHAR` = 512 bytes (Unicode build) |
| Per-page overhead | `used` + `rows` + 2 list pointers = 16 bytes → ~3% |
| Insert in a full page | page is **split in two** (`text.cpp:379-396`), `insertAfter` into the list |
| Erase | shrinks `used`; a page emptied to 0 is **never freed or merged** — it stays in the list and is skipped by `skipEmptyPages` (`text.cpp:647`) |
| Compaction | none |

The absence of merge/free means a long editing session monotonically fragments the buffer:
delete-heavy editing leaves a chain of `used == 0` pages that every traversal must walk past.

### 2.2 Encoding

- In memory: `TCHAR` = `wchar_t` = **UTF-16 code units**. No surrogate-pair awareness
  anywhere — column arithmetic (`_column++` in `TextBookmark::move`, `text.cpp:743`) treats
  every code unit as one column. Astral-plane characters break caret positioning.
- On disk: `Text::load` (`text.cpp:46`) opens with `feAutodetect` and reports the detected
  encoding back so `Text::save` (`text.cpp:75`) round-trips it. Supported encodings are
  `feAnsi`, `feUTF8`, `feUTF16`, `feRaw` (`common/files.h:200`).
- **Line endings are hard-coded CRLF.** `insertNewLine` writes `TEXT("\r\n")`
  (`text.cpp:245`); `refreshPage` counts only `0x0A` for the row count (`text.cpp:326`);
  `TextBookmark::move` treats `0x0D` as "start of new row" and `0x0A` as a skip
  (`text.cpp:731-739`); `seekEOL` scans for `0x0D` (`text.cpp:814`). **An LF-only file
  loads with a correct row count but with completely wrong column tracking** — a
  first-order Linux/macOS blocker.

### 2.3 Line indexing and the `TextBookmark` cursor

There is no line-start array. Random access goes through `TextBookmark` (`text.h:63`), a
cursor caching a full position:

| Field | Meaning |
|---|---|
| `_page`, `_offset` | list iterator + offset inside the page |
| `_position` | stream offset of the **start of the page** (so absolute pos = `_position + _offset`) |
| `_row`, `_column` | logical caret; `_column` accounts for tab expansion |
| `_virtual_column` | "sticky" column preserved across up/down movement |
| `_length` | lazily computed length of the current row, `BM_INVALID` when stale |

Two movement primitives:
- `goTo(disp)` (`text.cpp:660`) — raw stream movement, does **not** maintain row/column.
- `move(disp)` (`text.cpp:690`) — maintains row/column, and therefore must inspect every
  character it passes for `0x0A`/`0x0D`/`0x09`. Moving left over a newline triggers a
  nested `goBackToBOL()` + `seekEOL()` to recompute the previous row's length
  (`text.cpp:704-708`) — an O(line length) operation *per character crossed backwards*.

`moveTo(column, row)` (`text.cpp:483`) picks a strategy by heuristic:

```
if (row < _row  &&  (_row - row) < 2 * currentPage.rows)   walk line by line backwards
else if going far                                          moveToClosestRow(row)
```

`moveToClosestRow` (`text.cpp:757`) **restarts at the document start** and hops pages
accumulating `Page::rows` until the target row is inside a page, then walks. So a jump to
line *N* costs O(N/rows-per-page) page hops plus one intra-page line scan. Similarly
`moveOn(disp)` (`text.cpp:515`) uses `move()` for displacements below `PAGE_SIZE << 2` and
falls back to `moveToClosestPosition` (full restart) otherwise.

`_rowCount` is cached on `Text`, but `insertLine` and `eraseLine` recompute it with
`retrieveRowCount()` (`text.cpp:458`), which **walks every page in the document**. Pasting a
line therefore costs O(pages).

### 2.4 Undo/redo — a double-buffered byte journal

`TextHistory` (`text.h:221`, impl `text.cpp:893-1086`) is a `_TextWatcher` attached to the
`Text` (`document.cpp:58`); every `Text::insert`/`Text::erase` broadcasts to watchers
(`text.cpp:366`, `text.cpp:433`) before mutating.

Record format written by `addRecord` (`text.cpp:925`), forward in the buffer:

```
 DWORD position         (bit 31 set  ⇒  this was an ERASE, so undo re-inserts)
 TCHAR text[length]
 TCHAR 0                (terminator)
 DWORD length           (written last, so it can be read backwards)
```

`DWORD` here means **two `TCHAR`s** in Unicode builds (`#define dword_size 2`,
`text.cpp:19`) — the journal is a `TCHAR` array, not a byte array.

| Mechanism | Detail |
|---|---|
| Capacity | `UNDO_BUFFER_SIZE = 0x20000` (`document.h:16`), split into **two** buffers of `0x10000` `TCHAR` each (`text.cpp:896-899`) |
| Coalescing | consecutive same-operation records at an adjacent position extend `_lastLength` in place instead of opening a new record (`text.cpp:931`) |
| Overflow | `switchBuffer()` (`text.cpp:1000`) flips to the other buffer and **silently discards the generation before last** — undo depth is hard-bounded and history is lost without notice |
| Undo | `BackReader` (`text.cpp:860`) reads the trailing length, then the text, then the position, and applies the inverse op (`text.cpp:1024`) |
| Redo | `LiteralReader` reads the same record *forwards* (`text.cpp:1059`) — an asymmetric reader pair |
| Re-entrancy | `_locking` suppresses watcher callbacks while undo/redo mutate the text (`text.cpp:1045`) |

What is **not** in the journal: caret/selection state (the caret is merely moved to the
record's absolute position), breakpoint markers, and any grouping into user-visible
transactions. Because positions are absolute stream offsets, the journal is invalidated by
any external buffer change.

### 2.5 Performance summary

| Operation | Cost | Note |
|---|---|---|
| Insert/erase a char in an existing page | O(page) memmove | fine |
| Insert into a full page | O(page) + list node alloc | fine |
| `getRowLength(row)` | O(document) | `text.cpp:289` creates a fresh bookmark and does a full `moveTo` |
| Go to line N | O(N/pages) + O(line) | `moveToClosestRow` restarts from the top |
| Insert/erase a *line* | O(pages) | `retrieveRowCount()` full scan |
| Repaint one screen | O(visible chars) | `Document::startReading`/`continueReading`, `document.cpp:871` |
| **Any keystroke in a source file** | **O(document)** | full re-lex, see §3 |

### 2.6 Known defects in the buffer

| Defect | Location |
|---|---|
| `erase` copies `used - offset` characters instead of `used - offset - size` → reads past the page and leaves stale tail bytes | `text.cpp:440` |
| `_tcsncpy` used for **overlapping** ranges (undefined behaviour) in both `insert` and `erase` | `text.cpp:397`, `text.cpp:440` |
| `Text::copyTo` writes `buffer[length] = 0` before validating the buffer size | `text.cpp:96` |
| `retrieveRowCount` seeds the count at 1 even for an empty document | `text.cpp:461` |
| `findWord` dereferences `(*bookmark._page).text[bookmark._offset]` without checking `used` | `text.cpp:200` |

---

## 3. Document & syntax highlighting

### 3.1 Layering

```
Text                       raw buffer + watchers                         (text.cpp)
  └─ Document              caret, selection, frame, markers, edit verbs  (document.cpp)
       └─ SourceDoc        + lexical styling, tracker, bracket match     (sourcedoc.cpp)
            └─ EditFrame   GDI rendering of the above                    (win32/editframe.cpp)
```

`Document` owns three `TextBookmark`s — `_caret`, `_frame` (top-left of the viewport) — plus
an `int _selection` which is a **signed length relative to the caret** (negative = selection
extends backwards). `hasSelection()` is `_selection != 0`; the anchor is implicit. Markers
(breakpoint dots, error lines) live in `Map<size_t /*row*/, int /*style*/> _markers`
(`document.h:92`) and are shifted on row-count changes by `shiftMarkers`
(`document.cpp:830`) driven from `EditFrame::onEditorRowChange` (`editframe.cpp:667`).

### 3.2 Rendering protocol

`EditFrame::paint` (`editframe.cpp:172`) pulls *style runs*, not lines:

```cpp
LineInfo info = _currentDoc->startReading(&writer);   // document.cpp:871
while (true) {
   style  = _styles[_scheme][info.style];
   ... draw buffer with that style at (x,y) ...
   if (!_currentDoc->continueReading(info, &writer))  // document.cpp:888
      break;
}
```

Each iteration produces one maximal run of characters sharing a style. `LineInfo::readLine`
(`document.cpp:20`) asks the document for the run length via the virtual `defineStyle`, then
`Text::copyLineTo` (`text.cpp:137`) copies that many characters into a `LiteralWriter`,
**expanding tabs to spaces** (`text.cpp:167`, using `Settings::tabSize`) and stopping at
`0x0D`. The virtual chain is:

- `Document::defineStyle` (`document.cpp:840`) — handles the selection: returns
  `STYLE_SELECTION` for the selected span, and clamps the run length at the selection
  boundary.
- `SourceDoc::defineStyle` (`sourcedoc.cpp:226`) — if the base style is `STYLE_DEFAULT`,
  overlays (in priority order) the debugger **tracker** band, then row **markers**
  (breakpoints/error lines), then **bracket highlighting**, and otherwise asks the
  lexical styler for the run.

### 3.3 The lexical DFA

`sourcedoc.cpp:19-63`. 21 states, named by consecutive lowercase letters `'a'..'u'`:

| State | Char | Meaning | Style |
|---|---|---|---|
| `lexStart` | `a` | neutral / whitespace | `STYLE_DEFAULT` |
| `lexLookahead` | `b` | one-char lookahead pivot | `STYLE_OPERATOR` |
| `lexOperator`, `lexOperator2`, `lexOperator3` | `c`,`t`,`u` | operators | `STYLE_OPERATOR` |
| `lexLineComment` | `d` | `//` to end of line | `STYLE_COMMENT` |
| `lexKeyword` | `e` | keyword | `STYLE_KEYWORD` |
| `lexMessage` | `f` | message selector | `STYLE_MESSAGE` |
| `lexColon` | `g` | `:` | `STYLE_OPERATOR` |
| `lexObject` | `i` | identifier / object reference | `STYLE_DEFAULT` |
| `lexDigit` | `j` | numeric literal | `STYLE_NUMBER` |
| `lexQuote`, `lexQuote2` | `k`,`l` | `"…"` literal (with `""` escape) | `STYLE_STRING` |
| `lexComment`, `lexComment2` | `m`,`n` | `/* … */` block comment | `STYLE_COMMENT` |
| `lexStick` | `o` | `\|` | `STYLE_OPERATOR` |
| `lexCloseBracket` | `p` | closing bracket | `STYLE_OPERATOR` |
| `lexProperty` | `q` | obsolete property syntax | `STYLE_PROPERTY` |
| `lexHintOrStart`, `lexHint` | `r`,`s` | `#hint` annotations | `STYLE_HINT` |
| (`h` unnamed) | `h` | intermediate inside the table | inherits |

Transition:

```cpp
inline TCHAR makeStep(TCHAR ch, TCHAR state) {          // sourcedoc.cpp:65
   return ch < 128 ? lexDFA[state - lexStart][ch]
                   : lexDFA[state - lexStart][127];     // ALL non-ASCII == DEL
}
```

**Every character ≥ 128 is folded onto column 127.** Identifiers or literals containing
non-ASCII characters are mis-classified. Combined with the UTF-16 buffer this is a real
correctness bug, not a theoretical one.

State → colour is a plain `switch` (`sourcedoc.cpp:70`) mapping to the `STYLE_*` constants
of `ideconst.h:37-53`; the actual colours come from two hard-coded 16-entry
`StyleInfo` tables in the Win32 layer (`editframe.cpp:23` default scheme,
`editframe.cpp:42` "classic" scheme), all of them Courier New 10pt.

### 3.4 The styler's output format and its cost

`LexicalStylist::parse` (`sourcedoc.cpp:112`) runs the DFA over **the whole document** and
emits into two `MemoryDump`s:

```
_lexic : [DWORD style][DWORD end-position]  ...   one pair per style transition
_index : [DWORD lexic-offset] every 0x100 text positions   (INDEX_STEP/INDEX_ORDER, sourcedoc.h:17)
```

`_index` is the seek accelerator: `retrievePosition(pos)` (`sourcedoc.h:25`) does
`pos >> 8` to find the `_lexic` offset near a position, so redrawing a screen at line 5000
does not have to replay the token stream from the top. `proceed(LineInfo&)`
(`sourcedoc.cpp:153`) then walks forward from there to find the run covering the current
position.

The fatal part: `SourceDoc::onChange` (`sourcedoc.cpp:188`) calls `_stylist.parse(getText())`
— a **full document re-lex on every single keystroke** (`Document::insertChar` →
`onChange`, `document.cpp:523`). For a 10k-line source file this is a full O(N) scan plus
two `MemoryDump` rebuilds per character typed. There is no incremental re-lex, no dirty
region, no line-state cache.

### 3.5 Duplication with the compiler's lexer

`elc` has **its own, different** DFA: 23 states (`elc/source.h:18-36`, table driven through
the `DFA<>` template in `elc/dfa.h:21`) with distinct state names (`dfaSlashOperator`,
`dfaWildcard`, `dfaProtected`, `dfaHexInteger`, `dfaReal`…). The IDE's 21-state table is a
hand-maintained parallel copy. The two have already drifted — the IDE has `lexProperty`
marked "obsolete" and has no hex/real distinction, the compiler has no `lexHint` state.
**Any future LSP server must unify these**, and the compiler's table is the one to keep.

### 3.6 Bracket matching

`SourceDoc::highlightCharacter` (`sourcedoc.cpp:309`) runs on every caret move (called from
`Document::setCaret`, `document.cpp:233`). It reads the character under the caret, and if it
is one of `({[` / `)}]` scans with `findBracket` (`sourcedoc.cpp:279`) for the partner.
The scan is deliberately bounded to roughly the visible frame (`frameY += _size.x` at
`sourcedoc.cpp:284` — note this adds the frame **width** to a row bound, an evident bug), so
matching fails silently for distant partners.

---

## 4. UI architecture

### 4.1 The wrapper hierarchy

```
Control                       (window.h:13)   HWND + geometry + min-size constraint
├── Window                    (window.h:73)   + Window_Proc thunk, virtual Class_Proc
│   ├── AppWindow             (appwindow.h:122)   main frame
│   ├── EditFrame             (editframe.h:57)    custom-drawn editor
│   └── Splitter              (splitter.h:16)
├── ToolBar / StatusBar / TabBar / TreeView / ListView / Output      thin SendMessage shells
│   ├── TabBarPlus            (tabbar.h:67)       tabbed container for Output/Messages
│   ├── ContextBrowser        (browser.h:87)      watch tree
│   └── MessageLog            (messagelog.h:37)   compiler error list
└── Dialog                    (dialogs.h:38)      DialogBoxParam wrapper (not a Control)
```

`Control::create` (`window.cpp:58`) is a single `CreateWindowEx` parametrised by four
virtuals — `getStyle()`, `getExStyle()`, `getClassName()`, `getCaption()`. `Window_Proc`
(`window.cpp:158`) is the classic thunk: on `WM_CREATE` it stashes `this` (arriving via
`lpCreateParams`) into `GWL_USERDATA`, and thereafter forwards every message to the virtual
`Class_Proc`. `Window::Class_Proc` (`window.cpp:173`) translates just five messages
(`WM_SIZE`, `WM_SETFOCUS`, `WM_KILLFOCUS`, `WM_CLOSE`, `WM_SETCURSOR`) into virtuals; all
subclasses handle raw messages directly.

### 4.2 Message routing

Two channels, both `SendMessage`-based and both synchronous:

**Child → parent** via `Control::notify` (`window.cpp:113-144`), which builds an `NMHDR` (or
one of the extended variants `ExtNMHDR`, `ExtNMHDR2`, `MessageNMHDR`, `Message2NMHDR`,
`LineInfoNMHDR` from `idecommon.h:345-412`) and `SendMessage(_parent, WM_NOTIFY, …)`.
`AppWindow::onNotify` (`appwindow.cpp:466`) is a single 80-line switch that mixes
**standard** Win32 notification codes (`TCN_SELCHANGE`, `NM_DBLCLK`, `NM_RCLICK`,
`TTN_GETDISPINFO`, `TVN_KEYDOWN`, `TVN_ITEMEXPANDING`) with **custom** IDE codes
(`IDE_EDITOR_CHANGED`, `IDE_DEBUGGER_STEP`, `IDM_COMPILER_SUCCESSFUL`, …,
`ideconst.h:59-72`) in the same numeric space. This works only because the custom codes
(100-303) happen not to collide with the common-control ones.

**Menu/toolbar/accelerator → command** via `WM_COMMAND` → `AppWindow::doCommand`
(`appwindow.cpp:944`), a 250-line switch over `IDM_*`. There is no command object, no
enabled-state predicate; menu and toolbar enablement is set imperatively by hand in
`onEditorShow` / `onEditorHide` / `onProjectChanged` / `onDebuggerStart` / `onDebuggerStop`
(`appwindow.cpp:728-937`) — 60+ scattered `menu.enableItemById(...)` calls that must be kept
mutually consistent by discipline alone.

### 4.3 Layout

`LayoutManager` (`layout.cpp:56`) is a 35-line five-slot dock: top, bottom, left, right,
client. `AppWindow` wires it once in its constructor (`appwindow.cpp:311-314`) — toolbar on
top, watch splitter on the left, output splitter at the bottom, tab bar as client — and
`onResize` (`appwindow.cpp:424`) hands it the client rectangle minus the status bar.
Splitters are `Control` decorators: a `Splitter` *wraps* its client and reports
`client.width + 3` as its own width (`splitter.cpp:74`), so dragging the splitter resizes
the client and posts `IDM_LAYOUT_CHANGED` back to the frame (`splitter.cpp:197`). Dragging
installs a **global `WH_MOUSE_LL` hook** (`splitter.cpp:149`) with process-wide static state
(`splitter.cpp:17-18`).

### 4.4 Rendering

`EditFrame` does not use any Win32 text control. It renders into a `Canvas`
(`idecommon.h:195`, an `HDC` wrapper) with manual double-buffering: `_zbuffer` is a
compatible DC + bitmap (`editframe.cpp:298-303`), painted only when the `_cached` flag is
false, then `BitBlt`ed to the screen. Text goes out with `ExtTextOut(ETO_OPAQUE|ETO_CLIPPED)`
(`idecommon.cpp:323`).

Critically, **character positions are computed as `avgCharWidth * column`**
(`editframe.cpp:255`, `editframe.cpp:277`, `editframe.cpp:362`). The IDE is a fixed-pitch
editor by assumption; the `Canvas::TextWidth` call that would do it properly is commented out
at `editframe.cpp:254`. Caret, mouse hit-testing and scroll ranges all share this assumption.

### 4.5 How much is genuinely abstracted?

| Layer | Abstracted? | Verdict |
|---|---|---|
| `Control`/`Window` geometry + lifecycle | partially | The vocabulary (`create`/`show`/`resize`/`setFocus`) would survive a port; the implementation is 100% Win32. |
| `LayoutManager` | **yes** | 137 lines, no Win32 types except `Rectangle`. Genuinely portable logic. |
| `Canvas`/`Style`/`Font`/`Colour` | in name only | Every method is a GDI call; `Font::_fontID` is an `HFONT`, `Colour` is a packed COLORREF. |
| Common-control wrappers | no | `ToolBar::assign` speaks `TBBUTTON`; `TreeView` returns `HTREEITEM` (typedef'd to `TreeViewItem` but used as an integer key in `browser.cpp:195`). |
| Notification protocol | no | The core `Document` is fine, but everything above it passes `NMHDR*`. |
| Dialogs | no | `.rc` templates + `DialogBoxParam`; the layout lives in the resource script. |

Estimate: of the 8,802 Win32 lines, roughly **300-400 lines of layout/geometry logic** are
portable ideas. The rest is API-shaped.

---

## 5. THE DEBUGGER

This is the most valuable and the most interesting subsystem in `elide`. It is a real
native debugger: it launches the ELENA executable as a debuggee, patches INT3 breakpoints,
single-steps with the x86 trap flag, sets hardware breakpoints in DR0/DR7, reads the
debuggee's memory with `ReadProcessMemory`, walks ELENA object headers and VMTs *in the
target process*, and maps machine addresses back to source coordinates.

### 5.1 Split of responsibility

| Layer | File | Responsibility |
|---|---|---|
| `AppDebugger` | `appwindow.cpp:49-162` | Turns controller callbacks into `WM_NOTIFY` messages for the UI thread; resolves `.dnl` debug-module paths |
| `DebugController` | `debugcontroller.cpp` (691) | **Platform-independent**: loads debug info, maps address↔source, decides what a "step" means, inspects live objects |
| `Debugger` | `win32/debugger.cpp` (607) | **Platform-specific**: `CreateProcess`, `WaitForDebugEvent`, breakpoints, registers, memory |
| `ProcessContext` | `win32/debugger.h:72` | The debuggee's thread context + accessors (`EIP()`, `Self()`, `Local(n)`, `ClassVMT()`, …) |
| `BreakpointContext` | `win32/debugger.h:123` | The breakpoint address→original-byte map and re-arming state machine |
| `DebugEventManager` | `win32/debugger.h:24` | 4 Win32 auto/manual events used as the controller↔UI handshake |
| `ContextBrowser` / `DebuggerWatch` | `browser.cpp` | Renders the object graph the controller walks |

### 5.2 Launching the debuggee

`AppWindow::startDebugger` (`appwindow.cpp:1676`) builds the target path from the project
(`getTarget()`, `getArguments()`) and calls
`DebugController::start(exePath, commandLine, debugMode, breakpoints)`
(`debugcontroller.cpp:372`), which:

1. resets the `Debugger`,
2. if debug mode: derives `<target>.dn` and calls `loadDebugData` (§5.5); on failure the
   session is refused with *"Invalid or absent debug info"*,
3. resolves and arms the pending user breakpoints (`loadBreakpoints`, `debugcontroller.cpp:336`),
4. calls the private `start()` (`debugcontroller.cpp:221`).

`DebugController::start()` initialises the four events, sets `DEBUG_SUSPEND`, then
`_debugger.startThread(this)` — `CreateThread` on `debugEventThread` (`debugger.cpp:16`),
which immediately calls back `controller->debugThread()`. **All debug-API calls happen on
that one thread**, which matters because Win32 requires `WaitForDebugEvent` and
`ContinueDebugEvent` to be issued from the thread that attached.

Process creation (`Debugger::startProcess`, `debugger.cpp:343`):

```cpp
CreateProcess(exePath, (TCHAR*)cmdLine, NULL, NULL, FALSE,
              CREATE_NEW_CONSOLE | DEBUG_PROCESS,        // debugger.cpp:357
              NULL, currentPath, &si, &pi);
```

`DEBUG_PROCESS` (not `DEBUG_ONLY_THIS_PROCESS`) means **child processes of the debuggee are
also debugged**. The returned `hProcess`/`hThread` from `PROCESS_INFORMATION` are closed
immediately (`debugger.cpp:362-366`); the handles actually used come from the
`CREATE_PROCESS_DEBUG_EVENT`. There is no attach-to-running-process path.

### 5.3 The debug event state machine

Two nested loops. The **outer** loop is the controller's event pump
(`DebugController::debugThread`, `debugcontroller.cpp:64`):

```
                       ┌───────────────────────────────────────────┐
                       │  WaitForMultipleObjects(4 events, FALSE)  │  debugger.cpp:47
                       └──────┬────────┬────────┬─────────┬────────┘
       (index order = priority)│        │        │         │
              DEBUG_CLOSE(0)   │   SUSPEND(1)  RESUME(2)  ACTIVE(3)
                    │          │        │        │         │
   TerminateProcess │          │  reset ACTIVE   │   ┌─────┴────────────────────────┐
   + ContinueDebug  │          │  _running=false │   │ exception pending?           │
   set ACTIVE       │          │                 │   │  yes → processStep(), reset, │
   reset CLOSE      │          │                 │   │        run()                 │
                    │          │   _running=true │   │  no  → proceed(100 ms)       │
                    │          │   ContinueDebug │   │        · trapped → reset     │
                    │          │   reset RESUME  │   │          ACTIVE, processStep,│
                    │          │   set ACTIVE ───┘   │          _running=false      │
                    │          │                     │        · not trapped → run() │
                    └──────────┴─────────────────────┴──────────────────────────────┘
                                        loop while _debugger.isStarted()
                                                     │
                                  on exit: _events.close(); onStop(proceedCheckPoint())
```

Because `WaitForMultipleObjects` with `bWaitAll = FALSE` returns the **lowest signalled
index**, the event array order in `debugger.h:18-21` encodes a fixed priority:
`CLOSE > SUSPEND > RESUME > ACTIVE`. The 100 ms timeout in the ACTIVE branch means that
while the debuggee runs freely, the debug thread spins `WaitForDebugEvent` ten times a
second forever.

The **inner** loop is `Debugger::processEvent` (`debugger.cpp:375`), one `WaitForDebugEvent`
per call:

| `dwDebugEventCode` | Action | Line |
|---|---|---|
| `CREATE_PROCESS_DEBUG_EVENT` | capture `hProcess`/`hThread`; **install all pending INT3 breakpoints**; close `hFile` | 383 |
| `EXIT_PROCESS_DEBUG_EVENT` | null the handles, `started = false` | 389 |
| `CREATE_THREAD_DEBUG_EVENT` | ignored | 394 |
| `EXIT_THREAD_DEBUG_EVENT` | **`started = false`** — any thread exit ends the session | 396 |
| `LOAD_DLL_DEBUG_EVENT` | close `hFile` only (no module bookkeeping) | 399 |
| `UNLOAD_DLL`, `OUTPUT_DEBUG_STRING` | ignored | 402-405 |
| `RIP_EVENT` | `started = false` | 406 |
| `EXCEPTION_DEBUG_EVENT` | `processException` then re-`refresh` the context | 409 |

Every event first calls `ProcessContext::refresh(pid, tid)` (`debugger.cpp:82`), which does
`GetThreadContext(hThread, CONTEXT_FULL)` — and contains a hot-fix that rewrites `SegFs` to
`0x38` when it comes back zero (`debugger.cpp:89-92`), a symptom of the debuggee's
hand-written assembly prologue.

Exception dispatch (`Debugger::processException`, `debugger.cpp:417`):

| Exception | Handling |
|---|---|
| `EXCEPTION_SINGLE_STEP` | 1. `breakpoints.processStep()` re-arms a temporarily-removed breakpoint and applies the stack-level guard (see §5.4); if it consumed the event, done. 2. If `EIP ∈ [minAddress, maxAddress]` (the span of all known step addresses) call `Debugger::processStep()`. 3. If we did not stop, set the trap flag again → keep single-stepping. |
| `EXCEPTION_BREAKPOINT` | `breakpoints.processBreakpoint()` (see §5.4). If it was one of ours: `context.state = steps.get(EIP)`, `trapped = true`, `stepMode = false`, set trap flag so the byte can be restored after one instruction. |
| anything else, first chance | `needToHandle = true` → the next `ContinueDebugEvent` passes `DBG_EXCEPTION_NOT_HANDLED` so the debuggee's own SEH gets a turn (`debugger.cpp:480`) |
| anything else, second chance | record `{code, address}` in `ProcessException` and **`TerminateProcess`** (`debugger.cpp:444-446`); the controller later renders it via `getException_T` (`debugcontroller.cpp:16`, 19 mapped exception codes plus `ELENA_ERR_OUTOF_MEMORY`) |

`Debugger::processStep` (`debugger.cpp:452`) is the "did we land on a source line?" test:

```cpp
context.state = steps.get(context.context.Eip);   // MemoryMap<int, void*> address → DebugLineInfo*
if (context.state != NULL) {
   trapped = true;  stepMode = false;  proceedCheckPoint();
}
```

`proceedCheckPoint` (`debugger.cpp:462`) implements ELENA's "did that operation succeed?"
notion: if the previous step was flagged as a `dsProcedureStep`, the debugger set
`atCheckPoint`, and here it reports failure when **EAX == 0** — surfaced in the status bar
as *"Operation failed"* (`debugcontroller.cpp:213`).

### 5.4 Breakpoints

Two completely different mechanisms, chosen by purpose:

**Software breakpoints (INT3) — for user breakpoints.**

```cpp
unsigned char ProcessContext::setSoftwareBreakpoint(size_t addr) {   // debugger.cpp:144
   unsigned char code, terminator = 0xCC;
   readDump(addr, &code, 1);            // ReadProcessMemory
   writeDump(addr, &terminator, 1);     // WriteProcessMemory
   return code;                         // original byte, saved in the map
}
```

`BreakpointContext::breakpoints` is `Map<size_t /*address*/, char /*original byte*/>`
(`debugger.h:125`). Adding a breakpoint before the process exists just records the address
with byte 0 (`debugger.cpp:241`); the real patching happens in
`setSoftwareBreakpoints` on `CREATE_PROCESS_DEBUG_EVENT` (`debugger.cpp:257`). There is no
`FlushInstructionCache` call — harmless on x86, a bug waiting on other architectures.

Hitting one (`processBreakpoint`, `debugger.cpp:310`):

```
1. INT3 executed ⇒ EIP is one past the patch. Look up  EIP - 1  in the map.
2. SetThreadContext to rewind EIP to the breakpoint address       (setEIP, debugger.cpp:199)
3. WriteProcessMemory the original byte back                      (debugger.cpp:317)
4. softwareBreakpoint = true; nextBreakpoint = addr
5. set trap flag → after the *next* single instruction, processStep() re-patches 0xCC
```

**Hardware breakpoints (DR0) — for stepping and run-to-cursor.**

```cpp
void ProcessContext::setHardwareBreakpoint(size_t bp) {   // debugger.cpp:119
   context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
   context.Dr0 = bp;
   context.Dr7 = 0x000001;    // L0 only: local enable, break-on-execute, length 1
   SetThreadContext(hThread, &context);
   breakpointFlag = true;
}
```

**Only DR0 is ever used** — exactly one hardware breakpoint at a time. Chosen for stepping
because it needs no memory patching and no restore dance. Edge case: if the target address
equals the current EIP, `setHardwareBreakpoint` falls back to setting the trap flag
(`debugger.cpp:269-272`).

**Stack-level control** is how "step over" avoids stopping inside recursive calls.
`setHardwareBreakpoint(addr, ctx, withStackLevelControl=true)` records `stackLevel = EBP`
(`debugger.cpp:276`). On the hit, `processStep` (`debugger.cpp:296-305`) clears DR0 and
compares: if the current `EBP < stackLevel`, we are in a *deeper* frame (a recursive
re-entry), so the breakpoint is re-armed via trap flag and execution continues.

| Operation | Mechanism | Entry point |
|---|---|---|
| User breakpoint (F5 / margin click) | INT3, persistent | `DebugController::toggleBreakpoint`, `debugcontroller.cpp:351` |
| Run to cursor | DR0, with stack control | `debugcontroller.cpp:408` |
| Step over target | DR0, with stack control | `debugcontroller.cpp:452` |
| Step into (non-atomic) | trap flag, every instruction | `Debugger::setStepMode`, `debugger.cpp:497` |
| First `Run`/`Step` into `main` | DR0 at `_entryPoint` | `debugcontroller.cpp:433`, `:468` |

### 5.5 Debug information: format and loading

Two artefacts are produced by the toolchain:

**Per-module `*.dnl`** (an ELENA `Module` with two negative-id sections,
`elenaconst.h:226-227`):

| Section | Id | Contents |
|---|---|---|
| `DEBUG_LINEINFO_ID` | `(size_t)-1` | a flat array of fixed-size `DebugLineInfo` records |
| `DEBUG_STRINGS_ID` | `(size_t)-2` | the string pool for field/local/class names |

`DebugLineInfo` (`engine/elena.h:133`) is 24 bytes:

```cpp
struct DebugLineInfo {
   DebugSymbol symbol;            // dsStep / dsClass / dsField / dsLocal / dsProcedure / dsEnd …
   int col, row, length;          // source coordinates of the step, 0-based row
   union {
      struct { int nameRef; int flags; } symbol;   // class/symbol: name offset + VMT flags
      struct { size_t address;         } step;     // step: FILLED IN AT LOAD TIME
      struct { int nameRef; int level; } local;    // local: name offset + EBP slot index
   } addresses;
};
```

`DebugSymbol` (`elenaconst.h:203`) — the high nibble is the class (`dsDebugMask = 0xF0`):

| Symbol | Value | Meaning |
|---|---:|---|
| `dsStep` | `0x10` | a normal source step |
| `dsEOP` | `0x11` | end of procedure |
| `dsVirtualStep` | `0x12` | synthetic step; the debugger advances past it without resuming |
| `dsVirtualEnd` | `0x13` | synthetic end; **not** registered as a stoppable address (`debugcontroller.cpp:309`) |
| `dsProcedureStep` | `0x14` | step whose *result* should be checked (`EAX == 0` ⇒ failure) |
| `dsAtomicStep` | `0x18` | external/native code — "step into" degrades to "step over" |
| `dsSymbol` / `dsClass` | `0x20` / `0x30` | opens a symbol or class scope |
| `dsBase` | `0x40` | the `self` variable |
| `dsField` | `0x50` | an instance field |
| `dsLocal` | `0x60` | a local variable |
| `dsProcedure` / `dsEnd` | `0x70` / `0x80` | scope open / close |

The records are emitted by `ByteCodeCompiler` (`engine/bccompiler.cpp:56-170`:
`openClassDebugInfo`, `openSymbolDebugInfo`, `openProcedureDebugInfo`,
`writeFieldDebugInfo`, `writeLocal`, `writeBreakpoint`, `endDebugInfo`).

**Per-executable `<target>.dn`** (`elc/win32/linker.cpp:589`):

```
"EN.D10!"                        DEBUG_MODULE_SIGNATURE, elenaconst.h:150
DWORD entryPoint                 absolute VA of the startup symbol
── then the linker's ".debug" section, a stream of: ──
  literal reference name         '#' prefix ⇒ symbol, otherwise ⇒ class   (jitlinker.cpp:413/423)
  [DWORD vmtAddress]             classes only, = VMT VA + elVMTOffset     (jitlinker.cpp:433)
  DWORD stepAddress ...          one per breakpoint, in emission order    (jitlinker.cpp:48)
```

The linker relocates that section along with the image (`linker.cpp:407-410`), so the
addresses in `.dn` are final virtual addresses.

**Loading** (`DebugController::loadDebugData`, `debugcontroller.cpp:242` → `:318`):

```
read entryPoint
while not EOF:
    read reference name
    loadSymbolDebugInfo(name, reader)                      debugcontroller.cpp:258
        ├─ loadDebugModule(name) → open <name>.dnl         AppDebugger, appwindow.cpp:134
        ├─ mapReference(name) → offset of this symbol's first record
        ├─ if a class: read the VMT address ⇒ _classes[vmtPtr] = &record
        └─ walk records until the matching dsEnd, IN PLACE:
              dsField / dsLocal  → replace nameRef INDEX with a POINTER into the string section
              dsStep family      → fill addresses.step.address from the .dn stream
                                   and, unless dsVirtualEnd, _debugger.addStep(addr, &record)
```

Two consequences worth calling out. First, the loader **mutates the mapped module image in
the IDE's own address space**, converting a serialisable format into a pointer-bearing one
— `((DebugLineInfo*)current)->addresses.symbol.nameRef = (ref_t)stringReader.Address();`
(`debugcontroller.cpp:303`). This is fast but means `nameRef` silently changes meaning
between "on disk" and "in memory", and it casts pointers through 32-bit `ref_t`. Second,
`Debugger::addStep` (`debugger.cpp:487`) both fills `MemoryMap<int, void*> steps` and widens
`[minAddress, maxAddress]`, which is the fast reject used during single-stepping.

### 5.6 Address ↔ source mapping

**Source → address** (setting a breakpoint or running to cursor):
`findNearestAddress(module, row, col)` (`debugcontroller.cpp:184`) **linearly scans the whole
`DEBUG_LINEINFO` array** of the module for a `dsStep` record with a matching `row`, keeping
the one whose column is ≥ `col` and smallest. `0xFFFFFFFF` means "no code on that line". At
O(records) per breakpoint toggle this is fine for the sizes involved but is the obvious
place a real index would go.

**Address → source** (at every stop): the current position is *not* recovered from EIP but
from `context.state`, the `DebugLineInfo*` stored when the step was recognised.
`seekDebugLineInfo(ptr, &moduleName)` (`debugcontroller.cpp:129`) identifies the owning
module by asking `getDebugModule` (`debugcontroller.cpp:113`) which loaded module's
`DEBUG_LINEINFO` section **contains that host pointer**:

```cpp
size_t starting = (size_t)section->getArray();
if (starting <= address && (address - starting) < section->Length()) return module;
```

Then `showCurrentModule` (`debugcontroller.cpp:679`) fires `onLoadModule(name)` when the
module changed and `onStep(name, row, col, length)` always. `AppDebugger::onStep`
(`appwindow.cpp:59`) packages it into a `LineInfoNMHDR` and `SendMessage`s the main window,
which opens the source file if needed (`AppWindow::loadModule`, `appwindow.cpp:1655`) and
calls `EditFrame::setTracker(info, STYLE_TRACE_LINE, STYLE_TRACE)` (`appwindow.cpp:895`) —
this is why the current statement is highlighted as an exact span inside a banded line
(`SourceDoc::defineStyle`, `sourcedoc.cpp:237-249`).

### 5.7 Step-into / step-over logic

Both live in the controller and are driven entirely by the `DebugSymbol` of the current
record. `getNextStep(step, alwaysReturn)` (`debugcontroller.cpp:167`) walks forward in the
record array, skipping `dsLocal` entries, and returns the next `dsStep`-family record —
but only if its address differs from the current EIP (unless `alwaysReturn`).

**`stepOver`** (`debugcontroller.cpp:425`):

```
if not started:  hardware breakpoint at _entryPoint (no stack control); done
else:
    lineInfo = context.state
    if lineInfo->symbol == dsVirtualStep:
         processVirtualStep(getNextStep(lineInfo, true))   ← advance state, DO NOT resume
         processStep(); return                             ← report the new position immediately
    if lineInfo->symbol & dsProcedureStep:  setCheckMode()  ← check EAX after the step
    next = getNextStep(lineInfo)
    if next:  setBreakpoint(next->address, withStackLevelControl = true)   ← DR0
    else:     setStepMode()                                               ← trap flag
set DEBUG_RESUME
```

**`stepInto`** (`debugcontroller.cpp:460`) is identical except for the middle:

```
    if lineInfo->symbol & dsAtomicStep:      ← external / native code: cannot step in
         same next-step breakpoint as stepOver
    else setStepMode()                       ← single-step every instruction until we land
                                                on any address in `steps` (which may be inside
                                                the callee) — this is what "entering" means
```

So "step into" is implemented as **brute-force single-stepping** with a fast address-range
reject (`debugger.cpp:424`). Stepping into a call that goes through a lot of runtime code is
proportionally slow, which is precisely why `dsAtomicStep` exists.

The `dsVirtualStep` case deserves emphasis: it advances the *logical* position without
resuming the process at all — several ELENA source constructs compile to zero instructions,
and this is how the debugger walks over them.

### 5.8 Watch / object inspection

The controller walks live ELENA objects in the debuggee. Object layout knowledge is
encoded in `ProcessContext`:

| Accessor | Reads | Line |
|---|---|---|
| `ObjectPtr(addr)` | 4 bytes at `addr` | `debugger.cpp:189` |
| `ClassVMT(objPtr)` | 4 bytes at **`objPtr - 4`** — the object's VMT pointer | `debugger.cpp:169` |
| `VMTFlags(vmtPtr)` | 4 bytes at **`vmtPtr - 8`** — the VMT flag word | `debugger.cpp:179` |
| `Local(n)` | `EBP - n*4` (frame slot address, not the value) | `debugger.h:93` |
| `Current(n)` | `ESP + n*4` | `debugger.h:94` |
| `Self()` | `EDI` — the ELENA `self` register | `debugger.h:92` |
| `readDump(addr, buf, len)` | `ReadProcessMemory` | `debugger.cpp:155` |

**Auto-watch** (`readAutoContext`, `debugcontroller.cpp:586`): from the current record, walk
the `DebugLineInfo` array **backwards** (`index--`) until `dsProcedure`, collecting:
- `dsBase` → the `self` object, at frame slot `addresses.local.level` (negative levels only),
- `dsLocal` → a named local, whose name is the pointer patched in at load time.

Each pointer goes to `readObject` (`debugcontroller.cpp:573`), which resolves the class:

```
seekClassInfo(objAddr):                                  debugcontroller.cpp:140
   vmtPtr = ClassVMT(objAddr)                            read [obj-4] in the debuggee
   if VMTFlags(vmtPtr) & elRoleVMT:                      a "role" wrapper?
        vmtPtr = ClassVMT(vmtPtr)                        → unwrap to the owner class
   position = _classes[vmtPtr]                           VMT VA → DebugLineInfo* (from .dn)
   className = module->resolveReference(position - sectionStart)
```

Then `readContext` (`debugcontroller.cpp:617`) dispatches on the class's debug flags
(`info->addresses.symbol.flags & elDebugMask`, `elenaconst.h:268`):

| Flag | Reads from the debuggee | Rendered as |
|---|---|---|
| `elDebugLiteral` | 260 `wchar_t`; the leading DWORD is the length | the string |
| `elDebugDWORD` | 4 bytes | int (hex or decimal per `Settings::hexNumberMode`) |
| `elDebugQWORD` | 8 bytes | `__int64` |
| `elDebugReal64` | 8 bytes | `double` |
| `elDebugArray` | up to 100 slots + the length DWORD at **`obj - 8`** | indexed child nodes `[0]`, `[1]`, … |
| class name == `NIL_CLASS` | — | `<nil>` |
| otherwise | walks the class's `dsField` records, reading consecutive 4-byte slots | named child nodes |

The UI side (`browser.cpp`) stores each object's address in the tree item's `lParam`
(`TreeView::setParam`, `treeview.cpp:79`) and matches existing nodes by caption prefix to
avoid rebuilding the tree on every step (`DebuggerWatch::write`, `browser.cpp:74`).
Recursion is capped at `_deepLevel < 3` (`browser.cpp:61`); deeper levels are expanded
lazily on `TVN_ITEMEXPANDING` (`appwindow.cpp:664` → `ContextBrowser::browse`,
`browser.cpp:285`). `DebuggerAutoWatch::refresh` (`browser.cpp:230`) does mark-and-sweep:
mark all items, repopulate, delete anything still marked.

### 5.9 Hard limits of the current debugger

| Limit | Evidence |
|---|---|
| **32-bit x86 only** | `context.Eip/Ebp/Esp/Eax`, `Dr0/Dr7`, hard-coded 4-byte object slots, `0xFFFFFFFF` sentinels, `(int)` casts of addresses (`debugcontroller.cpp:609`) |
| **Single-threaded debuggee only** | `EXIT_THREAD_DEBUG_EVENT` ends the whole session (`debugger.cpp:396`); one `hThread`; DR0 is per-thread but set on one thread only |
| **One hardware breakpoint** | only `Dr0`/`Dr7 = 1` is ever written |
| No attach to running process | only `CreateProcess` |
| No call stack | there is no frame walker at all; the watch shows only the innermost frame's locals |
| No expression evaluation | the watch can only display what the debug records name |
| No conditional breakpoints, no watchpoints | `Breakpoint` is `{source, row, address}` (`debugcontroller.h:36`) |
| Data races | the watch tree is populated from `readContext` on the **UI thread** while the debug loop runs on another thread; no lock anywhere |
| Second-chance exceptions kill the process | `TerminateProcess` at `debugger.cpp:446` — no post-mortem inspection |

---

## 6. Project & settings persistence

Both file formats are the same INI dialect, parsed by `IniConfigFile`
(`common/config.cpp:24` load, `:64` save): `[section]` headers, `key=value` lines, and
bare `key` lines (value = `DEFAULT_STR`). No comments, no escaping, no quoting; the parser
splits at the **first** `=`. Save order follows hash-map iteration, so files are rewritten in
arbitrary section order. Booleans are Visual-Basic style: `-1` = true, absent/`0` = false.

### 6.1 The `.prj` file

Model class `ProjectInfo` (`ideproject.h:15`); keys in `ideconst.h:305-322`.

| Section | Key | Meaning | Accessor |
|---|---|---|---|
| `[project]` | `executable` | output binary name | `getTarget`, `ideproject.cpp:50` |
| | `output` | intermediate output directory | `getOutputPath`, `:55` |
| | `package` | root namespace of the project | `getPackage`, `:40` |
| | `entry` | start symbol, e.g. `'entry` | `getStartSymbol`, `:45` |
| | `template` | `console` / `gui` — selects an `elc` template cfg | `getTemplate`, `:65` |
| | `projecttype` | `0` library, `1` console, `2` GUI (`ProjectType`, `elenaconst.h:240`) | `getType`, `:82` |
| | `debuginfo` | `-1` ⇒ emit `.dn`/`.dnl` | `getDebugInfoEnabled`, `:60` |
| | `arguments` | debuggee command line | `getArguments`, `:35` |
| | `options` | extra `elc` flags, appended verbatim | `getOptions`, `:70` |
| | `warn:unresolved` etc. | passed through to `elc` untouched by the IDE | — |
| `[files]` | *relative path* (no value) | source files, relative to the project dir | `SourceFiles()`, `ideproject.h:54` |
| `[forwards]` | `name` = `reference` | forward declarations | `Forwards()`, `ideproject.h:59` |
| `[linker]` | — | read by `elc`, never touched by the IDE | — |

Real example (`examples/bsort/bsort.prj`):

```ini
[project]
executable=bsort.exe
entry='entry
template=console
projecttype=1
debuginfo=-1
warn:unresolved=0

[files]
bsort.l

[forwards]
'program=bsort'program
```

**Legacy migration** happens transparently on open (`ideproject.cpp:272` → `convert`,
`ideproject.cpp:16`): the pre-1.5 keys `type` (0/1/2 with a *different* meaning) and `debug`
are rewritten to `projecttype` and `debuginfo`, and the file is **saved immediately** —
opening an old project silently rewrites it on disk.

Two traps for a reimplementation:
- `ProjectInfo::getType()` (`ideproject.cpp:82`) does **not** return the stored value; it
  remaps `ptConsole→1, ptGUI→2, ptLibrary→0` for the settings-dialog combo box index. The
  stored value and the returned value share the same range but not the same meaning.
- `[files]` keys are stored **lower-cased and relativised** (`setName`, `ideproject.cpp:215`;
  `includeSource`, `:333`), using `PathRelativePathTo`. Name↔path resolution
  (`retrieveName` `:226` / `retrievePath` `:251`) walks project package root → library root →
  bare file name, and is what lets the debugger find `bsort'program` on disk.

### 6.2 `ide.cfg`

Always `<appdir>/ide.cfg` (`winmain.cpp:76`), loaded before the window is created and saved
unconditionally on exit (`winmain.cpp:204`) — including after a crash caught by the
catch-all in `WinMain`.

| Section | Key | Type | Default |
|---|---|---|---|
| `[settings]` | `defaultproject` | path | — |
| | `compileroutput`, `highlightsyntax`, `linenumbers`, `tabusing`, `unicode_output`, `remeber_path` *(sic)*, `remeber_project` *(sic)*, `autocomp`, `tabscore`, `highlightbrackets` | `-1`/`0` | see `idesettings.cpp:27-51` |
| | `style` | int, 0..`SCHEME_COUNT` | 0 |
| | `tabsize` | int, clamped 1..20 | 4 |
| | `encoding` | `FileEncoding` enum as int | `feAnsi` |
| `[plugins]` | *DLL path* (no value) | list | empty |
| `[recentfiles]` | *path* (no value) | max 10, MRU | — |
| `[recentprojects]` | *path* (no value) | max 10, MRU | — |

Loaded by `Settings::load` (`idesettings.cpp:134`) + `AppWindow::loadHistory`
(`appwindow.cpp:2131`); written back by `Settings::save` (`idesettings.cpp:183`).
`highlightbrackets` is written **only when false** (`idesettings.cpp:202`) — an
absent-means-true key, which any reimplementation must honour.

All settings are `static` members of a global `Settings` struct (`idesettings.h:40`), read
directly from deep inside the text engine (`Settings::tabSize` in `text.cpp:168`,
`text.cpp:741`) and the styler (`Settings::highlightSyntax` in `sourcedoc.cpp:190`). There is
no settings-change notification; `EditFrame::reloadSettings` (`editframe.cpp:312`) polls the
globals after the settings dialog closes.

---

## 7. Plugin system

The ABI is 52 lines (`idecommon/plugins.h`), shared by the IDE and by plugin DLLs.

```cpp
#define PLUGIN_REGISTER_FUN "RegisterPlugin"                       // plugins.h:14

enum PluginResult { pgrNone = 0, pgrSuccessful = 1, pgrNeedToRepaint = 3 };

class _Document { };                                               // plugins.h:27  — EMPTY

typedef PluginResult (*OnKeyPressedType)(TCHAR ch, _Document*);     // plugins.h:33
typedef PluginResult (*OnKeyDownType)(int keyCode, bool kbShift, bool kbCtrl, _Document*);

class _PluginManager {                                              // plugins.h:38
public:
   virtual OnKeyPressedType registerOnKeyPressed(OnKeyPressedType hook) = 0;
   virtual OnKeyDownType    registerOnKeyDownType(OnKeyDownType hook) = 0;
   virtual ~_PluginManager() {}
};

typedef int (__cdecl *RegisterFunction)(_PluginManager* manager);   // plugins.h:49
```

Loading (`PluginManager::registerPlagin` *(sic)*, `pluginmanager.cpp:13`) is 8 lines:

```cpp
HMODULE hModule = ::LoadLibrary(path);
if (hModule) {
   RegisterFunction function = (RegisterFunction)::GetProcAddress(hModule, PLUGIN_REGISTER_FUN);
   function(this);                     // ← no NULL check on `function`
}
```

Paths come from `[plugins]` in `ide.cfg`; registration happens at the end of
`AppWindow::create` (`appwindow.cpp:376-381`). Hooks form a chain of responsibility: the
registrar returns the previous hook, which the new plugin is expected to call
(`autoform.cpp:45-53`).

Invocation: the editor calls `_pluginManager->onKeyPressed(ch, dynamic_cast<_Document*>(_currentDoc))`
**before** its own handling (`editframe.cpp:394`); a `pgrSuccessful` result short-circuits
the built-in behaviour.

### 7.1 Assessment

| Issue | Detail |
|---|---|
| **`_Document` is an empty class** | The plugin receives a pointer it cannot do anything with. `Document` inherits `_Document` (`document.h:75`) but exposes no virtual API through it. The hook can only *veto* a keystroke, never inspect or edit. |
| **`onKeyDown` is dead** | Registered (`pluginmanager.h:31`) and callable (`pluginmanager.h:49`) but `EditFrame::onKeyDown` (`editframe.cpp:435`) never calls it. |
| **Uninitialised path** | `registerPlagin` does not check `GetProcAddress` for `NULL` before calling it. |
| **Fragile C++ ABI** | A pure-virtual class crosses the DLL boundary; plugin and host must share compiler, ABI, and `_UNICODE` setting (`TCHAR` is in the signature). |
| **The only plugin is a stub** | `autoform` forwards to the previous hook and returns `pgrNone`. `Plugin::onKeyPressed` (`autoform.cpp:23`) **falls off the end without returning a value** in the non-null branch — undefined behaviour. |

Effective conclusion: the plugin system is a two-hook keystroke filter, unfinished, with no
working client. It carries no design worth preserving.

---

## 8. The abandoned GTK port

`elenasrc/ide/gtk/main.cpp` — **49 lines, and they are the stock GTK+ 2 "Hello World"
sample**:

```c
static void helloWorld (GtkWidget *wid, GtkWidget *win) {          // gtk/main.cpp:4
  dialog = gtk_message_dialog_new (…, "Hello World!");
  gtk_dialog_run (GTK_DIALOG (dialog));  gtk_widget_destroy (dialog);
}
int main (int argc, char *argv[]) {                                 // gtk/main.cpp:14
  gtk_init (&argc, &argv);
  win  = gtk_window_new (GTK_WINDOW_TOPLEVEL);   gtk_window_set_title (…, "Hello World");
  vbox = gtk_vbox_new (TRUE, 6);
  button = gtk_button_new_from_stock (GTK_STOCK_DIALOG_INFO);   /* → helloWorld */
  button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);         /* → gtk_main_quit */
  gtk_widget_show_all (win);  gtk_main ();  return 0;
}
```

The matching build file `ide/codeblocks/elide_gtk.cbp` compiles **exactly one unit**
(`../gtk/main.cpp`) against `pkg-config gtk+-2.0`, producing a binary named `elide-gtk`.

What exists: a window, a vbox, two stock buttons, a modal message dialog.
What does **not** exist: any reference to `Text`, `Document`, `SourceDoc`, `ProjectInfo`,
`DebugController`, or any other IDE type; any `#include` from `ide/`; any attempt at a
portable `Control` abstraction.

**The port got 0% of the way.** It is a scaffolding commit that was never followed up. Its
only informational value is negative: it tells us that when the author looked at the
problem, the first thing he had to do was start a brand-new `main()` — because nothing in
`ide/` could be compiled outside Win32 (see §1.3). That fact is itself the strongest
argument against the "port the UI" option in §10.

---

## 9. Portability audit

Difficulty scale: **Trivial** (recompiles as-is) · **Low** (mechanical edits) ·
**Medium** (real work, design intact) · **High** (redesign required) ·
**Rewrite** (nothing survives).

| Subsystem | LOC | Win32-bound? | Difficulty | Notes |
|---|---:|---|---|---|
| `ideconst.h` — IDs & constants | 324 | No | Trivial | The only file that already compiles anywhere. Menu IDs become irrelevant under LSP/DAP; the `.prj` key names stay. |
| **Text engine** (`text.*`) | 1350 | Headers only | Medium | Logic is portable, but: `#include "idecommon.h"`, `TCHAR`/UTF-16, **hard-coded CRLF**, `Settings::` globals, and the buffer defects of §2.6. Cheaper to rewrite against a modern rope than to port. |
| **Document model** (`document.*`) | 1101 | Headers + `SB_VERT`/`SB_HORZ` | Medium | Depends on `Point`/`Rectangle` from `win32/idecommon.h` and on `Text`. Only meaningful together with the text engine. |
| **Syntax styler** (`sourcedoc.*`) | 422 | Headers only | **Low** | The DFA table + `LexicalStylist` are pure computation. Fixing the `ch < 128` fold and decoupling from `LineInfo` yields a portable tokenizer in a day. **Highest value-per-line in the core.** |
| **Debug controller** (`debugcontroller.*`) | 855 | Only via `#include "win32\debugger.h"` and the concrete `Debugger _debugger` member | **Medium** | The *design* is already platform-independent: file formats, record walking, source mapping, step semantics, object graph walking. Extracting a `_DebugTarget` interface (start/stop/read/write/registers/breakpoints/step) is a bounded refactor. **The single most reusable asset in `elide`.** |
| **Native debugger** (`win32/debugger.*`) | 832 | **Total** | **Rewrite per platform** | `CreateProcess(DEBUG_PROCESS)`, `WaitForDebugEvent`, `Read/WriteProcessMemory`, `Get/SetThreadContext`, `Dr0/Dr7`, `CONTEXT.Eip`. Linux needs `ptrace(PTRACE_TRACEME/PEEKDATA/GETREGS)` + `waitpid`; macOS needs Mach exception ports + `task_for_pid` (and code-signing entitlements). ~600 lines per platform. |
| Watch browser (`browser.*`) | 418 | Yes (`HTREEITEM`, `TreeView`) | Rewrite | The *traversal* logic already lives in the controller; `browser.cpp` is only tree-widget plumbing. Under DAP it becomes `variablesReference` bookkeeping — ~100 lines. |
| **Project model** (`ideproject.*`) | 439 | `MsgBox` + `PathRelativePathTo` | **Low** | INI parsing is in `common/config.cpp` and portable. Replace 3 `shlwapi` calls with `std::filesystem`. |
| Settings (`idesettings.*`) | 293 | `shlwapi`, `<direct.h>`, `GetModuleFileName` | Low | Same shape; ~40 lines of path handling to replace. |
| Layout manager (`layout.*`) | 137 | `Rectangle` only | Low | Genuinely portable dock logic — but any modern toolkit supplies this. |
| Plugin manager (`pluginmanager.*`, `plugins.h`) | 137 | `LoadLibrary`/`GetProcAddress` | Low to port, **but not worth it** | See §7 — no working client, empty `_Document`, fragile C++ ABI. |
| Message log (`messagelog.*`) | 103 | Yes | Rewrite | Under LSP this becomes `textDocument/publishDiagnostics`; the error-string parser (`appwindow.cpp:2072`) is the reusable half. |
| **Main window** (`win32/appwindow.*`) | 2455 | **Total** | Rewrite | 250-line command switch, 80-line notification switch, 60 imperative menu-enable calls. Also carries genuine app logic worth re-deriving: breakpoint shifting on edits (`:2003`), staleness check (`:1715`), clean-up (`:2033`), error parsing (`:2072`). |
| **Editor control** (`win32/editframe.*`) | 1438 | **Total** | Rewrite | GDI rendering, `avgCharWidth` grid assumption, `CreateCaret`, `SetScrollInfo`. |
| Dialogs (`win32/dialogs.*` + `ide.rc`) | 1274 | **Total** | Rewrite | Resource-script dialog templates. |
| Win32 primitives (`win32/idecommon.*`, `window.*`) | 1239 | **Total** | Rewrite | `Canvas`=HDC, `Font`=HFONT, `Clipboard`=HGLOBAL, `DateTime`=SYSTEMTIME. |
| Common controls (menu/toolbar/statusbar/tabbar/treeview/listview/splitter/output/accel) | 1396 | **Total** | Rewrite | Every method is a `SendMessage`. `splitter.cpp` also uses a global `WH_MOUSE_LL` hook. `output.cpp` shells `elc.exe` through an anonymous pipe. |
| `winmain.cpp` | 208 | **Total** | Rewrite | `WinMain`, `RegisterClassEx`, `GetMessage` loop. |
| GTK stub (`gtk/main.cpp`) | 49 | GTK+2 | — | Delete. |

**Totals:** ~4,600 LOC carry portable design (text/document/styler/controller/project/
settings/layout); ~9,900 LOC are Win32 API surface with no design content; of the portable
4,600, roughly **1,700 LOC (styler + debug controller + project model) are worth keeping
essentially as-is.**

Cross-cutting blockers that hit every option:

| Blocker | Scope |
|---|---|
| `TCHAR`/`_T()`/`_tcs*` everywhere | whole codebase; needs a single `char8_t`/`std::u8string` or `std::wstring` decision |
| 32-bit assumptions (`int` holds a pointer, 4-byte object slots) | debugger, watch, `ref_t` casts |
| Hard-coded CRLF | text engine |
| `ch < 128` in the lexer | styler |
| Global mutable `Settings` statics | text engine, styler, editor, project |
| No threading discipline (debug thread ↔ UI thread share state unlocked) | debugger + watch |

---

## 10. Modernization options

### Option A — Port the Win32 UI to a cross-platform toolkit (Qt / wxWidgets / GTK4)

**What it means.** Keep `AppWindow`'s structure and re-express `Control`, `Canvas`,
`EditFrame`, the dialogs and the common-control wrappers on top of a toolkit; keep the text
engine, document model, styler and controller; implement `Debugger` per platform.

| | |
|---|---|
| **Effort** | 9-15 person-months. ~9,900 LOC of UI to re-express, plus the §1.3 decoupling work (every core file must lose `idecommon.h`), plus ~1,200 LOC of new platform debugger backends. |
| **Risk** | **High.** The port has no compiling intermediate state: you cannot move one control at a time, because `Point`/`Rectangle`/`Style` live in the Win32 header that everything includes. The GTK attempt (§8) is direct evidence: the author started a fresh `main()` rather than incrementally porting, and stopped. |
| **What survives** | Text engine, document model, styler, controller, project model — ~4,600 LOC, all of which needs cleanup anyway. |
| **What is lost** | All 9,900 Win32 lines, and the `.rc` dialog layouts. |
| **What you get** | An editor that in 2025 is worse than every free alternative: no completion, no go-to-definition, no find-references, no multi-cursor, no LSP client, no Git integration, no extension ecosystem — and a fixed-pitch, `avgCharWidth`-grid renderer with a full-document re-lex per keystroke. |

The decisive objection is not cost, it is the ceiling. Ten person-months buys parity with
`elide` 2009 on three platforms. It does not buy anything a user of VS Code or Neovim would
consider adequate.

### Option B — Rewrite the IDE from scratch

**What it means.** New application (Qt Creator-like C++, or Electron/Tauri, or a Rust/egui
shell), reusing only the styler, controller and project model as libraries.

| | |
|---|---|
| **Effort** | 18-30 person-months for something a user would not immediately abandon. Text editing alone — a competent buffer, incremental highlighting, undo grouping, multi-cursor, search/replace with regex — is 6+ months before you have written a single ELENA-specific feature. |
| **Risk** | **Very high**, and of a different kind: it is unbounded scope. Every IDE feature users expect (fuzzy file open, symbol outline, refactoring, integrated terminal, VCS) is a separate project. |
| **What survives** | The same ~1,700 high-value LOC as Option A, used as libraries. Everything else is new code. |
| **What is lost** | Nothing of value — but you also inherit nothing, so the delivery date for "usable" is far out. |
| **What you get** | Full control over the experience, and the ability to make ELENA-specific IDE affordances (message-send visualisation, VMT browser, live object inspector) first-class. |

Rewriting is only rational if the ELENA-specific tooling is the *product*. For a language
with a small user base and one maintainer, spending the modernization budget on an editor
instead of on the LLVM backend, the GC and multithreading is a poor allocation.

### Option C — Drop the IDE; expose the toolchain as **LSP server + DAP adapter** ✅

**What it means.** Two headless executables:

```
elena-lsp   (Language Server Protocol)          elena-dap   (Debug Adapter Protocol)
├─ textDocument/semanticTokens ← sourcedoc DFA  ├─ launch/attach     ← DebugController::start
├─ publishDiagnostics          ← elc error output├─ setBreakpoints    ← findNearestAddress
├─ definition / references     ← .nl symbol table├─ stackTrace/scopes ← seekDebugLineInfo
├─ documentSymbol              ← module metadata ├─ variables         ← readContext/readFields
├─ completion                  ← VMT method table├─ next/stepIn/stepOut ← stepOver/stepInto
└─ workspace config            ← ProjectInfo     └─ evaluate          ← seekClassInfo
```

Every editor (VS Code, Neovim, Emacs, Helix, Zed, Sublime, Kate, IntelliJ via plugin) then
supports ELENA for free, on every platform, with completion, diagnostics, breakpoints,
stepping and variable inspection — using their own text engines, their own UIs, their own
keybindings.

| | |
|---|---|
| **Effort** | **3-6 person-months** total. Breakdown below. |
| **Risk** | **Low.** Both protocols are JSON-RPC over stdio with mature reference implementations and test suites; VS Code gives you a debuggable client on day one. Progress is incremental and each capability is independently shippable: diagnostics first, then semantic tokens, then breakpoints, then variables. |
| **What survives** | `sourcedoc.cpp`'s DFA (semantic tokens), **`debugcontroller.cpp` almost intact** (the DAP adapter *is* a `DebugController` with a JSON front end instead of `onStep`/`onStop` callbacks), `ideproject.cpp` (workspace configuration), `appwindow.cpp:2072`'s error parser (diagnostics), the `Breakpoint` model, and all the `.dn`/`.dnl` format knowledge. |
| **What is lost** | The `elide` UI — 9,900 LOC that nobody will miss, plus the text engine and document model (~2,450 LOC) which the host editor supplies far better than `Text` ever did. Also lost: the ability to say "ELENA ships with an IDE", which matters for marketing more than for users. |

**Effort breakdown for Option C:**

| Work item | Effort | Reuses |
|---|---:|---|
| Extract `_DebugTarget` interface out of `DebugController` (start/stop/read/write/regs/bp/step) | 2 weeks | `debugcontroller.*` unchanged in substance |
| Linux `ptrace` backend | 3-4 weeks | replaces `win32/debugger.cpp` |
| Windows backend (keep existing) | 1 week | `win32/debugger.cpp` behind the new interface |
| macOS Mach backend (optional, later) | 4-6 weeks | — |
| DAP adapter: JSON-RPC, launch, breakpoints, stepping, stackTrace, scopes, variables | 6-8 weeks | `readAutoContext`, `readContext`, `readFields`, `seekClassInfo`, `findNearestAddress` |
| LSP server: diagnostics by driving `elc` | 2 weeks | error parser from `appwindow.cpp:2072` |
| LSP: semantic tokens | 2 weeks | `sourcedoc.cpp` DFA, after the ASCII fix |
| LSP: documentSymbol / definition / completion from `.nl` metadata | 4-6 weeks | `engine/` module reader |
| VS Code extension (grammar, config, launch schema) | 2 weeks | — |
| **Total (Win+Linux)** | **~4.5 months** | |

### Comparison

| | A — Port UI | B — Rewrite IDE | C — LSP + DAP |
|---|---|---|---|
| Effort | 9-15 mo | 18-30 mo | **3-6 mo** |
| Risk | High (no incremental path) | Very high (unbounded scope) | **Low (incremental, protocol-driven)** |
| Cross-platform | Yes | Yes | **Yes, transitively — every editor** |
| Debugger reuse | Controller yes, backend rewritten | Same | **Controller yes — it *is* the product** |
| Styler reuse | Yes | Yes | **Yes** |
| Text engine reuse | Yes (but it is a liability) | No | No (correctly — the editor provides one) |
| UI code discarded | 9,900 | 9,900 | 9,900 |
| User-visible result | 2009 IDE, on Linux | New IDE, eventually | Modern tooling everywhere, soon |
| Ongoing maintenance | An IDE, forever | An IDE, forever | Two headless servers |
| Blocks the LLVM/GC/threading work? | Yes, heavily | Yes, severely | **Barely** |

### Recommendation: **Option C**, with a specific sequencing

1. **Delete `ide/win32/`, `ide/gtk/`, `ide/ide.rc`, `elenasrc/plugins/` and the `vc/`
   projects.** Nothing there is worth carrying. Tag the commit so the history is preserved.
2. **Promote `debugcontroller.*` to a first-class library** under `elenasrc/debug/`. Its
   only required change is replacing the concrete `Debugger _debugger` member with an
   abstract `_DebugTarget*`. The seam already exists conceptually (`_Controller`,
   `debugger.h:49`) — it just points the wrong way.
3. **Write the Linux `ptrace` backend before the DAP adapter.** It validates the interface
   against a genuinely different OS model (signals and `waitpid` instead of an event queue;
   `PTRACE_POKEDATA` word granularity instead of byte-granular `WriteProcessMemory`;
   `PTRACE_SINGLESTEP` instead of the EFLAGS trap flag) and will expose every remaining
   Win32 assumption in the controller. Doing it second risks baking Win32 semantics into
   the interface.
4. **Ship diagnostics first.** An LSP server that only reports `elc` errors is a week of
   work and immediately makes ELENA usable in any editor. Semantic tokens next, then
   symbols and completion.
5. **Unify the lexers.** The compiler's 23-state DFA (`elc/source.h`, `elc/dfa.h`) is the
   authoritative one; retire the IDE's 21-state copy (`sourcedoc.cpp:40`) and drive semantic
   tokens from the compiler's table so highlighting can never drift from the language again.
6. **Fix the debug information while you are in there**, because the modernization plan
   forces the issue anyway: 64-bit addresses (`DebugLineInfo::addresses.step.address` is a
   32-bit `size_t`), a per-module line index instead of `findNearestAddress`'s linear scan,
   a real frame walker (there is currently **no call stack at all** — DAP's `stackTrace` is
   mandatory), and a thread id on every event. An LLVM backend also opens the option of
   emitting **DWARF** instead of the bespoke `.dn`/`.dnl` pair, which would let `gdb` and
   `lldb` debug ELENA directly — worth evaluating before committing to a DAP adapter over
   the proprietary format.

**Why not A or B.** Both spend the majority of the modernization budget on the least
differentiated part of the system. `elide`'s editor is a fixed-pitch GDI grid with a
full-document re-lex per keystroke, a 256-char page list with a memory-corrupting `erase`
(`text.cpp:440`), CRLF hard-coded into its row arithmetic, and a two-buffer undo journal
that silently drops history. None of that is worth porting, and rewriting it competes
directly with editors that have a hundred times the engineering behind them. The debugger,
by contrast, encodes something nobody else has and nobody else can reconstruct: how ELENA
objects, VMTs, roles, messages and steps are laid out at runtime. **That knowledge is the
asset. LSP and DAP are the cheapest possible way to ship it to every developer on every
platform, and the only option that leaves the LLVM backend, the new GC and multithreading
as the main line of work rather than a side quest.**
