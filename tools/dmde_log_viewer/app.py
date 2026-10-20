#!/usr/bin/env python3

from __future__ import annotations

import json
import os
import re
import shutil
import sys
import csv
import base64
import tkinter as tk
import subprocess
import tempfile
import time
import struct
from tkinter import filedialog, messagebox, ttk

try:
    from PIL import Image, ImageTk
except ImportError:
    Image = None
    ImageTk = None

try:
    from .core import (
        DEV_BY_ID_RE,
        EXTRACTED_NAME_RE,
        USB_ID_RE,
        DmdeLogParser,
        DmdeScanStateParser,
        EntryTreeBuilder,
        LogEntry,
        TreeNode,
        detect_file_signature,
        detect_with_file_command,
    )
except ImportError:
    from core import (  # type: ignore
        DEV_BY_ID_RE,
        EXTRACTED_NAME_RE,
        USB_ID_RE,
        DmdeLogParser,
        DmdeScanStateParser,
        EntryTreeBuilder,
        LogEntry,
        TreeNode,
        detect_file_signature,
        detect_with_file_command,
    )


class DmdeLogViewerApp:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("Visor de Logs DMDE")
        self.root.geometry("1200x760")

        self.parser = DmdeLogParser()
        self.builder = EntryTreeBuilder()
        self.scan_state_parser = DmdeScanStateParser()
        self.entries: list[LogEntry] = []
        self.tree_model = TreeNode(name="/", path="/")
        self.item_to_node: dict[str, TreeNode] = {}
        self.current_tree_root = TreeNode(name="/", path="/")
        self.source_root_var = tk.StringVar()
        self.scan_summary: dict | None = None
        self.record_annotations: dict[str, dict] = {}
        self.extracted_inventory: list[dict] = []
        self.direct_scan_records: list[dict] = []
        self.ntfs_tree_root: TreeNode | None = None
        self.ntfs_recovery_entries: dict[str, dict] = {}
        self.recovery_filter_var = tk.StringVar(value="Todos")
        self.min_size_var = tk.StringVar(value="0")
        self.fast_scan_var = tk.BooleanVar(value=True)
        self.skip_deleted_var = tk.BooleanVar(value=False)
        self.hide_deleted_var = tk.BooleanVar(value=False)
        self.last_auto_verify_message = ""
        self.scan_progress_var = tk.DoubleVar(value=0.0)
        self.scan_progress_text_var = tk.StringVar(value="")
        self.scan_eta_text_var = tk.StringVar(value="")
        self.scan_size_text_var = tk.StringVar(value="")
        self.recovery_progress_var = tk.DoubleVar(value=0.0)
        self.recovery_progress_text_var = tk.StringVar(value="")
        self.recovery_eta_text_var = tk.StringVar(value="")
        self.recovery_size_text_var = tk.StringVar(value="")
        self.stop_scan_requested = False
        self.pause_scan_requested = False
        self.cancel_recovery_requested = False
        self.scan_running = False
        self.tree_icons: dict[str, tk.PhotoImage] = {}
        self.checked_paths: set[str] = set()
        self.progress_dialog: tk.Toplevel | None = None
        self.progress_pause_button: ttk.Button | None = None
        self.recovery_dialog: tk.Toplevel | None = None
        self.recovery_cancel_button: ttk.Button | None = None
        self.tree_context_menu: tk.Menu | None = None
        self.preview_image = None
        self.preview_photo = None
        self.preview_temp_path = ""
        self.preview_media_temp_path = ""
        self.preview_video_source_path = ""
        self.scan_started_at = 0.0
        self.scan_total_bytes_hint = 0
        self.recovery_started_at = 0.0

        self.filter_var = tk.StringVar()
        self.status_var = tk.StringVar(value="Abre un log de DMDE para empezar.")

        self._apply_dark_theme()
        self._build_ui()

    def load_input_file(self, file_path: str) -> bool:
        try:
            self.entries = self.parser.parse_file(file_path)
            self.tree_model = self.builder.build(self.entries)
            self.scan_summary = self.scan_state_parser.build_summary(
                self.scan_state_parser.parse_assignments(file_path)
            )
        except OSError as exc:
            messagebox.showerror("Error al abrir", str(exc))
            return False

        self.refresh_tree()
        real_path_entries = [entry for entry in self.entries if entry.source_kind == "path"]
        if real_path_entries:
            self.status_var.set(
                f"Cargadas {len(real_path_entries)} rutas desde {os.path.basename(file_path)}."
            )
            return True

        self.tree_model = self._build_summary_tree()
        self.refresh_tree(use_existing_tree=True)
        inferred_source = self.source_root_var.get().strip() or self._infer_source_device_from_scan()
        if inferred_source:
            self.source_root_var.set(inferred_source)
            self._auto_verify_types_if_possible()
        if self.last_auto_verify_message:
            self.status_var.set(self.last_auto_verify_message)
        else:
            self.status_var.set(
                "Este log parece contener solo estado del escaneo. No se encontraron rutas reales."
            )
        return True

    def _build_ui(self) -> None:
        toolbar = ttk.Frame(self.root, padding=8)
        toolbar.pack(fill=tk.X)

        ttk.Button(toolbar, text="Abrir log", command=self.open_log).pack(side=tk.LEFT)
        ttk.Button(toolbar, text="Abrir disco", command=self.open_device_picker).pack(side=tk.LEFT, padx=(8, 0))
        ttk.Button(toolbar, text="Utilidad disco", command=self.open_disk_utility).pack(side=tk.LEFT, padx=(8, 0))
        ttk.Button(toolbar, text="Abrir manifiesto", command=self.open_manifest).pack(side=tk.LEFT, padx=(8, 0))
        ttk.Button(toolbar, text="Escaneo directo", command=self.direct_device_scan).pack(side=tk.LEFT, padx=(8, 0))
        ttk.Button(toolbar, text="Escaneo NTFS", command=self.ntfs_low_level_scan).pack(side=tk.LEFT, padx=(8, 0))
        self.stop_scan_button = ttk.Button(toolbar, text="Stop", command=self.stop_direct_scan, state=tk.DISABLED)
        self.stop_scan_button.pack(side=tk.LEFT, padx=(8, 0))
        ttk.Button(toolbar, text="Guardar sesion", command=self.save_scan_session_as).pack(side=tk.LEFT, padx=(8, 0))
        ttk.Button(toolbar, text="Analizar extraidos", command=self.analyze_extracted_folder).pack(side=tk.LEFT, padx=(8, 0))
        ttk.Button(toolbar, text="Recuperar seleccionados", command=self.recover_selected).pack(side=tk.LEFT, padx=(8, 0))
        ttk.Checkbutton(toolbar, text="Modo turbo", variable=self.fast_scan_var).pack(side=tk.LEFT, padx=(12, 0))
        ttk.Checkbutton(toolbar, text="Omitir borrados al recuperar", variable=self.skip_deleted_var).pack(side=tk.LEFT, padx=(12, 0))
        ttk.Checkbutton(toolbar, text="Ocultar borrados", variable=self.hide_deleted_var, command=self._refresh_summary_tree).pack(side=tk.LEFT, padx=(12, 0))
        ttk.Label(toolbar, text="Filtro rapido:").pack(side=tk.LEFT, padx=(16, 4))
        filter_entry = ttk.Entry(toolbar, textvariable=self.filter_var, width=40)
        filter_entry.pack(side=tk.LEFT, fill=tk.X, expand=True)
        filter_entry.bind("<KeyRelease>", lambda _event: self.refresh_tree())

        source_bar = ttk.Frame(self.root, padding=(8, 0, 8, 8))
        source_bar.pack(fill=tk.X)
        ttk.Label(source_bar, text="Disco detectado:").pack(side=tk.LEFT)
        ttk.Entry(source_bar, textvariable=self.source_root_var, width=80).pack(
            side=tk.LEFT, fill=tk.X, expand=True, padx=(8, 8)
        )

        raw_filter_bar = ttk.Frame(self.root, padding=(8, 0, 8, 8))
        raw_filter_bar.pack(fill=tk.X)
        ttk.Label(raw_filter_bar, text="Biblioteca:").pack(side=tk.LEFT)
        class_combo = ttk.Combobox(
            raw_filter_bar,
            textvariable=self.recovery_filter_var,
            values=[
                "Todos",
                "Videos",
                "Imagenes",
                "Documentos",
                "Comprimidos",
                "Desconocidos",
            ],
            width=22,
            state="readonly",
        )
        class_combo.pack(side=tk.LEFT, padx=(6, 12))
        class_combo.bind("<<ComboboxSelected>>", lambda _event: self._refresh_summary_tree())

        ttk.Label(raw_filter_bar, text="Tamano minimo (bytes):").pack(side=tk.LEFT)
        min_size_entry = ttk.Entry(raw_filter_bar, textvariable=self.min_size_var, width=14)
        min_size_entry.pack(side=tk.LEFT, padx=(6, 12))
        min_size_entry.bind("<KeyRelease>", lambda _event: self._refresh_summary_tree())

        body = ttk.Panedwindow(self.root, orient=tk.HORIZONTAL)
        body.pack(fill=tk.BOTH, expand=True)

        left_frame = ttk.Frame(body, padding=8)
        right_frame = ttk.Frame(body, padding=8)
        body.add(left_frame, weight=3)
        body.add(right_frame, weight=2)

        self._create_tree_icons()

        tree_container = ttk.Frame(left_frame)
        tree_container.pack(fill=tk.BOTH, expand=True)

        self.tree = ttk.Treeview(tree_container, columns=("type", "status", "size", "source"), show="tree headings")
        self.tree.heading("#0", text="Ruta")
        self.tree.heading("type", text="Tipo")
        self.tree.heading("status", text="Estado")
        self.tree.heading("size", text="Tamano")
        self.tree.heading("source", text="Lineas")
        self.tree.column("#0", width=390, stretch=True)
        self.tree.column("type", width=160, anchor=tk.W)
        self.tree.column("status", width=140, anchor=tk.W)
        self.tree.column("size", width=120, anchor=tk.E)
        self.tree.column("source", width=90, anchor=tk.E)
        self.tree.configure(style="Treeview")
        tree_scroll_y = ttk.Scrollbar(tree_container, orient=tk.VERTICAL, command=self.tree.yview)
        tree_scroll_x = ttk.Scrollbar(tree_container, orient=tk.HORIZONTAL, command=self.tree.xview)
        self.tree.configure(yscrollcommand=tree_scroll_y.set, xscrollcommand=tree_scroll_x.set)
        self.tree.grid(row=0, column=0, sticky="nsew")
        tree_scroll_y.grid(row=0, column=1, sticky="ns")
        tree_scroll_x.grid(row=1, column=0, sticky="ew")
        tree_container.columnconfigure(0, weight=1)
        tree_container.rowconfigure(0, weight=1)
        self.tree.bind("<<TreeviewSelect>>", self.on_tree_select)
        self.tree.bind("<Button-1>", self.on_tree_click)
        self.tree.bind("<Button-3>", self.on_tree_context_menu)
        self.tree.bind("<space>", self.on_tree_toggle_check)
        self._create_tree_context_menu()

        preview_label = ttk.Label(right_frame, text="Vista previa")
        preview_label.pack(anchor=tk.W)

        preview_frame = ttk.Frame(right_frame)
        preview_frame.pack(fill=tk.BOTH, expand=True, pady=(0, 8))
        preview_frame.pack_propagate(False)
        preview_frame.configure(height=320)

        self.preview_canvas = tk.Label(
            preview_frame,
            text="Sin vista previa",
            anchor=tk.CENTER,
            justify=tk.CENTER,
            bg="#000000",
            fg="#f5f5f5",
            relief=tk.SUNKEN,
        )
        self.preview_canvas.pack(fill=tk.BOTH, expand=True)

        preview_controls = ttk.Frame(right_frame)
        preview_controls.pack(fill=tk.X, pady=(0, 8))
        self.preview_play_button = ttk.Button(
            preview_controls,
            text="Reproducir video",
            command=self.play_preview_video,
            state=tk.DISABLED,
        )
        self.preview_play_button.pack(side=tk.LEFT)

        raw_label = ttk.Label(right_frame, text="Lineas crudas asociadas")
        raw_label.pack(anchor=tk.W)

        self.raw_text = tk.Text(right_frame, wrap=tk.WORD, height=18)
        self.raw_text.pack(fill=tk.BOTH, expand=True)

        info_label = ttk.Label(right_frame, text="Detalles del nodo")
        info_label.pack(anchor=tk.W, pady=(8, 0))

        self.info_text = tk.Text(right_frame, wrap=tk.WORD, height=10)
        self.info_text.pack(fill=tk.BOTH, expand=True)

        status_bar = ttk.Label(self.root, textvariable=self.status_var, padding=8, anchor=tk.W)
        status_bar.pack(fill=tk.X)

    def _apply_dark_theme(self) -> None:
        bg = "#000000"
        panel = "#0a0a0a"
        panel_alt = "#050505"
        fg = "#f5f5f5"
        muted = "#a3a3a3"
        accent = "#2a2a2a"

        self.root.configure(bg=bg)
        style = ttk.Style()
        style.theme_use("clam")
        style.configure(".", background=bg, foreground=fg, fieldbackground=panel, bordercolor=panel)
        style.configure("TFrame", background=bg)
        style.configure("TPanedwindow", background=bg)
        style.configure("TLabel", background=bg, foreground=fg)
        style.configure("TButton", background=panel, foreground=fg, borderwidth=1, focusthickness=0)
        style.map("TButton", background=[("active", "#1a1a1a"), ("disabled", "#141414")], foreground=[("disabled", muted)])
        style.configure("TEntry", fieldbackground=panel, foreground=fg)
        style.configure("TCombobox", fieldbackground=panel, foreground=fg, arrowsize=16)
        style.configure(
            "TProgressbar",
            troughcolor="#0a0a0a",
            background="#16a34a",
            bordercolor="#0a0a0a",
            lightcolor="#16a34a",
            darkcolor="#16a34a",
        )
        style.configure(
            "Treeview",
            background=panel_alt,
            fieldbackground=panel_alt,
            foreground=fg,
            rowheight=40,
            font=("DejaVu Sans", 13),
        )
        style.configure("Treeview.Heading", background=panel, foreground=fg)
        style.map("Treeview", background=[("selected", "#202020")], foreground=[("selected", "#ffffff")])

    def open_log(self) -> None:
        file_path = filedialog.askopenfilename(
            title="Abrir log de DMDE",
            filetypes=[("Text files", "*.log *.txt *.csv"), ("All files", "*.*")],
        )
        if not file_path:
            return

        if not self.load_input_file(file_path):
            return
        if not any(entry.source_kind == "path" for entry in self.entries):
            messagebox.showwarning(
                "No hay arbol de archivos",
                "Este log de DMDE parece un fichero interno de estado del escaneo, no una lista exportada.\n\n"
                "Contiene metadatos del dispositivo y estructuras codificadas, pero no rutas claras de archivos o carpetas "
                "que el visor pueda reconstruir como arbol.",
            )
            self._show_scan_summary()

    def open_device_picker(self) -> None:
        devices = self._list_scanable_devices()
        if not devices:
            messagebox.showinfo("Sin discos", "No se encontraron discos o particiones conectados.")
            return

        picker = tk.Toplevel(self.root)
        picker.title("Elegir disco o particion")
        picker.geometry("920x440")
        picker.minsize(920, 440)
        picker.transient(self.root)
        picker.grab_set()
        picker.configure(bg="#000000")

        container = ttk.Frame(picker, padding=8)
        container.pack(fill=tk.BOTH, expand=True)
        container.columnconfigure(0, weight=1)
        container.rowconfigure(0, weight=1)

        columns = ("path", "type", "size", "fstype", "mount", "model")
        tree_frame = ttk.Frame(container)
        tree_frame.grid(row=0, column=0, sticky="nsew")
        tree_frame.columnconfigure(0, weight=1)
        tree_frame.rowconfigure(0, weight=1)

        tree = ttk.Treeview(tree_frame, columns=columns, show="headings")
        tree.heading("path", text="Dispositivo")
        tree.heading("type", text="Tipo")
        tree.heading("size", text="Tamano")
        tree.heading("fstype", text="FS")
        tree.heading("mount", text="Montaje")
        tree.heading("model", text="Modelo")
        tree.column("path", width=180, anchor=tk.W)
        tree.column("type", width=80, anchor=tk.W)
        tree.column("size", width=110, anchor=tk.E)
        tree.column("fstype", width=90, anchor=tk.W)
        tree.column("mount", width=120, anchor=tk.W)
        tree.column("model", width=280, anchor=tk.W)
        tree_scroll_y = ttk.Scrollbar(tree_frame, orient=tk.VERTICAL, command=tree.yview)
        tree_scroll_x = ttk.Scrollbar(tree_frame, orient=tk.HORIZONTAL, command=tree.xview)
        tree.configure(yscrollcommand=tree_scroll_y.set, xscrollcommand=tree_scroll_x.set)
        tree.grid(row=0, column=0, sticky="nsew")
        tree_scroll_y.grid(row=0, column=1, sticky="ns")
        tree_scroll_x.grid(row=1, column=0, sticky="ew")

        item_to_path: dict[str, str] = {}
        for device in devices:
            item_id = tree.insert(
                "",
                tk.END,
                values=(
                    device["path"],
                    device["type"],
                    self._format_size_compact(int(device["size"])),
                    device["fstype"],
                    device["mountpoint"],
                    device["label"],
                ),
            )
            item_to_path[item_id] = device["path"]

        def choose_selected() -> None:
            selection = tree.selection()
            if not selection:
                return
            picked_path = item_to_path.get(selection[0], "")
            if not picked_path:
                return
            self.source_root_var.set(picked_path)
            self.status_var.set(f"Origen seleccionado: {picked_path}")
            picker.destroy()

        button_row = ttk.Frame(container, padding=(0, 10, 0, 0))
        button_row.grid(row=1, column=0, sticky="ew")
        ttk.Button(button_row, text="Usar origen", command=choose_selected).pack(side=tk.RIGHT)
        ttk.Button(button_row, text="Cancelar", command=picker.destroy).pack(side=tk.RIGHT, padx=(0, 8))
        tree.bind("<Double-1>", lambda _event: choose_selected())
        picker.update_idletasks()

    def open_disk_utility(self) -> None:
        devices = self._list_scanable_devices()
        if not devices:
            messagebox.showinfo("Sin discos", "No se encontraron discos o particiones conectados.")
            return

        dialog = tk.Toplevel(self.root)
        dialog.title("Utilidad de disco")
        dialog.geometry("1120x720")
        dialog.minsize(980, 640)
        dialog.transient(self.root)
        dialog.grab_set()
        dialog.configure(bg="#000000")

        selected_path_var = tk.StringVar(value=self.source_root_var.get().strip())
        status_var = tk.StringVar(value="Selecciona una particion o disco y usa una accion segura.")

        container = ttk.Frame(dialog, padding=10)
        container.pack(fill=tk.BOTH, expand=True)
        container.columnconfigure(0, weight=3)
        container.columnconfigure(1, weight=2)
        container.rowconfigure(2, weight=1)

        header = ttk.Frame(container)
        header.grid(row=0, column=0, columnspan=2, sticky="ew", pady=(0, 8))
        header.columnconfigure(1, weight=1)
        ttk.Label(header, text="Dispositivo seleccionado:").grid(row=0, column=0, sticky="w")
        ttk.Entry(header, textvariable=selected_path_var).grid(row=0, column=1, sticky="ew", padx=(8, 0))

        left_frame = ttk.Frame(container)
        left_frame.grid(row=1, column=0, rowspan=2, sticky="nsew", padx=(0, 8))
        left_frame.columnconfigure(0, weight=1)
        left_frame.rowconfigure(0, weight=1)

        columns = ("path", "type", "size", "fstype", "mount", "model")
        tree = ttk.Treeview(left_frame, columns=columns, show="headings")
        tree.heading("path", text="Dispositivo")
        tree.heading("type", text="Tipo")
        tree.heading("size", text="Tamano")
        tree.heading("fstype", text="FS")
        tree.heading("mount", text="Montaje")
        tree.heading("model", text="Modelo")
        tree.column("path", width=190, anchor=tk.W)
        tree.column("type", width=70, anchor=tk.W)
        tree.column("size", width=110, anchor=tk.E)
        tree.column("fstype", width=90, anchor=tk.W)
        tree.column("mount", width=140, anchor=tk.W)
        tree.column("model", width=260, anchor=tk.W)
        tree_scroll_y = ttk.Scrollbar(left_frame, orient=tk.VERTICAL, command=tree.yview)
        tree_scroll_x = ttk.Scrollbar(left_frame, orient=tk.HORIZONTAL, command=tree.xview)
        tree.configure(yscrollcommand=tree_scroll_y.set, xscrollcommand=tree_scroll_x.set)
        tree.grid(row=0, column=0, sticky="nsew")
        tree_scroll_y.grid(row=0, column=1, sticky="ns")
        tree_scroll_x.grid(row=1, column=0, sticky="ew")

        info_panel = ttk.Frame(container)
        info_panel.grid(row=1, column=1, sticky="nsew")
        info_panel.columnconfigure(0, weight=1)
        info_panel.rowconfigure(1, weight=1)
        ttk.Label(
            info_panel,
            text=(
                "Pensado para recuperacion: primero diagnostica, luego prueba montaje en solo lectura.\n"
                "La reparacion NTFS puede modificar metadatos; usala solo si entiendes el riesgo."
            ),
            wraplength=360,
            justify=tk.LEFT,
        ).grid(row=0, column=0, sticky="ew", pady=(0, 8))

        output_text = tk.Text(info_panel, wrap=tk.WORD, height=24)
        output_text.grid(row=1, column=0, sticky="nsew")
        output_scroll = ttk.Scrollbar(info_panel, orient=tk.VERTICAL, command=output_text.yview)
        output_text.configure(yscrollcommand=output_scroll.set)
        output_scroll.grid(row=1, column=1, sticky="ns")

        button_bar = ttk.Frame(container)
        button_bar.grid(row=3, column=0, columnspan=2, sticky="ew", pady=(10, 0))

        item_to_device: dict[str, dict[str, str | int]] = {}
        path_to_item: dict[str, str] = {}
        for device in devices:
            item_id = tree.insert(
                "",
                tk.END,
                values=(
                    device["path"],
                    device["type"],
                    self._format_size_compact(int(device["size"])),
                    device["fstype"],
                    device["mountpoint"],
                    device["label"],
                ),
            )
            item_to_device[item_id] = device
            path_to_item[str(device["path"])] = item_id

        def append_output(title: str, text: str) -> None:
            output_text.insert(tk.END, f"\n[{title}]\n{text.strip() or '(sin salida)'}\n")
            output_text.see(tk.END)

        def selected_device() -> dict[str, str | int] | None:
            selection = tree.selection()
            if selection:
                item = selection[0]
                device = item_to_device.get(item)
                if device:
                    selected_path_var.set(str(device["path"]))
                    return device
            selected_path = selected_path_var.get().strip()
            for device in devices:
                if str(device["path"]) == selected_path:
                    return device
            return None

        def require_selection(require_partition: bool = False) -> dict[str, str | int] | None:
            device = selected_device()
            if device is None:
                messagebox.showwarning("Selecciona un dispositivo", "Elige un disco o particion primero.", parent=dialog)
                return None
            if require_partition and str(device.get("type") or "") != "part":
                messagebox.showwarning(
                    "Selecciona una particion",
                    "Para montar o reparar conviene elegir una particion concreta, no el disco completo.",
                    parent=dialog,
                )
                return None
            return device

        def run_command(title: str, command: list[str]) -> tuple[bool, str]:
            try:
                result = subprocess.run(command, capture_output=True, text=True, check=False)
            except FileNotFoundError:
                message = f"No se encontro el comando: {command[0]}"
                append_output(title, message)
                status_var.set(message)
                return False, message
            except PermissionError as exc:
                message = f"Permiso denegado. Prueba lanzando la app con sudo.\n\n{exc}"
                append_output(title, message)
                status_var.set("Operacion bloqueada por permisos.")
                return False, message
            except OSError as exc:
                message = str(exc)
                append_output(title, message)
                status_var.set("Fallo al ejecutar la utilidad.")
                return False, message

            stdout = result.stdout.strip()
            stderr = result.stderr.strip()
            chunks = [part for part in [stdout, stderr] if part]
            text = "\n\n".join(chunks) if chunks else f"Codigo de salida: {result.returncode}"
            append_output(title, text)
            status_var.set(f"{title}: codigo {result.returncode}")
            return result.returncode == 0, text

        def set_source_from_selection() -> None:
            device = require_selection()
            if device is None:
                return
            path = str(device["path"])
            self.source_root_var.set(path)
            selected_path_var.set(path)
            status_var.set(f"Origen activo actualizado a {path}.")
            self.status_var.set(f"Origen seleccionado: {path}")

        def diagnose_selected() -> None:
            device = require_selection()
            if device is None:
                return
            path = str(device["path"])
            status_var.set(f"Diagnosticando {path}...")
            append_output(
                "Resumen del dispositivo",
                "\n".join(
                    [
                        f"Ruta: {device['path']}",
                        f"Tipo: {device['type']}",
                        f"Tamano: {self._format_size_compact(int(device['size']))}",
                        f"Sistema de archivos: {device['fstype'] or '(sin detectar)'}",
                        f"Montaje: {device['mountpoint'] or '(sin montar)'}",
                        f"Modelo: {device['label'] or '(sin modelo)'}",
                    ]
                ),
            )
            run_command("lsblk", ["lsblk", "-f", path])
            run_command("blkid", ["blkid", path])

        def mount_read_only() -> None:
            device = require_selection(require_partition=True)
            if device is None:
                return
            path = str(device["path"])
            mount_dir = os.path.join(tempfile.gettempdir(), f"dmde_mount_{os.path.basename(path).replace('/', '_')}")
            os.makedirs(mount_dir, exist_ok=True)
            status_var.set(f"Intentando montar {path} en solo lectura...")
            ok, _ = run_command("Montaje solo lectura", ["mount", "-o", "ro", path, mount_dir])
            if ok:
                selected_path_var.set(path)
                status_var.set(f"Montado en solo lectura en {mount_dir}")
                self.status_var.set(f"{path} montado en solo lectura en {mount_dir}")
                append_output("Montaje listo", f"Si necesitas copiar contenido, revisa:\n{mount_dir}")

        def unmount_selected() -> None:
            device = require_selection(require_partition=True)
            if device is None:
                return
            mountpoint = str(device.get("mountpoint") or "").strip()
            if not mountpoint:
                mountpoint = os.path.join(
                    tempfile.gettempdir(),
                    f"dmde_mount_{os.path.basename(str(device['path'])).replace('/', '_')}",
                )
            status_var.set(f"Intentando desmontar {mountpoint}...")
            ok, _ = run_command("Desmontar", ["umount", mountpoint])
            if ok:
                status_var.set(f"Desmontado: {mountpoint}")

        def repair_ntfs() -> None:
            device = require_selection(require_partition=True)
            if device is None:
                return
            fstype = str(device.get("fstype") or "").lower()
            if "ntfs" not in fstype:
                messagebox.showwarning(
                    "Solo NTFS",
                    "La reparacion integrada de esta utilidad esta limitada a particiones NTFS mediante ntfsfix.",
                    parent=dialog,
                )
                return
            path = str(device["path"])
            confirmed = messagebox.askyesno(
                "Reparar NTFS",
                "Esto ejecutara ntfsfix y puede modificar metadatos del sistema de archivos.\n\n"
                "Hazlo solo si ya has intentado recuperar los datos o entiendes el riesgo.\n\n"
                f"Particion: {path}\n\nQuieres continuar?",
                parent=dialog,
            )
            if not confirmed:
                return
            status_var.set(f"Reparando NTFS en {path}...")
            ok, _ = run_command("ntfsfix", ["ntfsfix", path])
            if ok:
                self.status_var.set(f"ntfsfix completado en {path}.")
                messagebox.showinfo(
                    "Reparacion terminada",
                    "ntfsfix ha terminado. Revisa la salida y prueba primero a montar en solo lectura.",
                    parent=dialog,
                )

        def update_selection(_event: object | None = None) -> None:
            device = selected_device()
            if device is None:
                return
            selected_path_var.set(str(device["path"]))
            status_var.set(
                f"Seleccionado: {device['path']} ({device['fstype'] or 'sin FS detectado'})"
            )

        tree.bind("<<TreeviewSelect>>", update_selection)
        tree.bind("<Double-1>", lambda _event: set_source_from_selection())

        ttk.Button(button_bar, text="Usar como origen", command=set_source_from_selection).pack(side=tk.LEFT)
        ttk.Button(button_bar, text="Diagnosticar", command=diagnose_selected).pack(side=tk.LEFT, padx=(8, 0))
        ttk.Button(button_bar, text="Montar solo lectura", command=mount_read_only).pack(side=tk.LEFT, padx=(8, 0))
        ttk.Button(button_bar, text="Desmontar", command=unmount_selected).pack(side=tk.LEFT, padx=(8, 0))
        ttk.Button(button_bar, text="Reparar NTFS", command=repair_ntfs).pack(side=tk.LEFT, padx=(8, 0))
        ttk.Button(button_bar, text="Cerrar", command=dialog.destroy).pack(side=tk.RIGHT)

        footer = ttk.Label(container, textvariable=status_var, anchor=tk.W, padding=(0, 8, 0, 0))
        footer.grid(row=4, column=0, columnspan=2, sticky="ew")

        initial_path = selected_path_var.get().strip()
        if initial_path and initial_path in path_to_item:
            tree.selection_set(path_to_item[initial_path])
            tree.focus(path_to_item[initial_path])
            tree.see(path_to_item[initial_path])
            update_selection()
        elif devices:
            first_item = next(iter(item_to_device))
            tree.selection_set(first_item)
            tree.focus(first_item)
            tree.see(first_item)
            update_selection()

    def choose_recovery_destination(self) -> str:
        devices = [
            device
            for device in self._list_scanable_devices()
            if str(device.get("mountpoint") or "").strip()
        ]

        selected_destination = {"path": ""}

        picker = tk.Toplevel(self.root)
        picker.title("Elegir destino de recuperacion")
        picker.geometry("980x460")
        picker.minsize(980, 460)
        picker.transient(self.root)
        picker.grab_set()
        picker.configure(bg="#000000")

        container = ttk.Frame(picker, padding=8)
        container.pack(fill=tk.BOTH, expand=True)
        container.columnconfigure(0, weight=1)
        container.rowconfigure(1, weight=1)

        ttk.Label(
            container,
            text="Puedes recuperar en un disco montado o elegir una carpeta manualmente.",
        ).grid(row=0, column=0, sticky="w", pady=(0, 8))

        tree_frame = ttk.Frame(container)
        tree_frame.grid(row=1, column=0, sticky="nsew")
        tree_frame.columnconfigure(0, weight=1)
        tree_frame.rowconfigure(0, weight=1)

        columns = ("path", "type", "size", "fstype", "mount", "model")
        tree = ttk.Treeview(tree_frame, columns=columns, show="headings")
        tree.heading("path", text="Dispositivo")
        tree.heading("type", text="Tipo")
        tree.heading("size", text="Tamano")
        tree.heading("fstype", text="FS")
        tree.heading("mount", text="Montaje")
        tree.heading("model", text="Modelo")
        tree.column("path", width=180, anchor=tk.W)
        tree.column("type", width=80, anchor=tk.W)
        tree.column("size", width=110, anchor=tk.E)
        tree.column("fstype", width=90, anchor=tk.W)
        tree.column("mount", width=220, anchor=tk.W)
        tree.column("model", width=240, anchor=tk.W)
        tree_scroll_y = ttk.Scrollbar(tree_frame, orient=tk.VERTICAL, command=tree.yview)
        tree_scroll_x = ttk.Scrollbar(tree_frame, orient=tk.HORIZONTAL, command=tree.xview)
        tree.configure(yscrollcommand=tree_scroll_y.set, xscrollcommand=tree_scroll_x.set)
        tree.grid(row=0, column=0, sticky="nsew")
        tree_scroll_y.grid(row=0, column=1, sticky="ns")
        tree_scroll_x.grid(row=1, column=0, sticky="ew")

        item_to_destination: dict[str, str] = {}
        for device in devices:
            mountpoint = str(device.get("mountpoint") or "").strip()
            item_id = tree.insert(
                "",
                tk.END,
                values=(
                    device["path"],
                    device["type"],
                    self._format_size_compact(int(device["size"])),
                    device["fstype"],
                    mountpoint,
                    device["label"],
                ),
            )
            item_to_destination[item_id] = mountpoint

        def choose_selected_mount() -> None:
            selection = tree.selection()
            if not selection:
                return
            picked_path = item_to_destination.get(selection[0], "")
            if not picked_path:
                return
            selected_destination["path"] = picked_path
            picker.destroy()

        def choose_manual_folder() -> None:
            picked_path = filedialog.askdirectory(
                title="Elegir destino de recuperacion",
                parent=picker,
            )
            if not picked_path:
                return
            selected_destination["path"] = picked_path
            picker.destroy()

        button_row = ttk.Frame(container, padding=(0, 10, 0, 0))
        button_row.grid(row=2, column=0, sticky="ew")
        ttk.Button(button_row, text="Elegir carpeta...", command=choose_manual_folder).pack(side=tk.LEFT)
        ttk.Button(button_row, text="Usar disco montado", command=choose_selected_mount).pack(side=tk.RIGHT)
        ttk.Button(button_row, text="Cancelar", command=picker.destroy).pack(side=tk.RIGHT, padx=(0, 8))
        tree.bind("<Double-1>", lambda _event: choose_selected_mount())
        picker.wait_window()
        return selected_destination["path"]

    def _list_scanable_devices(self) -> list[dict[str, str | int]]:
        try:
            result = subprocess.run(
                ["lsblk", "-J", "-b", "-o", "PATH,TYPE,SIZE,FSTYPE,MOUNTPOINT,MODEL,SERIAL"],
                capture_output=True,
                text=True,
                check=True,
            )
            payload = json.loads(result.stdout)
        except (subprocess.SubprocessError, json.JSONDecodeError, FileNotFoundError):
            return []

        devices: list[dict[str, str | int]] = []

        def walk(nodes: list[dict]) -> None:
            for node in nodes:
                path = node.get("path") or ""
                node_type = node.get("type") or ""
                if path.startswith("/dev/") and node_type in {"disk", "part"}:
                    devices.append(
                        {
                            "path": path,
                            "type": node_type,
                            "size": int(node.get("size") or 0),
                            "fstype": node.get("fstype") or "",
                            "mountpoint": node.get("mountpoint") or "",
                            "label": " ".join(
                                part for part in [(node.get("model") or "").strip(), (node.get("serial") or "").strip()] if part
                            ),
                        }
                    )
                walk(node.get("children") or [])

        walk(payload.get("blockdevices", []))
        devices.sort(key=lambda item: (item["type"] != "disk", str(item["path"])))
        return devices

    def _format_size_compact(self, size: int) -> str:
        units = ["B", "KB", "MB", "GB", "TB", "PB"]
        value = float(size)
        unit_index = 0
        while value >= 1024.0 and unit_index < len(units) - 1:
            value /= 1024.0
            unit_index += 1
        if unit_index == 0:
            return f"{int(value)} {units[unit_index]}"
        return f"{value:.1f} {units[unit_index]}"

    def open_manifest(self) -> None:
        file_path = filedialog.askopenfilename(
            title="Abrir manifiesto o sesion",
            filetypes=[("JSON files", "*.json"), ("All files", "*.*")],
        )
        if not file_path:
            return

        try:
            with open(file_path, "r", encoding="utf-8") as handle:
                payload = json.load(handle)
        except json.JSONDecodeError as exc:
            messagebox.showerror(
                "Error al abrir",
                "La sesion o manifiesto JSON parece incompleto o corrupto.\n\n"
                f"Detalle tecnico: {exc}",
            )
            return
        except OSError as exc:
            messagebox.showerror("Error al abrir", str(exc))
            return

        if payload.get("format") == "dmde-scan-session-v1":
            self._load_scan_session_payload(payload, os.path.basename(file_path))
            return

        tree_payload = payload.get("tree")
        if not isinstance(tree_payload, dict):
            messagebox.showerror("Manifiesto invalido", "El JSON seleccionado no contiene un arbol valido.")
            return

        self.entries = []
        self.tree_model = self._deserialize_node(tree_payload)
        self.refresh_tree(use_existing_tree=True)
        self.status_var.set(f"Manifiesto cargado desde {os.path.basename(file_path)}.")

    def refresh_tree(self, use_existing_tree: bool = False) -> None:
        filter_text = self.filter_var.get().strip().lower()
        open_paths = self._collect_open_tree_paths()
        selected_paths = self._collect_selected_tree_paths()
        focused_path = self._focused_tree_path()
        self.tree.delete(*self.tree.get_children())
        self.item_to_node.clear()
        path_to_item: dict[str, str] = {}

        visible_root = self.tree_model if use_existing_tree and self.tree_model.children else self._filtered_tree_model()
        self.current_tree_root = visible_root
        for child in sorted(visible_root.children.values(), key=self._sort_key):
            self._insert_node("", child, filter_text, open_paths, path_to_item)

        if selected_paths:
            selected_item_ids = [path_to_item[path] for path in selected_paths if path in path_to_item]
            if selected_item_ids:
                self.tree.selection_set(selected_item_ids)
        if focused_path and focused_path in path_to_item:
            self.tree.focus(path_to_item[focused_path])

    def _collect_open_tree_paths(self) -> set[str]:
        open_paths: set[str] = set()

        def walk(item_id: str) -> None:
            node = self.item_to_node.get(item_id)
            if node is not None and self.tree.item(item_id, "open"):
                open_paths.add(node.path)
            for child_id in self.tree.get_children(item_id):
                walk(child_id)

        for item_id in self.tree.get_children():
            walk(item_id)
        return open_paths

    def _collect_selected_tree_paths(self) -> list[str]:
        paths: list[str] = []
        for item_id in self.tree.selection():
            node = self.item_to_node.get(item_id)
            if node is not None:
                paths.append(node.path)
        return paths

    def _focused_tree_path(self) -> str:
        item_id = self.tree.focus()
        node = self.item_to_node.get(item_id)
        return node.path if node is not None else ""

    def _filtered_tree_model(self) -> TreeNode:
        root = TreeNode(name="/", path="/", is_file=False)
        for entry in self.entries:
            if entry.source_kind != "path":
                continue
            parts = [part for part in entry.path.split("/") if part]
            if not parts:
                continue
            current = root
            current_path = ""
            for index, part in enumerate(parts):
                current_path = f"{current_path}/{part}"
                is_leaf = index == len(parts) - 1
                child = current.children.get(part)
                if child is None:
                    child = TreeNode(
                        name=part,
                        path=current_path,
                        is_file=is_leaf and entry.is_file,
                    )
                    current.add_child(child)
                if is_leaf:
                    child.is_file = entry.is_file
                    child.raw_lines.append(entry.raw_line)
                    child.line_numbers.append(entry.line_number)
                current = child
        return root

    def _record_matches_filters(self, record: dict) -> bool:
        selected_view = self.recovery_filter_var.get().strip()
        if selected_view and selected_view != "Todos":
            category = self._category_for_record(record)
            if category != selected_view:
                return False

        try:
            min_size = int(self.min_size_var.get().strip() or "0")
        except ValueError:
            min_size = 0
        if record.get("estimated_size_bytes", 0) < min_size:
            return False
        return True

    def _refresh_summary_tree(self) -> None:
        if self.scan_summary is None and not self.direct_scan_records and not self.ntfs_tree_root:
            return
        self.tree_model = self._build_summary_tree()
        self.refresh_tree(use_existing_tree=True)

    def _record_sort_key(self, record: dict) -> tuple[int, int, int]:
        return (
            -record.get("score", 0),
            -record.get("estimated_size_bytes", 0),
            record.get("start_offset", 0),
        )

    def _insert_node(
        self,
        parent_id: str,
        node: TreeNode,
        filter_text: str,
        open_paths: set[str] | None = None,
        path_to_item: dict[str, str] | None = None,
    ) -> bool:
        if filter_text and not self._node_matches_filter(node, filter_text):
            return False

        icon = self._icon_for_node(node)
        checkbox_prefix = "☑  " if node.path in self.checked_paths else "☐  "
        is_open = node.path in open_paths if open_paths else not filter_text
        item_id = self.tree.insert(
            parent_id,
            tk.END,
            text=f"{checkbox_prefix}{node.name}",
            image=icon,
            values=(
                self._display_type_for_node(node),
                self._display_status_for_node(node),
                self._format_size_compact(self._node_estimated_total_bytes(node)),
                len(node.raw_lines),
            ),
            open=is_open,
        )
        self.item_to_node[item_id] = node
        if path_to_item is not None:
            path_to_item[node.path] = item_id

        for child in sorted(node.children.values(), key=self._sort_key):
            self._insert_node(item_id, child, filter_text, open_paths, path_to_item)
        return True

    def on_tree_click(self, event: object) -> str | None:
        if not hasattr(event, "x") or not hasattr(event, "y"):
            return None
        item_id = self.tree.identify_row(event.y)
        if not item_id:
            return None
        element = self.tree.identify("element", event.x, event.y)
        if element == "Treeitem.indicator":
            return None
        self.tree.selection_set(item_id)
        self.on_tree_toggle_check(event)
        return "break"

    def on_tree_toggle_check(self, event: object) -> None:
        selection = self.tree.selection()
        item_id = selection[0] if selection else ""
        if not item_id and hasattr(event, "x") and hasattr(event, "y"):
            item_id = self.tree.identify_row(event.y)
        if not item_id:
            return
        node = self.item_to_node.get(item_id)
        if node is None:
            return
        should_check = node.path not in self.checked_paths
        self._set_checked_recursive(node, should_check)
        self.refresh_tree(use_existing_tree=True)
        self.tree.selection_set(item_id)

    def _create_tree_context_menu(self) -> None:
        menu = tk.Menu(self.root, tearoff=False, bg="#050505", fg="#f5f5f5", activebackground="#202020", activeforeground="#ffffff")
        menu.add_command(label="Marcar", command=self.context_mark_selected)
        menu.add_command(label="Desmarcar", command=self.context_unmark_selected)
        menu.add_separator()
        menu.add_command(label="Marcar todo dentro", command=self.context_mark_subtree)
        menu.add_command(label="Desmarcar todo dentro", command=self.context_unmark_subtree)
        menu.add_separator()
        menu.add_command(label="Recuperar este nodo", command=self.context_recover_selected_node)
        menu.add_command(label="Recuperar seleccionados aqui", command=self.context_recover_current_selection)
        menu.add_command(label="Vista previa grande", command=self.context_open_large_preview)
        menu.add_command(label="Copiar ruta interna", command=self.context_copy_path)
        menu.add_separator()
        menu.add_command(label="Expandir todo", command=self.context_expand_selected)
        menu.add_command(label="Contraer todo", command=self.context_collapse_selected)
        menu.add_separator()
        menu.add_command(label="Marcar solo imagenes dentro", command=self.context_mark_only_images)
        menu.add_command(label="Marcar solo videos dentro", command=self.context_mark_only_videos)
        menu.add_separator()
        menu.add_command(label="Filtrar por este tipo", command=self.context_filter_by_selected_type)
        menu.add_command(label="Quitar filtro de biblioteca", command=self.context_clear_library_filter)
        self.tree_context_menu = menu

    def on_tree_context_menu(self, event: object) -> str | None:
        if not hasattr(event, "x") or not hasattr(event, "y") or self.tree_context_menu is None:
            return None
        item_id = self.tree.identify_row(event.y)
        if not item_id:
            return None
        self.tree.selection_set(item_id)
        self.tree.focus(item_id)
        try:
            self.tree_context_menu.tk_popup(event.x_root, event.y_root)
        finally:
            self.tree_context_menu.grab_release()
        return "break"

    def _selected_tree_node(self) -> TreeNode | None:
        selection = self.tree.selection()
        if not selection:
            return None
        return self.item_to_node.get(selection[0])

    def _selected_tree_item_id(self) -> str:
        selection = self.tree.selection()
        return selection[0] if selection else ""

    def context_mark_selected(self) -> None:
        node = self._selected_tree_node()
        if node is None:
            return
        self.checked_paths.add(node.path)
        self.refresh_tree(use_existing_tree=True)
        self.status_var.set(f"Marcado: {node.name}")

    def context_unmark_selected(self) -> None:
        node = self._selected_tree_node()
        if node is None:
            return
        self.checked_paths.discard(node.path)
        self.refresh_tree(use_existing_tree=True)
        self.status_var.set(f"Desmarcado: {node.name}")

    def context_mark_subtree(self) -> None:
        node = self._selected_tree_node()
        if node is None:
            return
        self._set_checked_recursive(node, True)
        self.refresh_tree(use_existing_tree=True)
        self.status_var.set(f"Marcado todo dentro de: {node.name}")

    def context_unmark_subtree(self) -> None:
        node = self._selected_tree_node()
        if node is None:
            return
        self._set_checked_recursive(node, False)
        self.refresh_tree(use_existing_tree=True)
        self.status_var.set(f"Desmarcado todo dentro de: {node.name}")

    def context_copy_path(self) -> None:
        node = self._selected_tree_node()
        if node is None:
            return
        self.root.clipboard_clear()
        self.root.clipboard_append(node.path)
        self.status_var.set(f"Ruta copiada: {node.path}")

    def _set_tree_open_recursive(self, item_id: str, opened: bool) -> None:
        if not item_id:
            return
        self.tree.item(item_id, open=opened)
        for child_id in self.tree.get_children(item_id):
            self._set_tree_open_recursive(child_id, opened)

    def context_expand_selected(self) -> None:
        item_id = self._selected_tree_item_id()
        if not item_id:
            return
        self._set_tree_open_recursive(item_id, True)

    def context_collapse_selected(self) -> None:
        item_id = self._selected_tree_item_id()
        if not item_id:
            return
        self._set_tree_open_recursive(item_id, False)

    def context_filter_by_selected_type(self) -> None:
        node = self._selected_tree_node()
        if node is None or not node.is_file:
            return
        category = self._category_for_node(node)
        if category in {"Videos", "Imagenes", "Documentos", "Comprimidos", "Desconocidos"}:
            self.recovery_filter_var.set(category)
            self._refresh_summary_tree()
            self.status_var.set(f"Filtro aplicado: {category}")

    def context_clear_library_filter(self) -> None:
        self.recovery_filter_var.set("Todos")
        self._refresh_summary_tree()
        self.status_var.set("Filtro de biblioteca quitado.")

    def context_recover_selected_node(self) -> None:
        node = self._selected_tree_node()
        if node is None:
            return
        self._recover_nodes([node])

    def context_recover_current_selection(self) -> None:
        selected_item_ids = self.tree.selection()
        selected_nodes = [self.item_to_node[item_id] for item_id in selected_item_ids if item_id in self.item_to_node]
        if not selected_nodes:
            return
        self._recover_nodes(selected_nodes)

    def context_open_large_preview(self) -> None:
        node = self._selected_tree_node()
        if node is None:
            return
        preview_path = self._preview_path_for_node(node)
        if not preview_path:
            messagebox.showinfo("Sin vista previa", "Este elemento no tiene vista previa disponible.")
            return
        viewer = tk.Toplevel(self.root)
        viewer.title(f"Vista previa - {node.name}")
        viewer.geometry("1000x700")
        viewer.minsize(700, 500)
        viewer.configure(bg="#000000")

        frame = ttk.Frame(viewer, padding=8)
        frame.pack(fill=tk.BOTH, expand=True)
        image_label = tk.Label(frame, bg="#000000", fg="#f5f5f5", text="Cargando vista previa...")
        image_label.pack(fill=tk.BOTH, expand=True)

        try:
            if Image is not None and ImageTk is not None:
                image = Image.open(preview_path)
                viewer.update_idletasks()
                max_width = max(640, viewer.winfo_width() - 40)
                max_height = max(420, viewer.winfo_height() - 80)
                image.thumbnail((max_width, max_height))
                photo = ImageTk.PhotoImage(image)
            else:
                photo = tk.PhotoImage(file=preview_path)
        except Exception:
            image_label.configure(text=f"No se pudo abrir la vista previa\n{os.path.basename(preview_path)}")
            return

        image_label.configure(image=photo, text="")
        image_label.image = photo

    def _set_checked_by_category_recursive(self, node: TreeNode, target_category: str) -> None:
        if node.is_file:
            if self._category_for_node(node) == target_category:
                self.checked_paths.add(node.path)
            else:
                self.checked_paths.discard(node.path)
            return
        for child in node.children.values():
            self._set_checked_by_category_recursive(child, target_category)

    def context_mark_only_images(self) -> None:
        node = self._selected_tree_node()
        if node is None:
            return
        self._set_checked_recursive(node, False)
        self._set_checked_by_category_recursive(node, "Imagenes")
        self.refresh_tree(use_existing_tree=True)
        self.status_var.set(f"Marcadas solo imagenes dentro de: {node.name}")

    def context_mark_only_videos(self) -> None:
        node = self._selected_tree_node()
        if node is None:
            return
        self._set_checked_recursive(node, False)
        self._set_checked_by_category_recursive(node, "Videos")
        self.refresh_tree(use_existing_tree=True)
        self.status_var.set(f"Marcados solo videos dentro de: {node.name}")

    def _set_checked_recursive(self, node: TreeNode, checked: bool) -> None:
        if checked:
            self.checked_paths.add(node.path)
        else:
            self.checked_paths.discard(node.path)
        for child in node.children.values():
            self._set_checked_recursive(child, checked)

    def _node_matches_filter(self, node: TreeNode, filter_text: str) -> bool:
        type_text = self._display_type_for_node(node).lower()
        size_text = self._format_size_compact(self._node_estimated_total_bytes(node)).lower()
        if (
            filter_text in node.name.lower()
            or filter_text in node.path.lower()
            or filter_text in type_text
            or filter_text in size_text
        ):
            return True
        return any(self._node_matches_filter(child, filter_text) for child in node.children.values())

    def _sort_key(self, node: TreeNode) -> tuple[int, str]:
        return (1 if node.is_file else 0, node.name.lower())

    def _create_tree_icons(self) -> None:
        self.tree_icons = {
            "folder": self._make_icon("#d4a017", "folder"),
            "image": self._make_icon("#2e8b57", "image"),
            "video": self._make_icon("#1f78b4", "video"),
            "document": self._make_icon("#8b5cf6", "document"),
            "archive": self._make_icon("#c05621", "archive"),
            "file": self._make_icon("#6b7280", "file"),
        }

    def _make_icon(self, color: str, kind: str) -> tk.PhotoImage:
        image = tk.PhotoImage(width=16, height=16)
        image.put("#ffffff", to=(0, 0, 16, 16))
        if kind == "folder":
            image.put(color, to=(2, 5, 14, 13))
            image.put("#f6d36b", to=(3, 3, 8, 6))
        elif kind == "image":
            image.put(color, to=(2, 2, 14, 14))
            image.put("#d1fae5", to=(4, 4, 12, 12))
            image.put(color, to=(4, 10, 8, 12))
            image.put(color, to=(8, 8, 12, 12))
        elif kind == "video":
            image.put(color, to=(2, 3, 12, 13))
            image.put(color, to=(12, 5, 15, 11))
            image.put("#dbeafe", to=(4, 5, 10, 11))
        elif kind == "document":
            image.put(color, to=(3, 2, 13, 14))
            image.put("#ede9fe", to=(5, 4, 11, 12))
            image.put(color, to=(9, 2, 13, 6))
        elif kind == "archive":
            image.put(color, to=(3, 2, 13, 14))
            image.put("#fed7aa", to=(7, 2, 9, 14))
            image.put("#7c2d12", to=(6, 6, 10, 8))
        else:
            image.put(color, to=(3, 2, 13, 14))
            image.put("#e5e7eb", to=(5, 4, 11, 12))
        return image

    def _icon_for_node(self, node: TreeNode) -> tk.PhotoImage | str:
        if not node.is_file:
            return self.tree_icons.get("folder", "")

        type_text = self._display_type_for_node(node).lower()
        if any(token in type_text for token in ("jpeg", "png", "gif", "bmp", "tiff", "webp", "heic", "imagen")):
            return self.tree_icons.get("image", "")
        if any(token in type_text for token in ("mp4", "mov", "avi", "mkv", "mpeg", "mts", "3gp", "video")):
            return self.tree_icons.get("video", "")
        if any(token in type_text for token in ("pdf", "xml", "documento", "openpgp")):
            return self.tree_icons.get("document", "")
        if any(token in type_text for token in ("zip", "archive", "comprimido")):
            return self.tree_icons.get("archive", "")
        return self.tree_icons.get("file", "")

    def _display_type_for_node(self, node: TreeNode) -> str:
        if not node.is_file:
            return "Carpeta"

        ntfs_entry = self.ntfs_recovery_entries.get(node.path)
        if ntfs_entry:
            return self._type_label_from_name(node.name)

        if node.raw_lines:
            try:
                payload = json.loads(node.raw_lines[0])
            except json.JSONDecodeError:
                payload = None
            if isinstance(payload, dict) and {"start_offset", "estimated_size_bytes"} <= set(payload.keys()):
                annotation = self.record_annotations.get(self._record_key(payload), {})
                signature_label = annotation.get("signature_label") or payload.get("detected_signature")
                if signature_label:
                    return self._best_type_text(signature_label, annotation)
                description = annotation.get("file_description", "").strip()
                if description:
                    return self._short_file_description(description)
                return "Fragmento sin identificar"
        return self._type_label_from_name(node.name)

    def _display_status_for_node(self, node: TreeNode) -> str:
        if not node.is_file:
            ntfs_entry = self.ntfs_recovery_entries.get(node.path)
            if ntfs_entry and not ntfs_entry.get("active", True):
                return "Carpeta borrada"
            return "Carpeta"

        ntfs_entry = self.ntfs_recovery_entries.get(node.path)
        if ntfs_entry:
            resident_data = ntfs_entry.get("resident_data")
            runlist = ntfs_entry.get("data_runlist") or []
            is_active = bool(ntfs_entry.get("active", True))
            if resident_data is not None and len(resident_data) > 0:
                return "Completo"
            if runlist:
                return "Completo" if is_active else "Borrado"
            return "Solo estructura"

        if node.raw_lines:
            try:
                payload = json.loads(node.raw_lines[0])
            except json.JSONDecodeError:
                payload = None
            if isinstance(payload, dict) and {"start_offset", "estimated_size_bytes"} <= set(payload.keys()):
                annotation = self.record_annotations.get(self._record_key(payload), {})
                saved_path = annotation.get("saved_path")
                if saved_path and os.path.exists(saved_path):
                    return "Verificado"
                signature_label = annotation.get("signature_label") or payload.get("detected_signature")
                if signature_label and signature_label != "unknown_signature":
                    return "Probable"
                return "Sin identificar"

        source_path = self._source_file_path_for_node(node)
        if source_path and os.path.exists(source_path):
            return "Disponible"
        return "Desconocido"

    def _node_own_size_bytes(self, node: TreeNode) -> int:
        if node.size_bytes > 0:
            return node.size_bytes
        ntfs_entry = self.ntfs_recovery_entries.get(node.path)
        if ntfs_entry:
            return int(ntfs_entry.get("size", 0) or 0)
        if node.raw_lines:
            try:
                payload = json.loads(node.raw_lines[0])
            except json.JSONDecodeError:
                payload = None
            if isinstance(payload, dict) and {"start_offset", "estimated_size_bytes"} <= set(payload.keys()):
                annotation = self.record_annotations.get(self._record_key(payload), {})
                return self._preferred_record_size(payload, annotation)
        source_path = self._source_file_path_for_node(node)
        if source_path and os.path.isfile(source_path):
            try:
                return os.path.getsize(source_path)
            except OSError:
                return 0
        return 0

    def _node_estimated_total_bytes(self, node: TreeNode) -> int:
        if node.is_file:
            return self._node_own_size_bytes(node)
        return sum(self._node_estimated_total_bytes(child) for child in node.children.values())

    def _type_label_from_name(self, name: str) -> str:
        lowered = name.lower()
        mapping = {
            ".jpg": "Imagen JPEG",
            ".jpeg": "Imagen JPEG",
            ".png": "Imagen PNG",
            ".gif": "Imagen GIF",
            ".bmp": "Imagen BMP",
            ".tif": "Imagen TIFF",
            ".tiff": "Imagen TIFF",
            ".webp": "Imagen WEBP",
            ".heic": "Imagen HEIC",
            ".mp4": "Video MP4",
            ".mov": "Video MOV",
            ".avi": "Video AVI",
            ".mkv": "Video MKV",
            ".mpeg": "Video MPEG",
            ".mpg": "Video MPEG",
            ".mts": "Video MTS",
            ".3gp": "Video 3GP",
            ".mp3": "Audio MP3",
            ".wav": "Audio WAV",
            ".pdf": "Documento PDF",
            ".xml": "Documento XML",
            ".doc": "Documento DOC",
            ".docx": "Documento DOCX",
            ".xls": "Documento XLS",
            ".xlsx": "Documento XLSX",
            ".txt": "Documento TXT",
            ".zip": "Archivo ZIP",
            ".rar": "Archivo RAR",
            ".7z": "Archivo 7Z",
        }
        for extension, label in mapping.items():
            if lowered.endswith(extension):
                return label
        return "Archivo"

    def _category_for_node(self, node: TreeNode) -> str:
        if not node.is_file:
            return "Carpeta"
        type_text = self._display_type_for_node(node).lower()
        if any(token in type_text for token in ("jpeg", "png", "gif", "bmp", "tiff", "webp", "heic", "imagen")):
            return "Imagenes"
        if any(token in type_text for token in ("mp4", "mov", "avi", "mkv", "mpeg", "mts", "3gp", "video", "audio mp3", "audio wav")):
            return "Videos"
        if any(token in type_text for token in ("pdf", "xml", "documento", "doc", "docx", "xls", "xlsx", "txt", "openpgp")):
            return "Documentos"
        if any(token in type_text for token in ("zip", "rar", "7z", "archivo zip", "comprimido")):
            return "Comprimidos"
        return "Desconocidos"

    def _filter_tree_by_recovery_view(self, node: TreeNode, selected_view: str) -> TreeNode | None:
        ntfs_entry = self.ntfs_recovery_entries.get(node.path)
        if self.hide_deleted_var.get() and ntfs_entry and not ntfs_entry.get("active", True):
            return None
        if selected_view == "Todos":
            return node
        filtered_children: dict[str, TreeNode] = {}
        for name, child in node.children.items():
            filtered_child = self._filter_tree_by_recovery_view(child, selected_view)
            if filtered_child is not None:
                filtered_children[name] = filtered_child
        if not node.is_file:
            if node.name == selected_view or filtered_children:
                clone = TreeNode(
                    name=node.name,
                    path=node.path,
                    is_file=False,
                    size_bytes=node.size_bytes,
                    raw_lines=list(node.raw_lines),
                    line_numbers=list(node.line_numbers),
                )
                clone.children = filtered_children
                return clone
            return None
        if self._category_for_node(node) == selected_view:
            return TreeNode(
                name=node.name,
                path=node.path,
                is_file=True,
                size_bytes=node.size_bytes,
                raw_lines=list(node.raw_lines),
                line_numbers=list(node.line_numbers),
            )
        return None

    def _humanize_signature_label(self, label: str) -> str:
        mapping = {
            "jpeg_probable": "JPEG probable",
            "png_probable": "PNG probable",
            "gif_probable": "GIF probable",
            "bmp_probable": "BMP probable",
            "tiff_probable": "TIFF probable",
            "webp_probable": "WEBP probable",
            "heic_probable": "HEIC probable",
            "pdf_probable": "PDF probable",
            "zip_probable": "ZIP probable",
            "mp4_probable": "MP4 probable",
            "mov_probable": "MOV probable",
            "avi_probable": "AVI probable",
            "mkv_probable": "MKV probable",
            "3gp_probable": "3GP probable",
            "mp3_probable": "MP3 probable",
            "mts_probable": "MTS probable",
            "mpeg_probable": "MPEG probable",
            "openpgp_probable": "OpenPGP probable",
            "xml_probable": "XML probable",
            "unknown_signature": "Desconocido",
        }
        return mapping.get(label, label.replace("_", " "))

    def _best_type_text(self, signature_label: str, annotation: dict) -> str:
        description = (annotation.get("file_description") or "").strip()
        if description and signature_label == "unknown_signature":
            return self._short_file_description(description)
        if description:
            short_description = self._short_file_description(description)
            humanized = self._humanize_signature_label(signature_label)
            if short_description.lower() != humanized.lower():
                return f"{humanized} - {short_description}"
        return self._humanize_signature_label(signature_label)

    def _short_file_description(self, description: str) -> str:
        short = description.strip()
        replacements = {
            "MPEG sequence, v2, program multiplex": "Video MPEG",
            "OpenPGP Public Key": "Clave OpenPGP",
            "JPEG image data": "Imagen JPEG",
            "PNG image data": "Imagen PNG",
            "GIF image data": "Imagen GIF",
            "TIFF image data": "Imagen TIFF",
            "RIFF (little-endian) data, Web/P image": "Imagen WEBP",
            "ISO Media, HEIF Image": "Imagen HEIC",
            "PDF document": "Documento PDF",
            "Zip archive data": "Archivo ZIP",
            "Matroska data": "Video MKV",
            "Audio file with ID3": "Audio MP3",
            "MPEG ADTS, layer III": "Audio MP3",
            "ISO Media": "Video MP4/MOV",
            "XML": "Documento XML",
            "data": "Datos sin identificar",
        }
        for original, translated in replacements.items():
            if short.lower().startswith(original.lower()):
                return translated
        return short[:80]

    def on_tree_select(self, _event: object) -> None:
        selection = self.tree.selection()
        if not selection:
            return
        node = self.item_to_node.get(selection[0])
        if node is None:
            return

        self.raw_text.delete("1.0", tk.END)
        self.info_text.delete("1.0", tk.END)
        preview_path = self._update_preview_for_node(node)

        if node.raw_lines:
            formatted = []
            for line_number, raw_line in zip(node.line_numbers, node.raw_lines):
                formatted.append(f"Linea {line_number}: {raw_line}")
            self.raw_text.insert("1.0", "\n\n".join(formatted))
        else:
            self.raw_text.insert("1.0", "No hay lineas crudas asociadas a este nodo.")

        pretty = self._format_user_friendly_details(node, preview_path)
        self.info_text.insert("1.0", pretty)

    def _clear_preview_temp_file(self) -> None:
        if self.preview_temp_path and os.path.exists(self.preview_temp_path):
            try:
                os.unlink(self.preview_temp_path)
            except OSError:
                pass
        self.preview_temp_path = ""

    def _clear_preview_media_temp_file(self) -> None:
        if self.preview_media_temp_path and os.path.exists(self.preview_media_temp_path):
            try:
                os.unlink(self.preview_media_temp_path)
            except OSError:
                pass
        self.preview_media_temp_path = ""

    def _clear_all_preview_temp_files(self) -> None:
        self._clear_preview_temp_file()
        self._clear_preview_media_temp_file()

    def _update_preview_for_node(self, node: TreeNode) -> str:
        self._clear_all_preview_temp_files()
        self.preview_video_source_path = ""
        self.preview_play_button.configure(state=tk.DISABLED)
        if self._node_supports_video_preview(node):
            self.preview_video_source_path = self._video_source_path_for_node(node)
            if self.preview_video_source_path:
                self.preview_play_button.configure(state=tk.NORMAL)
        preview_path = self._preview_path_for_node(node)
        if not preview_path:
            self._clear_all_preview_temp_files()
            self.preview_image = None
            self.preview_photo = None
            self.preview_canvas.configure(image="", text="Sin vista previa")
            return ""
        try:
            if Image is not None and ImageTk is not None:
                image = Image.open(preview_path)
                self.preview_canvas.update_idletasks()
                max_width = max(420, self.preview_canvas.winfo_width() - 20)
                max_height = max(260, self.preview_canvas.winfo_height() - 20)
                image.thumbnail((max_width, max_height))
                self.preview_image = image
                self.preview_photo = ImageTk.PhotoImage(image)
            else:
                self.preview_photo = tk.PhotoImage(file=preview_path)
        except Exception:
            self.preview_photo = None
            self.preview_canvas.configure(text=f"Vista previa no disponible\n{os.path.basename(preview_path)}", image="")
            return preview_path
        self.preview_canvas.configure(image=self.preview_photo, text="")
        return preview_path

    def play_preview_video(self) -> None:
        if not self.preview_video_source_path or not os.path.exists(self.preview_video_source_path):
            messagebox.showinfo("Video no disponible", "No hay un video listo para reproducir.")
            return
        try:
            subprocess.Popen(
                ["ffplay", "-autoexit", self.preview_video_source_path],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
        except (OSError, FileNotFoundError) as exc:
            messagebox.showerror("No se pudo reproducir", str(exc))

    def _preview_path_for_node(self, node: TreeNode) -> str:
        if self._node_supports_video_preview(node):
            video_path = self.preview_video_source_path or self._video_source_path_for_node(node)
            if video_path:
                thumbnail_path = self._build_video_thumbnail(video_path)
                if thumbnail_path:
                    return thumbnail_path
            return ""
        if not self._node_supports_image_preview(node):
            return ""
        entry = self.ntfs_recovery_entries.get(node.path)
        if entry:
            preview_path = self._materialize_ntfs_preview(entry)
            if preview_path:
                return preview_path
        if node.raw_lines:
            try:
                payload = json.loads(node.raw_lines[0])
            except json.JSONDecodeError:
                payload = None
            if isinstance(payload, dict) and {"start_offset", "estimated_size_bytes"} <= set(payload.keys()):
                annotation = self.record_annotations.get(self._record_key(payload), {})
                saved_path = annotation.get("saved_path")
                if saved_path and os.path.exists(saved_path):
                    return saved_path
                preview_path = self._materialize_raw_record_preview(payload, annotation)
                if preview_path:
                    return preview_path
        candidate = self._source_file_path_for_node(node)
        if candidate and os.path.exists(candidate):
            return candidate
        return ""

    def _node_supports_image_preview(self, node: TreeNode) -> bool:
        type_text = self._display_type_for_node(node).lower()
        name = node.name.lower()
        return any(
            token in type_text or name.endswith(token)
            for token in (".png", ".gif", ".jpg", ".jpeg", ".webp", ".bmp", ".tif", ".tiff", ".heic", "imagen", "jpeg", "png", "gif", "bmp", "tiff", "webp", "heic")
        )

    def _node_supports_video_preview(self, node: TreeNode) -> bool:
        type_text = self._display_type_for_node(node).lower()
        name = node.name.lower()
        return any(
            token in type_text or name.endswith(token)
            for token in (".mp4", ".mov", ".avi", ".mkv", ".mpeg", ".mpg", ".mts", ".3gp", "video", "mp4", "mov", "avi", "mkv", "mpeg", "mts", "3gp")
        )

    def _video_source_path_for_node(self, node: TreeNode) -> str:
        entry = self.ntfs_recovery_entries.get(node.path)
        if entry:
            return self._materialize_ntfs_preview(entry)
        if node.raw_lines:
            try:
                payload = json.loads(node.raw_lines[0])
            except json.JSONDecodeError:
                payload = None
            if isinstance(payload, dict) and {"start_offset", "estimated_size_bytes"} <= set(payload.keys()):
                annotation = self.record_annotations.get(self._record_key(payload), {})
                saved_path = annotation.get("saved_path")
                if saved_path and os.path.exists(saved_path):
                    return saved_path
                return self._materialize_raw_record_preview(payload, annotation)
        candidate = self._source_file_path_for_node(node)
        if candidate and os.path.exists(candidate):
            return candidate
        return ""

    def _build_video_thumbnail(self, video_path: str) -> str:
        self._clear_preview_temp_file()
        try:
            with tempfile.NamedTemporaryFile(delete=False, suffix=".jpg") as handle:
                thumbnail_path = handle.name
        except OSError:
            return ""
        try:
            subprocess.run(
                [
                    "ffmpeg",
                    "-y",
                    "-i",
                    video_path,
                    "-ss",
                    "00:00:01",
                    "-frames:v",
                    "1",
                    thumbnail_path,
                ],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                check=True,
            )
        except (subprocess.SubprocessError, FileNotFoundError):
            try:
                os.unlink(thumbnail_path)
            except OSError:
                pass
            return ""
        self.preview_temp_path = thumbnail_path
        return thumbnail_path

    def _probe_media_info(self, media_path: str) -> dict[str, object]:
        try:
            result = subprocess.run(
                [
                    "ffprobe",
                    "-v",
                    "error",
                    "-print_format",
                    "json",
                    "-show_streams",
                    "-show_format",
                    media_path,
                ],
                capture_output=True,
                text=True,
                check=True,
            )
            payload = json.loads(result.stdout)
        except (subprocess.SubprocessError, json.JSONDecodeError, FileNotFoundError):
            return {}
        info: dict[str, object] = {}
        streams = payload.get("streams") or []
        for stream in streams:
            codec_type = stream.get("codec_type")
            if codec_type == "video" and "video" not in info:
                info["video"] = {
                    "width": int(stream.get("width") or 0),
                    "height": int(stream.get("height") or 0),
                    "codec": stream.get("codec_name") or "",
                }
            if codec_type == "audio" and "audio" not in info:
                info["audio"] = {
                    "codec": stream.get("codec_name") or "",
                }
        format_info = payload.get("format") or {}
        duration_text = format_info.get("duration")
        if duration_text:
            try:
                info["duration_seconds"] = float(duration_text)
            except (TypeError, ValueError):
                pass
        if format_info.get("format_name"):
            info["format_name"] = format_info.get("format_name")
        return info

    def _write_preview_temp_file(self, data: bytes, extension: str) -> str:
        self._clear_preview_media_temp_file()
        try:
            with tempfile.NamedTemporaryFile(delete=False, suffix=extension) as handle:
                handle.write(data)
                self.preview_media_temp_path = handle.name
        except OSError:
            self.preview_media_temp_path = ""
        return self.preview_media_temp_path

    def _materialize_raw_record_preview(self, record: dict, annotation: dict) -> str:
        device_path = str(record.get("device_path") or self.source_root_var.get().strip())
        if not device_path.startswith("/dev/"):
            return ""
        try:
            offset = int(record.get("start_offset", 0))
            size_bytes = int(record.get("estimated_size_bytes", 0))
            signature_offset = int(annotation.get("signature_offset", 0) or 0)
            preferred_size = self._preferred_record_size(record, annotation)
            read_offset = offset + signature_offset
            read_size = preferred_size if preferred_size > 0 else size_bytes
            read_size = min(read_size, 64 * 1024 * 1024)
            if read_size <= 0:
                return ""
            with open(device_path, "rb", buffering=0) as handle:
                handle.seek(read_offset)
                data = handle.read(read_size)
        except OSError:
            return ""
        if not data:
            return ""
        extension = str(annotation.get("suggested_extension", ".bin") or ".bin")
        return self._write_preview_temp_file(data, extension)

    def _materialize_ntfs_preview(self, entry: dict) -> str:
        extension = os.path.splitext(str(entry.get("name", "")))[1] or ".bin"
        resident_data = entry.get("resident_data")
        if resident_data:
            return self._write_preview_temp_file(resident_data, extension)
        device_path = str(entry.get("device_path") or self.source_root_var.get().strip())
        runlist = entry.get("data_runlist") or []
        cluster_size = int(entry.get("cluster_size", 0) or 0)
        remaining = min(int(entry.get("size", 0) or 0), 64 * 1024 * 1024)
        if not device_path.startswith("/dev/") or not runlist or cluster_size <= 0 or remaining <= 0:
            return ""
        chunks: list[bytes] = []
        try:
            with open(device_path, "rb", buffering=0) as handle:
                for lcn, run_len in runlist:
                    if remaining <= 0:
                        break
                    byte_len = min(run_len * cluster_size, remaining)
                    handle.seek(lcn * cluster_size)
                    chunk = handle.read(byte_len)
                    if not chunk:
                        break
                    chunks.append(chunk)
                    remaining -= len(chunk)
        except OSError:
            return ""
        data = b"".join(chunks)
        if not data:
            return ""
        return self._write_preview_temp_file(data, extension)

    def _format_user_friendly_details(self, node: TreeNode, preview_path: str) -> str:
        lines = [
            f"Nombre: {node.name}",
            f"Tipo: {self._display_type_for_node(node)}",
            f"Estado: {self._display_status_for_node(node)}",
            f"Tamano estimado: {self._format_size_compact(self._node_estimated_total_bytes(node))}",
            f"Ruta interna: {node.path}",
        ]
        if not node.is_file:
            lines.append(f"Elementos dentro: {len(node.children)}")
        ntfs_entry = self.ntfs_recovery_entries.get(node.path)
        if ntfs_entry:
            lines.append(f"Estado NTFS: {'Activo' if ntfs_entry.get('active') else 'Borrado'}")
        if node.raw_lines:
            try:
                payload = json.loads(node.raw_lines[0])
            except json.JSONDecodeError:
                payload = None
            if isinstance(payload, dict):
                annotation = None
                if {"start_offset", "estimated_size_bytes"} <= set(payload.keys()):
                    lines.append(f"Offset: 0x{int(payload.get('start_offset', 0)):x}")
                    annotation = self.record_annotations.get(self._record_key(payload), {})
                else:
                    annotation = {}
                original_name = (annotation or {}).get("original_name")
                if original_name:
                    lines.append(f"Nombre detectado: {original_name}")
                file_description = (annotation or {}).get("file_description")
                if file_description:
                    lines.append(f"Formato detectado: {self._short_file_description(file_description)}")
        if preview_path and Image is not None and self._node_supports_image_preview(node):
            try:
                with Image.open(preview_path) as image:
                    lines.append(f"Resolucion: {image.width} x {image.height}")
                    if image.mode:
                        lines.append(f"Color: {image.mode}")
                    if image.format:
                        lines.append(f"Formato imagen: {image.format}")
            except Exception:
                pass
        elif self._category_for_node(node) == "Videos":
            media_path = self.preview_video_source_path or self._video_source_path_for_node(node)
            media_info = self._probe_media_info(media_path) if media_path else {}
            video_info = media_info.get("video") if isinstance(media_info.get("video"), dict) else {}
            audio_info = media_info.get("audio") if isinstance(media_info.get("audio"), dict) else {}
            if video_info:
                width = int(video_info.get("width") or 0)
                height = int(video_info.get("height") or 0)
                if width > 0 and height > 0:
                    lines.append(f"Resolucion video: {width} x {height}")
                codec = str(video_info.get("codec") or "").strip()
                if codec:
                    lines.append(f"Codec video: {codec}")
            if audio_info:
                audio_codec = str(audio_info.get("codec") or "").strip()
                if audio_codec:
                    lines.append(f"Codec audio: {audio_codec}")
            duration_seconds = media_info.get("duration_seconds")
            if isinstance(duration_seconds, (int, float)) and duration_seconds > 0:
                lines.append(f"Duracion: {self._format_duration(float(duration_seconds))}")
            lines.append(
                "Vista previa: usa el boton 'Reproducir video' para abrir el clip seleccionado."
            )
        lines.append(f"Lineas del log asociadas: {len(node.raw_lines)}")
        return "\n".join(lines)

    def _source_file_path_for_node(self, node: TreeNode) -> str:
        source_root = self.source_root_var.get().strip()
        if not source_root or source_root.startswith("/dev/"):
            return ""
        relative_path = self._relative_node_path(node.path)
        if not relative_path:
            return ""
        return os.path.join(source_root, relative_path)

    def _show_scan_summary(self) -> None:
        if not self.scan_summary:
            return
        self.raw_text.delete("1.0", tk.END)
        self.info_text.delete("1.0", tk.END)
        self.raw_text.insert(
            "1.0",
            "Este archivo parece un estado/log de Full Scan de DMDE.\n"
            "No expone rutas claras de archivos, pero los metadatos parseados se muestran a la derecha.",
        )
        self.info_text.insert("1.0", json.dumps(self.scan_summary, indent=2, ensure_ascii=True))

    def _build_summary_tree(self) -> TreeNode:
        root = TreeNode(name="/", path="/", is_file=False)
        selected_view = self.recovery_filter_var.get().strip() or "Todos"

        recovery_root = self._build_recovery_library()
        filtered_recovery_root = self._filter_tree_by_recovery_view(recovery_root, selected_view)
        if filtered_recovery_root and filtered_recovery_root.children:
            root.add_child(filtered_recovery_root)

        if self.ntfs_tree_root and self.ntfs_tree_root.children:
            filtered_ntfs_root = self._filter_tree_by_recovery_view(self.ntfs_tree_root, selected_view)
            if filtered_ntfs_root and filtered_ntfs_root.children:
                root.add_child(filtered_ntfs_root)

        if not self.scan_summary:
            return root

        if selected_view != "Todos":
            return root

        summary_root = TreeNode(name="Resumen del escaneo", path="/Resumen del escaneo", is_file=False)
        root.add_child(summary_root)

        meta_root = TreeNode(name="Metadatos", path="/Resumen del escaneo/Metadatos", is_file=False)
        summary_root.add_child(meta_root)
        for key in ("scan_dev", "scan_range", "scan_range_end", "volume_candidates", "indx_offsets", "signature_types_total"):
            value = self.scan_summary.get(key, "")
            child = TreeNode(
                name=f"{key}: {value}",
                path=f"/Resumen del escaneo/Metadatos/{key}",
                is_file=True,
                raw_lines=[json.dumps({key: value}, ensure_ascii=True)],
            )
            meta_root.add_child(child)

        signatures_root = TreeNode(name="Firmas habilitadas", path="/Resumen del escaneo/Firmas habilitadas", is_file=False)
        summary_root.add_child(signatures_root)
        for token in self.scan_summary.get("signature_types_enabled", []):
            child = TreeNode(
                name=token,
                path=f"/Resumen del escaneo/Firmas habilitadas/{token}",
                is_file=True,
                raw_lines=[json.dumps({"signature": token}, ensure_ascii=True)],
            )
            signatures_root.add_child(child)

        blocks_root = TreeNode(name="Grupos de bloques", path="/Resumen del escaneo/Grupos de bloques", is_file=False)
        summary_root.add_child(blocks_root)
        for block_group in self.scan_summary.get("raw_block_groups", []):
            group_name = block_group.get("group", "unknown")
            stats = self._compute_block_group_stats(block_group)
            group_node = TreeNode(
                name=f"{group_name} ({block_group.get('records_parsed', 0)} records)",
                path=f"/Resumen del escaneo/Grupos de bloques/{group_name}",
                is_file=False,
                raw_lines=[json.dumps({"stats": stats, "block_group": block_group}, ensure_ascii=True)],
            )
            blocks_root.add_child(group_node)
            stats_root = TreeNode(
                name="Estadisticas",
                path=f"/Resumen del escaneo/Grupos de bloques/{group_name}/Estadisticas",
                is_file=False,
            )
            group_node.add_child(stats_root)
            for key, value in stats.items():
                child = TreeNode(
                    name=f"{key}: {value}",
                    path=f"/Resumen del escaneo/Grupos de bloques/{group_name}/Estadisticas/{key}",
                    is_file=True,
                    raw_lines=[json.dumps({key: value}, ensure_ascii=True)],
                )
                stats_root.add_child(child)
            pseudo_root = TreeNode(
                name="Hallazgos pseudo raw",
                path=f"/Resumen del escaneo/Grupos de bloques/{group_name}/Hallazgos pseudo raw",
                is_file=False,
            )
            group_node.add_child(pseudo_root)
            filtered_records = [record for record in block_group.get("decoded_records", []) if self._record_matches_filters(record)]
            filtered_records.sort(key=self._record_sort_key)

            top_root = TreeNode(
                name="Mejores candidatos",
                path=f"/Resumen del escaneo/Grupos de bloques/{group_name}/Mejores candidatos",
                is_file=False,
            )
            group_node.add_child(top_root)
            for index, record in enumerate(filtered_records[:5], start=1):
                record_name = self._format_pseudo_record_name(index, record)
                child = TreeNode(
                    name=record_name,
                    path=f"/Resumen del escaneo/Grupos de bloques/{group_name}/Mejores candidatos/record_{index}",
                    is_file=True,
                    size_bytes=self._preferred_record_size(record),
                    raw_lines=[json.dumps(record, ensure_ascii=True)],
                )
                top_root.add_child(child)

            for index, record in enumerate(filtered_records, start=1):
                record_name = self._format_pseudo_record_name(index, record)
                child = TreeNode(
                    name=record_name,
                    path=f"/Resumen del escaneo/Grupos de bloques/{group_name}/Hallazgos pseudo raw/record_{index}",
                    is_file=True,
                    size_bytes=self._preferred_record_size(record),
                    raw_lines=[json.dumps(record, ensure_ascii=True)],
                )
                pseudo_root.add_child(child)

        return root

    def _build_recovery_library(self) -> TreeNode:
        library_root = TreeNode(name="Biblioteca de recuperacion", path="/Biblioteca de recuperacion", is_file=False)
        categories = {
            "Videos": TreeNode(name="Videos", path="/Biblioteca de recuperacion/Videos", is_file=False),
            "Imagenes": TreeNode(name="Imagenes", path="/Biblioteca de recuperacion/Imagenes", is_file=False),
            "Documentos": TreeNode(name="Documentos", path="/Biblioteca de recuperacion/Documentos", is_file=False),
            "Comprimidos": TreeNode(name="Comprimidos", path="/Biblioteca de recuperacion/Comprimidos", is_file=False),
            "Desconocidos": TreeNode(name="Desconocidos", path="/Biblioteca de recuperacion/Desconocidos", is_file=False),
        }

        for block_group in (self.scan_summary or {}).get("raw_block_groups", []):
            for record in block_group.get("decoded_records", []):
                if not self._record_matches_filters(record):
                    continue
                category_name = self._category_for_record(record)
                category_node = categories[category_name]
                group_name = block_group.get("group", "sin_grupo")
                group_node = category_node.children.get(group_name)
                if group_node is None:
                    group_node = TreeNode(
                        name=group_name,
                        path=f"{category_node.path}/{group_name}",
                        is_file=False,
                    )
                    category_node.add_child(group_node)
                index = len(group_node.children) + 1
                record_name = self._format_recovery_item_name(index, record)
                group_node.add_child(
                    TreeNode(
                        name=record_name,
                        path=f"{group_node.path}/record_{index}",
                        is_file=True,
                        size_bytes=self._preferred_record_size(record),
                        raw_lines=[json.dumps(record, ensure_ascii=True)],
                    )
                )

        if self.direct_scan_records:
            source_group = TreeNode(
                name="Escaneo directo del disco",
                path="/Biblioteca de recuperacion/Escaneo directo del disco",
                is_file=False,
            )
            for category in categories.values():
                category.children["Escaneo directo del disco"] = TreeNode(
                    name="Escaneo directo del disco",
                    path=f"{category.path}/Escaneo directo del disco",
                    is_file=False,
                )
            for index, record in enumerate(self.direct_scan_records, start=1):
                if not self._record_matches_filters(record):
                    continue
                category_name = self._category_for_record(record)
                group_node = categories[category_name].children["Escaneo directo del disco"]
                record_name = self._format_recovery_item_name(index, record)
                group_node.add_child(
                    TreeNode(
                        name=record_name,
                        path=f"{group_node.path}/record_{index}",
                        is_file=True,
                        size_bytes=self._preferred_record_size(record),
                        raw_lines=[json.dumps(record, ensure_ascii=True)],
                    )
                )

        for category in categories.values():
            category.children = {name: child for name, child in category.children.items() if child.children}
            if category.children:
                library_root.add_child(category)

        return library_root

    def _category_for_record(self, record: dict) -> str:
        annotation = self.record_annotations.get(self._record_key(record), {})
        signature_label = annotation.get("signature_label") or record.get("detected_signature", "")
        description = (annotation.get("file_description") or "").lower()
        if signature_label in {"mp4_probable", "mov_probable", "avi_probable", "mpeg_probable", "mkv_probable", "3gp_probable", "mts_probable"}:
            return "Videos"
        if signature_label in {"jpeg_probable", "png_probable", "gif_probable", "bmp_probable", "tiff_probable", "webp_probable", "heic_probable"}:
            return "Imagenes"
        if signature_label in {"pdf_probable", "xml_probable", "openpgp_probable"}:
            return "Documentos"
        if signature_label in {"mp3_probable"}:
            return "Videos"
        if signature_label in {"zip_probable"}:
            return "Comprimidos"
        if "mpeg" in description or "video" in description or "quicktime" in description or "matroska" in description or "3gpp" in description or "transport stream" in description:
            return "Videos"
        if "jpeg" in description or "png" in description or "gif" in description or "bitmap" in description or "tiff" in description or "webp" in description or "heic" in description or "heif" in description:
            return "Imagenes"
        if "mp3" in description or "layer iii" in description or "audio" in description:
            return "Videos"
        if "pdf" in description or "document" in description:
            return "Documentos"
        if "xml" in description or "openpgp" in description or "key" in description:
            return "Documentos"
        if "zip" in description or "archive" in description:
            return "Comprimidos"
        return "Desconocidos"

    def _format_recovery_item_name(self, index: int, record: dict) -> str:
        annotation = self.record_annotations.get(self._record_key(record), {})
        signature_label = annotation.get("signature_label") or record.get("detected_signature", "unknown_signature")
        offset = int(record.get("start_offset", 0))
        size_bytes = self._preferred_record_size(record, annotation)
        suggested_extension = annotation.get("suggested_extension", ".bin")
        type_name = self._best_type_text(signature_label, annotation)
        base_name = f"{type_name} {index:03d} [0x{offset:x}]"
        if suggested_extension and suggested_extension != ".bin":
            return f"{base_name}{suggested_extension} ({size_bytes} bytes)"
        return f"{base_name} ({size_bytes} bytes)"

    def _format_pseudo_record_name(self, index: int, record: dict) -> str:
        offset = record.get("start_offset", 0)
        annotation = self.record_annotations.get(self._record_key(record), {})
        size_bytes = self._preferred_record_size(record, annotation)
        block_size = record.get("block_size", 0)
        span_units = record.get("estimated_span_units", 0)
        label = record.get("classification", {}).get("label", "unknown")
        bucket = record.get("classification", {}).get("bucket", "unknown")
        score = record.get("score", 0)
        signature_label = annotation.get("signature_label", record.get("detected_signature", ""))
        signature_suffix = f", firma={signature_label}" if signature_label else ""
        return (
            f"record_{index}: offset=0x{offset:x} size~={size_bytes}B "
            f"(score={score}, block={block_size}, units={span_units}, {bucket}/{label}{signature_suffix})"
        )

    def _preferred_record_size(self, record: dict, annotation: dict | None = None) -> int:
        annotation = annotation or self.record_annotations.get(self._record_key(record), {})
        extracted_size = int(annotation.get("content_size", 0) or 0)
        if extracted_size > 0:
            return extracted_size
        estimated = int(record.get("estimated_size_bytes", 0))
        signature_offset = int(annotation.get("signature_offset", 0) or 0)
        if signature_offset > 0 and estimated > signature_offset:
            return estimated - signature_offset
        return estimated

    def _compute_block_group_stats(self, block_group: dict) -> dict:
        decoded_records = block_group.get("decoded_records", [])
        sizes = [record.get("estimated_size_bytes", 0) for record in decoded_records]
        nonzero_sizes = [size for size in sizes if size > 0]
        offsets = [record.get("start_offset", 0) for record in decoded_records]
        labels: dict[str, int] = {}
        buckets: dict[str, int] = {}
        scores = [record.get("score", 0) for record in decoded_records]
        for record in decoded_records:
            classification = record.get("classification", {})
            label = classification.get("label", "unknown")
            bucket = classification.get("bucket", "unknown")
            labels[label] = labels.get(label, 0) + 1
            buckets[bucket] = buckets.get(bucket, 0) + 1
        return {
            "declared_count": block_group.get("declared_count", 0),
            "records_parsed": block_group.get("records_parsed", 0),
            "records_previewed": len(decoded_records),
            "nonzero_size_records": len(nonzero_sizes),
            "estimated_total_bytes_preview": sum(nonzero_sizes),
            "largest_estimated_bytes": max(nonzero_sizes, default=0),
            "lowest_offset_preview": min(offsets, default=0),
            "highest_offset_preview": max(offsets, default=0),
            "classification_counts": labels,
            "bucket_counts": buckets,
            "highest_score": max(scores, default=0),
            "average_score": round(sum(scores) / len(scores), 2) if scores else 0,
        }

    def _format_node_details(self, base_details: dict, node: TreeNode) -> str:
        if node.raw_lines:
            try:
                parsed = json.loads(node.raw_lines[0])
            except json.JSONDecodeError:
                parsed = None
            if isinstance(parsed, dict):
                merged = dict(base_details)
                record_key = None
                if {"start_offset", "estimated_size_bytes"} <= set(parsed.keys()):
                    record_key = self._record_key(parsed)
                if record_key and record_key in self.record_annotations:
                    parsed = dict(parsed)
                    parsed["annotation"] = self.record_annotations[record_key]
                merged["decoded"] = parsed
                return json.dumps(merged, indent=2, ensure_ascii=True)
            if isinstance(parsed, list):
                merged = dict(base_details)
                merged["decoded"] = parsed
                return json.dumps(merged, indent=2, ensure_ascii=True)
        return json.dumps(base_details, indent=2, ensure_ascii=True)

    def _ensure_progress_dialog(self, title: str) -> None:
        if self.progress_dialog and self.progress_dialog.winfo_exists():
            self.progress_dialog.title(title)
            return
        dialog = tk.Toplevel(self.root)
        dialog.title(title)
        dialog.geometry("700x250")
        dialog.minsize(700, 250)
        dialog.resizable(False, False)
        dialog.transient(self.root)
        dialog.grab_set()
        dialog.configure(bg="#000000")
        dialog.protocol("WM_DELETE_WINDOW", self.stop_direct_scan)

        frame = ttk.Frame(dialog, padding=12)
        frame.pack(fill=tk.BOTH, expand=True)
        ttk.Label(frame, textvariable=self.scan_progress_text_var, wraplength=660).pack(anchor=tk.W, pady=(0, 8))
        progress = ttk.Progressbar(frame, variable=self.scan_progress_var, maximum=100.0, mode="determinate")
        progress.pack(fill=tk.X)
        ttk.Label(frame, textvariable=self.scan_eta_text_var, wraplength=660).pack(anchor=tk.W, pady=(8, 0))
        ttk.Label(frame, textvariable=self.scan_size_text_var, wraplength=660).pack(anchor=tk.W, pady=(6, 0))
        ttk.Label(frame, textvariable=self.status_var, wraplength=660).pack(anchor=tk.W, pady=(12, 8))

        button_row = ttk.Frame(frame)
        button_row.pack(fill=tk.X, side=tk.BOTTOM, pady=(8, 0))
        ttk.Button(button_row, text="Guardar sesion", command=self.save_scan_session_as).pack(side=tk.LEFT)
        self.progress_pause_button = ttk.Button(button_row, text="Pausar", command=self.toggle_scan_pause)
        self.progress_pause_button.pack(side=tk.RIGHT)
        ttk.Button(button_row, text="Stop", command=self.stop_direct_scan).pack(side=tk.RIGHT, padx=(0, 8))

        dialog.update_idletasks()
        self.progress_dialog = dialog

    def _close_progress_dialog(self) -> None:
        if self.progress_dialog and self.progress_dialog.winfo_exists():
            try:
                self.progress_dialog.grab_release()
            except tk.TclError:
                pass
            self.progress_dialog.destroy()
        self.progress_dialog = None
        self.progress_pause_button = None

    def _ensure_recovery_dialog(self, title: str) -> None:
        if self.recovery_dialog and self.recovery_dialog.winfo_exists():
            self.recovery_dialog.title(title)
            return
        dialog = tk.Toplevel(self.root)
        dialog.title(title)
        dialog.geometry("700x220")
        dialog.minsize(700, 220)
        dialog.resizable(False, False)
        dialog.transient(self.root)
        dialog.grab_set()
        dialog.configure(bg="#000000")
        dialog.protocol("WM_DELETE_WINDOW", lambda: None)

        frame = ttk.Frame(dialog, padding=12)
        frame.pack(fill=tk.BOTH, expand=True)
        ttk.Label(frame, textvariable=self.recovery_progress_text_var, wraplength=660).pack(anchor=tk.W, pady=(0, 8))
        progress = ttk.Progressbar(frame, variable=self.recovery_progress_var, maximum=100.0, mode="determinate")
        progress.pack(fill=tk.X)
        ttk.Label(frame, textvariable=self.recovery_eta_text_var, wraplength=660).pack(anchor=tk.W, pady=(8, 0))
        ttk.Label(frame, textvariable=self.recovery_size_text_var, wraplength=660).pack(anchor=tk.W, pady=(6, 0))
        ttk.Label(frame, textvariable=self.status_var, wraplength=660).pack(anchor=tk.W, pady=(12, 8))

        button_row = ttk.Frame(frame)
        button_row.pack(fill=tk.X, side=tk.BOTTOM, pady=(8, 0))
        self.recovery_cancel_button = ttk.Button(button_row, text="Cancelar copia", command=self.cancel_recovery)
        self.recovery_cancel_button.pack(side=tk.RIGHT)

        dialog.update_idletasks()
        self.recovery_dialog = dialog

    def _close_recovery_dialog(self) -> None:
        if self.recovery_dialog and self.recovery_dialog.winfo_exists():
            try:
                self.recovery_dialog.grab_release()
            except tk.TclError:
                pass
            self.recovery_dialog.destroy()
        self.recovery_dialog = None
        self.recovery_cancel_button = None

    def cancel_recovery(self) -> None:
        self.cancel_recovery_requested = True
        self.status_var.set("Cancelando copia... espera a que termine el bloque actual.")
        if self.recovery_cancel_button is not None:
            self.recovery_cancel_button.configure(state=tk.DISABLED, text="Cancelando...")

    def _update_recovery_progress(
        self,
        copied_bytes: int,
        total_bytes: int,
        completed_items: int,
        total_items: int,
        current_label: str,
    ) -> None:
        percent = (copied_bytes / total_bytes) * 100 if total_bytes > 0 else 0.0
        self.recovery_progress_var.set(percent)
        self.recovery_progress_text_var.set(
            f"{percent:.1f}% | {completed_items}/{total_items} elemento(s) | {current_label}"
        )
        elapsed = max(0.0, time.time() - self.recovery_started_at)
        if copied_bytes > 0 and total_bytes > 0:
            estimated_total = elapsed / max(copied_bytes / total_bytes, 0.0001)
            remaining = max(0.0, estimated_total - elapsed)
            self.recovery_eta_text_var.set(
                f"Transcurrido: {self._format_duration(elapsed)} | Queda aprox.: {self._format_duration(remaining)}"
            )
        else:
            self.recovery_eta_text_var.set("Calculando tiempo estimado...")
        self.recovery_size_text_var.set(
            f"Copiado: {self._format_size_compact(copied_bytes)} / {self._format_size_compact(total_bytes)}"
            if total_bytes > 0
            else f"Copiado: {self._format_size_compact(copied_bytes)}"
        )
        self.root.update_idletasks()

    def _format_duration(self, seconds: float) -> str:
        total = max(0, int(seconds))
        hours, remainder = divmod(total, 3600)
        minutes, secs = divmod(remainder, 60)
        if hours:
            return f"{hours}h {minutes:02d}m {secs:02d}s"
        if minutes:
            return f"{minutes}m {secs:02d}s"
        return f"{secs}s"

    def _update_scan_eta(self, percent: float) -> None:
        if self.pause_scan_requested:
            self.scan_eta_text_var.set("En pausa")
            return
        if self.scan_started_at <= 0 or percent <= 0:
            self.scan_eta_text_var.set("Calculando tiempo estimado...")
            return
        elapsed = time.time() - self.scan_started_at
        if percent >= 100:
            self.scan_eta_text_var.set(f"Tiempo total: {self._format_duration(elapsed)}")
            return
        estimated_total = elapsed / (percent / 100.0)
        remaining = max(0.0, estimated_total - elapsed)
        self.scan_eta_text_var.set(
            f"Transcurrido: {self._format_duration(elapsed)} | Queda aprox.: {self._format_duration(remaining)}"
        )

    def _set_scan_size_status(
        self,
        scanned_bytes: int,
        total_bytes: int,
        found_bytes: int,
        scanned_label: str = "Escaneado",
        found_label: str = "Recuperable estimado",
    ) -> None:
        scanned_text = self._format_size_compact(scanned_bytes) if scanned_bytes > 0 else "0 B"
        total_text = self._format_size_compact(total_bytes) if total_bytes > 0 else "desconocido"
        found_text = self._format_size_compact(found_bytes) if found_bytes > 0 else "0 B"
        self.scan_size_text_var.set(
            f"{scanned_label}: {scanned_text} / {total_text} | {found_label}: {found_text}"
        )

    def _prompt_save_session_after_scan(self, scan_label: str) -> None:
        should_save = messagebox.askyesno(
            "Guardar sesion",
            f"{scan_label} ha terminado.\n\nQuieres guardar la sesion ahora?",
        )
        if should_save:
            self.save_scan_session_as()

    def toggle_scan_pause(self) -> None:
        if not self.scan_running:
            return
        self.pause_scan_requested = not self.pause_scan_requested
        if self.progress_pause_button is not None:
            self.progress_pause_button.configure(text="Reanudar" if self.pause_scan_requested else "Pausar")
        if self.pause_scan_requested:
            self.status_var.set("Escaneo en pausa. Puedes guardar la sesion o reanudar.")
            self.scan_progress_text_var.set("Escaneo en pausa")
            self.scan_eta_text_var.set("En pausa")
            if not self.scan_size_text_var.get():
                self.scan_size_text_var.set("Escaneo en pausa")

    def _wait_if_paused(self) -> None:
        while self.pause_scan_requested and not self.stop_scan_requested:
            self.root.update()
            time.sleep(0.1)

    def save_scan_session_as(self) -> None:
        file_path = filedialog.asksaveasfilename(
            title="Guardar sesion de escaneo",
            defaultextension=".json",
            filetypes=[("JSON files", "*.json"), ("All files", "*.*")],
        )
        if not file_path:
            return
        payload = self._serialize_scan_session()
        try:
            serialized = json.dumps(payload, indent=2, ensure_ascii=True)
            json.loads(serialized)
            directory = os.path.dirname(file_path) or "."
            with tempfile.NamedTemporaryFile("w", encoding="utf-8", dir=directory, delete=False) as handle:
                temp_path = handle.name
                handle.write(serialized)
                handle.flush()
                os.fsync(handle.fileno())
            with open(temp_path, "r", encoding="utf-8") as handle:
                json.load(handle)
            os.replace(temp_path, file_path)
        except (OSError, TypeError, ValueError) as exc:
            temp_path = locals().get("temp_path")
            if temp_path and os.path.exists(temp_path):
                try:
                    os.unlink(temp_path)
                except OSError:
                    pass
            messagebox.showerror("Error al guardar sesion", str(exc))
            return
        self.status_var.set(f"Sesion guardada en {file_path}.")

    def _serialize_scan_session(self) -> dict:
        payload = {
            "format": "dmde-scan-session-v1",
            "source_root": self.source_root_var.get().strip(),
            "scan_summary": self.scan_summary,
            "direct_scan_records": self.direct_scan_records,
            "ntfs_tree_root": self._serialize_node(self.ntfs_tree_root) if self.ntfs_tree_root else None,
            "ntfs_recovery_entries": self.ntfs_recovery_entries,
            "record_annotations": self.record_annotations,
            "checked_paths": sorted(self.checked_paths),
        }
        return self._json_safe_value(payload)

    def _json_safe_value(self, value: object) -> object:
        if isinstance(value, bytes):
            return {"__type__": "bytes_b64", "data": base64.b64encode(value).decode("ascii")}
        if isinstance(value, dict):
            return {str(key): self._json_safe_value(inner) for key, inner in value.items()}
        if isinstance(value, list):
            return [self._json_safe_value(item) for item in value]
        if isinstance(value, tuple):
            return [self._json_safe_value(item) for item in value]
        return value

    def _restore_json_safe_value(self, value: object) -> object:
        if isinstance(value, dict):
            if value.get("__type__") == "bytes_b64":
                data = value.get("data", "")
                if isinstance(data, str):
                    try:
                        return base64.b64decode(data.encode("ascii"))
                    except (ValueError, OSError):
                        return b""
                return b""
            return {key: self._restore_json_safe_value(inner) for key, inner in value.items()}
        if isinstance(value, list):
            return [self._restore_json_safe_value(item) for item in value]
        return value

    def _load_scan_session_payload(self, payload: dict, source_name: str) -> None:
        payload = self._restore_json_safe_value(payload)
        if not isinstance(payload, dict):
            raise ValueError("La sesion no contiene un formato valido.")
        self.entries = []
        self.scan_summary = payload.get("scan_summary")
        self.source_root_var.set(payload.get("source_root", ""))
        self.direct_scan_records = list(payload.get("direct_scan_records") or [])
        ntfs_tree_payload = payload.get("ntfs_tree_root")
        self.ntfs_tree_root = self._deserialize_node(ntfs_tree_payload) if isinstance(ntfs_tree_payload, dict) else None
        self.ntfs_recovery_entries = dict(payload.get("ntfs_recovery_entries") or {})
        self.record_annotations = dict(payload.get("record_annotations") or {})
        self.checked_paths = set(payload.get("checked_paths") or [])
        self.tree_model = self._build_summary_tree()
        self.refresh_tree(use_existing_tree=True)
        self.status_var.set(f"Sesion cargada desde {source_name}.")

    def direct_device_scan(self) -> None:
        if self.scan_running:
            messagebox.showinfo("Escaneo en curso", "Ya hay un escaneo directo en marcha.")
            return

        device_path = self.source_root_var.get().strip()
        if not device_path.startswith("/dev/"):
            messagebox.showinfo("Falta el disco", "Carga primero un log que detecte el disco, o rellena un /dev valido.")
            return
        scan_path = self._preferred_direct_scan_path(device_path)

        proceed = messagebox.askyesno(
            "Escaneo directo",
            "Este modo no usa el log de DMDE: escanea directamente por firmas de fotos y videos.\n\n"
            f"Origen elegido: {scan_path}\n\n"
            "Puede tardar bastante en un disco grande, pero encontrara muchos mas JPG/PNG/MP4 que el log.\n\n"
            "Quieres empezar ahora?",
        )
        if not proceed:
            return

        self.stop_scan_requested = False
        self.pause_scan_requested = False
        self.scan_running = True
        self.stop_scan_button.config(state=tk.NORMAL)
        self.scan_progress_var.set(0.0)
        self.scan_progress_text_var.set("Preparando escaneo...")
        self.scan_eta_text_var.set("Calculando tiempo estimado...")
        self.scan_size_text_var.set("Escaneado: 0 B | Recuperable estimado: 0 B")
        self.scan_total_bytes_hint = 0
        self.scan_started_at = time.time()
        self.direct_scan_records = []
        self._refresh_summary_tree()
        self._ensure_progress_dialog("Escaneo directo")
        if self.fast_scan_var.get():
            self.status_var.set("Escaneo directo en modo turbo: el arbol se actualizara al final o al detener.")
        self.root.update_idletasks()
        try:
            records = self._scan_device_for_media(scan_path)
        except OSError as exc:
            self.scan_progress_var.set(0.0)
            self.scan_progress_text_var.set("Escaneo detenido")
            self.scan_eta_text_var.set("")
            self.scan_size_text_var.set("")
            self.scan_running = False
            self.stop_scan_button.config(state=tk.DISABLED)
            messagebox.showerror(
                "No se pudo escanear el disco",
                f"No se pudo leer {scan_path}.\n\n{exc}\n\nPrueba lanzando la app con sudo.",
            )
            return

        self.scan_running = False
        self.pause_scan_requested = False
        self.stop_scan_button.config(state=tk.DISABLED)
        self.direct_scan_records = records
        self._refresh_summary_tree()
        if self.stop_scan_requested:
            self.scan_progress_text_var.set(f"Detenido: {len(records)} hallazgo(s)")
            self._update_scan_eta(float(self.scan_progress_var.get()))
            self._set_scan_size_status(
                0,
                0,
                sum(self._preferred_record_size(record) for record in records),
            )
            self.status_var.set(
                f"Escaneo directo detenido en {scan_path}: {len(records)} hallazgo(s) encontrados."
            )
        else:
            self.scan_progress_var.set(100.0)
            self.scan_progress_text_var.set(f"Completado: {len(records)} hallazgo(s)")
            self._update_scan_eta(100.0)
            self._set_scan_size_status(
                self._get_device_size(scan_path),
                self._get_device_size(scan_path),
                sum(self._preferred_record_size(record) for record in records),
            )
            self.status_var.set(
                f"Escaneo directo terminado en {scan_path}: {len(records)} hallazgo(s) de recuperacion."
            )
        if not records:
            messagebox.showinfo(
                "Sin resultados",
                "No se encontraron firmas recuperables en el escaneo directo con las reglas actuales.",
            )
        self._close_progress_dialog()
        self._prompt_save_session_after_scan("El escaneo directo")

    def stop_direct_scan(self) -> None:
        if not self.scan_running:
            return
        self.stop_scan_requested = True
        self.scan_progress_text_var.set("Deteniendo escaneo...")
        self.status_var.set("Solicitud de parada enviada. Espera a que termine el bloque actual.")

    def ntfs_low_level_scan(self) -> None:
        if self.scan_running:
            messagebox.showinfo("Escaneo en curso", "Ya hay un escaneo en marcha.")
            return

        device_path = self.source_root_var.get().strip()
        if not device_path.startswith("/dev/"):
            messagebox.showinfo("Falta el disco", "Carga primero un log que detecte el disco, o rellena un /dev valido.")
            return
        scan_path = self._preferred_direct_scan_path(device_path)

        proceed = messagebox.askyesno(
            "Escaneo NTFS",
            "Este modo intenta leer NTFS a bajo nivel: boot sector, MFT y directorios.\n\n"
            "Si la MFT responde, podra reconstruir carpetas y archivos reales.\n\n"
            f"Origen elegido: {scan_path}\n\n"
            "Quieres empezar ahora?",
        )
        if not proceed:
            return

        self.stop_scan_requested = False
        self.pause_scan_requested = False
        self.scan_running = True
        self.stop_scan_button.config(state=tk.NORMAL)
        self.scan_progress_var.set(0.0)
        self.scan_progress_text_var.set("Preparando escaneo NTFS...")
        self.scan_eta_text_var.set("Calculando tiempo estimado...")
        self.scan_size_text_var.set("MFT leida: 0 B | Datos reconstruibles: 0 B")
        self.scan_total_bytes_hint = 0
        self.scan_started_at = time.time()
        self.ntfs_tree_root = None
        self.ntfs_recovery_entries = {}
        self._refresh_summary_tree()
        self._ensure_progress_dialog("Escaneo NTFS")
        if self.fast_scan_var.get():
            self.status_var.set("Escaneo NTFS en modo turbo: el arbol se actualizara al final o al detener.")
        self.root.update_idletasks()

        try:
            ntfs_root = self._scan_ntfs_tree(scan_path)
        except OSError as exc:
            self.scan_progress_var.set(0.0)
            self.scan_progress_text_var.set("Escaneo NTFS detenido")
            self.scan_eta_text_var.set("")
            self.scan_size_text_var.set("")
            self.scan_running = False
            self.stop_scan_button.config(state=tk.DISABLED)
            messagebox.showerror(
                "No se pudo escanear NTFS",
                f"No se pudo leer {scan_path}.\n\n{exc}\n\nPrueba lanzando la app con sudo.",
            )
            return
        except ValueError as exc:
            self.scan_progress_var.set(0.0)
            self.scan_progress_text_var.set("No es NTFS valido")
            self.scan_eta_text_var.set("")
            self.scan_size_text_var.set("")
            self.scan_running = False
            self.stop_scan_button.config(state=tk.DISABLED)
            messagebox.showerror("Escaneo NTFS", str(exc))
            return

        self.scan_running = False
        self.pause_scan_requested = False
        self.stop_scan_button.config(state=tk.DISABLED)
        self.ntfs_tree_root = ntfs_root
        self._refresh_summary_tree()
        if self.stop_scan_requested:
            self.scan_progress_text_var.set("NTFS detenido")
            self._update_scan_eta(float(self.scan_progress_var.get()))
            self._set_scan_size_status(
                0,
                0,
                self._node_estimated_total_bytes(ntfs_root),
                scanned_label="MFT leida",
                found_label="Datos reconstruibles",
            )
            self.status_var.set("Escaneo NTFS detenido. Se muestran los registros encontrados hasta ahora.")
        else:
            self.scan_progress_var.set(100.0)
            files_found = self._count_tree_files(ntfs_root)
            self.scan_progress_text_var.set(f"NTFS completado: {files_found} archivo(s)")
            self._update_scan_eta(100.0)
            self._set_scan_size_status(
                self.scan_total_bytes_hint,
                self.scan_total_bytes_hint,
                self._node_estimated_total_bytes(ntfs_root),
                scanned_label="MFT leida",
                found_label="Datos reconstruibles",
            )
            self.status_var.set(
                f"Escaneo NTFS terminado en {scan_path}: {files_found} archivo(s) y carpetas reconstruidos."
            )
        self._close_progress_dialog()
        self._prompt_save_session_after_scan("El escaneo NTFS")

    def _count_tree_files(self, node: TreeNode) -> int:
        total = 1 if node.is_file else 0
        for child in node.children.values():
            total += self._count_tree_files(child)
        return total

    def _scan_ntfs_tree(self, device_path: str) -> TreeNode:
        device_size = self._get_device_size(device_path)
        turbo_mode = bool(self.fast_scan_var.get())
        with open(device_path, "rb", buffering=0) as handle:
            boot = self._read_exact(handle, 0, 512)
            ntfs = self._parse_ntfs_boot_sector(boot, device_size)
            mft_record = self._read_exact(handle, ntfs["mft_offset"], ntfs["record_size"])
            mft_record = self._apply_ntfs_fixup(mft_record, ntfs["sector_size"])
            mft_data_attr = self._find_ntfs_attribute(mft_record, 0x80)
            if not mft_data_attr or mft_data_attr.get("resident", True):
                raise ValueError("No se pudo localizar el flujo de datos de $MFT.")
            runlist = self._parse_ntfs_runlist(mft_data_attr["runlist"])
            if not runlist:
                raise ValueError("La MFT no tiene runlist legible.")
            mft_stream_size = int(mft_data_attr.get("real_size", 0) or 0)
            if mft_stream_size <= 0:
                mft_stream_size = sum(run_len for _lcn, run_len in runlist) * ntfs["cluster_size"]
            self.scan_total_bytes_hint = mft_stream_size
            record_count = max(1, min(mft_stream_size // ntfs["record_size"], 500000))

            entries: dict[int, dict] = {}
            deleted_entries: dict[int, dict] = {}
            active_total_bytes = 0
            last_tree_refresh = 0.0
            last_progress_refresh = 0.0
            for record_index in range(record_count):
                if self.stop_scan_requested:
                    break
                self._wait_if_paused()
                record = self._read_ntfs_mft_record(handle, ntfs, runlist, record_index)
                if not record:
                    continue
                parsed = self._parse_ntfs_file_record(record, record_index)
                if not parsed:
                    continue
                parsed["device_path"] = device_path
                parsed["cluster_size"] = ntfs["cluster_size"]
                if parsed["active"]:
                    entries[record_index] = parsed
                    active_total_bytes += int(parsed.get("size", 0) or 0)
                else:
                    deleted_entries[record_index] = parsed

                now = time.time()
                if now - last_progress_refresh > 0.5:
                    scanned_bytes = min((record_index + 1) * ntfs["record_size"], mft_stream_size)
                    percent = (record_index + 1) / record_count * 100 if record_count else 0
                    self.scan_progress_var.set(percent)
                    self.scan_progress_text_var.set(
                        f"NTFS {percent:.1f}% | {len(entries)} activos | {len(deleted_entries)} borrados"
                    )
                    self._update_scan_eta(percent)
                    self._set_scan_size_status(
                        scanned_bytes,
                        mft_stream_size,
                        active_total_bytes,
                        scanned_label="MFT leida",
                        found_label="Datos reconstruibles",
                    )
                    self.status_var.set(
                        f"Escaneo NTFS en progreso: {percent:.1f}% - {len(entries)} activos, {len(deleted_entries)} borrados."
                    )
                    self.root.update_idletasks()
                    last_progress_refresh = now
                if not turbo_mode and now - last_tree_refresh > 60.0:
                    self.ntfs_tree_root = self._build_ntfs_tree_model(entries, deleted_entries)
                    self._refresh_summary_tree()
                    self.root.update_idletasks()
                    last_tree_refresh = now

        return self._build_ntfs_tree_model(entries, deleted_entries)

    def _read_exact(self, handle: object, offset: int, size: int) -> bytes:
        handle.seek(offset)
        data = handle.read(size)
        if len(data) != size:
            raise OSError(f"No se pudieron leer {size} bytes en 0x{offset:x}")
        return data

    def _parse_ntfs_boot_sector(self, boot: bytes, device_size: int) -> dict:
        if len(boot) < 90 or boot[3:11] != b"NTFS    ":
            raise ValueError("El origen seleccionado no parece un volumen NTFS valido.")
        sector_size = int.from_bytes(boot[11:13], "little")
        sectors_per_cluster = boot[13]
        cluster_size = sector_size * sectors_per_cluster
        mft_lcn = int.from_bytes(boot[48:56], "little", signed=True)
        clusters_per_record_raw = struct.unpack("<b", boot[64:65])[0]
        if clusters_per_record_raw < 0:
            record_size = 1 << (-clusters_per_record_raw)
        else:
            record_size = clusters_per_record_raw * cluster_size
        if not sector_size or not cluster_size or not record_size:
            raise ValueError("No se pudieron interpretar los parametros basicos de NTFS.")
        mft_offset = mft_lcn * cluster_size
        if mft_offset <= 0 or mft_offset >= device_size:
            raise ValueError("La posicion de la MFT parece invalida.")
        return {
            "sector_size": sector_size,
            "cluster_size": cluster_size,
            "record_size": record_size,
            "mft_offset": mft_offset,
        }

    def _apply_ntfs_fixup(self, record: bytes, sector_size: int) -> bytes:
        fixed = bytearray(record)
        if len(fixed) < 42 or fixed[0:4] != b"FILE":
            return bytes(fixed)
        usa_offset = int.from_bytes(fixed[4:6], "little")
        usa_count = int.from_bytes(fixed[6:8], "little")
        if usa_offset <= 0 or usa_count <= 1:
            return bytes(fixed)
        usa_end = usa_offset + usa_count * 2
        if usa_end > len(fixed):
            return bytes(fixed)
        usa = fixed[usa_offset:usa_end]
        update_seq = usa[:2]
        for idx in range(1, usa_count):
            sector_end = idx * sector_size - 2
            repl_start = idx * 2
            repl_end = repl_start + 2
            if sector_end + 2 > len(fixed) or repl_end > len(usa):
                break
            if fixed[sector_end:sector_end + 2] == update_seq:
                fixed[sector_end:sector_end + 2] = usa[repl_start:repl_end]
        return bytes(fixed)

    def _find_ntfs_attribute(self, record: bytes, attr_type: int) -> dict | None:
        for attr in self._iter_ntfs_attributes(record):
            if attr["type"] == attr_type:
                return attr
        return None

    def _iter_ntfs_attributes(self, record: bytes) -> list[dict]:
        attrs: list[dict] = []
        if len(record) < 24:
            return attrs
        attr_offset = int.from_bytes(record[20:22], "little")
        while 0 < attr_offset + 16 <= len(record):
            attr_type = int.from_bytes(record[attr_offset:attr_offset + 4], "little")
            if attr_type == 0xFFFFFFFF:
                break
            attr_len = int.from_bytes(record[attr_offset + 4:attr_offset + 8], "little")
            if attr_len <= 0 or attr_offset + attr_len > len(record):
                break
            nonresident = record[attr_offset + 8]
            attr = {"type": attr_type, "resident": nonresident == 0}
            if nonresident == 0:
                content_len = int.from_bytes(record[attr_offset + 16:attr_offset + 20], "little")
                content_offset = int.from_bytes(record[attr_offset + 20:attr_offset + 22], "little")
                start = attr_offset + content_offset
                end = start + content_len
                if start <= len(record) and end <= len(record):
                    attr["content"] = record[start:end]
            else:
                run_offset = int.from_bytes(record[attr_offset + 32:attr_offset + 34], "little")
                real_size = int.from_bytes(record[attr_offset + 48:attr_offset + 56], "little")
                start = attr_offset + run_offset
                end = attr_offset + attr_len
                attr["runlist"] = record[start:end]
                attr["real_size"] = real_size
            attrs.append(attr)
            attr_offset += attr_len
        return attrs

    def _parse_ntfs_runlist(self, runlist: bytes) -> list[tuple[int, int]]:
        runs: list[tuple[int, int]] = []
        pos = 0
        current_lcn = 0
        while pos < len(runlist):
            header = runlist[pos]
            pos += 1
            if header == 0:
                break
            len_size = header & 0x0F
            off_size = header >> 4
            if pos + len_size + off_size > len(runlist):
                break
            run_len = int.from_bytes(runlist[pos:pos + len_size], "little", signed=False)
            pos += len_size
            run_off = int.from_bytes(runlist[pos:pos + off_size], "little", signed=True)
            pos += off_size
            current_lcn += run_off
            runs.append((current_lcn, run_len))
        return runs

    def _read_ntfs_mft_record(
        self, handle: object, ntfs: dict, runlist: list[tuple[int, int]], record_index: int
    ) -> bytes | None:
        record_size = ntfs["record_size"]
        cluster_size = ntfs["cluster_size"]
        stream_offset = record_index * record_size
        current_stream = 0
        for lcn, run_len in runlist:
            run_bytes = run_len * cluster_size
            if current_stream + run_bytes <= stream_offset:
                current_stream += run_bytes
                continue
            inside = stream_offset - current_stream
            disk_offset = lcn * cluster_size + inside
            try:
                record = self._read_exact(handle, disk_offset, record_size)
            except OSError:
                return None
            record = self._apply_ntfs_fixup(record, ntfs["sector_size"])
            if record[0:4] != b"FILE":
                return None
            return record
        return None

    def _parse_ntfs_file_record(self, record: bytes, record_index: int) -> dict | None:
        flags = int.from_bytes(record[22:24], "little")
        in_use = bool(flags & 0x0001)
        is_dir = bool(flags & 0x0002)
        attrs = self._iter_ntfs_attributes(record)
        best_name = ""
        parent_ref = 5
        file_size = 0
        resident_data: bytes | None = None
        data_runlist: list[tuple[int, int]] = []
        for attr in attrs:
            if attr["type"] == 0x30 and attr.get("content"):
                parsed_name = self._parse_ntfs_file_name_attr(attr["content"])
                if not parsed_name:
                    continue
                namespace = parsed_name["namespace"]
                if not best_name or namespace in {0x01, 0x03}:
                    best_name = parsed_name["name"]
                    parent_ref = parsed_name["parent_ref"]
                    file_size = parsed_name["real_size"]
            elif attr["type"] == 0x80 and not is_dir:
                if attr.get("resident", True) and attr.get("content") is not None:
                    file_size = max(file_size, len(attr["content"]))
                    resident_data = attr["content"]
                elif not attr.get("resident", True):
                    file_size = max(file_size, int(attr.get("real_size", 0) or 0))
                    data_runlist = self._parse_ntfs_runlist(attr.get("runlist", b""))
        if not best_name:
            if record_index == 5:
                best_name = "Root"
                parent_ref = 5
            else:
                return None
        return {
            "record_index": record_index,
            "name": best_name,
            "parent_ref": parent_ref,
            "is_dir": is_dir,
            "active": in_use,
            "size": file_size,
            "resident_data": resident_data,
            "data_runlist": data_runlist,
        }

    def _parse_ntfs_file_name_attr(self, content: bytes) -> dict | None:
        if len(content) < 66:
            return None
        parent_ref = int.from_bytes(content[0:6], "little")
        real_size = int.from_bytes(content[48:56], "little")
        name_len = content[64]
        namespace = content[65]
        name_bytes = content[66:66 + name_len * 2]
        try:
            name = name_bytes.decode("utf-16le", errors="ignore").strip("\x00")
        except UnicodeDecodeError:
            return None
        if not name:
            return None
        return {
            "parent_ref": parent_ref,
            "real_size": real_size,
            "namespace": namespace,
            "name": name,
        }

    def _build_ntfs_tree_model(self, entries: dict[int, dict], deleted_entries: dict[int, dict]) -> TreeNode:
        root = TreeNode(name="NTFS reconstruido", path="/NTFS reconstruido", is_file=False)
        active_root = TreeNode(name="Archivos y carpetas", path="/NTFS reconstruido/Archivos y carpetas", is_file=False)
        deleted_root = TreeNode(name="Borrados detectados", path="/NTFS reconstruido/Borrados detectados", is_file=False)
        root.add_child(active_root)
        root.add_child(deleted_root)
        node_cache: dict[tuple[str, int], TreeNode] = {}
        self.ntfs_recovery_entries = {}

        def ensure_node(entry: dict, source_name: str) -> TreeNode:
            cache_key = (source_name, entry["record_index"])
            if cache_key in node_cache:
                return node_cache[cache_key]
            parent_container = active_root if source_name == "active" else deleted_root
            parent_ref = entry["parent_ref"]
            if parent_ref == entry["record_index"] or parent_ref not in entries:
                parent_node = parent_container
            else:
                parent_node = ensure_node(entries[parent_ref], "active")
            safe_name = entry["name"].replace("/", "_")
            suffix = "" if entry["active"] else " [borrado]"
            node = TreeNode(
                name=f"{safe_name}{suffix}",
                path=f"{parent_node.path}/{safe_name}{suffix}",
                is_file=not entry["is_dir"],
                size_bytes=int(entry.get("size", 0) or 0),
                raw_lines=[json.dumps({k: v for k, v in entry.items() if k not in {"resident_data", "data_runlist"}}, ensure_ascii=True)],
            )
            parent_node.add_child(node)
            self.ntfs_recovery_entries[node.path] = entry
            node_cache[cache_key] = node
            return node

        for entry in sorted(entries.values(), key=lambda item: (item["parent_ref"], item["name"].lower())):
            if entry["record_index"] == 5:
                continue
            ensure_node(entry, "active")

        for entry in sorted(deleted_entries.values(), key=lambda item: (item["parent_ref"], item["name"].lower())):
            if entry["record_index"] == 5:
                continue
            ensure_node(entry, "deleted")

        root.children = {name: child for name, child in root.children.items() if child.children}
        return root

    def _preferred_direct_scan_path(self, device_path: str) -> str:
        try:
            result = subprocess.run(
                ["lsblk", "-J", "-b", "-o", "NAME,PATH,SIZE,TYPE"],
                capture_output=True,
                text=True,
                check=True,
            )
            payload = json.loads(result.stdout)
        except (subprocess.SubprocessError, json.JSONDecodeError, FileNotFoundError):
            return device_path

        def walk(nodes: list[dict]) -> dict | None:
            for node in nodes:
                if node.get("path") == device_path:
                    return node
                found = walk(node.get("children") or [])
                if found:
                    return found
            return None

        disk_node = walk(payload.get("blockdevices", []))
        if not disk_node:
            return device_path

        best_part_path = ""
        best_part_size = -1
        for child in disk_node.get("children") or []:
            if child.get("type") != "part":
                continue
            child_path = child.get("path") or ""
            child_size = int(child.get("size") or 0)
            if child_size > best_part_size and child_path.startswith("/dev/"):
                best_part_size = child_size
                best_part_path = child_path
        return best_part_path or device_path

    def _get_device_size(self, device_path: str) -> int:
        try:
            result = subprocess.run(
                ["blockdev", "--getsize64", device_path],
                capture_output=True,
                text=True,
                check=True,
            )
            size_text = result.stdout.strip()
            if size_text.isdigit():
                return int(size_text)
        except (subprocess.SubprocessError, FileNotFoundError):
            pass

        try:
            result = subprocess.run(
                ["lsblk", "-J", "-b", "-o", "PATH,SIZE"],
                capture_output=True,
                text=True,
                check=True,
            )
            payload = json.loads(result.stdout)
        except (subprocess.SubprocessError, json.JSONDecodeError, FileNotFoundError):
            payload = {"blockdevices": []}

        def walk(nodes: list[dict]) -> int:
            for node in nodes:
                if node.get("path") == device_path:
                    return int(node.get("size") or 0)
                found = walk(node.get("children") or [])
                if found:
                    return found
            return 0

        size = walk(payload.get("blockdevices", []))
        if size > 0:
            return size
        return os.path.getsize(device_path)

    def _scan_device_for_media(self, device_path: str) -> list[dict]:
        chunk_size = 64 * 1024 * 1024
        overlap = 4 * 1024 * 1024
        min_video_size = 256 * 1024
        max_results = 25000
        device_size = self._get_device_size(device_path)
        turbo_mode = bool(self.fast_scan_var.get())
        records: list[dict] = []
        seen_offsets: set[int] = set()
        last_status = 0.0
        last_tree_refresh = 0.0

        with open(device_path, "rb", buffering=0) as handle:
            offset = 0
            while offset < device_size and len(records) < max_results:
                if self.stop_scan_requested:
                    break
                self._wait_if_paused()
                handle.seek(offset)
                to_read = min(chunk_size, device_size - offset)
                chunk = handle.read(to_read)
                if not chunk:
                    break

                for record in self._find_media_signatures_in_chunk(chunk, offset, device_size, device_path):
                    record_offset = int(record["start_offset"])
                    if record_offset in seen_offsets:
                        continue
                    seen_offsets.add(record_offset)
                    annotation = {
                        "signature_label": record["detected_signature"],
                        "suggested_extension": record.get("suggested_extension", ".bin"),
                        "saved_path": None,
                        "original_name": None,
                        "file_description": self._humanize_signature_label(record["detected_signature"]),
                        "signature_offset": 0,
                        "content_size": int(record.get("estimated_size_bytes", 0)),
                    }
                    if annotation["signature_label"] in {"mp4_probable", "mov_probable"} and annotation["content_size"] < min_video_size:
                        continue
                    self.record_annotations[self._record_key(record)] = annotation
                    records.append(record)
                    self.direct_scan_records = list(records)
                    if len(records) >= max_results:
                        break

                now = time.time()
                if now - last_status > 0.5:
                    scanned_bytes = min(offset + to_read, device_size)
                    percent = (scanned_bytes / device_size) * 100 if device_size else 0
                    found_bytes = sum(self._preferred_record_size(record) for record in records)
                    self.scan_progress_var.set(percent)
                    self.scan_progress_text_var.set(f"{percent:.1f}% | {len(records)} hallazgos")
                    self._update_scan_eta(percent)
                    self._set_scan_size_status(scanned_bytes, device_size, found_bytes)
                    self.status_var.set(
                        f"Escaneo directo en progreso: {percent:.1f}% - {len(records)} hallazgo(s)."
                    )
                    if not turbo_mode and now - last_tree_refresh > 30.0:
                        self._refresh_summary_tree()
                        last_tree_refresh = now
                    self.root.update_idletasks()
                    last_status = now

                if offset + to_read >= device_size:
                    break
                offset += max(1, to_read - overlap)

        self.direct_scan_records = list(records)
        records.sort(key=self._record_sort_key)
        return records

    def _find_media_signatures_in_chunk(
        self, chunk: bytes, base_offset: int, device_size: int, device_path: str
    ) -> list[dict]:
        results: list[dict] = []

        def append_record(local_offset: int, size_bytes: int, label: str, extension: str, score: int = 100) -> None:
            if local_offset < 0 or size_bytes <= 0:
                return
            absolute_offset = base_offset + local_offset
            if absolute_offset + size_bytes > device_size:
                size_bytes = max(0, device_size - absolute_offset)
            if size_bytes <= 0:
                return
            results.append(
                {
                    "start_offset_hex": f"0x{absolute_offset:x}",
                    "start_offset": absolute_offset,
                    "block_size": 1,
                    "field_2": 0,
                    "field_3": size_bytes,
                    "field_4": size_bytes,
                    "field_5": size_bytes,
                    "field_6": size_bytes,
                    "estimated_span_units": size_bytes,
                    "estimated_size_bytes": size_bytes,
                    "offset_validity": "within_scan_range",
                    "classification": {
                        "label": "direct_signature_scan",
                        "bucket": "promising",
                        "confidence": "high",
                        "reasons": ["detected by direct signature scan"],
                    },
                    "score": score,
                    "detected_signature": label,
                    "suggested_extension": extension,
                    "scan_origin": "direct_scan",
                    "device_path": device_path,
                }
            )

        search_from = 0
        while True:
            pos = chunk.find(b"\xff\xd8\xff", search_from)
            if pos == -1:
                break
            end = chunk.find(b"\xff\xd9", pos + 2)
            if end != -1:
                append_record(pos, end + 2 - pos, "jpeg_probable", ".jpg", 120)
            search_from = pos + 3

        search_from = 0
        while True:
            pos = chunk.find(b"\x89PNG\r\n\x1a\n", search_from)
            if pos == -1:
                break
            end = chunk.find(b"IEND\xaeB`\x82", pos + 8)
            if end != -1:
                append_record(pos, end + 8 - pos, "png_probable", ".png", 120)
            search_from = pos + 8

        search_from = 0
        while True:
            pos = chunk.find(b"RIFF", search_from)
            if pos == -1 or pos + 12 > len(chunk):
                break
            kind = chunk[pos + 8:pos + 12]
            size = int.from_bytes(chunk[pos + 4:pos + 8], "little", signed=False) + 8
            if kind == b"WEBP" and 0 < size <= len(chunk) - pos:
                append_record(pos, size, "webp_probable", ".webp", 110)
            elif kind == b"AVI " and 256 * 1024 <= size <= len(chunk) - pos:
                append_record(pos, size, "avi_probable", ".avi", 90)
            search_from = pos + 4

        search_from = 0
        while True:
            pos = chunk.find(b"ftyp", search_from)
            if pos == -1 or pos < 4:
                break
            atom_pos = pos - 4
            if atom_pos + 8 > len(chunk):
                break
            size = int.from_bytes(chunk[atom_pos:atom_pos + 4], "big", signed=False)
            major = chunk[pos + 4:pos + 8]
            if 256 * 1024 <= size <= len(chunk) - atom_pos:
                if major in {b"qt  ", b"moov"}:
                    append_record(atom_pos, size, "mov_probable", ".mov", 100)
                elif major in {b"heic", b"heix", b"hevc", b"hevx", b"mif1", b"msf1"}:
                    append_record(atom_pos, size, "heic_probable", ".heic", 115)
                else:
                    append_record(atom_pos, size, "mp4_probable", ".mp4", 100)
            search_from = pos + 4

        return results

    def export_manifest(self) -> None:
        filtered_model = self.current_tree_root if self.current_tree_root.children else self._filtered_tree_model()
        if not filtered_model.children:
            messagebox.showinfo("Nada que exportar", "Abre primero un log.")
            return

        file_path = filedialog.asksaveasfilename(
            title="Guardar manifiesto",
            defaultextension=".json",
            filetypes=[("JSON files", "*.json"), ("All files", "*.*")],
        )
        if not file_path:
            return

        manifest = {
            "format": "dmde-log-viewer-manifest-v1",
            "entry_count": len(self.entries),
            "tree": self._serialize_node(filtered_model),
        }

        try:
            with open(file_path, "w", encoding="utf-8") as handle:
                json.dump(manifest, handle, indent=2, ensure_ascii=True)
        except OSError as exc:
            messagebox.showerror("Error al exportar", str(exc))
            return

        self.status_var.set(f"Manifiesto exportado a {file_path}.")

    def export_summary_csv(self) -> None:
        if not self.scan_summary:
            messagebox.showinfo("Nada que exportar", "Abre primero un log de estado.")
            return

        file_path = filedialog.asksaveasfilename(
            title="Guardar CSV de resumen",
            defaultextension=".csv",
            filetypes=[("CSV files", "*.csv"), ("All files", "*.*")],
        )
        if not file_path:
            return

        try:
            with open(file_path, "w", encoding="utf-8", newline="") as handle:
                writer = csv.writer(handle)
                writer.writerow(["section", "key", "value"])
                for key in ("scan_dev", "scan_range", "scan_range_end", "volume_candidates", "indx_offsets", "signature_types_total"):
                    writer.writerow(["metadata", key, self.scan_summary.get(key, "")])
                for token in self.scan_summary.get("signature_types_enabled", []):
                    writer.writerow(["signature", "enabled", token])
                for block_group in self.scan_summary.get("raw_block_groups", []):
                    group_name = block_group.get("group", "")
                    writer.writerow(["block_group", group_name, block_group.get("records_parsed", 0)])
                    for index, record in enumerate(block_group.get("decoded_records", []), start=1):
                        writer.writerow(
                            [
                                "block_record",
                                f"{group_name}:{index}",
                                record.get("start_offset_hex", ""),
                                record.get("start_offset", 0),
                                record.get("block_size", 0),
                                record.get("estimated_span_units", 0),
                                record.get("estimated_size_bytes", 0),
                                record.get("score", 0),
                                record.get("offset_validity", ""),
                                record.get("classification", {}).get("bucket", ""),
                                record.get("classification", {}).get("label", ""),
                                record.get("classification", {}).get("confidence", ""),
                                " | ".join(record.get("raw_fields", [])),
                            ]
                        )
        except OSError as exc:
            messagebox.showerror("Error al exportar", str(exc))
            return

        self.status_var.set(f"CSV de resumen exportado a {file_path}.")

    def export_findings_json(self) -> None:
        if not self.scan_summary:
            messagebox.showinfo("Nada que exportar", "Abre primero un log de estado.")
            return

        file_path = filedialog.asksaveasfilename(
            title="Guardar JSON de hallazgos",
            defaultextension=".json",
            filetypes=[("JSON files", "*.json"), ("All files", "*.*")],
        )
        if not file_path:
            return

        payload = {
            "format": "dmde-raw-findings-v1",
            "scan_dev": self.scan_summary.get("scan_dev", ""),
            "scan_range": self.scan_summary.get("scan_range", ""),
            "raw_block_groups": [],
        }
        for block_group in self.scan_summary.get("raw_block_groups", []):
            payload["raw_block_groups"].append(
                {
                    "group": block_group.get("group", ""),
                    "stats": self._compute_block_group_stats(block_group),
                    "decoded_records": block_group.get("decoded_records", []),
                }
            )

        try:
            with open(file_path, "w", encoding="utf-8") as handle:
                json.dump(payload, handle, indent=2, ensure_ascii=True)
        except OSError as exc:
            messagebox.showerror("Error al exportar", str(exc))
            return

        self.status_var.set(f"JSON de hallazgos exportado a {file_path}.")

    def analyze_extracted_folder(self) -> None:
        folder = filedialog.askdirectory(title="Elegir carpeta con fragmentos extraidos")
        if not folder:
            return

        annotated = 0
        skipped = 0
        for entry in os.scandir(folder):
            if not entry.is_file():
                continue
            match = EXTRACTED_NAME_RE.search(entry.name)
            if not match:
                skipped += 1
                continue
            offset = int(match.group(1), 16)
            size_bytes = int(match.group(2))
            record_key = f"{offset}:{size_bytes}"
            try:
                with open(entry.path, "rb") as handle:
                    header = handle.read(4096)
            except OSError:
                skipped += 1
                continue

            signature_info = detect_file_signature(header)
            self.record_annotations[record_key] = {
                "signature_label": signature_info["label"],
                "suggested_extension": signature_info["extension"],
                "saved_path": entry.path,
                "original_name": None,
            }
            annotated += 1

        self._refresh_summary_tree()
        self.status_var.set(f"Analizados {annotated} fragmento(s) de {folder}.")
        if annotated == 0:
            messagebox.showinfo(
                "Sin coincidencias",
                "No se encontraron nombres con patron off0x..._size... para relacionarlos con registros.",
            )
        elif skipped:
            messagebox.showinfo(
                "Analisis completado",
                f"Analizados {annotated} fragmento(s). Omitidos: {skipped}.",
            )

    def _auto_verify_types_if_possible(self) -> None:
        self.last_auto_verify_message = ""
        source_root = self.source_root_var.get().strip()
        if not source_root or not source_root.startswith("/dev/"):
            return
        records = self._collect_all_visible_pseudo_records()
        if not records:
            return
        checked, identified, errors, fatal_error = self._verify_records_against_device(source_root, records)
        if fatal_error:
            self.last_auto_verify_message = (
                f"Disco detectado en {source_root}, pero no se pudieron verificar tipos: {fatal_error}"
            )
            self.status_var.set(self.last_auto_verify_message)
            return
        self._refresh_summary_tree()
        if checked:
            self.last_auto_verify_message = (
                f"Disco detectado en {source_root}. Verificados {checked} fragmento(s), "
                f"identificados {identified} con tipo."
            )
            self.status_var.set(self.last_auto_verify_message)
        elif errors:
            self.last_auto_verify_message = (
                f"Disco detectado en {source_root}, pero hubo errores al verificar tipos."
            )
            self.status_var.set(self.last_auto_verify_message)

    def _verify_records_against_device(
        self, source_root: str, records: list[dict]
    ) -> tuple[int, int, list[str], str | None]:
        checked = 0
        identified = 0
        errors: list[str] = []
        try:
            with open(source_root, "rb", buffering=0) as handle:
                for record in records:
                    offset = int(record.get("start_offset", 0))
                    size_bytes = int(record.get("estimated_size_bytes", 0))
                    if offset < 0 or size_bytes <= 0:
                        continue
                    try:
                        handle.seek(offset)
                        header = handle.read(min(4096, size_bytes))
                        signature_info = detect_file_signature(header)
                        if signature_info["label"] == "unknown_signature":
                            sample = header
                            if size_bytes > len(sample):
                                # Read a much larger sample when the file signature is not at the very
                                # beginning of the fragment. This is common with recovered media chunks.
                                remaining = min(max(0, size_bytes - len(sample)), 64 * 1024 * 1024)
                                if remaining:
                                    sample += handle.read(remaining)
                            signature_info = detect_file_signature(sample)
                            file_info = self._detect_signature_with_file_sample(sample)
                            if file_info["label"] != "unknown_signature":
                                signature_info = file_info
                        self.record_annotations[self._record_key(record)] = {
                            "signature_label": signature_info["label"],
                            "suggested_extension": signature_info["extension"],
                            "saved_path": None,
                            "original_name": None,
                            "file_description": signature_info.get("description", ""),
                            "signature_offset": int(signature_info.get("signature_offset", 0) or 0),
                            "content_size": int(signature_info.get("content_size", 0) or 0),
                        }
                        record["detected_signature"] = signature_info["label"]
                        checked += 1
                        if signature_info["label"] != "unknown_signature" or signature_info.get("description"):
                            identified += 1
                    except OSError as exc:
                        errors.append(f"offset 0x{offset:x}: {exc}")
        except OSError as exc:
            return 0, 0, [], str(exc)
        return checked, identified, errors, None

    def _infer_source_device_from_scan(self) -> str:
        scan_dev = (self.scan_summary or {}).get("scan_dev", "")
        if not scan_dev:
            return ""
        match = DEV_BY_ID_RE.search(scan_dev)
        by_id_path = match.group(1) if match else ""
        if by_id_path and os.path.exists(by_id_path):
            try:
                resolved = os.path.realpath(by_id_path)
                if resolved.startswith("/dev/"):
                    return resolved
            except OSError:
                pass

        if by_id_path:
            fallback = self._resolve_device_from_lsblk(by_id_path)
            if fallback:
                return fallback
        return ""

    def _resolve_device_from_lsblk(self, by_id_path: str) -> str:
        try:
            result = subprocess.run(
                ["lsblk", "-J", "-o", "NAME,PATH,MODEL,SERIAL,VENDOR,TYPE"],
                capture_output=True,
                text=True,
                check=True,
            )
            payload = json.loads(result.stdout)
        except (subprocess.SubprocessError, json.JSONDecodeError, FileNotFoundError):
            return ""

        basename = os.path.basename(by_id_path)
        usb_match = USB_ID_RE.match(basename)
        model_hint = ""
        serial_hint = ""
        if usb_match:
            model_hint = usb_match.group(1).replace("_", " ").strip().lower()
            serial_hint = usb_match.group(2).strip().lower()

        candidates: list[dict] = []

        def walk(nodes: list[dict]) -> None:
            for node in nodes:
                if node.get("type") == "disk":
                    candidates.append(node)
                children = node.get("children") or []
                if children:
                    walk(children)

        walk(payload.get("blockdevices", []))

        for node in candidates:
            path = node.get("path") or ""
            model = (node.get("model") or "").strip().lower()
            serial = (node.get("serial") or "").strip().lower()
            if model_hint and serial_hint and model_hint in model and serial_hint in serial:
                return path

        for node in candidates:
            path = node.get("path") or ""
            model = (node.get("model") or "").strip().lower()
            if model_hint and model_hint in model:
                return path

        return ""

    def _detect_signature_with_file_sample(self, data: bytes) -> dict[str, str]:
        if not data:
            return {"label": "unknown_signature", "extension": ".bin", "description": ""}
        try:
            with tempfile.NamedTemporaryFile(delete=False) as handle:
                handle.write(data)
                temp_path = handle.name
        except OSError:
            return {"label": "unknown_signature", "extension": ".bin", "description": ""}

        try:
            return detect_with_file_command(temp_path)
        finally:
            try:
                os.unlink(temp_path)
            except OSError:
                pass

    def recover_selected(self) -> None:
        selected_item_ids = self.tree.selection()
        selected_nodes = [self.item_to_node[item_id] for item_id in selected_item_ids if item_id in self.item_to_node]
        checked_nodes = self._collect_checked_nodes()
        if checked_nodes:
            selected_nodes = checked_nodes
        if not selected_nodes:
            messagebox.showinfo("Nada seleccionado", "Marca o selecciona primero uno o varios archivos o carpetas.")
            return
        self._recover_nodes(selected_nodes)

    def _recover_nodes(self, selected_nodes: list[TreeNode]) -> None:
        source_root = self.source_root_var.get().strip()
        if not source_root:
            messagebox.showinfo("Falta el origen", "Elige primero un origen conectado.")
            return

        destination_root = self.choose_recovery_destination()
        if not destination_root:
            return

        if source_root.startswith("/dev/"):
            self._recover_from_block_device(source_root, destination_root, selected_nodes)
            return

        if not os.path.isdir(source_root):
            messagebox.showerror("Origen invalido", f"No es un directorio: {source_root}")
            return

        recoveries: list[tuple[str, str]] = []
        for node in selected_nodes:
            recoveries.extend(self._collect_recoveries(node, source_root, destination_root))

        if not recoveries:
            messagebox.showinfo(
                "Nada recuperable",
                "Los nodos seleccionados no se corresponden con rutas reales de origen.\n\n"
                "Esto es esperable en logs de estado raw de DMDE que no contienen rutas claras.",
            )
            return

        recovery_items = self._expand_filesystem_recoveries(recoveries)
        total_items = len(recovery_items)
        total_bytes = sum(size for kind, _src, _dst, size in recovery_items if kind == "file")
        copied = 0
        copied_bytes = 0
        errors: list[str] = []
        cancelled = False
        self.cancel_recovery_requested = False
        self.recovery_progress_var.set(0.0)
        self.recovery_progress_text_var.set("Preparando recuperacion...")
        self.recovery_eta_text_var.set("Calculando tiempo estimado...")
        self.recovery_size_text_var.set(
            f"Copiado: 0 B / {self._format_size_compact(total_bytes)}" if total_bytes > 0 else "Copiado: 0 B"
        )
        self.recovery_started_at = time.time()
        self._ensure_recovery_dialog("Recuperacion en curso")

        for kind, source_path, destination_path, size_bytes in recovery_items:
            if self.cancel_recovery_requested:
                cancelled = True
                break
            try:
                os.makedirs(os.path.dirname(destination_path), exist_ok=True)
                if kind == "dir":
                    if not os.path.exists(destination_path):
                        os.makedirs(destination_path, exist_ok=True)
                    self._normalize_output_permissions(destination_path)
                else:
                    with open(source_path, "rb") as in_handle, open(destination_path, "wb") as out_handle:
                        while True:
                            if self.cancel_recovery_requested:
                                cancelled = True
                                break
                            chunk = in_handle.read(1024 * 1024)
                            if not chunk:
                                break
                            out_handle.write(chunk)
                            copied_bytes += len(chunk)
                            self._update_recovery_progress(
                                copied_bytes,
                                total_bytes,
                                copied,
                                total_items,
                                os.path.basename(destination_path),
                            )
                    if cancelled:
                        break
                    self._normalize_output_permissions(destination_path)
                copied += 1
                self._update_recovery_progress(
                    copied_bytes,
                    total_bytes,
                    copied,
                    total_items,
                    os.path.basename(destination_path),
                )
            except OSError as exc:
                errors.append(f"{source_path} -> {destination_path}: {exc}")

        summary = f"Copiados {copied} elemento(s)."
        self._close_recovery_dialog()
        if cancelled:
            summary = (
                f"Copia cancelada. Elementos completados: {copied}."
                f"\nDatos copiados: {self._format_size_compact(copied_bytes)}"
            )
            if errors:
                summary += f"\nErrores: {len(errors)}"
            messagebox.showwarning("Copia cancelada", summary)
        elif errors:
            summary += f"\nErrores: {len(errors)}"
            messagebox.showwarning("Recuperacion terminada con errores", summary + "\n\n" + "\n".join(errors[:20]))
        else:
            messagebox.showinfo("Recuperacion completada", summary)
        self.status_var.set(summary)

    def _collect_checked_nodes(self) -> list[TreeNode]:
        ordered_nodes: list[TreeNode] = []
        seen_paths: set[str] = set()

        def walk(node: TreeNode) -> None:
            if node.path in self.checked_paths and node.path not in seen_paths:
                ordered_nodes.append(node)
                seen_paths.add(node.path)
            for child in node.children.values():
                walk(child)

        walk(self.current_tree_root)
        return ordered_nodes

    def _expand_filesystem_recoveries(self, recoveries: list[tuple[str, str]]) -> list[tuple[str, str, str, int]]:
        expanded: list[tuple[str, str, str, int]] = []
        seen_destinations: set[str] = set()
        for source_path, destination_path in recoveries:
            if os.path.isdir(source_path):
                if destination_path not in seen_destinations:
                    expanded.append(("dir", source_path, destination_path, 0))
                    seen_destinations.add(destination_path)
                for root, dirnames, filenames in os.walk(source_path):
                    relative_root = os.path.relpath(root, source_path)
                    target_root = destination_path if relative_root == "." else os.path.join(destination_path, relative_root)
                    if target_root not in seen_destinations:
                        expanded.append(("dir", root, target_root, 0))
                        seen_destinations.add(target_root)
                    for dirname in dirnames:
                        dir_source = os.path.join(root, dirname)
                        dir_target = os.path.join(target_root, dirname)
                        if dir_target not in seen_destinations:
                            expanded.append(("dir", dir_source, dir_target, 0))
                            seen_destinations.add(dir_target)
                    for filename in filenames:
                        file_source = os.path.join(root, filename)
                        file_target = os.path.join(target_root, filename)
                        try:
                            size = os.path.getsize(file_source)
                        except OSError:
                            size = 0
                        expanded.append(("file", file_source, file_target, size))
                continue
            try:
                size = os.path.getsize(source_path)
            except OSError:
                size = 0
            expanded.append(("file", source_path, destination_path, size))
        return expanded

    def _recover_from_block_device(self, device_path: str, destination_root: str, selected_nodes: list[TreeNode]) -> None:
        recoveries: list[tuple[dict, str]] = []
        ntfs_recoveries: list[tuple[dict, str]] = []
        seen_output_paths: set[str] = set()
        for node in selected_nodes:
            for record in self._collect_pseudo_records(node):
                output_path = self._build_pseudo_output_path(destination_root, record, seen_output_paths)
                recoveries.append((record, output_path))
                seen_output_paths.add(output_path)
            ntfs_recoveries.extend(self._collect_ntfs_recoveries(node, destination_root))

        if not recoveries and not ntfs_recoveries:
            messagebox.showinfo(
                "Nada recuperable",
                "Con un origen /dev solo se pueden extraer nodos pseudo raw o archivos NTFS con datos legibles.",
            )
            return

        total_items = len(recoveries) + len(ntfs_recoveries)
        total_bytes = 0
        for record, _output_path in recoveries:
            annotation = self.record_annotations.get(self._record_key(record), {})
            total_bytes += self._preferred_record_size(record, annotation)
        for entry, _output_path in ntfs_recoveries:
            if not entry.get("is_dir"):
                total_bytes += int(entry.get("size", 0) or 0)

        recovered = 0
        copied_bytes = 0
        errors: list[str] = []
        cancelled = False
        device_handles: dict[str, object] = {}
        self.cancel_recovery_requested = False
        self.recovery_progress_var.set(0.0)
        self.recovery_progress_text_var.set("Preparando extraccion...")
        self.recovery_eta_text_var.set("Calculando tiempo estimado...")
        self.recovery_size_text_var.set(
            f"Copiado: 0 B / {self._format_size_compact(total_bytes)}" if total_bytes > 0 else "Copiado: 0 B"
        )
        self.recovery_started_at = time.time()
        self._ensure_recovery_dialog("Extraccion en curso")
        try:
            for record, output_path in recoveries:
                if self.cancel_recovery_requested:
                    cancelled = True
                    break
                record_device_path = str(record.get("device_path") or device_path)
                handle = device_handles.get(record_device_path)
                if handle is None:
                    handle = open(record_device_path, "rb", buffering=0)
                    device_handles[record_device_path] = handle
                try:
                    offset = int(record.get("start_offset", 0))
                    size_bytes = int(record.get("estimated_size_bytes", 0))
                    if offset < 0 or size_bytes <= 0:
                        errors.append(f"Registro invalido para {output_path}")
                        continue
                    annotation = self.record_annotations.get(self._record_key(record), {})
                    signature_offset = int(annotation.get("signature_offset", 0) or 0)
                    content_size = int(annotation.get("content_size", 0) or 0)
                    read_offset = offset + signature_offset
                    available_size = max(0, size_bytes - signature_offset)
                    read_size = content_size if 0 < content_size <= available_size else available_size
                    if read_size <= 0:
                        read_offset = offset
                        read_size = size_bytes
                    os.makedirs(os.path.dirname(output_path), exist_ok=True)
                    handle.seek(read_offset)
                    sample = handle.read(min(read_size, 8 * 1024 * 1024))
                    signature_info = detect_file_signature(sample)
                    if signature_info["label"] == "unknown_signature" and annotation:
                        signature_info = {
                            "label": annotation.get("signature_label", "unknown_signature"),
                            "extension": annotation.get("suggested_extension", ".bin"),
                        }
                    output_path = self._apply_signature_extension(output_path, signature_info["extension"], seen_output_paths)
                    handle.seek(read_offset)
                    remaining = read_size
                    with open(output_path, "wb") as out_handle:
                        while remaining > 0:
                            if self.cancel_recovery_requested:
                                cancelled = True
                                break
                            chunk = handle.read(min(1024 * 1024, remaining))
                            if not chunk:
                                break
                            out_handle.write(chunk)
                            remaining -= len(chunk)
                            copied_bytes += len(chunk)
                            self._update_recovery_progress(
                                copied_bytes,
                                total_bytes,
                                recovered,
                                total_items,
                                os.path.basename(output_path),
                            )
                    if cancelled:
                        break
                    self._normalize_output_permissions(output_path)
                    annotation = {
                        "signature_label": signature_info["label"],
                        "suggested_extension": signature_info["extension"],
                        "saved_path": output_path,
                        "original_name": None,
                        "signature_offset": signature_offset,
                        "content_size": read_size,
                    }
                    record["detected_signature"] = signature_info["label"]
                    self.record_annotations[self._record_key(record)] = annotation
                    recovered += 1
                    self._update_recovery_progress(
                        copied_bytes,
                        total_bytes,
                        recovered,
                        total_items,
                        os.path.basename(output_path),
                    )
                except OSError as exc:
                    errors.append(f"{output_path}: {exc}")

            for entry, output_path in ntfs_recoveries:
                if cancelled or self.cancel_recovery_requested:
                    cancelled = True
                    break
                record_device_path = str(entry.get("device_path") or device_path)
                handle = device_handles.get(record_device_path)
                if handle is None:
                    handle = open(record_device_path, "rb", buffering=0)
                    device_handles[record_device_path] = handle
                try:
                    os.makedirs(os.path.dirname(output_path), exist_ok=True)
                    if entry.get("is_dir"):
                        os.makedirs(output_path, exist_ok=True)
                        self._normalize_output_permissions(output_path)
                        recovered += 1
                        self._update_recovery_progress(
                            copied_bytes,
                            total_bytes,
                            recovered,
                            total_items,
                            os.path.basename(output_path) or output_path,
                        )
                        continue
                    resident_data = entry.get("resident_data")
                    if resident_data is not None:
                        with open(output_path, "wb") as out_handle:
                            out_handle.write(resident_data)
                        copied_bytes += len(resident_data)
                        self._normalize_output_permissions(output_path)
                        recovered += 1
                        self._update_recovery_progress(
                            copied_bytes,
                            total_bytes,
                            recovered,
                            total_items,
                            os.path.basename(output_path),
                        )
                        continue
                    runlist = entry.get("data_runlist") or []
                    if not runlist:
                        errors.append(f"{output_path}: sin datos legibles")
                        continue
                    remaining = int(entry.get("size", 0) or 0)
                    cluster_size = int(entry.get("cluster_size", 0) or 0)
                    if remaining <= 0 or cluster_size <= 0:
                        errors.append(f"{output_path}: tamano o cluster invalidos")
                        continue
                    with open(output_path, "wb") as out_handle:
                        for lcn, run_len in runlist:
                            if self.cancel_recovery_requested:
                                cancelled = True
                                break
                            if remaining <= 0:
                                break
                            byte_len = run_len * cluster_size
                            read_len = min(byte_len, remaining)
                            handle.seek(lcn * cluster_size)
                            chunk = handle.read(read_len)
                            if not chunk:
                                break
                            out_handle.write(chunk)
                            remaining -= len(chunk)
                            copied_bytes += len(chunk)
                            self._update_recovery_progress(
                                copied_bytes,
                                total_bytes,
                                recovered,
                                total_items,
                                os.path.basename(output_path),
                            )
                    if cancelled:
                        break
                    self._normalize_output_permissions(output_path)
                    recovered += 1
                    self._update_recovery_progress(
                        copied_bytes,
                        total_bytes,
                        recovered,
                        total_items,
                        os.path.basename(output_path),
                    )
                except OSError as exc:
                    errors.append(f"{output_path}: {exc}")
        except OSError as exc:
            self._close_recovery_dialog()
            messagebox.showerror(
                "No se pudo leer el dispositivo",
                f"No se pudo abrir {device_path} en solo lectura.\n\n{exc}",
            )
            return
        finally:
            for handle in device_handles.values():
                try:
                    handle.close()
                except OSError:
                    pass

        summary = f"Extraidos {recovered} fragmento(s) desde {device_path}."
        self._close_recovery_dialog()
        if cancelled:
            summary = (
                f"Extraccion cancelada. Elementos completados: {recovered}."
                f"\nDatos copiados: {self._format_size_compact(copied_bytes)}"
            )
            if errors:
                summary += f"\nErrores: {len(errors)}"
            messagebox.showwarning("Extraccion cancelada", summary)
        elif errors:
            summary += f"\nErrores: {len(errors)}"
            messagebox.showwarning("Extraccion terminada con errores", summary + "\n\n" + "\n".join(errors[:20]))
        else:
            messagebox.showinfo("Extraccion completada", summary)
        self._refresh_summary_tree()
        self.status_var.set(summary)

    def _collect_ntfs_recoveries(self, node: TreeNode, destination_root: str) -> list[tuple[dict, str]]:
        recoveries: list[tuple[dict, str]] = []
        entry = self.ntfs_recovery_entries.get(node.path)
        if entry is not None:
            if self.skip_deleted_var.get() and not entry.get("active", True):
                entry = None
            if entry is not None:
                relative_path = node.path.removeprefix("/NTFS reconstruido/Archivos y carpetas/").removeprefix("/NTFS reconstruido/Borrados detectados/")
                if relative_path:
                    recoveries.append((entry, os.path.join(destination_root, relative_path)))
        for child in node.children.values():
            recoveries.extend(self._collect_ntfs_recoveries(child, destination_root))
        return recoveries

    def _collect_pseudo_records(self, node: TreeNode) -> list[dict]:
        records: list[dict] = []
        if node.raw_lines:
            try:
                payload = json.loads(node.raw_lines[0])
            except json.JSONDecodeError:
                payload = None
            if isinstance(payload, dict) and {"start_offset", "estimated_size_bytes", "classification"} <= set(payload.keys()):
                records.append(payload)
                return records

        for child in node.children.values():
            records.extend(self._collect_pseudo_records(child))
        return records

    def _collect_all_visible_pseudo_records(self) -> list[dict]:
        records: list[dict] = []
        seen_keys: set[str] = set()

        def walk(node: TreeNode) -> None:
            for child in node.children.values():
                child_records = self._collect_pseudo_records(child)
                for record in child_records:
                    key = self._record_key(record)
                    if key in seen_keys:
                        continue
                    seen_keys.add(key)
                    records.append(record)
                walk(child)

        walk(self.current_tree_root)
        return records

    def _build_pseudo_output_path(self, destination_root: str, record: dict, seen_output_paths: set[str]) -> str:
        bucket = record.get("classification", {}).get("bucket", "unknown")
        label = record.get("classification", {}).get("label", "unknown")
        offset = int(record.get("start_offset", 0))
        size_bytes = int(record.get("estimated_size_bytes", 0))
        score = int(record.get("score", 0))
        base_name = f"{bucket}_{label}_score{score}_off0x{offset:x}_size{size_bytes}.bin"
        output_path = os.path.join(destination_root, base_name)
        suffix = 1
        while output_path in seen_output_paths or os.path.exists(output_path):
            output_path = os.path.join(
                destination_root,
                f"{bucket}_{label}_score{score}_off0x{offset:x}_size{size_bytes}_{suffix}.bin",
            )
            suffix += 1
        return output_path

    def _apply_signature_extension(self, output_path: str, extension: str, seen_output_paths: set[str]) -> str:
        root, _old_ext = os.path.splitext(output_path)
        candidate = root + extension
        suffix = 1
        while candidate in seen_output_paths or os.path.exists(candidate):
            candidate = f"{root}_{suffix}{extension}"
            suffix += 1
        return candidate

    def _record_key(self, record: dict) -> str:
        return f"{int(record.get('start_offset', 0))}:{int(record.get('estimated_size_bytes', 0))}"

    def _normalize_output_permissions(self, target_path: str) -> None:
        uid = os.getuid()
        gid = os.getgid()
        sudo_uid = os.environ.get("SUDO_UID")
        sudo_gid = os.environ.get("SUDO_GID")
        if sudo_uid and sudo_gid:
            try:
                uid = int(sudo_uid)
                gid = int(sudo_gid)
            except ValueError:
                uid = os.getuid()
                gid = os.getgid()

        if os.path.isdir(target_path):
            for root, dirnames, filenames in os.walk(target_path):
                try:
                    os.chown(root, uid, gid)
                    os.chmod(root, 0o755)
                except OSError:
                    pass
                for dirname in dirnames:
                    path = os.path.join(root, dirname)
                    try:
                        os.chown(path, uid, gid)
                        os.chmod(path, 0o755)
                    except OSError:
                        pass
                for filename in filenames:
                    path = os.path.join(root, filename)
                    try:
                        os.chown(path, uid, gid)
                        os.chmod(path, 0o644)
                    except OSError:
                        pass
            return

        try:
            os.chown(target_path, uid, gid)
            os.chmod(target_path, 0o644)
        except OSError:
            pass

    def _collect_recoveries(
        self, node: TreeNode, source_root: str, destination_root: str
    ) -> list[tuple[str, str]]:
        recoveries: list[tuple[str, str]] = []
        relative_path = self._relative_node_path(node.path)
        if relative_path:
            source_path = os.path.join(source_root, relative_path)
            destination_path = os.path.join(destination_root, relative_path)
            if os.path.exists(source_path):
                recoveries.append((source_path, destination_path))
                return recoveries

        for child in node.children.values():
            recoveries.extend(self._collect_recoveries(child, source_root, destination_root))
        return recoveries

    def _relative_node_path(self, path: str) -> str:
        normalized = path.replace("\\", "/")
        for prefix in ("/$Root/", "/Root/"):
            if normalized.startswith(prefix):
                return normalized[len(prefix):].strip("/")
        if re.match(r"^[A-Za-z]:/", normalized):
            return normalized.split(":/", 1)[1].strip("/")
        if normalized.startswith("/"):
            return normalized.strip("/")
        return normalized.strip("/")

    def _deserialize_node(self, payload: dict) -> TreeNode:
        node = TreeNode(
            name=payload.get("name", ""),
            path=payload.get("path", ""),
            is_file=bool(payload.get("is_file", False)),
            size_bytes=int(payload.get("size_bytes", 0) or 0),
            raw_lines=list(payload.get("raw_lines", [])),
            line_numbers=list(payload.get("line_numbers", [])),
        )
        for child_payload in payload.get("children", []):
            child = self._deserialize_node(child_payload)
            node.children[child.name] = child
        return node

    def _serialize_node(self, node: TreeNode) -> dict:
        return {
            "name": node.name,
            "path": node.path,
            "is_file": node.is_file,
            "size_bytes": node.size_bytes,
            "line_numbers": node.line_numbers,
            "raw_lines": node.raw_lines,
            "children": [self._serialize_node(child) for child in sorted(node.children.values(), key=self._sort_key)],
        }


def main() -> None:
    initial_path = sys.argv[1] if len(sys.argv) > 1 else None
    root = tk.Tk()
    app = DmdeLogViewerApp(root)
    if initial_path:
        if initial_path.lower().endswith(".json"):
            try:
                with open(initial_path, "r", encoding="utf-8") as handle:
                    payload = json.load(handle)
                if payload.get("format") == "dmde-scan-session-v1":
                    app._load_scan_session_payload(payload, os.path.basename(initial_path))
                else:
                    app.entries = []
                    app.tree_model = app._deserialize_node(payload["tree"])
                    app.refresh_tree(use_existing_tree=True)
                    app.status_var.set(f"Manifiesto cargado desde {os.path.basename(initial_path)}.")
            except (OSError, json.JSONDecodeError, KeyError):
                app.status_var.set(f"No se pudo cargar el manifiesto inicial: {initial_path}")
        else:
            try:
                if not app.load_input_file(initial_path):
                    app.status_var.set(f"No se pudo cargar el log inicial: {initial_path}")
            except OSError:
                app.status_var.set(f"No se pudo cargar el log inicial: {initial_path}")
    root.mainloop()


if __name__ == "__main__":
    main()
