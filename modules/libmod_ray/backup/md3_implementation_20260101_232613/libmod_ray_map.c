/*
 * libmod_ray_map.c - Map Loading/Saving for Format v8
 * Complete rewrite - geometric sectors only
 */

#include "libmod_ray.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* External engine instance */
extern RAY_Engine g_engine;

/* External geometry functions */
extern void ray_detect_portals(RAY_Engine *engine);

/* ============================================================================
   MAP HEADER v8
   ============================================================================ */

typedef struct {
    char magic[8];              /* "RAYMAP\x1a" */
    uint32_t version;           /* 8 */
    uint32_t num_sectors;
    uint32_t num_portals;
    uint32_t num_sprites;
    uint32_t num_spawn_flags;
    float camera_x, camera_y, camera_z;
    float camera_rot, camera_pitch;
    int32_t skyTextureID;
} RAY_MapHeader_v8;

/* ============================================================================
   MAP LOADING
   ============================================================================ */

int ray_load_map_v8(const char *filename)
{
    printf("\n*** RAY_LOAD_MAP_V8 CALLED: %s ***\n", filename);
    
    FILE *file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "ERROR: No se pudo abrir el archivo: %s\n", filename);
        return 0;
    }
    
    /* Read header */
    RAY_MapHeader_v8 header;
    if (fread(&header, sizeof(RAY_MapHeader_v8), 1, file) != 1) {
        fprintf(stderr, "ERROR: No se pudo leer el header\n");
        fclose(file);
        return 0;
    }
    
    /* Verify magic number */
    if (memcmp(header.magic, "RAYMAP\x1a", 7) != 0) {
        fprintf(stderr, "ERROR: Formato de archivo inválido\n");
        fclose(file);
        return 0;
    }
    
    /* Verify version */
    if (header.version < 8 || header.version > 9) {
        fprintf(stderr, "ERROR: Versión no soportada: %u (solo v8-v9)\n", header.version);
        fclose(file);
        return 0;
    }
    
    printf("Cargando mapa v%u: %u sectores, %u portales, %u sprites\n",
           header.version, header.num_sectors, header.num_portals, header.num_sprites);
    
    /* Set camera */
    g_engine.camera.x = header.camera_x;
    g_engine.camera.y = header.camera_y;
    g_engine.camera.z = header.camera_z;
    g_engine.camera.rot = header.camera_rot;
    g_engine.camera.pitch = header.camera_pitch;
    g_engine.camera.current_sector_id = -1;  /* Will be determined on first frame */
    
    printf("DEBUG: Cámara leída del archivo: pos=(%.1f, %.1f, %.1f) rot=%.2f\n",
           header.camera_x, header.camera_y, header.camera_z, header.camera_rot);
    
    g_engine.skyTextureID = header.skyTextureID;
    
    /* Allocate sectors */
    /* FIX: Allocate MAX sectors to handle sparse IDs (e.g. ID 11 when only 10 sectors exist) */
    g_engine.sectors_capacity = RAY_MAX_SECTORS; // Fixed size 500
    g_engine.sectors = (RAY_Sector*)malloc(g_engine.sectors_capacity * sizeof(RAY_Sector));
    
    // Initialize IDs to -1 to detect empty slots
    for(int k=0; k<g_engine.sectors_capacity; k++) {
        memset(&g_engine.sectors[k], 0, sizeof(RAY_Sector));
        g_engine.sectors[k].sector_id = -1;
    }
    
    // Set num_sectors to MAX so loops check all possible IDs
    // Ideally we should track max_id + 1, but MAX is safer for now to avoid complexity.
    g_engine.num_sectors = RAY_MAX_SECTORS; 
    
    /* Load sectors */
    for (uint32_t i = 0; i < header.num_sectors; i++) {
        /* Read ID first to determine slot */
        int temp_id;
        fread(&temp_id, sizeof(int), 1, file);
        
        if (temp_id < 0 || temp_id >= RAY_MAX_SECTORS) {
            fprintf(stderr, "ERROR: Sector ID %d out of bounds (MAX %d)\n", temp_id, RAY_MAX_SECTORS);
            continue; // Skip or abort? Skip safest.
        }
        
        RAY_Sector *sector = &g_engine.sectors[temp_id];
        // memset(sector, 0, sizeof(RAY_Sector)); // Already zeroed
        
        sector->sector_id = temp_id;
        
        /* Read sector properties */
        // ID already read
        fread(&sector->floor_z, sizeof(float), 1, file);
        fread(&sector->ceiling_z, sizeof(float), 1, file);
        fread(&sector->floor_texture_id, sizeof(int), 1, file);
        fread(&sector->ceiling_texture_id, sizeof(int), 1, file);
        fread(&sector->light_level, sizeof(int), 1, file);
        
        printf("DEBUG SECTOR %d: floor_z=%.1f, ceil_z=%.1f, floor_tex=%d, ceil_tex=%d\n",
               sector->sector_id, sector->floor_z, sector->ceiling_z,
               sector->floor_texture_id, sector->ceiling_texture_id);
        
        /* Read vertices */
        uint32_t num_vertices;
        fread(&num_vertices, sizeof(uint32_t), 1, file);
        
        /* FIX: Allocate exactly what we need, or at least MAX */
        sector->vertices_capacity = (num_vertices > RAY_MAX_VERTICES_PER_SECTOR) ? num_vertices : RAY_MAX_VERTICES_PER_SECTOR;
        sector->vertices = (RAY_Point*)malloc(sector->vertices_capacity * sizeof(RAY_Point));
        sector->num_vertices = num_vertices;
        
        for (uint32_t v = 0; v < num_vertices; v++) {
            fread(&sector->vertices[v].x, sizeof(float), 1, file);
            fread(&sector->vertices[v].y, sizeof(float), 1, file);
        }
        
        /* Read walls */
        uint32_t num_walls;
        fread(&num_walls, sizeof(uint32_t), 1, file);
        
        /* FIX: Allocate exactly what we need, or at least MAX */
        sector->walls_capacity = (num_walls > RAY_MAX_WALLS_PER_SECTOR) ? num_walls : RAY_MAX_WALLS_PER_SECTOR;
        sector->walls = (RAY_Wall*)malloc(sector->walls_capacity * sizeof(RAY_Wall));
        sector->num_walls = num_walls;
        
        for (uint32_t w = 0; w < num_walls; w++) {
            RAY_Wall *wall = &sector->walls[w];
            
            fread(&wall->wall_id, sizeof(int), 1, file);
            fread(&wall->x1, sizeof(float), 1, file);
            fread(&wall->y1, sizeof(float), 1, file);
            fread(&wall->x2, sizeof(float), 1, file);
            fread(&wall->y2, sizeof(float), 1, file);
            fread(&wall->texture_id_lower, sizeof(int), 1, file);
            fread(&wall->texture_id_middle, sizeof(int), 1, file);
            fread(&wall->texture_id_upper, sizeof(int), 1, file);
            fread(&wall->texture_split_z_lower, sizeof(float), 1, file);
            fread(&wall->texture_split_z_upper, sizeof(float), 1, file);
            fread(&wall->portal_id, sizeof(int), 1, file);
            fread(&wall->flags, sizeof(int), 1, file);
        }
        
        /* Allocate portal IDs array (always needed) */
        sector->portals_capacity = RAY_MAX_WALLS_PER_SECTOR;
        /* Ensure portals capacity is at least num_walls since potentially every wall could be a portal */
        if (num_walls > sector->portals_capacity) sector->portals_capacity = num_walls;
        
        sector->portal_ids = (int*)malloc(sector->portals_capacity * sizeof(int));
        sector->num_portals = 0;  /* Will be filled by portal detection */
        
        /* Version 9+: Load nested sector data */
        if (header.version >= 9) {
            printf("DEBUG: Reading v9 data for sector %d...\n", sector->sector_id);
            
            fread(&sector->parent_sector_id, sizeof(int), 1, file);
            printf("  parent_sector_id = %d\n", sector->parent_sector_id);
            
            int32_t sector_type_int;
            fread(&sector_type_int, sizeof(int32_t), 1, file);
            sector->sector_type = sector_type_int;
            printf("  sector_type = %d\n", sector_type_int);
            
            /* CRITICAL: Read as bool (1 byte), not int (4 bytes)! */
            uint8_t is_solid_byte;
            fread(&is_solid_byte, sizeof(uint8_t), 1, file);
            sector->is_solid = is_solid_byte;
            printf("  is_solid = %d\n", is_solid_byte);
            
            fread(&sector->nesting_level, sizeof(int), 1, file);
            printf("  nesting_level = %d\n", sector->nesting_level);
            
            /* Read child IDs */
            uint32_t num_children;
            fread(&num_children, sizeof(uint32_t), 1, file);
            printf("  num_children = %u\n", num_children);
            
            /* SAFETY CHECK: Cap num_children to avoid overflow/allocation errors if data is corrupt */
            if (num_children > 1000) {
                printf("  WARNING: Suspiciously high num_children (%u). Clamping to 0 to prevent crash.\n", num_children);
                num_children = 0;
            }
            
            /* Allocate BEFORE reading */
            sector->children_capacity = RAY_MAX_WALLS_PER_SECTOR; 
            if (num_children > sector->children_capacity) {
                 sector->children_capacity = num_children; // Ensure we allocate enough
            }
            
            printf("  Allocating child_sector_ids array (%d * %zu bytes)...\n", 
                   sector->children_capacity, sizeof(int));
            sector->child_sector_ids = (int*)malloc(sector->children_capacity * sizeof(int));
            if (!sector->child_sector_ids) {
                fprintf(stderr, "ERROR: Failed to allocate child_sector_ids!\n");
                fclose(file);
                return 0;
            }
            sector->num_children = num_children;
            
            for (uint32_t c = 0; c < num_children; c++) {
                fread(&sector->child_sector_ids[c], sizeof(int), 1, file);
                printf("    child[%u] = %d\n", c, sector->child_sector_ids[c]);
            }
            
            printf("DEBUG: Finished reading v9 data for sector %d. Parent=%d, NumChildren=%d, Solid=%d\n", 
                   sector->sector_id, sector->parent_sector_id, sector->num_children, sector->is_solid);
        } else {
            /* Version 8: Initialize with default values */
            sector->parent_sector_id = -1;
            sector->children_capacity = RAY_MAX_WALLS_PER_SECTOR;
            sector->child_sector_ids = (int*)malloc(sector->children_capacity * sizeof(int));
            sector->num_children = 0;
            sector->sector_type = RAY_SECTOR_ROOT;
            sector->is_solid = 0;
            sector->nesting_level = 0;
        }
        
        // g_engine.num_sectors++; (Handled by fixed MAX size)
    }
    
    /* Allocate portals */
    printf("DEBUG: Allocating portals array (%u portals)...\n", header.num_portals);
    g_engine.portals_capacity = header.num_portals;
    g_engine.portals = (RAY_Portal*)malloc(g_engine.portals_capacity * sizeof(RAY_Portal));
    if (!g_engine.portals) {
        fprintf(stderr, "ERROR: Failed to allocate portals array!\n");
        fclose(file);
        return 0;
    }
    g_engine.num_portals = 0;
    
    /* Load portals */
    printf("DEBUG: Loading %u portals...\n", header.num_portals);
    for (uint32_t i = 0; i < header.num_portals; i++) {
        RAY_Portal *portal = &g_engine.portals[g_engine.num_portals];
        memset(portal, 0, sizeof(RAY_Portal));
        
        fread(&portal->portal_id, sizeof(int), 1, file);
        fread(&portal->sector_a, sizeof(int), 1, file);
        fread(&portal->sector_b, sizeof(int), 1, file);
        fread(&portal->wall_id_a, sizeof(int), 1, file);
        fread(&portal->wall_id_b, sizeof(int), 1, file);
        fread(&portal->x1, sizeof(float), 1, file);
        fread(&portal->y1, sizeof(float), 1, file);
        fread(&portal->x2, sizeof(float), 1, file);
        fread(&portal->y2, sizeof(float), 1, file);
        
        if (i < 5) {  // Debug first 5 portals
            printf("  Portal %u: id=%d, sectors %d<->%d\n", 
                   i, portal->portal_id, portal->sector_a, portal->sector_b);
        }
        
        /* Add portal to sectors */
        for (int s = 0; s < g_engine.num_sectors; s++) {
            RAY_Sector *sector = &g_engine.sectors[s];
            if (sector->sector_id == portal->sector_a || sector->sector_id == portal->sector_b) {
                if (sector->num_portals < sector->portals_capacity) {
                    sector->portal_ids[sector->num_portals++] = portal->portal_id;
                }
            }
        }
        
        g_engine.num_portals++;
    }
    
    /* Load sprites */
    g_engine.num_sprites = 0;
    for (uint32_t i = 0; i < header.num_sprites; i++) {
        if (g_engine.num_sprites >= g_engine.sprites_capacity) break;
        
        RAY_Sprite *sprite = &g_engine.sprites[g_engine.num_sprites];
        memset(sprite, 0, sizeof(RAY_Sprite));
        
        fread(&sprite->textureID, sizeof(int), 1, file);
        fread(&sprite->x, sizeof(float), 1, file);
        fread(&sprite->y, sizeof(float), 1, file);
        fread(&sprite->z, sizeof(float), 1, file);
        fread(&sprite->w, sizeof(int), 1, file);
        fread(&sprite->h, sizeof(int), 1, file);
        fread(&sprite->rot, sizeof(float), 1, file);
        
        sprite->dir = 1;
        sprite->speed = 0;
        sprite->moveSpeed = 0;
        sprite->rotSpeed = 0.0f;
        sprite->process_ptr = NULL;
        sprite->flag_id = -1;
        sprite->cleanup = 0;
        sprite->hidden = 0;
        
        g_engine.num_sprites++;
    }
    
    /* Load spawn flags */
    g_engine.num_spawn_flags = 0;
    for (uint32_t i = 0; i < header.num_spawn_flags; i++) {
        if (g_engine.num_spawn_flags >= g_engine.spawn_flags_capacity) break;
        
        RAY_SpawnFlag *flag = &g_engine.spawn_flags[g_engine.num_spawn_flags];
        memset(flag, 0, sizeof(RAY_SpawnFlag));
        
        fread(&flag->flag_id, sizeof(int), 1, file);
        fread(&flag->x, sizeof(float), 1, file);
        fread(&flag->y, sizeof(float), 1, file);
        fread(&flag->z, sizeof(float), 1, file);
        
        flag->occupied = 0;
        flag->process_ptr = NULL;
        
        g_engine.num_spawn_flags++;
    }
    
    fclose(file);
    
    /* Detect which sector the camera is in */
    RAY_Sector *camera_sector = ray_find_sector_at_point(&g_engine, g_engine.camera.x, g_engine.camera.y);
    if (camera_sector) {
        g_engine.camera.current_sector_id = camera_sector->sector_id;
        printf("RAY: Cámara en sector %d\n", camera_sector->sector_id);
        
        /* Calculate sector bounds to check if camera is too close to edges */
        float min_x = FLT_MAX, max_x = -FLT_MAX;
        float min_y = FLT_MAX, max_y = -FLT_MAX;
        
        for (int w = 0; w < camera_sector->num_walls; w++) {
            RAY_Wall *wall = &camera_sector->walls[w];
            if (wall->x1 < min_x) min_x = wall->x1;
            if (wall->x1 > max_x) max_x = wall->x1;
            if (wall->x2 < min_x) min_x = wall->x2;
            if (wall->x2 > max_x) max_x = wall->x2;
            if (wall->y1 < min_y) min_y = wall->y1;
            if (wall->y1 > max_y) max_y = wall->y1;
            if (wall->y2 < min_y) min_y = wall->y2;
            if (wall->y2 > max_y) max_y = wall->y2;
        }
        
        float center_x = (min_x + max_x) / 2.0f;
        float center_y = (min_y + max_y) / 2.0f;
        float margin = 50.0f; /* Minimum distance from edges */
        
        /* Check if camera is too close to any edge */
        if (g_engine.camera.x < min_x + margin || g_engine.camera.x > max_x - margin ||
            g_engine.camera.y < min_y + margin || g_engine.camera.y > max_y - margin) {
            
            printf("RAY: Cámara muy cerca del borde. Recentrando...\n");
            printf("RAY: Bounds: X[%.1f, %.1f] Y[%.1f, %.1f]\n", min_x, max_x, min_y, max_y);
            printf("RAY: Posición anterior: (%.1f, %.1f)\n", g_engine.camera.x, g_engine.camera.y);
            printf("RAY: Nueva posición (centro): (%.1f, %.1f)\n", center_x, center_y);
            
            g_engine.camera.x = center_x;
            g_engine.camera.y = center_y;
        }
        
        /* FIX: Auto-adjust Z to be at eye level above the floor */
        /* If camera Z is exactly 0 (default) or below floor, snap to floor + 64 */
        if (g_engine.camera.z == 0 || g_engine.camera.z < camera_sector->floor_z) {
            float old_z = g_engine.camera.z;
            g_engine.camera.z = camera_sector->floor_z + 64.0f; /* Eye height */
            printf("RAY: Ajustando altura cámara de %.1f a %.1f (Floor %.1f + 64)\n", 
                   old_z, g_engine.camera.z, camera_sector->floor_z);
        }
    } else {
        printf("RAY: ADVERTENCIA - Cámara no está dentro de ningún sector!\n");
        printf("RAY: Posición de cámara: (%.1f, %.1f)\n", g_engine.camera.x, g_engine.camera.y);
        
        /* Auto-position camera at center of first sector */
        if (g_engine.num_sectors > 0) {
            RAY_Sector *first_sector = &g_engine.sectors[0];
            
            /* Calculate bounding box of sector */
            float min_x = FLT_MAX, max_x = -FLT_MAX;
            float min_y = FLT_MAX, max_y = -FLT_MAX;
            
            for (int w = 0; w < first_sector->num_walls; w++) {
                RAY_Wall *wall = &first_sector->walls[w];
                if (wall->x1 < min_x) min_x = wall->x1;
                if (wall->x1 > max_x) max_x = wall->x1;
                if (wall->x2 < min_x) min_x = wall->x2;
                if (wall->x2 > max_x) max_x = wall->x2;
                if (wall->y1 < min_y) min_y = wall->y1;
                if (wall->y1 > max_y) max_y = wall->y1;
                if (wall->y2 < min_y) min_y = wall->y2;
                if (wall->y2 > max_y) max_y = wall->y2;
            }
            
            float center_x = (min_x + max_x) / 2.0f;
            float center_y = (min_y + max_y) / 2.0f;
            
            printf("RAY: Sector 0 bounds: X[%.1f, %.1f] Y[%.1f, %.1f]\n", min_x, max_x, min_y, max_y);
            printf("RAY: Calculado centro: (%.1f, %.1f)\n", center_x, center_y);
            
            /* Position camera at center, eye level (64 units above floor) */
            g_engine.camera.x = center_x;
            g_engine.camera.y = center_y;
            g_engine.camera.z = first_sector->floor_z + 64.0f;  // Eye level
            g_engine.camera.current_sector_id = 0;
            
            printf("RAY: Cámara reposicionada automáticamente a (%.1f, %.1f, %.1f) en sector 0\n",
                   g_engine.camera.x, g_engine.camera.y, g_engine.camera.z);
        } else {
            g_engine.camera.current_sector_id = -1;
        }
    }
    
    printf("Mapa cargado: %d sectores, %d portales, %d sprites, %d spawn flags\n",
           g_engine.num_sectors, g_engine.num_portals,
           g_engine.num_sprites, g_engine.num_spawn_flags);
    
    return 1;
}

/* ============================================================================
   MAP SAVING
   ============================================================================ */

int ray_save_map_v8(const char *filename)
{
    FILE *file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "ERROR: No se pudo crear el archivo: %s\n", filename);
        return 0;
    }
    
    /* Prepare header */
    RAY_MapHeader_v8 header;
    memcpy(header.magic, "RAYMAP\x1a", 7);
    header.magic[7] = 0;
    header.version = 9;  /* Changed to v9 to match nested sector support */
    header.num_sectors = g_engine.num_sectors;
    header.num_portals = g_engine.num_portals;
    header.num_sprites = g_engine.num_sprites;
    header.num_spawn_flags = g_engine.num_spawn_flags;
    header.camera_x = g_engine.camera.x;
    header.camera_y = g_engine.camera.y;
    header.camera_z = g_engine.camera.z;
    header.camera_rot = g_engine.camera.rot;
    header.camera_pitch = g_engine.camera.pitch;
    header.skyTextureID = g_engine.skyTextureID;
    
    /* Write header */
    fwrite(&header, sizeof(RAY_MapHeader_v8), 1, file);
    
    /* Write sectors */
    for (int i = 0; i < g_engine.num_sectors; i++) {
        RAY_Sector *sector = &g_engine.sectors[i];
        
        fwrite(&sector->sector_id, sizeof(int), 1, file);
        fwrite(&sector->floor_z, sizeof(float), 1, file);
        fwrite(&sector->ceiling_z, sizeof(float), 1, file);
        fwrite(&sector->floor_texture_id, sizeof(int), 1, file);
        fwrite(&sector->ceiling_texture_id, sizeof(int), 1, file);
        fwrite(&sector->light_level, sizeof(int), 1, file);
        
        /* Write vertices */
        uint32_t num_vertices = sector->num_vertices;
        fwrite(&num_vertices, sizeof(uint32_t), 1, file);
        for (int v = 0; v < sector->num_vertices; v++) {
            fwrite(&sector->vertices[v].x, sizeof(float), 1, file);
            fwrite(&sector->vertices[v].y, sizeof(float), 1, file);
        }
        
        /* Write walls */
        uint32_t num_walls = sector->num_walls;
        fwrite(&num_walls, sizeof(uint32_t), 1, file);
        for (int w = 0; w < sector->num_walls; w++) {
            RAY_Wall *wall = &sector->walls[w];
            
            fwrite(&wall->wall_id, sizeof(int), 1, file);
            fwrite(&wall->x1, sizeof(float), 1, file);
            fwrite(&wall->y1, sizeof(float), 1, file);
            fwrite(&wall->x2, sizeof(float), 1, file);
            fwrite(&wall->y2, sizeof(float), 1, file);
            fwrite(&wall->texture_id_lower, sizeof(int), 1, file);
            fwrite(&wall->texture_id_middle, sizeof(int), 1, file);
            fwrite(&wall->texture_id_upper, sizeof(int), 1, file);
            fwrite(&wall->texture_split_z_lower, sizeof(float), 1, file);
            fwrite(&wall->texture_split_z_upper, sizeof(float), 1, file);
            fwrite(&wall->portal_id, sizeof(int), 1, file);
            fwrite(&wall->flags, sizeof(int), 1, file);
        }
        
        /* Write v9 nested sector data */
        fwrite(&sector->parent_sector_id, sizeof(int), 1, file);
        
        int32_t sector_type_int = sector->sector_type;
        fwrite(&sector_type_int, sizeof(int32_t), 1, file);
        
        /* CRITICAL: Write as bool (1 byte), not int (4 bytes)! */
        uint8_t is_solid_byte = sector->is_solid;
        fwrite(&is_solid_byte, sizeof(uint8_t), 1, file);
        
        fwrite(&sector->nesting_level, sizeof(int), 1, file);
        
        /* Write child IDs */
        uint32_t num_children = sector->num_children;
        fwrite(&num_children, sizeof(uint32_t), 1, file);
        for (int c = 0; c < sector->num_children; c++) {
            fwrite(&sector->child_sector_ids[c], sizeof(int), 1, file);
        }
    }
    
    /* Write portals */
    for (int i = 0; i < g_engine.num_portals; i++) {
        RAY_Portal *portal = &g_engine.portals[i];
        
        fwrite(&portal->portal_id, sizeof(int), 1, file);
        fwrite(&portal->sector_a, sizeof(int), 1, file);
        fwrite(&portal->sector_b, sizeof(int), 1, file);
        fwrite(&portal->wall_id_a, sizeof(int), 1, file);
        fwrite(&portal->wall_id_b, sizeof(int), 1, file);
        fwrite(&portal->x1, sizeof(float), 1, file);
        fwrite(&portal->y1, sizeof(float), 1, file);
        fwrite(&portal->x2, sizeof(float), 1, file);
        fwrite(&portal->y2, sizeof(float), 1, file);
    }
    
    /* Write sprites */
    for (int i = 0; i < g_engine.num_sprites; i++) {
        RAY_Sprite *sprite = &g_engine.sprites[i];
        
        fwrite(&sprite->textureID, sizeof(int), 1, file);
        fwrite(&sprite->x, sizeof(float), 1, file);
        fwrite(&sprite->y, sizeof(float), 1, file);
        fwrite(&sprite->z, sizeof(float), 1, file);
        fwrite(&sprite->w, sizeof(int), 1, file);
        fwrite(&sprite->h, sizeof(int), 1, file);
        fwrite(&sprite->rot, sizeof(float), 1, file);
    }
    
    /* Write spawn flags */
    for (int i = 0; i < g_engine.num_spawn_flags; i++) {
        RAY_SpawnFlag *flag = &g_engine.spawn_flags[i];
        
        fwrite(&flag->flag_id, sizeof(int), 1, file);
        fwrite(&flag->x, sizeof(float), 1, file);
        fwrite(&flag->y, sizeof(float), 1, file);
        fwrite(&flag->z, sizeof(float), 1, file);
    }
    
    fclose(file);
    
    printf("Mapa guardado: %d sectores, %d portales\n",
           g_engine.num_sectors, g_engine.num_portals);
    
    return 1;
}
