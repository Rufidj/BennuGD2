#!/usr/bin/env python3
"""Verifica el descompilador recompilando su salida y comparando los dos .dcb.

Es el único control objetivo que no depende de jugar al juego:

    original.dcb  ->  decompile.py  ->  .prg  ->  compilador  ->  .dcb'

y luego se comparan las dos tablas. Sirve para los tres motores: se le pasa el
compilador que toque -- el bgdc de BennuGD1, el de BennuGD2 o el fxc de Fenix --
y el que se use tiene que ser el del motor del .dcb, no otro.

Compara, por este orden:
  - las cabeceras y el tamaño de codigo de cada proceso
  - el flujo de opcodes, cortado en el primer END (lo de detras son
    inicializadores, no se ejecuta, y mete ruido en 175 procesos)
  - los OPERANDOS que tienen que coincidir: PRIVATE/GET_PRIV/LOCAL/GET_LOCAL,
    ARRAY/INDEX y los destinos de salto; y que cada GLOBAL siga apuntando a la
    misma variable, aunque su offset se haya movido.

Lo ultimo es lo que de verdad importa: un parametro colocado en otra posicion
de la firma o un indice de array mal reconstruido NO cambian el flujo de
opcodes, solo el operando de un GET_PRIV o de un ARRAY. Mirando unicamente los
mnemonicos salian "identicos" 346 de 357 procesos con el fallo dentro.

Devuelve 0 si todo cuadra y 1 si no, para poder encadenarlo.

Necesita:
  - el compilador del motor del .dcb: el bgdc de BennuGD1 (SplinterGU/BennuGD,
    core/bgdc/src/bgdc), el de BennuGD2, o el fxc de Fenix
    (construir_fenix.sh)
  - los modulos stub de make_stub_modules.py, copiados donde bgdc los busca:
    <dir_de_bgdc>/../modules/

    python3 verificar_roundtrip.py <original.dcb> <ruta_bgdc>
"""
import difflib, io, os, re, subprocess, sys, tempfile
from dcb import DCB
from resolve import Resolver
import mn

AQUI = os.path.dirname(os.path.abspath(__file__))


# Operandos que TIENEN que coincidir exactamente: son offsets dentro del propio
# proceso o pasos de array, y no dependen del orden en que compile bgdc.
OPER_EXACTOS = {"PRIVATE", "GET_PRIV", "PUBLIC", "GET_PUBLIC",
                "LOCAL", "GET_LOCAL", "ARRAY", "INDEX"}
SALTOS = {"JUMP", "JFALSE", "JTRUE", "JTFALSE", "JNFALSE", "JNTRUE", "JFTRUE"}
# GLOBAL/GET_GLOBAL pueden ir desplazados (los modulos declaran sus propias
# globales antes, y no siempre del mismo tamaño), asi que el offset en crudo no
# vale: lo que se compara es la VARIABLE a la que apunta.
GLOBALES = {"GLOBAL", "GET_GLOBAL"}


def instrucciones(d, proc, solo_codigo=True, con_marcas=False):
    """Instrucciones (offset, mnemonico, operando) de un proceso.

    Con solo_codigo, corta en el primer END: lo que va detras es la zona de
    inicializadores, no se ejecuta nunca, y el descompilador emite ahi un END de
    mas en todos los procesos. Sin cortar, ese ruido tapa las diferencias
    reales: da "distinto" en 175 procesos de los que solo 7 lo son.
    """
    # La tabla de opcodes depende del motor: Fenix hasta la 0x0500 tiene los
    # suyos. Decodificar un .dcb de Fenix con la tabla de BennuGD daba
    # mnemonicos corridos y la comparacion no valia nada.
    if getattr(d, "fenix", False) and d.version < 0x0600:
        dec, suf = mn.decode_v0, mn.TYPESUFFIX_V1
    elif d.W == 4:
        dec, suf = mn.decode_v1, mn.TYPESUFFIX_V1
    else:
        dec, suf = mn.decode, mn.TYPESUFFIX
    out, i, code = [], 0, proc.code
    while i < len(code):
        name, hasp, ty, uns = dec(code[i] & 0xFFFF)
        # los marcadores de depuracion no son codigo: si un lado se compilo con
        # -g y el otro no, taparian cualquier comparacion
        if name in ("SENTENCE", "DEBUG") and not con_marcas:
            i += 2 if hasp else 1
            continue
        out.append((i, name + suf.get(ty, "") + (".u" if uns else ""), name,
                    code[i + 1] if hasp else None))
        if solo_codigo and name == "END":
            break
        i += 2 if hasp else 1
    return out


def mapa_indices(d, proc):
    """offset -> numero de instruccion REAL, contando desde el principio.

    Los marcadores de depuracion no cuentan, pero un salto SI puede apuntar a
    uno (el compilador salta al principio de la sentencia, que es el marcador),
    asi que su offset tiene que resolver a la instruccion que va detras.
    """
    todo = instrucciones(d, proc, solo_codigo=False, con_marcas=True)
    mapa, k = {}, 0
    for off, _, nm, _ in todo:
        mapa[off] = k
        if nm not in ("SENTENCE", "DEBUG"):
            k += 1
    return mapa


def opcodes(d, proc):
    """Secuencia de mnemonicos de un proceso, sin operandos.

    Los operandos de cadena, proceso y sysproc no sirven para comparar: sus
    indices dependen del orden en que el compilador los va creando, y ese orden
    cambia al recompilar. Los demas SI se comparan, en comparar_operandos().
    """
    return [t[1] for t in instrucciones(d, proc)]


def comparar_operandos(a, b, A, B):
    """Compara los operandos que deben coincidir, no solo los mnemonicos.

    Es la comprobacion que faltaba: un parametro colocado en la posicion
    equivocada de la firma o un indice de array mal reconstruido dejan el flujo
    de opcodes INTACTO y solo cambian el operando de un GET_PRIV o de un ARRAY.
    Mirando unicamente los mnemonicos, 346 de 357 procesos salian "identicos"
    con el fallo dentro.
    """
    vars_mal, saltos_mal, glob_mal = {}, {}, {}
    ej, ejg = {}, {}
    ra, rb = Resolver(a), Resolver(b)

    def gnombre(r, d, off):
        """Nombre al que apunta un offset de global, sin el subindice.

        Se quita el sufijo _VAR: el descompilador renombra asi, a proposito, las
        variables que se llaman igual que un proceso, porque BennuGD lo rechaza.
        """
        n = r.name_at(d.glovar, off)
        if not n:
            return None
        raiz = re.match(r'[A-Za-z_]\w*', n).group(0)
        return raiz[:-4] if raiz.endswith("_VAR") else raiz

    for n, p in A.items():
        q = B.get(n)
        if not q:
            continue
        x, y = instrucciones(a, p), instrucciones(b, q)
        if [t[1] for t in x] != [t[1] for t in y]:
            continue                     # flujo distinto: ya sale en el otro informe
        # Un salto se compara por la INSTRUCCION a la que va, no por el offset
        # de byte: si un lado se compilo con -g, los marcadores de depuracion
        # corren todos los offsets y saldrian "distintos" saltos que van al
        # mismo sitio.
        ix, iy = mapa_indices(a, p), mapa_indices(b, q)
        for (off, _, nm, o1), (_, _, _, o2) in zip(x, y):
            if o1 is None or o1 == o2:
                continue
            if nm in OPER_EXACTOS:
                vars_mal[n] = vars_mal.get(n, 0) + 1
                ej.setdefault(n, (off, nm, o1, o2))
            elif nm in SALTOS:
                if ix.get(o1, -1) == iy.get(o2, -2):
                    continue             # va a la misma instruccion
                saltos_mal[n] = saltos_mal.get(n, 0) + 1
                ej.setdefault(n, (off, nm, o1, o2))
            elif nm in GLOBALES:
                # el offset puede desplazarse (los modulos declaran sus propias
                # globales antes), pero tiene que seguir apuntando a la MISMA
                # variable: eso es lo que se compara
                g1, g2 = gnombre(ra, a, o1), gnombre(rb, b, o2)
                if g1 != g2:
                    glob_mal[n] = glob_mal.get(n, 0) + 1
                    ejg.setdefault(n, (off, g1, g2))

    print("\noperandos de acceso a variable (PRIVATE/GET_PRIV/LOCAL/ARRAY/INDEX)")
    print("   procesos con algun operando distinto: %d" % len(vars_mal))
    for n in sorted(vars_mal, key=lambda k: -vars_mal[k])[:10]:
        off, nm, o1, o2 = ej[n]
        print("      %-24s %d mal, p.ej. offset %d  %s %s -> %s"
              % (n, vars_mal[n], off, nm, o1, o2))
    print("destinos de salto")
    print("   procesos con algun salto distinto: %d" % len(saltos_mal))
    for n in sorted(saltos_mal, key=lambda k: -saltos_mal[k])[:10]:
        off, nm, o1, o2 = ej[n]
        print("      %-24s %d mal, p.ej. offset %d  %s %s -> %s"
              % (n, saltos_mal[n], off, nm, o1, o2))
    print("accesos a globales (el offset puede moverse; la variable no)")
    print("   procesos que apuntan a otra global: %d" % len(glob_mal))
    for n in sorted(glob_mal, key=lambda k: -glob_mal[k])[:10]:
        off, g1, g2 = ejg[n]
        print("      %-24s %d mal, p.ej. offset %d  %s -> %s"
              % (n, glob_mal[n], off, g1, g2))
    return len(vars_mal), len(saltos_mal), len(glob_mal)


# Procesos cuyo flujo difiere A PROPOSITO, con el motivo. Cualquier otro que
# difiera es un fallo: la gracia de tener la lista es que el informe pueda decir
# "0 diferencias inesperadas" y no haya ruido de fondo permanente que acabe
# haciendo que dejes de leerlo.
ESPERADAS = {
    "MAIN": "lleva el prologo bgd1_init_globals() que inyecta el descompilador",
}


def comparar_codigo(a, b, A, B):
    """Compara instruccion a instruccion y localiza la primera divergencia."""
    tot = ok = 0
    difs = []
    ratios = []
    for n, p in A.items():
        q = B.get(n)
        if not q:
            continue
        x, y = opcodes(a, p), opcodes(b, q)
        tot += 1
        if x == y:
            ok += 1
            ratios.append(1.0)
            continue
        # el prologo que inyectamos (inicializadores, bgd1_init_globals) desplaza
        # el flujo entero, asi que la igualdad estricta no sirve de medida: lo
        # que importa es cuanto del cuerpo coincide.
        ratios.append(difflib.SequenceMatcher(None, x, y).ratio())
        j = next((k for k in range(min(len(x), len(y))) if x[k] != y[k]),
                 min(len(x), len(y)))
        difs.append((abs(len(x) - len(y)), n, j, x[j:j + 4], y[j:j + 4]))
    print("\nprocesos con flujo de opcodes IDENTICO: %d de %d (%.1f%%)"
          % (ok, tot, 100.0 * ok / tot if tot else 0))
    if ratios:
        med = sum(ratios) / len(ratios)
        print("parecido medio del flujo de opcodes: %.3f" % med)
        print("procesos por debajo de 0.90: %d" % sum(1 for x in ratios if x < 0.90))
    difs.sort(reverse=True)
    inesperadas = [d for d in difs if d[1] not in ESPERADAS]
    for _, n, j, x, y in difs[:10]:
        motivo = ESPERADAS.get(n)
        print("   %-24s primera diferencia en la instruccion %d%s"
              % (n, j, "   (esperada: %s)" % motivo if motivo else ""))
        print("        original:    %s" % " ".join(x))
        print("        recompilado: %s" % " ".join(y))
    print("   diferencias de flujo INESPERADAS: %d" % len(inesperadas))
    return ok, tot, len(inesperadas)


# Cadenas que pueden diferir sin que sea un fallo: bgdc mete la fecha y la hora
# de compilacion en la tabla.
FECHA = re.compile(r"^\s*(\d{1,4}[/-]\w{1,9}[/-]\d{1,4}|\d{1,2}:\d{2}(:\d{2})?)\s*$")


def cadenas_vivas(d):
    """Indices de cadena que el .dcb usa de verdad.

    Son las que empuja el bytecode, las que estan como valor inicial de una
    variable STRING, los nombres de los modulos importados y los de los ficheros
    fuente. El resto son literales huerfanos del binario original.
    """
    import decompile
    dc = decompile.Decompiler(d)
    vivas = set()
    for p in d.procs:
        code, i = p.code, 0
        while i < len(code):
            nm, hasp, ty, uns = dc.mndecode(code[i] & 0xFFFF)
            arg = code[i + 1] if hasp and i + 1 < len(code) else None
            # SOLO el PUSH lleva un INDICE de cadena. En los demas opcodes con
            # tipo de cadena (PRIVATE.string, GET_PRIV.string...) el operando es
            # un OFFSET de variable, y tomarlo por un indice daba por "vivas"
            # cadenas que no lo estaban (justo las de offset multiplo de 4).
            if nm == "PUSH" and arg is not None and ty == dc.T_STRING:
                vivas.add(arg)
            i += 2 if hasp else 1
    r = Resolver(d)

    def del_imagen(varlist, img):
        for _nom, t, off in r.iter_scalars(varlist):
            if t != d.T_STRING:
                continue
            trozo = img[off:off + r.TS.get(t, r.W)]
            if len(trozo) == r.TS.get(t, r.W):
                vivas.add(int.from_bytes(trozo, "little"))

    base = d.base + d.h["OGlobal"]
    del_imagen(d.glovar, bytes(d.raw[base:base + d.h["SGlobal"]]))
    for p in d.procs:
        if p.pridata:
            del_imagen(p.privar, bytes(p.pridata))
        if p.pubdata:
            del_imagen(p.pubvar, bytes(p.pubdata))
    if d.h.get("NImports"):
        vivas |= set(d.w(d.h["OImports"], d.h["NImports"]))
    vivas |= {0, 1, 2}          # nombre del programa, fecha y hora de compilacion
    return vivas


def comparar_datos(a, b, A, B, prg):
    """Cadenas y valores iniciales: nada de esto se ve en el flujo de opcodes.

    El .dcb guarda el contenido inicial de globales y privadas. Si se pierde uno,
    el juego arranca con esa tabla o bandera a cero y el fallo aparece a media
    partida, sin que ninguna comparacion de codigo diga nada.

    Las globales son un caso aparte: el descompilador NO las emite como
    inicializadores de la declaracion sino como asignaciones en
    bgd1_init_globals(), que MAIN llama la primera. Asi que el valor correcto
    puede estar en la imagen de datos O en esa funcion; se acepta cualquiera de
    las dos, y solo se avisa si no esta en ninguna.
    """
    # ---- cadenas ----
    # Un .dcb puede llevar literales que NO usa: ni el bytecode los empuja ni
    # aparecen como valor inicial de ninguna variable. No hay fuente que los
    # regenere -- no hay donde escribirlos -- asi que contarlos como perdida es
    # una falsa alarma. Se listan aparte para que se vean, pero no fallan.
    vivas = cadenas_vivas(a)
    fa = [(i, x) for i, x in enumerate(a.strings) if x and not FECHA.match(x)]
    sb = set(b.strings)
    perdidas = [x for i, x in fa if x not in sb and i in vivas]
    muertas = [x for i, x in fa if x not in sb and i not in vivas]
    print("\ncadenas")
    print("   original %d, recompilado %d, del original que NO estan: %d"
          % (len(a.strings), len(b.strings), len(perdidas)))
    for x in perdidas[:8]:
        print("      %r" % x[:60])
    if muertas:
        print("   ademas %d que el original NO usa en ningun sitio (no hay"
              " fuente que las regenere):" % len(muertas))
        for x in muertas[:5]:
            print("      %r" % x[:60])

    # ---- valores iniciales de globales ----
    ra, rb = Resolver(a), Resolver(b)

    def imagen(d, off, size):
        base = d.base + d.h[off]
        return bytes(d.raw[base:base + d.h[size]])

    def valor(d, t, crudo):
        """Valor comparable. En una STRING lo que se guarda es el INDICE de la
        cadena, y ese indice cambia entre compilaciones: hay que comparar el
        texto, no el numero."""
        if t == d.T_STRING:
            return d.strings[crudo] if crudo < len(d.strings) else None
        return crudo

    da, db = imagen(a, "OGlobal", "SGlobal"), imagen(b, "OGlobal", "SGlobal")
    ea = {n: (t, o) for n, t, o in ra.iter_scalars(a.glovar)}
    eb = {n: (t, o) for n, t, o in rb.iter_scalars(b.glovar)}
    asignadas = set()
    try:
        src = io.open(prg, encoding="latin-1").read()
        m = re.search(r"^FUNCTION bgd1_init_globals.*?^END", src, re.S | re.M)
        if m:
            # el fuente sale con los identificadores en minuscula y la tabla del
            # .dcb los guarda en mayuscula: comparar sin distinguir
            asignadas = {x.upper() for x in
                         re.findall(r"^\s*([A-Za-z_]\w*)", m.group(0), re.M)}
    except OSError:
        pass
    perdidos = []
    for n in set(ea) & set(eb):
        (ta, oa), (tb_, ob) = ea[n], eb[n]
        ca = int.from_bytes(da[oa:oa + ra.TS.get(ta, a.W)], "little")
        cb = int.from_bytes(db[ob:ob + rb.TS.get(tb_, b.W)], "little")
        va, vb = valor(a, ta, ca), valor(b, tb_, cb)
        if va == vb or not ca:
            continue
        raiz = re.match(r"[A-Za-z_]\w*", n).group(0).upper()
        if raiz in asignadas or raiz[:-4] in asignadas:
            continue
        perdidos.append((n, va))
    print("valores iniciales de GLOBALES")
    print("   escalares comparados: %d, con el valor inicial perdido: %d"
          % (len(set(ea) & set(eb)), len(perdidos)))
    for n, v in sorted(perdidos)[:8]:
        print("      %-34s valor original = %d" % (n, v))

    # ---- valores iniciales de privadas ----
    malpriv, totpriv = [], 0
    for n, p in A.items():
        q = B.get(n)
        if not q or not p.pridata or not q.pridata:
            continue
        xa = {v: (t, o) for v, t, o in ra.iter_scalars(p.privar)}
        xb = {v: (t, o) for v, t, o in rb.iter_scalars(q.privar)}
        for v in set(xa) & set(xb):
            (ta, oa), (tb_, ob) = xa[v], xb[v]
            sa, sbz = ra.TS.get(ta, a.W), rb.TS.get(tb_, b.W)
            if oa + sa > len(p.pridata) or ob + sbz > len(q.pridata):
                continue
            totpriv += 1
            ca = int.from_bytes(p.pridata[oa:oa + sa], "little")
            cb = int.from_bytes(q.pridata[ob:ob + sbz], "little")
            va, vb = valor(a, ta, ca), valor(b, tb_, cb)
            if ca and va != vb:
                malpriv.append((n, v, va, vb))
    print("valores iniciales de PRIVADAS")
    print("   escalares comparados: %d, con el valor inicial distinto: %d"
          % (totpriv, len(malpriv)))
    for n, v, x, y in sorted(malpriv)[:8]:
        print("      %-20s %-24s %s -> %s" % (n, v, x, y))

    return len(perdidas), len(perdidos), len(malpriv)


def main():
    orig, bgdc = sys.argv[1], sys.argv[2]
    tmp = tempfile.mkdtemp(prefix="rt_")
    prg = os.path.join(tmp, "salida.prg")
    dcb = os.path.join(tmp, "salida.dcb")

    # el descompilador escribe el fuente el mismo, en su latin-1, con -o
    # --no-extraer y stdin cerrado: el verificador no quiere los assets, y sin
    # esto el descompilador heredaria el terminal y se quedaria esperando a la
    # pregunta de si extraerlos.
    r = subprocess.run([sys.executable, os.path.join(AQUI, "decompile.py"),
                        "-o", prg, "--no-extraer", orig],
                       stdin=subprocess.DEVNULL,
                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
    if r.returncode:
        print("EL DESCOMPILADOR FALLA:", (r.stderr or "").strip()[-300:])
        return 1
    print("descompilado:", r.stderr.strip().splitlines()[-1] if r.stderr else "ok")

    # El fxc de Fenix no tiene "-o": escribe el .dcb junto al fuente, con el
    # mismo nombre. Se detecta por el propio ejecutable.
    fenix = os.path.basename(bgdc).lower().startswith("fxc")
    # Compilar un .dcb de Fenix con el bgdc de BennuGD (o al reves) no da un
    # resultado comparable: los opcodes ni siquiera son los mismos.
    motor = DCB(orig).motor
    if (motor == "Fenix") != fenix:
        print("OJO: el .dcb es de %s y el compilador que se le pasa %s el fxc de"
              " Fenix" % (motor, "es" if fenix else "no es"))
    # -g hace que el fxc escriba la tabla de identificadores, sin la cual el
    # .dcb recompilado no tiene un solo nombre y no hay como emparejar los
    # procesos. Pero SOLO si el original tambien la trae: si no, el -g mete
    # marcadores de depuracion que corren todos los offsets y los destinos de
    # salto dejan de coincidir. Cuando el original no tiene nombres, se
    # emparejan por posicion mas abajo.
    # -g solo si el ORIGINAL ya lleva informacion de depuracion: mete
    # marcadores en el codigo que corren todos los offsets, y entonces los
    # destinos de salto dejan de coincidir aunque el programa sea el mismo.
    con_debug = any(p.d.get("NSentences") for p in DCB(orig).procs)
    orden = ([bgdc] + (["-g"] if con_debug else []) + [prg] if fenix
             else [bgdc, "-o", dcb, prg])
    # el banner de bgdc trae acentos en latin-1: decodificar en utf-8 revienta
    r = subprocess.run(orden, capture_output=True, text=True,
                       encoding="latin-1", cwd=tmp)
    errs = [l for l in (r.stdout + r.stderr).splitlines() if "error" in l.lower()]
    if errs:
        print("NO RECOMPILA:", errs[0])
        return 1

    a, b = DCB(orig), DCB(dcb)
    print("\n%-22s %12s %12s" % ("", "original", "recompilado"))
    for k in ("NProcs", "NLocVars", "NVarSpaces", "SLocal", "NImports",
              "NGloVars", "NStrings"):
        marca = "" if a.h[k] == b.h[k] else "  <-- difiere"
        print("%-22s %12d %12d%s" % (k, a.h[k], b.h[k], marca))

    # Sin distinguir mayusculas: cuando el .dcb no trae nombres, el
    # descompilador inventa "proc_388" y al recompilar vuelve como "PROC_388".
    # Y el cuerpo del programa (NParams == -1) es el MAIN del recompilado.
    # NParams == -1 lo lleva el cuerpo del programa, pero tambien los procesos
    # solo declarados (5 de los 586 de 5 Leaps): solo cuenta como MAIN si el
    # .dcb no trae nombres, que es cuando no hay ningun MAIN al que agarrarse.
    def clave(d):
        def f(p):
            if not d.h["NID"] and p.d["NParams"] in (0xFFFFFFFF, 0xFFFFFFFFFFFFFFFF):
                return "MAIN"
            return p.name.upper()
        return f
    A = {clave(a)(p): p for p in a.procs}
    B = {clave(b)(p): p for p in b.procs}
    if not set(A) & set(B) and len(a.procs):
        # Ningun nombre en comun: se emparejan por POSICION, quitando antes la
        # funcion que inyecta el descompilador, que en el original no existe.
        resto = [q for q in b.procs if not q.name.upper().endswith("_INIT_GLOBALS")]
        if len(resto) == len(a.procs):
            print("\n(el original no trae nombres: se emparejan por posicion)")
            A = {"#%d" % i: p for i, p in enumerate(a.procs)}
            B = {"#%d" % i: q for i, q in enumerate(resto)}
    faltan = sorted(set(A) - set(B))
    print("\nprocesos no recuperados: %d %s" % (len(faltan), faltan[:5]))

    peores = []
    for n, p in A.items():
        q = B.get(n)
        if q and p.d["SCode"] != q.d["SCode"]:
            peores.append((abs(q.d["SCode"] - p.d["SCode"]), n,
                           p.d["SCode"], q.d["SCode"]))
    iguales = len(A) - len(peores)
    print("procesos con tamaño de codigo identico: %d de %d" % (iguales, len(A)))
    peores.sort(reverse=True)
    for d, n, x, y in peores[:10]:
        print("   %-24s %8d -> %-8d (%+d)" % (n, x, y, y - x))

    ok, tot, inesp = comparar_codigo(a, b, A, B)
    vmal, smal, gmal = comparar_operandos(a, b, A, B)
    cmal, gimal, pimal = comparar_datos(a, b, A, B, prg)

    print("\n" + "=" * 62)
    if cmal or gimal or pimal:
        print("FALLA: %d cadena(s) perdida(s), %d valor(es) inicial(es) de global"
              " perdido(s), %d de privada distinto(s)" % (cmal, gimal, pimal))
        return 1
    if vmal or smal or gmal or inesp:
        print("FALLA: %d con operandos de variable mal, %d con saltos mal, %d"
              " apuntando a otra global, %d con el flujo distinto sin motivo"
              % (vmal, smal, gmal, inesp))
        return 1
    print("OK: mismos operandos de variable, mismos saltos y mismas globales")
    print("    (%d de %d procesos con el flujo de opcodes identico)" % (ok, tot))
    return 0


if __name__ == "__main__":
    sys.exit(main())
