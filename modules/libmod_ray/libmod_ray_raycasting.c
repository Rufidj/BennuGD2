/*
 * libmod_ray_raycasting.c - Raycasting for Geometric Sectors
 * Complete rewrite - no tile system
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

/* ============================================================================
   RAY-WALL INTERSECTION
   ============================================================================
 */

/* Find intersection between a ray and a wall segment */
void ray_find_wall_intersection(float ray_x, float ray_y, float ray_angle,
                                const RAY_Wall *wall, float *distance,
                                float *hit_x, float *hit_y) {
  if (!wall || !distance)
    return;

  *distance = FLT_MAX;

  /* Ray direction */
  float ray_dx = cosf(ray_angle);
  float ray_dy = -sinf(ray_angle);

  /* Wall vector */
  float wall_dx = wall->x2 - wall->x1;
  float wall_dy = wall->y2 - wall->y1;

  /* Calculate intersection using parametric line equations */
  float denom = ray_dx * wall_dy - ray_dy * wall_dx;

  if (fabsf(denom) < RAY_EPSILON) {
    return; /* Ray parallel to wall */
  }

  float t =
      ((wall->x1 - ray_x) * wall_dy - (wall->y1 - ray_y) * wall_dx) / denom;
  float u = ((wall->x1 - ray_x) * ray_dy - (wall->y1 - ray_y) * ray_dx) / denom;

  /* Check if intersection is valid with slight tolerance for vertices */
  float tolerance = 0.001f;
  if (t > 0 && u >= -tolerance && u <= 1.0f + tolerance) {
    /* Calculate intersection point */
    float ix = ray_x + t * ray_dx;
    float iy = ray_y + t * ray_dy;

    /* Calculate distance */
    float dx = ix - ray_x;
    float dy = iy - ray_y;
    *distance = sqrtf(dx * dx + dy * dy);

    if (hit_x)
      *hit_x = ix;
    if (hit_y)
      *hit_y = iy;
  }
}

/* ============================================================================
   SECTOR RAYCASTING
   ============================================================================
 */

/* Cast a ray against all walls in a sector */
void ray_cast_against_sector(RAY_Engine *engine, RAY_Sector *sector,
                             float ray_x, float ray_y, float ray_angle,
                             RAY_RayHit *hits, int *num_hits,
                             float accumulated_distance) {
  if (!engine || !sector || !hits || !num_hits)
    return;

  /* Cast ray against each wall in the sector */
  for (int i = 0; i < sector->num_walls; i++) {
    RAY_Wall *wall = &sector->walls[i];

    float distance, hit_x, hit_y;
    ray_find_wall_intersection(ray_x, ray_y, ray_angle, wall, &distance, &hit_x,
                               &hit_y);

    if (distance < FLT_MAX && *num_hits < RAY_MAX_RAYHITS) {
      /* Create hit record */
      RAY_RayHit *hit = &hits[*num_hits];
      memset(hit, 0, sizeof(RAY_RayHit));

      hit->x = hit_x;
      hit->y = hit_y;
      hit->sector_id = sector->sector_id;
      hit->wall_id = wall->wall_id;
      hit->wall = wall;
      hit->distance =
          distance + accumulated_distance; /* Add accumulated distance */
      hit->rayAngle = ray_angle;

      /* Calculate texture coordinate - use actual distance for tiling */
      float wall_length = sqrtf((wall->x2 - wall->x1) * (wall->x2 - wall->x1) +
                                (wall->y2 - wall->y1) * (wall->y2 - wall->y1));
      float hit_dist = sqrtf((hit_x - wall->x1) * (hit_x - wall->x1) +
                             (hit_y - wall->y1) * (hit_y - wall->y1));

      /* Use actual distance for tiling - texture repeats every RAY_TEXTURE_SIZE
       * units */
      hit->tileX = hit_dist;

      /* Fisheye correction based on TOTAL distance */
      float total_distance = distance + accumulated_distance;
      float angle_diff = ray_angle - engine->camera.rot;
      hit->correctDistance = total_distance * cosf(angle_diff);

      /* Wall height (from sector floor to ceiling) */
      hit->wallHeight = sector->ceiling_z - sector->floor_z;
      hit->wallZOffset = sector->floor_z;

      (*num_hits)++;
    }
  }
}

/* ============================================================================
   RECURSIVE PORTAL RAYCASTING
   ============================================================================
 */

/* Cast ray through portals recursively */
void ray_cast_through_portals(RAY_Engine *engine, int current_sector_id,
                              float ray_x, float ray_y, float ray_angle,
                              RAY_RayHit *hits, int *num_hits, int depth,
                              float accumulated_distance) {
  if (!engine || depth > engine->max_portal_depth)
    return;

  /* Safety: prevent hitting max hits limit */
  if (*num_hits >= RAY_MAX_RAYHITS - 10)
    return;

  /* Find current sector */
  RAY_Sector *sector = NULL;
  for (int i = 0; i < engine->num_sectors; i++) {
    if (engine->sectors[i].sector_id == current_sector_id) {
      sector = &engine->sectors[i];
      break;
    }
  }

  if (!sector)
    return;

  /* Cast ray against all walls in current sector */
  int initial_hits = *num_hits;
  ray_cast_against_sector(engine, sector, ray_x, ray_y, ray_angle, hits,
                          num_hits, accumulated_distance);

  /* Check if we hit any portals */
  int sector_hits_end = *num_hits; /* Store count before processing to avoid
                                      iterating over new recursive hits */

  for (int i = initial_hits; i < sector_hits_end; i++) {
    RAY_RayHit *hit = &hits[i];

    if (hit->wall && hit->wall->portal_id >= 0) {
      /* This is a portal - continue ray into adjacent sector */
      RAY_Portal *portal = NULL;
      for (int p = 0; p < engine->num_portals; p++) {
        if (engine->portals[p].portal_id == hit->wall->portal_id) {
          portal = &engine->portals[p];
          break;
        }
      }

      if (portal) {
        /* Determine which sector to enter */
        int next_sector_id = (portal->sector_a == current_sector_id)
                                 ? portal->sector_b
                                 : portal->sector_a;

        /* Nudge ray slightly forward to enter the next sector avoiding
         * precision issues */
        float nudge = 0.1f;
        float start_x = hit->x + cosf(ray_angle) * nudge;
        float start_y = hit->y - sinf(ray_angle) * nudge;

        /* Continue ray from nudged point with accumulated distance */
        /* Note: Add nudge to accumulated distance too? Usually negligible but
         * technically yes. */
        ray_cast_through_portals(engine, next_sector_id, start_x, start_y,
                                 ray_angle, hits, num_hits, depth + 1,
                                 hit->distance + nudge);
      }
    }
  }
}

/* ============================================================================
   MAIN RAYCASTING FUNCTION
   ============================================================================
 */

/* ============================================================================
   MAIN RAYCASTING FUNCTION (OPTIMIZED SECTOR TRAVERSAL)
   ============================================================================
 */

/* Helper for sorting local hits */
static int ray_local_hit_sorter(const void *a, const void *b) {
  const RAY_RayHit *ha = (const RAY_RayHit *)a;
  const RAY_RayHit *hb = (const RAY_RayHit *)b;
  if (ha->distance < hb->distance)
    return -1;
  if (ha->distance > hb->distance)
    return 1;
  return 0;
}

/* Cast a single ray from specified position using efficient Portal Traversal
 * with Transparency Support */
void ray_cast_ray(RAY_Engine *engine, int sector_id, float x, float y,
                  float ray_angle, int strip_idx, RAY_RayHit *hits,
                  int *num_hits) {
  if (!engine || !hits || !num_hits)
    return;

  *num_hits = 0;
  int max_depth = 32; // Limit recursion/iteration

  // Use specified origin
  float cur_x = x;
  float cur_y = y;
  int cur_sector_id = sector_id;
  float accum_dist = 0.0f;

  // If sector_id not provided (-1), find it
  if (cur_sector_id < 0) {
    RAY_Sector *s = ray_find_sector_at_point(engine, cur_x, cur_y);
    if (s)
      cur_sector_id = s->sector_id;
    else
      return; // Outside map
  }

  // Traversal Loop
  for (int depth = 0; depth < max_depth; depth++) {
    if (*num_hits >= RAY_MAX_RAYHITS)
      break;

    RAY_Sector *sector = &engine->sectors[cur_sector_id];

    // Capture ALL hits in this sector to support transparency (fences, etc)
    // Use a local buffer to process this sector's walls
    RAY_RayHit local_hits[RAY_MAX_WALLS_PER_SECTOR]; // Stack allocation safe?
                                                     // 16 walls usually.
    int num_local_hits = 0;

    /* --------------------------------------------------------------------
       CHECK SOLID CHILD SECTORS (ISLANDS)
       Allows rendering of floating boxes/columns without portals
       -------------------------------------------------------------------- */
    if (ray_sector_has_children(sector)) {
      int num_children = ray_sector_get_num_children(sector);

      for (int c = 0; c < num_children; c++) {
        int child_id = ray_sector_get_child(sector, c);

        /* Find child sector pointer */
        RAY_Sector *child = NULL;
        if (child_id >= 0 && child_id < engine->num_sectors &&
            engine->sectors[child_id].sector_id == child_id) {
          child = &engine->sectors[child_id];
        } else {
          for (int s = 0; s < engine->num_sectors; s++) {
            if (engine->sectors[s].sector_id == child_id) {
              child = &engine->sectors[s];
              break;
            }
          }
        }

        if (child) {
          if (strip_idx == 320 && cur_sector_id == 0) {
            printf("  Child %d (ID %d): Solid=%d, Walls=%d\n", c, child_id,
                   ray_sector_is_solid(child), child->num_walls);
          }

          if (ray_sector_is_solid(child)) {
            /* Raycast against island walls */
            for (int w = 0; w < child->num_walls &&
                            num_local_hits < RAY_MAX_WALLS_PER_SECTOR;
                 w++) {
              RAY_Wall *wall = &child->walls[w];
              float distance, hit_x, hit_y;
              ray_find_wall_intersection(cur_x, cur_y, ray_angle, wall,
                                         &distance, &hit_x, &hit_y);

              if (distance < FLT_MAX) {
                if (strip_idx == 320 && cur_sector_id == 0) {
                  printf("    HIT Wall %d! Dist=%.2f\n", wall->wall_id,
                         distance);
                }

                RAY_RayHit *hit = &local_hits[num_local_hits];
                memset(hit, 0, sizeof(RAY_RayHit));

                hit->x = hit_x;
                hit->y = hit_y;
                hit->sector_id = child->sector_id;
                hit->wall_id = wall->wall_id;
                hit->wall = wall;
                hit->distance = distance + accum_dist;
                hit->rayAngle = ray_angle;

                float dx = hit_x - wall->x1;
                float dy = hit_y - wall->y1;
                hit->tileX = sqrtf(dx * dx + dy * dy);

                hit->wallHeight = child->ceiling_z - child->floor_z;
                hit->wallZOffset = child->floor_z;

                float angle_diff = ray_angle - engine->camera.rot;
                hit->correctDistance = hit->distance * cosf(angle_diff);
                hit->strip = strip_idx;

                num_local_hits++;
              } else {
                // Debug miss if close?
                // if (strip_idx == 320) printf("    Miss Wall %d\n",
                // wall->wall_id);
              }
            }
          }
        }
      }
    }
    for (int i = 0;
         i < sector->num_walls && num_local_hits < RAY_MAX_WALLS_PER_SECTOR;
         i++) {
      RAY_Wall *wall = &sector->walls[i];
      float distance, hit_x, hit_y;
      ray_find_wall_intersection(cur_x, cur_y, ray_angle, wall, &distance,
                                 &hit_x, &hit_y);

      if (distance < FLT_MAX) {
        RAY_RayHit *hit = &local_hits[num_local_hits];
        memset(hit, 0, sizeof(RAY_RayHit));

        hit->x = hit_x;
        hit->y = hit_y;
        hit->sector_id = sector->sector_id;
        hit->wall_id = wall->wall_id;
        hit->wall = wall;
        hit->distance = distance + accum_dist; // Global distance
        hit->rayAngle = ray_angle;

        float hit_dist = sqrtf((hit_x - wall->x1) * (hit_x - wall->x1) +
                               (hit_y - wall->y1) * (hit_y - wall->y1));
        hit->tileX = hit_dist;
        hit->wallHeight = sector->ceiling_z - sector->floor_z;
        hit->wallZOffset = sector->floor_z;
        hit->strip = strip_idx;

        // Fisheye correction handled in renderer or here?
        // Renderer does it again, but let's set it.
        float angle_diff = ray_angle - engine->camera.rot;
        hit->correctDistance = hit->distance * cosf(angle_diff);

        num_local_hits++;
      }
    }

    /* NESTED SECTORS FIX: Check child sector walls */
    if (ray_sector_has_children(sector)) {
      static int child_loop_debug = 0;
      if (child_loop_debug < 20 && depth == 0) {
        printf("DEBUG RAYCAST: Sector %d has %d children. FirstChild=%d\n",
               sector->sector_id, ray_sector_get_num_children(sector),
               ray_sector_get_child(sector, 0));
        fflush(stdout);
        child_loop_debug++;
      }

      for (int child_idx = 0; child_idx < ray_sector_get_num_children(sector);
           child_idx++) {
        int child_id = ray_sector_get_child(sector, child_idx);
        if (child_id < 0 || child_id >= engine->num_sectors)
          continue;

        RAY_Sector *child_sector = &engine->sectors[child_id];

        /* DEBUG RAYCAST CHILD */
        if (child_loop_debug < 20 && depth == 0) {
          printf("  Checking child sector %d (parent %d). NumWalls=%d\n",
                 child_id, sector->sector_id, child_sector->num_walls);
          fflush(stdout);
        }

        // Cast against child sector walls
        for (int i = 0; i < child_sector->num_walls &&
                        num_local_hits < RAY_MAX_WALLS_PER_SECTOR;
             i++) {
          RAY_Wall *wall = &child_sector->walls[i];
          float distance, hit_x, hit_y;
          ray_find_wall_intersection(cur_x, cur_y, ray_angle, wall, &distance,
                                     &hit_x, &hit_y);

          /* DEBUG GEOMETRY SECTOR 11 */
          if (strip_idx == 400 && child_id == 11) {
            static int geom_debug = 0;
            if (geom_debug < 20) {
              printf("  Sec11 Wall %d: (%.1f, %.1f) -> (%.1f, %.1f). Ray(%.1f, "
                     "%.1f) Ang=%.2f. Dist=%.1f\n",
                     wall->wall_id, wall->x1, wall->y1, wall->x2, wall->y2,
                     cur_x, cur_y, ray_angle, distance);
              geom_debug++;
            }
          }

          if (distance < FLT_MAX) {
            /* DEBUG HIT */
            if (strip_idx == 400 && child_id == 11) {
              printf("DEBUG RAYCAST HIT: Sector 11 Wall %d at dist %.1f\n",
                     wall->wall_id, distance);
            }

            RAY_RayHit *hit = &local_hits[num_local_hits];
            memset(hit, 0, sizeof(RAY_RayHit));

            hit->x = hit_x;
            hit->y = hit_y;
            hit->sector_id = child_sector->sector_id;
            hit->wall_id = wall->wall_id;
            hit->wall = wall;
            hit->distance = distance + accum_dist;
            hit->rayAngle = ray_angle;

            float hit_dist = sqrtf((hit_x - wall->x1) * (hit_x - wall->x1) +
                                   (hit_y - wall->y1) * (hit_y - wall->y1));
            hit->tileX = hit_dist;
            hit->wallHeight = child_sector->ceiling_z - child_sector->floor_z;
            hit->wallZOffset = child_sector->floor_z;
            hit->strip = strip_idx;

            float angle_diff = ray_angle - engine->camera.rot;
            hit->correctDistance = hit->distance * cosf(angle_diff);

            num_local_hits++;
          }
        }
      }
    }

    if (num_local_hits == 0) {
      // No hits in this sector? Ray likely exits map.
      break;
    }

    // Sort local hits by distance (Near to Far)
    qsort(local_hits, num_local_hits, sizeof(RAY_RayHit), ray_local_hit_sorter);

    // Process hits: Add to global list and check for Traversal
    int sector_exit_found = 0;

    for (int i = 0; i < num_local_hits; i++) {
      if (*num_hits >= RAY_MAX_RAYHITS)
        break;

      // Copy to global output
      memcpy(&hits[*num_hits], &local_hits[i], sizeof(RAY_RayHit));
      (*num_hits)++;

      // Check traversal
      RAY_RayHit *hit = &local_hits[i];

      if (hit->wall && hit->wall->portal_id >= 0) {
        // It is a portal -> Traverse to Next Sector

        int next_sector_id = -1;
        for (int p = 0; p < engine->num_portals; p++) {
          if (engine->portals[p].portal_id == hit->wall->portal_id) {
            next_sector_id = (engine->portals[p].sector_a == cur_sector_id)
                                 ? engine->portals[p].sector_b
                                 : engine->portals[p].sector_a;
            break;
          }
        }

        if (next_sector_id >= 0) {
          // Traverse
          cur_sector_id = next_sector_id;
          accum_dist +=
              (hit->distance - accum_dist); // Update base for next sector

          // Nudge origin
          float nudge = 0.1f;
          cur_x = hit->x + cosf(ray_angle) * nudge;
          cur_y = hit->y - sinf(ray_angle) * nudge;

          sector_exit_found = 1;
          break; // Stop processing THIS sector's hits (we left it via this
                 // portal)
        }
      }
      // If Solid, we continue the loop (transparency support).
      // Effectively, we "see through" constraints in this sector until we hit
      // an exit.
    }

    if (!sector_exit_found) {
      // We processed all walls and found no portal.
      // Ray ends here (blocked by solid walls or sector boundary).
      break;
    }
  }
}

/* ============================================================================
   SPRITE RAYCASTING
   ============================================================================
 */

/* Cast ray against sprites */
void ray_cast_sprites(RAY_Engine *engine, float ray_angle, int strip_idx,
                      RAY_RayHit *hits, int *num_hits) {
  if (!engine || !hits || !num_hits)
    return;

  for (int i = 0; i < engine->num_sprites; i++) {
    RAY_Sprite *sprite = &engine->sprites[i];

    if (sprite->hidden || sprite->cleanup)
      continue;

    /* Calculate sprite position relative to camera */
    float dx = sprite->x - engine->camera.x;
    float dy = sprite->y - engine->camera.y;
    float distance = sqrtf(dx * dx + dy * dy);

    if (distance < 0.1f)
      continue;

    /* Calculate angle to sprite */
    float sprite_angle = atan2f(-dy, dx);

    /* Normalize angle difference */
    float angle_diff = sprite_angle - ray_angle;
    while (angle_diff > M_PI)
      angle_diff -= RAY_TWO_PI;
    while (angle_diff < -M_PI)
      angle_diff += RAY_TWO_PI;

    /* Check if sprite is hit by this ray (within sprite width) */
    float sprite_angular_width = atan2f(sprite->w / 2.0f, distance);

    if (fabsf(angle_diff) < sprite_angular_width) {
      /* Ray hits sprite */
      if (*num_hits < RAY_MAX_RAYHITS) {
        RAY_RayHit *hit = &hits[*num_hits];
        memset(hit, 0, sizeof(RAY_RayHit));

        hit->sprite = sprite;
        hit->distance = distance;
        hit->correctDistance = distance * cosf(ray_angle - engine->camera.rot);
        hit->rayAngle = ray_angle;
        hit->strip = strip_idx;
        hit->x = sprite->x;
        hit->y = sprite->y;

        sprite->distance = distance;
        sprite->rayhit = 1;

        (*num_hits)++;
      }
    }
  }
}

/* ============================================================================
   COLLISION DETECTION
   ============================================================================
 */

/* Check if a point can move to a new position (collision detection) */
int ray_check_collision(RAY_Engine *engine, float x, float y, float z,
                        float new_x, float new_y) {
  if (!engine)
    return 1; /* Blocked by default */

  /* Find sector at new position - Use Z-aware version! */
  RAY_Sector *new_sector = ray_find_sector_at_position(engine, new_x, new_y, z);

  if (!new_sector) {
    return 1; /* Outside all sectors - blocked */
  }

  /* Find sector at current position */
  RAY_Sector *current_sector = ray_find_sector_at_position(engine, x, y, z);

  if (!current_sector) {
    return 0; /* Currently outside - allow movement into sector */
  }

  // =========================================================================
  // 0. WALL INTERSECTION CHECK (ROBUST)
  // Prevent crossing any solid wall, regardless of sector hierarchy.
  // =========================================================================
  for (int w = 0; w < current_sector->num_walls; w++) {
    RAY_Wall *wall = &current_sector->walls[w];
    float ix, iy;

    // Use a small epsilon shrink for the check to allow 'sliding' along wall?
    // No, strict check first.
    if (ray_line_segment_intersect(x, y, new_x, new_y, wall->x1, wall->y1,
                                   wall->x2, wall->y2, &ix, &iy)) {

      // If SOLID WALL -> BLOCK
      // Also block if it's a closed door? (Not handled here, purely geometry)
      if (wall->portal_id == -1) {
        // Ignore intersection at exact start point to allow moving AWAY from
        // wall
        float dx = ix - x;
        float dy = iy - y;
        if (dx * dx + dy * dy > 0.001f) {
          return 1; // Collision with wall
        }
      }
    }
  }

  // =========================================================================
  // 0.B. NEW SECTOR WALL CHECK (CRITICAL FOR NESTED ISLANDS)
  // If moving into a nested sector (island), we must not cross its solid walls.
  // The parent sector might not know about these walls (no inner loop), so we
  // check the target.
  // =========================================================================
  if (new_sector && new_sector != current_sector) {
    for (int w = 0; w < new_sector->num_walls; w++) {
      RAY_Wall *wall = &new_sector->walls[w];
      float ix, iy;

      if (ray_line_segment_intersect(x, y, new_x, new_y, wall->x1, wall->y1,
                                     wall->x2, wall->y2, &ix, &iy)) {

        // If SOLID WALL -> BLOCK
        if (wall->portal_id == -1) {
          return 1; // Collision with nested sector wall
        }
      }
    }
  }

  // Physics Constants
  float player_height = 32.0f; // Simplified player height
  float step_height = 32.0f;   // Max step up

  /* Special handling for SOLID sectors (Columns, Boxes) */
  if (ray_sector_is_solid(new_sector) && new_sector != current_sector) {
    if (new_sector->sector_id != current_sector->sector_id)
      printf("DEBUG: Enter SOLID Sec %d from %d\n", new_sector->sector_id,
             current_sector->sector_id);
    int allowed_entry = 0;

    /* 1. Check Hierarchy (Robust check for Parent <-> Nested Room) */
    /* DISABLED: Force strictly portal-based movement. If a nested sector is
     * solid (no portal), we can't enter. */
    /*
    if (ray_sector_get_parent(new_sector) == current_sector->sector_id ||
        ray_sector_get_parent(current_sector) == new_sector->sector_id) {
        printf("DEBUG: Hierarchy Allow! %d <-> %d\n", new_sector->sector_id,
    current_sector->sector_id); allowed_entry = 1;
    }
    */

    /* 2. Check Portals (Fallback if hierarchy not set but portal exists) */
    if (!allowed_entry) {
      for (int i = 0; i < current_sector->num_portals; i++) {
        int portal_id = current_sector->portal_ids[i];
        for (int p = 0; p < engine->num_portals; p++) {
          if (engine->portals[p].portal_id == portal_id) {
            if (engine->portals[p].sector_a == new_sector->sector_id ||
                engine->portals[p].sector_b == new_sector->sector_id) {
              allowed_entry = 1;
              break;
            }
          }
        }
        if (allowed_entry)
          break;
      }
    }

    /* 3. Check Walls for Portal (Fallback if sector portal list is
     * desync/empty) */
    /* This guarantees that if a wall has a portal, we can pass, matching
     * rendering logic */
    if (!allowed_entry) {
      for (int w = 0; w < current_sector->num_walls; w++) {
        RAY_Wall *wall = &current_sector->walls[w];
        if (wall->portal_id != -1) {
          // Check if this portal connects to new_sector
          for (int p = 0; p < engine->num_portals; p++) {
            if (engine->portals[p].portal_id == wall->portal_id) {
              if (engine->portals[p].sector_a == new_sector->sector_id ||
                  engine->portals[p].sector_b == new_sector->sector_id) {
                allowed_entry = 1;
                break;
              }
            }
          }
        }
        if (allowed_entry)
          break;
      }
    }

    /* If entry is allowed (e.g. walking into a nested room), we must still
     * validate Z height */
    /* i.e. we can't walk into a solid block, but we CAN walk into a solid room
     */
    if (allowed_entry) {
      // Standard Walkability Check for the nested sector
      float floor_step = new_sector->floor_z - z;
      float ceiling_headroom = new_sector->ceiling_z - z;

      // Step check (e.g., max 32 units up)
      // Note: If floor_step is negative, we are stepping down (allowed)
      if (floor_step > step_height) {
        printf("DEBUG: Block Trigger: Floor Step Too High %.1f > %.1f\n",
               floor_step, step_height);
        return 1; // Wall too high
      }

      // Headroom check
      // If ceiling is too low for player height
      if (ceiling_headroom < player_height) {
        printf("DEBUG: Block Trigger: Headroom too low %.1f < %.1f\n",
               ceiling_headroom, player_height);
        return 1; // Bump head
      }

      return 0; // Entry permitted
    }

    /* If NOT allowed entry (true solid object), trigger "On Top / Below" logic
     */

    /* Check bounds with slight margin */
    float margin = 5.0f; // Tolerance

    // Are we strictly above the object?
    // Z increases downwards? No. Z is height usually?
    // Build Engine: Floor Z is height? OR Z is depth?
    // In previous edits we saw: floor_h = sector->floor_z - cam.z;
    // And render uses: y = half - (h / z).
    // Standard Build: Z increases DOWNWARDS usually (0=Ceiling, High=Floor).
    // But my sector data names are ceiling_z and floor_z.
    // Let's assume standard intuitive Z (Up is + or Up is -?).
    // If "floor_z > z + step" blocked me before... (Condition was if (new_floor
    // > z+step) return 1) This implies floor_z is the "ground level". And Z is
    // my feet. If floor > feet, floor is above me. Blocked. So Z increases
    // UPWARDS? Let's check `libmod_ray_move_forward`: g_engine.camera.z is Z.
    // Rendering: top = half - (ceil_h / z).
    // If ceil_h is positive, top is above horizon.
    // ceil_h = ceil_z - cam_z.
    // If ceil_z > cam_z -> Positive H -> Up on screen.
    // So Z increases UPWARDS. (Higher Z is higher altitude).

    // Collision Logic for Solid Box [floor_z, ceiling_z]
    // If (z >= ceiling_z) -> We are on top. OK. (Pass)
    // If (z + height <= floor_z) -> We are below? (Bridge). OK.
    // Otherwise -> Blocked.

    if (z >= new_sector->ceiling_z - margin)
      return 0; // On top

    // Step Up Check
    if (new_sector->ceiling_z <= z + step_height)
      return 0;

    // Fix: Allow moving BELOW solid sector (e.g. Floating Island)
    // If we are below the floor of the solid object.
    // Assuming player_height is headroom needed.
    if (z + player_height <= new_sector->floor_z + margin)
      return 0;

    if (new_sector->sector_id != current_sector->sector_id)
      printf("DEBUG: Block Trigger: Solid Sector Reject (Normal)\n");
    return 1; // Collision with solid body
  }

  /* Z-Height Check for Nested/Portal Sectors */

  // Check floor collision (too high step)
  // If we want to climb a step, floor must be reachable.
  // If new_floor > z + step: Wall.
  if (new_sector->floor_z > z + 32.0f && !ray_sector_is_solid(new_sector))
    return 1;

  // Check ceiling (bump head)
  if (new_sector->ceiling_z < z + 32.0f && !ray_sector_is_solid(new_sector))
    return 1;

  /* If moving to same sector, allow (Z check passed) */
  if (new_sector->sector_id == current_sector->sector_id) {
    return 0;
  }

  // Fix: Allow exiting a solid sector (if we were on top of it)
  if (ray_sector_is_solid(current_sector)) {
    return 0;
  }

  /* Moving to different sector - check if there's a portal */
  for (int i = 0; i < current_sector->num_portals; i++) {
    int portal_id = current_sector->portal_ids[i];

    for (int p = 0; p < engine->num_portals; p++) {
      RAY_Portal *portal = &engine->portals[p];

      if (portal->portal_id == portal_id) {
        /* Check if portal connects to new sector */
        if (portal->sector_a == new_sector->sector_id ||
            portal->sector_b == new_sector->sector_id) {

          /* Portal connecting to non-solid sector. Z-checks passed. OK. */
          return 0; /* Can move through portal */
        }
      }
    }
  }

  /* No direct portal found - check hierarchy (parent/child/grandchild) */
  /* Allow movement if one sector is an ancestor/descendant of the other */
  {
    /* Check if new_sector is a descendant of current_sector */
    int check_id = new_sector->parent_sector_id;
    while (check_id >= 0) {
      if (check_id == current_sector->sector_id) {
        return 0; /* new_sector is a descendant */
      }
      /* Walk up the hierarchy */
      RAY_Sector *parent = NULL;
      for (int s = 0; s < engine->num_sectors; s++) {
        if (engine->sectors[s].sector_id == check_id) {
          parent = &engine->sectors[s];
          break;
        }
      }
      if (parent)
        check_id = parent->parent_sector_id;
      else
        break;
    }

    /* Check if current_sector is a descendant of new_sector */
    check_id = current_sector->parent_sector_id;
    while (check_id >= 0) {
      if (check_id == new_sector->sector_id) {
        return 0; /* current_sector is a descendant */
      }
      RAY_Sector *parent = NULL;
      for (int s = 0; s < engine->num_sectors; s++) {
        if (engine->sectors[s].sector_id == check_id) {
          parent = &engine->sectors[s];
          break;
        }
      }
      if (parent)
        check_id = parent->parent_sector_id;
      else
        break;
    }

    /* Check if they share the same parent (siblings) */
    if (current_sector->parent_sector_id >= 0 &&
        current_sector->parent_sector_id == new_sector->parent_sector_id) {
      return 0; /* Siblings under same parent */
    }
  }

  /* Truly no connection - block */
  return 1;
}
