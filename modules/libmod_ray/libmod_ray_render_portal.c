/* ============================================================================
   PORTAL RENDERING - Recursive Sector Traversal
   ============================================================================
   
   This file implements proper portal rendering with recursive sector traversal
   and horizontal frustum clipping, replacing the broken column-based approach.
   
   Based on Build Engine / Quake architecture.
   ============================================================================ */

#include "libmod_ray.h"
#include <float.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Maximum recursion depth to prevent stack overflow */
#define MAX_PORTAL_DEPTH 32

/* ============================================================================
   EXTERNAL FUNCTIONS
   ============================================================================ */

/* These functions are defined in libmod_ray_render.c and libmod_ray_geometry.c */
extern float ray_strip_screen_height(float screenDistance, float correctDistance, float height);
extern uint32_t ray_sample_texture(GRAPH *texture, int tex_x, int tex_y);
extern uint32_t ray_fog_pixel(uint32_t pixel, float distance);
extern RAY_Sector* ray_find_sector_at_point(RAY_Engine *engine, float x, float y);

/* Helper to sanitize pixel format - keep as static inline to avoid conflicts */
static inline uint32_t ray_convert_pixel(uint32_t pixel) {
    if (!gPixelFormat) return pixel;
    
    uint8_t r = (pixel >> gPixelFormat->Rshift) & 0xFF;
    uint8_t g = (pixel >> gPixelFormat->Gshift) & 0xFF;
    uint8_t b = (pixel >> gPixelFormat->Bshift) & 0xFF;
    
    return SDL_MapRGB(gPixelFormat, r, g, b);
}

/* ============================================================================
   WALL PROJECTION
   ============================================================================ */

/* Project a wall segment to screen X coordinates
 * Returns 1 if wall is visible, 0 if behind camera or outside FOV
 */
int ray_project_wall_to_screen(RAY_Wall *wall,
                                float camera_x, float camera_y, float camera_rot,
                                int *out_x1, int *out_x2,
                                float *out_dist1, float *out_dist2)
{
    extern RAY_Engine g_engine;
    
    /* Transform wall endpoints to camera space */
    float dx1 = wall->x1 - camera_x;
    float dy1 = wall->y1 - camera_y;
    float dx2 = wall->x2 - camera_x;
    float dy2 = wall->y2 - camera_y;
    
    /* Rotate to camera orientation */
    float cos_rot = cosf(camera_rot);
    float sin_rot = sinf(camera_rot);
    
    float tx1 = dx1 * cos_rot + dy1 * sin_rot;  /* Forward (Z in camera space) */
    float ty1 = -dx1 * sin_rot + dy1 * cos_rot; /* Right (X in camera space) */
    
    float tx2 = dx2 * cos_rot + dy2 * sin_rot;
    float ty2 = -dx2 * sin_rot + dy2 * cos_rot;
    
    /* Check if wall is behind camera */
    if (tx1 <= 0.1f && tx2 <= 0.1f) {
        return 0;  /* Both points behind camera */
    }
    
    /* Clip wall to near plane if needed */
    float near_plane = 0.1f;
    if (tx1 < near_plane || tx2 < near_plane) {
        /* One point behind, one in front - clip to near plane */
        if (tx1 < near_plane) {
            float t = (near_plane - tx1) / (tx2 - tx1);
            ty1 = ty1 + t * (ty2 - ty1);
            tx1 = near_plane;
        }
        if (tx2 < near_plane) {
            float t = (near_plane - tx2) / (tx1 - tx2);
            ty2 = ty2 + t * (ty1 - ty2);
            tx2 = near_plane;
        }
    }
    
    /* Project to screen X coordinates */
    /* Screen X = center + (camera_space_x / camera_space_z) * viewDist */
    int center_x = g_engine.displayWidth / 2;
    
    int x1 = center_x + (int)((ty1 / tx1) * g_engine.viewDist);
    int x2 = center_x + (int)((ty2 / tx2) * g_engine.viewDist);
    
    /* Ensure x1 < x2 (wall might be backwards) */
    if (x1 > x2) {
        int temp_x = x1; x1 = x2; x2 = temp_x;
        float temp_d = tx1; tx1 = tx2; tx2 = temp_d;
    }
    
    /* Check if wall is outside screen */
    if (x2 < 0 || x1 >= g_engine.displayWidth) {
        return 0;  /* Completely off-screen */
    }
    
    /* Return results */
    *out_x1 = x1;
    *out_x2 = x2;
    *out_dist1 = tx1;
    *out_dist2 = tx2;
    
    return 1;  /* Wall is visible */
}

/* ============================================================================
   OCCLUSION BUFFER
   ============================================================================ */

RAY_OcclusionBuffer* ray_occlusion_buffer_create(int width)
{
    RAY_OcclusionBuffer *buf = (RAY_OcclusionBuffer*)malloc(sizeof(RAY_OcclusionBuffer));
    buf->width = width;
    buf->y_top = (int*)malloc(width * sizeof(int));
    buf->y_bottom = (int*)malloc(width * sizeof(int));
    
    /* Initialize to full screen visibility */
    extern RAY_Engine g_engine;
    for (int i = 0; i < width; i++) {
        buf->y_top[i] = 0;
        buf->y_bottom[i] = g_engine.displayHeight - 1;
    }
    
    return buf;
}

void ray_occlusion_buffer_free(RAY_OcclusionBuffer *buf)
{
    if (buf) {
        free(buf->y_top);
        free(buf->y_bottom);
        free(buf);
    }
}

/* ============================================================================
   WALL SPAN RENDERING
   ============================================================================ */

/* Render a textured wall span from x1 to x2 */
void ray_render_wall_span(GRAPH *dest,
                          RAY_Wall *wall,
                          RAY_Sector *sector,
                          int x1, int x2,
                          float dist1, float dist2,
                          float *z_buffer)
{
    extern RAY_Engine g_engine;
    
    if (!wall || !sector || !dest) return;
    if (x1 >= x2) return;
    
    /* Get wall texture */
    int tex_id = wall->texture_id_middle;
    if (tex_id <= 0) return;  /* No texture */
    
    GRAPH *texture = bitmap_get(g_engine.fpg_id, tex_id);
    if (!texture) return;
    
    /* Calculate wall height in world units */
    float wall_height = sector->ceiling_z - sector->floor_z;
    
    /* Wall endpoints in world space */
    float wall_dx = wall->x2 - wall->x1;
    float wall_dy = wall->y2 - wall->y1;
    float wall_length = sqrtf(wall_dx * wall_dx + wall_dy * wall_dy);
    
    /* For each column in the span, cast a ray */
    int strip_width = g_engine.stripWidth;
    for (int screen_x = x1; screen_x <= x2 && screen_x < g_engine.displayWidth; screen_x++) {
        if (screen_x < 0) continue;
        
        /* Calculate ray angle for this column */
        int strip = screen_x / strip_width;
        if (strip >= g_engine.rayCount) continue;
        
        float ray_angle = g_engine.camera.rot + g_engine.stripAngles[strip];
        
        /* Cast ray and find intersection with this wall */
        float ray_dx = cosf(ray_angle);
        float ray_dy = -sinf(ray_angle);
        
        /* Simple ray-wall intersection */
        /* TODO: Use proper raycasting from raycasting module */
        float distance = (dist1 + dist2) / 2.0f;  /* Approximate for now */
        
        /* Calculate wall screen height */
        int wall_screen_height = (int)ray_strip_screen_height(g_engine.viewDist,
                                                               distance,
                                                               wall_height);
        
        /* Calculate vertical position */
        float player_screen_z = ray_strip_screen_height(g_engine.viewDist,
                                                        distance,
                                                        g_engine.camera.z - sector->floor_z);
        
        int wall_bottom = g_engine.displayHeight / 2 + (int)player_screen_z;
        int wall_top = wall_bottom - wall_screen_height;
        
        /* Clamp to screen */
        if (wall_top < 0) wall_top = 0;
        if (wall_bottom >= g_engine.displayHeight) wall_bottom = g_engine.displayHeight - 1;
        
        /* Calculate texture X coordinate based on position along wall */
        int tex_x = (screen_x * 2) % texture->width;
        
        /* Render vertical strip */
        for (int screen_y = wall_top; screen_y < wall_bottom && screen_y < g_engine.displayHeight; screen_y++) {
            if (screen_y < 0) continue;
            
            /* Calculate texture Y coordinate */
            float progress = (float)(screen_y - wall_top) / (float)(wall_bottom - wall_top);
            int tex_y = (int)(progress * texture->height);
            if (tex_y >= texture->height) tex_y = texture->height - 1;
            
            /* Sample texture */
            uint32_t pixel = ray_sample_texture(texture, tex_x, tex_y);
            if (pixel == 0) continue;  /* Transparent */
            
            pixel = ray_convert_pixel(pixel);
            
            /* Apply fog if enabled */
            if (g_engine.fogOn) {
                pixel = ray_fog_pixel(pixel, distance);
            }
            
            /* Z-buffer check */
            int pixel_idx = screen_y * g_engine.displayWidth + screen_x;
            if (distance < z_buffer[pixel_idx]) {
                gr_put_pixel(dest, screen_x, screen_y, pixel);
                z_buffer[pixel_idx] = distance;
            }
        }
    }
}

/* ============================================================================
   RECURSIVE SECTOR RENDERING
   ============================================================================ */

void ray_render_sector_recursive(GRAPH *dest,
                                 int sector_id,
                                 RAY_Frustum frustum,
                                 int depth,
                                 RAY_OcclusionBuffer *occlusion,
                                 float *z_buffer)
{
    extern RAY_Engine g_engine;
    
    /* Prevent infinite recursion */
    if (depth >= MAX_PORTAL_DEPTH) {
        printf("WARNING: Max portal depth reached!\n");
        return;
    }
    
    /* Validate sector */
    if (sector_id < 0 || sector_id >= g_engine.num_sectors) {
        return;
    }
    
    RAY_Sector *sector = &g_engine.sectors[sector_id];
    
    /* Debug output for first few calls */
    static int debug_count = 0;
    if (debug_count < 5) {
        printf("RENDER_SECTOR: id=%d, frustum=[%d,%d], depth=%d, walls=%d\n",
               sector_id, frustum.x_left, frustum.x_right, depth, sector->num_walls);
        debug_count++;
    }
    
    /* ========================================================================
       PHASE 2: RENDER WALLS
       ======================================================================== */
    
    /* Process each wall in the sector */
    for (int w = 0; w < sector->num_walls; w++) {
        RAY_Wall *wall = &sector->walls[w];
        
        /* Project wall to screen coordinates */
        int wall_x1, wall_x2;
        float wall_dist1, wall_dist2;
        
        if (!ray_project_wall_to_screen(wall,
                                        g_engine.camera.x,
                                        g_engine.camera.y,
                                        g_engine.camera.rot,
                                        &wall_x1, &wall_x2,
                                        &wall_dist1, &wall_dist2)) {
            continue;  /* Wall not visible */
        }
        
        /* Clip wall to inherited frustum */
        int vis_x1 = wall_x1 > frustum.x_left ? wall_x1 : frustum.x_left;
        int vis_x2 = wall_x2 < frustum.x_right ? wall_x2 : frustum.x_right;
        
        if (vis_x1 >= vis_x2) {
            continue;  /* Wall completely clipped */
        }
        
        /* Check if this is a portal */
        int is_portal = (wall->portal_id >= 0);
        
        if (is_portal) {
            /* Find neighbor sector through portal */
            int neighbor_sector_id = -1;
            for (int p = 0; p < g_engine.num_portals; p++) {
                if (g_engine.portals[p].portal_id == wall->portal_id) {
                    if (g_engine.portals[p].sector_a == sector_id) {
                        neighbor_sector_id = g_engine.portals[p].sector_b;
                    } else if (g_engine.portals[p].sector_b == sector_id) {
                        neighbor_sector_id = g_engine.portals[p].sector_a;
                    }
                    break;
                }
            }
            
            if (neighbor_sector_id >= 0 && neighbor_sector_id < g_engine.num_sectors) {
                /* TODO: Render portal upper/lower sections (step effect) */
                /* For now, just recurse into neighbor sector */
                
                RAY_Frustum portal_frustum;
                portal_frustum.x_left = vis_x1;
                portal_frustum.x_right = vis_x2;
                
                /* Recurse into neighbor sector */
                ray_render_sector_recursive(dest, neighbor_sector_id, portal_frustum,
                                           depth + 1, occlusion, z_buffer);
            }
        } else {
            /* Solid wall - render with textures */
            ray_render_wall_span(dest, wall, sector, vis_x1, vis_x2,
                                wall_dist1, wall_dist2, z_buffer);
        }
    }
    
    /* ========================================================================
       PHASE 3: RENDER FLOOR/CEILING (TODO)
       ======================================================================== */
    
    /* TODO: Render floor and ceiling spans within frustum */
}


/* ============================================================================
   MAIN RENDER ENTRY POINT
   ============================================================================ */

void ray_render_frame_portal(GRAPH *dest)
{
    extern RAY_Engine g_engine;
    
    if (!dest) return;
    
    /* Clear screen */
    gr_clear(dest);
    
    /* Initialize z-buffer */
    int screen_pixels = g_engine.displayWidth * g_engine.displayHeight;
    float *z_buffer = (float*)malloc(screen_pixels * sizeof(float));
    for (int i = 0; i < screen_pixels; i++) {
        z_buffer[i] = FLT_MAX;
    }
    
    /* Create occlusion buffer */
    RAY_OcclusionBuffer *occlusion = ray_occlusion_buffer_create(g_engine.displayWidth);
    
    /* Get camera sector */
    int camera_sector_id = g_engine.camera.current_sector_id;
    if (camera_sector_id < 0 || camera_sector_id >= g_engine.num_sectors) {
        /* Find sector at camera position */
        RAY_Sector *cam_sec = ray_find_sector_at_point(&g_engine,
                                                        g_engine.camera.x,
                                                        g_engine.camera.y);
        if (cam_sec) {
            camera_sector_id = cam_sec->sector_id;
        } else {
            camera_sector_id = 0;  /* Default to first sector */
        }
    }
    
    /* Start recursive rendering from camera sector */
    RAY_Frustum initial_frustum;
    initial_frustum.x_left = 0;
    initial_frustum.x_right = g_engine.displayWidth - 1;
    
    printf("PORTAL RENDER: Starting from sector %d\n", camera_sector_id);
    
    ray_render_sector_recursive(dest, camera_sector_id, initial_frustum, 0, occlusion, z_buffer);
    
    /* Cleanup */
    ray_occlusion_buffer_free(occlusion);
    free(z_buffer);
    
    printf("PORTAL RENDER: Complete\n");
}
