#!/usr/bin/env python3
"""
Exporta el mapa de GTA 3 a glTF leyendo IPL/IDE y modelos DFF desde gta3.img.

Esta version prioriza:
- colocacion correcta de instancias mediante IPL
- geometria de mallas estaticas DFF
- salida estandar glTF 2.0 (.gltf + .bin)

Limitaciones conocidas:
- no extrae texturas TXD todavia
- no soporta geometrias "native" de RenderWare si aparecieran
"""

from __future__ import annotations

import argparse
import io
import json
import math
import mimetypes
import re
import struct
import shutil
import zlib
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Tuple

import numpy as np
from PIL import Image
from PIL import ImageFile


SECTOR_SIZE = 2048
SUPPORTED_GAMES = ("gta3", "gta4")
GAME_EXPORT_PRESETS = {
    "gta3": {
        "label": "GTA 3",
        "default_dat": "data/gta3.dat",
        "map_prefix": "gta3_map",
        "img_rel": "models/gta3.img",
        "dir_rel": "models/gta3.dir",
        "default_ide": "data/default.ide",
    },
    "gta4": {
        "label": "GTA 4",
        "default_dat": "",
        "map_prefix": "gta4_map",
        "img_rel": "",
        "dir_rel": "",
        "default_ide": "",
    },
}

CHUNK_STRUCT = 0x0001
CHUNK_STRING = 0x0002
CHUNK_EXTENSION = 0x0003
CHUNK_TEXTURE = 0x0006
CHUNK_MATERIAL = 0x0007
CHUNK_MATERIAL_LIST = 0x0008
CHUNK_FRAME_LIST = 0x000E
CHUNK_GEOMETRY = 0x000F
CHUNK_CLUMP = 0x0010
CHUNK_ATOMIC = 0x0014
CHUNK_GEOMETRY_LIST = 0x001A

GEOM_FLAG_TRISTRIP = 0x00000001
GEOM_FLAG_POSITIONS = 0x00000002
GEOM_FLAG_TEXTURED = 0x00000004
GEOM_FLAG_PRELIT = 0x00000008
GEOM_FLAG_NORMALS = 0x00000010
GEOM_FLAG_TEXTURED2 = 0x00000080
GEOM_FLAG_NATIVE = 0x01000000

BASIS_GTA_TO_GLTF = np.array(
    [
        [-1.0, 0.0, 0.0],
        [0.0, 0.0, 1.0],
        [0.0, -1.0, 0.0],
    ],
    dtype=np.float32,
)
BASIS_GLTF_TO_GTA = BASIS_GTA_TO_GLTF.T
EXPORTABLE_CATEGORIES = ("vehicles", "peds", "weapons", "props")
MODEL_NAME_ALIASES = {
    "null": "player",
}
WEAPON_MODEL_NAMES = {
    "grenade", "ak47", "bat", "colt45", "molotov", "rocket", "shotgun",
    "sniper", "uzi", "missile", "m16", "flame", "bomb", "fingers",
}
SHARED_CATEGORY_MODELS = {
    "weapons": [("weapons_bundle", "models/Generic/weapons.dff", "generic")],
    "props": [("wheels_bundle", "models/Generic/wheels.DFF", "generic")],
}


@dataclass
class DirEntry:
    offset_sectors: int
    size_sectors: int
    name: str


@dataclass
class ObjectDef:
    object_id: int
    model_name: str
    txd_name: str
    draw_distance: float
    flags: int


@dataclass
class Placement:
    object_id: int
    model_name: str
    position: Tuple[float, float, float]
    scale: Tuple[float, float, float]
    quaternion_xyzw: Tuple[float, float, float, float]
    source_ipl: str


@dataclass
class Gta4AssetDef:
    category: str
    model_name: str
    texture_name: str
    source_file: str
    section: str
    extra: Dict[str, object]


@dataclass
class Gta4Placement:
    model_name: str
    position: Tuple[float, float, float]
    quaternion_xyzw: Tuple[float, float, float, float]
    source_file: str
    source_kind: str
    extra: Dict[str, object]


@dataclass
class Gta4RscResource:
    source_path: str
    source_kind: str
    source_archive: str
    resource_index: int
    offset: int
    packed_size: int
    unpacked_size: int
    compression: str
    resource_type: str
    guessed_name: str
    name_candidates: List[str]
    strings: List[str]
    data: bytes


@dataclass
class Gta4TextureEntry:
    name: str
    format_name: str
    width: int
    height: int
    data_offset: int
    data_size: int
    dds_bytes: bytes


@dataclass
class Gta4GraphicsRef:
    descriptor_offset: int
    data_offset: int
    count_hint: int
    stride_hint: int
    label: str
    classification: str
    preview_u16: List[int]
    preview_f32: List[float]


@dataclass
class Gta4MeshCandidate:
    vertex_ref_offset: int
    index_ref_offset: int
    vertex_data_offset: int
    index_data_offset: int
    vertex_count_hint: int
    index_count_hint: int
    vertex_stride_hint: int
    score: int


@dataclass
class MaterialInfo:
    name: str
    rgba: Tuple[int, int, int, int]


@dataclass
class GeometrySubset:
    indices: np.ndarray
    material_name: str


@dataclass
class GeometryChunk:
    positions: np.ndarray
    normals: Optional[np.ndarray]
    uvs: Optional[np.ndarray]
    subsets: List[GeometrySubset]


@dataclass
class AtomicRef:
    frame_index: int
    geometry_index: int
    flags: int


@dataclass
class DffModel:
    name: str
    positions: np.ndarray
    normals: Optional[np.ndarray]
    uvs: Optional[np.ndarray]
    subsets: List[GeometrySubset]
    texture_paths: Dict[str, Path]


@dataclass
class ExportArtifacts:
    gltf: Dict[str, object]
    binary_blob: bytes


class RWParseError(RuntimeError):
    pass


def read_u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def read_i32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<i", data, offset)[0]


def read_f32(data: bytes, offset: int) -> float:
    return struct.unpack_from("<f", data, offset)[0]


def read_chunk_header(data: bytes, offset: int) -> Tuple[int, int, int, int]:
    if offset + 12 > len(data):
        raise RWParseError(f"Cabecera RenderWare fuera de rango en 0x{offset:x}")
    chunk_id, size, version = struct.unpack_from("<III", data, offset)
    return chunk_id, size, version, offset + 12


def next_sibling_offset(payload_offset: int, size: int) -> int:
    return payload_offset + size


def normalize_rel_path(path: str) -> Sequence[str]:
    clean = path.replace("\\", "/").strip().strip("/")
    return [part for part in clean.split("/") if part]


def resolve_case_path(root: Path, rel_path: str) -> Path:
    current = root
    for part in normalize_rel_path(rel_path):
        if not current.is_dir():
            raise FileNotFoundError(f"No existe el directorio intermedio: {current}")
        exact = current / part
        if exact.exists():
            current = exact
            continue
        lowered = part.lower()
        matches = [child for child in current.iterdir() if child.name.lower() == lowered]
        if not matches:
            raise FileNotFoundError(f"No se encontro '{part}' dentro de {current}")
        current = matches[0]
    return current


def iter_clean_lines(path: Path) -> Iterable[str]:
    with path.open("r", encoding="latin-1", errors="ignore") as handle:
        for raw in handle:
            line = raw.split("#", 1)[0].strip()
            if line:
                yield line


def parse_dat_references(game_root: Path, dat_rel_path: str) -> Tuple[List[Path], List[Path]]:
    dat_path = resolve_case_path(game_root, dat_rel_path)
    ide_paths: List[Path] = []
    ipl_paths: List[Path] = []
    for line in iter_clean_lines(dat_path):
        parts = line.split(None, 1)
        if len(parts) != 2:
            continue
        directive, rel_path = parts[0].upper(), parts[1].strip()
        if directive == "IDE":
            ide_paths.append(resolve_case_path(game_root, rel_path))
        elif directive == "IPL":
            ipl_paths.append(resolve_case_path(game_root, rel_path))
    try:
        default_ide = resolve_case_path(game_root, "data/default.ide")
    except FileNotFoundError:
        default_ide = None
    if default_ide is not None and all(path != default_ide for path in ide_paths):
        ide_paths.insert(0, default_ide)
    return ide_paths, ipl_paths


def ensure_supported_game(game: str) -> None:
    if game not in SUPPORTED_GAMES:
        raise SystemExit(f"Juego no soportado: {game}")


def resolve_gta4_virtual_path(game_root: Path, virtual_path: str) -> Path:
    cleaned = virtual_path.strip()
    lowered = cleaned.lower()
    mappings = {
        "common:/": "common/",
        "platform:/": "pc/",
        "platformimg:/": "pc/",
    }
    for prefix, rel_prefix in mappings.items():
        if lowered.startswith(prefix):
            rel_path = rel_prefix + cleaned[len(prefix):]
            try:
                return resolve_case_path(game_root, rel_path)
            except FileNotFoundError:
                path_obj = Path(rel_path)
                if path_obj.suffix.lower() == ".ipl":
                    wpl_rel = str(path_obj.with_suffix(".wpl")).replace("\\", "/")
                    return resolve_case_path(game_root, wpl_rel)
                if path_obj.suffix == "":
                    img_rel = f"{rel_path}.img"
                    try:
                        return resolve_case_path(game_root, img_rel)
                    except FileNotFoundError:
                        pass
                raise
    return resolve_case_path(game_root, cleaned)


def parse_gta4_dat_references(game_root: Path, dat_rel_path: str) -> Dict[str, List[Path]]:
    dat_path = resolve_case_path(game_root, dat_rel_path)
    refs: Dict[str, List[Path]] = {
        "dat": [dat_path],
        "ide": [],
        "ipl": [],
        "wpl": [],
        "imglist": [],
    }
    for line in iter_clean_lines(dat_path):
        parts = line.split(None, 1)
        if len(parts) != 2:
            continue
        directive = parts[0].upper()
        virtual_path = parts[1].strip()
        try:
            resolved = resolve_gta4_virtual_path(game_root, virtual_path)
        except FileNotFoundError:
            continue
        if directive == "IDE":
            refs["ide"].append(resolved)
        elif directive == "IPL":
            suffix = resolved.suffix.lower()
            if suffix == ".wpl":
                refs["wpl"].append(resolved)
            else:
                refs["ipl"].append(resolved)
        elif directive == "IMGLIST":
            refs["imglist"].append(resolved)
    for fallback_rel in ("common/data/default.ide", "common/data/vehicles.ide", "common/data/peds.ide"):
        try:
            fallback_path = resolve_case_path(game_root, fallback_rel)
        except FileNotFoundError:
            continue
        if fallback_path not in refs["ide"]:
            refs["ide"].insert(0, fallback_path)
    return refs


def parse_gta4_images_list(game_root: Path, image_list_path: Path) -> List[Dict[str, object]]:
    archives: List[Dict[str, object]] = []
    for line in iter_clean_lines(image_list_path):
        parts = line.split()
        if not parts:
            continue
        virtual_path = parts[0]
        extra = parts[1:]
        try:
            resolved = resolve_gta4_virtual_path(game_root, virtual_path)
        except FileNotFoundError:
            resolved = None
        archive_name = Path(virtual_path.replace("\\", "/")).name
        archive_path = resolved if resolved is not None else Path(virtual_path)
        archives.append(
            {
                "virtual_path": virtual_path,
                "resolved_path": str(archive_path) if resolved is not None else "",
                "exists": bool(resolved is not None and resolved.exists()),
                "archive_name": archive_name,
                "kind": archive_name.rsplit(".", 1)[-1].lower() if "." in archive_name else archive_name.lower(),
                "extra_tokens": extra,
            }
        )
    return archives


def parse_gta4_ide_file(path: Path) -> Dict[str, List[Gta4AssetDef]]:
    categories: Dict[str, List[Gta4AssetDef]] = {
        "map": [],
        "vehicles": [],
        "peds": [],
        "weapons": [],
        "props": [],
    }
    section: Optional[str] = None

    for line in iter_clean_lines(path):
        lower = line.lower()
        if lower == "end":
            section = None
            continue
        if lower in {"objs", "tobj", "cars", "peds", "weap"}:
            section = lower
            continue
        if section is None:
            continue

        fields = [field.strip() for field in line.split(",")]
        if not fields:
            continue

        if section in {"objs", "tobj"} and len(fields) >= 2:
            model_name = fields[0]
            texture_name = fields[1]
            extra: Dict[str, object] = {
                "raw_fields": fields,
            }
            if len(fields) >= 3:
                try:
                    extra["draw_distance"] = float(fields[2])
                except ValueError:
                    pass
            if len(fields) >= 4:
                try:
                    extra["flags"] = int(fields[3])
                except ValueError:
                    pass
            target_category = "props"
            if "/maps/" in path.as_posix().lower():
                target_category = "map"
            categories[target_category].append(
                Gta4AssetDef(
                    category=target_category,
                    model_name=model_name,
                    texture_name=texture_name,
                    source_file=str(path),
                    section=section,
                    extra=extra,
                )
            )
            continue

        if section == "cars" and len(fields) >= 2:
            categories["vehicles"].append(
                Gta4AssetDef(
                    category="vehicles",
                    model_name=fields[0],
                    texture_name=fields[1],
                    source_file=str(path),
                    section=section,
                    extra={"raw_fields": fields},
                )
            )
            continue

        if section == "peds" and len(fields) >= 2:
            texture_name = fields[1] if len(fields) >= 2 else fields[0]
            categories["peds"].append(
                Gta4AssetDef(
                    category="peds",
                    model_name=fields[0],
                    texture_name=texture_name,
                    source_file=str(path),
                    section=section,
                    extra={"raw_fields": fields},
                )
            )
            continue

        if section == "weap" and len(fields) >= 2:
            texture_name = fields[1]
            categories["weapons"].append(
                Gta4AssetDef(
                    category="weapons",
                    model_name=fields[0],
                    texture_name=texture_name,
                    source_file=str(path),
                    section=section,
                    extra={"raw_fields": fields},
                )
            )
            continue

    return categories


def gta4_joaat(text: str) -> int:
    value = 0
    for char in text.lower().encode("utf-8"):
        value = (value + char) & 0xFFFFFFFF
        value = (value + ((value << 10) & 0xFFFFFFFF)) & 0xFFFFFFFF
        value ^= value >> 6
    value = (value + ((value << 3) & 0xFFFFFFFF)) & 0xFFFFFFFF
    value ^= value >> 11
    value = (value + ((value << 15) & 0xFFFFFFFF)) & 0xFFFFFFFF
    return value


def build_gta4_model_hash_lookup(ide_paths: Sequence[Path]) -> Dict[int, List[str]]:
    lookup: Dict[int, List[str]] = {}
    for ide_path in ide_paths:
        section: Optional[str] = None
        for line in iter_clean_lines(ide_path):
            lower = line.lower()
            if lower == "end":
                section = None
                continue
            if lower in {"objs", "tobj", "cars", "peds", "weap"}:
                section = lower
                continue
            if section is None:
                continue
            fields = [field.strip() for field in line.split(",")]
            if not fields or not fields[0]:
                continue
            model_name = fields[0]
            hash_value = gta4_joaat(model_name)
            names = lookup.setdefault(hash_value, [])
            if model_name not in names:
                names.append(model_name)
    return lookup


def parse_gta4_text_ipl_file(path: Path, include_lod: bool) -> List[Gta4Placement]:
    placements: List[Gta4Placement] = []
    section: Optional[str] = None
    for line in iter_clean_lines(path):
        lower = line.lower()
        if lower == "end":
            section = None
            continue
        if lower in {"inst", "2dfx"}:
            section = lower
            continue

        fields = [field.strip() for field in line.split(",")]
        if section == "inst" and len(fields) >= 10:
            model_name = fields[1]
            lowered_model = model_name.lower()
            if not include_lod and lowered_model.startswith("lod"):
                continue
            try:
                x, y, z = map(float, fields[3:6])
                qx, qy, qz, qw = map(float, fields[6:10])
            except ValueError:
                continue
            placements.append(
                Gta4Placement(
                    model_name=model_name,
                    position=(x, y, z),
                    quaternion_xyzw=(qx, qy, qz, qw),
                    source_file=str(path),
                    source_kind="ipl_inst",
                    extra={"raw_fields": fields},
                )
            )
            continue

        if section == "2dfx" and len(fields) >= 9:
            try:
                x, y, z = map(float, fields[1:4])
                qx, qy, qz, qw = map(float, fields[5:9])
            except ValueError:
                continue
            model_name = fields[9] if len(fields) >= 10 else fields[0]
            placements.append(
                Gta4Placement(
                    model_name=model_name,
                    position=(x, y, z),
                    quaternion_xyzw=(qx, qy, qz, qw),
                    source_file=str(path),
                    source_kind="ipl_2dfx",
                    extra={"raw_fields": fields},
                )
            )
    return placements


def parse_gta4_wpl_header(path: Path) -> Dict[str, object]:
    data = path.read_bytes()
    if len(data) < 24:
        return {
            "path": str(path),
            "size_bytes": len(data),
            "valid_header": False,
        }
    version, entry_count, _unknown0, header_a, _unknown1, record_size = struct.unpack_from("<6I", data, 0)
    estimated_header_size = len(data) - (entry_count * record_size)
    if estimated_header_size < 0 or estimated_header_size > 1024:
        estimated_header_size = None
    return {
        "path": str(path),
        "size_bytes": len(data),
        "valid_header": True,
        "version": version,
        "entry_count": entry_count,
        "header_value": header_a,
        "record_size": record_size,
        "estimated_header_size": estimated_header_size,
    }


def parse_gta4_wpl_records(path: Path, model_hash_lookup: Optional[Dict[int, List[str]]] = None,
                           include_lod: bool = False) -> Dict[str, object]:
    data = path.read_bytes()
    if len(data) < 0x40:
        return {
            "path": str(path),
            "valid_header": False,
            "records": [],
        }
    version = read_u32(data, 0)
    entry_count = read_u32(data, 4)
    header_values = list(struct.unpack_from("<16I", data, 0))
    records: List[Dict[str, object]] = []
    cursor = 0x40
    max_records_by_size = max(0, (len(data) - cursor) // 48)
    parse_count = min(entry_count, max_records_by_size)
    for record_index in range(parse_count):
        record_offset = cursor + record_index * 48
        area_id = read_u32(data, record_offset)
        pos_x = read_f32(data, record_offset + 4)
        pos_y = read_f32(data, record_offset + 8)
        pos_z = read_f32(data, record_offset + 12)
        quat_x = read_f32(data, record_offset + 16)
        quat_y = read_f32(data, record_offset + 20)
        quat_z = read_f32(data, record_offset + 24)
        quat_w = read_f32(data, record_offset + 28)
        archetype_hash = read_u32(data, record_offset + 32)
        resolved_names = list(model_hash_lookup.get(archetype_hash, [])) if model_hash_lookup else []
        primary_name = resolved_names[0] if resolved_names else ""
        lowered_primary = primary_name.lower()
        if not include_lod and lowered_primary.startswith(("lod", "slod")):
            continue
        value_a = read_u32(data, record_offset + 36)
        value_b = read_u32(data, record_offset + 40)
        value_c = read_u32(data, record_offset + 44)
        records.append(
            {
                "record_index": record_index,
                "offset": record_offset,
                "area_id": area_id,
                "position": [pos_x, pos_y, pos_z],
                "quaternion_xyzw": [quat_x, quat_y, quat_z, quat_w],
                "archetype_hash": f"0x{archetype_hash:08x}",
                "model_name": primary_name,
                "model_name_candidates": resolved_names,
                "value_a": value_a,
                "value_b": value_b,
                "value_c": value_c,
            }
        )
    tail_offset = cursor + parse_count * 48
    tail_preview = data[tail_offset:tail_offset + 128]
    tail_strings = extract_gta4_strings(tail_preview, limit=8)
    return {
        "path": str(path),
        "valid_header": True,
        "version": version,
        "entry_count": entry_count,
        "parsed_records": parse_count,
        "header_values": header_values,
        "tail_offset": tail_offset,
        "tail_size": max(0, len(data) - tail_offset),
        "tail_strings": tail_strings,
        "records": records,
    }


def scan_rsc5_offsets(data: bytes) -> List[int]:
    offsets: List[int] = []
    start = 0
    while True:
        found = data.find(b"RSC\x05", start)
        if found < 0:
            return offsets
        offsets.append(found)
        start = found + 1


def extract_gta4_strings(data: bytes, limit: int = 24) -> List[str]:
    results: List[str] = []
    seen: set[str] = set()
    for match in re.finditer(rb"(?:pack:/[^\x00]{1,120}|[A-Za-z0-9_./:-]{4,120})", data):
        try:
            text = match.group(0).decode("latin-1", errors="ignore")
        except UnicodeDecodeError:
            continue
        if not any(ch.isalpha() for ch in text):
            continue
        if text in seen:
            continue
        seen.add(text)
        results.append(text)
        if len(results) >= limit:
            break
    return results


def infer_gta4_resource_name(strings: Sequence[str], fallback: str) -> str:
    for text in strings:
        lowered = text.lower()
        if lowered.startswith("pack:/"):
            stem = Path(lowered[6:]).stem
            if stem:
                return stem
    return fallback


def extract_gta4_name_candidates(strings: Sequence[str]) -> List[str]:
    candidates: List[str] = []
    seen: set[str] = set()
    for text in strings:
        values: List[Tuple[str, bool]] = [(text, False)]
        lowered = text.lower()
        if lowered.startswith("pack:/"):
            values.append((Path(text[6:]).stem, True))
        for value, from_pack in values:
            clean = value.strip().strip("\0")
            if not clean:
                continue
            if "." in clean and not clean.lower().startswith("pack:/"):
                clean = Path(clean).stem
            if len(clean) < 4:
                continue
            if from_pack:
                if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_./:-]*", clean):
                    continue
            else:
                # Keep only human-meaningful identifiers for model matching.
                if "_" not in clean:
                    continue
                if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", clean):
                    continue
            key = clean.lower()
            if key in seen:
                continue
            seen.add(key)
            candidates.append(clean)
    return candidates


def choose_best_gta4_candidate_name(candidates: Sequence[str]) -> str:
    def score(candidate: str) -> Tuple[int, int]:
        lowered = candidate.lower()
        value = 0
        if "_" in candidate:
            value += 12
        if any(ch.isdigit() for ch in candidate):
            value += 3
        if lowered.startswith(("lod", "slod", "l_")):
            value += 5
        if lowered.startswith("gta_"):
            value -= 20
        if lowered.endswith(".sps"):
            value -= 20
        if lowered.startswith(("cj_", "el", "bx", "nj", "mh", "bxe_", "hunts_")):
            value += 4
        return value, len(candidate)

    if not candidates:
        return ""
    return max(candidates, key=score)


def classify_gta4_resource(strings: Sequence[str]) -> str:
    lowered = " ".join(text.lower() for text in strings)
    if ".dds" in lowered or "dxt1" in lowered or "dxt5" in lowered:
        return "wtd_texture_pack"
    if ".anim" in lowered:
        return "animation_pack"
    if "pack:/" in lowered:
        return "model_or_pack"
    return "unknown"


def parse_gta4_rsc_bytes(blob: bytes, source_path: str, source_kind: str, source_archive: str,
                         resource_index: int, offset: int) -> Optional[Gta4RscResource]:
    if len(blob) < 16 or blob[:4] != b"RSC\x05":
        return None
    try:
        unpacked = zlib.decompress(blob[12:])
        compression = "zlib"
    except zlib.error:
        return None
    strings = extract_gta4_strings(unpacked)
    name_candidates = extract_gta4_name_candidates(strings)
    guessed_name = infer_gta4_resource_name(strings, f"res_{resource_index:05d}")
    resource_type = classify_gta4_resource(strings)
    best_candidate = choose_best_gta4_candidate_name(name_candidates)
    if resource_type != "wtd_texture_pack" and best_candidate:
        guessed_name = best_candidate
    elif guessed_name.startswith("res_") and best_candidate:
        guessed_name = best_candidate
    return Gta4RscResource(
        source_path=source_path,
        source_kind=source_kind,
        source_archive=source_archive,
        resource_index=resource_index,
        offset=offset,
        packed_size=len(blob),
        unpacked_size=len(unpacked),
        compression=compression,
        resource_type=resource_type,
        guessed_name=guessed_name,
        name_candidates=name_candidates,
        strings=strings,
        data=unpacked,
    )


def parse_gta4_img_resources(img_path: Path) -> List[Gta4RscResource]:
    data = img_path.read_bytes()
    offsets = scan_rsc5_offsets(data)
    resources: List[Gta4RscResource] = []
    for index, start in enumerate(offsets):
        end = offsets[index + 1] if index + 1 < len(offsets) else len(data)
        resource = parse_gta4_rsc_bytes(
            data[start:end],
            source_path=str(img_path),
            source_kind="img_rsc5",
            source_archive=img_path.name,
            resource_index=index,
            offset=start,
        )
        if resource is not None:
            resources.append(resource)
    return resources


def parse_gta4_img_resources_cached(img_path: Path, cache: Dict[str, List[Gta4RscResource]]) -> List[Gta4RscResource]:
    key = str(img_path)
    cached = cache.get(key)
    if cached is not None:
        return cached
    resources = parse_gta4_img_resources(img_path)
    cache[key] = resources
    return resources


def get_gta4_resource_geometry_info(resource: Gta4RscResource, cache: Dict[Tuple[str, int], Dict[str, int | bool]]) -> Dict[str, int | bool]:
    key = (resource.source_path, resource.resource_index)
    cached = cache.get(key)
    if cached is not None:
        return cached

    refs = parse_gta4_graphics_refs(resource)
    counts = Counter(ref.classification for ref in refs)
    mesh_candidates = build_gta4_mesh_candidates(refs)
    preview_candidate_count = 0
    preview_best_faces = 0
    for candidate in sorted(mesh_candidates, key=lambda item: item.score, reverse=True)[:8]:
        preview = try_build_gta4_mesh_preview(resource, candidate)
        if preview is None:
            continue
        preview_candidate_count += 1
        _, summary = preview
        verts, faces = parse_preview_summary(summary)
        preview_best_faces = max(preview_best_faces, faces)
    info: Dict[str, int | bool] = {
        "has_geometry": bool(mesh_candidates or (counts.get("vertex_like", 0) and counts.get("index_like", 0))),
        "mesh_candidate_count": len(mesh_candidates),
        "vertex_like_count": int(counts.get("vertex_like", 0)),
        "index_like_count": int(counts.get("index_like", 0)),
        "has_preview": bool(preview_candidate_count),
        "preview_candidate_count": preview_candidate_count,
        "preview_best_faces": preview_best_faces,
    }
    cache[key] = info
    return info


def build_gta4_resource_hash_lookup(resources: Sequence[Gta4RscResource]) -> Dict[int, List[Dict[str, object]]]:
    lookup: Dict[int, List[Dict[str, object]]] = {}
    for resource in resources:
        for candidate in resource.name_candidates:
            hash_value = gta4_joaat(candidate)
            entries = lookup.setdefault(hash_value, [])
            entries.append(
                {
                    "guessed_name": resource.guessed_name,
                    "candidate_name": candidate,
                    "resource_index": resource.resource_index,
                    "resource_type": resource.resource_type,
                    "source_archive": resource.source_archive,
                    "source_path": resource.source_path,
                    "match_kind": "name_hash",
                }
            )
    return lookup


def build_gta4_resource_name_lookup(resources: Sequence[Gta4RscResource]) -> Dict[str, List[Dict[str, object]]]:
    lookup: Dict[str, List[Dict[str, object]]] = {}
    for resource in resources:
        for candidate in {resource.guessed_name, *resource.name_candidates}:
            cleaned = str(candidate).strip()
            if not cleaned:
                continue
            key = cleaned.lower()
            lookup.setdefault(key, []).append(
                {
                    "guessed_name": resource.guessed_name,
                    "candidate_name": cleaned,
                    "resource_index": resource.resource_index,
                    "resource_type": resource.resource_type,
                    "source_archive": resource.source_archive,
                    "source_path": resource.source_path,
                    "match_kind": "exact_name",
                }
            )
    return lookup


def generate_gta4_nonlod_name_variants(name: str) -> List[str]:
    lowered = name.lower().strip()
    variants: List[str] = []
    seen: set[str] = set()

    def add(value: str) -> None:
        clean = value.strip("_- ")
        if not clean or clean == lowered or clean in seen:
            return
        seen.add(clean)
        variants.append(clean)

    if lowered.startswith("slod"):
        add(lowered[4:])
    if lowered.startswith("lod"):
        add(lowered[3:])
    add(lowered.replace("_lod", ""))
    add(lowered.replace("lod_", ""))
    add(lowered.replace("lod", ""))
    if lowered.endswith("lod"):
        add(lowered[:-3])
    return variants


def build_gta4_resource_archetype_lookup(resources: Sequence[Gta4RscResource], target_hashes: Sequence[int]) -> Tuple[Dict[int, List[Dict[str, object]]], Dict[Tuple[str, int], List[str]]]:
    hash_set = set(target_hashes)
    lookup: Dict[int, List[Dict[str, object]]] = {}
    resource_matches: Dict[Tuple[str, int], List[str]] = {}
    if not hash_set:
        return lookup, resource_matches

    for resource in resources:
        matched_hashes: List[str] = []
        data = resource.data
        for offset in range(0, len(data) - 4, 4):
            value = struct.unpack_from("<I", data, offset)[0]
            if value not in hash_set:
                continue
            text_hash = f"0x{value:08x}"
            matched_hashes.append(text_hash)
            lookup.setdefault(value, []).append(
                {
                    "guessed_name": resource.guessed_name,
                    "resource_index": resource.resource_index,
                    "resource_type": resource.resource_type,
                    "source_archive": resource.source_archive,
                    "source_path": resource.source_path,
                    "offset": offset,
                    "match_kind": "archetype_hash",
                }
            )
        if matched_hashes:
            unique_hashes = sorted(set(matched_hashes))
            resource_matches[(resource.source_path, resource.resource_index)] = unique_hashes
    return lookup, resource_matches


def guess_gta4_resource_role(resource_type: str, matched_hash_count: int, name_candidates: Sequence[str]) -> str:
    lowered_candidates = [candidate.lower() for candidate in name_candidates]
    if resource_type == "wtd_texture_pack":
        return "texture_bundle"
    if resource_type == "animation_pack":
        return "animation_bundle"
    if matched_hash_count >= 1:
        return "likely_model_bundle"
    if any(candidate.endswith(".sps") or candidate.startswith("gta_") for candidate in lowered_candidates):
        return "material_or_shader_bundle"
    return "unknown"


def is_gta4_lod_name(name: str) -> bool:
    lowered = name.lower()
    return lowered.startswith(("lod", "slod")) or "_lod" in lowered or lowered.endswith("lod")


def tokenize_gta4_name(name: str) -> set[str]:
    stop_tokens = {
        "lod",
        "slod",
        "nj",
        "nj5",
        "nj05",
        "gm",
        "mh",
        "bm",
        "bx",
        "el",
    }
    tokens: set[str] = set()
    for token in re.split(r"[^a-z0-9]+", name.lower()):
        if len(token) < 4 or token in stop_tokens or token.isdigit():
            continue
        tokens.add(token)
    return tokens


def score_gta4_resource_match(placement_name: str, match: Dict[str, object]) -> int:
    placement_lower = placement_name.lower()
    candidate_lower = str(match.get("candidate_name", "")).lower()
    guessed_lower = str(match.get("guessed_name", "")).lower()
    resource_type = str(match.get("resource_type", "")).lower()
    match_kind = str(match.get("match_kind", "name_hash")).lower()
    has_geometry = bool(match.get("has_geometry", False))
    mesh_candidate_count = int(match.get("mesh_candidate_count", 0) or 0)
    has_preview = bool(match.get("has_preview", False))
    preview_candidate_count = int(match.get("preview_candidate_count", 0) or 0)
    preview_best_faces = int(match.get("preview_best_faces", 0) or 0)

    score = 0
    if match_kind == "name_hash":
        score += 100
    elif match_kind == "exact_name":
        score += 120
    elif match_kind == "lod_alias":
        score += 135
    elif match_kind == "sibling_name":
        score += 95
    elif match_kind == "archetype_hash":
        score += 5

    if has_preview:
        score += 220 + min(preview_candidate_count, 6) * 20 + min(preview_best_faces, 400) // 10
    elif has_geometry:
        score += 90 + min(mesh_candidate_count, 6) * 10
    elif resource_type == "wtd_texture_pack":
        score -= 120
    elif resource_type == "unknown":
        score -= 10
    elif resource_type == "model_or_pack":
        score += 10

    if candidate_lower == placement_lower:
        score += 100
    elif guessed_lower == placement_lower:
        score += 80

    placement_tokens = tokenize_gta4_name(placement_lower)
    match_tokens = tokenize_gta4_name(candidate_lower) | tokenize_gta4_name(guessed_lower)
    overlap_count = 0
    if placement_tokens and match_tokens:
        overlap_count = len(placement_tokens & match_tokens)
        score += 20 * overlap_count

    placement_is_lod = is_gta4_lod_name(placement_lower)
    match_is_lod = is_gta4_lod_name(candidate_lower) or is_gta4_lod_name(guessed_lower)
    if placement_is_lod == match_is_lod:
        score += 15
    else:
        score -= 30

    if placement_is_lod and not match_is_lod and match_kind in {"lod_alias", "exact_name"}:
        score += 60

    if match_kind == "archetype_hash" and candidate_lower != placement_lower and guessed_lower != placement_lower:
        if overlap_count == 0:
            score -= 80

    if "texture" in guessed_lower or "diffuse" in guessed_lower:
        score -= 25

    if match_kind == "archetype_hash" and not has_geometry:
        score -= 120

    return score


def select_best_gta4_resource_matches(placement_name: str, matches: Sequence[Dict[str, object]], limit: int = 8) -> List[Dict[str, object]]:
    scored: List[Tuple[int, Dict[str, object]]] = []
    seen: set[Tuple[object, object, object]] = set()
    for match in matches:
        key = (
            str(match.get("source_path", "")),
            match.get("resource_index"),
            str(match.get("candidate_name", "")).lower(),
            str(match.get("match_kind", "name_hash")).lower(),
        )
        if key in seen:
            continue
        seen.add(key)
        score = score_gta4_resource_match(placement_name, match)
        enriched = dict(match)
        enriched["match_score"] = score
        scored.append((score, enriched))

    scored.sort(
        key=lambda item: (
            item[0],
            bool(item[1].get("has_preview", False)),
            bool(item[1].get("has_geometry", False)),
            str(item[1].get("match_kind", "")) == "name_hash",
            str(item[1].get("resource_type", "")) != "wtd_texture_pack",
        ),
        reverse=True,
    )
    return [match for score, match in scored[:limit] if score > 0]


def enrich_gta4_matches_with_geometry(matches: Sequence[Dict[str, object]],
                                      resource_by_key: Dict[Tuple[str, int], Gta4RscResource],
                                      geometry_cache: Dict[Tuple[str, int], Dict[str, int | bool]]) -> List[Dict[str, object]]:
    enriched_matches: List[Dict[str, object]] = []
    for match in matches:
        source_path = str(match.get("source_path", ""))
        try:
            resource_index = int(match.get("resource_index", -1))
        except (TypeError, ValueError):
            resource_index = -1
        resource = resource_by_key.get((source_path, resource_index))
        if resource is None:
            enriched_matches.append(dict(match))
            continue
        info = get_gta4_resource_geometry_info(resource, geometry_cache)
        enriched = dict(match)
        enriched.update(info)
        enriched_matches.append(enriched)
    return enriched_matches


def collect_gta4_lod_alias_matches(placement_name: str, name_lookup: Dict[str, List[Dict[str, object]]]) -> List[Dict[str, object]]:
    if not is_gta4_lod_name(placement_name):
        return []
    alias_matches: List[Dict[str, object]] = []
    for variant in generate_gta4_nonlod_name_variants(placement_name):
        for match in name_lookup.get(variant, []):
            enriched = dict(match)
            enriched["match_kind"] = "lod_alias"
            alias_matches.append(enriched)
    return alias_matches


def expand_gta4_sibling_name_matches(matches: Sequence[Dict[str, object]],
                                     name_lookup: Dict[str, List[Dict[str, object]]]) -> List[Dict[str, object]]:
    expanded: List[Dict[str, object]] = []
    for match in matches:
        expanded.append(match)
        guessed_name = str(match.get("guessed_name", "")).strip().lower()
        candidate_name = str(match.get("candidate_name", "")).strip().lower()
        sibling_keys = [key for key in (guessed_name, candidate_name) if key]
        for key in sibling_keys:
            for sibling in name_lookup.get(key, []):
                enriched = dict(sibling)
                enriched["match_kind"] = "sibling_name"
                expanded.append(enriched)
    return expanded


def classify_gta4_graphics_region(data: bytes, data_offset: int) -> Tuple[str, List[int], List[float]]:
    if data_offset < 0 or data_offset + 64 > len(data):
        return "out_of_range", [], []
    chunk = data[data_offset:data_offset + 64]
    preview_u16 = list(struct.unpack_from("<16H", chunk, 0))
    preview_f32_raw = struct.unpack_from("<8f", chunk, 0)
    preview_f32 = [float(value) for value in preview_f32_raw]

    small_u16 = sum(1 for value in preview_u16 if value < 4096)
    unique_u16 = len(set(preview_u16))
    finite_f32 = [value for value in preview_f32 if math.isfinite(value)]
    plausible_f32 = sum(1 for value in finite_f32 if abs(value) < 10000)

    if small_u16 >= 12 and unique_u16 >= 4:
        return "index_like", preview_u16, preview_f32
    if plausible_f32 >= 6:
        return "vertex_like", preview_u16, preview_f32
    return "unknown", preview_u16, preview_f32


def parse_gta4_graphics_refs(resource: Gta4RscResource) -> List[Gta4GraphicsRef]:
    refs: List[Gta4GraphicsRef] = []
    data = resource.data
    seen_offsets: set[int] = set()
    for descriptor_offset in range(0, max(0, len(data) - 32), 4):
        field0 = read_u32(data, descriptor_offset)
        field1 = read_u32(data, descriptor_offset + 4)
        field2 = read_u32(data, descriptor_offset + 8)
        field3 = read_u32(data, descriptor_offset + 12)
        if not (0x60000000 <= field2 < 0x70000000):
            continue
        data_offset = field2 & 0x0FFFFFFF
        if data_offset in seen_offsets:
            continue
        if data_offset >= len(data):
            continue
        if field3 not in {0, 0x24}:
            continue
        label = ""
        if 0x50000000 <= field0 < 0x60000000:
            text_offset = field0 & 0x0FFFFFFF
            if text_offset < len(data):
                label = data[text_offset:text_offset + 96].split(b"\0", 1)[0].decode("latin-1", errors="ignore")
        elif 0x50000000 <= read_u32(data, descriptor_offset + 16) < 0x60000000:
            text_offset = read_u32(data, descriptor_offset + 16) & 0x0FFFFFFF
            if text_offset < len(data):
                label = data[text_offset:text_offset + 96].split(b"\0", 1)[0].decode("latin-1", errors="ignore")
        classification, preview_u16, preview_f32 = classify_gta4_graphics_region(data, data_offset)
        refs.append(
            Gta4GraphicsRef(
                descriptor_offset=descriptor_offset,
                data_offset=data_offset,
                count_hint=field1,
                stride_hint=field3,
                label=label,
                classification=classification,
                preview_u16=preview_u16,
                preview_f32=preview_f32,
            )
        )
        seen_offsets.add(data_offset)
    return refs


def build_gta4_mesh_candidates(graphics_refs: Sequence[Gta4GraphicsRef]) -> List[Gta4MeshCandidate]:
    candidates: List[Gta4MeshCandidate] = []
    refs = sorted(graphics_refs, key=lambda ref: ref.descriptor_offset)
    for index_ref_index, index_ref in enumerate(refs):
        if index_ref.classification != "index_like":
            continue
        best_vertex: Optional[Gta4GraphicsRef] = None
        best_score = -10**9
        for back in range(max(0, index_ref_index - 6), index_ref_index):
            vertex_ref = refs[back]
            if vertex_ref.classification != "vertex_like":
                continue
            score = 0
            if vertex_ref.stride_hint == 36:
                score += 30
            if 0 < index_ref.count_hint <= max(6, vertex_ref.count_hint * 6):
                score += 20
            if vertex_ref.label and vertex_ref.label == index_ref.label:
                score += 5
            distance = index_ref.descriptor_offset - vertex_ref.descriptor_offset
            score -= distance // 16
            if score > best_score:
                best_score = score
                best_vertex = vertex_ref
        if best_vertex is None:
            continue
        candidates.append(
            Gta4MeshCandidate(
                vertex_ref_offset=best_vertex.descriptor_offset,
                index_ref_offset=index_ref.descriptor_offset,
                vertex_data_offset=best_vertex.data_offset,
                index_data_offset=index_ref.data_offset,
                vertex_count_hint=best_vertex.count_hint,
                index_count_hint=index_ref.count_hint,
                vertex_stride_hint=best_vertex.stride_hint,
                score=best_score,
            )
        )
    return candidates


def infer_gta4_index_buffer(data: bytes, data_offset: int, vertex_count: int, index_count_hint: int) -> Optional[List[int]]:
    available_u16 = (len(data) - data_offset) // 2
    if available_u16 < 3:
        return None

    lengths = []
    for candidate_length in (index_count_hint, index_count_hint * 3, min(index_count_hint * 6, available_u16)):
        if 3 <= candidate_length <= available_u16:
            lengths.append(candidate_length)
    lengths = list(dict.fromkeys(lengths))
    if not lengths:
        return None

    best_score = -10**9
    best_indices: Optional[List[int]] = None
    max_start = min(24, available_u16 - 3)
    for length in lengths:
        for start in range(max_start + 1):
            if start + length > available_u16:
                continue
            raw = list(struct.unpack_from(f"<{length}H", data, data_offset + start * 2))
            if not raw:
                continue
            filtered = [value for value in raw if value not in {0xFFFF, 0xCDCD}]
            if len(filtered) < 3:
                continue
            min_index = min(filtered)
            max_index = max(filtered)

            candidate_variants: List[List[int]] = [filtered]
            if min_index > 0:
                rebased = [value - min_index for value in filtered]
                candidate_variants.append(rebased)

            for variant in candidate_variants:
                if len(variant) < 3:
                    continue
                variant_max = max(variant)
                if variant_max >= vertex_count:
                    continue
                trimmed_len = (len(variant) // 3) * 3
                if trimmed_len < 3:
                    continue
                variant = variant[:trimmed_len]
                unique_values = len(set(variant))
                score = 0
                if start == 0:
                    score += 3
                if length == index_count_hint * 3:
                    score += 10
                elif length == index_count_hint:
                    score += 6
                score += min(variant_max, vertex_count) // 8
                score += unique_values
                if min_index > 0 and variant is not filtered:
                    score += 12
                if score > best_score:
                    best_score = score
                    best_indices = variant
    return best_indices


def try_build_gta4_mesh_preview(resource: Gta4RscResource, candidate: Gta4MeshCandidate) -> Optional[Tuple[str, str]]:
    if candidate.vertex_stride_hint != 36:
        return None
    data = resource.data
    vertex_region_size = candidate.vertex_count_hint * candidate.vertex_stride_hint
    if candidate.vertex_data_offset + vertex_region_size > len(data):
        return None

    positions: List[Tuple[float, float, float]] = []
    for vertex_index in range(candidate.vertex_count_hint):
        base = candidate.vertex_data_offset + vertex_index * candidate.vertex_stride_hint
        x, y, z = struct.unpack_from("<3f", data, base)
        if not all(math.isfinite(value) and abs(value) < 100000 for value in (x, y, z)):
            return None
        positions.append((float(x), float(y), float(z)))

    raw_indices = infer_gta4_index_buffer(data, candidate.index_data_offset, candidate.vertex_count_hint, candidate.index_count_hint)
    if raw_indices is None:
        return None
    face_count = len(raw_indices) // 3
    if face_count == 0:
        return None

    lines = ["# GTA4 heuristic mesh preview"]
    for x, y, z in positions:
        lines.append(f"v {x:.6f} {y:.6f} {z:.6f}")
    for face_index in range(face_count):
        i0, i1, i2 = raw_indices[face_index * 3: face_index * 3 + 3]
        lines.append(f"f {i0 + 1} {i1 + 1} {i2 + 1}")
    obj_text = "\n".join(lines) + "\n"
    summary = f"{len(positions)} verts, {face_count} faces"
    return obj_text, summary


def parse_preview_summary(summary: str) -> Tuple[int, int]:
    match = re.search(r"(\d+)\s+verts,\s+(\d+)\s+faces", summary)
    if not match:
        return 0, 0
    return int(match.group(1)), int(match.group(2))


def choose_best_gta4_preview_entry(manifest: Dict[str, object]) -> Optional[Dict[str, object]]:
    previews = manifest.get("mesh_previews")
    if not isinstance(previews, list):
        return None

    best_entry: Optional[Dict[str, object]] = None
    best_key: Optional[Tuple[int, int, int, int]] = None
    for preview in previews:
        if not isinstance(preview, dict):
            continue
        preview_path = Path(str(preview.get("file", "")))
        if not preview_path.exists():
            continue
        verts, faces = parse_preview_summary(str(preview.get("summary", "")))
        key = (
            faces,
            verts,
            int(preview.get("score", 0)),
            -int(preview.get("vertex_ref_offset", 0)),
        )
        if best_key is None or key > best_key:
            best_key = key
            best_entry = preview
    return best_entry


def choose_gta4_texture_name_candidates(manifest: Dict[str, object]) -> List[str]:
    candidates: List[str] = []
    for texture in manifest.get("decoded_textures", []):
        if isinstance(texture, dict):
            name = str(texture.get("name", "")).strip()
            if name:
                candidates.append(name)
    for value in manifest.get("strings", []):
        text = str(value).strip()
        if not text:
            continue
        if text.lower().endswith((".sps", ".fx")):
            continue
        if re.fullmatch(r"[A-Za-z0-9_]+", text):
            candidates.append(text)
    return list(dict.fromkeys(candidates))


def build_texture_stem_index(textures_dir: Path) -> Dict[str, Path]:
    index: Dict[str, Path] = {}
    for path in textures_dir.rglob("*"):
        if not path.is_file():
            continue
        if path.suffix.lower() not in {".png", ".jpg", ".jpeg", ".webp"}:
            continue
        index.setdefault(path.stem.lower(), path)
    return index


def compute_planar_uvs(positions: np.ndarray) -> np.ndarray:
    minimum = positions.min(axis=0)
    maximum = positions.max(axis=0)
    extents = maximum - minimum
    axis_order = list(np.argsort(extents))
    u_axis = int(axis_order[-1])
    v_axis = int(axis_order[-2]) if len(axis_order) > 1 else (u_axis + 1) % 3
    uv = positions[:, [u_axis, v_axis]].astype(np.float32)
    uv_min = uv.min(axis=0)
    uv_max = uv.max(axis=0)
    uv_extent = uv_max - uv_min
    for axis in range(2):
        if uv_extent[axis] > 1e-6:
            uv[:, axis] = (uv[:, axis] - uv_min[axis]) / uv_extent[axis]
        else:
            uv[:, axis] = 0.0
    return uv


def load_preview_obj_as_dff_model(obj_path: Path, model_name: str, material_name: str,
                                  texture_rel_path: Optional[Path]) -> Optional[DffModel]:
    positions: List[Tuple[float, float, float]] = []
    faces: List[Tuple[int, int, int]] = []
    for raw_line in obj_path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("v "):
            parts = line.split()
            if len(parts) < 4:
                continue
            positions.append((float(parts[1]), float(parts[2]), float(parts[3])))
        elif line.startswith("f "):
            parts = line.split()[1:]
            if len(parts) < 3:
                continue
            indices: List[int] = []
            for part in parts[:3]:
                vertex_text = part.split("/")[0]
                try:
                    vertex_index = int(vertex_text) - 1
                except ValueError:
                    vertex_index = -1
                if vertex_index < 0:
                    break
                indices.append(vertex_index)
            if len(indices) == 3:
                faces.append((indices[0], indices[1], indices[2]))

    if not positions or not faces:
        return None

    pos_array = np.array(positions, dtype=np.float32)
    idx_array = np.array(faces, dtype=np.uint32)
    normals = np.zeros_like(pos_array)
    for i0, i1, i2 in idx_array:
        p0 = pos_array[i0]
        p1 = pos_array[i1]
        p2 = pos_array[i2]
        normal = np.cross(p1 - p0, p2 - p0)
        length = float(np.linalg.norm(normal))
        if length <= 1e-8:
            continue
        normal /= length
        normals[i0] += normal
        normals[i1] += normal
        normals[i2] += normal
    normal_lengths = np.linalg.norm(normals, axis=1)
    valid = normal_lengths > 1e-8
    if np.any(valid):
        normals[valid] /= normal_lengths[valid][:, None]
    else:
        normals = None  # type: ignore[assignment]
    uvs = compute_planar_uvs(pos_array) if texture_rel_path is not None else None

    return DffModel(
        name=model_name,
        positions=pos_array,
        normals=normals,
        uvs=uvs,
        subsets=[GeometrySubset(indices=idx_array, material_name=material_name)],
        texture_paths={material_name: texture_rel_path} if texture_rel_path is not None else {},
    )


def collect_gta4_preview_zone_assets(output_dir: Path, zone_name: str, decoded_records: Sequence[Dict[str, object]],
                                     linked_extract_dir: Path, textures_dir: Optional[Path]) -> Optional[Dict[str, object]]:
    manifest_paths = sorted(linked_extract_dir.glob("*/manifest.json"))
    preview_models: Dict[str, DffModel] = {}
    preview_names: Dict[int, str] = {}
    preview_summaries: Dict[int, str] = {}
    preview_textures: Dict[int, str] = {}
    texture_index = build_texture_stem_index(textures_dir) if textures_dir is not None and textures_dir.exists() else {}
    zone_dir = output_dir / "assets" / "zones"
    zone_dir.mkdir(parents=True, exist_ok=True)
    zone_textures_dir = zone_dir / "textures"

    for manifest_path in manifest_paths:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        try:
            resource_index = int(manifest.get("resource_index", -1))
        except (TypeError, ValueError):
            continue
        preview = choose_best_gta4_preview_entry(manifest)
        if preview is None:
            continue
        obj_path = Path(str(preview.get("file", "")))
        if not obj_path.exists():
            continue
        model_name = f"resource_{resource_index}_{manifest_path.parent.name}"
        material_name = f"preview_{manifest_path.parent.name}"
        texture_rel_path: Optional[Path] = None
        for texture_name in choose_gta4_texture_name_candidates(manifest):
            resolved = texture_index.get(texture_name.lower())
            if resolved is None:
                continue
            zone_textures_dir.mkdir(parents=True, exist_ok=True)
            texture_target = zone_textures_dir / resolved.name
            if not texture_target.exists():
                shutil.copy2(resolved, texture_target)
            texture_rel_path = texture_target.relative_to(zone_dir)
            preview_textures[resource_index] = texture_name
            break
        model = load_preview_obj_as_dff_model(obj_path, model_name, material_name, texture_rel_path)
        if model is None:
            continue
        preview_names[resource_index] = model_name
        preview_models[model_name] = model
        preview_summaries[resource_index] = str(preview.get("summary", ""))

    if not preview_models:
        return None

    placements: List[Placement] = []
    missing_preview_matches = 0
    for record in decoded_records:
        matched = record.get("matched_resources")
        if not isinstance(matched, list):
            continue
        added_any = False
        for resource in matched:
            if not isinstance(resource, dict):
                continue
            try:
                resource_index = int(resource.get("resource_index", -1))
            except (TypeError, ValueError):
                continue
            model_name = preview_names.get(resource_index)
            if model_name is None:
                continue
            position = tuple(float(value) for value in record.get("position", [0.0, 0.0, 0.0]))
            quaternion = tuple(float(value) for value in record.get("quaternion_xyzw", [0.0, 0.0, 0.0, 1.0]))
            placements.append(
                Placement(
                    object_id=resource_index,
                    model_name=model_name,
                    position=position,
                    scale=(1.0, 1.0, 1.0),
                    quaternion_xyzw=quaternion,
                    source_ipl=str(record.get("source_file", zone_name)),
                )
            )
            added_any = True
            break
        if not added_any and matched:
            missing_preview_matches += 1

    if not placements:
        return None

    return {
        "zone": zone_name,
        "models": preview_models,
        "placements": placements,
        "missing_preview_matches": missing_preview_matches,
        "model_summaries": [
            {
                "resource_index": resource_index,
                "model_name": preview_names[resource_index],
                "summary": preview_summaries.get(resource_index, ""),
                "texture_name": preview_textures.get(resource_index, ""),
            }
            for resource_index in sorted(preview_names)
        ],
    }


def build_gta4_preview_zone_glb(output_dir: Path, zone_name: str, decoded_records: Sequence[Dict[str, object]],
                                linked_extract_dir: Path, output_format: str, double_sided: bool,
                                textures_dir: Optional[Path]) -> Optional[Dict[str, object]]:
    collected = collect_gta4_preview_zone_assets(output_dir, zone_name, decoded_records, linked_extract_dir, textures_dir)
    if collected is None:
        return None

    zone_dir = output_dir / "assets" / "zones"
    zone_dir.mkdir(parents=True, exist_ok=True)
    output_prefix = zone_dir / f"{zone_name}_preview"
    artifacts = build_scene_artifacts(
        collected["models"],
        collected["placements"],
        output_dir=zone_dir,
        binary_uri=None if output_format == "glb" else output_prefix.with_suffix(".bin").name,
        embed_images=(output_format == "glb"),
        flip_uv_v=True,
        flip_uv_u=False,
        double_sided=double_sided,
    )
    if output_format == "glb":
        scene_path = write_glb_export(artifacts, output_prefix)
    else:
        scene_path = write_gltf_export(artifacts, output_prefix)

    return {
        "zone": zone_name,
        "file": str(scene_path),
        "models": len(collected["models"]),
        "placements": len(collected["placements"]),
        "missing_preview_matches": collected["missing_preview_matches"],
        "model_summaries": collected["model_summaries"],
    }


def infer_gta4_zone_img_from_wpl(wpl_path: Path) -> Optional[Path]:
    candidate = wpl_path.with_suffix(".img")
    if candidate.exists():
        return candidate
    return None


def infer_gta4_related_zone_imgs(wpl_path: Path) -> List[Path]:
    related: List[Path] = []
    primary = infer_gta4_zone_img_from_wpl(wpl_path)
    if primary is not None:
        related.append(primary)

    parent = wpl_path.parent
    stem_lower = wpl_path.stem.lower()
    shared_candidates: List[str] = []
    if parent.name.lower() == "jersey" or stem_lower.startswith("nj_"):
        shared_candidates.append("nj_xref.img")
    if parent.name.lower() == "manhat" or stem_lower.startswith("manhat"):
        shared_candidates.append("manhatxr.img")
    if parent.name.lower() == "east" or stem_lower.startswith(("bronx_", "brook_", "queens_")):
        shared_candidates.append("east_xr.img")
    if parent.name.lower() == "leveldes" or stem_lower.startswith("level"):
        shared_candidates.append("level_xr.img")

    for candidate_name in shared_candidates:
        candidate = parent / candidate_name
        if candidate.exists() and candidate not in related:
            related.append(candidate)
    return related


def infer_gta4_shared_img_archives_for_wpl(wpl_path: Path, image_archives: Sequence[Dict[str, object]]) -> List[Path]:
    selected: List[Path] = []
    seen: set[Path] = set()

    def add(path: Path) -> None:
        if not path.exists() or path in seen:
            return
        seen.add(path)
        selected.append(path)

    for path in infer_gta4_related_zone_imgs(wpl_path):
        add(path)

    parent_name = wpl_path.parent.name.lower()
    for archive in image_archives:
        resolved = str(archive.get("resolved_path", ""))
        if not resolved:
            continue
        archive_path = Path(resolved)
        lowered = archive_path.as_posix().lower()

        if "/maps/generic/procobj.img" in lowered:
            add(archive_path)
            continue

        if "/maps/props/" in lowered:
            add(archive_path)
            continue

        if parent_name == "jersey" and "/maps/jersey/" in lowered:
            add(archive_path)
            continue
        if parent_name == "manhat" and "/maps/manhat/" in lowered:
            add(archive_path)
            continue
        if parent_name == "east" and "/maps/east/" in lowered:
            add(archive_path)
            continue
        if parent_name == "leveldes" and "/maps/leveldes/" in lowered:
            add(archive_path)
            continue

    return selected


def split_gta4_zone_and_global_imgs(wpl_path: Path, image_archives: Sequence[Dict[str, object]]) -> Tuple[List[Path], List[Path]]:
    local_imgs = infer_gta4_related_zone_imgs(wpl_path)
    local_set = set(local_imgs)
    all_imgs = infer_gta4_shared_img_archives_for_wpl(wpl_path, image_archives)
    global_imgs = [path for path in all_imgs if path not in local_set]
    return local_imgs, global_imgs


def pack_dds_header(width: int, height: int, fourcc: str, data_size: int) -> bytes:
    flags = 0x0002100F
    pitch_or_linear_size = data_size
    caps = 0x00001000
    pixel_flags = 0x00000004
    caps |= 0x00400008
    header = bytearray()
    header.extend(b"DDS ")
    header.extend(struct.pack("<I", 124))
    header.extend(struct.pack("<I", flags))
    header.extend(struct.pack("<I", height))
    header.extend(struct.pack("<I", width))
    header.extend(struct.pack("<I", pitch_or_linear_size))
    header.extend(struct.pack("<I", 0))
    header.extend(struct.pack("<I", 1))
    header.extend(b"\0" * (11 * 4))
    header.extend(struct.pack("<I", 32))
    header.extend(struct.pack("<I", pixel_flags))
    header.extend(fourcc.encode("ascii"))
    header.extend(struct.pack("<I", 0))
    header.extend(struct.pack("<I", 0))
    header.extend(struct.pack("<I", 0))
    header.extend(struct.pack("<I", 0))
    header.extend(struct.pack("<I", 0))
    header.extend(struct.pack("<I", caps))
    header.extend(struct.pack("<I", 0))
    header.extend(struct.pack("<I", 0))
    header.extend(struct.pack("<I", 0))
    header.extend(struct.pack("<I", 0))
    return bytes(header)


def expected_bc_texture_size(width: int, height: int, format_name: str) -> int:
    block_w = max(1, (width + 3) // 4)
    block_h = max(1, (height + 3) // 4)
    block_size = 8 if format_name == "DXT1" else 16
    return block_w * block_h * block_size


def parse_gta4_wtd_resource(resource: Gta4RscResource) -> List[Gta4TextureEntry]:
    data = resource.data
    textures: List[Gta4TextureEntry] = []
    for offset in range(0, max(0, len(data) - 80), 8):
        format_name = data[offset + 40: offset + 44].decode("ascii", errors="ignore").strip("\0")
        if format_name not in {"DXT1", "DXT3", "DXT5"}:
            continue
        data_ptr = read_u32(data, offset)
        if (data_ptr & 0xF0000000) != 0x60000000:
            continue
        name_ptr = read_u32(data, offset + 28)
        if (name_ptr & 0xF0000000) != 0x50000000:
            continue
        packed_dims = read_u32(data, offset + 36)
        width = packed_dims & 0xFFFF
        height = (packed_dims >> 16) & 0xFFFF
        if width <= 0 or height <= 0:
            continue
        data_offset = data_ptr & 0x0FFFFFFF
        if data_offset >= len(data):
            continue
        name_offset = name_ptr & 0x0FFFFFFF
        if name_offset >= len(data):
            continue
        raw_name = data[name_offset:name_offset + 128].split(b"\0", 1)[0].decode("latin-1", errors="ignore")
        if not raw_name:
            continue
        texture_name = Path(raw_name[6:] if raw_name.lower().startswith("pack:/") else raw_name).stem
        data_size = expected_bc_texture_size(width, height, format_name)
        if data_offset + data_size > len(data):
            continue
        dds_payload = data[data_offset:data_offset + data_size]
        dds_bytes = pack_dds_header(width, height, format_name, data_size) + dds_payload
        textures.append(
            Gta4TextureEntry(
                name=texture_name,
                format_name=format_name,
                width=width,
                height=height,
                data_offset=data_offset,
                data_size=data_size,
                dds_bytes=dds_bytes,
            )
        )
    unique: Dict[str, Gta4TextureEntry] = {}
    for texture in textures:
        key = texture.name.lower()
        current = unique.get(key)
        if current is None:
            unique[key] = texture
            continue

        current_score = (current.width * current.height, current.data_size)
        candidate_score = (texture.width * texture.height, texture.data_size)
        if candidate_score > current_score:
            unique[key] = texture
    return list(unique.values())


def write_gta4_texture_entry(texture: Gta4TextureEntry, output_dir: Path, texture_output_format: str) -> List[Dict[str, object]]:
    output_dir.mkdir(parents=True, exist_ok=True)
    exported: List[Dict[str, object]] = []

    dds_path = output_dir / f"{texture.name}.dds"
    if texture_output_format in {"dds", "both"}:
        dds_path.write_bytes(texture.dds_bytes)
        exported.append(
            {
                "name": texture.name,
                "format": texture.format_name,
                "width": texture.width,
                "height": texture.height,
                "file": str(dds_path),
                "output_kind": "dds",
            }
        )

    if texture_output_format in {"png", "both"}:
        png_path = output_dir / f"{texture.name}.png"
        try:
            ImageFile.LOAD_TRUNCATED_IMAGES = True
            png_source_dds = build_gta4_png_source_dds(texture)
            with Image.open(io.BytesIO(png_source_dds)) as image:
                processed = maybe_postprocess_gta4_png(image, texture.name, texture.format_name)
                processed.save(png_path)
            exported.append(
                {
                    "name": texture.name,
                    "format": texture.format_name,
                    "width": texture.width,
                    "height": texture.height,
                    "file": str(png_path),
                    "output_kind": "png",
                }
            )
        except Exception as exc:  # noqa: BLE001
            exported.append(
                {
                    "name": texture.name,
                    "format": texture.format_name,
                    "width": texture.width,
                    "height": texture.height,
                    "file": "",
                    "output_kind": "png_error",
                    "error": str(exc),
                }
            )

    return exported


def reorder_gta4_bc_payload_for_png(payload: bytes, format_name: str) -> bytes:
    if format_name == "DXT1":
        chunks = [payload[index:index + 8] for index in range(0, len(payload), 8)]
        return b"".join(chunk[4:8] + chunk[0:4] if len(chunk) == 8 else chunk for chunk in chunks)
    if format_name in {"DXT3", "DXT5"}:
        chunks = [payload[index:index + 16] for index in range(0, len(payload), 16)]
        return b"".join(chunk[4:8] + chunk[0:4] + chunk[12:16] + chunk[8:12] if len(chunk) == 16 else chunk for chunk in chunks)
    return payload


def is_gta4_likely_alpha_texture(texture_name: str) -> bool:
    lowered = texture_name.lower()
    alpha_markers = (
        "alpha",
        "mask",
        "glass",
        "grate",
        "grill",
        "fence",
        "chain",
        "wire",
        "leaf",
        "leav",
        "tree",
        "bush",
        "foliage",
        "net",
    )
    return any(marker in lowered for marker in alpha_markers)


def should_normalize_gta4_dxt1_opaque(payload: bytes, texture_name: str) -> bool:
    if is_gta4_likely_alpha_texture(texture_name):
        return False
    block_count = 0
    transparent_mode_blocks = 0
    for index in range(0, len(payload), 8):
        chunk = payload[index:index + 8]
        if len(chunk) < 8:
            continue
        block_count += 1
        color0 = int.from_bytes(chunk[0:2], "little")
        color1 = int.from_bytes(chunk[2:4], "little")
        if color0 <= color1:
            transparent_mode_blocks += 1
    if block_count == 0:
        return False
    return (transparent_mode_blocks / block_count) >= 0.02


def normalize_gta4_dxt1_opaque(payload: bytes) -> bytes:
    normalized = bytearray()
    for index in range(0, len(payload), 8):
        chunk = payload[index:index + 8]
        if len(chunk) < 8:
            normalized.extend(chunk)
            continue
        color0 = int.from_bytes(chunk[0:2], "little")
        color1 = int.from_bytes(chunk[2:4], "little")
        selector_bits = int.from_bytes(chunk[4:8], "little")
        if color0 <= color1:
            color0, color1 = color1, color0
            remapped_bits = 0
            for pixel_index in range(16):
                selector = (selector_bits >> (2 * pixel_index)) & 0x3
                remapped_bits |= (selector ^ 0x1) << (2 * pixel_index)
            selector_bits = remapped_bits
        normalized.extend(color0.to_bytes(2, "little"))
        normalized.extend(color1.to_bytes(2, "little"))
        normalized.extend(selector_bits.to_bytes(4, "little"))
    return bytes(normalized)


def build_gta4_png_source_dds(texture: Gta4TextureEntry) -> bytes:
    # GTA4 map WTDs have proven more reliable with the original block order.
    # Global BC block rewrites fix some assets but corrupt many roadside/map textures,
    # so PNG export keeps the raw payload unless a per-format postprocess is applied later.
    return texture.dds_bytes


def maybe_postprocess_gta4_png(image: Image.Image, texture_name: str, format_name: str) -> Image.Image:
    lowered = texture_name.lower()
    if format_name != "DXT5" or "_diff_" not in lowered:
        return image

    src = image.convert("RGBA")
    width, height = src.size
    dst = Image.new("RGBA", (width, height))
    src_pixels = src.load()
    dst_pixels = dst.load()
    for y in range(height):
        for x in range(width):
            r, g, b, a = src_pixels[x, y]
            scale = (b * (255.0 / 8.0) / 255.0) + 1.0
            co = ((r / 255.0) - 0.5) / scale
            cg = ((g / 255.0) - 0.5) / scale
            luminance = a / 255.0
            out_r = max(0.0, min(1.0, luminance + co - cg))
            out_g = max(0.0, min(1.0, luminance + cg))
            out_b = max(0.0, min(1.0, luminance - co - cg))
            dst_pixels[x, y] = (
                int(out_r * 255.0),
                int(out_g * 255.0),
                int(out_b * 255.0),
                255,
            )
    return dst


def export_gta4_rsc_resource(resource: Gta4RscResource, output_dir: Path,
                             texture_output_format: str = "dds") -> Dict[str, object]:
    resource_dir = output_dir / resource.guessed_name
    resource_dir.mkdir(parents=True, exist_ok=True)
    raw_path = resource_dir / "resource.bin"
    raw_path.write_bytes(resource.data)

    textures_dir = resource_dir / "textures"
    exported_textures: List[Dict[str, object]] = []
    if resource.resource_type == "wtd_texture_pack":
        for texture in parse_gta4_wtd_resource(resource):
            exported_textures.extend(write_gta4_texture_entry(texture, textures_dir, texture_output_format))

    graphics_refs = parse_gta4_graphics_refs(resource)
    mesh_candidates = build_gta4_mesh_candidates(graphics_refs)
    preview_exports: List[Dict[str, object]] = []
    previews_dir = resource_dir / "mesh_previews"
    successful_previews: List[Tuple[int, int, int, Gta4MeshCandidate, str]] = []
    for candidate in sorted(mesh_candidates, key=lambda item: item.score, reverse=True):
        preview = try_build_gta4_mesh_preview(resource, candidate)
        if preview is None:
            continue
        obj_text, summary = preview
        verts, faces = parse_preview_summary(summary)
        if faces <= 0 or verts <= 0:
            continue
        successful_previews.append((faces, verts, candidate.score, candidate, obj_text))
    successful_previews.sort(key=lambda item: (item[0], item[1], item[2]), reverse=True)

    for preview_index, (faces, verts, score, candidate, obj_text) in enumerate(successful_previews[:5], start=1):
        previews_dir.mkdir(parents=True, exist_ok=True)
        preview_path = previews_dir / f"preview_{preview_index:02d}.obj"
        preview_path.write_text(obj_text, encoding="utf-8")
        preview_exports.append(
            {
                "file": str(preview_path),
                "summary": f"{verts} verts, {faces} faces",
                "vertex_ref_offset": candidate.vertex_ref_offset,
                "index_ref_offset": candidate.index_ref_offset,
                "score": score,
            }
        )
    manifest = {
        "source_path": resource.source_path,
        "source_kind": resource.source_kind,
        "source_archive": resource.source_archive,
        "resource_index": resource.resource_index,
        "offset": resource.offset,
        "packed_size": resource.packed_size,
        "unpacked_size": resource.unpacked_size,
        "compression": resource.compression,
        "resource_type": resource.resource_type,
        "guessed_name": resource.guessed_name,
        "name_candidates": resource.name_candidates,
        "strings": resource.strings,
        "decoded_textures": exported_textures,
        "graphics_refs": [
            {
                "descriptor_offset": ref.descriptor_offset,
                "data_offset": ref.data_offset,
                "count_hint": ref.count_hint,
                "stride_hint": ref.stride_hint,
                "label": ref.label,
                "classification": ref.classification,
                "preview_u16": ref.preview_u16,
                "preview_f32": ref.preview_f32,
            }
            for ref in graphics_refs
        ],
        "mesh_candidates": [
            {
                "vertex_ref_offset": candidate.vertex_ref_offset,
                "index_ref_offset": candidate.index_ref_offset,
                "vertex_data_offset": candidate.vertex_data_offset,
                "index_data_offset": candidate.index_data_offset,
                "vertex_count_hint": candidate.vertex_count_hint,
                "index_count_hint": candidate.index_count_hint,
                "vertex_stride_hint": candidate.vertex_stride_hint,
                "score": candidate.score,
            }
            for candidate in mesh_candidates
        ],
        "mesh_previews": preview_exports,
    }
    manifest_path = resource_dir / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False), encoding="utf-8")
    return manifest


def sanitize_name_for_path(name: str) -> str:
    clean = re.sub(r"[^A-Za-z0-9._-]+", "_", name.strip())
    return clean.strip("._") or "resource"


def export_gta4_loose_wtd(path: Path, output_dir: Path, texture_output_format: str = "dds") -> Dict[str, object]:
    resource = parse_gta4_rsc_bytes(
        path.read_bytes(),
        source_path=str(path),
        source_kind="loose_rsc5",
        source_archive=path.name,
        resource_index=0,
        offset=0,
    )
    if resource is None:
        payload = {
            "source_path": str(path),
            "decoded_textures": [],
            "valid_rsc5": False,
        }
        target = output_dir / sanitize_name_for_path(path.stem)
        target.mkdir(parents=True, exist_ok=True)
        (target / "manifest.json").write_text(json.dumps(payload, indent=2, ensure_ascii=False), encoding="utf-8")
        return payload
    resource.guessed_name = sanitize_name_for_path(path.stem)
    return export_gta4_rsc_resource(resource, output_dir / sanitize_name_for_path(path.stem), texture_output_format)


def export_gta4_game_textures(game_root: Path, image_archives: Sequence[Dict[str, object]], output_dir: Path,
                              texture_output_format: str) -> Dict[str, object]:
    textures_root = output_dir / "assets" / "textures"
    textures_root.mkdir(parents=True, exist_ok=True)
    exported_files = 0
    exported_png = 0
    exported_dds = 0
    png_errors = 0
    archives_processed = 0
    resources_processed = 0
    texture_packs_found = 0
    manifest_entries: List[Dict[str, object]] = []

    for archive in image_archives:
        resolved = str(archive.get("resolved_path", ""))
        if not resolved:
            continue
        archive_path = Path(resolved)
        archive_name = archive_path.stem
        try:
            resources = parse_gta4_img_resources(archive_path)
        except Exception as exc:  # noqa: BLE001
            manifest_entries.append(
                {
                    "archive": archive_name,
                    "path": str(archive_path),
                    "error": str(exc),
                }
            )
            continue
        archives_processed += 1
        resources_processed += len(resources)
        archive_export_count = 0
        for resource in resources:
            if resource.resource_type != "wtd_texture_pack":
                continue
            texture_packs_found += 1
            target_dir = textures_root / sanitize_name_for_path(archive_name) / sanitize_name_for_path(resource.guessed_name)
            manifest = export_gta4_rsc_resource(resource, target_dir, texture_output_format)
            decoded = manifest.get("decoded_textures", [])
            if not isinstance(decoded, list):
                continue
            archive_export_count += len(decoded)
            for texture_entry in decoded:
                if not isinstance(texture_entry, dict):
                    continue
                kind = str(texture_entry.get("output_kind", ""))
                if kind == "dds":
                    exported_dds += 1
                    exported_files += 1
                elif kind == "png":
                    exported_png += 1
                    exported_files += 1
                elif kind == "png_error":
                    png_errors += 1
        manifest_entries.append(
            {
                "archive": archive_name,
                "path": str(archive_path),
                "decoded_outputs": archive_export_count,
            }
        )

    for loose_dir in [game_root / "pc" / "textures", game_root / "pc" / "models"]:
        if not loose_dir.exists():
            continue
        for item in sorted(loose_dir.iterdir(), key=lambda p: p.name.lower()):
            if not item.is_file() or item.suffix.lower() != ".wtd":
                continue
            texture_packs_found += 1
            manifest = export_gta4_loose_wtd(item, textures_root / "loose_wtd", texture_output_format)
            decoded = manifest.get("decoded_textures", [])
            if not isinstance(decoded, list):
                continue
            for texture_entry in decoded:
                if not isinstance(texture_entry, dict):
                    continue
                kind = str(texture_entry.get("output_kind", ""))
                if kind == "dds":
                    exported_dds += 1
                    exported_files += 1
                elif kind == "png":
                    exported_png += 1
                    exported_files += 1
                elif kind == "png_error":
                    png_errors += 1

    summary = {
        "textures_root": str(textures_root),
        "texture_output_format": texture_output_format,
        "archives_processed": archives_processed,
        "resources_processed": resources_processed,
        "texture_packs_found": texture_packs_found,
        "exported_files": exported_files,
        "exported_dds": exported_dds,
        "exported_png": exported_png,
        "png_errors": png_errors,
        "archives": manifest_entries,
    }
    summary_path = textures_root / "texture_export_summary.json"
    summary_path.write_text(json.dumps(summary, indent=2, ensure_ascii=False), encoding="utf-8")
    summary["summary_path"] = str(summary_path)
    return summary


def gta4_sector_index(position: Tuple[float, float, float], sector_size: float) -> Tuple[int, int]:
    transformed = BASIS_GTA_TO_GLTF @ np.array(position, dtype=np.float32)
    return math.floor(float(transformed[0]) / sector_size), math.floor(float(transformed[2]) / sector_size)


def group_gta4_placements_by_sector(placements: Sequence[Gta4Placement], sector_size: float) -> Dict[Tuple[int, int], List[Gta4Placement]]:
    grouped: Dict[Tuple[int, int], List[Gta4Placement]] = {}
    for placement in placements:
        sector = gta4_sector_index(placement.position, sector_size)
        grouped.setdefault(sector, []).append(placement)
    return grouped


def build_gta4_sector_manifest_from_preview_collections(output_dir: Path, collected_zones: Sequence[Dict[str, object]],
                                                        sector_size: float) -> Path:
    sectors_dir = output_dir / "assets" / "sectors"
    sectors_dir.mkdir(parents=True, exist_ok=True)
    grouped: Dict[Tuple[int, int], List[Placement]] = {}

    for zone in collected_zones:
        placements = zone.get("placements", [])
        if not isinstance(placements, list):
            continue
        for placement in placements:
            if not isinstance(placement, Placement):
                continue
            sector = sector_index_for_placement(placement, sector_size)
            grouped.setdefault(sector, []).append(placement)

    manifest: List[Dict[str, object]] = []
    for (sector_x, sector_z), sector_placements in sorted(grouped.items()):
        payload = {
            "sector_x": sector_x,
            "sector_z": sector_z,
            "sector_size": sector_size,
            "placements": [
                {
                    "model_name": placement.model_name,
                    "position": [float(v) for v in placement.position],
                    "quaternion_xyzw": [float(v) for v in placement.quaternion_xyzw],
                    "source_file": placement.source_ipl,
                    "source_kind": "preview_zone_model",
                }
                for placement in sector_placements
            ],
        }
        sector_path = sectors_dir / f"sec_{sector_x}_{sector_z}.json"
        sector_path.write_text(json.dumps(payload, indent=2, ensure_ascii=False), encoding="utf-8")
        manifest.append(
            {
                "sector_x": sector_x,
                "sector_z": sector_z,
                "file": sector_path.name,
                "placements": len(sector_placements),
            }
        )

    index_path = sectors_dir / "index.json"
    index_path.write_text(json.dumps({"sector_size": sector_size, "sectors": manifest}, indent=2, ensure_ascii=False), encoding="utf-8")
    return index_path


def build_gta4_sector_manifest(output_dir: Path, placements: Sequence[Gta4Placement], sector_size: float) -> Path:
    sectors_dir = output_dir / "assets" / "sectors"
    sectors_dir.mkdir(parents=True, exist_ok=True)
    grouped = group_gta4_placements_by_sector(placements, sector_size)
    manifest: List[Dict[str, object]] = []
    for (sector_x, sector_z), sector_placements in sorted(grouped.items()):
        payload = {
            "sector_x": sector_x,
            "sector_z": sector_z,
            "sector_size": sector_size,
            "placements": [
                {
                    "model_name": placement.model_name,
                    "position": [float(v) for v in placement.position],
                    "quaternion_xyzw": [float(v) for v in placement.quaternion_xyzw],
                    "source_file": placement.source_file,
                    "source_kind": placement.source_kind,
                }
                for placement in sector_placements
            ],
        }
        sector_path = sectors_dir / f"sec_{sector_x}_{sector_z}.json"
        sector_path.write_text(json.dumps(payload, indent=2, ensure_ascii=False), encoding="utf-8")
        manifest.append(
            {
                "sector_x": sector_x,
                "sector_z": sector_z,
                "file": sector_path.name,
                "placements": len(sector_placements),
            }
        )
    index_path = sectors_dir / "index.json"
    index_path.write_text(json.dumps({"sector_size": sector_size, "sectors": manifest}, indent=2, ensure_ascii=False), encoding="utf-8")
    return index_path


def export_gta4_preview_sector_glbs(output_dir: Path, collected_zones: Sequence[Dict[str, object]], sector_size: float,
                                    double_sided: bool) -> List[Dict[str, object]]:
    combined_models: Dict[str, DffModel] = {}
    combined_placements: List[Placement] = []
    for zone in collected_zones:
        zone_name = str(zone.get("zone", "zone"))
        models = zone.get("models", {})
        placements = zone.get("placements", [])
        if not isinstance(models, dict) or not isinstance(placements, list):
            continue
        name_map: Dict[str, str] = {}
        for model_name, model in models.items():
            unique_name = f"{zone_name}__{model_name}"
            rebased_texture_paths: Dict[str, Path] = {}
            for material_name, texture_path in model.texture_paths.items():
                if texture_path.is_absolute():
                    rebased_texture_paths[material_name] = texture_path
                else:
                    rebased_texture_paths[material_name] = Path("assets") / "zones" / texture_path
            combined_models[unique_name.lower()] = DffModel(
                name=unique_name,
                positions=model.positions,
                normals=model.normals,
                uvs=model.uvs,
                subsets=model.subsets,
                texture_paths=rebased_texture_paths,
            )
            name_map[model_name] = unique_name
        for placement in placements:
            remapped_name = name_map.get(placement.model_name, placement.model_name)
            combined_placements.append(
                Placement(
                    object_id=placement.object_id,
                    model_name=remapped_name,
                    position=placement.position,
                    scale=placement.scale,
                    quaternion_xyzw=placement.quaternion_xyzw,
                    source_ipl=placement.source_ipl,
                )
            )
    if not combined_models or not combined_placements:
        return []
    return export_sector_glbs(
        output_dir=output_dir,
        models=combined_models,
        placements=combined_placements,
        sector_size=sector_size,
        flip_uv_v=True,
        flip_uv_u=False,
        double_sided=double_sided,
    )


def copy_if_exists(src: Path, dst: Path) -> bool:
    if not src.exists():
        return False
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)
    return True


def run_gta4_export(args: argparse.Namespace) -> None:
    game_root = Path(args.game_root).expanduser()
    output_dir = Path(args.output_dir).expanduser()
    output_dir.mkdir(parents=True, exist_ok=True)
    effective_texture_output_format = args.texture_output_format
    if args.extract_textures and not args.textures_dir and effective_texture_output_format == "dds":
        effective_texture_output_format = "both"
        print("[info] Ajustando texturas GTA4 a 'both' para que el GLB pueda enlazar PNG ademas de DDS.")

    dat_rel_path = args.dat or "common/data/gta.dat"
    refs = parse_gta4_dat_references(game_root, dat_rel_path)
    print(f"[info] IDE detectados: {len(refs['ide'])}")
    print(f"[info] IPL texto detectados: {len(refs['ipl'])}")
    print(f"[info] WPL binarios detectados: {len(refs['wpl'])}")

    image_archives: List[Dict[str, object]] = []
    for imglist_path in refs["imglist"]:
        image_archives.extend(parse_gta4_images_list(game_root, imglist_path))
    print(f"[info] Archivos IMG listados: {len(image_archives)}")

    category_defs: Dict[str, List[Gta4AssetDef]] = {
        "map": [],
        "vehicles": [],
        "peds": [],
        "weapons": [],
        "props": [],
    }
    for ide_path in refs["ide"]:
        parsed = parse_gta4_ide_file(ide_path)
        for category_name, entries in parsed.items():
            category_defs[category_name].extend(entries)
    model_hash_lookup = build_gta4_model_hash_lookup(refs["ide"])
    texture_export_summary: Optional[Dict[str, object]] = None
    do_global_texture_export = bool(args.extract_textures and args.export_category)
    if args.extract_textures and not do_global_texture_export:
        print("[info] Modo mapa GTA4: se omite la extracción global y se usarán texturas de recursos enlazados por zona.")
    if do_global_texture_export:
        texture_export_summary = export_gta4_game_textures(
            game_root=game_root,
            image_archives=image_archives,
            output_dir=output_dir,
            texture_output_format=effective_texture_output_format,
        )
        print(
            f"[ok] Texturas GTA4 exportadas: {texture_export_summary['exported_files']} "
            f"(dds={texture_export_summary['exported_dds']}, png={texture_export_summary['exported_png']})"
        )
        if int(texture_export_summary.get("png_errors", 0)):
            print(f"[aviso] Conversiones PNG con error: {texture_export_summary['png_errors']}")

    requested_categories = list(dict.fromkeys(args.export_category))
    copied_raw_files: List[str] = []
    parsed_resource_manifests: List[Dict[str, object]] = []
    category_summaries: List[Dict[str, object]] = []
    archive_groups = {
        "vehicles": {"vehicles", "vehshare", "vehshare_truck", "vehshare_worn"},
        "peds": {"componentpeds", "pedprops"},
        "weapons": {"weapons"},
        "props": set(),
    }

    for category_name in requested_categories:
        category_dir = output_dir / "assets" / category_name
        category_dir.mkdir(parents=True, exist_ok=True)
        entries = category_defs.get(category_name, [])
        category_parsed_manifests: List[Dict[str, object]] = []
        manifest_path = category_dir / f"{category_name}_catalog.json"
        manifest_payload = {
            "game": "gta4",
            "category": category_name,
            "entries": [
                {
                    "model_name": entry.model_name,
                    "texture_name": entry.texture_name,
                    "source_file": entry.source_file,
                    "section": entry.section,
                    "extra": entry.extra,
                }
                for entry in entries
            ],
        }
        manifest_path.write_text(json.dumps(manifest_payload, indent=2, ensure_ascii=False), encoding="utf-8")

        matched_archives: List[str] = []
        wanted_archives = archive_groups.get(category_name, set())
        for archive in image_archives:
            archive_stem = Path(str(archive["archive_name"])).stem.lower()
            if category_name == "props":
                if archive_stem in {"vehicles", "weapons", "componentpeds", "pedprops", "anim", "cuts", "cutsprops"}:
                    continue
            elif archive_stem not in wanted_archives:
                continue
            resolved = str(archive.get("resolved_path", ""))
            if not resolved:
                continue
            archive_path = Path(resolved)
            dest = category_dir / "raw" / archive_path.name
            if copy_if_exists(archive_path, dest):
                matched_archives.append(dest.name)
                copied_raw_files.append(str(dest))
                resources_dir = category_dir / "parsed_resources" / archive_path.stem
                try:
                    resources = parse_gta4_img_resources(archive_path)
                except Exception as exc:  # noqa: BLE001
                    print(f"[aviso] No se pudo parsear {archive_path.name}: {exc}")
                    resources = []
                for resource in resources:
                    manifest = export_gta4_rsc_resource(resource, resources_dir, effective_texture_output_format)
                    parsed_resource_manifests.append(manifest)
                    category_parsed_manifests.append(manifest)
                if resources:
                    print(f"[info] {category_name}: {archive_path.name} recursos RSC5 {len(resources)}")

        loose_dirs = []
        if category_name == "props":
            loose_dirs = [game_root / "pc" / "textures", game_root / "pc" / "models"]
        for loose_dir in loose_dirs:
            if not loose_dir.exists():
                continue
            copied = 0
            parsed = 0
            for item in sorted(loose_dir.iterdir(), key=lambda p: p.name.lower()):
                if not item.is_file():
                    continue
                if item.suffix.lower() not in {".wtd", ".wdd", ".wdr"}:
                    continue
                dest = category_dir / "raw_loose" / item.name
                if copy_if_exists(item, dest):
                    copied += 1
                    copied_raw_files.append(str(dest))
                if item.suffix.lower() == ".wtd":
                    try:
                        manifest = export_gta4_loose_wtd(item, category_dir / "parsed_resources" / "loose_wtd", effective_texture_output_format)
                        parsed_resource_manifests.append(manifest)
                        category_parsed_manifests.append(manifest)
                        parsed += 1
                    except Exception as exc:  # noqa: BLE001
                        print(f"[aviso] No se pudo extraer {item.name}: {exc}")
            print(f"[info] {category_name}: archivos loose copiados {copied}, WTD parseados {parsed}")

        category_summaries.append(
            {
                "category": category_name,
                "entries": len(entries),
                "catalog": str(manifest_path),
                "raw_archives": matched_archives,
                "parsed_resources": len(category_parsed_manifests),
            }
        )
        print(f"[ok] {category_name}: catálogo escrito en {manifest_path}")

    copied_anim_files: List[str] = []
    if args.copy_ped_anims:
        animation_targets = [
            ("common/data/animgrp.dat", "assets/animations_raw/animgrp.dat"),
            ("common/data/ped.dat", "assets/animations_raw/ped.dat"),
            ("common/data/pedgrp.dat", "assets/animations_raw/pedgrp.dat"),
            ("common/data/pedpersonality.dat", "assets/animations_raw/pedpersonality.dat"),
            ("common/data/pedVariations.dat", "assets/animations_raw/pedVariations.dat"),
            ("pc/anim/anim.img", "assets/animations_raw/anim.img"),
        ]
        for rel_src, rel_dst in animation_targets:
            try:
                src = resolve_case_path(game_root, rel_src)
            except FileNotFoundError:
                continue
            dest = output_dir / rel_dst
            if copy_if_exists(src, dest):
                copied_anim_files.append(str(dest))
        if copied_anim_files:
            print(f"[ok] Animaciones/copias crudas GTA4: {len(copied_anim_files)}")
        else:
            print("[aviso] No se encontraron archivos crudos de animación GTA4 para copiar")

    placements: List[Gta4Placement] = []
    selected_ipl_paths = refs["ipl"]
    selected_wpl_paths = refs["wpl"]
    if args.ipl:
        requested_filters = [entry.lower() for entry in args.ipl]
        explicit_ipl: List[Path] = []
        explicit_wpl: List[Path] = []
        for raw_entry in args.ipl:
            candidate = Path(raw_entry).expanduser()
            if candidate.exists():
                if candidate.suffix.lower() == ".wpl":
                    explicit_wpl.append(candidate)
                else:
                    explicit_ipl.append(candidate)
                continue
            try:
                resolved = resolve_gta4_virtual_path(game_root, raw_entry)
            except FileNotFoundError:
                resolved = None
            if resolved is not None:
                if resolved.suffix.lower() == ".wpl":
                    explicit_wpl.append(resolved)
                else:
                    explicit_ipl.append(resolved)
        if explicit_ipl or explicit_wpl:
            selected_ipl_paths = explicit_ipl
            selected_wpl_paths = explicit_wpl
        else:
            selected_ipl_paths = [path for path in refs["ipl"] if any(token in str(path).lower() for token in requested_filters)]
            selected_wpl_paths = [path for path in refs["wpl"] if any(token in str(path).lower() for token in requested_filters)]

    if not args.skip_map:
        for ipl_path in selected_ipl_paths:
            placements.extend(parse_gta4_text_ipl_file(ipl_path, include_lod=args.include_lod))
        print(f"[info] Placements GTA4 texto detectados: {len(placements)}")

    wpl_headers = [parse_gta4_wpl_header(path) for path in selected_wpl_paths]
    wpl_records: List[Dict[str, object]] = []
    wpl_placements: List[Gta4Placement] = []
    wpl_dump_paths: List[str] = []
    zone_resource_indexes: List[Dict[str, object]] = []
    preview_zone_exports: List[Dict[str, object]] = []
    preview_zone_collections: List[Dict[str, object]] = []
    gta4_img_resource_cache: Dict[str, List[Gta4RscResource]] = {}
    gta4_geometry_cache: Dict[Tuple[str, int], Dict[str, int | bool]] = {}
    export_zone_previews = bool(getattr(args, "export_zone_previews", False))
    if selected_wpl_paths:
        wpl_dir = output_dir / "wpl_decoded"
        zone_index_dir = output_dir / "zone_resource_indexes"
        linked_resources_dir = output_dir / "wpl_linked_resources"
        wpl_dir.mkdir(parents=True, exist_ok=True)
        zone_index_dir.mkdir(parents=True, exist_ok=True)
        linked_resources_dir.mkdir(parents=True, exist_ok=True)
        for wpl_path in selected_wpl_paths:
            decoded = parse_gta4_wpl_records(wpl_path, model_hash_lookup, include_lod=args.include_lod)
            target_hashes = [int(str(record.get("archetype_hash", "0x0")), 16) for record in decoded.get("records", [])]
            target_hash_set = set(target_hashes)
            local_zone_img_paths, global_zone_img_paths = split_gta4_zone_and_global_imgs(wpl_path, image_archives)
            zone_img_paths = local_zone_img_paths + global_zone_img_paths
            zone_img_path = zone_img_paths[0] if zone_img_paths else None
            zone_resources: List[Gta4RscResource] = []
            resource_by_key: Dict[Tuple[str, int], Gta4RscResource] = {}
            zone_resource_lookup: Dict[int, List[Dict[str, object]]] = {}
            zone_resource_name_lookup: Dict[str, List[Dict[str, object]]] = {}
            zone_resource_hash_lookup: Dict[int, List[Dict[str, object]]] = {}
            resource_hash_matches: Dict[Tuple[str, int], List[str]] = {}
            zone_resource_index: Dict[str, object] = {
                "wpl_path": str(wpl_path),
                "img_path": str(zone_img_path) if zone_img_path is not None else "",
                "img_paths": [str(path) for path in zone_img_paths],
                "local_img_paths": [str(path) for path in local_zone_img_paths],
                "global_img_paths": [str(path) for path in global_zone_img_paths],
                "resources": [],
            }
            if zone_img_paths:
                try:
                    local_resources: List[Gta4RscResource] = []
                    global_resources: List[Gta4RscResource] = []
                    for img_path in local_zone_img_paths:
                        local_resources.extend(parse_gta4_img_resources_cached(img_path, gta4_img_resource_cache))
                    for img_path in global_zone_img_paths:
                        global_resources.extend(parse_gta4_img_resources_cached(img_path, gta4_img_resource_cache))
                    zone_resources = local_resources + global_resources
                    resource_by_key = {(resource.source_path, resource.resource_index): resource for resource in zone_resources}
                    zone_resource_lookup = build_gta4_resource_hash_lookup(zone_resources)
                    zone_resource_name_lookup = build_gta4_resource_name_lookup(zone_resources)
                    zone_resource_hash_lookup, resource_hash_matches = build_gta4_resource_archetype_lookup(local_resources, target_hashes)
                    zone_resource_index["resources"] = []
                    for resource in zone_resources:
                        matched_hashes = resource_hash_matches.get((resource.source_path, resource.resource_index), [])
                        zone_resource_index["resources"].append(
                            {
                                "guessed_name": resource.guessed_name,
                                "name_candidates": resource.name_candidates,
                                "resource_index": resource.resource_index,
                                "resource_type": resource.resource_type,
                                "source_path": resource.source_path,
                                "source_archive": resource.source_archive,
                                "direct_name_hash_hits": sum(
                                    1
                                    for candidate in resource.name_candidates
                                    if gta4_joaat(candidate) in target_hash_set
                                ),
                                "matched_archetype_hashes": matched_hashes,
                                "role_guess": guess_gta4_resource_role(resource.resource_type, len(matched_hashes), resource.name_candidates),
                            }
                        )
                except Exception as exc:  # noqa: BLE001
                    zone_resource_index["error"] = str(exc)
            final_matched_resource_keys: set[Tuple[str, int]] = set()
            for record in decoded.get("records", []):
                archetype_hash_text = str(record.get("archetype_hash", "0x00000000"))
                try:
                    archetype_hash = int(archetype_hash_text, 16)
                except ValueError:
                    archetype_hash = 0
                placement_name = str(record.get("model_name") or record.get("archetype_hash") or "")
                combined_matches = list(zone_resource_lookup.get(archetype_hash, []))
                combined_matches.extend(collect_gta4_lod_alias_matches(placement_name, zone_resource_name_lookup))
                combined_matches.extend(zone_resource_hash_lookup.get(archetype_hash, []))
                combined_matches = expand_gta4_sibling_name_matches(combined_matches, zone_resource_name_lookup)
                combined_matches = enrich_gta4_matches_with_geometry(combined_matches, resource_by_key, gta4_geometry_cache)
                selected_matches = select_best_gta4_resource_matches(placement_name, combined_matches)
                record["matched_resources"] = selected_matches
                for match in selected_matches:
                    try:
                        final_matched_resource_keys.add((str(match.get("source_path", "")), int(match.get("resource_index", -1))))
                    except (TypeError, ValueError):
                        continue
            wpl_records.append(decoded)
            dump_path = wpl_dir / f"{wpl_path.stem}.json"
            dump_path.write_text(json.dumps(decoded, indent=2, ensure_ascii=False), encoding="utf-8")
            wpl_dump_paths.append(str(dump_path))
            zone_index_path = zone_index_dir / f"{wpl_path.stem}.json"
            zone_index_path.write_text(json.dumps(zone_resource_index, indent=2, ensure_ascii=False), encoding="utf-8")
            linked_extract_dir = linked_resources_dir / wpl_path.stem
            extracted_resource_count = 0
            if zone_img_paths and zone_resource_index.get("resources"):
                for resource in zone_resources:
                    resource_key = (resource.source_path, resource.resource_index)
                    if resource_key not in final_matched_resource_keys:
                        continue
                    export_gta4_rsc_resource(resource, linked_extract_dir, effective_texture_output_format)
                    extracted_resource_count += 1
            linked_manifest = {
                "wpl_path": str(wpl_path),
                "img_path": zone_resource_index["img_path"],
                "img_paths": zone_resource_index["img_paths"],
                "extracted_dir": str(linked_extract_dir),
                "linked_resources": [
                    resource
                    for resource in zone_resource_index.get("resources", [])
                    if (
                        str(resource.get("source_path", "")),
                        int(resource.get("resource_index", -1)),
                    ) in final_matched_resource_keys
                ],
            }
            linked_manifest_path = linked_resources_dir / f"{wpl_path.stem}.json"
            linked_manifest_path.write_text(json.dumps(linked_manifest, indent=2, ensure_ascii=False), encoding="utf-8")
            preview_textures_dir: Optional[Path] = None
            if args.textures_dir:
                preview_textures_dir = Path(args.textures_dir).expanduser()
            elif args.extract_textures and linked_extract_dir.exists():
                preview_textures_dir = linked_extract_dir
            elif texture_export_summary is not None and args.texture_output_format in {"png", "both"}:
                preview_textures_dir = Path(str(texture_export_summary.get("textures_root", "")))
            preview_collection = collect_gta4_preview_zone_assets(
                output_dir=output_dir,
                zone_name=wpl_path.stem,
                decoded_records=decoded.get("records", []),
                linked_extract_dir=linked_extract_dir,
                textures_dir=preview_textures_dir,
            )
            if preview_collection is not None:
                preview_zone_collections.append(preview_collection)
            if export_zone_previews:
                preview_export = build_gta4_preview_zone_glb(
                    output_dir=output_dir,
                    zone_name=wpl_path.stem,
                    decoded_records=decoded.get("records", []),
                    linked_extract_dir=linked_extract_dir,
                    output_format=args.format,
                    double_sided=args.double_sided,
                    textures_dir=preview_textures_dir,
                )
                if preview_export is not None:
                    preview_zone_exports.append(preview_export)
                    print(
                        f"[ok] Zona preview {wpl_path.stem}: "
                        f"{preview_export['placements']} placements, {preview_export['models']} mallas -> {preview_export['file']}"
                    )
            zone_resource_indexes.append(
                {
                    "wpl_path": str(wpl_path),
                    "img_path": zone_resource_index["img_path"],
                    "index_path": str(zone_index_path),
                    "linked_manifest_path": str(linked_manifest_path),
                    "resources": len(zone_resource_index["resources"]),
                    "linked_resources": len(linked_manifest["linked_resources"]),
                    "extracted_resources": extracted_resource_count,
                }
            )
            for record in decoded.get("records", []):
                record_position = tuple(float(value) for value in record.get("position", [0.0, 0.0, 0.0]))
                record_quaternion = tuple(float(value) for value in record.get("quaternion_xyzw", [0.0, 0.0, 0.0, 1.0]))
                resolved_model_name = str(record.get("model_name") or record.get("archetype_hash") or "")
                wpl_placements.append(
                    Gta4Placement(
                        model_name=resolved_model_name,
                        position=record_position,
                        quaternion_xyzw=record_quaternion,
                        source_file=str(wpl_path),
                        source_kind="wpl_record",
                        extra={
                            "record_index": record.get("record_index", -1),
                            "archetype_hash": record.get("archetype_hash", ""),
                            "model_name_candidates": record.get("model_name_candidates", []),
                            "matched_resources": record.get("matched_resources", []),
                            "value_a": record.get("value_a", 0),
                            "value_b": record.get("value_b", 0),
                            "value_c": record.get("value_c", 0),
                        },
                    )
                )
    if wpl_headers:
        print(f"[info] WPL analizados: {len(wpl_headers)}")
    if wpl_placements:
        print(f"[info] Placements GTA4 binarios detectados: {len(wpl_placements)}")

    preview_sector_exports: List[Dict[str, object]] = []
    if args.sector_size > 0 and preview_zone_collections:
        preview_sector_exports = export_gta4_preview_sector_glbs(
            output_dir=output_dir,
            collected_zones=preview_zone_collections,
            sector_size=args.sector_size,
            double_sided=args.double_sided,
        )
        if preview_sector_exports:
            print(f"[ok] Sectores GTA4 generados: {len(preview_sector_exports)}")

    world_manifest_path = output_dir / "gta4_world_manifest.json"
    world_manifest = {
        "game": "gta4",
        "game_root": str(game_root),
        "dat": str(refs["dat"][0]) if refs["dat"] else dat_rel_path,
        "ide_files": [str(path) for path in refs["ide"]],
        "ipl_files": [str(path) for path in selected_ipl_paths],
        "wpl_files": wpl_headers,
        "wpl_records": wpl_records,
        "zone_resource_indexes": zone_resource_indexes,
        "zone_preview_exports": preview_zone_exports,
        "preview_sector_exports": preview_sector_exports,
        "texture_export_summary": texture_export_summary,
        "effective_texture_output_format": effective_texture_output_format,
        "image_archives": image_archives,
        "wpl_placements": [
            {
                "model_name": placement.model_name,
                "position": [float(v) for v in placement.position],
                "quaternion_xyzw": [float(v) for v in placement.quaternion_xyzw],
                "source_file": placement.source_file,
                "source_kind": placement.source_kind,
                "extra": placement.extra,
            }
            for placement in wpl_placements
        ],
        "text_placements": [
            {
                "model_name": placement.model_name,
                "position": [float(v) for v in placement.position],
                "quaternion_xyzw": [float(v) for v in placement.quaternion_xyzw],
                "source_file": placement.source_file,
                "source_kind": placement.source_kind,
                "extra": placement.extra,
            }
            for placement in placements
        ],
    }
    world_manifest_path.write_text(json.dumps(world_manifest, indent=2, ensure_ascii=False), encoding="utf-8")

    sector_index_path: Optional[Path] = None
    sector_source_placements = placements + wpl_placements
    if not args.skip_map and args.sector_size > 0:
        if preview_zone_collections:
            sector_index_path = build_gta4_sector_manifest_from_preview_collections(
                output_dir,
                preview_zone_collections,
                args.sector_size,
            )
        elif sector_source_placements:
            sector_index_path = build_gta4_sector_manifest(output_dir, sector_source_placements, args.sector_size)
    if sector_index_path is not None:
        print(f"[ok] Sectores metadata GTA4 generados: {sector_index_path}")

    export_summary = {
        "game": "gta4",
        "supports_glb_conversion": bool(preview_zone_exports or preview_sector_exports),
        "supports_resource_parsing": True,
        "supports_wtd_dds_extraction": True,
        "notes": [
            "GTA4 usa WDR/WDD/WTD/WPL; esta versión ya parsea recursos RSC5/IMG y extrae WTD simples a DDS.",
            "Los sectores GTA4 se escriben en assets/sectors/sec_X_Z.glb con coordenadas de mundo e index.json.",
            "Los preview por zona WPL son opcionales y sirven solo para depuración.",
            "La geometría GTA4 sigue siendo heurística: sirve para inspección y pruebas, pero todavía no reemplaza un parser completo de WDR/WDD.",
        ],
        "ide_files": len(refs["ide"]),
        "ipl_text_files": len(selected_ipl_paths),
        "wpl_files": len(selected_wpl_paths),
        "wpl_record_dumps": wpl_dump_paths,
        "zone_resource_indexes": zone_resource_indexes,
        "zone_preview_exports": preview_zone_exports,
        "preview_sector_exports": preview_sector_exports,
        "texture_export_summary": texture_export_summary,
        "effective_texture_output_format": effective_texture_output_format,
        "text_placements": len(placements),
        "wpl_placements": len(wpl_placements),
        "image_archives": len(image_archives),
        "category_exports": category_summaries,
        "copied_raw_files": copied_raw_files,
        "copied_animation_files": copied_anim_files,
        "parsed_resource_manifests": parsed_resource_manifests,
        "world_manifest": str(world_manifest_path),
        "sector_manifest": str(sector_index_path) if sector_index_path is not None else "",
    }
    summary_path = output_dir / "export_summary.json"
    summary_path.write_text(json.dumps(export_summary, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"[ok] Manifiesto mundo GTA4: {world_manifest_path}")
    print(f"[ok] Resumen escrito en: {summary_path}")


def run_gta3_export(args: argparse.Namespace) -> None:
    game_preset = GAME_EXPORT_PRESETS["gta3"]
    if args.extract_textures:
        print("[aviso] La extracción/conversión automática de texturas desde el juego todavía no está implementada para GTA3.")

    game_root = Path(args.game_root).expanduser()
    output_dir = Path(args.output_dir).expanduser()
    output_dir.mkdir(parents=True, exist_ok=True)
    extracted_root = Path(args.extracted_root).expanduser() if args.extracted_root else None
    extracted_models_dir = extracted_root / "models" if extracted_root else None
    textures_dir = Path(args.textures_dir).expanduser() if args.textures_dir else None
    textures_out_dir = output_dir / "textures"
    if textures_dir is None and extracted_root is not None:
        candidate = extracted_root / "txd"
        if candidate.exists():
            textures_dir = candidate
    if textures_dir is not None:
        textures_out_dir.mkdir(parents=True, exist_ok=True)

    dat_rel_path = args.dat or game_preset["default_dat"]
    ide_paths, dat_ipl_paths = parse_dat_references(game_root, dat_rel_path)
    if args.ipl:
        ipl_paths = []
        for entry in args.ipl:
            path = Path(entry).expanduser()
            if not path.is_absolute():
                path = resolve_case_path(game_root, entry)
            ipl_paths.append(path)
    else:
        ipl_paths = dat_ipl_paths

    object_defs: Dict[str, ObjectDef] = {}
    category_defs: Dict[str, List[ObjectDef]] = {name: [] for name in EXPORTABLE_CATEGORIES}
    for ide_path in ide_paths:
        object_defs.update(parse_ide_file(ide_path))
        parsed_categories = parse_export_categories(ide_path)
        for category_name, entries in parsed_categories.items():
            category_defs[category_name].extend(entries)

    placements: List[Placement] = []
    if not args.skip_map:
        for ipl_path in ipl_paths:
            placements.extend(parse_ipl_file(ipl_path, include_lod=args.include_lod))

    dir_path = resolve_case_path(game_root, str(game_preset["dir_rel"]))
    img_path = resolve_case_path(game_root, str(game_preset["img_rel"]))
    img_archive = ImgArchive(img_path, parse_dir_archive(dir_path))

    models: Dict[str, DffModel] = {}
    missing_models: List[str] = []
    scene_path: Optional[Path] = None

    if not args.skip_map:
        unique_model_names = sorted({placement.model_name for placement in placements}, key=str.lower)
        for index, model_name in enumerate(unique_model_names, start=1):
            data = collect_model_bytes(game_root, img_archive, model_name, extracted_models_dir)
            if data is None:
                missing_models.append(model_name)
                continue
            try:
                model = parse_dff_model(model_name, data)
                object_def = object_defs.get(model_name.lower())
                txd_name = object_def.txd_name if object_def is not None else None
                if textures_dir is not None:
                    for subset in model.subsets:
                        match = find_matching_texture(textures_dir, txd_name, subset.material_name)
                        if match is None:
                            continue
                        dest = textures_out_dir / match.name
                        if not dest.exists():
                            dest.write_bytes(match.read_bytes())
                        model.texture_paths[subset.material_name] = Path("textures") / match.name
                models[model_name.lower()] = model
            except Exception as exc:  # noqa: BLE001
                print(f"[aviso] No se pudo convertir {model_name}: {exc}")
                missing_models.append(model_name)
            if index % 100 == 0:
                print(f"[info] Modelos procesados: {index}/{len(unique_model_names)}")

        if args.sector_size > 0:
            if args.format != "glb":
                raise SystemExit("La exportacion por sectores esta pensada para --format glb")
            manifest = export_sector_glbs(
                output_dir,
                models,
                placements,
                args.sector_size,
                flip_uv_v=not args.no_flip_v,
                flip_uv_u=args.flip_u,
                double_sided=args.double_sided,
            )
            scene_path = output_dir / "assets" / "sectors"
            print(f"[ok] Sectores generados: {len(manifest)}")
        else:
            output_prefix = output_dir / str(game_preset["map_prefix"])
            if args.format == "glb":
                artifacts = build_scene_artifacts(
                    models,
                    placements,
                    output_dir,
                    binary_uri=None,
                    embed_images=True,
                    flip_uv_v=not args.no_flip_v,
                    flip_uv_u=args.flip_u,
                    double_sided=args.double_sided,
                )
                scene_path = write_glb_export(artifacts, output_prefix)
            else:
                artifacts = build_scene_artifacts(
                    models,
                    placements,
                    output_dir,
                    binary_uri=f"{game_preset['map_prefix']}.bin",
                    embed_images=False,
                    flip_uv_v=not args.no_flip_v,
                    flip_uv_u=args.flip_u,
                    double_sided=args.double_sided,
                )
                scene_path = write_gltf_export(artifacts, output_prefix)

    write_summary(output_dir, placements, object_defs, models, missing_models)

    category_summaries: List[Dict[str, object]] = []
    requested_categories = list(dict.fromkeys(args.export_category))
    if requested_categories:
        if args.format != "glb":
            raise SystemExit("La exportacion por categorias usa GLB para empaquetar cada recurso")
        textures_out_dir.mkdir(parents=True, exist_ok=True)
        for category_name in requested_categories:
            print(f"[info] Exportando categoria: {category_name}")
            summary = export_named_models(
                output_dir,
                category_name,
                category_defs[category_name],
                game_root,
                img_archive,
                extracted_models_dir,
                textures_dir,
                textures_out_dir,
                flip_uv_v=not args.no_flip_v,
                flip_uv_u=args.flip_u,
                double_sided=args.double_sided,
            )
            category_summaries.append(summary)
            print(f"[ok] {category_name}: {summary['exported']}/{summary['requested']} GLB")

    copied_ped_ifp = None
    if args.copy_ped_anims:
        copied_ped_ifp = copy_raw_ped_ifp(output_dir, game_root)
        if copied_ped_ifp is not None:
            print(f"[ok] Animacion cruda copiada en: {copied_ped_ifp}")
        else:
            print("[aviso] No se encontro anim/ped.ifp para copiarlo")

    export_summary_path = output_dir / "export_summary.json"
    summary_data = json.loads(export_summary_path.read_text(encoding="utf-8"))
    if category_summaries:
        summary_data["category_exports"] = category_summaries
    if copied_ped_ifp is not None:
        summary_data["raw_animation_copy"] = str(copied_ped_ifp)
    export_summary_path.write_text(json.dumps(summary_data, indent=2, ensure_ascii=False), encoding="utf-8")

    if scene_path is not None:
        print(f"[ok] Escena escrita en: {scene_path}")
    else:
        print("[ok] Mapa omitido por --skip-map")
    print(f"[ok] Resumen escrito en: {export_summary_path}")


def parse_ide_file(path: Path) -> Dict[str, ObjectDef]:
    result: Dict[str, ObjectDef] = {}
    section: Optional[str] = None
    for line in iter_clean_lines(path):
        lower = line.lower()
        if lower in {"objs", "tobj", "anim"}:
            section = lower
            continue
        if lower == "end":
            section = None
            continue
        if section not in {"objs", "tobj"}:
            continue
        fields = [field.strip() for field in line.split(",")]
        if len(fields) < 5:
            continue
        try:
            object_id = int(fields[0])
            model_name = fields[1]
            txd_name = fields[2]
            draw_distance = float(fields[-2])
            flags = int(fields[-1])
        except ValueError:
            continue
        result[model_name.lower()] = ObjectDef(
            object_id=object_id,
            model_name=model_name,
            txd_name=txd_name,
            draw_distance=draw_distance,
            flags=flags,
        )
    return result


def parse_export_categories(path: Path) -> Dict[str, List[ObjectDef]]:
    categories: Dict[str, List[ObjectDef]] = {name: [] for name in EXPORTABLE_CATEGORIES}
    section: Optional[str] = None

    for line in iter_clean_lines(path):
        lower = line.lower()
        if lower in {"cars", "peds", "objs", "tobj", "anim"}:
            section = lower
            continue
        if lower == "end":
            section = None
            continue

        fields = [field.strip() for field in line.split(",")]
        if len(fields) < 3:
            continue

        if section == "cars":
            try:
                object_id = int(fields[0])
            except ValueError:
                continue
            categories["vehicles"].append(
                ObjectDef(
                    object_id=object_id,
                    model_name=fields[1],
                    txd_name=fields[2],
                    draw_distance=0.0,
                    flags=0,
                )
            )
            continue

        if section == "peds":
            try:
                object_id = int(fields[0])
            except ValueError:
                continue
            categories["peds"].append(
                ObjectDef(
                    object_id=object_id,
                    model_name=fields[1],
                    txd_name=fields[2],
                    draw_distance=0.0,
                    flags=0,
                )
            )
            continue

        if section not in {"objs", "tobj"} or len(fields) < 5:
            continue

        try:
            object_id = int(fields[0])
            draw_distance = float(fields[-2])
            flags = int(fields[-1])
        except ValueError:
            continue

        object_def = ObjectDef(
            object_id=object_id,
            model_name=fields[1],
            txd_name=fields[2],
            draw_distance=draw_distance,
            flags=flags,
        )

        if 170 <= object_id <= 183:
            categories["weapons"].append(object_def)
        elif 160 <= object_id <= 169 or 184 <= object_id <= 399:
            categories["props"].append(object_def)

    return categories


def parse_ipl_file(path: Path, include_lod: bool) -> List[Placement]:
    placements: List[Placement] = []
    source_name = path.name.lower()
    if not include_lod and source_name == "overview.ipl":
        return placements
    section: Optional[str] = None
    for line in iter_clean_lines(path):
        lower = line.lower()
        if lower == "inst":
            section = "inst"
            continue
        if lower == "end":
            section = None
            continue
        if section != "inst":
            continue
        fields = [field.strip() for field in line.split(",")]
        if len(fields) < 12:
            continue
        model_name = fields[1]
        lowered_model = model_name.lower()
        if not include_lod and (lowered_model.startswith("lod") or lowered_model.startswith("islandlod")):
            continue
        try:
            object_id = int(fields[0])
            x, y, z = map(float, fields[2:5])
            sx, sy, sz = map(float, fields[5:8])
            qx, qy, qz, qw = map(float, fields[8:12])
        except ValueError:
            continue
        placements.append(
            Placement(
                object_id=object_id,
                model_name=model_name,
                position=(x, y, z),
                scale=(sx, sy, sz),
                quaternion_xyzw=(qx, qy, qz, qw),
                source_ipl=str(path),
            )
        )
    return placements


def parse_dir_archive(dir_path: Path) -> Dict[str, DirEntry]:
    entries: Dict[str, DirEntry] = {}
    with dir_path.open("rb") as handle:
        while True:
            chunk = handle.read(32)
            if not chunk:
                break
            if len(chunk) != 32:
                raise RuntimeError(f"Entrada DIR incompleta en {dir_path}")
            offset_sectors, size_sectors = struct.unpack_from("<II", chunk, 0)
            name = chunk[8:].split(b"\0", 1)[0].decode("ascii", errors="ignore")
            entries[name.lower()] = DirEntry(offset_sectors, size_sectors, name)
    return entries


class ImgArchive:
    def __init__(self, img_path: Path, entries: Dict[str, DirEntry]) -> None:
        self.img_path = img_path
        self.entries = entries

    def read_file(self, filename: str) -> bytes:
        entry = self.entries.get(filename.lower())
        if entry is None:
            raise FileNotFoundError(f"No existe {filename} dentro de {self.img_path.name}")
        with self.img_path.open("rb") as handle:
            handle.seek(entry.offset_sectors * SECTOR_SIZE)
            return handle.read(entry.size_sectors * SECTOR_SIZE)


def extract_rw_string(data: bytes, offset: int) -> str:
    chunk_id, size, _, payload = read_chunk_header(data, offset)
    if chunk_id != CHUNK_STRING:
        return ""
    raw = data[payload : payload + size]
    return raw.split(b"\0", 1)[0].decode("latin-1", errors="ignore")


def parse_texture_name(texture_chunk: bytes) -> str:
    offset = 0
    chunk_id, size, _, payload = read_chunk_header(texture_chunk, offset)
    if chunk_id != CHUNK_TEXTURE:
        return "material"
    offset = payload

    struct_id, struct_size, _, struct_payload = read_chunk_header(texture_chunk, offset)
    if struct_id != CHUNK_STRUCT:
        raise RWParseError("Chunk Texture sin Struct")
    offset = next_sibling_offset(struct_payload, struct_size)

    name = "material"
    if offset + 12 <= len(texture_chunk):
        name = extract_rw_string(texture_chunk, offset) or name
    return name


def parse_material_list(data: bytes, offset: int) -> List[MaterialInfo]:
    chunk_id, size, _, payload = read_chunk_header(data, offset)
    if chunk_id != CHUNK_MATERIAL_LIST:
        raise RWParseError("Se esperaba MaterialList")

    struct_id, struct_size, _, struct_payload = read_chunk_header(data, payload)
    if struct_id != CHUNK_STRUCT:
        raise RWParseError("MaterialList sin Struct")
    material_count = read_u32(data, struct_payload)

    cursor = next_sibling_offset(struct_payload, struct_size)
    materials: List[MaterialInfo] = []
    for index in range(material_count):
        child_id, child_size, _, child_payload = read_chunk_header(data, cursor)
        if child_id != CHUNK_MATERIAL:
            raise RWParseError(f"Material #{index} invalido")

        sub_cursor = child_payload
        struct2_id, struct2_size, _, struct2_payload = read_chunk_header(data, sub_cursor)
        if struct2_id != CHUNK_STRUCT:
            raise RWParseError("Material sin Struct")
        rgba = tuple(data[struct2_payload + 4 : struct2_payload + 8])
        sub_cursor = next_sibling_offset(struct2_payload, struct2_size)

        material_name = f"material_{index}"
        if sub_cursor + 12 <= cursor + 12 + child_size:
            maybe_texture_id, _, _, _ = read_chunk_header(data, sub_cursor)
            if maybe_texture_id == CHUNK_TEXTURE:
                texture_chunk = data[sub_cursor : sub_cursor + 12 + read_u32(data, sub_cursor + 4)]
                material_name = parse_texture_name(texture_chunk) or material_name

        materials.append(MaterialInfo(name=material_name, rgba=rgba))  # type: ignore[arg-type]
        cursor = next_sibling_offset(child_payload, child_size)

    return materials


def parse_frame_list(data: bytes, offset: int) -> List[np.ndarray]:
    chunk_id, size, _, payload = read_chunk_header(data, offset)
    if chunk_id != CHUNK_FRAME_LIST:
        raise RWParseError("Se esperaba FrameList")
    struct_id, struct_size, _, struct_payload = read_chunk_header(data, payload)
    if struct_id != CHUNK_STRUCT:
        raise RWParseError("FrameList sin Struct")

    frame_count = read_u32(data, struct_payload)
    cursor = struct_payload + 4
    local_matrices: List[np.ndarray] = []
    parents: List[int] = []
    for _ in range(frame_count):
        floats = struct.unpack_from("<12f", data, cursor)
        cursor += 48
        parent = read_i32(data, cursor)
        cursor += 4
        _matrix_flags = read_u32(data, cursor)
        cursor += 4

        right = np.array(floats[0:3], dtype=np.float32)
        up = np.array(floats[3:6], dtype=np.float32)
        at = np.array(floats[6:9], dtype=np.float32)
        pos = np.array(floats[9:12], dtype=np.float32)

        local = np.eye(4, dtype=np.float32)
        local[0:3, 0] = right
        local[0:3, 1] = up
        local[0:3, 2] = at
        local[0:3, 3] = pos
        local_matrices.append(local)
        parents.append(parent)

    world_matrices: List[np.ndarray] = [np.eye(4, dtype=np.float32) for _ in range(frame_count)]
    for index, local in enumerate(local_matrices):
        parent = parents[index]
        if parent >= 0:
            world_matrices[index] = world_matrices[parent] @ local
        else:
            world_matrices[index] = local
    return world_matrices


def parse_atomic(data: bytes, offset: int) -> AtomicRef:
    chunk_id, size, _, payload = read_chunk_header(data, offset)
    if chunk_id != CHUNK_ATOMIC:
        raise RWParseError("Se esperaba Atomic")
    struct_id, struct_size, _, struct_payload = read_chunk_header(data, payload)
    if struct_id != CHUNK_STRUCT or struct_size < 16:
        raise RWParseError("Atomic sin Struct valido")
    frame_index, geometry_index, flags, _unused = struct.unpack_from("<IIII", data, struct_payload)
    return AtomicRef(frame_index=frame_index, geometry_index=geometry_index, flags=flags)


def parse_geometry(data: bytes, offset: int) -> GeometryChunk:
    chunk_id, size, _, payload = read_chunk_header(data, offset)
    if chunk_id != CHUNK_GEOMETRY:
        raise RWParseError("Se esperaba Geometry")
    struct_id, struct_size, _, struct_payload = read_chunk_header(data, payload)
    if struct_id != CHUNK_STRUCT:
        raise RWParseError("Geometry sin Struct")

    cursor = struct_payload
    format_flags = read_u32(data, cursor)
    cursor += 4
    triangle_count = read_u32(data, cursor)
    cursor += 4
    vertex_count = read_u32(data, cursor)
    cursor += 4
    morph_target_count = read_u32(data, cursor)
    cursor += 4

    if format_flags & GEOM_FLAG_NATIVE:
        raise RWParseError("La geometria usa formato native y no esta soportada")

    uv_set_count = (format_flags >> 8) & 0xFF
    if uv_set_count == 0:
        if format_flags & GEOM_FLAG_TEXTURED2:
            uv_set_count = 2
        elif format_flags & GEOM_FLAG_TEXTURED:
            uv_set_count = 1

    has_prelit = bool(format_flags & GEOM_FLAG_PRELIT)
    has_normals = bool(format_flags & GEOM_FLAG_NORMALS)

    cursor += 12  # surface properties

    if has_prelit:
        cursor += vertex_count * 4

    uv_sets: List[np.ndarray] = []
    for _ in range(uv_set_count):
        values = np.frombuffer(data, dtype="<f4", count=vertex_count * 2, offset=cursor).copy()
        uv_sets.append(values.reshape(vertex_count, 2))
        cursor += vertex_count * 8

    tri_raw = np.frombuffer(data, dtype="<u2", count=triangle_count * 4, offset=cursor).copy()
    cursor += triangle_count * 8
    tri_raw = tri_raw.reshape(triangle_count, 4)

    material_list_offset = next_sibling_offset(struct_payload, struct_size)
    materials = parse_material_list(data, material_list_offset)
    extension_offset = material_list_offset + 12 + read_u32(data, material_list_offset + 4)
    _extension_chunk_id, _extension_size, _, _extension_payload = read_chunk_header(data, extension_offset)

    positions: Optional[np.ndarray] = None
    normals: Optional[np.ndarray] = None
    for morph_index in range(morph_target_count):
        _sphere = np.frombuffer(data, dtype="<f4", count=4, offset=cursor).copy()
        cursor += 16
        has_vertices = read_u32(data, cursor)
        cursor += 4
        has_morph_normals = read_u32(data, cursor)
        cursor += 4

        morph_positions = None
        morph_normals = None
        if has_vertices:
            morph_positions = np.frombuffer(data, dtype="<f4", count=vertex_count * 3, offset=cursor).copy()
            morph_positions = morph_positions.reshape(vertex_count, 3)
            cursor += vertex_count * 12
        if has_morph_normals:
            morph_normals = np.frombuffer(data, dtype="<f4", count=vertex_count * 3, offset=cursor).copy()
            morph_normals = morph_normals.reshape(vertex_count, 3)
            cursor += vertex_count * 12

        if morph_index == 0:
            positions = morph_positions
            normals = morph_normals

    if positions is None:
        raise RWParseError("Geometry sin posiciones")
    if has_normals and normals is None:
        raise RWParseError("Geometry marca normales pero no las contiene")

    # RpTriangle guarda indices en un orden peculiar; este mapeo respeta el winding esperado.
    triangle_materials = tri_raw[:, 2].astype(np.int32)
    indices = np.stack([tri_raw[:, 1], tri_raw[:, 0], tri_raw[:, 3]], axis=1).astype(np.uint32)
    subsets: List[GeometrySubset] = []
    if len(materials) == 0:
        subsets.append(GeometrySubset(indices=indices, material_name="material"))
    else:
        for material_index, material in enumerate(materials):
            mask = triangle_materials == material_index
            if not np.any(mask):
                continue
            subsets.append(GeometrySubset(indices=indices[mask].copy(), material_name=material.name))
        if not subsets:
            subsets.append(GeometrySubset(indices=indices, material_name=materials[0].name))

    uvs = uv_sets[0] if uv_sets else None
    return GeometryChunk(
        positions=positions.astype(np.float32),
        normals=normals.astype(np.float32) if normals is not None else None,
        uvs=uvs.astype(np.float32) if uvs is not None else None,
        subsets=subsets,
    )


def parse_dff_model(name: str, data: bytes) -> DffModel:
    root_id, root_size, _, root_payload = read_chunk_header(data, 0)
    if root_id != CHUNK_CLUMP:
        raise RWParseError("El archivo no empieza con un Clump")

    cursor = root_payload
    child_id, child_size, _, child_payload = read_chunk_header(data, cursor)
    if child_id != CHUNK_STRUCT:
        raise RWParseError("Clump sin Struct")
    cursor = next_sibling_offset(child_payload, child_size)

    frames = parse_frame_list(data, cursor)
    cursor = cursor + 12 + read_u32(data, cursor + 4)

    geom_list_id, geom_list_size, _, geom_list_payload = read_chunk_header(data, cursor)
    if geom_list_id != CHUNK_GEOMETRY_LIST:
        raise RWParseError("Clump sin GeometryList")
    geom_struct_id, geom_struct_size, _, geom_struct_payload = read_chunk_header(data, geom_list_payload)
    if geom_struct_id != CHUNK_STRUCT:
        raise RWParseError("GeometryList sin Struct")
    geometry_count = read_u32(data, geom_struct_payload)

    geom_cursor = next_sibling_offset(geom_struct_payload, geom_struct_size)
    geometries: List[GeometryChunk] = []
    for _ in range(geometry_count):
        geometry = parse_geometry(data, geom_cursor)
        geometries.append(geometry)
        geom_cursor = geom_cursor + 12 + read_u32(data, geom_cursor + 4)
    cursor = next_sibling_offset(geom_list_payload, geom_list_size)

    atomics: List[AtomicRef] = []
    while cursor < root_payload + root_size:
        chunk_id, size, _, _payload = read_chunk_header(data, cursor)
        if chunk_id == CHUNK_ATOMIC:
            atomics.append(parse_atomic(data, cursor))
        cursor = cursor + 12 + size

    if not atomics:
        raise RWParseError("El modelo no contiene atomics")

    merged_positions: List[np.ndarray] = []
    merged_normals: List[np.ndarray] = []
    merged_uvs: List[np.ndarray] = []
    merged_subsets: Dict[str, List[np.ndarray]] = {}
    vertex_base = 0

    for atomic in atomics:
        geometry = geometries[atomic.geometry_index]
        frame_matrix = frames[atomic.frame_index]
        transform = frame_matrix[0:3, 0:3]
        translation = frame_matrix[0:3, 3]

        positions = (geometry.positions @ transform.T) + translation
        merged_positions.append(positions.astype(np.float32))

        if geometry.normals is not None:
            normal_matrix = np.linalg.inv(transform).T
            normals = geometry.normals @ normal_matrix.T
            lengths = np.linalg.norm(normals, axis=1, keepdims=True)
            lengths[lengths == 0.0] = 1.0
            normals = normals / lengths
            merged_normals.append(normals.astype(np.float32))

        if geometry.uvs is not None:
            merged_uvs.append(geometry.uvs.astype(np.float32))

        for subset in geometry.subsets:
            merged_subsets.setdefault(subset.material_name, []).append(
                (subset.indices + vertex_base).astype(np.uint32)
            )
        vertex_base += len(geometry.positions)

    positions = np.vstack(merged_positions)
    normals = np.vstack(merged_normals) if merged_normals else None
    uvs = np.vstack(merged_uvs) if merged_uvs else None
    subsets: List[GeometrySubset] = []
    for material_name, subset_list in merged_subsets.items():
        subsets.append(GeometrySubset(indices=np.vstack(subset_list), material_name=material_name))

    positions = positions @ BASIS_GTA_TO_GLTF.T
    if normals is not None:
        normals = normals @ BASIS_GTA_TO_GLTF.T

    return DffModel(
        name=name,
        positions=positions.astype(np.float32),
        normals=normals.astype(np.float32) if normals is not None else None,
        uvs=uvs.astype(np.float32) if uvs is not None else None,
        subsets=subsets,
        texture_paths={},
    )


def quaternion_to_matrix_xyzw(qx: float, qy: float, qz: float, qw: float) -> np.ndarray:
    norm = math.sqrt(qx * qx + qy * qy + qz * qz + qw * qw)
    if norm == 0.0:
        return np.eye(3, dtype=np.float32)
    x = qx / norm
    y = qy / norm
    z = qz / norm
    w = qw / norm

    return np.array(
        [
            [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
            [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
            [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)],
        ],
        dtype=np.float32,
    )


def placement_matrix(placement: Placement) -> np.ndarray:
    rotation_gta = quaternion_to_matrix_xyzw(*placement.quaternion_xyzw)
    scale = np.diag(np.array(placement.scale, dtype=np.float32))
    rotation_scaled_gta = rotation_gta @ scale
    rotation_scaled_gltf = BASIS_GTA_TO_GLTF @ rotation_scaled_gta @ BASIS_GLTF_TO_GTA

    translation_gta = np.array(placement.position, dtype=np.float32)
    translation_gltf = BASIS_GTA_TO_GLTF @ translation_gta

    matrix = np.eye(4, dtype=np.float32)
    matrix[0:3, 0:3] = rotation_scaled_gltf
    matrix[0:3, 3] = translation_gltf
    return matrix


class BinWriter:
    def __init__(self) -> None:
        self.data = bytearray()

    def append(self, blob: bytes, alignment: int = 4) -> Tuple[int, int]:
        while len(self.data) % alignment:
            self.data.append(0)
        offset = len(self.data)
        self.data.extend(blob)
        return offset, len(blob)


def float_list(values: np.ndarray) -> List[float]:
    return [float(value) for value in values.tolist()]


def pad_bytes(data: bytes, alignment: int, pad_byte: bytes = b"\x00") -> bytes:
    remainder = len(data) % alignment
    if remainder == 0:
        return data
    return data + (pad_byte * (alignment - remainder))


def find_matching_texture(textures_dir: Optional[Path], txd_name: Optional[str], material_name: str) -> Optional[Path]:
    if textures_dir is None or not textures_dir.exists():
        return None

    wanted = []
    if txd_name:
        wanted.append(f"{txd_name}_{material_name}.png".lower())
    wanted.append(f"{material_name}.png".lower())

    exact_matches: Dict[str, Path] = {}
    suffix_matches: List[Path] = []
    for path in textures_dir.iterdir():
        if not path.is_file():
            continue
        lowered = path.name.lower()
        exact_matches[lowered] = path
        if lowered.endswith(f"_{material_name.lower()}.png"):
            suffix_matches.append(path)

    for key in wanted:
        match = exact_matches.get(key)
        if match is not None:
            return match

    if suffix_matches:
        if txd_name:
            prefix = f"{txd_name.lower()}_"
            for path in suffix_matches:
                if path.name.lower().startswith(prefix):
                    return path
        return suffix_matches[0]

    return None


def build_scene_artifacts(models: Dict[str, DffModel], placements: Sequence[Placement], output_dir: Path,
                          binary_uri: Optional[str], embed_images: bool,
                          flip_uv_v: bool = True, flip_uv_u: bool = False,
                          double_sided: bool = False) -> ExportArtifacts:
    bin_writer = BinWriter()

    gltf: Dict[str, object] = {
        "asset": {"version": "2.0", "generator": "gta3_map_exporter"},
        "scene": 0,
        "scenes": [{"nodes": []}],
        "nodes": [],
        "meshes": [],
        "materials": [],
        "images": [],
        "textures": [],
        "buffers": [],
        "bufferViews": [],
        "accessors": [],
    }

    materials: List[Dict[str, object]] = gltf["materials"]  # type: ignore[assignment]
    images: List[Dict[str, object]] = gltf["images"]  # type: ignore[assignment]
    textures: List[Dict[str, object]] = gltf["textures"]  # type: ignore[assignment]
    meshes: List[Dict[str, object]] = gltf["meshes"]  # type: ignore[assignment]
    nodes: List[Dict[str, object]] = gltf["nodes"]  # type: ignore[assignment]
    buffer_views: List[Dict[str, object]] = gltf["bufferViews"]  # type: ignore[assignment]
    accessors: List[Dict[str, object]] = gltf["accessors"]  # type: ignore[assignment]
    scene_nodes: List[int] = gltf["scenes"][0]["nodes"]  # type: ignore[index]

    material_index_by_name: Dict[str, int] = {}
    image_index_by_uri: Dict[str, int] = {}
    texture_index_by_uri: Dict[str, int] = {}
    mesh_index_by_model: Dict[str, int] = {}
    warned_missing_textures: set[str] = set()

    def add_buffer_view(offset: int, length: int, target: Optional[int]) -> int:
        view: Dict[str, object] = {
            "buffer": 0,
            "byteOffset": offset,
            "byteLength": length,
        }
        if target is not None:
            view["target"] = target
        buffer_views.append(view)
        return len(buffer_views) - 1

    def add_accessor(buffer_view: int, component_type: int, count: int, accessor_type: str,
                     minimum: Optional[List[float]] = None, maximum: Optional[List[float]] = None) -> int:
        accessor: Dict[str, object] = {
            "bufferView": buffer_view,
            "componentType": component_type,
            "count": count,
            "type": accessor_type,
        }
        if minimum is not None:
            accessor["min"] = minimum
        if maximum is not None:
            accessor["max"] = maximum
        accessors.append(accessor)
        return len(accessors) - 1

    def ensure_material(material_name: str, texture_path: Optional[Path]) -> int:
        if material_name not in material_index_by_name:
            base_material: Dict[str, object] = {
                "name": material_name,
                "pbrMetallicRoughness": {
                    "baseColorFactor": [0.8, 0.8, 0.8, 1.0],
                    "metallicFactor": 0.0,
                    "roughnessFactor": 1.0,
                },
                "doubleSided": double_sided,
            }
            if texture_path is not None:
                texture_uri = texture_path.as_posix()
                texture_key = texture_uri
                abs_texture = output_dir / texture_path
                if not embed_images or abs_texture.exists():
                    if embed_images:
                        image_bytes = abs_texture.read_bytes()
                        image_offset, image_length = bin_writer.append(image_bytes, alignment=4)
                        image_view = add_buffer_view(image_offset, image_length, None)
                        mime_type = mimetypes.guess_type(abs_texture.name)[0] or "image/png"
                        texture_key = f"embedded:{texture_uri}"
                        if texture_key not in image_index_by_uri:
                            image_index_by_uri[texture_key] = len(images)
                            images.append({"bufferView": image_view, "mimeType": mime_type, "name": abs_texture.name})
                    else:
                        if texture_key not in image_index_by_uri:
                            image_index_by_uri[texture_key] = len(images)
                            images.append({"uri": texture_uri})

                    if texture_key not in texture_index_by_uri:
                        texture_index_by_uri[texture_key] = len(textures)
                        textures.append({"source": image_index_by_uri[texture_key]})
                    base_material["pbrMetallicRoughness"]["baseColorTexture"] = {  # type: ignore[index]
                        "index": texture_index_by_uri[texture_key]
                    }
                elif texture_uri not in warned_missing_textures:
                    warned_missing_textures.add(texture_uri)
                    print(f"[aviso] Textura ausente, se exporta sin ella: {abs_texture}")

            material_index_by_name[material_name] = len(materials)
            materials.append(base_material)
        return material_index_by_name[material_name]

    for model_name, model in models.items():
        pos_bytes = model.positions.astype("<f4").tobytes()
        pos_off, pos_len = bin_writer.append(pos_bytes, alignment=4)
        pos_view = add_buffer_view(pos_off, pos_len, 34962)
        pos_accessor = add_accessor(
            pos_view,
            5126,
            len(model.positions),
            "VEC3",
            minimum=float_list(model.positions.min(axis=0)),
            maximum=float_list(model.positions.max(axis=0)),
        )

        normal_accessor = None
        if model.normals is not None:
            nrm_bytes = model.normals.astype("<f4").tobytes()
            nrm_off, nrm_len = bin_writer.append(nrm_bytes, alignment=4)
            nrm_view = add_buffer_view(nrm_off, nrm_len, 34962)
            normal_accessor = add_accessor(nrm_view, 5126, len(model.normals), "VEC3")

        uv_accessor = None
        if model.uvs is not None:
            uv = model.uvs.copy()
            if flip_uv_u:
                uv[:, 0] = 1.0 - uv[:, 0]
            if flip_uv_v:
                uv[:, 1] = 1.0 - uv[:, 1]
            uv_bytes = uv.astype("<f4").tobytes()
            uv_off, uv_len = bin_writer.append(uv_bytes, alignment=4)
            uv_view = add_buffer_view(uv_off, uv_len, 34962)
            uv_accessor = add_accessor(uv_view, 5126, len(uv), "VEC2")

        attributes: Dict[str, int] = {"POSITION": pos_accessor}
        if normal_accessor is not None:
            attributes["NORMAL"] = normal_accessor
        if uv_accessor is not None:
            attributes["TEXCOORD_0"] = uv_accessor

        primitives: List[Dict[str, object]] = []
        for subset in model.subsets:
            material_name = subset.material_name or "material"
            material_index = ensure_material(material_name, model.texture_paths.get(material_name))
            flat_indices = subset.indices[:, ::-1].reshape(-1).astype("<u4")
            idx_bytes = flat_indices.tobytes()
            idx_off, idx_len = bin_writer.append(idx_bytes, alignment=4)
            idx_view = add_buffer_view(idx_off, idx_len, 34963)
            idx_accessor = add_accessor(idx_view, 5125, flat_indices.size, "SCALAR")
            primitives.append(
                {
                    "attributes": attributes,
                    "indices": idx_accessor,
                    "material": material_index,
                    "mode": 4,
                }
            )

        mesh = {
            "name": model_name,
            "primitives": primitives,
        }
        mesh_index_by_model[model_name.lower()] = len(meshes)
        meshes.append(mesh)

    missing_meshes = 0
    for placement in placements:
        mesh_index = mesh_index_by_model.get(placement.model_name.lower())
        if mesh_index is None:
            missing_meshes += 1
            continue
        matrix = placement_matrix(placement)
        node = {
            "name": f"{placement.model_name}_{len(nodes)}",
            "mesh": mesh_index,
            "matrix": [float(value) for value in matrix.T.reshape(-1)],
        }
        nodes.append(node)
        scene_nodes.append(len(nodes) - 1)

    if missing_meshes:
        print(f"[aviso] {missing_meshes} instancias se omitieron porque no se encontro su malla.")

    buffer_entry: Dict[str, object] = {"byteLength": len(bin_writer.data)}
    if binary_uri is not None:
        buffer_entry["uri"] = binary_uri
    gltf["buffers"].append(buffer_entry)

    return ExportArtifacts(gltf=gltf, binary_blob=bytes(bin_writer.data))


def write_gltf_export(artifacts: ExportArtifacts, output_prefix: Path) -> Path:
    bin_path = output_prefix.with_suffix(".bin")
    gltf_path = output_prefix.with_suffix(".gltf")
    bin_path.write_bytes(artifacts.binary_blob)
    gltf_path.write_text(json.dumps(artifacts.gltf, indent=2), encoding="utf-8")
    return gltf_path


def write_glb_export(artifacts: ExportArtifacts, output_prefix: Path) -> Path:
    glb_path = output_prefix.with_suffix(".glb")
    json_chunk = json.dumps(artifacts.gltf, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    json_chunk = pad_bytes(json_chunk, 4, b" ")
    bin_chunk = pad_bytes(artifacts.binary_blob, 4, b"\x00")

    total_length = 12 + 8 + len(json_chunk) + 8 + len(bin_chunk)
    glb = bytearray()
    glb.extend(struct.pack("<III", 0x46546C67, 2, total_length))
    glb.extend(struct.pack("<I4s", len(json_chunk), b"JSON"))
    glb.extend(json_chunk)
    glb.extend(struct.pack("<I4s", len(bin_chunk), b"BIN\x00"))
    glb.extend(bin_chunk)
    glb_path.write_bytes(glb)
    return glb_path


def write_summary(output_dir: Path, placements: Sequence[Placement], object_defs: Dict[str, ObjectDef],
                  models: Dict[str, DffModel], missing_models: Sequence[str]) -> None:
    summary = {
        "placements_total": len(placements),
        "object_defs_total": len(object_defs),
        "unique_models_exported": len(models),
        "textured_models": sum(1 for model in models.values() if model.texture_paths),
        "missing_models": sorted(set(missing_models)),
    }
    (output_dir / "export_summary.json").write_text(json.dumps(summary, indent=2, ensure_ascii=False), encoding="utf-8")


def sector_index_for_placement(placement: Placement, sector_size: float) -> Tuple[int, int]:
    matrix = placement_matrix(placement)
    x = float(matrix[0, 3])
    z = float(matrix[2, 3])
    return math.floor(x / sector_size), math.floor(z / sector_size)


def group_placements_by_sector(placements: Sequence[Placement], sector_size: float) -> Dict[Tuple[int, int], List[Placement]]:
    grouped: Dict[Tuple[int, int], List[Placement]] = {}
    for placement in placements:
        sector_index = sector_index_for_placement(placement, sector_size)
        grouped.setdefault(sector_index, []).append(placement)
    return grouped


def bake_model_with_placement(model: DffModel, placement: Placement, baked_name: str) -> DffModel:
    matrix = placement_matrix(placement)
    transform = matrix[0:3, 0:3]
    translation = matrix[0:3, 3]

    positions = (model.positions @ transform.T) + translation

    normals = None
    if model.normals is not None:
        normal_matrix = np.linalg.inv(transform).T
        normals = model.normals @ normal_matrix.T
        lengths = np.linalg.norm(normals, axis=1, keepdims=True)
        lengths[lengths == 0.0] = 1.0
        normals = (normals / lengths).astype(np.float32)

    subsets = [GeometrySubset(indices=subset.indices.copy(), material_name=subset.material_name) for subset in model.subsets]
    texture_paths = dict(model.texture_paths)

    return DffModel(
        name=baked_name,
        positions=positions.astype(np.float32),
        normals=normals.astype(np.float32) if normals is not None else None,
        uvs=model.uvs.copy() if model.uvs is not None else None,
        subsets=subsets,
        texture_paths=texture_paths,
    )


def make_identity_placement(model_name: str) -> Placement:
    return Placement(
        object_id=0,
        model_name=model_name,
        position=(0.0, 0.0, 0.0),
        scale=(1.0, 1.0, 1.0),
        quaternion_xyzw=(0.0, 0.0, 0.0, 1.0),
        source_ipl="baked",
    )


def export_sector_glbs(output_dir: Path, models: Dict[str, DffModel], placements: Sequence[Placement], sector_size: float,
                       flip_uv_v: bool, flip_uv_u: bool, double_sided: bool) -> List[Dict[str, object]]:
    sectors_dir = output_dir / "assets" / "sectors"
    sectors_dir.mkdir(parents=True, exist_ok=True)
    grouped = group_placements_by_sector(placements, sector_size)
    manifest: List[Dict[str, object]] = []

    for (sector_x, sector_z), sector_placements in sorted(grouped.items()):
        baked_models: Dict[str, DffModel] = {}
        baked_placements: List[Placement] = []
        for index, placement in enumerate(sector_placements):
            source_model = models.get(placement.model_name.lower())
            if source_model is None:
                continue
            baked_name = f"{placement.model_name}_{index}"
            baked_models[baked_name.lower()] = bake_model_with_placement(source_model, placement, baked_name)
            baked_placements.append(make_identity_placement(baked_name))

        if not baked_models:
            continue

        output_prefix = sectors_dir / f"sec_{sector_x}_{sector_z}"
        artifacts = build_scene_artifacts(
            baked_models,
            baked_placements,
            output_dir,
            binary_uri=None,
            embed_images=True,
            flip_uv_v=flip_uv_v,
            flip_uv_u=flip_uv_u,
            double_sided=double_sided,
        )
        scene_path = write_glb_export(artifacts, output_prefix)
        manifest.append(
            {
                "sector_x": sector_x,
                "sector_z": sector_z,
                "file": scene_path.name,
                "placements": len(baked_placements),
                "models": len(baked_models),
            }
        )

    (sectors_dir / "index.json").write_text(
        json.dumps({"sector_size": sector_size, "sectors": manifest}, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )
    return manifest


def export_named_models(output_dir: Path, category_name: str, object_defs: Sequence[ObjectDef], game_root: Path,
                        img_archive: ImgArchive, extracted_models_dir: Optional[Path], textures_dir: Optional[Path],
                        textures_out_dir: Path, flip_uv_v: bool, flip_uv_u: bool,
                        double_sided: bool) -> Dict[str, object]:
    category_dir = output_dir / "assets" / category_name
    category_dir.mkdir(parents=True, exist_ok=True)

    exported = 0
    missing: List[str] = []
    failed: List[str] = []

    unique_defs: Dict[str, ObjectDef] = {}
    for object_def in object_defs:
        unique_defs.setdefault(object_def.model_name.lower(), object_def)

    for index, object_def in enumerate(sorted(unique_defs.values(), key=lambda item: item.model_name.lower()), start=1):
        model_name = object_def.model_name
        data = collect_model_bytes(game_root, img_archive, model_name, extracted_models_dir)
        if data is None:
            missing.append(model_name)
            continue

        try:
            model = parse_dff_model(model_name, data)
            if textures_dir is not None:
                for subset in model.subsets:
                    match = find_matching_texture(textures_dir, object_def.txd_name, subset.material_name)
                    if match is None:
                        continue
                    dest = textures_out_dir / match.name
                    if not dest.exists():
                        dest.write_bytes(match.read_bytes())
                    model.texture_paths[subset.material_name] = Path("textures") / match.name

            artifacts = build_scene_artifacts(
                {model_name.lower(): model},
                [make_identity_placement(model_name)],
                output_dir,
                binary_uri=None,
                embed_images=True,
                flip_uv_v=flip_uv_v,
                flip_uv_u=flip_uv_u,
                double_sided=double_sided,
            )
            write_glb_export(artifacts, category_dir / model_name)
            exported += 1
        except Exception as exc:  # noqa: BLE001
            print(f"[aviso] No se pudo exportar {category_name}/{model_name}: {exc}")
            failed.append(model_name)

        if index % 50 == 0:
            print(f"[info] {category_name}: {index}/{len(unique_defs)}")

    shared_exports = export_shared_category_models(
        output_dir,
        category_name,
        game_root,
        textures_dir,
        textures_out_dir,
        flip_uv_v,
        flip_uv_u,
        double_sided,
    )

    return {
        "category": category_name,
        "requested": len(unique_defs),
        "exported": exported,
        "shared_exports": shared_exports,
        "missing": sorted(missing),
        "failed": sorted(failed),
    }


def copy_raw_ped_ifp(output_dir: Path, game_root: Path) -> Optional[Path]:
    try:
        ped_ifp = resolve_case_path(game_root, "anim/ped.ifp")
    except FileNotFoundError:
        return None

    animations_dir = output_dir / "assets" / "animations_raw"
    animations_dir.mkdir(parents=True, exist_ok=True)
    dest = animations_dir / "ped.ifp"
    dest.write_bytes(ped_ifp.read_bytes())
    return dest


def collect_model_bytes(game_root: Path, img_archive: ImgArchive, model_name: str,
                        extracted_models_dir: Optional[Path]) -> Optional[bytes]:
    resolved_model_name = MODEL_NAME_ALIASES.get(model_name.lower(), model_name)

    if extracted_models_dir is not None and extracted_models_dir.exists():
        lower_target = f"{resolved_model_name}.dff".lower()
        for path in extracted_models_dir.iterdir():
            if path.is_file() and path.name.lower() == lower_target:
                return path.read_bytes()
        for path in extracted_models_dir.rglob("*"):
            if path.is_file() and path.name.lower() == lower_target:
                return path.read_bytes()

    filename = f"{resolved_model_name}.dff"
    try:
        return img_archive.read_file(filename)
    except FileNotFoundError:
        pass

    generic_rel = f"models/Generic/{filename}"
    try:
        generic_path = resolve_case_path(game_root, generic_rel)
    except FileNotFoundError:
        shared_rel: Optional[str] = None
        lowered = model_name.lower()
        if lowered in WEAPON_MODEL_NAMES:
            shared_rel = "models/Generic/weapons.dff"
        elif lowered.startswith("wheel_"):
            shared_rel = "models/Generic/wheels.DFF"
        if shared_rel is None:
            return None
        generic_path = resolve_case_path(game_root, shared_rel)
    return generic_path.read_bytes()


def export_shared_category_models(output_dir: Path, category_name: str, game_root: Path,
                                  textures_dir: Optional[Path], textures_out_dir: Path,
                                  flip_uv_v: bool, flip_uv_u: bool,
                                  double_sided: bool) -> List[str]:
    exported_files: List[str] = []
    category_dir = output_dir / "assets" / category_name
    category_dir.mkdir(parents=True, exist_ok=True)

    for bundle_name, rel_path, txd_name in SHARED_CATEGORY_MODELS.get(category_name, []):
        try:
            bundle_path = resolve_case_path(game_root, rel_path)
        except FileNotFoundError:
            continue

        try:
            model = parse_dff_model(bundle_name, bundle_path.read_bytes())
            if textures_dir is not None:
                for subset in model.subsets:
                    match = find_matching_texture(textures_dir, txd_name, subset.material_name)
                    if match is None:
                        continue
                    dest = textures_out_dir / match.name
                    if not dest.exists():
                        dest.write_bytes(match.read_bytes())
                    model.texture_paths[subset.material_name] = Path("textures") / match.name

            artifacts = build_scene_artifacts(
                {bundle_name.lower(): model},
                [make_identity_placement(bundle_name)],
                output_dir,
                binary_uri=None,
                embed_images=True,
                flip_uv_v=flip_uv_v,
                flip_uv_u=flip_uv_u,
                double_sided=double_sided,
            )
            write_glb_export(artifacts, category_dir / bundle_name)
            exported_files.append(f"{bundle_name}.glb")
        except Exception as exc:  # noqa: BLE001
            print(f"[aviso] No se pudo exportar {category_name}/{bundle_name}: {exc}")

    return exported_files


def main() -> None:
    parser = argparse.ArgumentParser(description="Exporta mapas y assets de GTA a glTF/GLB.")
    parser.add_argument("--game", choices=list(SUPPORTED_GAMES), default="gta3", help="Juego origen")
    parser.add_argument("--game-root", required=True, help="Ruta raiz de la instalacion del juego")
    parser.add_argument("--dat", default="", help="Archivo DAT principal a leer")
    parser.add_argument("--output-dir", required=True, help="Directorio de salida")
    parser.add_argument("--include-lod", action="store_true", help="Incluye objetos LOD del IPL")
    parser.add_argument("--ipl", action="append", default=[], help="IPL concreto a exportar (ruta relativa o absoluta). Puede repetirse.")
    parser.add_argument("--extracted-root", default="", help="Ruta con models/ y txd/ ya extraidos")
    parser.add_argument("--textures-dir", default="", help="Ruta con PNG ya convertidos desde TXD")
    parser.add_argument("--extract-textures", action="store_true",
                        help="Extrae texturas desde el juego. GTA4: exporta WTD a DDS/PNG; GTA3: aun no implementado.")
    parser.add_argument("--texture-output-format", choices=["dds", "png", "both"], default="dds",
                        help="Formato de salida para texturas extraidas")
    parser.add_argument("--format", choices=["gltf", "glb"], default="gltf", help="Formato de salida")
    parser.add_argument("--sector-size", type=float, default=0.0, help="Si es > 0, exporta GLB por sectores usando ese tamaño de rejilla")
    parser.add_argument("--export-zone-previews", action="store_true",
                        help="GTA4: tambien exporta GLB/glTF preview por zona en assets/zones/.")
    parser.add_argument("--export-category", action="append", default=[], choices=list(EXPORTABLE_CATEGORIES),
                        help="Exporta modelos sueltos por categoria a assets/<categoria>/. Puede repetirse.")
    parser.add_argument("--copy-ped-anims", action="store_true",
                        help="Copia anim/ped.ifp a assets/animations_raw/ped.ifp")
    parser.add_argument("--skip-map", action="store_true",
                        help="No exporta el mapa; solo genera categorias y/o animaciones crudas")
    parser.add_argument("--no-flip-v", action="store_true",
                        help="No invierte la coordenada V de las UV al escribir glTF/GLB")
    parser.add_argument("--flip-u", action="store_true",
                        help="Invierte la coordenada U de las UV al escribir glTF/GLB")
    parser.add_argument("--double-sided", action="store_true",
                        help="Marca los materiales como doble cara en el glTF/GLB")
    args = parser.parse_args()
    ensure_supported_game(args.game)
    if args.game == "gta3":
        run_gta3_export(args)
        return
    if args.game == "gta4":
        run_gta4_export(args)
        return
    raise SystemExit(f"Juego no soportado: {args.game}")


if __name__ == "__main__":
    main()
