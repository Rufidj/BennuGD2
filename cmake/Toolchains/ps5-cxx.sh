#!/bin/sh
# C++ compiler entry point for the PS5 (Prospero) cross build.
# CMAKE_C_COMPILER and CMAKE_CXX_COMPILER both have to go through the same
# prospero-clang18 wrapper (there's no separate "clang++" for this target),
# so unlike a normal gcc/g++ or clang/clang++ pair, nothing here picks C vs
# C++ by the compiler binary's own name - only the .c/.cpp file extension
# would, and that's wrong for the .c files in this project that need the
# C++ front end for their raw-string GLSL/HLSL (see libmod_3d's CMakeLists.txt,
# set_source_files_properties(... LANGUAGE CXX)). This wrapper is what makes
# that LANGUAGE CXX override actually take effect: it forces -x c++ itself,
# since CMake's own CXX compile rule does not insert it automatically.
# -x c++ must only apply when actually compiling a source (-c present): on
# the final LINK command CMake also invokes this same CXX compiler (as the
# link driver, once anything in the executable has a C++ object in it), and
# -x c++ there would make clang try to "compile" the .a archives being linked
# instead of just passing them to the linker.
for arg in "$@"; do
    if [ "$arg" = "-c" ]; then
        exec "$(dirname "$0")/ps5-cc.sh" -x c++ "$@"
    fi
done
exec "$(dirname "$0")/ps5-cc.sh" "$@"
