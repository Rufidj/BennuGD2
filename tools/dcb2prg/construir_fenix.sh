#!/bin/bash
# Compila el fxc y el fxi de Fenix. El fxc hace falta para verificar el
# round-trip de un .dcb de Fenix, igual que el bgdc de BennuGD1 para los suyos;
# el fxi, para poder EJECUTAR lo recompilado y ver que funciona.
#
#   ./construir_fenix.sh <fuentes_de_fenix> [dir_salida]
#
# Detalles que cuestan de encontrar:
#   -m32          Fenix es de 32 bits; en 64 los punteros no caben
#   -DTARGET_linux/-DTARGET_Linux   define __stdcall y mete dlfcn.h. Las dos
#                 grafias: la 0.86g usa la minuscula y la 0.93 la mayuscula
#   -DVERSION     la ponia autotools desde configure.in
#   -fcommon      el codigo es anterior a gcc 10: hay definiciones tentativas
#                 repetidas ("debug" en main.c y c_main.c)
#   libz de 32 bits: se enlaza por ruta, porque no suele haber el enlace .so
#   -fgnu89-inline   (solo fxi) el "_inline" de g_draw.c con semantica C99 no
#                 emite el cuerpo: 20 simbolos sin definir al enlazar
#   img_png.c     usa la API de libpng 1.2 (png_ptr->jmpbuf, info_ptr->palette):
#                 se parchea sobre una copia, sin tocar las fuentes
#
# Necesita ademas las cabeceras de SDL 1.2 de 32 bits (solo las cabeceras: fxc
# no llama a SDL, las usa para los tipos).
set -u
SRC="${1:-}"
OUT="${2:-$(pwd)/fenixbuild}"
SDL="${SDL_INC:-}"
if [ -z "$SRC" ] || [ ! -d "$SRC/fxc/src" ]; then
    sed -n '2,18p' "$0" | sed 's/^# \{0,1\}//'
    exit 2
fi
if [ -z "$SDL" ]; then
    SDL="$(dirname "$(find / -name SDL_types.h -path '*32*' 2>/dev/null | head -1)")"
    [ -z "$SDL" ] && SDL="$(dirname "$(find / -name SDL_types.h 2>/dev/null | head -1)")"
fi
[ -z "$SDL" ] && { echo "no encuentro SDL_types.h; ponlo en \$SDL_INC"; exit 2; }

Z="$(ls /usr/lib/i386-linux-gnu/libz.so* 2>/dev/null | head -1)"
[ -z "$Z" ] && Z="-lz"

mkdir -p "$OUT/obj" && cd "$OUT/obj" || exit 1
# VERSION la ponia autotools desde configure.in; sin ella no compila main.c
VER="$(sed -n 's/.*AC_INIT([^,]*, *\([^)]*\)).*/\1/p' "$SRC/configure.in" 2>/dev/null | head -1)"
[ -z "$VER" ] && VER="0.0"
DEF="-m32 -w -fcommon -DTARGET_linux -DTARGET_Linux -DUSE_SDL -DVERSION=\"$VER\" -DPACKAGE=\"fenix\""
INC="-I$SRC/include -I$SRC/fxc/inc -I$SDL"
# Se compila lo que HAYA, no una lista fija: el juego de ficheros cambia entre
# versiones (la 0.93 cambio mmx_scale2x.c por scaler_*.c, por ejemplo).
# dcbw_old.c es el escritor viejo que ya no entra en el Makefile: duplicaria
# simbolos.
for f in "$SRC/fxc/src"/*.c "$SRC/common/files.c" "$SRC/common/xctype.c"; do
    b="$(basename "$f" .c)"
    [ "$b" = "dcbw_old" ] && continue
    gcc $DEF $INC -c -o "$b.o" "$f" || exit 1
done
# El fxc de la 0.93 ya enlaza con SDL (el de la 0.86g no lo necesitaba)
SDLLIB="${SDL_LIB:-$(dirname "$(find / -name 'libSDL-1.2.so*' 2>/dev/null | head -1)")}"
SDLOPT=""
[ -n "$SDLLIB" ] && SDLOPT="-L$SDLLIB -Wl,-rpath,$SDLLIB -lSDL"
gcc -m32 -fcommon -o "$OUT/fxc" ./*.o $SDLOPT "$Z" -ldl || exit 1
echo "fxc en $OUT/fxc"

# ---- fxi (el interprete) ----
PNG="$(ls /usr/lib/i386-linux-gnu/libpng16.so* 2>/dev/null | head -1)"
if [ -z "$SDLLIB" ] || [ -z "$PNG" ]; then
    echo "sin SDL 1.2 o libpng de 32 bits: me quedo solo con el fxc"
    cd "$OUT" && rm -rf obj
    exit 0
fi
rm -f ./*.o
DEF="$DEF -fgnu89-inline"
INC="-I$SRC/include -I$SRC/fxi/inc -I$SDL"     # el fxi tiene sus propias cabeceras
# la API de libpng cambio: se parchea una copia, las fuentes no se tocan
sed -e 's/png_ptr->jmpbuf/png_jmpbuf(png_ptr)/g' \
    -e '/info_ptr->palette *= *NULL/d' "$SRC/fxi/src/img_png.c" > img_png_fix.c
gcc $DEF $INC -c -o img_png.o img_png_fix.c || exit 1
for f in "$SRC/fxi/src"/*.c "$SRC/common"/*.c; do
    b="$(basename "$f" .c)"
    [ "$b" = "img_png" ] && continue          # se usa la copia parcheada
    gcc $DEF $INC -c -o "$b.o" "$f" || exit 1
done
# La 0.93 mueve la ventana con Xlib (XQueryTree/XMoveWindow), la 0.86g no
X11="$(ls /usr/lib/i386-linux-gnu/libX11.so* /home/*/bgd1build/lib32/libX11.so* 2>/dev/null | head -1)"
gcc -m32 -fcommon -o "$OUT/fxi" ./*.o -L"$SDLLIB" -Wl,-rpath,"$SDLLIB" \
    -lSDL -lSDL_mixer "$PNG" "$Z" ${X11:+"$X11"} -lm -ldl -lpthread || exit 1
cd "$OUT" && rm -rf obj
echo "fxi en $OUT/fxi"
"$OUT/fxc" 2>&1 | head -2
