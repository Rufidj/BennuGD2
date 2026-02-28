/*
 * libmod_ray_render.c - Rendering System for Geometric Sectors
 * Complete rewrite - Build Engine style rendering
 */

#include "libmod_ray.h"
#include "libmod_ray_compat.h"
#include <SDL2/SDL.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* External engine instance */
extern RAY_Engine g_engine;

/* External raycasting functions */
extern void ray_cast_ray(RAY_Engine *engine, int sector_id, float x, float y,
                         float ray_angle, int strip_idx, RAY_RayHit *hits,
                         int *num_hits);
extern void ray_cast_sprites(RAY_Engine *engine, float ray_angle, int strip_idx,
                             RAY_RayHit *hits, int *num_hits);

/* External globals */
extern SDL_PixelFormat *gPixelFormat;

/* ============================================================================
   UTILITY FUNCTIONS
   ============================================================================
 */

/* Helper to sanitize pixel format (force opaque RGB) */
static inline uint32_t ray_convert_pixel(uint32_t pixel) {
  /* If gPixelFormat is not available, we can't do safe conversion */
  if (!gPixelFormat)
    return pixel;

  /* Extract RGB using the global format masks/shifts */
  uint8_t r = (pixel >> gPixelFormat->Rshift) & 0xFF;
  uint8_t g = (pixel >> gPixelFormat->Gshift) & 0xFF;
  uint8_t b = (pixel >> gPixelFormat->Bshift) & 0xFF;

  /* Re-map to opaque color in the same format */
  return SDL_MapRGB(gPixelFormat, r, g, b);
}

float ray_screen_distance(float screenWidth, float fovRadians) {
  return (screenWidth / 2.0f) / tanf(fovRadians / 2.0f);
}

float ray_strip_screen_height(float screenDistance, float correctDistance,
                              float height) {
  if (correctDistance < 1.0f)
    correctDistance = 1.0f;
  return (screenDistance / correctDistance) * height;
}

/* ============================================================================
   TEXTURE SAMPLING
   ============================================================================
 */

uint32_t ray_sample_texture(GRAPH *texture, int tex_x, int tex_y) {
  if (!texture || tex_x < 0 || tex_y < 0 || tex_x >= texture->width ||
      tex_y >= texture->height) {
    return 0;
  }

  return gr_get_pixel(texture, tex_x, tex_y);
}

/* Vertical Linear Sampling - Only interpolates vertically to avoid horizontal
 * artifacts */
uint32_t ray_sample_texture_bilinear(GRAPH *texture, float u, float v) {
  if (!texture)
    return 0;

  /* Round U to nearest integer - no horizontal interpolation */
  int tex_x = (int)(u + 0.5f);

  /* Vertical interpolation */
  float fv = v - 0.5f;
  int y0 = (int)floorf(fv);
  float fy = fv - y0;
  int y1 = y0 + 1;

  /* Wrap coordinates */
  tex_x = (tex_x % texture->width + texture->width) % texture->width;
  y0 = (y0 % texture->height + texture->height) % texture->height;
  y1 = (y1 % texture->height + texture->height) % texture->height;

  /* Sample two vertical pixels */
  uint32_t c0 = gr_get_pixel(texture, tex_x, y0);
  uint32_t c1 = gr_get_pixel(texture, tex_x, y1);

  if (!gPixelFormat)
    return c0;

  /* Extract and interpolate */
  uint8_t r0, g0, b0;
  uint8_t r1, g1, b1;
  SDL_GetRGB(c0, gPixelFormat, &r0, &g0, &b0);
  SDL_GetRGB(c1, gPixelFormat, &r1, &g1, &b1);

  uint8_t rf = (uint8_t)(r0 * (1.0f - fy) + r1 * fy);
  uint8_t gf = (uint8_t)(g0 * (1.0f - fy) + g1 * fy);
  uint8_t bf = (uint8_t)(b0 * (1.0f - fy) + b1 * fy);

  return SDL_MapRGB(gPixelFormat, rf, gf, bf);
}

/* ============================================================================
   FOG SYSTEM
   ============================================================================
 */

uint32_t ray_fog_pixel(uint32_t pixel, float distance) {
  if (!g_engine.fogOn)
    return pixel;

  if (distance < g_engine.fog_start_distance)
    return pixel;
  if (distance > g_engine.fog_end_distance) {
    return (g_engine.fog_r << 16) | (g_engine.fog_g << 8) | g_engine.fog_b;
  }

  float fog_factor = (distance - g_engine.fog_start_distance) /
                     (g_engine.fog_end_distance - g_engine.fog_start_distance);

  uint8_t r = (pixel >> 16) & 0xFF;
  uint8_t g = (pixel >> 8) & 0xFF;
  uint8_t b = pixel & 0xFF;

  r = (uint8_t)(r * (1.0f - fog_factor) + g_engine.fog_r * fog_factor);
  g = (uint8_t)(g * (1.0f - fog_factor) + g_engine.fog_g * fog_factor);
  b = (uint8_t)(b * (1.0f - fog_factor) + g_engine.fog_b * fog_factor);

  return (r << 16) | (g << 8) | b;
}

/* ============================================================================
   WALL RENDERING WITH MULTIPLE TEXTURES
   ============================================================================
 */

void ray_draw_wall_strip(GRAPH *dest, RAY_RayHit *rayHit, int screen_x,
                         int *ceiling_clip, int *floor_clip) {
  if (!dest || !rayHit || !rayHit->wall)
    return;

  RAY_Wall *wall = rayHit->wall;

  /* Portal walls should render upper/lower textures, but not middle */
  int is_portal = (wall->portal_id >= 0);

  /* DEBUG: Print first wall rendering attempt */
  static int debug_once = 0;
  if (debug_once < 3) {
    printf("RAY DEBUG: Wall render - lower=%d, middle=%d, upper=%d, "
           "is_portal=%d, split_lower_z=%.1f, split_upper_z=%.1f\n",
           wall->texture_id_lower, wall->texture_id_middle,
           wall->texture_id_upper, is_portal, wall->texture_split_z_lower,
           wall->texture_split_z_upper);
    debug_once++;
  }

  /* Calculate wall screen height */
  int wall_screen_height = (int)ray_strip_screen_height(
      g_engine.viewDist, rayHit->correctDistance, rayHit->wallHeight);

  /* Calculate player screen Z (for vertical positioning) */
  /* Calculate player screen Z (projected floor position relative to center) */
  /* player_screen_z is POSITIVE if camera > floor (looking down pushes floor
   * down?) */
  /* Wait: If CameraZ > FloorZ, diff is positive. */
  /* Standard Projection: Y = center + (CamZ - FloorZ) / Dist * Scale */
  /* So player_screen_z should be added to center to get Floor Y */

  float player_screen_z =
      ray_strip_screen_height(g_engine.viewDist, rayHit->correctDistance,
                              g_engine.camera.z - rayHit->wallZOffset);

  /* Calculate wall top and bottom on screen */
  /* Correct Logic: */
  /* Wall Bottom is at Floor Level = Center + player_screen_z */
  /* Wall Top is at Ceiling Level = Wall Bottom - wall_screen_height */

  /* Correct Logic with Pitch: */
  /* Wall Bottom is at Floor Level = Center + Pitch + player_screen_z */
  int wall_bottom = g_engine.displayHeight / 2 + (int)g_engine.camera.pitch +
                    (int)player_screen_z;
  int wall_top = wall_bottom - wall_screen_height;

  /* Texture coordinate X (horizontal along wall) */
  int tex_x = ((int)rayHit->tileX) % RAY_TEXTURE_SIZE;
  if (tex_x < 0)
    tex_x += RAY_TEXTURE_SIZE;

  /* Calculate screen heights for texture splits */
  /* NOTE: texture_split_z values are ABSOLUTE world Z coordinates */
  /* We need to convert them to screen positions */

  float split_lower_z_world = wall->texture_split_z_lower;
  float split_upper_z_world = wall->texture_split_z_upper;
  int use_full_wall = (fabsf(wall->texture_split_z_lower - 64.0f) < 0.1f &&
                       fabsf(wall->texture_split_z_upper - 192.0f) < 0.1f);

  /* For portals, AUTOMATICALLY set splits to neighbor sector heights */
  if (is_portal) {
    for (int p = 0; p < g_engine.num_portals; p++) {
      if (g_engine.portals[p].portal_id == wall->portal_id) {
        /* Determine neighbor sector */
        /* Warning: rayHit->sector_id might be unreliable if ray passed through
         * multiple portals? */
        /* But wait, rayHit->wall belongs to a specific sector. We need THAT
         * sector. */
        /* Actually g_engine.portals[p] links sector_a and sector_b. One of them
         * is our current sector. */
        /* To be safe, checking which one contains this wall is hard without
         * sector->walls iteration. */
        /* But we can check rayHit->sector_id (the sector we are rendering
         * FROM). */
        /* Wait, if we are rendering a wall, we are inside its sector looking AT
         * it. */

        int neighbor_id = -1;
        if (g_engine.portals[p].sector_a == rayHit->sector_id)
          neighbor_id = g_engine.portals[p].sector_b;
        else if (g_engine.portals[p].sector_b == rayHit->sector_id)
          neighbor_id = g_engine.portals[p].sector_a;

        if (neighbor_id >= 0 && neighbor_id < g_engine.num_sectors) {
          RAY_Sector *neighbor = &g_engine.sectors[neighbor_id];
          split_lower_z_world = neighbor->floor_z;
          split_upper_z_world = neighbor->ceiling_z;
          use_full_wall = 0; /* Force split rendering logic */

          /* DEBUG PORTAL SPLITS */
          static int portal_debug = 0;
          if (portal_debug < 10) { // Increased limit
            printf("PORTAL DEBUG: RayHitSector=%d Wall=%d Portal=%d -> "
                   "Neighbor=%d. Splits=%.1f/%.1f\n",
                   rayHit->sector_id, wall->wall_id, wall->portal_id,
                   neighbor_id, split_lower_z_world, split_upper_z_world);
            portal_debug++;
          }
        } else {
          // Debug failure to find neighbor
          static int portal_fail = 0;
          if (portal_fail < 5) {
            printf("PORTAL FAIL: RayHitSector=%d Portal=%d (SecA=%d, SecB=%d) "
                   "-> No Neighbor Found!\n",
                   rayHit->sector_id, wall->portal_id,
                   g_engine.portals[p].sector_a, g_engine.portals[p].sector_b);
            portal_fail++;
          }
        }
        break;
      }
    }
  }

  float split_lower_relative = split_lower_z_world - rayHit->wallZOffset;
  float split_upper_relative = split_upper_z_world - rayHit->wallZOffset;

  int split_lower_screen, split_upper_screen;

  if (use_full_wall) {
    /* Use entire wall for middle texture */
    split_lower_screen =
        wall_bottom; // Inverted? No, screen Y increases down. Bottom > Top.
    split_upper_screen =
        wall_top; // Wait, split_lower should be closer to bottom?

    // Let's rethink splits.
    // Lower texture: Floor to Split1.
    // Middle texture: Split1 to Split2.
    // Upper texture: Split2 to Ceiling.

    // Screen Y:
    // Floor is at wall_bottom (High Y).
    // Ceiling is at wall_top (Low Y).

    // So Split Lower (Z=64) is closer to Floor.
    // Split Upper (Z=192) is closer to Ceiling.

    split_lower_screen = wall_bottom; // Default to full wall? No, full wall
                                      // means middle covers everything.
    /* If use_full_wall, middle texture covers (bottom -> top). */
    /* So lower texture is empty (bottom -> bottom), upper is empty (top -> top)
     */
    split_lower_screen = wall_bottom;
    split_upper_screen = wall_top;
  } else {
    /* Clamp splits to wall bounds */
    if (split_lower_relative < 0)
      split_lower_relative = 0;
    if (split_lower_relative > rayHit->wallHeight)
      split_lower_relative = rayHit->wallHeight;
    if (split_upper_relative < 0)
      split_upper_relative = 0;
    if (split_upper_relative > rayHit->wallHeight)
      split_upper_relative = rayHit->wallHeight;

    /* Convert relative Z height to screen pixels */
    /* relative 0 = Floor (wall_bottom). relative H = Ceiling (wall_top). */
    /* Y = Bottom - (RelativeZ / WallH) * ScreenH */

    split_lower_screen =
        wall_bottom -
        (int)((split_lower_relative / rayHit->wallHeight) * wall_screen_height);
    split_upper_screen =
        wall_bottom -
        (int)((split_upper_relative / rayHit->wallHeight) * wall_screen_height);
  }

  /* Calculate vertical scaling factor (World Units per Screen Pixel) */
  float scale_factor = rayHit->wallHeight / (float)wall_screen_height;

  /* Render each texture section */
  int strip_width = g_engine.stripWidth;

  /* LOWER TEXTURE (floor to split_lower) -- STRETCH TO FIT */
  /* User Request: For portals, use MIDDLE texture for the lower step */
  int lower_tex_id =
      is_portal ? wall->texture_id_middle : wall->texture_id_lower;

  if (lower_tex_id > 0) {
    GRAPH *texture = bitmap_get(g_engine.fpg_id, lower_tex_id);
    if (texture) {
      int section_top = split_lower_screen;
      int section_bottom = wall_bottom;

      for (int sy = section_top;
           sy < section_bottom && sy < g_engine.displayHeight; sy++) {
        if (sy < 0)
          continue;

        /* Calculate texture Y coordinate - Stretched */
        float progress =
            (float)(sy - section_top) / (float)(section_bottom - section_top);
        int tex_y = (int)(progress * texture->height);
        if (tex_y >= texture->height)
          tex_y = texture->height - 1;

        uint32_t pixel = ray_sample_texture(texture, tex_x, tex_y);
        if (pixel == 0)
          continue; /* Transparent */

        pixel = ray_convert_pixel(pixel);

        if (g_engine.fogOn) {
          pixel = ray_fog_pixel(pixel, rayHit->distance);
        }

        for (int sx = 0;
             sx < strip_width && screen_x + sx < g_engine.displayWidth; sx++) {
          gr_put_pixel(dest, screen_x + sx, sy, pixel);
        }
      }
    }
  }

  /* MIDDLE TEXTURE (split_lower to split_upper) */
  if (!is_portal && wall->texture_id_middle > 0) {
    GRAPH *texture = bitmap_get(g_engine.fpg_id, wall->texture_id_middle);
    if (texture) {
      int section_top = split_upper_screen;
      int section_bottom = split_lower_screen;

      for (int sy = section_top;
           sy < section_bottom && sy < g_engine.displayHeight; sy++) {
        if (sy < 0)
          continue;

        float progress =
            (float)(sy - section_top) / (float)(section_bottom - section_top);
        int tex_y = (int)(progress * texture->height);
        if (tex_y >= texture->height)
          tex_y = texture->height - 1;

        uint32_t pixel = ray_sample_texture(texture, tex_x, tex_y);
        if (pixel == 0)
          continue;

        pixel = ray_convert_pixel(pixel);

        if (g_engine.fogOn) {
          pixel = ray_fog_pixel(pixel, rayHit->distance);
        }

        for (int sx = 0;
             sx < strip_width && screen_x + sx < g_engine.displayWidth; sx++) {
          gr_put_pixel(dest, screen_x + sx, sy, pixel);
        }
      }
    }
  }

  /* UPPER TEXTURE (split_upper to ceiling) -- STRETCH TO FIT */
  if (wall->texture_id_upper > 0) {
    GRAPH *texture = bitmap_get(g_engine.fpg_id, wall->texture_id_upper);
    if (texture) {
      int section_top = wall_top;
      int section_bottom = split_upper_screen;

      for (int sy = section_top;
           sy < section_bottom && sy < g_engine.displayHeight; sy++) {
        if (sy < 0)
          continue;

        /* Calculate texture Y coordinate - Stretched */
        float progress =
            (float)(sy - section_top) / (float)(section_bottom - section_top);
        int tex_y = (int)(progress * texture->height);
        if (tex_y >= texture->height)
          tex_y = texture->height - 1;

        uint32_t pixel = ray_sample_texture(texture, tex_x, tex_y);
        if (pixel == 0)
          continue;

        pixel = ray_convert_pixel(pixel);

        if (g_engine.fogOn) {
          pixel = ray_fog_pixel(pixel, rayHit->distance);
        }

        for (int sx = 0;
             sx < strip_width && screen_x + sx < g_engine.displayWidth; sx++) {
          gr_put_pixel(dest, screen_x + sx, sy, pixel);
        }
      }
    }
  }

  /* CLIP INTERSECTION: Update clipping bounds to reduce visible range */
  int strip_idx = screen_x / g_engine.stripWidth;
  if (strip_idx >= 0 && strip_idx < g_engine.rayCount) {
    /* Check if this wall belongs to a solid child sector */
    /* Solid child sectors should NOT update clipping - we want parent
     * floor/ceiling to render around them, not be blocked by them */
    int is_solid_child = 0;

    /* Find the sector this wall belongs to */
    RAY_Sector *wall_sector = NULL;
    for (int i = 0; i < g_engine.num_sectors; i++) {
      if (g_engine.sectors[i].sector_id == rayHit->sector_id) {
        wall_sector = &g_engine.sectors[i];
        break;
      }
    }

    /* For solid child sectors WITHOUT ceiling texture, do NOT update
     * ceiling_clip */
    /* This allows parent ceiling to render through them */
    int skip_ceiling_clip =
        (wall_sector && ray_sector_get_parent(wall_sector) >= 0 &&
         ray_sector_is_solid(wall_sector) &&
         wall_sector->ceiling_texture_id <= 0);

    if (!skip_ceiling_clip) {
      /* Normal clipping: ceiling can't render below wall */
      if (ceiling_clip && wall_bottom < ceiling_clip[strip_idx]) {
        static int clip_update_debug = 0;
        if (clip_update_debug < 5 && strip_idx == g_engine.rayCount / 2) {
          printf(
              "CLIP UPDATE: strip=%d, wall_bottom=%d, ceiling_clip: %d->%d\n",
              strip_idx, wall_bottom, ceiling_clip[strip_idx], wall_bottom);
          clip_update_debug++;
        }
        ceiling_clip[strip_idx] = wall_bottom;
      }
    } else {
      static int skip_clip_debug = 0;
      if (skip_clip_debug < 5 && strip_idx == g_engine.rayCount / 2) {
        printf("SKIPPING CEILING_CLIP UPDATE: solid sector %d without ceiling "
               "texture\n",
               wall_sector->sector_id);
        skip_clip_debug++;
      }
    }

    /* floor_clip: MAXIMUM of current and wall_top (floor can't render above
     * wall) */
    if (floor_clip && wall_top > floor_clip[strip_idx]) {
      static int clip_update_debug2 = 0;
      if (clip_update_debug2 < 5 && strip_idx == g_engine.rayCount / 2) {
        printf("FLOOR CLIP UPDATE: %d (sector_id=%d)\n", wall_top,
               rayHit->sector_id);
        clip_update_debug2++;
      }
      floor_clip[strip_idx] = wall_top;
    }
  }
}

/* ============================================================================
   FLOOR AND CEILING RENDERING
   ============================================================================
 */

void ray_draw_floor_ceiling(GRAPH *dest, int screen_x, float ray_angle,
                            int sector_id, float min_distance,
                            float max_distance, float *z_buffer,
                            int *ceiling_clip, int *floor_clip) {
  if (!dest)
    return;

  int strip_width = g_engine.stripWidth;
  float cos_factor = cosf(ray_angle - g_engine.camera.rot);

  /* Find the specified sector */
  RAY_Sector *sector = NULL;
  // Fast lookup if ID matches index
  if (sector_id >= 0 && sector_id < g_engine.num_sectors &&
      g_engine.sectors[sector_id].sector_id == sector_id) {
    sector = &g_engine.sectors[sector_id];
  } else {
    for (int i = 0; i < g_engine.num_sectors; i++) {
      if (g_engine.sectors[i].sector_id == sector_id) {
        sector = &g_engine.sectors[i];
        break;
      }
    }
  }
  if (!sector)
    return;

  /* Detect Solid Child Sector Mode (External View) */
  int is_solid_child =
      (ray_sector_get_parent(sector) >= 0 && ray_sector_is_solid(sector));

  /* Determine surfaces to draw based on camera position relative to sector */
  /* DEFAULT (Interior View): Top is Ceiling, Bottom is Floor */
  int draw_top = 1;    // Screen Top (Ceiling)
  int draw_bottom = 1; // Screen Bottom (Floor)

  float top_z = sector->ceiling_z;
  int top_tex = sector->ceiling_texture_id;

  float bottom_z = sector->floor_z;
  int bottom_tex = sector->floor_texture_id;

  if (is_solid_child) {
    /* EXTERIOR VIEW (Solid Block) */
    draw_top = 0;
    draw_bottom = 0;

    /* If Camera < FloorZ (Below box): See Bottom Face (acts as Ceiling) */
    if (g_engine.camera.z < sector->floor_z) {
      draw_top = 1;            // Draw in upper half of screen
      top_z = sector->floor_z; // The surface is the box bottom
      top_tex =
          sector->floor_texture_id; // Use floor texture for bottom face? Or
                                    // ceiling? Convention: Floor texture on
                                    // floor surface. The bottom of a crate is
                                    // its floor. So use floor_texture.
    }

    /* If Camera > CeilingZ (Above box): See Top Face (acts as Floor) */
    if (g_engine.camera.z > sector->ceiling_z) {
      draw_bottom = 1;              // Draw in lower half of screen
      bottom_z = sector->ceiling_z; // The surface is the box top
      bottom_tex =
          sector->ceiling_texture_id; // Use ceiling texture for top face.
    }
  }

  /* Calculate clipping */
  int horizon_y = g_engine.displayHeight / 2 + (int)g_engine.camera.pitch;
  int floor_start_y = horizon_y + 1;
  int ceiling_end_y = horizon_y - 1;

  /* Clamp horizon to screen */
  if (floor_start_y < 0)
    floor_start_y = 0;
  if (ceiling_end_y >= g_engine.displayHeight)
    ceiling_end_y = g_engine.displayHeight - 1;

  /* --------------------------------------------------------------------
     UPPER SCREEN HALF (Ceiling / Bottom Face of Box)
     -------------------------------------------------------------------- */
  if (g_engine.drawCeiling && draw_top && top_tex > 0) {
    GRAPH *texture = bitmap_get(g_engine.fpg_id, top_tex);
    if (texture) {
      for (int screen_y = 0;
           screen_y < ceiling_end_y && screen_y < g_engine.displayHeight;
           screen_y++) {
        int dy = horizon_y - screen_y;
        if (dy <= 0)
          continue;

        float distance_to_plane = top_z - g_engine.camera.z;

        /* For normal ceiling, dist is positive. */
        /* For solid box bottom face (Cam < FloorZ), dist is positive (FloorZ >
         * Cam). */
        /* If negative/zero, we can't see it (backface or invalid). */
        if (distance_to_plane <= 0.1f)
          continue;

        float ratio = distance_to_plane / fabsf((float)dy);
        float perp_distance = g_engine.viewDist * ratio;

        float angle_diff = ray_angle - g_engine.camera.rot;
        float euclidean_distance = perp_distance / cosf(angle_diff);

        /* Clip to sector boundaries */
        if (euclidean_distance > max_distance)
          break; // Optimization: too far
        if (euclidean_distance < min_distance)
          continue; // Too close (portal gap)

        float x_end = g_engine.camera.x + euclidean_distance * cosf(ray_angle);
        float y_end = g_engine.camera.y + euclidean_distance * -sinf(ray_angle);

        /* VERTICAL CLIPPING */
        int strip_idx = screen_x / strip_width;
        if (ceiling_clip && strip_idx < g_engine.rayCount) {
          if (screen_y >= ceiling_clip[strip_idx])
            continue;
        }

        /* Z-BUFFER CHECK */
        if (z_buffer && strip_idx < g_engine.rayCount) {
          if (euclidean_distance >= z_buffer[strip_idx])
            continue;
        }

        int tex_x = ((int)x_end) % RAY_TEXTURE_SIZE;
        int tex_y = ((int)y_end) % RAY_TEXTURE_SIZE;
        if (tex_x < 0)
          tex_x += RAY_TEXTURE_SIZE;
        if (tex_y < 0)
          tex_y += RAY_TEXTURE_SIZE;

        /* Scale texture */
        tex_x = (tex_x * texture->width) / RAY_TEXTURE_SIZE;
        tex_y = (tex_y * texture->height) / RAY_TEXTURE_SIZE;

        uint32_t pixel = ray_sample_texture(texture, tex_x, tex_y);
        pixel = ray_convert_pixel(pixel);
        if (g_engine.fogOn)
          pixel = ray_fog_pixel(pixel, euclidean_distance);

        for (int sx = 0;
             sx < strip_width && screen_x + sx < g_engine.displayWidth; sx++)
          gr_put_pixel(dest, screen_x + sx, screen_y, pixel);
      }
    }
  }

  /* --------------------------------------------------------------------
     LOWER SCREEN HALF (Floor / Top Face of Box)
     -------------------------------------------------------------------- */
  if (g_engine.drawTexturedFloor && draw_bottom && bottom_tex > 0) {
    GRAPH *texture = bitmap_get(g_engine.fpg_id, bottom_tex);
    if (texture) {
      for (int screen_y = floor_start_y; screen_y < g_engine.displayHeight;
           screen_y++) {
        int dy = screen_y - horizon_y;
        if (dy <= 0)
          continue;

        float distance_to_plane = g_engine.camera.z - bottom_z;

        /* For normal floor, dist is positive (Cam > Floor). */
        /* For solid box top face (Cam > CeilZ), dist is positive (Cam > CeilZ).
         */
        if (distance_to_plane <= 0.1f)
          continue;

        float ratio = distance_to_plane / (float)dy;
        float perp_distance = g_engine.viewDist * ratio;

        float angle_diff = ray_angle - g_engine.camera.rot;
        float euclidean_distance = perp_distance / cosf(angle_diff);

        if (euclidean_distance > max_distance)
          continue;
        if (euclidean_distance < min_distance)
          break; // Optimization: closer pixels will be closer

        float x_end = g_engine.camera.x + euclidean_distance * cosf(ray_angle);
        float y_end = g_engine.camera.y + euclidean_distance * -sinf(ray_angle);

        /* VERTICAL CLIPPING */
        int strip_idx = screen_x / strip_width;
        if (floor_clip && strip_idx < g_engine.rayCount) {
          if (screen_y <= floor_clip[strip_idx])
            continue;
        }

        /* Z-BUFFER CHECK */
        if (z_buffer && strip_idx < g_engine.rayCount) {
          if (euclidean_distance >= z_buffer[strip_idx])
            continue;
        }

        int tex_x = ((int)x_end) % RAY_TEXTURE_SIZE;
        int tex_y = ((int)y_end) % RAY_TEXTURE_SIZE;
        if (tex_x < 0)
          tex_x += RAY_TEXTURE_SIZE;
        if (tex_y < 0)
          tex_y += RAY_TEXTURE_SIZE;

        tex_x = (tex_x * texture->width) / RAY_TEXTURE_SIZE;
        tex_y = (tex_y * texture->height) / RAY_TEXTURE_SIZE;

        uint32_t pixel = ray_sample_texture(texture, tex_x, tex_y);
        pixel = ray_convert_pixel(pixel);
        if (g_engine.fogOn)
          pixel = ray_fog_pixel(pixel, euclidean_distance);

        for (int sx = 0;
             sx < strip_width && screen_x + sx < g_engine.displayWidth; sx++)
          gr_put_pixel(dest, screen_x + sx, screen_y, pixel);
      }
    }
  }
}

/* ============================================================================
   SPRITE RENDERING
   ============================================================================
 */

static int ray_sprite_sorter(const void *a, const void *b) {
  const RAY_Sprite *sa = (const RAY_Sprite *)a;
  const RAY_Sprite *sb = (const RAY_Sprite *)b;

  if (sa->distance > sb->distance)
    return -1;
  if (sa->distance < sb->distance)
    return 1;
  return 0;
}

void ray_draw_sprites(GRAPH *dest, float *z_buffer) {
  if (!dest || !z_buffer)
    return;

  /* Calculate sprite distances */
  for (int i = 0; i < g_engine.num_sprites; i++) {
    RAY_Sprite *sprite = &g_engine.sprites[i];
    if (sprite->hidden || sprite->cleanup)
      continue;

    float dx = sprite->x - g_engine.camera.x;
    float dy = sprite->y - g_engine.camera.y;
    sprite->distance = sqrtf(dx * dx + dy * dy);
  }

  /* Sort sprites by distance */
  qsort(g_engine.sprites, g_engine.num_sprites, sizeof(RAY_Sprite),
        ray_sprite_sorter);

  /* Render sprites */
  for (int i = 0; i < g_engine.num_sprites; i++) {
    RAY_Sprite *sprite = &g_engine.sprites[i];
    if (sprite->hidden || sprite->cleanup || sprite->distance == 0)
      continue;

    float dx = sprite->x - g_engine.camera.x;
    float dy = sprite->y - g_engine.camera.y;
    float sprite_angle = atan2f(-dy, dx);

    while (sprite_angle - g_engine.camera.rot > M_PI)
      sprite_angle -= RAY_TWO_PI;
    while (sprite_angle - g_engine.camera.rot < -M_PI)
      sprite_angle += RAY_TWO_PI;

    float angle_diff = sprite_angle - g_engine.camera.rot;

    // Skip FOV check for MD2 models as they manage their own
    // visibility/clipping
    if (!sprite->model && fabsf(angle_diff) > g_engine.fovRadians / 2.0f + 0.5f)
      continue;

    float sprite_screen_x = tanf(angle_diff) * g_engine.viewDist;
    int screen_x = g_engine.displayWidth / 2 - (int)sprite_screen_x;

    float sprite_screen_height =
        (g_engine.viewDist / sprite->distance) * sprite->h;
    float sprite_screen_width =
        (g_engine.viewDist / sprite->distance) * sprite->w;

    float sprite_z_offset = sprite->z - g_engine.camera.z;
    float sprite_screen_z =
        (g_engine.viewDist / sprite->distance) * sprite_z_offset;

    int screen_y = g_engine.displayHeight / 2 -
                   (int)(sprite_screen_height / 2) + (int)sprite_screen_z;

    GRAPH *sprite_texture = NULL;

    /* Check for MD2 Model */
    if (sprite->model) {
      ray_render_md2(dest, sprite);
      continue; // Skip billboard rendering
    }

    if (sprite->process_ptr) {
      sprite_texture = instance_graph(sprite->process_ptr);
    }
    if (!sprite_texture && sprite->textureID > 0) {
      sprite_texture = bitmap_get(g_engine.fpg_id, sprite->textureID);
    }
    if (!sprite_texture)
      continue;

    int start_x = screen_x - (int)(sprite_screen_width / 2);
    int end_x = screen_x + (int)(sprite_screen_width / 2);

    for (int sx = start_x; sx < end_x; sx++) {
      if (sx < 0 || sx >= g_engine.displayWidth)
        continue;

      int strip = sx / g_engine.stripWidth;
      if (strip >= 0 && strip < g_engine.rayCount) {
        if (z_buffer[strip] > 0 && sprite->distance > z_buffer[strip]) {
          continue;
        }
      }

      float tex_x_f =
          ((float)(sx - start_x) / sprite_screen_width) * sprite_texture->width;
      int tex_x = (int)tex_x_f;
      if (tex_x < 0 || tex_x >= sprite_texture->width)
        continue;

      for (int sy = screen_y; sy < screen_y + (int)sprite_screen_height; sy++) {
        if (sy < 0 || sy >= g_engine.displayHeight)
          continue;

        float tex_y_f = ((float)(sy - screen_y) / sprite_screen_height) *
                        sprite_texture->height;
        int tex_y = (int)tex_y_f;
        if (tex_y < 0 || tex_y >= sprite_texture->height)
          continue;

        uint32_t pixel = gr_get_pixel(sprite_texture, tex_x, tex_y);
        if (pixel == 0)
          continue;

        /* PIXEL CONVERSION */
        pixel = ray_convert_pixel(pixel);

        if (g_engine.fogOn) {
          pixel = ray_fog_pixel(pixel, sprite->distance);
        }

        gr_put_pixel(dest, sx, sy, pixel);
      }
    }
  }
}

/* ============================================================================
   MAIN RENDER FUNCTION
   ============================================================================
 */

/* Hit sorter for painter's algorithm */
static int ray_hit_sorter(const void *a, const void *b) {
  const RAY_RayHit *ha = (const RAY_RayHit *)a;
  const RAY_RayHit *hb = (const RAY_RayHit *)b;

  if (ha->distance < hb->distance)
    return -1;
  if (ha->distance > hb->distance)
    return 1;
  return 0;
}

void ray_render_frame(GRAPH *dest) {
  if (!dest || !g_engine.initialized)
    return;

  printf("FRAME START\n");
  fflush(stdout);

  /* DEBUG: Print camera and sector info */
  static int debug_counter = 0;
  if (debug_counter++ % 60 == 0) { // Print every 60 frames
                                   // ... (keep existing debug code) ...
  }

  /* Clear screen with sky color */
  uint32_t sky_color = 0x87CEEB;
  gr_clear_as(dest, sky_color);

  /* Render skybox if available */
  if (g_engine.skyTextureID > 0) {
    GRAPH *sky_texture = bitmap_get(g_engine.fpg_id, g_engine.skyTextureID);
    if (sky_texture) {
      /* Sky offset for pitch */
      int horizon_y = dest->height / 2 + (int)g_engine.camera.pitch;

      for (int x = 0; x < dest->width; x++) {
        float screen_angle =
            ((float)x / dest->width - 0.5f) * g_engine.fovRadians;
        float total_angle = g_engine.camera.rot + screen_angle;
        total_angle = fmodf(total_angle, 2.0f * M_PI);
        if (total_angle < 0)
          total_angle += 2.0f * M_PI;

        int tex_x = (int)((total_angle / (2.0f * M_PI)) * sky_texture->width);
        if (tex_x >= sky_texture->width)
          tex_x = sky_texture->width - 1;

        for (int y = 0; y < dest->height; y++) {
          /* Parallax vertical mapping - center texture on horizon */
          int dy = y - horizon_y;
          int tex_y = (sky_texture->height / 2) + dy;

          /* Clamp or wrap tex_y if needed, but usually sky textures are
           * designed for some vertical padding */
          if (tex_y < 0)
            tex_y = 0;
          if (tex_y >= sky_texture->height)
            tex_y = sky_texture->height - 1;

          uint32_t pixel = ray_sample_texture(sky_texture, tex_x, tex_y);
          gr_put_pixel(dest, x, y, pixel);
        }
      }
    }
  }

  /* Allocate buffers */
  // Using simple stack allocation for small arrays if possible, but rayCount is
  // dynamic optimization: reuse static buffers if possible, but for now stick
  // to malloc
  RAY_RayHit *all_rayhits = (RAY_RayHit *)malloc(
      g_engine.rayCount * RAY_MAX_RAYHITS * sizeof(RAY_RayHit));
  int *rayhit_counts = (int *)calloc(g_engine.rayCount, sizeof(int));
  float *z_buffer = (float *)malloc(g_engine.rayCount * sizeof(float));
  int *ceiling_clip = (int *)malloc(g_engine.rayCount * sizeof(int));
  int *floor_clip = (int *)malloc(g_engine.rayCount * sizeof(int));

  if (!all_rayhits || !rayhit_counts || !z_buffer || !ceiling_clip ||
      !floor_clip) {
    if (all_rayhits)
      free(all_rayhits);
    if (rayhit_counts)
      free(rayhit_counts);
    if (z_buffer)
      free(z_buffer);
    if (ceiling_clip)
      free(ceiling_clip);
    if (floor_clip)
      free(floor_clip);
    return;
  }

  /* Initialize z-buffer and clipping arrays */
  for (int i = 0; i < g_engine.rayCount; i++) {
    z_buffer[i] = FLT_MAX;
    ceiling_clip[i] =
        g_engine.displayHeight - 1; /* Can render down to bottom initially */
    floor_clip[i] = 0;              /* Can render up to top initially */
  }

  /* RAYCAST PHASE */
  // Parallelize this loop if possible in future
  for (int strip = 0; strip < g_engine.rayCount; strip++) {
    float strip_angle = g_engine.stripAngles[strip];
    float ray_angle = g_engine.camera.rot + strip_angle;
    int num_hits = 0;

    /* Cast ray against walls */
    ray_cast_ray(&g_engine, g_engine.camera.current_sector_id,
                 g_engine.camera.x, g_engine.camera.y, ray_angle, strip,
                 &all_rayhits[strip * RAY_MAX_RAYHITS], &num_hits);

    /* Cast ray against sprites */
    ray_cast_sprites(&g_engine, ray_angle, strip,
                     &all_rayhits[strip * RAY_MAX_RAYHITS], &num_hits);

    rayhit_counts[strip] = num_hits;

    /* Update z-buffer with closest wall */
    /* IMPORTANT: Skip walls from solid child sectors - we want parent
     * floor/ceiling to render behind them, but the walls themselves will still
     * block visually */
    for (int h = 0; h < num_hits; h++) {
      RAY_RayHit *hit = &all_rayhits[strip * RAY_MAX_RAYHITS + h];
      if (hit->wall && hit->distance < z_buffer[strip]) {
        /* Check if this wall belongs to a solid child sector */
        int is_solid_child = 0;
        for (int i = 0; i < g_engine.num_sectors; i++) {
          if (g_engine.sectors[i].sector_id == hit->sector_id &&
              ray_sector_get_parent(&g_engine.sectors[i]) >= 0 &&
              ray_sector_is_solid(&g_engine.sectors[i])) {
            is_solid_child = 1;
            break;
          }
        }

        /* Only update z-buffer for non-solid-child walls */
        if (!is_solid_child) {
          z_buffer[strip] = hit->distance;
        }
      }
    }
  }

  /* RENDER PHASE */
  for (int strip = 0; strip < g_engine.rayCount; strip++) {
    int screen_x = strip * g_engine.stripWidth;
    float ray_angle = g_engine.camera.rot + g_engine.stripAngles[strip];

    int num_hits = rayhit_counts[strip];
    RAY_RayHit *hits = &all_rayhits[strip * RAY_MAX_RAYHITS];

    /* Sort hits by distance (ascending) to ensure correct Painter's Algorithm
     */
    if (num_hits > 1) {
      qsort(hits, num_hits, sizeof(RAY_RayHit), ray_hit_sorter);
    }

    /* Find closest wall hit for floor/ceiling clipping */
    RAY_RayHit *closest_wall = NULL;
    float closest_dist = FLT_MAX;

    for (int h = 0; h < num_hits; h++) {
      if (hits[h].wall && hits[h].distance < closest_dist) {
        closest_wall = &hits[h];
        closest_dist = hits[h].distance;
      }
    }

    /* Get camera sector */
    int camera_sector_id = g_engine.camera.current_sector_id;
    if (camera_sector_id < 0 || camera_sector_id >= g_engine.num_sectors) {
      RAY_Sector *cam_sec = ray_find_sector_at_point(
          &g_engine, g_engine.camera.x, g_engine.camera.y);
      if (cam_sec)
        camera_sector_id = cam_sec->sector_id;
      else
        camera_sector_id = 0;
    }

    /* DEBUG: Print hits for center strip */
    if (strip == 400) {
      static int frame_debug = 0;
      if (frame_debug < 50) {
        // printf("Frame %d Strip 400 Hits: %d\n", frame_debug, num_hits);
        for (int h = 0; h < num_hits; h++) {
          if (hits[h].sector_id == 11) {
            printf("RENDER HIT: Sector 11 found at dist %.1f\n",
                   hits[h].distance);
          }
        }
        frame_debug++;
      }
    }

    /* ===================================================================
       NESTED SECTOR DETECTION: Mark child sectors for hierarchical rendering
       =================================================================== */

    /* Mark hits that belong to ANY nested child sector */
    for (int h = 0; h < num_hits; h++) {
      hits[h].is_child_sector = 0;

      if (hits[h].wall) {
        /* Validate sector index */
        /* Warning: sector_id is not index! Need lookup? */
        /* Usually sector_id == index if sequentially created, but safest is
         * lookup or direct check if we have pointer. */
        /* hits[h].wall->sector? No. */
        /* But hits[h].sector_id is available. */

        RAY_Sector *sec = NULL;
        // Optimization: try direct index if it matches
        if (hits[h].sector_id >= 0 &&
            hits[h].sector_id < g_engine.num_sectors &&
            g_engine.sectors[hits[h].sector_id].sector_id ==
                hits[h].sector_id) {
          sec = &g_engine.sectors[hits[h].sector_id];
        } else {
          // Fallback search
          for (int i = 0; i < g_engine.num_sectors; i++) {
            if (g_engine.sectors[i].sector_id == hits[h].sector_id) {
              sec = &g_engine.sectors[i];
              break;
            }
          }
        }

        if (sec && ray_sector_get_parent(sec) >= 0) {
          hits[h].is_child_sector = 1;
        }
      }
    }

    /* ===================================================================
       PHASE 1: RENDER FLOOR/CEILING with hierarchical support
       =================================================================== */

    /* Strategy:
     * 1. Render parent sector (camera sector) and portals (Background)
     * 2. Render floor/ceiling for child sectors (Foreground)
     * 3. Finally render walls (Phase 2) to cover edges correctly
     */

    /* STEP 1: Render parent sector (camera sector) and portals */
    float current_dist = 0.0f;
    int current_sector_id = camera_sector_id;

    /* Iterate through sorted hits (Near to Far) */
    for (int h = 0; h < num_hits; h++) {
      RAY_RayHit *hit = &hits[h];
      if (!hit->wall)
        continue;

      /* Skip HOLLOW child sectors - already rendered with their own
       * floor/ceiling */
      /* But DON'T skip SOLID child sectors - we want parent to render around
       * them */
      if (hit->is_child_sector) {
        /* Check if this child is solid or hollow */
        RAY_Sector *child_sector = NULL;
        for (int i = 0; i < g_engine.num_sectors; i++) {
          if (g_engine.sectors[i].sector_id == hit->sector_id) {
            child_sector = &g_engine.sectors[i];
            break;
          }
        }

        /* Only skip hollow children (they have their own floor/ceiling) */
        if (child_sector && !ray_sector_is_solid(child_sector)) {
          /* Update current_dist to skip over hollow child sector */
          if (hit->distance > current_dist) {
            current_dist = hit->distance;
          }
          continue;
        }
        /* For solid children, continue processing normally -
         * parent floor/ceiling will render, z-buffer will block at walls */
      }

      float hit_dist = hit->distance;

      /* Render floor/ceiling segment for current sector */
      if (hit_dist > current_dist + 0.1f) {
        ray_draw_floor_ceiling(dest, screen_x, ray_angle, current_sector_id,
                               current_dist, hit_dist, z_buffer, ceiling_clip,
                               floor_clip);
      }

      /* Update generic current_dist */
      current_dist = hit_dist;

      /* If it's a portal, switch sector context */
      if (hit->wall->portal_id >= 0) {
        /* Find next sector */
        for (int p = 0; p < g_engine.num_portals; p++) {
          if (g_engine.portals[p].portal_id == hit->wall->portal_id) {
            int sA = g_engine.portals[p].sector_a;
            int sB = g_engine.portals[p].sector_b;

            int wall_sector = hit->sector_id;

            if (wall_sector == sA)
              current_sector_id = sB;
            else if (wall_sector == sB)
              current_sector_id = sA;

            break;
          }
        }
      } else {
        /* Solid wall - continue rendering floor/ceiling behind it */
        /* The wall itself was drawn in Phase 1 and set the Z-buffer */
        /* The floor/ceiling behind it will be clipped by Z-buffer where the
         * wall exists */
        /* But will show up in gaps (like above a floating box) */
      }
    }

    /* Fallback: Render infinite floor/ceiling if no solid wall hit */
    if (current_dist < FLT_MAX) {
      float far_dist = g_engine.viewDist * 4.0f;
      ray_draw_floor_ceiling(dest, screen_x, ray_angle, current_sector_id,
                             current_dist, far_dist, z_buffer, ceiling_clip,
                             floor_clip);
    }

    /* STEP 2: Render child sectors (Foreground) */
    /* We need to render the floor/ceiling (or top/bottom) of nested sectors. */
    /* For a solid box, this means drawing its "Lid" and "Base". */
    /* We start drawing from the wall intersection distance. */

    for (int h = 0; h < num_hits; h++) {
      if (hits[h].is_child_sector) {
        /* We found a child sector wall intersection */
        /* Render the child sector's flat surfaces (Floor/Ceiling/Lid/Base) */
        /* The 'ray_draw_floor_ceiling' function has been updated to handle
         * Solid Child logic */
        ray_draw_floor_ceiling(dest, screen_x, ray_angle, hits[h].sector_id,
                               hits[h].distance, /* Start drawing at the wall */
                               FLT_MAX,          /* Draw until occluded */
                               z_buffer, ceiling_clip, floor_clip);
      }
    }

    /* ===================================================================
       PHASE 2: RENDER WALLS with hierarchical ordering
       =================================================================== */

    static int wall_render_debug = 0;

    /* STEP 1: Render parent sector and portal walls (back-to-front) */
    for (int h = num_hits - 1; h >= 0; h--) {
      if (hits[h].wall && !hits[h].is_child_sector) {
        if (wall_render_debug < 10) {
          printf("RENDERING PARENT WALL: h=%d, sector_id=%d, distance=%.1f\n",
                 h, hits[h].sector_id, hits[h].distance);
          wall_render_debug++;
        }
        ray_draw_wall_strip(dest, &hits[h], screen_x, ceiling_clip, floor_clip);
      }
    }

    /* STEP 2: Render child sector walls (back-to-front) */
    for (int h = num_hits - 1; h >= 0; h--) {
      if (hits[h].wall && hits[h].is_child_sector) {
        if (wall_render_debug < 10) {
          printf("RENDERING CHILD WALL: h=%d, sector_id=%d, distance=%.1f\n", h,
                 hits[h].sector_id, hits[h].distance);
          wall_render_debug++;
        }
        ray_draw_wall_strip(dest, &hits[h], screen_x, ceiling_clip, floor_clip);
      }
    }
  }

  /* Render sprites */
  ray_draw_sprites(dest, z_buffer);

  /* Cleanup */
  free(all_rayhits);
  free(rayhit_counts);
  free(z_buffer);
  free(ceiling_clip);
  free(floor_clip);
}
