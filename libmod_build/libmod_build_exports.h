/*
 * libmod_build_exports.h
 * Build Engine Module Exports for BennuGD2
 */

#ifndef __LIBMOD_BUILD_EXPORTS
#define __LIBMOD_BUILD_EXPORTS

#include <stddef.h>
#include "bgddl.h"

#if defined(__BGDC__) || !defined(__STATIC__)

#include "libmod_build.h"

/* Constantes exportadas */
DLCONSTANT __bgdexport(libmod_build, constants_def)[] = {
    {NULL, 0, 0}
};

DLSYSFUNCS __bgdexport(libmod_build, functions_exports)[] = {
    // Map loading
    FUNC("BUILD_LOAD_MAP", "S", TYPE_INT, libmod_build_load_map),
    
    // Texture loading
    FUNC("BUILD_LOAD_PALETTE", "S", TYPE_INT, libmod_build_load_palette),
    FUNC("BUILD_LOAD_ART", "S", TYPE_INT, libmod_build_load_art),
    
    // Rendering
    // Parameters: width, height
    // Returns: render buffer code (GRAPH code)
    FUNC("BUILD_RENDER", "II", TYPE_INT, libmod_build_render),
    
    // Camera control
    // Parameters: x, y, z, ang, horiz, sectnum
    FUNC("BUILD_SET_CAMERA", "IIIIII", TYPE_INT, libmod_build_set_camera),
    
    // Movement functions
    // Parameter: speed (distance to move)
    FUNC("BUILD_MOVE_FORWARD", "I", TYPE_INT, libmod_build_move_forward),
    FUNC("BUILD_MOVE_BACKWARD", "I", TYPE_INT, libmod_build_move_backward),
    FUNC("BUILD_STRAFE_LEFT", "I", TYPE_INT, libmod_build_strafe_left),
    FUNC("BUILD_STRAFE_RIGHT", "I", TYPE_INT, libmod_build_strafe_right),
    FUNC("BUILD_MOVE_VERTICAL", "I", TYPE_INT, libmod_build_move_vertical),
    
    // Look functions
    // Parameter: delta (amount to rotate/tilt)
    FUNC("BUILD_LOOK_HORIZONTAL", "I", TYPE_INT, libmod_build_look_horizontal),
    FUNC("BUILD_LOOK_VERTICAL", "I", TYPE_INT, libmod_build_look_vertical),
    
    FUNC(0, 0, 0, 0)
};

#endif

/* Hooks del módulo */
void __bgdexport(libmod_build, module_initialize)();
void __bgdexport(libmod_build, module_finalize)();

#endif // __LIBMOD_BUILD_EXPORTS
