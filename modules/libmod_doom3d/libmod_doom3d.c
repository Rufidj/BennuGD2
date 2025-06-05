#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bgddl.h"
#include "libmod.h"
#include "SDL2/SDL.h"
#include "SDL2/SDL2_gfxPrimitives.h"

int moddoom3d_init(INSTANCE *my, int *params) { return 1; }
int moddoom3d_render(INSTANCE *my, int *params) { return 1; }
int moddoom3d_finish(INSTANCE *my, int *params) { return 1; }

DLCONSTANT constants[] = {
    { NULL, 0 }
};

DLVARFIXUP libdoom3d_var[] = {
    { NULL, NULL, -1 }
};

DLSYSFUNCS functions_exports[] = {
    { "DOOM3D_INIT", "II", TYPE_INT, moddoom3d_init },
    { "DOOM3D_RENDER", "", TYPE_INT, moddoom3d_render },
    { "DOOM3D_FINISH", "", TYPE_INT, moddoom3d_finish },
    { NULL, NULL, 0, NULL }
};

char * __bgdexport( libmod_doom3d, module_name ) ( void ) {
    return "libmod_doom3d";
}

DLVARFIXUP * __bgdexport( libmod_doom3d, globals ) ( void ) {
    return libdoom3d_var;
}

DLCONSTANT * __bgdexport( libmod_doom3d, constants ) ( void ) {
    return constants;
}

DLSYSFUNCS * __bgdexport( libmod_doom3d, functions_exports ) ( void ) {
    return functions_exports;
}