#!/usr/bin/env python3
"""Parser del formato .dcb de BennuGD.

Soporta dos revisiones del mismo contenedor:
  * 0x0800 -- BennuGD2, campos de 64 bits.
  * 0x0710 -- BennuGD1, campos de 32 bits (misma disposición, palabra estrecha).
"""
import struct, sys, gzip, mmap

# Los mismos campos en ambas revisiones; sólo cambia el ancho de palabra.
HDR_FIELDS = ("Version NProcs NFiles NID NStrings NLocStrings NLocVars NGloVars NVarSpaces "
              "SGlobal SLocal SText NImports NSourceFiles NSysProcsCodes r1a r1b "
              "OProcsTab OID OStrings OText OGlobal OGloVars OLocal OLocVars OLocStrings "
              "OVarSpaces OFilesTab OImports OSourceFiles OSysProcsCodes r2a r2b").split()

PROC_FIELDS = ("ID Flags NParams NPriVars NPriStrings NPubVars NPubStrings NSentences "
               "SPrivate SPublic SCode OPrivate OPriVars OPriStrings OPublic OPubVars "
               "OPubStrings OCode OSentences OExitCode OErrorCode").split()

# Fenix, el antecesor: misma cabecera de fichero (donde BennuGD puso
# NSysProcsCodes/OSysProcsCodes, Fenix tenia reservados, y ocupan lo mismo) pero
# la de cada PROCESO acaba en OSentences: los manejadores ONEXIT/ONERROR son
# cosa de BennuGD. Y sus tipos caben en 6 trozos, no en 8.
PROC_FIELDS_V0 = PROC_FIELDS[:-2]

# Fenix 0.93 (DCB 0x0600) ya lleva ONEXIT: 20 campos. Le falta el OErrorCode de
# BennuGD1, que es de su manejo de errores.
PROC_FIELDS_V06 = PROC_FIELDS[:-1]

# Y las de Fenix VIEJO (0x0200 y anteriores) no tienen variables PUBLIC: son 13
# campos. Deducido del propio fichero: 280 + 38 procesos x 52 bytes = 2256, que
# es justo donde empieza la tabla de cadenas, y OPrivate + SPrivate == OCode en
# todos los procesos.
PROC_FIELDS_V00 = ("ID Flags NParams NPriVars NPriStrings NSentences SPrivate SCode "
                   "OPrivate OPriVars OPriStrings OCode OSentences").split()
MAX_TYPECHUNKS_V0 = 6

BASETYPES = ["UNDEF", "INT", "QWORD", "INT32", "DWORD", "SHORT", "WORD", "SBYTE", "BYTE",
             "CHAR", "FLOAT", "DOUBLE", "STRING", "ARRAY", "STRUCT", "POINTER"]
TYPE_ARRAY, TYPE_STRUCT, TYPE_STRING = 13, 14, 12

TYPESIZE = {1: 8, 2: 8, 3: 4, 4: 4, 5: 2, 6: 2, 7: 1, 8: 1, 9: 1, 10: 4, 11: 8, 12: 8, 15: 8}

# BennuGD1 usa códigos de tipo NO contiguos y distintos de los de BennuGD2.
# Deducidos del propio .dcb: 9=FLOAT (frame_time), 16=STRING (argv), 17=ARRAY,
# 18=STRUCT (mouse, fileinfo), 19=POINTER.
BASETYPES_V1 = {0: "UNDEF", 1: "INT", 2: "DWORD", 3: "SHORT", 4: "WORD", 5: "SBYTE",
                6: "BYTE", 8: "CHAR", 9: "FLOAT", 16: "STRING", 17: "ARRAY",
                18: "STRUCT", 19: "POINTER"}
TYPESIZE_V1 = {1: 4, 2: 4, 3: 2, 4: 2, 5: 1, 6: 1, 8: 1, 9: 4, 16: 4, 19: 4}
TYPE_ARRAY_V1, TYPE_STRUCT_V1, TYPE_STRING_V1 = 17, 18, 16

MAX_TYPECHUNKS = 8
DCB_MAGIC = b"dcb\r\n\x1f\x00\x00"


class Var:
    __slots__ = ("chunks", "members", "id", "offset", "varspace", "name")


class Proc:
    pass


class DCB:
    def __init__(self, path):
        fp = open(path, "rb")
        head = fp.read(2)
        fp.seek(0)
        if head == b"\x1f\x8b":
            self.raw = gzip.decompress(fp.read())
            fp.close()
        else:
            # mmap: estos ficheros pueden llevar cientos de MB de assets incrustados
            self.raw = mmap.mmap(fp.fileno(), 0, access=mmap.ACCESS_READ)
            self._fp = fp

        self.base = 0
        if self.raw[:8] != DCB_MAGIC:
            sig = self.raw[-20:]
            if sig[:11] == b"DCB Stub\x1a\r\n":
                self.base = struct.unpack("<q", sig[12:20])[0]
            else:
                idx = self.raw.find(DCB_MAGIC)
                if idx < 0:
                    raise SystemExit("no es un .dcb")
                self.base = idx

        # ¿64 o 32 bits? La versión de BennuGD2 es >= 0x0800 en un uint64.
        v64, = struct.unpack_from("<Q", self.raw, self.base + 8)
        if 0x0800 <= v64 < 0x10000:
            self.W, self.wf = 8, "Q"
            self.typesize = TYPESIZE
            self.basetypes = {i: n for i, n in enumerate(BASETYPES)}
            self.T_ARRAY, self.T_STRUCT, self.T_STRING = TYPE_ARRAY, TYPE_STRUCT, TYPE_STRING
            self.T_FLOAT, self.T_DOUBLE = 10, 11
        else:
            self.W, self.wf = 4, "I"
            self.typesize = TYPESIZE_V1
            self.basetypes = BASETYPES_V1
            self.T_ARRAY, self.T_STRUCT, self.T_STRING = (
                TYPE_ARRAY_V1, TYPE_STRUCT_V1, TYPE_STRING_V1)
            self.T_FLOAT, self.T_DOUBLE = 9, None   # BennuGD1 no tiene DOUBLE

        n = len(HDR_FIELDS)
        vals = struct.unpack_from("<%d%s" % (n, self.wf), self.raw, self.base + 8)
        self.h = dict(zip(HDR_FIELDS, vals))
        self.version = self.h["Version"]

        # Fenix llega hasta la 0x0500; BennuGD1 empieza en 0x0710.
        self.fenix = self.version < 0x0700
        # De donde viene el fichero, con nombre y apellidos: no es lo mismo un
        # .dcb de Fenix que uno de BennuGD1, aunque los dos sean de 32 bits.
        self.motor = ("Fenix" if self.fenix else
                      "BennuGD1" if self.W == 4 else "BennuGD2")
        self.chunks = MAX_TYPECHUNKS_V0 if self.fenix else MAX_TYPECHUNKS
        campos_proc = PROC_FIELDS_V0 if self.fenix else PROC_FIELDS
        if self.fenix:
            campos_proc = self._campos_proc_fenix()

        self.ID_SIZE = 60 + self.W
        # BaseType[K] + Count[K] + Members + ID + Offset + Varspace
        self.VAR_SIZE = self.chunks * (1 + self.W) + 4 * self.W
        self.PROC_SIZE = len(campos_proc) * self.W
        self.proc_fields = campos_proc

        self._read_ids()
        self._read_strings()
        self._read_varspaces()
        self._read_procs()
        self._read_srcfiles()

    def _campos_proc_fenix(self):
        """Cuál de las dos disposiciones de proceso usa este .dcb de Fenix.

        No basta con la version: entre 0x0200 y 0x0500 aparecieron las
        variables PUBLIC y la tabla creció de 13 campos a 19. Se prueban las dos
        y se queda la que da un reparto COHERENTE: el codigo de cada proceso
        tiene que caber en el fichero y empezar despues de la propia tabla.
        """
        n, o = self.h["NProcs"], self.h["OProcsTab"]
        if not n:
            return PROC_FIELDS_V0
        # Se prueba primero la que corresponde a la version, pero sin fiarse:
        # el reparto tiene que salir coherente. Fenix fue añadiendo campos
        # (0x0200: 13, sin PUBLIC; 0x0500: 19; 0x0600: 20, con ONEXIT).
        if self.version >= 0x0600:
            orden = (PROC_FIELDS_V06, PROC_FIELDS_V0, PROC_FIELDS_V00)
        elif self.version >= 0x0300:
            orden = (PROC_FIELDS_V0, PROC_FIELDS_V06, PROC_FIELDS_V00)
        else:
            orden = (PROC_FIELDS_V00, PROC_FIELDS_V0, PROC_FIELDS_V06)
        for campos in orden:
            paso = len(campos) * self.W
            fin = o + n * paso
            if fin > len(self.raw) - self.base:
                continue
            ok = True
            for i in range(n):
                d = dict(zip(campos, self.w(o + i * paso, len(campos))))
                if not (fin <= d["OCode"] and
                        d["OCode"] + d["SCode"] <= len(self.raw) - self.base):
                    ok = False
                    break
            if ok:
                return campos
        return PROC_FIELDS_V0

    # ---- lectura de palabras ----
    def w(self, off, n=1):
        return struct.unpack_from("<%d%s" % (n, self.wf), self.raw, self.base + off)

    def _read_ids(self):
        self.idname = {}
        o = self.h["OID"]
        for _ in range(self.h["NID"]):
            name = bytes(self.raw[self.base + o:self.base + o + 60]).split(b"\0")[0]
            code, = self.w(o + 60)
            self.idname[code] = name.decode("latin-1")
            o += self.ID_SIZE

    def _read_strings(self):
        n = self.h["NStrings"]
        offs = self.w(self.h["OStrings"], n) if n else ()
        b = self.base + self.h["OText"]
        data = bytes(self.raw[b:b + self.h["SText"]])
        self.strings = []
        for so in offs:
            e = data.find(b"\0", so)
            # Las cadenas del .dcb estan en la pagina de codigos DOS: el bgdc de
            # BennuGD convierte el fuente (latin-1) a cp850 al compilar. Leerlas
            # como latin-1 y volver a escribirlas hacia que "CUMPLEAÑOS" acabase
            # siendo otra cosa al recompilar, porque el byte pasaba por la
            # conversion una segunda vez.
            self.strings.append(data[so:e if e >= 0 else None].decode("cp850"))

    def _read_vars(self, off, count):
        out = []
        W, K = self.W, self.chunks
        for _ in range(count):
            bt = self.raw[self.base + off:self.base + off + K]
            cnt = self.w(off + K, K)
            members, vid, voff, vsp = self.w(off + K + K * W, 4)
            v = Var()
            v.chunks = list(zip(bt, cnt))
            v.members, v.id, v.offset, v.varspace = members, vid, voff, vsp
            v.name = self.idname.get(vid, "id_%d" % vid)
            out.append(v)
            off += self.VAR_SIZE
        return out

    def _read_varspaces(self):
        self.glovar = self._read_vars(self.h["OGloVars"], self.h["NGloVars"])
        self.locvar = self._read_vars(self.h["OLocVars"], self.h["NLocVars"])
        self.varspaces = []
        o, specs = self.h["OVarSpaces"], []
        for _ in range(self.h["NVarSpaces"]):
            nv, ov = self.w(o, 2)
            specs.append((nv, ov))
            o += 2 * self.W
        for nv, ov in specs:
            self.varspaces.append(self._read_vars(ov, nv) if nv else [])

    def _read_procs(self):
        self.procs = []
        o = self.h["OProcsTab"]
        for i in range(self.h["NProcs"]):
            d = dict(zip(self.proc_fields, self.w(o, len(self.proc_fields))))
            for k in ("NPubVars", "NPubStrings", "SPublic", "OPublic",
                      "OPubVars", "OPubStrings", "OExitCode", "OErrorCode"):
                d.setdefault(k, 0)
            p = Proc()
            p.d, p.index = d, i
            p.name = self.idname.get(d["ID"], "proc_%d" % d["ID"])
            p.code = list(self.w(d["OCode"], d["SCode"] // self.W)) if d["SCode"] else []
            p.privar = self._read_vars(d["OPriVars"], d["NPriVars"])
            p.pubvar = self._read_vars(d["OPubVars"], d["NPubVars"])
            # contenido inicial de las variables del proceso
            b = self.base
            p.pridata = bytes(self.raw[b + d["OPrivate"]:b + d["OPrivate"] + d["SPrivate"]])
            p.pubdata = bytes(self.raw[b + d["OPublic"]:b + d["OPublic"] + d["SPublic"]])
            p.sentences = []
            if d["NSentences"]:
                so = d["OSentences"]
                for _ in range(d["NSentences"]):
                    p.sentences.append(self.w(so, 4))
                    so += 4 * self.W
            self.procs.append(p)
            o += self.PROC_SIZE

    def _read_srcfiles(self):
        self.srcfiles = []
        if not self.h["NSourceFiles"]:
            return
        o = self.h["OSourceFiles"]
        for _ in range(self.h["NSourceFiles"]):
            size, = self.w(o)
            o += self.W
            b = self.base + o
            self.srcfiles.append(bytes(self.raw[b:b + size]).split(b"\0")[0].decode("latin-1"))
            o += size

    def embedded_files(self):
        """Ficheros incrustados: (nombre, offset, tamaño, flags).

        DCB_FILE empaquetado: SName(W), Flags(1), SFile(W), OFile(W), nombre[SName].
        Validado: offset(n) + tamaño(n) == offset(n+1) en toda la tabla.
        """
        out, o, W = [], self.h["OFilesTab"], self.W
        if self.fenix:
            # Fenix: Name[55] + Flags(1) + SFile + OFile, todo de tamaño fijo.
            # BennuGD lo empaqueto (SName, Flags, SFile, OFile, nombre[SName]).
            for _ in range(self.h["NFiles"]):
                name = bytes(self.raw[self.base + o:self.base + o + 55]).split(b"\0")[0]
                flags = self.raw[self.base + o + 55]
                sfile, ofile = self.w(o + 56, 2)
                out.append((name.decode("latin-1"), ofile, sfile, flags))
                o += 64
            return out
        for _ in range(self.h["NFiles"]):
            slen, = self.w(o)
            flags = self.raw[self.base + o + W]
            sfile, ofile = self.w(o + W + 1, 2)
            o += 3 * W + 1
            name = bytes(self.raw[self.base + o:self.base + o + slen]).split(b"\0")[0]
            out.append((name.decode("latin-1"), ofile, sfile, flags))
            o += slen
        return out

    def extract(self, destdir):
        """Vuelca los ficheros incrustados a disco, respetando sus rutas."""
        import os
        n = 0
        for name, off, size, flags in self.embedded_files():
            rel = name.replace("\\", "/").lstrip("/")
            while rel.startswith("../"):
                rel = rel[3:]
            dst = os.path.join(destdir, rel)
            os.makedirs(os.path.dirname(dst) or ".", exist_ok=True)
            with open(dst, "wb") as fh:
                fh.write(self.raw[self.base + off:self.base + off + size])
            n += 1
        return n


if __name__ == "__main__":
    if len(sys.argv) > 2 and sys.argv[1] == "--extract":
        d = DCB(sys.argv[2])
        dest = sys.argv[3] if len(sys.argv) > 3 else "assets"
        print("extraidos %d ficheros en %s" % (d.extract(dest), dest))
        sys.exit(0)
    d = DCB(sys.argv[1])
    print("version=0x%04x  palabra=%d bits" % (d.version, d.W * 8))
    print("procs=%(NProcs)d ids=%(NID)d strings=%(NStrings)d glovars=%(NGloVars)d "
          "locvars=%(NLocVars)d varspaces=%(NVarSpaces)d srcfiles=%(NSourceFiles)d "
          "ficheros_incrustados=%(NFiles)d" % d.h)
    if d.srcfiles:
        print("fuentes:", d.srcfiles)
    print("procs:", [p.name for p in d.procs][:30])
    print("strings[:15]:", d.strings[:15])
    if d.h["NFiles"]:
        fs = d.embedded_files()
        print("incrustados: %d ficheros, %d bytes (--extract para volcarlos)"
              % (len(fs), sum(f[2] for f in fs)))
