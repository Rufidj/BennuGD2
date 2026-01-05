#include "libmod_ray.h"
#include "libmod_ray_md3.h"
#include <math.h>

extern RAY_Engine g_engine;
extern float *g_zbuffer;
extern int g_zbuffer_size;

#define MD3_XYZ_SCALE (1.0f / 64.0f)

/* Triangle Rasterizer Helper (Adapted for MD3) */
static void draw_triangle_md3(GRAPH *dest, RAY_Point p1, RAY_Point p2, RAY_Point p3, 
                              float u1, float v1, float u2, float v2, float u3, float v3,
                              float z1, float z2, float z3,
                              int textureID);

/* Main MD3 Render Function */
void ray_render_md3(GRAPH *dest, RAY_Sprite *sprite) {
    if (!sprite || !sprite->model) return;
    
    // Check Magic Number to confirm MD3
    int *magic = (int*)sprite->model;
    if (*magic != MD3_MAGIC) return;
    
    RAY_MD3_Model *model = (RAY_MD3_Model*)sprite->model;
    
    // Transform Setup
    float cos_cam = cosf(g_engine.camera.rot);
    float sin_cam = sinf(g_engine.camera.rot);
    float cos_model = cosf(sprite->rot);
    float sin_model = sinf(sprite->rot);
    
    int half_w = g_engine.displayWidth / 2;
    int half_h = g_engine.displayHeight / 2;
    
    // Buffers for transformed vertices (Reused per surface)
    static RAY_Point screen_verts[MD3_MAX_VERTICES];
    static float depth_verts[MD3_MAX_VERTICES];
    static int valid_verts[MD3_MAX_VERTICES];
    
    // Loop Surfaces
    for (int s = 0; s < model->header.numSurfaces; s++) {
        RAY_MD3_Surface *surf = &model->surfaces[s];
        
        int frame1 = sprite->currentFrame;
        int frame2 = sprite->nextFrame;
        float interp = sprite->interpolation;
        
        // Clamp frames
        if (frame1 >= surf->header.numFrames) frame1 = surf->header.numFrames - 1;
        if (frame2 >= surf->header.numFrames) frame2 = surf->header.numFrames - 1;
        
        md3_vertex_t *verts1 = &surf->vertices[frame1 * surf->header.numVerts];
        md3_vertex_t *verts2 = &surf->vertices[frame2 * surf->header.numVerts];
        
        // Transform Vertices
        for (int i = 0; i < surf->header.numVerts; i++) {
             // Decompress and Interpolate
             float x1 = verts1[i].x * MD3_XYZ_SCALE;
             float y1 = verts1[i].y * MD3_XYZ_SCALE;
             float z1 = verts1[i].z * MD3_XYZ_SCALE;
             
             float x2 = verts2[i].x * MD3_XYZ_SCALE;
             float y2 = verts2[i].y * MD3_XYZ_SCALE;
             float z2 = verts2[i].z * MD3_XYZ_SCALE;
             
             float lx = x1 + interp * (x2 - x1);
             float ly = y1 + interp * (y2 - y1);
             float lz = z1 + interp * (z2 - z1);
             
             // Apply model scale (default 1.0 if not set)
             float scale_factor = (sprite->model_scale > 0.0f) ? sprite->model_scale : 1.0f;
             lx *= scale_factor;
             ly *= scale_factor;
             lz *= scale_factor;
             
             // Model -> World
             float wx = lx * cos_model - ly * sin_model + sprite->x;
             float wy = lx * sin_model + ly * cos_model + sprite->y;
             float wz = lz + sprite->z;
             
             // World -> Camera
             float dx = wx - g_engine.camera.x;
             float dy = wy - g_engine.camera.y;
             float dz = wz - g_engine.camera.z + 16.0f;
             
             float cam_z_depth = dx * cos_cam + dy * sin_cam;
             float cam_x_lat = -dx * sin_cam + dy * cos_cam;
             float cam_y_vert = dz;
             
             // Project
             if (cam_z_depth < 1.0f) {
                 valid_verts[i] = 0;
                 continue;
             }
             
             valid_verts[i] = 1;
             depth_verts[i] = cam_z_depth;
             
             float scale = g_engine.viewDist / cam_z_depth;
             screen_verts[i].x = half_w + (cam_x_lat * scale);
             screen_verts[i].y = half_h - (cam_y_vert * scale);
        }
        
        // Draw Triangles
        int use_texture = model->textureID; // Default to model texture
        if (surf->textureID > 0) use_texture = surf->textureID; // Override per surface?
        // Note: MD3 has shaders inside file, but we usually ignore them in software render or map them to Bennu IDs.
        // Assuming sprite->model->textureID is set by user as the Skin.
        // If sprite->textureID is set (on Sprite struct), use that?
        if (sprite->textureID > 0) use_texture = sprite->textureID;
        
        for (int i = 0; i < surf->header.numTriangles; i++) {
            int idx1 = surf->triangles[i].indexes[0];
            int idx2 = surf->triangles[i].indexes[1];
            int idx3 = surf->triangles[i].indexes[2];
            
            if (!valid_verts[idx1] || !valid_verts[idx2] || !valid_verts[idx3]) continue;
            
            // Backface Culling
            RAY_Point p1 = screen_verts[idx1];
            RAY_Point p2 = screen_verts[idx2];
            RAY_Point p3 = screen_verts[idx3];
            
            // Backface Culling disabled - some models have inconsistent normals
            // float cross = (p2.x - p1.x) * (p3.y - p1.y) - (p2.y - p1.y) * (p3.x - p1.x);
            // if (cross <= 0) continue;
            
            // Texture Coords (Directly from array)
            // MD3 UVs are usually 0.0-1.0. We need to scale by texture size later in rasterizer.
            // The rasterizer expects them.
            
            float u1 = surf->texCoords[idx1].s;
            float v1 = surf->texCoords[idx1].t;
            float u2 = surf->texCoords[idx2].s;
            float v2 = surf->texCoords[idx2].t;
            float u3 = surf->texCoords[idx3].s;
            float v3 = surf->texCoords[idx3].t;
            
            draw_triangle_md3(dest, p1, p2, p3, u1, v1, u2, v2, u3, v3, 
                             depth_verts[idx1], depth_verts[idx2], depth_verts[idx3],
                             use_texture);
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Rasterizer Implementation (Simplified for brevity, same logic as MD2) */

typedef struct {
    float x, dx;
    float inv_z, d_inv_z;
    float u_over_z, du_over_z;
    float v_over_z, dv_over_z;
} Edge;

static void rasterize_scanline_md3(GRAPH *dest, int y, Edge *left, Edge *right, int textureID) {
    int x_start = (int)ceilf(left->x);
    int x_end = (int)ceilf(right->x);
    if (x_start < 0) x_start = 0;
    if (x_end >= g_engine.displayWidth) x_end = g_engine.displayWidth; 
    if (x_start >= x_end) return;
    
    float span = right->x - left->x;
    if (span < 1.0f) span = 1.0f;
    float inv_span = 1.0f / span;
    
    float d_inv_z = (right->inv_z - left->inv_z) * inv_span;
    float d_u_over_z = (right->u_over_z - left->u_over_z) * inv_span;
    float d_v_over_z = (right->v_over_z - left->v_over_z) * inv_span;
    
    float pre = (float)x_start - left->x;
    float iz = left->inv_z + d_inv_z * pre;
    float uz = left->u_over_z + d_u_over_z * pre;
    float vz = left->v_over_z + d_v_over_z * pre;
    
    GRAPH *tex = NULL;
    if (textureID > 0) {
        tex = bitmap_get(0, textureID);
        if (!tex) tex = bitmap_get(g_engine.fpg_id, textureID);
    }
    
    for (int x = x_start; x < x_end; x++) {
        int idx = y * g_engine.displayWidth + x;
        float z = (iz > 0.000001f) ? (1.0f / iz) : 100000.0f;
        
        if (z < g_zbuffer[idx]) {
            uint32_t color = 0xFFFF00FF;
            if (tex) {
                float u = uz * z;
                float v = vz * z;
                
                int tx = (int)(u * tex->width); // Scale UV (0-1) to pixels
                int ty = (int)(v * tex->height);
                
                // Wrap
                tx %= tex->width; if(tx<0) tx+=tex->width;
                ty %= tex->height; if(ty<0) ty+=tex->height;
                
                color = gr_get_pixel(tex, tx, ty);
            }
            if ((color & 0xFF000000) != 0) {
                gr_put_pixel(dest, x, y, color);
                g_zbuffer[idx] = z;
            }
        }
        iz += d_inv_z; uz += d_u_over_z; vz += d_v_over_z;
    }
}

static void setup_edge_md3(Edge *e, RAY_Point *p1, RAY_Point *p2, float z1, float z2, float u1, float v1, float u2, float v2) {
    float dy = p2->y - p1->y;
    e->x = p1->x;
    float iz1 = 1.0f / ((z1<0.1f)?0.1f:z1);
    float iz2 = 1.0f / ((z2<0.1f)?0.1f:z2);
    
    e->inv_z = iz1;
    e->u_over_z = u1 * iz1;
    e->v_over_z = v1 * iz1;
    
    if (dy >= 1.0f) {
        float idy = 1.0f / dy;
        e->dx = (p2->x - p1->x) * idy;
        e->d_inv_z = (iz2 - iz1) * idy;
        e->du_over_z = (u2 * iz2 - u1 * iz1) * idy;
        e->dv_over_z = (v2 * iz2 - v1 * iz1) * idy;
    } else {
        e->dx = 0; e->d_inv_z = 0; e->du_over_z = 0; e->dv_over_z = 0;
    }
}

static void draw_triangle_md3(GRAPH *dest, RAY_Point p1, RAY_Point p2, RAY_Point p3, 
                              float u1, float v1, float u2, float v2, float u3, float v3,
                              float z1, float z2, float z3,
                              int textureID) 
{
    // Sort by Y
    RAY_Point *top=&p1, *mid=&p2, *bot=&p3;
    float *zt=&z1, *zm=&z2, *zb=&z3;
    float ut=u1, vt=v1, um=u2, vm=v2, ub=u3, vb=v3;
    
    // Bubble sort 3 items (ugly but works)
#define SWAP(a,b) { RAY_Point *t=a; a=b; b=t; }
#define SWAPF(a,b) { float t=a; a=b; b=t; }
    if (mid->y < top->y) { SWAP(top,mid); SWAPF(*zt,*zm); SWAPF(ut,um); SWAPF(vt,vm); }
    if (bot->y < top->y) { SWAP(top,bot); SWAPF(*zt,*zb); SWAPF(ut,ub); SWAPF(vt,vb); }
    if (bot->y < mid->y) { SWAP(mid,bot); SWAPF(*zm,*zb); SWAPF(um,ub); SWAPF(vm,vb); }

    int y_start = (int)ceilf(top->y);
    int y_end = (int)ceilf(bot->y);
    int y_mid = (int)ceilf(mid->y);
    if (y_start >= g_engine.displayHeight || y_end < 0) return;  // FIXED: was displayWidth
    
    Edge long_e, short_e;
    setup_edge_md3(&long_e, top, bot, *zt, *zb, ut, vt, ub, vb);
    
    // Top-Mid
    setup_edge_md3(&short_e, top, mid, *zt, *zm, ut, vt, um, vm);
    
    // Pre-step
    float pre = (float)y_start - top->y;
    long_e.x += long_e.dx * pre; long_e.inv_z += long_e.d_inv_z * pre; long_e.u_over_z += long_e.du_over_z * pre; long_e.v_over_z += long_e.dv_over_z * pre;
    short_e.x += short_e.dx * pre; short_e.inv_z += short_e.d_inv_z * pre; short_e.u_over_z += short_e.du_over_z * pre; short_e.v_over_z += short_e.dv_over_z * pre;

    for (int y=y_start; y<y_mid; y++) {
        if (y>=0 && y<g_engine.displayHeight) {
            if (short_e.x < long_e.x) rasterize_scanline_md3(dest, y, &short_e, &long_e, textureID);
            else rasterize_scanline_md3(dest, y, &long_e, &short_e, textureID);
        }
        short_e.x += short_e.dx; short_e.inv_z += short_e.d_inv_z; short_e.u_over_z += short_e.du_over_z; short_e.v_over_z += short_e.dv_over_z;
        long_e.x += long_e.dx; long_e.inv_z += long_e.d_inv_z; long_e.u_over_z += long_e.du_over_z; long_e.v_over_z += long_e.dv_over_z;
    }
    
    // Mid-Bot
    setup_edge_md3(&short_e, mid, bot, *zm, *zb, um, vm, ub, vb);
    float pre_m = (float)y_mid - mid->y;
    short_e.x += short_e.dx * pre_m; short_e.inv_z += short_e.d_inv_z * pre_m; short_e.u_over_z += short_e.du_over_z * pre_m; short_e.v_over_z += short_e.dv_over_z * pre_m;
    
    // Fix long edge
    if (y_start != y_mid) {
        float d = (float)y_mid - top->y;
        setup_edge_md3(&long_e, top, bot, *zt, *zb, ut, vt, ub, vb);
        long_e.x += long_e.dx * d; long_e.inv_z += long_e.d_inv_z * d; long_e.u_over_z += long_e.du_over_z * d; long_e.v_over_z += long_e.dv_over_z * d;
    }
    
    for (int y=y_mid; y<y_end; y++) {
        if (y>=0 && y<g_engine.displayHeight) {
             if (short_e.x < long_e.x) rasterize_scanline_md3(dest, y, &short_e, &long_e, textureID);
             else rasterize_scanline_md3(dest, y, &long_e, &short_e, textureID);
        }
        short_e.x += short_e.dx; short_e.inv_z += short_e.d_inv_z; short_e.u_over_z += short_e.du_over_z; short_e.v_over_z += short_e.dv_over_z;
        long_e.x += long_e.dx; long_e.inv_z += long_e.d_inv_z; long_e.u_over_z += long_e.du_over_z; long_e.v_over_z += long_e.dv_over_z;
    }
}
