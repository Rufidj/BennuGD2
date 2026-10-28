#!/bin/bash
export DEVKITPRO=/opt/devkitpro DEVKITA64=/opt/devkitpro/devkitA64 PORTLIBS_PREFIX=/opt/devkitpro/portlibs/switch
export PATH=$DEVKITA64/bin:$DEVKITPRO/tools/bin:$PATH
export PKG_CONFIG_PATH=$PORTLIBS_PREFIX/lib/pkgconfig PKG_CONFIG_LIBDIR=$PORTLIBS_PREFIX/lib/pkgconfig
cd "$(dirname "$0")/src/ffmpeg-7.1"
PREFIX="$(cd "$(dirname "$0")" && pwd)/prefix"
./configure --pkg-config=pkg-config --pkg-config-flags=--static --prefix="$PREFIX" --enable-gpl --enable-version3 --disable-shared --enable-static \
  --cross-prefix=aarch64-none-elf- --enable-cross-compile \
  --arch=aarch64 --cpu=cortex-a57 --target-os=horizon --enable-pic \
  --extra-cflags='-D__SWITCH__ -D_GNU_SOURCE -O2 -march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIC -ftls-model=local-exec -I/opt/devkitpro/libnx/include -I/opt/devkitpro/portlibs/switch/include' \
  --extra-cxxflags='-D__SWITCH__ -D_GNU_SOURCE -O2 -march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIC -ftls-model=local-exec -I/opt/devkitpro/libnx/include -I/opt/devkitpro/portlibs/switch/include' \
  --extra-ldflags='-fPIE -L${PORTLIBS_PREFIX}/lib -L${DEVKITPRO}/libnx/lib' \
  --disable-runtime-cpudetect --disable-programs --disable-debug --disable-doc --disable-autodetect \
  --enable-asm --enable-neon \
  --disable-avdevice --disable-encoders --disable-muxers \
  --enable-swscale --enable-swresample --enable-network \
  --disable-protocols --enable-protocol=file,http,https,ftp,tcp,udp,rtmp,tls,httpproxy,crypto,data \
  --enable-zlib --enable-bzlib --enable-libass --enable-libfreetype --enable-libfribidi --enable-libdav1d \
  --enable-libnx --enable-nvtegra --enable-mbedtls && make -j4 && make install && echo FF_OK
