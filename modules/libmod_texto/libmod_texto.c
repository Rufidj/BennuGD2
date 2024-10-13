#include <stdio.h>
#include <stdint.h>
#include "bgddl.h"
#include "bgdrtm.h"
#include "dlvaracc.h"  // Para acceder a variables de BennuGD

static int texto_initialized = 0;

// Implementación de la función para mostrar texto
int64_t mostrarTexto( INSTANCE * my, int64_t * params ) {
    // Obtener el string desde los parámetros
    const char * texto = string_get(params[0]);

    // Mostrar el texto por consola
    if (texto) {
        printf("%s\n", texto);
        string_discard(params[0]);  // Liberar el string una vez usado
    } else {
        printf("Error: el texto no puede ser NULL.\n");
    }

    return 1;  // Devuelve un entero como valor de retorno
}

/*------------------------------------------------------------------------------*/

/* Funciones de inicialización del módulo                                      */
void  __bgdexport( libmod_texto, module_initialize )() {
    if ( !texto_initialized ) {
        texto_initialized = 1;
        printf("Módulo libmod_texto inicializado.\n");
    }
}

/* --------------------------------------------------------------------------- */

void __bgdexport( libmod_texto, module_finalize )() {
    if ( texto_initialized ) {
        texto_initialized = 0;
        printf("Módulo libmod_texto finalizado.\n");
    }
}

/* --------------------------------------------------------------------------- */

#include "libmod_texto_exports.h"

