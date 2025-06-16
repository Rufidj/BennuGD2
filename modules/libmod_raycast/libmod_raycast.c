/*  
 *  Copyright (C) SplinterGU (Fenix/BennuGD) (Since 2006)  
 *    
 *  This file is part of Bennu Game Development  
 */  
  
#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include <math.h>  
  
#include "bgddl.h"  
#include "bgdrtm.h"  
#include "libbggfx.h"  
#include "libmod_raycast.h"  
  
/* --------------------------------------------------------------------------- */  
/* Variables globales del motor */  
/* --------------------------------------------------------------------------- */  
  
static RaycastEngine engine;  
static GRAPH *raycast_bitmap = NULL;  
  
/* --------------------------------------------------------------------------- */  
/* Funciones auxiliares */  
/* --------------------------------------------------------------------------- */  
  
static void raycast_cleanup(RaycastEngine *eng) {  
    if (eng->map) {  
        free(eng->map);  
        eng->map = NULL;  
    }  
    eng->initialized = 0;  
}  
  
static double normalize_angle(double angle) {  
    while (angle < 0) angle += 2 * M_PI;  
    while (angle >= 2 * M_PI) angle -= 2 * M_PI;  
    return angle;  
}  
  
static int get_map_value(RaycastEngine *eng, int x, int y) {  
    if (x < 0 || x >= eng->map_width || y < 0 || y >= eng->map_height) {  
        return 1; // Pared por defecto fuera del mapa  
    }  
    return eng->map[y * eng->map_width + x];  
}  
  
static void cast_ray(RaycastEngine *eng, double ray_angle, double *distance, int *wall_type) {  
    double ray_x = eng->player_x;  
    double ray_y = eng->player_y;  
    double ray_dx = cos(ray_angle);  
    double ray_dy = sin(ray_angle);  
      
    double step = 0.01; // Paso pequeño para el ray casting  
    *distance = 0;  
    *wall_type = 0;  
      
    while (*distance < eng->max_distance) {  
        ray_x += ray_dx * step;  
        ray_y += ray_dy * step;  
        *distance += step;  
          
        int map_x = (int)ray_x;  
        int map_y = (int)ray_y;  
          
        int wall = get_map_value(eng, map_x, map_y);  
        if (wall != 0) {  
            *wall_type = wall;  
            break;  
        }  
    }  
}  
  
static void render_column(RaycastEngine *eng, int x, double distance, int wall_type) {  
    if (!raycast_bitmap || !raycast_bitmap->surface) return;  
      
    // Calcular altura de la pared  
    double wall_height = (eng->screen_height / 2.0) / distance;  
    int wall_start = (eng->screen_height / 2) - (int)(wall_height / 2);  
    int wall_end = (eng->screen_height / 2) + (int)(wall_height / 2);  
      
    // Limitar a los bordes de la pantalla  
    if (wall_start < 0) wall_start = 0;  
    if (wall_end >= eng->screen_height) wall_end = eng->screen_height - 1;  
      
    // Color basado en el tipo de pared  
    uint32_t wall_color;  
    switch (wall_type) {  
        case 1: wall_color = 0xFF0000FF; break; // Rojo  
        case 2: wall_color = 0xFF00FF00; break; // Verde  
        case 3: wall_color = 0xFFFF0000; break; // Azul  
        default: wall_color = 0xFFFFFFFF; break; // Blanco  
    }  
      
    // Aplicar sombreado basado en distancia  
    double shade = 1.0 - (distance / eng->max_distance);  
    if (shade < 0.1) shade = 0.1;  
      
    uint8_t r = (uint8_t)((wall_color & 0xFF) * shade);  
    uint8_t g = (uint8_t)(((wall_color >> 8) & 0xFF) * shade);  
    uint8_t b = (uint8_t)(((wall_color >> 16) & 0xFF) * shade);  
    uint32_t shaded_color = 0xFF000000 | (b << 16) | (g << 8) | r;  
      
    // Dibujar la columna usando SDL_Surface  
    SDL_Surface *surface = raycast_bitmap->surface;  
    if (SDL_MUSTLOCK(surface)) SDL_LockSurface(surface);  
      
    uint32_t *pixels = (uint32_t*)surface->pixels;  
    int pitch = surface->pitch / 4;  
      
    for (int y = 0; y < eng->screen_height; y++) {  
        if (x >= 0 && x < eng->screen_width && y >= 0 && y < eng->screen_height) {  
            if (y < wall_start) {  
                // Cielo  
                pixels[y * pitch + x] = 0xFF87CEEB; // Sky blue  
            } else if (y >= wall_start && y <= wall_end) {  
                // Pared  
                pixels[y * pitch + x] = shaded_color;  
            } else {  
                // Suelo  
                pixels[y * pitch + x] = 0xFF8B4513; // Brown  
            }  
        }  
    }  
      
    if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);  
}  
  
/* --------------------------------------------------------------------------- */  
/* Funciones exportadas */  
/* --------------------------------------------------------------------------- */  
  
int64_t libmod_raycast_init_engine(INSTANCE *my, int64_t *params) {  
    int width = (int)params[0];  
    int height = (int)params[1];  
      
    if (width <= 0 || height <= 0) {  
        return 0;  
    }  
      
    // Limpiar motor anterior si existe  
    if (engine.initialized) {  
        raycast_cleanup(&engine);  
    }  
      
    // Inicializar estructura  
    memset(&engine, 0, sizeof(RaycastEngine));  
    engine.screen_width = width;  
    engine.screen_height = height;  
    engine.fov = M_PI / 3.0; // 60 grados  
    engine.max_distance = 20.0;  
    engine.player_x = 1.5;  
    engine.player_y = 1.5;  
    engine.player_angle = 0.0;  
      
    // Crear superficie SDL para el bitmap  
    SDL_Surface *surface = SDL_CreateRGBSurface(0, width, height, 32,   
                                                0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);  
    if (!surface) {  
        return 0;  
    }  
      
    // Crear bitmap para renderizado  
    if (raycast_bitmap) {  
        bitmap_destroy(raycast_bitmap);  
    }  
      
    raycast_bitmap = bitmap_new(0, width, height, surface);  
    SDL_FreeSurface(surface); // bitmap_new hace su propia copia  
      
    if (!raycast_bitmap) {  
        return 0;  
    }  
      
    engine.initialized = 1;  
    return raycast_bitmap->code; // Retornar ID del gráfico  
}  
  
int64_t libmod_raycast_render_frame(INSTANCE *my, int64_t *params) {  
    if (!engine.initialized || !raycast_bitmap || !raycast_bitmap->surface) {  
        return 0;  
    }  
      
    // Limpiar bitmap  
    SDL_FillRect(raycast_bitmap->surface, NULL, 0);  
      
    // Renderizar cada columna  
    for (int x = 0; x < engine.screen_width; x++) {  
        // Calcular ángulo del rayo  
        double camera_x = 2 * x / (double)engine.screen_width - 1;  
        double ray_angle = engine.player_angle + camera_x * (engine.fov / 2);  
        ray_angle = normalize_angle(ray_angle);  
          
        // Lanzar rayo  
        double distance;  
        int wall_type;  
        cast_ray(&engine, ray_angle, &distance, &wall_type);  
          
        // Corregir distancia por efecto ojo de pez  
        distance *= cos(ray_angle - engine.player_angle);  
          
        // Renderizar columna  
        render_column(&engine, x, distance, wall_type);  
    }  
      
    return 1;  
}  
  
int64_t libmod_raycast_set_player_pos(INSTANCE *my, int64_t *params) {  
    if (!engine.initialized) {  
        return 0;  
    }  
      
    engine.player_x = *(float*)&params[0];  
    engine.player_y = *(float*)&params[1];  
    engine.player_angle = normalize_angle(*(float*)&params[2]);  
      
    return 1;  
}  
  
int64_t libmod_raycast_load_map(INSTANCE *my, int64_t *params) {  
    if (!engine.initialized) {  
        return 0;  
    }  
      
    int width = (int)params[0];  
    int height = (int)params[1];  
    int *map_data = (int*)params[2];  
      
    if (width <= 0 || height <= 0 || !map_data) {  
        return 0;  
    }  
      
    // Liberar mapa anterior  
    if (engine.map) {  
        free(engine.map);  
    }  
      
    // Crear nuevo mapa  
    engine.map_width = width;  
    engine.map_height = height;  
    engine.map = malloc(width * height * sizeof(int));  
      
    if (!engine.map) {  
        return 0;  
    }  
      
    // Copiar datos del mapa  
    memcpy(engine.map, map_data, width * height * sizeof(int));  
      
    return 1;  
}  
  
int64_t libmod_raycast_set_wall_texture(INSTANCE *my, int64_t *params) {  
    if (!engine.initialized) {  
        return 0;  
    }  
      
    // Por ahora solo guardamos los IDs, implementación futura  
    int wall_type = (int)params[0];  
    int texture_id = (int)params[1];  
      
    // TODO: Implementar sistema de texturas  
    return 1;  
}  
  
int64_t libmod_raycast_move_player(INSTANCE *my, int64_t *params) {  
    if (!engine.initialized) {  
        return 0;  
    }  
      
    float distance = *(float*)&params[0];  
      
    double new_x = engine.player_x + cos(engine.player_angle) * distance;  
    double new_y = engine.player_y + sin(engine.player_angle) * distance;  
      
    // Verificar colisiones  
    if (get_map_value(&engine, (int)new_x, (int)engine.player_y) == 0) {  
        engine.player_x = new_x;  
    }  
    if (get_map_value(&engine, (int)engine.player_x, (int)new_y) == 0) {  
        engine.player_y = new_y;  
    }  
      
    return 1;  
}  
  
int64_t libmod_raycast_rotate_player(INSTANCE *my, int64_t *params) {  
    if (!engine.initialized) {  
        return 0;  
    }  
      
    float angle_delta = *(float*)&params[0];  
    engine.player_angle = normalize_angle(engine.player_angle + angle_delta);  
      
    return 1;  
}  
  
/* --------------------------------------------------------------------------- */  
/* Funciones de inicialización del módulo */  
/* --------------------------------------------------------------------------- */  
  
void __bgdexport(libmod_raycast, module_initialize)() {  
    memset(&engine, 0, sizeof(RaycastEngine));  
    engine.initialized = 0;  
    raycast_bitmap = NULL;  
}  
  
void __bgdexport(libmod_raycast, module_finalize)() {  
    if (engine.initialized) {  
        raycast_cleanup(&engine);  
    }  
    if (raycast_bitmap) {  
        bitmap_destroy(raycast_bitmap);  
        raycast_bitmap = NULL;  
    }  
}  
  
/* --------------------------------------------------------------------------- */  
/* exports                                                                     */  
/* --------------------------------------------------------------------------- */  
  
#include "libmod_raycast_exports.h"  
  
/* --------------------------------------------------------------------------- */