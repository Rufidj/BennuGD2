#!/bin/bash
# Build the BennuGD2 runtime eboot for one PS5 title.
#   apps/bennugd2/build-title.sh PPSA00063 "Pruebas 3D (Bennugd2)" apps/pruebas_63/sce_sys
# Needs: the engine libs in apps/bennugd2/libs/bgdclean (copy them from BennuGD2/build/x86_64-sie-ps5/bin),
#        FFmpeg + deps in apps/bennugd2/libs/ffmpeg, and the title's sce_sys (param.json, icon0.png).
# The eboot is the same for every title; the title id only goes into the signed package metadata.
set -e
TITLE_ID="$1"; APP_NAME="$2"; SCE_SYS="$3"
[ -n "$TITLE_ID" ] && [ -n "$APP_NAME" ] && [ -n "$SCE_SYS" ] || { echo "usage: $0 TITLEID \"Name\" sce_sys_dir"; exit 2; }
cd "$(dirname "$0")/../.."
B=apps/bennugd2; G=$B/libs/bgdclean; F=$B/libs/ffmpeg
INCLUDES="vendor/zlib-shim vendor/bennugd2-core-include vendor/bennugd2-bgdrtm vendor/bennugd2-bgdi vendor/bgd2-realgl-shims vendor/realgl-shims vendor/ps5-opengl-sdk/include vendor/sdl2-real/include/SDL2"
DEFS="__PROSPERO__ __STATIC__ USE_REAL_PS5_OPENGL USE_SDL2_GPU"
# order matters: engine -> ffmpeg and its deps -> libz/samplerate -> GL driver/C++ runtime -> libc gaps LAST
ARCH="$G/libbgdrtm.a $G/libbggfx.a $G/libbginput.a $G/libbgload.a $G/libbgsound.a $G/libmod_3d.a $G/libmod_ads.a $G/libmod_debug.a $G/libmod_gfx.a $G/libmod_iap.a $G/libmod_input.a $G/libmod_misc.a $G/libmod_net.a $G/libmod_sound.a $G/libmod_video.a $G/libsdlhandler.a $F/libtheoradec.a $F/libvorbis.a $F/libogg.a $B/libs/libps5_runtime_gaps.a $B/libs/libSDL2_gpu.a $B/libs/libSDL2_image_shim.a $B/libs/libSDL2_mixer.a $B/libs/libSDL2.a $F/libavformat.a $F/libavcodec.a $F/libswscale.a $F/libswresample.a $F/libavutil.a $F/libssl.a $F/libcrypto.a $F/libiconv.a $F/liblzma.a $F/libbz2.a $B/libs/libz.a $B/libs/libsamplerate.a vendor/ps5-opengl-sdk/lib/libPS5OpenGL.a vendor/payload-sdk-cxx/libcxx.a vendor/payload-sdk-cxx/libcxxabi.a vendor/payload-sdk-cxx/libunwind.a vendor/extras/libextras.a $B/libs/libps5_runtime_gaps.a"
timeout 900 env APP_WRAP_SYMBOLS="malloc calloc realloc free posix_memalign malloc_usable_size fopen opendir gzopen open stat lstat getcwd realpath" \
  APP_DEFINITIONS="$DEFS" APP_INCLUDE_PATHS="$INCLUDES" APP_STATIC_ARCHIVES="$ARCH" APP_SOURCE_DIR="$B/src" \
  APP_PARAM="$SCE_SYS/param.json" APP_SCE_SYS="$SCE_SYS" APP_ASSETS="" TITLE_ID="$TITLE_ID" APP_NAME="$APP_NAME" USE_CCACHE=0 \
  make app 2>&1 | grep -v "warning:" | grep -i "error\|undefined\|Build complete"
ls -l "dist/$TITLE_ID/eboot.bin"
