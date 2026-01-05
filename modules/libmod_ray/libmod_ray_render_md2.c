#include "libmod_ray.h"
#include "libmod_ray_md2.h"
#include <math.h>

extern RAY_Engine g_engine;
extern float *g_zbuffer; // Defined in render_build.c
extern int g_zbuffer_size;

/* Triangle Rasterizer Helper */
static void draw_triangle_md2(GRAPH *dest, RAY_Point p1, RAY_Point p2, RAY_Point p3, 
                              md2_texCoord_t t1, md2_texCoord_t t2, md2_texCoord_t t3,
                              float z1, float z2, float z3,
                              int textureID);

/* Main MD2 Render Function */
void ray_render_md2(GRAPH *dest, RAY_Sprite *sprite) {
    if (!sprite || !sprite->model) return;
    
    RAY_MD2_Model *model = (RAY_MD2_Model*)sprite->model;
    
    /* 1. Interpolate Vertices */
    /* We need to transform all vertices for the current frame state */
    /* Optimization: Only transform vertices used by visible triangles? 
       For now, transform all (MD2s are small, ~500 verts max usually) */
       
    // Local buffers for transformed vertices
    // We use a static buffer to avoid malloc per frame, assuming single threaded
    static RAY_Point screen_verts[MD2_MAX_VERTICES];
    static float depth_verts[MD2_MAX_VERTICES];
    static int valid_verts[MD2_MAX_VERTICES]; // 1 if in front of camera
    
    // Camera transform pre-calc
    float cos_cam = cosf(g_engine.camera.rot);
    float sin_cam = sinf(g_engine.camera.rot);
    
    // Model transform
    // Sprite x,y,z is the center/base. MD2 coordinates are local.
    // We also need rotation. Sprite->rot.
    float cos_model = cosf(sprite->rot);
    float sin_model = sinf(sprite->rot);
    
    // Combined rotation?
    // World X = ModelX * cosM - ModelY * sinM + SpriteX
    // Then World -> Camera -> Screen
    
    int num_verts = model->header.numVertices;
    
    for (int i = 0; i < num_verts; i++) {
        vec3_t v_local;
        ray_md2_interpolate_vertex(model, sprite->currentFrame, sprite->nextFrame, sprite->interpolation, i, &v_local);
        
        // 1. Model -> World Rotation + Translation
        // Note: MD2 usually has Z as up. Ray Engine also has Z as up (mostly).
        // Let's assume standard orientation.
        // Rotation around Z axis (sprite->rot)
        float wx = v_local.x * cos_model - v_local.y * sin_model + sprite->x;
        float wy = v_local.x * sin_model + v_local.y * cos_model + sprite->y;
        float wz = v_local.z + sprite->z; // + Z offset
        
        // 2. World -> Camera
        float dx = wx - g_engine.camera.x;
        float dy = wy - g_engine.camera.y;
        float dz = wz - g_engine.camera.z + 16.0f; // Eye level adjustment?
        
        // Rotate by Camera Angle
        // Camera Forward is X+? Or standard Raycasting?
        // In this engine (Build port):
        // Transform to camera space
        // result.x = (dx * cos_rot + dy * sin_rot);   // Forward (depth)
        // result.y = (-dx * sin_rot + dy * cos_rot);  // Right (lateral)
        
        float cam_z_depth = dx * cos_cam + dy * sin_cam;
        float cam_x_lat = -dx * sin_cam + dy * cos_cam;
        float cam_y_vert = dz; // Z is vertical in world, so it maps to Y in screen... no wait.
        
        // 3. Project to Screen
        // Standard projection: x_screen = center + (x_lat * fov / z_depth)
        // y_screen = center - (y_vert * fov / z_depth)
        
        if (cam_z_depth < 1.0f) {
            valid_verts[i] = 0;
            continue;
        }
        
        valid_verts[i] = 1;
        depth_verts[i] = cam_z_depth;
        
        // Use engine projection constants
        int half_w = g_engine.displayWidth / 2;
        int half_h = g_engine.displayHeight / 2;
        // viewDist is usually half_width for 90 deg FOV
        float scale = g_engine.viewDist / cam_z_depth;
        
        screen_verts[i].x = half_w + (cam_x_lat * scale);
        // In Build engine, Z grows down? NO, Z is height.
        // Screen Y grows down.
        // If cam_y_vert is positive (above camera), it should be UP on screen (smaller Y)
        // y_screen = half_h - (height * scale)
        screen_verts[i].y = half_h - (cam_y_vert * scale);
    }
    
    /* 2. Draw Triangles */
    int num_tris = model->header.numTriangles;
    for (int i = 0; i < num_tris; i++) {
        int idx1 = model->triangles[i].vertexIndices[0];
        int idx2 = model->triangles[i].vertexIndices[1];
        int idx3 = model->triangles[i].vertexIndices[2];
        
        // Clipping: If any vertex is invalid (behind camera), skip triangle?
        // Ideally should clip against Z-plane, but for now simple rejection
        if (!valid_verts[idx1] || !valid_verts[idx2] || !valid_verts[idx3]) continue;
        
        // Backface Culling
        // Calculate signed area or normal. 
        RAY_Point p1 = screen_verts[idx1];
        RAY_Point p2 = screen_verts[idx2];
        RAY_Point p3 = screen_verts[idx3];
        
        float cross_product = (p2.x - p1.x) * (p3.y - p1.y) - (p2.y - p1.y) * (p3.x - p1.x);
        if (cross_product <= 0) continue; // Assume Counter-Clockwise winding
        
        // Texture Coords
        md2_texCoord_t t1 = model->texCoords[model->triangles[i].textureIndices[0]];
        md2_texCoord_t t2 = model->texCoords[model->triangles[i].textureIndices[1]];
        md2_texCoord_t t3 = model->texCoords[model->triangles[i].textureIndices[2]];
        
        draw_triangle_md2(dest, p1, p2, p3, t1, t2, t3, 
                         depth_verts[idx1], depth_verts[idx2], depth_verts[idx3],
                         model->textureID);
    }
}

/* Edge structure for rasterizer */
typedef struct {
    float x, dx;
    float inv_z, d_inv_z;     // 1/z
    float u_over_z, du_over_z; // u/z
    float v_over_z, dv_over_z; // v/z
} Edge;

static void rasterize_scanline(GRAPH *dest, int y, Edge *left, Edge *right, int textureID) {
    int x_start = (int)ceilf(left->x);
    int x_end = (int)ceilf(right->x);
    
    // Safety clamp
    if (x_start < 0) x_start = 0;
    if (x_end >= g_engine.displayWidth) x_end = g_engine.displayWidth; 
    if (x_start >= x_end) return;
    
    float span_width = right->x - left->x;
    if (span_width < 1.0f) span_width = 1.0f;
    float inv_span = 1.0f / span_width;
    
    // Interpolate across span using perspective correct values
    float d_inv_z = (right->inv_z - left->inv_z) * inv_span;
    float d_u_over_z = (right->u_over_z - left->u_over_z) * inv_span;
    float d_v_over_z = (right->v_over_z - left->v_over_z) * inv_span;
    
    // Adjust start values based on sub-pixel correction (ceilf)
    float prestep = (float)x_start - left->x;
    float curr_inv_z = left->inv_z + d_inv_z * prestep;
    float curr_u_over_z = left->u_over_z + d_u_over_z * prestep;
    float curr_v_over_z = left->v_over_z + d_v_over_z * prestep;
    
    GRAPH *tex = NULL;
    if (textureID > 0) {
        // Try file 0 first (User Loaded Graphics), then FPG
        tex = bitmap_get(0, textureID);
        if (!tex) tex = bitmap_get(g_engine.fpg_id, textureID);
    }
    
    for (int x = x_start; x < x_end; x++) {
        // Z-Buffer Check
        int pixel_idx = y * g_engine.displayWidth + x;
        
        // Recover true Z for depth test
        // inv_z = 1/z -> z = 1/inv_z
        // Avoid division by zero
        // Small epsilon
        float z = (curr_inv_z > 0.000001f) ? (1.0f / curr_inv_z) : 100000.0f;
        
        if (z < g_zbuffer[pixel_idx]) {
            uint32_t color = 0xFFFF00FF; // Fallback pink
            
            if (tex) {
                // Perspective Correction:
                // u = (u/z) / (1/z)
                // v = (v/z) / (1/z)
                float u = curr_u_over_z * z;
                float v = curr_v_over_z * z;
                
                int tx = (int)u;
                int ty = (int)v;
                
                // Wrap or Clamp
                tx %= tex->width; if(tx<0) tx+=tex->width;
                ty %= tex->height; if(ty<0) ty+=tex->height;
                
                color = gr_get_pixel(tex, tx, ty);
            }
            
            if ((color & 0xFF000000) != 0) { // Alpha check
                 gr_put_pixel(dest, x, y, color);
                 g_zbuffer[pixel_idx] = z;
            }
        }
        
        curr_inv_z += d_inv_z;
        curr_u_over_z += d_u_over_z;
        curr_v_over_z += d_v_over_z;
    }
}

/* Helper to setup edge with perspective correct attribs */
static void setup_edge(Edge *edge, RAY_Point *p1, RAY_Point *p2, float z1, float z2, 
                       md2_texCoord_t *t1, md2_texCoord_t *t2) 
{
    float dy = p2->y - p1->y;
    edge->x = p1->x;
    
    // Perspective attributes at vertices
    float inv_z1 = 1.0f / ((z1 < 0.1f) ? 0.1f : z1);
    float inv_z2 = 1.0f / ((z2 < 0.1f) ? 0.1f : z2);
    
    float uz1 = (float)t1->s * inv_z1;
    float vz1 = (float)t1->t * inv_z1;
    float uz2 = (float)t2->s * inv_z2;
    float vz2 = (float)t2->t * inv_z2;
    
    edge->inv_z = inv_z1;
    edge->u_over_z = uz1;
    edge->v_over_z = vz1;
    
    if (dy >= 1.0f) {
        float inv_dy = 1.0f / dy;
        edge->dx = (p2->x - p1->x) * inv_dy;
        edge->d_inv_z = (inv_z2 - inv_z1) * inv_dy;
        edge->du_over_z = (uz2 - uz1) * inv_dy;
        edge->dv_over_z = (vz2 - vz1) * inv_dy;
    } else {
        edge->dx = 0; edge->d_inv_z = 0; edge->du_over_z = 0; edge->dv_over_z = 0;
    }
}

static void draw_triangle_md2(GRAPH *dest, RAY_Point p1, RAY_Point p2, RAY_Point p3, 
                              md2_texCoord_t t1, md2_texCoord_t t2, md2_texCoord_t t3,
                              float z1, float z2, float z3,
                              int textureID) 
{
    // Sort vertices by Y
    RAY_Point *top = &p1, *mid = &p2, *bot = &p3;
    md2_texCoord_t *ttop = &t1, *tmid = &t2, *tbot = &t3;
    float *ztop = &z1, *zmid = &z2, *zbot = &z3;
    
    if (mid->y < top->y) { 
        RAY_Point *tmp=top; top=mid; mid=tmp; 
        md2_texCoord_t *tt=ttop; ttop=tmid; tmid=tt;
        float *tz=ztop; ztop=zmid; zmid=tz;
    }
    if (bot->y < top->y) {
        RAY_Point *tmp=top; top=bot; bot=tmp;
        md2_texCoord_t *tt=ttop; ttop=tbot; tbot=tt;
        float *tz=ztop; ztop=zbot; zbot=tz;
    }
    if (bot->y < mid->y) {
        RAY_Point *tmp=mid; mid=bot; bot=tmp;
        md2_texCoord_t *tt=tmid; tmid=tbot; tbot=tt;
        float *tz=zmid; zmid=zbot; zbot=tz;
    }
    
    int y_start = (int)ceilf(top->y);
    int y_end = (int)ceilf(bot->y);
    int y_mid = (int)ceilf(mid->y);
    
    if (y_start >= g_engine.displayHeight || y_end < 0) return;
    
    // Edges
    Edge long_edge, short_edge;
    
    setup_edge(&long_edge, top, bot, *ztop, *zbot, ttop, tbot);
    
    // Adjust long edge for subpixel start
    float prestep = (float)y_start - top->y;
    long_edge.x += long_edge.dx * prestep;
    long_edge.inv_z += long_edge.d_inv_z * prestep;
    long_edge.u_over_z += long_edge.du_over_z * prestep;
    long_edge.v_over_z += long_edge.dv_over_z * prestep;

    // Top -> Mid
    setup_edge(&short_edge, top, mid, *ztop, *zmid, ttop, tmid);
    short_edge.x += short_edge.dx * prestep;
    short_edge.inv_z += short_edge.d_inv_z * prestep;
    short_edge.u_over_z += short_edge.du_over_z * prestep;
    short_edge.v_over_z += short_edge.dv_over_z * prestep;

    for (int y = y_start; y < y_mid; y++) {
         if (y >= 0 && y < g_engine.displayHeight) {
             if (short_edge.x < long_edge.x) rasterize_scanline(dest, y, &short_edge, &long_edge, textureID);
             else rasterize_scanline(dest, y, &long_edge, &short_edge, textureID);
         }
         short_edge.x += short_edge.dx; short_edge.inv_z += short_edge.d_inv_z; short_edge.u_over_z += short_edge.du_over_z; short_edge.v_over_z += short_edge.dv_over_z;
         long_edge.x += long_edge.dx; long_edge.inv_z += long_edge.d_inv_z; long_edge.u_over_z += long_edge.du_over_z; long_edge.v_over_z += long_edge.dv_over_z;
    }
    
    // Mid -> Bot
    setup_edge(&short_edge, mid, bot, *zmid, *zbot, tmid, tbot);
    float prestep_mid = (float)y_mid - mid->y;
    short_edge.x += short_edge.dx * prestep_mid;
    short_edge.inv_z += short_edge.d_inv_z * prestep_mid;
    short_edge.u_over_z += short_edge.du_over_z * prestep_mid;
    short_edge.v_over_z += short_edge.dv_over_z * prestep_mid;
    
    // IMPORTANT: Fix long edge drift if top loop was skipped or short
    // Simplest way: Recalculate long_edge from top at y_mid
    if (y_start != y_mid) {
        // If we ran the loop, it should be fine, but floating point error can accumulate.
        // For robustness, re-interpolate long_edge to y_mid
        float delta = (float)y_mid - top->y;
        
        // Re-setup to base
        setup_edge(&long_edge, top, bot, *ztop, *zbot, ttop, tbot);
        
        long_edge.x += long_edge.dx * delta;
        long_edge.inv_z += long_edge.d_inv_z * delta;
        long_edge.u_over_z += long_edge.du_over_z * delta;
        long_edge.v_over_z += long_edge.dv_over_z * delta;
    } 

    for (int y = y_mid; y < y_end; y++) {
         if (y >= 0 && y < g_engine.displayHeight) {
             if (short_edge.x < long_edge.x) rasterize_scanline(dest, y, &short_edge, &long_edge, textureID);
             else rasterize_scanline(dest, y, &long_edge, &short_edge, textureID);
         }
         short_edge.x += short_edge.dx; short_edge.inv_z += short_edge.d_inv_z; short_edge.u_over_z += short_edge.du_over_z; short_edge.v_over_z += short_edge.dv_over_z;
         long_edge.x += long_edge.dx; long_edge.inv_z += long_edge.d_inv_z; long_edge.u_over_z += long_edge.du_over_z; long_edge.v_over_z += long_edge.dv_over_z;
    }
}
