#!/usr/bin/env python3
"""Extrae las variables PREDEFINIDAS de Fenix a vars_fenix.json.

Un .dcb de Fenix compilado sin depuracion no lleva tabla de identificadores ni
de variables: NID, NGloVars y NLocVars valen 0. Sin eso, cada acceso sale como
"global@70580" y el fuente es ilegible. Pero las variables del MOTOR (x, y,
graph, mouse, joy...) siempre estan en el mismo sitio: sus declaraciones estan
en fxc/src/main_div.c, en las cadenas locals_def y globals_def, escritas en el
propio lenguaje. De ahi se calcula offset -> nombre.

    python3 build_vars_fenix.py <fuentes_de_fenix> [salida.json]

Las reglas de reparto son las de Fenix: todo escalar ocupa 4 bytes, los STRUCT
van en linea y "nombre[n]" reserva n+1 elementos (indices 0..n, como en DIV).
"""
import json, os, re, sys

W = 4


def saca_cadena(texto, nombre):
    """El contenido de `char * <nombre> = "..." "..." ;` ya sin escapes."""
    m = re.search(r'char\s*\*\s*%s\s*=' % re.escape(nombre), texto)
    if not m:
        return ""
    trozos, i = [], m.end()
    lit = re.compile(r'"((?:[^"\\]|\\.)*)"')
    # Entre dos trozos del literal puede haber COMENTARIOS de C: la declaracion
    # esta partida en "..." /* nota */ "...". Cortando en el primer hueco no
    # vacio se perdia media tabla (de 358 variables a las 1428 primeras).
    hueco = re.compile(r'(?:\s|/\*.*?\*/)*\Z', re.S)
    while True:
        n = lit.search(texto, i)
        if not n or not hueco.match(texto[i:n.start()]):
            break
        trozos.append(n.group(1))
        i = n.end()
    s = "".join(trozos)
    for a, b in (("\\n", "\n"), ("\\t", "\t"), ('\\"', '"'), ("\\\\", "\\")):
        s = s.replace(a, b)
    return s


def reparte(texto, prefijo="", base=0, salida=None, defec=None):
    """Recorre una declaracion y devuelve {offset: nombre}, mas el tamaño."""
    if salida is None:
        salida = {}
    if defec is None:
        defec = {}
    off = base
    # fuera comentarios y valores iniciales, que no cambian el reparto
    texto = re.sub(r'//[^\n]*', ' ', texto)
    toks = [t.strip() for t in re.split(r'[;\n]', texto)]
    i = 0
    lineas = [t for t in toks if t]
    while i < len(lineas):
        l = lineas[i]
        m = re.match(r'STRUCT\s+([A-Za-z_]\w*)\s*(?:\[\s*(\d+)\s*\])?$', l, re.I)
        if m:
            nom, dim = m.group(1), m.group(2)
            # buscar el END que cierra
            hondo, j = 1, i + 1
            while j < len(lineas) and hondo:
                if re.match(r'STRUCT\b', lineas[j], re.I):
                    hondo += 1
                elif re.match(r'END$', lineas[j], re.I):
                    hondo -= 1
                j += 1
            dentro = "\n".join(lineas[i + 1:j - 1])
            n = int(dim) + 1 if dim else 1
            uno, unodef = {}, {}
            tam = reparte(dentro, "", 0, uno, unodef)
            for k in range(n):
                etiq = "%s%s[%d]" % (prefijo, nom, k) if dim else prefijo + nom
                for o, nm in uno.items():
                    salida[off + k * tam + o] = etiq + "." + nm
                # los valores por defecto van con su offset ABSOLUTO: con el
                # local se pisaban unos a otros y salian anclas falsas
                for o, val in unodef.items():
                    defec[off + k * tam + o] = val
            off += n * tam
            i = j
            continue
        if re.match(r'END$', l, re.I):
            i += 1
            continue
        # "pointer x", "a, b, c", "timer[9]"
        l = re.sub(r'^\s*(pointer|string|int|byte|word|dword|float)\s+', '', l, flags=re.I)
        for pieza in l.split(","):
            m = re.match(r'([A-Za-z_]\w*)\s*(?:\[\s*(\d+)\s*\])?\s*(?:=\s*(-?\d+))?\s*$',
                         pieza.strip())
            if not m:
                continue
            nom, dim, ini = m.group(1), m.group(2), m.group(3)
            if ini is not None and not dim:
                defec[off] = int(ini)
            n = int(dim) + 1 if dim else 1
            for k in range(n):
                salida[off + k * W] = ("%s%s[%d]" % (prefijo, nom, k) if dim
                                       else prefijo + nom)
            off += n * W
        i += 1
    return off - base



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
    src = os.path.join(sys.argv[1], "fxc", "src", "main_div.c")
    # El nombre lleva la version: las tablas CAMBIAN entre versiones de Fenix
    # (0.93 inserto funciones y locales por medio y corrio todo lo de detras),
    # asi que hay que guardar una por cada una y elegir la que toque.
    ver = version_dcb(sys.argv[1])
    dst = sys.argv[2] if len(sys.argv) > 2 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "vars_fenix_%04X.json" % ver)
    if not os.path.exists(src):
        print("no encuentro %s" % src)
        return 2
    texto = open(src, encoding="latin-1").read()
    out = {}
    for clave, nombre in (("locals", "locals_def"), ("globals", "globals_def")):
        tab, defec = {}, {}
        tam = reparte(saca_cadena(texto, nombre), salida=tab, defec=defec)
        out[clave] = {str(k): v for k, v in sorted(tab.items())}
        # los valores por defecto sirven para MEDIR hasta donde vale esta tabla
        # en un .dcb concreto: entre versiones de Fenix se añadieron variables
        # al final, y aplicar la tabla nueva a un fichero viejo pone nombres de
        # motor encima de las variables del programa
        out[clave + "_ini"] = {str(k): v for k, v in sorted(defec.items())}
        print("%-8s %3d variables, %d bytes" % (clave, len(tab), tam))
    json.dump(out, open(dst, "w"), indent=0, sort_keys=True)
    print("-> %s" % dst)
    return 0


if __name__ == "__main__":
    sys.exit(main())
