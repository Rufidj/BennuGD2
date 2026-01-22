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

#include <stdio.h>

#include "libbggfx.h"

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : gr_get_pixel
 *
 *  Read a pixel from a bitmap
 *
 *  PARAMS :
 *      dest            Destination bitmap
 *      x, y            Pixel coordinates
 *
 *  RETURN VALUE :
 *      1, 8 or 16-bit integer with the pixel value
 *
 */
#if 0
int64_t gr_get_pixel( GRAPH * gr, int64_t x, int64_t y ) {
    if ( !gr ) return -1;

    if ( x < 0 || y < 0 ) return -1;

    if ( gr_create_image_for_graph( gr ) ) return -1;

    if ( !gr->tex ) return -1;

#ifdef USE_SDL2
    if ( x >= ( int64_t ) gr->width || y >= ( int64_t ) gr->height ) return -1;
#endif
#ifdef USE_SDL2_GPU
    if ( x >= ( int64_t ) gr->tex->w || y >= ( int64_t ) gr->tex->h ) return -1;
#endif

#ifdef USE_SDL2
    SDL_Texture* auxTexture = SDL_CreateTexture( gRenderer, gPixelFormat->format, SDL_TEXTUREACCESS_TARGET, 1, 1 );
    SDL_SetRenderTarget( gRenderer, auxTexture );

    SDL_SetRenderDrawColor( gRenderer, 0, 0, 0, 0 );
    SDL_RenderClear( gRenderer );

    SDL_SetTextureBlendMode( auxTexture, SDL_BLENDMODE_NONE );

    SDL_Rect sourceRect = { x, y, 1, 1 };
    SDL_RenderCopy(gRenderer, gr->tex, &sourceRect, NULL);

    Uint32 pixelColor;

    SDL_RenderReadPixels( gRenderer, NULL, gPixelFormat->format, &pixelColor, sizeof( Uint32 ) );

    SDL_DestroyTexture( auxTexture );

    SDL_SetRenderTarget( gRenderer, NULL );

    return pixelColor;
#endif
#ifdef USE_SDL2_GPU
    if ( !gr->tex->target ) GPU_LoadTarget( gr->tex );
    SDL_Color c = GPU_GetPixel( gr->tex->target, ( Sint16 ) x, ( Sint16 ) y );
    return SDL_MapRGBA( gPixelFormat, c.r, c.g, c.b, c.a );
#endif
}
#else

int64_t gr_get_pixel( GRAPH * gr, int64_t x, int64_t y ) {

    if ( !gr ) return -1;

    if ( x < 0 || y < 0 ) return -1;

    if ( gr_create_image_for_graph( gr ) ) return -1;

    if ( !gr->tex ) return -1;

    bitmap_update_surface( gr );

    if ( !gr->surface ) return -1;

    if ( x >= ( int64_t ) gr->surface->w || y >= ( int64_t ) gr->surface->h ) return -1;

    int bpp = gr->surface->format->BytesPerPixel;
    uint8_t *p = (uint8_t *)gr->surface->pixels + y * gr->surface->pitch + x * bpp;
    uint32_t pixel = 0;

    switch(bpp) {
        case 1:
            pixel = *p;
            break;
        case 2:
            pixel = *(uint16_t *)p;
            break;
        case 3:
            if(SDL_BYTEORDER == SDL_BIG_ENDIAN)
                pixel = p[0] << 16 | p[1] << 8 | p[2];
            else
                pixel = p[0] | p[1] << 8 | p[2] << 16;
            break;
        case 4:
            pixel = *(uint32_t *)p;
            break;
        default:
            return 0;
    }

    /* Convert to global format if needed (e.g. 8-bit palettized to 32-bit RGBA) */
    if ( gr->surface->format->format != gPixelFormat->format ) {
        Uint8 r, g, b, a;
        SDL_GetRGBA( pixel, gr->surface->format, &r, &g, &b, &a );
        return (int32_t)SDL_MapRGBA( gPixelFormat, r, g, b, a );
    }

    return (int32_t)pixel;
}
#endif

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : gr_put_pixel
 *
 *  Paint a pixel with no clipping whatsoever, except by
 *  the bitmap's dimensions
 *
 *  PARAMS :
 *      dest            Destination bitmap
 *      x, y            Pixel coordinates
 *      color           1, 8 or 16-bit pixel value
 *
 *  RETURN VALUE :
 *      None
 *
 */

void gr_put_pixel( GRAPH * gr, int64_t x, int64_t y, int64_t color ) {

    if ( !gr ) return;

    if ( x < 0 || y < 0 ) return;

    if ( gr_create_image_for_graph( gr ) ) return;

#ifdef USE_SDL2
    if ( x >= ( int64_t ) gr->width || y >= ( int64_t ) gr->height ) return;
#endif
#ifdef USE_SDL2_GPU
    if ( x >= ( int64_t ) gr->tex->w || y >= ( int64_t ) gr->tex->h ) return;
#endif

#ifdef USE_SDL2
    SDL_Color c;
    SDL_SetRenderTarget( gRenderer, gr->tex );
    SDL_GetRGBA( color, gPixelFormat, &c.r, &c.g, &c.b, &c.a ) ;
    SDL_SetRenderDrawColor( gRenderer, c.r, c.g, c.b, c.a );
    SDL_Rect rect = { (int)x, (int)y, 1, 1 };
    SDL_RenderFillRect( gRenderer, &rect );
    SDL_SetRenderTarget( gRenderer, NULL );
#endif
#ifdef USE_SDL2_GPU
    SDL_Color c;
    SDL_GetRGBA( color, gPixelFormat, &c.r, &c.g, &c.b, &c.a ) ;
    GPU_Pixel( gr->tex->target, ( float ) x, ( float ) y, c );

    gr->dirty = 1;
#endif
}

/* --------------------------------------------------------------------------- */
