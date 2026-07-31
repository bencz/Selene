#----------------------------------------------------------------------------
#   ELENA Language Project -- build system
#
#   Replaces the original CodeBlocks (.cbp) and Visual Studio (.vcproj)
#   project files, neither of which had a makefile or build script of any kind.
#
#   Milestone 1: build the console toolchain (sg, asm2binx, elc) on Linux.
#   The Win32 GUI IDE (elide) is not built here.
#----------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.16)

project(elena
    VERSION 1.5.0.0
    DESCRIPTION "ELENA Language toolchain"
    LANGUAGES CXX)

#----------------------------------------------------------------------------
# Options
#----------------------------------------------------------------------------

# The object model, bytecode operands and .nl module format all assume 32-bit
# pointers. A 64-bit build produces 25 hard errors and, more dangerously,
# silently different file formats. Until phase P2 (module format v2) lands,
# 32-bit is the only correct configuration.
#   See docs/plan/17-llvm-backend-and-targets.md
option(ELENA_32BIT "Build 32-bit (required until module format v2)" ON)

option(ELENA_BUILD_SG       "Build the syntax generator"      ON)
option(ELENA_BUILD_ELC      "Build the compiler"              ON)
option(ELENA_BUILD_API2HTML "Build the API doc generator"     OFF)
option(ELENA_BUILD_SEMDUMP  "Build the module dumper / disassembler" ON)

# asm2binx assembles src/asm/*.asm into the bin/*.bin runtime primitives.
#
# It is deliberately NOT built off Windows. Porting it would be wasted effort:
# the .bin files it produces contain x86 assembly that calls kernel32/user32
# directly, and the only consumer is the PE linker. A working reference build
# can therefore only exist on Windows -- where asm2binx already compiles with
# MinGW or MSVC and needs no porting at all.
#
# The tool is scheduled for deletion once the runtime is reimplemented in C.
#   See docs/plan/19-runtime-in-c.md
option(ELENA_BUILD_ASM2BIN  "Build the x86 assembler (Windows only)" ${WIN32})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Binaries go into the build tree, NOT into the source tree's bin/.
#
# Writing to a shared ${CMAKE_SOURCE_DIR}/bin means two build directories -- say
# a wchar_t build and a UTF-8 build -- silently overwrite each other's binaries
# while each still believes its own output is up to date. That produces test
# results attributed to the wrong compiler.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

#----------------------------------------------------------------------------
# Platform selection
#
# Each subproject has a platform subdirectory (win32/). The POSIX counterpart
# lives in common/posix and supplies <tchar.h>, <io.h> and unicode.h shims;
# that directory is placed on the include path so the original angle-bracket
# includes resolve to it without source changes.
#----------------------------------------------------------------------------

set(SRC ${CMAKE_SOURCE_DIR}/elenasrc)

if(WIN32)
    set(ELENA_PLATFORM_DIR win32)
else()
    set(ELENA_PLATFORM_DIR posix)
endif()

# The platform directory goes on the include path FIRST. That is what lets
# shared code write #include "platform.h" / "unicode.h" -- and, on POSIX,
# #include <tchar.h> / <io.h> / <direct.h> -- and resolve to the right
# implementation with no conditional compilation at the include site.
include_directories(BEFORE ${SRC}/common/${ELENA_PLATFORM_DIR})

include_directories(
    ${SRC}/common
    ${SRC}/engine
    ${SRC}/elc
)

#----------------------------------------------------------------------------
# Common flags
#
# Translated from elenasrc/*/codeblocks/*.cbp:
#   -march=pentium2 -O3 -W -D_UNICODE -DUNICODE -s
# -march=pentium2 is dropped: it predates the baseline of every supported
# target and constrains nothing we want constrained.
#----------------------------------------------------------------------------

# Character model.
#
# ON (default): TCHAR is char and all strings are UTF-8.
#   - No byte order, which matters directly for the big-endian targets
#     (s390x, ppc64 BE, ppc32).
#   - Modules are ~31% smaller and identical on every platform.
#   - Needs no libc: the wcs* family does not exist under -ffreestanding, so a
#     wchar_t-based runtime could not be linked into a kernel image without
#     reimplementing it first. See docs/plan/20-os-development.md.
#
# OFF (legacy, kept only for A/B comparison during the migration): TCHAR is
#   wchar_t -- 2 bytes on Windows, 4 on Linux and macOS, so modules built on the
#   two platforms are mutually unreadable. To be deleted once the tree is under
#   version control and the UTF-8 path has soaked.
option(ELENA_UTF8 "Use UTF-8 char strings instead of wchar_t" ON)

if(NOT ELENA_UTF8)
    add_compile_definitions(_UNICODE UNICODE)
endif()

# -m32 is only for a 64-bit host toolchain that must be told to emit 32-bit
# code. The i686 MinGW cross compiler is already 32-bit and rejects nothing, but
# passing it is noise.
if(ELENA_32BIT AND NOT MSVC AND NOT MINGW)
    add_compile_options(-m32)
    add_link_options(-m32)
endif()

if(NOT MSVC)
    add_compile_options(
        -Wall
        # The codebase is 2009-era C++98. These are pervasive and not
        # actionable in milestone 1; they are re-enabled subsystem by
        # subsystem as each is modernized.
        -Wno-unused-parameter
        -Wno-unused-variable
        -Wno-unused-but-set-variable
        -Wno-sign-compare
        -Wno-parentheses
        -Wno-reorder
        -Wno-class-memaccess
        -Wno-deprecated-declarations
    )
endif()

#----------------------------------------------------------------------------
# Shared source groups
#
# The original build had no static libraries: every target recompiled the
# shared sources with its own flags. That is preserved here, because api2html
# is an ANSI build while everything else is Unicode.
#----------------------------------------------------------------------------

set(COMMON_SRC
    ${SRC}/common/altstrings.cpp
    ${SRC}/common/config.cpp
    ${SRC}/common/dump.cpp
    ${SRC}/common/files.cpp
)

set(ENGINE_SRC
    ${SRC}/engine/bccompiler.cpp
    ${SRC}/engine/bytecode.cpp
    ${SRC}/engine/jitcompiler.cpp
    ${SRC}/engine/jitlinker.cpp
    ${SRC}/engine/module.cpp
    ${SRC}/engine/section.cpp
)

#----------------------------------------------------------------------------
# sg -- syntax generator
#
# Reads dat/sg/syntax.txt and writes the LL(1) parser table that elc loads at
# startup. elc cannot run without it, so this is the first bootstrap step.
#----------------------------------------------------------------------------

if(ELENA_BUILD_SG)
    add_executable(sg
        ${SRC}/sg/sg.cpp
        ${SRC}/elc/parsertable.cpp
        ${SRC}/elc/source.cpp
        ${COMMON_SRC}
    )

    # --- parser table generation ---
    #
    # sg reads the grammar and writes the LL(1) table elc loads at startup.
    # elc hard-fails without it, so this belongs in the build rather than in a
    # README step.
    #
    # sg has no -o option and writes beside its input, so the grammar is copied
    # into the build tree first.
    #
    # NOTE: syntax.dat is a raw memory image that embeds sizeof(size_t) and
    # sizeof(TCHAR) with no magic and no version, so a table generated by one
    # configuration is misread by another rather than rejected. Because sg and
    # elc are built here with identical settings that is safe today, but it
    # breaks under cross-compilation and during the 32->64 bit transition.
    # Giving syntax.dat a header is ~10 lines and should happen before then.
    set(SYNTAX_SRC ${CMAKE_BINARY_DIR}/syntax.txt)
    set(SYNTAX_DAT ${CMAKE_BINARY_DIR}/syntax.dat)

    add_custom_command(
        OUTPUT ${SYNTAX_DAT}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_SOURCE_DIR}/dat/sg/syntax.txt ${SYNTAX_SRC}
        COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:sg> ${SYNTAX_SRC}
        COMMAND ${CMAKE_COMMAND} -E copy ${SYNTAX_DAT}
                ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/syntax.dat
        DEPENDS sg ${CMAKE_SOURCE_DIR}/dat/sg/syntax.txt
        COMMENT "Generating parser table from dat/sg/syntax.txt"
        VERBATIM
    )
    add_custom_target(parser_table ALL DEPENDS ${SYNTAX_DAT})
endif()

# --- runtime data files ---
#
# elc resolves elc.cfg, the templates and winstub relative to its own
# executable, so they must sit next to it in the build tree.
add_custom_target(runtime_data ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/bin/elc.cfg ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/bin/templates ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/templates
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/bin/targets ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/targets
    COMMENT "Staging elc.cfg, project templates and target bindings"
    VERBATIM
)

#----------------------------------------------------------------------------
# asm2binx -- the custom x86 assembler
#
# Assembles src/asm/*.asm into the bin/*.bin runtime primitives.
# Slated for deletion once the runtime is reimplemented in C, but nothing can
# link until it has run at least once.
#   See docs/plan/19-runtime-in-c.md
#----------------------------------------------------------------------------

if(ELENA_BUILD_ASM2BIN)
    add_executable(asm2binx
        ${SRC}/asm2bin/asm2binx.cpp
        ${SRC}/asm2bin/x86assembler.cpp
        ${SRC}/asm2bin/x86jumphelper.cpp
        ${SRC}/elc/source.cpp
        ${SRC}/engine/module.cpp
        ${SRC}/engine/section.cpp
        ${SRC}/engine/targetinfo.cpp
        ${SRC}/engine/win32/x86helper.cpp
        ${COMMON_SRC}
    )
    # engine/win32 contains no Windows API -- it is x86-specific, not
    # OS-specific, and should be renamed engine/x86.
    target_include_directories(asm2binx PRIVATE ${SRC}/engine/win32)
endif()

#----------------------------------------------------------------------------
# elc -- the compiler
#----------------------------------------------------------------------------

if(ELENA_BUILD_ELC)
    # Front end, shared project handling, and the compiler proper.
    set(ELC_SRC
        ${SRC}/elc/${ELENA_PLATFORM_DIR}/elc.cpp
        ${SRC}/elc/elcproject.cpp
        ${SRC}/elc/compiler.cpp
        ${SRC}/elc/derivation.cpp
        ${SRC}/elc/parser.cpp
        ${SRC}/elc/parsertable.cpp
        ${SRC}/elc/project.cpp
        ${SRC}/elc/source.cpp
        ${SRC}/engine/bccompiler.cpp
        ${SRC}/engine/bytecode.cpp
        ${SRC}/engine/module.cpp
        ${SRC}/engine/section.cpp
        ${SRC}/engine/targetinfo.cpp
        ${COMMON_SRC}
    )

    # The code generator and linker are Win32/x86-only and are not built
    # elsewhere: the linker emits PE/COFF and the JIT emits Win32-hosted x86,
    # so on POSIX they could not produce a runnable image anyway. The POSIX
    # build therefore stops after byte code generation until the LLVM backend
    # replaces both.
    if(WIN32)
        list(APPEND ELC_SRC
            ${SRC}/elc/win32/linker.cpp
            ${SRC}/engine/jitcompiler.cpp
            ${SRC}/engine/jitlinker.cpp
            ${SRC}/engine/win32/x86jitcompiler.cpp
            ${SRC}/engine/win32/x86helper.cpp
            ${SRC}/engine/win32/imagesection.cpp
        )
    endif()

    add_executable(elc ${ELC_SRC})
    target_include_directories(elc PRIVATE
        ${SRC}/engine/win32
        ${SRC}/elc/${ELENA_PLATFORM_DIR}
    )
endif()

#----------------------------------------------------------------------------
# semdump -- module dumper / disassembler
#
# Prints a .sem module's header, symbol tables and disassembled byte code.
# Also the front half of the future LLVM translator: decoding a section into
# (opcode, arguments) is the same work whether it feeds a printer or an IR
# builder.
#----------------------------------------------------------------------------

if(ELENA_BUILD_SEMDUMP)
    add_executable(semdump
        ${SRC}/semdump/semdump.cpp
        ${SRC}/engine/bytecode.cpp
        ${SRC}/engine/module.cpp
        ${SRC}/engine/section.cpp
        ${SRC}/engine/targetinfo.cpp
        ${COMMON_SRC}
    )
endif()

#----------------------------------------------------------------------------
# Summary
#----------------------------------------------------------------------------

message(STATUS "")
message(STATUS "ELENA ${PROJECT_VERSION}")
message(STATUS "  system         : ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "  platform layer : ${ELENA_PLATFORM_DIR}")
if(CMAKE_CROSSCOMPILING)
    message(STATUS "  cross          : yes, emulator = ${CMAKE_CROSSCOMPILING_EMULATOR}")
endif()
message(STATUS "  word size      : ${CMAKE_SIZEOF_VOID_P}*8 host / 32 target")
message(STATUS "  compiler       : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  output         : ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "")
