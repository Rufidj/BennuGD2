#!/bin/bash
# Construye BennuGD1 (32 bits) desde fuentes, con sus dependencias.
#
# BennuGD1 es un motor de 32 bits: su maquina virtual guarda la pila como
# "int *" (ver core/include/instance_st.h), asi que un build de 64 bits trunca
# los punteros y revienta. Hay que compilarlo con -m32.
#
# Sirve para dos cosas:
#   - bgdc: verificar el descompilador recompilando su salida (verificar_roundtrip.py)
#   - bgdi: ejecutar el juego reconstruido en su motor original
#
# Requiere: gcc-multilib y las librerias :i386 de zlib, libpng, SDL2, vorbis,
# ogg, openssl y X11. Para VER el juego hace falta ademas OpenGL de 32 bits
# (en NVIDIA, libnvidia-gl-*:i386; con Mesa, libgl1-mesa-dri:i386).
set -e
W=${1:-$PWD/bgd1build}
mkdir -p "$W"; cd "$W"

echo "== enlaces a las librerias de 32 bits =="
mkdir -p lib32 inc32/SDL2 inc32/openssl
for l in z png16 SDL2-2.0 vorbis vorbisfile vorbisenc ogg crypto ssl m dl pthread \
         X11 Xext Xrandr Xcursor Xi asound pulse; do
    for f in /usr/lib/i386-linux-gnu/lib$l.so.*; do
        [ -e "$f" ] && ln -sf "$f" "lib32/lib${l}.so" && break
    done
done
ln -sf /usr/lib/i386-linux-gnu/libSDL2-2.0.so.0 lib32/libSDL2.so
ln -sf /usr/lib/i386-linux-gnu/libpng16.so.16   lib32/libpng.so
# cabeceras que en Debian van por arquitectura y solo estan las de 64 bits;
# ambas contemplan el tamaño de puntero via __LP64__, asi que valen
cp /usr/include/x86_64-linux-gnu/SDL2/_real_SDL_config.h inc32/SDL2/ 2>/dev/null || true
cp /usr/include/x86_64-linux-gnu/openssl/*.h            inc32/openssl/ 2>/dev/null || true

echo "== SDL 1.2 autentica (1.2.15) =="
# NO usar sdl12-compat: emula SDL 1.2 sobre SDL2 y se comporta distinto en cosas
# que BennuGD1 da por sentadas (SDL_CreateRGBSurfaceFrom devolviendo NULL en
# set_icon, y el escalado por scale_mode al rehacer el modo de video). Con la
# SDL 1.2 de verdad no hace falta parchear el motor.
[ -d sdl12real ] || git clone --depth 1 https://github.com/libsdl-org/SDL-1.2.git sdl12real
cd sdl12real
[ -f Makefile ] || ./configure --prefix=$W/inst32 --host=i686-linux-gnu --build=x86_64-linux-gnu \
  CC="gcc -m32" CFLAGS="-m32 -O2 -fcommon" LDFLAGS="-m32 -L$W/lib32" \
  --disable-static --enable-shared --disable-esd --disable-arts --disable-nas \
  --enable-video-x11 --disable-video-directfb >/dev/null
make -j4 >/dev/null && make install >/dev/null
cd "$W"

export PKG_CONFIG_PATH=$W/inst32/lib/pkgconfig
COMUN="--host=i686-linux-gnu --build=x86_64-linux-gnu CC=gcc -m32"

echo "== libvorbisfile de 32 bits =="
# Debian no empaqueta libvorbisfile:i386 (solo libvorbis/libvorbisenc), y
# SDL_mixer lo necesita para la musica OGG. Se compila desde fuentes.
if [ ! -e lib32/libvorbisfile.so ]; then
    [ -d vorbis ] || git clone --depth 1 https://github.com/xiph/vorbis.git vorbis
    cd vorbis; [ -f configure ] || ./autogen.sh >/dev/null 2>&1 || autoreconf -i
    ./configure --prefix=$W/inst32 --host=i686-linux-gnu --build=x86_64-linux-gnu \
      --disable-oggtest CC="gcc -m32" CFLAGS="-m32 -O2" \
      LDFLAGS="-m32 -L$W/lib32" OGG_CFLAGS="-I/usr/include" OGG_LIBS="-L$W/lib32 -logg" >/dev/null
    make -j4 install >/dev/null
    cd "$W"
    for l in vorbis vorbisfile vorbisenc; do ln -sf $W/inst32/lib/lib$l.so lib32/lib$l.so; done
fi

echo "== SDL_mixer 1.2 =="
[ -d sdlmix ] || git clone --depth 1 -b SDL-1.2 https://github.com/libsdl-org/SDL_mixer.git sdlmix
cd sdlmix; [ -f configure ] || autoreconf -i
./configure --prefix=$W/inst32 --host=i686-linux-gnu --build=x86_64-linux-gnu \
  CC="gcc -m32" CFLAGS="-m32 -O2" LDFLAGS="-m32 -L$W/lib32 -L$W/inst32/lib" \
  --disable-music-mod --disable-music-midi --disable-music-flac \
  --disable-music-mp3 --disable-sdltest >/dev/null
make -j4 install >/dev/null; cd "$W"

echo "== BennuGD1 =="
[ -d bgd1 ] || git clone --depth 1 https://github.com/SplinterGU/BennuGD.git bgd1

# el DES que busca ya no esta en libssl sino en libcrypto
for d in core modules; do
    cd bgd1/$d; [ -f configure ] || autoreconf -i
    ./configure --host=i686-linux-gnu --build=x86_64-linux-gnu CC="gcc -m32" \
      CFLAGS="-m32 -O2 -I$W/inc32" \
      LDFLAGS="-m32 -L$W/lib32 -L$W/inst32/lib -lcrypto" >/dev/null
    make -j4 || true
    cd "$W"
done

echo "== reunir modulos donde bgdc/bgdi los buscan =="
# los buscan en <dir_del_ejecutable>/../modules/, no en el directorio actual
mkdir -p bgd1/core/modules
find bgd1/modules -name "*.so" -path "*/.libs/*" -exec cp {} bgd1/core/modules/ \;
cd bgd1/core/modules
for f in libmod_*.so; do [ -f "$f" ] && mv "$f" "${f#lib}"; done
cd "$W"

echo
echo "listo:"
echo "  bgdc: $W/bgd1/core/bgdc/src/bgdc"
echo "  bgdi: $W/bgd1/core/bgdi/src/bgdi"
echo "  export LD_LIBRARY_PATH=$W/inst32/lib:$W/lib32"
