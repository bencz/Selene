#----------------------------------------------------------------------------
#   Cross build: 64-bit Windows, from Linux, with MinGW-w64
#
#   NOTE: this will NOT build yet.
#
#   Selene is still 32-bit only. The object model, byte code operands and the
#   .sem module format all assume 32-bit pointers, and a 64-bit build produces
#   hard errors -- `ref_t` is `size_t` with reference tags packed into the top
#   eight bits including the sign bit, so every mask test changes meaning.
#
#   The file is here because the 64-bit port needs a target to aim at, and
#   because it documents that the blocker is the code, not the toolchain: the
#   MinGW-w64 x86_64 compiler is present and working.
#
#   See docs/plan/17-llvm-backend-and-targets.md section 2.
#
#   Usage, once the 64-bit work lands:
#     cmake -S . -B build-win64 \
#           -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-x64.cmake \
#           -DELENA_32BIT=OFF
#----------------------------------------------------------------------------

set(CMAKE_SYSTEM_NAME      Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)

set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_RC_COMPILER  ${TOOLCHAIN_PREFIX}-windres)

set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_EXE_LINKER_FLAGS_INIT "-static -static-libgcc -static-libstdc++")

find_program(WINE_EXECUTABLE wine)
if(WINE_EXECUTABLE)
    set(CMAKE_CROSSCOMPILING_EMULATOR ${WINE_EXECUTABLE})
endif()
