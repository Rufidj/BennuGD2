#!/bin/sh
# Builds libps5_runtime_gaps.a (ps5_runtime_gaps.c + ps5_net_shim.c) and installs it where
# native-app-boilerplate's build-title.sh (apps/bennugd2/libs/libps5_runtime_gaps.a) expects it.
#
# ps5_runtime_gaps.c covers what this sandbox's libc refuses: opendir()/stat()/getcwd() on most
# paths (glob() falls back to getdents() or a title-relative scan instead), case-insensitive asset
# lookups (BennuGD's own convention), and the libc gaps FFmpeg's dependencies pull in (OpenSSL's
# _setjmp, nl_langinfo, sendmmsg...). ps5_net_shim.c covers getaddrinfo (null on this console's libc).
#
#   ps5/tools/build-runtime-gaps.sh /path/to/native-app-boilerplate
set -e
BP="${1:?path to native-app-boilerplate}"; SDK="$BP/.deps/native/ps5-payload-sdk"
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$BP/apps/bennugd2/libs"
[ -d "$SDK" ] || { echo "missing $SDK (run the boilerplate's dependency setup first)" >&2; exit 2; }
mkdir -p "$OUT"
WORK="$(mktemp -d)"
for src in ps5_runtime_gaps.c ps5_net_shim.c; do
  PS5_PAYLOAD_SDK="$SDK" PS5_CLANG="${PS5_CLANG:-$(command -v clang-18 || command -v clang)}" USE_CCACHE=0 \
    sh "$BP/tooling/prospero-clang18" -std=c11 -O2 -c "$HERE/$src" -o "$WORK/$(basename "$src" .c).o"
done
"$SDK/bin/prospero-ar" rcs "$OUT/libps5_runtime_gaps.a" "$WORK"/*.o
rm -rf "$WORK"
echo "installed $OUT/libps5_runtime_gaps.a"
