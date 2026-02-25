#if 0
/*
 * libmod_ray_render_decals.c - Decal rendering for floors/ceilings
 * Simplified implementation for BennuGD2
 */

#include "libmod_ray.h"
#include <math.h>

// Use direct gr_put_pixel for reliability
#define FAST_PUT_PIXEL(g, x, y, c) gr_put_pixel(g, x, y, c)

extern RAY_Engine g_engine;
extern float *g_zbuffer;
extern int halfxdimen, halfydimen;

/* Render a single decal on floor or ceiling */
void render_decal(GRAPH *dest, RAY_Decal *decal, RAY_Sector *sector) {
    if (!decal || !sector) return;
    
    // TEST: Draw a green line at y=100 just to prove we can draw here
    if (decal->id == 0) {
        for(int i=0; i<100; i++) FAST_PUT_PIXEL(dest, i, 100, 0xFF00FF00);
    }
    
    // Debug print (once per decal ID)
    static int logged_decals[100] = {0};
    if (decal->id >= 0 && decal->id < 100 && !logged_decals[decal->id]) {
        printf("RAY: Attempting to render decal %d in sector %d (floor=%d). Height diff: %.2f\n", 
               decal->id, decal->sector_id, decal->is_floor, 
               (decal->is_floor ? sector->floor_z : sector->ceiling_z) - g_engine.camera.z);
        logged_decals[decal->id] = 1;
    }

    /* Get texture */
    GRAPH *texture = bitmap_get(g_engine.fpg_id, decal->texture_id);
    if (!texture) {
        if (decal->id >= 0 && decal->id < 100 && logged_decals[decal->id] == 1) {
            printf("RAY: Decal %d skipped - texture %d not found\n", decal->id, decal->texture_id);
            logged_decals[decal->id] = 2;
        }
        return;
    }
    
    /* Camera position */
    float cam_x = g_engine.camera.x;
    float cam_y = g_engine.camera.y;
    float cam_z = g_engine.camera.z;
    float cam_rot = g_engine.camera.rot;
    float cam_pitch = g_engine.camera.pitch;
    
    /* Decal height (floor or ceiling) */
    float decal_z = decal->is_floor ? sector->floor_z : sector->ceiling_z;
    float height_diff = decal_z - cam_z;
    
    /* Skip if too close to camera height to avoid division by zero or huge projections */
    if (fabsf(height_diff) < 1.0f) return;
    
    /* Decal bounds */
    float half_w = decal->width / 2.0f;
    float half_h = decal->height / 2.0f;
    float min_x = decal->x - half_w;
    float max_x = decal->x + half_w;
    float min_y = decal->y - half_h;
    float max_y = decal->y + half_h;
    
    /* Precalc projection constants */
    // Ensure dimensions are initialized
    if (halfxdimen == 0) halfxdimen = g_engine.displayWidth / 2;
    if (halfydimen == 0) halfydimen = g_engine.displayHeight / 2;
    
    float view_dist = (float)halfxdimen;
    int horizon = halfydimen + (int)cam_pitch;
    
    /* Iterate over screen columns */
    for (int x = 0; x < g_engine.displayWidth; x++) {
        /* Ray direction */
        float ray_angle = cam_rot + g_engine.stripAngles[x];
        float ray_dx = cosf(ray_angle);
        float ray_dy = sinf(ray_angle);
        
        /* Ray vs AABB Intersection (Slab method) */
        float t_min = 0.0f;
        float t_max = 100000.0f;
        
        /* Check X slabs */
        if (fabsf(ray_dx) < 0.00001f) {
            if (cam_x < min_x || cam_x > max_x) continue; // Parallel and outside
        } else {
            float inv_dx = 1.0f / ray_dx;
            float t1 = (min_x - cam_x) * inv_dx;
            float t2 = (max_x - cam_x) * inv_dx;
            if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
            if (t1 > t_min) t_min = t1;
            if (t2 < t_max) t_max = t2;
            if (t_min > t_max) continue;
        }
        
        /* Check Y slabs */
        if (fabsf(ray_dy) < 0.00001f) {
            if (cam_y < min_y || cam_y > max_y) continue; // Parallel and outside
        } else {
            float inv_dy = 1.0f / ray_dy;
            float t1 = (min_y - cam_y) * inv_dy;
            float t2 = (max_y - cam_y) * inv_dy;
            if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
            if (t1 > t_min) t_min = t1;
            if (t2 < t_max) t_max = t2;
            if (t_min > t_max) continue;
        }
        
        /* Check if intersection is behind camera */
        if (t_max < 0.1f) continue;
        if (t_min < 0.1f) t_min = 0.1f;
        
        static int debug_hits = 0;
        if (debug_hits < 5) {
             printf("RAY DEBUG: Decal %d intersected at x=%d t_min=%.2f t_max=%.2f\n", decal->id, x, t_min, t_max);
             debug_hits++;
        }
        
        /* Project t_min and t_max to screen Y */
        /* formula: screen_y = horizon + (height_diff * view_dist) / (distance * cos_fisheye) */
        /* Note: g_engine.stripAngles[x] is the angle relative to center, used for fisheye correction */
        float fisheye_correction = cosf(g_engine.stripAngles[x]);
        
        // Calculate Y projected (perspective division)
        // t is Euclidean distance, we need z-distance for projection which is t * fisheye_correction?
        // Actually for floor casting: z_depth = t * cos(angle_offset)
        // y = horizon + (h * d) / z_depth
        
        float z_min = t_min * fisheye_correction;
        float z_max = t_max * fisheye_correction;
        
        int y1 = horizon + (int)((height_diff * view_dist) / z_min);
        int y2 = horizon + (int)((height_diff * view_dist) / z_max);
        
        // Sort y1, y2
        int y_start, y_end;
        if (y1 < y2) { y_start = y1; y_end = y2; } else { y_start = y2; y_end = y1; }
        
        // Clamp to screen
        if (y_start < 0) y_start = 0;
        if (y_end >= g_engine.displayHeight) y_end = g_engine.displayHeight - 1;
        
        // Draw vertical strip
        static int debug_cols = 0;
        if (debug_cols < 3 && t_max < 1000) { // Only debug a valid finite intersection
             printf("RAY DEBUG: Column x=%d y_range=[%d, %d] z_range=[%.2f, %.2f]\n", 
                    x, y_start, y_end, z_min, z_max);
             debug_cols++;
        }

        for (int y = y_start; y <= y_end; y++) {
            // Reverse projection to get exact world pos for this pixel
            // This is needed for texture coordinates
            if (y == horizon) continue;
            
            // Calculate distance (must be positive)
            float dist = fabsf(height_diff * view_dist) / fabsf((float)(y - horizon));
            dist /= fisheye_correction; // Convert back to euclidean
            
            float world_x = cam_x + ray_dx * dist;
            float world_y = cam_y + ray_dy * dist;
            
            // Texture coords
            float u = (world_x - min_x) / decal->width;
            float v = (world_y - min_y) / decal->height;
            
            // Explicit bounds check (due to float precision in reverse proj)
            if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) continue;
            
            int tex_x = (int)(u * texture->width);
            int tex_y = (int)(v * texture->height);
            
            // Clamp texture coords
            if (tex_x >= texture->width) tex_x = texture->width - 1;
            if (tex_y >= texture->height) tex_y = texture->height - 1;
            if (tex_x < 0) tex_x = 0;
            if (tex_y < 0) tex_y = 0;
            
            // Z-buffer check
            int buffer_idx = y * g_engine.displayWidth + x;
            // DISABLED Z-BUFFER CHECK FOR DEBUGGING
            // if (g_zbuffer[buffer_idx] < dist - 1.0f) { 
            //     continue; 
            // }
            
            /* FORCE RED DEBUG */
            uint32_t tex_color = 0xFFFF0000; // Red opaque
            
            static int debug_draw = 0;
            if (debug_draw < 10) {
                 printf("RAY DEBUG: Draw pixel (%d,%d) color=%08x tex=(%d,%d) alpha=%.2f\n", x, y, tex_color, tex_x, tex_y, decal->alpha);
                 debug_draw++;
            }
            
            FAST_PUT_PIXEL(dest, x, y, tex_color);
            
             /* Only update Z-buffer if opaque */
            if (decal->alpha >= 0.99f) {
                // We should probably NOT update z-buffer for floor decals to allow stacking?
                // Or maybe yes, to occlude things below?
                // For now, let's NOT update it to behave like a true overlay
                // g_zbuffer[buffer_idx] = dist; 
            }
        }
    }
}

/* Render all decals for a given sector */
void render_sector_decals(GRAPH *dest, int sector_id) {
    if (sector_id < 0 || sector_id >= g_engine.num_sectors) return;
    
    RAY_Sector *sector = &g_engine.sectors[sector_id];
    
    /* Collect decals for this sector */
    int decal_count = 0;
    RAY_Decal *sector_decals[100];  /* Max 100 decals per sector */
    
    for (int d = 0; d < g_engine.num_decals; d++) {
        if (g_engine.decals[d].sector_id == sector_id) {
            if (decal_count < 100) {
                sector_decals[decal_count++] = &g_engine.decals[d];
            }
        }
    }
    
    /* Sort by render_order (bubble sort) */
    for (int i = 0; i < decal_count - 1; i++) {
        for (int j = 0; j < decal_count - i - 1; j++) {
            if (sector_decals[j]->render_order > sector_decals[j + 1]->render_order) {
                RAY_Decal *temp = sector_decals[j];
                sector_decals[j] = sector_decals[j + 1];
                sector_decals[j + 1] = temp;
            }
        }
    }
    
    /* Render each decal */
    for (int i = 0; i < decal_count; i++) {
        render_decal(dest, sector_decals[i], sector);
    }
}
#endif
