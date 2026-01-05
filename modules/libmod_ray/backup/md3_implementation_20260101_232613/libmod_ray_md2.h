#ifndef __LIBMOD_RAY_MD2_H
#define __LIBMOD_RAY_MD2_H

#include <stdint.h>
#include "libmod_ray.h"

/* MD2 Constants */
#define MD2_MAGIC 844121161 /* "IDP2" */
#define MD2_VERSION 8
#define MD2_MAX_TRIANGLES 4096
#define MD2_MAX_VERTICES 2048
#define MD2_MAX_FRAMES 512
#define MD2_MAX_SKINS 32
#define MD2_MAX_SKINNAME 64

/* Vector 3D for MD2 operations */
typedef struct {
    float x, y, z;
} vec3_t;

/* MD2 File Header */
typedef struct {
    int32_t magic;          /* Magic number "IDP2" */
    int32_t version;        /* Version number (must be 8) */
    int32_t skinWidth;      /* Texture width */
    int32_t skinHeight;     /* Texture height */
    int32_t frameSize;      /* Size in bytes of a frame */
    int32_t numSkins;       /* Number of skins */
    int32_t numVertices;    /* Number of vertices per frame */
    int32_t numTexCoords;   /* Number of texture coordinates */
    int32_t numTriangles;   /* Number of triangles */
    int32_t numGlCommands;  /* Number of OpenGL commands */
    int32_t numFrames;      /* Number of frames */
    int32_t offsetSkins;    /* Offset to skin names */
    int32_t offsetTexCoords;/* Offset to s-t texture coordinates */
    int32_t offsetTriangles;/* Offset to triangles */
    int32_t offsetFrames;   /* Offset to frame data */
    int32_t offsetGlCommands;/* Offset to OpenGL commands */
    int32_t offsetEnd;      /* Offset to end of file */
} md2_header_t;

/* Compressed Vertex (as stored in file) */
typedef struct {
    uint8_t v[3];           /* Compressed position (x, y, z) */
    uint8_t lightNormalIndex; /* Index to normal vector */
} md2_vertex_t;

/* Frame Data (as stored in file) */
typedef struct {
    float scale[3];         /* Scale factors */
    float translate[3];     /* Translation vector */
    char name[16];          /* Frame name */
    md2_vertex_t *vertices; /* Array of vertices (dynamic) */
} md2_frame_t;

/* Triangle definition */
typedef struct {
    uint16_t vertexIndices[3]; /* Indices to vertices */
    uint16_t textureIndices[3];/* Indices to texture coordinates */
} md2_triangle_ptr_t;

/* Texture Coordinate */
typedef struct {
    int16_t s, t;
} md2_texCoord_t;

/* Runtime Model Structure */
typedef struct RAY_MD2_Model {
    md2_header_t header;
    md2_frame_t *frames;
    md2_triangle_ptr_t *triangles;
    md2_texCoord_t *texCoords;
    int *glCommands;        /* Not used for software rendering usually */
    
    /* Pre-calculated / Runtime data */
    int textureID;          /* BennuGD texture ID (skin) */
    char name[64];
} RAY_MD2_Model;

/* API Functions */
RAY_MD2_Model *ray_md2_load(const char *filename);
void ray_md2_free(RAY_MD2_Model *model);

/* Helper for vertex decompression */
void ray_md2_interpolate_vertex(RAY_MD2_Model *model, int frame1, int frame2, float interpolation, int vertexIndex, vec3_t *out);

#endif /* __LIBMOD_RAY_MD2_H */
