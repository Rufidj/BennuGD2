#!/bin/sh
# C/C++ compiler entry point for the PS5 (Prospero) cross build.
# A plain CMAKE_C_COMPILER is invoked by ninja/make as a fresh process that
# does not inherit variables set by the CMake configure step, so the env
# that native-app-boilerplate's prospero-clang18 wrapper needs is hardcoded
# here instead of relying on the invoking shell to export it.
export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-/home/ruben/Escritorio/ps5-opengl/native-app-boilerplate/.deps/native/ps5-payload-sdk}"
export PS5_CLANG="${PS5_CLANG:-/usr/bin/clang-18}"
export USE_CCACHE=0
# clang resolves its linker by name from the target triple ("prospero-lld")
# via PATH - it isn't a standard binutils/lld name, so it's only ever found
# if the payload SDK's own bin/ is on PATH.
export PATH="$PS5_PAYLOAD_SDK/bin:$PATH"
# Some archives (SDL_gpu's own find_package(Threads) use) get compiled with
# -pthread, which makes clang embed a "needs libpthread" ELF dependent-library
# annotation; ld.lld then tries to autolink it with -lpthread, which doesn't
# exist as a separate lib on this payload SDK (its functions are already in
# libc). Only matters at link time (a plain -c compile ignores -Wl,...).
for arg in "$@"; do
    if [ "$arg" = "-c" ]; then
        exec /home/ruben/Escritorio/ps5-opengl/native-app-boilerplate/tooling/prospero-clang18 "$@"
    fi
done
exec /home/ruben/Escritorio/ps5-opengl/native-app-boilerplate/tooling/prospero-clang18 -Wl,--no-dependent-libraries "$@"
