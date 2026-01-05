/*
 * Doom Engine Module Exports for BennuGD2
 */

#ifndef __LIBMOD_DOOM_EXPORTS
#define __LIBMOD_DOOM_EXPORTS

#include "bgddl.h"

#if defined(__BGDC__) || !defined(__STATIC__)

#include "libmod_doom.h"

/* Constantes exportadas */
DLCONSTANT __bgdexport(libmod_doom, constants_def)[] = {
    // Ángulos
    {"ANG45", TYPE_INT, ANG45},
    {"ANG90", TYPE_INT, ANG90},
    {"ANG180", TYPE_INT, ANG180},
    {"ANG270", TYPE_INT, ANG270},
    {"ANG1", TYPE_INT, ANG1},
    
    // Punto fijo
    {"FRACUNIT", TYPE_INT, FRACUNIT},
    
    {NULL, 0, 0}
};

DLSYSFUNCS __bgdexport(libmod_doom, functions_exports)[] = {
    // Inicialización y gestión
    FUNC("DOOM_INIT", "", TYPE_INT, libmod_doom_init),
    FUNC("DOOM_SHUTDOWN", "", TYPE_INT, libmod_doom_shutdown),
    
    // Carga de datos
    FUNC("DOOM_LOAD_WAD", "S", TYPE_INT, libmod_doom_load_wad),
    FUNC("DOOM_LOAD_MAP", "S", TYPE_INT, libmod_doom_load_map),
    
    // Renderizado
    FUNC("DOOM_RENDER_FRAME", "II", TYPE_INT, libmod_doom_render_frame),
    
    // Control de cámara
    FUNC("DOOM_SET_CAMERA", "IIII", TYPE_INT, libmod_doom_set_camera),
    FUNC("DOOM_MOVE_FORWARD", "I", TYPE_INT, libmod_doom_move_forward),
    FUNC("DOOM_MOVE_BACKWARD", "I", TYPE_INT, libmod_doom_move_backward),
    FUNC("DOOM_STRAFE_LEFT", "I", TYPE_INT, libmod_doom_strafe_left),
    FUNC("DOOM_STRAFE_RIGHT", "I", TYPE_INT, libmod_doom_strafe_right),
    FUNC("DOOM_TURN_LEFT", "I", TYPE_INT, libmod_doom_turn_left),
    FUNC("DOOM_TURN_RIGHT", "I", TYPE_INT, libmod_doom_turn_right),
    
    FUNC(0, 0, 0, 0)
};

#endif

/* Hooks del módulo */
void __bgdexport(libmod_doom, module_initialize)();
void __bgdexport(libmod_doom, module_finalize)();

#endif // __LIBMOD_DOOM_EXPORTS
