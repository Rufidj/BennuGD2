#!/usr/bin/env bash
# Compila las dependencias para Android que necesita el motor:
#   mbedTLS  (para el HTTPS del FFmpeg: la tienda de addons y las listas)
#   FFmpeg   (el modulo de video, con TLS para poder descargar por https)
#
#   ./build-android-deps-media.sh [arm64-v8a|x86_64|armeabi-v7a|x86]
#
# Las deja en vendor/android/<target>/<abi> (prefix de la app).
set -e

NDK="${ANDROID_NDK:-$HOME/Android/Sdk/ndk/27.0.12077973}"
TOOLCHAIN="$NDK/toolchains/llvm/prebuilt/linux-x86_64"
API=21
BUILD_JOBS=$(nproc)
PREFIX_BASE="$(pwd)/vendor/android"
WORK="${WORK:-/tmp/opencode/mediadeps}"

ABIS="${1:-arm64-v8a}"
case "$ABIS" in
  arm64-v8a)   ARCH="aarch64"; HOST="aarch64-linux-android" ;;
  x86_64)      ARCH="x86_64"; HOST="x86_64-linux-android" ;;
  armeabi-v7a) ARCH="arm";    HOST="armv7a-linux-androideabi" ;;
  x86)         ARCH="i686";   HOST="i686-linux-android" ;;
esac
PREFIX="$PREFIX_BASE/$HOST/$ABIS"
CC="$TOOLCHAIN/bin/${HOST}${API}-clang"
CXX="$TOOLCHAIN/bin/${HOST}${API}-clang++"
AR="$TOOLCHAIN/bin/llvm-ar"
RANLIB="$TOOLCHAIN/bin/llvm-ranlib"

mkdir -p "$WORK"
cd "$WORK"

# ---------------------------------------------------------------- mbedTLS
if [ ! -d mbedtls-3.6.2 ]; then
  curl -sLO https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.2/mbedtls-3.6.2.tar.bz2
  mkdir -p mbedtls-3.6.2
  tar xjf mbedtls-3.6.2.tar.bz2 -C mbedtls-3.6.2 --strip-components=1
fi
cd mbedtls-3.6.2
rm -rf build-$ABIS
cmake -B build-$ABIS \
  -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=$ABIS -DANDROID_PLATFORM=android-$API -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_TESTING=OFF -DENABLE_PROGRAMS=OFF \
  -DUSE_SHARED_MBEDTLS_LIBRARY=ON -DUSE_STATIC_MBEDTLS_LIBRARY=OFF \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" > /dev/null
cmake --build build-$ABIS -j$BUILD_JOBS > /dev/null
cmake --install build-$ABIS > /dev/null
echo "mbedTLS instalado en $PREFIX"

# ---------------------------------------------------------------- FFmpeg
cd "$WORK"
if [ ! -d ffmpeg-6.1.1 ]; then
  curl -sLO https://ffmpeg.org/releases/ffmpeg-6.1.1.tar.xz
  tar xf ffmpeg-6.1.1.tar.xz
fi
cd ffmpeg-6.1.1
make distclean > /dev/null 2>&1 || true
./configure \
  --target-os=android --arch=$ARCH \
  --cc="$CC" --cxx="$CXX" --ar="$AR" --ranlib="$RANLIB" \
  --enable-cross-compile --enable-shared --disable-static \
  --prefix="$PREFIX" \
  --disable-programs --disable-doc --disable-ffplay --disable-ffprobe --disable-avdevice \
  --disable-avfilter --disable-postproc --disable-vaapi --disable-vdpau --disable-videotoolbox \
  --disable-sdl2 --disable-xlib --disable-iconv --disable-debug \
  --enable-network --enable-protocols --enable-demuxers --enable-decoders --enable-parsers --enable-bsfs \
  --enable-swscale --enable-swresample --enable-avcodec --enable-avformat --enable-avutil \
  --enable-pthreads --enable-jni --enable-mediacodec \
  --enable-mbedtls --enable-version3 \
  --extra-cflags="-I$PREFIX/include" --extra-ldflags="-L$PREFIX/lib" > /dev/null
make -j$BUILD_JOBS > /dev/null
make install > /dev/null
echo "FFmpeg (con HTTPS) instalado en $PREFIX"
grep -E "^CONFIG_(MBEDTLS|HTTPS_PROTOCOL)" ffbuild/config.mak
