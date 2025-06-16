/*  
 *  Copyright (C) SplinterGU (Fenix/BennuGD) (Since 2006)  
 *  
 *  This file is part of Bennu Game Development  
 */  
  
#ifndef __LIBMOD_RAYCAST_EXPORTS_H  
#define __LIBMOD_RAYCAST_EXPORTS_H  
  
#include "bgddl.h"  
#include "libmod_raycast.h"  
  
// Definición de las funciones exportadas  
DLSYSFUNCS __bgdexport(libmod_raycast, functions_exports)[] = {  
    FUNC("raycast_init", "II", TYPE_INT, libmod_raycast_init_engine),  
    FUNC("raycast_render", "", TYPE_INT, libmod_raycast_render_frame),  
    FUNC("raycast_set_pos", "FFF", TYPE_INT, libmod_raycast_set_player_pos),  
    FUNC("raycast_load_map", "IIP", TYPE_INT, libmod_raycast_load_map),  
    FUNC("raycast_set_wall_texture", "II", TYPE_INT, libmod_raycast_set_wall_texture),  
    FUNC("raycast_move_player", "F", TYPE_INT, libmod_raycast_move_player),  
    FUNC("raycast_rotate_player", "F", TYPE_INT, libmod_raycast_rotate_player),  
    FUNC(0, 0, 0, 0)  
};
  
#endif