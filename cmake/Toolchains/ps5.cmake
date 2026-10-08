# CMake toolchain for PS5 (Prospero), mirroring the $DEVKITPRO/cmake/Switch.cmake
# pattern already used by build.sh's "switch" target. Produces x86_64-sie-ps5
# static libraries/objects via the same prospero-clang18 wrapper and sysroot
# that native-app-boilerplate's own `make app` uses to build the known-good
# triangle/BennuGD2 test titles.
#
# This file only sets up the compiler; it does NOT produce a signed eboot by
# itself. Packaging (sce_sys, signing, static module table linking into one
# binary) is still native-app-boilerplate's job, same as it is today for the
# hand-built test apps. CMake's job here is to get each BennuGD2 module
# compiling and archived as a clean, reproducible static library instead of
# hand-copied objects in /tmp.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(PS5_TOOLCHAIN_DIR "${CMAKE_CURRENT_LIST_DIR}" CACHE PATH "")

set(CMAKE_C_COMPILER   "${PS5_TOOLCHAIN_DIR}/ps5-cc.sh"  CACHE FILEPATH "")
set(CMAKE_CXX_COMPILER "${PS5_TOOLCHAIN_DIR}/ps5-cxx.sh" CACHE FILEPATH "")
set(CMAKE_ASM_COMPILER "${PS5_TOOLCHAIN_DIR}/ps5-cc.sh" CACHE FILEPATH "")
set(CMAKE_AR      "/usr/bin/llvm-ar-18"     CACHE FILEPATH "")
set(CMAKE_RANLIB  "/usr/bin/llvm-ranlib-18" CACHE FILEPATH "")

# The wrapper is a plain POSIX shell script, not a cross binary CMake can
# try_compile/try_run against the normal way - skip the probes.
set(CMAKE_C_COMPILER_WORKS   1)
set(CMAKE_CXX_COMPILER_WORKS 1)
set(CMAKE_C_COMPILER_FORCED   TRUE)
set(CMAKE_CXX_COMPILER_FORCED TRUE)

# BennuGD2's static-platform builds (switch, ps3) only ever produce .a
# archives feeding one final statically-linked binary.
set(LIBRARY_BUILD_TYPE STATIC CACHE STRING "" FORCE)
add_definitions(-D__PROSPERO__ -D__STATIC__ -DUSE_REAL_PS5_OPENGL -DGL_GLEXT_PROTOTYPES=1)

# Everything (SDL2, GL headers, zlib, ...) comes from the proven PS5
# integration tree, not from the host's own /usr - never fall back to it.
set(CMAKE_FIND_ROOT_PATH
    "/home/ruben/Escritorio/ps5-opengl/native-app-boilerplate/.deps/native/ps5-payload-sdk"
    "/home/ruben/Escritorio/ps5-opengl"
    "/home/ruben/BennuGD2/vendor")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
