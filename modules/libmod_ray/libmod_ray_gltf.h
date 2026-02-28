#ifndef __LIBMOD_RAY_GLTF_H
#define __LIBMOD_RAY_GLTF_H

#include "SDL_gpu.h"
#include "cgltf.h"
#include "libmod_ray.h"
#include <stdint.h>

#define GLTF_MAGIC 0x46544C47 /* "GLTF" */

typedef struct {
  uint32_t magic;
  cgltf_data *data;
  GPU_Image **textures;
  int textures_count;
  int textureID; // Default texture ID if none in GLTF
  char name[64];
} RAY_GLTF_Model;

RAY_GLTF_Model *ray_gltf_load(const char *filename);
void ray_gltf_apply_animation(RAY_GLTF_Model *model, int anim_index,
                              float time);
void ray_gltf_free(RAY_GLTF_Model *model);

#endif /* __LIBMOD_RAY_GLTF_H */
