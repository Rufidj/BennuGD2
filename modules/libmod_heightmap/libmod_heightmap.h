/*  
 * Copyright (C) 2025 - Heightmap Module for BennuGD2  
 * This file is part of Bennu Game Development  
 */  
  
#ifndef __LIBMOD_HEIGHTMAP_H  
#define __LIBMOD_HEIGHTMAP_H  
  
#include <stdint.h>  
#include <math.h>  
  
/* Inclusiones necesarias de BennuGD2 */  
#include "bgddl.h"  
#include "libbggfx.h"  
#include "g_bitmap.h"  
#include "g_blit.h"  
#include "g_pixel.h"  
#include "g_clear.h"  
#include "g_grlib.h"  
#include "xstrings.h"  
#include "m_map.h"  
#include <GL/glew.h>
  
/* Estructuras para el módulo de heightmap */  
typedef struct {      
    int64_t id;      
    GRAPH *heightmap;        // Para modo tradicional    
    GRAPH *texturemap;           
    int64_t width;      
    int64_t height;      
    float *height_cache;         
    int cache_valid;    
} HEIGHTMAP;

// Constantes de conversión de coordenadas  
#define WORLD_TO_SPRITE_SCALE 10.0f  
#define SPRITE_TO_WORLD_SCALE 0.1f  
  
// Constantes de renderizado  
#define DEFAULT_MAX_RENDER_DISTANCE 8000.0f  
#define DEFAULT_CHUNK_SIZE 512  
#define DEFAULT_CHUNK_RADIUS 15  
  
// Constantes de fog  
#define FOG_MIN_VISIBILITY 0.6f  
#define FOG_MAX_DISTANCE 800.0f

// Constantes de proyección  
#define PROJECTION_HEIGHT_SCALE 300.0f  
#define PROJECTION_CENTER_Y 120.0f  
  
// Constantes de calidad de renderizado  
#define QUALITY_STEP_NEAR 0.2f  
#define QUALITY_STEP_MID 0.5f  
#define QUALITY_STEP_FAR 1.0f  
#define QUALITY_DISTANCE_NEAR 50.0f  
#define QUALITY_DISTANCE_MID 200.0f  
  
// Constantes de agua  
#define WATER_WAVE_FREQUENCY 0.05f  
#define WATER_UV_SCALE 0.01f  
#define WATER_TIME_SCALE_U 0.1f  
#define WATER_TIME_SCALE_V 0.05f


  
typedef struct {  
    float x, y, z;  
    float angle, pitch;  
    float fov;  
    float near, far;  // Agregar estos campos  
} CAMERA_3D; 

typedef struct {    
    int screen_x, screen_y;    
    float distance;          // NUEVO: distancia real para depth buffer  
    float distance_scale;    // Mantener para compatibilidad  
    int scaled_width, scaled_height;    
    uint8_t alpha;    
    int valid;    
    float fog_tint_factor;
} BILLBOARD_PROJECTION;

// MODELOS 3D //

// Estructura para mallas 3D en GPU  
typedef struct {  
    GLuint vao;  
    GLuint vbo;  
    GLuint ebo;  
    int vertex_count;  
    int triangle_count;  
    GRAPH *texture;  
    int64_t id;  
      
    // Datos de transformación  
    float world_x, world_y, world_z;  
    float angle;  
    float scale;  
} GPU_MESH;  
  
#define MAX_MESHES 100  
static GPU_MESH meshes[MAX_MESHES];  
static int mesh_count = 0;  
  
// Variables para el shader de mallas  
static BGD_SHADER *mesh_shader = NULL;  
static BGD_SHADER_PARAMETERS *mesh_params = NULL;

// Estructura para matriz 4x4  
typedef struct {  
    float m[16];  // Almacenada en column-major order (como OpenGL)  
} mat4;  
  
// Crear matriz identidad  
static void mat4_identity(mat4 *out) {  
    for (int i = 0; i < 16; i++) {  
        out->m[i] = (i % 5 == 0) ? 1.0f : 0.0f;  
    }  
}  
  
// Multiplicar dos matrices  
static void mat4_multiply(mat4 *out, const mat4 *a, const mat4 *b) {  
    mat4 result;  
    for (int row = 0; row < 4; row++) {  
        for (int col = 0; col < 4; col++) {  
            float sum = 0.0f;  
            for (int i = 0; i < 4; i++) {  
                sum += a->m[row + i * 4] * b->m[i + col * 4];  
            }  
            result.m[row + col * 4] = sum;  
        }  
    }  
    memcpy(out->m, result.m, sizeof(float) * 16);  
}  
  
// Crear matriz de perspectiva  
static void mat4_perspective(mat4 *out, float fov_radians, float aspect, float near, float far) {  
    mat4_identity(out);  
      
    float f = 1.0f / tanf(fov_radians * 0.5f);  
      
    out->m[0] = f / aspect;  
    out->m[5] = f;  
    out->m[10] = (far + near) / (near - far);  
    out->m[11] = -1.0f;  
    out->m[14] = (2.0f * far * near) / (near - far);  
    out->m[15] = 0.0f;  
}  
  

// MODELOS 3D //

/* Constantes */  
#define MAX_HEIGHTMAPS 512  
#define DEFAULT_FOV 60.0f  
#define DEFAULT_NEAR 1.0f  
#define DEFAULT_FAR 1000.0f  
  
/* Variables globales del módulo */  
extern HEIGHTMAP heightmaps[MAX_HEIGHTMAPS];  
extern CAMERA_3D camera;  
extern int64_t next_heightmap_id;  
  
/* Funciones principales */  
extern int64_t libmod_heightmap_load(INSTANCE *my, int64_t *params);  
extern int64_t libmod_heightmap_create(INSTANCE *my, int64_t *params);  
extern int64_t libmod_heightmap_unload(INSTANCE *my, int64_t *params);  
extern int64_t libmod_heightmap_get_height(INSTANCE *my, int64_t *params);  
extern int64_t libmod_heightmap_set_camera(INSTANCE *my, int64_t *params);  
extern int64_t libmod_heightmap_render_3d(INSTANCE *my, int64_t *params);  
extern int64_t libmod_heightmap_load_texture(INSTANCE *my, int64_t *params); // Nueva función  
extern int64_t libmod_heightmap_update_sprite_3d(INSTANCE *my, int64_t *params);
extern int64_t libmod_heightmap_load_overlay_mask(INSTANCE *my, int64_t *params);  
extern int64_t libmod_heightmap_load_bridge_texture(INSTANCE *my, int64_t *params);  
extern int64_t libmod_heightmap_set_bridge_height(INSTANCE *my, int64_t *params);
extern int64_t libmod_heightmap_update_billboard_graph(INSTANCE *my, int64_t *params);
// Declaración para renderizado GPU  
extern int64_t libmod_heightmap_render_voxelspace_gpu(INSTANCE *my, int64_t *params);
// Declaraciones forward para skybox  
static uint32_t sample_sky_texture(float screen_x, float screen_y, float camera_angle, float camera_pitch, float time);  
static void render_skybox(float camera_angle, float camera_pitch, float time, int quality_step);

// Declaraciones forward para efectos atmosféricos  
static void render_atmospheric_particles(float time, int quality_step, HEIGHTMAP *hm); 
static float calculate_atmospheric_lighting(float distance, float height);  
static float calculate_volumetric_fog(float world_z, float distance);


/* Funciones internas */  
extern float get_height_at(HEIGHTMAP *hm, float x, float y);  
extern void build_height_cache(HEIGHTMAP *hm);  
/* Funciones internas para tiles */  
extern void build_tile_height_cache(HEIGHTMAP *hm, int tile_index);  
extern void load_tile_from_folder_on_demand(HEIGHTMAP *hm, int tile_x, int tile_y);  
extern void load_tile_from_fpg_on_demand(HEIGHTMAP *hm, int tile_x, int tile_y);  
extern float get_height_from_tile_cache(HEIGHTMAP *hm, int tile_index, float local_x, float local_y);  
extern int64_t libmod_heightmap_load_tiles_from_folder(INSTANCE *my, int64_t *params);
#endif