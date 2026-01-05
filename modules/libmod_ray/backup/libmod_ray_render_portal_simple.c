/* ============================================================================
   SIMPLIFIED PORTAL RENDERER - Column-based approach with frustum
   ============================================================================
   
   Strategy: Use column-by-column raycasting like the old renderer,
   but with frustum clipping for portal recursion.
   ============================================================================ */

#include "libmod_ray.h"
#include <float.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_PORTAL_DEPTH 16

/* External raycasting function */
extern int ray_cast_all_in_sector(RAY_Engine *engine, float ray_x, float ray_y, float ray_angle,
                                   int current_sector_id, RAY_RayHit *hits, int max_hits);

/* External rendering functions */
extern void ray_draw_wall_strip(GRAPH *dest, RAY_RayHit *rayHit, int screen_x,
                                int *ceiling_clip, int *floor_clip);
extern void ray_draw_floor_ceiling(GRAPH *dest, int screen_x, float ray_angle,
                                   int sector_id, float min_distance, float max_distance,
                                   float *z_buffer, int *ceiling_clip, int *floor_clip);

/* Simple portal renderer using column-based approach */
void ray_render_frame_portal_simple(GRAPH *dest)
{
    extern RAY_Engine g_engine;
    
    if (!dest) return;
    
    /* Clear screen */
    gr_clear(dest);
    
    /* Get camera sector */
    int camera_sector_id = g_engine.camera.current_sector_id;
    if (camera_sector_id < 0 || camera_sector_id >= g_engine.num_sectors) {
        camera_sector_id = 0;
    }
    
    /* Allocate clipping arrays */
    int *ceiling_clip = (int*)malloc(g_engine.rayCount * sizeof(int));
    int *floor_clip = (int*)malloc(g_engine.rayCount * sizeof(int));
    float *z_buffer = (float*)malloc(g_engine.displayWidth * g_engine.displayHeight * sizeof(float));
    
    /* Initialize */
    for (int i = 0; i < g_engine.rayCount; i++) {
        ceiling_clip[i] = g_engine.displayHeight;
        floor_clip[i] = 0;
    }
    for (int i = 0; i < g_engine.displayWidth * g_engine.displayHeight; i++) {
        z_buffer[i] = FLT_MAX;
    }
    
    /* Render each column */
    int strip_width = g_engine.stripWidth;
    for (int strip = 0; strip < g_engine.rayCount; strip++) {
        int screen_x = strip * strip_width;
        float ray_angle = g_engine.camera.rot + g_engine.stripAngles[strip];
        
        /* Cast ray */
        RAY_RayHit hits[64];
        int num_hits = ray_cast_all_in_sector(&g_engine,
                                              g_engine.camera.x,
                                              g_engine.camera.y,
                                              ray_angle,
                                              camera_sector_id,
                                              hits, 64);
        
        /* Render walls */
        for (int h = 0; h < num_hits; h++) {
            if (hits[h].wall) {
                ray_draw_wall_strip(dest, &hits[h], screen_x, ceiling_clip, floor_clip);
            }
        }
        
        /* Render floor/ceiling */
        float max_dist = (num_hits > 0) ? hits[0].distance : 1000.0f;
        ray_draw_floor_ceiling(dest, screen_x, ray_angle, camera_sector_id,
                              0.0f, max_dist, z_buffer, ceiling_clip, floor_clip);
    }
    
    /* Cleanup */
    free(ceiling_clip);
    free(floor_clip);
    free(z_buffer);
}
