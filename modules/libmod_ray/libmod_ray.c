/*
 * libmod_ray.c - Raycasting Module for BennuGD2
 * Geometric Sector-Based Engine (Build Engine Style)
 */

#include "libmod_ray.h"
#include "libmod_ray_compat.h"
#include "libmod_ray_gltf.h"
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
   ESTADO GLOBAL DEL MOTOR
   ============================================================================
 */

RAY_Engine g_engine = {0};
float *g_zbuffer = NULL;

/* Render graph global */
static GRAPH *render_graph = NULL;
static GRAPH *lowres_buffer = NULL; // Low-resolution rendering buffer

/* External functions */
extern int ray_load_map_v8(const char *filename);
extern int ray_save_map_v8(const char *filename);
extern void ray_render_frame(GRAPH *dest);
extern void ray_render_frame_portal(GRAPH *dest);
extern void ray_render_frame_portal_simple(GRAPH *dest);
extern void
ray_render_frame_build(GRAPH *dest);          /* Build Engine style renderer */
extern void ray_render_frame_gpu(void *dest); /* GPU renderer */
static int g_use_gpu = 1;                     // Default: Software

extern void ray_detect_portals(RAY_Engine *engine);
extern int ray_check_collision(RAY_Engine *engine, float x, float y, float z,
                               float new_x, float new_y, float step_h);

/* ============================================================================
   INICIALIZACIÓN Y FINALIZACIÓN
   ============================================================================
 */

int64_t libmod_ray_init(INSTANCE *my, int64_t *params) {
  int screen_w = (int)params[0];
  int screen_h = (int)params[1];
  int fov = (int)params[2];
  int strip_width = (int)params[3];

  if (g_engine.initialized) {
    fprintf(stderr, "RAY: Motor ya inicializado\n");
    return 0;
  }

  /* Configuración básica */
  g_engine.displayWidth = screen_w;
  g_engine.displayHeight = screen_h;

  // PERFORMANCE: Internal resolution scaling (DISABLED)
  // Set to 1.0 for full resolution rendering
  g_engine.resolutionScale = 1.0f;
  g_engine.internalWidth = screen_w;
  g_engine.internalHeight = screen_h;

  printf("RAY: Internal Resolution: %dx%d (%.0f%%)\n", g_engine.internalWidth,
         g_engine.internalHeight, g_engine.resolutionScale * 100.0f);

  g_engine.fovDegrees = fov;
  g_engine.fovRadians = (float)fov * M_PI / 180.0f;
  g_engine.stripWidth = strip_width;
  g_engine.rayCount =
      g_engine.internalWidth / strip_width; // Use internal width
  g_engine.viewDist =
      ray_screen_distance((float)g_engine.internalWidth, g_engine.fovRadians);

  /* Precalcular ángulos de strips */
  g_engine.stripAngles = (float *)malloc(g_engine.rayCount * sizeof(float));
  if (!g_engine.stripAngles) {
    fprintf(stderr, "RAY: Error al asignar memoria para stripAngles\n");
    return 0;
  }

  for (int strip = 0; strip < g_engine.rayCount; strip++) {
    float screenX = (g_engine.rayCount / 2 - strip) * strip_width;
    float angle = atanf(screenX / g_engine.viewDist);
    g_engine.stripAngles[strip] = angle;
  }

  /* Inicializar cámara */
  memset(&g_engine.camera, 0, sizeof(RAY_Camera));
  g_engine.camera.x = 384.0f;
  g_engine.camera.y = 384.0f;
  g_engine.camera.z = 0.0f;
  g_engine.camera.rot = 0.0f;
  g_engine.camera.pitch = 0.0f;
  g_engine.camera.moveSpeed = RAY_WORLD_UNIT / 16.0f;
  g_engine.camera.rotSpeed = 1.5f * M_PI / 180.0f;
  g_engine.camera.current_sector_id = -1;

  /* Inicializar arrays dinámicos */
  g_engine.sprites_capacity = RAY_MAX_SPRITES;
  g_engine.sprites =
      (RAY_Sprite *)calloc(g_engine.sprites_capacity, sizeof(RAY_Sprite));
  g_engine.num_sprites = 0;

  g_engine.spawn_flags_capacity = RAY_MAX_SPAWN_FLAGS;
  g_engine.spawn_flags = (RAY_SpawnFlag *)calloc(g_engine.spawn_flags_capacity,
                                                 sizeof(RAY_SpawnFlag));
  g_engine.num_spawn_flags = 0;

  g_engine.sectors_capacity = RAY_MAX_SECTORS;
  g_engine.sectors =
      (RAY_Sector *)calloc(g_engine.sectors_capacity, sizeof(RAY_Sector));
  g_engine.num_sectors = 0;

  g_engine.portals_capacity = RAY_MAX_PORTALS;
  g_engine.portals =
      (RAY_Portal *)calloc(g_engine.portals_capacity, sizeof(RAY_Portal));
  g_engine.num_portals = 0;

  /* Opciones de renderizado por defecto */
  g_engine.drawMiniMap = 1;
  g_engine.drawTexturedFloor = 1;
  g_engine.drawCeiling = 1;
  g_engine.drawWalls = 1;
  g_engine.drawWeapon = 1;
  g_engine.fogOn = 0;
  g_engine.skyTextureID = 0;

  /* Fog - Configuración por defecto */
  g_engine.fog_r = 150;
  g_engine.fog_g = 150;
  g_engine.fog_b = 180;
  g_engine.fog_start_distance = RAY_WORLD_UNIT * 8;
  g_engine.fog_end_distance = RAY_WORLD_UNIT * 20;

  /* Minimapa - Configuración por defecto */
  g_engine.minimap_size = 200;
  g_engine.minimap_x = 10;
  g_engine.minimap_y = 10;
  g_engine.minimap_scale = 0.5f;

  /* Portal Rendering Configuration */
  g_engine.max_portal_depth = 16; /* Aumentado para mejor visibilidad */
  g_engine.portal_rendering_enabled = 1;

  /* Billboard */
  g_engine.billboard_enabled = 1;
  g_engine.billboard_directions = 12;

  g_engine.fpg_id = 0;
  g_engine.last_ticks = 0;
  g_engine.default_step_height =
      32.0f; /* Altura de escalón estándar (0.25 unidades mundo) */
  g_engine.initialized = 1;

  printf("RAY: Motor inicializado (v9 - Flat Sectors) - %dx%d, FOV=%d, "
         "stripWidth=%d, rayCount=%d\n",
         screen_w, screen_h, fov, strip_width, g_engine.rayCount);

  return 1;
}

int64_t libmod_ray_shutdown(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized) {
    return 0;
  }

  /* Liberar stripAngles */
  if (g_engine.stripAngles) {
    free(g_engine.stripAngles);
    g_engine.stripAngles = NULL;
  }

  /* Liberar sprites */
  if (g_engine.sprites) {
    free(g_engine.sprites);
    g_engine.sprites = NULL;
  }

  /* Liberar spawn flags */
  if (g_engine.spawn_flags) {
    free(g_engine.spawn_flags);
    g_engine.spawn_flags = NULL;
  }

  /* Liberar sectors */
  if (g_engine.sectors) {
    for (int i = 0; i < g_engine.num_sectors; i++) {
      RAY_Sector *sector = &g_engine.sectors[i];
      if (sector->vertices)
        free(sector->vertices);
      if (sector->walls)
        free(sector->walls);
      if (sector->portal_ids)
        free(sector->portal_ids);
    }
    free(g_engine.sectors);
    g_engine.sectors = NULL;
  }

  /* Liberar portals */
  if (g_engine.portals) {
    free(g_engine.portals);
    g_engine.portals = NULL;
  }

  /* Liberar render graph */
  if (render_graph) {
    bitmap_destroy(render_graph);
    render_graph = NULL;
  }

  memset(&g_engine, 0, sizeof(RAY_Engine));

  printf("RAY: Motor finalizado\n");
  return 1;
}

/* ============================================================================
   CARGA DE MAPAS
   ============================================================================
 */

void ray_calculate_all_sector_bounds() {
  for (int i = 0; i < g_engine.num_sectors; i++) {
    RAY_Sector *sector = &g_engine.sectors[i];

    if (sector->num_walls == 0) {
      sector->min_x = 0;
      sector->min_y = 0;
      sector->max_x = 0;
      sector->max_y = 0;
      continue;
    }

    // Init with first point of first wall
    sector->min_x = sector->walls[0].x1;
    sector->max_x = sector->walls[0].x1;
    sector->min_y = sector->walls[0].y1;
    sector->max_y = sector->walls[0].y1;

    for (int w = 0; w < sector->num_walls; w++) {
      RAY_Wall *wall = &sector->walls[w];

      // Checks for X1, Y1
      if (wall->x1 < sector->min_x)
        sector->min_x = wall->x1;
      if (wall->x1 > sector->max_x)
        sector->max_x = wall->x1;
      if (wall->y1 < sector->min_y)
        sector->min_y = wall->y1;
      if (wall->y1 > sector->max_y)
        sector->max_y = wall->y1;

      // Checks for X2, Y2 (Redundant if walls are connected, but safer)
      if (wall->x2 < sector->min_x)
        sector->min_x = wall->x2;
      if (wall->x2 > sector->max_x)
        sector->max_x = wall->x2;
      if (wall->y2 < sector->min_y)
        sector->min_y = wall->y2;
      if (wall->y2 > sector->max_y)
        sector->max_y = wall->y2;
    }
  }
}

/* ============================================================================
   STATIC PVS (Potentially Visible Set) BAKING
   ============================================================================
 */

static void ray_bake_pvs_recursive(int source_id, int current_id, int depth,
                                   uint8_t *visited) {
  if (depth <= 0)
    return;

  // Mark as visible
  // Matrix is flat: [source * num_sectors + current]
  int idx = source_id * g_engine.num_sectors + current_id;
  g_engine.pvs_matrix[idx] = 1;

  RAY_Sector *sector = &g_engine.sectors[current_id];

  // Traverse portals
  for (int p = 0; p < sector->num_portals; p++) {
    int portal_id = sector->portal_ids[p];
    if (portal_id < 0 || portal_id >= g_engine.num_portals)
      continue;

    RAY_Portal *portal = &g_engine.portals[portal_id];

    int next_id = -1;
    if (portal->sector_a == current_id)
      next_id = portal->sector_b;
    else if (portal->sector_b == current_id)
      next_id = portal->sector_a;

    if (next_id != -1 && !visited[next_id]) {
      visited[next_id] = 1;
      // Recurse with decremented depth
      ray_bake_pvs_recursive(source_id, next_id, depth - 1, visited);
      visited[next_id] = 0;
    }
  }
}

void ray_bake_pvs() {
  if (g_engine.num_sectors == 0)
    return;

  printf("RAY: Baking Static PVS for %d sectors...\n", g_engine.num_sectors);

  // Allocate Matrix (num * num bytes)
  if (g_engine.pvs_matrix)
    free(g_engine.pvs_matrix);
  g_engine.pvs_matrix =
      (uint8_t *)calloc(g_engine.num_sectors * g_engine.num_sectors, 1);

  if (!g_engine.pvs_matrix) {
    fprintf(stderr, "RAY: Error allocating PVS matrix\n");
    return;
  }

  // Temporary visited array for recursion
  uint8_t *visited = (uint8_t *)malloc(g_engine.num_sectors);

  for (int i = 0; i < g_engine.num_sectors; i++) {
    // Clear visited for this source sector
    memset(visited, 0, g_engine.num_sectors);

    // Setup recursion
    visited[i] = 1;

    // Mark self as visible
    g_engine.pvs_matrix[i * g_engine.num_sectors + i] = 1;

    // Start traversal (Depth 32 is sufficient for most maps)
    ray_bake_pvs_recursive(i, i, 32, visited);
  }

  // HIERARCHY FIX: Mark all parent-child sector pairs as mutually visible
  // This ensures nested sectors are always visible from their parent
  // We need to do this TRANSITIVELY - grandparents can see grandchildren, etc.

  // Helper function to recursively mark all descendants as visible
  void mark_descendants_visible(int ancestor_idx, int descendant_idx) {
    // Mark ancestor->descendant as visible
    g_engine.pvs_matrix[ancestor_idx * g_engine.num_sectors + descendant_idx] =
        1;
    // Mark descendant->ancestor as visible (bidirectional)
    g_engine.pvs_matrix[descendant_idx * g_engine.num_sectors + ancestor_idx] =
        1;

    // Recursively mark all children of this descendant
    RAY_Sector *desc_sector = &g_engine.sectors[descendant_idx];
    for (int c = 0; c < desc_sector->num_children; c++) {
      int child_id = desc_sector->child_sector_ids[c];

      // Find child index
      for (int j = 0; j < g_engine.num_sectors; j++) {
        if (g_engine.sectors[j].sector_id == child_id) {
          mark_descendants_visible(ancestor_idx, j);
          break;
        }
      }
    }
  }

  for (int i = 0; i < g_engine.num_sectors; i++) {
    RAY_Sector *sector = &g_engine.sectors[i];

    // For each child of this sector
    for (int c = 0; c < sector->num_children; c++) {
      int child_id = sector->child_sector_ids[c];

      // Find the child sector's index
      int child_index = -1;
      for (int j = 0; j < g_engine.num_sectors; j++) {
        if (g_engine.sectors[j].sector_id == child_id) {
          child_index = j;
          break;
        }
      }

      if (child_index >= 0) {
        // Recursively mark this child and all its descendants as visible
        mark_descendants_visible(i, child_index);
      }
    }
  }

  free(visited);
  g_engine.pvs_ready = 1;
  printf("RAY: PVS Bake Complete.\n");
}

int64_t libmod_ray_load_map(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized) {
    fprintf(stderr, "RAY: Motor no inicializado\n");
    return 0;
  }

  const char *filename = string_get((int)params[0]);
  int fpg_id = (int)params[1];

  g_engine.fpg_id = fpg_id;

  printf("RAY: Cargando mapa: %s (FPG: %d)\n", filename, fpg_id);

  int result = ray_load_map(filename);

  if (result) {
    // Optimización 1: Calcular AABB de todos los sectores
    ray_calculate_all_sector_bounds();

    // Optimización 2: Static PVS Bake
    ray_bake_pvs();

    printf("RAY: Mapa cargado exitosamente\n");
    printf("RAY: %d sectores, %d portales, %d sprites\n", g_engine.num_sectors,
           g_engine.num_portals, g_engine.num_sprites);
  } else {
    fprintf(stderr, "RAY: Error al cargar el mapa\n");
  }

  string_discard((int)params[0]);
  return result;
}

int64_t libmod_ray_free_map(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;

  /* Liberar PVS */
  if (g_engine.pvs_matrix) {
    free(g_engine.pvs_matrix);
    g_engine.pvs_matrix = NULL;
  }
  g_engine.pvs_ready = 0;

  /* Liberar sectores */
  if (g_engine.sectors) {
    for (int i = 0; i < g_engine.num_sectors; i++) {
      RAY_Sector *sector = &g_engine.sectors[i];
      if (sector->vertices)
        free(sector->vertices);
      if (sector->walls)
        free(sector->walls);
      if (sector->portal_ids)
        free(sector->portal_ids);
    }
    free(g_engine.sectors);
    g_engine.sectors =
        (RAY_Sector *)calloc(g_engine.sectors_capacity, sizeof(RAY_Sector));
  }
  g_engine.num_sectors = 0;

  /* Liberar portales */
  if (g_engine.portals) {
    free(g_engine.portals);
    g_engine.portals =
        (RAY_Portal *)calloc(g_engine.portals_capacity, sizeof(RAY_Portal));
  }
  g_engine.num_portals = 0;

  /* Liberar sprites */
  g_engine.num_sprites = 0;
  g_engine.num_spawn_flags = 0;

  printf("RAY: Mapa liberado\n");
  return 1;
}

/* ============================================================================
   CÁMARA - GETTERS
   ============================================================================
 */

int64_t libmod_ray_get_camera_x(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;
  float val = g_engine.camera.x;
  return (int64_t) * (int32_t *)&val;
}

int64_t libmod_ray_get_camera_y(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;
  float val = g_engine.camera.y;
  return (int64_t) * (int32_t *)&val;
}

int64_t libmod_ray_get_camera_z(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;
  float val = g_engine.camera.z;
  return (int64_t) * (int32_t *)&val;
}

int64_t libmod_ray_get_camera_rot(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;
  float val = g_engine.camera.rot;
  return (int64_t) * (int32_t *)&val;
}

int64_t libmod_ray_get_camera_pitch(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;
  float val = g_engine.camera.pitch;
  return (int64_t) * (int32_t *)&val;
}

int64_t libmod_ray_get_sprite_x(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;
  int id = (int)params[0];
  if (id < 0 || id >= g_engine.num_sprites)
    return 0;
  float val = g_engine.sprites[id].x;
  return (int64_t) * (int32_t *)&val;
}

int64_t libmod_ray_get_sprite_y(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;
  int id = (int)params[0];
  if (id < 0 || id >= g_engine.num_sprites)
    return 0;
  float val = g_engine.sprites[id].y;
  return (int64_t) * (int32_t *)&val;
}

int64_t libmod_ray_get_sprite_z(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;
  int id = (int)params[0];
  if (id < 0 || id >= g_engine.num_sprites)
    return 0;
  float val = g_engine.sprites[id].z;
  return (int64_t) * (int32_t *)&val;
}

/* ============================================================================
   CÁMARA - SETTER
   ============================================================================
 */

int64_t libmod_ray_set_camera(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;

  float x = *(float *)&params[0];
  float y = *(float *)&params[1];
  float z = *(float *)&params[2];
  float rot = *(float *)&params[3];
  float pitch = *(float *)&params[4];

  g_engine.camera.x = x;
  g_engine.camera.y = y;
  g_engine.camera.z = z;
  g_engine.camera.rot = rot;
  g_engine.camera.pitch = pitch;

  /* Limitar pitch */
  const float max_pitch = M_PI / 2.0f * 0.99f;
  if (g_engine.camera.pitch > max_pitch)
    g_engine.camera.pitch = max_pitch;
  if (g_engine.camera.pitch < -max_pitch)
    g_engine.camera.pitch = -max_pitch;

  /* Update sector based on new position */
  RAY_Sector *sector = ray_find_sector_at_position(&g_engine, x, y, z);
  if (sector) {
    g_engine.camera.current_sector_id = sector->sector_id;
  }

  return 1;
}

/* ============================================================================
   MOVIMIENTO
   ============================================================================
 */

int64_t libmod_ray_move_forward(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;

  float speed = *(float *)&params[0];
  float newX = g_engine.camera.x + cosf(g_engine.camera.rot) * speed;
  float newY = g_engine.camera.y + sinf(g_engine.camera.rot) *
                                       speed; // Fixed: Matches renderer (+sin)

  if (!ray_check_collision(&g_engine, g_engine.camera.x, g_engine.camera.y,
                           g_engine.camera.z, newX, newY,
                           g_engine.default_step_height)) {
    g_engine.camera.x = newX;
    g_engine.camera.y = newY;

    /* Update current sector */
    RAY_Sector *sector =
        ray_find_sector_at_position(&g_engine, newX, newY, g_engine.camera.z);
    if (sector) {
      g_engine.camera.current_sector_id = sector->sector_id;

      if (ray_sector_is_solid(sector)) {
        float step_h = sector->ceiling_z - g_engine.camera.z;
        if (step_h > 0 && step_h < g_engine.default_step_height) {
          g_engine.camera.z = sector->ceiling_z + 1.0f;
        }
      } else {
        // Follow floor height changes (ramps/steps) while preserving eye height
        float eye_height = g_engine.camera.z - sector->floor_z;
        if (eye_height < 0) {
          // Camera is below the new floor - snap up
          g_engine.camera.z = sector->floor_z + 32.0f;
        }
      }
    }
  }

  return 1;
}

int64_t libmod_ray_move_backward(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;

  float speed = *(float *)&params[0];
  float newX = g_engine.camera.x - cosf(g_engine.camera.rot) * speed;
  float newY = g_engine.camera.y - sinf(g_engine.camera.rot) * speed;

  if (!ray_check_collision(&g_engine, g_engine.camera.x, g_engine.camera.y,
                           g_engine.camera.z, newX, newY,
                           g_engine.default_step_height)) {
    g_engine.camera.x = newX;
    g_engine.camera.y = newY;

    /* Update current sector */
    RAY_Sector *sector =
        ray_find_sector_at_position(&g_engine, newX, newY, g_engine.camera.z);
    if (sector) {
      g_engine.camera.current_sector_id = sector->sector_id;

      if (ray_sector_is_solid(sector)) {
        float step_h = sector->ceiling_z - g_engine.camera.z;
        if (step_h > 0 && step_h < g_engine.default_step_height) {
          g_engine.camera.z = sector->ceiling_z + 1.0f;
        }
      } else {
        // Follow floor height changes (ramps/steps) while preserving eye height
        float eye_height = g_engine.camera.z - sector->floor_z;
        if (eye_height < 0) {
          // Camera is below the new floor - snap up
          g_engine.camera.z = sector->floor_z + 32.0f;
        }
      }
    }
  }

  return 1;
}

int64_t libmod_ray_strafe_left(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;

  float speed = *(float *)&params[0];
  // Swapped to -PI/2 for Left
  float newX = g_engine.camera.x + cosf(g_engine.camera.rot - M_PI / 2) * speed;
  float newY = g_engine.camera.y + sinf(g_engine.camera.rot - M_PI / 2) * speed;

  if (!ray_check_collision(&g_engine, g_engine.camera.x, g_engine.camera.y,
                           g_engine.camera.z, newX, newY,
                           g_engine.default_step_height)) {
    g_engine.camera.x = newX;
    g_engine.camera.y = newY;

    /* Update current sector */
    RAY_Sector *sector =
        ray_find_sector_at_position(&g_engine, newX, newY, g_engine.camera.z);
    if (sector) {
      g_engine.camera.current_sector_id = sector->sector_id;

      if (ray_sector_is_solid(sector)) {
        float step_h = sector->ceiling_z - g_engine.camera.z;
        if (step_h > 0 && step_h < g_engine.default_step_height) {
          g_engine.camera.z = sector->ceiling_z + 1.0f;
        }
      } else {
        // Follow floor height changes (ramps/steps) while preserving eye height
        float eye_height = g_engine.camera.z - sector->floor_z;
        if (eye_height < 0) {
          // Camera is below the new floor - snap up
          g_engine.camera.z = sector->floor_z + 32.0f;
        }
      }
    }
  }

  return 1;
}

int64_t libmod_ray_strafe_right(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;

  float speed = *(float *)&params[0];
  // Swapped to +PI/2 for Right
  float newX = g_engine.camera.x + cosf(g_engine.camera.rot + M_PI / 2) * speed;
  float newY = g_engine.camera.y + sinf(g_engine.camera.rot + M_PI / 2) * speed;

  if (!ray_check_collision(&g_engine, g_engine.camera.x, g_engine.camera.y,
                           g_engine.camera.z, newX, newY,
                           g_engine.default_step_height)) {
    g_engine.camera.x = newX;
    g_engine.camera.y = newY;

    /* Update current sector */
    RAY_Sector *sector =
        ray_find_sector_at_position(&g_engine, newX, newY, g_engine.camera.z);
    if (sector) {
      g_engine.camera.current_sector_id = sector->sector_id;

      if (ray_sector_is_solid(sector)) {
        float step_h = sector->ceiling_z - g_engine.camera.z;
        if (step_h > 0 && step_h < g_engine.default_step_height) {
          g_engine.camera.z = sector->ceiling_z + 1.0f;
        }
      } else {
        // Follow floor height changes (ramps/steps) while preserving eye height
        float eye_height = g_engine.camera.z - sector->floor_z;
        if (eye_height < 0) {
          // Camera is below the new floor - snap up
          g_engine.camera.z = sector->floor_z + 32.0f;
        }
      }
    }
  }

  return 1;
}

int64_t libmod_ray_rotate(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;

  float delta = *(float *)&params[0];
  g_engine.camera.rot += delta; // Inverted from -= to +=

  /* Normalizar ángulo */
  while (g_engine.camera.rot < 0)
    g_engine.camera.rot += RAY_TWO_PI;
  while (g_engine.camera.rot >= RAY_TWO_PI)
    g_engine.camera.rot -= RAY_TWO_PI;

  return 1;
}

int64_t libmod_ray_look_up_down(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;

  float delta = *(float *)&params[0];
  g_engine.camera.pitch += delta;

  /* Limitar pitch */
  const float max_pitch = M_PI / 2.0f * 0.99f;
  if (g_engine.camera.pitch > max_pitch)
    g_engine.camera.pitch = max_pitch;
  if (g_engine.camera.pitch < -max_pitch)
    g_engine.camera.pitch = -max_pitch;

  return 1;
}

int64_t libmod_ray_move_up_down(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;

  float delta = *(float *)&params[0];
  g_engine.camera.z += delta;

  /* Update current sector based on new Z */
  RAY_Sector *sector = ray_find_sector_at_position(
      &g_engine, g_engine.camera.x, g_engine.camera.y, g_engine.camera.z);
  if (sector) {
    g_engine.camera.current_sector_id = sector->sector_id;
  }

  return 1;
}

int64_t libmod_ray_jump(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;

  if (!g_engine.camera.jumping) {
    g_engine.camera.jumping = 1;
    g_engine.camera.heightJumped = 0;
  }

  return 1;
}

/* ============================================================================
   RENDERIZADO
   ============================================================================
 */

int64_t libmod_ray_render(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized) {
    fprintf(stderr, "RAY: Motor no inicializado\n");
    return 0;
  }

  int graph_id = (int)params[0];
  GRAPH *dest = NULL;

  // Si graph_id es 0, crear un nuevo graph automáticamente
  if (graph_id == 0) {
    dest = bitmap_new_syslib(g_engine.displayWidth, g_engine.displayHeight);
    if (!dest) {
      fprintf(stderr, "RAY: No se pudo crear graph\n");
      return 0;
    }
    graph_id = dest->code;
  } else {
    dest = bitmap_get(0, graph_id);
    if (!dest) {
      fprintf(stderr, "RAY: Graph no válido: %d\n", graph_id);
      return 0;
    }
  }

  /* Automatic animation update logic */
  uint32_t current_ticks = SDL_GetTicks();
  float dt = 0.0f;
  if (g_engine.last_ticks > 0) {
    uint32_t diff = current_ticks - g_engine.last_ticks;
    if (diff == 0)
      dt = 0.001f; /* Force minimum progression for high-FPS */
    else
      dt = diff / 1000.0f;
  } else {
    dt = 0.016f;
  }
  g_engine.last_ticks = current_ticks;

  if (dt > 0.1f)
    dt = 0.1f; /* Cap large drops */

  g_engine.time += dt;

  for (int i = 0; i < g_engine.num_sprites; ++i) {
    RAY_Sprite *s = &g_engine.sprites[i];
    if (s->glb_anim_speed != 0) {
      s->glb_anim_time += dt * s->glb_anim_speed;
    }
  }

  /* SOFTWARE RENDERING (Stable - Active) */
  if (!g_use_gpu) {
    ray_render_frame_build(dest);
  } else {
    /* GPU RENDERING (SDL_gpu - Testing) */
    ray_render_frame_gpu(dest);
  }

  return graph_id;
}

/* ============================================================================
   CONFIGURACIÓN
   ============================================================================
 */

int64_t libmod_ray_set_fog(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;

  int enabled = (int)params[0];
  int r = (int)params[1];
  int g = (int)params[2];
  int b = (int)params[3];
  float start_dist = *(float *)&params[4];
  float end_dist = *(float *)&params[5];

  g_engine.fogOn = enabled;
  g_engine.fog_r = r;
  g_engine.fog_g = g;
  g_engine.fog_b = b;
  g_engine.fog_start_distance = start_dist;
  g_engine.fog_end_distance = end_dist;

  return 1;
}

int64_t libmod_ray_set_draw_minimap(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;
  g_engine.drawMiniMap = (int)params[0];
  return 1;
}

int64_t libmod_ray_set_draw_weapon(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;
  g_engine.drawWeapon = (int)params[0];
  return 1;
}

int64_t libmod_ray_set_billboard(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;
  g_engine.billboard_enabled = (int)params[0];
  g_engine.billboard_directions = (int)params[1];
  return 1;
}

int64_t libmod_ray_check_collision(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;

  float x = *(float *)&params[0];
  float y = *(float *)&params[1];
  float new_x = *(float *)&params[2];
  float new_y = *(float *)&params[3];

  return ray_check_collision(&g_engine, x, y, g_engine.camera.z, new_x, new_y,
                             g_engine.default_step_height);
}

int64_t libmod_ray_check_collision_z(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;

  float x = *(float *)&params[0];
  float y = *(float *)&params[1];
  float z = *(float *)&params[2];
  float new_x = *(float *)&params[3];
  float new_y = *(float *)&params[4];

  return ray_check_collision(&g_engine, x, y, z, new_x, new_y,
                             g_engine.default_step_height);
}

int64_t libmod_ray_set_minimap(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;

  g_engine.minimap_size = (int)params[0];
  g_engine.minimap_x = (int)params[1];
  g_engine.minimap_y = (int)params[2];
  g_engine.minimap_scale = *(float *)&params[3];

  return 1;
}

/* ============================================================================
   SPRITES DINÁMICOS
   ============================================================================
 */

int64_t libmod_ray_add_sprite(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;

  float x = *(float *)&params[0];
  float y = *(float *)&params[1];
  float z = *(float *)&params[2];
  int fileID = (int)params[3];
  int textureID = (int)params[4];
  int w = (int)params[5];
  int h = (int)params[6];
  int flags = (int)params[7];

  if (g_engine.num_sprites >= g_engine.sprites_capacity) {
    fprintf(stderr, "RAY: Máximo de sprites alcanzado\n");
    return -1;
  }

  RAY_Sprite *sprite = &g_engine.sprites[g_engine.num_sprites];
  memset(sprite, 0, sizeof(RAY_Sprite));

  sprite->x = x;
  sprite->y = y;
  sprite->z = z;
  sprite->fileID = fileID;
  sprite->textureID = textureID;
  sprite->w = w;
  sprite->h = h;
  sprite->flags = flags;
  sprite->dir = 1;
  sprite->rot = 0.0f;
  sprite->process_ptr = my;
  sprite->flag_id = -1;
  sprite->model_scale = 1.0f;  // Default scale
  sprite->glb_anim_index = -1; // Default: no animation
  sprite->glb_anim_speed = 0.0f;

  g_engine.num_sprites++;

  return g_engine.num_sprites - 1;
}

int64_t libmod_ray_remove_sprite(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;

  int sprite_id = (int)params[0];

  if (sprite_id < 0 || sprite_id >= g_engine.num_sprites) {
    return 0;
  }

  g_engine.sprites[sprite_id].cleanup = 1;

  return 1;
}

int64_t libmod_ray_update_sprite_position(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;

  int sprite_id = (int)params[0];
  float x = *(float *)&params[1];
  float y = *(float *)&params[2];
  float z = *(float *)&params[3];

  if (sprite_id < 0 || sprite_id >= g_engine.num_sprites) {
    return 0;
  }

  g_engine.sprites[sprite_id].x = x;
  g_engine.sprites[sprite_id].y = y;
  g_engine.sprites[sprite_id].z = z;

  return 1;
}

/* ============================================================================
   SPAWN FLAGS
   ============================================================================
 */

int64_t libmod_ray_set_flag(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;

  int flag_id = (int)params[0];

  for (int i = 0; i < g_engine.num_spawn_flags; i++) {
    if (g_engine.spawn_flags[i].flag_id == flag_id) {
      g_engine.spawn_flags[i].occupied = 1;
      g_engine.spawn_flags[i].process_ptr = my;
      return 1;
    }
  }

  return 0;
}

int64_t libmod_ray_clear_flag(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;

  int flag_id = (int)params[0];

  for (int i = 0; i < g_engine.num_spawn_flags; i++) {
    if (g_engine.spawn_flags[i].flag_id == flag_id) {
      g_engine.spawn_flags[i].occupied = 0;
      g_engine.spawn_flags[i].process_ptr = NULL;
      return 1;
    }
  }

  return 0;
}

int64_t libmod_ray_get_camera_sector(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return -1;
  return g_engine.camera.current_sector_id;
}

int64_t libmod_ray_get_flag_x(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;

  int flag_id = (int)params[0];

  for (int i = 0; i < g_engine.num_spawn_flags; i++) {
    if (g_engine.spawn_flags[i].flag_id == flag_id) {
      float val = g_engine.spawn_flags[i].x;
      printf("DEBUG: RAY_GET_FLAG_X(%d) -> Found X=%.2f\n", flag_id, val);
      return (int64_t) * (int32_t *)&val;
    }
  }

  return 0;
}

int64_t libmod_ray_get_flag_y(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;

  int flag_id = (int)params[0];

  for (int i = 0; i < g_engine.num_spawn_flags; i++) {
    if (g_engine.spawn_flags[i].flag_id == flag_id) {
      float val = g_engine.spawn_flags[i].y;
      printf("DEBUG: RAY_GET_FLAG_Y(%d) -> Found Y=%.2f\n", flag_id, val);
      return (int64_t) * (int32_t *)&val;
    }
  }

  return 0;
}

int64_t libmod_ray_get_flag_z(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;

  int flag_id = (int)params[0];

  for (int i = 0; i < g_engine.num_spawn_flags; i++) {
    if (g_engine.spawn_flags[i].flag_id == flag_id) {
      float val = g_engine.spawn_flags[i].z;
      return (int64_t) * (int32_t *)&val;
    }
  }

  return 0;
}

/* ============================================================================
   SKYBOX
   ============================================================================
 */

int64_t libmod_ray_set_sky_texture(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;
  g_engine.skyTextureID = (int)params[0];
  return 1;
}

/* ============================================================================
   DOORS (Legacy - not used in geometric system)
   ============================================================================
 */

int64_t libmod_ray_toggle_door(INSTANCE *my, int64_t *params) {
  // Doors are not implemented in the geometric sector system
  // This is a legacy function kept for API compatibility
  return 0;
}

/* ============================================================================
   MD2 MODELS
   ============================================================================
 */

#include "libmod_ray_camera.h"
#include "libmod_ray_md2.h"
#include "libmod_ray_md3.h"

int64_t libmod_ray_load_md2(INSTANCE *my, int64_t *params) {
  const char *filename = string_get((int)params[0]);
  RAY_MD2_Model *model = ray_md2_load(filename);
  string_discard((int)params[0]);
  if (!model)
    return 0;
  return (int64_t)(intptr_t)model;
}

int64_t libmod_ray_load_md3(INSTANCE *my, int64_t *params) {
  const char *filename = string_get((int)params[0]);
  RAY_MD3_Model *model = ray_md3_load(filename);
  string_discard((int)params[0]);
  if (!model)
    return 0;
  return (int64_t)(intptr_t)model;
}

int64_t libmod_ray_load_gltf(INSTANCE *my, int64_t *params) {
  const char *filename = string_get((int)params[0]);
  RAY_GLTF_Model *model = ray_gltf_load(filename);
  string_discard((int)params[0]);
  if (!model)
    return 0;
  return (int64_t)(intptr_t)model;
}

int64_t libmod_ray_get_gltf_anim_count(INSTANCE *my, int64_t *params) {
  RAY_GLTF_Model *model = (RAY_GLTF_Model *)(intptr_t)params[0];
  if (!model || !model->data)
    return 0;
  return (int64_t)model->data->animations_count;
}

int64_t libmod_ray_set_sprite_md2(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;
  int sprite_id = (int)params[0];
  int64_t model_ptr = params[1];
  int skin_id = (int)params[2]; // Texture ID for skin

  if (sprite_id < 0 || sprite_id >= g_engine.num_sprites)
    return 0;

  RAY_Sprite *s = &g_engine.sprites[sprite_id];
  s->model = (struct RAY_Model *)(intptr_t)model_ptr;
  s->textureID = skin_id; // Set as sprite-specific skin

  if (s->model) {
    int magic = *(int *)s->model;
    if (magic == MD2_MAGIC) {
      // Still set model default as fallback, but sprite skin takes priority
      ((RAY_MD2_Model *)s->model)->textureID = skin_id;
    } else if (magic == MD3_MAGIC) {
      ((RAY_MD3_Model *)s->model)->textureID = skin_id;
    } else if (magic == GLTF_MAGIC) {
      ((RAY_GLTF_Model *)s->model)->textureID = skin_id;
    }
  }
  return 1;
}

int64_t libmod_ray_set_sprite_md3(INSTANCE *my, int64_t *params) {
  return libmod_ray_set_sprite_md2(my, params);
}

int64_t libmod_ray_set_sprite_gltf(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;
  int sprite_id = (int)params[0];
  int64_t model_ptr = params[1];

  if (sprite_id < 0 || sprite_id >= g_engine.num_sprites)
    return 0;

  RAY_Sprite *s = &g_engine.sprites[sprite_id];
  s->model = (struct RAY_Model *)(intptr_t)model_ptr;
  return 1;
}

int64_t libmod_ray_set_sprite_anim(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;
  int sprite_id = (int)params[0];
  int frame = (int)params[1];
  int next_frame = (int)params[2];
  float interp = *(float *)&params[3];

  if (sprite_id < 0 || sprite_id >= g_engine.num_sprites)
    return 0;

  RAY_Sprite *s = &g_engine.sprites[sprite_id];
  s->currentFrame = frame;
  s->nextFrame = next_frame;
  s->interpolation = interp;
  return 1;
}

int64_t libmod_ray_set_sprite_glb_anim(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;
  int sprite_id = (int)params[0];
  int anim_index = (int)params[1];
  float anim_time = *(float *)&params[2];

  if (sprite_id < 0 || sprite_id >= g_engine.num_sprites)
    return 0;

  RAY_Sprite *s = &g_engine.sprites[sprite_id];
  s->glb_anim_index = anim_index;
  s->glb_anim_time = anim_time;
  return 1;
}

int64_t libmod_ray_set_sprite_glb_speed(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;
  int sprite_id = (int)params[0];
  float speed = *(float *)&params[1];

  if (sprite_id < 0 || sprite_id >= g_engine.num_sprites)
    return 0;

  RAY_Sprite *s = &g_engine.sprites[sprite_id];
  s->glb_anim_speed = speed;
  return 1;
}

int64_t libmod_ray_set_sprite_angle(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;

  int sprite_id = (int)params[0];
  float angle = *(float *)&params[1];

  if (sprite_id < 0 || sprite_id >= g_engine.num_sprites)
    return 0;

  // Angle in degrees to radians
  g_engine.sprites[sprite_id].rot = angle * M_PI / 180.0f;

  return 1;
}

int64_t libmod_ray_set_sprite_md3_surface_texture(INSTANCE *my,
                                                  int64_t *params) {
  if (!g_engine.initialized)
    return 0;
  int sprite_id = (int)params[0];
  int surface_idx = (int)params[1];
  int texture_id = (int)params[2];

  if (sprite_id < 0 || sprite_id >= g_engine.num_sprites)
    return 0;

  RAY_Sprite *s = &g_engine.sprites[sprite_id];
  if (surface_idx < 0 || surface_idx >= 32)
    return 0;

  s->md3_surface_textures[surface_idx] = texture_id;
  return 1;
}

int64_t libmod_ray_get_md3_tag(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;
  int sprite_id = (int)params[0];
  const char *tag_name = string_get(params[1]);
  float *out_x = (float *)(intptr_t)params[2];
  float *out_y = (float *)(intptr_t)params[3];
  float *out_z = (float *)(intptr_t)params[4];
  float *out_angle = (float *)(intptr_t)params[5];

  if (sprite_id < 0 || sprite_id >= g_engine.num_sprites) {
    string_discard(params[1]);
    return 0;
  }

  RAY_Sprite *s = &g_engine.sprites[sprite_id];
  if (!s->model || *(int *)s->model != MD3_MAGIC) {
    string_discard(params[1]);
    return 0;
  }

  RAY_MD3_Model *model = (RAY_MD3_Model *)s->model;
  int num_tags = model->header.numTags;
  int current_frame = s->currentFrame;
  int next_frame = s->nextFrame;
  float interp = s->interpolation;

  if (current_frame >= model->header.numFrames)
    current_frame = model->header.numFrames - 1;
  if (next_frame >= model->header.numFrames)
    next_frame = model->header.numFrames - 1;

  int tag_idx = -1;
  for (int i = 0; i < num_tags; i++) {
    if (strcmp(model->tags[i].name, tag_name) == 0) {
      tag_idx = i;
      break;
    }
  }

  if (tag_idx == -1) {
    string_discard(params[1]);
    return 0;
  }

  md3_tag_t *t1 = &model->tags[current_frame * num_tags + tag_idx];
  md3_tag_t *t2 = &model->tags[next_frame * num_tags + tag_idx];

  float lx = t1->origin.x + interp * (t2->origin.x - t1->origin.x);
  float ly = t1->origin.y + interp * (t2->origin.y - t1->origin.y);
  float lz = t1->origin.z + interp * (t2->origin.z - t1->origin.z);

  float cos_model = cosf(s->rot);
  float sin_model = sinf(s->rot);
  float scale_factor = (s->model_scale > 0.0f) ? s->model_scale : 1.0f;

  lx *= scale_factor;
  ly *= scale_factor;
  lz *= scale_factor;

  if (out_x)
    *out_x = lx * cos_model - ly * sin_model + s->x;
  if (out_y)
    *out_y = lx * sin_model + ly * cos_model + s->y;
  if (out_z)
    *out_z = lz + s->z;

  if (out_angle) {
    float local_angle = atan2f(t1->axis[0][1], t1->axis[0][0]);
    *out_angle = (local_angle * 180.0f / M_PI) + (s->rot * 180.0f / M_PI);
  }

  string_discard(params[1]);
  return 1;
}

int64_t libmod_ray_set_sprite_scale(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;

  int sprite_id = (int)params[0];
  float scale = *(float *)&params[1];

  if (sprite_id < 0 || sprite_id >= g_engine.num_sprites)
    return 0;

  g_engine.sprites[sprite_id].model_scale = scale;

  return 1;
}

// Get floor height at x,y
int64_t libmod_ray_get_floor_height(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;

  float x = *(float *)&params[0];
  float y = *(float *)&params[1];

  RAY_Sector *sector = ray_find_sector_at_point(&g_engine, x, y);
  if (sector) {
    float val = sector->floor_z;
    return (int64_t) * (int32_t *)&val;
  }

  return 0;
}

/* ============================================================================
   CÁMARA UPDATE (MOUSE LOOK)
   ============================================================================
 */

int64_t libmod_ray_camera_update(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;

  float sensitivity = *(float *)&params[0];

  int mx, my_pos;
  Uint32 buttons = SDL_GetRelativeMouseState(&mx, &my_pos);

  // Rotate (Yaw) - X axis
  if (mx != 0) {
    g_engine.camera.rot +=
        mx * sensitivity; // Mouse Right -> Rotate Right (Increase Angle)

    // Normalize angle
    while (g_engine.camera.rot < 0)
      g_engine.camera.rot += RAY_TWO_PI;
    while (g_engine.camera.rot >= RAY_TWO_PI)
      g_engine.camera.rot -= RAY_TWO_PI;
  }

  // Pitch (Look Up/Down) - Y axis
  if (my_pos != 0) {
    // Mouse Down (Positive Y) -> Look Down (Decrease Pitch)
    // Mouse Up (Negative Y) -> Look Up (Increase Pitch)
    g_engine.camera.pitch -= my_pos * sensitivity;

    // Clamp pitch
    const float max_pitch = M_PI / 2.0f * 0.99f;
    if (g_engine.camera.pitch > max_pitch)
      g_engine.camera.pitch = max_pitch;
    if (g_engine.camera.pitch < -max_pitch)
      g_engine.camera.pitch = -max_pitch;
  }

  return 1;
}

/* ============================================================================
   MD3 TAG SYSTEM
   ============================================================================
 */

int64_t libmod_ray_get_tag_point(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;

  int sprite_id = (int)params[0];
  const char *tag_name = string_get((int)params[1]);
  int *ptr_x = (int *)params[2]; // Pointers to FLOAT variables
  int *ptr_y = (int *)params[3];
  int *ptr_z = (int *)params[4];

  // Default return
  int result = 0;

  if (sprite_id >= 0 && sprite_id < g_engine.num_sprites) {
    RAY_Sprite *s = &g_engine.sprites[sprite_id];

    if (s->model && (*(int *)s->model) == MD3_MAGIC) {
      RAY_MD3_Model *model = (RAY_MD3_Model *)s->model;

      // Safe Frame Clamping
      int frame1 = s->currentFrame;
      int frame2 = s->nextFrame;
      if (frame1 >= model->header.numFrames)
        frame1 = model->header.numFrames - 1;
      if (frame2 >= model->header.numFrames)
        frame2 = model->header.numFrames - 1;
      if (frame1 < 0)
        frame1 = 0;
      if (frame2 < 0)
        frame2 = 0;

      // Search Tag
      md3_tag_t *tag1 = NULL;
      md3_tag_t *tag2 = NULL;

      int offset1 = frame1 * model->header.numTags;
      int offset2 = frame2 * model->header.numTags;

      for (int i = 0; i < model->header.numTags; i++) {
        if (strcmp(model->tags[offset1 + i].name, tag_name) == 0) {
          tag1 = &model->tags[offset1 + i];
          tag2 = &model->tags[offset2 + i];
          break;
        }
      }

      if (tag1 && tag2) {
        // Found! Interpolate
        // NOTE: MD3 tags are ALREADY in world units (floats), unlike vertices
        // which are compressed int16 So we do NOT apply the 1/64 scale here
        float interp = s->interpolation;

        float lx = tag1->origin.x + interp * (tag2->origin.x - tag1->origin.x);
        float ly = tag1->origin.y + interp * (tag2->origin.y - tag1->origin.y);
        float lz = tag1->origin.z + interp * (tag2->origin.z - tag1->origin.z);

        // Transform to World Space
        float cos_rot = cos(s->rot);
        float sin_rot = sin(s->rot);

        // MD3 standard: X=Forward, Y=Left, Z=Up
        // Bennu World: Raycasting Logic uses s->rot (radians)
        // Rotated X = x*cos - y*sin
        // Rotated Y = x*sin + y*cos

        float wx = lx * cos_rot - ly * sin_rot + s->x;
        float wy = lx * sin_rot + ly * cos_rot + s->y;
        float wz = lz + s->z;

        if (ptr_x)
          *(float *)ptr_x = wx;
        if (ptr_y)
          *(float *)ptr_y = wy;
        if (ptr_z)
          *(float *)ptr_z = wz;

        result = 1;
      }
    }
  }

  string_discard((int)params[1]);
  return result;
}

/* ============================================================================
   ILUMINACIÓN
   ============================================================================
 */

int64_t libmod_ray_add_light(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized || g_engine.num_lights >= RAY_MAX_LIGHTS)
    return -1;

  RAY_Light *l = &g_engine.lights[g_engine.num_lights];
  l->x = *(float *)&params[0];
  l->y = *(float *)&params[1];
  l->z = *(float *)&params[2];

  /* Color: params[3]=R, params[4]=G, params[5]=B (0-255) */
  l->r = (float)params[3] / 255.0f;
  l->g = (float)params[4] / 255.0f;
  l->b = (float)params[5] / 255.0f;

  l->intensity = *(float *)&params[6];
  l->falloff = *(float *)&params[7];

  printf("RAY: Light added at (%.1f, %.1f, %.1f) Color RGB=(%.2f, %.2f, %.2f) "
         "Intensity=%.1f\n",
         l->x, l->y, l->z, l->r, l->g, l->b, l->intensity);

  return g_engine.num_lights++;
}

int64_t libmod_ray_clear_lights(INSTANCE *my, int64_t *params) {
  /* v25: Lights are loaded from .raymap file - don't clear them */
  printf("RAY: WARNING - RAY_LIGHT_CLEAR() called but lights are map-owned. "
         "Ignoring.\n");
  return 1;
}

int64_t libmod_ray_set_texture_quality(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;
  g_engine.texture_quality = (int)params[0];
  return 1;
}

int64_t libmod_ray_set_collision_box(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;
  int sprite_id = (int)params[0];
  float w = *(float *)&params[1];
  float d = *(float *)&params[2]; // Parameter 2 is usually Depth in the editor
  float h = *(float *)&params[3]; // Parameter 3 is Height

  if (sprite_id >= 0 && sprite_id < g_engine.num_sprites) {
    g_engine.sprites[sprite_id].col_w = w;
    g_engine.sprites[sprite_id].col_h = h;
    g_engine.sprites[sprite_id].col_d = d;
    return 1;
  }
  return 0;
}

int64_t libmod_ray_get_collision(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return -1;
  int sprite_id = (int)params[0];
  if (sprite_id < 0 || sprite_id >= g_engine.num_sprites)
    return -1;

  RAY_Sprite *s1 = &g_engine.sprites[sprite_id];
  if (s1->cleanup)
    return -1;

  for (int i = 0; i < g_engine.num_sprites; i++) {
    if (i == sprite_id)
      continue;
    RAY_Sprite *s2 = &g_engine.sprites[i];
    if (s2->cleanup || s2->hidden)
      continue;

    // Check intersection (Simple AABB)
    if (fabsf(s1->x - s2->x) < (s1->col_w + s2->col_w) * 0.5f &&
        fabsf(s1->y - s2->y) < (s1->col_d + s2->col_d) * 0.5f &&
        fabsf(s1->z - s2->z) < (s1->col_h + s2->col_h) * 0.5f) {
      return i;
    }
  }
  return -1;
}

int64_t libmod_ray_set_sprite_flags(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;
  int sprite_id = (int)params[0];
  int flags = (int)params[1];
  if (sprite_id >= 0 && sprite_id < g_engine.num_sprites) {
    g_engine.sprites[sprite_id].flags = flags;
    // Handle specific flags (e.g., bit 0 = invisible)
    if (flags & 1)
      g_engine.sprites[sprite_id].hidden = 1;
    else
      g_engine.sprites[sprite_id].hidden = 0;
    return 1;
  }
  return 0;
}

int64_t libmod_ray_set_sprite_graph(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;
  int sprite_id = (int)params[0];
  int graph = (int)params[1];
  if (sprite_id >= 0 && sprite_id < g_engine.num_sprites) {
    g_engine.sprites[sprite_id].textureID = graph;
    return 1;
  }
  return 0;
}

int64_t libmod_ray_camera_load(INSTANCE *my, int64_t *params) {
  const char *filename = string_get((int)params[0]);
  int id = ray_camera_load_path(filename);
  string_discard((int)params[0]);
  return id;
}

int64_t libmod_ray_camera_play(INSTANCE *my, int64_t *params) {
  int id = (int)params[0];
  ray_camera_play_path(id);
  return 1;
}

int64_t libmod_ray_camera_is_playing(INSTANCE *my, int64_t *params) {
  return ray_camera_is_playing();
}

int64_t libmod_ray_camera_path_update(INSTANCE *my, int64_t *params) {
  float dt = *(float *)&params[0];
  ray_camera_update(dt);

  if (ray_camera_is_playing()) {
    CameraState state;
    ray_camera_get_state(&state);
    g_engine.camera.x = state.x;
    g_engine.camera.y = state.y;
    g_engine.camera.z = state.z;
    g_engine.camera.rot = state.yaw;
    g_engine.camera.pitch = state.pitch;

    // Actualizar FOV si está presente en la secuencia
    if (state.fov > 0) {
      g_engine.fovDegrees = (int)state.fov;
      g_engine.fovRadians = state.fov * M_PI / 180.0f;
    }
  }
  return 1;
}

int64_t libmod_ray_set_fov(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;
  float fov = *(float *)&params[0];
  g_engine.fovDegrees = (int)fov;
  g_engine.fovRadians = fov * M_PI / 180.0f;
  return 1;
}

int64_t libmod_ray_camera_stop(INSTANCE *my, int64_t *params) {
  ray_camera_stop_path();
  return 1;
}

int64_t libmod_ray_camera_pause(INSTANCE *my, int64_t *params) {
  ray_camera_pause_path();
  return 1;
}

int64_t libmod_ray_camera_resume(INSTANCE *my, int64_t *params) {
  ray_camera_resume_path();
  return 1;
}

int64_t libmod_ray_camera_get_time(INSTANCE *my, int64_t *params) {
  float t = ray_camera_get_time();
  return *(int32_t *)&t;
}

int64_t libmod_ray_camera_set_time(INSTANCE *my, int64_t *params) {
  float t = *(float *)&params[0];
  ray_camera_set_time(t);
  return 1;
}

int64_t libmod_ray_camera_free(INSTANCE *my, int64_t *params) {
  int id = (int)params[0];
  ray_camera_free_path(id);
  return 1;
}

/* ============================================================================
   FÍSICA Y MOVIMIENTO AVANZADO
   ============================================================================
 */

int64_t libmod_ray_set_step_height(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;
  g_engine.default_step_height = *(float *)&params[0];
  return 1;
}

int64_t libmod_ray_get_floor_height_z(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;

  float x = *(float *)&params[0];
  float y = *(float *)&params[1];
  float z = *(float *)&params[2];

  RAY_Sector *sector = ray_find_sector_at_position(&g_engine, x, y, z);
  if (sector) {
    float val = sector->floor_z;
    return (int64_t) * (int32_t *)&val;
  }

  return 0;
}

int64_t libmod_ray_move_sprite(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;

  int sprite_id = (int)params[0];
  float dist = *(float *)&params[1];
  float step_h = *(float *)&params[2];

  if (sprite_id < 0 || sprite_id >= g_engine.num_sprites)
    return 0;

  RAY_Sprite *s = &g_engine.sprites[sprite_id];
  if (s->cleanup)
    return 0;

  // Si step_h es 0 o negativo, usar el valor por defecto del motor
  if (step_h <= 0)
    step_h = g_engine.default_step_height;

  float newX = s->x + cosf(s->rot) * dist;
  float newY = s->y + sinf(s->rot) * dist;

  // Guardar altura original para cálculos de climbing
  float oldZ = s->z;

  // Temporalmente cambiar step_height global para usar el check_collision
  // existente (O podríamos añadir un parámetro a ray_check_collision, pero esto
  // es más rápido sin cambiar muchas firmas)
  float old_engine_step = g_engine.default_step_height;
  g_engine.default_step_height = step_h;

  int blocked =
      ray_check_collision(&g_engine, s->x, s->y, s->z, newX, newY, step_h);

  g_engine.default_step_height = old_engine_step;

  if (!blocked) {
    s->x = newX;
    s->y = newY;

    // Actualizar Z automáticamente (climbing)
    RAY_Sector *sector =
        ray_find_sector_at_position(&g_engine, newX, newY, s->z);
    if (sector) {
      // Si entramos en un sector sólido (bloque), subimos a su techo
      if (ray_sector_is_solid(sector)) {
        float floor_top = sector->ceiling_z;
        if (floor_top > s->z && floor_top <= s->z + step_h) {
          s->z = floor_top + 0.1f; // Pequeño margen para no estar "dentro"
        }
      } else {
        // Sector normal, ajustar a su suelo
        s->z = sector->floor_z;
      }
    }
    return 1; // Movido
  }

  return 0; // Bloqueado
}

int64_t libmod_ray_check_collision_h(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return 0;

  float x = *(float *)&params[0];
  float y = *(float *)&params[1];
  float z = *(float *)&params[2];
  float new_x = *(float *)&params[3];
  float new_y = *(float *)&params[4];
  float step_h = *(float *)&params[5];

  return ray_check_collision(&g_engine, x, y, z, new_x, new_y, step_h);
}

int64_t libmod_ray_check_sprite_collision(INSTANCE *my, int64_t *params) {
  if (!g_engine.initialized)
    return -1;

  int self_id = (int)params[0];
  float new_x = *(float *)&params[1];
  float new_y = *(float *)&params[2];
  float radius = *(float *)&params[3];

  if (radius <= 0.0f)
    radius = 32.0f;

  for (int i = 0; i < g_engine.num_sprites; i++) {
    if (i == self_id)
      continue;
    RAY_Sprite *other = &g_engine.sprites[i];
    if (other->hidden || other->cleanup)
      continue;

    float other_radius =
        (other->col_w > 0) ? other->col_w / 2.0f : other->w / 2.0f;
    float min_dist = radius + other_radius;

    float dx = new_x - other->x;
    float dy = new_y - other->y;
    float dist_sq = dx * dx + dy * dy;

    if (dist_sq < min_dist * min_dist) {
      float self_z = 0;
      if (self_id >= 0 && self_id < g_engine.num_sprites) {
        self_z = g_engine.sprites[self_id].z;
      }
      float self_h = (self_id >= 0 && self_id < g_engine.num_sprites)
                         ? (g_engine.sprites[self_id].col_h > 0
                                ? g_engine.sprites[self_id].col_h
                                : g_engine.sprites[self_id].h)
                         : 64.0f;
      float other_h = (other->col_h > 0) ? other->col_h : other->h;

      if (self_z < other->z + other_h && self_z + self_h > other->z) {
        return i;
      }
    }
  }
  return -1;
}

void __bgdexport(libmod_ray, module_initialize)() {}
void __bgdexport(libmod_ray, module_finalize)() {}

#include "libmod_ray_exports.h"