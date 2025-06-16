/*  
 *  Copyright (C) SplinterGU (Fenix/BennuGD) (Since 2006)  
 *  
 *  This file is part of Bennu Game Development  
 */  
  
#ifndef __RAYCAST_ENGINE_H  
#define __RAYCAST_ENGINE_H  
  
#include "libmod_raycast.h"  
  
// Funciones del motor de raycasting  
void raycast_render(RaycastEngine *engine);  
int raycast_load_map(RaycastEngine *engine, int width, int height, int *map_data);  
void raycast_move_player(RaycastEngine *engine, float distance);  
void raycast_cleanup(RaycastEngine *engine);  
  
// Funciones auxiliares  
float cast_ray(RaycastEngine *engine, float angle, int *wall_type, float *wall_hit_x);  
void render_column(RaycastEngine *engine, int x, float distance, int wall_type, float wall_hit_x);  
  
#endif