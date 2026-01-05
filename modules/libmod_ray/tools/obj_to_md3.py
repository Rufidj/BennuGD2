#!/usr/bin/env python3
"""
OBJ to MD3 Converter
Converts Wavefront OBJ files to Quake 3 MD3 format
Preserves UV coordinates and allows scale adjustment
"""

import struct
import sys
import os
import shutil
from pathlib import Path

try:
    from PIL import Image, ImageDraw
    PIL_AVAILABLE = True
except ImportError:
    PIL_AVAILABLE = False
    print("Warning: PIL/Pillow not installed. Texture atlas generation disabled.")
    print("Install with: pip install Pillow")

try:
    from pygltflib import GLTF2
    import numpy as np
    GLB_AVAILABLE = True
except ImportError:
    GLB_AVAILABLE = False
    print("Warning: pygltflib not installed. GLB support disabled.")
    print("Install with: pip install pygltflib numpy")

class MD3Converter:
    def __init__(self):
        self.vertices = []
        self.texcoords = []
        self.triangles = []
        self.normals = []
        self.texture_file = None
        self.materials = {}  # Store material colors
        self.current_material = None
        
    def load_mtl(self, mtl_path):
        """Load MTL file and extract texture information or material colors"""
        if not os.path.exists(mtl_path):
            print(f"Warning: MTL file not found: {mtl_path}")
            return None
            
        print(f"Loading MTL file: {mtl_path}")
        
        current_mat = None
        
        with open(mtl_path, 'r') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                    
                parts = line.split()
                if not parts:
                    continue
                
                # New material
                if parts[0] == 'newmtl':
                    current_mat = parts[1]
                    self.materials[current_mat] = {'color': (128, 128, 128)}  # Default gray
                
                # Diffuse color
                elif parts[0] == 'Kd' and current_mat:
                    r = int(float(parts[1]) * 255)
                    g = int(float(parts[2]) * 255)
                    b = int(float(parts[3]) * 255)
                    self.materials[current_mat]['color'] = (r, g, b)
                
                # Diffuse texture map
                elif parts[0] == 'map_Kd':
                    texture_path = ' '.join(parts[1:])
                    # Handle relative paths
                    mtl_dir = os.path.dirname(mtl_path)
                    full_texture_path = os.path.join(mtl_dir, texture_path)
                    
                    if os.path.exists(full_texture_path):
                        self.texture_file = full_texture_path
                        print(f"Found texture: {self.texture_file}")
                        return self.texture_file
                    else:
                        print(f"Warning: Texture file not found: {full_texture_path}")
        
        if self.materials:
            print(f"Found {len(self.materials)} materials with colors:")
            for mat_name, mat_data in self.materials.items():
                r, g, b = mat_data['color']
                print(f"  - {mat_name}: RGB({r}, {g}, {b})")
        
        return None
    
    def load_glb(self, filepath):
        """Load GLB file and extract geometry, UVs, and materials"""
        if not GLB_AVAILABLE:
            print("Error: pygltflib not installed. Cannot load GLB files.")
            return False
        
        print(f"Loading GLB file: {filepath}")
        
        try:
            gltf = GLTF2().load(filepath)
        except Exception as e:
            print(f"Error loading GLB: {e}")
            return False
        
        # Extract materials
        if gltf.materials:
            for mat_idx, material in enumerate(gltf.materials):
                mat_name = material.name or f"Material_{mat_idx}"
                
                # Get base color
                if material.pbrMetallicRoughness and material.pbrMetallicRoughness.baseColorFactor:
                    color_factor = material.pbrMetallicRoughness.baseColorFactor
                    r = int(color_factor[0] * 255)
                    g = int(color_factor[1] * 255)
                    b = int(color_factor[2] * 255)
                    self.materials[mat_name] = {'color': (r, g, b)}
                else:
                    self.materials[mat_name] = {'color': (200, 200, 200)}
        
        # Extract mesh data
        obj_vertices = []
        obj_texcoords = []
        face_materials = []
        
        for mesh in gltf.meshes:
            for primitive in mesh.primitives:
                # Get vertex positions
                if primitive.attributes.POSITION is not None:
                    accessor = gltf.accessors[primitive.attributes.POSITION]
                    buffer_view = gltf.bufferViews[accessor.bufferView]
                    buffer = gltf.buffers[buffer_view.buffer]
                    
                    # Read binary data
                    data = gltf.get_data_from_buffer_uri(buffer.uri) if buffer.uri else gltf.binary_blob()
                    
                    # Parse vertices
                    vertex_data = np.frombuffer(
                        data[buffer_view.byteOffset:buffer_view.byteOffset + buffer_view.byteLength],
                        dtype=np.float32
                    ).reshape(-1, 3)
                    
                    base_vertex = len(obj_vertices)
                    obj_vertices.extend([(v[0], v[1], v[2]) for v in vertex_data])
                
                # Get UVs
                if primitive.attributes.TEXCOORD_0 is not None:
                    accessor = gltf.accessors[primitive.attributes.TEXCOORD_0]
                    buffer_view = gltf.bufferViews[accessor.bufferView]
                    buffer = gltf.buffers[buffer_view.buffer]
                    data = gltf.get_data_from_buffer_uri(buffer.uri) if buffer.uri else gltf.binary_blob()
                    
                    uv_data = np.frombuffer(
                        data[buffer_view.byteOffset:buffer_view.byteOffset + buffer_view.byteLength],
                        dtype=np.float32
                    ).reshape(-1, 2)
                    
                    obj_texcoords.extend([(uv[0], uv[1]) for uv in uv_data])
                
                # Get indices (triangles)
                if primitive.indices is not None:
                    accessor = gltf.accessors[primitive.indices]
                    buffer_view = gltf.bufferViews[accessor.bufferView]
                    buffer = gltf.buffers[buffer_view.buffer]
                    data = gltf.get_data_from_buffer_uri(buffer.uri) if buffer.uri else gltf.binary_blob()
                    
                    # Determine index type
                    if accessor.componentType == 5123:  # UNSIGNED_SHORT
                        indices = np.frombuffer(
                            data[buffer_view.byteOffset:buffer_view.byteOffset + buffer_view.byteLength],
                            dtype=np.uint16
                        )
                    else:  # UNSIGNED_INT
                        indices = np.frombuffer(
                            data[buffer_view.byteOffset:buffer_view.byteOffset + buffer_view.byteLength],
                            dtype=np.uint32
                        )
                    
                    # Create triangles
                    mat_name = None
                    if primitive.material is not None and gltf.materials:
                        mat_name = gltf.materials[primitive.material].name
                    
                    for i in range(0, len(indices), 3):
                        idx1 = base_vertex + indices[i]
                        idx2 = base_vertex + indices[i+1]
                        idx3 = base_vertex + indices[i+2]
                        
                        # Store as (v_idx, vt_idx, vn_idx) format
                        self.triangles.append([
                            (idx1, idx1, 0),
                            (idx2, idx2, 0),
                            (idx3, idx3, 0)
                        ])
                        face_materials.append(mat_name)
        
        print(f"Loaded: {len(obj_vertices)} vertices, {len(obj_texcoords)} UVs, {len(self.triangles)} triangles")
        
        # Now process like OBJ
        return self._process_geometry(obj_vertices, obj_texcoords, face_materials)
    
    def _process_geometry(self, obj_vertices, obj_texcoords, face_materials):
        """Common geometry processing for both OBJ and GLB"""
        """Load OBJ file and extract geometry"""
        print(f"Loading OBJ file: {filepath}")
        
        # Try to find and load MTL file
        obj_dir = os.path.dirname(filepath)
        obj_name = os.path.splitext(os.path.basename(filepath))[0]
        mtl_path = os.path.join(obj_dir, obj_name + '.mtl')
        
        if os.path.exists(mtl_path):
            self.load_mtl(mtl_path)
        else:
            print(f"No MTL file found (looked for: {mtl_path})")
        
        obj_vertices = []
        obj_texcoords = []
        obj_normals = []
        current_material = None
        face_materials = []  # Track material for each face
        
        with open(filepath, 'r') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                    
                parts = line.split()
                if not parts:
                    continue
                    
                # Material usage
                if parts[0] == 'usemtl':
                    current_material = parts[1] if len(parts) > 1 else None
                    
                # Vertex positions
                elif parts[0] == 'v':
                    x, y, z = float(parts[1]), float(parts[2]), float(parts[3])
                    obj_vertices.append((x, y, z))
                    
                # Texture coordinates
                elif parts[0] == 'vt':
                    u, v = float(parts[1]), float(parts[2])
                    obj_texcoords.append((u, v))
                    
                # Normals
                elif parts[0] == 'vn':
                    nx, ny, nz = float(parts[1]), float(parts[2]), float(parts[3])
                    obj_normals.append((nx, ny, nz))
                    
                # Faces
                elif parts[0] == 'f':
                    # Parse face (supports v, v/vt, v/vt/vn, v//vn)
                    face_verts = []
                    for i in range(1, len(parts)):
                        indices = parts[i].split('/')
                        v_idx = int(indices[0]) - 1
                        vt_idx = int(indices[1]) - 1 if len(indices) > 1 and indices[1] else 0
                        vn_idx = int(indices[2]) - 1 if len(indices) > 2 and indices[2] else 0
                        face_verts.append((v_idx, vt_idx, vn_idx))
                    
                    # Triangulate if needed (assumes convex polygons)
                    for i in range(1, len(face_verts) - 1):
                        tri = [face_verts[0], face_verts[i], face_verts[i+1]]
                        self.triangles.append(tri)
                        face_materials.append(current_material)  # Track material for this triangle
        
        print(f"Loaded: {len(obj_vertices)} vertices, {len(obj_texcoords)} UVs, {len(self.triangles)} triangles")
        
        # Check if we have UVs
        has_uvs = len(obj_texcoords) > 0
        
        if not has_uvs:
            print("⚠ No UV coordinates found in OBJ file")
            print("✓ Generating automatic UVs for material atlas mapping...")
        
        # Build unique vertex list with UVs
        vertex_map = {}
        unique_vertices = []
        unique_texcoords = []
        triangle_indices = []
        
        # Calculate atlas UV coordinates for materials
        num_materials = max(len(self.materials), 1)
        cols = int(num_materials ** 0.5) + 1
        rows = (num_materials + cols - 1) // cols
        
        material_list = list(self.materials.keys()) if self.materials else []
        material_uvs = {}
        
        for idx, mat_name in enumerate(material_list):
            row = idx // cols
            col = idx % cols
            # Center of each cell in normalized UV space
            u = (col + 0.5) / cols
            v = (row + 0.5) / rows
            material_uvs[mat_name] = (u, v)
        
        default_uv = (0.5, 0.5)  # Center of texture if no materials
        
        for tri_idx, tri in enumerate(self.triangles):
            tri_indices = []
            tri_material = face_materials[tri_idx] if tri_idx < len(face_materials) else None
            
            # Determine UV for this entire triangle
            if tri_material and tri_material in material_uvs:
                tri_uv = material_uvs[tri_material]
            else:
                tri_uv = default_uv
            
            for v_idx, vt_idx, vn_idx in tri:
                # Determine UV coordinates for this vertex
                if has_uvs and vt_idx < len(obj_texcoords):
                    uv = obj_texcoords[vt_idx]
                    key = (v_idx, vt_idx)  # Original key with UV index
                else:
                    # Auto-generate UV - use triangle's material UV
                    uv = tri_uv
                    # Key includes UV to allow same vertex with different UVs
                    key = (v_idx, uv[0], uv[1])
                
                if key not in vertex_map:
                    vertex_map[key] = len(unique_vertices)
                    
                    # Convert from Y-up (OBJ) to Z-up (Quake/MD3)
                    vx, vy, vz = obj_vertices[v_idx]
                    converted_vertex = (vx, vz, vy)  # Swap Y and Z (wheels on ground)
                    unique_vertices.append(converted_vertex)
                    unique_texcoords.append(uv)
                    
                tri_indices.append(vertex_map[key])
            triangle_indices.append(tri_indices)
        
        self.vertices = unique_vertices
        self.texcoords = unique_texcoords
        self.triangles = triangle_indices
        self.face_materials = face_materials  # Store for texture baking
        
        print(f"Unique vertices: {len(self.vertices)}")
    
    def generate_texture_atlas(self, output_path, atlas_size=512, use_uv_layout=True):
        """Generate a texture atlas from material colors"""
        if not PIL_AVAILABLE:
            print("Cannot generate texture atlas: PIL/Pillow not installed")
            return None
        
        if not self.materials:
            print("No materials found, cannot generate atlas")
            return None
        
        print(f"Generating texture atlas with {len(self.materials)} materials...")
        
        # Check if we have UVs for UV-based baking
        has_uvs = len(self.texcoords) > 10  # More than just a few UVs
        
        if use_uv_layout and has_uvs:
            print(f"✓ Using UV layout baking ({len(self.texcoords)} UVs found)")
            return self._bake_uv_texture(output_path, atlas_size)
        else:
            print(f"⚠ Using simple color grid (no UVs or disabled)")
            return self._generate_color_grid(output_path, atlas_size)
    
    def _bake_uv_texture(self, output_path, atlas_size):
        """Bake texture using UV coordinates - renders each triangle at its UV position"""
        from PIL import ImageDraw
        
        # Create image with background color
        img = Image.new('RGB', (atlas_size, atlas_size), color=(128, 128, 128))
        draw = ImageDraw.Draw(img)
        
        # Build material lookup
        material_colors = {name: data['color'] for name, data in self.materials.items()}
        
        # Track which material each triangle uses (from OBJ parsing)
        # We need to re-parse to get material assignments
        # For now, use a simple approach: render all triangles with their vertex UVs
        
        print(f"Baking {len(self.triangles)} triangles to texture...")
        
        # Render each triangle at its UV position
        for tri_idx, tri in enumerate(self.triangles):
            # Get triangle indices
            idx1, idx2, idx3 = tri
            
            # Get UV coordinates (already in 0-1 range)
            if idx1 < len(self.texcoords) and idx2 < len(self.texcoords) and idx3 < len(self.texcoords):
                u1, v1 = self.texcoords[idx1]
                u2, v2 = self.texcoords[idx2]
                u3, v3 = self.texcoords[idx3]
                
                # Convert to pixel coordinates
                x1, y1 = int(u1 * atlas_size), int((1.0 - v1) * atlas_size)
                x2, y2 = int(u2 * atlas_size), int((1.0 - v2) * atlas_size)
                x3, y3 = int(u3 * atlas_size), int((1.0 - v3) * atlas_size)
                
                # Get material color for this triangle
                tri_material = self.face_materials[tri_idx] if tri_idx < len(self.face_materials) else None
                if tri_material and tri_material in material_colors:
                    color = material_colors[tri_material]
                    # Avoid pure black (0,0,0) as it might be treated as transparent
                    if color == (0, 0, 0):
                        color = (1, 1, 1)  # Very dark gray instead of pure black
                else:
                    # Fallback: use gray for unmapped triangles
                    color = (128, 128, 128)
                    if tri_idx < 10:  # Debug first few unmapped triangles
                        print(f"⚠ Triangle {tri_idx}: material '{tri_material}' not found in {list(material_colors.keys())}")
                
                # Draw filled triangle
                draw.polygon([(x1, y1), (x2, y2), (x3, y3)], fill=color, outline=color)
        
        # Save
        img.save(output_path)
        print(f"✓ UV-baked texture generated: {output_path} ({atlas_size}x{atlas_size})")
        return output_path
    
    def _generate_color_grid(self, output_path, atlas_size):
        """Generate simple color grid atlas (fallback)"""
        from PIL import ImageDraw
        
        img = Image.new('RGB', (atlas_size, atlas_size), color=(255, 255, 255))
        draw = ImageDraw.Draw(img)
        
        if len(self.materials) == 1:
            mat_data = list(self.materials.values())[0]
            color = mat_data['color']
            draw.rectangle([0, 0, atlas_size, atlas_size], fill=color)
        else:
            num_materials = len(self.materials)
            cols = int(num_materials ** 0.5) + 1
            rows = (num_materials + cols - 1) // cols
            
            cell_w = atlas_size // cols
            cell_h = atlas_size // rows
            padding = 2
            
            for idx, (mat_name, mat_data) in enumerate(self.materials.items()):
                row = idx // cols
                col = idx % cols
                
                x1 = col * cell_w + padding
                y1 = row * cell_h + padding
                x2 = (col + 1) * cell_w - padding
                y2 = (row + 1) * cell_h - padding
                
                color = mat_data['color']
                draw.rectangle([x1, y1, x2, y2], fill=color)
        
        img.save(output_path)
        print(f"✓ Color grid atlas generated: {output_path} ({atlas_size}x{atlas_size})")
        return output_path
        
    def write_md3(self, output_path, surface_name="model", scale=1.0):
        """Write MD3 file"""
        print(f"Writing MD3 file: {output_path}")
        print(f"Scale factor: {scale}")
        
        with open(output_path, 'wb') as f:
            # MD3 Header
            f.write(b'IDP3')  # Magic
            f.write(struct.pack('<i', 15))  # Version
            f.write(surface_name.encode('ascii').ljust(64, b'\0'))  # Name
            f.write(struct.pack('<i', 0))  # Flags
            f.write(struct.pack('<i', 1))  # Num frames
            f.write(struct.pack('<i', 0))  # Num tags
            f.write(struct.pack('<i', 1))  # Num surfaces
            f.write(struct.pack('<i', 0))  # Num skins
            
            # Offsets (will be updated)
            ofs_frames = 108
            ofs_tags = ofs_frames + (56 * 1)  # 1 frame
            ofs_surfaces = ofs_tags
            ofs_eof = 0  # Will calculate
            
            f.write(struct.pack('<i', ofs_frames))
            f.write(struct.pack('<i', ofs_tags))
            f.write(struct.pack('<i', ofs_surfaces))
            f.write(struct.pack('<i', 0))  # ofs_eof (placeholder)
            
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
            surface_start = f.tell()
            f.write(b'IDP3')  # Magic
            f.write(surface_name.encode('ascii').ljust(64, b'\0'))
            f.write(struct.pack('<i', 0))  # Flags
            f.write(struct.pack('<i', 1))  # Num frames
            f.write(struct.pack('<i', 0))  # Num shaders
            f.write(struct.pack('<i', len(self.vertices)))  # Num verts
            f.write(struct.pack('<i', len(self.triangles)))  # Num triangles
            
            # Surface offsets
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
                f.write(struct.pack('<ff', u, 1.0 - v))  # Flip V for MD3
            
            # Vertices (compressed)
            for vx, vy, vz in self.vertices:
                # MD3 uses 1/64 scale compression
                x = int(vx * scale * 64.0)
                y = int(vy * scale * 64.0)
                z = int(vz * scale * 64.0)
                
                # Clamp to short range
                x = max(-32768, min(32767, x))
                y = max(-32768, min(32767, y))
                z = max(-32768, min(32767, z))
                
                f.write(struct.pack('<hhh', x, y, z))
                f.write(struct.pack('<BB', 0, 0))  # Normal (encoded, we'll use 0,0)
            
            # Update ofs_eof
            ofs_eof = f.tell()
            f.seek(104)
            f.write(struct.pack('<i', ofs_eof))
            
        print(f"MD3 file written successfully!")
        print(f"Vertices: {len(self.vertices)}, Triangles: {len(self.triangles)}")

def main():
    if len(sys.argv) < 2:
        print("Usage: python obj_to_md3.py <input.obj> [output.md3] [scale]")
        print("Example: python obj_to_md3.py car.obj car.md3 10.0")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else input_file.replace('.obj', '.md3')
    scale = float(sys.argv[3]) if len(sys.argv) > 3 else 1.0
    
    if not os.path.exists(input_file):
        print(f"Error: Input file '{input_file}' not found")
        sys.exit(1)
    
    converter = MD3Converter()
    converter.load_obj(input_file)
    converter.write_md3(output_file, surface_name=Path(input_file).stem, scale=scale)
    
    # Copy texture if found, or generate atlas from materials
    if converter.texture_file:
        output_dir = os.path.dirname(output_file) or '.'
        texture_name = os.path.basename(converter.texture_file)
        output_texture = os.path.join(output_dir, texture_name)
        
        try:
            shutil.copy2(converter.texture_file, output_texture)
            print(f"\n✓ Texture copied: {output_texture}")
        except Exception as e:
            print(f"\n⚠ Warning: Could not copy texture: {e}")
    elif converter.materials and PIL_AVAILABLE:
        # Generate texture atlas from material colors
        output_dir = os.path.dirname(output_file) or '.'
        model_name = Path(input_file).stem
        atlas_path = os.path.join(output_dir, f"{model_name}.png")
        
        generated = converter.generate_texture_atlas(atlas_path)
        if generated:
            print(f"✓ Use this texture in your game: {atlas_path}")
    else:
        print("\n⚠ No texture found in MTL file and no materials to generate atlas")
    
    print("\nConversion complete!")
    print(f"Input: {input_file}")
    print(f"Output: {output_file}")
    print(f"Scale: {scale}x")

if __name__ == "__main__":
    main()
