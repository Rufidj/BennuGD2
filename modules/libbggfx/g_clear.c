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

#include "libbggfx.h"

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : gr_clear
 *
 *  Clear a bitmap (paint all pixels as 0 [transparent])
 *
 *  PARAMS :
 *      dest            Bitmap to clear
 *
 *  RETURN VALUE :
 *      None
 *
 */

void gr_clear( GRAPH * dest ) {
    if ( !dest ) return;

    if ( !dest->surface ) {
        uint32_t rmask, gmask, bmask, amask;
        getRGBA_mask( 32, &rmask, &gmask, &bmask, &amask );
        dest->surface = SDL_CreateRGBSurface(0, dest->width, dest->height, 32, rmask, gmask, bmask, amask );
        if ( !dest->surface ) return;
    }

    memset( dest->surface->pixels, '\0', dest->surface->h * dest->surface->pitch );

    if ( dest->texture ) SDL_UpdateTexture(dest->texture, NULL, dest->surface->pixels, dest->surface->pitch /*w * sizeof( uint32_t )*/ );
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : gr_clear_as
 *
 *  Clear a bitmap (paint all pixels as the given color)
 *
 *  PARAMS :
 *      dest            Bitmap to clear
 *      color           32bit color value
 *
 *  RETURN VALUE :
 *      None
 *
 */

void gr_clear_as( GRAPH * dest, int color ) {
    if ( !dest ) return;

    if ( !dest->surface ) {
        uint32_t rmask, gmask, bmask, amask;
        getRGBA_mask( 32, &rmask, &gmask, &bmask, &amask );
        dest->surface = SDL_CreateRGBSurface( 0, dest->width, dest->height, 32, rmask, gmask, bmask, amask );
        if ( !dest->surface ) return;
    }

    if ( !color ) {
        gr_clear( dest );
        return;
    }

    int elements = dest->surface->h * dest->surface->pitch / 4 ;

    uint32_t * mem = dest->surface->pixels;
    while( elements-- ) *mem++ = color ;

    if ( dest->texture ) SDL_UpdateTexture(dest->texture, NULL, dest->surface->pixels, dest->surface->pitch /*w * sizeof( uint32_t )*/ );
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : gr_clear_region_as
 *
 *  Clear a region bitmap (paint all pixels as 0 [transparent])
 *
 *  PARAMS :
 *      dest            Bitmap to clear
 *      region          Region to clear or NULL for the whole screen
 *      color           32-bit color value
 *
 *  RETURN VALUE :
 *      None
 *
 */

void gr_clear_region_as( GRAPH * dest, REGION * region, int color ) {
    int x, y, w, h;

    if ( !dest || !dest->surface ) return;

//    if ( !dest ) dest = scrbitmap ;

    if ( !region ) {
        x = 0 ;
        y = 0 ;
        w = dest->width - 1 ;
        h = dest->height - 1 ;
    } else {
        x = MAX( MIN( region->x, region->x2 ), 0 ) ;
        y = MAX( MIN( region->y, region->y2 ), 0 ) ;
        w = MIN( MAX( region->x, region->x2 ), dest->width - 1 ) - x ;
        h = MIN( MAX( region->y, region->y2 ), dest->height - 1 ) - y ;
    }

    if ( x > dest->width || region->y > dest->height ) return;
    if ( ( x + w ) < 0 || ( y + h ) < 0 ) return;

    int w2, h2, inc = dest->surface->pitch / 4;
    uint32_t * mem, * pmem = ( (uint32_t *) dest->surface->pixels ) + inc * y + x;

    for ( h2 = 0; h2 < h; h2++ ) {
        mem = pmem;
        for ( w2 = 0; w2 < w; w2++ ) {
            *mem++ = color;
        }
        pmem += inc;
    }

    if ( dest->texture ) SDL_UpdateTexture(dest->texture, NULL, dest->surface->pixels, dest->surface->pitch /*w * sizeof( uint32_t )*/ );

}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : gr_clear_region
 *
 *  Clear a region bitmap (paint all pixels as 0 [transparent])
 *
 *  PARAMS :
 *      dest            Bitmap to clear
 *      region          Region to clear or NULL for the whole screen
 *
 *  RETURN VALUE :
 *      None
 *
 */

void gr_clear_region( GRAPH * dest, REGION * region ) {
    gr_clear_region_as( dest, region, 0 );
}

/* --------------------------------------------------------------------------- */