#!/bin/sh
# Builds SDL2_mixer 2.8.0 (src copied from the emsdk port) for PS5 as libSDL2_mixer.a.
# Codecs: WAV, AIFF, VOC (built in), MP3 via minimp3, OGG via stb_vorbis. No CMD/MIDI/FLAC/etc.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
APP=${APP:?set APP to the native-app-boilerplate checkout}
OUT=$HERE/obj; mkdir -p "$OUT"
CC="env PS5_PAYLOAD_SDK=$APP/.deps/native/ps5-payload-sdk PS5_CLANG=/usr/bin/clang-18 USE_CCACHE=0 sh $APP/tooling/prospero-clang18"
FLAGS="-std=c11 -O2 -ffunction-sections -fdata-sections -w -D__PROSPERO__ -DMUSIC_WAV -DMUSIC_MP3_MINIMP3 -DMUSIC_OGG -DOGG_USE_STB -DWAV_MUSIC -DMAX_NUMBER_OF_CHANNELS=16 -I$HERE/include -I$HERE/src -I$HERE/src/codecs -I$APP/vendor/sdl2-real/include/SDL2"
SRCS="src/mixer.c src/music.c src/effects_internal.c src/effect_position.c src/effect_stereoreverse.c src/utils.c src/codecs/load_aiff.c src/codecs/load_voc.c src/codecs/music_wav.c src/codecs/mp3utils.c src/codecs/music_minimp3.c src/codecs/music_ogg_stb.c"
OBJS=""
for s in $SRCS; do
  o=$OUT/$(echo $s | tr / _).o
  $CC $FLAGS -c "$HERE/$s" -o "$o"
  OBJS="$OBJS $o"
done
rm -f "$HERE/libSDL2_mixer.a"
ar rcs "$HERE/libSDL2_mixer.a" $OBJS
echo "built $HERE/libSDL2_mixer.a"
