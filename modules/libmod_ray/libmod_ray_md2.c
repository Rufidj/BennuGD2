#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libmod_ray_md2.h"

RAY_MD2_Model *ray_md2_load(const char *filename) {
    FILE *fp;
    RAY_MD2_Model *model;
    
    fp = fopen(filename, "rb");
    if (!fp) {
        printf("RAY_MD2: Could not open file %s\n", filename);
        return NULL;
    }
    
    /* 1. Read Header */
    model = (RAY_MD2_Model *)malloc(sizeof(RAY_MD2_Model));
    if (!model) {
        fclose(fp);
        return NULL;
    }
    
    (void)fread(&model->header, sizeof(md2_header_t), 1, fp);
    
    if (model->header.magic != MD2_MAGIC || model->header.version != MD2_VERSION) {
        printf("RAY_MD2: Invalid MD2 file %s (Magic: %d, Version: %d)\n", filename, model->header.magic, model->header.version);
        free(model);
        fclose(fp);
        return NULL;
    }
    
    strncpy(model->name, filename, 63);
    model->textureID = 0; // Set later manually
    
    /* 2. Allocate and Read Texture Coordinates */
    model->texCoords = (md2_texCoord_t *)malloc(sizeof(md2_texCoord_t) * model->header.numTexCoords);
    fseek(fp, model->header.offsetTexCoords, SEEK_SET);
    (void)fread(model->texCoords, sizeof(md2_texCoord_t), model->header.numTexCoords, fp);
    
    /* 3. Allocate and Read Triangles */
    model->triangles = (md2_triangle_ptr_t *)malloc(sizeof(md2_triangle_ptr_t) * model->header.numTriangles);
    fseek(fp, model->header.offsetTriangles, SEEK_SET);
    (void)fread(model->triangles, sizeof(md2_triangle_ptr_t), model->header.numTriangles, fp);
    
    /* 4. Allocate and Read Frames */
    model->frames = (md2_frame_t *)malloc(sizeof(md2_frame_t) * model->header.numFrames);
    fseek(fp, model->header.offsetFrames, SEEK_SET);
    
    for (int i = 0; i < model->header.numFrames; i++) {
        /* Read frame header parts: scale (12), translate (12), name (16) = 40 bytes */
        (void)fread(model->frames[i].scale, sizeof(float), 3, fp);
        (void)fread(model->frames[i].translate, sizeof(float), 3, fp);
        (void)fread(model->frames[i].name, sizeof(char), 16, fp);
        
        /* Allocate vertices for this frame */
        model->frames[i].vertices = (md2_vertex_t *)malloc(sizeof(md2_vertex_t) * model->header.numVertices);
        (void)fread(model->frames[i].vertices, sizeof(md2_vertex_t), model->header.numVertices, fp);
    }
    
    /* Done reading */
    fclose(fp);
    
    printf("RAY_MD2: Loaded %s (Frames: %d, Polys: %d)\n", filename, model->header.numFrames, model->header.numTriangles);
    return model;
}

void ray_md2_free(RAY_MD2_Model *model) {
    if (!model) return;
    
    if (model->frames) {
        for (int i = 0; i < model->header.numFrames; i++) {
            if (model->frames[i].vertices) free(model->frames[i].vertices);
        }
        free(model->frames);
    }
    
    if (model->triangles) free(model->triangles);
    if (model->texCoords) free(model->texCoords);
    
    free(model);
}

void ray_md2_interpolate_vertex(RAY_MD2_Model *model, int frame1, int frame2, float interpolation, int vertexIndex, vec3_t *out) {
    if (!model || frame1 < 0 || frame1 >= model->header.numFrames || frame2 < 0 || frame2 >= model->header.numFrames) return;
    
    md2_frame_t *f1 = &model->frames[frame1];
    md2_frame_t *f2 = &model->frames[frame2];
    
    md2_vertex_t *v1 = &f1->vertices[vertexIndex];
    md2_vertex_t *v2 = &f2->vertices[vertexIndex];
    
    /* Decompress v1 */
    float x1 = (v1->v[0] * f1->scale[0]) + f1->translate[0];
    float y1 = (v1->v[1] * f1->scale[1]) + f1->translate[1];
    float z1 = (v1->v[2] * f1->scale[2]) + f1->translate[2];
    
    /* Decompress v2 */
    float x2 = (v2->v[0] * f2->scale[0]) + f2->translate[0];
    float y2 = (v2->v[1] * f2->scale[1]) + f2->translate[1];
    float z2 = (v2->v[2] * f2->scale[2]) + f2->translate[2];
    
    /* Interpolate */
    out->x = x1 + interpolation * (x2 - x1);
    out->y = y1 + interpolation * (y2 - y1);
    out->z = z1 + interpolation * (z2 - z1);
}
