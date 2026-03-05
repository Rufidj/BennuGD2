#ifndef __LIBMOD_RAY_H
#define __LIBMOD_RAY_H

#include <math.h>
#include <stdint.h>

/* Inclusiones necesarias de BennuGD2 */
#include "bgddl.h"
#include "g_base.h"
#include "g_bitmap.h"
#include "g_blit.h"
#include "g_clear.h"
#include "g_grlib.h"
#include "g_pixel.h"
#include "libbggfx.h"
#include "xstrings.h"

/* ============================================================================
   CONSTANTES DEL MOTOR
   ============================================================================
 */

#define RAY_WORLD_UNIT 128      /* Unidad base del mundo */
#define RAY_TEXTURE_SIZE 128    /* Tamaño de texturas */
#define RAY_MAX_SPRITES 2000    /* Máximo de sprites */
#define RAY_MAX_SPAWN_FLAGS 500 /* Máximo de spawn flags */
#define RAY_MAX_SECTORS 5000    /* Máximo de sectores */
#define RAY_MAX_PORTALS 10000   /* Máximo de portales */
#define RAY_MAX_LIGHTS 64       /* Máximo de luces puntuales */
#define RAY_MAX_VERTICES_PER_SECTOR                                            \
  256 /* Máximo vértices por sector (aumentado para mapas complejos) */
#define RAY_MAX_WALLS_PER_SECTOR                                               \
  256 /* Máximo paredes por sector (aumentado para soportar hijos) */
#define RAY_MAX_RAYHITS                                                        \
  1024 /* Máximo hits de raycasting (aumentado para profundidad) */
#define RAY_TWO_PI (M_PI * 2.0f)

/* Epsilon para comparaciones de coordenadas */
#define RAY_EPSILON 0.1f
#define RAY_INFINITY 1000000.0f

/* Sector Flags for Liquids and special effects */
#define RAY_SECTOR_FLAG_WATER 1
#define RAY_SECTOR_FLAG_LAVA 2
#define RAY_SECTOR_FLAG_ACID 4
#define RAY_SECTOR_FLAG_SCROLL_X 8
#define RAY_SECTOR_FLAG_SCROLL_Y 16
#define RAY_SECTOR_FLAG_LIQUID_FLOOR 32
#define RAY_SECTOR_FLAG_LIQUID_CEILING 64
#define RAY_SECTOR_FLAG_LIQUID_WALLS 128
#define RAY_SECTOR_FLAG_RIPPLES 256

/* ============================================================================
   ESTRUCTURAS BÁSICAS
   ============================================================================
 */

/* Punto 2D */
typedef struct {
  float x, y;
} RAY_Point;

/* ============================================================================
   PORTAL RENDERING STRUCTURES
   ============================================================================
 */

/* Horizontal Frustum - defines visible X range for recursive rendering */
typedef struct {
  int x_left;  /* Left edge of visible frustum (inclusive) */
  int x_right; /* Right edge of visible frustum (inclusive) */
} RAY_Frustum;

/* Occlusion Buffer - tracks occluded screen areas per column */
typedef struct {
  int *y_top;    /* Top of visible area per column */
  int *y_bottom; /* Bottom of visible area per column */
  int width;     /* Screen width */
} RAY_OcclusionBuffer;

/* ============================================================================
   WALL - Pared de un sector
   ============================================================================
 */

typedef struct {
  int wall_id;          /* ID único de la pared */
  float x1, y1, x2, y2; /* Segmento de pared en world space */

  /* Texturas múltiples por altura (elegidas manualmente en editor) */
  int texture_id_lower;        /* Textura parte baja */
  int texture_id_middle;       /* Textura parte media */
  int texture_id_upper;        /* Textura parte alta */
  float texture_split_z_lower; /* Altura donde cambia baja->media */
  float texture_split_z_upper; /* Altura donde cambia media->alta */

  int texture_id_lower_normal;  /* Normal map parte baja */
  int texture_id_middle_normal; /* Normal map parte media */
  int texture_id_upper_normal;  /* Normal map parte alta */

  int portal_id; /* -1 si no es portal, >=0 si es portal */
  int flags;     /* Flags adicionales */
} RAY_Wall;

/* ============================================================================
   SECTOR - Polígono convexo que define un área del mapa
   ============================================================================
 */

typedef struct {
  int sector_id; /* ID único del sector */

  /* Geometría del polígono (máximo RAY_MAX_VERTICES_PER_SECTOR vértices) */
  RAY_Point *vertices;   /* Vértices del polígono */
  int num_vertices;      /* Número de vértices */
  int vertices_capacity; /* Capacidad del array */

  /* Alturas */
  float floor_z;   /* Altura del suelo */
  float ceiling_z; /* Altura del techo */

  /* Texturas */
  int floor_texture_id;   /* ID de textura del suelo */
  int ceiling_texture_id; /* ID de textura del techo */

  /* Normal Maps */
  int floor_normal_id;   /* ID de normal map del suelo */
  int ceiling_normal_id; /* ID de normal map del techo */

  /* Paredes del sector */
  RAY_Wall *walls;    /* Array de paredes */
  int num_walls;      /* Número de paredes */
  int walls_capacity; /* Capacidad del array */

  /* Portales */
  int *portal_ids;      /* IDs de portales conectados */
  int num_portals;      /* Número de portales */
  int portals_capacity; /* Capacidad del array */

  /* Iluminación */
  int light_level; /* Nivel de iluminación (0-255) */

  /* ========================================================================
     SECTOR HIERARCHY - For nested sectors (boxes, columns, platforms)
     ======================================================================== */

  int parent_sector_id;  /* -1 = root sector, >=0 = parent ID */
  int *child_sector_ids; /* Array of child sector IDs */
  int num_children;      /* Number of children */
  int children_capacity; /* Capacity of child array */

  /* Optimization: AABB (Axis Aligned Bounding Box) */
  float min_x, min_y;
  float max_x, max_y;

  int flags;              /* Sector flags (Liquid types, etc.) */
  float liquid_intensity; /* Intensity of the distortion effect (0.0 to 1.0+) */
  float liquid_speed;     /* Speed of the distortion/ripples (0.0 to 10.0+) */

  /* v28+: Volumetric Fog per sector */
  float fog_color_r, fog_color_g, fog_color_b; /* Fog color (0.0 - 1.0) */
  float fog_density;                           /* 0 = no fog, 100 = full */
  float fog_start;                             /* Distance where fog begins */
  float fog_end; /* Distance where fog is fully opaque */
} RAY_Sector;

/* ============================================================================
   PORTAL - Conexión entre dos sectores
   ============================================================================
 */

typedef struct {
  int portal_id;            /* ID único del portal */
  int sector_a, sector_b;   /* IDs de sectores conectados */
  int wall_id_a, wall_id_b; /* IDs de paredes en cada sector */
  float x1, y1, x2, y2;     /* Segmento del portal en world space */

  /* Clipping Information (calculado durante rendering) */
  int screen_x1, screen_x2;               /* Rango de columnas en pantalla */
  int screen_y1_top, screen_y2_top;       /* Límite superior */
  int screen_y1_bottom, screen_y2_bottom; /* Límite inferior */
  int visible;                            /* 1 si es visible desde la cámara */
} RAY_Portal;

/* ============================================================================
   SPRITES
   ============================================================================
 */

/* Forward declaration for MD2 models */
typedef struct RAY_Model RAY_Model;
extern float *g_zbuffer;

typedef struct {
  float x, y, z;
  int w, h;
  int dir;   /* -1 izquierda, 1 derecha */
  float rot; /* Rotación en radianes */
  int speed; /* 1 adelante, -1 atrás */
  int moveSpeed;
  float rotSpeed;
  float distance;        /* Distancia al jugador (para z-buffer) */
  int fileID;            /* ID del FPG (0 para g_engine.fpg_id) */
  int textureID;         /* ID de textura en el FPG */
  int flags;             /* Flags (billboard directions, etc) */
  INSTANCE *process_ptr; /* Puntero al proceso BennuGD vinculado */
  int flag_id;           /* ID de la flag de spawn asociada */
  int cleanup;           /* 1 si debe eliminarse */
  int frameRate;
  int frame;
  int hidden; /* 1 si está oculto */
  int jumping;
  float heightJumped;
  int rayhit; /* 1 si fue golpeado por un rayo */

  /* Collision box (OBB/AABB) */
  float col_w, col_h, col_d;
  int type; /* Entity type ID for filtering collisions */

  /* Soporte MD2/MD3/glTF */
  struct RAY_Model
      *model; /* Puntero al modelo MD2/MD3/glTF (o NULL si es sprite plano) */
  int currentFrame;
  int nextFrame;
  float interpolation; /* Factor de interpolación entre frames (0.0 - 1.0) */
  float model_scale;   /* Factor de escala del modelo (1.0 = normal, 10.0 = 10x
                          más grande) */
  int md3_surface_textures[32]; /* Texturas por superficie si es MD3 */

  /* Soporte animación glTF */
  int glb_anim_index;
  float glb_anim_time;
  float glb_anim_speed;

  /* Physics body (NULL = no physics, static sprite) */
  struct RAY_PhysicsBody *physics;
} RAY_Sprite;

/* ============================================================================
   PHYSICS BODY - Rigid body simulation properties
   ============================================================================
 */
typedef struct RAY_PhysicsBody {
  /* Linear motion */
  float vx, vy, vz; /* Velocity (units/sec) */
  float ax, ay, az; /* Accumulated forces/acceleration */

  /* Angular motion */
  float ang_vx, ang_vy, ang_vz; /* Angular velocity (rad/sec) */
  float rot_x, rot_y;           /* Current tilt angles (pitch/roll) */

  /* Material properties */
  float mass;            /* Mass in kg (0 = infinite/static) */
  float inv_mass;        /* Precomputed 1/mass (0 if static) */
  float friction;        /* Surface friction [0..1] */
  float restitution;     /* Bounciness [0..1] */
  float gravity_scale;   /* Gravity multiplier (1.0 = normal) */
  float linear_damping;  /* Air resistance for movement [0..1] */
  float angular_damping; /* Air resistance for rotation [0..1] */

  /* Collision shape */
  float col_radius; /* Bounding sphere/cylinder radius */
  float col_height; /* Vertical extent */

  /* Flags */
  int is_static;       /* 1 = immovable */
  int is_kinematic;    /* 1 = moved by code, not physics */
  int is_trigger;      /* 1 = overlap detection only */
  int lock_rot_x;      /* 1 = prevent tipping on X */
  int lock_rot_y;      /* 1 = prevent tipping on Y */
  int lock_rot_z;      /* 1 = prevent spinning on Z */
  int on_ground;       /* 1 = resting on a surface */
  int collision_layer; /* Bitmask for collision filtering */
  int collision_mask;  /* Which layers this body collides with */

  /* Sector awareness */
  int current_sector_id; /* Which sector this body is in */
} RAY_PhysicsBody;

/* ============================================================================
   SPAWN FLAGS - Posiciones de spawn para sprites
   ============================================================================
 */

typedef struct {
  int flag_id;           /* ID único de la flag (1, 2, 3...) */
  float x, y, z;         /* Posición de spawn en el mundo */
  int occupied;          /* 1 si ya hay un sprite en esta flag */
  INSTANCE *process_ptr; /* Puntero al proceso vinculado */
} RAY_SpawnFlag;

/* ============================================================================
   LIGHTS
   ============================================================================
 */

typedef struct {
  float x, y, z;
  float r, g, b;
  float intensity; // Radius/Intensity
  float falloff;   // 1=linear, 2=quadratic
} RAY_Light;

/* ============================================================================
   RAY HIT - Información de colisión de un rayo
   ============================================================================
 */

typedef struct {
  float x, y;            /* Posición del impacto en world space */
  int sector_id;         /* ID del sector golpeado */
  int wall_id;           /* ID de la pared golpeada */
  int strip;             /* Columna de pantalla */
  float tileX;           /* Coordenada X dentro de la textura */
  float distance;        /* Distancia al impacto */
  float correctDistance; /* Distancia corregida (fisheye) */
  float rayAngle;        /* Ángulo del rayo */
  RAY_Sprite *sprite;    /* Sprite golpeado (NULL si es pared) */
  RAY_Wall *wall;        /* Pared golpeada (NULL si es sprite) */
  float wallHeight;      /* Altura de la pared */
  float wallZOffset;     /* Z-offset (altura base) de la pared */
  float sortdistance;    /* Distancia de ordenamiento */
  int is_child_sector;   /* 1 si este sector es hijo del sector de la cámara */
} RAY_RayHit;

/* ============================================================================
   CÁMARA
   ============================================================================
 */

typedef struct {
  float x, y, z;
  float rot;   /* Rotación en radianes */
  float pitch; /* Pitch (mirar arriba/abajo) */
  float moveSpeed;
  float rotSpeed;

  /* Jumping */
  int jumping;
  float heightJumped;

  /* Sector actual */
  int current_sector_id; /* ID del sector donde está la cámara */
} RAY_Camera;

/* ============================================================================
   CLIPPING WINDOW - Para renderizado de portales
   ============================================================================
 */

typedef struct {
  int x1, x2;    /* Rango horizontal (columnas) */
  int *y_top;    /* Array de límites superiores */
  int *y_bottom; /* Array de límites inferiores */
} RAY_ClipWindow;

/* ============================================================================
   ESTADO DEL MOTOR
   ============================================================================
 */

typedef struct {
  /* Configuración de pantalla */
  int displayWidth, displayHeight;   // Target/Output resolution
  int internalWidth, internalHeight; // Rendering resolution (can be lower)
  float resolutionScale;             // Scale factor (e.g., 0.5 for half-res)
  int stripWidth;
  int rayCount;
  int fovDegrees;
  float fovRadians;
  float viewDist;

  /* Ángulos precalculados */
  float *stripAngles;

  /* Cámara */
  RAY_Camera camera;

  /* Sectores y portales (ÚNICO sistema de geometría) */
  RAY_Sector *sectors;
  int num_sectors;
  int sectors_capacity;

  RAY_Portal *portals;
  int num_portals;
  int portals_capacity;

  /* Sprites */
  RAY_Sprite *sprites;
  int num_sprites;
  int sprites_capacity;

  /* Spawn Flags */
  RAY_SpawnFlag *spawn_flags;
  int num_spawn_flags;
  int spawn_flags_capacity;

  /* FPG de texturas */
  int fpg_id;

  /* Física */
  float default_step_height; /* Altura máxima de escalón (climbing) */

  /* Skybox */
  int skyTextureID; /* ID de textura para el cielo */

  /* Configuration */
  int drawMiniMap;
  int drawTexturedFloor;
  int drawCeiling;
  int drawWalls;
  int drawWeapon;
  int fogOn;

  /* Texture Filtering */
  int texture_quality; /* 0 = Nearest, 1 = Bilinear */

  /* Fog configuration */
  uint8_t fog_r, fog_g, fog_b;
  float fog_start_distance;
  float fog_end_distance;

  /* Minimapa configuration */
  int minimap_size;
  int minimap_x, minimap_y;
  float minimap_scale;

  /* Portal Rendering Configuration */
  int max_portal_depth;         /* Profundidad máxima de recursión */
  int portal_rendering_enabled; /* 1 = activo, 0 = desactivado */

  /* Billboard */
  int billboard_enabled;
  int billboard_directions;

  /* PVS (Static Potentially Visible Set) */
  uint8_t *pvs_matrix; /* Matrix: pvs[source_id * num_sectors + target_id] */
  int pvs_ready;       /* 1 if PVS is baked and valid */

  /* Luces puntuales */
  RAY_Light lights[RAY_MAX_LIGHTS];
  int num_lights;

  /* Inicializado */
  int initialized;
  float time;          /* Tiempo global para shaders */
  uint32_t last_ticks; /* Para cálculo de delta time en animaciones */
} RAY_Engine;

/* ============================================================================
   FUNCIONES PÚBLICAS - API del motor
   ============================================================================
 */

/* Inicialización */
extern int64_t libmod_ray_init(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_shutdown(INSTANCE *my, int64_t *params);

/* Carga de mapas */
extern int64_t libmod_ray_load_map(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_free_map(INSTANCE *my, int64_t *params);

/* Cámara */
extern int64_t libmod_ray_set_camera(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_get_camera_x(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_get_camera_y(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_get_camera_z(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_get_camera_rot(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_get_camera_pitch(INSTANCE *my, int64_t *params);

/* Movimiento */
extern int64_t libmod_ray_move_forward(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_move_backward(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_strafe_left(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_strafe_right(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_rotate(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_look_up_down(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_move_up_down(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_jump(INSTANCE *my, int64_t *params);

/* Renderizado */
extern int64_t libmod_ray_render(INSTANCE *my, int64_t *params);
extern void ray_render_md2(GRAPH *dest, RAY_Sprite *sprite);

/* Configuración */
extern int64_t libmod_ray_set_fog(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_set_draw_minimap(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_set_draw_weapon(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_set_billboard(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_check_collision(INSTANCE *my, int64_t *params);

/* Sprites dinámicos */
extern int64_t libmod_ray_add_sprite(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_remove_sprite(INSTANCE *my, int64_t *params);

/* Spawn Flags */
extern int64_t libmod_ray_set_flag(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_clear_flag(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_get_flag_x(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_get_flag_y(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_get_flag_z(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_update_sprite_position(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_set_sprite_angle(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_get_floor_height(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_camera_is_playing(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_set_minimap(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_set_collision_box(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_get_collision(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_set_sprite_flags(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_set_sprite_graph(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_set_sprite_md3_surface_texture(INSTANCE *my,
                                                         int64_t *params);
extern int64_t libmod_ray_camera_load(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_camera_play(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_camera_path_update(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_camera_stop(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_camera_pause(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_camera_resume(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_camera_get_time(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_camera_set_time(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_camera_free(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_set_fov(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_set_texture_quality(INSTANCE *my, int64_t *params);

/* Distances (v29+) */
extern int64_t libmod_ray_get_dist(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_get_camera_dist(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_get_point_dist(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_get_angle(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_get_camera_angle(INSTANCE *my, int64_t *params);

/* Iluminación */
extern int64_t libmod_ray_add_light(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_clear_lights(INSTANCE *my, int64_t *params);

/* ============================================================================
   PHYSICS ENGINE
   ============================================================================
 */

/* Core physics functions (C API) */
extern void ray_physics_init(void);
extern void ray_physics_step(float dt);
extern RAY_PhysicsBody *ray_physics_create_body(float mass, float radius,
                                                float height);
extern void ray_physics_destroy_body(RAY_PhysicsBody *body);
extern void ray_physics_apply_force(RAY_PhysicsBody *body, float fx, float fy,
                                    float fz);
extern void ray_physics_apply_impulse(RAY_PhysicsBody *body, float ix, float iy,
                                      float iz);
extern void ray_physics_set_velocity(RAY_PhysicsBody *body, float vx, float vy,
                                     float vz);

/* BennuGD2 bindings */
extern int64_t libmod_ray_physics_enable(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_physics_set_mass(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_physics_set_friction(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_physics_set_restitution(INSTANCE *my,
                                                  int64_t *params);
extern int64_t libmod_ray_physics_set_gravity_scale(INSTANCE *my,
                                                    int64_t *params);
extern int64_t libmod_ray_physics_set_damping(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_physics_set_static(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_physics_set_kinematic(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_physics_set_trigger(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_physics_set_lock_rotation(INSTANCE *my,
                                                    int64_t *params);
extern int64_t libmod_ray_physics_set_collision_layer(INSTANCE *my,
                                                      int64_t *params);
extern int64_t libmod_ray_physics_apply_force_bgd(INSTANCE *my,
                                                  int64_t *params);
extern int64_t libmod_ray_physics_apply_impulse_bgd(INSTANCE *my,
                                                    int64_t *params);
extern int64_t libmod_ray_physics_get_velocity(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_physics_step_bgd(INSTANCE *my, int64_t *params);

/* ============================================================================
   FUNCIONES INTERNAS - Geometría
   ============================================================================
 */

/* Geometría de polígonos */
int ray_point_in_polygon(float px, float py, const RAY_Point *vertices,
                         int num_vertices);
int ray_point_in_sector_local(RAY_Sector *sector, float px, float py);
int ray_polygon_is_convex(const RAY_Point *vertices, int num_vertices);
int ray_line_segment_intersect(float x1, float y1, float x2, float y2, float x3,
                               float y3, float x4, float y4, float *ix,
                               float *iy);

/* Sectores */
RAY_Sector *ray_sector_create(int sector_id, float floor_z, float ceiling_z,
                              int floor_tex, int ceiling_tex);
void ray_sector_free(RAY_Sector *sector);
void ray_sector_add_vertex(RAY_Sector *sector, float x, float y);
void ray_sector_add_wall(RAY_Sector *sector, const RAY_Wall *wall);
void ray_sector_add_portal(RAY_Sector *sector, int portal_id);
RAY_Sector *ray_find_sector_at_point(RAY_Engine *engine, float x, float y);
RAY_Sector *ray_find_sector_at_position(RAY_Engine *engine, float x, float y,
                                        float z);

/* Paredes */
RAY_Wall *ray_wall_create(int wall_id, float x1, float y1, float x2, float y2);
void ray_wall_set_textures(RAY_Wall *wall, int lower, int middle, int upper,
                           float split_lower, float split_upper);

/* Portales */
RAY_Portal *ray_portal_create(int portal_id, int sector_a, int sector_b,
                              int wall_id_a, int wall_id_b, float x1, float y1,
                              float x2, float y2);
void ray_portal_free(RAY_Portal *portal);
void ray_detect_portals(RAY_Engine *engine);
int ray_portal_is_visible(RAY_Portal *portal, RAY_Camera *camera);

/* ============================================================================
   FUNCIONES INTERNAS - Map Loading/Saving
   ============================================================================
 */

/* Map Loading/Saving (v9) */
int ray_load_map(const char *filename);
int ray_save_map_v9(const char *filename);

/* Deprecated (kept for temporary compatibility if needed) */
int ray_load_map_v8(const char *filename);
int ray_save_map_v8(const char *filename);

/* ============================================================================
   FUNCIONES INTERNAS - Raycasting
   ============================================================================
 */

void ray_cast_against_sector(RAY_Engine *engine, RAY_Sector *sector,
                             float ray_x, float ray_y, float ray_angle,
                             RAY_RayHit *hits, int *num_hits,
                             float accumulated_distance);
void ray_find_wall_intersection(float ray_x, float ray_y, float ray_angle,
                                const RAY_Wall *wall, float *distance,
                                float *hit_x, float *hit_y);

/* ============================================================================
   FUNCIONES INTERNAS - Renderizado
   ============================================================================
 */

void ray_render_frame(GRAPH *dest);
void ray_render_sector(RAY_Engine *engine, RAY_Sector *sector,
                       RAY_ClipWindow *clip_window, int depth);
void ray_render_sector_walls(RAY_Engine *engine, RAY_Sector *sector,
                             RAY_ClipWindow *clip_window);
void ray_render_sector_floor(RAY_Engine *engine, RAY_Sector *sector,
                             RAY_ClipWindow *clip_window);
void ray_render_sector_ceiling(RAY_Engine *engine, RAY_Sector *sector,
                               RAY_ClipWindow *clip_window);
void ray_render_sector_portals(RAY_Engine *engine, RAY_Sector *sector,
                               RAY_ClipWindow *clip_window, int depth);

/* Clipping Window */
RAY_ClipWindow *ray_clip_window_create(int screen_width, int screen_height);
void ray_clip_window_free(RAY_ClipWindow *window);
void ray_clip_window_reset(RAY_ClipWindow *window, int screen_width,
                           int screen_height);
void ray_clip_window_clip_to_portal(RAY_ClipWindow *window, RAY_Portal *portal);

/* Utilidades */
float ray_screen_distance(float screenWidth, float fovRadians);
float ray_strip_angle(float screenX, float screenDistance);
float ray_strip_screen_height(float screenDistance, float correctDistance,
                              float height);

/* Shared Rendering API (Used by Recursive and Build renderers) */
void ray_draw_floor_ceiling(GRAPH *dest, int screen_x, float ray_angle,
                            int sector_id, float dist_start, float dist_end,
                            float *z_buffer, int *clip_top, int *clip_bottom);
void ray_draw_wall_strip(GRAPH *dest, RAY_RayHit *hit, int screen_x,
                         int *clip_top, int *clip_bottom);
uint32_t ray_sample_texture(GRAPH *texture, int tex_x, int tex_y);
uint32_t ray_sample_texture_bilinear(GRAPH *texture, float u, float v);
void ray_cast_ray(RAY_Engine *engine, int sector_id, float x, float y,
                  float angle, int strip_idx, RAY_RayHit *hits, int *num_hits);
uint32_t ray_fog_pixel(uint32_t pixel, float distance);

#endif /* __LIBMOD_RAY_H */
