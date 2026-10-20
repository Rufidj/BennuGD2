#!/usr/bin/env python3
from __future__ import annotations

import subprocess
import sys
import threading
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox, ttk


GAME_PRESETS = {
    "gta3": {
        "label": "GTA 3",
        "game_root": "/home/ruben/.wine/drive_c/Program Files (x86)/Grand Theft Auto III",
        "extracted_root": "/home/ruben/Descargas/GTA3/ARCHIVOS",
        "output_dir": "/home/ruben/BennuGD2/tmp/gta3_glb_gui",
        "dat_path": "data/gta3.dat",
    },
    "gta4": {
        "label": "GTA 4",
        "game_root": "/media/ruben/4ED0B68AD0B677B3/Grand Theft Auto IV Complete Edition/GTAIV",
        "extracted_root": "/home/ruben/Descargas/GTA4/ARCHIVOS",
        "output_dir": "/home/ruben/BennuGD2/tmp/gta4_glb_gui",
        "dat_path": "",
    },
}


class ExporterGui:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("GTA Map Exporter")
        self.root.geometry("860x620")

        self.selected_game = tk.StringVar(value="gta3")
        self.game_root = tk.StringVar(value=GAME_PRESETS["gta3"]["game_root"])
        self.extracted_root = tk.StringVar(value=GAME_PRESETS["gta3"]["extracted_root"])
        self.textures_dir = tk.StringVar(value="")
        self.output_dir = tk.StringVar(value=GAME_PRESETS["gta3"]["output_dir"])
        self.dat_path = tk.StringVar(value=GAME_PRESETS["gta3"]["dat_path"])
        self.ipl_path = tk.StringVar(value="")
        self.extract_textures = tk.BooleanVar(value=False)
        self.texture_output_format = tk.StringVar(value="dds")
        self.include_lod = tk.BooleanVar(value=False)
        self.output_format = tk.StringVar(value="glb")
        self.sector_size = tk.StringVar(value="0")
        self.export_vehicles = tk.BooleanVar(value=True)
        self.export_peds = tk.BooleanVar(value=True)
        self.export_weapons = tk.BooleanVar(value=True)
        self.export_props = tk.BooleanVar(value=True)
        self.copy_ped_anims = tk.BooleanVar(value=True)
        self.export_map = tk.BooleanVar(value=True)
        self.export_zone_previews = tk.BooleanVar(value=False)
        self.flip_uv_v = tk.BooleanVar(value=True)
        self.flip_uv_u = tk.BooleanVar(value=False)
        self.double_sided = tk.BooleanVar(value=False)
        self.status_text = tk.StringVar()
        self.dat_label_text = tk.StringVar(value="DAT")
        self.ipl_label_text = tk.StringVar(value="IPL opcional")

        self.run_button: ttk.Button
        self.log: tk.Text
        self.game_status_label: ttk.Label
        self.dat_label: ttk.Label
        self.ipl_label: ttk.Label

        self._build()
        self._on_game_changed()

    def _build(self) -> None:
        frame = ttk.Frame(self.root, padding=12)
        frame.pack(fill="both", expand=True)

        row = 0
        ttk.Label(frame, text="Juego").grid(row=row, column=0, sticky="w", pady=6)
        game_combo = ttk.Combobox(
            frame,
            textvariable=self.selected_game,
            values=list(GAME_PRESETS.keys()),
            state="readonly",
            width=12,
        )
        game_combo.grid(row=row, column=1, sticky="w", pady=6)
        game_combo.bind("<<ComboboxSelected>>", self._on_game_changed)
        self.game_status_label = ttk.Label(frame, textvariable=self.status_text)
        self.game_status_label.grid(row=row, column=2, sticky="w", pady=6)
        row += 1

        self._path_row(frame, row, "Ruta juego", self.game_root, directory=True)
        row += 1
        self._path_row(frame, row, "Extraídos", self.extracted_root, directory=True)
        row += 1
        self._path_row(frame, row, "PNG texturas", self.textures_dir, directory=True)
        row += 1
        self._path_row(frame, row, "Salida", self.output_dir, directory=True)
        row += 1
        self.dat_label = self._path_row(frame, row, self.dat_label_text, self.dat_path, directory=False)
        row += 1
        self.ipl_label = self._path_row(frame, row, self.ipl_label_text, self.ipl_path, directory=False)
        row += 1

        ttk.Label(frame, text="Formato").grid(row=row, column=0, sticky="w", pady=6)
        ttk.Combobox(frame, textvariable=self.output_format, values=["glb", "gltf"], state="readonly", width=12).grid(
            row=row, column=1, sticky="w", pady=6
        )
        ttk.Checkbutton(frame, text="Incluir LOD", variable=self.include_lod).grid(row=row, column=2, sticky="w", pady=6)
        row += 1

        ttk.Label(frame, text="Tamaño sector").grid(row=row, column=0, sticky="w", pady=6)
        ttk.Entry(frame, textvariable=self.sector_size, width=12).grid(row=row, column=1, sticky="w", pady=6)
        ttk.Label(frame, text="0 = mapa único, > 0 = assets/sectors/sec_X_Z.glb").grid(row=row, column=2, sticky="w", pady=6)
        row += 1

        ttk.Checkbutton(frame, text="GTA4: exportar previews por zona", variable=self.export_zone_previews).grid(row=row, column=0, sticky="w", pady=6)
        ttk.Label(frame, text="Si está desactivado, la salida principal serán solo sectores en assets/sectors").grid(row=row, column=1, columnspan=2, sticky="w", pady=6)
        row += 1

        ttk.Checkbutton(frame, text="Invertir V de UV", variable=self.flip_uv_v).grid(row=row, column=0, sticky="w", pady=6)
        ttk.Label(frame, text="Actívalo si carteles o texturas salen boca abajo").grid(row=row, column=1, columnspan=2, sticky="w", pady=6)
        row += 1

        ttk.Checkbutton(frame, text="Invertir U de UV", variable=self.flip_uv_u).grid(row=row, column=0, sticky="w", pady=6)
        ttk.Label(frame, text="Actívalo si los textos salen en espejo").grid(row=row, column=1, columnspan=2, sticky="w", pady=6)
        row += 1

        ttk.Checkbutton(frame, text="Materiales doble cara", variable=self.double_sided).grid(row=row, column=0, sticky="w", pady=6)
        ttk.Label(frame, text="Úsalo solo si necesitas ver planos por ambas caras").grid(row=row, column=1, columnspan=2, sticky="w", pady=6)
        row += 1

        ttk.Checkbutton(frame, text="Extraer texturas del juego", variable=self.extract_textures).grid(row=row, column=0, sticky="w", pady=6)
        ttk.Combobox(frame, textvariable=self.texture_output_format, values=["dds", "png", "both"], state="readonly", width=12).grid(
            row=row, column=1, sticky="w", pady=6
        )
        ttk.Label(frame, text="GTA4: usa PNG para GLB texturizado. GTA3: aún no implementado").grid(row=row, column=2, sticky="w", pady=6)
        row += 1

        ttk.Label(frame, text="Extras GLB").grid(row=row, column=0, sticky="w", pady=6)
        extras = ttk.Frame(frame)
        extras.grid(row=row, column=1, columnspan=2, sticky="w", pady=6)
        ttk.Checkbutton(extras, text="Exportar mapa", variable=self.export_map).pack(side="left")
        ttk.Checkbutton(extras, text="Vehículos", variable=self.export_vehicles).pack(side="left")
        ttk.Checkbutton(extras, text="Peds", variable=self.export_peds).pack(side="left", padx=(8, 0))
        ttk.Checkbutton(extras, text="Armas", variable=self.export_weapons).pack(side="left", padx=(8, 0))
        ttk.Checkbutton(extras, text="Props", variable=self.export_props).pack(side="left", padx=(8, 0))
        ttk.Checkbutton(extras, text="Copiar ped.ifp", variable=self.copy_ped_anims).pack(side="left", padx=(8, 0))
        row += 1

        button_row = ttk.Frame(frame)
        button_row.grid(row=row, column=0, columnspan=3, sticky="we", pady=(8, 10))
        self.run_button = ttk.Button(button_row, text="Exportar", command=self.start_export)
        self.run_button.pack(side="left")
        ttk.Button(button_row, text="Abrir carpeta salida", command=self.open_output_hint).pack(side="left", padx=(8, 0))
        row += 1

        self.log = tk.Text(frame, height=22, wrap="word")
        self.log.grid(row=row, column=0, columnspan=3, sticky="nsew")

        scrollbar = ttk.Scrollbar(frame, orient="vertical", command=self.log.yview)
        scrollbar.grid(row=row, column=3, sticky="ns")
        self.log.configure(yscrollcommand=scrollbar.set)

        frame.columnconfigure(1, weight=1)
        frame.columnconfigure(2, weight=0)
        frame.rowconfigure(row, weight=1)

    def _on_game_changed(self, _event: object | None = None) -> None:
        preset = GAME_PRESETS[self.selected_game.get()]
        self.game_root.set(str(preset["game_root"]))
        self.extracted_root.set(str(preset["extracted_root"]))
        self.output_dir.set(str(preset["output_dir"]))
        self.dat_path.set(str(preset["dat_path"]))
        if self.selected_game.get() == "gta3":
            self.status_text.set("Implementado: mapa, sectores y assets")
            self.dat_label_text.set("DAT")
            self.ipl_label_text.set("IPL opcional")
            self.run_button.configure(state="normal")
        else:
            self.status_text.set("Implementado: parser GTA4 + sectores GLB + JSON de coordenadas")
            self.dat_label_text.set("DAT GTA4")
            self.ipl_label_text.set("Filtro/zona o ruta IPL/WPL")
            self.run_button.configure(state="normal")

    def _path_row(self, parent: ttk.Frame, row: int, label: str | tk.StringVar, variable: tk.StringVar,
                  directory: bool) -> ttk.Label:
        widget = ttk.Label(parent, textvariable=label) if isinstance(label, tk.StringVar) else ttk.Label(parent, text=label)
        widget.grid(row=row, column=0, sticky="w", pady=6)
        ttk.Entry(parent, textvariable=variable, width=90).grid(row=row, column=1, sticky="we", pady=6, padx=(8, 8))
        ttk.Button(
            parent,
            text="Examinar",
            command=lambda: self._browse(variable, directory),
        ).grid(row=row, column=2, sticky="w", pady=6)
        return widget

    def _browse(self, variable: tk.StringVar, directory: bool) -> None:
        current = variable.get().strip()
        initial = str(Path(current).expanduser().parent if current else Path.home())
        if directory:
            selected = filedialog.askdirectory(initialdir=initial)
        else:
            selected = filedialog.askopenfilename(initialdir=initial)
        if selected:
            variable.set(selected)

    def append_log(self, text: str) -> None:
        self.log.insert("end", text)
        self.log.see("end")

    def start_export(self) -> None:
        if not self.game_root.get().strip():
            messagebox.showerror("Falta ruta", "Necesito la ruta del juego.")
            return
        if not self.output_dir.get().strip():
            messagebox.showerror("Falta ruta", "Necesito la carpeta de salida.")
            return

        self.log.delete("1.0", "end")
        self.run_button.configure(state="disabled")
        thread = threading.Thread(target=self._run_export, daemon=True)
        thread.start()

    def _run_export(self) -> None:
        script_path = Path(__file__).with_name("export_gta3_map.py")
        cmd = [
            sys.executable,
            "-u",
            str(script_path),
            "--game",
            self.selected_game.get().strip(),
            "--game-root",
            self.game_root.get().strip(),
            "--output-dir",
            self.output_dir.get().strip(),
            "--format",
            self.output_format.get().strip(),
        ]

        dat_path = self.dat_path.get().strip()
        if dat_path:
            cmd.extend(["--dat", dat_path])

        extracted_root = self.extracted_root.get().strip()
        if extracted_root:
            cmd.extend(["--extracted-root", extracted_root])

        textures_dir = self.textures_dir.get().strip()
        if textures_dir:
            cmd.extend(["--textures-dir", textures_dir])

        ipl_path = self.ipl_path.get().strip()
        if ipl_path:
            cmd.extend(["--ipl", ipl_path])

        if self.include_lod.get():
            cmd.append("--include-lod")

        sector_size = self.sector_size.get().strip()
        if sector_size and sector_size != "0":
            cmd.extend(["--sector-size", sector_size])

        if self.selected_game.get().strip() == "gta4" and self.export_zone_previews.get():
            cmd.append("--export-zone-previews")

        if self.export_vehicles.get():
            cmd.extend(["--export-category", "vehicles"])
        if self.export_peds.get():
            cmd.extend(["--export-category", "peds"])
        if self.export_weapons.get():
            cmd.extend(["--export-category", "weapons"])
        if self.export_props.get():
            cmd.extend(["--export-category", "props"])
        if self.copy_ped_anims.get():
            cmd.append("--copy-ped-anims")
        if not self.export_map.get():
            cmd.append("--skip-map")
        if not self.flip_uv_v.get():
            cmd.append("--no-flip-v")
        if self.flip_uv_u.get():
            cmd.append("--flip-u")
        if self.double_sided.get():
            cmd.append("--double-sided")
        if self.extract_textures.get():
            cmd.append("--extract-textures")
            cmd.extend(["--texture-output-format", self.texture_output_format.get().strip()])

        self.root.after(0, lambda: self.append_log("Comando:\n" + " ".join(cmd) + "\n\n"))

        process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )

        assert process.stdout is not None
        for line in process.stdout:
            self.root.after(0, lambda text=line: self.append_log(text))

        code = process.wait()
        if code == 0:
            self.root.after(0, lambda: self.append_log("\nExportación terminada.\n"))
        else:
            self.root.after(0, lambda: self.append_log(f"\nLa exportación terminó con código {code}.\n"))
        self.root.after(0, lambda: self.run_button.configure(state="normal"))

    def open_output_hint(self) -> None:
        messagebox.showinfo("Salida", self.output_dir.get().strip() or "Sin ruta de salida")


def main() -> None:
    root = tk.Tk()
    ExporterGui(root)
    root.mainloop()


if __name__ == "__main__":
    main()
