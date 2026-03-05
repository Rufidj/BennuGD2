#ifndef __LIBMOD_RAY_MD3_H
#define __LIBMOD_RAY_MD3_H

#include "libmod_ray.h"
#include <stdint.h>

/* MD3 Constants */
#define MD3_MAGIC 860898377 /* "IDP3" */
#define MD3_VERSION 15
#define MD3_XYZ_SCALE (1.0f / 64.0f)
#define MD3_MAX_FRAMES 1024
#define MD3_MAX_TAGS 16
#define MD3_MAX_SURFACES 128
#define MD3_MAX_SHADERS 256
#define MD3_MAX_VERTICES 65536
#define MD3_MAX_TRIANGLES 131072

/* Vector 3D */
typedef struct {
  float x, y, z;
} md3_vec3_t;

/* MD3 File Header */
typedef struct {
  int32_t magic;   /* "IDP3" */
  int32_t version; /* 15 */
  char name[64];   /* Path name */
  int32_t flags;
  int32_t numFrames;
  int32_t numTags;
  int32_t numSurfaces;
  int32_t numSkins; /* Not used typically */
  int32_t offsetFrames;
  int32_t offsetTags;
  int32_t offsetSurfaces;
  int32_t offsetEnd;
} md3_header_t;

/* Frame Data (Bone control) */
typedef struct {
  md3_vec3_t minBounds;
  md3_vec3_t maxBounds;
  md3_vec3_t localOrigin;
  float radius;
  char name[16];
} md3_frame_t;

/* Tag (Attachment point) */
typedef struct {
  char name[64];
  md3_vec3_t origin;
  float axis[3][3]; /* Rotation matrix */
} md3_tag_t;

/* Surface Header (Mesh) */
typedef struct {
  int32_t magic; /* "IDP3" */
  char name[64];
  int32_t flags;
  int32_t numFrames;
  int32_t numShaders;
  int32_t numVerts;
  int32_t numTriangles;
  int32_t offsetTriangles;
  int32_t offsetShaders;
  int32_t offsetTexCoords;
  int32_t offsetXyzNormals;
  int32_t offsetEnd; /* Offset to next surface relative to this surface start */
} md3_surface_header_t;

/* Shader (Texture) */
typedef struct {
  char name[64];
  int32_t shaderIndex;
} md3_shader_t;

/* Triangle */
typedef struct {
  int32_t indexes[3];
} md3_triangle_t;

/* TexCoord */
typedef struct {
  float s, t;
} md3_texCoord_t;

/* Vertex (Compressed) */
typedef struct {
  int16_t x, y, z;
  uint8_t normal[2]; /* Spherical coordinates (zenith, azimuth) encoded */
} md3_vertex_t;

/* ------------------------------------------------------------------------- */
/* Runtime Structures */

/* Loaded Surface */
typedef struct {
  md3_surface_header_t header;
  md3_shader_t *shaders;
  md3_triangle_t *triangles;
  md3_texCoord_t *texCoords;
  md3_vertex_t
      *vertices; /* All frames interleaved: frame0_verts, frame1_verts... */

  /* Pre-calculated / Runtime data */
  int textureID; /* BennuGD texture ID override */
} RAY_MD3_Surface;

/* Runtime Model */
typedef struct RAY_MD3_Model {
  md3_header_t header;
  md3_frame_t *frames;
  md3_tag_t *tags;
  RAY_MD3_Surface *surfaces; /* Array of parsed surfaces */

  /* Runtime Data */
  int textureID; /* Default texture ID for all surfaces if not overridden */
  char name[64];
} RAY_MD3_Model;

/* API Functions */
RAY_MD3_Model *ray_md3_load(const char *filename);
void ray_md3_free(RAY_MD3_Model *model);

/* Rendering Helper (Forward Decl) */
// void ray_md3_interpolate_surface(...);

#endif /* __LIBMOD_RAY_MD3_H */
