"""Resolución de offsets de byte a nombres de variable (incl. structs y arrays)."""
from dcb import BASETYPES


class Resolver:
    def __init__(self, dcb):
        self.d = dcb
        self._vs_size = {}
        self.TS = dcb.typesize
        self.W = dcb.W
        self.BT = dcb.basetypes
        self.TYPE_ARRAY, self.TYPE_STRUCT = dcb.T_ARRAY, dcb.T_STRUCT
        self.TYPE_POINTER = 19 if dcb.W == 4 else 15

    def chunk_size(self, chunks, i=0):
        if i >= len(chunks):
            return self.W
        t, c = chunks[i]
        if t == self.TYPE_ARRAY:
            return c * self.chunk_size(chunks, i + 1)
        if t == self.TYPE_STRUCT:
            return 0  # lo rellena el llamante con el varspace
        return self.TS.get(t, self.W)

    def var_size(self, v):
        return self._size(v.chunks, 0, v.members)

    def _size(self, chunks, i, members):
        if i >= len(chunks):
            return self.W
        t, c = chunks[i]
        if t == self.TYPE_ARRAY:
            return c * self._size(chunks, i + 1, members)
        if t == self.TYPE_STRUCT:
            return self.varspace_size(members)
        return self.TS.get(t, self.W)

    def varspace_size(self, idx):
        if idx in self._vs_size:
            return self._vs_size[idx]
        if idx == 0xFFFFFFFF or idx >= len(self.d.varspaces):
            return self.W
        self._vs_size[idx] = self.W  # corta recursión
        vs = self.d.varspaces[idx]
        total = 0
        for v in vs:
            total = max(total, v.offset + self.var_size(v))
        self._vs_size[idx] = total or self.W
        return self._vs_size[idx]

    def name_at(self, varlist, off):
        """Devuelve la expresión textual de la variable en el offset dado."""
        best = None
        for v in varlist:
            sz = self.var_size(v)
            if v.offset <= off < v.offset + sz:
                if best is None or v.offset > best[0].offset:
                    best = (v, sz)
        if best is None:
            return None
        v, sz = best
        return v.name + self._path(v.chunks, 0, v.members, off - v.offset)

    def var_at(self, varlist, off):
        """La variable que cubre el offset, y el desplazamiento dentro de ella.

        Hace falta para poder seguir resolviendo CAMPOS de struct cuando el
        compilador emite el acceso en varios pasos (ARRAY para el indice
        dinamico y luego INDEX con el offset del campo).
        """
        best = None
        for v in varlist:
            sz = self.var_size(v)
            if v.offset <= off < v.offset + sz:
                if best is None or v.offset > best.offset:
                    best = v
        return (best, off - best.offset) if best else (None, 0)

    def path_struct(self, varlist, off, ms):
        """Camino textual hasta el struct del varspace `ms` que empieza en `off`.

        Al pasar un struct por parametro el compilador emite la direccion de su
        PRIMER escalar, asi que "mitorreta.arma" y "mitorreta.arma.velocidad"
        son el mismo offset. Quitar todos los campos del final dejaba
        "mitorreta", que es otro struct y otro parametro.
        """
        v, rel = self.var_at(varlist, off)
        if v is None:
            return None
        return self._pstruct(v.name, v.chunks, 0, v.members, rel, ms)

    def _pstruct(self, texto, chunks, i, members, rel, ms):
        while i < len(chunks) and chunks[i][0] == self.TYPE_ARRAY:
            esz = self._size(chunks, i + 1, members)
            if esz <= 0:
                return None
            idx, rel = divmod(rel, esz)
            texto += "[%d]" % idx
            i += 1
        if i >= len(chunks) or chunks[i][0] != self.TYPE_STRUCT:
            return None
        if members == ms and rel == 0:
            return texto
        if members == 0xFFFFFFFF or members >= len(self.d.varspaces):
            return None
        best = None
        for x in self.d.varspaces[members]:
            if x.offset <= rel < x.offset + self.var_size(x):
                if best is None or x.offset > best.offset:
                    best = x
        if best is None:
            return None
        return self._pstruct(texto + "." + best.name, best.chunks, 0,
                             best.members, rel - best.offset, ms)

    def leaf_at(self, varlist, off):
        """La variable-hoja (miembro final) que empieza justo en `off`.

        Hace falta para saber si el campo al que se llega es un PUNTERO a
        struct: "map_mapa.lronda.ultimo" se compila como GET_GLOBAL del campo
        puntero y luego INDEX, y sin reconocerlo salia "*(map_mapa.lronda + 8)".
        """
        v, rel = self.var_at(varlist, off)
        while v is not None:
            ch, i, mem = v.chunks, 0, v.members
            while i < len(ch) and ch[i][0] == self.TYPE_ARRAY:
                esz = self._size(ch, i + 1, mem)
                if esz <= 0:
                    return None
                rel %= esz
                i += 1
            if i >= len(ch) or ch[i][0] != self.TYPE_STRUCT:
                # solo vale si el offset cae EXACTAMENTE en la hoja: a media
                # variable no hay campo que nombrar
                return v if rel == 0 else None
            if mem == 0xFFFFFFFF or mem >= len(self.d.varspaces):
                return None
            v, rel = self.var_at(self.d.varspaces[mem], rel)
        return None

    def desc_at(self, varlist, off):
        """Nivel del tipo donde cae la PRIMERA dimension de array al llegar a `off`.

        Cuando el compilador pliega los indices constantes 0 de la izquierda, un
        acceso como "gui.objetivos[i].enable" sale del bytecode como
        GLOBAL <offset del primer escalar>; ARRAY <paso>; INDEX <campo>. Con
        solo el nombre del escalar no hay forma de rehacerlo: el ARRAY acababa
        indexando el escalar ("...estado[15]", que ni es un array) en vez del
        struct que lo contiene. Esto devuelve (chunks, members, nivel) del
        primer array del camino, que es donde el ARRAY tiene que morder.
        """
        v, rel = self.var_at(varlist, off)
        if v is None:
            return None
        return self._desc(v.chunks, 0, v.members, rel)

    def _desc(self, chunks, i, members, rel):
        while i < len(chunks):
            t, c = chunks[i]
            if t == self.TYPE_ARRAY:
                return (chunks, members, i)
            if t != self.TYPE_STRUCT:
                return None
            if members == 0xFFFFFFFF or members >= len(self.d.varspaces):
                return None
            best = None
            for x in self.d.varspaces[members]:
                if x.offset <= rel < x.offset + self.var_size(x):
                    if best is None or x.offset > best.offset:
                        best = x
            if best is None:
                return None
            chunks, members, rel, i = best.chunks, best.members, rel - best.offset, 0
        return None

    def path_desde(self, chunks, i, members, rel):
        """Camino textual (".campo", "[n]") desde un nivel concreto del tipo."""
        return self._path(chunks, i, members, rel)

    def _path(self, chunks, i, members, rel):
        if i >= len(chunks) or rel == 0 and (i >= len(chunks) or chunks[i][0] not in (self.TYPE_ARRAY, self.TYPE_STRUCT)):
            return "" if rel == 0 else "+%d" % rel
        t, c = chunks[i]
        if t == self.TYPE_ARRAY:
            esz = self._size(chunks, i + 1, members)
            if esz <= 0:
                return "+%d" % rel
            idx, r = divmod(rel, esz)
            return "[%d]" % idx + self._path(chunks, i + 1, members, r)
        if t == self.TYPE_STRUCT:
            if members == 0xFFFFFFFF or members >= len(self.d.varspaces):
                return "+%d" % rel if rel else ""
            sub = self.d.varspaces[members]
            best = None
            for v in sub:
                if v.offset <= rel < v.offset + self.var_size(v):
                    if best is None or v.offset > best.offset:
                        best = v
            if best is None:
                return "+%d" % rel if rel else ""
            return "." + best.name + self._path(best.chunks, 0, best.members, rel - best.offset)
        return "" if rel == 0 else "+%d" % rel

    def iter_scalars(self, varlist, prefix="", base=0):
        """Recorre todos los elementos escalares: (nombre, tipo, offset)."""
        for v in varlist:
            yield from self._iter(v.chunks, 0, v.members,
                                  prefix + v.name, base + v.offset)

    def _iter(self, chunks, i, members, name, off):
        if i >= len(chunks):
            return
        t, c = chunks[i]
        if t == self.TYPE_ARRAY:
            esz = self._size(chunks, i + 1, members)
            for k in range(c):
                yield from self._iter(chunks, i + 1, members,
                                      "%s[%d]" % (name, k), off + k * esz)
            return
        if t == self.TYPE_POINTER:
            yield name, t, off
            return
        if t == self.TYPE_STRUCT:
            if members == 0xFFFFFFFF or members >= len(self.d.varspaces):
                return
            for sv in self.d.varspaces[members]:
                yield from self._iter(sv.chunks, 0, sv.members,
                                      "%s.%s" % (name, sv.name), off + sv.offset)
            return
        yield name, t, off

    def name_at_top(self, varlist, off):
        """Nombre de la variable de primer nivel que empieza justo en `off`.

        Los parámetros 'V' reciben la variable entera (save/load de un struct
        completo). Si se baja al primer campo se guardan sólo sus bytes: era
        la causa de que las partidas salieran de 8 bytes.
        """
        for v in varlist:
            if v.offset == off:
                return v.name
        return None

    def scalar_size(self, varlist, off):
        """Tamaño del escalar de fondo en `off`, bajando dentro de structs."""
        best = None
        for v in varlist:
            if v.offset <= off < v.offset + self.var_size(v):
                if best is None or v.offset > best.offset:
                    best = v
        if best is None:
            return self.W
        return self._scal(best.chunks, 0, best.members, off - best.offset)

    def _scal(self, chunks, i, members, rel):
        while i < len(chunks) and chunks[i][0] == self.TYPE_ARRAY:
            esz = self._size(chunks, i + 1, members)
            if esz > 0:
                rel %= esz
            i += 1
        if i >= len(chunks):
            return self.W
        t = chunks[i][0]
        if t == self.TYPE_STRUCT:
            if members == 0xFFFFFFFF or members >= len(self.d.varspaces):
                return self.W
            return self.scalar_size(self.d.varspaces[members], rel)
        return self.TS.get(t, self.W)

    def type_decl(self, v):
        """Reconstruye 'INT x[10]', 'BYTE * p' o 'STRUCT ... END name'."""
        dims, stars = [], 0
        i = 0
        while i < len(v.chunks) and v.chunks[i][0] == self.TYPE_ARRAY:
            dims.append(v.chunks[i][1])
            i += 1
        # POINTER es un chunk delante del tipo apuntado: "BYTE * p"
        while i < len(v.chunks) and v.chunks[i][0] == self.TYPE_POINTER:
            stars += 1
            i += 1
        base = v.chunks[i][0] if i < len(v.chunks) else 0
        name = v.name + "".join("[%d]" % (n - 1) for n in dims)
        if stars:
            name = "*" * stars + " " + name
        if base == self.TYPE_STRUCT:
            return "STRUCT", name, v.members
        # puntero sin tipo apuntado declarado -> el genérico POINTER de BennuGD
        if stars and base in (0, self.TYPE_POINTER):
            return "POINTER", v.name + "".join("[%d]" % (n - 1) for n in dims), None
        return self.BT.get(base, 'UNDEF'), name, None
