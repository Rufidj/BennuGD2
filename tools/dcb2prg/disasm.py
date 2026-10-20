#!/usr/bin/env python3
"""Desensamblador de bytecode .dcb con resolución de nombres."""
import sys, struct
from dcb import DCB
from resolve import Resolver
import os
import re
import mn


class Disasm:
    def __init__(self, d):
        self.d = d
        self.r = Resolver(d)
        self.fenix = getattr(d, "fenix", False)
        self.sysnames = self._sysprocs()
        self.mndecode = (mn.decode_v0 if self.fenix and d.version < 0x0600 else
                         mn.decode_v1 if d.W == 4 else mn.decode)
        self.tsuf = mn.TYPESUFFIX_V1 if d.W == 4 else mn.TYPESUFFIX
        self.t_string = mn.T_STRING_V1 if d.W == 4 else mn.T_STRING

    def _sysprocs(self):
        """code -> nombre, leyendo la tabla de fixup de SYSPROCS del propio dcb.

        Fenix no lleva esa tabla: sus codigos son fijos y vienen de sus fuentes
        (sysprocs_fenix.json, ver build_sysprocs_fenix.py).
        """
        if self.fenix:
            # la tabla depende de la VERSION de Fenix: ver tabla_fenix()
            import decompile
            tab = decompile.Decompiler.tabla_fenix(self, "sysprocs_fenix")
            return {int(k): (v[0], v[1], 0) for k, v in tab.items()}
        out = {}
        o = self.d.h["OSysProcsCodes"]
        raw = self.d.raw
        for i in range(self.d.h["NSysProcsCodes"]):
            W = self.d.W
            sid, = self.d.w(o)
            ty, = struct.unpack_from("<I", raw, self.d.base + o + W)
            params, code = self.d.w(o + W + 4, 2)
            o += 3 * W + 4 + params
            out[code] = (self.d.idname.get(sid, "sys_%d" % sid), params, ty)
        return out

    def var(self, kind, off, proc):
        if kind == "GLOBAL":
            n = self.r.name_at(self.d.glovar, off)
        elif kind == "LOCAL":
            n = self.r.name_at(self.d.locvar, off)
        elif kind == "PRIVATE":
            n = self.r.name_at(proc.privar, off)
        elif kind == "PUBLIC":
            n = self.r.name_at(proc.pubvar, off)
        else:
            n = None
        return n or "%s@%d" % (kind.lower(), off)

    def proc_lines(self, proc):
        code = proc.code
        out, i = [], 0
        sent = {s[3]: s for s in proc.sentences}
        while i < len(code):
            c = code[i] & 0xFFFF
            name, hasp, ty, uns = self.mndecode(c)
            suf = self.tsuf.get(ty, "")
            if uns:
                suf += ".u"
            arg = code[i + 1] if hasp and i + 1 < len(code) else None
            txt = name + suf
            if hasp:
                if name in ("PRIVATE", "GLOBAL", "LOCAL", "PUBLIC"):
                    txt += " %d  ; %s" % (arg, self.var(name, arg, proc))
                elif name in ("GET_PRIV", "GET_GLOBAL", "GET_LOCAL", "GET_PUBLIC"):
                    k = {"GET_PRIV": "PRIVATE", "GET_GLOBAL": "GLOBAL",
                         "GET_LOCAL": "LOCAL", "GET_PUBLIC": "PUBLIC"}[name]
                    txt += " %d  ; %s" % (arg, self.var(k, arg, proc))
                elif name in ("SYSCALL", "SYSPROC"):
                    s = self.sysnames.get(arg)
                    txt += " %d  ; %s(%d args)" % (arg, s[0] if s else "?", s[1] if s else -1)
                elif name in ("CALL", "PROC", "CLONE", "TYPE"):
                    pn = self.d.procs[arg].name if 0 <= arg < len(self.d.procs) else "?"
                    txt += " %d  ; %s" % (arg, pn)
                elif name == "PUSH":
                    txt += " %d" % arg
                    if ty == self.t_string and 0 <= arg < len(self.d.strings):
                        txt += "  ; %r" % self.d.strings[arg]
                else:
                    txt += " %d" % arg
            mark = ""
            if i in sent:
                nf, nl, nc, _ = sent[i]
                f = self.d.srcfiles[nf] if nf < len(self.d.srcfiles) else "?"
                mark = "   [%s:%d:%d]" % (f, nl, nc)
            out.append("%5d: %s%s" % (i, txt, mark))
            i += 2 if hasp else 1
        return out


if __name__ == "__main__":
    d = DCB(sys.argv[1])
    dis = Disasm(d)
    want = sys.argv[2] if len(sys.argv) > 2 else None
    # De que motor es el fichero: los opcodes de Fenix y los de BennuGD no son
    # los mismos, asi que conviene que se vea de que tabla sale el listado.
    print("// %s 0x%04X - %d procesos, palabra de %d bytes"
          % (d.motor, d.version, d.h["NProcs"], d.W))
    for p in d.procs:
        if want and p.name != want.upper():
            continue
        print("=" * 60)
        print("PROCESS %s  (params=%d, flags=%d, code=%d words)" %
              (p.name, p.d["NParams"], p.d["Flags"], len(p.code)))
        for v in p.privar:
            b, nm, m = dis.r.type_decl(v)
            print("   PRIVATE %s %s" % (b, nm))
        for l in dis.proc_lines(p):
            print(l)
