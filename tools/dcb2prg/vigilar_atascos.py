#!/usr/bin/env python3
"""Detecta procesos ATASCADOS en un juego de BennuGD2 en marcha.

Es el hueco que ninguna comparacion estatica cubre. Un proceso puede estar
perfectamente reconstruido y aun asi quedarse girando para siempre en un bucle
de espera cuya condicion nunca se cumple: el motor sigue dibujando frames, el
juego no se cuelga ni peta, y desde fuera solo se ve que "algo no aparece".

Caso real que motivo esto (SoRR portado): 35 procesos ENEMIGO vivos atascados en

    WHILE (F_ENEMIGO[TIPO_EN] < 0) ... FRAME; END

que esta DESPUES de un ENEMIGOS_PANTALLA++, asi que cada atascado consumia un
hueco del limite de enemigos en pantalla y no lo soltaba nunca. Resultado: los
niveles se recorrian vacios. Costo una sesion entera encontrarlo a mano; esto lo
canta en dos segundos.

Como funciona: se toman dos muestras de las instancias vivas separadas por unos
segundos y se hace un CENSO por (proceso, offset de bytecode).

Ojo con la señal: comparar el `codeptr` entre dos frames NO sirve para decidir
si un proceso avanza. En la frontera de frame todo proceso esta parado en su
FRAME, y uno sano con un solo "LOOP ... FRAME; END" esta SIEMPRE en el mismo
sitio. Con ese criterio salian atascados 27 de 27 procesos de un juego que
funcionaba perfectamente.

Lo que si delata un atasco es el RELEVO: en un juego sano las instancias de un
proceso nacen y mueren. Un grupo numeroso de instancias del MISMO proceso,
paradas en el MISMO offset, que sigue ahi entero pasados varios segundos sin que
ninguna se haya renovado, es un bucle de espera que no termina. Eso es lo que se
marca. La ultima palabra la tienes tu: un proceso dormido o congelado con
signal() tambien persiste, y eso puede ser correcto.

    python3 vigilar_atascos.py <juego.dcb> [frames_entre_muestras]

Juega hasta la situacion sospechosa y pulsa Ctrl+C EN LA TERMINAL (no cierres la
ventana). El resto es automatico.

NECESITA SIMBOLOS. El build normal de BennuGD2 va sin ellos, asi que hay que
recompilar SOLO libbgdrtm para usar esto, y dejarlo como estaba al terminar. No
hace falta bajar la optimizacion: "-O3 -g" corre igual de rapido.

    cd <build de BennuGD2>
    cmake -DCMAKE_C_FLAGS_RELEASE="-O3 -g -DNDEBUG" . && make bgdrtm -j8
    ...usar la herramienta...
    cmake -DCMAKE_C_FLAGS_RELEASE="-O3 -DNDEBUG" . && make bgdrtm -j8

Si sale "No symbol \"first_instance\" in current context", es que falta el -g.
"""
import os
import subprocess
import sys
import tempfile

def buscar_bgdi():
    """El interprete de BennuGD2: $BGDI, el PATH, o el build junto al repo."""
    env = os.environ.get("BGDI")
    if env:
        return env
    from shutil import which
    hallado = which("bgdi")
    if hallado:
        return hallado
    raiz = os.path.dirname(os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))))          # tools/dcb2prg -> raiz del repo
    import glob
    for c in sorted(glob.glob(os.path.join(raiz, "build", "*", "bin", "bgdi"))):
        return c
    return None

GDB = r'''
set pagination off
set confirm off

python
import gdb

def muestra():
    """(id, nombre, codeptr) de cada instancia viva."""
    out = {}
    try:
        it = gdb.parse_and_eval("first_instance")
        n = 0
        while int(it) != 0 and n < 40000:
            p = int(it)
            try:
                nom = it["proc"]["name"].string()
            except Exception:
                nom = "?"
            try:
                pid = int(gdb.parse_and_eval(
                    "*(long long*)((char*)((struct _instance*)%d)->locdata)" % p))
                pc = int(gdb.parse_and_eval(
                    "(long long)(((struct _instance*)%d)->codeptr"
                    " - ((struct _instance*)%d)->code)" % (p, p)))
            except Exception:
                pid, pc = p, -1
            out[pid] = (nom, pc)
            it = it["next"]
            n += 1
    except Exception as e:
        print("  ERROR recorriendo instancias:", e)
    return out

gdb.execute("run")

print("\n########## CENSO DE PROCESOS ##########")
a = muestra()
print("  instancias vivas: %d" % len(a))

# dejar correr sin intervenir: gr_wait_frame se llama una vez por frame, asi que
# un breakpoint con ignore cuenta frames exactos
NFRAMES = FRAMES
bp = gdb.Breakpoint("gr_wait_frame")
bp.silent = True
bp.ignore_count = NFRAMES
gdb.execute("continue")
bp.delete()

b = muestra()
print("  instancias vivas tras %d frames: %d" % (NFRAMES, len(b)))

from collections import Counter, defaultdict
grupos = defaultdict(list)
for iid, (nom, pc) in b.items():
    grupos[(nom, pc)].append(iid)

filas = []
for (nom, pc), ids in grupos.items():
    # cuantas de las instancias de este grupo ya estaban, en el mismo sitio
    viejas = sum(1 for i in ids if i in a and a[i][1] == pc)
    filas.append((len(ids), viejas, nom, pc))
filas.sort(reverse=True)

print("\n  censo por (proceso, offset), tras %d frames" % NFRAMES)
print("  %-24s %8s %10s %8s"
      % ("proceso", "cuantos", "sin relevo", "offset"))
sospechosos = 0
for n, viejas, nom, pc in filas[:20]:
    marca = ""
    if n >= 3 and viejas == n:
        marca = "   <-- SOSPECHOSO: %d parados en el mismo sitio, ninguno se renovo" % n
        sospechosos += 1
    print("  %-24s %8d %10s %8d%s"
          % (nom, n, "%d (%d%%)" % (viejas, 100 * viejas // n), pc, marca))

print("\n  grupos sospechosos: %d" % sospechosos)
if sospechosos:
    print("  Para ver que hace ese offset:")
    print("     python3 disasm.py <juego.dcb> <PROCESO>")
    print("  y busca la instruccion en ese numero. Si cae dentro de un WHILE de")
    print("  espera, ahi tienes el atasco.")
else:
    print("  nada llamativo: los grupos numerosos se van renovando")
gdb.execute("kill")
end
'''


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    dcb = os.path.abspath(sys.argv[1])
    frames = int(sys.argv[2]) if len(sys.argv) > 2 else 600

    bgdi = buscar_bgdi()
    if not bgdi or not os.path.exists(bgdi):
        print("no encuentro bgdi. Ponlo en el PATH o pasa la ruta en $BGDI:")
        print("    BGDI=/ruta/a/bgdi python3 %s <juego.dcb>" % os.path.basename(__file__))
        return 2

    fd, gdbfile = tempfile.mkstemp(suffix=".gdb")
    with os.fdopen(fd, "w") as fh:
        fh.write(GDB.replace("FRAMES", str(frames)))

    print()
    print("=" * 62)
    print("  Juega hasta la situacion sospechosa (algo que no aparece, que no")
    print("  reacciona, enemigos que no salen...) y pulsa Ctrl+C EN ESTA")
    print("  TERMINAL. No cierres la ventana del juego ni lo mates.")
    print("=" * 62)
    print()

    r = subprocess.run(["gdb", "-batch", "-x", gdbfile, "--args", bgdi, dcb],
                       cwd=os.path.dirname(dcb) or ".")
    os.unlink(gdbfile)
    return r.returncode


if __name__ == "__main__":
    sys.exit(main())
