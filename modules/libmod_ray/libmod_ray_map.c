/*
 * libmod_ray_map.c - Map Loading System
 * Implements loading of .raymap binary format (version 1 and 2)
 */

#include "libmod_ray.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern RAY_Engine g_engine;

/* ============================================================================
   MAP FILE FORMAT (.raymap)
   ============================================================================ */

typedef struct {
    char magic[8];           /* "RAYMAP\x1a" */
    uint32_t version;        /* 1, 2, 3, or 4 */
    uint32_t map_width;
    uint32_t map_height;
    uint32_t num_levels;     /* Typically 3 */
    uint32_t num_sprites;
    uint32_t num_thin_walls;
    uint32_t num_thick_walls;
    uint32_t num_spawn_flags; /* Version 3+ */
    /* Version 2+ adds camera fields: */
    float camera_x;
    float camera_y;
    float camera_z;
    float camera_rot;
    float camera_pitch;
    int32_t skyTextureID;    /* ID de textura para skybox (0 = sin skybox) */
} RAY_MapHeader;

/* ============================================================================
   MAP LOADING
   ============================================================================ */

int ray_load_map_from_file(const char *filename, int fpg_id)
{
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "RAY: No se puede abrir mapa: %s\n", filename);
        return 0;
    }
    
    /* Leer header base */
    RAY_MapHeader header;
    memset(&header, 0, sizeof(RAY_MapHeader));
    
    if (fread(header.magic, 1, 8, f) != 8 ||
        fread(&header.version, sizeof(uint32_t), 1, f) != 1 ||
        fread(&header.map_width, sizeof(uint32_t), 1, f) != 1 ||
        fread(&header.map_height, sizeof(uint32_t), 1, f) != 1 ||
        fread(&header.num_levels, sizeof(uint32_t), 1, f) != 1 ||
        fread(&header.num_sprites, sizeof(uint32_t), 1, f) != 1 ||
        fread(&header.num_thin_walls, sizeof(uint32_t), 1, f) != 1 ||
        fread(&header.num_thick_walls, sizeof(uint32_t), 1, f) != 1) {
        fprintf(stderr, "RAY: Error leyendo header del mapa\n");
        fclose(f);
        return 0;
    }
    
    /* Leer num_spawn_flags si es versión 3+ */
    if (header.version >= 3) {
        if (fread(&header.num_spawn_flags, sizeof(uint32_t), 1, f) != 1) {
            fprintf(stderr, "RAY: Error leyendo num_spawn_flags\n");
            fclose(f);
            return 0;
        }
    } else {
        header.num_spawn_flags = 0;
    }
    
    /* Verificar magic */
    if (memcmp(header.magic, "RAYMAP\x1a", 7) != 0) {
        fprintf(stderr, "RAY: Archivo no es un mapa válido\n");
        fclose(f);
        return 0;
    }
    
    /* Verificar versión */
    if (header.version < 1 || header.version > 4) {
        fprintf(stderr, "RAY: Versión de mapa no soportada: %u\n", header.version);
        fclose(f);
        return 0;
    }
    
    printf("RAY: Cargando mapa v%d %dx%d con %d niveles\n",
           header.version, header.map_width, header.map_height, header.num_levels);
    printf("RAY: Header completo:\n");
    printf("  - version: %u\n", header.version);
    printf("  - map_width: %u\n", header.map_width);
    printf("  - map_height: %u\n", header.map_height);
    printf("  - num_levels: %u\n", header.num_levels);
    printf("  - num_sprites: %u\n", header.num_sprites);
    printf("  - num_thin_walls: %u\n", header.num_thin_walls);
    printf("  - num_thick_walls: %u\n", header.num_thick_walls);
    printf("  - num_spawn_flags: %u\n", header.num_spawn_flags);
    
    /* Leer campos de cámara si es versión 2+ */
    if (header.version >= 2) {
        if (fread(&header.camera_x, sizeof(float), 1, f) != 1 ||
            fread(&header.camera_y, sizeof(float), 1, f) != 1 ||
            fread(&header.camera_z, sizeof(float), 1, f) != 1 ||
            fread(&header.camera_rot, sizeof(float), 1, f) != 1 ||
            fread(&header.camera_pitch, sizeof(float), 1, f) != 1 ||
            fread(&header.skyTextureID, sizeof(int32_t), 1, f) != 1) {
            fprintf(stderr, "RAY: Error leyendo datos de cámara/skybox\n");
            fclose(f);
            return 0;
        }
        
        /* Aplicar posición de cámara */
        g_engine.camera.x = header.camera_x;
        g_engine.camera.y = header.camera_y;
        g_engine.camera.z = header.camera_z;
        g_engine.camera.rot = header.camera_rot;
        g_engine.camera.pitch = header.camera_pitch;
        
        /* Aplicar skybox */
        g_engine.skyTextureID = header.skyTextureID;
        
        printf("RAY: Cámara cargada: (%.1f, %.1f, %.1f)\n",
               header.camera_x, header.camera_y, header.camera_z);
        printf("RAY: Skybox ID: %d\n", header.skyTextureID);
    }
    
    /* Crear grids del raycaster */
    ray_raycaster_create_grids(&g_engine.raycaster,
                               header.map_width,
                               header.map_height,
                               header.num_levels,
                               RAY_TILE_SIZE);
    
    /* Leer datos de grids */
    for (uint32_t level = 0; level < header.num_levels; level++) {
        size_t grid_size = header.map_width * header.map_height * sizeof(int);
        if (fread(g_engine.raycaster.grids[level], grid_size, 1, f) != 1) {
            fprintf(stderr, "RAY: Error leyendo grid nivel %d\n", level);
            fclose(f);
            return 0;
        }
        
        /* Contar celdas no vacías para detectar datos basura */
        int non_zero_count = 0;
        int corrupted_count = 0;
        int total_cells = header.map_width * header.map_height;
        for (int i = 0; i < total_cells; i++) {
            int val = g_engine.raycaster.grids[level][i];
            if (val != 0) {
                non_zero_count++;
                /* Detectar valores corruptos (IDs de textura < 2000 para incluir todas las puertas) */
                if (val < 0 || val > 2000) {
                    printf("RAY: ADVERTENCIA - Valor corrupto detectado en grid %d[%d]: %d\n", 
                           level, i, val);
                    g_engine.raycaster.grids[level][i] = 0;
                    corrupted_count++;
                    non_zero_count--;
                }
            }
        }
        
        
        if (corrupted_count > 0) {
            printf("RAY: Limpiados %d valores corruptos en grid nivel %d\n", 
                   corrupted_count, level);
        }
        
        /* DESHABILITADO: No limpiar grids automáticamente basándose en cantidad de celdas
         * Esto eliminaba datos válidos como torres pequeñas en nivel 2
        if (level > 0 && non_zero_count > 0 && non_zero_count < 10) {
            printf("RAY: ADVERTENCIA - Grid nivel %d tiene solo %d celdas con datos, limpiando...\n", 
                   level, non_zero_count);
            memset(g_engine.raycaster.grids[level], 0, grid_size);
            non_zero_count = 0;
        }
        */
        
        /* DEBUG: Contar y mostrar puertas */
        int door_count = 0;
        for (int i = 0; i < total_cells; i++) {
            int val = g_engine.raycaster.grids[level][i];
            if (val >= 1001 && val <= 2000) {
                door_count++;
                if (door_count <= 5) {
                    int x = i % header.map_width;
                    int y = i / header.map_width;
                    printf("RAY: Puerta encontrada en nivel %d: pos(%d,%d) ID=%d\n", 
                           level, x, y, val);
                }
            }
        }
        if (door_count > 0) {
            printf("RAY: Total de puertas en nivel %d: %d\n", level, door_count);
        }
        
        printf("RAY: Grid nivel %d - %d celdas con paredes\n", level, non_zero_count);
    }
    
    /* Leer sprites */
    g_engine.num_sprites = 0;
    for (uint32_t i = 0; i < header.num_sprites && i < RAY_MAX_SPRITES; i++) {
        RAY_Sprite sprite;
        
        /* Leer datos del sprite */
        if (fread(&sprite.textureID, sizeof(int), 1, f) != 1 ||
            fread(&sprite.x, sizeof(float), 1, f) != 1 ||
            fread(&sprite.y, sizeof(float), 1, f) != 1 ||
            fread(&sprite.z, sizeof(float), 1, f) != 1 ||
            fread(&sprite.w, sizeof(int), 1, f) != 1 ||
            fread(&sprite.h, sizeof(int), 1, f) != 1 ||
            fread(&sprite.level, sizeof(int), 1, f) != 1 ||
            fread(&sprite.rot, sizeof(float), 1, f) != 1) {
            fprintf(stderr, "RAY: Error leyendo sprite %d\n", i);
            fclose(f);
            return 0;
        }
        
        /* Inicializar resto de campos */
        sprite.dir = 0;
        sprite.speed = 0;
        sprite.moveSpeed = 0;
        sprite.rotSpeed = 0;
        sprite.distance = 0;
        sprite.cleanup = 0;
        sprite.frameRate = 0;
        sprite.frame = 0;
        sprite.hidden = 0;
        sprite.jumping = 0;
        sprite.heightJumped = 0;
        sprite.rayhit = 0;
        
        g_engine.sprites[g_engine.num_sprites++] = sprite;
    }
    
    /* Saltar ThinWalls standalone (el editor no los maneja) */
    for (uint32_t i = 0; i < header.num_thin_walls; i++) {
        /* Cada thin wall: x1, y1, x2, y2, wallType, horizontal, height, z, slope, hidden */
        /* = 4 floats + 3 ints + 3 floats + 1 int = 8 floats + 4 ints = 48 bytes */
        fseek(f, 48, SEEK_CUR);
    }
    
    /* Leer ThickWalls */
    g_engine.num_thick_walls = 0;
    for (uint32_t i = 0; i < header.num_thick_walls && i < RAY_MAX_THICK_WALLS; i++) {
        RAY_ThickWall *tw = (RAY_ThickWall*)malloc(sizeof(RAY_ThickWall));
        if (!tw) continue;
        
        ray_thick_wall_init(tw);
        
        /* Leer campos básicos - IMPORTANTE: el editor escribe 'slope' ANTES de x,y,w,h */
        if (fread(&tw->type, sizeof(int), 1, f) != 1 ||
            fread(&tw->slopeType, sizeof(int), 1, f) != 1 ||
            fread(&tw->slope, sizeof(float), 1, f) != 1 ||
            fread(&tw->x, sizeof(float), 1, f) != 1 ||
            fread(&tw->y, sizeof(float), 1, f) != 1 ||
            fread(&tw->w, sizeof(float), 1, f) != 1 ||
            fread(&tw->h, sizeof(float), 1, f) != 1 ||
            fread(&tw->ceilingTextureID, sizeof(int), 1, f) != 1 ||
            fread(&tw->floorTextureID, sizeof(int), 1, f) != 1 ||
            fread(&tw->startHeight, sizeof(float), 1, f) != 1 ||
            fread(&tw->endHeight, sizeof(float), 1, f) != 1 ||
            fread(&tw->invertedSlope, sizeof(int), 1, f) != 1) {
            fprintf(stderr, "RAY: Error leyendo thick wall %d\n", i);
            free(tw);
            fclose(f);
            return 0;
        }
        
        /* Leer puntos si es TRIANGLE o QUAD */
        if (tw->type == 2 || tw->type == 3) {  // TRIANGLE o QUAD
            int num_points;
            if (fread(&num_points, sizeof(int), 1, f) != 1) {
                free(tw);
                fclose(f);
                return 0;
            }
            
            tw->num_points = num_points;
            tw->points = (RAY_Point*)malloc(num_points * sizeof(RAY_Point));
            if (!tw->points) {
                free(tw);
                fclose(f);
                return 0;
            }
            
            for (int p = 0; p < num_points; p++) {
                if (fread(&tw->points[p].x, sizeof(float), 1, f) != 1 ||
                    fread(&tw->points[p].y, sizeof(float), 1, f) != 1) {
                    ray_thick_wall_free(tw);
                    free(tw);
                    fclose(f);
                    return 0;
                }
            }
        }
        
        /* Leer ThinWalls del ThickWall */
        int num_thin_walls;
        if (fread(&num_thin_walls, sizeof(int), 1, f) != 1) {
            ray_thick_wall_free(tw);
            free(tw);
            fclose(f);
            return 0;
        }
        
        tw->num_thin_walls = 0;
        tw->thin_walls_capacity = num_thin_walls;
        tw->thinWalls = (RAY_ThinWall*)malloc(num_thin_walls * sizeof(RAY_ThinWall));
        
        for (int t = 0; t < num_thin_walls; t++) {
            RAY_ThinWall *thin = &tw->thinWalls[t];
            if (fread(&thin->x1, sizeof(float), 1, f) != 1 ||
                fread(&thin->y1, sizeof(float), 1, f) != 1 ||
                fread(&thin->x2, sizeof(float), 1, f) != 1 ||
                fread(&thin->y2, sizeof(float), 1, f) != 1 ||
                fread(&thin->wallType, sizeof(int), 1, f) != 1 ||
                fread(&thin->horizontal, sizeof(int), 1, f) != 1 ||
                fread(&thin->height, sizeof(float), 1, f) != 1 ||
                fread(&thin->z, sizeof(float), 1, f) != 1 ||
                fread(&thin->slope, sizeof(float), 1, f) != 1 ||
                fread(&thin->hidden, sizeof(int), 1, f) != 1) {
                fprintf(stderr, "RAY: Error leyendo thin wall %d del thick wall %d\n", t, i);
                ray_thick_wall_free(tw);
                free(tw);
                fclose(f);
                return 0;
            }
            thin->thickWall = tw;
            tw->num_thin_walls++;
        }
        
        g_engine.thickWalls[g_engine.num_thick_walls++] = tw;
    }
    
    /* Leer floor/ceiling grids si existen (versión 2 o mapas con datos extra) */
    long current_pos = ftell(f);
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, current_pos, SEEK_SET);
    
    if (current_pos < file_size) {
        printf("RAY: Leyendo floor/ceiling grids...\n");
        
        
        size_t grid_size = header.map_width * header.map_height * sizeof(int);
        
        /* Leer floor grids (3 niveles) - GUARDAR TODOS EN EL ARRAY */
        g_engine.floorGrids[0] = (int*)calloc(header.map_width * header.map_height, sizeof(int));
        g_engine.floorGrids[1] = (int*)calloc(header.map_width * header.map_height, sizeof(int));
        g_engine.floorGrids[2] = (int*)calloc(header.map_width * header.map_height, sizeof(int));
        
        if (g_engine.floorGrids[0] && g_engine.floorGrids[1] && g_engine.floorGrids[2]) {
            if (fread(g_engine.floorGrids[0], grid_size, 1, f) == 1 &&
                fread(g_engine.floorGrids[1], grid_size, 1, f) == 1 &&
                fread(g_engine.floorGrids[2], grid_size, 1, f) == 1) {
                
                /* DEBUG: Contar celdas no vacías en nivel 0 */
                int floor_count = 0;
                for (int i = 0; i < (int)(header.map_width * header.map_height); i++) {
                    if (g_engine.floorGrids[0][i] != 0) floor_count++;
                }
                printf("RAY: Floor grid nivel 0 - %d celdas con textura - Primeras 10: ", floor_count);
                for (int i = 0; i < 10; i++) {
                    printf("%d ", g_engine.floorGrids[0][i]);
                }
                printf("\n");
            } else {
                fprintf(stderr, "RAY: Error leyendo floor grids\n");
            }
        }
        
        /* Leer ceiling grids (3 niveles) - GUARDAR TODOS EN EL ARRAY */
        g_engine.ceilingGrids[0] = (int*)calloc(header.map_width * header.map_height, sizeof(int));
        g_engine.ceilingGrids[1] = (int*)calloc(header.map_width * header.map_height, sizeof(int));
        g_engine.ceilingGrids[2] = (int*)calloc(header.map_width * header.map_height, sizeof(int));
        
        if (g_engine.ceilingGrids[0] && g_engine.ceilingGrids[1] && g_engine.ceilingGrids[2]) {
            if (fread(g_engine.ceilingGrids[0], grid_size, 1, f) == 1 &&
                fread(g_engine.ceilingGrids[1], grid_size, 1, f) == 1 &&
                fread(g_engine.ceilingGrids[2], grid_size, 1, f) == 1) {
                
                /* DEBUG: Contar celdas no vacías en nivel 0 */
                int ceiling_count = 0;
                for (int i = 0; i < (int)(header.map_width * header.map_height); i++) {
                    if (g_engine.ceilingGrids[0][i] != 0) ceiling_count++;
                }
                printf("RAY: Ceiling grid nivel 0 - %d celdas con textura - Primeras 10: ", ceiling_count);
                for (int i = 0; i < 10; i++) {
                    printf("%d ", g_engine.ceilingGrids[0][i]);
                }
                printf("\n");
            } else {
                fprintf(stderr, "RAY: Error leyendo ceiling grids\n");
            }
        }
        
        /* Saltar floor height grids si existen (3 niveles de floats) */
        /* No los usamos por ahora, pero debemos saltarlos */
        current_pos = ftell(f);
        if (current_pos < file_size) {
            size_t float_grid_size = header.map_width * header.map_height * sizeof(float);
            fseek(f, float_grid_size * 3, SEEK_CUR);
        }
    } else {
        /* Si no hay datos de floor/ceiling, inicializar a ceros */
        printf("RAY: No hay floor/ceiling data, inicializando a ceros\n");
        for (int level = 0; level < 3; level++) {
            g_engine.floorGrids[level] = (int*)calloc(header.map_width * header.map_height, sizeof(int));
            g_engine.ceilingGrids[level] = (int*)calloc(header.map_width * header.map_height, sizeof(int));
        }
    }
    
    /* Inicializar array de puertas */
    g_engine.doors = (RAY_Door*)calloc(header.map_width * header.map_height, sizeof(RAY_Door));
    if (!g_engine.doors) {
        fprintf(stderr, "RAY: Error al asignar memoria para doors\n");
        fclose(f);
        return 0;
    }
    
    /* Inicializar todas las puertas */
    for (int i = 0; i < header.map_width * header.map_height; i++) {
        g_engine.doors[i].state = 0;        /* Cerrada */
        g_engine.doors[i].offset = 0.0f;    /* Sin offset */
        g_engine.doors[i].animating = 0;    /* No animándose */
        g_engine.doors[i].anim_speed = 2.0f; /* Velocidad de animación */
    }
    
    /* Leer spawn flags si es versión 3+ */
    if (header.version >= 3 && header.num_spawn_flags > 0) {
        printf("RAY: Leyendo %d spawn flags...\n", header.num_spawn_flags);
        
        for (uint32_t i = 0; i < header.num_spawn_flags; i++) {
            int flag_id;
            float x, y, z;
            int level;
            
            if (fread(&flag_id, sizeof(int), 1, f) != 1 ||
                fread(&x, sizeof(float), 1, f) != 1 ||
                fread(&y, sizeof(float), 1, f) != 1 ||
                fread(&z, sizeof(float), 1, f) != 1 ||
                fread(&level, sizeof(int), 1, f) != 1) {
                fprintf(stderr, "RAY: Error leyendo spawn flag %d\n", i);
                break;
            }
            
            /* Crear spawn flag en el motor */
            if (g_engine.num_spawn_flags < g_engine.spawn_flags_capacity) {
                RAY_SpawnFlag *flag = &g_engine.spawn_flags[g_engine.num_spawn_flags];
                flag->flag_id = flag_id;
                flag->x = x;
                flag->y = y;
                flag->z = z;
                flag->level = level;
                flag->occupied = 0;
                flag->process_ptr = NULL;
                g_engine.num_spawn_flags++;
                
                printf("RAY: Spawn flag %d cargada en (%.1f, %.1f, %.1f) nivel %d\n",
                       flag_id, x, y, z, level);
            } else {
                fprintf(stderr, "RAY: Capacidad de spawn flags excedida\n");
            }
        }
        
        printf("RAY: %d spawn flags cargadas\n", g_engine.num_spawn_flags);
    }
    
    fclose(f);
    
    printf("RAY: Mapa cargado exitosamente\n");
    printf("  - Sprites: %d\n", g_engine.num_sprites);
    printf("  - ThickWalls: %d\n", g_engine.num_thick_walls);
    printf("  - Spawn Flags: %d\n", g_engine.num_spawn_flags);
    
    return 1;
}

/* ============================================================================
   EXPORTED FUNCTIONS
   ============================================================================ */

int64_t libmod_ray_load_map(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) {
        fprintf(stderr, "RAY: Motor no inicializado\n");
        return 0;
    }
    
    const char *filename = string_get(params[0]);
    int fpg_id = (int)params[1];
    
    /* Verificar que el FPG existe */
    if (!grlib_get(fpg_id)) {
        fprintf(stderr, "RAY: El fpg_id %d no existe\n", fpg_id);
        string_discard(params[0]);
        return 0;
    }
    
    g_engine.fpg_id = fpg_id;
    
    int result = ray_load_map_from_file(filename, fpg_id);
    
    string_discard(params[0]);
    return result;
}

int64_t libmod_ray_free_map(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    /* Liberar grids */
    if (g_engine.raycaster.grids) {
        for (int i = 0; i < g_engine.raycaster.gridCount; i++) {
            if (g_engine.raycaster.grids[i]) {
                free(g_engine.raycaster.grids[i]);
                g_engine.raycaster.grids[i] = NULL;
            }
        }
        free(g_engine.raycaster.grids);
        g_engine.raycaster.grids = NULL;
    }
    
    /* Liberar thin walls */
    for (int i = 0; i < g_engine.num_thin_walls; i++) {
        if (g_engine.thinWalls[i]) {
            free(g_engine.thinWalls[i]);
            g_engine.thinWalls[i] = NULL;
        }
    }
    g_engine.num_thin_walls = 0;
    
    /* Liberar thick walls */
    for (int i = 0; i < g_engine.num_thick_walls; i++) {
        if (g_engine.thickWalls[i]) {
            ray_thick_wall_free(g_engine.thickWalls[i]);
            free(g_engine.thickWalls[i]);
            g_engine.thickWalls[i] = NULL;
        }
    }
    g_engine.num_thick_walls = 0;
    
    /* Liberar floor/ceiling grids por nivel */
    for (int level = 0; level < 3; level++) {
        if (g_engine.floorGrids[level]) {
            free(g_engine.floorGrids[level]);
            g_engine.floorGrids[level] = NULL;
        }
        if (g_engine.ceilingGrids[level]) {
            free(g_engine.ceilingGrids[level]);
            g_engine.ceilingGrids[level] = NULL;
        }
    }
    
    /* Liberar doors */
    if (g_engine.doors) {
        free(g_engine.doors);
        g_engine.doors = NULL;
    }
    
    /* Limpiar sprites */
    g_engine.num_sprites = 0;
    
    printf("RAY: Mapa liberado\n");
    return 1;
}
