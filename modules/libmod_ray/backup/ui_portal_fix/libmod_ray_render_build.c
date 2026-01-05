/*
 * libmod_ray_render_build.c - Build Engine Renderer Port from EDuke32
 * 
 * This is a direct port of the Build Engine renderer from EDuke32
 * adapted to work with BennuGD2 and .raymap format.
 */

#include "libmod_ray.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

extern RAY_Engine g_engine;
extern uint32_t ray_sample_texture(GRAPH *texture, int tex_x, int tex_y);
extern uint32_t ray_fog_pixel(uint32_t pixel, float distance);

/* ============================================================================
   BUILD ENGINE CONSTANTS AND GLOBALS
   ============================================================================ */

#define MAXWALLS 8192
#define MAXSCREENWIDTH 2048

// Projected wall coordinates (from EDuke32)
static int32_t xb1[MAXWALLS];  // Screen X start for each wall
static int32_t xb2[MAXWALLS];  // Screen X end for each wall
static int32_t yb1[MAXWALLS];  // Depth at start
static int32_t yb2[MAXWALLS];  // Depth at end

// Clipping arrays (from EDuke32)
static int16_t umost[MAXSCREENWIDTH];  // Upper clipping boundary
static int16_t dmost[MAXSCREENWIDTH];  // Lower clipping boundary
static int16_t uplc[MAXSCREENWIDTH];   // Upper portal clip
static int16_t dplc[MAXSCREENWIDTH];   // Lower portal clip
static int16_t uwall[MAXSCREENWIDTH];  // Upper wall boundary
static int16_t dwall[MAXSCREENWIDTH];  // Lower wall boundary

// Texture mapping arrays
static int32_t swall[MAXSCREENWIDTH];  // Wall scale
static int32_t lwall[MAXSCREENWIDTH];  // Wall texture coordinate

// Screen dimensions
static int32_t xdimen, ydimen;
static int32_t halfxdimen, halfydimen;
static int32_t viewingrange;

// Framebuffer
static uint8_t *frameplace;
static int32_t ylookup[MAXSCREENWIDTH];

/* ============================================================================
   COORDINATE TRANSFORMS
   ============================================================================ */

typedef struct {
    float x, y;
} vec2_t;

// Transform world coordinates to camera space
static vec2_t transform_to_camera(float world_x, float world_y)
{
    float dx = world_x - g_engine.camera.x;
    float dy = world_y - g_engine.camera.y;
    
    float cos_rot = cosf(g_engine.camera.rot);
    float sin_rot = sinf(g_engine.camera.rot);
    
    vec2_t result;
    result.x = (dx * cos_rot + dy * sin_rot);   // Forward (depth)
    result.y = (-dx * sin_rot + dy * cos_rot);  // Right (lateral)
    
    return result;
}

// Simplified get_screen_coords with proper near-plane clipping
// Outputs: sx (SCREEN X - int), z (DEPTH - float), u_factor (0..1 interpolation factor for texture correction)
static int get_screen_coords(vec2_t p1, vec2_t p2,
                             int32_t *sx1ptr, float *z1ptr, float *u1_factor,
                             int32_t *sx2ptr, float *z2ptr, float *u2_factor)
{
    const float NEAR_Z = 1.0f; // Minimum depth

    // If both points are behind, reject
    if (p1.x < NEAR_Z && p2.x < NEAR_Z) return 0;

    // Track interpolation factors (start at 0.0 and 1.0)
    float t1 = 0.0f;
    float t2 = 1.0f;

    // Clip against Near Plane
    vec2_t cp1 = p1;
    vec2_t cp2 = p2;

    if (p1.x < NEAR_Z) {
        float t = (NEAR_Z - p1.x) / (p2.x - p1.x);
        cp1.x = NEAR_Z;
        cp1.y = p1.y + t * (p2.y - p1.y);
        t1 = t; // New start is at t
    }
    
    if (p2.x < NEAR_Z) {
        float t = (NEAR_Z - p2.x) / (p1.x - p2.x);
        cp2.x = NEAR_Z;
        cp2.y = p2.y + t * (p1.y - p2.y);
        // t is from p2 towards p1? No, formula above:
        // P = P1 + t(P2-P1).
        // If P2 < NEAR_Z, we are clipping the END.
        // So we want the intersection point t.
        // Yes, t is the factor from P1.
        t2 = t; // New end is at t
    }

    // Output Interpolation factors for U correction
    *u1_factor = t1;
    *u2_factor = t2;

    // Project to screen
    // sx = half_width + (y * half_width / x)
    
    // float calculations for precision
    // Use slightly larger FOV factor? 
    // Build uses xdimen * .8 roughly? halfxdimen is roughly 90 deg horizontal?
    // halfxdimen (320 scale) corresponds to 90 deg.
    
    *sx1ptr = halfxdimen + (int32_t)((cp1.y * (float)halfxdimen) / cp1.x);
    *z1ptr = cp1.x; 
    
    *sx2ptr = halfxdimen + (int32_t)((cp2.y * (float)halfxdimen) / cp2.x);
    *z2ptr = cp2.x;

    // Handle swapped order (backface or crossed wall)
    // If sx1 > sx2, we swap everything to ensure left-to-right drawing
    if (*sx1ptr > *sx2ptr) {
        int32_t tmp = *sx1ptr; *sx1ptr = *sx2ptr; *sx2ptr = tmp;
        float tmp_z = *z1ptr; *z1ptr = *z2ptr; *z2ptr = tmp_z;
        float tmp_u = *u1_factor; *u1_factor = *u2_factor; *u2_factor = tmp_u;
        return 1; 
    }
    
    return 1;
}

// Simplified wallmost - calculates wall heights for each screen column
// Based on EDuke32's owallmost but simplified
static void wallmost(int16_t *mostbuf, int32_t w, float z_height)
{
    int32_t x1 = xb1[w];
    int32_t x2 = xb2[w];
    int32_t y1 = yb1[w];  // depth at x1
    int32_t y2 = yb2[w];  // depth at x2
    
    if (x1 < 0) x1 = 0;
    if (x2 >= xdimen) x2 = xdimen - 1;
    if (x1 > x2) return;
    
    // Calculate screen Y for each X by interpolating depth and projecting height
    for (int x = x1; x <= x2; x++) {
        // Interpolate depth
        float t = (x2 > x1) ? (float)(x - x1) / (float)(x2 - x1) : 0.0f;
        int32_t depth = y1 + (int32_t)(t * (y2 - y1));
        
        if (depth < 256) {
            mostbuf[x] = halfydimen;
            continue;
        }
        
        // Project height to screen Y
        // screen_y = halfydimen - (height * halfydimen) / depth
        float scale = (float)halfydimen / (float)depth;
        int screen_y = halfydimen - (int)(z_height * scale);
        
        // Clamp
        if (screen_y < 0) screen_y = 0;
        if (screen_y >= ydimen) screen_y = ydimen - 1;
        
        mostbuf[x] = (int16_t)screen_y;
    }
}

/* ============================================================================
   MAIN RENDERER
   ============================================================================ */

/* ============================================================================
   LINEAR WALL RENDERING (Fixes curvature distortion)
   ============================================================================ */

// Simple Z-Buffer
extern float *g_zbuffer;
static int g_zbuffer_size = 0;

static void check_resize_zbuffer() {
    int size = g_engine.displayWidth * g_engine.displayHeight;
    if (size > g_zbuffer_size) {
        if (g_zbuffer) free(g_zbuffer);
        g_zbuffer = (float*)malloc(size * sizeof(float));
        g_zbuffer_size = size;
    }
    // Clear Z-buffer to infinity
    for (int i = 0; i < size; i++) g_zbuffer[i] = 100000.0f;
}

// Helper to draw a vertical column of floor/ceiling
static void draw_plane_column(GRAPH *dest, int x, int y_start, int y_end, float height_diff, GRAPH *texture)
{
    if (y_start > y_end) return;
    
    // Fallback color logic handled inside loop for convenience or here
    uint32_t fallback_color = (height_diff > 0) ? 0xFF505050 : 0xFF707070;

    float cos_rot = cosf(g_engine.camera.rot);
    float sin_rot = sinf(g_engine.camera.rot);
    float half_w = (float)g_engine.displayWidth / 2.0f;
    float half_h = (float)g_engine.displayHeight / 2.0f;
    
    float x_offset = (float)x - half_w;
    // Use halfxdimen as view_dist to match wall projection (effectively 90 deg FOV)
    float view_dist = (float)halfxdimen; 
    
    // Restore original rotation formula (Forward = cos, sin; Right = -sin, cos)
    // This directs X forward and Y right relative to camera yaw.
    float ray_dir_x = view_dist * cos_rot - x_offset * sin_rot;
    float ray_dir_y = view_dist * sin_rot + x_offset * cos_rot;
    
    for (int y = y_start; y <= y_end; y++) {
        // Z-Buffer check
        int pixel_idx = y * g_engine.displayWidth + x;
        
        float dy = (float)y - half_h;
        if (fabsf(dy) < 0.1f) continue;
        
        // Calculate true Z depth for this pixel
        float z_depth = fabsf((height_diff * view_dist) / dy);
        
        if (z_depth >= g_zbuffer[pixel_idx]) continue;
        g_zbuffer[pixel_idx] = z_depth; // Write Z
        
        if (!texture) {
            gr_put_pixel(dest, x, y, fallback_color);
            continue;
        }
        
        // Scale MUST be positive to project forward
        // height_diff can be negative (if logic is relative), so we force abs distance
        // dy can be negative (ceiling), but distance is positive.
        float scale = fabsf(height_diff) / fabsf(dy); 
        
        float map_x = g_engine.camera.x + ray_dir_x * scale;
        float map_y = g_engine.camera.y + ray_dir_y * scale;

        // Determine texture scaling factor
        // User reported 0.125f made textures giant, reverting to 1.0f
        float tex_scale = 1.0f; 
        
        // Standard mapping (removed negation)
        int tex_x = ((int)(map_x * tex_scale)) % texture->width;
        int tex_y = ((int)(map_y * tex_scale)) % texture->height;
        
        if (tex_x < 0) tex_x += texture->width;
        if (tex_y < 0) tex_y += texture->height;
        
        uint32_t pixel = ray_sample_texture(texture, tex_x, tex_y);
        
        if (g_engine.fogOn) {
            float dist = z_depth; 
            pixel = ray_fog_pixel(pixel, dist);
        }
        
        gr_put_pixel(dest, x, y, pixel);
    }
}



static void draw_wall_segment_linear(GRAPH *dest, int x1, int x2, 
                                     int y1_ceil, int y2_ceil, 
                                     int y1_floor, int y2_floor,
                                     float z1, float z2,
                                     GRAPH *texture, float u1, float u2,
                                     RAY_Wall *wall,
                                     RAY_Sector *sector,
                                     int clip_min_x, int clip_max_x,
                                     int flags) // flags: 1=WALL, 2=FLOOR_CEIL
{
    if (x1 > x2) return;
    
    // Pre-calculate slopes for linear interpolation of screen coordinates
    float span_width = (float)(x2 - x1);
    if (span_width < 1.0f) span_width = 1.0f;
    
    float dy_ceil = (float)(y2_ceil - y1_ceil) / span_width;
    float dy_floor = (float)(y2_floor - y1_floor) / span_width;
    
    float curr_y_ceil = (float)y1_ceil;
    float curr_y_floor = (float)y1_floor;
    
    // For Z-buffering (linear interpolation of 1/z) and Texture Mapping (u/z)
    float inv_z1 = 1.0f / z1;
    float inv_z2 = 1.0f / z2;
    float d_inv_z = (inv_z2 - inv_z1) / span_width;
    float curr_inv_z = inv_z1;
    
    // Texture coordinates perspective correction
    float u_over_z1 = u1 * inv_z1;
    float u_over_z2 = u2 * inv_z2;
    float d_u_over_z = (u_over_z2 - u_over_z1) / span_width;
    float curr_u_over_z = u_over_z1;
    
    // Clip to screen X AND to portal window (clip_min_x, clip_max_x)
    int start_x = x1;
    int end_x = x2;
    
    // Initial clipping (geometry vs screen 0)
    if (start_x < 0) {
        float clip_amount = (float)(-start_x);
        curr_y_ceil += dy_ceil * clip_amount;
        curr_y_floor += dy_floor * clip_amount;
        curr_inv_z += d_inv_z * clip_amount;
        curr_u_over_z += d_u_over_z * clip_amount;
        start_x = 0;
    }
    
    // Further clipping (geometry vs portal window start)
    if (start_x < clip_min_x) {
        float clip_amount = (float)(clip_min_x - start_x);
        curr_y_ceil += dy_ceil * clip_amount;
        curr_y_floor += dy_floor * clip_amount;
        curr_inv_z += d_inv_z * clip_amount;
        curr_u_over_z += d_u_over_z * clip_amount;
        start_x = clip_min_x;
    }
    
    if (end_x >= g_engine.displayWidth) {
        end_x = g_engine.displayWidth - 1;
    }
    
    if (end_x > clip_max_x) {
        end_x = clip_max_x;
    }
    
    // Raycasting Setup for exact U coordinate (fixes texture swimming)
    float cos_rot = cosf(g_engine.camera.rot);
    float sin_rot = sinf(g_engine.camera.rot);
    float half_w = (float)g_engine.displayWidth / 2.0f;
    float view_dist = (float)halfxdimen; 
    
    // Wall vector
    float wx1 = wall->x1;
    float wy1 = wall->y1;
    float wx2 = wall->x2;
    float wy2 = wall->y2;
    
    // Wall direction vector
    float wdx = wx2 - wx1;
    float wdy = wy2 - wy1;
    float wall_len_sq = wdx*wdx + wdy*wdy;
    
    // Camera pos
    float cx = g_engine.camera.x;
    float cy = g_engine.camera.y;

    for (int x = start_x; x <= end_x; x++) {
        int y_top = (int)curr_y_ceil;
        int y_bot = (int)curr_y_floor;
        
        // Clamp Y
        if (y_top < 0) y_top = 0;
        if (y_bot >= g_engine.displayHeight) y_bot = g_engine.displayHeight - 1;
        
        // Apply Vertical Clipping
        int min_y = umost[x];
        int max_y = dmost[x];
        
        // Calculate Perspective Correct Z
        // We still use interpolated Z for depth buffering and scaling
        float z = 1.0f / curr_inv_z;

        // Calculate EXACT U using Ray Intersection
        // Ray for this column
        float x_offset = (float)x - half_w;
        float rdx = view_dist * cos_rot - x_offset * sin_rot;
        float rdy = view_dist * sin_rot + x_offset * cos_rot;
        
        // Intersect Ray (cx,cy) + t(rdx,rdy) with Wall (wx1,wy1) + s(wdx,wdy)
        // Solved via cross product
        // t = ((wx1-cx)*wdy - (wy1-cy)*wdx) / (rdx*wdy - rdy*wdx)
        // We need intersection point to get distance from wx1,wy1 -> U
        
        float det = rdx * wdy - rdy * wdx;
        float u = 0.0f;
        
        if (fabsf(det) > 0.001f) {
            float t = ((wx1 - cx) * wdy - (wy1 - cy) * wdx) / det;
            // Intersection point in world space
            float ix = cx + rdx * t;
            float iy = cy + rdy * t;
            
            // Distance from wall start (u)
            // Projected onto wall vector to handle non-axis aligned walls correctly
            // u = dot(I - W1, WallDir) / |WallDir|
            // Simplified: distance(I, W1). But need sign if behind? (shouldn't be)
            float dux = ix - wx1;
            float duy = iy - wy1;
            
            // Dot product projection for stability
            // u = (dux * wdx + duy * wdy) / sqrt(wall_len_sq) ???
            // Actually u is simply length if we assume 0..len mapping.
            // u = sqrt(dux*dux + duy*duy);
            // Better: use dot product to allow extrapolation if needed and avoid sqrt
             u = (dux * wdx + duy * wdy) / sqrtf(wall_len_sq);
        } else {
             // Parallel ray? Use interpolated U as fallback
             u = curr_u_over_z * z;
        }

        // Draw Wall
        if (flags & 1) { 
             int draw_top = (y_top < min_y) ? min_y : y_top;
             int draw_bot = (y_bot > max_y) ? max_y : y_bot;
             
             if (draw_bot >= draw_top && texture) {
                 // Texture X coordinate
                 int tex_x = ((int)u) % texture->width;
                 if (tex_x < 0) tex_x += texture->width;
                 
                 // Calculate texture scale factor for vertical mapping
                 float wall_height_screen = (float)(curr_y_floor - curr_y_ceil);
                 if (wall_height_screen < 1.0f) wall_height_screen = 1.0f;
                 
                 float v_step = (float)texture->height / wall_height_screen;
                 float base_v = (float)(y_top - curr_y_ceil) * v_step;
                 float curr_v = base_v + (float)(draw_top - y_top) * v_step;

                 for (int y = draw_top; y <= draw_bot; y++) {
                     // Z-Buffer check
                     int pixel_idx = y * g_engine.displayWidth + x;
                     if (z < g_zbuffer[pixel_idx]) {
                         int tex_y = (int)curr_v;
                         if (tex_y < 0) tex_y = 0; 
                         if (tex_y >= texture->height) tex_y = texture->height - 1;
                         
                         uint32_t pixel = ray_sample_texture(texture, tex_x, tex_y);
                         
                         if (pixel != 0) { 
                             if (g_engine.fogOn) pixel = ray_fog_pixel(pixel, z);
                             gr_put_pixel(dest, x, y, pixel);
                             g_zbuffer[pixel_idx] = z; // Write Z
                         }
                     }
                     curr_v += v_step;
                 }
            }
            else if (draw_bot >= draw_top && y_bot > y_top) { // Solid color fallback
                 uint32_t color = 0xFF808080;
                 for (int y = draw_top; y <= draw_bot; y++) {
                      int pixel_idx = y * g_engine.displayWidth + x;
                      if (z < g_zbuffer[pixel_idx]) {
                          gr_put_pixel(dest, x, y, color);
                          g_zbuffer[pixel_idx] = z;
                      }
                 }
            }
        }
        
        if (flags & 2) {
            // Draw Ceiling (0 to y_top - 1)
            // Clipped: [min_y, min(max_y, y_top-1)]
            int ceil_end = y_top - 1;
            int draw_c_start = min_y;
            int draw_c_end = (ceil_end > max_y) ? max_y : ceil_end;
            
            if (draw_c_end >= draw_c_start) { // Valid range
                if (draw_c_start < 0) draw_c_start = 0; // Sanity
                
                GRAPH *ceil_tex = NULL;
                if (sector->ceiling_texture_id > 0) ceil_tex = bitmap_get(g_engine.fpg_id, sector->ceiling_texture_id);
                if (ceil_tex) {
                    float ceil_h = sector->ceiling_z - g_engine.camera.z;
                    draw_plane_column(dest, x, draw_c_start, draw_c_end, ceil_h, ceil_tex);
                }
            }
            
            // Draw Floor (y_bot + 1 to H - 1)
            // Clipped: [max(min_y, y_bot+1), max_y]
            int floor_start = y_bot + 1;
            int draw_f_start = (floor_start < min_y) ? min_y : floor_start;
            int draw_f_end = max_y;
            
            if (draw_f_end >= draw_f_start) {
                if (draw_f_end >= g_engine.displayHeight) draw_f_end = g_engine.displayHeight - 1; // Sanity
                
                GRAPH *floor_tex = NULL;
                if (sector->floor_texture_id > 0) floor_tex = bitmap_get(g_engine.fpg_id, sector->floor_texture_id);
                if (floor_tex) {
                    float floor_h = sector->floor_z - g_engine.camera.z;
                    draw_plane_column(dest, x, draw_f_start, draw_f_end, floor_h, floor_tex);
                }
            }
        }
        
        curr_y_ceil += dy_ceil;
        curr_y_floor += dy_floor;
        curr_inv_z += d_inv_z;
        curr_u_over_z += d_u_over_z;
    }
}

// Render a convex solid sector (e.g. column, box) by casting rays for every screen column
// finding entry (front wall) and exit (back wall) points to draw the precise volume.
static void render_solid_sector(GRAPH *dest, int sector_id, int min_x, int max_x) {
    if (sector_id < 0 || sector_id >= g_engine.num_sectors) return;
    RAY_Sector *sector = &g_engine.sectors[sector_id];
    
    // Safety check: Don't render if outside screen
    if (min_x >= g_engine.displayWidth || max_x < 0) return;
    int draw_x1 = (min_x < 0) ? 0 : min_x;
    int draw_x2 = (max_x >= g_engine.displayWidth) ? g_engine.displayWidth - 1 : max_x;
    
    // Pre-calculate Sector Wall Data for faster intersection
    // (In a real engine, we'd cache this or use spatial partition)
    
    float cos_rot = cosf(g_engine.camera.rot);
    float sin_rot = sinf(g_engine.camera.rot);
    float half_w = (float)g_engine.displayWidth / 2.0f;
    float view_dist = (float)halfxdimen; 
    float cx = g_engine.camera.x;
    float cy = g_engine.camera.y;
    float cz = g_engine.camera.z;
    
    float sect_floor = sector->floor_z - cz;
    float sect_ceil = sector->ceiling_z - cz;
    
    // Fix: Zero-Height Sectors (e.g. Floating Platforms with floor==ceil)
    // Extend floor downwards to give volume
    if (fabsf(sect_ceil - sect_floor) < 1.0f) {
        sect_floor -= 32.0f; 
    }
    
    GRAPH *wall_tex = NULL; // Assuming uniform texture for optimization? Or we need to find WHICH wall was hit.
    GRAPH *ceil_tex = NULL;
    if (sector->ceiling_texture_id > 0) ceil_tex = bitmap_get(g_engine.fpg_id, sector->ceiling_texture_id);
    
    // Per-column Scan
    for (int x = draw_x1; x <= draw_x2; x++) {
        // Construct Ray
        float x_offset = (float)x - half_w;
        float rdx = view_dist * cos_rot - x_offset * sin_rot;
        float rdy = view_dist * sin_rot + x_offset * cos_rot;
        
        // Find Entry (Near) and Exit (Far) intersections
        float t_near = FLT_MAX;
        float t_far = -FLT_MAX;
        
        RAY_Wall *near_wall = NULL;
        float near_u = 0.0f;
        
        for (int w = 0; w < sector->num_walls; w++) {
             RAY_Wall *wall = &sector->walls[w];
             float wx1 = wall->x1; float wy1 = wall->y1;
             float wdx = wall->x2 - wall->x1;
             float wdy = wall->y2 - wall->y1;
             
             float det = rdx * wdy - rdy * wdx;
             if (fabsf(det) > 0.0001f) {
                 float t = ((wx1 - cx) * wdy - (wy1 - cy) * wdx) / det;
                 
                 float s_chk = -1.0f;
                 if (fabsf(wdx) > fabsf(wdy)) {
                     s_chk = (cx + t * rdx - wx1) / wdx;
                 } else {
                     s_chk = (cy + t * rdy - wy1) / wdy;
                 }
                 
                 if (s_chk >= 0.0f && s_chk <= 1.0f && t > 0.1f) { // Valid hit forward
                     if (t < t_near) {
                         t_near = t;
                         near_wall = wall;
                         
                         // Calculate U
                         float dist_sq = wdx*wdx + wdy*wdy;
                         float wall_len = sqrtf(dist_sq);
                         near_u = wall_len * s_chk;
                     }
                     if (t > t_far) {
                         t_far = t;
                     }
                 }
             }
        }
        
        // Check if we hit any wall (entry or exit)
        if (t_far > 0.1f) {
            // If we are inside the sector (or crossed the front wall), t_near might be invalid or behind us.
            // If near_wall is NULL but t_far is valid, we are likely inside.
            int y_near_top, y_near_bot;
            
            if (near_wall && t_near > 0.1f) {
                 // Normal case: outside looking in
                 y_near_top = halfydimen - (int)(sect_ceil / t_near);
                 y_near_bot = halfydimen - (int)(sect_floor / t_near);
            } else {
                 // Inside looking out (or very close), clamp near plane
                 t_near = 0.1f; 
                 // If t is small, Height/t is HUGE. 
                 // If sect_ceil > 0 (below cam), y = half - HUGE = -HUGE (Top of screen clipped)
                 // If sect_ceil < 0 (above cam), y = half - (-HUGE) = +HUGE (Bottom of screen clipped)
                 
                 // We clamp to avoid integer logic breaking
                 if (sect_ceil > 0) y_near_top = -32000; else y_near_top = 32000;
                 if (sect_floor > 0) y_near_bot = -32000; else y_near_bot = 32000;
            }
            
            // Apply Clipping
            int min_y = umost[x];
            int max_y = dmost[x];
            
            int draw_top = (y_near_top < min_y) ? min_y : y_near_top;
            int draw_bot = (y_near_bot > max_y) ? max_y : y_near_bot;
            
            // Only draw wall if it exists and is in front
            if (near_wall && t_near > 0.1f) {

            
            if (near_wall->texture_id_middle > 0) 
                 wall_tex = bitmap_get(g_engine.fpg_id, near_wall->texture_id_middle);
            
            if (draw_bot >= draw_top) {
                 // Sample Wall
                 int tex_x = 0;
                 if (wall_tex) {
                     tex_x = ((int)near_u) % wall_tex->width;
                     if (tex_x < 0) tex_x += wall_tex->width;
                 }
                 
                 float wall_h_scr = (float)(y_near_bot - y_near_top);
                 if (wall_h_scr < 1.0f) wall_h_scr = 1.0f;
                 
                 float v_step = 0.0f;
                 if (wall_tex) v_step = (float)wall_tex->height / wall_h_scr;
                 
                 float curr_v = 0.0f;
                 if (wall_tex) curr_v = (float)(draw_top - y_near_top) * v_step;

                 for (int y = draw_top; y <= draw_bot; y++) {
                     int pixel_idx = y * g_engine.displayWidth + x;
                     
                     // Z-Buffer Check
                     if (t_near * view_dist < g_zbuffer[pixel_idx]) {
                         uint32_t pix = 0; 
                         
                         if (wall_tex) {
                             int tex_y = (int)curr_v;
                             if (tex_y < 0) tex_y = 0;
                             if (tex_y >= wall_tex->height) tex_y = wall_tex->height - 1;
                             
                             pix = ray_sample_texture(wall_tex, tex_x, tex_y);
                         } else {
                             pix = 0xFF00FF; // MAGENTA DEBUG for missing texture
                         }
                         
                         if (g_engine.fogOn) pix = ray_fog_pixel(pix, t_near * view_dist);
                         gr_put_pixel(dest, x, y, pix);
                         g_zbuffer[pixel_idx] = t_near * view_dist;
                     }
                     if (wall_tex) curr_v += v_step;
                 }
                }
            }
            
            // Render Lid (Ceiling)
            // Note: We checks t_far > 0.1 to ensure we are looking at the object
            // relaxed condition: t_far > 0.1 (valid hit) is enough.
            if (t_far > 0.1f && ceil_tex) {
                 // Fix: Remove * halfydimen, match wall projection scale
                 int y_far_top = halfydimen - (int)(sect_ceil / t_far);
                 
                 // If looking down at the box top:
                 // Near edge is lower on screen (larger Y).
                 // Far edge is higher on screen (smaller Y).
                 // So y_near_top > y_far_top.
                 // We want to draw from Far (Top) to Near (Bottom) or vice versa.
                 // Clipping window expects min_y (top) to max_y (bottom).
                 
                 int lid_start = y_far_top; 
                 int lid_end = y_near_top;
                 
                 // Render if we are looking from above (lid_end > lid_start)
                 // Or just forcefully render min-max range
                 if (lid_end < lid_start) {
                     int temp = lid_start; lid_start = lid_end; lid_end = temp;
                 }

                 int draw_l_start = (lid_start < min_y) ? min_y : lid_start;
                 int draw_l_end = (lid_end > max_y) ? max_y : lid_end;
                 
                 if (draw_l_end >= draw_l_start) {
                     // We use a simplified constant height plane drawer for now
                     // Ideally we need perspective correct u/v mapping for the lid surface
                     draw_plane_column(dest, x, draw_l_start, draw_l_end, sect_ceil, ceil_tex);
                 }
            }
            
            // Render Bottom Lid (Floor) - Needed for floating islands
            GRAPH *floor_tex = NULL;
            if (sector->floor_texture_id > 0) floor_tex = bitmap_get(g_engine.fpg_id, sector->floor_texture_id);
            
            if (t_far > 0.1f && floor_tex) {
                 int y_far_bot = halfydimen - (int)(sect_floor / t_far);
                 
                 // Bottom face range: [y_near_bot, y_far_bot]
                 // y_near_bot is the bottom of the front wall
                 
                 int lid_start = y_near_bot; 
                 int lid_end = y_far_bot;
                 
                 if (lid_end < lid_start) {
                     int temp = lid_start; lid_start = lid_end; lid_end = temp;
                 }

                 int draw_l_start = (lid_start < min_y) ? min_y : lid_start;
                 int draw_l_end = (lid_end > max_y) ? max_y : lid_end;
                 
                 if (draw_l_end >= draw_l_start) {
                     draw_plane_column(dest, x, draw_l_start, draw_l_end, sect_floor, floor_tex);
                 }
            }
        }
    }
}

// Recursive rendering function
static void render_sector(GRAPH *dest, int sector_id, int min_x, int max_x, int depth)
{
    if (depth > 16) return;
    if (sector_id < 0 || sector_id >= g_engine.num_sectors) return;
    
    RAY_Sector *sector = &g_engine.sectors[sector_id];
    
    // Project all walls in this sector
    for (int w = 0; w < sector->num_walls; w++) {
        RAY_Wall *wall = &sector->walls[w];
        
        // Transform to camera space
        vec2_t p1 = transform_to_camera(wall->x1, wall->y1);
        vec2_t p2 = transform_to_camera(wall->x2, wall->y2);
        
        // Project to screen
        int32_t sx1, sx2;
        float z1, z2;
        float uf1, uf2;
        
        if (!get_screen_coords(p1, p2, &sx1, &z1, &uf1, &sx2, &z2, &uf2)) {
            continue;
        }
        
        // Horizontal Clipping against portal window
        // We only render the part of the wall that is within [min_x, max_x]
        
        // Simple trivial rejection
        if (sx2 < min_x || sx1 > max_x) continue;
        
        // Calculate clipped range
        int draw_x1 = (sx1 < min_x) ? min_x : sx1;
        int draw_x2 = (sx2 > max_x) ? max_x : sx2;
        
        // If clipping reduced the span to nothing, skip
        if (draw_x1 > draw_x2) continue;

        // Heights
        float floor_h = sector->floor_z - g_engine.camera.z;
        float ceil_h = sector->ceiling_z - g_engine.camera.z;
        
        // Calculate screen Y for top/bottom
        int y1_top = halfydimen - (int)((ceil_h * halfydimen) / z1);
        int y1_bot = halfydimen - (int)((floor_h * halfydimen) / z1);
        int y2_top = halfydimen - (int)((ceil_h * halfydimen) / z2);
        int y2_bot = halfydimen - (int)((floor_h * halfydimen) / z2);
        
        // Determine if this wall is a portal to another sector
        // Calculate texture U coordinates common for all wall parts
        // Approximation of distance for U coordinates
        float dist_sq = powf(wall->x2 - wall->x1, 2) + powf(wall->y2 - wall->y1, 2);
        float wall_len = sqrtf(dist_sq);
        
        // Apply Clipping Factors to U
        // If uf1 > 0, the start was clipped.
        // If uf2 < 1, the end was clipped.
        // Or if inputs were swapped, factors are swapped too.
        
        float u1 = wall_len * uf1;
        float u2 = wall_len * uf2;

        int next_sector_id = -1;
        if (wall->portal_id != -1 && wall->portal_id < g_engine.num_portals) {
            RAY_Portal *portal = &g_engine.portals[wall->portal_id];
            if (portal->sector_a == sector_id) next_sector_id = portal->sector_b;
            else if (portal->sector_b == sector_id) next_sector_id = portal->sector_a;
        }

        // Determine render flags
        // If sector is solid (column/box), we ONLY want to draw the walls.
        // Drawing floor/ceiling would fill the screen OUTSIDE the object (standard room behavior),
        // which overwrites the parent sector. 
        // Note: This leaves solid objects without top/bottom caps (lids). 
        // Caps require polygon rendering inside the wall loop, which is not yet implemented.
        // For wall-to-ceiling columns, this is perfect.
        int draw_flags = 3; // Default: Wall + Floor/Ceil
        if (sector->is_solid || sector->sector_type == RAY_SECTOR_NESTED_COLUMN || sector->sector_type == RAY_SECTOR_NESTED_BOX) {
            draw_flags = 1; // Wall Only
        }

        // Is this a portal?
        if (next_sector_id != -1) {
            RAY_Sector *next_sector = &g_engine.sectors[next_sector_id];
            float next_floor_h = next_sector->floor_z - g_engine.camera.z;
            float next_ceil_h = next_sector->ceiling_z - g_engine.camera.z;
            
            // Calculate next sector heights (the "hole")
            int ny1_top = halfydimen - (int)((next_ceil_h * halfydimen) / z1);
            int ny1_bot = halfydimen - (int)((next_floor_h * halfydimen) / z1);
            int ny2_top = halfydimen - (int)((next_ceil_h * halfydimen) / z2);
            int ny2_bot = halfydimen - (int)((next_floor_h * halfydimen) / z2);
            
            // ---------------------------------------------------------
            // PORTAL CLIPPING: Update Vertical Window for Next Sector
            // ---------------------------------------------------------
            
            // Allocate backup arrays (on stack to avoid malloc thrashing, size is MAXSCREENWIDTH)
            // Warning: large stack usage. MAXSCREENWIDTH=2048 * 2 bytes * 2 arrays = 8KB. Safe on PC.
            int16_t saved_umost[MAXSCREENWIDTH];
            int16_t saved_dmost[MAXSCREENWIDTH];
            
            // Interpolation setup for portal window
            float dx = (float)(draw_x2 - draw_x1);
            if (dx < 1.0f) dx = 1.0f;
            
            // The portal window is the intersection of the current sector's view 
            // and the next sector's vertical opening.
            // Current sector vertical range is defined by y_top and y_bot (interpolated).
            // Next sector vertical range "hole" is ny_top and ny_bot.
            // Effectively, the new window is [max(y_curr, y_next_ceil), min(y_curr_floor, y_next_floor)]
            
            // Steps to interpolate
            // Note: We need to interpolate values relative to sx1, like in draw_wall_segment
            float span = (float)(sx2 - sx1);
            if (span < 1.0f) span = 1.0f;
            
            float d_y1t = (float)(y2_top - y1_top) / span;
            float d_ny1t = (float)(ny2_top - ny1_top) / span;
            float d_y1b = (float)(y2_bot - y1_bot) / span;
            float d_ny1b = (float)(ny2_bot - ny1_bot) / span;
            
            // Starting values at draw_x1
            float c_y1t = (float)y1_top + d_y1t * (float)(draw_x1 - sx1);
            float c_ny1t = (float)ny1_top + d_ny1t * (float)(draw_x1 - sx1);
            float c_y1b = (float)y1_bot + d_y1b * (float)(draw_x1 - sx1);
            float c_ny1b = (float)ny1_bot + d_ny1b * (float)(draw_x1 - sx1);
            
            for (int x = draw_x1; x <= draw_x2; x++) {
                // Save current state
                saved_umost[x] = umost[x];
                saved_dmost[x] = dmost[x];
                
                // Calculate portal vertical limits at this column
                int cy_top_curr = (int)c_y1t;
                int cny_top_next = (int)c_ny1t;
                int cy_bot_curr = (int)c_y1b;
                int cny_bot_next = (int)c_ny1b;
                
                // The hole begins at the lower of the two ceilings (visually higher Y value if looking down? No)
                // Y increases downwards (0 is top).
                // Ceiling is at Y=0...top. Floor is at Y=bot...H.
                // Visible region is y_top...y_bot.
                // The step blocks y_top...ny_top.
                // So the new top is max(y_top, ny_top).
                int new_top = (cy_top_curr > cny_top_next) ? cy_top_curr : cny_top_next;
                
                // The step blocks ny_bot...y_bot.
                // The new bottom is min(y_bot, ny_bot).
                int new_bot = (cy_bot_curr < cny_bot_next) ? cy_bot_curr : cny_bot_next;
                
                // Clamp to existing window
                if (new_top < umost[x]) new_top = umost[x];
                if (new_bot > dmost[x]) new_bot = dmost[x];
                
                // Update global clipping arrays
                umost[x] = (int16_t)new_top;
                dmost[x] = (int16_t)new_bot;
                
                // Advance interpolators
                c_y1t += d_y1t;
                c_ny1t += d_ny1t;
                c_y1b += d_y1b;
                c_ny1b += d_ny1b;
                
                // ---------------------------------------------------------
                // PORTAL STEP RENDERING (Upper/Lower Walls)
                // ---------------------------------------------------------
                // We need to fill the gaps between current sector height and next sector height.
                
                // Texture Fetching (Optimized: do outside loop? No, wall ptr is outside)
                // Assuming wall->texture_id_upper/lower are valid
                
                // Calculate U coordinate for this column (Ray Intersection)
                // Reuse logic from draw_wall_segment_linear
                float u_coord = 0.0f;
                float z_depth = 1.0f; // Need depth for Z-buffer
                
                {
                    float half_w = (float)g_engine.displayWidth / 2.0f;
                    float view_dist = (float)halfxdimen; 
                    float cos_rot = cosf(g_engine.camera.rot);
                    float sin_rot = sinf(g_engine.camera.rot);
                    float cx = g_engine.camera.x;
                    float cy = g_engine.camera.y;
                    
                    float x_offset = (float)x - half_w;
                    float rdx = view_dist * cos_rot - x_offset * sin_rot;
                    float rdy = view_dist * sin_rot + x_offset * cos_rot;
                    
                    float wdx = wall->x2 - wall->x1;
                    float wdy = wall->y2 - wall->y1;
                    float det = rdx * wdy - rdy * wdx;
                    
                    if (fabsf(det) > 0.001f) {
                        float t = ((wall->x1 - cx) * wdy - (wall->y1 - cy) * wdx) / det;
                        z_depth = t; // Approximate depth along ray
                        
                        float ix = cx + rdx * t;
                        float iy = cy + rdy * t;
                        float dux = ix - wall->x1;
                        float duy = iy - wall->y1;
                        // U = distance from start
                         u_coord = (dux * wdx + duy * wdy) / sqrtf(wdx*wdx + wdy*wdy);
                    }
                }
                
                // Clipping Limits (Original limits of this sector)
                int clip_min = saved_umost[x];
                int clip_max = saved_dmost[x];
                
                // --- UPPER STEP ---
                // --- UPPER STEP ---
                if (cny_top_next > cy_top_curr) {
                    GRAPH *tex_upper = NULL;
                    if (wall->texture_id_upper > 0) 
                        tex_upper = bitmap_get(g_engine.fpg_id, wall->texture_id_upper);
                        
                    int step_top = cy_top_curr;
                    int step_bot = cny_top_next;
                    
                    int draw_top = (step_top < clip_min) ? clip_min : step_top;
                    int draw_bot = (step_bot > clip_max) ? clip_max : step_bot;
                    
                    if (draw_bot >= draw_top) {
                        int tex_x = 0;
                        if (tex_upper) {
                            tex_x = ((int)u_coord) % tex_upper->width;
                            if (tex_x < 0) tex_x += tex_upper->width;
                        }
                        
                        float wall_h_full = (float)(cy_bot_curr - cy_top_curr);
                        if (wall_h_full < 1.0f) wall_h_full = 1.0f;
                        
                        float v_step = 0.0f; 
                        if (tex_upper) v_step = (float)tex_upper->height / wall_h_full;
                        
                        float curr_v = 0.0f;
                        if (tex_upper) curr_v = (float)(draw_top - cy_top_curr) * v_step;
                        
                        for (int y = draw_top; y <= draw_bot; y++) {
                             int pixel_idx = y * g_engine.displayWidth + x;
                             if (z_depth < g_zbuffer[pixel_idx]) {
                                 uint32_t pix;
                                 if (tex_upper) {
                                     int tex_y = (int)curr_v;
                                     if (tex_y < 0) tex_y = 0;
                                     if (tex_y >= tex_upper->height) tex_y = tex_upper->height - 1;
                                     
                                     pix = ray_sample_texture(tex_upper, tex_x, tex_y);
                                     
                                     // DEBUG: If pixel is black/transparent, show GREEN to differentiate from Fog/Null
                                     if (pix == 0) pix = 0x00FF00; 
                                     
                                     // FOG DISABLED for debugging
                                     // if (g_engine.fogOn) pix = ray_fog_pixel(pix, z_depth * halfxdimen);
                                 } else {
                                     pix = 0xFF0000; // RED DEBUG for Upper Step (Missing Texture)
                                 }
                                 
                                 gr_put_pixel(dest, x, y, pix);
                                 g_zbuffer[pixel_idx] = z_depth * halfxdimen;
                             }
                             if (tex_upper) curr_v += v_step;
                        }
                    }
                }
                
                // --- LOWER STEP ---
                if (cny_bot_next < cy_bot_curr) {
                    GRAPH *tex_lower = NULL;
                    if (wall->texture_id_lower > 0) 
                        tex_lower = bitmap_get(g_engine.fpg_id, wall->texture_id_lower);
                        
                    int step_top = cny_bot_next;
                    int step_bot = cy_bot_curr;
                    
                    int draw_top = (step_top < clip_min) ? clip_min : step_top;
                    int draw_bot = (step_bot > clip_max) ? clip_max : step_bot;
                    
                    if (draw_bot >= draw_top) {
                         int tex_x = 0;
                         if (tex_lower) {
                             tex_x = ((int)u_coord) % tex_lower->width;
                             if (tex_x < 0) tex_x += tex_lower->width;
                         }

                        float wall_h_full = (float)(cy_bot_curr - cy_top_curr);
                        if (wall_h_full < 1.0f) wall_h_full = 1.0f;
                        
                        float v_step = 0.0f;
                        if (tex_lower) v_step = (float)tex_lower->height / wall_h_full;
                        
                        float curr_v = 0.0f;
                        if (tex_lower) curr_v = (float)(draw_top - cy_top_curr) * v_step;
                         
                         for (int y = draw_top; y <= draw_bot; y++) {
                             int pixel_idx = y * g_engine.displayWidth + x;
                             if (z_depth < g_zbuffer[pixel_idx]) {
                                 uint32_t pix;
                                 if (tex_lower) {
                                     int tex_y = (int)curr_v;
                                     tex_y = tex_y % tex_lower->height;
                                     if (tex_y < 0) tex_y += tex_lower->height;
                                     
                                     pix = ray_sample_texture(tex_lower, tex_x, tex_y);
                                     
                                     // DEBUG: Green for transparent
                                     if (pix == 0) pix = 0x00FF00;
                                     
                                     // FOG DISABLED
                                     // if (g_engine.fogOn) pix = ray_fog_pixel(pix, z_depth * halfxdimen);
                                 } else {
                                     pix = 0x0000FF; // BLUE DEBUG for Lower Step (Missing Texture)
                                 }
                                 
                                 gr_put_pixel(dest, x, y, pix);
                                 g_zbuffer[pixel_idx] = z_depth * halfxdimen;
                             }
                             if (tex_lower) curr_v += v_step;
                        }
                    }
                }
            }
            
            // Recursion
            render_sector(dest, next_sector_id, draw_x1, draw_x2, depth + 1);
            
            // Restore clipping arrays
            for (int x = draw_x1; x <= draw_x2; x++) {
                umost[x] = saved_umost[x];
                dmost[x] = saved_dmost[x];
            }
            // ---------------------------------------------------------
            
            // Draw Upper Step (Ceiling transition)
            GRAPH *upper_tex = NULL;
            if (wall->texture_id_upper > 0) upper_tex = bitmap_get(g_engine.fpg_id, wall->texture_id_upper);
            
            if (upper_tex) {
                 draw_wall_segment_linear(dest, sx1, sx2,
                                          y1_top, y2_top,   // Top of screen (current ceil)
                                          ny1_top, ny2_top, // Bottom of step (next ceil)
                                          z1, z2, upper_tex, u1, u2, 
                                          wall, sector, 
                                          min_x, max_x, 1); // flag 1 = WALL ONLY (no floor/ceil)
            }

            // Draw Lower Step (Floor transition)
            GRAPH *lower_tex = NULL;
            if (wall->texture_id_lower > 0) lower_tex = bitmap_get(g_engine.fpg_id, wall->texture_id_lower);
            
            if (lower_tex) {
                 draw_wall_segment_linear(dest, sx1, sx2,
                                          ny1_bot, ny2_bot, // Top of step (next floor)
                                          y1_bot, y2_bot,   // Bottom of screen (current floor)
                                          z1, z2, lower_tex, u1, u2, 
                                          wall, sector,
                                          min_x, max_x, 1); // flag 1 = WALL ONLY
            }
        }
        
        if (next_sector_id == -1) {
             GRAPH *texture = NULL;
             if (wall->texture_id_middle > 0) texture = bitmap_get(g_engine.fpg_id, wall->texture_id_middle);
             
             draw_wall_segment_linear(dest, sx1, sx2, 
                                      y1_top, y2_top, y1_bot, y2_bot, 
                                      z1, z2, texture, u1, u2, 
                                      wall, sector,
                                      min_x, max_x, draw_flags); // USE draw_flags HERE (was 3)
        } else {
             // Portal - draw ONLY floor and ceiling
             draw_wall_segment_linear(dest, sx1, sx2, 
                                      y1_top, y2_top, y1_bot, y2_bot, 
                                      z1, z2, NULL, 0, 0, 
                                      wall, sector,
                                      min_x, max_x, 2); // flag 2 = FLOOR/CEIL ONLY (skip wall)
        }
    }

    // Render Nested Sectors (Children) - Islands, Columns, Solid Objects
    if (sector->num_children > 0 && sector->child_sector_ids) {
        for (int i = 0; i < sector->num_children; i++) {
            int child_id = sector->child_sector_ids[i];
            RAY_Sector *child = &g_engine.sectors[child_id];
            
            if (child->is_solid) {
                 // Forward declare or move function? 
                 // Best: declare static void render_solid_sector(...) at top or just rely on re-ordering via tool now.
                 // We will move render_solid_sector definition BEFORE render_sector in the next chunk.
                 render_solid_sector(dest, child_id, min_x, max_x);
            } else {
                 // Normal recursive render
                 render_sector(dest, child_id, min_x, max_x, depth + 1);
            }
        }
    }
}


void ray_render_frame_build(GRAPH *dest)
{
    if (!dest || !g_engine.initialized) return;
    
    gr_clear(dest);
    check_resize_zbuffer();
    
    // Initialize screen dimensions
    xdimen = g_engine.displayWidth;
    ydimen = g_engine.displayHeight;
    halfxdimen = xdimen / 2;
    halfydimen = ydimen / 2;
    viewingrange = halfxdimen;
    
    // Initialize clipping arrays
    for (int x = 0; x < xdimen; x++) {
        umost[x] = 0;
        dmost[x] = ydimen - 1;
    }
    
    int camera_sector_id = g_engine.camera.current_sector_id;
    if (camera_sector_id < 0 || camera_sector_id >= g_engine.num_sectors) camera_sector_id = 0;
    
    // Start recursive rendering
    render_sector(dest, camera_sector_id, 0, xdimen - 1, 0);
}
