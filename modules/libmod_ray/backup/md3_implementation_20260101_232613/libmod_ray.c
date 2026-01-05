/*
 * libmod_ray.c - Raycasting Module for BennuGD2
 * Geometric Sector-Based Engine (Build Engine Style)
 */

#include "libmod_ray.h"
#include <stdlib.h>
#include <float.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
   ESTADO GLOBAL DEL MOTOR
   ============================================================================ */

RAY_Engine g_engine = {0};
float *g_zbuffer = NULL;

/* Render graph global */
static GRAPH *render_graph = NULL;

/* External functions */
extern int ray_load_map_v8(const char *filename);
extern int ray_save_map_v8(const char *filename);
extern void ray_render_frame(GRAPH *dest);
extern void ray_render_frame_portal(GRAPH *dest);
extern void ray_render_frame_portal_simple(GRAPH *dest);
extern void ray_render_frame_build(GRAPH *dest);  /* Build Engine style renderer */
extern void ray_detect_portals(RAY_Engine *engine);
extern int ray_check_collision(RAY_Engine *engine, float x, float y, float z, float new_x, float new_y);

/* ============================================================================
   INICIALIZACIÓN Y FINALIZACIÓN
   ============================================================================ */

int64_t libmod_ray_init(INSTANCE *my, int64_t *params) {
    int screen_w = (int)params[0];
    int screen_h = (int)params[1];
    int fov = (int)params[2];
    int strip_width = (int)params[3];
    
    if (g_engine.initialized) {
        fprintf(stderr, "RAY: Motor ya inicializado\n");
        return 0;
    }
    
    /* Configuración básica */
    g_engine.displayWidth = screen_w;
    g_engine.displayHeight = screen_h;
    g_engine.fovDegrees = fov;
    g_engine.fovRadians = (float)fov * M_PI / 180.0f;
    g_engine.stripWidth = strip_width;
    g_engine.rayCount = screen_w / strip_width;
    g_engine.viewDist = ray_screen_distance((float)screen_w, g_engine.fovRadians);
    
    /* Precalcular ángulos de strips */
    g_engine.stripAngles = (float*)malloc(g_engine.rayCount * sizeof(float));
    if (!g_engine.stripAngles) {
        fprintf(stderr, "RAY: Error al asignar memoria para stripAngles\n");
        return 0;
    }
    
    for (int strip = 0; strip < g_engine.rayCount; strip++) {
        float screenX = (g_engine.rayCount / 2 - strip) * strip_width;
        float angle = atanf(screenX / g_engine.viewDist);
        g_engine.stripAngles[strip] = angle;
    }
    
    /* Inicializar cámara */
    memset(&g_engine.camera, 0, sizeof(RAY_Camera));
    g_engine.camera.x = 384.0f;
    g_engine.camera.y = 384.0f;
    g_engine.camera.z = 0.0f;
    g_engine.camera.rot = 0.0f;
    g_engine.camera.pitch = 0.0f;
    g_engine.camera.moveSpeed = RAY_WORLD_UNIT / 16.0f;
    g_engine.camera.rotSpeed = 1.5f * M_PI / 180.0f;
    g_engine.camera.current_sector_id = -1;
    
    /* Inicializar arrays dinámicos */
    g_engine.sprites_capacity = RAY_MAX_SPRITES;
    g_engine.sprites = (RAY_Sprite*)calloc(g_engine.sprites_capacity, sizeof(RAY_Sprite));
    g_engine.num_sprites = 0;
    
    g_engine.spawn_flags_capacity = RAY_MAX_SPAWN_FLAGS;
    g_engine.spawn_flags = (RAY_SpawnFlag*)calloc(g_engine.spawn_flags_capacity, sizeof(RAY_SpawnFlag));
    g_engine.num_spawn_flags = 0;
    
    g_engine.sectors_capacity = RAY_MAX_SECTORS;
    g_engine.sectors = (RAY_Sector*)calloc(g_engine.sectors_capacity, sizeof(RAY_Sector));
    g_engine.num_sectors = 0;
    
    g_engine.portals_capacity = RAY_MAX_PORTALS;
    g_engine.portals = (RAY_Portal*)calloc(g_engine.portals_capacity, sizeof(RAY_Portal));
    g_engine.num_portals = 0;
    
    /* Opciones de renderizado por defecto */
    g_engine.drawMiniMap = 1;
    g_engine.drawTexturedFloor = 1;
    g_engine.drawCeiling = 1;
    g_engine.drawWalls = 1;
    g_engine.drawWeapon = 1;
    g_engine.fogOn = 0;
    g_engine.skyTextureID = 0;
    
    /* Fog - Configuración por defecto */
    g_engine.fog_r = 150;
    g_engine.fog_g = 150;
    g_engine.fog_b = 180;
    g_engine.fog_start_distance = RAY_WORLD_UNIT * 8;
    g_engine.fog_end_distance = RAY_WORLD_UNIT * 20;
    
    /* Minimapa - Configuración por defecto */
    g_engine.minimap_size = 200;
    g_engine.minimap_x = 10;
    g_engine.minimap_y = 10;
    g_engine.minimap_scale = 0.5f;
    
    /* Portal Rendering Configuration */
    g_engine.max_portal_depth = 16;      /* Aumentado para mejor visibilidad */
    g_engine.portal_rendering_enabled = 1;
    
    /* Billboard */
    g_engine.billboard_enabled = 1;
    g_engine.billboard_directions = 12;
    
    g_engine.fpg_id = 0;
    g_engine.initialized = 1;
    
    printf("RAY: Motor inicializado (v8 - Geometric Sectors) - %dx%d, FOV=%d, stripWidth=%d, rayCount=%d\n",
           screen_w, screen_h, fov, strip_width, g_engine.rayCount);
    
    return 1;
}

int64_t libmod_ray_shutdown(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) {
        return 0;
    }
    
    /* Liberar stripAngles */
    if (g_engine.stripAngles) {
        free(g_engine.stripAngles);
        g_engine.stripAngles = NULL;
    }
    
    /* Liberar sprites */
    if (g_engine.sprites) {
        free(g_engine.sprites);
        g_engine.sprites = NULL;
    }
    
    /* Liberar spawn flags */
    if (g_engine.spawn_flags) {
        free(g_engine.spawn_flags);
        g_engine.spawn_flags = NULL;
    }
    
    /* Liberar sectors */
    if (g_engine.sectors) {
        for (int i = 0; i < g_engine.num_sectors; i++) {
            RAY_Sector *sector = &g_engine.sectors[i];
            if (sector->vertices) free(sector->vertices);
            if (sector->walls) free(sector->walls);
            if (sector->portal_ids) free(sector->portal_ids);
        }
        free(g_engine.sectors);
        g_engine.sectors = NULL;
    }
    
    /* Liberar portals */
    if (g_engine.portals) {
        free(g_engine.portals);
        g_engine.portals = NULL;
    }
    
    /* Liberar render graph */
    if (render_graph) {
        bitmap_destroy(render_graph);
        render_graph = NULL;
    }
    
    memset(&g_engine, 0, sizeof(RAY_Engine));
    
    printf("RAY: Motor finalizado\n");
    return 1;
}

/* ============================================================================
   CARGA DE MAPAS
   ============================================================================ */

int64_t libmod_ray_load_map(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) {
        fprintf(stderr, "RAY: Motor no inicializado\n");
        return 0;
    }
    
    const char *filename = string_get((int)params[0]);
    int fpg_id = (int)params[1];
    
    g_engine.fpg_id = fpg_id;
    
    printf("RAY: Cargando mapa: %s (FPG: %d)\n", filename, fpg_id);
    
    int result = ray_load_map_v8(filename);
    
    if (result) {
        printf("RAY: Mapa cargado exitosamente\n");
        printf("RAY: %d sectores, %d portales, %d sprites\n",
               g_engine.num_sectors, g_engine.num_portals, g_engine.num_sprites);
    } else {
        fprintf(stderr, "RAY: Error al cargar el mapa\n");
    }
    
    string_discard((int)params[0]);
    return result;
}

int64_t libmod_ray_free_map(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    /* Liberar sectores */
    if (g_engine.sectors) {
        for (int i = 0; i < g_engine.num_sectors; i++) {
            RAY_Sector *sector = &g_engine.sectors[i];
            if (sector->vertices) free(sector->vertices);
            if (sector->walls) free(sector->walls);
            if (sector->portal_ids) free(sector->portal_ids);
        }
        free(g_engine.sectors);
        g_engine.sectors = (RAY_Sector*)calloc(g_engine.sectors_capacity, sizeof(RAY_Sector));
    }
    g_engine.num_sectors = 0;
    
    /* Liberar portales */
    if (g_engine.portals) {
        free(g_engine.portals);
        g_engine.portals = (RAY_Portal*)calloc(g_engine.portals_capacity, sizeof(RAY_Portal));
    }
    g_engine.num_portals = 0;
    
    /* Liberar sprites */
    g_engine.num_sprites = 0;
    g_engine.num_spawn_flags = 0;
    
    printf("RAY: Mapa liberado\n");
    return 1;
}

/* ============================================================================
   CÁMARA - GETTERS
   ============================================================================ */

int64_t libmod_ray_get_camera_x(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    float val = g_engine.camera.x;
    return (int64_t)*(int32_t*)&val;
}

int64_t libmod_ray_get_camera_y(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    return *(int64_t*)&g_engine.camera.y;
}

int64_t libmod_ray_get_camera_z(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    return *(int64_t*)&g_engine.camera.z;
}

int64_t libmod_ray_get_camera_rot(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    return *(int64_t*)&g_engine.camera.rot;
}

int64_t libmod_ray_get_camera_pitch(INSTANCE *my, int64_t *params) {
   if (!g_engine.initialized) return 0;
    return *(int64_t*)&g_engine.camera.pitch;
}

/* ============================================================================
   CÁMARA - SETTER
   ============================================================================ */

int64_t libmod_ray_set_camera(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    float x = *(float*)&params[0];
    float y = *(float*)&params[1];
    float z = *(float*)&params[2];
    float rot = *(float*)&params[3];
    float pitch = *(float*)&params[4];
    
    g_engine.camera.x = x;
    g_engine.camera.y = y;
    g_engine.camera.z = z;
    g_engine.camera.rot = rot;
    g_engine.camera.pitch = pitch;
    
    /* Limitar pitch */
    const float max_pitch = M_PI / 2.0f * 0.99f;
    if (g_engine.camera.pitch > max_pitch) g_engine.camera.pitch = max_pitch;
    if (g_engine.camera.pitch < -max_pitch) g_engine.camera.pitch = -max_pitch;
    
    return 1;
}

/* ============================================================================
   MOVIMIENTO
   ============================================================================ */

int64_t libmod_ray_move_forward(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    float speed = *(float*)&params[0];
    float newX = g_engine.camera.x + cosf(g_engine.camera.rot) * speed;
    float newY = g_engine.camera.y + sinf(g_engine.camera.rot) * speed; // Fixed: Matches renderer (+sin)
    
    if (!ray_check_collision(&g_engine, g_engine.camera.x, g_engine.camera.y, g_engine.camera.z, newX, newY)) {
        g_engine.camera.x = newX;
        g_engine.camera.y = newY;
        
        /* Update current sector */
        RAY_Sector *sector = ray_find_sector_at_point(&g_engine, newX, newY);
        if (sector) {
            // If it's a solid sector and we are ON TOP of it, do not enter it.
            // Stay in the parent/current sector (assumed to be the space around/above).
            int enter_sector = 1;
            if (sector->is_solid) {
                 // Tolerance used in collision check
                 if (g_engine.camera.z >= sector->ceiling_z - 5.0f) {
                     enter_sector = 0;
                 }
            }
            
            if (enter_sector) {
                g_engine.camera.current_sector_id = sector->sector_id;
            }
        }
    }
    
    return 1;
}

int64_t libmod_ray_move_backward(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    float speed = *(float*)&params[0];
    float newX = g_engine.camera.x - cosf(g_engine.camera.rot) * speed;
    float newY = g_engine.camera.y - sinf(g_engine.camera.rot) * speed; // Fixed: Matches renderer (-sin)
    
    if (!ray_check_collision(&g_engine, g_engine.camera.x, g_engine.camera.y, g_engine.camera.z, newX, newY)) {
        g_engine.camera.x = newX;
        g_engine.camera.y = newY;
        
        /* Update current sector */
        RAY_Sector *sector = ray_find_sector_at_point(&g_engine, newX, newY);
        if (sector) {
            g_engine.camera.current_sector_id = sector->sector_id;
        }
    }
    
    return 1;
}

int64_t libmod_ray_strafe_left(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    float speed = *(float*)&params[0];
    // Swapped to -PI/2 for Left
    float newX = g_engine.camera.x + cosf(g_engine.camera.rot - M_PI / 2) * speed;
    float newY = g_engine.camera.y + sinf(g_engine.camera.rot - M_PI / 2) * speed; 
    
    if (!ray_check_collision(&g_engine, g_engine.camera.x, g_engine.camera.y, g_engine.camera.z, newX, newY)) {
        g_engine.camera.x = newX;
        g_engine.camera.y = newY;
        
        /* Update current sector */
        RAY_Sector *sector = ray_find_sector_at_point(&g_engine, newX, newY);
        if (sector) {
            g_engine.camera.current_sector_id = sector->sector_id;
        }
    }
    
    return 1;
}

int64_t libmod_ray_strafe_right(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    float speed = *(float*)&params[0];
    // Swapped to +PI/2 for Right
    float newX = g_engine.camera.x + cosf(g_engine.camera.rot + M_PI / 2) * speed;
    float newY = g_engine.camera.y + sinf(g_engine.camera.rot + M_PI / 2) * speed; 
    
    if (!ray_check_collision(&g_engine, g_engine.camera.x, g_engine.camera.y, g_engine.camera.z, newX, newY)) {
        g_engine.camera.x = newX;
        g_engine.camera.y = newY;
        
        /* Update current sector */
        RAY_Sector *sector = ray_find_sector_at_point(&g_engine, newX, newY);
        if (sector) {
            g_engine.camera.current_sector_id = sector->sector_id;
        }
    }
    
    return 1;
}

int64_t libmod_ray_rotate(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    float delta = *(float*)&params[0];
    g_engine.camera.rot += delta; // Inverted from -= to +=
    
    /* Normalizar ángulo */
    while (g_engine.camera.rot < 0) g_engine.camera.rot += RAY_TWO_PI;
    while (g_engine.camera.rot >= RAY_TWO_PI) g_engine.camera.rot -= RAY_TWO_PI;
    
    return 1;
}

int64_t libmod_ray_look_up_down(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    float delta = *(float*)&params[0];
    g_engine.camera.pitch += delta;
    
    /* Limitar pitch */
    const float max_pitch = M_PI / 2.0f * 0.99f;
    if (g_engine.camera.pitch > max_pitch) g_engine.camera.pitch = max_pitch;
    if (g_engine.camera.pitch < -max_pitch) g_engine.camera.pitch = -max_pitch;
    
    return 1;
}

int64_t libmod_ray_jump(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    if (!g_engine.camera.jumping) {
        g_engine.camera.jumping = 1;
        g_engine.camera.heightJumped = 0;
    }
    
    return 1;
}

/* ============================================================================
   RENDERIZADO
   ============================================================================ */

int64_t libmod_ray_render(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) {
        fprintf(stderr, "RAY: Motor no inicializado\n");
        return 0;
    }
    
    int graph_id = (int)params[0];
    GRAPH *dest = NULL;
    
    // Si graph_id es 0, crear un nuevo graph automáticamente
    if (graph_id == 0) {
        dest = bitmap_new_syslib(g_engine.displayWidth, g_engine.displayHeight);
        if (!dest) {
            fprintf(stderr, "RAY: No se pudo crear graph\n");
            return 0;
        }
        graph_id = dest->code;
    } else {
        dest = bitmap_get(0, graph_id);
        if (!dest) {
            fprintf(stderr, "RAY: Graph no válido: %d\n", graph_id);
            return 0;
        }
    }
    
    /* RENDER FRAME */
    
    /* BUILD ENGINE RENDERER (Active) */
    /* BUILD ENGINE RENDERER (Active) */
    ray_render_frame_build(dest);
    
    /* CLASSIC RAYCASTER (Deprecated) */
    // ray_render_frame(dest);
    
    return graph_id;
}

/* ============================================================================
   CONFIGURACIÓN
   ============================================================================ */

int64_t libmod_ray_set_fog(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    int enabled = (int)params[0];
    int r = (int)params[1];
    int g = (int)params[2];
    int b = (int)params[3];
    float start_dist = *(float*)&params[4];
    float end_dist = *(float*)&params[5];
    
    g_engine.fogOn = enabled;
    g_engine.fog_r = r;
    g_engine.fog_g = g;
    g_engine.fog_b = b;
    g_engine.fog_start_distance = start_dist;
    g_engine.fog_end_distance = end_dist;
    
    return 1;
}

int64_t libmod_ray_set_draw_minimap(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    g_engine.drawMiniMap = (int)params[0];
    return 1;
}

int64_t libmod_ray_set_draw_weapon(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    g_engine.drawWeapon = (int)params[0];
    return 1;
}

int64_t libmod_ray_set_billboard(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    g_engine.billboard_enabled = (int)params[0];
    g_engine.billboard_directions = (int)params[1];
    return 1;
}

int64_t libmod_ray_check_collision(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    float x = *(float*)&params[0];
    float y = *(float*)&params[1];
    float new_x = *(float*)&params[2];
    float new_y = *(float*)&params[3];
    
    return ray_check_collision(&g_engine, x, y, g_engine.camera.z, new_x, new_y);
}

int64_t libmod_ray_set_minimap(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    g_engine.minimap_size = (int)params[0];
    g_engine.minimap_x = (int)params[1];
    g_engine.minimap_y = (int)params[2];
    g_engine.minimap_scale = *(float*)&params[3];
    
    return 1;
}

/* ============================================================================
   SPRITES DINÁMICOS
   ============================================================================ */

int64_t libmod_ray_add_sprite(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    float x = *(float*)&params[0];
    float y = *(float*)&params[1];
    float z = *(float*)&params[2];
    int textureID = (int)params[3];
    int w = (int)params[4];
    int h = (int)params[5];
    
    if (g_engine.num_sprites >= g_engine.sprites_capacity) {
        fprintf(stderr, "RAY: Máximo de sprites alcanzado\n");
        return -1;
    }
    
    RAY_Sprite *sprite = &g_engine.sprites[g_engine.num_sprites];
    memset(sprite, 0, sizeof(RAY_Sprite));
    
    sprite->x = x;
    sprite->y = y;
    sprite->z = z;
    sprite->textureID = textureID;
    sprite->w = w;
    sprite->h = h;
    sprite->dir = 1;
    sprite->rot = 0.0f;
    sprite->process_ptr = my;
    sprite->flag_id = -1;
    
    g_engine.num_sprites++;
    
    return g_engine.num_sprites - 1;
}

int64_t libmod_ray_remove_sprite(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    int sprite_id = (int)params[0];
    
    if (sprite_id < 0 || sprite_id >= g_engine.num_sprites) {
        return 0;
    }
    
    g_engine.sprites[sprite_id].cleanup = 1;
    
    return 1;
}

int64_t libmod_ray_update_sprite_position(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    int sprite_id = (int)params[0];
    float x = *(float*)&params[1];
    float y = *(float*)&params[2];
    float z = *(float*)&params[3];
    
    if (sprite_id < 0 || sprite_id >= g_engine.num_sprites) {
        return 0;
    }
    
    g_engine.sprites[sprite_id].x = x;
    g_engine.sprites[sprite_id].y = y;
    g_engine.sprites[sprite_id].z = z;
    
    return 1;
}

/* ============================================================================
   SPAWN FLAGS
   ============================================================================ */

int64_t libmod_ray_set_flag(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    int flag_id = (int)params[0];
    
    for (int i = 0; i < g_engine.num_spawn_flags; i++) {
        if (g_engine.spawn_flags[i].flag_id == flag_id) {
            g_engine.spawn_flags[i].occupied = 1;
            g_engine.spawn_flags[i].process_ptr = my;
            return 1;
        }
    }
    
    return 0;
}

int64_t libmod_ray_clear_flag(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    int flag_id = (int)params[0];
    
    for (int i = 0; i < g_engine.num_spawn_flags; i++) {
        if (g_engine.spawn_flags[i].flag_id == flag_id) {
            g_engine.spawn_flags[i].occupied = 0;
            g_engine.spawn_flags[i].process_ptr = NULL;
            return 1;
        }
    }
    
    return 0;
}

int64_t libmod_ray_get_camera_sector(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return -1;
    return g_engine.camera.current_sector_id;
}

int64_t libmod_ray_get_flag_x(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    int flag_id = (int)params[0];
    
    for (int i = 0; i < g_engine.num_spawn_flags; i++) {
        if (g_engine.spawn_flags[i].flag_id == flag_id) {
            float val = g_engine.spawn_flags[i].x;
            printf("DEBUG: RAY_GET_FLAG_X(%d) -> Found X=%.2f\n", flag_id, val);
            return (int64_t)*(int32_t*)&val;
        }
    }
    
    return 0;
}

int64_t libmod_ray_get_flag_y(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    int flag_id = (int)params[0];
    
    for (int i = 0; i < g_engine.num_spawn_flags; i++) {
        if (g_engine.spawn_flags[i].flag_id == flag_id) {
            float val = g_engine.spawn_flags[i].y;
            printf("DEBUG: RAY_GET_FLAG_Y(%d) -> Found Y=%.2f\n", flag_id, val);
            return (int64_t)*(int32_t*)&val;
        }
    }
    
    return 0;
}

int64_t libmod_ray_get_flag_z(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    int flag_id = (int)params[0];
    
    for (int i = 0; i < g_engine.num_spawn_flags; i++) {
        if (g_engine.spawn_flags[i].flag_id == flag_id) {
            float val = g_engine.spawn_flags[i].z;
            return (int64_t)*(int32_t*)&val;
        }
    }
    
    return 0;
}

/* ============================================================================
   SKYBOX
   ============================================================================ */

int64_t libmod_ray_set_sky_texture(INSTANCE *my, int64_t *params)
{
    if (!g_engine.initialized) return 0;
    g_engine.skyTextureID = (int)params[0];
    return 1;
}

/* ============================================================================
   DOORS (Legacy - not used in geometric system)
   ============================================================================ */

int64_t libmod_ray_toggle_door(INSTANCE *my, int64_t *params)
{
    // Doors are not implemented in the geometric sector system
    // This is a legacy function kept for API compatibility
    return 0;
}

/* ============================================================================
   MD2 MODELS
   ============================================================================ */

#include "libmod_ray_md2.h"
#include "libmod_ray_md3.h"

int64_t libmod_ray_load_md2(INSTANCE *my, int64_t *params) {
    const char *filename = string_get((int)params[0]);
    RAY_MD2_Model *model = ray_md2_load(filename);
    string_discard((int)params[0]);
    if (!model) return 0;
    return (int64_t)(intptr_t)model;
}

int64_t libmod_ray_load_md3(INSTANCE *my, int64_t *params) {
    const char *filename = string_get((int)params[0]);
    RAY_MD3_Model *model = ray_md3_load(filename);
    string_discard((int)params[0]);
    if (!model) return 0;
    return (int64_t)(intptr_t)model;
}

int64_t libmod_ray_set_sprite_md2(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    int sprite_id = (int)params[0];
    int64_t model_ptr = params[1];
    int skin_id = (int)params[2]; // Texture ID for skin
    
    if (sprite_id < 0 || sprite_id >= g_engine.num_sprites) return 0;
    
    RAY_Sprite *s = &g_engine.sprites[sprite_id];
    s->model = (struct RAY_Model*)(intptr_t)model_ptr;
    
    if (s->model) {
        int magic = *(int*)s->model;
        if (magic == MD2_MAGIC) {
             ((RAY_MD2_Model*)s->model)->textureID = skin_id;
        } else if (magic == MD3_MAGIC) {
             ((RAY_MD3_Model*)s->model)->textureID = skin_id;
        }
    }
    return 1;
}

int64_t libmod_ray_set_sprite_anim(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    int sprite_id = (int)params[0];
    int frame = (int)params[1];
    int next_frame = (int)params[2];
    float interp = *(float*)&params[3];
    
    if (sprite_id < 0 || sprite_id >= g_engine.num_sprites) return 0;
    
    RAY_Sprite *s = &g_engine.sprites[sprite_id];
    s->currentFrame = frame;
    s->nextFrame = next_frame;
    s->interpolation = interp;
    return 1;
}

int64_t libmod_ray_set_sprite_angle(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    int sprite_id = (int)params[0];
    float angle = *(float*)&params[1];
    
    if (sprite_id < 0 || sprite_id >= g_engine.num_sprites) return 0;
    
    // Angle in degrees to radians
    g_engine.sprites[sprite_id].rot = angle * M_PI / 180.0f;
    
    return 1;
}

// Get floor height at x,y
int64_t libmod_ray_get_floor_height(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    float x = *(float*)&params[0];
    float y = *(float*)&params[1];
    
    RAY_Sector *sector = ray_find_sector_at_point(&g_engine, x, y);
    if (sector) {
        float val = sector->floor_z;
        return (int64_t)*(int32_t*)&val;
    }
    
    return 0;
}

/* ============================================================================
   MD3 TAG SYSTEM
   ============================================================================ */

int64_t libmod_ray_get_tag_point(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    int sprite_id = (int)params[0];
    const char *tag_name = string_get((int)params[1]);
    int *ptr_x = (int*)params[2]; // Pointers to FLOAT variables
    int *ptr_y = (int*)params[3];
    int *ptr_z = (int*)params[4];
    
    // Default return
    int result = 0;
    
    if (sprite_id >= 0 && sprite_id < g_engine.num_sprites) {
        RAY_Sprite *s = &g_engine.sprites[sprite_id];
        
        if (s->model && (*(int*)s->model) == MD3_MAGIC) {
            RAY_MD3_Model *model = (RAY_MD3_Model*)s->model;
            
            // Safe Frame Clamping
            int frame1 = s->currentFrame;
            int frame2 = s->nextFrame;
            if (frame1 >= model->header.numFrames) frame1 = model->header.numFrames - 1;
            if (frame2 >= model->header.numFrames) frame2 = model->header.numFrames - 1;
            if (frame1 < 0) frame1 = 0;
            if (frame2 < 0) frame2 = 0;
            
            // Search Tag
            md3_tag_t *tag1 = NULL;
            md3_tag_t *tag2 = NULL;
            
            int offset1 = frame1 * model->header.numTags;
            int offset2 = frame2 * model->header.numTags;
            
            for (int i = 0; i < model->header.numTags; i++) {
                if (strcmp(model->tags[offset1 + i].name, tag_name) == 0) {
                    tag1 = &model->tags[offset1 + i];
                    tag2 = &model->tags[offset2 + i];
                    break;
                }
            }
            
            if (tag1 && tag2) {
                // Found! Interpolate
                // NOTE: MD3 tags are ALREADY in world units (floats), unlike vertices which are compressed int16
                // So we do NOT apply the 1/64 scale here
                float interp = s->interpolation;
                
                float lx = tag1->origin.x + interp * (tag2->origin.x - tag1->origin.x);
                float ly = tag1->origin.y + interp * (tag2->origin.y - tag1->origin.y);
                float lz = tag1->origin.z + interp * (tag2->origin.z - tag1->origin.z);
                
                // Transform to World Space
                float cos_rot = cos(s->rot);
                float sin_rot = sin(s->rot);
                
                // MD3 standard: X=Forward, Y=Left, Z=Up
                // Bennu World: Raycasting Logic uses s->rot (radians)
                // Rotated X = x*cos - y*sin
                // Rotated Y = x*sin + y*cos
                
                float wx = lx * cos_rot - ly * sin_rot + s->x;
                float wy = lx * sin_rot + ly * cos_rot + s->y;
                float wz = lz + s->z;
                
                if (ptr_x) *(float*)ptr_x = wx;
                if (ptr_y) *(float*)ptr_y = wy;
                if (ptr_z) *(float*)ptr_z = wz;
                
                result = 1;
            }
        }
    }
    
    string_discard((int)params[1]);
    return result;
}

#include "libmod_ray_exports.h"