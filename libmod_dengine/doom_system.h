//
// Doom System Header
//

#ifndef __DOOM_SYSTEM_H
#define __DOOM_SYSTEM_H

#include "libmod_doom.h"

// Funciones de error y sistema
void I_Error(const char *error, ...);
void I_BeginRead(void);
void I_EndRead(void);
byte *I_ZoneBase(int *size);

#endif
