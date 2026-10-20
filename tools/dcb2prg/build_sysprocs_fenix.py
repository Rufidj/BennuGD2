#!/usr/bin/env python3
"""Extrae la tabla de funciones del sistema de Fenix a sysprocs_fenix.json.

Fenix NO guarda esa tabla en el .dcb: donde BennuGD puso NSysProcsCodes /
OSysProcsCodes, Fenix tenia campos reservados. Asi que el operando de un
SYSCALL/SYSPROC es un codigo FIJO del interprete, y para ponerle nombre hay que
sacarlo de las fuentes.

    python3 build_sysprocs_fenix.py <fuentes_de_fenix> [salida.json]

Se lee include/sysprocs.h, cuyas filas son:
    { 0x138, "LOAD_WAV", "S", TYPE_DWORD, 1, SYSMACRO(fxi_load_wav) },
      codigo  nombre     tipos de parametro   nº de parametros
"""
import json, os, re, sys

FILA = re.compile(r'\{\s*(0x[0-9A-Fa-f]+)\s*,\s*"([^"]+)"\s*,\s*"([^"]*)"\s*,'
                  r'\s*\w+\s*,\s*(-?\d+)')



def version_dcb(src):
    """La DCB_VERSION que declara ese arbol de fuentes de Fenix."""
    import re
    h = os.path.join(src, "include", "dcb.h")
    if os.path.exists(h):
        m = re.search(r"#define\s+DCB_VERSION\s+(0x[0-9A-Fa-f]+)",
                      open(h, encoding="latin-1").read())
        if m:
            return int(m.group(1), 16)
    return 0x0500


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    src = os.path.join(sys.argv[1], "include", "sysprocs.h")
    # El nombre lleva la version: las tablas CAMBIAN entre versiones de Fenix
    # (0.93 inserto funciones y locales por medio y corrio todo lo de detras),
    # asi que hay que guardar una por cada una y elegir la que toque.
    ver = version_dcb(sys.argv[1])
    dst = sys.argv[2] if len(sys.argv) > 2 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "sysprocs_fenix_%04X.json" % ver)
    if not os.path.exists(src):
        print("no encuentro %s" % src)
        return 2
    tab = {}
    for l in open(src, encoding="latin-1"):
        m = FILA.match(l.strip())
        if m:
            code, nom, ptypes, npar = m.groups()
            tab[str(int(code, 16))] = [nom, int(npar), ptypes]
    json.dump(tab, open(dst, "w"), indent=0, sort_keys=True)
    print("%d funciones -> %s" % (len(tab), dst))
    return 0


if __name__ == "__main__":
    sys.exit(main())
