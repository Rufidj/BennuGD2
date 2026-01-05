#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "libmod_doom.h"

// I_Error - Terminar con mensaje de error
void I_Error(const char *error, ...)
{
    char msgbuf[512];
    va_list argptr;
    
    va_start(argptr, error);
    vsnprintf(msgbuf, sizeof(msgbuf), error, argptr);
    va_end(argptr);
    
    fprintf(stderr, "\nDOOM Error: %s\n", msgbuf);
    fflush(stderr);
    
    exit(1);
}

// I_BeginRead - Marcar inicio de lectura (no-op en nuestra implementación)
void I_BeginRead(void)
{
    // No-op
}

// I_EndRead - Marcar fin de lectura (no-op en nuestra implementación)
void I_EndRead(void)
{
    // No-op
}

// I_ZoneBase - Asignar memoria para la zona
byte *I_ZoneBase(int *size)
{
    // Asignar 16 MB por defecto
    *size = 16 * 1024 * 1024;
    byte *zonemem = malloc(*size);
    
    if (zonemem == NULL)
    {
        I_Error("No se pudo asignar %d MB para la zona de memoria", *size / (1024 * 1024));
    }
    
    printf("DOOM: Zona de memoria: %d MB asignados\n", *size / (1024 * 1024));
    return zonemem;
}
