#include "libmod_ray.h"
#include <float.h>

/* Try to include Bennu's GPU headers if available in include path */
/* If not found, we declare prototypes manually based on user info */
/* #include "g_draw.h" - We might not find it if not implicitly added */

/* Using BennuGD2 headers via libmod_ray.h -> libbggfx.h */
/* We rely on the headers for drawing_color_*, drawing_blend_mode, and draw_polygon */

/* Draw Functions */
/* Draw functions are included via libbggfx.h */

extern RAY_Engine g_engine;

/* ============================================================================
   GPU WALL RENDERING
   ============================================================================ */

/* Coordinate Transforms */
typedef struct { float x, y; } vec2_t_gpu;

static vec2_t_gpu transform_to_camera_gpu(float wx, float wy) {
    float dx = wx - g_engine.camera.x;
    float dy = wy - g_engine.camera.y;
    float cos_rot = cosf(g_engine.camera.rot);
    float sin_rot = sinf(g_engine.camera.rot);
    
    vec2_t_gpu p;
    p.x = dx * sin_rot - dy * cos_rot;
    p.y = dx * cos_rot + dy * sin_rot;
    return p;
}

extern GPU_Target * gRenderer; // Global Renderer from BennuGD2

static void draw_wall_quad_bennu(GRAPH *dest, REGION *clip,
                                 int x1, int y1_top, int y1_bot, 
                                 int x2, int y2_top, int y2_bot, 
                                 int texture_id) 
{
    // 1. Try to get Texture
    GRAPH *tex_map = NULL;
    GPU_Image *gpu_image = NULL;
    
    if (texture_id > 0) {
        tex_map = bitmap_get(g_engine.fpg_id, texture_id);
        if (tex_map) {
             gpu_image = (GPU_Image *)tex_map->tex;
        }
    }

    // 2. Set Clipping (DISABLED FOR DEBUGGING)
    /*
    if (clip) {
        GPU_SetClip(gRenderer, (int)clip->x, (int)clip->y, (int)(clip->x2 - clip->x), (int)(clip->y2 - clip->y));
    } else {
        GPU_UnsetClip(gRenderer);
    }
    */
    
    if (gpu_image && texture_id != -1) {
        // GPU TEXTURED RENDERING
        GPU_Target *target = GPU_GetContextTarget();
        if (!target) target = gRenderer;
        
        // Calculate texture coordinates
        // Wall length for U coordinate
        float wall_length = sqrtf((x2-x1)*(x2-x1) + (y2_bot-y1_bot)*(y2_bot-y1_bot));
        float wall_height = (float)(y1_bot - y1_top); // Screen space height
        
        // Normalize texture coordinates (0-1 range)
        float u_scale = wall_length / (float)gpu_image->w;
        float v_scale = wall_height / (float)gpu_image->h;
        
        // Vertices: [x, y, s, t] where s,t are texture coordinates
        // GPU_BATCH_XY_ST = 0x5 (positions + texture coords)
        float values[16] = {
            // Vertex 0: Bottom-left
            (float)x1, (float)y1_bot, 0.0f, v_scale,
            // Vertex 1: Bottom-right  
            (float)x2, (float)y2_bot, u_scale, v_scale,
            // Vertex 2: Top-right
            (float)x2, (float)y2_top, u_scale, 0.0f,
            // Vertex 3: Top-left
            (float)x1, (float)y1_top, 0.0f, 0.0f
        };
        
        unsigned short indices[6] = {0, 1, 2, 0, 2, 3};
        
        // GPU_BATCH_XY_ST = 5 (XY positions + ST texture coords)
        GPU_TriangleBatch(gpu_image, target, 4, values, 6, indices, 5);
        
    } else {
        // --- SOLID / PORTAL ---
    
        if (texture_id == -1) {
            // Portal Blue
            drawing_color_r = 0; drawing_color_g = 0; drawing_color_b = 255;
        } else {
            // Wall Red (No Texture Found)
            drawing_color_r = 255; drawing_color_g = 0; drawing_color_b = 0;
        }
        drawing_color_a = 255;
        drawing_blend_mode = 1;

        /* Tri 1: Bottom-Left -> Bottom-Right -> Top-Right */
        draw_triangle_filled(dest, clip, x1, y1_bot, x2, y2_bot, x2, y2_top);
        
        /* Tri 2: Bottom-Left -> Top-Right -> Top-Left */
        draw_triangle_filled(dest, clip, x1, y1_bot, x2, y2_top, x1, y1_top);
    }
}

/* ============================================================================
   RECURSIVE RENDERER
   ============================================================================ */

static void render_sector_gpu_recursive_bennu(GRAPH *dest, int sector_id, REGION *active_clip, int depth) {
    if (depth > 8) return;
    if (sector_id < 0 || sector_id >= g_engine.num_sectors) return;

    RAY_Sector *sector = &g_engine.sectors[sector_id];
    
    int sw = g_engine.displayWidth;
    int sh = g_engine.displayHeight;
    int half_w = sw / 2;
    int half_h = sh / 2;
    int half_dim = sw / 2;
    
    // Render Walls
    for (int w = 0; w < sector->num_walls; w++) {
        RAY_Wall *wall = &sector->walls[w];
        
        // 1. Transform
        vec2_t_gpu p1 = transform_to_camera_gpu(wall->x1, wall->y1);
        vec2_t_gpu p2 = transform_to_camera_gpu(wall->x2, wall->y2);
        
        // 2. Clip behind camera
        if (p1.y <= 0.1f && p2.y <= 0.1f) continue;
        
        float tx1 = p1.x, ty1 = p1.y;
        float tx2 = p2.x, ty2 = p2.y;
        
        if (ty1 <= 0.1f) {
           float t = (0.1f - ty1) / (ty2 - ty1);
           tx1 = tx1 + t * (tx2 - tx1);
           ty1 = 0.1f;
        }
        if (ty2 <= 0.1f) {
           float t = (0.1f - ty2) / (ty1 - ty2);
           tx2 = tx2 + t * (tx1 - tx2);
           ty2 = 0.1f;
        }
        
        // 3. Project
        int sx1 = half_w + (int)(tx1 * half_dim / ty1);
        int sx2 = half_w + (int)(tx2 * half_dim / ty2);
        
        // DEBUG GEOMETRY
        static int debug_geo = 0;
        if (debug_geo < 100) {
            printf("GPU: S=%d W=%d. World(%.1f,%.1f) -> Cam(%.1f,%.1f) -> Screen(%d, %d). Range[%ld-%ld]\n",
                   sector_id, w, wall->x1, wall->y1, p1.x, p1.y, sx1, sx2, 
                   active_clip?active_clip->x:-1, active_clip?active_clip->x2:-1);
        }
        debug_geo++;

        // NOTE: sx1 > sx2 is valid for walls on the side! (Receding)
        // We only cull if it's TRULY degenerate (equal) OR if winding is strict?
        // Let's allow it for now.
        if (sx1 == sx2) continue; 
        
        // Clamp logic to avoid exploding rasterizer
        if (sx1 < -32000) sx1 = -32000;
        if (sx1 >  32000) sx1 =  32000;
        if (sx2 < -32000) sx2 = -32000;
        if (sx2 >  32000) sx2 =  32000;
        
        // 4. Viewport X Clip (Simple check against Region X range)
        if (active_clip) {
            if (sx2 < active_clip->x || sx1 > active_clip->x2) continue;
        }

        // Heights
        float floor_h = sector->floor_z - g_engine.camera.z;
        float ceil_h = sector->ceiling_z - g_engine.camera.z;
        
        int y1_top = half_h - (int)((ceil_h * half_dim) / ty1);
        int y1_bot = half_h - (int)((floor_h * half_dim) / ty1);
        int y2_top = half_h - (int)((ceil_h * half_dim) / ty2);
        int y2_bot = half_h - (int)((floor_h * half_dim) / ty2);
        
        // Portal Logic
        int next_sector = -1;
        if (wall->portal_id != -1 && wall->portal_id < g_engine.num_portals) {
            RAY_Portal *p = &g_engine.portals[wall->portal_id];
            next_sector = (p->sector_a == sector_id) ? p->sector_b : p->sector_a;
        }
        
        if (next_sector != -1) {
            RAY_Sector *n_sect = &g_engine.sectors[next_sector];
            float n_floor_h = n_sect->floor_z - g_engine.camera.z;
            float n_ceil_h = n_sect->ceiling_z - g_engine.camera.z;
            
            int ny1_top = half_h - (int)((n_ceil_h * half_dim) / ty1);
            int ny1_bot = half_h - (int)((n_floor_h * half_dim) / ty1);
            int ny2_top = half_h - (int)((n_ceil_h * half_dim) / ty2);
            int ny2_bot = half_h - (int)((n_floor_h * half_dim) / ty2);
            
            // Steps
            if (n_ceil_h < ceil_h) {
                 draw_wall_quad_bennu(dest, active_clip, sx1, y1_top, ny1_top, sx2, y2_top, ny2_top, wall->texture_id_upper);
            }
            if (n_floor_h > floor_h) {
                 draw_wall_quad_bennu(dest, active_clip, sx1, ny1_bot, y1_bot, sx2, ny2_bot, y2_bot, wall->texture_id_lower);
            }
            
            // Clip for Next Sector
            int cx1 = (active_clip) ? ((sx1 < active_clip->x) ? active_clip->x : sx1) : sx1;
            int cx2 = (active_clip) ? ((sx2 > active_clip->x2) ? active_clip->x2 : sx2) : sx2;
            
            if (cx2 > cx1) {
                // Construct new clip region
                // Inherit Y clip from parent? Or full height?
                // Portals are technically constrained by Y...
                // Passing new region.
                // Note: Bennu REGION is usually struct { int x,y,x2,y2 }.
                // We use a local struct copy.
                REGION new_clip;
                new_clip.x = cx1;
                new_clip.x2 = cx2;
                new_clip.y = (active_clip) ? active_clip->y : 0;
                new_clip.y2 = (active_clip) ? active_clip->y2 : sh;
                
                render_sector_gpu_recursive_bennu(dest, next_sector, &new_clip, depth + 1);
            }
        } else {
            draw_wall_quad_bennu(dest, active_clip, sx1, y1_top, y1_bot, sx2, y2_top, y2_bot, wall->texture_id_middle);
        }
    }
}

void ray_render_frame_gpu(void *dest_graph_ptr) {
    GRAPH *dest = (GRAPH*)dest_graph_ptr;
    
    // Setup Root Clip (Full Screen)
    REGION root_clip;
    root_clip.x = 0;
    root_clip.y = 0;
    root_clip.x2 = g_engine.displayWidth;
    root_clip.y2 = g_engine.displayHeight;
    
    if (g_engine.camera.current_sector_id >= 0) {
        render_sector_gpu_recursive_bennu(dest, g_engine.camera.current_sector_id, &root_clip, 0);
    }
}
