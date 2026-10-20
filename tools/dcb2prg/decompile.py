#!/usr/bin/env python3
"""Descompilador .dcb -> fuente BennuGD2 (.prg)."""
import os, re, struct, sys, time
from dcb import DCB
from resolve import Resolver
import mn

try:
    from port_bgd1to2 import Porter, portabilidad
except ImportError:
    Porter = None
    portabilidad = None

PREC = {"||": 1, "&&": 2, "^^": 3, "|": 3, "^": 4, "&": 5,
        "==": 6, "!=": 6, "<": 7, ">": 7, "<=": 7, ">=": 7,
        "<<": 8, ">>": 8, "+": 9, "-": 9, "*": 10, "/": 10, "%": 10}

# Conversiones que estrechan, con la que las deshace y el cast que representan.
INVERSA = {"FLOAT2INT": "INT2FLOAT", "DOUBLE2INT": "INT2DOUBLE",
           "DOUBLE2FLOAT": "FLOAT2DOUBLE"}
CASTNOM = {"FLOAT2INT": "(INT)", "DOUBLE2INT": "(INT)", "DOUBLE2FLOAT": "(FLOAT)"}

NEG = {"==": "!=", "!=": "==", "<": ">=", ">": "<=", "<=": ">", ">=": "<"}


def sin_indice0(t):
    """"NOMBRE[0]" -> "NOMBRE".

    Un array de bytes usado como cadena llega a la pila como la direccion de su
    primer elemento; en el fuente se escribe el array entero.
    """
    return t[:-3] if t.endswith("[0]") else t


class E:
    __slots__ = ("t", "p", "addr", "space", "off", "raw", "scal", "vinfo")

    def __init__(self, t, p=99, addr=False, space=None, off=0, raw=None, scal=0,
                 vinfo=None):
        self.t, self.p, self.addr, self.space, self.off = t, p, addr, space, off
        # (chunks, members, nivel) del tipo de la variable: permite resolver los
        # campos de struct que el compilador emite como "ARRAY n; INDEX offset"
        self.vinfo = vinfo
        self.raw = raw  # literal entero sin tipar, reinterpretable como float/double
        self.scal = scal  # tamaño del escalar base, para pasar offset -> índice

    def par(self, need):
        return "(%s)" % self.t if self.p < need else self.t

    def astype(self, ty, t_float=0x5000, t_double=0x6000):
        """Reinterpreta un literal crudo según el tipo que exige la operación."""
        if self.raw is None:
            return self
        if t_double is not None and ty == t_double:
            return E(dbl(self.raw))
        if ty == t_float:
            return E(flt(self.raw))
        return self


def real(v):
    """Literal real que SIGA siendo real al recompilar.

    "%g" de 1.0 da "1", que en el fuente es un literal ENTERO: se perdia el tipo
    y con el la conversion que el compilador emitia ("a = (double) 1" volvia a
    compilar como "a = 1", sin el DOUBLE2INT). Se le pone ".0" cuando hace falta.
    """
    t = "%g" % v
    return t if any(c in t for c in ".eEnif") else t + ".0"


def dbl(bits):
    v = struct.unpack("<d", struct.pack("<Q", bits & 0xFFFFFFFFFFFFFFFF))[0]
    return real(v)


def flt(bits):
    return real(struct.unpack("<f", struct.pack("<I", bits & 0xFFFFFFFF))[0])


# Marca de "aqui no supe reconstruir la expresion". Distintiva a proposito: hay
# que poder contarla en el resultado. Antes era un "?" suelto, que se confundia
# con el del operador ternario y no se contaba en ningun sitio: el descompilador
# decia "0 instrucciones sin traducir" mientras emitia codigo que ni compilaba.
FALTA = "<?>"

WORDBITS = 64  # lo fija DCB al cargar; 32 para los .dcb de BennuGD1


class _VarFenix:
    """Variable sintetica: el .dcb de Fenix sin depuracion no trae ninguna."""
    __slots__ = ("chunks", "members", "id", "offset", "varspace", "name")


def sgn(v, bits=None):
    b = bits or WORDBITS
    return v - (1 << b) if v >= (1 << (b - 1)) else v


def pop(st):
    """Desapila tolerando pilas vacías: preferimos marcar '?' antes que abortar."""
    return st.pop() if st else E(FALTA)


def negate(e):
    """Niega una condición de forma legible."""
    for op in ("==", "!=", "<=", ">=", "<", ">"):
        pre = " %s " % op
        if e.p == PREC[op] and pre in e.t:
            l, _, r = e.t.partition(pre)
            return E("%s %s %s" % (l, NEG[op], r), e.p)
    return E("NOT " + e.par(11), 11)


class Decompiler:
    def __init__(self, d):
        self.d = d
        self.r = Resolver(d)
        self.sysnames = self._sysprocs()
        self.warn = 0
        self.budget = 0
        self.porter = None      # si se activa, traduce símbolos a BennuGD2
        self.unresolved_vars = {}   # offsets que no caen en ninguna variable
        self.dims_obs, self.dims_uso = {}, {}
        self.priv_sin_nombre = {}
        # variables que llevan el ID de otro proceso: hay que declararlas con
        # el nombre de ese proceso para poder leer sus PUBLIC (ver
        # scan_remote_public)
        self.rem_tipo, self.pub_bloque = {}, {}
        # La funcion que repone los valores iniciales de las globales no existia
        # en el original: se marca con el motor del que viene el .dcb, no con
        # "bgd1_" a secas, que en un fichero de Fenix no significa nada.
        self.nombre_init = {"Fenix": "fenix_init_globals",
                            "BennuGD1": "bgd1_init_globals",
                            "BennuGD2": "bgd2_init_globals"}[d.motor]
        self.sin_cuerpo = []
        self.decl_space, self.decl_owner = None, -1
        self.fin_proc = -1
        self.pidx = {id(p): i for i, p in enumerate(d.procs)}
        # BennuGD1 permitía una variable con el mismo nombre que un proceso;
        # BennuGD2 lo rechaza, así que hay que renombrar esas variables.
        self.procnames = {p.name.upper() for p in d.procs}
        self.renamed_vars = set()
        self.needs_init = False
        global WORDBITS
        WORDBITS = d.W * 8
        # Fenix tiene sus propios codigos: BennuGD metio JTFALSE/JTTRUE en
        # 0x1E-0x1F y corrio 39 opcodes de sitio.
        self.fenix = getattr(d, "fenix", False)
        # Fenix 0.93 (DCB 0x0600) ya numera los opcodes como BennuGD1: metio
        # JTFALSE/JTTRUE en 0x1E-0x1F igual que el fork. Su tabla es la de
        # BennuGD1 salvo un renombrado (CHRSTR = STRI2CHR), asi que se reusa.
        self.mndecode = (mn.decode_v0 if self.fenix and d.version < 0x0600 else
                         mn.decode_v1 if d.W == 4 else mn.decode)
        if self.fenix:
            self.vars_fenix()
        # BennuGD1 emite AND/OR explícitos y usa JTFALSE sólo como salto de
        # cortocircuito, en vez de construir && / || con el propio salto.
        self.v1 = (d.W == 4)
        self.trunc_locals = set()
        # BennuGD1 no empuja literales float como bits crudos: usa opcodes de
        # conversion explicitos (INT2FLOAT). Reinterpretar el operando de un
        # PUSH sin tipo convertia enteros como 10000 en denormales (1.4013e-41).
        # BennuGD2 si lo hace, asi que alli se mantiene.
        # Un literal float se empuja como PUSH SIN TIPO y es la OPERACION la que
        # lleva el sufijo .float, asi que hay que reinterpretar los bits. Pero un
        # literal entero usado en contexto float lleva un INT2FLOAT explicito
        # delante: en ese caso NO se reinterpreta (reinterpretarlo convertia 10000
        # en el denormal 1.4013e-41). Medido sobre el juego de prueba: 110 PUSH
        # crudos consumidos por operaciones .float frente a 19 con INT2FLOAT.
        self.reinterpretar_literales = True
        # los flags de tipo del opcode viven en 0xF00 (v1) o 0xF000 (v2)
        if self.v1:
            self.T_STRING, self.T_FLOAT, self.T_DOUBLE = (
                mn.T_STRING_V1, mn.T_FLOAT_V1, mn.T_DOUBLE_V1)
        else:
            self.T_STRING, self.T_FLOAT, self.T_DOUBLE = mn.T_STRING, mn.T_FLOAT, mn.T_DOUBLE
        # Nombres con ambito de bloque: hay que arreglarlos antes de nada
        for vl in [d.glovar, d.locvar] + list(d.varspaces):
            self.desmangle(vl)
        for pr in d.procs:
            self.desmangle(pr.privar)
            self.desmangle(pr.pubvar)
        self.silent_jumps = set()   # saltos de cierre de bucle: no emitir nada
        self.tipos_nombrados = {}   # varspace -> nombre de TYPE a declarar
        self.pila_vs = set()        # varspaces en curso: corta la recursion
        self.planos = []            # procesos que hubo que emitir con goto

    def aplica_dims(self):
        """Da forma de ARRAY a las variables sinteticas, con lo deducido.

        Hace falta una pasada previa: las dimensiones salen de los pasos de los
        ARRAY del bytecode, y hasta que no se conocen, un indice CONSTANTE (que
        el compilador pliega dentro del offset) no se puede separar de la base.
        Sin esto salia "tabla[n][300][j]" en un array declarado [21].
        """
        self.calcula_dims()
        porof = {}
        for k, txt in self.unresolved_vars.items():
            m = re.search(r"offset (\d+) del espacio GLOBAL", txt)
            if m:
                porof[int(m.group(1))] = self.dims_uso.get(k, "")
        # Las variables del PROGRAMA no estan en ninguna tabla: hay que crearlas.
        # Con su forma puesta, el segundo pase ya sabe separar un indice
        # constante (que el compilador plegó dentro del offset) de la base.
        hay = {v.offset for v in self.d.glovar}
        cambiadas = 0
        for off, suf in sorted(porof.items()):
            dims = [int(x) + 1 for x in re.findall(r"\[(\d+)\]", suf)]
            v = next((x for x in self.d.glovar if x.offset == off), None)
            if v is None:
                if off in hay:
                    continue
                v = _VarFenix()
                v.members, v.id, v.offset, v.varspace = 0xFFFFFFFF, 0, off, 0
                v.name = "UNRESOLVED_GLOBAL_%d" % off
                self.d.glovar.append(v)
            v.chunks = ([(self.r.TYPE_ARRAY, c) for c in dims] + [(1, 0)])
            v.chunks = (v.chunks + [(0, 0)] * self.d.chunks)[:self.d.chunks]
            cambiadas += 1
        self.d.glovar.sort(key=lambda x: x.offset)
        return cambiadas

    def tabla_fenix(self, base):
        """La tabla de Fenix que le toca a este .dcb, por version.

        Las tablas CAMBIAN entre versiones: la 0.93 metio funciones y locales
        por medio y corrio todo lo de detras (20 codigos de sysproc y 25 offsets
        de local distintos frente a la 0.86g). Se coge la mas antigua que sea
        >= la del fichero: hacia atras las tablas solo pierden cosas del final,
        asi que esa es la que menos se equivoca.
        """
        import glob, json
        aqui = os.path.dirname(os.path.abspath(__file__))
        hay = []
        for f in glob.glob(os.path.join(aqui, base + "_*.json")):
            m = re.search(r"_([0-9A-Fa-f]{4})\.json$", f)
            if m:
                hay.append((int(m.group(1), 16), f))
        if not hay:
            suelto = os.path.join(aqui, base + ".json")
            return json.load(open(suelto)) if os.path.exists(suelto) else {}
        hay.sort()
        elegida = next((f for v, f in hay if v >= self.d.version), hay[-1][1])
        return json.load(open(elegida))

    def usa_del_motor(self):
        """Que le pide el programa al motor: funciones, globales y locales.

        Es lo que hay que comprobar contra el motor de destino para saber si el
        portado es posible.
        """
        fn = {}
        for p in self.d.procs:
            code, i = p.code, 0
            while i < len(code):
                nm, hasp, ty, uns = self.mndecode(code[i] & 0xFFFF)
                arg = code[i + 1] if hasp and i + 1 < len(code) else None
                if nm in ("SYSCALL", "SYSPROC") and arg in self.sysnames:
                    _n, _np, _ex, _pt = self.sysnames[arg]
                    fn[_n] = _np - _ex      # los '+' no se escriben en el fuente
                i += 2 if hasp else 1
        # De las variables del motor, SOLO las que el programa toca de verdad:
        # listar las 400 que declara Fenix no dice nada de si el portado es
        # posible. Se sacan de los operandos, resolviendo offset -> nombre.
        motor = {n.split(".")[0].split("[")[0]
                 for n in getattr(self, "motor_fenix", set())}
        gl, lo = set(), set()
        for p in self.d.procs:
            code, i = p.code, 0
            while i < len(code):
                nm, hasp, ty, uns = self.mndecode(code[i] & 0xFFFF)
                arg = code[i + 1] if hasp and i + 1 < len(code) else None
                if arg is not None:
                    if nm in ("GLOBAL", "GET_GLOBAL"):
                        n = self.r.name_at(self.d.glovar, arg)
                        if n:
                            gl.add(n.split(".")[0].split("[")[0])
                    elif nm in ("LOCAL", "GET_LOCAL", "REMOTE", "GET_REMOTE"):
                        n = self.r.name_at(self.d.locvar, arg)
                        if n:
                            lo.add(n.split(".")[0].split("[")[0])
                i += 2 if hasp else 1
        return fn, gl & motor, lo & motor

    def tope_motor_fenix(self, tab):
        """Hasta que offset vale la tabla de globales del motor en ESTE .dcb.

        Entre versiones de Fenix se fueron añadiendo variables al final, asi que
        la tabla de la 0x0500 (la unica cuyo fuente hay) es mas larga que la que
        uso un juego viejo. Aplicarla entera pondria nombres de motor
        ("cdinfo.track[4].minutes") encima de las variables del programa.

        Se mide: el compilador deja en la imagen de datos el valor inicial de
        cada variable del motor, y esos valores se conocen. Se recorren en orden
        y se corta en el primero que no cuadra.
        """
        base = self.d.base + self.d.h["OGlobal"]
        img = bytes(self.d.raw[base:base + self.d.h["SGlobal"]])
        tope = 0
        for k in sorted(tab.get("globals_ini", {}), key=int):
            off, esperado = int(k), tab["globals_ini"][k]
            if off + 4 > len(img):
                break
            if struct.unpack_from("<i", img, off)[0] != esperado:
                break
            # solo un valor NO NULO prueba de verdad que la tabla sigue
            # valiendo ahi: un cero cuadra con cualquier hueco a cero. Cortando
            # en el ultimo ancla firme se pierde algun nombre del motor, pero no
            # se le pone nunca a una variable del programa.
            if esperado:
                tope = off + 4
        return tope or None

    def anota_dim(self, texto, paso):
        """Apunta el PASO de un ARRAY sobre una variable que no tiene nombre.

        En un .dcb sin tabla de variables no hay declaraciones, asi que las
        dimensiones hay que deducirlas del bytecode: el operando de ARRAY es el
        paso en bytes de la dimension que se indexa, y el cociente entre dos
        pasos consecutivos es el numero de elementos de la de dentro.
        """
        # el texto ya viene en minusculas: la comparacion va sin distinguir caja
        m = re.match(r"(unresolved_\w+)", (texto or "").lower())
        if not m or not paso:
            return
        self.dims_obs.setdefault(m.group(1).upper(), {})[texto.count("[")] = sgn(paso)

    def calcula_dims(self):
        """De los pasos observados a un sufijo de declaracion "[a][b][c]".

        La dimension de MAS AFUERA no se puede sacar de los pasos: se estima con
        el hueco hasta la siguiente variable usada, y por eso va marcada con un
        comentario. Las de dentro son exactas.
        """
        self.dims_uso = {}
        usados = {}
        # PRIVATE y PUBLIC son de CADA proceso: si se mezclan los offsets de
        # todos en la misma lista, el hueco hasta "la siguiente variable" sale
        # de otro proceso y las dimensiones no valen nada. El indice del proceso
        # viene en el propio nombre.
        for k, txt in self.unresolved_vars.items():
            m = re.search(r"offset (\d+) del espacio (\w+)", txt)
            if not m:
                continue
            esp = m.group(2)
            mp = re.match(r"UNRESOLVED_(?:PRIVATE|PUBLIC)_(\d+)_", k)
            clave = (esp, int(mp.group(1))) if mp else (esp, -1)
            usados.setdefault(clave, []).append((int(m.group(1)), k))
        for esp in usados:
            usados[esp].sort()

        def tope(clave):
            esp, pi = clave
            if esp == "GLOBAL":
                return self.d.h["SGlobal"]
            if esp == "LOCAL":
                return self.d.h["SLocal"]
            if 0 <= pi < len(self.d.procs):
                return self.d.procs[pi].d["S" + esp.capitalize()[:3] + ("vate" if esp == "PRIVATE" else "lic")]
            return None
        for esp, lista in usados.items():
            for i, (off, k) in enumerate(lista):
                pasos = self.dims_obs.get(k)
                if not pasos:
                    continue
                s = [pasos[n] for n in sorted(pasos)]
                if len(s) < 1 or s[-1] <= 0:
                    continue
                dentro = [s[j] // s[j + 1] for j in range(len(s) - 1)]
                sig = lista[i + 1][0] if i + 1 < len(lista) else tope(esp)
                fuera = ((sig - off) // s[0]) if sig and sig > off else None
                if fuera is not None and fuera <= 1:
                    # Se indexa pero el hueco hasta la siguiente variable "usada"
                    # da un solo elemento: es que esas de detras SON elementos de
                    # este array, alcanzados con indice constante (el compilador
                    # los pliega dentro del offset y no hay forma de
                    # distinguirlos de una variable suelta). Se absorben las que
                    # caen justo en el paso y no se indexan a su vez.
                    j, fin = i + 1, off + s[0]
                    while j < len(lista):
                        o2, k2 = lista[j]
                        if o2 != fin or self.dims_obs.get(k2):
                            break
                        fin += s[0]
                        j += 1
                    fuera = (fin - off) // s[0]
                cuentas = ([fuera] if fuera and fuera > 1 else []) + dentro
                if not cuentas:
                    continue
                self.dims_uso[k] = "".join("[%d]" % (c - 1) for c in cuentas if c > 1)

    def vars_fenix(self):
        """Pone nombre a las variables del MOTOR en un .dcb de Fenix.

        Compilado sin depuracion, un .dcb de Fenix no lleva tabla de
        identificadores ni de variables (NID, NGloVars y NLocVars a 0): cada
        acceso sale como "global@70580". Pero las del motor (x, y, graph, mouse,
        joy...) estan siempre en el mismo offset, y sus declaraciones vienen en
        las fuentes de Fenix. Se sintetizan aqui para que el resolver las nombre
        como si el .dcb las trajera. Las del PROGRAMA no hay forma de
        recuperarlas: ese nombre no se guardo en ningun sitio.
        """
        tab = self.tabla_fenix("vars_fenix")
        if not tab:
            return

        def construir(dic, tope):
            out = []
            for off, nom in sorted(((int(k), v) for k, v in dic.items())):
                if tope and off >= tope:
                    break               # de ahi en adelante son del programa
                v = type(self.d.glovar[0])() if self.d.glovar else _VarFenix()
                v.chunks = [(1, 0)] + [(0, 0)] * (self.d.chunks - 1)
                v.members, v.id, v.offset, v.varspace = 0xFFFFFFFF, 0, off, 0
                v.name = nom.upper()
                out.append(v)
            return out

        # Los PARAMETROS son las primeras privadas. Sin tabla de variables no
        # tienen nombre, y el de la firma ("param0") tenia que coincidir con el
        # que usa el cuerpo: si no, el proceso declara un parametro y usa otra
        # variable. En Fenix todo escalar ocupa 4 bytes.
        for pr in self.d.procs:
            if pr.privar or not pr.d["SPrivate"]:
                continue
            n = self.nparams(pr)
            for k in range(n):
                v = _VarFenix()
                v.chunks = [(1, 0)] + [(0, 0)] * (self.d.chunks - 1)
                v.members, v.id, v.offset, v.varspace = 0xFFFFFFFF, 0, k * 4, 0
                v.name = "PARAM%d" % k
                pr.privar.append(v)

        # La lista de nombres del motor se usa SIEMPRE, tanto si hay que
        # sintetizar las variables (compilado sin depuracion) como si el .dcb ya
        # las trae: en los dos casos las declara el interprete y el programa no
        # debe repetirlas ni reponerles su valor inicial.
        self.motor_fenix = {n.upper() for n in tab.get("locals", {}).values()}
        self.motor_fenix |= {n.upper() for n in tab.get("globals", {}).values()}
        self.motor_fenix |= {n.split(".")[0].split("[")[0]
                             for n in list(self.motor_fenix)}
        if not self.d.locvar:
            self.d.locvar = construir(tab.get("locals", {}), self.d.h["SLocal"])
        if not self.d.glovar:
            self.d.glovar = construir(tab.get("globals", {}),
                                      self.tope_motor_fenix(tab))

    def _sysprocs(self):
        # Fenix no lleva la tabla en el .dcb: el operando del SYSCALL es un
        # codigo fijo del interprete. Se saca de sus fuentes con
        # build_sysprocs_fenix.py.
        if getattr(self.d, "fenix", False):
            return {int(k): (v[0], v[1], v[2].count("+"), v[2])
                    for k, v in self.tabla_fenix("sysprocs_fenix").items()}
        out = {}
        W = self.d.W
        o = self.d.h["OSysProcsCodes"]
        for _ in range(self.d.h["NSysProcsCodes"]):
            sid, = self.d.w(o)                       # Id
            params, code = self.d.w(o + W + 4, 2)    # Type es uint32 en ambas revisiones
            o += 3 * W + 4
            ptypes = bytes(self.d.raw[self.d.base + o:self.d.base + o + params])
            o += params
            # Cada '+' en los ParamTypes es un argumento que GENERA el compilador
            # (p.ej. save()/load() reciben además dirección y tamaño). En el
            # fuente no se escriben, así que hay que descontarlos al emitir.
            extra = ptypes.count(b"+")
            out[code] = (self.d.idname.get(sid, "sys_%d" % sid), params, extra,
                         ptypes.decode("latin-1"))
        return out

    def index(self, ins):
        """Precalcula sucesor/predecesor de cada offset (compartido por toda la recursión)."""
        keys = sorted(ins)
        self.nxt = {k: (keys[j + 1] if j + 1 < len(keys) else None) for j, k in enumerate(keys)}
        self.prv = {k: (keys[j - 1] if j else None) for j, k in enumerate(keys)}

    # Opcodes cuyo operando es un OFFSET dentro del propio proceso.
    SALTAN = {"JUMP", "JFALSE", "JTRUE", "JTFALSE", "JNFALSE", "JNTRUE",
              "JFTRUE", "JNOCASE", "CLONE", "EXITHNDLR", "ERRHNDLR", "NCALL"}

    # Marcas de depuracion: no ejecutan nada, solo dicen en que linea del fuente
    # estabamos. Las mete bgdc con -g.
    MARCAS = {"SENTENCE", "DEBUG"}

    def decode(self, proc):
        code, ins, i = proc.code, {}, 0
        while i < len(code):
            c = code[i] & 0xFFFF
            name, hasp, ty, uns = self.mndecode(c)
            arg = code[i + 1] if hasp and i + 1 < len(code) else None
            ins[i] = (name, ty, uns, arg, 2 if hasp else 1)
            i += 2 if hasp else 1

        # Un .dcb compilado con -g lleva un SENTENCE intercalado ANTES de casi
        # cada sentencia. Los detectores de bucle miran la instruccion contigua
        # ("justo antes del destino hay un JUMP atras"), y con las marcas de por
        # medio dejan de reconocer nada: salian 0 FOR y 28 "bucle no
        # estructurado". Se quitan aqui y se reescriben los saltos que apuntaban
        # a una marca, de modo que todo lo de abajo ve el mismo flujo que en un
        # .dcb sin depuracion.
        if any(v[0] in self.MARCAS for v in ins.values()):
            claves = sorted(ins)
            siguiente = {}
            destino = None
            for k in reversed(claves):
                if ins[k][0] in self.MARCAS:
                    siguiente[k] = destino
                else:
                    destino = k
                    siguiente[k] = k
            limpio = {}
            for k in claves:
                if ins[k][0] in self.MARCAS:
                    continue
                name, ty, uns, arg, sz = ins[k]
                if name in self.SALTAN and arg in siguiente:
                    arg = siguiente[arg]
                limpio[k] = (name, ty, uns, arg, sz)
            ins = limpio
        return ins

    @staticmethod
    def nparams(proc):
        """NParams saneado: el campo es uint64 y usa -1 como centinela."""
        n = sgn(proc.d["NParams"])
        return n if 0 <= n <= 64 else 0

    def safe_name(self, n):
        """Evita que una variable choque con el nombre de un proceso.

        Actúa sobre el identificador base, ignorando subíndices y miembros:
        "CONGELAR[3]" debe renombrarse igual que "CONGELAR".
        """
        m = re.match(r'([A-Za-z_]\w*)(.*)$', n, re.S)
        if not m:
            return n
        base, rest = m.group(1), m.group(2)
        if base.upper() in self.procnames:
            self.renamed_vars.add(base)
            return base + "_VAR" + rest
        return n

    @staticmethod
    def desmangle(vl):
        """Quita el "#" de los nombres con ambito de bloque.

        BennuGD deja declarar variables dentro de un BEGIN/END anidado, y en el
        .dcb las guarda como "A#1" (el numero es el nivel de bloque) para que no
        choquen con la "A" de fuera. Cada una tiene su PROPIO hueco de memoria
        -- en el ejemplo, offsets 0, 8 y 16 -- asi que son variables distintas de
        verdad y emitirlas planas con nombres distintos es equivalente.

        Hay que hacerlo si o si: en el fuente "#" arranca una directiva de
        preprocesador, y "a#1 = 2;" da "Unknown preprocessor directive". El
        ambito visual del bloque se pierde, pero el .dcb no guarda donde
        empezaba ni acababa.
        """
        usados = {v.name.upper() for v in vl if "#" not in v.name}
        for v in vl:
            if "#" not in v.name:
                continue
            base = v.name.replace("#", "_")
            nuevo, n = base, 0
            while nuevo.upper() in usados:
                n += 1
                nuevo = "%s_%d" % (base, n)
            usados.add(nuevo.upper())
            v.name = nuevo

    @staticmethod
    def minus(n):
        """Identificadores en minuscula.

        El .dcb guarda TODO en mayusculas: BennuGD normaliza asi al compilar, y
        el nombre tal cual lo escribio el programador no se conserva. Como el
        lenguaje no distingue mayusculas de minusculas, emitirlos en minuscula
        es puramente cosmetico y se parece mucho mas a como se escribe en
        realidad. Las palabras clave y los tipos (INT, BYTE...) siguen en
        mayuscula, que es la convencion habitual.
        """
        return n.lower() if n else n

    def vname(self, space, off, proc):
        vl = {"GLOBAL": self.d.glovar, "LOCAL": self.d.locvar,
              "PRIVATE": proc.privar, "PUBLIC": proc.pubvar}[space]
        n = self.r.name_at(vl, off)
        if n is None:
            # offset que no cae dentro de ninguna variable declarada: se emite
            # un identificador válido y declarable para no romper la compilación
            # PRIVATE y PUBLIC son de cada proceso: el offset 0 de uno no es
            # el mismo que el de otro, asi que el nombre lleva el proceso.
            if space in ("PRIVATE", "PUBLIC"):
                n = "UNRESOLVED_%s_%d_%d" % (space, self.pidx.get(id(proc), 0), off)
                self.priv_sin_nombre.setdefault(id(proc), {})[n] = off
            else:
                n = "UNRESOLVED_%s_%d" % (space, off)
            self.unresolved_vars[n] = "offset %d del espacio %s" % (off, space)
        elif space in ("PRIVATE", "PUBLIC", "GLOBAL"):
            n = self.safe_name(n)
        if self.porter:
            head, _, rest = n.partition(".")
            if space == "GLOBAL":
                head = self.porter.map_global(head)
            elif space == "LOCAL":
                head = self.porter.map_local(head)
            n = head + ("." + rest if rest else "")
        return self.minus(n)

    # ================= núcleo: emisión estructurada =================
    def emit(self, proc, ins, lo, hi, ind, out):
        pad = "    " * ind
        st = []
        i = lo
        nxt, prv = self.nxt, self.prv
        stmt_start = lo
        seen = set()
        mark = {}          # offset -> índice en `out` donde empezó a emitirse

        while i is not None and i < hi:
            # cortafuegos: patrones de flujo no reconocidos podrían no avanzar
            self.budget -= 1
            if self.budget <= 0:
                out.append(pad + "// <descompilación abortada: flujo no reconocido>")
                self.warn += 1
                return hi
            if i in seen:
                out.append(pad + "// <salto ya visitado @%d>" % i)
                return hi
            seen.add(i)
            if i in self.etiquetas:
                out.append("    " * max(0, ind - 1) + "lbl_%d:" % i)
            mark.setdefault(i, len(out))
            # y a nivel de PROCESO: un REPEAT o un LOOP puede empezar fuera del
            # bloque que se esta emitiendo ahora, y con la marca local no habia
            # forma de envolverlo
            # Solo en Fenix: alli NO hay goto, asi que un bucle que empieza
            # fuera del bloque hay que envolverlo como sea. En BennuGD si lo
            # hay, y el modo plano con etiquetas es FIEL: forzar la estructura
            # se inventaba un if/else que no estaba (se vio en test1.prg).
            if self.fenix and out is getattr(self, "out_body", None):
                self.marca_proc.setdefault(i, len(out))
            name, ty, uns, arg, sz = ins[i]

            # JTFALSE/JTTRUE conservan el valor en pila: son && y || con cortocircuito
            if name in ("JTFALSE", "JTTRUE"):
                # Los dos motores usan el mismo idioma: JTFALSE deja el valor en
                # pila y el AND/OR posterior combina los dos operandos; el || añade
                # el atajo "POP; PUSH 1; JUMP". Saltarlo y dejar que el AND/OR haga
                # su trabajo da la expresion correcta. Antes esto solo se aplicaba
                # a BennuGD1 y en BennuGD2 salian cosas como "A && 1 || B" o
                # "A && (? && B)".
                i = self.skip_shortcircuit(ins, i, nxt)
                continue
                lhs = pop(st) if st else E(FALTA)
                op = "&&" if name == "JTFALSE" else "||"
                pr = PREC[op]
                j = nxt[i]
                if j is not None and ins[j][0] == "POP":
                    j = nxt[j]
                rhs = self.sub_expr(proc, ins, j, arg, out, pad)
                st.append(E("%s %s %s" % (lhs.par(pr), op, rhs.par(pr + 1)), pr))
                i = arg
                continue

            if name in ("JFALSE", "JTRUE"):
                cond = pop(st) if st else E(FALTA)
                if name == "JFALSE":
                    pass  # salta si es falso -> la condición se mantiene
                else:
                    cond = negate(cond)
                tgt = arg
                if tgt < i:
                    # Salto condicional hacia ATRAS: es el cierre de un
                    # REPEAT ... UNTIL. El cuerpo ya esta emitido, asi que se
                    # envuelve a posteriori, igual que se hace con el LOOP.
                    # Soltar el UNTIL a pelo dejaba una sentencia sin bucle que
                    # ni compila.
                    at = mark.get(tgt)
                    if at is None and out is getattr(self, "out_body", None):
                        at = self.marca_proc.get(tgt)
                    if at is not None:
                        cuerpo = [("    " + l) if l.strip() else l
                                  for l in out[at:]]
                        out[at:] = ([pad + "REPEAT"] + cuerpo
                                    + [pad + "UNTIL (%s);" % cond.t])
                    else:
                        out.append(pad + "// bucle no estructurado -> %d" % tgt)
                        out.append(pad + "UNTIL (%s);" % cond.t)
                        self.warn += 1
                    i = nxt[i]
                    stmt_start = i
                    continue
                # FOR: "cond; JFALSE fin; JUMP cuerpo; incremento; JUMP cond; cuerpo:"
                j = nxt[i]
                if j is not None and ins[j][0] == "JUMP":
                    body = ins[j][3]
                    pb = prv.get(body)
                    if (pb is not None and ins[pb][0] == "JUMP"
                            and ins[pb][3] == stmt_start and body > j):
                        incr = []
                        self.emit(proc, ins, nxt[j], pb, 0, incr)
                        # El init de un FOR es una EXPRESION-sentencia: acaba
                        # en "LET; PTR; POP". Una sentencia normal de antes del
                        # bucle acaba en LETNP, y tragarsela como init cambiaba
                        # el codigo ("lista[i] = 0;" pasaba a ser el init).
                        _p1 = prv.get(stmt_start)
                        _p2 = prv.get(_p1) if _p1 is not None else None
                        _p3 = prv.get(_p2) if _p2 is not None else None
                        _es_init = (_p1 is not None and ins[_p1][0] == "POP"
                                    and _p2 is not None and ins[_p2][0] == "PTR"
                                    and _p3 is not None
                                    and ins[_p3][0] in mn.ASSIGNOP
                                    and not ins[_p3][0].endswith("NP"))
                        init = ""
                        if _es_init and out and out[-1].strip().endswith(";"):
                            init = out.pop().strip().rstrip(";")
                        step = ", ".join(x.strip().rstrip(";") for x in incr if x.strip())
                        out.append(pad + "FOR (%s; %s; %s)" % (init, cond.t, step))
                        self.silent_jumps.add(nxt[j])
                        self.silent_jumps.add(stmt_start)
                        # el cierre natural del bucle es el salto que termina
                        # JUSTO en el final del cuerpo; los demas son CONTINUE
                        _fc = getattr(self, "fin_cuerpo", None)
                        self.fin_cuerpo = tgt
                        self.emit(proc, ins, body, tgt, ind + 1, out)
                        self.fin_cuerpo = _fc
                        out.append(pad + "END")
                        i = tgt
                        stmt_start = i
                        continue

                pt = prv.get(tgt)
                # WHILE / FROM: justo antes del destino hay un JUMP atrás a la condición
                if pt is not None and ins[pt][0] == "JUMP" and ins[pt][3] == stmt_start:
                    frm = self.match_from(ins, nxt[i], pt, tgt, nxt, prv, proc, cond, out, pad)
                    _fc = getattr(self, "fin_cuerpo", None)
                    if frm:
                        header, body_end = frm
                        out.append(pad + header)
                        self.fin_cuerpo = body_end
                        self.emit(proc, ins, nxt[i], body_end, ind + 1, out)
                    else:
                        out.append(pad + "WHILE (%s)" % cond.t)
                        self.fin_cuerpo = pt
                        self.emit(proc, ins, nxt[i], pt, ind + 1, out)
                    self.fin_cuerpo = _fc
                    out.append(pad + "END")
                    i = tgt
                    stmt_start = i
                    continue
                # TERNARIO "cond ? a : b"
                #
                # Tiene el MISMO esqueleto de saltos que un if/else, pero las dos
                # ramas dejan un VALOR en la pila en vez de ejecutar sentencias, y
                # el compilador cierra la rama falsa con un NOP. Ese NOP es la
                # marca: un if/else de verdad no lo lleva (comprobado compilando
                # los dos). Sin esto salian dos IF vacios y la expresion se perdia
                # entera: '? += ? + (LINE + "|")'.
                if pt is not None and ins[pt][0] == "JUMP" and ins[pt][3] > tgt:
                    _fin = ins[pt][3]
                    _ult = prv.get(_fin)
                    if _ult is not None and ins[_ult][0] == "NOP" and _ult >= tgt:
                        _a = self.sub_expr(proc, ins, nxt[i], pt, out, pad)
                        _b = self.sub_expr(proc, ins, tgt, _ult, out, pad)
                        st.append(E("%s ? %s : %s"
                                    % (cond.par(1), _a.par(1), _b.par(1)), 0))
                        i = _fin
                        continue

                # IF / ELSE
                #
                # Cuidado: "cond; JFALSE t; JUMP fuera; t: ..." dentro de un
                # bucle NO es un if/else, es un BREAK en la rama THEN. Se
                # distinguen porque el supuesto ELSE termina saltando hacia
                # atrás (el cierre del bucle). Sin esta comprobación el BREAK
                # se pierde y la rama sale vacía.
                _endif = ins[pt][3] if (pt is not None and ins[pt][0] == "JUMP") else None
                _cierra_bucle = False
                if _endif is not None:
                    _last = prv.get(_endif)
                    # El cierre del bucle puede ser un JUMP (LOOP) o un salto
                    # CONDICIONAL hacia atras (REPEAT ... UNTIL). Mirando solo
                    # el JUMP, el UNTIL de un repeat se colaba dentro del ELSE
                    # y salia un "ELSE UNTIL(...) END" que ni compila.
                    if (_last is not None
                            and ins[_last][0] in ("JUMP", "JFALSE", "JTRUE")
                            and ins[_last][3] < tgt):
                        _cierra_bucle = True
                # Ojo con el ">": si el fin del if coincide con el inicio del
                # else, el original llevaba un "ELSE" vacio. Reconstruirlo
                # (cambiando esto por ">=") sale MAS CARO de lo que arregla:
                # convierte en if/else un monton de if sueltos cuyo cuerpo
                # termina en un salto al mismo sitio, y el round-trip empeora.
                if (pt is not None and ins[pt][0] == "JUMP" and ins[pt][3] > tgt <= hi
                        and not _cierra_bucle):
                    endif = ins[pt][3]
                    out.append(pad + "IF (%s)" % cond.t)
                    self.emit(proc, ins, nxt[i], pt, ind + 1, out)
                    out.append(pad + "ELSE")
                    self.emit(proc, ins, tgt, endif, ind + 1, out)
                    out.append(pad + "END")
                    i = endif
                    stmt_start = i
                    continue
                out.append(pad + "IF (%s)" % cond.t)
                self.emit(proc, ins, nxt[i], tgt, ind + 1, out)
                out.append(pad + "END")
                i = tgt
                stmt_start = i
                continue

            if name == "JUMP":
                tgt = arg
                # REPEAT..UNTIL: "JUMP cuerpo; JUMP cond;" (el 2º es el destino de CONTINUE)
                j = nxt[i]
                if (j is not None and ins[j][0] == "JUMP" and tgt == nxt[j]
                        and ins[j][3] > tgt):
                    cond_start = ins[j][3]
                    out.append(pad + "REPEAT")
                    self.emit(proc, ins, nxt[j], cond_start, ind + 1, out)
                    cond, after = self.eval_cond(proc, ins, cond_start, hi, nxt, out, pad)
                    out.append(pad + "UNTIL (%s);" % cond)
                    i = after
                    stmt_start = i
                    continue
                if tgt <= i:  # cierre de un LOOP ... END
                    if tgt in self.silent_jumps:
                        # El cierre natural del bucle es la ULTIMA instruccion
                        # del cuerpo. Un salto al MISMO sitio desde enmedio es un
                        # CONTINUE que escribio el programador: silenciarlo
                        # tambien dejaba la rama vacia y se perdia la sentencia.
                        if nxt[i] != getattr(self, "fin_cuerpo", None):
                            out.append(pad + "CONTINUE;")
                        i = nxt[i]
                        stmt_start = i
                        continue
                    at = mark.get(tgt)
                    if at is None and out is getattr(self, "out_body", None):
                        at = self.marca_proc.get(tgt)
                    if at is not None:
                        # el cuerpo ya está emitido: se envuelve a posteriori
                        cuerpo = [("    " + l) if l.strip() else l
                                  for l in out[at:]]
                        out[at:] = [pad + "LOOP"] + cuerpo + [pad + "END"]
                    else:
                        out.append(pad + "// bucle no estructurado -> %d" % tgt)
                        self.warn += 1
                    i = nxt[i]
                    stmt_start = i
                    continue
                # salto adelante: BREAK/CONTINUE o fin de rama.
                # Saltar EXACTAMENTE al final del bloque es la salida natural
                # (el JUMP que el compilador pone al cerrar un IF): emitir ahi
                # un BREAK daba "Out of loop" cuando no habia bucle alrededor.
                if tgt > hi:
                    out.append(pad + "BREAK;")
                    i = nxt[i]
                    stmt_start = i
                    continue
                i = tgt
                stmt_start = i
                continue

            # CLONE: el padre salta al operando, el hijo ejecuta el bloque siguiente
            if name == "CLONE":
                out.append(pad + "CLONE")
                self.emit(proc, ins, nxt[i], arg, ind + 1, out)
                out.append(pad + "END")
                i = arg
                stmt_start = i
                continue

            if name == "SWITCH":
                sw = pop(st) if st else E(FALTA)
                # Un SWITCH puede ser el CUERPO de un bucle: el salto de cierre
                # cae dentro de un case (normalmente el default) y emit_switch no
                # sabe que hay un bucle, asi que soltaba "bucle no estructurado"
                # y el proceso entero se iba al modo plano. Aqui se mira ANTES de
                # entrar si algun salto atras del rango apunta al principio de
                # esta sentencia; si lo hay, el switch va envuelto en LOOP...END
                # y esos saltos se silencian (el ultimo cierra, los demas serian
                # CONTINUE).
                fin_sw = self.fin_switch(ins, stmt_start, nxt[i], hi)
                _fc = getattr(self, "fin_cuerpo", None)
                if fin_sw is not None:
                    out.append(pad + "LOOP")
                    ind += 1
                    pad = "    " * ind
                    self.silent_jumps.add(stmt_start)
                    self.fin_cuerpo = fin_sw
                out.append(pad + "SWITCH (%s)" % sw.t)
                i = self.emit_switch(proc, ins, nxt[i], hi, ind + 1, out, nxt, prv)
                out.append(pad + "END")
                if fin_sw is not None:
                    ind -= 1
                    pad = "    " * ind
                    out.append(pad + "END")
                    self.fin_cuerpo = _fc
                    # y se deja de silenciar ese destino: si no, un salto atras
                    # POSTERIOR al bucle emitia un CONTINUE fuera de bucle
                    # ("Out of loop" al recompilar)
                    self.silent_jumps.discard(stmt_start)
                    # se sigue donde acaba el SWITCH, que es lo que devuelve
                    # emit_switch: fin_sw solo marca el cierre del bucle y cae
                    # DENTRO del switch, asi que usarlo reemitia medio cuerpo
                stmt_start = i
                continue

            # El END que cierra el proceso es implicito: BennuGD ya lo pone.
            # Emitirlo como "RETURN;" solo se notaba cuando caia DENTRO de una
            # rama (el DEFAULT de un switch, p.ej.), donde el recorte de la cola
            # no llega: metia una sentencia que el original no tenia y
            # descolocaba todos los saltos siguientes.
            if name == "END" and i == self.fin_proc:
                return nxt[i]
            self.step(proc, name, ty, uns, arg, st, out, pad)
            i = nxt[i]
            if not st:
                stmt_start = i
        return i

    def match_from(self, ins, body_lo, jump_back, tgt, nxt, prv, proc, cond, out, pad):
        """Reconoce el epílogo de un FROM..TO: guarda 'var < to', incremento y salto atrás.

        Devuelve (cabecera, fin_del_cuerpo) o None si el bucle es un WHILE normal.
        """
        # el incremento son las 3 instrucciones previas al salto atrás
        p1 = prv.get(jump_back)
        if p1 is None or ins[p1][0] != "POP":
            return None
        p2 = prv.get(p1)
        if p2 is None or ins[p2][0] not in ("INC", "DEC"):
            return None
        p3 = prv.get(p2)
        if p3 is None or ins[p3][0] not in ("PRIVATE", "LOCAL", "GLOBAL", "PUBLIC"):
            return None
        var = self.vname(ins[p3][0], ins[p3][3], proc)
        step = ins[p2][3] or 1
        if ins[p2][0] == "DEC":
            step = -step
        # la guarda: GET var; PUSH to; cmp; JFALSE tgt
        g = prv.get(p3)
        if g is None or ins[g][0] != "JFALSE" or ins[g][3] != tgt:
            return None
        c3 = prv.get(g); c2 = prv.get(c3) if c3 else None; c1 = prv.get(c2) if c2 else None
        if c1 is None or ins[c1][0] not in ("GET_PRIV", "GET_LOCAL", "GET_GLOBAL", "GET_PUBLIC"):
            return None
        to = cond.t.split(" <= ")[-1].split(" >= ")[-1] if (" <= " in cond.t or " >= " in cond.t) else None
        if to is None:
            return None
        # el inicializador ya se emitió como sentencia suelta: lo absorbemos
        init = None
        if out and out[-1].strip().startswith(var + " = "):
            init = out.pop().strip()[len(var) + 3:].rstrip(";")
        head = "FROM %s = %s TO %s%s;" % (var, init if init is not None else FALTA, to,
                                          "" if step == 1 else " STEP %d" % step)
        return head, c1

    def skip_shortcircuit(self, ins, i, nxt):
        """v1: salta el JTFALSE (y el idiom 'POP; PUSH 1; JUMP' del ||)."""
        a = nxt[i]
        if a is not None and ins[a][0] == "POP":
            b = nxt[a]
            if b is not None and ins[b][0] == "PUSH" and ins[b][3] == 1:
                c = nxt[b]
                if c is not None and ins[c][0] == "JUMP":
                    return ins[i][3]          # continuar donde evalúa el 2º operando
        return a

    def sub_expr(self, proc, ins, lo, hi, out, pad):
        """Evalúa [lo,hi) como una única expresión y devuelve su valor."""
        nxt, st, i = self.nxt, [], lo
        while i is not None and i < hi:
            name, ty, uns, arg, sz = ins[i]
            if name in ("JTFALSE", "JTTRUE"):
                # Los dos motores usan el mismo idioma: JTFALSE deja el valor en
                # pila y el AND/OR posterior combina los dos operandos; el || añade
                # el atajo "POP; PUSH 1; JUMP". Saltarlo y dejar que el AND/OR haga
                # su trabajo da la expresion correcta. Antes esto solo se aplicaba
                # a BennuGD1 y en BennuGD2 salian cosas como "A && 1 || B" o
                # "A && (? && B)".
                i = self.skip_shortcircuit(ins, i, nxt)
                continue
                lhs = pop(st) if st else E(FALTA)
                op = "&&" if name == "JTFALSE" else "||"
                pr = PREC[op]
                j = nxt[i]
                if j is not None and ins[j][0] == "POP":
                    j = nxt[j]
                rhs = self.sub_expr(proc, ins, j, arg, out, pad)
                st.append(E("%s %s %s" % (lhs.par(pr), op, rhs.par(pr + 1)), pr))
                i = arg
                continue
            self.step(proc, name, ty, uns, arg, st, out, pad)
            i = nxt[i]
        return st[-1] if st else E(FALTA)

    def eval_cond(self, proc, ins, i, hi, nxt, out, pad):
        """Ejecuta el tramo de condición hasta el salto y devuelve (texto, pos siguiente)."""
        st = []
        while i is not None and i < hi:
            name, ty, uns, arg, sz = ins[i]
            if name in ("JFALSE", "JTRUE"):
                c = pop(st) if st else E(FALTA)
                return (c.t if name == "JFALSE" else negate(c).t), nxt[i]
            if name in ("JTFALSE", "JTTRUE"):
                # Los dos motores usan el mismo idioma: JTFALSE deja el valor en
                # pila y el AND/OR posterior combina los dos operandos; el || añade
                # el atajo "POP; PUSH 1; JUMP". Saltarlo y dejar que el AND/OR haga
                # su trabajo da la expresion correcta. Antes esto solo se aplicaba
                # a BennuGD1 y en BennuGD2 salian cosas como "A && 1 || B" o
                # "A && (? && B)".
                i = self.skip_shortcircuit(ins, i, nxt)
                continue
                lhs = pop(st) if st else E(FALTA)
                op = "&&" if name == "JTFALSE" else "||"
                pr = PREC[op]
                j = nxt[i]
                if j is not None and ins[j][0] == "POP":
                    j = nxt[j]
                st.append(E("%s %s %s" % (lhs.par(pr),
                            op, self.sub_expr(proc, ins, j, arg, out, pad).par(pr + 1)), pr))
                i = arg
                continue
            self.step(proc, name, ty, uns, arg, st, out, pad)
            i = nxt[i]
        return FALTA, i

    # Marcas que deja la reconstruccion estructurada cuando no ha podido con el
    # flujo. Si aparece alguna, el resultado no vale y hay que rehacerlo plano.
    FRACASO = ("bucle no estructurado", "salto ya visitado",
               "descompilacion abortada", "descompilación abortada", FALTA)

    def emit_plano(self, proc, ins, ind, out):
        """Renderizado LINEAL con etiquetas y GOTO.

        Red de seguridad para el flujo que no es reducible a bucles y condicionales:
        el codigo escrito con "goto" y etiquetas, tipico de maquinas de estados,
        no se puede meter a la fuerza en LOOP/IF sin inventarselo. Antes salian
        LOOP y BREAK que no estaban, y un "// bucle no estructurado" que ni
        compila. Asi sale feo pero es FIEL: BennuGD admite etiquetas y goto.
        """
        pad = "    " * ind
        keys = sorted(ins)
        pos = {k: n for n, k in enumerate(keys)}
        destinos = set()
        for k in keys:
            name, ty, uns, arg, sz = ins[k]
            if name in self.SALTAN and arg is not None and arg in ins:
                destinos.add(arg)
        saltar = set()          # cabeceras de case: no se emiten en el recorrido
        st, n = [], 0
        # Un IF cuyo cuerpo no recibe ningun salto de fuera se puede escribir
        # como IF ... END de verdad, sin negar la condicion ni meter un GOTO.
        # `pendientes` lleva los offsets donde hay que cerrarlos.
        pendientes, nivel = [], 0
        while n < len(keys):
            k = keys[n]
            while pendientes and k >= pendientes[-1]:
                pendientes.pop()
                nivel -= 1
                out.append("    " * (ind + nivel) + "END")
            pad = "    " * (ind + nivel)
            if k in saltar:
                n += 1
                continue
            if k in destinos:
                out.append("%slbl_%d:" % ("    " * max(0, ind - 1), k))
                st = []
            name, ty, uns, arg, sz = ins[k]
            if name in ("JTFALSE", "JTTRUE"):
                # cortocircuito de && / ||: la expresion se evalua entera aparte
                lhs = pop(st)
                op = "&&" if name == "JTFALSE" else "||"
                pr = PREC[op]
                j = self.nxt[k]
                if j is not None and ins.get(j, ("",))[0] == "POP":
                    j = self.nxt[j]
                rhs = self.sub_expr(proc, ins, j, arg, out, pad)
                st.append(E("%s %s %s" % (lhs.par(pr), op, rhs.par(pr + 1)), pr))
                n = pos.get(arg, len(keys))
                continue
            if name == "SWITCH":
                # El despacho se deja ESTRUCTURADO (asi el valor se evalua una
                # sola vez, que es lo que hace el bytecode) y cada rama salta a
                # su cuerpo, que se emite mas abajo con su etiqueta. Sin esto el
                # modo plano no sabia de switch y soltaba "// ?? SWITCH",
                # "// ?? CASE" y "// ?? JNOCASE": justo donde mas falta hace.
                val = pop(st)
                casos, cab, fin = self.plano_switch(proc, ins, k, len(ins) and max(ins) + 1, pad)
                saltar |= cab
                out.append(pad + "SWITCH (%s)" % val.t)
                hay_def = False
                for etiq, cuerpo in casos:
                    destinos.add(cuerpo)
                    if etiq is None:
                        hay_def = True
                        out.append(pad + "    DEFAULT:")
                    else:
                        out.append(pad + "    CASE %s:" % ", ".join(etiq))
                    out.append(pad + "        GOTO lbl_%d;" % cuerpo)
                    out.append(pad + "    END")
                if not hay_def and fin is not None:
                    destinos.add(fin)
                    out.append(pad + "    DEFAULT:")
                    out.append(pad + "        GOTO lbl_%d;" % fin)
                    out.append(pad + "    END")
                out.append(pad + "END")
                st = []
                n += 1
                continue
            if name == "JUMP":
                out.append(pad + "GOTO lbl_%d;" % arg)
                st = []
            elif name in ("JFALSE", "JTRUE"):
                cond = pop(st)
                # "JFALSE aqui_al_lado; JUMP destino" es como el compilador emite
                # un "IF (cond) GOTO destino; END": reconocerlo evita negar la
                # condicion y meter un salto de mas (28 instrucciones en vez de
                # las 25 del original).
                j = self.nxt.get(k)
                if (name == "JFALSE" and j is not None and j in ins
                        and ins[j][0] == "JUMP" and self.nxt.get(j) == arg):
                    out.append(pad + "IF (%s) GOTO lbl_%d; END" % (cond.t, ins[j][3]))
                    n = pos.get(arg, len(keys))
                    st = []
                    continue
                # Cuerpo limpio: ninguna instruccion de dentro es destino de
                # otro salto, asi que cabe en un IF de verdad. Negar la
                # condicion y saltar cuesta un JUMP de mas y no es lo que
                # ponia el original.
                if (name == "JFALSE" and arg is not None and arg in ins
                        and arg > k
                        and not any(k < x < arg for x in destinos)):
                    out.append(pad + "IF (%s)" % cond.t)
                    pendientes.append(arg)
                    nivel += 1
                    st = []
                    n += 1
                    continue
                if name == "JFALSE":
                    cond = negate(cond)
                out.append(pad + "IF (%s) GOTO lbl_%d; END" % (cond.t, arg))
            elif name == "END":
                # el END que cierra el proceso es implicito (igual que en el
                # modo estructurado): emitirlo mete un RETURN que el original
                # no tiene y cambia el tamaño del .dcb
                if k != self.fin_proc:
                    out.append(pad + "RETURN;")
                st = []
            else:
                self.step(proc, name, ty, uns, arg, st, out, pad)
            n += 1
        while pendientes:
            pendientes.pop()
            nivel -= 1
            out.append("    " * (ind + nivel) + "END")

    def fin_switch(self, ins, inicio, desde, hi):
        """¿Este switch es el cuerpo de un bucle?

        Lo es si dentro de su rango hay un salto ATRAS que apunta al principio de
        la sentencia (donde se evalua el valor del switch). Devuelve el offset
        siguiente al ULTIMO de esos saltos, que es donde acaba el bucle, o None.
        """
        ultimo = None
        for k in sorted(ins):
            if k < desde or k >= hi:
                continue
            name, ty, uns, arg, sz = ins[k]
            if name == "JUMP" and arg == inicio and k > inicio:
                ultimo = k
        return self.nxt.get(ultimo) if ultimo is not None else None

    def plano_switch(self, proc, ins, k, hi, pad):
        """Analiza un switch para el modo plano.

        Devuelve (casos, cabeceras, fin): los casos como (etiquetas, offset del
        cuerpo) -- con etiquetas None para el DEFAULT --, los offsets de las
        instrucciones que forman la cadena de comparacion (que no hay que emitir
        en el recorrido lineal) y el offset donde acaba el switch.
        """
        casos, cab, st = [], set(), []
        basura = []
        i, fin = self.nxt.get(k), None
        while i is not None and i < hi:
            etiq, j = [], i
            while j is not None and j < hi and ins[j][0] != "JNOCASE":
                name, ty, uns, arg, sz = ins[j]
                cab.add(j)
                if name == "CASE":
                    etiq.append(pop(st).t if st else FALTA)
                elif name == "CASE_R":
                    b_ = pop(st).t
                    a_ = pop(st).t
                    etiq.append("%s..%s" % (a_, b_))
                else:
                    self.step(proc, name, ty, uns, arg, st, basura, pad)
                j = self.nxt.get(j)
            if j is None or j >= hi:
                if not etiq and i is not None and i < hi:
                    casos.append((None, i))      # DEFAULT
                break
            cab.add(j)
            salta = ins[j][3]
            casos.append((etiq, self.nxt.get(j)))
            anterior = self.prv.get(salta)
            if anterior is not None and ins.get(anterior, ("",))[0] == "JUMP":
                fin = ins[anterior][3]
            if salta <= i:
                break
            i = salta
        return casos, cab, fin

    # Lo unico que puede aparecer entre dos casos de un switch: la expresion
    # constante del CASE. Cualquier otra cosa significa que el sondeo se ha
    # salido de la cadena.
    # Lo que NO puede aparecer en la etiqueta de un CASE. Antes esto era una
    # lista de opcodes PERMITIDOS, y se dejaba fuera cualquier cosa que no se
    # hubiese visto todavia: con "case get_value():" el CALL no estaba en ella,
    # el sondeo se rendia y el switch entero se descompilaba como un DEFAULT con
    # un "// ?? JNOCASE" dentro. Una etiqueta es una EXPRESION, asi que lo que
    # la descarta es encontrar flujo de control o una sentencia.
    NO_CASE = ({"JUMP", "JFALSE", "JTRUE", "JTFALSE", "JTTRUE", "JNFALSE",
                "JNTRUE", "SWITCH", "FRAME", "RETURN", "END", "CLONE",
                "PROC", "SYSPROC", "NCALL", "DEBUG", "GOTO"}
               | set(mn.ASSIGNOP))

    def emit_switch(self, proc, ins, i, hi, ind, out, nxt, prv):
        pad = "    " * ind
        end = hi
        while i is not None and i < hi:
            st = []
            labels = []
            j = i
            # Recoge las expresiones CASE hasta el JNOCASE. Es un SONDEO: puede
            # no haber JNOCASE (ultimo caso o default) y entonces habra recorrido
            # codigo normal. Por eso escribe en una lista aparte, que solo se
            # vuelca si el sondeo acierta: si no, esas sentencias se quedaban en
            # el fuente como "// ?? JUMP", "// ?? JFALSE"... 30 seguidas en un
            # proceso de 5leaps.
            sonda = []
            while j is not None and j < hi and ins[j][0] != "JNOCASE":
                name, ty, uns, arg, sz = ins[j]
                if name in self.NO_CASE:
                    # no es una cadena de casos: es codigo normal (un switch
                    # anidado dentro de un case, por ejemplo). Abandonar el
                    # sondeo en vez de atravesarlo dejando "// ??" por medio.
                    j = None
                    break
                if name == "CASE":
                    labels.append(pop(st).t if st else FALTA)
                elif name == "CASE_R":
                    b_ = pop(st).t; a_ = pop(st).t
                    labels.append("%s..%s" % (a_, b_))
                else:
                    self.step(proc, name, ty, uns, arg, st, sonda, pad)
                j = nxt[j]
            if j is not None and j < hi:
                out += sonda
            if j is None or j >= hi:
                if labels or i >= end:
                    break
                # El DEFAULT acaba donde acaba el SWITCH, que ya lo dice el
                # JUMP con que cierran los casos, no donde acaba el bloque de
                # fuera: usando `hi` se tragaba las sentencias de DESPUES del
                # switch y todos los saltos posteriores salian corridos.
                out.append(pad[:-4] + "DEFAULT:")
                self.emit(proc, ins, i, end, ind, out)
                out.append(pad[:-4] + "END")
                return end
            skip = ins[j][3]
            out.append(pad[:-4] + "CASE %s:" % ", ".join(labels))
            body_end = prv.get(skip)
            self.emit(proc, ins, nxt[j], body_end, ind, out)
            out.append(pad[:-4] + "END")   # en BennuGD cada CASE cierra con END
            if body_end is not None and ins[body_end][0] == "JUMP":
                end = ins[body_end][3]
            if skip <= i:
                break
            i = skip
            if i >= end:
                break
        return end

    # ================= sentencias sueltas =================
    def step(self, proc, name, ty, uns, arg, st, out, pad):
        # un cast solo cuenta si su inverso viene JUSTO despues
        pend = getattr(self, "cast_pend", None)
        self.cast_pend = None
        if name in ("PRIVATE", "GLOBAL", "LOCAL", "PUBLIC"):
            vl = {"GLOBAL": self.d.glovar, "LOCAL": self.d.locvar,
                  "PRIVATE": proc.privar, "PUBLIC": proc.pubvar}[name]
            v, rel = self.r.var_at(vl, arg)
            # con rel != 0 el compilador ha plegado indices y offsets de campo:
            # hay que localizar el array dentro del tipo (ver desc_at)
            vi = self.r.desc_at(vl, arg) or (
                (v.chunks, v.members, 0) if v is not None and rel == 0 else None)
            st.append(E(self.vname(name, arg, proc), 99, True, name, arg,
                        scal=self.r.scalar_size(vl, arg), vinfo=vi)); return
        if name in ("GET_PRIV", "GET_GLOBAL", "GET_LOCAL", "GET_PUBLIC"):
            sp = {"GET_PRIV": "PRIVATE", "GET_GLOBAL": "GLOBAL",
                  "GET_LOCAL": "LOCAL", "GET_PUBLIC": "PUBLIC"}[name]
            vl = {"GLOBAL": self.d.glovar, "LOCAL": self.d.locvar,
                  "PRIVATE": proc.privar, "PUBLIC": proc.pubvar}[sp]
            v, rel = self.r.var_at(vl, arg)
            ms, nivp = (self.punteros_a_struct(v) if (v is not None and rel == 0)
                        else (None, 0))
            if ms is not None and nivp >= 2:
                # "STRUCT ** p": el valor todavia no es la direccion de un
                # struct, hace falta desreferenciar nivp-1 veces. Se escribe
                # "(*p).campo" (probado contra bgdc).
                st.append(E(self.vname(sp, arg, proc), 99, False, sp, arg,
                            vinfo=([(self.r.TYPE_POINTER, nivp - 1)], ms, 0,
                                   False)))
                return
            ms = ms if nivp == 1 else None
            if ms is None and v is not None:
                # el puntero a struct puede ser un CAMPO ("map_mapa.lronda"),
                # no la variable de primer nivel
                hoja = self.r.leaf_at(vl, arg)
                if hoja is not None and hoja is not v:
                    ms = self.apunta_a_struct(hoja)
            if ms is not None:
                # Leer un puntero a struct da la DIRECCION del struct, no un
                # valor suelto: "path.moves[i]" compila a GET_PRIV path; ARRAY 1,
                # con el offset del campo plegado. Hay que reponer el camino del
                # campo o sale "*PATH[i]", que ni compila ni significa lo mismo.
                # La base se deja DESNUDA ("minave"): el camino del campo lo
                # pone quien indexa. Pegarlo aqui ("minave.tipo.nombre") hacia
                # que el INDEX, que solo quita un nivel al recomponer, dejase
                # "minave.tipo.idnave" cuando idnave es campo de primer nivel.
                # el 4o elemento a False: el texto ya es el camino de verdad
                # ("map_mapa.lronda"), el INDEX que siga no debe pelarle nada
                st.append(E(self.vname(sp, arg, proc), 99, True, None, 0,
                            scal=self.r.scalar_size(self.d.varspaces[ms], 0),
                            vinfo=([(self.r.TYPE_STRUCT, 0)], ms, 0, False)))
                return
            st.append(E(self.vname(sp, arg, proc), 99, False, sp, arg)); return
        if name in ("REMOTE", "REMOTE_PUBLIC", "GET_REMOTE", "GET_REMOTE_PUBLIC"):
            inst = pop(st)
            # el acceso remoto indexa los LOCALES de la otra instancia
            # (o sus PUBLIC en la variante _PUBLIC), nunca sus privadas
            sp = "PUBLIC" if "PUBLIC" in name else "LOCAL"
            # el nombre sale de las publicas del proceso DESTINO, no de las del
            # que hace el acceso (que normalmente no tiene ninguna)
            dst = proc
            if sp == "PUBLIC" and inst.space is not None:
                dst = self.rem_tipo.get(
                    self.rem_key(inst.space, inst.off, self.pidx[id(proc)]),
                    proc)
            vn = self.vname(sp, arg, dst)
            if self.porter and vn.upper() in self.porter.stub_vars:
                # la local ya no existe en BennuGD2: "otro.X" no es expresable,
                # así que se marca en vez de inventar una equivalencia
                st.append(E("0 /* TODO: %s.%s no existe en BennuGD2 */"
                            % (inst.t, vn), 99)); return
            st.append(E("%s.%s" % (inst.par(99), vn),
                        99, not name.startswith("GET"))); return
        if name == "PUSH":
            if ty == self.T_STRING:
                s = self.d.strings[arg] if arg < len(self.d.strings) else ""
                st.append(E('"%s"' % s.replace('"', '\\"')))
            elif self.T_DOUBLE is not None and ty == self.T_DOUBLE:
                st.append(E(dbl(arg)))
            elif ty == self.T_FLOAT: st.append(E(flt(arg)))
            else: st.append(E(str(sgn(arg)), 99, raw=arg))
            return
        if name == "INDEX":
            if not st: st.append(E(FALTA))
            t = st[-1]
            # len(vinfo) > 3: viene de desreferenciar un campo puntero a
            # struct. PTR deja la expresion como VALOR, pero el INDEX que sigue
            # es el campo del struct apuntado, no aritmetica de punteros.
            if t.vinfo and t.space is None and (t.addr or len(t.vinfo) > 3):
                # Tras un ARRAY con indice dinamico, el compilador emite el
                # desplazamiento del CAMPO con INDEX. Tratarlo como indice de
                # array daba "PATHS[i].MOVES[80]" donde el original ponia
                # "PATHS[i].LEN".
                ch, mem, lvl = t.vinfo[:3]
                # Solo hay que pelar el ultimo campo cuando lo puso el propio
                # resolver al nombrar el primer escalar. Si el texto ya es el
                # camino de verdad ("lista.ultimo"), pelarlo daba
                # "lista.siguiente", que es otro campo.
                pelar = t.vinfo[3] if len(t.vinfo) > 3 else True
                # Si el offset cae JUSTO en un miembro que es un struct
                # embebido, el camino para AHI. Bajar hasta su primer escalar
                # ("nodo.evento.primerdialogo") sobra: el original compara la
                # DIRECCION del struct ("nodo.evento == evento") y no lleva PTR.
                # Si detras viene un PTR, ya lo repone el propio PTR.
                emb = self.miembro_struct(ch, lvl, mem, sgn(arg))
                if emb is not None:
                    base = t.t
                    if pelar:
                        base = re.sub(r"(\[0\])+$", "", base)
                        base = re.sub(r"\.[A-Za-z_]\w*(\[0\])*$", "", base)
                    st[-1] = E(base + "." + self.minus(emb.name), 99, True,
                               scal=t.scal,
                               vinfo=([(self.r.TYPE_STRUCT, 1)], emb.members,
                                      0, False))
                    return
                camino = self.minus(self.r.path_desde(ch, lvl, mem, sgn(arg)))
                if camino and not camino.startswith("+"):
                    base = t.t
                    if pelar:
                        base = re.sub(r"(\[0\])+$", "", base)
                        base = re.sub(r"\.[A-Za-z_]\w*(\[0\])*$", "", base)
                    # Un campo que es PUNTERO a struct se sigue encadenando:
                    # "lista.ultimo.siguiente" son INDEX; PTR; INDEX. Sin
                    # arrastrar el tipo salia "*(lista.ultimo + 28)".
                    vi2 = None
                    f = self.campo_en(ch, lvl, mem, sgn(arg))
                    if f is not None:
                        ms2, niv2 = self.punteros_a_struct(f)
                        if ms2 is not None:
                            vi2 = ([(self.r.TYPE_POINTER, niv2 - 1)], ms2, 0,
                                   False)
                    st[-1] = E(base + camino, 99, True, scal=t.scal, vinfo=vi2)
                    return
            if t.addr and t.space is not None:
                # dirección: desplazamiento a miembro de struct / elemento de array
                st[-1] = E(self.vname(t.space, t.off + arg, proc), 99, True, t.space, t.off + arg)
            elif t.addr and t.scal:
                # dirección ya indexada: el operando es un offset en bytes,
                # se convierte a índice y rellena el siguiente hueco "[0]"
                k = sgn(arg) // t.scal
                m = re.search(r'\[0\]', t.t)
                txt = (t.t[:m.start()] + "[%d]" % k + t.t[m.end():]) if m \
                    else "%s[%d]" % (t.t, k)
                st[-1] = E(txt, 99, True, scal=t.scal)
            else:
                # valor: el compilador usa INDEX como suma de constante
                k = sgn(arg)
                st[-1] = E("%s %s %d" % (t.par(9), "+" if k >= 0 else "-", abs(k)), 9)
            return
        if name == "ARRAY":
            idx = pop(st)
            if not st: st.append(E(FALTA))
            t = st[-1]
            # El resolver da el nombre con las dimensiones a 0 ("a[0][0]") y
            # cada ARRAY rellena un hueco. CUÁL se rellena lo dice el operando,
            # que es el PASO en bytes de la dimensión indexada: el compilador
            # pliega los índices constantes 0 de la izquierda, así que "a[0][i]"
            # sale como un ÚNICO ARRAY con el paso de la dimensión interna.
            # Rellenando siempre el primer hueco, "a[0][i]" se reconstruía como
            # "a[i][0]", que apunta a otro sitio de la memoria.
            salto, ndims = 0, 0
            if t.vinfo:
                ch, mem, lvl = t.vinfo[:3]
                dims = [k for k in range(len(ch)) if ch[k][0] == self.r.TYPE_ARRAY]
                ndims = len(dims[lvl:])
                for pos, k in enumerate(dims[lvl:]):
                    if self.r._size(ch, k + 1, mem) == sgn(arg):
                        salto = pos
                        break
            texto = t.t
            if "[0]" not in texto and t.vinfo:
                # base de struct sin hueco: el compilador pliega el offset del
                # campo, asi que hay que reponerlo ("minave" -> "minave.moves[0]")
                _ch, _mem, _lvl = t.vinfo[:3]
                if _ch and _ch[0][0] == self.r.TYPE_STRUCT:
                    texto += self.minus(self.r.path_desde(_ch, _lvl, _mem, 0))
            t = E(texto, t.p, t.addr, t.space, t.off, t.raw, t.scal, t.vinfo)
            # "salto" cuenta DIMENSIONES del tipo, pero en el texto solo hay
            # hueco donde el indice quedo a 0: en "enemy_palettes[29][0]" la
            # dimension 1 es el segundo corchete, no el segundo "[0]". Se mapea
            # dimension -> corchete y se comprueba que ese corchete sea el hueco.
            # desc_at devuelve el PRIMER array del camino, asi que los
            # corchetes de ese nivel son los primeros del texto: la dimension
            # "salto" es el corchete "salto", tenga el valor que tenga. Contar
            # solo los "[0]" confundia "enemy_palettes[29][0]" (dimension 1 =
            # segundo corchete) con el primer hueco.
            todos = list(re.finditer(r'\[[^\[\]]*\]', t.t))
            m = None
            if ndims and salto < len(todos) and todos[salto].group(0) == "[0]":
                m = todos[salto]
            if m is None:
                huecos = [h for h in todos if h.group(0) == "[0]"]
                if salto < len(huecos):
                    m = huecos[salto]
            if m is not None:
                txt = t.t[:m.start()] + "[%s]" % idx.t + t.t[m.end():]
            else:
                txt = t.t + "[%s]" % idx.t
            vi = None
            if t.vinfo:
                ch, mem, lvl = t.vinfo[:3]
                # se consumen los niveles saltados (índice 0) y el indexado
                vi = (ch, mem, lvl + salto + 1)
            # ARRAY calcula una DIRECCION (base + indice*paso), venga de donde
            # venga el operando. Si era un puntero (GET_PRIV de un "int *p"), el
            # resultado seguia marcado como valor y el PTR siguiente le metia un
            # asterisco: "*myarray[i]" en vez de "myarray[i]", que ni compila
            # ("Pointer required").
            self.anota_dim(t.t, arg)
            st[-1] = E(txt, 99, True, scal=t.scal, vinfo=vi); return
        if name == "PTR":
            t = pop(st)
            vi2, txt = None, None
            if t.vinfo and t.vinfo[0] and t.vinfo[0][0][0] == self.r.TYPE_POINTER:
                # se lee algo que apunta a un struct. El contador dice cuantos
                # niveles de puntero quedan por quitar: con 0 el texto ya es el
                # camino bueno ("lista.ultimo"); con mas, hay que escribir el
                # asterisco y parentizar para poder poner el punto detras.
                k = t.vinfo[0][0][1]
                if k <= 0:
                    txt = t.t
                    vi2 = ([(self.r.TYPE_STRUCT, 0)], t.vinfo[1], 0, False)
                elif k == 1:
                    txt = "(*%s)" % t.t
                    vi2 = ([(self.r.TYPE_STRUCT, 0)], t.vinfo[1], 0, False)
                else:
                    txt = "*" + t.t
                    vi2 = ([(self.r.TYPE_POINTER, k - 1)], t.vinfo[1], 0, False)
            elif self.es_struct(t.vinfo) and t.vinfo[2] == 0:
                # struct SIN indexar: desreferenciarlo lee su PRIMER campo. El
                # compilador se ahorra el "INDEX 0", asi que hay que reponer el
                # camino o sale "*motor", que no es un numero.
                txt = t.t + self.minus(self.r.path_desde(
                    [(self.r.TYPE_STRUCT, 0)], 0, t.vinfo[1], 0))
                # y si ese primer campo es a su vez un PUNTERO a struct, lo que
                # se ha leido es una direccion: hay que seguir el tipo para que
                # el INDEX siguiente de "nodo.siguiente.anterior" y no
                # "*(nodo.siguiente + 4)"
                f0 = self.campo_en([(self.r.TYPE_STRUCT, 0)], 0, t.vinfo[1], 0)
                ms0 = self.apunta_a_struct(f0) if f0 is not None else None
                if ms0 is not None:
                    vi2 = ([(self.r.TYPE_STRUCT, 0)], ms0, 0, False)
            if txt is None:
                txt = t.t if t.addr else "*" + t.par(11)
            st.append(E(txt, 99, vinfo=vi2)); return
        if name in mn.BINOP:
            op = mn.BINOP[name]; p = PREC.get(op, 5)
            if self.reinterpretar_literales:
                r_ = pop(st).astype(ty, self.T_FLOAT, self.T_DOUBLE)
                l_ = pop(st).astype(ty, self.T_FLOAT, self.T_DOUBLE)
            else:
                r_ = pop(st); l_ = pop(st)
            # La DIRECCION de un struct embebido usada como valor se escribe con
            # & ("&nodo.evento == evento"): sin el, bgdc dice "Data type not
            # accepted here" y con el sale el mismo bytecode que el original.
            l_, r_ = self.amp(l_), self.amp(r_)
            st.append(E("%s %s %s" % (l_.par(p), op, r_.par(p + 1)), p)); return
        if name in mn.UNOP:
            st.append(E(mn.UNOP[name] + pop(st).par(11), 11)); return
        if name in mn.ASSIGNOP:
            v = pop(st)
            if self.reinterpretar_literales:
                v = v.astype(ty, self.T_FLOAT, self.T_DOUBLE)
            # Asignar una DIRECCION de struct sin PTR delante es un "&" del
            # fuente: "nave.lpath = &map_mapa.lpath[i]". Sin el, se leia como
            # valor ("...lpath[i].primero") y bgdc metia un PTR de mas.
            dir_ = self.dir_struct(v)
            if dir_ is not None:
                v = E(dir_, 11)
            tgt = pop(st)
            # El destino de una asignacion es SIEMPRE una direccion. Si lo que
            # hay en la pila es un VALOR (viene de GET_PRIV, no de PRIVATE), es
            # el contenido de un puntero usado como destino: hay que escribir
            # "*p = x". Sin el asterisco salia "p = x", que machaca el puntero
            # en vez de escribir donde apunta: no es lo mismo ni de lejos.
            # Un puntero a struct SIN indexar apunta a su primer campo: el
            # compilador se ahorra el INDEX 0. "*lista.ultimo = x" no compila
            # ("Struct required"), lo que se escribe es "lista.ultimo.idnave".
            campo0 = self.campo_cero(tgt)
            if campo0 is not None:
                tgt = E(tgt.t + campo0, 99, True, tgt.space, tgt.off,
                        None, tgt.scal)
            elif not tgt.addr:
                tgt = E("*" + tgt.par(11), 11, True, tgt.space, tgt.off,
                        None, tgt.scal)
            op = mn.ASSIGNOP[name]
            if tgt.t.upper() in self.trunc_locals:
                # BennuGD1 las tenía enteras: sin truncar, las comparaciones
                # por igualdad del código original nunca se cumplen
                if op == "=":
                    out.append(pad + "%s = (INT)(%s);" % (tgt.t, v.t))
                else:
                    out.append(pad + "%s = (INT)(%s %s %s);"
                               % (tgt.t, tgt.t, op[:-1], v.t))
            else:
                out.append(pad + "%s %s %s;" % (tgt.t, op, v.t))
            if name != "LETNP":
                # LET deja en la pila la DIRECCION del destino, no el valor:
                # "t = t2 = 0" son LET; PTR; LETNP. Marcarlo como valor hacia
                # que el PTR siguiente escribiese "*t2" ("Pointer required").
                st.append(E(tgt.t, 99, True, tgt.space, tgt.off,
                            None, tgt.scal))
            return
        if name in ("INC", "DEC", "POSTINC", "POSTDEC"):
            t = pop(st); op = "+" if "INC" in name else "-"
            # Prefijo y sufijo NO son el mismo opcode: "++i" compila a INC y
            # "i++" a POSTINC. Emitir siempre el sufijo daba un round-trip con
            # 5 procesos eternamente "distintos"; y ese ruido de fondo
            # permanente es justo lo que hace que dejes de mirar el informe.
            pre = name in ("INC", "DEC")
            if arg in (1, None):
                txt = ("%s%s%s" % (op, op, t.t)) if pre else ("%s%s%s" % (t.t, op, op))
            else:
                txt = "%s %s= %s" % (t.t, op, arg)
            out.append(pad + txt + ";")
            st.append(E(t.t)); return
        if name == "NCALL" and arg in getattr(self, "ins_actual", ()):
            # "call etiqueta" de BennuGD: una subrutina DENTRO del proceso, no un
            # sysproc. Se trataba como llamada al sistema, no encontraba nada y
            # la sentencia desaparecia del fuente; el cuerpo de la subrutina
            # quedaba suelto al final como codigo muerto.
            self.etiquetas.add(arg)
            out.append(pad + "call lbl_%d;" % arg)
            return
        if name in ("SYSCALL", "SYSPROC", "NCALL"):
            fn, np, extra, ptypes = self.sysnames.get(
                arg, ("sys_%d" % arg, 0, 0, ""))
            if self.porter:
                fn = self.porter.map_func(fn, np - extra)
                pt2 = self.porter.param_types(fn, np - extra)
                if pt2:
                    ptypes = pt2
            vals = [pop(st) for _ in range(min(np, len(st)))][::-1]
            vals = vals[:len(vals) - extra] if extra else vals
            # Los parámetros marcados 'P' reciben una DIRECCIÓN: en el fuente
            # hay que escribir "&var". Sin el & se pasa el valor como puntero
            # y la función nativa escribe en una dirección inventada.
            args = []
            for k, e in enumerate(vals):
                t = ptypes[k] if k < len(ptypes) else ""
                if t == "V" and e.addr and e.space:
                    # 'V' recibe la variable completa: si la dirección es el
                    # inicio de un struct hay que nombrar el struct, no su
                    # primer campo (si no, save() escribe sólo unos bytes)
                    vl = {"GLOBAL": self.d.glovar, "LOCAL": self.d.locvar,
                          "PRIVATE": proc.privar, "PUBLIC": proc.pubvar}[e.space]
                    top = self.r.name_at_top(vl, e.off)
                    if top and self.porter:
                        # el nombre de primer nivel también necesita la
                        # traducción v1->v2 (fps -> frame_info.fps)
                        if e.space == "GLOBAL":
                            top = self.porter.map_global(top)
                        elif e.space == "LOCAL":
                            top = self.porter.map_local(top)
                    args.append(self.safe_name(top) if top else e.t)
                elif t == "P" and e.addr:
                    args.append("&" + e.t)
                else:
                    args.append(e.t)
            call = "%s(%s)" % (fn.lower(), ", ".join(args))
            (out.append(pad + call + ";") if name == "SYSPROC" else st.append(E(call)))
            return
        if name in ("CALL", "PROC", "TYPE"):
            p = self.d.procs[arg] if 0 <= arg < len(self.d.procs) else None
            pn = p.name.lower() if p else "proc_%d" % arg
            np = self.nparams(p) if p else 0
            if name == "TYPE":
                st.append(E("TYPE " + pn)); return
            vals = [pop(st) for _ in range(min(np, len(st)))][::-1]
            # Un parametro declarado puntero recibe la DIRECCION, y en el fuente
            # eso se escribe con &. Sin esto, "queue_pop(&r, &c)" se descompilaba
            # como "queue_pop(r, c)", que pasa el VALOR: ni compila igual ni hace
            # lo mismo. Los sysprocs ya lo hacian (ptypes "P"); las funciones del
            # propio programa, no. Tampoco vale excluir los arrays: "&LISTA[0]"
            # apunta al elemento 0, o sea a la misma direccion que el array, y
            # parece el array entero -- pero pasar el array pelado ni siquiera
            # compila ("Data type not accepted here"), asi que el & va igual.
            args = []
            for k, e in enumerate(vals):
                if e.addr and self.par_struct(p, k):
                    # se pasa el struct entero: quitar el ".CAMPO[0]" con que el
                    # resolver nombra su primer escalar ("PATHS[i].MOVES[0]" es
                    # la misma direccion que "PATHS[i]", pero lo que se escribe
                    # en el fuente es el struct)
                    exacto = self.camino_struct(e, p, k, proc)
                    if exacto is None:
                        exacto = self.quita_primer_escalar(e, p, k)
                    if exacto is not None:
                        args.append(exacto)
                        continue
                    t = e.t
                    while True:
                        t2 = re.sub(r"\.[A-Za-z_]\w*(\[0\])*$", "", t)
                        if t2 == t:
                            break
                        t = t2
                    args.append(t)
                elif e.addr and self.par_puntero(p, k):
                    args.append("&" + e.t)
                else:
                    args.append(e.t)
            call = "%s(%s)" % (pn, ", ".join(args))
            (out.append(pad + call + ";") if name == "PROC" else st.append(E(call)))
            return
        if name == "FRAME":
            v = pop(st).t if st else "100"
            out.append(pad + ("FRAME;" if v == "100" else "FRAME %s;" % v)); return
        if name == "A2STR":
            # Array de bytes leido como cadena. El operando es la PROFUNDIDAD en
            # la pila del valor afectado, no siempre la cima: en
            # "dialogo.nombre == \"...\"" el compilador empuja antes la cadena y
            # emite "A2STR 1". Convirtiendo siempre la cima salia la comparacion
            # al reves y con un solo caracter.
            k = 1 + (arg or 0)
            if len(st) >= k:
                e = st[-k]
                # si la base es un struct sin indexar, el array es su primer
                # campo y el compilador se ahorra el INDEX 0: hay que reponerlo
                # o sale el struct pelado, que bgdc compila como POINTER2STR
                st[-k] = E(sin_indice0(e.t + (self.campo_cero(e) or "")))
            return
        if name == "STR2A":
            # copia una cadena dentro de un array de bytes. No lleva LET: la
            # propia instruccion asigna (destino en [-2], cadena en [-1]) y deja
            # la cadena en la pila, que el POP siguiente descarta.
            v = pop(st) if st else E(FALTA)
            dst = pop(st) if st else E(FALTA)
            out.append(pad + "%s = %s;"
                       % (sin_indice0(dst.t + (self.campo_cero(dst) or "")), v.t))
            st.append(v)
            return
        if name == "STRI2CHR":
            # No es una conversion de un solo operando: consume la CADENA y el
            # INDICE y deja el caracter. Tratarlo como el resto de CONVERT
            # descuadraba la pila y salia "*source = i" en vez de
            # "dest[i] = source[i]".
            idx = pop(st)
            cad = pop(st)
            st.append(E("%s[%s]" % (cad.par(1), idx.t), 1)); return
        if name in mn.CONVERT:
            # La conversion explicita ya ha hecho el trabajo: ese literal es un
            # entero de verdad y no los bits de un float, asi que se le quita la
            # marca de "reinterpretable". OJO: el operando de la conversion es la
            # PROFUNDIDAD en la pila del valor afectado, no siempre la cima:
            # "PUSH <bits de 0.05>; INT2FLOAT 1" convierte el elemento de DEBAJO.
            k = 1 + (arg or 0)

            # Un cast que el programador ESCRIBIO se reconoce porque va seguido
            # de su inverso: "a * (int) c" compila a FLOAT2INT; INT2FLOAT antes
            # del MUL.float, porque el compilador trunca y vuelve a ensanchar
            # para operar. Esa ida y vuelta no la emite nunca por su cuenta, asi
            # que delata el cast. Sin esto, "b = a * (int) c" y "b = a * c" se
            # descompilaban IGUAL y la cuenta cambiaba (12 en vez de 18).
            # Las conversiones sueltas SI son implicitas (coercion al asignar o
            # al pasar un argumento) y no llevan cast en el fuente.
            if pend and pend[0] == k and INVERSA.get(pend[1]) == name:
                if len(st) >= k:
                    e = st[-k]
                    st[-k] = E("%s %s" % (CASTNOM[pend[1]], e.par(11)), 11,
                               e.addr, e.space, e.off, None, e.scal)
                return
            if name in INVERSA:
                self.cast_pend = (k, name)

            if len(st) >= k and st[-k].raw is not None:
                e = st[-k]
                # Una conversion DESDE double/float dice que ese literal crudo
                # eran los BITS de un real, no un entero: "a = (double) 1" sale
                # como "PUSH 4607182418800017408; DOUBLE2INT", y sin
                # reinterpretarlo se emitia ese numero tal cual. Es el mismo
                # criterio que ya se aplicaba en las operaciones aritmeticas.
                txt = e.t
                if self.reinterpretar_literales:
                    if name in ("DOUBLE2INT", "DOUBLE2FLOAT", "DOUBLE2STR"):
                        txt = dbl(e.raw)
                    elif name in ("FLOAT2INT", "FLOAT2DOUBLE", "FLOAT2STR"):
                        txt = flt(e.raw)
                st[-k] = E(txt, e.p, e.addr, e.space, e.off, None, e.scal)
            return
        if name == "POP":
            if st:
                e = pop(st)
                if e.t.endswith(")") and "(" in e.t:
                    out.append(pad + e.t + ";")
            return
        if name == "DUP":
            t = st[-1] if st else E(FALTA); st.append(E(t.t, t.p, t.addr, t.space, t.off)); return
        if name == "RETURN":
            out.append(pad + ("RETURN %s;" % pop(st).t if st else "RETURN;")); return
        if name == "END":
            out.append(pad + "RETURN;")
            return
        if name == "SUBSTR":
            c = pop(st).t; b_ = pop(st).t; a_ = pop(st).t
            st.append(E("substr(%s, %s, %s)" % (a_, b_, c))); return
        if name == "STRACAT":
            b_ = pop(st).t; a_ = pop(st).t; st.append(E("%s + %s" % (a_, b_), 9)); return
        if name in ("COPY_STRUCT", "COPY_ARRAY", "COPY_ARRAY_REPEAT"):
            v = pop(st).t; t = pop(st).t
            out.append(pad + "%s = %s;" % (t, v)); return
        if name in ("NOP", "SENTENCE", "DEBUG", "CASE", "CASE_R"):
            return
        if name in ("EXITHNDLR", "ERRHNDLR"):
            out.append(pad + "// %s -> offset %d" % (name, arg)); return
        self.warn += 1
        out.append(pad + "// ?? %s %s" % (name, arg if arg is not None else ""))

    # ================= declaraciones =================
    # tipos cuyo tamaño cambió de BennuGD1 (32 bits) a BennuGD2 (64 bits).
    # En una variable que se lee o escribe en disco hay que conservar el ancho
    # original o el fichero deja de cuadrar con la estructura.
    ANCHO_BGD1 = {"INT": "INT32", "QWORD": "DWORD"}

    def decl_lines(self, varlist, data=None, ind=1, strings=None, top=True,
                   inits=None, narrow=False):
        pad = "    " * ind
        lines = []
        for v in varlist:
            base, nm, members = self.r.type_decl(v)
            # ¿esta variable viaja a disco con load/save/fread/fwrite?
            estrecha = narrow or (top and v.name in getattr(self, "disk_vars", ()))
            _ms, _niv = self.punteros_a_struct(v)
            if members is not None and _ms is not None:
                # PUNTERO a struct: "STRUCT * nombre ... END" no es sintaxis
                # valida en ningun sitio. Siempre con un TYPE con nombre. Los
                # asteriscos ya vienen en el nombre que da type_decl
                # ("** nextcp"), y aqui hacen falta: sin ellos se declararia el
                # struct entero en vez de un puntero.
                nom = self.minus(self.safe_name(nm) if top else nm)
                lines.append(pad + "%s %s;" % (self.nombre_tipo(_ms), nom))
            elif members is not None and (members in self.tipos_nombrados
                                          or members in self.pila_vs):
                # Struct AUTORREFERENTE (una lista enlazada: un miembro que
                # apunta al mismo tipo). Meterlo en linea se llamaba a si mismo
                # sin fin: RecursionError. Se corta con un TYPE con nombre, que
                # es como se escribe en el fuente.
                # ese struct ya se declara como TYPE con nombre: usarlo en vez de
                # repetir la estructura entera
                nom = self.minus(self.safe_name(nm) if top else nm)
                lines.append(pad + "%s %s;" % (self.nombre_tipo(members), nom))
            elif members is not None:
                sub = self.d.varspaces[members] if members < len(self.d.varspaces) else []
                lines.append(pad + "STRUCT %s" % self.minus(self.safe_name(nm) if top else nm))
                # dentro de un struct los campos van cualificados: no colisionan
                self.pila_vs.add(members)
                lines += self.decl_lines(sub, None, ind + 1, top=False,
                                         narrow=estrecha)
                self.pila_vs.discard(members)
                lines.append(pad + "END")
            else:
                if estrecha:
                    if base in ("STRING", "POINTER"):
                        self.disk_warn.add("%s: campo %s de tipo %s (4 bytes en "
                                           "BennuGD1, 8 en BennuGD2)" % (nm, nm, base))
                    base = self.ANCHO_BGD1.get(base, base)
                nom = self.minus(self.safe_name(nm) if top else nm)
                ini = (inits or {}).get(v.name) if top else None
                # "TORRETA otro;" en vez de "INT otro;": es lo unico que deja
                # escribir despues "otro.enemigo" (ver scan_remote_public)
                if top:
                    base = self.tipo_remoto(v.offset) or base
                lines.append(pad + "%s %s%s;" % (base, nom,
                                                 " = %s" % ini if ini else ""))
        return lines

    def scan_disk_vars(self):
        """Variables que se leen/escriben en disco con load/save/fread/fwrite.

        El fichero lo escribió BennuGD1 con enteros de 32 bits. En BennuGD2 un
        INT ocupa 8, así que la misma estructura mide el doble y load() deja
        cada pareja de valores del fichero metida en un solo entero: los datos
        salen desplazados 32 bits o a cero. Hay que declararlas con el ancho
        original.

        En el bytecode la llamada empuja la ruta, la dirección del dato y dos
        argumentos que genera el compilador (descriptor del varspace y tamaño).
        El dato es la primera dirección que cae dentro de una variable con
        nombre; la generada por el compilador no resuelve a ninguna.
        """
        self.disk_vars, self.disk_warn = set(), set()
        # Sólo tiene sentido al portar un .dcb de BennuGD1 a BennuGD2: es ahí
        # donde el ancho cambia. En la salida BennuGD1 nativa INT32 ni existe, y
        # un .dcb de BennuGD2 ya escribió sus ficheros con enteros de 64 bits.
        if not self.porter or not self.v1:
            return
        codes = {c for c, (nm, _, _, _) in self.sysnames.items()
                 if nm.upper() in ("LOAD", "SAVE", "FREAD", "FWRITE")}
        if not codes:
            return
        espacios = {"GLOBAL": self.d.glovar, "PUBLIC": None, "PRIVATE": None}
        for proc in self.d.procs:
            code, i, prev = proc.code, 0, []
            while i < len(code):
                name, hasp, ty, uns = self.mndecode(code[i] & 0xFFFF)
                arg = code[i + 1] if hasp and i + 1 < len(code) else None
                if name in ("SYSPROC", "SYSCALL") and arg in codes:
                    for nm2, a2 in reversed(prev[-10:]):
                        if nm2 not in espacios:
                            continue
                        vl = (self.d.glovar if nm2 == "GLOBAL" else
                              proc.pubvar if nm2 == "PUBLIC" else proc.privar)
                        n = self.r.name_at(vl, a2)
                        if n:                       # la generada no resuelve
                            self.disk_vars.add(re.match(r'[A-Za-z_]\w*', n).group(0))
                            break
                prev.append((name, arg))
                i += 2 if hasp else 1

    def local_param(self, name):
        """Parámetro que es una local reservada: con tipo explícito al portar."""
        if self.porter:
            t = self.porter.local_type(name)
            if t:
                return "%s %s" % (t, name)
        return name

    def quita_primer_escalar(self, e, dest, k):
        """Quita del texto el camino al primer escalar del struct que se pasa.

        El compilador emite la DIRECCION del struct, que es la de su primer
        escalar, y el resolver la nombra asi: "minave.motor.velocidadactual".
        Lo que se escribe en el fuente es "minave.motor". Quitar todos los
        campos del final dejaba "minave", que es otro struct.
        """
        privs = sorted(dest.privar, key=lambda v: v.offset)[:self.nparams(dest)]
        ms = self.apunta_a_struct(privs[k]) if k < len(privs) else None
        if ms is None:
            return None
        if self.es_struct(e.vinfo) and e.vinfo[1] == ms and len(e.vinfo) > 3:
            return e.t                    # el texto ya es ese struct, tal cual
        suf = self.minus(self.r.path_desde([(self.r.TYPE_STRUCT, 0)], 0, ms, 0))
        if suf and e.t.endswith(suf) and len(e.t) > len(suf):
            return e.t[:-len(suf)]
        return None

    def camino_struct(self, e, dest, k, proc):
        """Camino hasta el struct EXACTO que espera el parametro k, o None.

        Sin esto, "disparo(x, y, angle, mitorreta.arma, ...)" se descompilaba
        como "disparo(..., mitorreta, ...)": misma sintaxis, otro struct.
        """
        if e.space is None:
            return None
        privs = sorted(dest.privar, key=lambda v: v.offset)[:self.nparams(dest)]
        ms = self.apunta_a_struct(privs[k]) if k < len(privs) else None
        if ms is None:
            return None
        vl = {"GLOBAL": self.d.glovar, "LOCAL": self.d.locvar,
              "PRIVATE": proc.privar, "PUBLIC": proc.pubvar}.get(e.space)
        if vl is None:
            return None
        camino = self.r.path_struct(vl, e.off, ms)
        return self.minus(self.safe_name(camino)) if camino else None

    def par_struct(self, proc, k):
        """¿El parametro k es un struct pasado por referencia?"""
        if proc is None:
            return False
        privs = sorted(proc.privar, key=lambda v: v.offset)[:self.nparams(proc)]
        return k < len(privs) and self.apunta_a_struct(privs[k]) is not None

    def par_puntero(self, proc, k):
        """¿El parametro k de ese proceso esta declarado como puntero?"""
        if proc is None:
            return False
        privs = sorted(proc.privar, key=lambda v: v.offset)[:self.nparams(proc)]
        if k >= len(privs):
            return False
        if self.apunta_a_struct(privs[k]) is not None:
            return False        # struct por referencia: se pasa sin & (ver abajo)
        ch, i = privs[k].chunks, 0
        while i < len(ch) and ch[i][0] == self.r.TYPE_ARRAY:
            i += 1
        return i < len(ch) and ch[i][0] == self.r.TYPE_POINTER

    def params_of(self, proc, ins):
        """Recupera nombres de parámetros del prólogo: LOCAL off / GET_PRIV n / LETNP.

        El prólogo NO sigue el orden de la firma: ahí sólo van las copias a
        locales reservadas (x, y, graph, file...), que pueden ocupar cualquier
        posición entre los parámetros. El orden real lo da el offset de la
        privada, que es el operando del GET_PRIV; se guarda en self.parbypriv
        para que params_ordered() reconstruya la firma.
        """
        names, keys = [], sorted(ins)
        self.parbypriv = {}
        # Sólo las privadas ANÓNIMAS son parámetros copiados a una local: si la
        # privada tiene nombre propio, ese LOCAL = GET_PRIV es una sentencia de
        # verdad del cuerpo ("GRAPH = TIPO;"), no parte del prólogo.
        anon = {v.offset for v in proc.privar if sgn(v.id) == -1}
        k = 0
        while k + 2 < len(keys) and len(names) < self.nparams(proc):
            a, b, c = ins[keys[k]], ins[keys[k + 1]], ins[keys[k + 2]]
            if a[0] == "LOCAL" and b[0] == "GET_PRIV" and c[0] in ("LETNP", "LET") \
                    and b[3] in anon:
                nom = self.local_param(self.vname("LOCAL", a[3], proc))
            elif a[0] == "PRIVATE" and b[0] == "GET_PRIV" \
                    and c[0] in ("LETNP", "LET") and b[3] in anon:
                # Misma condicion que arriba: solo es prologo si el ORIGEN es
                # una privada anonima. Sin ese filtro, un "aux = cadena;" con
                # que empieza el cuerpo se tomaba por una copia de parametro y
                # la sentencia desaparecia del fuente.
                nom = self.vname("PRIVATE", a[3], proc)
            else:
                break
            names.append(nom)
            self.parbypriv[b[3]] = nom
            k += 3
        if k < len(keys):
            return names, keys[k]
        # el prologo se ha comido TODO el codigo: el cuerpo esta vacio. Devolver
        # 0 lo reemitia como si fuese cuerpo ("angle = id_4294967295;", que usa
        # el nombre inventado de una privada anonima y no compila).
        return names, (keys[-1] + 1 if keys else 0)

    def params_ordered(self, proc, npar):
        """Firma en el orden real: el de los offsets de las privadas.

        Los parámetros ocupan las npar primeras privadas. Cada una es o bien
        una privada con nombre propio, o bien anónima (id -1) porque se copia a
        una local reservada en el prólogo, y entonces el nombre sale de
        self.parbypriv.

        Devuelve None si algo no cuadra, para caer al camino antiguo.
        """
        privs = sorted(proc.privar, key=lambda v: v.offset)[:npar]
        if len(privs) != npar:
            return None
        bypriv = getattr(self, "parbypriv", {})
        nombres, conname = [], set()
        for v in privs:
            if sgn(v.id) != -1:           # privada con nombre propio: manda ella
                nombres.append(self.param_decl(v))
                conname.add(v.name)
            elif v.offset in bypriv:      # anónima: el nombre sale del prólogo
                nombres.append(bypriv[v.offset])
            else:
                return None               # anónima sin prólogo: no sé nombrarla
        return nombres, conname

    def array_inits(self, varlist, data):
        """Inicializadores de ARRAYS de escalares, listos para la declaracion.

        El original los guarda en la imagen de datos del proceso, no como codigo.
        Emitirlos como asignaciones al principio del cuerpo funciona, pero mete
        en el fuente sentencias que el programa original no tenia y desplaza todo
        el proceso. BennuGD admite "INT tabla[3] = 1, 2, 3;" en la declaracion,
        que es exactamente lo que hacia el compilador.

        Devuelve {nombre_raiz: "v0, v1, ..."} solo para arrays de escalares sin
        struct por medio; el resto se sigue emitiendo como asignaciones.
        """
        d = self.d
        porcampo = {}
        for name, t, off in self.r.iter_scalars(varlist):
            m = re.match(r"([A-Za-z_]\w*)((?:\[\d+\])+)$", name)
            if not m:
                porcampo.setdefault(None, []).append(name)   # marca "no simple"
                continue
            porcampo.setdefault(m.group(1), []).append((off, t))
        fuera = set()
        for name, t, off in self.r.iter_scalars(varlist):
            if "." in name:
                fuera.add(name.split(".")[0])
        out = {}
        for raiz, elems in porcampo.items():
            if raiz is None or raiz in fuera:
                continue
            vals = []
            for off, t in sorted(elems):
                sz = self.r.TS.get(t, self.r.W)
                chunk = data[off:off + sz]
                if len(chunk) != sz:
                    vals = []
                    break
                vals.append(self.lit(chunk, t))
            vacio = ("0", '""')
            if not vals or all(v in vacio for v in vals):
                continue
            while len(vals) > 1 and vals[-1] in vacio:
                vals.pop()
            out[raiz] = ", ".join(vals)
        return out

    def lit(self, chunk, t):
        """Un escalar crudo como literal del fuente."""
        d = self.d
        if t == d.T_STRING:
            idx = int.from_bytes(chunk, "little")
            if idx >= len(d.strings):
                return '""'
            return '"%s"' % d.strings[idx].replace('"', '\\"')
        if t == d.T_FLOAT:
            return "%g" % struct.unpack("<f", chunk[:4])[0]
        if d.T_DOUBLE is not None and t == d.T_DOUBLE:
            return "%g" % struct.unpack("<d", chunk[:8])[0]
        return str(int.from_bytes(chunk, "little", signed=True))

    def var_inits(self, varlist, data):
        """Valores iniciales de un espacio de variables: [(nombre, valor)].

        El DCB guarda el contenido inicial de globales, privadas y públicas.
        Ignorarlo deja el juego con todas sus tablas y banderas a cero.
        """
        d = self.d
        out = []
        for name, t, off in self.r.iter_scalars(varlist):
            sz = self.r.TS.get(t, self.r.W)
            chunk = data[off:off + sz]
            if len(chunk) != sz or not any(chunk):
                continue
            if t == d.T_STRING:
                idx = int.from_bytes(chunk, "little")
                if idx >= len(d.strings) or not d.strings[idx]:
                    continue
                val = '"%s"' % d.strings[idx].replace('"', '\\"')
            elif t == d.T_FLOAT:
                val = "%g" % struct.unpack("<f", chunk[:4])[0]
            elif d.T_DOUBLE is not None and t == d.T_DOUBLE:
                val = "%g" % struct.unpack("<d", chunk[:8])[0]
            else:
                val = str(int.from_bytes(chunk, "little", signed=True))
            out.append((name, val))
        return out

    def global_inits(self, skip):
        """Valores iniciales de las globales del juego, que el DCB sí guarda.

        Los ARRAYS de escalares van en la DECLARACION ("INT tabla[3] = 1,2,3;"),
        que es lo que hacia el original: emitirlos como asignaciones metia en el
        fuente sentencias que el programa no tenia -- en un mapa de 24x32 eran
        768 lineas y una funcion bgd1_init_globals() enorme que no existia.

        Lo que no encaja en la declaracion (structs, mezclas) si se emite como
        asignaciones en esa funcion, que MAIN llama antes que a nada.
        """
        d = self.d
        base = d.base + d.h["OGlobal"]
        data = bytes(d.raw[base:base + d.h["SGlobal"]])
        self.glo_arrays = {k: v for k, v in self.array_inits(d.glovar, data).items()
                           if k.upper() not in skip}
        out = []
        for name, val in self.var_inits(d.glovar, data):
            raiz = re.match(r'[A-Za-z_]\w*', name).group(0)
            if raiz.upper() in skip:
                continue                      # la inicializa el propio motor
            if raiz in self.glo_arrays:
                continue                      # va en la declaracion
            out.append("    %s = %s;" % (self.minus(self.safe_name(name)), val))
        return out

    def apunta_a_struct(self, v):
        """Si v es un puntero a struct, devuelve el varspace apuntado."""
        ms, niveles = self.punteros_a_struct(v)
        return ms if niveles == 1 else None

    def punteros_a_struct(self, v):
        """(varspace apuntado, niveles de puntero) o (None, 0).

        "STRUCT ** p" no es sintaxis de BennuGD: un puntero doble a struct se
        escribe con el TYPE con nombre y sus asteriscos ("tipo_2 ** p").
        """
        ch, n = v.chunks, 0
        while n < len(ch) and ch[n][0] == self.r.TYPE_POINTER:
            n += 1
        if (n and n < len(ch) and ch[n][0] == self.r.TYPE_STRUCT
                and v.members is not None and v.members < len(self.d.varspaces)):
            return v.members, n
        return None, 0

    def registrar_tipos(self):
        """Reserva un TYPE con nombre para cada struct que se pasa por parametro.

        Hay que hacerlo ANTES de emitir el bloque GLOBAL: si el mismo struct lo
        usa una global (lo normal: "path_result paths[N]" y los parametros que
        la reciben comparten varspace), la global se declara con ESE tipo en vez
        de repetir la estructura entera, que es lo que hacia el original.
        """
        for proc in self.d.procs:
            n = self.nparams(proc)
            for v in sorted(proc.privar, key=lambda x: x.offset)[:n]:
                ms = self.apunta_a_struct(v)
                if ms is not None:
                    self.nombre_tipo(ms)

    def nombre_tipo(self, members):
        """Nombre para un struct que hay que declarar con TYPE ... END.

        BennuGD pasa los structs por referencia, asi que en el .dcb el parametro
        es un puntero a struct. Pero "STRUCT * p" NO es una declaracion de
        parametro valida ("Parameter name invalid"): la unica forma de escribirlo
        en el fuente es con un tipo con nombre, "path_result path". El nombre
        original no esta en el .dcb, asi que se inventa uno estable.
        """
        return self.tipos_nombrados.setdefault(members, "tipo_%d" % members)

    def campo_cero(self, e):
        """Camino al primer campo si la expresion es un struct sin indexar."""
        # nivel 0: la base sigue SIN indexar. Tras un ARRAY el nivel sube y
        # "path.moves[i]" ya es el destino: anadirle el campo 0 daba
        # "path.moves[i].moves[0]", que es otro sitio.
        if not self.es_struct(e.vinfo) or e.vinfo[2] != 0:
            return None
        return self.minus(self.r.path_desde(
            [(self.r.TYPE_STRUCT, 0)], 0, e.vinfo[1], 0)) or None

    # Marcadores de vinfo: (STRUCT,0) = la expresion YA es la direccion de un
    # struct (un puntero leido); (STRUCT,1) = es un struct EMBEBIDO, y usarlo
    # como valor exige escribir "&" delante.
    def es_struct(self, vinfo):
        return bool(vinfo) and vinfo[0] in ([(self.r.TYPE_STRUCT, 0)],
                                            [(self.r.TYPE_STRUCT, 1)])

    def dir_struct(self, e):
        """"&camino" si la expresion es la DIRECCION de un struct, o None."""
        if not e.addr or not e.vinfo:
            return None
        ch, mem, lvl = e.vinfo[:3]
        if lvl >= len(ch) or ch[lvl][0] != self.r.TYPE_STRUCT:
            return None
        if mem is None or mem >= len(self.d.varspaces):
            return None
        # El resolver nombra la direccion con el camino de su primer escalar.
        # Si ese camino NO esta al final del texto, la expresion no es la
        # direccion de un struct entero (o ya paso por un PTR) y meterle un & es
        # peor que dejarlo: solo se toca cuando se puede comprobar.
        suf = self.minus(self.r.path_desde(ch, lvl, mem, 0))
        if suf and e.t.endswith(suf) and len(e.t) > len(suf):
            return "&" + e.t[:-len(suf)]
        if e.vinfo[0] == [(self.r.TYPE_STRUCT, 1)]:
            return "&" + e.t          # struct embebido, el texto ya es exacto
        return None

    def amp(self, e):
        """Antepone & si la expresion es la direccion de un struct embebido."""
        if e.addr and e.vinfo and e.vinfo[0] == [(self.r.TYPE_STRUCT, 1)]:
            return E("&" + e.par(11), 11)
        return e

    def miembro_struct(self, ch, lvl, mem, rel):
        """El miembro que EMPIEZA en `rel` y es un struct embebido, o None."""
        if lvl >= len(ch) or ch[lvl][0] != self.r.TYPE_STRUCT:
            return None
        if mem is None or mem >= len(self.d.varspaces):
            return None
        for v in self.d.varspaces[mem]:
            if v.offset == rel and v.chunks and v.chunks[0][0] == self.r.TYPE_STRUCT \
                    and v.members is not None and v.members < len(self.d.varspaces):
                return v
        return None

    def campo_en(self, ch, lvl, mem, rel):
        """El miembro exacto al que apunta un offset dentro de un struct."""
        if lvl >= len(ch) or ch[lvl][0] != self.r.TYPE_STRUCT:
            return None
        if mem is None or mem >= len(self.d.varspaces):
            return None
        # leaf_at, no var_at: el campo puede estar anidado ("nave.lpath"), y
        # quedarse en el miembro de primer nivel perdia que era un puntero
        return self.r.leaf_at(self.d.varspaces[mem], rel)

    def scan_remote_public(self):
        """Variables que guardan el ID de otro proceso y leen sus PUBLIC.

        "otro.enemigo" solo compila si "otro" se DECLARA con el nombre del
        proceso como tipo ("TORRETA otro;"): es lo unico que le da a bgdc el
        varspace de las publicas del otro proceso (c_data.c:551 y :906,
        c_main.c:1057). El .dcb no guarda ese tipo -- la variable es un INT y
        el enlace es cosa del compilador -- asi que hay que deducirlo.

        Se deduce por dos vias, la primera manda:
          1. de que proceso se ASIGNA la variable ("mipuntomapa = torreta();");
          2. el unico proceso cuyas publicas cubren todos los offsets usados.
        """
        self.rem_tipo, self.pub_bloque = {}, {}
        self.pidx = {id(p): i for i, p in enumerate(self.d.procs)}
        BASE = {"GET_PRIV": "PRIVATE", "GET_GLOBAL": "GLOBAL",
                "GET_LOCAL": "LOCAL", "GET_PUBLIC": "PUBLIC",
                "PRIVATE": "PRIVATE", "GLOBAL": "GLOBAL",
                "LOCAL": "LOCAL", "PUBLIC": "PUBLIC"}
        uso, asig = {}, {}
        for pi, proc in enumerate(self.d.procs):
            code, i, prev = proc.code, 0, []
            while i < len(code):
                nm, hasp, ty, uns = self.mndecode(code[i] & 0xFFFF)
                arg = code[i + 1] if hasp and i + 1 < len(code) else None
                if nm in ("REMOTE_PUBLIC", "GET_REMOTE_PUBLIC") and prev:
                    pn, pa = prev[-1]
                    if pn in BASE:
                        # se guarda tambien si el acceso era de CADENA: sirve
                        # para descartar candidatos cuyo campo no es del tipo
                        uso.setdefault(self.rem_key(BASE[pn], pa, pi),
                                       set()).add((arg, ty == self.T_STRING))
                elif nm.startswith("LET") and len(prev) >= 2:
                    # "var = otroproceso();" -> el tipo de var es ese proceso
                    (n1, a1), (n2, a2) = prev[-2], prev[-1]
                    if n2 == "CALL" and n1 in BASE and a2 is not None \
                            and 0 <= a2 < len(self.d.procs):
                        asig[self.rem_key(BASE[n1], a1, pi)] = self.d.procs[a2]
                prev.append((nm, arg))
                i += 2 if hasp else 1
        for k, offs in uso.items():
            cand = [p for p in self.d.procs if p.pubvar
                    and all(self.cuadra_pub(p, o, es_str) for o, es_str in offs)]
            elegido = asig.get(k)
            if elegido not in cand:
                # Sin asignacion directa hay que adivinar: el .dcb no guarda a
                # que proceso apunta una variable de tipo proceso. Se prefiere
                # el que comparte trozo de nombre con la variable ("mipanel" ->
                # MAP_MOSTRAR_PANEL_NOTIFICACION) y, en empate, el que menos
                # publicas tenga, que es el que menos nombres inventa.
                nombre = (self.r.name_at(self.espacio_de(k), k[2]) or "").upper()
                raiz = re.sub(r"^(MI|MIS|LA|EL|LOS|LAS)_?", "", nombre)
                elegido = min(cand, key=lambda p: (
                    0 if raiz and raiz in p.name.upper() else
                    1 if len(raiz) > 3 and raiz[:5] in p.name.upper() else 2,
                    len(p.pubvar), p.name)) if cand else None
            if elegido is not None:
                self.rem_tipo[k] = elegido

    def espacio_de(self, k):
        """La lista de variables del espacio al que se refiere una clave."""
        sp, pi, _off = k
        if sp == "GLOBAL":
            return self.d.glovar
        if sp == "LOCAL":
            return self.d.locvar
        pr = self.d.procs[pi]
        return pr.pubvar if sp == "PUBLIC" else pr.privar

    def cuadra_pub(self, p, off, es_str):
        """El proceso tiene una publica en ese offset y del tipo del acceso."""
        v, rel = self.r.var_at(p.pubvar, off)
        if v is None or self.r.name_at(p.pubvar, off) is None:
            return False
        return (self.r.type_decl(v)[0] == "STRING") == bool(es_str)

    @staticmethod
    def rem_key(space, off, pi):
        """Identifica una variable: las privadas/publicas son de cada proceso."""
        return (space, pi if space in ("PRIVATE", "PUBLIC") else -1, off)

    def tipo_remoto(self, off):
        """Nombre del proceso con que declarar la variable, o None."""
        p = self.rem_tipo.get(self.rem_key(self.decl_space, off, self.decl_owner))
        return self.minus(p.name) if p is not None else None

    def param_decl(self, v):
        """Declaración de un parámetro.

        BennuGD no admite dimensiones en la firma ("INT a[5]" es un error de
        sintaxis): los arrays se pasan por puntero.
        """
        ms, niveles = self.punteros_a_struct(v)
        if ms is not None:
            # un parametro con el TYPE a secas YA se recibe por referencia
            # (STRUCT *); los asteriscos son los niveles de mas
            estrellas = " " + "*" * niveles if niveles > 1 else ""
            return "%s%s %s" % (self.nombre_tipo(ms), estrellas,
                                self.minus(self.safe_name(v.name)))
        base, nm, members = self.r.type_decl(v)
        if "[" in nm:
            return "%s * %s" % (base, self.minus(self.safe_name(v.name)))
        return "%s %s" % (self.tipo_remoto(v.offset) or base,
                          self.minus(self.safe_name(nm)))

    def proc_source(self, proc):
        # el final del cuerpo del bucle es por proceso: si se colara de uno a
        # otro, un salto atras se tomaria por CONTINUE o por bucle sin
        # estructurar en el proceso siguiente
        self.fin_cuerpo = None
        self.decl_space, self.decl_owner = "PRIVATE", self.pidx[id(proc)]
        ins = self.decode(proc)
        self.fin_proc = max(ins) if ins else -1
        pnames, body_start = self.params_of(proc, ins)
        npar = self.nparams(proc)
        # La firma puede ser MIXTA: unos parámetros se copian a locales
        # reservadas en el prólogo (x, y, file, flags, z...) y van sin tipo; los
        # demás son privadas con nombre, que en BennuGD2 sí necesitan tipo.
        # El orden lo mandan los offsets de las privadas, NO el prólogo: dar por
        # hecho que las locales van primero descoloca la firma entera y las
        # llamadas acaban pasando cada argumento al parámetro equivocado.
        self.parnames = set()
        orden = self.params_ordered(proc, npar)
        if orden:
            pnames, self.parnames = orden
        else:
            named = sorted((v for v in proc.privar if sgn(v.id) != -1),
                           key=lambda v: v.offset)
            faltan = npar - len(pnames)
            if faltan > 0:
                extra = named[:faltan]
                pnames += [self.param_decl(v) for v in extra]
                self.parnames = {v.name for v in extra}
        while len(pnames) < npar:
            pnames.append("INT param%d" % len(pnames))
        kw = "PROCESS" if proc.d["Flags"] & 1 else "FUNCTION"
        # NParams == -1 es "no se cuantos": lo lleva el cuerpo del programa,
        # pero TAMBIEN un proceso solo declarado y nunca definido (5 de los 586
        # de 5 Leaps). Solo es el programa si se llama MAIN o si el .dcb no
        # trae tabla de identificadores y por tanto no hay ningun nombre.
        if (proc.d["NParams"] in (0xFFFFFFFF, 0xFFFFFFFFFFFFFFFF)
                and (proc.name == "MAIN" or not self.d.h["NID"])):
            # NParams == -1 es el centinela de "programa suelto": el fuente no
            # tenia "PROCESS MAIN()", solo "BEGIN ... END". Ponerle la cabecera
            # deja NParams a 0 y el .dcb ya no sale igual. Se mira el centinela
            # y no el nombre: un .dcb sin tabla de identificadores no tiene
            # ningun proceso llamado MAIN.
            head = None
        elif proc.name == "MAIN":
            head = "PROCESS MAIN()"
        else:
            # el mismo criterio que las variables: en las LLAMADAS ya salian en
            # minuscula, asi que la declaracion tenia que ir igual
            head = "%s %s(%s)" % (kw, self.minus(proc.name), ", ".join(pnames))
        out = [head] if head else []
        if not proc.code:
            # Proceso DECLARADO y nunca definido: en el .dcb no tiene ni una
            # instruccion. Darle un cuerpo vacio le mete un END que el original
            # no tiene, asi que solo se emite su declaracion adelantada.
            # head es None cuando el proceso es el cuerpo del programa
            # ("BEGIN ... END" suelto): eso no se declara por delante
            if head:
                self.sin_cuerpo.append(head)
            if head and proc.pubvar:
                self.decl_space, self.decl_owner = "PUBLIC", self.pidx[id(proc)]
                self.pub_bloque[head] = (["PUBLIC"]
                                         + self.decl_lines(proc.pubvar) + ["END"])
            return ["// %s: declarado y nunca definido" % (head or "programa"), ""]
        # El DCB guarda el contenido inicial de las privadas/públicas: los
        # escalares van en la propia declaración y el resto (arrays, structs)
        # como asignaciones al principio del cuerpo.
        pinit = self.var_inits(proc.privar, proc.pridata)
        pinit += self.var_inits(proc.pubvar, proc.pubdata)
        pars = getattr(self, "parnames", ())
        # Los arrays de escalares se inicializan en la DECLARACION, como hacia el
        # compilador original; solo lo que no encaja ahi (structs, mezclas) queda
        # como asignaciones al principio del cuerpo.
        arr = self.array_inits(proc.privar, proc.pridata)
        arr.update(self.array_inits(proc.pubvar, proc.pubdata))
        simple, compuestos = {}, []
        for nom, val in pinit:
            raiz = re.match(r'[A-Za-z_]\w*', nom).group(0)
            if raiz in pars:
                continue                      # lo fija quien llama al proceso
            if raiz in arr:
                continue                      # va en la declaracion
            if nom == raiz:
                simple[nom] = val
            else:
                compuestos.append("    %s = %s;" % (self.safe_name(nom), val))
        simple.update(arr)
        self.body_inits = compuestos

        priv = [v for v in proc.privar
                if sgn(v.id) != -1 and v.name not in getattr(self, "parnames", ())]
        if priv:
            out.append("PRIVATE")
            out += self.decl_lines(priv, inits=simple)
        # Sitio para las privadas que el .dcb no declara (Fenix sin depuracion):
        # cuales son solo se sabe DESPUES de recorrer el cuerpo.
        hueco_priv = len(out)
        if proc.pubvar:
            # Las PUBLIC NO van aqui: se declaran en la declaracion adelantada,
            # que es lo que las hace visibles para "otro.campo" antes de que el
            # proceso aparezca. Repetirlas en el cuerpo es "BEGIN expected".
            self.decl_space, self.decl_owner = "PUBLIC", self.pidx[id(proc)]
            self.pub_bloque[head] = (["PUBLIC"]
                                     + self.decl_lines(proc.pubvar, inits=simple)
                                     + ["END"])
        out.append("BEGIN")
        if proc.name == "MAIN" and self.needs_init:
            out.append("    %s();" % self.nombre_init)
        out += getattr(self, "body_inits", [])
        self.index(ins)
        self.silent_jumps = set()
        self.budget = 20 * (len(ins) + 16)
        self.ins_actual = ins
        self.marca_proc = {}
        self.etiquetas = {v[3] for v in ins.values()
                          if v[0] == "NCALL" and v[3] in ins}
        body = []
        self.out_body = body
        self.emit(proc, ins, body_start, max(ins) + 1 if ins else 0, 1, body)
        # Si la reconstruccion estructurada no ha podido con el flujo, lo que ha
        # salido no vale (LOOP/BREAK inventados, comentarios que no compilan):
        # se rehace lineal con etiquetas y goto, que siempre es fiel.
        if any(m in l for l in body for m in self.FRACASO):
            aviso = sum(1 for l in body for m in self.FRACASO if m in l)
            self.warn -= 0
            body = []
            self.index(ins)
            self.emit_plano(proc, ins, 1, body)
            self.planos.append(proc.name)
        # El bytecode acaba todo proceso con END, que traducimos como RETURN.
        # Si el cuerpo ya termina en un RETURN explicito, ese segundo sobra: el
        # original no lo tenia y BennuGD lo hace implicito de todas formas.
        while len(body) > 1 and body[-1].strip() == "RETURN;":
            anterior = body[-2].strip()
            if anterior.startswith("RETURN") or anterior == "END":
                body.pop()
            else:
                break
        sueltas = self.priv_sin_nombre.get(id(proc), {})
        if sueltas:
            decl = ["    INT %s%s;  // offset %d" % (self.minus(n),
                                                     self.dims_uso.get(n, ""), o)
                    for n, o in sorted(sueltas.items(), key=lambda x: x[1])]
            out[hueco_priv:hueco_priv] = (
                (["PRIVATE"] if not priv else []) + decl)
        out += body
        out.append("END")
        out.append("")
        return out


# Colores ANSI para la traza. Se apagan solos si stderr no es un terminal, si
# TERM=dumb o si esta NO_COLOR (no-color.org), de modo que redirigir la salida a
# un fichero sigue dando texto limpio; FORCE_COLOR los enciende a la fuerza.
# NO_COLOR manda sobre todo lo demas: es la peticion explicita del usuario de no
# ver color nunca, y ante la duda se elige lo conservador.
if "NO_COLOR" in os.environ:
    COLOR = False
elif os.environ.get("FORCE_COLOR"):
    COLOR = True
else:
    COLOR = (hasattr(sys.stderr, "isatty") and sys.stderr.isatty()
             and os.environ.get("TERM", "") not in ("", "dumb"))

# gris, rojo, verde, amarillo, cian, negrita, blanco fuerte
TENUE, ROJO, VERDE, AMBAR, CIAN, FUERTE, CLARO = (
    "2", "31", "32", "33", "36", "1", "97")


def c(codigo, txt):
    """Envuelve en un codigo ANSI, o lo deja crudo si no hay color."""
    return "\033[%sm%s\033[0m" % (codigo, txt) if COLOR else str(txt)


def paso(msg, tipo="dato"):
    """Traza de progreso. Va por stderr porque el .prg sale por stdout.

    tipo: titulo (cabecera de fase), dato (clave : valor), aviso, malo, ok,
    crudo (ya viene coloreado desde el sitio de la llamada).
    """
    if tipo == "titulo":
        msg = c(FUERTE + ";" + CIAN, msg)
    elif tipo == "aviso":
        msg = c(AMBAR, msg)
    elif tipo == "malo":
        msg = c(ROJO, msg)
    elif tipo == "ok":
        msg = c(FUERTE + ";" + VERDE, msg)
    elif tipo == "dato":
        # "   clave   : valor" -> la clave en gris, el valor en claro
        m = re.match(r"^(\s*)([^:]+?)(\s*):(\s*)(.*)$", msg, re.S)
        if m:
            ini, clave, hueco, sep, valor = m.groups()
            msg = ini + c(TENUE, clave + hueco + ":") + sep + c(CLARO, valor)
        else:
            msg = c(TENUE, msg)
    print(msg, file=sys.stderr, flush=True)


def firma_version():
    """Identifica el descompilador que produjo esta salida: md5 corto y fecha."""
    import hashlib
    partes = []
    for f in ("decompile.py", "port_bgd1to2.py", "resolve.py", "dcb.py", "mn.py"):
        ruta = os.path.join(os.path.dirname(os.path.abspath(__file__)), f)
        try:
            with open(ruta, "rb") as fh:
                partes.append(hashlib.md5(fh.read()).hexdigest())
        except OSError:
            partes.append("?")
    h = hashlib.md5("".join(partes).encode()).hexdigest()[:10]
    propio = os.path.join(os.path.dirname(os.path.abspath(__file__)), "decompile.py")
    try:
        fecha = time.strftime("%Y-%m-%d %H:%M", time.localtime(os.path.getmtime(propio)))
    except OSError:
        fecha = "?"
    return "dcb2prg %s  (decompile.py de %s)" % (h, fecha)


def ayuda():
    """Menu de ayuda. Se muestra al invocar sin argumentos."""
    t = lambda x: c(FUERTE + ";" + CIAN, x)
    o = lambda x: c(VERDE, x)
    g = lambda x: c(TENUE, x)
    return "\n".join([
        t("dcb2prg") + g("  -  descompilador de .dcb de BennuGD a fuente .prg"),
        "",
        t("USO"),
        "  decompile.py " + o("-o") + " <salida.prg> [opciones] <entrada.dcb> [PROCESO]",
        "",
        t("OBLIGATORIO"),
        "  " + o("-o <salida.prg>") + "   donde escribir el fuente. Con " + o("-o -") + " sale por stdout,",
        "                    para encadenar con una tuberia.",
        "",
        t("OPCIONES"),
        "  " + o("--bgd1") + "            porta el fuente a BennuGD1. Solo tiene sentido desde",
        "                    un .dcb de Fenix: escribe los import que BennuGD",
        "                    necesita (Fenix no tiene modulos) y avisa de lo que",
        "                    no tenga equivalente.",
        "  " + o("--bgd2") + "            ademas de descompilar, porta el fuente a BennuGD2:",
        "                    renombra las funciones, inyecta los shims de",
        "                    compatibilidad y emula las paletas de 8 bits.",
        "  " + o("PROCESO") + "           descompila solo ese proceso, en vez del programa entero.",
        "  " + o("--extraer[=DIR]") + "   vuelca los assets incrustados en el .dcb. Sin DIR van a",
        "                    la carpeta del .prg, que es donde el juego los busca",
        "                    (dentro llevan sus rutas: data/, mod/...). Si no se dice",
        "                    nada y hay assets, se pregunta (solo si es un terminal).",
        "  " + o("--no-extraer") + "      no preguntar ni extraer nada.",
        "",
        t("ENTORNO"),
        "  " + o("NO_COLOR") + "          apaga el color de la traza (manda sobre FORCE_COLOR).",
        "  " + o("FORCE_COLOR") + "       lo enciende aunque la salida no sea un terminal.",
        "",
        t("EJEMPLOS"),
        "  decompile.py -o juego.prg juego.dcb",
        g("      recupera el fuente en el dialecto original de BennuGD1"),
        "  decompile.py -o juego2.prg --bgd2 juego.dcb",
        g("      y ademas lo porta a BennuGD2"),
        "  decompile.py -o - juego.dcb ENEMIGO | less",
        g("      mira un solo proceso por pantalla"),
        "",
        t("HERRAMIENTAS DEL PAQUETE"),
        "  " + o("disasm.py") + " <dcb> [PROCESO]      desensamblado crudo, con offsets",
        "  " + o("dcb.py") + " <dcb>                   resumen del contenedor",
        "  " + o("dcb.py") + " --extract <dcb> <dir>   vuelca los assets incrustados",
        "  " + o("verificar.sh") + " <dcb> <bgdc> [portado.dcb]",
        g("      pasa todas las comprobaciones estaticas de una vez"),
        "  " + o("verificar_roundtrip.py") + " <dcb> <bgdc>",
        g("      recompila la salida y la compara con el original"),
        "  " + o("verificar_portado.py") + " <orig.dcb> <portado.dcb>",
        g("      compara el portado a BennuGD2 con el original"),
        "  " + o("jugar.sh") + " <juego.dcb>            lanza el juego desde SU carpeta",
        g("      (bgdi resuelve las rutas desde el directorio de trabajo, asi que"),
        g("       desde otro sitio no encuentra los assets y se cierra al arrancar)"),
        "  " + o("vigilar_atascos.py") + " <juego.dcb>",
        g("      busca procesos atascados en el juego en marcha"),
        "  " + o("construir_bennugd1_32.sh") + " <dir>",
        g("      compila el motor de BennuGD1 de 32 bits, que hace falta"),
        g("      para verificar el round-trip"),
    ])


def assets(d, entrada, destino, extraer):
    """Los .dcb suelen llevar dentro los graficos, sonidos y fuentes del juego.

    Recuperar el fuente sin ellos deja un proyecto que no arranca, y es facil no
    enterarse de que estaban ahi. Asi que se avisa siempre, y se extraen si se
    pide (--extraer) o si el usuario dice que si. Cuando no hay terminal (un
    script, un CI) no se pregunta: se avisa y sigue, para no bloquearse.

    Van a la carpeta del .prg, NO a una subcarpeta: dentro del .dcb las rutas ya
    son las relativas que usa el juego ("data/adam.fpg", "mod/..."), y el
    programa las abre tal cual desde su directorio de trabajo. Metiendolas en un
    "juego_assets/" habria que moverlas a mano para que el juego arrancase.
    """
    try:
        fs = d.embedded_files()
    except Exception:
        return 0
    if not fs:
        return 0
    total = sum(f[2] for f in fs)
    carpetas = sorted({(n.replace("\\", "/").split("/")[0] + "/")
                       for n, _, _, _ in fs if "/" in n.replace("\\", "/")})
    paso("   incrustados: %s   %s   %s"
         % (c(CLARO, "%d ficheros" % len(fs)),
            c(TENUE, "%.1f MB" % (total / 1048576.0)),
            c(TENUE, " ".join(carpetas))), "crudo")

    if extraer is False:
        return 0

    # destino por defecto: donde va a quedar el .prg, que es desde donde se
    # ejecutara el juego
    if extraer:
        raiz = extraer
    else:
        base = destino if destino and destino != "-" else entrada
        raiz = os.path.dirname(os.path.abspath(base)) or "."

    pisa = 0
    for n, _, _, _ in fs:
        rel = n.replace("\\", "/").lstrip("/")
        while rel.startswith("../"):
            rel = rel[3:]
        if os.path.exists(os.path.join(raiz, rel)):
            pisa += 1

    if extraer is None:
        if not (hasattr(sys.stdin, "isatty") and sys.stdin.isatty()):
            paso("   (usa --extraer para volcarlos, o --no-extraer para callar esto)")
            return 0
        # si hay que sobreescribir, lo prudente es que el "no" sea el defecto
        aviso = ""
        if pisa:
            aviso = c(AMBAR, " sobreescribe %d ya existentes" % pisa)
        sys.stderr.write("   %s %s%s %s "
                         % (c(AMBAR, "¿extraer en"), c(CLARO, raiz),
                            aviso, "[s/N]" if pisa else "[S/n]"))
        sys.stderr.flush()
        try:
            r = sys.stdin.readline().strip().lower()
        except (EOFError, KeyboardInterrupt):
            sys.stderr.write("\n")
            return 0
        if pisa:
            if not r or r[0] != "s":
                return 0
        elif r and r[0] == "n":
            return 0

    n = d.extract(raiz)
    paso("   extraidos : %s en %s" % (c(CLARO, "%d ficheros" % n), c(CLARO, raiz)),
         "crudo")
    return n


def main():
    t0 = time.time()
    args = sys.argv[1:]
    to_bgd2 = "--bgd2" in args
    to_bgd1 = "--bgd1" in args
    args = [a for a in args if a not in ("--bgd2", "--bgd1")]
    if to_bgd1 and to_bgd2:
        sys.exit("--bgd1 y --bgd2 son destinos distintos: elige uno")

    # -o es obligatorio: escribir un fuente de varios MB por stdout sin querer,
    # o perderlo por olvidar el redirector, es demasiado facil.
    destino = None
    extraer = None            # None = preguntar, False = no, str = directorio
    argv = []
    i = 0
    while i < len(args):
        if args[i] == "--no-extraer":
            extraer = False
            i += 1
            continue
        if args[i] == "--extraer":
            extraer = ""      # directorio por defecto
            i += 1
            continue
        if args[i].startswith("--extraer="):
            extraer = args[i][10:]
            i += 1
            continue
        if args[i] == "-o":
            if i + 1 >= len(args):
                paso("falta el nombre del fichero tras -o", "malo")
                paso(ayuda(), "crudo")
                return 2
            destino = args[i + 1]
            i += 2
            continue
        if args[i].startswith("-o") and len(args[i]) > 2:
            destino = args[i][2:]
            i += 1
            continue
        argv.append(args[i])
        i += 1

    if not argv or destino is None:
        if argv and destino is None:
            paso("falta -o <salida.prg>: hay que decir donde va el fuente", "malo")
        paso(ayuda(), "crudo")
        return 2

    skip = set()
    paso("== leyendo %s ==" % argv[0], "titulo")
    d = DCB(argv[0])
    paso("   origen    : %s 0x%04X (%d bits, palabra de %d bytes)"
         % (d.motor, d.version, d.W * 8, d.W))
    paso("   procesos  : %d" % d.h["NProcs"])
    paso("   globales  : %d   locales: %d   varspaces: %d"
         % (d.h["NGloVars"], d.h["NLocVars"], d.h["NVarSpaces"]))
    paso("   cadenas   : %d   identificadores: %d" % (d.h["NStrings"], d.h["NID"]))
    paso("   imports   : %d   ficheros fuente: %d"
         % (d.h["NImports"], d.h["NSourceFiles"]))
    # El destino no se da por supuesto: sin --bgd2 se descompila al MISMO
    # dialecto del que viene, que puede ser Fenix, y con --bgd2 se porta.
    destino_motor = ("BennuGD2" if to_bgd2 else
                     "BennuGD1" if to_bgd1 else d.motor)
    portando = destino_motor != d.motor
    paso("   dialecto  : %s%s"
         % (destino_motor, " (portado desde %s)" % d.motor if portando
            else " (el mismo, sin portar)"))
    if to_bgd1 and d.motor == "BennuGD1":
        paso("   ya es BennuGD1: --bgd1 no tiene nada que hacer", "aviso")
    if to_bgd1 and d.motor == "BennuGD2":
        sys.exit("no se puede bajar un .dcb de BennuGD2 a BennuGD1: BennuGD2 usa "
                 "enteros de 64 bits y funciones que en BennuGD1 no existen")
    if to_bgd2 and d.motor == "BennuGD2":
        paso("   ya es BennuGD2: --bgd2 no tiene nada que hacer", "aviso")
    extraidos = assets(d, argv[0], destino, extraer)
    dc = Decompiler(d)
    if getattr(d, "fenix", False):
        # Pasada en seco para deducir la forma de los arrays y volver a empezar
        # con ella puesta (ver aplica_dims). Solo hace falta cuando el .dcb no
        # trae tabla de variables, que es el caso de todos los juegos de Fenix.
        pre = Decompiler(d)
        for _p in d.procs:
            try:
                pre.proc_source(_p)
            except Exception:
                pass
        n = pre.aplica_dims()
        if n:
            paso("   %d variables globales reconstruidas como arrays" % n)
        dc = Decompiler(d)
        # las dimensiones de las privadas hacen falta al DECLARARLAS, o sea
        # antes de recorrer el cuerpo: se traen de la pasada en seco
        dc.dims_uso = pre.dims_uso
    if to_bgd2:
        if Porter is None:
            sys.exit("falta port_bgd1to2.py")
        dc.porter = Porter()
        dc.trunc_locals = dc.porter.int_locals()
    want = argv[1].upper() if len(argv) > 1 else None
    lines = []
    if not want:
        lines.append("// Descompilado de %s" % argv[0])
        lines.append("// origen: %s 0x%04X   destino: %s"
                     % (d.motor, d.version, destino_motor))
        # Huella de la version que genero esto. Sin ella no hay forma de saber,
        # ante una salida pegada en un mensaje, con que decompile.py se hizo: ya
        # hemos perseguido dos veces "fallos" que eran de una copia anterior.
        lines.append("// %s" % firma_version())
        lines.append("// %d procesos, %d identificadores, %d cadenas" %
                     (d.h["NProcs"], d.h["NID"], d.h["NStrings"]))
        if d.srcfiles:
            lines.append("// Ficheros fuente originales: %s" % ", ".join(d.srcfiles))
        lines.append("")
        # Fenix no tiene modulos: lo lleva todo dentro del interprete, y por eso
        # sus .dcb traen NImports = 0. Al portarlo a BennuGD hay que averiguar
        # que modulos hacen falta a partir de las funciones que llama, o el
        # fuente no compila.
        if portando and d.fenix and portabilidad:
            _fn, _gl, _lo = dc.usa_del_motor()
            # Hacia BennuGD2 el portador RENOMBRA (load_fpg -> fpg_load, put ->
            # map_put...): hay que comprobar el nombre de llegada, no el de
            # salida, o se avisa de veinte funciones que si tienen equivalente.
            if dc.porter:
                _fn = {(dc.porter.map_func(f) or f): n for f, n in _fn.items()}
                _gl = {dc.porter.map_global(g) or g for g in _gl}
                _lo = {dc.porter.map_local(l) or l for l in _lo}
            _mods, _sinfn, _sing, _sinl, _otras = portabilidad(
                _fn, _gl, _lo, destino_motor, _fn)
            paso("== portando de %s a %s ==" % (d.motor, destino_motor), "titulo")
            paso("   %d funciones del sistema usadas, %d modulos de %s necesarios"
                 % (len(_fn), len(_mods), destino_motor))
            for _nom, _lista, _que in (("funciones", _sinfn, "no existen en %s" % destino_motor),
                                       ("funciones", _otras, "cambian de firma en %s" % destino_motor),
                                       ("globales", _sing, "no las declara %s" % destino_motor),
                                       ("locales", _sinl, "no las declara %s" % destino_motor)):
                if _lista:
                    paso("   %d %s que %s: %s"
                         % (len(_lista), _nom, _que, ", ".join(_lista[:8])
                            + (" ..." if len(_lista) > 8 else "")), "aviso")
            dc.sin_equivalente = {n.upper() for n in _sing + _sinl}
            if not (_sinfn or _sing or _sinl or _otras):
                paso("   todo lo que usa tiene equivalente: el portado es completo")
            else:
                paso("   el portado NO es completo: el fuente que sale no compila"
                     " tal cual, hay que resolver lo de arriba", "aviso")
                if destino_motor == "BennuGD2":
                    paso("   suele salir mejor portar primero a BennuGD1 (--bgd1),"
                         " que de Fenix va directo", "aviso")
            lines.append("// --- PORTADO DE %s A %s ---" % (d.motor, destino_motor))
            if _sinfn or _sing or _sinl or _otras:
                lines.append("// OJO: el portado NO esta completo. Esto hay que"
                             " resolverlo a mano y hasta entonces no compila:")
                for _nom, _lista in (("funciones que no existen", _sinfn),
                                     ("funciones que cambian de firma", _otras),
                                     ("globales que no declara el motor", _sing),
                                     ("locales que no declara el motor", _sinl)):
                    if _lista:
                        lines.append("//   %s: %s" % (_nom, ", ".join(_lista)))
            else:
                lines.append("// Todo lo que el programa usa del motor tiene"
                             " equivalente en %s." % destino_motor)
            lines.append("")
            for _m in _mods:
                lines.append('import "%s";' % _m)
            if _mods:
                lines.append("")
        # la tabla OImports indexa la de cadenas: da los módulos enlazados
        if d.h["NImports"] and not to_bgd2:
            names = [d.strings[i] for i in d.w(d.h["OImports"], d.h["NImports"])
                     if i < len(d.strings)]
            # BennuGD1 llama a sus modulos "mod_*" y BennuGD2 "libmod_*"; el
            # resto (libbggfx, libbgload...) son librerias internas del motor que
            # se arrastran solas y no se importan.
            def _es_modulo(n):
                return n.startswith("mod_") or n.startswith("libmod_")
            mods = [n for n in names if _es_modulo(n)]
            libs = [n for n in names if not _es_modulo(n)]
            for m in mods:
                lines.append('import "%s";' % m)
            if libs:
                lines.append("// dependencias arrastradas: %s" % ", ".join(libs))
            lines.append("")
        if to_bgd2:
            lines.append("// --- PORTADO A BennuGD2 ---")
            lines.append("// __IMPORTS__")
            lines.append("// __PORTREPORT__")
            lines.append("")
        # las globales del motor las declara BennuGD: redeclararlas es un error
        skip = Porter().engine_globals() if Porter else set()
        # las del motor de Fenix tampoco se declaran: las pone el interprete
        _mf = getattr(dc, "motor_fenix", set())
        skip |= _mf | {n.split(".")[0].split("[")[0] for n in _mf}
        # lo que el motor de DESTINO no trae lo tiene que declarar el programa
        skip -= getattr(dc, "sin_equivalente", set())
        propias = [v for v in d.glovar if v.name.upper() not in skip]
        omitidas = len(d.glovar) - len(propias)
        if omitidas:
            lines.append("// %d globales del motor omitidas (las declara %s)"
                         % (omitidas, destino_motor))
        lines.append("// __TIPOS__")
        lines.append("GLOBAL")
        if Porter:
            _pg = Porter().pal_globals()
        dc.registrar_tipos()
        dc.scan_disk_vars()
        dc.scan_remote_public()
        # los inicializadores hay que calcularlos ANTES de emitir el bloque
        # GLOBAL: los arrays de escalares van en la propia declaracion
        inits = dc.global_inits(skip)
        dc.needs_init = bool(inits)
        if dc.disk_vars:
            lines.append("// %d variables se leen/escriben en disco y conservan el"
                         " ancho de 32 bits de %s:" % (len(dc.disk_vars), d.motor))
            lines.append("//     " + ", ".join(sorted(dc.disk_vars)))
        dc.decl_space, dc.decl_owner = "GLOBAL", -1
        lines += dc.decl_lines(propias, None, inits=getattr(dc, "glo_arrays", {}))
        lines.append("// __PALGLOBALS__")
        lines.append("// __STUBS__")
        lines.append("// __UNRESOLVED__")
        lines.append("END")
        lines.append("")
        lines.append("// __LOCALS__")
        # el prelude va DESPUÉS del bloque GLOBAL: BennuGD exige que las
        # globales se declaren antes que cualquier proceso o función
        lines.append("// __PRELUDE__")
        lines.append("// __INITS__")
        lines.append("// __DECLARES__")
        lines.append("")
    paso("== recuperando valores iniciales de las globales ==", "titulo")
    if want:
        inits = dc.global_inits(skip)
        dc.needs_init = bool(inits)
    paso("   %d globales con valor inicial, %d arrays en la declaracion"
         % (len(inits), len(getattr(dc, "glo_arrays", {}))))

    objetivo = [p for p in d.procs if not want or p.name == want]
    paso("== descompilando %d procesos ==" % len(objetivo), "titulo")
    total = len(objetivo)
    for n, p in enumerate(objetivo, 1):
        antes = dc.warn
        clase = "PROCESS" if p.d["Flags"] & 1 else "FUNCTION"
        ni = len(p.code) // 2
        # indice en gris, PROCESS/FUNCTION en cian, nombre resaltado, y el
        # numero de instrucciones en ambar si el proceso es de los enormes
        paso("   " + c(TENUE, "[%*d/%d]" % (len(str(total)), n, total))
             + " " + c(CIAN, clase) + " " * max(1, 9 - len(clase))
             + c(FUERTE, p.name) + " " * max(1, 29 - len(p.name))
             + c(AMBAR if ni >= 10000 else TENUE, "%6d" % ni)
             + c(TENUE, " instrucciones"), "crudo")
        lines += dc.proc_source(p)
        if dc.warn > antes:
            paso("            ^^ %d instrucciones sin traducir" % (dc.warn - antes), "aviso")
    # Solo se declaran los offsets sin nombre que el codigo llega a usar: los
    # demas son restos de expresiones descartadas y solo inflarian NGloVars.
    cuerpo = "\n".join(lines)
    # OJO con la caja: los nombres se generan en MAYUSCULAS y el fuente sale en
    # minusculas, asi que buscarlos tal cual no encontraba ninguno y la
    # declaracion se quedaba vacia (en un .dcb de Fenix sin depuracion eso son
    # TODAS las variables del programa).
    dc.calcula_dims()
    bajo = cuerpo.lower()
    # solo las del espacio GLOBAL: las privadas sin nombre las declara cada
    # proceso en su propio bloque PRIVATE (si no, salen declaradas dos veces)
    usados = {k: v for k, v in dc.unresolved_vars.items()
              if "espacio GLOBAL" in v and bajo.count(k.lower()) > 0}
    if usados:
        u = ["    // offsets sin variable declarada: revisar",
             ] + ["    INT %s%s;  // %s"
                  % (dc.minus(k), dc.dims_uso.get(k.upper(), ""), v)
                  for k, v in sorted(usados.items())]
    else:
        u = []
    lines = [l.replace("// __UNRESOLVED__", "\n".join(u)) for l in lines]
    if not to_bgd2:
        lines = [l.replace("// __PALGLOBALS__", "") for l in lines]
    # Declaraciones adelantadas de todos los procesos.
    # BennuGD2 pasó x/y/z/size de INT a DOUBLE. Si un proceso se llama antes de
    # declararse, bgdc fija el tipo del parámetro por los argumentos y luego
    # choca con el tipo real de la local (c_main.c:918). Declarar primero, con
    # los tipos explícitos, evita las dos cosas.
    # Y en BennuGD1 son ademas OBLIGATORIAS para todo proceso con variables
    # PUBLIC: es el unico sitio donde se pueden declarar de forma que "otro.x"
    # las vea (ver scan_remote_public).
    ENTEROS = ("INT", "DWORD", "BYTE", "WORD", "SHORT", "SBYTE", "QWORD")

    def _con_tipo(head):
        """La firma lleva algun parametro con tipo que bgdc no pueda inferir."""
        pars = head[head.index("(") + 1:head.rindex(")")].strip()
        for pr in pars.split(","):
            pr = pr.strip()
            if not pr:
                continue
            t = pr.split()[0].upper()
            if len(pr.split()) > 1 and (t not in ENTEROS or "*" in pr):
                return True
        return False

    decls = []
    for l in list(lines) + dc.sin_cuerpo:
        if l.startswith(("PROCESS ", "FUNCTION ")) and l.rstrip().endswith(")"):
            pub = dc.pub_bloque.get(l.strip())
            # En BennuGD1 tambien hacen falta: si un proceso se LLAMA antes de
            # definirse, bgdc fija el tipo de cada parametro por los argumentos
            # de esa llamada y luego choca con el declarado
            # ("Parameter type invalid", c_main.c:1083). Solo se libran los que
            # no tienen parametros con tipo propio.
            if not to_bgd2 and not pub and not _con_tipo(l) \
                    and l not in dc.sin_cuerpo:
                continue
            decls += ["DECLARE " + l.strip()] + (pub or []) + ["END"]
    if decls:
        decls = ["// declaraciones adelantadas: ver nota en el README"] + decls + [""]
    # Los TYPE con nombre solo se conocen tras recorrer todos los procesos: se
    # registran al declarar los parametros que son punteros a struct.
    # Los TYPE se emiten en orden de DEPENDENCIA: si tipo_A tiene un miembro de
    # tipo_B, tipo_B va antes. Un tipo puede referirse a SI MISMO (una lista
    # enlazada) y eso BennuGD lo acepta, asi que la autorreferencia no cuenta.
    def _deps(ms):
        out = set()
        if ms < len(d.varspaces):
            for v in d.varspaces[ms]:
                if (v.members is not None and v.members != ms
                        and v.members in dc.tipos_nombrados):
                    out.add(v.members)
        return out

    orden, vistos = [], set()
    def _visita(ms, pila):
        if ms in vistos or ms in pila:
            return
        pila.add(ms)
        for dep in sorted(_deps(ms)):
            _visita(dep, pila)
        pila.discard(ms)
        vistos.add(ms)
        orden.append(ms)
    for ms in sorted(dc.tipos_nombrados):
        _visita(ms, set())

    tipos = []
    for ms in orden:
        nom = dc.tipos_nombrados[ms]
        tipos.append("TYPE %s" % nom)
        dc.pila_vs.add(ms)
        tipos += dc.decl_lines(d.varspaces[ms], None, 1, top=False)
        dc.pila_vs.discard(ms)
        tipos.append("END")
        tipos.append("")
    if tipos:
        tipos = ["// BennuGD pasa los structs por referencia y no admite",
                 "// \"STRUCT * p\" como parametro: hace falta un tipo con nombre.",
                 "//",
                 "// Los CAMPOS salen del .dcb: cada DCB_VAR del varspace lleva su",
                 "// identificador, su offset y sus chunks de tipo. Lo que NO guarda el",
                 "// .dcb es QUE nombre de TYPE le correspondia a cada varspace:",
                 "// DCB_VARSPACE es solo {NVars, OVars}, sin campo de nombre. El nombre",
                 "// original si sobrevive en la tabla de identificadores, pero mezclado",
                 "// con las palabras reservadas y los #define y sin ningun enlace al",
                 "// varspace, asi que no se puede saber cual es. De ahi \"tipo_N\":",
                 "// renombralo a mano si sabes cual era."] + tipos
    lines = [l.replace("// __TIPOS__", "\n".join(tipos)) for l in lines]
    lines = [l.replace("// __DECLARES__", "\n".join(decls)) for l in lines]
    text = "\n".join(lines)
    # Locales declaradas por el PROGRAMA, no por el motor. Sin su bloque LOCAL,
    # un acceso remoto "otro.saved_status" no compila: bgdc busca el nombre en
    # el espacio LOCAL (c_code.c:1815), que es donde tiene que estar.
    motor_loc = set()
    if Porter:
        _pt = Porter()
        motor_loc = ({k.upper() for k in _pt.v1.get("locals", {})} |
                     {k.upper() for k in _pt.v2.get("locals", {})})
    dc.decl_space, dc.decl_owner = "LOCAL", -1
    _mf = getattr(dc, "motor_fenix", set())
    motor_loc |= _mf | {n.split(".")[0].split("[")[0] for n in _mf}
    motor_loc -= getattr(dc, "sin_equivalente", set())
    propias_loc = [v for v in d.locvar if v.name.upper() not in motor_loc
                   and re.search(r"\b%s\b" % re.escape(dc.minus(v.name)), text)]
    if to_bgd2:
        # los imports sólo se conocen tras recorrer todo el código
        imps = "\n".join('import "%s";' % m for m in dc.porter.imports())
        text = text.replace("// __IMPORTS__", imps or "// (sin módulos detectados)")
        text = text.replace("// __PORTREPORT__", "\n".join(dc.porter.report()))
        # La profundidad de color del programa original decide como convertir lo
        # que devolvia map_get_pixel(). Se detecta del propio fuente; si no dice
        # nada, se queda el 16 por defecto de BennuGD1.
        # El shim de load_fpg solo tiene sentido si el juego usa paletas: lo que
        # aporta es registrar la paleta original de cada FPG para el shader. Si no
        # hay paletas, se deja la llamada directa y el shim no se emite.
        if not dc.porter.pal_usado:
            text = text.replace("bgd1_load_fpg(", "fpg_load(")
            dc.porter.shims_used.pop("bgd1_load_fpg", None)

        m = re.search(r"\bGRAPH_MODE\s*=\s*(8|16|32)\b", text)
        if m:
            dc.porter.map_depth = int(m.group(1))
        # PALETTE es la unica local de BennuGD1 con efecto de RENDER: significa
        # "esta instancia se dibuja con esta paleta". En BennuGD2 el equivalente
        # es el shader, que tambien es local por proceso, asi que tras cada
        # asignacion a PALETTE se sincroniza el shader de intercambio de paleta.
        # Tiene que ir en linea: una FUNCTION no puede tocar las locales de quien
        # la llama.
        if dc.porter.pal_usado:
            text, n_iny = re.subn(
                r"^([ \t]*)(PALETTE\s*=[^;]+;)[ \t]*$",
                r"\1\2\n\1shader_params = bgd1_pal_params(BGD1_BASE[file], PALETTE);"
                r"\n\1IF (shader_params != 0) shader = bgd1_pal_shader();"
                r" ELSE shader = NULL; END",
                text, flags=re.M)
            paso("   shader de paleta inyectado en %d asignaciones a PALETTE" % n_iny)

        text = text.replace("// __PRELUDE__", "\n".join(dc.porter.prelude()))
        text = text.replace("// __PALGLOBALS__", "\n".join(dc.porter.pal_globals()))
        # Las locales del motor de BennuGD1 que BennuGD2 no aporta se declaran en
        # un bloque LOCAL del programa, NO como globales: cada instancia necesita
        # su propia copia. Como globales, todas compartirian valor y la logica del
        # juego se rompe (en Streets of Rage, PALETTE global hacia que todos los
        # enemigos compartiesen paleta y fallasen los "IF (PALETTE == ...)").
        faltan = [n for n in dc.porter.missing_locals([v.name for v in d.locvar])
                  if re.search(r"\b%s\b" % re.escape(n), text)]
        if faltan:
            blk = ["// locales del motor que BennuGD2 ya no trae: cada instancia",
                   "// necesita la suya, declararlas como globales rompe el juego",
                   "LOCAL"] + ["    INT %s;" % dc.safe_name(n) for n in faltan] \
                + dc.decl_lines(propias_loc) + ["END", ""]
            text = text.replace("// __LOCALS__", "\n".join(blk))
            quitar = {n.upper() for n in faltan}
            dc.porter.stub_vars = {k: v for k, v in dc.porter.stub_vars.items()
                                   if k.upper() not in quitar}
        elif propias_loc:
            text = text.replace("// __LOCALS__", "\n".join(
                ["LOCAL"] + dc.decl_lines(propias_loc) + ["END", ""]))
        else:
            text = text.replace("// __LOCALS__", "")

        stubs = dc.porter.stub_decls()
        text = text.replace("// __STUBS__", "\n".join(stubs) if stubs else "")
    if not to_bgd2:
        text = text.replace("// __LOCALS__", "\n".join(
            ["LOCAL"] + dc.decl_lines(propias_loc) + ["END", ""])
            if propias_loc else "")
    if inits:
        blk = ["// Valores iniciales de las globales, tal como los guarda el",
               "// .dcb. MAIN llama a esto antes que a nada.",
               "FUNCTION %s()" % dc.nombre_init, "BEGIN"] + inits + ["END", ""]
        text = text.replace("// __INITS__", "\n".join(blk))
    else:
        text = text.replace("// __INITS__", "")
    paso("== escribiendo fuente ==", "titulo")
    # Los marcadores que solo se sustituyen al portar a BennuGD2 no pintan nada
    # en el fuente de BennuGD1, y las sustituciones por cadena vacia dejan
    # lineas en blanco de sobra. Fuera ambas cosas.
    salida, blancos = [], 0
    for l in text.split("\n"):
        if re.fullmatch(r"\s*// __[A-Z_]+__", l):
            continue
        blancos = blancos + 1 if not l.strip() else 0
        if blancos > 1:
            continue
        salida.append(l)
    text = "\n".join(salida)
    # Las cadenas del .dcb se leen como latin-1; escribir la salida en UTF-8
    # convertia cada byte alto en dos y corrompia las eñes y los acentos
    # ("CUMPLEA\xa5OS" salia como "CUMPLEA\xc2\xa5OS"), de modo que el .prg
    # recompilado ya no contenia las mismas cadenas que el original. Se escribe
    # en el mismo latin-1 del que se leyo, byte a byte.
    crudo = text.encode("latin-1", "replace") + b"\n"
    if destino == "-":
        sys.stdout.buffer.write(crudo)
        sys.stdout.buffer.flush()
    else:
        with open(destino, "wb") as fh:
            fh.write(crudo)
    paso("   fichero   : %s" % ("(stdout)" if destino == "-" else destino))
    paso("   %d lineas, %d caracteres" % (text.count("\n") + 1, len(text)))
    paso("== resumen ==", "titulo")
    # Se cuenta sobre el TEXTO, no con el contador interno: el sondeo de la
    # cadena de casos descarta lo que emite y el contador seguia sumandolo (156
    # "sin traducir" cuando en el fichero solo quedaban 2).
    sin_traducir = text.count("// ?? ")
    paso("   " + c(TENUE, "instrucciones sin traducir:") + " "
         + c(VERDE if not sin_traducir else ROJO, str(sin_traducir)), "crudo")
    if dc.planos:
        paso("   " + c(TENUE, "flujo no estructurable (etiquetas y goto):") + " "
             + c(AMBAR, "%d procesos" % len(dc.planos)) + " "
             + c(TENUE, ", ".join(sorted(dc.planos)[:6])), "crudo")
    huecos = text.count(FALTA)
    paso("   " + c(TENUE, "expresiones sin reconstruir:") + " "
         + c(VERDE if not huecos else ROJO, str(huecos))
         + (c(ROJO, "   <-- el fuente NO va a compilar, busca %s" % FALTA)
            if huecos else ""), "crudo")
    if dc.unresolved_vars:
        # Lo normal es que sean el descriptor de tipos que el compilador genera
        # como argumento extra de save()/load(): no son variables del juego y
        # bgdc los vuelve a generar solo, asi que no se declaran. Si alguno SI
        # aparece en el codigo, entonces si es una variable que no supimos
        # resolver, y eso hay que mirarlo.
        paso("   offsets del espacio global sin nombre: %d "
             "(argumento oculto de save/load, lo regenera bgdc)"
             % len(dc.unresolved_vars))
        if usados:
            paso("   ATENCION: %d de ellos SI se usan en el codigo: son variables"
                 " sin resolver, revisar" % len(usados))
            for k in sorted(usados)[:10]:
                paso("      %s" % k)
    if to_bgd2:
        paso("   portado a BennuGD2: %d simbolos renombrados, %d sin equivalente"
             % (len(dc.porter.renamed), len(dc.porter.unresolved)))
    paso("   tiempo    : %.1f s" % (time.time() - t0))
    paso("   listo.", "ok")


if __name__ == "__main__":
    sys.exit(main() or 0)
