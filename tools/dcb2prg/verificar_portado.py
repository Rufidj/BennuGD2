#!/usr/bin/env python3
"""Compara el .dcb original de BennuGD1 con el .dcb del portado a BennuGD2.

Es la verificación estática del portado: mientras la comparación de assets sólo
ve el código que llega a ejecutarse, ésta recorre TODOS los procesos.

    python3 verificar_portado.py original.dcb portado.dcb [PROCESO]

Los dos motores no comparten tabla de opcodes ni tipos, así que comparar los
flujos en crudo da ruido puro. Antes se normalizan las diferencias que son
CONSECUENCIA ESPERADA del portado, no fallos:

  - Los sufijos de tipo: BennuGD2 tiene DOUBLE y BennuGD1 no, así que las
    mismas operaciones llevan sufijos distintos.
  - Las conversiones (INT2DOUBLE, DOUBLE2INT, INT2FLOAT...): BennuGD2 las
    inserta sola donde v1 no las necesitaba, porque x/y/z/size pasaron de
    entero a real.
  - Las llamadas: un sysproc de v1 puede acabar siendo una función del propio
    programa en v2 (los shims de compatibilidad), así que SYSCALL, SYSPROC,
    CALL y PROC se unifican en un solo token.
  - Las marcas de depuración (SENTENCE, NOP, DEBUG).

Lo que queda tras normalizar sí es comparable: si un proceso baja de parecido,
es que el portado cambió su estructura de verdad.

Además comprueba dos cosas que el parecido de opcodes NO ve, porque no cambian
el flujo, y que han dado fallos reales:

  - El ORDEN DE LOS PARÁMETROS de cada proceso. Las llamadas pasan los
    argumentos por posición: si el portado los coloca en otro orden, cada
    llamada entrega cada valor al parámetro equivocado.
  - La DISPOSICIÓN de las variables que se leen o escriben en disco. Sus
    ficheros los escribió BennuGD1 con enteros de 4 bytes; si el portado los
    declara con los 8 de BennuGD2, la estructura mide el doble y load() deja
    cada pareja de valores del fichero metida en un solo entero.

Devuelve 0 si todo cuadra y 1 si no.
"""
import difflib
import re
import sys

from dcb import DCB
from resolve import Resolver
import mn

# Un sysproc de v1 puede ser una función del programa en v2 (shims), y una
# llamada a proceso puede cambiar de forma: todas cuentan como "una llamada".
LLAMADAS = {"SYSCALL", "SYSPROC", "CALL", "PROC", "NCALL", "CLONE", "TYPE"}

# Ruido de traducción, no diferencias de comportamiento.
IGNORAR = mn.CONVERT | {"SENTENCE", "NOP", "DEBUG"}


# El esqueleto de control: saltos, llamadas, bucles y salidas. Es inmune a como
# se reescriban las expresiones, y es lo que decide si el programa toma las
# mismas decisiones. El portador reescribe "X -= 50" como "X = (INT)(X - 50)"
# (x/y/z eran enteras en BennuGD1 y son reales en BennuGD2, y el codigo de v1
# depende de que trunque), lo que cambia la forma del bytecode sin cambiar el
# comportamiento: comparando solo el control, ese ruido desaparece.
CONTROL = {"JUMP", "JFALSE", "JTRUE", "JTFALSE", "JTTRUE", "JNOCASE",
           "SWITCH", "CASE", "CASE_R", "RETURN", "END", "FRAME", "LLAMADA",
           "REPEAT", "BREAK", "EXITHNDLR", "ERRHNDLR", "RESUME", "RESUMEGOTO"}


def flujo(d, proc):
    """Secuencia normalizada de opcodes de un proceso."""
    dec = mn.decode_v1 if d.W == 4 else mn.decode
    out, i, code = [], 0, proc.code
    while i < len(code):
        name, hasp, _ty, _u = dec(code[i] & 0xFFFF)
        i += 2 if hasp else 1
        if name in IGNORAR:
            continue
        out.append("LLAMADA" if name in LLAMADAS else name)
    # bgdc de BennuGD2 cierra los procesos con un END extra que el de BennuGD1
    # no emite: es generacion de codigo, no comportamiento.
    while out and out[-1] == "END":
        out.pop()
    return out


def firma(P):
    """Nombres de los parametros en el orden REAL: el de los offsets."""
    n = P.d["NParams"]
    n = n if 0 <= n <= 64 else 0
    return [("<local>" if v.name.startswith("id_") else v.name)
            for v in sorted(P.privar, key=lambda v: v.offset)[:n]]


def comparar_firmas(a, b, A, B):
    """El orden de los parametros tiene que ser EL MISMO en los dos .dcb.

    Las llamadas pasan los argumentos por posicion, asi que si el portador
    coloca un parametro en otro sitio, cada llamada del programa entrega cada
    valor al parametro equivocado. Es invisible en el flujo de opcodes: solo
    cambia el operando de los GET_PRIV.
    """
    mal = [(n, firma(A[n]), firma(B[n])) for n in A
           if n in B and firma(A[n]) != firma(B[n])]
    print("\nfirmas (orden de los parametros)")
    print("   procesos con la firma en distinto orden: %d de %d" % (len(mal), len(A)))
    for n, x, y in mal[:10]:
        print("      %s" % n)
        print("         original: %s" % ", ".join(x))
        print("         portado : %s" % ", ".join(y))
    return len(mal)


def vars_de_disco(d):
    """Variables que son argumento de load/save/fread/fwrite, del bytecode.

    La llamada empuja la ruta, la direccion del dato y dos argumentos que
    genera el compilador. El dato es la primera direccion que resuelve a una
    variable con nombre; la que genera el compilador no resuelve a ninguna.
    """
    dec = mn.decode_v1 if d.W == 4 else mn.decode
    r = Resolver(d)
    W, o, codes = d.W, d.h["OSysProcsCodes"], set()
    for _ in range(d.h["NSysProcsCodes"]):
        sid, = d.w(o)
        params, code = d.w(o + W + 4, 2)
        o += 3 * W + 4 + params
        if d.idname.get(sid, "").upper() in ("LOAD", "SAVE", "FREAD", "FWRITE"):
            codes.add(code)
    out = set()
    for proc in d.procs:
        code, i, prev = proc.code, 0, []
        while i < len(code):
            nm, hasp, ty, uns = dec(code[i] & 0xFFFF)
            arg = code[i + 1] if hasp and i + 1 < len(code) else None
            if nm in ("SYSPROC", "SYSCALL") and arg in codes:
                for nm2, a2 in reversed(prev[-10:]):
                    if nm2 != "GLOBAL":
                        continue
                    n = r.name_at(d.glovar, a2)
                    if n:
                        out.add(re.match(r"[A-Za-z_]\w*", n).group(0))
                        break
            prev.append((nm, arg))
            i += 2 if hasp else 1
    return out


def comparar_structs_disco(a, b, datadir=None):
    """Las variables que van a disco tienen que conservar su DISPOSICION.

    Los ficheros los escribio BennuGD1 con enteros de 4 bytes. Si el portado
    los declara con los 8 de BennuGD2, la estructura mide el doble y load()
    deja cada pareja de valores del fichero metida en un solo entero: los datos
    salen desplazados 32 bits o a cero. El tamaño total puede diferir en unos
    pocos bytes de relleno (BennuGD2 alinea a 8) sin que pase nada, porque
    loadtypes() lee campo a campo; lo que NO puede cambiar son los offsets.
    """
    def campos(d, raiz):
        v = next((x for x in d.glovar if x.name == raiz), None)
        if v is None or v.members is None or v.members >= len(d.varspaces):
            return None
        return {m.name: m.offset for m in d.varspaces[v.members]}

    def tam(d, raiz):
        gv = sorted(d.glovar, key=lambda v: v.offset)
        for i, v in enumerate(gv):
            if v.name == raiz:
                fin = gv[i + 1].offset if i + 1 < len(gv) else d.h["SGlobal"]
                return fin - v.offset
        return None

    raices = sorted(vars_de_disco(a))
    print("\nvariables que se leen/escriben en disco: %d" % len(raices))
    tot = mal = 0
    for raiz in raices:
        x, y = campos(a, raiz), campos(b, raiz)
        ta, tb = tam(a, raiz), tam(b, raiz)
        if x is None or y is None:
            print("   %-18s (sin campos que comparar)  %s -> %s bytes" % (raiz, ta, tb))
            continue
        com = set(x) & set(y)
        m = sum(1 for n in com if x[n] != y[n])
        tot += len(com); mal += m
        # unos pocos bytes de mas son relleno (BennuGD2 alinea a 8) y no
        # molestan; lo que delata el fallo es que crezca de verdad
        aviso = ""
        if m or (ta and tb and tb - ta > 8):
            aviso = "   <-- MAL: enteros de 64 bits para un fichero de 32"
        print("   %-18s %4d campos, %d en offset incorrecto   %s -> %s bytes%s"
              % (raiz, len(com), m, ta, tb, aviso))
    print("   TOTAL: %d campos, %d en offset incorrecto" % (tot, mal))
    if datadir:
        print("   (compara ademas el tamaño con los ficheros de %s)" % datadir)
    return mal


def main():
    orig, port = sys.argv[1], sys.argv[2]
    quiere = sys.argv[3].upper() if len(sys.argv) > 3 else None

    a, b = DCB(orig), DCB(port)
    A = {p.name.upper(): p for p in a.procs}
    B = {p.name.upper(): p for p in b.procs}

    # Dos comprobaciones que el parecido de opcodes NO ve, porque no cambian el
    # flujo: el orden de los parametros y la disposicion de las structs que se
    # leen o escriben en disco. Las dos han dado fallos reales.
    mal_firmas = comparar_firmas(a, b, A, B)
    mal_campos = comparar_structs_disco(a, b)

    faltan = sorted(set(A) - set(B))
    sobran = sorted(set(B) - set(A))
    print("procesos: original %d, portado %d" % (len(A), len(B)))
    if faltan:
        print("  NO PORTADOS (%d): %s" % (len(faltan), faltan[:8]))
    if sobran:
        print("  añadidos por el portador (%d): %s" % (len(sobran), sobran[:8]))

    filas = []
    for n, p in A.items():
        q = B.get(n)
        if not q or (quiere and n != quiere):
            continue
        x, y = flujo(a, p), flujo(b, q)
        r = difflib.SequenceMatcher(None, x, y).ratio()
        cx = [t for t in x if t in CONTROL]
        cy = [t for t in y if t in CONTROL]
        rc = difflib.SequenceMatcher(None, cx, cy).ratio() if (cx or cy) else 1.0
        filas.append((rc, r, n, len(x), len(y), len(cx), len(cy)))

    if not filas:
        return 1
    filas.sort()
    med = sum(f[1] for f in filas) / len(filas)
    medc = sum(f[0] for f in filas) / len(filas)
    print("\n%d procesos comparados" % len(filas))
    print("  parecido medio del flujo completo   : %.4f" % med)
    print("  parecido medio del ESQUELETO DE CONTROL: %.4f" % medc)
    for corte in (1.0, 0.999, 0.99, 0.95):
        print("    procesos con control >= %.3f: %d"
              % (corte, sum(1 for f in filas if f[0] >= corte)))

    print("\nlos peores por esqueleto de control:")
    print("  %-26s %8s %8s   %s" % ("proceso", "control", "completo", "saltos+llamadas"))
    for rc, r, n, lx, ly, cx, cy in filas[:12]:
        print("  %-26s %8.4f %8.4f   %5d -> %-5d (%+d)"
              % (n, rc, r, cx, cy, cy - cx))

    if quiere and filas:
        p, q = A[quiere], B[quiere]
        x, y = flujo(a, p), flujo(b, q)
        print("\ndiferencias en %s:" % quiere)
        for tag, i1, i2, j1, j2 in difflib.SequenceMatcher(None, x, y).get_opcodes():
            if tag != "equal":
                print("  %-8s orig[%d:%d]=%s" % (tag, i1, i2, " ".join(x[i1:i2])[:70]))
                print("           port[%d:%d]=%s" % (j1, j2, " ".join(y[j1:j2])[:70]))

    print("\n" + "=" * 62)
    if mal_firmas or mal_campos:
        print("FALLA: %d proceso(s) con la firma en distinto orden y %d campo(s)"
              " de disco en offset incorrecto" % (mal_firmas, mal_campos))
        return 1
    print("OK: mismas firmas y misma disposicion de las structs de disco")
    return 0


if __name__ == "__main__":
    sys.exit(main())
