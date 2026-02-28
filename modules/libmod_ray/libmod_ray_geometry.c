/*
 * libmod_ray_geometry.c - Geometric Functions for Sector-Based Engine
 * Complete rewrite - no tile system
 */

#include "libmod_ray.h"
#include "libmod_ray_compat.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
   POLYGON GEOMETRY
   ============================================================================
 */

/* Point in polygon test using ray casting algorithm */
int ray_point_in_polygon(float px, float py, const RAY_Point *vertices,
                         int num_vertices) {
  if (!vertices || num_vertices < 3)
    return 0;

  int inside = 0;
  float x1, y1, x2, y2;

  x1 = vertices[num_vertices - 1].x;
  y1 = vertices[num_vertices - 1].y;

  for (int i = 0; i < num_vertices; i++) {
    x2 = vertices[i].x;
    y2 = vertices[i].y;

    if (((y2 > py) != (y1 > py)) &&
        (px < (x1 - x2) * (py - y2) / (y1 - y2) + x2)) {
      inside = !inside;
    }

    x1 = x2;
    y1 = y2;
  }

  return inside;
}

/* Check if a polygon is convex */
int ray_polygon_is_convex(const RAY_Point *vertices, int num_vertices) {
  if (!vertices || num_vertices < 3)
    return 0;

  int sign = 0;

  for (int i = 0; i < num_vertices; i++) {
    float dx1 = vertices[(i + 1) % num_vertices].x - vertices[i].x;
    float dy1 = vertices[(i + 1) % num_vertices].y - vertices[i].y;
    float dx2 =
        vertices[(i + 2) % num_vertices].x - vertices[(i + 1) % num_vertices].x;
    float dy2 =
        vertices[(i + 2) % num_vertices].y - vertices[(i + 1) % num_vertices].y;

    float cross = dx1 * dy2 - dy1 * dx2;

    if (cross != 0) {
      if (sign == 0) {
        sign = (cross > 0) ? 1 : -1;
      } else if (((cross > 0) ? 1 : -1) != sign) {
        return 0; /* Not convex */
      }
    }
  }

  return 1; /* Convex */
}

/* Line segment intersection */
int ray_line_segment_intersect(float x1, float y1, float x2, float y2, float x3,
                               float y3, float x4, float y4, float *ix,
                               float *iy) {
  float denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);

  if (fabsf(denom) < RAY_EPSILON) {
    return 0; /* Parallel or coincident */
  }

  float t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / denom;
  float u = -((x1 - x2) * (y1 - y3) - (y1 - y2) * (x1 - x3)) / denom;

  if (t >= 0 && t <= 1 && u >= 0 && u <= 1) {
    if (ix)
      *ix = x1 + t * (x2 - x1);
    if (iy)
      *iy = y1 + t * (y2 - y1);
    return 1;
  }

  return 0;
}

/* ============================================================================
   SECTOR MANAGEMENT
   ============================================================================
 */

RAY_Sector *ray_sector_create(int sector_id, float floor_z, float ceiling_z,
                              int floor_tex, int ceiling_tex) {
  RAY_Sector *sector = (RAY_Sector *)malloc(sizeof(RAY_Sector));
  if (!sector)
    return NULL;

  memset(sector, 0, sizeof(RAY_Sector));

  sector->sector_id = sector_id;
  sector->floor_z = floor_z;
  sector->ceiling_z = ceiling_z;
  sector->floor_texture_id = floor_tex;
  sector->ceiling_texture_id = ceiling_tex;
  sector->light_level = 255;

  /* Allocate arrays */
  sector->vertices_capacity = RAY_MAX_VERTICES_PER_SECTOR;
  sector->vertices =
      (RAY_Point *)malloc(sector->vertices_capacity * sizeof(RAY_Point));

  sector->walls_capacity = RAY_MAX_WALLS_PER_SECTOR;
  sector->walls = (RAY_Wall *)malloc(sector->walls_capacity * sizeof(RAY_Wall));

  sector->portals_capacity = RAY_MAX_WALLS_PER_SECTOR;
  sector->portal_ids = (int *)malloc(sector->portals_capacity * sizeof(int));

  /* Hierarchy fields */
  sector->parent_sector_id = -1;
  sector->children_capacity = RAY_MAX_WALLS_PER_SECTOR;
  sector->child_sector_ids =
      (int *)malloc(sector->children_capacity * sizeof(int));
  sector->num_children = 0;
  sector->flags = 0;
  sector->liquid_intensity = 0.0f;
  sector->liquid_speed = 1.0f;

  return sector;
}

void ray_sector_free(RAY_Sector *sector) {
  if (!sector)
    return;

  if (sector->vertices)
    free(sector->vertices);
  if (sector->walls)
    free(sector->walls);
  if (sector->portal_ids)
    free(sector->portal_ids);
  // BUILD_ENGINE: No hierarchy fields to free
  // if (sector->child_sector_ids) free(sector->child_sector_ids);

  free(sector);
}

void ray_sector_add_vertex(RAY_Sector *sector, float x, float y) {
  if (!sector || sector->num_vertices >= RAY_MAX_VERTICES_PER_SECTOR)
    return;

  sector->vertices[sector->num_vertices].x = x;
  sector->vertices[sector->num_vertices].y = y;
  sector->num_vertices++;
}

void ray_sector_add_wall(RAY_Sector *sector, const RAY_Wall *wall) {
  if (!sector || !wall || sector->num_walls >= RAY_MAX_WALLS_PER_SECTOR)
    return;

  memcpy(&sector->walls[sector->num_walls], wall, sizeof(RAY_Wall));
  sector->num_walls++;
}

void ray_sector_add_portal(RAY_Sector *sector, int portal_id) {
  if (!sector || sector->num_portals >= sector->portals_capacity)
    return;

  sector->portal_ids[sector->num_portals] = portal_id;
  sector->num_portals++;
}

/* Find which sector contains a point - hierarchical version for nested sectors
 */
RAY_Sector *ray_find_sector_at_point(RAY_Engine *engine, float x, float y) {
  if (!engine)
    return NULL;

  /* Find all sectors that contain this point */
  int candidate_count = 0;
  int candidates[RAY_MAX_SECTORS];

  for (int i = 0; i < engine->num_sectors; i++) {
    RAY_Sector *sector = &engine->sectors[i];
    if (ray_point_in_polygon(x, y, sector->vertices, sector->num_vertices)) {
      if (candidate_count < RAY_MAX_SECTORS) {
        candidates[candidate_count++] = i;
      }
    }
  }

  /* No sectors found */
  if (candidate_count == 0) {
    return NULL;
  }

  /* Only one sector - return it */
  if (candidate_count == 1) {
    return &engine->sectors[candidates[0]];
  }

  /* Prioritize the LAST candidate (usually the innermost/newest sector)
   * This ensures that nested sectors (islands) are returned instead of the
   * parent
   */
  return &engine->sectors[candidates[candidate_count - 1]];
}

/* Find sector at (x,y,z) - handles solid nested sectors correctly */
RAY_Sector *ray_find_sector_at_position(RAY_Engine *engine, float x, float y,
                                        float z) {
  if (!engine)
    return NULL;

  /* Find all sectors that contain this point (x,y) */
  int candidate_count = 0;
  int candidates[RAY_MAX_SECTORS];

  for (int i = 0; i < engine->num_sectors; i++) {
    RAY_Sector *sector = &engine->sectors[i];
    if (ray_point_in_polygon(x, y, sector->vertices, sector->num_vertices)) {
      if (candidate_count < RAY_MAX_SECTORS) {
        candidates[candidate_count++] = i;
      }
    }
  }

  /* No sectors found */
  if (candidate_count == 0) {
    return NULL;
  }

  /* Only one sector - return it */
  if (candidate_count == 1) {
    return &engine->sectors[candidates[0]];
  }

  /* Multiple sectors - filter by Z for Solid Sectors */
  /* Sort/Selection logic:
     We want the deepest nested sector that actually contains the point Z.
     If a nested sector is SOLID, we must be inside its Z range [floor, ceil].
     If we are outside its Z range, we ignore it (unless it's the only option?).
  */

  RAY_Sector *best_match = NULL;

  /* Search from most nested (last) to least nested (first)
     This ensures we pick the 'Platform' instead of the 'Room' if we are inside
     it.
  */
  for (int i = candidate_count - 1; i >= 0; i--) {
    int idx = candidates[i];
    RAY_Sector *s = &engine->sectors[idx];

    /* A sector is a valid container if Z is within its bounds.
       For solid sectors (islands/platforms), we allow a small margin above
       the ceiling to keep the player "on top" of the platform.
    */
    float tolerance = ray_sector_is_solid(s) ? 2.0f : 0.0f;
    if (z >= s->floor_z && z < s->ceiling_z + tolerance) {
      return s;
    }

    /* Fallback: if we are exactly ON the floor/ceiling (floating point
       precision), accept it if it's the most nested candidate so far.
    */
    if (fabsf(z - s->floor_z) < 0.1f || fabsf(z - s->ceiling_z) < 0.1f) {
      if (!best_match)
        best_match = s;
    }
  }

  /* Fallback 2: If we are not strictly inside ANY Z-range (e.g. we are 'Void'
     or on top), default to the innermost sector at this point (2D search
     result).
  */
  if (best_match)
    return best_match;

  return ray_find_sector_at_point(engine, x, y);
}

/* ============================================================================
   WALL MANAGEMENT
   ============================================================================
 */

RAY_Wall *ray_wall_create(int wall_id, float x1, float y1, float x2, float y2) {
  RAY_Wall *wall = (RAY_Wall *)malloc(sizeof(RAY_Wall));
  if (!wall)
    return NULL;

  memset(wall, 0, sizeof(RAY_Wall));

  wall->wall_id = wall_id;
  wall->x1 = x1;
  wall->y1 = y1;
  wall->x2 = x2;
  wall->y2 = y2;
  wall->portal_id = -1; /* No portal by default */

  /* Default texture splits (divide wall into 3 equal parts) */
  wall->texture_split_z_lower = 64.0f;
  wall->texture_split_z_upper = 192.0f;

  return wall;
}

void ray_wall_set_textures(RAY_Wall *wall, int lower, int middle, int upper,
                           float split_lower, float split_upper) {
  if (!wall)
    return;

  wall->texture_id_lower = lower;
  wall->texture_id_middle = middle;
  wall->texture_id_upper = upper;
  wall->texture_split_z_lower = split_lower;
  wall->texture_split_z_upper = split_upper;
}

/* ============================================================================
   PORTAL MANAGEMENT
   ============================================================================
 */

RAY_Portal *ray_portal_create(int portal_id, int sector_a, int sector_b,
                              int wall_id_a, int wall_id_b, float x1, float y1,
                              float x2, float y2) {
  RAY_Portal *portal = (RAY_Portal *)malloc(sizeof(RAY_Portal));
  if (!portal)
    return NULL;

  memset(portal, 0, sizeof(RAY_Portal));

  portal->portal_id = portal_id;
  portal->sector_a = sector_a;
  portal->sector_b = sector_b;
  portal->wall_id_a = wall_id_a;
  portal->wall_id_b = wall_id_b;
  portal->x1 = x1;
  portal->y1 = y1;
  portal->x2 = x2;
  portal->y2 = y2;

  return portal;
}

void ray_portal_free(RAY_Portal *portal) {
  if (portal)
    free(portal);
}

/* Automatic portal detection - find walls that share coordinates */
void ray_detect_portals(RAY_Engine *engine) {
  if (!engine)
    return;

  /* Clear existing portals */
  engine->num_portals = 0;

  int next_portal_id = 0;

  /* Compare all walls of all sectors */
  for (int i = 0; i < engine->num_sectors; i++) {
    RAY_Sector *sector_a = &engine->sectors[i];

    for (int w_a = 0; w_a < sector_a->num_walls; w_a++) {
      RAY_Wall *wall_a = &sector_a->walls[w_a];

      /* Skip if this wall already has a portal */
      if (wall_a->portal_id >= 0)
        continue;

      /* Compare with walls of other sectors */
      for (int j = i + 1; j < engine->num_sectors; j++) {
        RAY_Sector *sector_b = &engine->sectors[j];

        for (int w_b = 0; w_b < sector_b->num_walls; w_b++) {
          RAY_Wall *wall_b = &sector_b->walls[w_b];

          /* Skip if this wall already has a portal */
          if (wall_b->portal_id >= 0)
            continue;

          /* Check if walls share coordinates (with epsilon tolerance) */
          int same_coords = (fabsf(wall_a->x1 - wall_b->x1) < RAY_EPSILON &&
                             fabsf(wall_a->y1 - wall_b->y1) < RAY_EPSILON &&
                             fabsf(wall_a->x2 - wall_b->x2) < RAY_EPSILON &&
                             fabsf(wall_a->y2 - wall_b->y2) < RAY_EPSILON) ||
                            (fabsf(wall_a->x1 - wall_b->x2) < RAY_EPSILON &&
                             fabsf(wall_a->y1 - wall_b->y2) < RAY_EPSILON &&
                             fabsf(wall_a->x2 - wall_b->x1) < RAY_EPSILON &&
                             fabsf(wall_a->y2 - wall_b->y1) < RAY_EPSILON);

          if (same_coords) {
            /* Create portal */
            if (engine->num_portals >= engine->portals_capacity) {
              /* Reallocate if needed */
              engine->portals_capacity *= 2;
              engine->portals = (RAY_Portal *)realloc(engine->portals,
                                                      engine->portals_capacity *
                                                          sizeof(RAY_Portal));
            }

            RAY_Portal *portal = &engine->portals[engine->num_portals];
            portal->portal_id = next_portal_id;
            portal->sector_a = sector_a->sector_id;
            portal->sector_b = sector_b->sector_id;
            portal->wall_id_a = wall_a->wall_id;
            portal->wall_id_b = wall_b->wall_id;
            portal->x1 = wall_a->x1;
            portal->y1 = wall_a->y1;
            portal->x2 = wall_a->x2;
            portal->y2 = wall_a->y2;

            /* Assign portal ID to both walls */
            wall_a->portal_id = next_portal_id;
            wall_b->portal_id = next_portal_id;

            /* Add portal to both sectors */
            ray_sector_add_portal(sector_a, next_portal_id);
            ray_sector_add_portal(sector_b, next_portal_id);

            engine->num_portals++;
            next_portal_id++;

            break; /* Found portal for wall_a, move to next wall */
          }
        }
      }
    }
  }
}

/* Check if portal is visible from camera */
int ray_portal_is_visible(RAY_Portal *portal, RAY_Camera *camera) {
  if (!portal || !camera)
    return 0;

  /* Calculate vector from camera to portal midpoint */
  float mid_x = (portal->x1 + portal->x2) / 2.0f;
  float mid_y = (portal->y1 + portal->y2) / 2.0f;

  float dx = mid_x - camera->x;
  float dy = mid_y - camera->y;

  /* Calculate angle to portal */
  float angle_to_portal = atan2f(-dy, dx);

  /* Normalize angle difference */
  float angle_diff = angle_to_portal - camera->rot;
  while (angle_diff > M_PI)
    angle_diff -= RAY_TWO_PI;
  while (angle_diff < -M_PI)
    angle_diff += RAY_TWO_PI;

  /* Check if within FOV (assuming 90 degree FOV) */
  return fabsf(angle_diff) < M_PI / 2.0f;
}
