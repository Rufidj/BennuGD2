#!/bin/bash
# Lanza un .dcb de BennuGD2 SITUANDOSE EN SU CARPETA.
#
#   ./jugar.sh <juego.dcb> [argumentos del juego...]
#
# Hace falta porque bgdi NO se cambia al directorio del .dcb: resuelve las rutas
# desde el directorio de trabajo. Y los juegos abren sus assets con rutas
# relativas ("data/adam.fpg", "mod/...", "palettes/", "savegame/"), asi que
# lanzado desde otro sitio no encuentra nada y se cierra al arrancar, sin ningun
# mensaje de error. Es exactamente el mismo motivo por el que el lanzador de
# BennuGD1 hace este mismo cd.
#
# El interprete se busca en $BGDI, en el PATH, y por ultimo en el build que
# acompaña al repo.
set -u

DCB="${1:-}"
if [ -z "$DCB" ]; then
    sed -n '2,15p' "$0" | sed 's/^# \{0,1\}//'
    exit 2
fi
if [ ! -f "$DCB" ]; then
    echo "no existe: $DCB"
    exit 2
fi
shift

if [ -n "${BGDI:-}" ]; then
    INTERPRETE="$BGDI"
elif command -v bgdi >/dev/null 2>&1; then
    INTERPRETE="$(command -v bgdi)"
else
    RAIZ="$(cd "$(dirname "$0")/../.." && pwd)"
    INTERPRETE="$(ls "$RAIZ"/build/*/bin/bgdi 2>/dev/null | head -1)"
fi
if [ -z "${INTERPRETE:-}" ] || [ ! -x "$INTERPRETE" ]; then
    echo "no encuentro bgdi. Ponlo en el PATH o pasa la ruta en \$BGDI."
    exit 2
fi

CARPETA="$(cd "$(dirname "$DCB")" && pwd)"
FICHERO="$(basename "$DCB")"

# aviso temprano: si no estan los assets, el juego se cierra sin decir nada
faltan=""
for d in data mod; do
    [ -e "$CARPETA/$d" ] || faltan="$faltan $d"
done
if [ -n "$faltan" ]; then
    echo "AVISO: en $CARPETA no hay:$faltan"
    echo "       si el juego los usa, se cerrara nada mas arrancar."
    echo "       Extraelos del .dcb original con:"
    echo "           decompile.py -o <salida.prg> --extraer <original.dcb>"
fi

cd "$CARPETA" || exit 1
exec "$INTERPRETE" "$FICHERO" "$@"
