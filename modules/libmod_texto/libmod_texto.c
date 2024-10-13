#include <stdio.h>
#include <stdint.h>
#include "bgddl.h"
#include "bgdrtm.h"

int64_t mostrarTexto( INSTANCE * my, int64_t * params ) {
    char * texto = (char *)params[0];  // Convertir el parámetro a un string
    printf("%s\n", texto);
    return 1; // Puedes devolver algún valor si lo necesitas
}

int main() {
    return 0;
}

#include "libmod_texto_exports.h"

