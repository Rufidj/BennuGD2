// create_test_map.prg
// Crea un mapa de prueba simple en formato v8 para probar el motor geométrico
import "libmod_file";
import "libmod_misc";

PROCESS main()
PRIVATE
    int file_handle;
    string magic = "RAYMAP" + chr(26);  // Magic number
    int version = 8;
    
    // Camera data
    float cam_x = 384.0;
    float cam_y = 384.0;
    float cam_z = 0.0;
    float cam_rot = 0.0;
    int cam_enabled = 1;
    
    // Sector 1 - Cuadrado grande
    int sector1_id = 0;
    float sector1_floor_z = 0.0;
    float sector1_ceiling_z = 256.0;
    int sector1_floor_tex = 1;
    int sector1_ceiling_tex = 2;
    int sector1_light = 255;
    int sector1_num_vertices = 4;
    
    // Sector 2 - Cuadrado conectado
    int sector2_id = 1;
    float sector2_floor_z = 0.0;
    float sector2_ceiling_z = 256.0;
    int sector2_floor_tex = 3;
    int sector2_ceiling_tex = 4;
    int sector2_light = 255;
    int sector2_num_vertices = 4;
    
    int num_sectors = 2;
    int num_portals = 1;
    int num_sprites = 0;
    int num_spawn_flags = 0;
BEGIN
    say("Creando mapa de prueba test_v8.raymap...");
    
    file_handle = fopen("test_v8.raymap", O_WRITE);
    if (file_handle < 0)
        say("ERROR: No se pudo crear el archivo");
        return;
    end
    
    // Header
    fwrite(file_handle, &magic, 7);
    fwrite(file_handle, &version, 4);
    
    // Camera
    fwrite(file_handle, &cam_x, 4);
    fwrite(file_handle, &cam_y, 4);
    fwrite(file_handle, &cam_z, 4);
    fwrite(file_handle, &cam_rot, 4);
    fwrite(file_handle, &cam_enabled, 4);
    
    // Number of sectors
    fwrite(file_handle, &num_sectors, 4);
    
    // ===== SECTOR 1 =====
    fwrite(file_handle, &sector1_id, 4);
    fwrite(file_handle, &sector1_floor_z, 4);
    fwrite(file_handle, &sector1_ceiling_z, 4);
    fwrite(file_handle, &sector1_floor_tex, 4);
    fwrite(file_handle, &sector1_ceiling_tex, 4);
    fwrite(file_handle, &sector1_light, 4);
    
    // Vertices (cuadrado 256x256 centrado en 384,384)
    fwrite(file_handle, &sector1_num_vertices, 4);
    write_vertex(file_handle, 256.0, 256.0);  // Bottom-left
    write_vertex(file_handle, 512.0, 256.0);  // Bottom-right
    write_vertex(file_handle, 512.0, 512.0);  // Top-right
    write_vertex(file_handle, 256.0, 512.0);  // Top-left
    
    // Walls (4 paredes)
    fwrite(file_handle, &sector1_num_vertices, 4);  // num_walls = num_vertices
    write_wall(file_handle, 0, 256.0, 256.0, 512.0, 256.0, 5, 5, 5, 64.0, 192.0, -1);  // Bottom
    write_wall(file_handle, 1, 512.0, 256.0, 512.0, 512.0, 6, 6, 6, 64.0, 192.0, 0);   // Right (PORTAL)
    write_wall(file_handle, 2, 512.0, 512.0, 256.0, 512.0, 7, 7, 7, 64.0, 192.0, -1);  // Top
    write_wall(file_handle, 3, 256.0, 512.0, 256.0, 256.0, 8, 8, 8, 64.0, 192.0, -1);  // Left
    
    // ===== SECTOR 2 =====
    fwrite(file_handle, &sector2_id, 4);
    fwrite(file_handle, &sector2_floor_z, 4);
    fwrite(file_handle, &sector2_ceiling_z, 4);
    fwrite(file_handle, &sector2_floor_tex, 4);
    fwrite(file_handle, &sector2_ceiling_tex, 4);
    fwrite(file_handle, &sector2_light, 4);
    
    // Vertices (cuadrado 256x256 a la derecha del primero)
    fwrite(file_handle, &sector2_num_vertices, 4);
    write_vertex(file_handle, 512.0, 256.0);  // Bottom-left (compartido)
    write_vertex(file_handle, 768.0, 256.0);  // Bottom-right
    write_vertex(file_handle, 768.0, 512.0);  // Top-right
    write_vertex(file_handle, 512.0, 512.0);  // Top-left (compartido)
    
    // Walls (4 paredes)
    fwrite(file_handle, &sector2_num_vertices, 4);
    write_wall(file_handle, 4, 512.0, 256.0, 768.0, 256.0, 9, 9, 9, 64.0, 192.0, -1);   // Bottom
    write_wall(file_handle, 5, 768.0, 256.0, 768.0, 512.0, 10, 10, 10, 64.0, 192.0, -1); // Right
    write_wall(file_handle, 6, 768.0, 512.0, 512.0, 512.0, 11, 11, 11, 64.0, 192.0, -1); // Top
    write_wall(file_handle, 7, 512.0, 512.0, 512.0, 256.0, 12, 12, 12, 64.0, 192.0, 0);  // Left (PORTAL)
    
    // ===== PORTALS =====
    fwrite(file_handle, &num_portals, 4);
    write_portal(file_handle, 0, 0, 1, 1, 7, 512.0, 256.0, 512.0, 512.0);
    
    // ===== SPRITES =====
    fwrite(file_handle, &num_sprites, 4);
    
    // ===== SPAWN FLAGS =====
    fwrite(file_handle, &num_spawn_flags, 4);
    
    fclose(file_handle);
    
    say("¡Mapa creado exitosamente!");
    say("Ejecuta: bgdi test_geometric.prg");
    say("Asegúrate de tener textures.fpg con texturas 1-12");
END

FUNCTION write_vertex(int fh, float x, float y)
BEGIN
    fwrite(fh, &x, 4);
    fwrite(fh, &y, 4);
END

FUNCTION write_wall(int fh, int id, float x1, float y1, float x2, float y2, 
                    int tex_low, int tex_mid, int tex_up, 
                    float split_low, float split_up, int portal)
BEGIN
    fwrite(fh, &id, 4);
    fwrite(fh, &x1, 4);
    fwrite(fh, &y1, 4);
    fwrite(fh, &x2, 4);
    fwrite(fh, &y2, 4);
    fwrite(fh, &tex_low, 4);
    fwrite(fh, &tex_mid, 4);
    fwrite(fh, &tex_up, 4);
    fwrite(fh, &split_low, 4);
    fwrite(fh, &split_up, 4);
    fwrite(fh, &portal, 4);
    fwrite(fh, &id, 4);  // flags (reusing id as dummy)
END

FUNCTION write_portal(int fh, int id, int sector_a, int sector_b, 
                      int wall_a, int wall_b, float x1, float y1, float x2, float y2)
BEGIN
    fwrite(fh, &id, 4);
    fwrite(fh, &sector_a, 4);
    fwrite(fh, &sector_b, 4);
    fwrite(fh, &wall_a, 4);
    fwrite(fh, &wall_b, 4);
    fwrite(fh, &x1, 4);
    fwrite(fh, &y1, 4);
    fwrite(fh, &x2, 4);
    fwrite(fh, &y2, 4);
END
