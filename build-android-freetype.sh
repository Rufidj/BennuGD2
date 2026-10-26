#!/usr/bin/env bash
# Compila FreeType para una ABI de Android y lo deja en vendor/android/<target>/<abi> (lo que usa build.sh).
#   ./build-android-freetype.sh [armeabi-v7a|arm64-v8a|x86_64|x86]
# Sin PNG, zlib, bzip2, harfbuzz ni brotli: el motor solo lo usa para pintar texto TrueType.
set -e
NDK="${ANDROID_NDK:-$HOME/Android/Sdk/ndk/27.0.12077973}"
ABI="${1:-armeabi-v7a}"
VER=2.13.2
case "$ABI" in
  arm64-v8a)   HOST="aarch64-linux-android" ;;
  x86_64)      HOST="x86_64-linux-android" ;;
  armeabi-v7a) HOST="armv7a-linux-androideabi" ;;
  x86)         HOST="i686-linux-android" ;;
  *) echo "ABI desconocida: $ABI" >&2; exit 2 ;;
esac
PREFIX="$(pwd)/vendor/android/$HOST/$ABI"
WORK="${WORK:-/tmp/opencode/mediadeps}"
mkdir -p "$WORK"; cd "$WORK"
[ -f freetype-$VER.tar.xz ] || curl -sLO https://download.savannah.gnu.org/releases/freetype/freetype-$VER.tar.xz
[ -d freetype-$VER ] || tar xf freetype-$VER.tar.xz
cd freetype-$VER
cmake -B build-$ABI -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=$ABI -DANDROID_PLATFORM=android-21 -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" -DFT_DISABLE_ZLIB=ON -DFT_DISABLE_BZIP2=ON -DFT_DISABLE_PNG=ON \
  -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_BROTLI=ON
cmake --build build-$ABI -j"$(nproc)"
cmake --install build-$ABI
echo "FreeType instalado en $PREFIX"
