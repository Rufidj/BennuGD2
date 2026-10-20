#!/usr/bin/env python3
"""Extrae las tablas de símbolos exportados de un árbol de fuentes BennuGD.

Recorre los *_exports.h y recoge lo que los módulos publican al lenguaje:
funciones (macro FUNC), constantes (DLCONSTANT) y variables globales/locales
(globals_def / locals_def). Sirve para comparar BennuGD1 con BennuGD2 y saber
qué se puede portar automáticamente y qué no existe en el destino.

    python3 build_symtab.py <dir_fuentes> <salida.json>
"""
import json, os, re, sys

FUNC_RE = re.compile(
    r'\bFUNC\w*\s*\(\s*"([^"]+)"\s*,\s*"([^"]*)"\s*,\s*(\w+)\s*,', re.S)
CONST_RE = re.compile(r'\{\s*"([A-Z_0-9]+)"\s*,\s*(\w+)\s*,\s*(-?\w+)\s*\}')
EXPORT_RE = re.compile(r'__bgdexport\(\s*(\w+)\s*,\s*(\w+)\s*\)')
# declaraciones dentro de las cadenas de globals_def / locals_def
DECL_RE = re.compile(r'\b([A-Za-z_][A-Za-z_0-9]*)\s*(?:\[[^\]]*\])?\s*(?:=|;)')

TYPEWORDS = {"TYPE_INT", "TYPE_DWORD", "TYPE_SHORT", "TYPE_WORD", "TYPE_BYTE",
             "TYPE_SBYTE", "TYPE_CHAR", "TYPE_FLOAT", "TYPE_DOUBLE", "TYPE_STRING",
             "TYPE_POINTER", "TYPE_UNDEFINED", "TYPE_QWORD", "TYPE_INT32"}


def read(path):
    with open(path, "rb") as fh:
        return fh.read().decode("latin-1").replace("\r\n", "\n")


def strip_comments(s):
    s = re.sub(r'/\*.*?\*/', ' ', s, flags=re.S)
    return re.sub(r'//[^\n]*', ' ', s)


def parse_decls(body, fields=None, types=None):
    """Nombres declarados en el nivel superior de un globals_def / locals_def.

    Respeta STRUCT..END: de un struct interesa su nombre, no el de sus campos.
    Si se pasa `fields`, ademas anota cada campo como CAMPO -> "struct.campo",
    que es lo que hace falta para portar globales sueltas de v1 a structs de v2.
    """
    names, depth, path = [], 0, []
    for raw in body.replace(";", ";\n").split("\n"):
        line = raw.strip()
        if not line:
            continue
        low = line.lower()
        if low.startswith("end"):
            depth = max(0, depth - 1)
            if path:
                path.pop()
            continue
        m = re.match(r'(?i)struct\s+([A-Za-z_]\w*)', line)
        if m:
            if depth == 0:
                names.append(m.group(1).upper())
            path.append(m.group(1).lower())
            depth += 1
            continue

        # "<tipo> <nombre>[dim] = valor ;"  ->  queremos <nombre>
        # el tipo puede faltar: BennuGD1 declara "sound_freq = 22050;" a secas
        m = re.match(r'(?i)(?:(?:[A-Za-z_]\w*\s*\*?\s+)+)?([A-Za-z_]\w*)'
                     r'\s*(?:\[[^\]]*\])?\s*(?:=|;|$)', line)
        if m:
            nm = m.group(1).upper()
            if nm.startswith("TYPE_") or nm in ("STRUCT", "END"):
                continue
            ty = m2.group(1).upper() if (m2 := re.match(
                r'(?i)\s*([A-Za-z_]\w*)\s*\*?\s+[A-Za-z_]\w*', line)) else None
            if ty and not ty.startswith("TYPE_") and types is not None:
                types.setdefault(nm, ty)
            if depth == 0:
                names.append(nm)
            elif fields is not None:
                fields.setdefault(nm, ".".join(path + [nm.lower()]))
    return names


def module_of(path):
    base = os.path.basename(os.path.dirname(path))
    return base or os.path.basename(path)


def collect_strings(text, start):
    """Concatena las cadenas C consecutivas que siguen a una posición."""
    out, i = [], start
    for m in re.finditer(r'"((?:[^"\\]|\\.)*)"', text[start:start + 20000]):
        seg = text[start + i - start:start + m.start()] if False else None
        out.append(m.group(1))
        # parar al cerrar la sentencia
        nxt = text[start + m.end(): start + m.end() + 40]
        if ";" in nxt.split('"')[0]:
            break
    return "".join(out).replace("\\n", "\n")


def scan(tree):
    funcs, consts, gvars, lvars = {}, {}, {}, {}
    gfields, lfields = {}, {}
    gtypes, ltypes = {}, {}
    for root, _dirs, files in os.walk(tree):
        for fn in files:
            if not fn.endswith((".h", ".c")):
                continue
            path = os.path.join(root, fn)
            try:
                text = strip_comments(read(path))
            except OSError:
                continue
            # además de los *_exports.h, el core declara argc/argv/os_id y las
            # locales base en main*.c mediante las mismas cadenas globals_def
            if ("exports" not in fn
                    and not fn.startswith(("mod_", "libmod_", "lib"))
                    and "globals_def" not in text and "locals_def" not in text):
                continue
            mod = module_of(path)

            for name, params, rtype in FUNC_RE.findall(text):
                if rtype not in TYPEWORDS:
                    continue
                funcs.setdefault(name.upper(), []).append(
                    {"module": mod, "params": params, "ret": rtype})

            spots = [(m.end(), m.group(2)) for m in EXPORT_RE.finditer(text)]
            spots += [(m.end(), m.group(1))
                      for m in re.finditer(r'\b(globals_def|locals_def)\s*=', text)]
            for _end, _kind in spots:
                m = type("M", (), {"end": lambda self, e=_end: e})()
                kind = _kind
                if kind in ("globals_def", "locals_def"):
                    body = collect_strings(text, m.end())
                    is_g = kind == "globals_def"
                    tgt = gvars if is_g else lvars
                    ftgt = gfields if is_g else lfields
                    ttgt = gtypes if is_g else ltypes
                    for nm in parse_decls(body, ftgt, ttgt):
                        tgt.setdefault(nm, mod)
                elif kind == "constants_def":
                    seg = text[m.end():m.end() + 8000]
                    for cn, ct, cv in CONST_RE.findall(seg):
                        consts.setdefault(cn.upper(), {"module": mod, "value": cv})
    return {"functions": funcs, "constants": consts, "globals": gvars,
            "locals": lvars, "global_fields": gfields, "local_fields": lfields,
            "global_types": gtypes, "local_types": ltypes}


if __name__ == "__main__":
    data = scan(sys.argv[1])
    with open(sys.argv[2], "w") as fh:
        json.dump(data, fh, indent=1, sort_keys=True)
    print("%s -> funciones=%d constantes=%d globales=%d locales=%d campos=%d" % (
        sys.argv[2], len(data["functions"]), len(data["constants"]),
        len(data["globals"]), len(data["locals"]),
        len(data["global_fields"]) + len(data["local_fields"])))
