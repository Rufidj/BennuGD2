#!/usr/bin/env python3

import argparse
import json
import math
import mimetypes
import pathlib
import struct
from dataclasses import dataclass, field
from io import BytesIO
from typing import Dict, List, Optional, Sequence, Tuple

from PIL import Image


Vec2 = Tuple[float, float]
Vec3 = Tuple[float, float, float]


@dataclass
class Material:
    name: str
    diffuse: Tuple[float, float, float] = (1.0, 1.0, 1.0)
    alpha: float = 1.0
    texture_path: Optional[pathlib.Path] = None


@dataclass
class ObjVertexRef:
    v: int
    vt: Optional[int]
    vn: Optional[int]


@dataclass
class Triangle:
    refs: Tuple[ObjVertexRef, ObjVertexRef, ObjVertexRef]
    material_name: Optional[str]


@dataclass
class ObjScene:
    positions: List[Vec3] = field(default_factory=list)
    texcoords: List[Vec2] = field(default_factory=list)
    normals: List[Vec3] = field(default_factory=list)
    triangles: List[Triangle] = field(default_factory=list)
    materials: Dict[str, Material] = field(default_factory=dict)
    mtllibs: List[pathlib.Path] = field(default_factory=list)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Convierte OBJ/MTL a GLB, con opcion de dividir por sectores.")
    parser.add_argument("input_obj", help="Ruta al archivo OBJ")
    parser.add_argument("--output", help="Ruta del GLB de salida para exportacion unica")
    parser.add_argument("--output-dir", help="Directorio de salida para exportacion por sectores")
    parser.add_argument(
        "--sector-size",
        type=float,
        default=0.0,
        help="Tamano del sector en unidades del mundo. Si es > 0, genera GLB por sectores.",
    )
    parser.add_argument(
        "--manifest-name",
        default="manifest.json",
        help="Nombre del manifest para salida por sectores",
    )
    args = parser.parse_args()
    if args.sector_size > 0 and not args.output_dir:
        parser.error("--output-dir es obligatorio cuando usas --sector-size")
    if args.sector_size <= 0 and not args.output:
        parser.error("--output es obligatorio cuando no usas --sector-size")
    return args


def normalize_index(index: int, size: int) -> int:
    if index > 0:
        return index - 1
    if index < 0:
        return size + index
    raise ValueError("OBJ index 0 no es valido")


def parse_vertex_ref(token: str, scene: ObjScene) -> ObjVertexRef:
    parts = token.split("/")
    v = normalize_index(int(parts[0]), len(scene.positions))
    vt = None
    vn = None
    if len(parts) > 1 and parts[1]:
        vt = normalize_index(int(parts[1]), len(scene.texcoords))
    if len(parts) > 2 and parts[2]:
        vn = normalize_index(int(parts[2]), len(scene.normals))
    return ObjVertexRef(v=v, vt=vt, vn=vn)


def parse_mtl(mtl_path: pathlib.Path) -> Dict[str, Material]:
    materials: Dict[str, Material] = {}
    current: Optional[Material] = None
    with mtl_path.open("r", encoding="utf-8", errors="ignore") as handle:
        for raw_line in handle:
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            key = parts[0]
            values = parts[1:]
            if key == "newmtl" and values:
                current = Material(name=" ".join(values))
                materials[current.name] = current
            elif current is None:
                continue
            elif key == "Kd" and len(values) >= 3:
                current.diffuse = (float(values[0]), float(values[1]), float(values[2]))
            elif key in ("d", "Tr") and values:
                alpha = float(values[0])
                current.alpha = 1.0 - alpha if key == "Tr" else alpha
            elif key == "map_Kd" and values:
                texture_name = " ".join(values)
                current.texture_path = (mtl_path.parent / texture_name).resolve()
    return materials


def parse_obj(obj_path: pathlib.Path) -> ObjScene:
    scene = ObjScene()
    current_material: Optional[str] = None
    with obj_path.open("r", encoding="utf-8", errors="ignore") as handle:
        for raw_line in handle:
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            key = parts[0]
            values = parts[1:]
            if key == "v" and len(values) >= 3:
                scene.positions.append((float(values[0]), float(values[1]), float(values[2])))
            elif key == "vt" and len(values) >= 2:
                scene.texcoords.append((float(values[0]), 1.0 - float(values[1])))
            elif key == "vn" and len(values) >= 3:
                scene.normals.append((float(values[0]), float(values[1]), float(values[2])))
            elif key == "usemtl" and values:
                current_material = " ".join(values)
            elif key == "mtllib" and values:
                mtllib_path = (obj_path.parent / " ".join(values)).resolve()
                scene.mtllibs.append(mtllib_path)
            elif key == "f" and len(values) >= 3:
                refs = [parse_vertex_ref(token, scene) for token in values]
                for i in range(1, len(refs) - 1):
                    scene.triangles.append(
                        Triangle(
                            refs=(refs[0], refs[i], refs[i + 1]),
                            material_name=current_material,
                        )
                    )

    for mtllib_path in scene.mtllibs:
        if mtllib_path.exists():
            scene.materials.update(parse_mtl(mtllib_path))
    return scene


def sub(a: Vec3, b: Vec3) -> Vec3:
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def cross(a: Vec3, b: Vec3) -> Vec3:
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def normalize(v: Vec3) -> Vec3:
    length = math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])
    if length <= 1e-8:
        return (0.0, 1.0, 0.0)
    return (v[0] / length, v[1] / length, v[2] / length)


def triangle_centroid(points: Sequence[Vec3]) -> Vec3:
    return (
        (points[0][0] + points[1][0] + points[2][0]) / 3.0,
        (points[0][1] + points[1][1] + points[2][1]) / 3.0,
        (points[0][2] + points[1][2] + points[2][2]) / 3.0,
    )


def sectorize(scene: ObjScene, sector_size: float) -> Dict[Tuple[int, int], List[Triangle]]:
    min_x = min(pos[0] for pos in scene.positions)
    min_z = min(pos[2] for pos in scene.positions)
    sectors: Dict[Tuple[int, int], List[Triangle]] = {}
    for tri in scene.triangles:
        points = [scene.positions[ref.v] for ref in tri.refs]
        centroid = triangle_centroid(points)
        sx = int(math.floor((centroid[0] - min_x) / sector_size))
        sz = int(math.floor((centroid[2] - min_z) / sector_size))
        sectors.setdefault((sx, sz), []).append(tri)
    return sectors


@dataclass
class BufferChunk:
    data: bytearray = field(default_factory=bytearray)

    def append(self, blob: bytes, alignment: int = 4) -> Tuple[int, int]:
        while len(self.data) % alignment:
            self.data.append(0)
        offset = len(self.data)
        self.data.extend(blob)
        return offset, len(blob)


def load_texture_bytes(texture_path: pathlib.Path) -> Tuple[bytes, str]:
    suffix = texture_path.suffix.lower()
    mime = mimetypes.types_map.get(suffix, "")
    if mime in ("image/png", "image/jpeg"):
        return texture_path.read_bytes(), mime

    image = Image.open(texture_path)
    if image.mode not in ("RGB", "RGBA"):
        image = image.convert("RGBA")
    if image.mode == "RGB":
        image = image.convert("RGBA")
    out = BytesIO()
    image.save(out, format="PNG")
    return out.getvalue(), "image/png"


def build_glb(scene: ObjScene, triangles: List[Triangle], output_path: pathlib.Path, node_name: str) -> Dict[str, object]:
    buffer_blob = BufferChunk()
    images_json = []
    textures_json = []
    materials_json = []
    material_index_by_name: Dict[Optional[str], int] = {}
    texture_index_by_path: Dict[pathlib.Path, int] = {}

    def get_material_index(material_name: Optional[str]) -> int:
        if material_name in material_index_by_name:
            return material_index_by_name[material_name]

        material = scene.materials.get(material_name or "")
        mat_json = {
            "name": material_name or "default",
            "pbrMetallicRoughness": {
                "baseColorFactor": [1.0, 1.0, 1.0, 1.0],
                "metallicFactor": 0.0,
                "roughnessFactor": 1.0,
            },
            "doubleSided": True,
        }
        if material is not None:
            mat_json["pbrMetallicRoughness"]["baseColorFactor"] = [
                material.diffuse[0],
                material.diffuse[1],
                material.diffuse[2],
                material.alpha,
            ]
            if material.alpha < 1.0:
                mat_json["alphaMode"] = "BLEND"
            if material.texture_path and material.texture_path.exists():
                if material.texture_path not in texture_index_by_path:
                    image_bytes, mime_type = load_texture_bytes(material.texture_path)
                    image_offset, image_length = buffer_blob.append(image_bytes)
                    buffer_view_index = len(buffer_views_json)
                    buffer_views_json.append(
                        {
                            "buffer": 0,
                            "byteOffset": image_offset,
                            "byteLength": image_length,
                        }
                    )
                    image_index = len(images_json)
                    images_json.append(
                        {
                            "bufferView": buffer_view_index,
                            "mimeType": mime_type,
                            "name": material.texture_path.name,
                        }
                    )
                    texture_index_by_path[material.texture_path] = len(textures_json)
                    textures_json.append({"source": image_index})
                mat_json["pbrMetallicRoughness"]["baseColorTexture"] = {
                    "index": texture_index_by_path[material.texture_path]
                }

        material_index_by_name[material_name] = len(materials_json)
        materials_json.append(mat_json)
        return material_index_by_name[material_name]

    def pack_f32(values: Sequence[float]) -> bytes:
        return struct.pack("<" + "f" * len(values), *values)

    def pack_u32(values: Sequence[int]) -> bytes:
        return struct.pack("<" + "I" * len(values), *values)

    def min_max_vec3(vectors: Sequence[Vec3]) -> Tuple[List[float], List[float]]:
        mins = [min(v[i] for v in vectors) for i in range(3)]
        maxs = [max(v[i] for v in vectors) for i in range(3)]
        return mins, maxs

    primitives_json = []
    buffer_views_json: List[Dict[str, object]] = []
    accessors_json: List[Dict[str, object]] = []

    triangles_by_material: Dict[Optional[str], List[Triangle]] = {}
    for tri in triangles:
        triangles_by_material.setdefault(tri.material_name, []).append(tri)

    for material_name, material_tris in triangles_by_material.items():
        vertices: List[Tuple[Vec3, Vec3, Vec2]] = []
        indices: List[int] = []
        vertex_map: Dict[Tuple[int, Optional[int], Optional[int], Tuple[float, float, float]], int] = {}

        for tri in material_tris:
            points = [scene.positions[ref.v] for ref in tri.refs]
            face_normal = normalize(cross(sub(points[1], points[0]), sub(points[2], points[0])))
            for ref in tri.refs:
                pos = scene.positions[ref.v]
                uv = scene.texcoords[ref.vt] if ref.vt is not None and ref.vt < len(scene.texcoords) else (0.0, 0.0)
                if ref.vn is not None and ref.vn < len(scene.normals):
                    normal = normalize(scene.normals[ref.vn])
                    key = (ref.v, ref.vt, ref.vn, normal)
                else:
                    normal = face_normal
                    key = (ref.v, ref.vt, None, normal)
                if key not in vertex_map:
                    vertex_map[key] = len(vertices)
                    vertices.append((pos, normal, uv))
                indices.append(vertex_map[key])

        positions_flat: List[float] = []
        normals_flat: List[float] = []
        texcoords_flat: List[float] = []
        positions_vec: List[Vec3] = []
        for pos, normal, uv in vertices:
            positions_vec.append(pos)
            positions_flat.extend(pos)
            normals_flat.extend(normal)
            texcoords_flat.extend(uv)

        pos_offset, pos_length = buffer_blob.append(pack_f32(positions_flat))
        pos_bv = len(buffer_views_json)
        buffer_views_json.append(
            {
                "buffer": 0,
                "byteOffset": pos_offset,
                "byteLength": pos_length,
                "target": 34962,
            }
        )

        normal_offset, normal_length = buffer_blob.append(pack_f32(normals_flat))
        normal_bv = len(buffer_views_json)
        buffer_views_json.append(
            {
                "buffer": 0,
                "byteOffset": normal_offset,
                "byteLength": normal_length,
                "target": 34962,
            }
        )

        uv_offset, uv_length = buffer_blob.append(pack_f32(texcoords_flat))
        uv_bv = len(buffer_views_json)
        buffer_views_json.append(
            {
                "buffer": 0,
                "byteOffset": uv_offset,
                "byteLength": uv_length,
                "target": 34962,
            }
        )

        index_offset, index_length = buffer_blob.append(pack_u32(indices))
        index_bv = len(buffer_views_json)
        buffer_views_json.append(
            {
                "buffer": 0,
                "byteOffset": index_offset,
                "byteLength": index_length,
                "target": 34963,
            }
        )

        mins, maxs = min_max_vec3(positions_vec)
        pos_accessor = len(accessors_json)
        accessors_json.append(
            {
                "bufferView": pos_bv,
                "componentType": 5126,
                "count": len(vertices),
                "type": "VEC3",
                "min": mins,
                "max": maxs,
            }
        )

        normal_accessor = len(accessors_json)
        accessors_json.append(
            {
                "bufferView": normal_bv,
                "componentType": 5126,
                "count": len(vertices),
                "type": "VEC3",
            }
        )

        uv_accessor = len(accessors_json)
        accessors_json.append(
            {
                "bufferView": uv_bv,
                "componentType": 5126,
                "count": len(vertices),
                "type": "VEC2",
            }
        )

        index_accessor = len(accessors_json)
        accessors_json.append(
            {
                "bufferView": index_bv,
                "componentType": 5125,
                "count": len(indices),
                "type": "SCALAR",
            }
        )

        primitives_json.append(
            {
                "attributes": {
                    "POSITION": pos_accessor,
                    "NORMAL": normal_accessor,
                    "TEXCOORD_0": uv_accessor,
                },
                "indices": index_accessor,
                "material": get_material_index(material_name),
            }
        )

    gltf = {
        "asset": {"version": "2.0", "generator": "tr_obj_to_glb.py"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0, "name": node_name}],
        "meshes": [{"name": node_name, "primitives": primitives_json}],
        "buffers": [{"byteLength": len(buffer_blob.data)}],
        "bufferViews": buffer_views_json,
        "accessors": accessors_json,
        "materials": materials_json,
        "images": images_json,
        "textures": textures_json,
        "samplers": [{"magFilter": 9729, "minFilter": 9987, "wrapS": 10497, "wrapT": 10497}],
    }

    for texture in gltf["textures"]:
        texture["sampler"] = 0

    json_blob = json.dumps(gltf, separators=(",", ":")).encode("utf-8")
    while len(json_blob) % 4:
        json_blob += b" "
    while len(buffer_blob.data) % 4:
        buffer_blob.data.append(0)

    total_length = 12 + 8 + len(json_blob) + 8 + len(buffer_blob.data)
    with output_path.open("wb") as handle:
        handle.write(struct.pack("<4sII", b"glTF", 2, total_length))
        handle.write(struct.pack("<I4s", len(json_blob), b"JSON"))
        handle.write(json_blob)
        handle.write(struct.pack("<I4s", len(buffer_blob.data), b"BIN\x00"))
        handle.write(buffer_blob.data)

    return {
        "file": output_path.name,
        "triangles": len(triangles),
    }


def triangle_bounds(scene: ObjScene, triangles: Sequence[Triangle]) -> Dict[str, List[float]]:
    coords: List[Vec3] = []
    for tri in triangles:
        for ref in tri.refs:
            coords.append(scene.positions[ref.v])
    mins = [min(v[i] for v in coords) for i in range(3)]
    maxs = [max(v[i] for v in coords) for i in range(3)]
    return {"min": mins, "max": maxs}


def export_single(scene: ObjScene, output_path: pathlib.Path) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    build_glb(scene, scene.triangles, output_path, output_path.stem)


def export_sectors(scene: ObjScene, output_dir: pathlib.Path, sector_size: float, manifest_name: str) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    sector_map = sectorize(scene, sector_size)
    manifest = {
        "source": "OBJ",
        "sector_size": sector_size,
        "sector_count": len(sector_map),
        "sectors": [],
    }
    for (sx, sz), triangles in sorted(sector_map.items()):
        name = f"sector_{sx}_{sz}.glb"
        out_path = output_dir / name
        build_glb(scene, triangles, out_path, f"sector_{sx}_{sz}")
        entry = {
            "sector": [sx, sz],
            "file": name,
            "triangles": len(triangles),
        }
        entry.update(triangle_bounds(scene, triangles))
        manifest["sectors"].append(entry)

    with (output_dir / manifest_name).open("w", encoding="utf-8") as handle:
        json.dump(manifest, handle, indent=2)


def main() -> None:
    args = parse_args()
    obj_path = pathlib.Path(args.input_obj).resolve()
    scene = parse_obj(obj_path)
    if not scene.positions or not scene.triangles:
        raise SystemExit("El OBJ no contiene geometria util para exportar.")
    if args.sector_size > 0:
        export_sectors(scene, pathlib.Path(args.output_dir).resolve(), args.sector_size, args.manifest_name)
    else:
        export_single(scene, pathlib.Path(args.output).resolve())


if __name__ == "__main__":
    main()
