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
    {"SPRITE_INVISIBLE", TYPE_INT, 1}, {NULL, 0, 0}};

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
    FUNC("RAY_CAMERA_UPDATE", "F", TYPE_INT, libmod_ray_camera_update),
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
    FUNC("RAY_CHECK_COLLISION_Z", "FFFFF", TYPE_INT,
         libmod_ray_check_collision_z),
    FUNC("RAY_CHECK_COLLISION_EXT", "FFFFFF", TYPE_INT,
         libmod_ray_check_collision_h),
    FUNC("RAY_TOGGLE_DOOR", "", TYPE_INT, libmod_ray_toggle_door),
    FUNC("RAY_ADD_SPRITE", "FFFIIIII", TYPE_INT, libmod_ray_add_sprite),
    FUNC("RAY_SET_FLAG", "I", TYPE_INT, libmod_ray_set_flag),
    FUNC("RAY_CLEAR_FLAG", "", TYPE_INT, libmod_ray_clear_flag),
    FUNC("RAY_GET_FLAG_X", "I", TYPE_FLOAT, libmod_ray_get_flag_x),
    FUNC("RAY_GET_FLAG_Y", "I", TYPE_FLOAT, libmod_ray_get_flag_y),
    FUNC("RAY_GET_FLAG_Z", "I", TYPE_FLOAT, libmod_ray_get_flag_z),
    FUNC("RAY_UPDATE_SPRITE_POSITION", "IFFF", TYPE_INT,
         libmod_ray_update_sprite_position),
    FUNC("RAY_REMOVE_SPRITE", "I", TYPE_INT, libmod_ray_remove_sprite),
    FUNC("RAY_LOAD_MD2", "S", TYPE_INT, libmod_ray_load_md2),
    FUNC("RAY_LOAD_MD3", "S", TYPE_INT, libmod_ray_load_md3),
    FUNC("RAY_LOAD_GLTF", "S", TYPE_INT, libmod_ray_load_gltf),
    FUNC("RAY_GET_GLTF_ANIM_COUNT", "I", TYPE_INT,
         libmod_ray_get_gltf_anim_count),
    FUNC("RAY_SET_SPRITE_MD2", "III", TYPE_INT, libmod_ray_set_sprite_md2),
    FUNC("RAY_SET_SPRITE_MD3", "III", TYPE_INT, libmod_ray_set_sprite_md3),
    FUNC("RAY_SET_SPRITE_GLTF", "II", TYPE_INT, libmod_ray_set_sprite_gltf),
    FUNC("RAY_SET_SPRITE_ANIM", "IIIF", TYPE_INT, libmod_ray_set_sprite_anim),
    FUNC("RAY_SET_SPRITE_GLB_ANIM", "IIF", TYPE_INT,
         libmod_ray_set_sprite_glb_anim),
    FUNC("RAY_SET_SPRITE_GLB_SPEED", "IF", TYPE_INT,
         libmod_ray_set_sprite_glb_speed),
    FUNC("RAY_SET_SPRITE_ANGLE", "IF", TYPE_INT, libmod_ray_set_sprite_angle),
    FUNC("RAY_SET_SPRITE_SCALE", "IF", TYPE_INT, libmod_ray_set_sprite_scale),
    FUNC("RAY_SET_SPRITE_FLAGS", "II", TYPE_INT, libmod_ray_set_sprite_flags),
    FUNC("RAY_SET_SPRITE_GRAPH", "II", TYPE_INT, libmod_ray_set_sprite_graph),
    FUNC("RAY_SET_COLLISION_BOX", "IFFF", TYPE_INT,
         libmod_ray_set_collision_box),
    FUNC("RAY_GET_COLLISION", "I", TYPE_INT, libmod_ray_get_collision),
    FUNC("RAY_GET_SPRITE_X", "I", TYPE_FLOAT, libmod_ray_get_sprite_x),
    FUNC("RAY_GET_SPRITE_Y", "I", TYPE_FLOAT, libmod_ray_get_sprite_y),
    FUNC("RAY_GET_SPRITE_Z", "I", TYPE_FLOAT, libmod_ray_get_sprite_z),
    FUNC("RAY_GET_FLOOR_HEIGHT", "FF", TYPE_FLOAT, libmod_ray_get_floor_height),
    FUNC("RAY_GET_TAG_POINT", "ISPPP", TYPE_INT, libmod_ray_get_tag_point),
    FUNC("RAY_SET_TEXTURE_QUALITY", "I", TYPE_INT,
         libmod_ray_set_texture_quality),
    FUNC("RAY_CAMERA_LOAD", "S", TYPE_INT, libmod_ray_camera_load),
    FUNC("RAY_CAMERA_PLAY", "I", TYPE_INT, libmod_ray_camera_play),
    FUNC("RAY_CAMERA_IS_PLAYING", "", TYPE_INT, libmod_ray_camera_is_playing),
    FUNC("RAY_CAMERA_PATH_UPDATE", "F", TYPE_INT,
         libmod_ray_camera_path_update),
    FUNC("RAY_CAMERA_STOP", "", TYPE_INT, libmod_ray_camera_stop),
    FUNC("RAY_CAMERA_PAUSE", "", TYPE_INT, libmod_ray_camera_pause),
    FUNC("RAY_CAMERA_RESUME", "", TYPE_INT, libmod_ray_camera_resume),
    FUNC("RAY_CAMERA_GET_TIME", "", TYPE_FLOAT, libmod_ray_camera_get_time),
    FUNC("RAY_CAMERA_SET_TIME", "F", TYPE_INT, libmod_ray_camera_set_time),
    FUNC("RAY_CAMERA_FREE", "I", TYPE_INT, libmod_ray_camera_free),
    FUNC("RAY_SET_FOV", "F", TYPE_INT, libmod_ray_set_fov),
    FUNC("RAY_SET_SPRITE_MD3_SURFACE", "III", TYPE_INT,
         libmod_ray_set_sprite_md3_surface_texture),
    FUNC("RAY_LIGHT_ADD", "FFFIIIFF", TYPE_INT, libmod_ray_add_light),
    FUNC("RAY_LIGHT_CLEAR", "", TYPE_INT, libmod_ray_clear_lights),
    FUNC("RAY_MOVE_SPRITE", "IFF", TYPE_INT, libmod_ray_move_sprite),
    FUNC("RAY_SET_STEP_HEIGHT", "F", TYPE_INT, libmod_ray_set_step_height),
    FUNC("RAY_GET_FLOOR_HEIGHT_Z", "FFF", TYPE_FLOAT,
         libmod_ray_get_floor_height_z),
    /* Physics Engine */
    FUNC("RAY_PHYSICS_ENABLE", "IFFF", TYPE_INT, libmod_ray_physics_enable),
    FUNC("RAY_PHYSICS_SET_MASS", "IF", TYPE_INT, libmod_ray_physics_set_mass),
    FUNC("RAY_PHYSICS_SET_FRICTION", "IF", TYPE_INT,
         libmod_ray_physics_set_friction),
    FUNC("RAY_PHYSICS_SET_RESTITUTION", "IF", TYPE_INT,
         libmod_ray_physics_set_restitution),
    FUNC("RAY_PHYSICS_SET_GRAVITY_SCALE", "IF", TYPE_INT,
         libmod_ray_physics_set_gravity_scale),
    FUNC("RAY_PHYSICS_SET_DAMPING", "IFF", TYPE_INT,
         libmod_ray_physics_set_damping),
    FUNC("RAY_PHYSICS_SET_STATIC", "II", TYPE_INT,
         libmod_ray_physics_set_static),
    FUNC("RAY_PHYSICS_SET_KINEMATIC", "II", TYPE_INT,
         libmod_ray_physics_set_kinematic),
    FUNC("RAY_PHYSICS_SET_TRIGGER", "II", TYPE_INT,
         libmod_ray_physics_set_trigger),
    FUNC("RAY_PHYSICS_LOCK_ROTATION", "IIII", TYPE_INT,
         libmod_ray_physics_set_lock_rotation),
    FUNC("RAY_PHYSICS_SET_LAYER", "III", TYPE_INT,
         libmod_ray_physics_set_collision_layer),
    FUNC("RAY_PHYSICS_APPLY_FORCE", "IFFF", TYPE_INT,
         libmod_ray_physics_apply_force_bgd),
    FUNC("RAY_PHYSICS_APPLY_IMPULSE", "IFFF", TYPE_INT,
         libmod_ray_physics_apply_impulse_bgd),
    FUNC("RAY_PHYSICS_GET_VELOCITY", "II", TYPE_FLOAT,
         libmod_ray_physics_get_velocity),
    FUNC("RAY_PHYSICS_STEP", "F", TYPE_INT, libmod_ray_physics_step_bgd),
    /* Sprite-to-Sprite Collision */
    FUNC("RAY_CHECK_SPRITE_COLLISION", "IFFF", TYPE_INT,
         libmod_ray_check_sprite_collision),
    /* Distance Functions */
    FUNC("RAY_GET_DIST", "II", TYPE_FLOAT, libmod_ray_get_dist),
    FUNC("RAY_GET_CAMERA_DIST", "I", TYPE_FLOAT, libmod_ray_get_camera_dist),
    FUNC("RAY_GET_POINT_DIST", "FFFFFF", TYPE_FLOAT, libmod_ray_get_point_dist),
    FUNC("RAY_GET_ANGLE", "II", TYPE_FLOAT, libmod_ray_get_angle),
    FUNC("RAY_GET_CAMERA_ANGLE", "I", TYPE_FLOAT, libmod_ray_get_camera_angle),
    FUNC(NULL, NULL, 0, NULL)};

#endif

/* Hooks del módulo */
void __bgdexport(libmod_ray, module_initialize)();
void __bgdexport(libmod_ray, module_finalize)();

#endif /* __LIBMOD_RAY_EXPORTS */
