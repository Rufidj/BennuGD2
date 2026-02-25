#define CGLTF_IMPLEMENTATION
#include "libmod_ray_gltf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* SDL2 for image loading from memory */
#include "SDL.h"

RAY_GLTF_Model *ray_gltf_load(const char *filename) {
  cgltf_options options = {0};
  cgltf_data *data = NULL;
  cgltf_result result = cgltf_parse_file(&options, filename, &data);

  if (result != cgltf_result_success) {
    printf("RAY_GLTF: Error parsing file %s (%d)\n", filename, (int)result);
    return NULL;
  }

  result = cgltf_load_buffers(&options, data, filename);
  if (result != cgltf_result_success) {
    cgltf_free(data);
    printf("RAY_GLTF: Error loading buffers for %s (%d)\n", filename,
           (int)result);
    return NULL;
  }

  RAY_GLTF_Model *model = (RAY_GLTF_Model *)malloc(sizeof(RAY_GLTF_Model));
  model->magic = GLTF_MAGIC;
  model->data = data;
  model->textureID = 0;
  strncpy(model->name, filename, 63);

  /* Load internal textures */
  model->textures_count = (int)data->images_count;
  model->textures =
      (GPU_Image **)calloc(model->textures_count, sizeof(GPU_Image *));

  for (cgltf_size i = 0; i < data->images_count; ++i) {
    cgltf_image *image = &data->images[i];
    if (image->buffer_view) {
      void *ptr = (uint8_t *)image->buffer_view->buffer->data +
                  image->buffer_view->offset;
      size_t size = image->buffer_view->size;
      SDL_RWops *rw = SDL_RWFromMem(ptr, (int)size);
      if (rw) {
        model->textures[i] = GPU_LoadImage_RW(rw, 1);
        if (model->textures[i]) {
          GPU_SetImageFilter(model->textures[i], GPU_FILTER_LINEAR);
          GPU_GenerateMipmaps(model->textures[i]);
          printf("RAY_GLTF: Loaded internal texture %d (%s)\n", (int)i,
                 image->name ? image->name : "unnamed");
        }
      }
    } else if (image->uri && strncmp(image->uri, "data:", 5) != 0) {
      /* Extract path from filename to load external texture */
      char path[256];
      strncpy(path, filename, 255);
      char *last_slash = strrchr(path, '/');
      if (last_slash)
        *(last_slash + 1) = '\0';
      else
        path[0] = '\0';
      strncat(path, image->uri, 255);

      model->textures[i] = GPU_LoadImage(path);
      if (model->textures[i]) {
        GPU_SetImageFilter(model->textures[i], GPU_FILTER_LINEAR);
        GPU_GenerateMipmaps(model->textures[i]);
        printf("RAY_GLTF: Loaded external texture %d from %s\n", (int)i, path);
      }
    }
  }

  printf("RAY_GLTF: Loaded %s (Meshes: %d, Nodes: %d, Textures: %d)\n",
         filename, (int)data->meshes_count, (int)data->nodes_count,
         model->textures_count);
  return model;
}

#include <math.h>

void ray_gltf_apply_animation(RAY_GLTF_Model *model, int anim_index,
                              float time) {
  if (!model || !model->data || anim_index < 0 ||
      (cgltf_size)anim_index >= model->data->animations_count)
    return;

  cgltf_animation *anim = &model->data->animations[anim_index];
  for (cgltf_size i = 0; i < anim->channels_count; ++i) {
    cgltf_animation_channel *channel = &anim->channels[i];
    cgltf_node *node = channel->target_node;
    cgltf_animation_sampler *sampler = channel->sampler;

    if (!node || !sampler)
      continue;

    cgltf_accessor *input = sampler->input;
    cgltf_accessor *output = sampler->output;

    if (input->count < 1)
      continue;

    /* Get duration from last keyframe */
    float duration = 0;
    cgltf_accessor_read_float(input, input->count - 1, &duration, 1);
    if (duration <= 0)
      duration = 0.001f;

    float t = fmodf(time, duration);
    if (t < 0)
      t += duration;

    /* Find keyframe pair using binary search */
    cgltf_size k = 0;
    cgltf_size low = 0, high = input->count - 1;
    while (low + 1 < high) {
      cgltf_size mid = low + (high - low) / 2;
      float mid_t;
      cgltf_accessor_read_float(input, mid, &mid_t, 1);
      if (mid_t <= t)
        low = mid;
      else
        high = mid;
    }
    k = low;

    float t0, t1;
    cgltf_accessor_read_float(input, k, &t0, 1);

    /* Handle single keyframe or end of track */
    if (k + 1 < input->count) {
      cgltf_accessor_read_float(input, k + 1, &t1, 1);
    } else {
      t1 = t0;
    }

    float alpha = 0;
    if (t1 > t0) {
      alpha = (t - t0) / (t1 - t0);
    }

    if (channel->target_path == cgltf_animation_path_type_translation) {
      float v0[3], v1[3];
      cgltf_accessor_read_float(output, k, v0, 3);
      if (k + 1 < output->count)
        cgltf_accessor_read_float(output, k + 1, v1, 3);
      else
        for (int j = 0; j < 3; j++)
          v1[j] = v0[j];

      node->has_translation = 1;
      node->has_matrix = 0;
      for (int j = 0; j < 3; ++j)
        node->translation[j] = v0[j] + alpha * (v1[j] - v0[j]);
    } else if (channel->target_path == cgltf_animation_path_type_rotation) {
      float v0[4], v1[4];
      cgltf_accessor_read_float(output, k, v0, 4);
      if (k + 1 < output->count)
        cgltf_accessor_read_float(output, k + 1, v1, 4);
      else
        for (int j = 0; j < 4; j++)
          v1[j] = v0[j];

      node->has_rotation = 1;
      node->has_matrix = 0;
      float dot = v0[0] * v1[0] + v0[1] * v1[1] + v0[2] * v1[2] + v0[3] * v1[3];
      float sign = (dot < 0.0f) ? -1.0f : 1.0f;
      for (int j = 0; j < 4; ++j)
        node->rotation[j] = v0[j] + alpha * (v1[j] * sign - v0[j]);
      float len = sqrtf(node->rotation[0] * node->rotation[0] +
                        node->rotation[1] * node->rotation[1] +
                        node->rotation[2] * node->rotation[2] +
                        node->rotation[3] * node->rotation[3]);
      if (len > 0.0001f)
        for (int j = 0; j < 4; ++j)
          node->rotation[j] /= len;
    } else if (channel->target_path == cgltf_animation_path_type_scale) {
      float v0[3], v1[3];
      cgltf_accessor_read_float(output, k, v0, 3);
      if (k + 1 < output->count)
        cgltf_accessor_read_float(output, k + 1, v1, 3);
      else
        for (int j = 0; j < 3; j++)
          v1[j] = v0[j];

      node->has_scale = 1;
      node->has_matrix = 0;
      for (int j = 0; j < 3; ++j)
        node->scale[j] = v0[j] + alpha * (v1[j] - v0[j]);
    }
  }
}

void ray_gltf_free(RAY_GLTF_Model *model) {
  if (!model)
    return;

  if (model->textures) {
    for (int i = 0; i < model->textures_count; ++i) {
      if (model->textures[i])
        GPU_FreeImage(model->textures[i]);
    }
    free(model->textures);
  }

  if (model->data)
    cgltf_free(model->data);
  free(model);
}
