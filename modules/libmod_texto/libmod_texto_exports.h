/*
 *  Copyright (C) Tu Nombre (Tu Proyecto)
 *
 *  This file is part of Bennu Game Development
 *
 *  This software is provided 'as-is', without any express or implied
 *  warranty. In no event will the authors be held liable for any damages
 *  arising from the use of this software.
 *
 *  Permission is granted to anyone to use this software for any purpose,
 *  including commercial applications, and to alter it and redistribute it
 *  freely, subject to the following restrictions:
 *
 *     1. The origin of this software must not be misrepresented; you must not
 *     claim that you wrote the original software. If you use this software
 *     in a product, an acknowledgment in the product documentation would be
 *     appreciated but is not required.
 *
 *     2. Altered source versions must be plainly marked as such, and must not
 *     be misrepresented as being the original software.
 *
 *     3. This notice may not be removed or altered from any source
 *     distribution.
 *
 */

#ifndef __LIBMOD_TEXTO_EXPORTS
#define __LIBMOD_TEXTO_EXPORTS

/* ----------------------------------------------------------------- */

#include "bgddl.h"

/* ----------------------------------------------------------------- */

// Exportar las funciones
DLSYSFUNCS  __bgdexport(libmod_Texto, functions_exports)[] = {
    FUNC("mostrarTexto", "S", TYPE_INT, mostrarTexto), // Cambia el tipo de retorno y los parámetros según sea necesario
    FUNC(0, 0, 0, 0) // Fin de la lista de funciones
};

/* ----------------------------------------------------------------- */

char* __bgdexport(libmod_Texto, modules_dependency)[] = {
    NULL // Aquí puedes agregar dependencias de otros módulos si es necesario
};

/* ----------------------------------------------------------------- */

#endif

