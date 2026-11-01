# Cross-build only the standalone preview on Linux; the injected game runtime
# and the existing Visual Studio projects are not part of this toolchain.
# Install g++-mingw-w64-x86-64-posix and supply the official MinGW packages with
# -DSDL2_DIR=<SDL2>/x86_64-w64-mingw32/lib/cmake/SDL2
# -DSDL2_image_DIR=<SDL2_image>/x86_64-w64-mingw32/lib/cmake/SDL2_image
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc-posix)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++-posix)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# The preview has a normal main() and writes automation results to stdout.
set(SDL2_NO_MWINDOWS ON CACHE BOOL "Keep standalone preview console output")
