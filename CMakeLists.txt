# Selene v2 — phase P0 build.
#
# Builds the 2015 toolchain on modern Linux, 64-bit, as-is plus the minimal
# mechanical fixes: common, engine, elc, and the tools that survive the
# modernization plan (sg, og, ecv, asm2bin). See docs/02-modernization-plan.md.
#
# Deliberately NOT built: elenavm, elenasm, elenart, elt, ide, gui — they are
# scheduled for deletion, not porting (docs/02 §6).
#
# The x86 JIT (engine/x86jitcompiler.cpp) and the hand-written ELF writer
# (elc/linux32/linker.cpp) are still compiled at this phase because elc's
# driver and image builder reference them; they are replaced, then deleted,
# when llvmgen lands (P2).

cmake_minimum_required(VERSION 3.16)
project(selene VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# Output into the build tree, never into the source tree's bin/ (which holds
# source data: configs, templates, scripts). Two build directories writing
# to one bin/ silently overwrite each other — learned in round 1.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(SRC ${CMAKE_SOURCE_DIR}/elenasrc2)

# The 2015 code selects its platform with _WIN32 / _LINUX32. The "32" is
# historical naming — it means "Linux", and is defined for 64-bit builds too.
add_compile_definitions(_LINUX32)

include_directories(${SRC}/common ${SRC}/engine)

# The codebase is 2015 C++ compiled by much older GCC; keep modern GCC's
# diagnostics from drowning the real errors during P0/P1. These relaxations
# are removed as the hazard classes in docs/01 §10 are fixed.
add_compile_options(
   -Wno-conversion-null
   -Wno-write-strings
)

# --- libraries -------------------------------------------------------------

add_library(selene_common STATIC
   ${SRC}/common/altstrings.cpp
   ${SRC}/common/config.cpp
   ${SRC}/common/dump.cpp
   ${SRC}/common/files.cpp
)

add_library(selene_engine STATIC
   ${SRC}/engine/bytecode.cpp
   ${SRC}/engine/jitcompiler.cpp
   ${SRC}/engine/jitlinker.cpp
   ${SRC}/engine/libman.cpp
   ${SRC}/engine/loader.cpp
   ${SRC}/engine/module.cpp
   ${SRC}/engine/parsertable.cpp
   ${SRC}/engine/rtman.cpp
   ${SRC}/engine/section.cpp
   ${SRC}/engine/textsource.cpp
   ${SRC}/engine/x86helper.cpp
   ${SRC}/engine/x86jitcompiler.cpp
)
target_link_libraries(selene_engine PUBLIC selene_common)

# --- tools -----------------------------------------------------------------

add_executable(sg
   ${SRC}/tools/sg/sg.cpp
)
target_include_directories(sg PRIVATE ${SRC}/tools/sg/linux32)
target_link_libraries(sg PRIVATE selene_engine)

add_executable(og
   ${SRC}/tools/og/og.cpp
)
target_link_libraries(og PRIVATE selene_engine)

add_executable(ecv
   ${SRC}/tools/ecv/ecv.cpp
)
target_link_libraries(ecv PRIVATE selene_engine)

add_executable(asm2bin
   ${SRC}/tools/asm2bin/asm2binx.cpp
   ${SRC}/tools/asm2bin/ecassembler.cpp
   ${SRC}/tools/asm2bin/x86assembler.cpp
   ${SRC}/tools/asm2bin/x86jumphelper.cpp
   ${SRC}/elc/source.cpp
)
target_include_directories(asm2bin PRIVATE ${SRC}/elc)
target_link_libraries(asm2bin PRIVATE selene_engine)

# --- the compiler ----------------------------------------------------------

add_executable(elc
   ${SRC}/elc/bcwriter.cpp
   ${SRC}/elc/compiler.cpp
   ${SRC}/elc/derivation.cpp
   ${SRC}/elc/image.cpp
   ${SRC}/elc/parser.cpp
   ${SRC}/elc/project.cpp
   ${SRC}/elc/source.cpp
   ${SRC}/elc/syntaxtree.cpp
   ${SRC}/elc/linux32/elc.cpp
   ${SRC}/elc/linux32/linker.cpp
)
target_include_directories(elc PRIVATE ${SRC}/elc)
target_link_libraries(elc PRIVATE selene_engine)
