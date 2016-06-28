/*
 *  Copyright (C) 2006-2016 SplinterGU (Fenix/BennuGD)
 *  Copyright (C) 2002-2006 Fenix Team (Fenix)
 *  Copyright (C) 1999-2002 José Luis Cebrián Pagüe (Fenix)
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
 *     2. Altered source versions must be plainly marked as such, and must not be
 *     misrepresented as being the original software.
 *
 *     3. This notice may not be removed or altered from any source
 *     distribution.
 *
 */

/* --------------------------------------------------------------------------- */

#ifndef __M_TEXT_H
#define __M_TEXT_H

#include "bgdrtm.h"
#include "bgddl.h"

extern int64_t libmod_gfx_text_write( INSTANCE * my, int64_t * params );
extern int64_t libmod_gfx_text_write2( INSTANCE * my, int64_t * params );
extern int64_t libmod_gfx_text_write_in_map( INSTANCE * my, int64_t * params );
extern int64_t libmod_gfx_text_write_var( INSTANCE * my, int64_t * params );
extern int64_t libmod_gfx_text_write_var2( INSTANCE * my, int64_t * params );
extern int64_t libmod_gfx_text_write_string( INSTANCE * my, int64_t * params );
extern int64_t libmod_gfx_text_write_string2( INSTANCE * my, int64_t * params );
extern int64_t libmod_gfx_text_write_int( INSTANCE * my, int64_t * params );
extern int64_t libmod_gfx_text_write_int2( INSTANCE * my, int64_t * params );
extern int64_t libmod_gfx_text_write_float( INSTANCE * my, int64_t * params );
extern int64_t libmod_gfx_text_write_float2( INSTANCE * my, int64_t * params );
extern int64_t libmod_gfx_text_move( INSTANCE * my, int64_t * params );
extern int64_t libmod_gfx_text_move2( INSTANCE * my, int64_t * params );
extern int64_t libmod_gfx_text_delete( INSTANCE * my, int64_t * params );
extern int64_t libmod_gfx_text_height( INSTANCE * my, int64_t * params );
extern int64_t libmod_gfx_text_width( INSTANCE * my, int64_t * params );
extern int64_t libmod_gfx_text_set_color( INSTANCE * my, int64_t * params );
extern int64_t libmod_gfx_text_get_color( INSTANCE * my, int64_t * params );
extern int64_t libmod_gfx_text_set_alpha( INSTANCE * my, int64_t * params );
extern int64_t libmod_gfx_text_get_alpha( INSTANCE * my, int64_t * params );

#endif