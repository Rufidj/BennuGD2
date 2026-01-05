#!/usr/bin/env python3
"""
Universal Model to MD3 Converter
Converts OBJ and GLB files to Quake 3 MD3 format
Supports UV mapping, material baking, and automatic texture atlas generation
"""

import struct
import sys
import os
import shutil
from pathlib import Path

# Optional dependencies
try:
    from PIL import Image, ImageDraw
    PIL_AVAILABLE = True
except ImportError:
    PIL_AVAILABLE = False

try:
    from pygltflib import GLTF2
    import numpy as np
    GLB_AVAILABLE = True
except ImportError:
    GLB_AVAILABLE = False

class ModelConverter:
    """Base class for model conversion with common MD3 writing logic"""
    
    def __init__(self):
        self.vertices = []
        self.texcoords = []
        self.triangles = []
        self.materials = {}
        self.face_materials = []
        self.texture_file = None
        self.texture_files = []  # Multiple textures for atlas merging
        self.material_textures = {}  # Map material name to texture file
    
    def write_md3(self, output_path, surface_name="model", scale=1.0):
        """Write MD3 file"""
        print(f"Writing MD3 file: {output_path}")
        print(f"Scale factor: {scale}")
        
        with open(output_path, 'wb') as f:
            # MD3 Header
            f.write(b'IDP3')
            f.write(struct.pack('<i', 15))
            f.write(surface_name.encode('ascii').ljust(64, b'\0'))
            f.write(struct.pack('<i', 0))  # Flags
            f.write(struct.pack('<i', 1))  # Num frames
            f.write(struct.pack('<i', 0))  # Num tags
            f.write(struct.pack('<i', 1))  # Num surfaces
            f.write(struct.pack('<i', 0))  # Num skins
            
            ofs_frames = 108
            ofs_tags = ofs_frames + 56
            ofs_surfaces = ofs_tags
            
            f.write(struct.pack('<i', ofs_frames))
            f.write(struct.pack('<i', ofs_tags))
            f.write(struct.pack('<i', ofs_surfaces))
            f.write(struct.pack('<i', 0))  # ofs_eof placeholder
            
            # Frame (bounding box)
            min_x = min(v[0] for v in self.vertices) * scale
            min_y = min(v[1] for v in self.vertices) * scale
            min_z = min(v[2] for v in self.vertices) * scale
            max_x = max(v[0] for v in self.vertices) * scale
            max_y = max(v[1] for v in self.vertices) * scale
            max_z = max(v[2] for v in self.vertices) * scale
            
            f.write(struct.pack('<fff', min_x, min_y, min_z))
            f.write(struct.pack('<fff', max_x, max_y, max_z))
            center_x = (min_x + max_x) / 2
            center_y = (min_y + max_y) / 2
            center_z = (min_z + max_z) / 2
            f.write(struct.pack('<fff', center_x, center_y, center_z))
            radius = max(max_x - min_x, max_y - min_y, max_z - min_z) / 2
            f.write(struct.pack('<f', radius))
            f.write(b'frame_0\0'.ljust(16, b'\0'))
            
            # Surface
            f.write(b'IDP3')
            f.write(surface_name.encode('ascii').ljust(64, b'\0'))
            f.write(struct.pack('<i', 0))  # Flags
            f.write(struct.pack('<i', 1))  # Num frames
            f.write(struct.pack('<i', 0))  # Num shaders
            f.write(struct.pack('<i', len(self.vertices)))
            f.write(struct.pack('<i', len(self.triangles)))
            
            ofs_tris = 108
            ofs_shaders = ofs_tris + (12 * len(self.triangles))
            ofs_st = ofs_shaders
            ofs_verts = ofs_st + (8 * len(self.vertices))
            ofs_end = ofs_verts + (8 * len(self.vertices))
            
            f.write(struct.pack('<i', ofs_tris))
            f.write(struct.pack('<i', ofs_shaders))
            f.write(struct.pack('<i', ofs_st))
            f.write(struct.pack('<i', ofs_verts))
            f.write(struct.pack('<i', ofs_end))
            
            # Triangles
            for tri in self.triangles:
                f.write(struct.pack('<iii', tri[0], tri[1], tri[2]))
            
            # Texture coordinates
            for u, v in self.texcoords:
                f.write(struct.pack('<ff', u, 1.0 - v))
            
            # Vertices (compressed)
            for vx, vy, vz in self.vertices:
                x = int(vx * scale * 64.0)
                y = int(vy * scale * 64.0)
                z = int(vz * scale * 64.0)
                x = max(-32768, min(32767, x))
                y = max(-32768, min(32767, y))
                z = max(-32768, min(32767, z))
                f.write(struct.pack('<hhh', x, y, z))
                f.write(struct.pack('<BB', 0, 0))
            
            # Update ofs_eof
            ofs_eof = f.tell()
            f.seek(104)
            f.write(struct.pack('<i', ofs_eof))
        
        print(f"✓ MD3 written: {len(self.vertices)} vertices, {len(self.triangles)} triangles")
    
    def decimate(self, target_triangles=10000):
        """Reduce triangle count using simple decimation"""
        if len(self.triangles) <= target_triangles:
            print(f"✓ No decimation needed ({len(self.triangles)} triangles)")
            return
        
        print(f"⚠ High poly model detected: {len(self.triangles)} triangles")
        print(f"✓ Decimating to ~{target_triangles} triangles...")
        
        # Simple decimation: keep every Nth triangle
        ratio = target_triangles / len(self.triangles)
        step = max(1, int(1.0 / ratio))
        
        # Keep triangles at regular intervals
        decimated_triangles = []
        decimated_materials = []
        
        for i in range(0, len(self.triangles), step):
            decimated_triangles.append(self.triangles[i])
            if i < len(self.face_materials):
                decimated_materials.append(self.face_materials[i])
        
        # Rebuild vertex list (remove unused vertices)
        used_vertices = set()
        for tri in decimated_triangles:
            for idx in tri:
                used_vertices.add(idx)
        
        # Remap vertices
        old_to_new = {}
        new_vertices = []
        new_texcoords = []
        
        for old_idx in sorted(used_vertices):
            old_to_new[old_idx] = len(new_vertices)
            if old_idx < len(self.vertices):
                new_vertices.append(self.vertices[old_idx])
            if old_idx < len(self.texcoords):
                new_texcoords.append(self.texcoords[old_idx])
        
        # Remap triangle indices
        remapped_triangles = []
        for tri in decimated_triangles:
            remapped_tri = [old_to_new.get(idx, 0) for idx in tri]
            remapped_triangles.append(remapped_tri)
        
        self.vertices = new_vertices
        self.texcoords = new_texcoords
        self.triangles = remapped_triangles
        self.face_materials = decimated_materials
        
        print(f"✓ Decimated: {len(self.vertices)} vertices, {len(self.triangles)} triangles")
    
    def merge_textures(self, output_path):
        """Merge multiple textures into single atlas and remap UVs"""
        if not PIL_AVAILABLE:
            print("⚠ PIL not available, cannot merge textures")
            return None
        
        if len(self.texture_files) <= 1:
            return self.texture_file
        
        print(f"✓ Merging {len(self.texture_files)} textures into atlas...")
        
        # Load all textures
        images = []
        for tex_file in self.texture_files:
            try:
                img = Image.open(tex_file)
                images.append((tex_file, img))
            except Exception as e:
                print(f"⚠ Could not load {tex_file}: {e}")
        
        if not images:
            return None
        
        # Calculate atlas layout (simple grid)
        num_textures = len(images)
        cols = int(num_textures ** 0.5) + 1
        rows = (num_textures + cols - 1) // cols
        
        # Find max texture size
        max_w = max(img.width for _, img in images)
        max_h = max(img.height for _, img in images)
        
        # Create atlas
        atlas_w = max_w * cols
        atlas_h = max_h * rows
        atlas = Image.new('RGB', (atlas_w, atlas_h), color=(128, 128, 128))
        
        # Place textures and build UV remap
        texture_uvs = {}  # texture_file -> (u_offset, v_offset, u_scale, v_scale)
        
        for idx, (tex_file, img) in enumerate(images):
            row = idx // cols
            col = idx % cols
            
            x = col * max_w
            y = row * max_h
            
            atlas.paste(img, (x, y))
            
            # Calculate UV transform
            u_offset = col / cols
            v_offset = row / rows
            u_scale = img.width / atlas_w
            v_scale = img.height / atlas_h
            
            texture_uvs[tex_file] = (u_offset, v_offset, u_scale, v_scale)
        
        # Save merged atlas
        atlas.save(output_path)
        print(f"✓ Merged atlas: {output_path} ({atlas_w}x{atlas_h})")
        
        # Remap UVs
        print(f"✓ Remapping UVs for {len(self.texcoords)} vertices...")
        
        # Create new texcoords array (same size as original)
        new_texcoords = list(self.texcoords)  # Copy original
        
        for tri_idx, tri in enumerate(self.triangles):
            # Get material for this triangle
            mat_name = self.face_materials[tri_idx] if tri_idx < len(self.face_materials) else None
            tex_file = self.material_textures.get(mat_name)
            
            if tex_file and tex_file in texture_uvs:
                u_off, v_off, u_scale, v_scale = texture_uvs[tex_file]
                
                # Remap UVs for this triangle's vertices
                for vert_idx in tri:
                    if vert_idx < len(new_texcoords):
                        u, v = self.texcoords[vert_idx]
                        new_u = u_off + (u * u_scale)
                        new_v = v_off + (v * v_scale)
                        new_texcoords[vert_idx] = (new_u, new_v)
        
        # Apply remapped UVs
        self.texcoords = new_texcoords
        print(f"✓ UVs remapped successfully")
        
        return output_path
    
    def generate_texture_atlas(self, output_path, atlas_size=2048):
        """Generate texture atlas from materials using UV layout if available"""
        if not PIL_AVAILABLE:
            print("⚠ PIL not available, cannot generate atlas")
            return None
        
        if not self.materials:
            print("⚠ No materials found")
            return None
        
        # Check if UVs are valid (not all zeros, in range 0-1)
        has_valid_uvs = False
        if len(self.texcoords) > 10:
            valid_count = sum(1 for u, v in self.texcoords if 0.0 <= u <= 1.0 and 0.0 <= v <= 1.0 and (u > 0.01 or v > 0.01))
            has_valid_uvs = valid_count > len(self.texcoords) * 0.5  # At least 50% valid
        
        if has_valid_uvs:
            print(f"✓ UV-baking texture ({len(self.texcoords)} UVs, {valid_count} valid)")
            return self._bake_uv_atlas(output_path, atlas_size)
        else:
            print(f"⚠ No valid UVs, using color grid ({len(self.materials)} materials)")
            return self._bake_color_grid(output_path, atlas_size)
    
    def _bake_uv_atlas(self, output_path, atlas_size):
        """Bake texture using UV coordinates"""
        img = Image.new('RGB', (atlas_size, atlas_size), color=(128, 128, 128))
        draw = ImageDraw.Draw(img)
        
        material_colors = {name: data['color'] for name, data in self.materials.items()}
        
        for tri_idx, tri in enumerate(self.triangles):
            idx1, idx2, idx3 = tri
            
            if idx1 < len(self.texcoords) and idx2 < len(self.texcoords) and idx3 < len(self.texcoords):
                u1, v1 = self.texcoords[idx1]
                u2, v2 = self.texcoords[idx2]
                u3, v3 = self.texcoords[idx3]
                
                x1 = int(u1 * atlas_size)
                y1 = int((1.0 - v1) * atlas_size)
                x2 = int(u2 * atlas_size)
                y2 = int((1.0 - v2) * atlas_size)
                x3 = int(u3 * atlas_size)
                y3 = int((1.0 - v3) * atlas_size)
                
                tri_material = self.face_materials[tri_idx] if tri_idx < len(self.face_materials) else None
                if tri_material and tri_material in material_colors:
                    color = material_colors[tri_material]
                    if color == (0, 0, 0):
                        color = (1, 1, 1)  # Avoid pure black
                else:
                    color = (128, 128, 128)
                
                draw.polygon([(x1, y1), (x2, y2), (x3, y3)], fill=color, outline=color)
        
        img.save(output_path)
        print(f"✓ UV atlas: {output_path}")
        return output_path
    
    def _bake_color_grid(self, output_path, atlas_size):
        """Simple color grid fallback"""
        img = Image.new('RGB', (atlas_size, atlas_size), color=(255, 255, 255))
        draw = ImageDraw.Draw(img)
        
        num_materials = len(self.materials)
        cols = int(num_materials ** 0.5) + 1
        rows = (num_materials + cols - 1) // cols
        cell_w = atlas_size // cols
        cell_h = atlas_size // rows
        
        for idx, (mat_name, mat_data) in enumerate(self.materials.items()):
            row = idx // cols
            col = idx % cols
            x1 = col * cell_w + 2
            y1 = row * cell_h + 2
            x2 = (col + 1) * cell_w - 2
            y2 = (row + 1) * cell_h - 2
            color = mat_data['color']
            draw.rectangle([x1, y1, x2, y2], fill=color)
        
        img.save(output_path)
        print(f"✓ Color grid: {output_path}")
        return output_path


class OBJConverter(ModelConverter):
    """OBJ format converter"""
    
    def load(self, filepath):
        """Load OBJ file"""
        print(f"Loading OBJ: {filepath}")
        
        # Load MTL if exists
        obj_dir = os.path.dirname(filepath)
        obj_name = os.path.splitext(os.path.basename(filepath))[0]
        mtl_path = os.path.join(obj_dir, obj_name + '.mtl')
        
        if os.path.exists(mtl_path):
            self._load_mtl(mtl_path)
        
        # Parse OBJ
        obj_vertices = []
        obj_texcoords = []
        current_material = None
        
        with open(filepath, 'r') as f:
            for line in f:
                parts = line.strip().split()
                if not parts:
                    continue
                
                if parts[0] == 'usemtl':
                    current_material = parts[1] if len(parts) > 1 else None
                elif parts[0] == 'v':
                    obj_vertices.append((float(parts[1]), float(parts[2]), float(parts[3])))
                elif parts[0] == 'vt':
                    obj_texcoords.append((float(parts[1]), float(parts[2])))
                elif parts[0] == 'f':
                    face_verts = []
                    for i in range(1, len(parts)):
                        indices = parts[i].split('/')
                        v_idx = int(indices[0]) - 1
                        vt_idx = int(indices[1]) - 1 if len(indices) > 1 and indices[1] else 0
                        face_verts.append((v_idx, vt_idx))
                    
                    # Triangulate
                    for i in range(1, len(face_verts) - 1):
                        self.triangles.append([face_verts[0], face_verts[i], face_verts[i+1]])
                        self.face_materials.append(current_material)
        
        print(f"Loaded: {len(obj_vertices)} verts, {len(obj_texcoords)} UVs, {len(self.triangles)} tris")
        
        # Build unique vertices with axis conversion
        self._build_vertices(obj_vertices, obj_texcoords)
        return True
    
    def _load_mtl(self, mtl_path):
        """Load MTL materials"""
        print(f"Loading MTL: {mtl_path}")
        current_mat = None
        
        with open(mtl_path, 'r') as f:
            for line in f:
                parts = line.strip().split()
                if not parts:
                    continue
                
                if parts[0] == 'newmtl':
                    current_mat = parts[1]
                    self.materials[current_mat] = {'color': (128, 128, 128)}
                elif parts[0] == 'Kd' and current_mat:
                    r = int(float(parts[1]) * 255)
                    g = int(float(parts[2]) * 255)
                    b = int(float(parts[3]) * 255)
                    self.materials[current_mat]['color'] = (r, g, b)
                elif parts[0] == 'map_Kd':
                    texture_path = ' '.join(parts[1:])
                    mtl_dir = os.path.dirname(mtl_path)
                    full_path = os.path.join(mtl_dir, texture_path)
                    if os.path.exists(full_path):
                        self.texture_file = full_path
        
        if self.materials:
            print(f"Found {len(self.materials)} materials")
    
    def _build_vertices(self, obj_vertices, obj_texcoords):
        """Build unique vertex list with UVs and axis conversion"""
        has_uvs = len(obj_texcoords) > 0
        vertex_map = {}
        unique_vertices = []
        unique_texcoords = []
        triangle_indices = []
        
        # Calculate material UV positions for auto-gen
        num_materials = max(len(self.materials), 1)
        cols = int(num_materials ** 0.5) + 1
        rows = (num_materials + cols - 1) // cols
        material_uvs = {}
        
        for idx, mat_name in enumerate(self.materials.keys()):
            row = idx // cols
            col = idx % cols
            u = (col + 0.5) / cols
            v = (row + 0.5) / rows
            material_uvs[mat_name] = (u, v)
        
        default_uv = (0.5, 0.5)
        
        for tri_idx, tri in enumerate(self.triangles):
            tri_material = self.face_materials[tri_idx] if tri_idx < len(self.face_materials) else None
            tri_uv = material_uvs.get(tri_material, default_uv)
            tri_indices = []
            
            for v_idx, vt_idx in tri:
                if has_uvs and vt_idx < len(obj_texcoords):
                    uv = obj_texcoords[vt_idx]
                    key = (v_idx, vt_idx)
                else:
                    uv = tri_uv
                    key = (v_idx, uv[0], uv[1])
                
                if key not in vertex_map:
                    vertex_map[key] = len(unique_vertices)
                    vx, vy, vz = obj_vertices[v_idx]
                    unique_vertices.append((vx, vz, vy))  # Y-up to Z-up
                    unique_texcoords.append(uv)
                
                tri_indices.append(vertex_map[key])
            triangle_indices.append(tri_indices)
        
        self.vertices = unique_vertices
        self.texcoords = unique_texcoords
        self.triangles = triangle_indices
        print(f"Unique vertices: {len(self.vertices)}")


class GLBConverter(ModelConverter):
    """GLB/glTF format converter"""
    
    def load(self, filepath):
        """Load GLB file"""
        if not GLB_AVAILABLE:
            print("Error: pygltflib not installed")
            print("Install with: pip install pygltflib numpy")
            return False
        
        print(f"Loading GLB: {filepath}")
        
        try:
            gltf = GLTF2().load(filepath)
        except Exception as e:
            print(f"Error loading GLB: {e}")
            return False
        
        # Extract materials
        if gltf.materials:
            for mat_idx, material in enumerate(gltf.materials):
                mat_name = material.name or f"Material_{mat_idx}"
                
                # Try to extract texture
                if material.pbrMetallicRoughness and material.pbrMetallicRoughness.baseColorTexture:
                    texture_index = material.pbrMetallicRoughness.baseColorTexture.index
                    if texture_index < len(gltf.textures):
                        texture = gltf.textures[texture_index]
                        if texture.source is not None and texture.source < len(gltf.images):
                            image = gltf.images[texture.source]
                            
                            # Extract embedded texture
                            if image.bufferView is not None:
                                buffer_view = gltf.bufferViews[image.bufferView]
                                data = gltf.binary_blob()
                                image_data = data[buffer_view.byteOffset:buffer_view.byteOffset + buffer_view.byteLength]
                                
                                # Save texture
                                texture_name = image.name or f"texture_{mat_idx}.png"
                                if not texture_name.endswith(('.png', '.jpg', '.jpeg')):
                                    texture_name += '.png'
                                
                                texture_path = os.path.join(os.path.dirname(filepath), texture_name)
                                with open(texture_path, 'wb') as f:
                                    f.write(image_data)
                                
                                print(f"✓ Extracted texture: {texture_name}")
                                
                                # Track texture for this material
                                self.material_textures[mat_name] = texture_path
                                self.texture_files.append(texture_path)
                                
                                if not self.texture_file:  # Use first texture found
                                    self.texture_file = texture_path
                
                # Get base color as fallback
                if material.pbrMetallicRoughness and material.pbrMetallicRoughness.baseColorFactor:
                    cf = material.pbrMetallicRoughness.baseColorFactor
                    r, g, b = int(cf[0] * 255), int(cf[1] * 255), int(cf[2] * 255)
                    self.materials[mat_name] = {'color': (r, g, b)}
                else:
                    self.materials[mat_name] = {'color': (200, 200, 200)}
        
        # Extract geometry
        obj_vertices = []
        obj_texcoords = []
        
        for mesh in gltf.meshes:
            for primitive in mesh.primitives:
                base_vertex = len(obj_vertices)
                
                # Vertices
                if primitive.attributes.POSITION is not None:
                    accessor = gltf.accessors[primitive.attributes.POSITION]
                    buffer_view = gltf.bufferViews[accessor.bufferView]
                    data = gltf.binary_blob()
                    vertex_data = np.frombuffer(
                        data[buffer_view.byteOffset:buffer_view.byteOffset + buffer_view.byteLength],
                        dtype=np.float32
                    ).reshape(-1, 3)
                    obj_vertices.extend([(v[0], v[1], v[2]) for v in vertex_data])
                
                # UVs
                if primitive.attributes.TEXCOORD_0 is not None:
                    accessor = gltf.accessors[primitive.attributes.TEXCOORD_0]
                    buffer_view = gltf.bufferViews[accessor.bufferView]
                    data = gltf.binary_blob()
                    uv_data = np.frombuffer(
                        data[buffer_view.byteOffset:buffer_view.byteOffset + buffer_view.byteLength],
                        dtype=np.float32
                    ).reshape(-1, 2)
                    obj_texcoords.extend([(uv[0], uv[1]) for uv in uv_data])
                
                # Indices
                if primitive.indices is not None:
                    accessor = gltf.accessors[primitive.indices]
                    buffer_view = gltf.bufferViews[accessor.bufferView]
                    data = gltf.binary_blob()
                    
                    if accessor.componentType == 5123:  # UNSIGNED_SHORT
                        indices = np.frombuffer(
                            data[buffer_view.byteOffset:buffer_view.byteOffset + buffer_view.byteLength],
                            dtype=np.uint16
                        )
                    else:
                        indices = np.frombuffer(
                            data[buffer_view.byteOffset:buffer_view.byteOffset + buffer_view.byteLength],
                            dtype=np.uint32
                        )
                    
                    mat_name = None
                    if primitive.material is not None and gltf.materials:
                        mat_name = gltf.materials[primitive.material].name
                    
                    for i in range(0, len(indices), 3):
                        idx1 = base_vertex + indices[i]
                        idx2 = base_vertex + indices[i+1]
                        idx3 = base_vertex + indices[i+2]
                        self.triangles.append([(idx1, idx1), (idx2, idx2), (idx3, idx3)])
                        self.face_materials.append(mat_name)
        
        print(f"Loaded: {len(obj_vertices)} verts, {len(obj_texcoords)} UVs, {len(self.triangles)} tris")
        
        # Build vertices (reuse OBJ logic)
        obj_converter = OBJConverter()
        obj_converter.materials = self.materials
        obj_converter.triangles = self.triangles
        obj_converter.face_materials = self.face_materials
        obj_converter._build_vertices(obj_vertices, obj_texcoords)
        
        self.vertices = obj_converter.vertices
        self.texcoords = obj_converter.texcoords
        self.triangles = obj_converter.triangles
        
        return True


def main():
    if len(sys.argv) < 2:
        print("Usage: python model_to_md3.py <input.obj|input.glb> [output.md3] [scale]")
        print("Example: python model_to_md3.py car.obj car.md3 10.0")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else Path(input_file).stem + '.md3'
    scale = float(sys.argv[3]) if len(sys.argv) > 3 else 1.0
    
    if not os.path.exists(input_file):
        print(f"Error: File not found: {input_file}")
        sys.exit(1)
    
    # Detect format
    ext = Path(input_file).suffix.lower()
    
    if ext == '.obj':
        converter = OBJConverter()
    elif ext in ['.glb', '.gltf']:
        converter = GLBConverter()
    else:
        print(f"Error: Unsupported format: {ext}")
        print("Supported: .obj, .glb, .gltf")
        sys.exit(1)
    
    # Load and convert
    if not converter.load(input_file):
        sys.exit(1)
    
    # Auto-decimate if too many triangles or vertices
    # MD3 renderer has MD3_MAX_VERTICES = 4096 limit
    MAX_VERTICES = 4000  # Leave some margin
    MAX_TRIANGLES = 2000  # MD3 standard
    
    if len(converter.triangles) > 5000:
        converter.decimate(target_triangles=MAX_TRIANGLES)
    
    # Check vertex count and decimate more if needed
    iteration = 0
    max_iterations = 5
    while len(converter.vertices) > MAX_VERTICES and iteration < max_iterations:
        prev_vertices = len(converter.vertices)
        current_tris = len(converter.triangles)
        new_target = int(current_tris * 0.7)  # Reduce by 30%
        print(f"⚠ Too many vertices ({len(converter.vertices)} > {MAX_VERTICES})")
        print(f"⚠ Decimating further: {current_tris} -> {new_target} triangles")
        converter.decimate(target_triangles=new_target)
        
        # Check if decimation actually worked
        if len(converter.vertices) >= prev_vertices:
            print(f"⚠ Decimation not reducing vertices, stopping")
            print(f"⚠ Model too complex for MD3 format ({len(converter.vertices)} vertices)")
            break
        
        if new_target < 500:  # Safety: don't go below 500 triangles
            print(f"⚠ Cannot reduce further, model too complex")
            break
        
        iteration += 1
    
    # Merge textures if multiple found (GLB) - BEFORE writing MD3
    if len(converter.texture_files) > 1:
        output_dir = os.path.dirname(output_file) or '.'
        merged_atlas = os.path.join(output_dir, Path(input_file).stem + '_atlas.png')
        converter.merge_textures(merged_atlas)
        converter.texture_file = merged_atlas
    
    converter.write_md3(output_file, surface_name=Path(input_file).stem, scale=scale)
    
    # Handle texture
    if converter.texture_file:
        output_dir = os.path.dirname(output_file) or '.'
        texture_name = os.path.basename(converter.texture_file)
        output_texture = os.path.join(output_dir, texture_name)
        
        # Only copy if source and destination are different
        if os.path.abspath(converter.texture_file) != os.path.abspath(output_texture):
            shutil.copy2(converter.texture_file, output_texture)
            print(f"✓ Texture copied: {output_texture}")
        else:
            print(f"✓ Texture already in place: {output_texture}")
    elif converter.materials and PIL_AVAILABLE:
        output_dir = os.path.dirname(output_file) or '.'
        atlas_path = os.path.join(output_dir, Path(input_file).stem + '.png')
        converter.generate_texture_atlas(atlas_path)
    
    print(f"\n✓ Conversion complete!")
    print(f"  Input: {input_file}")
    print(f"  Output: {output_file}")
    print(f"  Scale: {scale}x")

if __name__ == "__main__":
    main()
