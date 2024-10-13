#ifndef __LIBMOD_TEXTO_EXPORTS
#define __LIBMOD_TEXTO_EXPORTS

#include "bgddl.h"

// Declaración de las funciones exportadas
DLSYSFUNCS  __bgdexport( libmod_texto, functions_exports )[] = {
    FUNC("MOSTRAR_TEXTO", "S", TYPE_INT, mostrarTexto),
    FUNC(0, 0, 0, 0)  // Final de la lista de funciones
};

char * __bgdexport( libmod_texto, modules_dependency )[] = {
    "libbggfx",
    "libbginput",
    NULL
};

#endif

