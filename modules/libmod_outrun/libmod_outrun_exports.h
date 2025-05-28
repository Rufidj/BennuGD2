#ifndef __LIBMOD_OUTRUN_EXPORTS
#define __LIBMOD_OUTRUN_EXPORTS

#include "bgddl.h"
#include "libmod_outrun.h"

#if defined(__BGDC__) || !defined(__STATIC__)

DLCONSTANT __bgdexport(libmod_outrun, constants_def)[] = {
    { NULL, 0, 0 }
};

#endif

DLSYSFUNCS __bgdexport(libmod_outrun, functions_exports)[] = {
    FUNC("DRAW_ROAD", "F", TYPE_INT, modoutrun_draw_road),
    FUNC(0, 0, 0, 0)
};

char * __bgdexport(libmod_outrun, modules_dependencies)[] = {
    "libgrbase", "libvideo", NULL
};

#endif