/*
 *  Copyright (C) SplinterGU (Fenix/BennuGD) (Since 2006)
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

#ifndef __RESOLUTION_H
#define __RESOLUTION_H

/* -------------------------------------------------------------------- */
/* Libreria gráfica                                                     */
/* -------------------------------------------------------------------- */
#if 0
#define RESOLXY(m,r,x,y)                                            \
    {                                                               \
        int64_t res = LOCINT64(m, r, RESOLUTION );                  \
        if ( res > 0 ) {                                            \
            if ( x < 0 )    ( x ) = ( ( x ) - ( res - 1 )) / res;   \
            else            ( x ) /= res;                           \
                                                                    \
            if ( y < 0 )    ( y ) = ( ( y ) - ( res - 1 )) / res;   \
            else            ( y ) /= res;                           \
        } else if ( res < 0 ) {                                     \
            ( x ) *= -res;                                          \
            ( y ) *= -res;                                          \
        }                                                           \
    }

#define RESOLXYZ(m,r,x,y,z) \
    {                                                               \
        int64_t res = LOCINT64(m, r, RESOLUTION );                  \
        if ( res > 0 ) {                                            \
            if ( x < 0 )    ( x ) = ( ( x ) - ( res - 1 )) / res;   \
            else            ( x ) /= res;                           \
                                                                    \
            if ( y < 0 )    ( y ) = ( ( y ) - ( res - 1 )) / res;   \
            else            ( y ) /= res;                           \
                                                                    \
            if ( z < 0 )    ( z ) = ( ( z ) - ( res - 1 )) / res;   \
            else            ( z ) /= res;                           \
        } else if ( res < 0 ) {                                     \
            ( x ) *= -res;                                          \
            ( y ) *= -res;                                          \
            ( z ) *= -res;                                          \
        }                                                           \
    }
#else
#define RESOLXY(m,r,x,y)                                            \
    {                                                               \
        int64_t res = LOCINT64(m, r, RESOLUTION );                  \
        if ( res > 0 ) {                                            \
            ( x ) /= res;                                           \
            ( y ) /= res;                                           \
        } else if ( res < 0 ) {                                     \
            ( x ) *= -res;                                          \
            ( y ) *= -res;                                          \
        }                                                           \
    }

#define RESOLXYZ(m,r,x,y,z) \
    {                                                               \
        int64_t res = LOCINT64(m, r, RESOLUTION );                  \
        if ( res > 0 ) {                                            \
            ( x ) /= res;                                           \
            ( y ) /= res;                                           \
            ( z ) /= res;                                           \
        } else if ( res < 0 ) {                                     \
            ( x ) *= -res;                                          \
            ( y ) *= -res;                                          \
            ( z ) *= -res;                                          \
        }                                                           \
    }
#endif

/* RESOLXY/RESOLXYZ above hardcode the bare "RESOLUTION" identifier, which
 * only exists when __LIBBGFGX is defined (see libbggfx.h's own guard
 * around its RESOLUTION enum member) - i.e. only inside libbggfx's own
 * .c files. A caller outside libbggfx (e.g. libmod_gfx's m_mathgfx.c/
 * m_collision.c, whose own local-resolution enum member is named
 * LOCGFX_RESOLUTION, not RESOLUTION) needs the constant name as its own
 * parameter instead. */
#define RESOLXY_NAMED(m,r,resname,x,y)                               \
    {                                                                \
        int64_t res = LOCINT64(m, r, resname );                      \
        if ( res > 0 ) {                                             \
            ( x ) /= res;                                            \
            ( y ) /= res;                                            \
        } else if ( res < 0 ) {                                      \
            ( x ) *= -res;                                           \
            ( y ) *= -res;                                           \
        }                                                            \
    }

#endif
