/*
 * libmod_ray_raycasting.c - Core Raycasting Functions
 * Port of raycasting.cpp from Andrew Lim's raycasting engine
 */

#include "libmod_ray.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
   RAYCASTER - GRID MANAGEMENT
   ============================================================================ */

void ray_raycaster_create_grids(RAY_Raycaster *rc, int width, int height, int count, int tileSize)
{
    rc->gridWidth = width;
    rc->gridHeight = height;
    rc->gridCount = count;
    rc->tileSize = tileSize;
    
    rc->grids = (int**)malloc(count * sizeof(int*));
    for (int z = 0; z < count; z++) {
        rc->grids[z] = (int*)calloc(width * height, sizeof(int));
    }
}

/* ============================================================================
   RAYCASTER - UTILITY FUNCTIONS
   ============================================================================ */

/* Port of Raycaster::screenDistance() */
float ray_screen_distance(float screenWidth, float fovRadians)
{
    return (screenWidth / 2.0f) / tanf(fovRadians / 2.0f);
}

/* Port of Raycaster::stripAngle() */
float ray_strip_angle(float screenX, float screenDistance)
{
    return atanf(screenX / screenDistance);
}

/* Port of Raycaster::stripScreenHeight() */
float ray_strip_screen_height(float screenDistance, float correctDistance, float tileSize)
{
    return floorf(screenDistance / correctDistance * tileSize + 0.5f);
}

/* ============================================================================
   RAYCASTER - HELPER FUNCTIONS
   ============================================================================ */

static int any_space_below(RAY_Raycaster *rc, int x, int y, int z)
{
    if (z == 0) {
        int *grid = rc->grids[0];
        if (x >= 0 && y >= 0) {
            if (ray_is_door(grid[x + y * rc->gridWidth])) {
                return 1;
            }
        }
    }
    
    for (int level = z - 1; level >= 0; level--) {
        int *grid = rc->grids[level];
        int gridOffset = x + y * rc->gridWidth;
        if (grid[gridOffset] == 0 || ray_is_door(grid[gridOffset])) {
            return 1;
        }
    }
    return 0;
}

static int any_space_above(RAY_Raycaster *rc, int x, int y, int z)
{
    /* MODIFICADO: Siempre retornar 1 para permitir renderizado multi-nivel */
    /* Esto permite ver estructuras flotantes y torres en niveles superiores */
    /* incluso si hay paredes en niveles inferiores */
    return 1;
    
    /* Código original comentado - implementaba oclusión estricta
    if (z == 0) {
        int *grid = rc->grids[0];
        if (x >= 0 && y >= 0) {
            if (ray_is_door(grid[x + y * rc->gridWidth])) {
                return 1;
            }
        }
    }
    
    for (int level = z + 1; level < rc->gridCount; level++) {
        int *grid = rc->grids[level];
        int gridOffset = x + y * rc->gridWidth;
        if (grid[gridOffset] == 0 || ray_is_door(grid[gridOffset])) {
            return 1;
        }
    }
    return 0;
    */
}

static int needs_next_wall(RAY_Raycaster *rc, float playerZ, int x, int y, int z)
{
    /* Siempre permitir puertas */
    if (z == 0) {
        int *grid = rc->grids[0];
        if (x >= 0 && y >= 0) {
            if (ray_is_door(grid[x + y * rc->gridWidth])) {
                return 1;
            }
        }
    }
    
    /* Calcular altura del ojo y límites del nivel */
    float eyeHeight = rc->tileSize / 2.0f + playerZ;
    float wallBottom = z * rc->tileSize;
    float wallTop = wallBottom + rc->tileSize;
    
    /* Si el ojo está dentro del nivel actual, no buscar más niveles
     * Esto hace que desde dentro de una habitación solo veas ese nivel */
    if (eyeHeight >= wallBottom && eyeHeight <= wallTop) {
        return 0;  // Estamos dentro de este nivel, no buscar más
    }
    
    /* Si el ojo está por encima o por debajo, continuar buscando
     * Esto permite ver edificios completos desde fuera */
    int eyeAboveWall = eyeHeight > wallTop;
    int eyeBelowWall = eyeHeight < wallBottom;
    
    if (eyeAboveWall) {
        return any_space_above(rc, x, y, z);
    }
    if (eyeBelowWall) {
        return any_space_below(rc, x, y, z);
    }
    
    return 0;
}

/* Usar fmod para texturas (compatible con motor original) */
static float fmod2(float a, int b)
{
    return fmodf(a, (float)b);
}

/* ============================================================================
   RAYCASTER - MAIN RAYCAST FUNCTION
   ============================================================================ */

void ray_raycaster_raycast(RAY_Raycaster *rc, RAY_RayHit *hits, int *num_hits,
                           int playerX, int playerY, float playerZ,
                           float playerRot, float stripAngle, int stripIdx,
                           RAY_Sprite *sprites, int num_sprites)
{
    if (!rc->grids || rc->gridCount == 0) {
        *num_hits = 0;
        return;
    }
    
    int hit_count = 0;
    const int max_hits = RAY_MAX_RAYHITS;
    
    /* Calcular ángulo del rayo */
    float rayAngle = stripAngle + playerRot;
    while (rayAngle < 0) rayAngle += RAY_TWO_PI;
    while (rayAngle >= RAY_TWO_PI) rayAngle -= RAY_TWO_PI;
    
    /* Determinar cuadrante */
    int right = (rayAngle < RAY_TWO_PI * 0.25f && rayAngle >= 0) ||
                (rayAngle > RAY_TWO_PI * 0.75f);
    int up = rayAngle < RAY_TWO_PI * 0.5f && rayAngle >= 0;
    
    int *groundGrid = rc->grids[0];
    int currentTileX = playerX / rc->tileSize;
    int currentTileY = playerY / rc->tileSize;
    
    /* Iterar por cada nivel del grid */
    for (int level = 0; level < rc->gridCount; level++) {
        int *grid = rc->grids[level];
        
        /* ========================================
           VERTICAL LINES CHECKING
           ======================================== */
        
        /* Find x coordinate of vertical lines on the right and left */
        float vx = 0;
        if (right) {
            vx = floorf(playerX / rc->tileSize) * rc->tileSize + rc->tileSize;
        } else {
            vx = floorf(playerX / rc->tileSize) * rc->tileSize - 1;
        }
        
        /* Calculate y coordinate of those lines */
        float vy = playerY + (playerX - vx) * tanf(rayAngle);
        
        /* Calculate stepping vector for each line */
        float stepx = right ? rc->tileSize : -rc->tileSize;
        float stepy = rc->tileSize * tanf(rayAngle);
        
        /* tan() returns positive values in Quadrant 1 and Quadrant 4 but window
         * coordinates need negative coordinates for Y-axis so we reverse */
        if (right) {
            stepy = -stepy;
        }
        
        /* Recorrer líneas verticales */
        /* NOTA: No verificamos límites en el while para permitir detectar muros en los bordes */
        for (int step = 0; step < 100; step++) /* Máximo 100 pasos para evitar loops infinitos */
        {
            int wallY = (int)floorf(vy / rc->tileSize);
            int wallX = (int)floorf(vx / rc->tileSize);
            
            /* Verificar que wallX y wallY estén dentro de los límites del grid */
            if (wallX < 0 || wallX >= rc->gridWidth || 
                wallY < 0 || wallY >= rc->gridHeight) {
                break; /* Salir del loop si estamos fuera del mapa */
            }
            
            int wallOffset = wallX + wallY * rc->gridWidth;
            
            
            /* Check if current cell is a wall (treat doors like normal walls) */
            if (grid[wallOffset] > 0) {
                
                float distX = playerX - vx;
                float distY = playerY - vy;
                float blockDist = distX * distX + distY * distY;
                
                if (blockDist > 0 && hit_count < max_hits) {
                    float texX = fmod2(vy, rc->tileSize);
                    texX = right ? texX : rc->tileSize - texX;
                    
                    RAY_RayHit *rayHit = &hits[hit_count];
                    rayHit->x = vx;
                    rayHit->y = vy;
                    rayHit->rayAngle = rayAngle;
                    rayHit->strip = stripIdx;
                    rayHit->wallType = grid[wallOffset];
                    rayHit->wallX = wallX;
                    rayHit->wallY = wallY;
                    rayHit->level = level;
                    rayHit->up = up;
                    rayHit->right = right;
                    rayHit->distance = sqrtf(blockDist);
                    rayHit->sortdistance = rayHit->distance;
                    rayHit->sprite = NULL;
                    rayHit->thinWall = NULL;
                    rayHit->wallHeight = rc->tileSize;
                    
                    rayHit->correctDistance = rayHit->distance * cosf(stripAngle);
                    rayHit->horizontal = 0;
                    rayHit->tileX = texX;
                    
                    hit_count++;
                    
                    int gaps = needs_next_wall(rc, playerZ, wallX, wallY, level);
                    if (!gaps) {
                        break;
                    }
                }
            }
            
            vx += stepx;
            vy += stepy;
        }
        
        /* ========================================
           HORIZONTAL LINES CHECKING
           ======================================== */
        
        /* Encontrar coordenada Y de líneas horizontales */
        float hy = 0;
        if (up) {
            hy = floorf(playerY / rc->tileSize) * rc->tileSize - 1;
        } else {
            hy = floorf(playerY / rc->tileSize) * rc->tileSize + rc->tileSize;
        }
        
        /* Calcular coordenada X */
        float hx = playerX + (playerY - hy) / tanf(rayAngle);
        stepy = up ? -rc->tileSize : rc->tileSize;
        stepx = rc->tileSize / tanf(rayAngle);
        
        if (!up) {
            stepx = -stepx;
        }
        
        /* Recorrer líneas horizontales */
        /* NOTA: No verificamos límites en el while para permitir detectar muros en los bordes */
        for (int step = 0; step < 100; step++) /* Máximo 100 pasos para evitar loops infinitos */
        {
            int wallY = (int)floorf(hy / rc->tileSize);
            int wallX = (int)floorf(hx / rc->tileSize);
            
            /* Verificar que wallX y wallY estén dentro de los límites del grid */
            if (wallX < 0 || wallX >= rc->gridWidth || 
                wallY < 0 || wallY >= rc->gridHeight) {
                break; /* Salir del loop si estamos fuera del mapa */
            }
            
            int wallOffset = wallX + wallY * rc->gridWidth;
            
            
            /* Check if current cell is a wall (treat doors like normal walls) */
            if (grid[wallOffset] > 0) {
                
                float distX = playerX - hx;
                float distY = playerY - hy;
                float blockDist = distX * distX + distY * distY;
                
                if (blockDist > 0 && hit_count < max_hits) {
                    float texX = fmod2(hx, rc->tileSize);
                    texX = up ? rc->tileSize - texX : texX;
                    
                    RAY_RayHit *rayHit = &hits[hit_count];
                    rayHit->x = hx;
                    rayHit->y = hy;
                    rayHit->rayAngle = rayAngle;
                    rayHit->strip = stripIdx;
                    rayHit->wallType = grid[wallOffset];
                    rayHit->wallX = wallX;
                    rayHit->wallY = wallY;
                    rayHit->level = level;
                    rayHit->up = up;
                    rayHit->right = right;
                    rayHit->distance = sqrtf(blockDist);
                    rayHit->sortdistance = rayHit->distance;
                    rayHit->sprite = NULL;
                    rayHit->thinWall = NULL;
                    rayHit->wallHeight = rc->tileSize;
                    
                    rayHit->correctDistance = rayHit->distance * cosf(stripAngle);
                    rayHit->horizontal = 1;
                    rayHit->tileX = texX;
                    
                    hit_count++;
                    
                    int gaps = needs_next_wall(rc, playerZ, wallX, wallY, level);
                    if (!gaps) {
                        break;
                    }
                }
            }
            
            hx += stepx;
            hy += stepy;
        }
    }
    
    
    
    /* ========================================
       DEDUPLICACIÓN DE HITS
       Si vertical y horizontal encontraron la misma pared,
       solo quedarnos con el más cercano
       ======================================== */
    
    /* Eliminar hits duplicados que están en la misma posición de grid */
    for (int i = 0; i < hit_count; i++) {
        if (hits[i].wallType == 0) continue; /* Ya eliminado */
        
        for (int j = i + 1; j < hit_count; j++) {
            if (hits[j].wallType == 0) continue; /* Ya eliminado */
            
            /* Si ambos hits están en el mismo tile y nivel */
            if (hits[i].wallX == hits[j].wallX && 
                hits[i].wallY == hits[j].wallY &&
                hits[i].level == hits[j].level) {
                
                /* Eliminar el más lejano */
                if (hits[i].distance < hits[j].distance) {
                    hits[j].wallType = 0; /* Marcar como eliminado */
                } else {
                    hits[i].wallType = 0; /* Marcar como eliminado */
                    break; /* Este hit ya fue eliminado, pasar al siguiente */
                }
            }
        }
    }
    
    /* Compactar array eliminando hits marcados como 0 */
    int write_idx = 0;
    for (int read_idx = 0; read_idx < hit_count; read_idx++) {
        if (hits[read_idx].wallType != 0) {
            if (write_idx != read_idx) {
                hits[write_idx] = hits[read_idx];
            }
            write_idx++;
        }
    }
    hit_count = write_idx;
    
    *num_hits = hit_count;
}
