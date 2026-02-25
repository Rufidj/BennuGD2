#include "libmod_ray_md3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

RAY_MD3_Model *ray_md3_load(const char *filename) {
  FILE *fp;
  RAY_MD3_Model *model;

  fp = fopen(filename, "rb");
  if (!fp) {
    printf("RAY_MD3: Could not open file %s\n", filename);
    return NULL;
  }

  /* 1. Read Header */
  model = (RAY_MD3_Model *)malloc(sizeof(RAY_MD3_Model));
  memset(model, 0, sizeof(RAY_MD3_Model));

  if (fread(&model->header, sizeof(md3_header_t), 1, fp) != 1) {
    free(model);
    fclose(fp);
    return NULL;
  }

  if (model->header.magic != MD3_MAGIC ||
      model->header.version != MD3_VERSION) {
    printf("RAY_MD3: Invalid file or version %s (Magic: %d, Version: %d)\n",
           filename, model->header.magic, model->header.version);
    free(model);
    fclose(fp);
    return NULL;
  }

  strncpy(model->name, filename, 63);
  model->textureID = 0;

  /* 2. Read Frames */
  if (model->header.numFrames > 0) {
    model->frames =
        (md3_frame_t *)malloc(sizeof(md3_frame_t) * model->header.numFrames);
    fseek(fp, model->header.offsetFrames, SEEK_SET);
    if (fread(model->frames, sizeof(md3_frame_t), model->header.numFrames,
              fp) != (size_t)model->header.numFrames) {
      printf("RAY_MD3: Error reading frames\n");
    }
  }

  /* 3. Read Tags */
  if (model->header.numTags > 0) {
    model->tags = (md3_tag_t *)malloc(
        sizeof(md3_tag_t) * model->header.numTags * model->header.numFrames);
    fseek(fp, model->header.offsetTags, SEEK_SET);
    if (fread(model->tags, sizeof(md3_tag_t),
              model->header.numTags * model->header.numFrames, fp) !=
        (size_t)(model->header.numTags * model->header.numFrames)) {
      printf("RAY_MD3: Error reading tags\n");
    }
  }

  /* 4. Read Surfaces */
  if (model->header.numSurfaces > 0) {
    model->surfaces = (RAY_MD3_Surface *)malloc(sizeof(RAY_MD3_Surface) *
                                                model->header.numSurfaces);
    memset(model->surfaces, 0,
           sizeof(RAY_MD3_Surface) * model->header.numSurfaces);

    long surfaceStart = model->header.offsetSurfaces;

    for (int i = 0; i < model->header.numSurfaces; i++) {
      RAY_MD3_Surface *surf = &model->surfaces[i];

      fseek(fp, surfaceStart, SEEK_SET);
      if (fread(&surf->header, sizeof(md3_surface_header_t), 1, fp) != 1) {
        printf("RAY_MD3: Error reading surface header %d\n", i);
        break;
      }

      /* Check surface magic */
      if (surf->header.magic != MD3_MAGIC) {
        printf("RAY_MD3: Invalid surface magic in surface %d\n", i);
      }

      /* Read Shaders */
      if (surf->header.numShaders > 0) {
        surf->shaders = (md3_shader_t *)malloc(sizeof(md3_shader_t) *
                                               surf->header.numShaders);
        fseek(fp, surfaceStart + surf->header.offsetShaders, SEEK_SET);
        if (fread(surf->shaders, sizeof(md3_shader_t), surf->header.numShaders,
                  fp) != (size_t)surf->header.numShaders) {
          printf("RAY_MD3: Error reading shaders in surface %d\n", i);
        }
      }

      /* Read Triangles */
      if (surf->header.numTriangles > 0) {
        surf->triangles = (md3_triangle_t *)malloc(sizeof(md3_triangle_t) *
                                                   surf->header.numTriangles);
        fseek(fp, surfaceStart + surf->header.offsetTriangles, SEEK_SET);
        if (fread(surf->triangles, sizeof(md3_triangle_t),
                  surf->header.numTriangles,
                  fp) != (size_t)surf->header.numTriangles) {
          printf("RAY_MD3: Error reading triangles in surface %d\n", i);
        }
      }

      /* Read TexCoords */
      if (surf->header.numVerts > 0) {
        surf->texCoords = (md3_texCoord_t *)malloc(sizeof(md3_texCoord_t) *
                                                   surf->header.numVerts);
        fseek(fp, surfaceStart + surf->header.offsetTexCoords, SEEK_SET);
        if (fread(surf->texCoords, sizeof(md3_texCoord_t),
                  surf->header.numVerts, fp) != (size_t)surf->header.numVerts) {
          printf("RAY_MD3: Error reading texCoords in surface %d\n", i);
        }
      }

      /* Read Vertices (numVerts * numFrames) */
      if (surf->header.numVerts > 0) {
        int totalVerts = surf->header.numVerts * surf->header.numFrames;
        surf->vertices =
            (md3_vertex_t *)malloc(sizeof(md3_vertex_t) * totalVerts);
        fseek(fp, surfaceStart + surf->header.offsetXyzNormals, SEEK_SET);
        if (fread(surf->vertices, sizeof(md3_vertex_t), totalVerts, fp) !=
            (size_t)totalVerts) {
          printf("RAY_MD3: Error reading vertices in surface %d\n", i);
        }
      }

      /* Move to next surface */
      surfaceStart += surf->header.offsetEnd;
    }
  }

  fclose(fp);
  printf("RAY_MD3: Loaded %s (Surfaces: %d, Frames: %d)\n", filename,
         model->header.numSurfaces, model->header.numFrames);
  return model;
}

void ray_md3_free(RAY_MD3_Model *model) {
  if (!model)
    return;

  if (model->frames)
    free(model->frames);
  if (model->tags)
    free(model->tags);

  if (model->surfaces) {
    for (int i = 0; i < model->header.numSurfaces; i++) {
      RAY_MD3_Surface *surf = &model->surfaces[i];
      if (surf->shaders)
        free(surf->shaders);
      if (surf->triangles)
        free(surf->triangles);
      if (surf->texCoords)
        free(surf->texCoords);
      if (surf->vertices)
        free(surf->vertices);
    }
    free(model->surfaces);
  }

  free(model);
}
