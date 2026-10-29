#!/usr/bin/env python3
"""GUI de escritorio para las herramientas de tools/dcb2prg.

Envuelve los scripts de línea de comandos (decompile.py, disasm.py, dcb.py,
extract_dcb.py, verificar_roundtrip.py, verificar_portado.py,
vigilar_atascos.py y las herramientas de construcción de tablas) en una
ventana con pestañas, consola en vivo y barra de progreso. No instala nada
nuevo: usa solo tkinter, que viene con Python.

    python3 gui.py

Cada pestaña solo junta los argumentos y delega en el script real - la lógica
de descompilación/verificación sigue viviendo únicamente ahí.
"""
import os
import re
import signal
import subprocess
import sys
import threading
import queue
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from tkinter.scrolledtext import ScrolledText

AQUI = os.path.dirname(os.path.abspath(__file__))
PROGRESO_RE = re.compile(r"\[\s*(\d+)\s*/\s*(\d+)\s*\]")

DCB_FILTROS = (("Ficheros .dcb", "*.dcb"), ("Todos", "*.*"))
PRG_FILTROS = (("Fuentes .prg", "*.prg"), ("Todos", "*.*"))
EXE_FILTROS = (("Ejecutables", "*.exe"), ("Todos", "*.*"))
JSON_FILTROS = (("JSON", "*.json"), ("Todos", "*.*"))


def ruta_script(nombre):
    return os.path.join(AQUI, nombre)


class Consola(ttk.Frame):
    """Panel compartido por todas las pestañas: texto de salida en vivo,
    barra de progreso y los botones para interrumpir. Solo corre un proceso
    a la vez - tiene poco sentido lanzar dos descompilaciones encima."""

    def __init__(self, master):
        super().__init__(master)
        self.texto = ScrolledText(self, height=16, bg="#111318", fg="#d8d8d8",
                                   insertbackground="#d8d8d8", font=("monospace", 10),
                                   state="disabled", wrap="word")
        self.texto.pack(fill="both", expand=True, padx=6, pady=(6, 2))
        self.texto.tag_configure("err", foreground="#ff8080")

        barra = ttk.Frame(self)
        barra.pack(fill="x", padx=6, pady=(0, 6))
        self.progreso = ttk.Progressbar(barra, mode="determinate")
        self.progreso.pack(side="left", fill="x", expand=True)
        self.etiqueta = ttk.Label(barra, text="listo", width=16, anchor="e")
        self.etiqueta.pack(side="left", padx=(6, 0))
        self.boton_ctrlc = ttk.Button(barra, text="Ctrl+C", width=8,
                                       command=self._enviar_ctrlc, state="disabled")
        self.boton_ctrlc.pack(side="left", padx=(6, 0))
        self.boton_cancelar = ttk.Button(barra, text="Cerrar", width=8,
                                          command=self._forzar, state="disabled")
        self.boton_cancelar.pack(side="left", padx=(4, 0))

        self.proc = None
        self.cola = queue.Queue()
        self.after(80, self._tick)

    def limpiar(self):
        self.texto.configure(state="normal")
        self.texto.delete("1.0", "end")
        self.texto.configure(state="disabled")

    def escribir(self, linea, tag=None):
        self.texto.configure(state="normal")
        self.texto.insert("end", linea + "\n", tag or ())
        self.texto.see("end")
        self.texto.configure(state="disabled")

    def ejecutar(self, args):
        """Lanza 'args' (lista, ya con el intérprete de Python al principio si
        hace falta) en un hilo aparte y vuelca su salida en vivo."""
        if self.proc is not None:
            messagebox.showwarning("dcb2prg", "Ya hay algo en marcha - espera a que termine.")
            return
        self.limpiar()
        self.progreso.stop()
        self.progreso.configure(mode="indeterminate")
        self.progreso.start(12)
        self.etiqueta.configure(text="en marcha...")
        self.boton_ctrlc.configure(state="normal")
        self.boton_cancelar.configure(state="normal")
        self.escribir("$ " + " ".join(args))
        # NO_COLOR: decompile.py pinta con escapes ANSI que un Text normal no
        # entiende: mejor texto plano que un churro de códigos en pantalla.
        env = dict(os.environ, NO_COLOR="1", PYTHONUNBUFFERED="1")
        hilo = threading.Thread(target=self._correr, args=(args, env), daemon=True)
        hilo.start()

    def _correr(self, args, env):
        try:
            self.proc = subprocess.Popen(
                args, cwd=AQUI, env=env, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT, text=True, bufsize=1)
        except OSError as e:
            self.cola.put(("linea", "no se pudo lanzar: %s" % e, "err"))
            self.cola.put(("fin", 1))
            return
        for linea in self.proc.stdout:
            linea = linea.rstrip("\n")
            self.cola.put(("linea", linea, None))
            m = PROGRESO_RE.search(linea)
            if m:
                self.cola.put(("progreso", int(m.group(1)), int(m.group(2))))
        rc = self.proc.wait()
        self.proc = None
        self.cola.put(("fin", rc))

    def _enviar_ctrlc(self):
        """Para vigilar_atascos.py (y cualquier otra herramienta interactiva):
        Ctrl+C real, no matar el proceso - es la señal que gdb está esperando."""
        if self.proc is not None:
            try:
                self.proc.send_signal(signal.SIGINT)
            except OSError:
                pass
            self.escribir("(Ctrl+C enviado)")

    def _forzar(self):
        if self.proc is not None:
            self.proc.terminate()
            self.escribir("(cerrado)")

    def _tick(self):
        try:
            while True:
                item = self.cola.get_nowait()
                tipo = item[0]
                if tipo == "linea":
                    self.escribir(item[1], item[2])
                elif tipo == "progreso":
                    n, total = item[1], item[2]
                    if str(self.progreso["mode"]) != "determinate":
                        self.progreso.stop()
                        self.progreso.configure(mode="determinate", maximum=total)
                    self.progreso["value"] = n
                    self.etiqueta.configure(text="%d/%d" % (n, total))
                elif tipo == "fin":
                    rc = item[1]
                    self.progreso.stop()
                    if str(self.progreso["mode"]) != "determinate":
                        self.progreso.configure(mode="determinate", maximum=1)
                    self.progreso["value"] = self.progreso["maximum"] if rc == 0 else 0
                    self.etiqueta.configure(text="OK" if rc == 0 else "fallo (%d)" % rc)
                    self.boton_ctrlc.configure(state="disabled")
                    self.boton_cancelar.configure(state="disabled")
                    self.escribir("-- terminado, código %d --" % rc, None if rc == 0 else "err")
        except queue.Empty:
            pass
        self.after(80, self._tick)


def fila_ruta(padre, etiqueta, fila, guardar=False, filtros=(("Todos", "*.*"),), directorio=False):
    """Label + Entry + botón '...' en la fila dada. Devuelve la StringVar."""
    ttk.Label(padre, text=etiqueta).grid(row=fila, column=0, sticky="w", padx=4, pady=3)
    var = tk.StringVar()
    ttk.Entry(padre, textvariable=var, width=52).grid(row=fila, column=1, sticky="we", padx=4, pady=3)

    def elegir():
        if directorio:
            p = filedialog.askdirectory()
        elif guardar:
            p = filedialog.asksaveasfilename(filetypes=filtros)
        else:
            p = filedialog.askopenfilename(filetypes=filtros)
        if p:
            var.set(p)

    ttk.Button(padre, text="...", width=3, command=elegir).grid(row=fila, column=2, padx=4, pady=3)
    padre.columnconfigure(1, weight=1)
    return var


def pedir(var, etiqueta):
    v = var.get().strip()
    if not v:
        messagebox.showerror("dcb2prg", "Falta: %s" % etiqueta)
        return None
    return v


class PestanaDescompilar(ttk.Frame):
    """decompile.py - la operación principal del proyecto."""

    def __init__(self, master, consola):
        super().__init__(master, padding=10)
        self.consola = consola
        self.entrada = fila_ruta(self, "Fichero .dcb de entrada:", 0, filtros=DCB_FILTROS)
        self.salida = fila_ruta(self, "Fuente .prg de salida:", 1, guardar=True, filtros=PRG_FILTROS)

        ttk.Label(self, text="Solo este proceso (opcional):").grid(row=2, column=0, sticky="w", padx=4, pady=3)
        self.proceso = tk.StringVar()
        ttk.Entry(self, textvariable=self.proceso, width=30).grid(row=2, column=1, sticky="w", padx=4, pady=3)

        self.modo = tk.StringVar(value="")
        marco_modo = ttk.LabelFrame(self, text="Dialecto de salida")
        marco_modo.grid(row=3, column=0, columnspan=3, sticky="we", padx=4, pady=(8, 3))
        ttk.Radiobutton(marco_modo, text="Tal cual (dialecto original del .dcb)",
                         variable=self.modo, value="").pack(anchor="w")
        ttk.Radiobutton(marco_modo, text="--bgd1 (porta Fenix a BennuGD1)",
                         variable=self.modo, value="--bgd1").pack(anchor="w")
        ttk.Radiobutton(marco_modo, text="--bgd2 (porta a BennuGD2: renombra, shims, paletas)",
                         variable=self.modo, value="--bgd2").pack(anchor="w")

        self.extraer = tk.StringVar(value="no")
        marco_extraer = ttk.LabelFrame(self, text="Assets incrustados en el .dcb")
        marco_extraer.grid(row=4, column=0, columnspan=3, sticky="we", padx=4, pady=3)
        ttk.Radiobutton(marco_extraer, text="Extraer junto al .prg",
                         variable=self.extraer, value="si").pack(anchor="w")
        ttk.Radiobutton(marco_extraer, text="No extraer",
                         variable=self.extraer, value="no").pack(anchor="w")

        ttk.Button(self, text="Descompilar", command=self._ejecutar).grid(
            row=5, column=0, columnspan=3, pady=(10, 0))

    def _ejecutar(self):
        entrada = pedir(self.entrada, "fichero .dcb de entrada")
        salida = pedir(self.salida, "fuente .prg de salida")
        if not entrada or not salida:
            return
        args = [sys.executable, ruta_script("decompile.py"), "-o", salida]
        if self.modo.get():
            args.append(self.modo.get())
        args.append("--extraer" if self.extraer.get() == "si" else "--no-extraer")
        args.append(entrada)
        if self.proceso.get().strip():
            args.append(self.proceso.get().strip())
        self.consola.ejecutar(args)


class PestanaDesensamblar(ttk.Frame):
    """disasm.py - bytecode crudo con nombres resueltos, para ver un proceso suelto."""

    def __init__(self, master, consola):
        super().__init__(master, padding=10)
        self.consola = consola
        self.entrada = fila_ruta(self, "Fichero .dcb:", 0, filtros=DCB_FILTROS)
        ttk.Label(self, text="Solo este proceso (opcional):").grid(row=1, column=0, sticky="w", padx=4, pady=3)
        self.proceso = tk.StringVar()
        ttk.Entry(self, textvariable=self.proceso, width=30).grid(row=1, column=1, sticky="w", padx=4, pady=3)
        ttk.Button(self, text="Desensamblar", command=self._ejecutar).grid(
            row=2, column=0, columnspan=3, pady=(10, 0))

    def _ejecutar(self):
        entrada = pedir(self.entrada, "fichero .dcb")
        if not entrada:
            return
        args = [sys.executable, ruta_script("disasm.py"), entrada]
        if self.proceso.get().strip():
            args.append(self.proceso.get().strip())
        self.consola.ejecutar(args)


class PestanaResumen(ttk.Frame):
    """dcb.py - resumen del contenedor, o volcado de los assets incrustados."""

    def __init__(self, master, consola):
        super().__init__(master, padding=10)
        self.consola = consola
        self.entrada = fila_ruta(self, "Fichero .dcb:", 0, filtros=DCB_FILTROS)
        ttk.Button(self, text="Ver resumen", command=self._resumen).grid(
            row=1, column=0, pady=(10, 0), sticky="w", padx=4)

        ttk.Separator(self).grid(row=2, column=0, columnspan=3, sticky="we", pady=10)
        self.destino = fila_ruta(self, "Carpeta donde volcar los assets:", 3, directorio=True)
        ttk.Button(self, text="Extraer assets", command=self._extraer).grid(
            row=4, column=0, pady=(10, 0), sticky="w", padx=4)

    def _resumen(self):
        entrada = pedir(self.entrada, "fichero .dcb")
        if not entrada:
            return
        self.consola.ejecutar([sys.executable, ruta_script("dcb.py"), entrada])

    def _extraer(self):
        entrada = pedir(self.entrada, "fichero .dcb")
        if not entrada:
            return
        args = [sys.executable, ruta_script("dcb.py"), "--extract", entrada]
        if self.destino.get().strip():
            args.append(self.destino.get().strip())
        self.consola.ejecutar(args)


class PestanaExtraerExe(ttk.Frame):
    """extract_dcb.py - saca el .dcb pegado al final de un .exe/stub."""

    def __init__(self, master, consola):
        super().__init__(master, padding=10)
        self.consola = consola
        self.entrada = fila_ruta(self, "Ejecutable (.exe o binario con el .dcb pegado):", 0, filtros=EXE_FILTROS)
        self.salida = fila_ruta(self, "Fichero .dcb de salida (opcional):", 1, guardar=True, filtros=DCB_FILTROS)
        ttk.Button(self, text="Extraer .dcb", command=self._ejecutar).grid(
            row=2, column=0, columnspan=3, pady=(10, 0))

    def _ejecutar(self):
        entrada = pedir(self.entrada, "ejecutable de entrada")
        if not entrada:
            return
        args = [sys.executable, ruta_script("extract_dcb.py"), entrada]
        if self.salida.get().strip():
            args.append(self.salida.get().strip())
        self.consola.ejecutar(args)


class PestanaRoundtrip(ttk.Frame):
    """verificar_roundtrip.py - el único control objetivo sin jugar al juego."""

    def __init__(self, master, consola):
        super().__init__(master, padding=10)
        self.consola = consola
        self.original = fila_ruta(self, "Fichero .dcb original:", 0, filtros=DCB_FILTROS)
        self.bgdc = fila_ruta(self, "Compilador (bgdc de BennuGD1/2 o fxc de Fenix):", 1)
        ttk.Label(self, text="Tiene que ser el compilador del MISMO motor que el .dcb.",
                  foreground="#888").grid(row=2, column=0, columnspan=3, sticky="w", padx=4)
        ttk.Button(self, text="Verificar", command=self._ejecutar).grid(
            row=3, column=0, columnspan=3, pady=(10, 0))

    def _ejecutar(self):
        original = pedir(self.original, "fichero .dcb original")
        bgdc = pedir(self.bgdc, "compilador")
        if not original or not bgdc:
            return
        self.consola.ejecutar([sys.executable, ruta_script("verificar_roundtrip.py"), original, bgdc])


class PestanaPortado(ttk.Frame):
    """verificar_portado.py - compara el .dcb de BennuGD1 con su portado a BennuGD2."""

    def __init__(self, master, consola):
        super().__init__(master, padding=10)
        self.consola = consola
        self.original = fila_ruta(self, "Fichero .dcb original (BennuGD1/Fenix):", 0, filtros=DCB_FILTROS)
        self.portado = fila_ruta(self, "Fichero .dcb portado (BennuGD2):", 1, filtros=DCB_FILTROS)
        ttk.Label(self, text="Solo este proceso (opcional):").grid(row=2, column=0, sticky="w", padx=4, pady=3)
        self.proceso = tk.StringVar()
        ttk.Entry(self, textvariable=self.proceso, width=30).grid(row=2, column=1, sticky="w", padx=4, pady=3)
        ttk.Button(self, text="Comparar", command=self._ejecutar).grid(
            row=3, column=0, columnspan=3, pady=(10, 0))

    def _ejecutar(self):
        original = pedir(self.original, "fichero .dcb original")
        portado = pedir(self.portado, "fichero .dcb portado")
        if not original or not portado:
            return
        args = [sys.executable, ruta_script("verificar_portado.py"), original, portado]
        if self.proceso.get().strip():
            args.append(self.proceso.get().strip())
        self.consola.ejecutar(args)


class PestanaAtascos(ttk.Frame):
    """vigilar_atascos.py - lanza el juego bajo gdb y detecta procesos colgados.

    Es interactiva: hay que jugar hasta la situación sospechosa y entonces
    pulsar el botón "Ctrl+C" del panel de abajo (no "Cerrar", eso lo mata)."""

    def __init__(self, master, consola):
        super().__init__(master, padding=10)
        self.consola = consola
        self.entrada = fila_ruta(self, "Fichero .dcb del juego:", 0, filtros=DCB_FILTROS)
        ttk.Label(self, text="Fotogramas de margen entre las dos muestras:").grid(
            row=1, column=0, sticky="w", padx=4, pady=3)
        self.frames = tk.StringVar(value="600")
        ttk.Entry(self, textvariable=self.frames, width=10).grid(row=1, column=1, sticky="w", padx=4, pady=3)
        ttk.Label(self,
                  text="bgdi tiene que estar en el PATH, o pon su ruta en la\n"
                       "variable de entorno BGDI antes de abrir esta ventana.",
                  foreground="#888").grid(row=2, column=0, columnspan=3, sticky="w", padx=4, pady=(6, 0))
        ttk.Button(self, text="Lanzar y vigilar", command=self._ejecutar).grid(
            row=3, column=0, columnspan=3, pady=(10, 0))
        ttk.Label(self,
                  text="Juega hasta ver el bloqueo y pulsa el botón \"Ctrl+C\" de abajo\n"
                       "(no \"Cerrar\" - eso mata el juego sin generar el informe).",
                  foreground="#c80").grid(row=4, column=0, columnspan=3, sticky="w", padx=4, pady=(8, 0))

    def _ejecutar(self):
        entrada = pedir(self.entrada, "fichero .dcb")
        if not entrada:
            return
        args = [sys.executable, ruta_script("vigilar_atascos.py"), entrada]
        frames = self.frames.get().strip()
        if frames:
            args.append(frames)
        self.consola.ejecutar(args)


class PestanaAvanzado(ttk.Frame):
    """Herramientas de mantenimiento: reconstruir las tablas que usan las
    demás (symtab, sysprocs/vars de Fenix, módulos .so de mentira). Uso raro,
    así que van con rutas en crudo en vez de UI dedicada por script."""

    def __init__(self, master, consola):
        super().__init__(master, padding=10)
        self.consola = consola
        fila = 0

        ttk.Label(self, text="build_symtab.py - tabla de símbolos exportados",
                  font=("", 10, "bold")).grid(row=fila, column=0, columnspan=3, sticky="w", padx=4, pady=(0, 2))
        fila += 1
        self.symtab_src = fila_ruta(self, "  Árbol de fuentes BennuGD:", fila, directorio=True)
        fila += 1
        self.symtab_dst = fila_ruta(self, "  JSON de salida:", fila, guardar=True, filtros=JSON_FILTROS)
        fila += 1
        ttk.Button(self, text="Construir", command=self._symtab).grid(row=fila, column=0, sticky="w", padx=4, pady=(2, 10))
        fila += 1

        ttk.Separator(self).grid(row=fila, column=0, columnspan=3, sticky="we", pady=6)
        fila += 1
        ttk.Label(self, text="build_sysprocs_fenix.py / build_vars_fenix.py - tablas de Fenix",
                  font=("", 10, "bold")).grid(row=fila, column=0, columnspan=3, sticky="w", padx=4, pady=(4, 2))
        fila += 1
        self.fenix_src = fila_ruta(self, "  Árbol de fuentes de Fenix:", fila, directorio=True)
        fila += 1
        botones = ttk.Frame(self)
        botones.grid(row=fila, column=0, columnspan=3, sticky="w", padx=4, pady=(2, 10))
        ttk.Button(botones, text="Sysprocs", command=self._sysprocs).pack(side="left")
        ttk.Button(botones, text="Variables", command=self._vars).pack(side="left", padx=(6, 0))
        fila += 1

        ttk.Separator(self).grid(row=fila, column=0, columnspan=3, sticky="we", pady=6)
        fila += 1
        ttk.Label(self, text="make_stub_modules.py - módulos .so de mentira para bgdc",
                  font=("", 10, "bold")).grid(row=fila, column=0, columnspan=3, sticky="w", padx=4, pady=(4, 2))
        fila += 1
        self.stub_src = fila_ruta(self, "  Fuente (cabecera de módulo o .dcb con --dcb):", fila)
        fila += 1
        self.stub_dst = fila_ruta(self, "  Destino (fichero o carpeta, según el modo):", fila, directorio=True)
        fila += 1
        ttk.Button(self, text="Generar", command=self._stubs).grid(row=fila, column=0, sticky="w", padx=4, pady=(2, 0))

    def _symtab(self):
        src = pedir(self.symtab_src, "árbol de fuentes")
        dst = pedir(self.symtab_dst, "JSON de salida")
        if not src or not dst:
            return
        self.consola.ejecutar([sys.executable, ruta_script("build_symtab.py"), src, dst])

    def _sysprocs(self):
        src = pedir(self.fenix_src, "árbol de fuentes de Fenix")
        if not src:
            return
        self.consola.ejecutar([sys.executable, ruta_script("build_sysprocs_fenix.py"), src])

    def _vars(self):
        src = pedir(self.fenix_src, "árbol de fuentes de Fenix")
        if not src:
            return
        self.consola.ejecutar([sys.executable, ruta_script("build_vars_fenix.py"), src])

    def _stubs(self):
        src = pedir(self.stub_src, "fuente")
        dst = pedir(self.stub_dst, "destino")
        if not src or not dst:
            return
        self.consola.ejecutar([sys.executable, ruta_script("make_stub_modules.py"), src, dst])


def main():
    raiz = tk.Tk()
    raiz.title("dcb2prg")
    raiz.geometry("1060x640")
    raiz.minsize(1000, 520)
    try:
        ttk.Style(raiz).theme_use("clam")
    except tk.TclError:
        pass

    partido = ttk.PanedWindow(raiz, orient="vertical")
    partido.pack(fill="both", expand=True)

    pestanas = ttk.Notebook(partido)
    partido.add(pestanas, weight=3)

    consola = Consola(partido)
    partido.add(consola, weight=2)

    for titulo, clase in (
        ("Descompilar", PestanaDescompilar),
        ("Desensamblar", PestanaDesensamblar),
        ("Resumen .dcb", PestanaResumen),
        ("Extraer .exe", PestanaExtraerExe),
        ("Roundtrip", PestanaRoundtrip),
        ("Portado", PestanaPortado),
        ("Atascos", PestanaAtascos),
        ("Avanzado", PestanaAvanzado),
    ):
        pestanas.add(clase(pestanas, consola), text=titulo)

    raiz.mainloop()


if __name__ == "__main__":
    main()
