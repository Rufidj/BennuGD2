/*  
 *  Copyright (C) SplinterGU (Fenix/BennuGD) (Since 2006)  
 *  
 *  This file is part of Bennu Game Development  
 */  
  
#ifndef __LIBMOD_RAYCAST_H  
#define __LIBMOD_RAYCAST_H  
  
#include "bgddl.h"  // Para el tipo INSTANCE  
#include <stdint.h>  
#include <math.h>  
#include <stdlib.h>  
#include <string.h>  
  
/* --------------------------------------------------------------------------- */  
  
// Constantes del motor de raycasting  
#define MAX_MAP_WIDTH 64  
#define MAX_MAP_HEIGHT 64  
#define MAX_TEXTURES 32  
#define MAX_WALL_TYPES 16  
#define FOV 60.0f  
#define PI 3.14159265359f  
  
/* --------------------------------------------------------------------------- */  
  
// Estructura del jugador  
typedef struct {  
    float x, y;           // Posición  
    float angle;          // Ángulo de vista  
    float fov;            // Campo de visión  
} Player;  
  
// Estructura de textura  
typedef struct {  
    int width, height;  
    uint32_t *data;  
} Texture;  
  
// Motor principal de raycasting  
typedef struct {  
    int width, height;    // Resolución del framebuffer  
    uint32_t *framebuffer; // Buffer de píxeles  
      
    Player player;        // Jugador  
      
    int map_width, map_height;  
    int *map_data;        // Datos del mapa como array 1D  
      
    Texture textures[MAX_TEXTURES];  
    int texture_count;  
      
    float *z_buffer;      // Buffer de profundidad  
    int initialized;  
} RaycastEngine;  
  
/* --------------------------------------------------------------------------- */  
  
// Funciones internas del motor (sin cast_ray ya que es estática)  
int raycast_init(RaycastEngine *engine, int width, int height);  
void raycast_cleanup(RaycastEngine *engine);  
void raycast_render(RaycastEngine *engine);  
int raycast_load_map(RaycastEngine *engine, int width, int height, int *data);  
int raycast_set_wall_texture(RaycastEngine *engine, int wall_id, int texture_id);  
void raycast_move_player(RaycastEngine *engine, float distance);  
void raycast_rotate_player(RaycastEngine *engine, float angle);  
  
/* --------------------------------------------------------------------------- */  
  
// Funciones exportadas para BennuGD  
extern int64_t libmod_raycast_init_engine(INSTANCE * my, int64_t * params);  
extern int64_t libmod_raycast_render_frame(INSTANCE * my, int64_t * params);  
extern int64_t libmod_raycast_set_player_pos(INSTANCE * my, int64_t * params);  
extern int64_t libmod_raycast_load_map(INSTANCE * my, int64_t * params);  
extern int64_t libmod_raycast_set_wall_texture(INSTANCE * my, int64_t * params);  
extern int64_t libmod_raycast_move_player(INSTANCE * my, int64_t * params);  
extern int64_t libmod_raycast_rotate_player(INSTANCE * my, int64_t * params);  
extern int64_t libmod_raycast_load_texture(INSTANCE * my, int64_t * params);  
extern int64_t libmod_raycast_get_player_pos(INSTANCE * my, int64_t * params);  
  
/* --------------------------------------------------------------------------- */  
  
// Funciones de inicialización del módulo  
extern void __bgdexport(libmod_raycast, module_initialize)();  
extern void __bgdexport(libmod_raycast, module_finalize)();  
  
/* --------------------------------------------------------------------------- */  
  
#endif