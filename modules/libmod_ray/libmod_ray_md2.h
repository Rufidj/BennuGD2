#ifndef __LIBMOD_RAY_MD2_H
#define __LIBMOD_RAY_MD2_H

#include "libmod_ray.h"

#define MD2_MAGIC 844121161 /* "IDP2" */
#define MD2_VERSION 8

typedef struct {
  int32_t magic;
  int32_t version;
  int32_t skinWidth;
  int32_t skinHeight;
  int32_t frameSize;
  int32_t numSkins;
  int32_t numVertices;
  int32_t numTexCoords;
  int32_t numTriangles;
  int32_t numGlCommands;
  int32_t numFrames;
  int32_t offsetSkins;
  int32_t offsetTexCoords;
  int32_t offsetTriangles;
  int32_t offsetFrames;
  int32_t offsetGlCommands;
  int32_t offsetEnd;
} md2_header_t;

typedef struct {
  uint8_t v[3];
  uint8_t normalIndex;
} md2_vertex_t;

typedef struct {
  float scale[3];
  float translate[3];
  char name[16];
  md2_vertex_t *vertices;
} md2_frame_t;

typedef struct {
  int16_t s, t;
} md2_texCoord_t;

typedef struct {
  int16_t vertexIndices[3];
  int16_t textureIndices[3];
} md2_triangle_t;

typedef struct {
  md2_header_t header;
  md2_frame_t *frames;
  md2_texCoord_t *texCoords;
  md2_triangle_t *triangles;
  int textureID;
  char name[64];
} RAY_MD2_Model;

/* Vector for interpolation */
typedef struct {
  float x, y, z;
} md2_vec3_t;

RAY_MD2_Model *ray_md2_load(const char *filename);
void ray_md2_free(RAY_MD2_Model *model);
void ray_md2_interpolate_vertex(RAY_MD2_Model *model, int frame1, int frame2,
                                float interpolation, int vertexIndex,
                                md2_vec3_t *out);

#endif
