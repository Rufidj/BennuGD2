#define CGLTF_IMPLEMENTATION
#include "libmod_ray_gltf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* SDL2 for image loading from memory */
#include "SDL.h"

static void compute_node_world_matrix(cgltf_node *node, mat4 parent_world) {
  mat4 local;
  if (node->has_matrix) {
    memcpy(local, node->matrix, sizeof(mat4));
  } else {
    float t[3] = {0, 0, 0};
    float q[4] = {0, 0, 0, 1};
    float s[3] = {1, 1, 1};
    if (node->has_translation)
      memcpy(t, node->translation, sizeof(t));
    if (node->has_rotation)
      memcpy(q, node->rotation, sizeof(q));
    if (node->has_scale)
      memcpy(s, node->scale, sizeof(s));
    mat4_from_trs(local, t, q, s);
  }

  mat4 world;
  mat4_mul(world, parent_world, local);

  /* Cache world transform in the node's matrix field */
  memcpy(node->matrix, world, sizeof(mat4));
  node->has_matrix = 1;

  for (cgltf_size i = 0; i < node->children_count; ++i) {
    compute_node_world_matrix(node->children[i], world);
  }
}

void ray_gltf_update_matrices(RAY_GLTF_Model *model) {
  if (!model || !model->data)
    return;
  cgltf_data *data = model->data;

  /* 1. Compute World Matrices for all nodes */
  mat4 ident;
  mat4_identity(ident);

  /* Process root nodes */
  for (cgltf_size i = 0; i < data->nodes_count; ++i) {
    if (!data->nodes[i].parent) {
      compute_node_world_matrix(&data->nodes[i], ident);
    }
  }

  /* 2. Compute Skin Joint Matrices */
  for (cgltf_size s = 0; s < data->skins_count; ++s) {
    cgltf_skin *skin = &data->skins[s];
    float *matrices = model->skin_matrices[s];

    for (cgltf_size j = 0; j < skin->joints_count; ++j) {
      cgltf_node *joint_node = skin->joints[j];
      mat4 inv_bind;
      cgltf_accessor_read_float(skin->inverse_bind_matrices, j, inv_bind, 16);

      mat4 joint_matrix;
      mat4_mul(joint_matrix, joint_node->matrix, inv_bind);
      memcpy(&matrices[j * 16], joint_matrix, sizeof(mat4));
    }
  }
}

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
  memset(model, 0, sizeof(RAY_GLTF_Model));
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
        }
      }
    } else if (image->uri && strncmp(image->uri, "data:", 5) != 0) {
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
      }
    }
  }

  /* Allocate skin matrices */
  model->skins_count = (int)data->skins_count;
  if (model->skins_count > 0) {
    model->skin_matrices =
        (float **)calloc(model->skins_count, sizeof(float *));
    for (int s = 0; s < model->skins_count; s++) {
      model->skin_matrices[s] =
          (float *)malloc(data->skins[s].joints_count * 16 * sizeof(float));
    }
  }

  printf("RAY_GLTF: Loaded %s (Meshes: %d, Skins: %d, Textures: %d)\n",
         filename, (int)data->meshes_count, model->skins_count,
         model->textures_count);
  return model;
}

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

    float duration = 0;
    cgltf_accessor_read_float(input, input->count - 1, &duration, 1);
    if (duration <= 0)
      duration = 0.001f;

    float t = fmodf(time, duration);
    if (t < 0)
      t += duration;

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

    float t0, t1;
    cgltf_accessor_read_float(input, low, &t0, 1);
    t1 = t0;
    if (low + 1 < input->count)
      cgltf_accessor_read_float(input, low + 1, &t1, 1);

    float alpha = 0;
    if (t1 > t0)
      alpha = (t - t0) / (t1 - t0);

    if (channel->target_path == cgltf_animation_path_type_translation) {
      float v0[3], v1[3];
      cgltf_accessor_read_float(output, low, v0, 3);
      if (low + 1 < output->count)
        cgltf_accessor_read_float(output, low + 1, v1, 3);
      else
        memcpy(v1, v0, sizeof(v0));
      node->has_translation = 1;
      for (int j = 0; j < 3; ++j)
        node->translation[j] = v0[j] + alpha * (v1[j] - v0[j]);
    } else if (channel->target_path == cgltf_animation_path_type_rotation) {
      float v0[4], v1[4];
      cgltf_accessor_read_float(output, low, v0, 4);
      if (low + 1 < output->count)
        cgltf_accessor_read_float(output, low + 1, v1, 4);
      else
        memcpy(v1, v0, sizeof(v0));
      node->has_rotation = 1;
      float dot = v0[0] * v1[0] + v0[1] * v1[1] + v0[2] * v1[2] + v0[3] * v1[3];
      float sign = (dot < 0.0f) ? -1.0f : 1.0f;
      for (int j = 0; j < 4; ++j)
        node->rotation[j] = v0[j] + alpha * (v1[j] * sign - v0[j]);
      float len = sqrtf(node->rotation[0] * node->rotation[0] +
                        node->rotation[1] * node->rotation[1] +
                        node->rotation[2] * node->rotation[2] +
                        node->rotation[3] * node->rotation[3]);
      if (len > 0)
        for (int j = 0; j < 4; j++)
          node->rotation[j] /= len;
    } else if (channel->target_path == cgltf_animation_path_type_scale) {
      float v0[3], v1[3];
      cgltf_accessor_read_float(output, low, v0, 3);
      if (low + 1 < output->count)
        cgltf_accessor_read_float(output, low + 1, v1, 3);
      else
        memcpy(v1, v0, sizeof(v0));
      node->has_scale = 1;
      for (int j = 0; j < 3; ++j)
        node->scale[j] = v0[j] + alpha * (v1[j] - v0[j]);
    }
    node->has_matrix = 0; // Clear cached world matrix
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
  if (model->skin_matrices) {
    for (int i = 0; i < model->skins_count; i++)
      if (model->skin_matrices[i])
        free(model->skin_matrices[i]);
    free(model->skin_matrices);
  }
  if (model->data)
    cgltf_free(model->data);
  free(model);
}
