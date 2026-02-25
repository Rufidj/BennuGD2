#include "libmod_ray.h"
#include "libmod_ray_md3.h"
#include <math.h>
#include <stdlib.h>

extern RAY_Engine g_engine;
extern float *g_zbuffer;

/* --- SOFTWARE RASTERIZER (MD3 Variant) --- */

typedef struct {
  float x, dx;
  float inv_z, d_inv_z;
  float u_over_z, du_over_z;
  float v_over_z, dv_over_z;
} EdgeMD3;

static void setup_edge_md3(EdgeMD3 *edge, RAY_Point *p1, RAY_Point *p2,
                           float z1, float z2, float u1, float v1, float u2,
                           float v2) {
  float dy = p2->y - p1->y;
  edge->x = p1->x;
  float iz1 = 1.0f / (z1 < 0.1f ? 0.1f : z1);
  float iz2 = 1.0f / (z2 < 0.1f ? 0.1f : z2);
  edge->inv_z = iz1;
  edge->u_over_z = u1 * iz1;
  edge->v_over_z = v1 * iz1;
  if (dy >= 1.0f) {
    float i_dy = 1.0f / dy;
    edge->dx = (p2->x - p1->x) * i_dy;
    edge->d_inv_z = (iz2 - iz1) * i_dy;
    edge->du_over_z = (u2 * iz2 - u1 * iz1) * i_dy;
    edge->dv_over_z = (v2 * iz2 - v1 * iz1) * i_dy;
  } else {
    edge->dx = 0;
    edge->d_inv_z = 0;
    edge->du_over_z = 0;
    edge->dv_over_z = 0;
  }
}

static void rasterize_scanline_md3(GRAPH *dest, int y, EdgeMD3 *left,
                                   EdgeMD3 *right, int textureID) {
  int x1 = (int)ceilf(left->x), x2 = (int)ceilf(right->x);
  int iw = g_engine.internalWidth;
  if (x1 < 0)
    x1 = 0;
  if (x2 > iw)
    x2 = iw;
  if (x1 >= x2)
    return;
  float span = right->x - left->x;
  if (span < 1.0f)
    span = 1.0f;
  float i_span = 1.0f / span;
  float tiz = (right->inv_z - left->inv_z) * i_span,
        tuz = (right->u_over_z - left->u_over_z) * i_span,
        tvz = (right->v_over_z - left->v_over_z) * i_span;
  float pre = (float)x1 - left->x;
  float iz = left->inv_z + tiz * pre, uz = left->u_over_z + tuz * pre,
        vz = left->v_over_z + tvz * pre;
  GRAPH *tex = textureID > 0 ? (bitmap_get(0, textureID)
                                    ? bitmap_get(0, textureID)
                                    : bitmap_get(g_engine.fpg_id, textureID))
                             : NULL;
  for (int x = x1; x < x2; x++) {
    int idx = y * iw + x;
    float z = 1.0f / (iz > 0.000001f ? iz : 0.000001f);
    if (z < g_zbuffer[idx] - 0.1f) {
      uint32_t color = 0xAA00AA;
      if (tex) {
        int tx = (int)(uz * z * (float)tex->width) % tex->width;
        if (tx < 0)
          tx += tex->width;
        int ty = (int)(vz * z * (float)tex->height) % tex->height;
        if (ty < 0)
          ty += tex->height;
        color = gr_get_pixel(tex, tx, ty);
      }
      if ((color & 0xFF000000) != 0 || (color & 0xFFFFFF) != 0) {
        gr_put_pixel(dest, x, y, color);
        g_zbuffer[idx] = z;
      }
    }
    iz += tiz;
    uz += tuz;
    vz += tvz;
  }
}

static void draw_triangle_md3(GRAPH *dest, RAY_Point p1, RAY_Point p2,
                              RAY_Point p3, float u1, float v1, float u2,
                              float v2, float u3, float v3, float z1, float z2,
                              float z3, int textureID) {
  RAY_Point *top = &p1, *mid = &p2, *bot = &p3;
  float *zt = &z1, *zm = &z2, *zb = &z3;
  float ut = u1, vt = v1, um = u2, vm = v2, ub = u3, vb = v3;
  if (mid->y < top->y) {
    RAY_Point *t = top;
    top = mid;
    mid = t;
    float *z = zt;
    zt = zm;
    zm = z;
    float u = ut;
    ut = um;
    um = u;
    float v = vt;
    vt = vm;
    vm = v;
  }
  if (bot->y < top->y) {
    RAY_Point *t = top;
    top = bot;
    bot = t;
    float *z = zt;
    zt = zb;
    zb = z;
    float u = ut;
    ut = ub;
    ub = u;
    float v = vt;
    vt = vb;
    vb = v;
  }
  if (bot->y < mid->y) {
    RAY_Point *t = mid;
    mid = bot;
    bot = t;
    float *z = zm;
    zm = zb;
    zb = z;
    float u = um;
    um = ub;
    ub = u;
    float v = vm;
    vm = vb;
    vb = v;
  }
  if (bot->y == top->y)
    return;
  EdgeMD3 e1, e2, e3;
  setup_edge_md3(&e1, top, bot, *zt, *zb, ut, vt, ub, vb);
  setup_edge_md3(&e2, top, mid, *zt, *zm, ut, vt, um, vm);
  setup_edge_md3(&e3, mid, bot, *zm, *zb, um, vm, ub, vb);
  for (int y = (int)ceilf(top->y); y < (int)ceilf(mid->y); y++) {
    rasterize_scanline_md3(dest, y, &e1, &e2, textureID);
    e1.x += e1.dx;
    e1.inv_z += e1.d_inv_z;
    e1.u_over_z += e1.du_over_z;
    e1.v_over_z += e1.dv_over_z;
    e2.x += e2.dx;
    e2.inv_z += e2.d_inv_z;
    e2.u_over_z += e2.du_over_z;
    e2.v_over_z += e2.dv_over_z;
  }
  for (int y = (int)ceilf(mid->y); y < (int)ceilf(bot->y); y++) {
    rasterize_scanline_md3(dest, y, &e1, &e3, textureID);
    e1.x += e1.dx;
    e1.inv_z += e1.d_inv_z;
    e1.u_over_z += e1.du_over_z;
    e1.v_over_z += e1.dv_over_z;
    e3.x += e3.dx;
    e3.inv_z += e3.d_inv_z;
    e3.u_over_z += e3.du_over_z;
    e3.v_over_z += e3.dv_over_z;
  }
}

void ray_render_md3(GRAPH *dest, RAY_Sprite *sprite) {
  if (!sprite || !sprite->model)
    return;
  RAY_MD3_Model *model = (RAY_MD3_Model *)sprite->model;
  float cs_cam = cosf(g_engine.camera.rot), sn_cam = sinf(g_engine.camera.rot);
  float cs_mod = cosf(sprite->rot), sn_mod = sinf(sprite->rot);
  int iw = g_engine.internalWidth, ih = g_engine.internalHeight;
  /* ANCHORED FOCAL LENGTH: Syced with Build Engine Walls */
  float focal = (float)iw * 0.5f;
  float hx = (float)iw * 0.5f;
  float hy = (float)ih * 0.5f + g_engine.camera.pitch;
  float scale = sprite->model_scale > 0 ? sprite->model_scale : 1.0f;
  float interp = sprite->interpolation;

  for (int s = 0; s < model->header.numSurfaces; s++) {
    RAY_MD3_Surface *surf = &model->surfaces[s];
    int f1 = sprite->currentFrame % surf->header.numFrames;
    int f2 = sprite->nextFrame % surf->header.numFrames;
    md3_vertex_t *v1 = &surf->vertices[f1 * surf->header.numVerts];
    md3_vertex_t *v2 = &surf->vertices[f2 * surf->header.numVerts];

    RAY_Point s_verts[surf->header.numVerts];
    float d_verts[surf->header.numVerts];
    uint8_t valid[surf->header.numVerts];
    for (int i = 0; i < surf->header.numVerts; i++) {
      float lx =
          (v1[i].x + interp * (v2[i].x - v1[i].x)) * MD3_XYZ_SCALE * scale;
      float ly =
          (v1[i].y + interp * (v2[i].y - v1[i].y)) * MD3_XYZ_SCALE * scale;
      float lz =
          (v1[i].z + interp * (v2[i].z - v1[i].z)) * MD3_XYZ_SCALE * scale;

      /* Rotation and Pos (Reference Code Logic) */
      float rx = lx * cs_mod - ly * sn_mod;
      float ry = lx * sn_mod + ly * cs_mod;
      float dx = rx + sprite->x - g_engine.camera.x;
      float dy = ry + sprite->y - g_engine.camera.y;
      float dz = lz + sprite->z - g_engine.camera.z;

      /* SYNCED TRANSFORM (Reference and Build Engine match) */
      float tz = dx * cs_cam + dy * sn_cam;
      float tx = -dx * sn_cam + dy * cs_cam;

      if (tz < 1.0f) {
        valid[i] = 0;
        continue;
      }
      valid[i] = 1;
      d_verts[i] = tz;
      s_verts[i].x = hx + (tx * focal / tz);
      s_verts[i].y = hy - (dz * focal / tz);
    }
    int tID = (s < 32 && sprite->md3_surface_textures[s] > 0)
                  ? sprite->md3_surface_textures[s]
                  : (surf->textureID ? surf->textureID : model->textureID);
    for (int i = 0; i < surf->header.numTriangles; i++) {
      int i1 = surf->triangles[i].indexes[0],
          i2 = surf->triangles[i].indexes[1],
          i3 = surf->triangles[i].indexes[2];
      if (!valid[i1] || !valid[i2] || !valid[i3])
        continue;
      // Front face only? Reference disables this for some models.
      // float cp = (s_verts[i2].x - s_verts[i1].x) * (s_verts[i3].y -
      // s_verts[i1].y) - (s_verts[i2].y - s_verts[i1].y) * (s_verts[i3].x -
      // s_verts[i1].x); if (cp <= 0) continue;
      draw_triangle_md3(dest, s_verts[i1], s_verts[i2], s_verts[i3],
                        surf->texCoords[i1].s, surf->texCoords[i1].t,
                        surf->texCoords[i2].s, surf->texCoords[i2].t,
                        surf->texCoords[i3].s, surf->texCoords[i3].t,
                        d_verts[i1], d_verts[i2], d_verts[i3], tID);
    }
  }
}
