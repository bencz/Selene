# The Platform Layer — Organizing OS-Specific Code

> **Rule: the build system selects the file. The code never asks which platform
> it is running on.**
>
> `#ifdef _WIN32` in shared code is a defect, not a technique.

---

## 1. Why not `#ifdef`

Scattered conditionals fail in three specific ways, all of which this codebase
has already demonstrated:

| Failure | Evidence |
|---|---|
| **Only one branch is ever compiled**, so the other rots silently | `ide/gtk/main.cpp` — a 49-line stub that references nothing. The author started a port, found nothing compiled, and stopped |
| **Divergence hides in the branches** | The wide path routines normalized `\` separators; the narrow ones did not. The wchar_t build created output directories, the UTF-8 build failed — same source, same function names |
| **The count only grows** | Six targets are planned (Linux, Windows, macOS × x86-64, arm64, ppc, s390x). `#if defined(A) && !defined(B)` does not survive that |

## 2. The three mechanisms, and when each applies

### 2.1 Per-platform source directories — for behavioural divergence

Used when the implementations are genuinely different code.

```
elenasrc/elc/
    elcproject.cpp          <- shared: project handling, config, options
    win32/elc.cpp           <- GetModuleFileName, CommandLineToArgvW, PE linking
    posix/elc.cpp           <- /proc/self/exe or _NSGetExecutablePath, no linking
```

CMake picks one:

```cmake
${SRC}/elc/${ELENA_PLATFORM_DIR}/elc.cpp
```

This is what the original codebase already did (`elc/win32/`, `engine/win32/`,
`ide/win32/`). The port extended it rather than inventing something new.

### 2.2 A platform header, same names on every platform — for primitives

Used for small operations where an interface is enough.

```
elenasrc/common/win32/platform.h     namespace Platform { ... }
elenasrc/common/posix/platform.h     namespace Platform { ... }   <- same names
```

CMake puts exactly one of those directories on the include path:

```cmake
include_directories(BEFORE ${SRC}/common/${ELENA_PLATFORM_DIR})
```

so shared code writes

```cpp
#include "platform.h"
...
Platform::removeFile(name);
```

and there is **no conditional at the include site and none at the call site**.
The same trick makes `#include "unicode.h"` resolve correctly, and on POSIX it
is what lets the untouched 2009 sources keep saying `#include <tchar.h>`.

### 2.3 Compatibility shims — transitional only

`common/posix/` also provides `tchar.h`, `io.h` and `direct.h`, which hijack
names the MSVC headers supply. This kept ~33,500 lines of 2009 C++ compiling
**unmodified** during the port, which was the right trade for a first build.

It is not the end state. As each subsystem is rewritten for UTF-8 and module
format v2, its shim usage should be replaced by explicit `Platform::` calls, and
the shims deleted. Track it as debt, not as architecture.

## 3. Where conditionals are still legitimate

**Inside a platform header or a platform source file** — that is what those files
are for. A `#ifdef __APPLE__` inside `posix/elc.cpp` to choose between
`/proc/self/exe` and `_NSGetExecutablePath` is correct: it is a *variation within
POSIX*, and it lives in the POSIX file.

Everywhere else, the answer is a new entry in `Platform::`.

## 4. Current state

Verified: **zero platform conditionals in shared code.**

```
$ grep -rn '_WIN32\|__APPLE__\|__linux' \
      elenasrc/common/*.h elenasrc/common/*.cpp \
      elenasrc/elc/*.h    elenasrc/elc/*.cpp \
      elenasrc/engine/*.h elenasrc/engine/*.cpp
(no matches)
```

`Platform::` currently provides:

| Name | Purpose |
|---|---|
| `Platform::Separator` | The native path separator, typed as `TCHAR` so callers need no cast in either character model |
| `Platform::isSeparator(ch)` | Accepts `/` and `\` on every platform — required, because the project's `.cfg` and `.prj` files use `\` and those same strings feed `pathToName()` to derive module names |
| `Platform::removeFile(path)` | Narrow and wide overloads, normalizing separators |

It is deliberately small. Entries get added when a real divergence appears, not
speculatively.

## 5. What belongs here as the project grows

The audits identified these as the platform-divergent surfaces still to come:

| Area | Why it lands in `Platform::` |
|---|---|
| Executable path lookup | `GetModuleFileName` / `/proc/self/exe` / `_NSGetExecutablePath` |
| Directory creation and traversal | Already diverging; currently in the shims |
| Dynamic library loading | `LoadLibrary` / `dlopen` — needed if packages become shared libraries |
| Threads, TLS, atomics | **Do not put these here.** C11 `<stdatomic.h>` and `_Thread_local` are portable; see [`../plan/19-runtime-in-c.md`](../plan/19-runtime-in-c.md) |
| Debug target control | `WaitForDebugEvent` / `ptrace` / Mach exception ports — behind a `_DebugTarget` interface, mechanism 2.1 |
| Object file emission | Delegated to LLVM entirely; no platform layer needed |

The last two rows matter: the goal is for the platform layer to **shrink** as the
LLVM backend absorbs code generation and linking, not to accumulate every
OS-flavoured thing in the project.

## 6. The one rule, restated

> If you are about to write `#ifdef` in a file that both platforms compile,
> you have found a missing `Platform::` entry.
