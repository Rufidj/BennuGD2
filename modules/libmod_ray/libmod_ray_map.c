/*
 * libmod_ray_map.c - Map Loading/Saving for Format v8
 * Complete rewrite - geometric sectors only
 */

#include "libmod_ray.h"
#include "libmod_ray_compat.h"
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* External engine instance */
extern RAY_Engine g_engine;

/* External geometry functions */
extern void ray_detect_portals(RAY_Engine *engine);

/* Forward declaration for nested sector portal detection */
static void ray_detect_shared_walls_with_children(void);
static void ray_detect_all_shared_walls(void);
static void ray_reconstruct_hierarchy(void);
static int ray_point_in_sector_local(RAY_Sector *sector, float px, float py);
static float ray_sector_area(RAY_Sector *s);

/* ============================================================================
   MAP HEADER v8
   ============================================================================
 */

typedef struct {
  char magic[8];    /* "RAYMAP\x1a" */
  uint32_t version; /* 8 */
  uint32_t num_sectors;
  uint32_t num_portals;
  uint32_t num_sprites;
  uint32_t num_spawn_flags;
  float camera_x, camera_y, camera_z;
  float camera_rot, camera_pitch;
  int32_t skyTextureID;
} RAY_MapHeader_v8;

/* ============================================================================
   MAP LOADING
   ============================================================================
 */

/* ============================================================================
   MAP HEADER v9
   ============================================================================
 */

typedef struct {
  char magic[8];    /* "RAYMAP\x1a" */
  uint32_t version; /* 9 */
  uint32_t num_sectors;
  uint32_t num_portals;
  uint32_t num_sprites;
  uint32_t num_spawn_flags;
  float camera_x, camera_y, camera_z;
  float camera_rot, camera_pitch;
  int32_t skyTextureID;
} RAY_MapHeader_v9;

/* ============================================================================
   MAP SAVING (V9)
   ============================================================================
 */

int ray_save_map_v9(const char *filename) {
  FILE *file = fopen(filename, "wb");
  if (!file) {
    fprintf(stderr, "RAY: Error creating file %s\n", filename);
    return 0;
  }

  printf("RAY: Saving map v9 to %s...\n", filename);

  /* 1. Header */
  RAY_MapHeader_v9 header;
  memcpy(header.magic, "RAYMAP\x1a", 8);
  header.version = 27; /* Updated for liquid_speed support */
  header.num_sectors = g_engine.num_sectors;
  header.num_portals = g_engine.num_portals;
  header.num_sprites = g_engine.num_sprites;
  header.num_spawn_flags = g_engine.num_spawn_flags;
  header.camera_x = g_engine.camera.x;
  header.camera_y = g_engine.camera.y;
  header.camera_z = g_engine.camera.z;
  header.camera_rot = g_engine.camera.rot;
  header.camera_pitch = g_engine.camera.pitch;
  header.skyTextureID = g_engine.skyTextureID;

  fwrite(&header, sizeof(RAY_MapHeader_v9), 1, file);

  /* 2. Sectors */
  for (int i = 0; i < g_engine.num_sectors; i++) {
    RAY_Sector *s = &g_engine.sectors[i];

    fwrite(&s->sector_id, sizeof(int), 1, file);
    fwrite(&s->floor_z, sizeof(float), 1, file);
    fwrite(&s->ceiling_z, sizeof(float), 1, file);
    fwrite(&s->floor_texture_id, sizeof(int), 1, file);
    fwrite(&s->ceiling_texture_id, sizeof(int), 1, file);
    fwrite(&s->light_level, sizeof(int), 1, file);

    /* Vertices */
    fwrite(&s->num_vertices, sizeof(int), 1, file);
    for (int v = 0; v < s->num_vertices; v++) {
      fwrite(&s->vertices[v].x, sizeof(float), 1, file);
      fwrite(&s->vertices[v].y, sizeof(float), 1, file);
    }

    /* Walls */
    fwrite(&s->num_walls, sizeof(int), 1, file);
    for (int w = 0; w < s->num_walls; w++) {
      RAY_Wall *wall = &s->walls[w];
      fwrite(&wall->wall_id, sizeof(int), 1, file);
      fwrite(&wall->x1, sizeof(float), 1, file);
      fwrite(&wall->y1, sizeof(float), 1, file);
      fwrite(&wall->x2, sizeof(float), 1, file);
      fwrite(&wall->y2, sizeof(float), 1, file);
      fwrite(&wall->texture_id_lower, sizeof(int), 1, file);
      fwrite(&wall->texture_id_middle, sizeof(int), 1, file);
      fwrite(&wall->texture_id_upper, sizeof(int), 1, file);
      fwrite(&wall->texture_split_z_lower, sizeof(float), 1, file);
      fwrite(&wall->texture_split_z_upper, sizeof(float), 1, file);
      fwrite(&wall->portal_id, sizeof(int), 1, file);
      fwrite(&wall->flags, sizeof(int), 1, file);
    }

    /* Removed: Hierarchy fields */
  }

  /* 3. Portals */
  for (int i = 0; i < g_engine.num_portals; i++) {
    RAY_Portal *p = &g_engine.portals[i];
    fwrite(&p->portal_id, sizeof(int), 1, file);
    fwrite(&p->sector_a, sizeof(int), 1, file);
    fwrite(&p->sector_b, sizeof(int), 1, file);
    fwrite(&p->wall_id_a, sizeof(int), 1, file);
    fwrite(&p->wall_id_b, sizeof(int), 1, file);
    fwrite(&p->x1, sizeof(float), 1, file);
    fwrite(&p->y1, sizeof(float), 1, file);
    fwrite(&p->x2, sizeof(float), 1, file);
    fwrite(&p->y2, sizeof(float), 1, file);
  }

  /* 4. Sprites */
  for (int i = 0; i < g_engine.num_sprites; i++) {
    RAY_Sprite *s = &g_engine.sprites[i];
    fwrite(&s->textureID, sizeof(int), 1, file);
    fwrite(&s->x, sizeof(float), 1, file);
    fwrite(&s->y, sizeof(float), 1, file);
    fwrite(&s->z, sizeof(float), 1, file);
    fwrite(&s->w, sizeof(int), 1, file);
    fwrite(&s->h, sizeof(int), 1, file);
    fwrite(&s->rot, sizeof(float), 1, file);
    // NOTE: runtime fields like 'visible' or animation state are NOT saved
  }

  /* 5. Spawn Flags */
  for (int i = 0; i < g_engine.num_spawn_flags; i++) {
    RAY_SpawnFlag *f = &g_engine.spawn_flags[i];
    fwrite(&f->flag_id, sizeof(int), 1, file);
    fwrite(&f->x, sizeof(float), 1, file);
    fwrite(&f->y, sizeof(float), 1, file);
    fwrite(&f->z, sizeof(float), 1, file);
  }

  fclose(file);
  printf("RAY: Saved map v9 (%d sectors, %d portals)\n", g_engine.num_sectors,
         g_engine.num_portals);
  return 1;
}

/* ============================================================================
   MAP LOADING (V9)
   ============================================================================
 */

/* Forward declarations for portal detection */
static void ray_detect_all_shared_walls(void);
static void ray_detect_nested_sectors(void);

int ray_load_map_v9(FILE *file, RAY_MapHeader_v9 *header) {
  if (!file || !header)
    return 0;

  int map_version = header->version;
  if (map_version < 9 || map_version > 27) {
    printf("RAY: ERROR - Map version %d not supported by this engine (9-27 "
           "only)\n",
           map_version);
    return 0;
  }
  printf("RAY: Loading Map v%d (%d sectors)...\n", map_version,
         header->num_sectors);

  /* 1. Allocate memory */
  g_engine.num_sectors = 0;
  if (header->num_sectors > 0) {
    // free existing if any? Assuming clean slate or handled by caller (like
    // ray_unload_map)
    g_engine.sectors =
        (RAY_Sector *)calloc(header->num_sectors, sizeof(RAY_Sector));
    g_engine.sectors_capacity = header->num_sectors;
  }
  g_engine.num_portals = 0;
  if (header->num_portals > 0) {
    g_engine.portals =
        (RAY_Portal *)calloc(header->num_portals, sizeof(RAY_Portal));
    g_engine.portals_capacity = header->num_portals;
  }
  g_engine.num_sprites = 0;
  if (header->num_sprites > 0) {
    g_engine.sprites =
        (RAY_Sprite *)calloc(header->num_sprites, sizeof(RAY_Sprite));
    g_engine.sprites_capacity = header->num_sprites;
  }
  g_engine.num_spawn_flags = 0;
  if (header->num_spawn_flags > 0) {
    g_engine.spawn_flags =
        (RAY_SpawnFlag *)calloc(header->num_spawn_flags, sizeof(RAY_SpawnFlag));
    g_engine.spawn_flags_capacity = header->num_spawn_flags;
  }

  /* 2. Load basic state */
  g_engine.camera.x = header->camera_x;
  g_engine.camera.y = header->camera_y;
  g_engine.camera.z = header->camera_z;
  g_engine.camera.rot = header->camera_rot;
  g_engine.camera.pitch = header->camera_pitch;
  g_engine.skyTextureID = header->skyTextureID;

  /* 3. Sectors */
  for (int i = 0; i < header->num_sectors; i++) {
    RAY_Sector *s = &g_engine.sectors[i];

    if (fread(&s->sector_id, sizeof(int), 1, file) != 1) {
      fprintf(stderr, "RAY: Error reading sector %d header\n", i);
      break;
    }
    (void)fread(&s->floor_z, sizeof(float), 1, file);
    (void)fread(&s->ceiling_z, sizeof(float), 1, file);
    (void)fread(&s->floor_texture_id, sizeof(int), 1, file);
    (void)fread(&s->ceiling_texture_id, sizeof(int), 1, file);
    (void)fread(&s->light_level, sizeof(int), 1, file);

    /* v24+: Normal maps */
    if (map_version >= 24) {
      (void)fread(&s->floor_normal_id, sizeof(int), 1, file);
      (void)fread(&s->ceiling_normal_id, sizeof(int), 1, file);
    } else {
      s->floor_normal_id = 0;
      s->ceiling_normal_id = 0;
    }

    /* v22+: Sector flags */
    if (map_version >= 22) {
      (void)fread(&s->flags, sizeof(int), 1, file);
    } else {
      s->flags = 0;
    }

    /* v26+: Liquid settings */
    if (map_version >= 26) {
      (void)fread(&s->liquid_intensity, sizeof(float), 1, file);
    } else {
      s->liquid_intensity = 1.0f;
    }
    if (map_version >= 27) {
      (void)fread(&s->liquid_speed, sizeof(float), 1, file);
    } else {
      s->liquid_speed = 1.0f;
    }

    printf("RAY: Loading sector %d: floor_z=%.1f, ceiling_z=%.1f\n",
           s->sector_id, s->floor_z, s->ceiling_z);

    /* Vertices */
    (void)fread(&s->num_vertices, sizeof(int), 1, file);
    s->vertices_capacity = s->num_vertices > RAY_MAX_VERTICES_PER_SECTOR
                               ? s->num_vertices
                               : RAY_MAX_VERTICES_PER_SECTOR;
    s->vertices = (RAY_Point *)calloc(s->vertices_capacity, sizeof(RAY_Point));
    for (int v = 0; v < s->num_vertices; v++) {
      (void)fread(&s->vertices[v].x, sizeof(float), 1, file);
      (void)fread(&s->vertices[v].y, sizeof(float), 1, file);
    }
    printf("RAY:   %d vertices loaded\n", s->num_vertices);

    /* Walls */
    (void)fread(&s->num_walls, sizeof(int), 1, file);
    s->walls_capacity = s->num_walls > RAY_MAX_WALLS_PER_SECTOR
                            ? s->num_walls
                            : RAY_MAX_WALLS_PER_SECTOR;
    s->walls = (RAY_Wall *)calloc(s->walls_capacity, sizeof(RAY_Wall));
    for (int w = 0; w < s->num_walls; w++) {
      RAY_Wall *wall = &s->walls[w];
      (void)fread(&wall->wall_id, sizeof(int), 1, file);
      (void)fread(&wall->x1, sizeof(float), 1, file);
      (void)fread(&wall->y1, sizeof(float), 1, file);
      (void)fread(&wall->x2, sizeof(float), 1, file);
      (void)fread(&wall->y2, sizeof(float), 1, file);
      (void)fread(&wall->texture_id_lower, sizeof(int), 1, file);
      (void)fread(&wall->texture_id_middle, sizeof(int), 1, file);
      (void)fread(&wall->texture_id_upper, sizeof(int), 1, file);
      (void)fread(&wall->texture_split_z_lower, sizeof(float), 1, file);
      (void)fread(&wall->texture_split_z_upper, sizeof(float), 1, file);
      (void)fread(&wall->portal_id, sizeof(int), 1, file);
      (void)fread(&wall->flags, sizeof(int), 1, file);

      /* v24+: Normal maps */
      if (map_version >= 24) {
        (void)fread(&wall->texture_id_lower_normal, sizeof(int), 1, file);
        (void)fread(&wall->texture_id_middle_normal, sizeof(int), 1, file);
        (void)fread(&wall->texture_id_upper_normal, sizeof(int), 1, file);
      } else {
        wall->texture_id_lower_normal = 0;
        wall->texture_id_middle_normal = 0;
        wall->texture_id_upper_normal = 0;
      }
    }
    printf("RAY:   %d walls loaded\n", s->num_walls);

    // Try to load hierarchy fields (may not exist in old maps)
    // Check if there's more data to read
    long current_pos = ftell(file);
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, current_pos, SEEK_SET);

    // If there's enough data for hierarchy fields, read them
    if (current_pos + sizeof(int) * 2 <= file_size) {
      (void)fread(&s->parent_sector_id, sizeof(int), 1, file);
      (void)fread(&s->num_children, sizeof(int), 1, file);

      if (s->num_children > 0 && s->num_children < 100) { // Sanity check
        s->children_capacity = s->num_children;
        s->child_sector_ids = (int *)malloc(s->num_children * sizeof(int));
        for (int c = 0; c < s->num_children; c++) {
          (void)fread(&s->child_sector_ids[c], sizeof(int), 1, file);
        }
        printf("RAY:   Sector %d: parent=%d, children=%d\n", s->sector_id,
               s->parent_sector_id, s->num_children);
      } else {
        // Invalid data, reset to defaults
        s->parent_sector_id = -1;
        s->child_sector_ids = NULL;
        s->num_children = 0;
        s->children_capacity = 0;
      }
    } else {
      // Old map format without hierarchy
      s->parent_sector_id = -1;
      s->child_sector_ids = NULL;
      s->num_children = 0;
      s->children_capacity = 0;
    }

    // Allocate portal_ids array for the sector (runtime lookup)
    // We'll populate this when we read the portal list later
    s->portals_capacity =
        RAY_MAX_WALLS_PER_SECTOR; // Enough for 1 portal per wall
    s->portal_ids = (int *)calloc(s->portals_capacity, sizeof(int));
    s->num_portals = 0;

    // Increment sector count as we successfully load each one
    g_engine.num_sectors++;

    // Calculate basic AABB
    s->min_x = FLT_MAX;
    s->min_y = FLT_MAX;
    s->max_x = -FLT_MAX;
    s->max_y = -FLT_MAX;
    for (int v = 0; v < s->num_vertices; v++) {
      if (s->vertices[v].x < s->min_x)
        s->min_x = s->vertices[v].x;
      if (s->vertices[v].x > s->max_x)
        s->max_x = s->vertices[v].x;
      if (s->vertices[v].y < s->min_y)
        s->min_y = s->vertices[v].y;
      if (s->vertices[v].y > s->max_y)
        s->max_y = s->vertices[v].y;
    }
  }

  printf("RAY: Loaded %d sectors (expected %d)\n", g_engine.num_sectors,
         header->num_sectors);

  /* 4. Portals */
  for (int i = 0; i < header->num_portals; i++) {
    RAY_Portal *p = &g_engine.portals[i];
    (void)fread(&p->portal_id, sizeof(int), 1, file);
    (void)fread(&p->sector_a, sizeof(int), 1, file);
    (void)fread(&p->sector_b, sizeof(int), 1, file);
    (void)fread(&p->wall_id_a, sizeof(int), 1, file);
    (void)fread(&p->wall_id_b, sizeof(int), 1, file);
    (void)fread(&p->x1, sizeof(float), 1, file);
    (void)fread(&p->y1, sizeof(float), 1, file);
    (void)fread(&p->x2, sizeof(float), 1, file);
    (void)fread(&p->y2, sizeof(float), 1, file);

    // Re-link sector's portal list
    if (p->sector_a >= 0 && p->sector_a < g_engine.num_sectors) {
      RAY_Sector *sa = &g_engine.sectors[p->sector_a];
      if (sa->num_portals < sa->portals_capacity)
        sa->portal_ids[sa->num_portals++] = p->portal_id;
    }
    if (p->sector_b >= 0 && p->sector_b < g_engine.num_sectors) {
      RAY_Sector *sb = &g_engine.sectors[p->sector_b];
      if (sb->num_portals < sb->portals_capacity)
        sb->portal_ids[sb->num_portals++] = p->portal_id;
    }
  }
  g_engine.num_portals = header->num_portals;

  /* 5. Sprites */
  for (int i = 0; i < header->num_sprites; i++) {
    RAY_Sprite *s = &g_engine.sprites[i];
    (void)fread(&s->textureID, sizeof(int), 1, file);
    (void)fread(&s->x, sizeof(float), 1, file);
    (void)fread(&s->y, sizeof(float), 1, file);
    (void)fread(&s->z, sizeof(float), 1, file);
    (void)fread(&s->w, sizeof(int), 1, file);
    (void)fread(&s->h, sizeof(int), 1, file);
    (void)fread(&s->rot, sizeof(float), 1, file);
  }
  g_engine.num_sprites = header->num_sprites;

  /* 6. Spawn Flags */
  for (int i = 0; i < header->num_spawn_flags; i++) {
    RAY_SpawnFlag *f = &g_engine.spawn_flags[i];
    (void)fread(&f->flag_id, sizeof(int), 1, file);
    (void)fread(&f->x, sizeof(float), 1, file);
    (void)fread(&f->y, sizeof(float), 1, file);
    (void)fread(&f->z, sizeof(float), 1, file);
  }
  g_engine.num_spawn_flags = header->num_spawn_flags;

  /* 7. Auto-detect portals (Build Engine style) */
  printf("RAY: Auto-detecting portals between sectors...\n");
  printf("RAY: Preserving %d manual portals from file\n", g_engine.num_portals);

  // NOTE: We do NOT clear portals here - we preserve manual portals from the
  // file and add auto-detected ones for shared walls

  ray_detect_all_shared_walls();
  // ray_detect_nested_sectors();  // DISABLED: Build Engine doesn't auto-create
  // portals for nested sectors
  printf("RAY: Portal detection complete. Total portals: %d\n",
         g_engine.num_portals);

  /* 8. Reconstruct hierarchy (Handles nested sectors for maps that don't save
   * it) */
  ray_reconstruct_hierarchy();

  /* 9. Detect camera's starting sector */
  g_engine.camera.current_sector_id = -1;
  float min_area = FLT_MAX;

  for (int i = 0; i < g_engine.num_sectors; i++) {
    RAY_Sector *s = &g_engine.sectors[i];
    if (ray_point_in_sector_local(s, g_engine.camera.x, g_engine.camera.y)) {
      // Pick the smallest sector (highest nesting level)
      float area = ray_sector_area(s);
      if (area < min_area) {
        min_area = area;
        g_engine.camera.current_sector_id = s->sector_id;
      }
    }
  }

  if (g_engine.camera.current_sector_id != -1) {
    printf("RAY: Camera starts in sector %d (Area=%.1f)\n",
           g_engine.camera.current_sector_id, min_area);
  } else {
    printf("RAY: WARNING: Camera started outside any sector!\n");
  }
  if (g_engine.camera.current_sector_id < 0) {
    printf("RAY: WARNING - Camera not inside any sector! Defaulting to sector "
           "0\n");
    if (g_engine.num_sectors > 0) {
      g_engine.camera.current_sector_id = g_engine.sectors[0].sector_id;
    }
  }

  /* 8.5 Skip entities and paths - read lights from END of file instead.
   * Lights are always the LAST section in the .raymap file (v25).
   * Format: num_lights(4) + N * 36 bytes (id,x,y,z,r,g,b,intensity,falloff)
   * We try different counts starting from 0 to find the right one. */

  /* 9. Light Points (v25+) - Read from end of file */
  if (map_version >= 25) {
    /* Save current position */
    long saved_pos = ftell(file);

    /* Get file size */
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);

    /* The lights section is at the end:
     * [num_lights: 4 bytes][light0: 36 bytes][light1: 36 bytes]...
     * So we read num_lights from (file_end - 4 - N*36)
     * But we don't know N yet. Read the num_lights first by trying end-4. */

    /* First, read the candidate num_lights from different positions.
     * Strategy: seek to positions where num_lights COULD be, check if the
     * value makes sense (0-16), and if the offset math adds up. */
    int found = 0;
    for (uint32_t try_n = 0; try_n <= RAY_MAX_LIGHTS && !found; try_n++) {
      long candidate_pos = file_size - 4 - (long)try_n * 36;
      if (candidate_pos < saved_pos)
        break; /* Can't be before the entities section */

      fseek(file, candidate_pos, SEEK_SET);
      uint32_t num_l = 0;
      if (fread(&num_l, sizeof(uint32_t), 1, file) == 1) {
        if (num_l == try_n) {
          /* Found it! num_lights matches the position */
          printf("RAY: Found lights section at offset %ld (num_lights=%u)\n",
                 candidate_pos, num_l);
          g_engine.num_lights =
              (num_l < RAY_MAX_LIGHTS) ? num_l : RAY_MAX_LIGHTS;
          for (int i = 0; i < g_engine.num_lights; i++) {
            struct {
              int32_t id;
              float x, y, z;
              int32_t r, g, b;
              float intensity, falloff;
            } temp_l;
            if (fread(&temp_l, sizeof(temp_l), 1, file) == 1) {
              g_engine.lights[i].x = temp_l.x;
              g_engine.lights[i].y = temp_l.y;
              g_engine.lights[i].z = temp_l.z;
              g_engine.lights[i].r = (float)temp_l.r / 255.0f;
              g_engine.lights[i].g = (float)temp_l.g / 255.0f;
              g_engine.lights[i].b = (float)temp_l.b / 255.0f;
              g_engine.lights[i].intensity = temp_l.intensity;
              g_engine.lights[i].falloff = temp_l.falloff;
              printf("RAY:   Light[%d] pos=(%.1f, %.1f, %.1f) "
                     "rgb=(%d,%d,%d) intensity=%.1f falloff=%.1f\n",
                     i, temp_l.x, temp_l.y, temp_l.z, temp_l.r, temp_l.g,
                     temp_l.b, temp_l.intensity, temp_l.falloff);
            }
          }
          printf("RAY: %d light points loaded from file.\n",
                 g_engine.num_lights);
          found = 1;
        }
      }
    }

    if (!found) {
      printf("RAY: No lights section found in file.\n");
      g_engine.num_lights = 0;
    }
  } else {
    g_engine.num_lights = 0;
  }

  return 1;
}

int ray_load_map(const char *filename) {
  if (!filename)
    return 0;

  // Auto-detect v8 (deprecated) vs v9
  FILE *file = fopen(filename, "rb");
  if (!file)
    return 0;

  RAY_MapHeader_v9 header;
  if (fread(&header, sizeof(RAY_MapHeader_v9), 1, file) != 1) {
    fclose(file);
    return 0;
  }

  printf("RAY: Detected map version: %u\n", header.version);

  if (header.version < 9) {
    fclose(file);
    return ray_load_map_v8(filename); // Will print error/deprecation
  }

  // Support for v27 (Fluid Speed)
  if (header.version > 9 && header.version < 22) {
    printf("RAY: Warning - Map version %u is old.\n", header.version);
  } else if (header.version > 27) {
    printf("RAY: Warning - Map version %u is newer than expected (max "
           "supported: 27). "
           "Attempts to load might fail.\n",
           header.version);
  }

  // Reset file pointer to start of file for ray_load_map_v9 to read properly?
  // No, ray_load_map_v9 takes 'header' as argument and assumes file is
  // positioned AFTER header? Let's check ray_load_map_v9 implementation. It
  // takes 'header' pointer. It does NOT read header again. It proceeds to read
  // Sectors. IF v23 header size == v9 header size, we are fine.

  int result = ray_load_map_v9(file, &header);
  fclose(file);
  return result;
}

/* ============================================================================
   AUTOMATIC HIERARCHY RECONSTRUCTION
   ============================================================================
 */

/* Helper to calculate polygon area for sorting parents */
static float ray_sector_area(RAY_Sector *s) {
  float area = 0;
  for (int i = 0; i < s->num_vertices; i++) {
    int next = (i + 1) % s->num_vertices;
    area += (s->vertices[i].x * s->vertices[next].y);
    area -= (s->vertices[i].y * s->vertices[next].x);
  }
  return fabsf(area) * 0.5f;
}

/* Helper to check point inside sector (Simple PIP) */
/* Re-implementing simplified version to avoid dependency issues if external
 * func not available */
static int ray_point_in_sector_local(RAY_Sector *sector, float x, float y) {
  int i, j, c = 0;
  for (i = 0, j = sector->num_vertices - 1; i < sector->num_vertices; j = i++) {
    if (((sector->vertices[i].y > y) != (sector->vertices[j].y > y)) &&
        (x < (sector->vertices[j].x - sector->vertices[i].x) *
                     (y - sector->vertices[i].y) /
                     (sector->vertices[j].y - sector->vertices[i].y) +
                 sector->vertices[i].x))
      c = !c;
  }
  return c;
}

/* ============================================================================
   AUTOMATIC PORTAL DETECTION FOR NESTED SECTORS
   ============================================================================
 */

/* Helper to add a wall to a sector (dynamic array) */
static int add_wall_to_sector(RAY_Sector *sector, RAY_Wall *wall) {
  if (sector->num_walls >= sector->walls_capacity) {
    fprintf(stderr, "RAY: Cannot add wall - sector at capacity\n");
    return -1;
  }

  sector->walls[sector->num_walls] = *wall;
  return sector->num_walls++;
}

/* Helper function to calculate overlap region between two walls on the same
 * line */
static int calculate_wall_overlap(RAY_Wall *w1, RAY_Wall *w2,
                                  float *overlap_start_x,
                                  float *overlap_start_y, float *overlap_end_x,
                                  float *overlap_end_y) {
  float epsilon = 2.0f;

  // Check if walls are on the same vertical line
  if (fabsf(w1->x1 - w1->x2) < epsilon && fabsf(w2->x1 - w2->x2) < epsilon) {
    if (fabsf(w1->x1 - w2->x1) < epsilon) {
      // Both vertical, same X - calculate Y overlap
      float w1_min_y = fminf(w1->y1, w1->y2);
      float w1_max_y = fmaxf(w1->y1, w1->y2);
      float w2_min_y = fminf(w2->y1, w2->y2);
      float w2_max_y = fmaxf(w2->y1, w2->y2);

      float overlap_min_y = fmaxf(w1_min_y, w2_min_y);
      float overlap_max_y = fminf(w1_max_y, w2_max_y);

      if (overlap_max_y >= overlap_min_y - epsilon) {
        *overlap_start_x = w1->x1;
        *overlap_start_y = overlap_min_y;
        *overlap_end_x = w1->x1;
        *overlap_end_y = overlap_max_y;
        return 1;
      }
    }
  }

  // Check if walls are on the same horizontal line
  if (fabsf(w1->y1 - w1->y2) < epsilon && fabsf(w2->y1 - w2->y2) < epsilon) {
    if (fabsf(w1->y1 - w2->y1) < epsilon) {
      // Both horizontal, same Y - calculate X overlap
      float w1_min_x = fminf(w1->x1, w1->x2);
      float w1_max_x = fmaxf(w1->x1, w1->x2);
      float w2_min_x = fminf(w2->x1, w2->x2);
      float w2_max_x = fmaxf(w2->x1, w2->x2);

      float overlap_min_x = fmaxf(w1_min_x, w2_min_x);
      float overlap_max_x = fminf(w1_max_x, w2_max_x);

      if (overlap_max_x >= overlap_min_x - epsilon) {
        *overlap_start_x = overlap_min_x;
        *overlap_start_y = w1->y1;
        *overlap_end_x = overlap_max_x;
        *overlap_end_y = w1->y1;
        return 1;
      }
    }
  }

  return 0;
}

/* Split a wall into segments based on overlap region */
static int split_wall_for_portal(RAY_Sector *sector, int wall_idx,
                                 float overlap_x1, float overlap_y1,
                                 float overlap_x2, float overlap_y2,
                                 int *portal_segment_idx) {
  RAY_Wall *original = &sector->walls[wall_idx];
  float epsilon = 2.0f;

  // Determine if wall is vertical or horizontal
  int is_vertical = fabsf(original->x1 - original->x2) < epsilon;

  if (is_vertical) {
    // Vertical wall - split by Y coordinates
    float wall_min_y = fminf(original->y1, original->y2);
    float wall_max_y = fmaxf(original->y1, original->y2);
    float overlap_min_y = fminf(overlap_y1, overlap_y2);
    float overlap_max_y = fmaxf(overlap_y1, overlap_y2);

    // Check if we need to split
    int need_before = fabsf(wall_min_y - overlap_min_y) > epsilon;
    int need_after = fabsf(wall_max_y - overlap_max_y) > epsilon;

    if (!need_before && !need_after) {
      // Entire wall is portal - no split needed
      *portal_segment_idx = wall_idx;
      return 1;
    }

    // We need to split - create new wall segments
    RAY_Wall temp_original = *original;
    int current_idx = wall_idx;

    // Segment 1: Before overlap (if needed)
    if (need_before) {
      original->x1 = temp_original.x1;
      original->y1 = wall_min_y;
      original->x2 = temp_original.x1;
      original->y2 = overlap_min_y;
      original->portal_id = -1;
      current_idx++;
    }

    // Segment 2: Overlap (portal)
    RAY_Wall portal_wall = temp_original;
    portal_wall.x1 = temp_original.x1;
    portal_wall.y1 = overlap_min_y;
    portal_wall.x2 = temp_original.x1;
    portal_wall.y2 = overlap_max_y;
    portal_wall.portal_id = -1;

    if (need_before) {
      *portal_segment_idx = add_wall_to_sector(sector, &portal_wall);
    } else {
      sector->walls[wall_idx] = portal_wall;
      *portal_segment_idx = wall_idx;
    }

    // Segment 3: After overlap (if needed)
    if (need_after) {
      RAY_Wall after_wall = temp_original;
      after_wall.x1 = temp_original.x1;
      after_wall.y1 = overlap_max_y;
      after_wall.x2 = temp_original.x1;
      after_wall.y2 = wall_max_y;
      after_wall.portal_id = -1;
      add_wall_to_sector(sector, &after_wall);
    }

    return 1;
  } else {
    // Horizontal wall - split by X coordinates
    float wall_min_x = fminf(original->x1, original->x2);
    float wall_max_x = fmaxf(original->x1, original->x2);
    float overlap_min_x = fminf(overlap_x1, overlap_x2);
    float overlap_max_x = fmaxf(overlap_x1, overlap_x2);

    int need_before = fabsf(wall_min_x - overlap_min_x) > epsilon;
    int need_after = fabsf(wall_max_x - overlap_max_x) > epsilon;

    if (!need_before && !need_after) {
      *portal_segment_idx = wall_idx;
      return 1;
    }

    RAY_Wall temp_original = *original;

    if (need_before) {
      original->x1 = wall_min_x;
      original->y1 = temp_original.y1;
      original->x2 = overlap_min_x;
      original->y2 = temp_original.y1;
      original->portal_id = -1;
    }

    RAY_Wall portal_wall = temp_original;
    portal_wall.x1 = overlap_min_x;
    portal_wall.y1 = temp_original.y1;
    portal_wall.x2 = overlap_max_x;
    portal_wall.y2 = temp_original.y1;
    portal_wall.portal_id = -1;

    if (need_before) {
      *portal_segment_idx = add_wall_to_sector(sector, &portal_wall);
    } else {
      sector->walls[wall_idx] = portal_wall;
      *portal_segment_idx = wall_idx;
    }

    if (need_after) {
      RAY_Wall after_wall = temp_original;
      after_wall.x1 = overlap_max_x;
      after_wall.y1 = temp_original.y1;
      after_wall.x2 = wall_max_x;
      after_wall.y2 = temp_original.y1;
      after_wall.portal_id = -1;
      add_wall_to_sector(sector, &after_wall);
    }

    return 1;
  }
}

/* Detect walls shared between ANY two sectors and create portals (Build Engine
 * style) */
static void ray_detect_all_shared_walls(void) {
  int portals_created = 0;

  printf("RAY: Detecting shared walls between all sectors (Build Engine "
         "style)...\n");

  /* Compare all sector pairs */
  for (int i = 0; i < g_engine.num_sectors; i++) {
    RAY_Sector *sector_a = &g_engine.sectors[i];

    printf("RAY: Sector %d has %d walls\n", i, sector_a->num_walls);
    for (int wa = 0; wa < sector_a->num_walls; wa++) {
      RAY_Wall *wall_a = &sector_a->walls[wa];
      printf("RAY:   Wall %d: (%.1f,%.1f) -> (%.1f,%.1f) portal_id=%d\n", wa,
             wall_a->x1, wall_a->y1, wall_a->x2, wall_a->y2, wall_a->portal_id);
    }

    for (int j = i + 1; j < g_engine.num_sectors; j++) {
      RAY_Sector *sector_b = &g_engine.sectors[j];

      /* Compare all walls of sector_a with all walls of sector_b */
      for (int wa = 0; wa < sector_a->num_walls; wa++) {
        RAY_Wall *wall_a = &sector_a->walls[wa];

        /* Skip if already a portal */
        if (wall_a->portal_id != -1) {
          continue;
        }

        for (int wb = 0; wb < sector_b->num_walls; wb++) {
          RAY_Wall *wall_b = &sector_b->walls[wb];

          /* Skip if already a portal */
          if (wall_b->portal_id != -1) {
            continue;
          }

          /* Calculate overlap region */
          float overlap_x1, overlap_y1, overlap_x2, overlap_y2;
          if (calculate_wall_overlap(wall_a, wall_b, &overlap_x1, &overlap_y1,
                                     &overlap_x2, &overlap_y2)) {
            /* Check portal capacity */
            if (g_engine.num_portals >= g_engine.portals_capacity) {
              printf("RAY: WARNING - Portal capacity reached\n");
              return;
            }

            /* Split walls if needed and get portal segment indices */
            int portal_wall_a_idx = wa;
            int portal_wall_b_idx = wb;

            split_wall_for_portal(sector_a, wa, overlap_x1, overlap_y1,
                                  overlap_x2, overlap_y2, &portal_wall_a_idx);
            split_wall_for_portal(sector_b, wb, overlap_x1, overlap_y1,
                                  overlap_x2, overlap_y2, &portal_wall_b_idx);

            /* Get the portal segment walls */
            RAY_Wall *portal_wall_a = &sector_a->walls[portal_wall_a_idx];
            RAY_Wall *portal_wall_b = &sector_b->walls[portal_wall_b_idx];

            /* Create bidirectional portal */
            RAY_Portal *new_portal = &g_engine.portals[g_engine.num_portals];
            memset(new_portal, 0, sizeof(RAY_Portal));

            new_portal->portal_id = g_engine.num_portals;
            new_portal->sector_a = sector_a->sector_id;
            new_portal->sector_b = sector_b->sector_id;
            new_portal->wall_id_a = portal_wall_a_idx;
            new_portal->wall_id_b = portal_wall_b_idx;
            new_portal->x1 = overlap_x1;
            new_portal->y1 = overlap_y1;
            new_portal->x2 = overlap_x2;
            new_portal->y2 = overlap_y2;

            /* Assign portal to both wall segments */
            portal_wall_a->portal_id = new_portal->portal_id;
            portal_wall_b->portal_id = new_portal->portal_id;

            /* Auto-assign step textures from main wall texture (Build Engine
             * style) */
            if (portal_wall_a->texture_id_upper == 0) {
              portal_wall_a->texture_id_upper =
                  portal_wall_a->texture_id_middle;
            }
            if (portal_wall_a->texture_id_lower == 0) {
              portal_wall_a->texture_id_lower =
                  portal_wall_a->texture_id_middle;
            }
            if (portal_wall_b->texture_id_upper == 0) {
              portal_wall_b->texture_id_upper =
                  portal_wall_b->texture_id_middle;
            }
            if (portal_wall_b->texture_id_lower == 0) {
              portal_wall_b->texture_id_lower =
                  portal_wall_b->texture_id_middle;
            }

            /* Add to both sectors portal lists */
            if (sector_a->num_portals < sector_a->portals_capacity) {
              sector_a->portal_ids[sector_a->num_portals++] =
                  new_portal->portal_id;
            }
            if (sector_b->num_portals < sector_b->portals_capacity) {
              sector_b->portal_ids[sector_b->num_portals++] =
                  new_portal->portal_id;
            }

            g_engine.num_portals++;
            portals_created++;

            printf("RAY: Created portal %d: Sector %d (wall %d) <-> Sector %d "
                   "(wall %d)\n",
                   new_portal->portal_id, sector_a->sector_id, wa,
                   sector_b->sector_id, wb);

            break; /* Found match for this wall_a */
          }
        }
      }
    }
  }

  printf("RAY: Created %d automatic portals\n", portals_created);
}

/* Detect nested sectors (sectors completely inside other sectors) and rebuild
 * parent-child links */
static void ray_reconstruct_hierarchy(void) {
  printf("RAY: Reconstructing sector hierarchy...\n");

  // 1. Reset all hierarchy links
  for (int i = 0; i < g_engine.num_sectors; i++) {
    RAY_Sector *s = &g_engine.sectors[i];
    s->parent_sector_id = -1;

    if (s->child_sector_ids) {
      free(s->child_sector_ids);
      s->child_sector_ids = NULL;
    }
    s->num_children = 0;
    s->children_capacity = 0;
  }

  // 2. For each sector, find the smallest sector that completely contains it
  for (int i = 0; i < g_engine.num_sectors; i++) {
    RAY_Sector *child = &g_engine.sectors[i];
    int best_parent = -1;
    float min_area = FLT_MAX;

    for (int j = 0; j < g_engine.num_sectors; j++) {
      if (i == j)
        continue;

      RAY_Sector *parent = &g_engine.sectors[j];

      // Quick AABB check
      if (child->min_x < parent->min_x || child->max_x > parent->max_x ||
          child->min_y < parent->min_y || child->max_y > parent->max_y) {
        continue;
      }

      // Check if all vertices of child are inside parent
      int all_inside = 1;
      for (int v = 0; v < child->num_vertices; v++) {
        if (!ray_point_in_sector_local(parent, child->vertices[v].x,
                                       child->vertices[v].y)) {
          all_inside = 0;
          break;
        }
      }

      if (all_inside) {
        float area = ray_sector_area(parent);
        if (area < min_area) {
          min_area = area;
          best_parent = j;
        }
      }
    }

    // 3. Set parent-child relationship
    if (best_parent != -1) {
      child->parent_sector_id = best_parent;
      RAY_Sector *p = &g_engine.sectors[best_parent];

      if (p->num_children >= p->children_capacity) {
        p->children_capacity = (p->num_children == 0) ? 4 : p->num_children * 2;
        p->child_sector_ids = (int *)realloc(
            p->child_sector_ids, p->children_capacity * sizeof(int));
      }
      p->child_sector_ids[p->num_children++] = i;
    }
  }

  // Debug summary
  int nested_count = 0;
  for (int i = 0; i < g_engine.num_sectors; i++) {
    if (g_engine.sectors[i].parent_sector_id != -1)
      nested_count++;
  }
  printf("RAY: Hierarchy reconstruction complete. %d nested sectors found.\n",
         nested_count);
}
/* Detect walls shared between parent and child sectors and create portals */

/* ============================================================================
   MAP SAVING
   ============================================================================
 */

/* ============================================================================
   V8 COMPATIBILITY STUBS (DEPRECATED)
   ============================================================================
 */

int ray_load_map_v8(const char *filename) {
  fprintf(stderr,
          "RAY: Map format v8 is deprecated. Please re-export to v9.\n");
  return -1;
}

int ray_save_map_v8(const char *filename) {
  fprintf(stderr, "RAY: Map format v8 is deprecated. Please use v9.\n");
  return -1;
}
