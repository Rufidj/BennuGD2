//
// Doom Rendering System - Drawing Functions
// Adaptado de r_draw.c
//

#include "libmod_doom.h"
#include "doom_render.h"
#include <SDL2/SDL.h>

// Globales necesarias para el dibujado
// En el futuro usaremos las variables dc_* originales
// extern byte* ylookup[MAXHEIGHT];
// extern int dc_x;
// extern int dc_yl;
// extern int dc_yh;
// extern byte* dc_source;

extern SDL_PixelFormat * gPixelFormat;

// ============================================================================
// R_DrawColumn
// Dibuja una columna vertical de color sólido (Debug/Wireframe implementation)
// ============================================================================
void R_DrawColumn(int x, int y1, int y2, int color)
{
    if (!doom_render_buffer) return;
    
    // Clipping básico
    if (x < 0 || x >= doom_render_buffer->width) return;
    if (y1 < 0) y1 = 0;
    if (y2 >= doom_render_buffer->height) y2 = doom_render_buffer->height - 1;
    if (y1 > y2) return;

    // TODO: Optimizar con acceso directo al buffer si es posible
    // Uint32* pixels = (Uint32*)doom_render_buffer->data;
    
    // Convertir color (asumiendo 0xFFFFFFFF format o similar)
    // Para simplificar, pasamos color como int directo 0xAARRGGBB
    // Pero gPixelFormat puede ser distinto.
    // Asumiremos que 'color' ya viene formateado o es un valor simple.
    // En R_StoreWallRange pasamos 0xFFFFFFFF (Blanco).
    
    // Mapear color blanco para prueba
    uint32_t final_color;
    if (color == 0xFFFFFFFF)
        final_color = SDL_MapRGBA(gPixelFormat, 255, 255, 255, 255);
    else
        final_color = color;

    for (int y = y1; y <= y2; y++)
    {
        gr_put_pixel(doom_render_buffer, x, y, final_color);
    }
}
