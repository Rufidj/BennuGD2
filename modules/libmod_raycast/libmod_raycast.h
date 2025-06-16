/*  
 *  Copyright (C) SplinterGU (Fenix/BennuGD) (Since 2006)  
 *    
 *  This file is part of Bennu Game Development  
 */  
  
#ifndef __LIBMOD_RAYCAST_H  
#define __LIBMOD_RAYCAST_H  
  
#include <stdint.h>  
#include "bgddl.h"  
  
/* --------------------------------------------------------------------------- */  
/* Estructuras del motor de raycast */  
/* --------------------------------------------------------------------------- */  
  
typedef struct {  
    int initialized;  
      
    // Configuración de pantalla  
    int screen_width;  
    int screen_height;  
      
    // Configuración del jugador  
    double player_x;  
    double player_y;  
    double player_angle;  
      
    // Configuración de renderizado  
    double fov;           // Campo de visión  
    double max_distance;  // Distancia máxima de renderizado  
      
    // Mapa  
    int *map;  
    int map_width;  
    int map_height;  
      
} RaycastEngine;  
  
/* --------------------------------------------------------------------------- */  
/* Declaraciones de funciones exportadas - SIN static */  
/* --------------------------------------------------------------------------- */  
  
int64_t libmod_raycast_init_engine(INSTANCE *my, int64_t *params);  
int64_t libmod_raycast_render_frame(INSTANCE *my, int64_t *params);  
int64_t libmod_raycast_set_player_pos(INSTANCE *my, int64_t *params);  
int64_t libmod_raycast_load_map(INSTANCE *my, int64_t *params);  
int64_t libmod_raycast_set_wall_texture(INSTANCE *my, int64_t *params);  
int64_t libmod_raycast_move_player(INSTANCE *my, int64_t *params);  
int64_t libmod_raycast_rotate_player(INSTANCE *my, int64_t *params);  
  
/* --------------------------------------------------------------------------- */  
/* Funciones de inicialización del módulo */  
/* --------------------------------------------------------------------------- */  
  
void __bgdexport(libmod_raycast, module_initialize)();  
void __bgdexport(libmod_raycast, module_finalize)();  
  
/* --------------------------------------------------------------------------- */  
  
#endif