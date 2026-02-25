/* ============================================================================
   RECURSIVE SECTOR RENDERING FOR NESTED SECTORS
   ============================================================================

   This file implements Doom/Build-style recursive sector rendering.
   Each sector is rendered with clip bounds inherited from its parent,
   ensuring proper occlusion of parent geometry by child sectors./*
 * libmod_ray_render_recursive.c - Recursive Portal Rendering (Experimental)
 */

#include "libmod_ray.h"
#include "libmod_ray_compat.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* External engine instance */
extern RAY_Engine g_engine;

/* Check visibility function */
extern void ray_cast_ray(RAY_Engine *engine, int sector_id, float x, float y,
                         float ray_angle, int strip_idx, RAY_RayHit *hits,
                         int *num_hits);

#define MAX_RECURSION_DEPTH 16

/* Helper: Project a world Z coordinate to screen Y coordinate */
static int project_z_to_screen_y(float world_z, float distance,
                                 float camera_z) {
  if (distance < 0.1f)
    return g_engine.displayHeight / 2;

  float dz = world_z - camera_z;
  float screen_y =
      (g_engine.displayHeight / 2.0f) - (dz * g_engine.viewDist / distance);

  return (int)screen_y;
}

/* Recursive sector rendering for a single column */
void render_sector_column_recursive(GRAPH *dest, int screen_x, float ray_angle,
                                    int sector_id,
                                    int clip_top,    /* Inherited from parent */
                                    int clip_bottom, /* Inherited from parent */
                                    float *z_buffer, int depth) {
  /* Prevent infinite recursion */
  if (depth >= MAX_RECURSION_DEPTH)
    return;
  if (clip_top >= clip_bottom)
    return; /* No visible space */

  /* Get sector */
  if (sector_id < 0 || sector_id >= g_engine.num_sectors)
    return;
  RAY_Sector *sector = &g_engine.sectors[sector_id];

  /* Cast ray in this sector */
  RAY_RayHit hits[RAY_MAX_RAYHITS];
  int num_hits = 0;

  ray_cast_ray(&g_engine, sector_id, g_engine.camera.x, g_engine.camera.y,
               ray_angle, -1, hits, &num_hits);

  if (num_hits == 0) {
    /* No walls hit - render floor/ceiling to horizon */
    ray_draw_floor_ceiling(dest, screen_x, ray_angle, sector_id, 0.0f,
                           g_engine.viewDist * 4.0f, z_buffer, &clip_top,
                           &clip_bottom);
    return;
  }

  /* Sort hits by distance (near to far) */
  for (int i = 0; i < num_hits - 1; i++) {
    for (int j = i + 1; j < num_hits; j++) {
      if (hits[j].distance < hits[i].distance) {
        RAY_RayHit temp = hits[i];
        hits[i] = hits[j];
        hits[j] = temp;
      }
    }
  }

  /* Render walls and recurse into child sectors */
  float current_dist = 0.0f;
  int local_clip_top = clip_top;
  int local_clip_bottom = clip_bottom;

  for (int h = 0; h < num_hits; h++) {
    RAY_RayHit *hit = &hits[h];
    if (!hit->wall)
      continue;

    /* Render floor/ceiling segment before this wall */
    if (hit->distance > current_dist + 0.1f) {
      ray_draw_floor_ceiling(dest, screen_x, ray_angle, sector_id, current_dist,
                             hit->distance, z_buffer, &local_clip_top,
                             &local_clip_bottom);
    }

    /* Render the wall */
    ray_draw_wall_strip(dest, hit, screen_x, &local_clip_top,
                        &local_clip_bottom);

    /* Update local clip bounds based on wall */
    int wall_screen_height =
        (int)ray_strip_screen_height(g_engine.viewDist, hit->correctDistance,
                                     sector->ceiling_z - sector->floor_z);
    int wall_top = (g_engine.displayHeight - wall_screen_height) / 2;
    int wall_bottom = wall_top + wall_screen_height;

    /* Intersect clip bounds */
    if (wall_bottom < local_clip_top)
      local_clip_top = wall_bottom;
    if (wall_top > local_clip_bottom)
      local_clip_bottom = wall_top;

    /* Check if this wall leads to a child sector */
    /* For nested sectors (not portals), check if hit point is inside a child */
    float hit_x = g_engine.camera.x + hit->distance * cosf(ray_angle);
    float hit_y = g_engine.camera.y + hit->distance * -sinf(ray_angle);

    /* Check all child sectors */
    for (int c = 0; c < ray_sector_get_num_children(sector); c++) {
      int child_id = ray_sector_get_child(sector, c);
      if (child_id < 0 || child_id >= g_engine.num_sectors)
        continue;

      RAY_Sector *child = &g_engine.sectors[child_id];

      /* Check if ray passes through child sector */
      if (ray_point_in_polygon(hit_x, hit_y, child->vertices,
                               child->num_vertices)) {
        /* Calculate child sector's ceiling/floor screen positions */
        int child_ceiling_y = project_z_to_screen_y(
            child->ceiling_z, hit->distance, g_engine.camera.z);
        int child_floor_y = project_z_to_screen_y(child->floor_z, hit->distance,
                                                  g_engine.camera.z);

        /* Intersect with current clip bounds */
        int new_clip_top = (child_ceiling_y > local_clip_top) ? child_ceiling_y
                                                              : local_clip_top;
        int new_clip_bottom = (child_floor_y < local_clip_bottom)
                                  ? child_floor_y
                                  : local_clip_bottom;

        /* Recurse into child sector */
        if (new_clip_top < new_clip_bottom) {
          render_sector_column_recursive(dest, screen_x, ray_angle, child_id,
                                         new_clip_top, new_clip_bottom,
                                         z_buffer, depth + 1);
        }
      }
    }

    current_dist = hit->distance;

    /* If solid wall, stop */
    if (hit->wall->portal_id < 0)
      break;
  }

  /* Render remaining floor/ceiling to horizon */
  if (current_dist < g_engine.viewDist * 4.0f) {
    ray_draw_floor_ceiling(dest, screen_x, ray_angle, sector_id, current_dist,
                           g_engine.viewDist * 4.0f, z_buffer, &local_clip_top,
                           &local_clip_bottom);
  }
}
