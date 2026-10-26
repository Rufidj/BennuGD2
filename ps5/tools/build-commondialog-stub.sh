#!/bin/sh
# Builds libSceCommonDialog.so (a link stub, soname libSceCommonDialog.sprx) and installs it in the payload SDK's
# target/lib, where native-app-boilerplate's `make app` picks every *.so up as an import library.
# (libSceImeDialog / libSceUserService / libSceSysmodule stubs already ship with the SDK.)
#   ps5/tools/build-commondialog-stub.sh /path/to/native-app-boilerplate
set -e
BP="${1:?path to native-app-boilerplate}"; SDK="$BP/.deps/native/ps5-payload-sdk"
HERE="$(cd "$(dirname "$0")" && pwd)"; OUT="$(mktemp -d)"
PS5_PAYLOAD_SDK="$SDK" PS5_CLANG="${PS5_CLANG:-/usr/bin/clang-18}" USE_CCACHE=0 \
  sh "$BP/tooling/prospero-clang18" -std=c11 -O2 -fPIC -c "$HERE/commondialog_link_stub.c" -o "$OUT/stub.o"
"$SDK/bin/prospero-lld" --shared -soname libSceCommonDialog.sprx -o "$SDK/target/lib/libSceCommonDialog.so" "$OUT/stub.o"
rm -rf "$OUT"; echo "installed $SDK/target/lib/libSceCommonDialog.so"
