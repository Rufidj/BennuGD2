/*
 * libmod_ray - Raycasting Module Exports for BennuGD2
 * Port of Andrew Lim's SDL2 Raycasting Engine
 */

#ifndef __LIBMOD_RAY_EXPORTS
#define __LIBMOD_RAY_EXPORTS

#include "bgddl.h"

#if defined(__BGDC__) || !defined(__STATIC__)

#include "libmod_ray.h"

/* Constantes exportadas */
DLCONSTANT __bgdexport(libmod_ray, constants_def)[] = {
    { NULL, 0, 0 }
};

DLSYSFUNCS __bgdexport(libmod_ray, functions_exports)[] = {
    FUNC("RAY_INIT", "IIII", TYPE_INT, libmod_ray_init),
    FUNC("RAY_SHUTDOWN", "", TYPE_INT, libmod_ray_shutdown),
    FUNC("RAY_LOAD_MAP", "SI", TYPE_INT, libmod_ray_load_map),
    FUNC("RAY_FREE_MAP", "", TYPE_INT, libmod_ray_free_map),
    FUNC("RAY_RENDER", "I", TYPE_INT, libmod_ray_render),
    FUNC("RAY_MOVE_FORWARD", "F", TYPE_INT, libmod_ray_move_forward),
    FUNC("RAY_MOVE_BACKWARD", "F", TYPE_INT, libmod_ray_move_backward),
    FUNC("RAY_STRAFE_LEFT", "F", TYPE_INT, libmod_ray_strafe_left),
    FUNC("RAY_STRAFE_RIGHT", "F", TYPE_INT, libmod_ray_strafe_right),
    FUNC("RAY_ROTATE", "F", TYPE_INT, libmod_ray_rotate),
    FUNC("RAY_LOOK_UP_DOWN", "F", TYPE_INT, libmod_ray_look_up_down),
    FUNC("RAY_MOVE_UP_DOWN", "F", TYPE_INT, libmod_ray_move_up_down),
    FUNC("RAY_JUMP", "", TYPE_INT, libmod_ray_jump),
    FUNC("RAY_SET_CAMERA", "FFFFF", TYPE_INT, libmod_ray_set_camera),
    FUNC("RAY_GET_CAMERA_X", "", TYPE_FLOAT, libmod_ray_get_camera_x),
    FUNC("RAY_GET_CAMERA_Y", "", TYPE_FLOAT, libmod_ray_get_camera_y),
    FUNC("RAY_GET_CAMERA_Z", "", TYPE_FLOAT, libmod_ray_get_camera_z),
    FUNC("RAY_GET_CAMERA_ROT", "", TYPE_FLOAT, libmod_ray_get_camera_rot),
    FUNC("RAY_GET_CAMERA_PITCH", "", TYPE_FLOAT, libmod_ray_get_camera_pitch),
    FUNC("RAY_GET_CAMERA_SECTOR", "", TYPE_INT, libmod_ray_get_camera_sector),
    FUNC("RAY_SET_FOG", "IIIIFF", TYPE_INT, libmod_ray_set_fog),
    FUNC("RAY_SET_DRAW_MINIMAP", "I", TYPE_INT, libmod_ray_set_draw_minimap),
    FUNC("RAY_SET_MINIMAP", "IIIIF", TYPE_INT, libmod_ray_set_minimap),
    FUNC("RAY_SET_DRAW_WEAPON", "I", TYPE_INT, libmod_ray_set_draw_weapon),
    FUNC("RAY_SET_SKY_TEXTURE", "I", TYPE_INT, libmod_ray_set_sky_texture),
    FUNC("RAY_SET_BILLBOARD", "II", TYPE_INT, libmod_ray_set_billboard),
    FUNC("RAY_CHECK_COLLISION", "FFFF", TYPE_INT, libmod_ray_check_collision),
    FUNC("RAY_TOGGLE_DOOR", "", TYPE_INT, libmod_ray_toggle_door),
    FUNC("RAY_ADD_SPRITE", "FFFIII", TYPE_INT, libmod_ray_add_sprite),
    FUNC("RAY_SET_FLAG", "I", TYPE_INT, libmod_ray_set_flag),
    FUNC("RAY_CLEAR_FLAG", "", TYPE_INT, libmod_ray_clear_flag),
    FUNC("RAY_GET_FLAG_X", "I", TYPE_FLOAT, libmod_ray_get_flag_x),
    FUNC("RAY_GET_FLAG_Y", "I", TYPE_FLOAT, libmod_ray_get_flag_y),
    FUNC("RAY_GET_FLAG_Z", "I", TYPE_FLOAT, libmod_ray_get_flag_z),
    FUNC("RAY_UPDATE_SPRITE_POSITION", "IFFF", TYPE_INT, libmod_ray_update_sprite_position),
    FUNC("RAY_REMOVE_SPRITE", "I", TYPE_INT, libmod_ray_remove_sprite),
    FUNC("RAY_LOAD_MD2", "S", TYPE_INT, libmod_ray_load_md2),
    FUNC("RAY_LOAD_MD3", "S", TYPE_INT, libmod_ray_load_md3),
    FUNC("RAY_SET_SPRITE_MD2", "III", TYPE_INT, libmod_ray_set_sprite_md2),
    FUNC("RAY_SET_SPRITE_ANIM", "IIIF", TYPE_INT, libmod_ray_set_sprite_anim),
    FUNC("RAY_SET_SPRITE_ANGLE", "IF", TYPE_INT, libmod_ray_set_sprite_angle),
    FUNC("RAY_SET_SPRITE_SCALE", "IF", TYPE_INT, libmod_ray_set_sprite_scale),
    FUNC("RAY_GET_FLOOR_HEIGHT", "FF", TYPE_FLOAT, libmod_ray_get_floor_height),
    FUNC("RAY_GET_TAG_POINT", "ISPPP", TYPE_INT, libmod_ray_get_tag_point),
    FUNC("RAY_SET_TEXTURE_QUALITY", "I", TYPE_INT, libmod_ray_set_texture_quality),
    FUNC(NULL, NULL, 0, NULL)
};

#endif

/* Hooks del módulo */
void __bgdexport(libmod_ray, module_initialize)();
void __bgdexport(libmod_ray, module_finalize)();

#endif /* __LIBMOD_RAY_EXPORTS */
