#!/bin/bash
# Pasa TODAS las comprobaciones estaticas de una vez y falla a la primera.
#
#   ./verificar.sh <original.dcb> <ruta_del_bgdc_de_bennugd1> [portado.dcb]
#
# El bgdc de BennuGD1 tambien se puede dar en $BGDC. Se construye con
# construir_bennugd1_32.sh (BennuGD1 es de 32 bits: en 64 truncan los punteros).
#
#   1. round-trip del descompilador   original.dcb -> .prg -> .dcb'
#      (cabeceras, flujo de opcodes, operandos, saltos, globales, cadenas y
#       valores iniciales)
#   2. verificacion del portado       original.dcb vs portado.dcb
#      (orden de los parametros y disposicion de las structs que van a disco)
#
# Lo que NO cubre esto es el comportamiento en marcha: para eso esta
# vigilar_atascos.py, que hay que ejecutar jugando.
set -u
AQUI="$(cd "$(dirname "$0")" && pwd)"

ORIG="${1:-}"
BGDC="${2:-${BGDC:-}}"
PORT="${3:-}"

if [ -z "$ORIG" ] || [ -z "$BGDC" ]; then
    sed -n '2,16p' "$0" | sed 's/^# \{0,1\}//'
    [ -n "$ORIG" ] && [ -z "$BGDC" ] && echo "falta el bgdc de BennuGD1 (argumento 2 o \$BGDC)"
    exit 2
fi
if [ ! -x "$BGDC" ]; then
    echo "no puedo ejecutar el bgdc de BennuGD1: $BGDC"
    exit 2
fi

fallos=0
linea() { printf '\n%s\n' "==============================================================" ; }

linea
echo "1/2  ROUND-TRIP DEL DESCOMPILADOR"
linea
if python3 "$AQUI/verificar_roundtrip.py" "$ORIG" "$BGDC"; then
    echo ">>> round-trip OK"
else
    echo ">>> round-trip FALLA"
    fallos=$((fallos + 1))
fi

if [ -n "$PORT" ]; then
    linea
    echo "2/2  VERIFICACION DEL PORTADO A BennuGD2"
    linea
    if python3 "$AQUI/verificar_portado.py" "$ORIG" "$PORT"; then
        echo ">>> portado OK"
    else
        echo ">>> portado FALLA"
        fallos=$((fallos + 1))
    fi
else
    linea
    echo "2/2  portado: omitido (no se paso el .dcb portado)"
fi

linea
if [ "$fallos" -eq 0 ]; then
    echo "TODO OK"
    exit 0
fi
echo "$fallos comprobacion(es) FALLAN"
exit 1
