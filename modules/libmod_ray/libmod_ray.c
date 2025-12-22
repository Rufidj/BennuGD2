/*
 * libmod_ray.c - Raycasting Module for BennuGD2
 * Port of Andrew Lim's SDL2 Raycasting Engine
 * https://github.com/andrew-lim/sdl2-raycast
 */

#include "libmod_ray.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
   ESTADO GLOBAL DEL MOTOR
   ============================================================================ */

RAY_Engine g_engine = {0};

/* Render graph global (como en OLD) */
static GRAPH *render_graph = NULL;

/* ============================================================================
   UTILIDADES
   ============================================================================ */

int ray_is_door(int wallType) {
    return ray_is_vertical_door(wallType) || ray_is_horizontal_door(wallType);
}

int ray_is_vertical_door(int wallType) {
    return wallType > RAY_DOOR_VERTICAL_MIN && wallType <= RAY_DOOR_VERTICAL_MAX;
}

int ray_is_horizontal_door(int wallType) {
    return wallType > RAY_DOOR_HORIZONTAL_MIN;
}

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
    
    /* Configuración básica - NO inicializar ventana, solo configurar el motor */
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
        g_engine.stripAngles[strip] = ray_strip_angle(screenX, g_engine.viewDist);
    }
    
    /* Inicializar cámara */
    memset(&g_engine.camera, 0, sizeof(RAY_Camera));
    g_engine.camera.moveSpeed = RAY_TILE_SIZE / 16.0f;
    g_engine.camera.rotSpeed = 1.5f * M_PI / 180.0f;
    
    /* Inicializar arrays dinámicos */
    g_engine.sprites_capacity = RAY_MAX_SPRITES;
    g_engine.sprites = (RAY_Sprite*)calloc(g_engine.sprites_capacity, sizeof(RAY_Sprite));
    
    g_engine.thin_walls_capacity = RAY_MAX_THIN_WALLS;
    g_engine.thinWalls = (RAY_ThinWall**)calloc(g_engine.thin_walls_capacity, sizeof(RAY_ThinWall*));
    
    g_engine.thick_walls_capacity = RAY_MAX_THICK_WALLS;
    g_engine.thickWalls = (RAY_ThickWall**)calloc(g_engine.thick_walls_capacity, sizeof(RAY_ThickWall*));
    
    /* Inicializar spawn flags */
    g_engine.spawn_flags_capacity = 100; /* Máximo 100 flags por mapa */
    g_engine.spawn_flags = (RAY_SpawnFlag*)calloc(g_engine.spawn_flags_capacity, sizeof(RAY_SpawnFlag));
    g_engine.num_spawn_flags = 0;
    
    /* Opciones de renderizado por defecto */
    g_engine.drawMiniMap = 1;
    g_engine.drawTexturedFloor = 1;
    g_engine.drawCeiling = 1;
    g_engine.drawWalls = 1;
    g_engine.drawWeapon = 1;
    g_engine.fogOn = 0;
    g_engine.skyTextureID = 0;  /* 0 = color sólido azul */
    g_engine.skipDrawnFloorStrips = 1;
    g_engine.skipDrawnSkyboxStrips = 1;
    g_engine.skipDrawnHighestCeilingStrips = 1;
    g_engine.highestCeilingLevel = 3;
    
    /* Fog - Configuración por defecto */
    g_engine.fog_r = 150;
    g_engine.fog_g = 150;
    g_engine.fog_b = 180;
    g_engine.fog_start_distance = RAY_TILE_SIZE * 8;  /* 8 baldosas */
    g_engine.fog_end_distance = RAY_TILE_SIZE * 20;   /* 20 baldosas */
    
    /* Minimapa - Configuración por defecto */
    g_engine.minimap_size = 200;
    g_engine.minimap_x = 10;
    g_engine.minimap_y = 10;
    g_engine.minimap_scale = 0.5f;
    
    /* Billboard - Activado por defecto con 12 direcciones */
    g_engine.billboard_enabled = 1;
    g_engine.billboard_directions = 12;
    
    g_engine.initialized = 1;
    
    printf("RAY: Motor inicializado - %dx%d, FOV=%d, stripWidth=%d, rayCount=%d\n",
           screen_w, screen_h, fov, strip_width, g_engine.rayCount);
    printf("RAY: NOTA - La ventana debe ser inicializada con set_mode() antes de RAY_INIT\n");
    
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
    
    /* Liberar thin walls */
    if (g_engine.thinWalls) {
        free(g_engine.thinWalls);
        g_engine.thinWalls = NULL;
    }
    
    /* Liberar thick walls */
    if (g_engine.thickWalls) {
        for (int i = 0; i < g_engine.num_thick_walls; i++) {
            if (g_engine.thickWalls[i]) {
                ray_thick_wall_free(g_engine.thickWalls[i]);
                free(g_engine.thickWalls[i]);
            }
        }
        free(g_engine.thickWalls);
        g_engine.thickWalls = NULL;
    }
    
    /* Liberar grids */
    if (g_engine.raycaster.grids) {
        for (int i = 0; i < g_engine.raycaster.gridCount; i++) {
            if (g_engine.raycaster.grids[i]) {
                free(g_engine.raycaster.grids[i]);
            }
        }
        free(g_engine.raycaster.grids);
        g_engine.raycaster.grids = NULL;
    }
    
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
    
    /* Liberar spawn flags */
    if (g_engine.spawn_flags) {
        free(g_engine.spawn_flags);
        g_engine.spawn_flags = NULL;
    }
    g_engine.num_spawn_flags = 0;
    
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
   CÁMARA - GETTERS
   ============================================================================ */

int64_t libmod_ray_get_camera_x(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    return *(int64_t*)&g_engine.camera.x;
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
   COLISIONES
   ============================================================================ */

/* Verificar si una posición colisiona con una pared */
static int ray_check_collision(float x, float y, float radius) {
    if (!g_engine.raycaster.grids || !g_engine.raycaster.grids[0]) {
        return 0;
    }
    
    int *grid = g_engine.raycaster.grids[0];
    int gridWidth = g_engine.raycaster.gridWidth;
    int gridHeight = g_engine.raycaster.gridHeight;
    
    /* Verificar los 4 puntos cardinales alrededor del jugador */
    float checkPoints[4][2] = {
        {x + radius, y}, {x - radius, y}, {x, y + radius}, {x, y - radius}
    };
    
    for (int i = 0; i < 4; i++) {
        int gridX = (int)(checkPoints[i][0] / RAY_TILE_SIZE);
        int gridY = (int)(checkPoints[i][1] / RAY_TILE_SIZE);
        
        if (gridX < 0 || gridX >= gridWidth || gridY < 0 || gridY >= gridHeight) {
            return 1; /* Fuera del mapa */
        }
        
        int cellValue = grid[gridX + gridY * gridWidth];
        if (cellValue > 0) {
            /* Si es puerta, verificar si está abierta */
            if (ray_is_door(cellValue)) {
                int doorOffset = gridX + gridY * gridWidth;
                if (g_engine.doors && doorOffset >= 0 && doorOffset < gridWidth * gridHeight) {
                    RAY_Door *door = &g_engine.doors[doorOffset];
                    if (door->offset < 0.9f) return 1; /* Puerta cerrada */
                }
            } else {
                return 1; /* Pared normal */
            }
        }
    }
    return 0;
}

/* ============================================================================
   MOVIMIENTO
   ============================================================================*/
/* Movement functions */
int64_t libmod_ray_move_forward(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    float speed = *(float*)&params[0];
    float newX = g_engine.camera.x + cosf(g_engine.camera.rot) * speed;
    float newY = g_engine.camera.y - sinf(g_engine.camera.rot) * speed;
    
    /* Clamp to map bounds */
    float mapWidth = g_engine.raycaster.gridWidth * RAY_TILE_SIZE;
    float mapHeight = g_engine.raycaster.gridHeight * RAY_TILE_SIZE;
    if (newX < 0) newX = 0;
    if (newX >= mapWidth) newX = mapWidth - 1;
    if (newY < 0) newY = 0;
    if (newY >= mapHeight) newY = mapHeight - 1;
    
    /* TODO: Implementar detección de colisiones */
    /* Verificar colisión antes de mover */
    if (!ray_check_collision(newX, newY, 20.0f)) {
        g_engine.camera.x = newX;
        g_engine.camera.y = newY;
    }
    
    return 1;
}

int64_t libmod_ray_move_backward(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    float speed = *(float*)&params[0];
    float newX = g_engine.camera.x - cosf(g_engine.camera.rot) * speed;
    float newY = g_engine.camera.y + sinf(g_engine.camera.rot) * speed;
    
    /* Clamp to map bounds */
    float mapWidth = g_engine.raycaster.gridWidth * RAY_TILE_SIZE;
    float mapHeight = g_engine.raycaster.gridHeight * RAY_TILE_SIZE;
    if (newX < 0) newX = 0;
    if (newX >= mapWidth) newX = mapWidth - 1;
    if (newY < 0) newY = 0;
    if (newY >= mapHeight) newY = mapHeight - 1;
    
    /* TODO: Implementar detección de colisiones */
    /* Verificar colisión antes de mover */
    if (!ray_check_collision(newX, newY, 20.0f)) {
        g_engine.camera.x = newX;
        g_engine.camera.y = newY;
    }
    
    return 1;
}

int64_t libmod_ray_strafe_left(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    float speed = *(float*)&params[0];
    float newX = g_engine.camera.x + cosf(g_engine.camera.rot + M_PI / 2) * speed;
    float newY = g_engine.camera.y - sinf(g_engine.camera.rot + M_PI / 2) * speed;
    
    /* Clamp to map bounds */
    float mapWidth = g_engine.raycaster.gridWidth * RAY_TILE_SIZE;
    float mapHeight = g_engine.raycaster.gridHeight * RAY_TILE_SIZE;
    if (newX < 0) newX = 0;
    if (newX >= mapWidth) newX = mapWidth - 1;
    if (newY < 0) newY = 0;
    if (newY >= mapHeight) newY = mapHeight - 1;
    
    /* TODO: Implementar detección de colisiones */
    /* Verificar colisión antes de mover */
    if (!ray_check_collision(newX, newY, 20.0f)) {
        g_engine.camera.x = newX;
        g_engine.camera.y = newY;
    }
    
    return 1;
}

int64_t libmod_ray_strafe_right(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    float speed = *(float*)&params[0];
    float newX = g_engine.camera.x + cosf(g_engine.camera.rot - M_PI / 2) * speed;
    float newY = g_engine.camera.y - sinf(g_engine.camera.rot - M_PI / 2) * speed;
    
    /* Clamp to map bounds */
    float mapWidth = g_engine.raycaster.gridWidth * RAY_TILE_SIZE;
    float mapHeight = g_engine.raycaster.gridHeight * RAY_TILE_SIZE;
    if (newX < 0) newX = 0;
    if (newX >= mapWidth) newX = mapWidth - 1;
    if (newY < 0) newY = 0;
    if (newY >= mapHeight) newY = mapHeight - 1;
    
    /* TODO: Implementar detección de colisiones */
    /* Verificar colisión antes de mover */
    if (!ray_check_collision(newX, newY, 20.0f)) {
        g_engine.camera.x = newX;
        g_engine.camera.y = newY;
    }
    
    return 1;
}

int64_t libmod_ray_rotate(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    float delta = *(float*)&params[0];
    g_engine.camera.rot -= delta;  /* Invertido para rotar en la dirección correcta */
    
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
   DOOR ANIMATION UPDATE
   ============================================================================ */

void ray_update_doors(float delta_time) {
    if (!g_engine.doors || !g_engine.initialized) return;
    
    int total_doors = g_engine.raycaster.gridWidth * g_engine.raycaster.gridHeight;
    static int debug_count = 0;
    
    for (int i = 0; i < total_doors; i++) {
        RAY_Door *door = &g_engine.doors[i];
        
        if (!door->animating) continue;
        
        /* Calcular incremento de offset basado en velocidad y delta time */
        float increment = door->anim_speed * delta_time;
        
        if (door->state == 1) {
            /* Abriendo - incrementar offset hacia 1.0 */
            door->offset += increment;
            if (door->offset >= 1.0f) {
                door->offset = 1.0f;
                door->animating = 0; /* Animación completa */
                if (debug_count < 5) {
                    printf("RAY: Puerta %d completó animación de APERTURA, offset=%.2f\n", i, door->offset);
                    debug_count++;
                }
            } else if (debug_count < 20) {
                printf("RAY: Puerta %d animando APERTURA, offset=%.2f\n", i, door->offset);
                debug_count++;
            }
        } else {
            /* Cerrando - decrementar offset hacia 0.0 */
            door->offset -= increment;
            if (door->offset <= 0.0f) {
                door->offset = 0.0f;
                door->animating = 0; /* Animación completa */
                if (debug_count < 5) {
                    printf("RAY: Puerta %d completó animación de CIERRE, offset=%.2f\n", i, door->offset);
                    debug_count++;
                }
            } else if (debug_count < 20) {
                printf("RAY: Puerta %d animando CIERRE, offset=%.2f\n", i, door->offset);
                debug_count++;
            }
        }
    }
}

/* ============================================================================
   UPDATE - Actualización de física (llamar cada frame)
   ============================================================================ */

void ray_update_physics(float delta_time) {
    /* Actualizar animaciones de puertas */
    ray_update_doors(delta_time);
    
    /* Constantes de salto (del motor original) */
    const float MAX_JUMP_DISTANCE = 3.0f * RAY_TILE_SIZE;
    const float HALF_JUMP_DISTANCE = MAX_JUMP_DISTANCE / 2.0f;
    const float JUMP_SPEED = 8.0f; /* Velocidad de salto ajustable */
    
    if (g_engine.camera.jumping) {
        /* Fase ascendente del salto */
        if (g_engine.camera.heightJumped < HALF_JUMP_DISTANCE) {
            float jump_increment = JUMP_SPEED * delta_time;
            g_engine.camera.z += jump_increment;
            g_engine.camera.heightJumped += jump_increment;
            
            /* Alcanzó el punto máximo */
            if (g_engine.camera.heightJumped >= HALF_JUMP_DISTANCE) {
                g_engine.camera.heightJumped = HALF_JUMP_DISTANCE;
            }
        }
        /* Fase descendente del salto */
        else if (g_engine.camera.heightJumped < MAX_JUMP_DISTANCE) {
            float fall_increment = JUMP_SPEED * delta_time;
            g_engine.camera.z -= fall_increment;
            g_engine.camera.heightJumped += fall_increment;
            
            /* Terminó el salto */
            if (g_engine.camera.heightJumped >= MAX_JUMP_DISTANCE) {
                g_engine.camera.jumping = 0;
                g_engine.camera.heightJumped = 0;
            }
        }
    }
    
    /* Limpiar sprites marcados para eliminación */
    int write_idx = 0;
    for (int read_idx = 0; read_idx < g_engine.num_sprites; read_idx++) {
        if (!g_engine.sprites[read_idx].cleanup) {
            if (write_idx != read_idx) {
                g_engine.sprites[write_idx] = g_engine.sprites[read_idx];
            }
            write_idx++;
        }
    }
    g_engine.num_sprites = write_idx;
}

/* ============================================================================
   CONFIGURACIÓN
   ============================================================================ */

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

int64_t libmod_ray_set_sky_texture(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    g_engine.skyTextureID = (int)params[0];
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
    float radius = *(float*)&params[2];
    
    return ray_check_collision(x, y, radius);
}

/* ============================================================================
   PUERTAS
   ============================================================================ */

int64_t libmod_ray_toggle_door(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    if (!g_engine.doors) return 0;
    
    /* Hacer un raycast simple para encontrar la puerta más cercana frente al jugador */
    float rayAngle = g_engine.camera.rot;
    int playerX = (int)g_engine.camera.x;
    int playerY = (int)g_engine.camera.y;
    
    /* Normalizar ángulo */
    while (rayAngle < 0) rayAngle += RAY_TWO_PI;
    while (rayAngle >= RAY_TWO_PI) rayAngle -= RAY_TWO_PI;
    
    /* Determinar cuadrante */
    int right = (rayAngle < RAY_TWO_PI * 0.25f && rayAngle >= 0) ||
                (rayAngle > RAY_TWO_PI * 0.75f);
    int up = rayAngle < RAY_TWO_PI * 0.5f && rayAngle >= 0;
    
    int *grid = g_engine.raycaster.grids[0]; /* Solo nivel 0 por ahora */
    int tileSize = RAY_TILE_SIZE;
    
    /* Buscar intersección vertical */
    float vx = right ? (floorf(playerX / tileSize) * tileSize + tileSize)
                     : (floorf(playerX / tileSize) * tileSize - 1);
    float vy = playerY + (playerX - vx) * tanf(rayAngle);
    float stepx = right ? tileSize : -tileSize;
    float stepy = tileSize * tanf(rayAngle);
    if (right) stepy = -stepy;
    
    float min_door_dist = FLT_MAX;
    int door_x = -1, door_y = -1;
    
    /* Recorrer líneas verticales */
    for (int i = 0; i < 20; i++) { /* Máximo 20 pasos */
        if (vx < 0 || vx >= g_engine.raycaster.gridWidth * tileSize ||
            vy < 0 || vy >= g_engine.raycaster.gridHeight * tileSize) break;
        
        int wallY = (int)floorf(vy / tileSize);
        int wallX = (int)floorf(vx / tileSize);
        
        if (wallX >= 0 && wallX < g_engine.raycaster.gridWidth &&
            wallY >= 0 && wallY < g_engine.raycaster.gridHeight) {
            
            int wallType = grid[wallX + wallY * g_engine.raycaster.gridWidth];
            
            if (ray_is_door(wallType)) {
                float dx = playerX - vx;
                float dy = playerY - vy;
                float dist = sqrtf(dx * dx + dy * dy);
                
                if (dist < min_door_dist) {
                    min_door_dist = dist;
                    door_x = wallX;
                    door_y = wallY;
                }
                break;
            }
        }
        
        vx += stepx;
        vy += stepy;
    }
    
    /* Buscar intersección horizontal */
    float hy = up ? (floorf(playerY / tileSize) * tileSize - 1)
                  : (floorf(playerY / tileSize) * tileSize + tileSize);
    float hx = playerX + (playerY - hy) / tanf(rayAngle);
    stepy = up ? -tileSize : tileSize;
    stepx = tileSize / tanf(rayAngle);
    if (!up) stepx = -stepx;
    
    /* Recorrer líneas horizontales */
    for (int i = 0; i < 20; i++) {
        if (hx < 0 || hx >= g_engine.raycaster.gridWidth * tileSize ||
            hy < 0 || hy >= g_engine.raycaster.gridHeight * tileSize) break;
        
        int wallY = (int)floorf(hy / tileSize);
        int wallX = (int)floorf(hx / tileSize);
        
        if (wallX >= 0 && wallX < g_engine.raycaster.gridWidth &&
            wallY >= 0 && wallY < g_engine.raycaster.gridHeight) {
            
            int wallType = grid[wallX + wallY * g_engine.raycaster.gridWidth];
            
            if (ray_is_door(wallType)) {
                float dx = playerX - hx;
                float dy = playerY - hy;
                float dist = sqrtf(dx * dx + dy * dy);
                
                if (dist < min_door_dist) {
                    min_door_dist = dist;
                    door_x = wallX;
                    door_y = wallY;
                }
                break;
            }
        }
        
        hx += stepx;
        hy += stepy;
    }
    
    /* Si encontramos una puerta, togglearla */
    if (door_x >= 0 && door_y >= 0 && min_door_dist < RAY_TILE_SIZE * 2.0f) {
        int offset = door_x + door_y * g_engine.raycaster.gridWidth;
        RAY_Door *door = &g_engine.doors[offset];
        
        /* Toggle state */
        door->state = !door->state;
        
        /* Iniciar animación */
        door->animating = 1;
        
        printf("RAY: Puerta detectada automáticamente en (%d, %d) cambiada a estado %d, iniciando animación\n", 
               door_x, door_y, door->state);
        
        return 1;
    }
    
    printf("RAY: No se encontró puerta frente al jugador\n");
    return 0;
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
    sprite->w = w;
    sprite->h = h;
    sprite->textureID = textureID;
    sprite->level = 0; /* TODO: Calcular nivel correcto */
    
    g_engine.num_sprites++;
    return g_engine.num_sprites - 1;
}

int64_t libmod_ray_remove_sprite(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    int index = (int)params[0];
    
    if (index < 0 || index >= g_engine.num_sprites) {
        return 0;
    }
    
    /* Marcar para eliminación */
    g_engine.sprites[index].cleanup = 1;
    
    return 1;
}

/* ============================================================================
   SPAWN FLAGS - Sistema de banderas de spawn
   ============================================================================ */

int64_t libmod_ray_set_flag(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    if (!my) return 0;
    
    int flag_id = (int)params[0];
    
    /* Buscar la flag por ID */
    RAY_SpawnFlag *flag = NULL;
    for (int i = 0; i < g_engine.num_spawn_flags; i++) {
        if (g_engine.spawn_flags[i].flag_id == flag_id) {
            flag = &g_engine.spawn_flags[i];
            break;
        }
    }
    
    if (!flag) {
        fprintf(stderr, "RAY: Flag %d no encontrada en el mapa\n", flag_id);
        return 0;
    }
    
    /* Verificar si la flag ya está ocupada */
    if (flag->occupied) {
        fprintf(stderr, "RAY: Flag %d ya está ocupada por otro proceso\n", flag_id);
        return 0;
    }
    
    /* Buscar o crear sprite para este proceso */
    RAY_Sprite *sprite = NULL;
    
    /* Primero buscar si ya existe un sprite para este proceso */
    for (int i = 0; i < g_engine.num_sprites; i++) {
        if (g_engine.sprites[i].process_ptr == my) {
            sprite = &g_engine.sprites[i];
            break;
        }
    }
    
    /* Si no existe, crear uno nuevo */
    if (!sprite) {
        if (g_engine.num_sprites >= g_engine.sprites_capacity) {
            fprintf(stderr, "RAY: Máximo de sprites alcanzado\n");
            return 0;
        }
        sprite = &g_engine.sprites[g_engine.num_sprites++];
        memset(sprite, 0, sizeof(RAY_Sprite));
    }
    
    /* Configurar sprite con datos de la flag */
    sprite->x = flag->x;
    sprite->y = flag->y;
    sprite->z = flag->z;
    sprite->level = flag->level;
    sprite->process_ptr = my;  /* Vincular al proceso */
    sprite->flag_id = flag_id;
    sprite->w = 128;  /* Tamaño por defecto */
    sprite->h = 128;
    sprite->textureID = 0;  /* Se usará el graph del proceso */
    sprite->hidden = 0;
    sprite->cleanup = 0;
    
    /* Marcar flag como ocupada */
    flag->occupied = 1;
    flag->process_ptr = my;
    
    printf("RAY: Proceso %p vinculado a flag %d en posición (%.1f, %.1f, %.1f)\n",
           (void*)my, flag_id, flag->x, flag->y, flag->z);
    
    return 1;
}

int64_t libmod_ray_clear_flag(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    if (!my) return 0;
    
    /* Buscar el sprite de este proceso */
    for (int i = 0; i < g_engine.num_sprites; i++) {
        if (g_engine.sprites[i].process_ptr == my) {
            int flag_id = g_engine.sprites[i].flag_id;
            
            /* Liberar la flag */
            for (int j = 0; j < g_engine.num_spawn_flags; j++) {
                if (g_engine.spawn_flags[j].flag_id == flag_id) {
                    g_engine.spawn_flags[j].occupied = 0;
                    g_engine.spawn_flags[j].process_ptr = NULL;
                    break;
                }
            }
            
            /* Marcar sprite para eliminación */
            g_engine.sprites[i].cleanup = 1;
            
            printf("RAY: Proceso %p desvinculado de flag %d\n", (void*)my, flag_id);
            return 1;
        }
    }
    
    return 0;
}

int64_t libmod_ray_get_flag_x(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    int flag_id = (int)params[0];
    
    for (int i = 0; i < g_engine.num_spawn_flags; i++) {
        if (g_engine.spawn_flags[i].flag_id == flag_id) {
            return *(int64_t*)&g_engine.spawn_flags[i].x;
        }
    }
    
    return 0;
}

int64_t libmod_ray_get_flag_y(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    int flag_id = (int)params[0];
    
    for (int i = 0; i < g_engine.num_spawn_flags; i++) {
        if (g_engine.spawn_flags[i].flag_id == flag_id) {
            return *(int64_t*)&g_engine.spawn_flags[i].y;
        }
    }
    
    return 0;
}

int64_t libmod_ray_get_flag_z(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    int flag_id = (int)params[0];
    
    for (int i = 0; i < g_engine.num_spawn_flags; i++) {
        if (g_engine.spawn_flags[i].flag_id == flag_id) {
            return *(int64_t*)&g_engine.spawn_flags[i].z;
        }
    }
    
    return 0;
}

int64_t libmod_ray_update_sprite_position(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) {
        fprintf(stderr, "RAY_UPDATE_SPRITE_POSITION: Motor no inicializado\n");
        return 0;
    }
    if (!my) {
        fprintf(stderr, "RAY_UPDATE_SPRITE_POSITION: Instancia nula\n");
        return 0;
    }
    
    float x = *(float*)&params[0];
    float y = *(float*)&params[1];
    float z = *(float*)&params[2];
    
    /* Verificar que los valores son válidos */
    if (isnan(x) || isnan(y) || isnan(z)) {
        fprintf(stderr, "RAY_UPDATE_SPRITE_POSITION: Valores NaN detectados (x=%f, y=%f, z=%f)\n", x, y, z);
        return 0;
    }
    
    /* Buscar el sprite vinculado a este proceso */
    for (int i = 0; i < g_engine.num_sprites; i++) {
        if (g_engine.sprites[i].process_ptr == my) {
            g_engine.sprites[i].x = x;
            g_engine.sprites[i].y = y;
            g_engine.sprites[i].z = z;
            return 1;
        }
    }
    
    /* No se encontró el sprite - esto es normal si aún no se ha llamado a RAY_SET_FLAG */
    return 0;
}

/* ============================================================================
   FOG Y MINIMAPA - Configuración
   ============================================================================ */

int64_t libmod_ray_set_fog(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    g_engine.fogOn = (int)params[0];
    g_engine.fog_r = (uint8_t)params[1];
    g_engine.fog_g = (uint8_t)params[2];
    g_engine.fog_b = (uint8_t)params[3];
    g_engine.fog_start_distance = *(float*)&params[4];
    g_engine.fog_end_distance = *(float*)&params[5];
    
    return 1;
}

int64_t libmod_ray_set_minimap(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    g_engine.drawMiniMap = (int)params[0];
    g_engine.minimap_size = (int)params[1];
    g_engine.minimap_x = (int)params[2];
    g_engine.minimap_y = (int)params[3];
    g_engine.minimap_scale = *(float*)&params[4];
    
    return 1;
}

/* ============================================================================
   RENDERIZADO
   ============================================================================ */

/* Declaración de función de renderizado completo */
extern void ray_render_frame(GRAPH *dest);

int64_t libmod_ray_render(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) {
        fprintf(stderr, "RAY: Motor no inicializado\n");
        return 0;
    }
    
    
    /* Crear graph la primera vez si no existe */
    if (!render_graph) {
        /* Crear graph la primera vez usando bitmap_new_syslib */
        printf("RAY: Creating render graph %dx%d\n", g_engine.displayWidth, g_engine.displayHeight);
        render_graph = bitmap_new_syslib(g_engine.displayWidth, g_engine.displayHeight);
        printf("RAY: bitmap_new_syslib returned %p\n", (void*)render_graph);
        
        if (!render_graph) {
            fprintf(stderr, "RAY: No se pudo crear graph de renderizado\n");
            return 0;
        }
    }
    
    /* Renderizar frame completo */
    ray_render_frame(render_graph);
    
    /* Retornar el code del graph para que BennuGD lo muestre */
    return render_graph->code;
}

/* ============================================================================
   CARGA DE MAPAS
   ============================================================================ */

/* Declaraciones de funciones de carga de mapas */
extern int64_t libmod_ray_load_map(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_free_map(INSTANCE *my, int64_t *params);

/* ============================================================================
   HOOKS DEL MÓDULO
   ============================================================================ */

void __bgdexport(libmod_ray, module_initialize)() {
    printf("RAY: Módulo libmod_ray cargado\n");
}

void __bgdexport(libmod_ray, module_finalize)() {
    if (g_engine.initialized) {
        libmod_ray_shutdown(NULL, NULL);
    }
    printf("RAY: Módulo libmod_ray descargado\n");
}

/* Incluir exports al final del archivo */
#include "libmod_ray_exports.h"

