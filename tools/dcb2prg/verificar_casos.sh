#!/bin/bash
# Pasa el round-trip a una CARPETA de casos de prueba, no a un solo .dcb.
#
#   ./verificar_casos.sh <carpeta_con_prg> <ruta_del_bgdc> [.prg ...]
#
# Para cada <caso>.prg:
#     bgdc  ->  caso.dcb  ->  decompile.py  ->  caso-des.prg  ->  bgdc  ->  caso-des.dcb
# y compara los DOS .dcb con verificar_roundtrip.py.
#
# OJO: que el descompilado COMPILE no demuestra nada. Un END de mas al final de
# un proceso compila igual de bien y cambia el programa. Solo la comparacion de
# bytecode lo ve, y por eso este script existe: la comprobacion de "compila sin
# errores" que se usaba antes daba por buenos casos que no lo eran.
#
# El bgdc puede ser el de BennuGD1 o el de BennuGD2: el verificador lee las dos
# revisiones del formato.
set -u
AQUI="$(cd "$(dirname "$0")" && pwd)"

DIR="${1:-}"
BGDC="${2:-${BGDC:-}}"
if [ -z "$DIR" ] || [ -z "$BGDC" ] || [ ! -x "$BGDC" ]; then
    sed -n '2,17p' "$0" | sed 's/^# \{0,1\}//'
    exit 2
fi
shift 2

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

if [ "$#" -gt 0 ]; then
    CASOS=("$@")
else
    CASOS=("$DIR"/*.prg)
fi

fallos=0 hechos=0
for src in "${CASOS[@]}"; do
    [ -f "$src" ] || continue
    n="$(basename "$src" .prg)"
    case "$n" in *-des|*-d) continue;; esac      # salidas de una pasada anterior
    if ! "$BGDC" -o "$TMP/$n.dcb" "$src" >/dev/null 2>&1 || [ ! -f "$TMP/$n.dcb" ]; then
        printf '  %-24s el ORIGINAL no compila (se omite)\n' "$n"
        continue
    fi
    hechos=$((hechos + 1))
    if ! python3 "$AQUI/decompile.py" -o "$TMP/$n-des.prg" --no-extraer \
            "$TMP/$n.dcb" </dev/null >/dev/null 2>&1; then
        printf '  %-24s FALLA: el descompilador aborta\n' "$n"
        fallos=$((fallos + 1))
        continue
    fi
    salida="$(cd "$TMP" && python3 "$AQUI/verificar_roundtrip.py" "$n.dcb" "$BGDC" 2>&1)"
    ident="$(printf '%s' "$salida" | grep -ao 'IDENTICO: [0-9]* de [0-9]*' | head -1)"
    if printf '%s' "$salida" | grep -aq '^OK'; then
        printf '  %-24s OK    %s\n' "$n" "$ident"
    else
        printf '  %-24s FALLA %s\n' "$n" "$ident"
        printf '%s\n' "$salida" | grep -aE '^(FALLA|NO RECOMPILA)' | sed 's/^/        /'
        fallos=$((fallos + 1))
    fi
done

echo
if [ "$fallos" -eq 0 ]; then
    echo "TODO OK ($hechos casos)"
    exit 0
fi
echo "$fallos de $hechos casos FALLAN"
exit 1
