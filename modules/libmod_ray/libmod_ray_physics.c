/* ============================================================================
   libmod_ray_physics.c - Rigid Body Physics Engine for libmod_ray
   ============================================================================
   Full physics simulation with gravity, collision detection & response,
   friction, restitution, angular velocity (tipping/rolling/spinning),
   and sector-aware floor/ceiling/wall constraints.
   ============================================================================
 */

#include "bgddl.h"
#include "libmod_ray.h"
#include "libmod_ray_compat.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Physics constants */
#define GRAVITY -980.0f /* cm/s² (9.8 m/s² = 980 cm/s²) */
#define PHYSICS_EPSILON 0.001f
#define SLEEP_VELOCITY 0.5f /* Below this, body is considered at rest */
#define MAX_CONTACTS 256
#define COLLISION_SLOP 0.01f  /* Allowed penetration before correction */
#define BAUMGARTE_FACTOR 0.2f /* Positional correction factor */

extern RAY_Engine g_engine;

/* External collision function from libmod_ray_raycasting.c */
extern int ray_check_collision(RAY_Engine *engine, float x, float y, float z,
                               float new_x, float new_y, float step_h);

/* ============================================================================
   PHYSICS BODY LIFECYCLE
   ============================================================================
 */

void ray_physics_init(void) {
  printf("RAY_PHYSICS: Physics engine initialized (gravity=%.1f cm/s²)\n",
         -GRAVITY);
}

RAY_PhysicsBody *ray_physics_create_body(float mass, float radius,
                                         float height) {
  RAY_PhysicsBody *body = (RAY_PhysicsBody *)calloc(1, sizeof(RAY_PhysicsBody));
  if (!body)
    return NULL;

  body->mass = mass;
  body->inv_mass = (mass > PHYSICS_EPSILON) ? 1.0f / mass : 0.0f;
  body->col_radius = radius;
  body->col_height = height;

  /* Sensible defaults */
  body->friction = 0.5f;
  body->restitution = 0.3f;
  body->gravity_scale = 1.0f;
  body->linear_damping = 0.05f;
  body->angular_damping = 0.1f;
  body->collision_layer = 1;         /* Default layer */
  body->collision_mask = 0xFFFFFFFF; /* Collide with everything */

  return body;
}

void ray_physics_destroy_body(RAY_PhysicsBody *body) {
  if (body)
    free(body);
}

/* ============================================================================
   FORCE & IMPULSE APPLICATION
   ============================================================================
 */

void ray_physics_apply_force(RAY_PhysicsBody *body, float fx, float fy,
                             float fz) {
  if (!body || body->is_static || body->is_kinematic)
    return;
  body->ax += fx * body->inv_mass;
  body->ay += fy * body->inv_mass;
  body->az += fz * body->inv_mass;
}

void ray_physics_apply_impulse(RAY_PhysicsBody *body, float ix, float iy,
                               float iz) {
  if (!body || body->is_static || body->is_kinematic)
    return;
  body->vx += ix * body->inv_mass;
  body->vy += iy * body->inv_mass;
  body->vz += iz * body->inv_mass;
}

void ray_physics_set_velocity(RAY_PhysicsBody *body, float vx, float vy,
                              float vz) {
  if (!body)
    return;
  body->vx = vx;
  body->vy = vy;
  body->vz = vz;
}

/* ============================================================================
   SECTOR GEOMETRY QUERIES
   ============================================================================
 */

/* Find which sector a point (x, y) belongs to.
   For physics: skip solid sectors (buildings) so cars stay on the street. */
static int find_sector_at(float px, float py) {
  int best = -1;
  float min_area = 1e18f;
  for (int i = 0; i < g_engine.num_sectors; i++) {
    RAY_Sector *s = &g_engine.sectors[i];
    if (px < s->min_x || px > s->max_x || py < s->min_y || py > s->max_y)
      continue;
    /* Skip solid sectors (buildings) — physics objects live on walkable ground
     */
    if (ray_sector_is_solid(s))
      continue;
    if (ray_point_in_sector_local(s, px, py)) {
      float area = (s->max_x - s->min_x) * (s->max_y - s->min_y);
      if (area < min_area) {
        min_area = area;
        best = s->sector_id;
      }
    }
  }
  return best;
}

/* Check if point is inside any solid (building) sector */
static int is_inside_solid_sector(float px, float py) {
  for (int i = 0; i < g_engine.num_sectors; i++) {
    RAY_Sector *s = &g_engine.sectors[i];
    if (!ray_sector_is_solid(s))
      continue;
    if (px < s->min_x || px > s->max_x || py < s->min_y || py > s->max_y)
      continue;
    if (ray_point_in_sector_local(s, px, py))
      return 1;
  }
  return 0;
}

/* Get floor height at position (x, y) in a given sector */
static float get_floor_at(float px, float py) {
  int sid = find_sector_at(px, py);
  if (sid < 0)
    return 0.0f;
  for (int i = 0; i < g_engine.num_sectors; i++) {
    if (g_engine.sectors[i].sector_id == sid)
      return g_engine.sectors[i].floor_z;
  }
  return 0.0f;
}

/* Get ceiling height at position (x, y) in a given sector */
static float get_ceiling_at(float px, float py) {
  int sid = find_sector_at(px, py);
  if (sid < 0)
    return 9999.0f;
  for (int i = 0; i < g_engine.num_sectors; i++) {
    if (g_engine.sectors[i].sector_id == sid)
      return g_engine.sectors[i].ceiling_z;
  }
  return 9999.0f;
}

/* Check wall collision: does moving from (ox,oy) to (nx,ny) cross a wall?
   Returns 1 if blocked, sets (out_x, out_y) to slide position.
   Also reflects velocity (vx, vy) off the wall normal for bounce. */
static int check_wall_collision(float ox, float oy, float *nx, float *ny,
                                float radius, int sector_id, float *vx,
                                float *vy, float restitution) {
  int blocked = 0;
  for (int i = 0; i < g_engine.num_sectors; i++) {
    RAY_Sector *s = &g_engine.sectors[i];
    for (int w = 0; w < s->num_walls; w++) {
      RAY_Wall *wall = &s->walls[w];

      /* Skip portal walls — those are openings */
      if (wall->portal_id >= 0)
        continue;

      /* Wall segment: (x1,y1) to (x2,y2) */
      float wx = wall->x2 - wall->x1;
      float wy = wall->y2 - wall->y1;
      float wlen = sqrtf(wx * wx + wy * wy);
      if (wlen < PHYSICS_EPSILON)
        continue;

      /* Wall normal (pointing inward) */
      float wnx = -wy / wlen;
      float wny = wx / wlen;

      /* Distance from new position to wall line */
      float dx = *nx - wall->x1;
      float dy = *ny - wall->y1;
      float dist = dx * wnx + dy * wny;

      if (dist < radius && dist > -radius * 0.5f) {
        /* Check if we're within the wall segment bounds */
        float along = (dx * wx + dy * wy) / (wlen * wlen);
        if (along >= -0.1f && along <= 1.1f) {
          /* Push out */
          float push = radius - dist + COLLISION_SLOP;
          *nx += wnx * push;
          *ny += wny * push;
          blocked = 1;

          /* Reflect velocity off wall normal */
          if (vx && vy) {
            float vel_dot_n = (*vx) * wnx + (*vy) * wny;
            if (vel_dot_n < 0) { /* Only reflect if moving toward wall */
              *vx -= (1.0f + restitution) * vel_dot_n * wnx;
              *vy -= (1.0f + restitution) * vel_dot_n * wny;
              /* Apply friction along wall */
              *vx *= 0.9f;
              *vy *= 0.9f;
            }
          }
        }
      }
    }
  }
  return blocked;
}

/* ============================================================================
   BODY-BODY COLLISION DETECTION & RESPONSE
   ============================================================================
 */

typedef struct {
  RAY_Sprite *a, *b;
  float nx, ny, nz; /* Contact normal (A to B) */
  float depth;      /* Penetration depth */
} PhysicsContact;

static PhysicsContact s_contacts[MAX_CONTACTS];
static int s_num_contacts = 0;

/* Detect cylinder-cylinder collision between two sprites */
static void detect_body_collision(RAY_Sprite *a, RAY_Sprite *b) {
  if (!a->physics || !b->physics)
    return;
  RAY_PhysicsBody *pa = a->physics, *pb = b->physics;

  /* Both static/kinematic → skip */
  if ((pa->is_static || pa->is_kinematic) &&
      (pb->is_static || pb->is_kinematic))
    return;

  /* Layer check */
  if (!(pa->collision_layer & pb->collision_mask) &&
      !(pb->collision_layer & pa->collision_mask))
    return;

  /* 2D circle-circle test (XY plane) */
  float dx = b->x - a->x;
  float dy = b->y - a->y;
  float dist_sq = dx * dx + dy * dy;
  float min_dist = pa->col_radius + pb->col_radius;

  if (dist_sq >= min_dist * min_dist)
    return;

  /* Vertical overlap check */
  float a_bot = a->z, a_top = a->z + pa->col_height;
  float b_bot = b->z, b_top = b->z + pb->col_height;
  if (a_top <= b_bot || b_top <= a_bot)
    return;

  /* Contact! */
  if (s_num_contacts >= MAX_CONTACTS)
    return;

  float dist = sqrtf(dist_sq);
  PhysicsContact *c = &s_contacts[s_num_contacts++];
  c->a = a;
  c->b = b;
  c->depth = min_dist - dist;

  if (dist > PHYSICS_EPSILON) {
    c->nx = dx / dist;
    c->ny = dy / dist;
  } else {
    c->nx = 1.0f;
    c->ny = 0.0f;
  }
  c->nz = 0.0f;

  /* Check if vertical collision is dominant */
  float vert_overlap_a = a_top - b_bot;
  float vert_overlap_b = b_top - a_bot;
  float vert_overlap = fminf(vert_overlap_a, vert_overlap_b);
  if (vert_overlap < c->depth && vert_overlap > 0) {
    c->nx = 0;
    c->ny = 0;
    c->nz = (a->z < b->z) ? -1.0f : 1.0f;
    c->depth = vert_overlap;
  }
}

/* Resolve a contact with impulse-based collision response */
static void resolve_contact(PhysicsContact *c) {
  RAY_PhysicsBody *pa = c->a->physics;
  RAY_PhysicsBody *pb = c->b->physics;

  /* Triggers don't resolve physically */
  if (pa->is_trigger || pb->is_trigger)
    return;

  float inv_mass_sum = pa->inv_mass + pb->inv_mass;
  if (inv_mass_sum < PHYSICS_EPSILON)
    return;

  /* Relative velocity along contact normal */
  float rel_vx = pb->vx - pa->vx;
  float rel_vy = pb->vy - pa->vy;
  float rel_vz = pb->vz - pa->vz;
  float rel_vn = rel_vx * c->nx + rel_vy * c->ny + rel_vz * c->nz;

  /* Don't resolve if separating */
  if (rel_vn > 0)
    return;

  /* Restitution (bounciness) */
  float e = fminf(pa->restitution, pb->restitution);

  /* Normal impulse magnitude */
  float j = -(1.0f + e) * rel_vn / inv_mass_sum;

  /* Apply normal impulse */
  float jnx = j * c->nx;
  float jny = j * c->ny;
  float jnz = j * c->nz;

  if (!pa->is_static && !pa->is_kinematic) {
    pa->vx -= jnx * pa->inv_mass;
    pa->vy -= jny * pa->inv_mass;
    pa->vz -= jnz * pa->inv_mass;
  }
  if (!pb->is_static && !pb->is_kinematic) {
    pb->vx += jnx * pb->inv_mass;
    pb->vy += jny * pb->inv_mass;
    pb->vz += jnz * pb->inv_mass;
  }

  /* Friction impulse (tangential) */
  float avg_friction = (pa->friction + pb->friction) * 0.5f;
  float tan_vx = rel_vx - rel_vn * c->nx;
  float tan_vy = rel_vy - rel_vn * c->ny;
  float tan_vz = rel_vz - rel_vn * c->nz;
  float tan_speed = sqrtf(tan_vx * tan_vx + tan_vy * tan_vy + tan_vz * tan_vz);

  if (tan_speed > PHYSICS_EPSILON) {
    float jtx = -tan_vx / tan_speed;
    float jty = -tan_vy / tan_speed;
    float jtz = -tan_vz / tan_speed;
    float jt = fminf(fabsf(j) * avg_friction, tan_speed / inv_mass_sum);

    if (!pa->is_static && !pa->is_kinematic) {
      pa->vx -= jt * jtx * pa->inv_mass;
      pa->vy -= jt * jty * pa->inv_mass;
      pa->vz -= jt * jtz * pa->inv_mass;
    }
    if (!pb->is_static && !pb->is_kinematic) {
      pb->vx += jt * jtx * pb->inv_mass;
      pb->vy += jt * jty * pb->inv_mass;
      pb->vz += jt * jtz * pb->inv_mass;
    }
  }

  /* Angular velocity from off-center collision */
  if (!pa->is_static && !pa->is_kinematic && !pa->lock_rot_z) {
    float torque = (c->nx * (c->a->y - c->a->y) - c->ny * (c->a->x - c->a->x)) *
                   j * pa->inv_mass * 0.1f;
    pa->ang_vz += torque;
  }
  if (!pb->is_static && !pb->is_kinematic && !pb->lock_rot_z) {
    float torque = (c->nx * (c->b->y - c->b->y) - c->ny * (c->b->x - c->b->x)) *
                   j * pb->inv_mass * 0.1f;
    pb->ang_vz -= torque;
  }

  /* Positional correction (Baumgarte stabilization) */
  if (c->depth > COLLISION_SLOP) {
    float correction =
        (c->depth - COLLISION_SLOP) * BAUMGARTE_FACTOR / inv_mass_sum;
    if (!pa->is_static && !pa->is_kinematic) {
      c->a->x -= correction * pa->inv_mass * c->nx;
      c->a->y -= correction * pa->inv_mass * c->ny;
      c->a->z -= correction * pa->inv_mass * c->nz;
    }
    if (!pb->is_static && !pb->is_kinematic) {
      c->b->x += correction * pb->inv_mass * c->nx;
      c->b->y += correction * pb->inv_mass * c->ny;
      c->b->z += correction * pb->inv_mass * c->nz;
    }
  }
}

/* ============================================================================
   MAIN PHYSICS STEP
   ============================================================================
 */

void ray_physics_step(float dt) {
  if (dt <= 0 || dt > 0.1f)
    dt = 0.016f; /* Clamp to ~60fps */

  /* --- 1. INTEGRATION: Apply gravity + velocity → position --- */
  for (int i = 0; i < g_engine.num_sprites; i++) {
    RAY_Sprite *s = &g_engine.sprites[i];
    RAY_PhysicsBody *p = s->physics;
    if (!p || p->is_static || p->is_kinematic)
      continue;

    /* Gravity */
    p->vz += GRAVITY * p->gravity_scale * dt;

    /* Accumulated forces */
    p->vx += p->ax * dt;
    p->vy += p->ay * dt;
    p->vz += p->az * dt;

    /* Reset forces for next frame */
    p->ax = p->ay = p->az = 0;

    /* Damping (air resistance) */
    float lin_damp = powf(1.0f - p->linear_damping, dt);
    p->vx *= lin_damp;
    p->vy *= lin_damp;
    /* Don't damp vertical velocity as much (gravity already handles it) */
    if (p->on_ground)
      p->vz *= lin_damp;

    float ang_damp = powf(1.0f - p->angular_damping, dt);
    if (!p->lock_rot_x)
      p->ang_vx *= ang_damp;
    else
      p->ang_vx = 0;
    if (!p->lock_rot_y)
      p->ang_vy *= ang_damp;
    else
      p->ang_vy = 0;
    if (!p->lock_rot_z)
      p->ang_vz *= ang_damp;
    else
      p->ang_vz = 0;

    /* Update position */
    float new_x = s->x + p->vx * dt;
    float new_y = s->y + p->vy * dt;
    float new_z = s->z + p->vz * dt;

    /* --- 2. MAX VELOCITY CAPPING --- */
    float max_speed = 800.0f;
    float speed_sq = p->vx * p->vx + p->vy * p->vy;
    if (speed_sq > max_speed * max_speed) {
      float speed = sqrtf(speed_sq);
      p->vx = p->vx / speed * max_speed;
      p->vy = p->vy / speed * max_speed;
    }

    /* --- 3. SECTOR WALL COLLISION (uses engine's proven collision system) ---
     */
    /* Per-axis collision for wall sliding */
    float check_z =
        s->z + 5.0f; /* Check slightly above ground to detect walls */

    /* X-axis movement */
    if (ray_check_collision(&g_engine, s->x, s->y, check_z, new_x, s->y,
                            5.0f)) {
      p->vx = -p->vx * p->restitution; /* Bounce */
      new_x = s->x;                    /* Block X movement */
    }
    /* Y-axis movement */
    if (ray_check_collision(&g_engine, s->x, s->y, check_z, new_x, new_y,
                            5.0f)) {
      p->vy = -p->vy * p->restitution; /* Bounce */
      new_y = s->y;                    /* Block Y movement */
    }

    /* --- 3. FLOOR / CEILING COLLISION --- */
    float floor_z = get_floor_at(new_x, new_y);
    float ceil_z = get_ceiling_at(new_x, new_y);

    p->on_ground = 0;

    /* Floor collision */
    if (new_z <= floor_z) {
      new_z = floor_z;
      if (p->vz < 0) {
        /* Bounce */
        if (fabsf(p->vz) > 10.0f) {
          p->vz = -p->vz * p->restitution;
          /* Angular velocity from impact */
          if (!p->lock_rot_x)
            p->ang_vx += p->vx * 0.005f;
          if (!p->lock_rot_y)
            p->ang_vy += p->vy * 0.005f;
        } else {
          p->vz = 0;
          p->on_ground = 1;
        }
        /* Ground friction */
        p->vx *= (1.0f - p->friction * dt * 5.0f);
        p->vy *= (1.0f - p->friction * dt * 5.0f);
      }
    }

    /* Ceiling collision */
    if (new_z + p->col_height >= ceil_z) {
      new_z = ceil_z - p->col_height;
      if (p->vz > 0) {
        p->vz = -p->vz * p->restitution * 0.5f;
      }
    }

    /* Update angular rotation */
    p->rot_x += p->ang_vx * dt;
    p->rot_y += p->ang_vy * dt;
    s->rot += p->ang_vz * dt;

    /* Clamp tilt angles (prevent full flip) */
    if (p->rot_x > 1.2f)
      p->rot_x = 1.2f;
    if (p->rot_x < -1.2f)
      p->rot_x = -1.2f;
    if (p->rot_y > 1.2f)
      p->rot_y = 1.2f;
    if (p->rot_y < -1.2f)
      p->rot_y = -1.2f;

    /* Self-righting torque when on ground (objects try to stand up) */
    if (p->on_ground) {
      p->ang_vx -= p->rot_x * 2.0f * dt;
      p->ang_vy -= p->rot_y * 2.0f * dt;
      p->rot_x *= (1.0f - dt * 3.0f);
      p->rot_y *= (1.0f - dt * 3.0f);
    }

    /* Commit position */
    s->x = new_x;
    s->y = new_y;
    s->z = new_z;
  }

  /* --- 4. BROAD PHASE + NARROW PHASE COLLISION DETECTION --- */
  s_num_contacts = 0;
  for (int i = 0; i < g_engine.num_sprites; i++) {
    if (!g_engine.sprites[i].physics)
      continue;
    for (int j = i + 1; j < g_engine.num_sprites; j++) {
      if (!g_engine.sprites[j].physics)
        continue;
      detect_body_collision(&g_engine.sprites[i], &g_engine.sprites[j]);
    }
  }

  /* --- 5. COLLISION RESPONSE --- */
  for (int i = 0; i < s_num_contacts; i++) {
    resolve_contact(&s_contacts[i]);
  }
}

/* ============================================================================
   BENNUGD2 BINDINGS
   ============================================================================
 */

/* ray_physics_enable(sprite_index, mass, radius, height)
   Enables physics on a sprite. Creates a RAY_PhysicsBody. */
int64_t libmod_ray_physics_enable(INSTANCE *my, int64_t *params) {
  int idx = (int)params[0];
  if (idx < 0 || idx >= g_engine.num_sprites)
    return -1;

  RAY_Sprite *s = &g_engine.sprites[idx];
  float mass = *(float *)&params[1];
  float radius = *(float *)&params[2];
  float height = *(float *)&params[3];

  if (s->physics)
    ray_physics_destroy_body(s->physics);

  s->physics = ray_physics_create_body(mass, radius, height);
  return s->physics ? 0 : -1;
}

/* ray_physics_set_mass(sprite_index, mass) */
int64_t libmod_ray_physics_set_mass(INSTANCE *my, int64_t *params) {
  int idx = (int)params[0];
  if (idx < 0 || idx >= g_engine.num_sprites)
    return -1;
  RAY_PhysicsBody *p = g_engine.sprites[idx].physics;
  if (!p)
    return -1;
  p->mass = *(float *)&params[1];
  p->inv_mass = (p->mass > PHYSICS_EPSILON) ? 1.0f / p->mass : 0.0f;
  return 0;
}

/* ray_physics_set_friction(sprite_index, friction) */
int64_t libmod_ray_physics_set_friction(INSTANCE *my, int64_t *params) {
  int idx = (int)params[0];
  if (idx < 0 || idx >= g_engine.num_sprites)
    return -1;
  RAY_PhysicsBody *p = g_engine.sprites[idx].physics;
  if (!p)
    return -1;
  p->friction = *(float *)&params[1];
  return 0;
}

/* ray_physics_set_restitution(sprite_index, restitution) */
int64_t libmod_ray_physics_set_restitution(INSTANCE *my, int64_t *params) {
  int idx = (int)params[0];
  if (idx < 0 || idx >= g_engine.num_sprites)
    return -1;
  RAY_PhysicsBody *p = g_engine.sprites[idx].physics;
  if (!p)
    return -1;
  p->restitution = *(float *)&params[1];
  return 0;
}

/* ray_physics_set_gravity_scale(sprite_index, scale) */
int64_t libmod_ray_physics_set_gravity_scale(INSTANCE *my, int64_t *params) {
  int idx = (int)params[0];
  if (idx < 0 || idx >= g_engine.num_sprites)
    return -1;
  RAY_PhysicsBody *p = g_engine.sprites[idx].physics;
  if (!p)
    return -1;
  p->gravity_scale = *(float *)&params[1];
  return 0;
}

/* ray_physics_set_damping(sprite_index, linear, angular) */
int64_t libmod_ray_physics_set_damping(INSTANCE *my, int64_t *params) {
  int idx = (int)params[0];
  if (idx < 0 || idx >= g_engine.num_sprites)
    return -1;
  RAY_PhysicsBody *p = g_engine.sprites[idx].physics;
  if (!p)
    return -1;
  p->linear_damping = *(float *)&params[1];
  p->angular_damping = *(float *)&params[2];
  return 0;
}

/* ray_physics_set_static(sprite_index, is_static) */
int64_t libmod_ray_physics_set_static(INSTANCE *my, int64_t *params) {
  int idx = (int)params[0];
  if (idx < 0 || idx >= g_engine.num_sprites)
    return -1;
  RAY_PhysicsBody *p = g_engine.sprites[idx].physics;
  if (!p)
    return -1;
  p->is_static = (int)params[1];
  if (p->is_static)
    p->inv_mass = 0;
  return 0;
}

/* ray_physics_set_kinematic(sprite_index, is_kinematic) */
int64_t libmod_ray_physics_set_kinematic(INSTANCE *my, int64_t *params) {
  int idx = (int)params[0];
  if (idx < 0 || idx >= g_engine.num_sprites)
    return -1;
  RAY_PhysicsBody *p = g_engine.sprites[idx].physics;
  if (!p)
    return -1;
  p->is_kinematic = (int)params[1];
  return 0;
}

/* ray_physics_set_trigger(sprite_index, is_trigger) */
int64_t libmod_ray_physics_set_trigger(INSTANCE *my, int64_t *params) {
  int idx = (int)params[0];
  if (idx < 0 || idx >= g_engine.num_sprites)
    return -1;
  RAY_PhysicsBody *p = g_engine.sprites[idx].physics;
  if (!p)
    return -1;
  p->is_trigger = (int)params[1];
  return 0;
}

/* ray_physics_set_lock_rotation(sprite_index, lock_x, lock_y, lock_z) */
int64_t libmod_ray_physics_set_lock_rotation(INSTANCE *my, int64_t *params) {
  int idx = (int)params[0];
  if (idx < 0 || idx >= g_engine.num_sprites)
    return -1;
  RAY_PhysicsBody *p = g_engine.sprites[idx].physics;
  if (!p)
    return -1;
  p->lock_rot_x = (int)params[1];
  p->lock_rot_y = (int)params[2];
  p->lock_rot_z = (int)params[3];
  return 0;
}

/* ray_physics_set_collision_layer(sprite_index, layer, mask) */
int64_t libmod_ray_physics_set_collision_layer(INSTANCE *my, int64_t *params) {
  int idx = (int)params[0];
  if (idx < 0 || idx >= g_engine.num_sprites)
    return -1;
  RAY_PhysicsBody *p = g_engine.sprites[idx].physics;
  if (!p)
    return -1;
  p->collision_layer = (int)params[1];
  p->collision_mask = (int)params[2];
  return 0;
}

/* ray_physics_apply_force(sprite_index, fx, fy, fz) */
int64_t libmod_ray_physics_apply_force_bgd(INSTANCE *my, int64_t *params) {
  int idx = (int)params[0];
  if (idx < 0 || idx >= g_engine.num_sprites)
    return -1;
  RAY_PhysicsBody *p = g_engine.sprites[idx].physics;
  if (!p)
    return -1;
  float fx = *(float *)&params[1];
  float fy = *(float *)&params[2];
  float fz = *(float *)&params[3];
  ray_physics_apply_force(p, fx, fy, fz);
  return 0;
}

/* ray_physics_apply_impulse(sprite_index, ix, iy, iz) */
int64_t libmod_ray_physics_apply_impulse_bgd(INSTANCE *my, int64_t *params) {
  int idx = (int)params[0];
  if (idx < 0 || idx >= g_engine.num_sprites)
    return -1;
  RAY_PhysicsBody *p = g_engine.sprites[idx].physics;
  if (!p)
    return -1;
  float ix = *(float *)&params[1];
  float iy = *(float *)&params[2];
  float iz = *(float *)&params[3];
  ray_physics_apply_impulse(p, ix, iy, iz);
  return 0;
}

/* ray_physics_get_velocity(sprite_index) → returns packed vx|vy|vz */
int64_t libmod_ray_physics_get_velocity(INSTANCE *my, int64_t *params) {
  int idx = (int)params[0];
  int component = (int)params[1]; /* 0=vx, 1=vy, 2=vz */
  if (idx < 0 || idx >= g_engine.num_sprites)
    return 0;
  RAY_PhysicsBody *p = g_engine.sprites[idx].physics;
  if (!p)
    return 0;
  float v = (component == 0) ? p->vx : (component == 1) ? p->vy : p->vz;
  int64_t result;
  *(float *)&result = v;
  return result;
}

/* ray_physics_step(dt_ms) — dt in milliseconds */
int64_t libmod_ray_physics_step_bgd(INSTANCE *my, int64_t *params) {
  float dt = *(float *)&params[0];
  ray_physics_step(dt / 1000.0f); /* Convert ms to seconds */
  return 0;
}
