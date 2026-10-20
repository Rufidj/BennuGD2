#!/usr/bin/env python3
"""Genera módulos .so "de mentira" para el bgdc de BennuGD1.

Los símbolos van prefijados con el nombre del módulo: _dlibaddr() de BennuGD1
compone "<modulo>_<simbolo>" (es lo que hace la macro __bgdexport).

El compilador de BennuGD1 hace dlopen de cada módulo importado y sólo lee sus
TABLAS DE DATOS (nombres de función, tipos de parámetro, constantes y las
declaraciones de globales/locales). No llama a ninguna implementación. Así que
para recompilar un .prg basta con stubs que expongan esas tablas, sin
necesidad de SDL ni del resto de dependencias del motor original.

Sirve para verificar el descompilador: original.dcb -> .prg -> recompilar ->
comparar bytecode.

    python3 make_stub_modules.py <fuentes_bennugd1> <dir_salida>

Los modulos de TERCEROS (mod_steam y demas) no estan en las fuentes de
BennuGD1, asi que sin ellos un .dcb que los importe no se puede recompilar y no
hay forma de verificar la descompilacion. Para esos, el propio .dcb sirve de
fuente: lleva la tabla de sysprocs con el nombre y los tipos de parametro de
cada funcion, que es justo lo que bgdc lee.

    python3 make_stub_modules.py --dcb <fichero.dcb> <modulo> <dir_salida> \
                                 <fuentes_bennugd1>

Se meten en el stub las funciones del .dcb que NO exporte ningun modulo de las
fuentes. El tipo de RETORNO no viaja en el .dcb: se declara INT, que vale salvo
que la funcion devuelva cadena.
"""
import os, re, subprocess, sys

# Enum BASETYPE de BennuGD1 (códigos no contiguos)
BT = {"TYPE_UNDEFINED": 0, "TYPE_INT": 1, "TYPE_DWORD": 2, "TYPE_SHORT": 3,
      "TYPE_WORD": 4, "TYPE_SBYTE": 5, "TYPE_BYTE": 6, "TYPE_CHAR": 8,
      "TYPE_FLOAT": 9, "TYPE_STRING": 16, "TYPE_ARRAY": 17, "TYPE_STRUCT": 18,
      "TYPE_POINTER": 19}

FUNC_RE = re.compile(r'\bFUNC\w*\s*\(\s*"([^"]+)"\s*,\s*"([^"]*)"\s*,\s*(\w+)\s*,')
CONST_RE = re.compile(r'\{\s*"([A-Za-z_0-9]+)"\s*,\s*(\w+)\s*,\s*([^}]+?)\s*\}')
EXPORT_RE = re.compile(r'__bgdexport\(\s*(\w+)\s*,\s*(\w+)\s*\)')


def read(p):
    return open(p, "rb").read().decode("latin-1").replace("\r\n", "\n")


def strip_comments(s):
    s = re.sub(r'/\*.*?\*/', ' ', s, flags=re.S)
    return re.sub(r'//[^\n]*', ' ', s)


def cadenas(text, pos):
    """Concatena las cadenas C consecutivas que siguen a `pos`.

    No vale buscar el ';' que cierra la sentencia: puede aparecer DENTRO de la
    propia cadena ("timer[9];\n"). Se van tomando literales mientras entre uno
    y el siguiente sólo haya espacios; el primer hueco con otra cosa termina.
    """
    out = []
    lit = re.compile(r'"((?:[^"\\]|\\.)*)"')
    m = lit.search(text, pos, pos + 40000)
    while m:
        out.append(m.group(1))
        n = lit.search(text, m.end(), m.end() + 200)
        if not n or text[m.end():n.start()].strip():
            break
        m = n
    # el literal viene con las secuencias de escape sin interpretar: si se deja
    # "\\n" tal cual, bgdc ve una sola linea y no reconoce las declaraciones
    txt = "".join(out)
    for a, b in (("\\n", "\n"), ("\\t", "\t"), ('\\"', '"'), ("\\\\", "\\")):
        txt = txt.replace(a, b)
    return txt


def escapa(s):
    return s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")


def modulo(dirmod):
    """Extrae las tablas de un directorio de módulo."""
    datos = {"funcs": [], "consts": [], "globals": "", "locals": "",
             "types": "", "deps": []}
    for fn in sorted(os.listdir(dirmod)):
        if not fn.endswith((".h", ".c")):
            continue
        t = strip_comments(read(os.path.join(dirmod, fn)))
        for nom, par, ret in FUNC_RE.findall(t):
            if ret in BT:
                datos["funcs"].append((nom, par, BT[ret]))
        for m in EXPORT_RE.finditer(t):
            clave = m.group(2)
            if clave in ("globals_def", "locals_def", "types_def"):
                datos[{"globals_def": "globals", "locals_def": "locals",
                       "types_def": "types"}[clave]] = cadenas(t, m.end())
            elif clave == "constants_def":
                seg = t[m.end():m.end() + 8000]
                for cn, ct, cv in CONST_RE.findall(seg):
                    if ct in BT:
                        datos["consts"].append((cn, BT[ct], cv.strip()))
            elif clave == "modules_dependency":
                for d in re.findall(r'"([^"]+)"', t[m.end():m.end() + 2000]):
                    datos["deps"].append(d)
    return datos


def genera_c(nombre, d):
    L = ['/* Stub generado para el bgdc de BennuGD1: solo tablas de datos. */',
         '#include <stdlib.h>', '',
         'typedef struct { char *name; char *paramtypes; int type; void *func; } DLSYSFUNCS;',
         'typedef struct { char *name; int type; int code; } DLCONSTANT;', '']
    L.append('DLSYSFUNCS %s_functions_exports[] = {' % nombre)
    for nom, par, ret in d["funcs"]:
        L.append('    { "%s", "%s", %d, (void *)1 },' % (nom, par, ret))
    L.append('    { NULL, NULL, 0, NULL }')
    L.append('};')
    L.append('')
    L.append('DLCONSTANT %s_constants_def[] = {' % nombre)
    for cn, ct, cv in d["consts"]:
        val = cv if re.fullmatch(r'-?\d+', cv) else '0'
        L.append('    { "%s", %d, %s },' % (cn, ct, val))
    L.append('    { NULL, 0, 0 }')
    L.append('};')
    L.append('')
    for clave, sym in (("globals", "globals_def"), ("locals", "locals_def"),
                       ("types", "types_def")):
        txt = d.get(clave, "")
        L.append('char *%s_%s = %s;' % (nombre, sym,
                 '"%s"' % escapa(txt) if txt else 'NULL'))
    L.append('')
    L.append('char *%s_modules_dependency[] = {' % nombre)
    for dep in d["deps"]:
        L.append('    "%s",' % dep)
    L.append('    NULL')
    L.append('};')
    return "\n".join(L) + "\n"


def conocidas(src):
    """Nombres de funcion que exporta algun modulo de las fuentes."""
    mods = os.path.join(src, "modules")
    out = set()
    for nombre in sorted(os.listdir(mods)):
        d = os.path.join(mods, nombre)
        if os.path.isdir(d):
            out |= {n.upper() for n, _p, _r in modulo(d)["funcs"]}
    return out


def desde_dcb(ruta, modname, out, src):
    """Stub de un modulo de terceros a partir de la tabla de sysprocs del .dcb."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from dcb import DCB
    from decompile import Decompiler
    dc = Decompiler(DCB(ruta))
    ya = conocidas(src)
    funcs = []
    for _code, (nom, _np, _ex, ptypes) in sorted(dc.sysnames.items()):
        # "#COPY#", "#MEMCOPY#"... son pseudofunciones internas del compilador,
        # no las exporta ningun modulo
        if nom.upper() not in ya and re.fullmatch(r"[A-Za-z_]\w*", nom):
            funcs.append((nom, ptypes, BT["TYPE_INT"]))
    if not funcs:
        print("el .dcb no usa ninguna funcion fuera de los modulos de BennuGD1")
        return 0
    print("funciones que se meten en %s: %s"
          % (modname, ", ".join(n for n, _p, _r in funcs)))
    os.makedirs(out, exist_ok=True)
    c = os.path.join(out, modname + ".c")
    open(c, "w").write(genera_c(modname, {"funcs": funcs, "consts": [],
                                          "globals": "", "locals": "",
                                          "types": "", "deps": []}))
    so = os.path.join(out, modname + ".so")
    # bgdc de BennuGD1 es de 32 bits: un .so de 64 no lo puede abrir
    r = subprocess.run(["gcc", "-m32", "-shared", "-fPIC", "-o", so, c],
                       capture_output=True, text=True)
    if r.returncode:
        r = subprocess.run(["gcc", "-shared", "-fPIC", "-o", so, c],
                           capture_output=True, text=True)
    if r.returncode:
        print("  fallo: %s" % r.stderr.strip().split("\n")[0])
        return 1
    os.remove(c)
    print("stub escrito en %s" % so)
    return 0


def main():
    if sys.argv[1:2] == ["--dcb"]:
        if len(sys.argv) < 6:
            print(__doc__)
            return 2
        return desde_dcb(sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5])
    src, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    mods = os.path.join(src, "modules")
    hechos = 0
    for nombre in sorted(os.listdir(mods)):
        d = os.path.join(mods, nombre)
        if not os.path.isdir(d) or not nombre.startswith(("mod_", "lib")):
            continue
        datos = modulo(d)
        # se generan tambien los modulos sin tablas: otros los declaran como
        # dependencia y bgdc los exige aunque no exporten nada
        c = os.path.join(out, nombre + ".c")
        open(c, "w").write(genera_c(nombre, datos))
        so = os.path.join(out, nombre + ".so")
        r = subprocess.run(["gcc", "-shared", "-fPIC", "-o", so, c],
                           capture_output=True, text=True)
        if r.returncode:
            print("  fallo %s: %s" % (nombre, r.stderr.strip().split("\n")[0]))
        else:
            hechos += 1
            os.remove(c)
    print("modulos stub generados: %d en %s" % (hechos, out))


if __name__ == "__main__":
    sys.exit(main() or 0)
