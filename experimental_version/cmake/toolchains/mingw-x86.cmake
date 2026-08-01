#----------------------------------------------------------------------------
#   Cross build: 32-bit Windows, from Linux, with MinGW-w64
#
#   Why this exists
#   ---------------
#   The x86 code generator and the PE linker are the only parts of Selene that
#   can produce a runnable program today, and neither can be built or executed
#   on Linux. Every change touching them was therefore unverifiable -- pairing a
#   byte order or slot-index change on the writer side with the reader side and
#   hoping.
#
#   With MinGW-w64 and Wine that stops being true: the Windows toolchain builds
#   here and runs here, so asm2binx can assemble the runtime, elc can link, and
#   the result can be executed.
#
#   Usage:
#     cmake -S . -B build-win32 \
#           -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-x86.cmake
#     cmake --build build-win32 -j
#     wine build-win32/bin/elc.exe --target=?
#----------------------------------------------------------------------------

set(CMAKE_SYSTEM_NAME      Windows)
set(CMAKE_SYSTEM_PROCESSOR i686)

set(TOOLCHAIN_PREFIX i686-w64-mingw32)

set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_RC_COMPILER  ${TOOLCHAIN_PREFIX}-windres)

set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})

# Look for programs on the host, but headers and libraries only in the target
# sysroot -- otherwise the build picks up Linux headers.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Static so the produced .exe runs under Wine without the MinGW runtime DLLs
# alongside it.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static -static-libgcc -static-libstdc++")

# Run cross-built executables through Wine, which is what lets add_custom_command
# steps -- notably the parser table generation -- work in a cross build.
find_program(WINE_EXECUTABLE wine)
if(WINE_EXECUTABLE)
    set(CMAKE_CROSSCOMPILING_EMULATOR ${WINE_EXECUTABLE})
endif()
