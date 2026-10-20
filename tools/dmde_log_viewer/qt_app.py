from __future__ import annotations

import json
import os
import re
import shutil
import struct
import subprocess
import sys
import time

try:
    from .core import DmdeLogParser, DmdeScanStateParser, EntryTreeBuilder, TreeNode
except ImportError:
    from core import DmdeLogParser, DmdeScanStateParser, EntryTreeBuilder, TreeNode  # type: ignore

try:
    from PySide6.QtCore import Qt, QSize
    from PySide6.QtGui import QAction, QColor, QBrush, QIcon, QPainter, QPixmap
    from PySide6.QtWidgets import (
        QApplication,
        QCheckBox,
        QDialog,
        QFileDialog,
        QComboBox,
        QHBoxLayout,
        QLabel,
        QLineEdit,
        QMainWindow,
        QMenu,
        QMessageBox,
        QTableWidget,
        QTableWidgetItem,
        QProgressBar,
        QPushButton,
        QSplitter,
        QTextEdit,
        QToolBar,
        QTreeWidget,
        QTreeWidgetItem,
        QVBoxLayout,
        QWidget,
    )
except ImportError:  # pragma: no cover - optional dependency
    Qt = None
    QSize = None
    QAction = None
    QColor = None
    QBrush = None
    QIcon = None
    QPainter = None
    QPixmap = None
    QApplication = None
    QCheckBox = None
    QDialog = object
    QFileDialog = None
    QComboBox = None
    QHBoxLayout = None
    QLabel = None
    QLineEdit = None
    QMainWindow = object
    QMenu = None
    QMessageBox = None
    QTableWidget = None
    QTableWidgetItem = None
    QProgressBar = None
    QPushButton = None
    QSplitter = None
    QTextEdit = None
    QToolBar = None
    QTreeWidget = None
    QTreeWidgetItem = None
    QVBoxLayout = None
    QWidget = None


IMAGE_EXTENSIONS = {".jpg", ".jpeg", ".png", ".gif", ".bmp", ".tif", ".tiff", ".webp", ".heic"}
VIDEO_EXTENSIONS = {".mp4", ".mov", ".avi", ".mkv", ".mts", ".3gp", ".mpg", ".mpeg"}
DOCUMENT_EXTENSIONS = {".pdf", ".doc", ".docx", ".txt", ".xml", ".odt", ".rtf", ".pgp"}
ARCHIVE_EXTENSIONS = {".zip", ".rar", ".7z", ".gz", ".tar"}


class RecoveryProgressDialog(QDialog):
    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setWindowTitle("Recuperando archivos")
        self.setModal(True)
        self.resize(520, 220)
        self.cancel_requested = False

        layout = QVBoxLayout(self)
        self.status_label = QLabel("Preparando recuperacion...")
        self.status_label.setWordWrap(True)
        layout.addWidget(self.status_label)

        self.progress_bar = QProgressBar()
        self.progress_bar.setRange(0, 1000)
        self.progress_bar.setValue(0)
        layout.addWidget(self.progress_bar)

        self.bytes_label = QLabel("0 B / 0 B")
        layout.addWidget(self.bytes_label)

        self.time_label = QLabel("Transcurrido: 0s | Queda aprox.: calculando...")
        layout.addWidget(self.time_label)

        button_row = QHBoxLayout()
        button_row.addStretch(1)
        self.cancel_button = QPushButton("Cancelar")
        self.cancel_button.clicked.connect(self.request_cancel)
        button_row.addWidget(self.cancel_button)
        layout.addLayout(button_row)

    def request_cancel(self) -> None:
        self.cancel_requested = True
        self.status_label.setText("Cancelando... espera a que termine el bloque actual.")
        self.cancel_button.setEnabled(False)


class ScanProgressDialog(QDialog):
    def __init__(self, title: str, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setWindowTitle(title)
        self.setModal(True)
        self.resize(560, 240)
        self.cancel_requested = False

        layout = QVBoxLayout(self)
        self.summary_label = QLabel("Preparando escaneo...")
        self.summary_label.setWordWrap(True)
        layout.addWidget(self.summary_label)

        self.progress_bar = QProgressBar()
        self.progress_bar.setRange(0, 1000)
        self.progress_bar.setValue(0)
        layout.addWidget(self.progress_bar)

        self.time_label = QLabel("Transcurrido: 0s | Queda aprox.: calculando...")
        layout.addWidget(self.time_label)

        self.size_label = QLabel("Escaneado: 0 B / desconocido | Recuperable estimado: 0 B")
        layout.addWidget(self.size_label)

        self.detail_label = QLabel("")
        self.detail_label.setWordWrap(True)
        layout.addWidget(self.detail_label)

        button_row = QHBoxLayout()
        button_row.addStretch(1)
        self.cancel_button = QPushButton("Stop")
        self.cancel_button.clicked.connect(self.request_cancel)
        button_row.addWidget(self.cancel_button)
        layout.addLayout(button_row)

    def request_cancel(self) -> None:
        self.cancel_requested = True
        self.detail_label.setText("Deteniendo escaneo... espera a que termine el bloque actual.")
        self.cancel_button.setEnabled(False)


class QtRecoveryMainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("Recuperador DMDE - Vista Qt")
        self.resize(1480, 920)

        self.parser = DmdeLogParser()
        self.builder = EntryTreeBuilder()
        self.scan_state_parser = DmdeScanStateParser()
        self.entries = []
        self.tree_model = TreeNode(name="/", path="/")
        self.current_file_path = ""
        self.current_summary: dict = {}
        self.checked_paths: set[str] = set()
        self._updating_checks = False
        self._scan_tree_refreshing = False
        self.tree_items_by_path: dict[str, QTreeWidgetItem] = {}
        self.icon_cache: dict[str, QIcon] = {}
        self.node_size_cache: dict[str, int] = {}
        self.recovery_dialog: RecoveryProgressDialog | None = None
        self.scan_dialog: ScanProgressDialog | None = None
        self.ntfs_recovery_entries: dict[str, dict] = {}

        self._build_ui()
        self._apply_dark_theme()

    def _build_ui(self) -> None:
        toolbar = QToolBar("Principal", self)
        toolbar.setMovable(False)
        toolbar.setIconSize(QSize(18, 18))
        self.addToolBar(toolbar)

        open_log_button = QPushButton("Abrir log")
        open_log_button.clicked.connect(self.open_log)
        toolbar.addWidget(open_log_button)

        open_manifest_button = QPushButton("Abrir manifiesto")
        open_manifest_button.clicked.connect(self.open_manifest)
        toolbar.addWidget(open_manifest_button)

        open_disk_button = QPushButton("Abrir disco")
        open_disk_button.clicked.connect(self.open_device_picker)
        toolbar.addWidget(open_disk_button)

        ntfs_scan_button = QPushButton("Escaneo NTFS")
        ntfs_scan_button.clicked.connect(self.ntfs_low_level_scan)
        toolbar.addWidget(ntfs_scan_button)

        recover_button = QPushButton("Recuperar marcados")
        recover_button.clicked.connect(self.recover_marked)
        toolbar.addWidget(recover_button)

        expand_action = QAction("Expandir", self)
        expand_action.triggered.connect(lambda: self.tree.expandAll())
        toolbar.addAction(expand_action)

        collapse_action = QAction("Contraer", self)
        collapse_action.triggered.connect(lambda: self.tree.collapseAll())
        toolbar.addAction(collapse_action)

        toolbar.addSeparator()

        self.source_edit = QLineEdit()
        self.source_edit.setPlaceholderText("Disco detectado o ruta de origen")
        self.source_edit.setMinimumWidth(360)
        toolbar.addWidget(self.source_edit)

        central = QWidget()
        root_layout = QVBoxLayout(central)
        root_layout.setContentsMargins(8, 8, 8, 8)
        root_layout.setSpacing(8)

        filter_row = QHBoxLayout()
        filter_row.setSpacing(8)
        filter_row.addWidget(QLabel("Filtro rapido:"))

        self.filter_edit = QLineEdit()
        self.filter_edit.setPlaceholderText("Nombre, ruta o extension...")
        self.filter_edit.textChanged.connect(self.refresh_tree)
        filter_row.addWidget(self.filter_edit, 2)

        filter_row.addWidget(QLabel("Biblioteca:"))
        self.category_combo = QComboBox()
        self.category_combo.addItems(["Todos", "Imagenes", "Videos", "Documentos", "Comprimidos", "Otros"])
        self.category_combo.currentTextChanged.connect(self.refresh_tree)
        filter_row.addWidget(self.category_combo)

        self.turbo_checkbox = QCheckBox("Modo turbo")
        self.turbo_checkbox.setChecked(True)
        filter_row.addWidget(self.turbo_checkbox)

        self.selection_label = QLabel("0 marcados")
        filter_row.addWidget(self.selection_label)
        filter_row.addStretch(1)
        root_layout.addLayout(filter_row)

        splitter = QSplitter(Qt.Horizontal, self)

        left_panel = QWidget()
        left_layout = QVBoxLayout(left_panel)
        left_layout.setContentsMargins(0, 0, 0, 0)

        self.tree = QTreeWidget()
        self.tree.setHeaderLabels(["Ruta", "Tipo", "Tamano", "Lineas"])
        self.tree.setAlternatingRowColors(True)
        self.tree.setUniformRowHeights(False)
        self.tree.setIconSize(QSize(14, 14))
        self.tree.setIndentation(18)
        self.tree.setAnimated(False)
        self.tree.setExpandsOnDoubleClick(True)
        self.tree.setAllColumnsShowFocus(False)
        self.tree.itemSelectionChanged.connect(self._update_details)
        self.tree.itemChanged.connect(self._handle_item_changed)
        self.tree.setContextMenuPolicy(Qt.CustomContextMenu)
        self.tree.customContextMenuRequested.connect(self._open_tree_context_menu)
        self.tree.setColumnWidth(0, 420)
        self.tree.setColumnWidth(1, 140)
        self.tree.setColumnWidth(2, 120)
        self.tree.setColumnWidth(3, 80)
        left_layout.addWidget(self.tree)

        right_panel = QWidget()
        right_layout = QVBoxLayout(right_panel)
        right_layout.setContentsMargins(0, 0, 0, 0)
        right_layout.setSpacing(8)

        self.summary_label = QLabel("Base Qt preparada para la futura migracion.")
        self.summary_label.setWordWrap(True)
        right_layout.addWidget(self.summary_label)

        preview_title = QLabel("Vista previa")
        right_layout.addWidget(preview_title)

        self.preview_label = QLabel("Sin vista previa")
        self.preview_label.setAlignment(Qt.AlignCenter)
        self.preview_label.setMinimumHeight(280)
        self.preview_label.setWordWrap(True)
        right_layout.addWidget(self.preview_label)

        raw_title = QLabel("Resumen / lineas crudas")
        right_layout.addWidget(raw_title)

        self.raw_text = QTextEdit()
        self.raw_text.setReadOnly(True)
        right_layout.addWidget(self.raw_text, 2)

        detail_title = QLabel("Detalles del nodo")
        right_layout.addWidget(detail_title)

        self.detail_text = QTextEdit()
        self.detail_text.setReadOnly(True)
        right_layout.addWidget(self.detail_text, 3)

        splitter.addWidget(left_panel)
        splitter.addWidget(right_panel)
        splitter.setStretchFactor(0, 3)
        splitter.setStretchFactor(1, 2)
        root_layout.addWidget(splitter, 1)

        self.setCentralWidget(central)
        self.statusBar().showMessage("Qt listo. Falta migrar escaneo, recuperacion y previsualizacion avanzada.")

    def open_device_picker(self) -> None:
        devices = self._list_scannable_devices()
        if not devices:
            QMessageBox.information(self, "Sin discos", "No se encontraron discos o particiones conectados.")
            return

        dialog = QDialog(self)
        dialog.setWindowTitle("Elegir disco o particion")
        dialog.resize(960, 480)
        layout = QVBoxLayout(dialog)

        table = QTableWidget(len(devices), 6)
        table.setHorizontalHeaderLabels(["Dispositivo", "Tipo", "Tamano", "FS", "Montaje", "Modelo"])
        table.setSelectionBehavior(QTableWidget.SelectRows)
        table.setSelectionMode(QTableWidget.SingleSelection)
        table.setEditTriggers(QTableWidget.NoEditTriggers)
        for row, device in enumerate(devices):
            values = [
                str(device["path"]),
                str(device["type"]),
                self._format_size(int(device["size"])),
                str(device["fstype"]),
                str(device["mountpoint"]),
                str(device["label"]),
            ]
            for column, value in enumerate(values):
                table.setItem(row, column, QTableWidgetItem(value))
        table.resizeColumnsToContents()
        layout.addWidget(table)

        button_row = QHBoxLayout()
        button_row.addStretch(1)
        use_button = QPushButton("Usar origen")
        cancel_button = QPushButton("Cancelar")
        button_row.addWidget(cancel_button)
        button_row.addWidget(use_button)
        layout.addLayout(button_row)

        def choose_selected() -> None:
            row = table.currentRow()
            if row < 0:
                return
            picked_path = str(devices[row]["path"])
            resolved_path = self._preferred_scan_path(picked_path, prefer_ntfs=True)
            self.source_edit.setText(resolved_path)
            dialog.accept()

        use_button.clicked.connect(choose_selected)
        cancel_button.clicked.connect(dialog.reject)
        table.doubleClicked.connect(lambda _index: choose_selected())
        if devices:
            table.selectRow(0)
        dialog.exec()

    def _list_scannable_devices(self) -> list[dict[str, str | int]]:
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
                                part
                                for part in [(node.get("model") or "").strip(), (node.get("serial") or "").strip()]
                                if part
                            ),
                        }
                    )
                walk(node.get("children") or [])

        walk(payload.get("blockdevices", []))
        devices.sort(key=lambda item: (item["type"] != "disk", str(item["path"])))
        return devices

    def _preferred_scan_path(self, device_path: str, prefer_ntfs: bool = False) -> str:
        flat_devices = self._list_scannable_devices()
        selected_flat = next((device for device in flat_devices if str(device.get("path")) == device_path), None)
        if selected_flat is not None and str(selected_flat.get("type")) == "part":
            return device_path

        prefix = f"{device_path}"
        child_parts = [
            device
            for device in flat_devices
            if str(device.get("type")) == "part" and str(device.get("path", "")).startswith(prefix)
        ]
        if child_parts:
            ntfs_parts = [
                device for device in child_parts if "ntfs" in str(device.get("fstype") or "").lower()
            ]
            candidates = ntfs_parts if prefer_ntfs and ntfs_parts else child_parts
            best = max(candidates, key=lambda item: int(item.get("size") or 0))
            best_path = str(best.get("path") or "")
            if best_path.startswith("/dev/"):
                return best_path

        try:
            result = subprocess.run(
                ["lsblk", "-J", "-b", "-o", "PATH,SIZE,TYPE,FSTYPE"],
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

        selected_node = walk(payload.get("blockdevices", []))
        if not selected_node:
            return device_path
        if selected_node.get("type") == "part":
            return device_path

        best_ntfs_part = ""
        best_ntfs_size = -1
        best_part = ""
        best_part_size = -1
        for child in selected_node.get("children") or []:
            if child.get("type") != "part":
                continue
            child_path = child.get("path") or ""
            child_size = int(child.get("size") or 0)
            child_fstype = str(child.get("fstype") or "").lower()
            if child_path.startswith("/dev/") and child_size > best_part_size:
                best_part = child_path
                best_part_size = child_size
            if prefer_ntfs and "ntfs" in child_fstype and child_path.startswith("/dev/") and child_size > best_ntfs_size:
                best_ntfs_part = child_path
                best_ntfs_size = child_size

        return best_ntfs_part or best_part or device_path

    def _apply_dark_theme(self) -> None:
        self.setStyleSheet(
            """
            QMainWindow, QWidget {
                background: #000000;
                color: #f5f5f5;
                font-size: 13px;
            }
            QToolBar {
                background: #050505;
                border: 0;
                spacing: 6px;
            }
            QPushButton, QComboBox, QLineEdit {
                background: #0a0a0a;
                color: #f5f5f5;
                border: 1px solid #2a2a2a;
                padding: 6px 10px;
                min-height: 28px;
            }
            QPushButton:hover, QComboBox:hover, QLineEdit:hover {
                border: 1px solid #16a34a;
            }
            QTreeWidget, QTextEdit {
                background: #070707;
                color: #ececec;
                border: 1px solid #1f1f1f;
                alternate-background-color: #0b0b0b;
                selection-background-color: #1a2b20;
                selection-color: #ffffff;
            }
            QHeaderView::section {
                background: #090909;
                color: #d6d6d6;
                border: 0;
                border-bottom: 1px solid #1f1f1f;
                padding: 6px;
            }
            QTreeView::item {
                padding: 4px 6px;
                border-bottom: 1px solid #0e0e0e;
            }
            QTreeView::item:selected {
                background: #1a2b20;
                color: #ffffff;
            }
            QTreeView::branch {
                background: #070707;
            }
            QLabel {
                color: #f5f5f5;
            }
            QScrollBar:vertical, QScrollBar:horizontal {
                background: #050505;
                border: none;
            }
            QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
                background: #16a34a;
                min-height: 24px;
                min-width: 24px;
            }
            """
        )

    def open_log(self) -> None:
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Abrir log de DMDE",
            "",
            "Logs (*.log *.txt *.csv);;Todos (*.*)",
        )
        if not file_path:
            return
        self.load_log(file_path)

    def open_manifest(self) -> None:
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Abrir manifiesto o sesion",
            "",
            "JSON (*.json);;Todos (*.*)",
        )
        if not file_path:
            return
        try:
            with open(file_path, "r", encoding="utf-8") as handle:
                payload = json.load(handle)
        except (OSError, json.JSONDecodeError) as exc:
            QMessageBox.critical(self, "Error al abrir", str(exc))
            return

        if payload.get("format") == "dmde-scan-session-v1":
            tree_payload = payload.get("tree")
            self.source_edit.setText(payload.get("source_root", ""))
        else:
            tree_payload = payload.get("tree")

        if not isinstance(tree_payload, dict):
            QMessageBox.warning(self, "No soportado", "Este manifiesto no contiene un arbol compatible.")
            return

        self.tree_model = self._deserialize_node(tree_payload)
        self.current_summary = {"origen": file_path, "tipo": "manifiesto"}
        self.refresh_tree()
        self.raw_text.setPlainText(json.dumps(payload, indent=2, ensure_ascii=False)[:30000])
        self.summary_label.setText(f"Manifiesto cargado: {os.path.basename(file_path)}")
        self.statusBar().showMessage(f"Manifiesto cargado: {os.path.basename(file_path)}")

    def load_log(self, file_path: str) -> None:
        try:
            self.entries = self.parser.parse_file(file_path)
            self.tree_model = self.builder.build(self.entries)
            self.current_summary = self.scan_state_parser.build_summary(
                self.scan_state_parser.parse_assignments(file_path)
            )
        except OSError as exc:
            QMessageBox.critical(self, "Error al abrir", str(exc))
            return

        self.current_file_path = file_path
        self.refresh_tree()
        self.raw_text.setPlainText(json.dumps(self.current_summary, indent=2, ensure_ascii=False))
        self.summary_label.setText(
            f"Log cargado: {os.path.basename(file_path)} | rutas detectadas: {len(self.entries)}"
        )
        inferred_source = self.current_summary.get("scan_dev", "")
        if inferred_source:
            self.source_edit.setText(str(inferred_source))
        self.statusBar().showMessage("Vista Qt actualizada desde log.")

    def refresh_tree(self) -> None:
        if self._scan_tree_refreshing:
            selected_category = "Todos"
            filter_text = ""
        else:
            filter_text = self.filter_edit.text().strip().lower()
            selected_category = self.category_combo.currentText()

        self.node_size_cache.clear()
        self._updating_checks = True
        self.tree.clear()
        self.tree_items_by_path.clear()

        for child in sorted(self.tree_model.children.values(), key=self._sort_key):
            self._add_tree_item(None, child, filter_text, selected_category)

        self._updating_checks = False
        self._update_selection_label()
        self.tree.expandToDepth(1)

    def _add_tree_item(
        self,
        parent: QTreeWidgetItem | None,
        node: TreeNode,
        filter_text: str,
        selected_category: str,
    ) -> QTreeWidgetItem | None:
        if not self._node_matches_filters(node, filter_text, selected_category):
            return None

        item = QTreeWidgetItem(
            [
                node.name,
                self._display_type(node),
                self._format_size(node.size_bytes if node.is_file else self._node_total_size(node)),
                str(len(node.raw_lines)),
            ]
        )
        item.setData(0, Qt.UserRole, node)
        item.setFlags(item.flags() | Qt.ItemIsUserCheckable | Qt.ItemIsSelectable | Qt.ItemIsEnabled)
        item.setCheckState(0, Qt.Checked if node.path in self.checked_paths else Qt.Unchecked)
        item.setIcon(0, self._icon_for_node(node))

        if not node.is_file:
            item.setForeground(1, QBrush(QColor("#9aa49d")))
        elif self._category_for_node(node) == "Imagenes":
            item.setForeground(1, QBrush(QColor("#aebfc8")))
        elif self._category_for_node(node) == "Videos":
            item.setForeground(1, QBrush(QColor("#c7ba96")))

        if parent is None:
            self.tree.addTopLevelItem(item)
        else:
            parent.addChild(item)
        self.tree_items_by_path[node.path] = item

        for child in sorted(node.children.values(), key=self._sort_key):
            self._add_tree_item(item, child, filter_text, selected_category)
        return item

    def _node_matches_filters(self, node: TreeNode, filter_text: str, selected_category: str) -> bool:
        own_match = not filter_text or filter_text in node.name.lower() or filter_text in node.path.lower()
        category_match = selected_category == "Todos" or self._category_for_node(node) == selected_category

        if own_match and category_match:
            return True

        return any(self._node_matches_filters(child, filter_text, selected_category) for child in node.children.values())

    def _category_for_node(self, node: TreeNode) -> str:
        if not node.is_file:
            return "Todos"
        extension = os.path.splitext(node.name)[1].lower()
        if extension in IMAGE_EXTENSIONS:
            return "Imagenes"
        if extension in VIDEO_EXTENSIONS:
            return "Videos"
        if extension in DOCUMENT_EXTENSIONS:
            return "Documentos"
        if extension in ARCHIVE_EXTENSIONS:
            return "Comprimidos"
        return "Otros"

    def _display_type(self, node: TreeNode) -> str:
        if not node.is_file:
            return "Carpeta"
        extension = os.path.splitext(node.name)[1].lower()
        if extension:
            return extension[1:].upper()
        return "Archivo"

    def _sort_key(self, node: TreeNode) -> tuple[int, str]:
        return (1 if node.is_file else 0, node.name.lower())

    def _node_total_size(self, node: TreeNode) -> int:
        cached = self.node_size_cache.get(node.path)
        if cached is not None:
            return cached
        if node.is_file:
            size = int(node.size_bytes or 0)
            self.node_size_cache[node.path] = size
            return size
        size = sum(self._node_total_size(child) for child in node.children.values())
        self.node_size_cache[node.path] = size
        return size

    def _format_size(self, size: int) -> str:
        units = ["B", "KB", "MB", "GB", "TB", "PB"]
        value = float(size)
        unit_index = 0
        while value >= 1024.0 and unit_index < len(units) - 1:
            value /= 1024.0
            unit_index += 1
        if unit_index == 0:
            return f"{int(value)} {units[unit_index]}"
        return f"{value:.1f} {units[unit_index]}"

    def _update_details(self) -> None:
        selection = self.tree.selectedItems()
        if not selection:
            self.preview_label.setText("Sin vista previa")
            self.preview_label.setPixmap(QPixmap())
            self.detail_text.clear()
            return

        node = selection[0].data(0, Qt.UserRole)
        if not isinstance(node, TreeNode):
            return
        size_text = selection[0].text(2)
        raw_size = node.size_bytes if node.is_file else self.node_size_cache.get(node.path, 0)

        details = {
            "nombre": node.name,
            "ruta": node.path,
            "tipo": "archivo" if node.is_file else "carpeta",
            "categoria": self._category_for_node(node),
            "tamano": size_text,
            "tamano_bytes": raw_size,
            "hijos": len(node.children),
            "lineas_asociadas": node.line_numbers[:50],
        }
        self.detail_text.setPlainText(json.dumps(details, indent=2, ensure_ascii=False))

        preview_path = self._preview_path_for_node(node)
        if preview_path and self._node_supports_image_preview(node):
            self._show_image_preview(preview_path)
        elif preview_path and self._node_supports_video_preview(node):
            self.preview_label.setPixmap(QPixmap())
            self.preview_label.setText(
                "Video detectado.\n\nEn la migracion completa ira aqui el reproductor y el fotograma previo."
            )
        else:
            self.preview_label.setPixmap(QPixmap())
            self.preview_label.setText("Sin vista previa disponible para este elemento.")

    def _preview_path_for_node(self, node: TreeNode) -> str:
        if not node.is_file:
            return ""
        if self.current_file_path and os.path.isfile(self.current_file_path):
            base_dir = os.path.dirname(self.current_file_path)
            candidate = os.path.join(base_dir, node.name)
            if os.path.isfile(candidate):
                return candidate
        return ""

    def ntfs_low_level_scan(self) -> None:
        device_path = self.source_edit.text().strip()
        if not device_path:
            QMessageBox.information(self, "Falta el origen", "Elige primero un disco o particion.")
            return
        if not device_path.startswith("/dev/"):
            QMessageBox.warning(
                self,
                "Origen no valido",
                "El escaneo NTFS Qt actual espera una particion o disco tipo /dev/...",
            )
            return

        scan_path = self._preferred_scan_path(device_path, prefer_ntfs=True)

        self.scan_dialog = ScanProgressDialog("Escaneo NTFS", self)
        self.scan_dialog.show()
        QApplication.processEvents()
        started_at = time.time()
        if self.turbo_checkbox.isChecked():
            self.statusBar().showMessage("Escaneo NTFS en modo turbo: el arbol se actualizara al final o al detener.")
        try:
            ntfs_root = self._scan_ntfs_tree(scan_path, started_at)
        except Exception as exc:
            if self.scan_dialog is not None:
                self.scan_dialog.close()
                self.scan_dialog = None
            QMessageBox.critical(
                self,
                "Escaneo NTFS",
                f"{exc}\n\nRuta intentada: {scan_path}",
            )
            return

        cancelled = bool(self.scan_dialog.cancel_requested) if self.scan_dialog is not None else False
        if self.scan_dialog is not None:
            self.scan_dialog.close()
            self.scan_dialog = None

        self.tree_model = ntfs_root
        self.refresh_tree()
        files_found = self._count_tree_files(ntfs_root)
        if cancelled:
            summary = f"Escaneo NTFS detenido. Se muestran {files_found} archivo(s) encontrados hasta ahora."
        else:
            summary = f"Escaneo NTFS terminado: {files_found} archivo(s) y carpetas reconstruidos."
        self.summary_label.setText(summary)
        self.statusBar().showMessage(summary)
        self.source_edit.setText(scan_path)

    def _scan_ntfs_tree(self, device_path: str, started_at: float) -> TreeNode:
        device_size = self._get_device_size(device_path)
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
            record_count = max(1, min(mft_stream_size // ntfs["record_size"], 500000))

            entries: dict[int, dict] = {}
            deleted_entries: dict[int, dict] = {}
            active_total_bytes = 0
            last_progress_refresh = 0.0
            last_tree_refresh = 0.0
            turbo_mode = bool(self.turbo_checkbox.isChecked())
            for record_index in range(record_count):
                if self.scan_dialog is not None and self.scan_dialog.cancel_requested:
                    break
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
                if now - last_progress_refresh > 0.4:
                    scanned_bytes = min((record_index + 1) * ntfs["record_size"], mft_stream_size)
                    percent = (record_index + 1) / record_count * 100 if record_count else 0.0
                    self._update_scan_progress(
                        percent,
                        len(entries),
                        len(deleted_entries),
                        scanned_bytes,
                        mft_stream_size,
                        active_total_bytes,
                        started_at,
                    )
                    QApplication.processEvents()
                    last_progress_refresh = now
                if not turbo_mode and now - last_tree_refresh > 20.0:
                    self.tree_model = self._build_ntfs_tree_model(entries, deleted_entries)
                    self._scan_tree_refreshing = True
                    try:
                        self.refresh_tree()
                    finally:
                        self._scan_tree_refreshing = False
                    if self.scan_dialog is not None:
                        self.scan_dialog.detail_label.setText(
                            f"Actualizacion interactiva del arbol: {len(entries)} activos y {len(deleted_entries)} borrados."
                        )
                    QApplication.processEvents()
                    last_tree_refresh = now

        return self._build_ntfs_tree_model(entries, deleted_entries)

    def _update_scan_progress(
        self,
        percent: float,
        active_count: int,
        deleted_count: int,
        scanned_bytes: int,
        total_bytes: int,
        found_bytes: int,
        started_at: float,
    ) -> None:
        if self.scan_dialog is None:
            return
        self.scan_dialog.progress_bar.setValue(int(max(0.0, min(100.0, percent)) * 10))
        self.scan_dialog.summary_label.setText(
            f"NTFS {percent:.1f}% | {active_count} activos | {deleted_count} borrados"
        )
        elapsed = max(0.001, time.time() - started_at)
        if percent > 0.0 and percent < 100.0:
            estimated_total = elapsed / (percent / 100.0)
            remaining = max(0.0, estimated_total - elapsed)
            remaining_text = self._format_duration(int(remaining))
        elif percent >= 100.0:
            remaining_text = "0s"
        else:
            remaining_text = "calculando..."
        self.scan_dialog.time_label.setText(
            f"Transcurrido: {self._format_duration(int(elapsed))} | Queda aprox.: {remaining_text}"
        )
        self.scan_dialog.size_label.setText(
            f"MFT leida: {self._format_size(scanned_bytes)} / {self._format_size(total_bytes)} | "
            f"Datos reconstruibles: {self._format_size(found_bytes)}"
        )
        self.scan_dialog.detail_label.setText(
            f"Escaneo NTFS en progreso sobre {self.source_edit.text().strip()}."
        )

    def _read_exact(self, handle: object, offset: int, size: int) -> bytes:
        handle.seek(offset)
        data = handle.read(size)
        if len(data) != size:
            raise OSError(f"No se pudieron leer {size} bytes en 0x{offset:x}")
        return data

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
            return os.path.getsize(device_path)
        except OSError:
            return 0

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

    def _count_tree_files(self, node: TreeNode) -> int:
        total = 1 if node.is_file else 0
        for child in node.children.values():
            total += self._count_tree_files(child)
        return total

    def recover_marked(self) -> None:
        selected_nodes = self._collect_checked_nodes()
        if not selected_nodes:
            QMessageBox.information(self, "Nada marcado", "Marca archivos o carpetas primero.")
            return

        destination_root = QFileDialog.getExistingDirectory(
            self,
            "Elegir destino de recuperacion",
            "",
        )
        if not destination_root:
            return

        source_root = self.source_edit.text().strip()
        if source_root.startswith("/dev/") and self.ntfs_recovery_entries:
            ntfs_plan = self._build_ntfs_recovery_plan(selected_nodes, destination_root)
            if not ntfs_plan:
                QMessageBox.warning(
                    self,
                    "Sin datos NTFS recuperables",
                    "Los nodos marcados no contienen datos NTFS legibles con la informacion actual.",
                )
                return
            self._run_ntfs_recovery_plan(ntfs_plan, source_root)
            return

        copy_plan = self._build_copy_plan(selected_nodes, destination_root)
        if not copy_plan:
            QMessageBox.warning(
                self,
                "Sin archivos copiables",
                "No se encontraron archivos reales accesibles para copiar con la informacion actual.\n\n"
                "Consejo: en esta primera version Qt funciona mejor si 'Disco detectado' apunta a una carpeta o raiz donde existan esos archivos.",
            )
            return

        self._run_copy_plan(copy_plan)

    def _build_copy_plan(self, selected_nodes: list[TreeNode], destination_root: str) -> list[tuple[str, str, int]]:
        plan: list[tuple[str, str, int]] = []
        seen_destinations: set[str] = set()
        for node in selected_nodes:
            for source_path, output_path in self._collect_recoveries(node, destination_root):
                if output_path in seen_destinations:
                    continue
                seen_destinations.add(output_path)
                try:
                    size = os.path.getsize(source_path)
                except OSError:
                    size = 0
                plan.append((source_path, output_path, size))
        return plan

    def _build_ntfs_recovery_plan(
        self,
        selected_nodes: list[TreeNode],
        destination_root: str,
    ) -> list[tuple[dict, str, int]]:
        plan: list[tuple[dict, str, int]] = []
        seen_paths: set[str] = set()
        for node in selected_nodes:
            for entry, output_path in self._collect_ntfs_recoveries(node, destination_root):
                if output_path in seen_paths:
                    continue
                seen_paths.add(output_path)
                size = 0 if entry.get("is_dir") else int(entry.get("size", 0) or 0)
                plan.append((entry, output_path, size))
        return plan

    def _collect_checked_nodes(self) -> list[TreeNode]:
        selected: list[TreeNode] = []
        seen: set[str] = set()

        def walk(node: TreeNode) -> None:
            if node.path in self.checked_paths and node.path not in seen:
                selected.append(node)
                seen.add(node.path)
            for child in node.children.values():
                walk(child)

        walk(self.tree_model)
        return selected

    def _collect_recoveries(self, node: TreeNode, destination_root: str) -> list[tuple[str, str]]:
        recoveries: list[tuple[str, str]] = []
        if node.is_file:
            source_path = self._existing_source_path_for_node(node)
            if source_path:
                relative_path = self._relative_node_path(node.path) or os.path.basename(source_path)
                recoveries.append((source_path, os.path.join(destination_root, relative_path)))
            return recoveries

        for child in node.children.values():
            recoveries.extend(self._collect_recoveries(child, destination_root))
        return recoveries

    def _collect_ntfs_recoveries(self, node: TreeNode, destination_root: str) -> list[tuple[dict, str]]:
        recoveries: list[tuple[dict, str]] = []
        entry = self.ntfs_recovery_entries.get(node.path)
        if entry is not None:
            relative_path = (
                node.path.removeprefix("/NTFS reconstruido/Archivos y carpetas/")
                .removeprefix("/NTFS reconstruido/Borrados detectados/")
            )
            if relative_path:
                recoveries.append((entry, os.path.join(destination_root, relative_path)))
        for child in node.children.values():
            recoveries.extend(self._collect_ntfs_recoveries(child, destination_root))
        return recoveries

    def _existing_source_path_for_node(self, node: TreeNode) -> str:
        candidates: list[str] = []
        if os.path.isabs(node.path) and os.path.exists(node.path):
            candidates.append(node.path)

        source_root = self.source_edit.text().strip()
        relative_path = self._relative_node_path(node.path)
        if source_root and relative_path:
            candidates.append(os.path.join(source_root, relative_path))

        if self.current_file_path:
            base_dir = os.path.dirname(self.current_file_path)
            if relative_path:
                candidates.append(os.path.join(base_dir, relative_path))
            candidates.append(os.path.join(base_dir, node.name))

        for candidate in candidates:
            normalized = os.path.normpath(candidate)
            if os.path.isfile(normalized):
                return normalized
        return ""

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

    def _run_copy_plan(self, copy_plan: list[tuple[str, str, int]]) -> None:
        self.recovery_dialog = RecoveryProgressDialog(self)
        self.recovery_dialog.show()
        QApplication.processEvents()

        total_files = len(copy_plan)
        total_bytes = sum(size for _src, _dst, size in copy_plan)
        copied_bytes = 0
        copied_files = 0
        errors: list[str] = []
        started_at = time.time()

        for source_path, output_path, size in copy_plan:
            if self.recovery_dialog.cancel_requested:
                break
            try:
                os.makedirs(os.path.dirname(output_path), exist_ok=True)
                with open(source_path, "rb") as in_handle, open(output_path, "wb") as out_handle:
                    while True:
                        if self.recovery_dialog.cancel_requested:
                            break
                        chunk = in_handle.read(1024 * 1024)
                        if not chunk:
                            break
                        out_handle.write(chunk)
                        copied_bytes += len(chunk)
                        self._update_recovery_progress(
                            copied_bytes,
                            total_bytes,
                            copied_files,
                            total_files,
                            os.path.basename(output_path),
                            started_at,
                        )
                        QApplication.processEvents()
                if self.recovery_dialog.cancel_requested:
                    break
                shutil.copystat(source_path, output_path, follow_symlinks=True)
                copied_files += 1
                self._update_recovery_progress(
                    copied_bytes,
                    total_bytes,
                    copied_files,
                    total_files,
                    os.path.basename(output_path),
                    started_at,
                )
                QApplication.processEvents()
            except OSError as exc:
                errors.append(f"{source_path}: {exc}")

        cancelled = bool(self.recovery_dialog.cancel_requested)
        self.recovery_dialog.close()
        self.recovery_dialog = None

        summary = f"Archivos copiados: {copied_files}/{total_files} | Datos: {self._format_size(copied_bytes)}"
        if cancelled:
            QMessageBox.warning(self, "Recuperacion cancelada", summary)
        elif errors:
            QMessageBox.warning(
                self,
                "Recuperacion con errores",
                summary + f"\nErrores: {len(errors)}\n\n" + "\n".join(errors[:15]),
            )
        else:
            QMessageBox.information(self, "Recuperacion completada", summary)
        self.statusBar().showMessage(summary)

    def _run_ntfs_recovery_plan(
        self,
        recovery_plan: list[tuple[dict, str, int]],
        device_path: str,
    ) -> None:
        self.recovery_dialog = RecoveryProgressDialog(self)
        self.recovery_dialog.show()
        QApplication.processEvents()

        total_items = len(recovery_plan)
        total_bytes = sum(size for _entry, _path, size in recovery_plan)
        copied_bytes = 0
        copied_items = 0
        errors: list[str] = []
        started_at = time.time()
        device_handles: dict[str, object] = {}

        try:
            for entry, output_path, _size in recovery_plan:
                if self.recovery_dialog.cancel_requested:
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
                        copied_items += 1
                        self._update_recovery_progress(
                            copied_bytes,
                            total_bytes,
                            copied_items,
                            total_items,
                            os.path.basename(output_path) or output_path,
                            started_at,
                        )
                        QApplication.processEvents()
                        continue

                    resident_data = entry.get("resident_data")
                    if resident_data is not None:
                        with open(output_path, "wb") as out_handle:
                            out_handle.write(resident_data)
                        copied_bytes += len(resident_data)
                        self._normalize_output_permissions(output_path)
                        copied_items += 1
                        self._update_recovery_progress(
                            copied_bytes,
                            total_bytes,
                            copied_items,
                            total_items,
                            os.path.basename(output_path),
                            started_at,
                        )
                        QApplication.processEvents()
                        continue

                    runlist = entry.get("data_runlist") or []
                    remaining = int(entry.get("size", 0) or 0)
                    cluster_size = int(entry.get("cluster_size", 0) or 0)
                    if not runlist:
                        errors.append(f"{output_path}: sin datos legibles")
                        continue
                    if remaining <= 0 or cluster_size <= 0:
                        errors.append(f"{output_path}: tamano o cluster invalidos")
                        continue

                    with open(output_path, "wb") as out_handle:
                        for lcn, run_len in runlist:
                            if self.recovery_dialog.cancel_requested:
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
                                copied_items,
                                total_items,
                                os.path.basename(output_path),
                                started_at,
                            )
                            QApplication.processEvents()

                    if self.recovery_dialog.cancel_requested:
                        break
                    self._normalize_output_permissions(output_path)
                    copied_items += 1
                    self._update_recovery_progress(
                        copied_bytes,
                        total_bytes,
                        copied_items,
                        total_items,
                        os.path.basename(output_path),
                        started_at,
                    )
                    QApplication.processEvents()
                except OSError as exc:
                    errors.append(f"{output_path}: {exc}")
        except OSError as exc:
            if self.recovery_dialog is not None:
                self.recovery_dialog.close()
                self.recovery_dialog = None
            QMessageBox.critical(
                self,
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

        cancelled = bool(self.recovery_dialog.cancel_requested)
        self.recovery_dialog.close()
        self.recovery_dialog = None

        summary = f"Extraidos {copied_items}/{total_items} elemento(s) | Datos: {self._format_size(copied_bytes)}"
        if cancelled:
            QMessageBox.warning(self, "Extraccion cancelada", summary)
        elif errors:
            QMessageBox.warning(
                self,
                "Extraccion con errores",
                summary + f"\nErrores: {len(errors)}\n\n" + "\n".join(errors[:15]),
            )
        else:
            QMessageBox.information(self, "Extraccion completada", summary)
        self.statusBar().showMessage(summary)

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
            try:
                os.chown(target_path, uid, gid)
                os.chmod(target_path, 0o755)
            except OSError:
                pass
            return

        try:
            os.chown(target_path, uid, gid)
            os.chmod(target_path, 0o644)
        except OSError:
            pass

    def _update_recovery_progress(
        self,
        copied_bytes: int,
        total_bytes: int,
        copied_files: int,
        total_files: int,
        current_name: str,
        started_at: float,
    ) -> None:
        if self.recovery_dialog is None:
            return
        percent = 0.0 if total_bytes <= 0 else min(100.0, (copied_bytes / total_bytes) * 100.0)
        self.recovery_dialog.progress_bar.setValue(int(percent * 10))
        self.recovery_dialog.status_label.setText(
            f"Recuperando {copied_files}/{total_files}: {current_name}"
        )
        self.recovery_dialog.bytes_label.setText(
            f"{self._format_size(copied_bytes)} / {self._format_size(total_bytes)}"
        )
        elapsed = max(0.001, time.time() - started_at)
        if copied_bytes > 0 and total_bytes > copied_bytes:
            rate = copied_bytes / elapsed
            remaining_seconds = int((total_bytes - copied_bytes) / max(rate, 1.0))
            remaining_text = self._format_duration(remaining_seconds)
        else:
            remaining_text = "calculando..."
        self.recovery_dialog.time_label.setText(
            f"Transcurrido: {self._format_duration(int(elapsed))} | Queda aprox.: {remaining_text}"
        )

    def _format_duration(self, seconds: int) -> str:
        if seconds < 60:
            return f"{seconds}s"
        minutes, secs = divmod(seconds, 60)
        if minutes < 60:
            return f"{minutes}m {secs}s"
        hours, minutes = divmod(minutes, 60)
        return f"{hours}h {minutes}m"

    def _node_supports_image_preview(self, node: TreeNode) -> bool:
        return os.path.splitext(node.name)[1].lower() in IMAGE_EXTENSIONS

    def _node_supports_video_preview(self, node: TreeNode) -> bool:
        return os.path.splitext(node.name)[1].lower() in VIDEO_EXTENSIONS

    def _show_image_preview(self, file_path: str) -> None:
        pixmap = QPixmap(file_path)
        if pixmap.isNull():
            self.preview_label.setPixmap(QPixmap())
            self.preview_label.setText("No se pudo cargar la imagen.")
            return
        scaled = pixmap.scaled(
            self.preview_label.size(),
            Qt.KeepAspectRatio,
            Qt.SmoothTransformation,
        )
        self.preview_label.setPixmap(scaled)
        self.preview_label.setText("")

    def resizeEvent(self, event: object) -> None:
        super().resizeEvent(event)
        selection = self.tree.selectedItems()
        if selection:
            self._update_details()

    def _handle_item_changed(self, item: QTreeWidgetItem, column: int) -> None:
        if self._updating_checks or column != 0:
            return
        node = item.data(0, Qt.UserRole)
        if not isinstance(node, TreeNode):
            return

        checked = item.checkState(0) == Qt.Checked
        self._updating_checks = True
        if checked:
            self.checked_paths.add(node.path)
        else:
            self.checked_paths.discard(node.path)
        self._update_parent_states(item.parent())
        self._updating_checks = False
        self._update_selection_label()

    def _set_checked_recursive(self, item: QTreeWidgetItem, checked: bool) -> None:
        node = item.data(0, Qt.UserRole)
        if isinstance(node, TreeNode):
            if checked:
                self.checked_paths.add(node.path)
            else:
                self.checked_paths.discard(node.path)
        item.setCheckState(0, Qt.Checked if checked else Qt.Unchecked)
        for index in range(item.childCount()):
            self._set_checked_recursive(item.child(index), checked)

    def _update_parent_states(self, item: QTreeWidgetItem | None) -> None:
        while item is not None:
            child_states = {item.child(index).checkState(0) for index in range(item.childCount())}
            if child_states == {Qt.Checked}:
                item.setCheckState(0, Qt.Checked)
            elif child_states == {Qt.Unchecked}:
                item.setCheckState(0, Qt.Unchecked)
            else:
                item.setCheckState(0, Qt.PartiallyChecked)
            node = item.data(0, Qt.UserRole)
            if isinstance(node, TreeNode):
                if item.checkState(0) == Qt.Checked:
                    self.checked_paths.add(node.path)
                else:
                    self.checked_paths.discard(node.path)
            item = item.parent()

    def _update_selection_label(self) -> None:
        self.selection_label.setText(f"{len(self.checked_paths)} marcados")

    def _open_tree_context_menu(self, position: object) -> None:
        item = self.tree.itemAt(position)
        if item is None:
            return
        menu = QMenu(self.tree)
        expand_action = menu.addAction("Expandir rama")
        collapse_action = menu.addAction("Contraer rama")
        menu.addSeparator()
        mark_action = menu.addAction("Marcar rama")
        unmark_action = menu.addAction("Desmarcar rama")
        selected = menu.exec(self.tree.viewport().mapToGlobal(position))
        if selected == expand_action:
            self.tree.expandItem(item)
        elif selected == collapse_action:
            self.tree.collapseItem(item)
        elif selected == mark_action:
            self._updating_checks = True
            self._set_checked_recursive(item, True)
            self._update_parent_states(item.parent())
            self._updating_checks = False
            self._update_selection_label()
        elif selected == unmark_action:
            self._updating_checks = True
            self._set_checked_recursive(item, False)
            self._update_parent_states(item.parent())
            self._updating_checks = False
            self._update_selection_label()

    def _icon_for_node(self, node: TreeNode) -> QIcon:
        key = "folder"
        if node.is_file:
            category = self._category_for_node(node)
            if category == "Imagenes":
                key = "image"
            elif category == "Videos":
                key = "video"
            elif category == "Documentos":
                key = "document"
            elif category == "Comprimidos":
                key = "archive"
            else:
                key = "file"
        if key not in self.icon_cache:
            self.icon_cache[key] = self._build_icon(key)
        return self.icon_cache[key]

    def _build_icon(self, kind: str) -> QIcon:
        pixmap = QPixmap(14, 14)
        pixmap.fill(Qt.transparent)
        color_map = {
            "folder": "#2f8f57",
            "image": "#4a8cab",
            "video": "#a78f42",
            "document": "#7d6b9c",
            "archive": "#a56d39",
            "file": "#8d959d",
        }
        color = QColor(color_map.get(kind, "#8d959d"))
        painter = QPainter(pixmap)
        painter.setRenderHint(QPainter.Antialiasing, True)
        painter.setPen(Qt.NoPen)
        painter.setBrush(color)
        painter.drawRoundedRect(1, 1, 12, 12, 3, 3)
        painter.end()
        return QIcon(pixmap)

    def _deserialize_node(self, payload: dict) -> TreeNode:
        node = TreeNode(
            name=str(payload.get("name", "")),
            path=str(payload.get("path", "")),
            is_file=bool(payload.get("is_file", False)),
            size_bytes=int(payload.get("size_bytes", 0) or 0),
            raw_lines=list(payload.get("raw_lines", [])),
            line_numbers=list(payload.get("line_numbers", [])),
        )
        for child_payload in payload.get("children", []):
            child = self._deserialize_node(child_payload)
            node.children[child.name] = child
        return node


def main() -> int:
    if QApplication is None:
        print("PySide6 no esta instalado. Instala PySide6 para usar la interfaz Qt.")
        return 1
    app = QApplication(sys.argv)
    window = QtRecoveryMainWindow()
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
