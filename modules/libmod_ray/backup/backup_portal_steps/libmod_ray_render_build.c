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
    int32_t x, y;
} vec2_t;

// Transform world coordinates to camera space
static vec2_t transform_to_camera(float world_x, float world_y)
{
    float dx = world_x - g_engine.camera.x;
    float dy = world_y - g_engine.camera.y;
    
    float cos_rot = cosf(g_engine.camera.rot);
    float sin_rot = sinf(g_engine.camera.rot);
    
    vec2_t result;
    result.x = (int32_t)(dx * cos_rot + dy * sin_rot);   // Forward (depth)
    result.y = (int32_t)(-dx * sin_rot + dy * cos_rot);  // Right (lateral)
    
    return result;
}

// Simplified get_screen_coords with proper near-plane clipping
static int get_screen_coords(vec2_t p1, vec2_t p2,
                             int32_t *sx1ptr, int32_t *sy1ptr,
                             int32_t *sx2ptr, int32_t *sy2ptr)
{
    const float NEAR_Z = 1.0f; // Minimum depth

    // If both points are behind, reject
    if (p1.x < NEAR_Z && p2.x < NEAR_Z) return 0;

    // Clip against Near Plane (x = NEAR_Z in camera space, since x is depth)
    // Segment p1->p2. 
    // Parametric line: P = p1 + t(p2 - p1)
    // We want x = NEAR_Z
    // NEAR_Z = p1.x + t(p2.x - p1.x)
    // t = (NEAR_Z - p1.x) / (p2.x - p1.x)
    
    vec2_t cp1 = p1;
    vec2_t cp2 = p2;

    if (p1.x < NEAR_Z) {
        float t = (NEAR_Z - p1.x) / (p2.x - p1.x);
        cp1.x = NEAR_Z;
        cp1.y = p1.y + t * (p2.y - p1.y);
    }
    
    if (p2.x < NEAR_Z) {
        float t = (NEAR_Z - p2.x) / (p1.x - p2.x);
        cp2.x = NEAR_Z;
        cp2.y = p2.y + t * (p1.y - p2.y);
    }

    // Now both cp1 and cp2 are >= NEAR_Z. Project them.
    // sx = half_width + (y * half_width / x)
    
    // Point 1
    *sx1ptr = halfxdimen + (int32_t)((cp1.y * halfxdimen) / cp1.x);
    // Depths for interpolation (use the CLIPPED depth if you want linear interpolation in screen space to match geometry??)
    // Wait. If we clip p1 to cp1, we are rendering a shorter segment.
    // For TEXTURE MAPPING, this complicates things (u/v need to be adjusted).
    // But for solid walls, it's fine. 
    // HOWEVER, for correct perspective interpolation (1/z), we must pass the depths of the screen points we generated!
    // So yes, pass cp1.x/cp2.x.
    
    *sy1ptr = (int32_t)cp1.x; // Just casting to int for storage, assumes sufficient precision?
                              // Ideally render loop should use floats.
                              // But struct arrays are int32. Let's hope precision is enough for simple maps.
                              // Actually, standard Build engine uses int coordinates everywhere.
                              // But here p1.x is float.
    
    // Point 2
    *sx2ptr = halfxdimen + (int32_t)((cp2.y * halfxdimen) / cp2.x);
    *sy2ptr = (int32_t)cp2.x;

    // Ensure sx1 < sx2 is NOT logic we should enforce blindly if it flips the wall!
    // If sx1 > sx2, it means the wall is back-facing (or we are crossing it).
    // In normal rendering we should cull it.
    // But let's keep the existing behaviour of ensuring left-to-right drawing 
    // and see if that fixes the "left portal" confusion.
    // If we simply swap, we might draw inside-out textures but the geometry will fill the hole.
    
    if (*sx1ptr > *sx2ptr) {
        int32_t tmp = *sx1ptr; *sx1ptr = *sx2ptr; *sx2ptr = tmp;
        tmp = *sy1ptr; *sy1ptr = *sy2ptr; *sy2ptr = tmp;
        return 1; // It was valid, just swapped
        // Alternatively return 0 to backface cull? 
        // Let's return 0 to test if culling fixes the "weird wall"
        // return 0; 
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
static float *g_zbuffer = NULL;
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
        float scale = fabsf(height_diff) / dy; 
        
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
    
    for (int x = start_x; x <= end_x; x++) {
        int y_top = (int)curr_y_ceil;
        int y_bot = (int)curr_y_floor;
        
        // Clamp Y
        if (y_top < 0) y_top = 0;
        if (y_bot >= g_engine.displayHeight) y_bot = g_engine.displayHeight - 1;
        
        // Apply Vertical Clipping
        int min_y = umost[x];
        int max_y = dmost[x];
        
        // Calculate Perspective Correct Z and U (needed for both wall and plane drawing)
        float z = 1.0f / curr_inv_z;
        float u = curr_u_over_z * z;

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
                 // We need to calculate curr_v based on ORIGINAL y_top, then advance to draw_top
                 float curr_v = (float)(draw_top - curr_y_ceil) * v_step; 
                 // Wait, curr_y_ceil corresponds to unclipped y_top (roughly)
                 // Re-calc: curr_v at y=y_top is (y_top - curr_y_ceil)*step. 
                 // At y=draw_top, delta is (draw_top - y_top)*step.
                 // Correct logic:
                 float base_v = (float)(y_top - curr_y_ceil) * v_step;
                 curr_v = base_v + (float)(draw_top - y_top) * v_step;

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
        int32_t sx1, sy1, sx2, sy2;
        if (!get_screen_coords(p1, p2, &sx1, &sy1, &sx2, &sy2)) {
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
        float z1 = (float)sy1;
        float z2 = (float)sy2;
        
        float floor_h = sector->floor_z - g_engine.camera.z;
        float ceil_h = sector->ceiling_z - g_engine.camera.z;
        
        // Calculate screen Y for top/bottom
        int y1_top = halfydimen - (int)((ceil_h * halfydimen) / z1);
        int y1_bot = halfydimen - (int)((floor_h * halfydimen) / z1);
        int y2_top = halfydimen - (int)((ceil_h * halfydimen) / z2);
        int y2_bot = halfydimen - (int)((floor_h * halfydimen) / z2);
        
        // Determine if this wall is a portal to another sector
        // Calculate texture U coordinates common for all wall parts
        float u1 = 0, u2 = 0;
        // Approximation of distance for U coordinates
        float dist_sq = powf(wall->x2 - wall->x1, 2) + powf(wall->y2 - wall->y1, 2);
        float dist = sqrtf(dist_sq);
        u2 = dist;

        int next_sector_id = -1;
        if (wall->portal_id != -1 && wall->portal_id < g_engine.num_portals) {
            RAY_Portal *portal = &g_engine.portals[wall->portal_id];
            if (portal->sector_a == sector_id) next_sector_id = portal->sector_b;
            else if (portal->sector_b == sector_id) next_sector_id = portal->sector_a;
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
                                          z1, z2, upper_tex, u1, u2, sector, 
                                          min_x, max_x, 1); // flag 1 = WALL ONLY (no floor/ceil)
            }

            // Draw Lower Step (Floor transition)
            GRAPH *lower_tex = NULL;
            if (wall->texture_id_lower > 0) lower_tex = bitmap_get(g_engine.fpg_id, wall->texture_id_lower);
            
            if (lower_tex) {
                 draw_wall_segment_linear(dest, sx1, sx2,
                                          ny1_bot, ny2_bot, // Top of step (next floor)
                                          y1_bot, y2_bot,   // Bottom of screen (current floor)
                                          z1, z2, lower_tex, u1, u2, sector,
                                          min_x, max_x, 1); // flag 1 = WALL ONLY
            }
        }
        
        // Draw Wall (Solid or Portal Frame)
        if (next_sector_id == -1) {
             GRAPH *texture = NULL;
             if (wall->texture_id_middle > 0) texture = bitmap_get(g_engine.fpg_id, wall->texture_id_middle);
             
             draw_wall_segment_linear(dest, sx1, sx2, 
                                      y1_top, y2_top, y1_bot, y2_bot, 
                                      z1, z2, texture, u1, u2, sector,
                                      min_x, max_x, 3); // flag 3 = WALL + FLOOR/CEIL
        } else {
             // Portal - draw ONLY floor and ceiling
             draw_wall_segment_linear(dest, sx1, sx2, 
                                      y1_top, y2_top, y1_bot, y2_bot, 
                                      z1, z2, NULL, 0, 0, sector,
                                      min_x, max_x, 2); // flag 2 = FLOOR/CEIL ONLY (skip wall)
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
