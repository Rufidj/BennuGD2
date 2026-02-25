/*
 * libmod_ray_decal_baking.c - Bake decals into floor/ceiling textures
 * 
 * This system composites decal textures directly into floor/ceiling textures
 * at map load time, achieving perfect perspective projection with zero runtime cost.
 */

#include "libmod_ray.h"
#include "libmod_ray_compat.h"
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

extern RAY_Engine g_engine;

/* Bake a single decal into a texture */
static void bake_decal_into_texture(GRAPH *dest_texture, RAY_Decal *decal, 
                                     float sector_width, float sector_height,
                                     float sector_min_x, float sector_min_y) {
    if (!dest_texture || !decal) return;
    
    GRAPH *decal_texture = bitmap_get(g_engine.fpg_id, decal->texture_id);
    if (!decal_texture) return;
    
    /* Ensure surfaces are available */
    bitmap_update_surface(dest_texture);
    bitmap_update_surface(decal_texture);
    
    if (!dest_texture->surface || !decal_texture->surface) return;
    
    /* Lock surfaces */
    if (SDL_MUSTLOCK(dest_texture->surface)) SDL_LockSurface(dest_texture->surface);
    if (SDL_MUSTLOCK(decal_texture->surface)) SDL_LockSurface(decal_texture->surface);
    
    uint32_t *dest_pixels = (uint32_t*)dest_texture->surface->pixels;
    uint32_t *decal_pixels = (uint32_t*)decal_texture->surface->pixels;
    
    if (!dest_pixels || !decal_pixels) goto cleanup;
    
    int dest_pitch = dest_texture->surface->pitch / 4;
    int decal_pitch = decal_texture->surface->pitch / 4;
    
    /* Calculate decal bounds in texture space */
    float decal_half_w = decal->width / 2.0f;
    float decal_half_h = decal->height / 2.0f;
    
    float decal_min_x = decal->x - decal_half_w;
    float decal_max_x = decal->x + decal_half_w;
    float decal_min_y = decal->y - decal_half_h;
    float decal_max_y = decal->y + decal_half_h;
    
    /* Map to texture coordinates (0-1) */
    float u_min = (decal_min_x - sector_min_x) / sector_width;
    float u_max = (decal_max_x - sector_min_x) / sector_width;
    float v_min = (decal_min_y - sector_min_y) / sector_height;
    float v_max = (decal_max_y - sector_min_y) / sector_height;
    
    /* Clamp to texture bounds */
    if (u_min < 0.0f) u_min = 0.0f;
    if (u_max > 1.0f) u_max = 1.0f;
    if (v_min < 0.0f) v_min = 0.0f;
    if (v_max > 1.0f) v_max = 1.0f;
    
    /* Convert to pixel coordinates */
    int tex_x_start = (int)(u_min * dest_texture->width);
    int tex_x_end = (int)(u_max * dest_texture->width);
    int tex_y_start = (int)(v_min * dest_texture->height);
    int tex_y_end = (int)(v_max * dest_texture->height);
    
    /* Composite decal onto texture */
    for (int ty = tex_y_start; ty < tex_y_end && ty < dest_texture->height; ty++) {
        for (int tx = tex_x_start; tx < tex_x_end && tx < dest_texture->width; tx++) {
            /* Calculate UV in decal space */
            float u = (float)(tx - tex_x_start) / (float)(tex_x_end - tex_x_start);
            float v = (float)(ty - tex_y_start) / (float)(tex_y_end - tex_y_start);
            
            /* Apply rotation if needed */
            if (fabsf(decal->rotation) > 0.001f) {
                float cos_r = cosf(-decal->rotation);
                float sin_r = sinf(-decal->rotation);
                float cu = u - 0.5f;
                float cv = v - 0.5f;
                u = cu * cos_r - cv * sin_r + 0.5f;
                v = cu * sin_r + cv * cos_r + 0.5f;
            }
            
            /* Bounds check */
            if (u < 0.0f || u >= 1.0f || v < 0.0f || v >= 1.0f) continue;
            
            /* Sample decal texture */
            int decal_x = (int)(u * decal_texture->width);
            int decal_y = (int)(v * decal_texture->height);
            
            if (decal_x < 0 || decal_x >= decal_texture->width ||
                decal_y < 0 || decal_y >= decal_texture->height) continue;
            
            uint32_t decal_color = decal_pixels[decal_y * decal_pitch + decal_x];
            
            /* Check alpha */
            uint8_t alpha = (decal_color >> 24) & 0xFF;
            if (alpha < 10) continue;
            
            /* Alpha blend onto destination */
            uint32_t *dest_pixel = &dest_pixels[ty * dest_pitch + tx];
            
            if (alpha == 255 && decal->alpha >= 0.99f) {
                /* Opaque - direct copy */
                *dest_pixel = decal_color;
            } else {
                /* Alpha blend */
                uint8_t dr = (*dest_pixel >> 16) & 0xFF;
                uint8_t dg = (*dest_pixel >> 8) & 0xFF;
                uint8_t db = *dest_pixel & 0xFF;
                
                uint8_t sr = (decal_color >> 16) & 0xFF;
                uint8_t sg = (decal_color >> 8) & 0xFF;
                uint8_t sb = decal_color & 0xFF;
                
                float a = decal->alpha * (alpha / 255.0f);
                
                uint8_t fr = (uint8_t)(sr * a + dr * (1.0f - a));
                uint8_t fg = (uint8_t)(sg * a + dg * (1.0f - a));
                uint8_t fb = (uint8_t)(sb * a + db * (1.0f - a));
                
                *dest_pixel = 0xFF000000 | (fr << 16) | (fg << 8) | fb;
            }
        }
    }
    
cleanup:
    if (SDL_MUSTLOCK(dest_texture->surface)) SDL_UnlockSurface(dest_texture->surface);
    if (SDL_MUSTLOCK(decal_texture->surface)) SDL_UnlockSurface(decal_texture->surface);
    
    dest_texture->dirty = 1;
}

/* Bake all decals for a sector into its floor/ceiling textures */
static void bake_sector_decals(RAY_Sector *sector) {
    if (!sector) return;
    
    /* Calculate sector bounds */
    if (sector->num_vertices < 3) return;
    
    float min_x = sector->vertices[0].x;
    float max_x = sector->vertices[0].x;
    float min_y = sector->vertices[0].y;
    float max_y = sector->vertices[0].y;
    
    for (int i = 1; i < sector->num_vertices; i++) {
        if (sector->vertices[i].x < min_x) min_x = sector->vertices[i].x;
        if (sector->vertices[i].x > max_x) max_x = sector->vertices[i].x;
        if (sector->vertices[i].y < min_y) min_y = sector->vertices[i].y;
        if (sector->vertices[i].y > max_y) max_y = sector->vertices[i].y;
    }
    
    float sector_width = max_x - min_x;
    float sector_height = max_y - min_y;
    
    if (sector_width < 0.1f || sector_height < 0.1f) return;
    
    /* Bake decals directly into the original textures */
    for (int i = 0; i < g_engine.num_decals; i++) {
        RAY_Decal *decal = &g_engine.decals[i];
        if (decal->sector_id != sector->sector_id) continue;
        
        /* Get the target texture (floor or ceiling) */
        int texture_id = decal->is_floor ? sector->floor_texture_id : sector->ceiling_texture_id;
        if (texture_id <= 0) continue;
        
        GRAPH *target = bitmap_get(g_engine.fpg_id, texture_id);
        if (!target) continue;
        
        bake_decal_into_texture(target, decal, sector_width, sector_height, min_x, min_y);
    }
}

/* Main function: Bake all decals into floor/ceiling textures */
void ray_bake_decals(void) {
    if (g_engine.num_decals == 0) return;
    
    printf("RAY: Baking %d decals into floor/ceiling textures...\n", g_engine.num_decals);
    
    for (int i = 0; i < g_engine.num_sectors; i++) {
        bake_sector_decals(&g_engine.sectors[i]);
    }
    
    printf("RAY: Decal baking complete.\n");
}
