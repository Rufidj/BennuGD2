#ifndef __LIBMOD_RAY_H
#define __LIBMOD_RAY_H

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

/* Constantes del motor */
#define RAY_TILE_SIZE 128
#define RAY_TEXTURE_SIZE 128
#define RAY_MAX_SPRITES 1000
#define RAY_MAX_THIN_WALLS 1000
#define RAY_MAX_THICK_WALLS 100
#define RAY_MAX_RAYHITS 2000
#define RAY_TWO_PI (M_PI * 2.0f)

/* Tipos de ThickWall */
#define RAY_THICK_WALL_TYPE_NONE 0
#define RAY_THICK_WALL_TYPE_RECT 1
#define RAY_THICK_WALL_TYPE_TRIANGLE 2
#define RAY_THICK_WALL_TYPE_QUAD 3

/* Tipos de Slope */
#define RAY_SLOPE_TYPE_WEST_EAST 1
#define RAY_SLOPE_TYPE_NORTH_SOUTH 2

/* Tipos de puertas (compatibilidad con motor original) */
#define RAY_DOOR_VERTICAL_MIN 1000
#define RAY_DOOR_VERTICAL_MAX 1499
#define RAY_DOOR_HORIZONTAL_MIN 1500

/* ============================================================================
   ESTRUCTURAS BÁSICAS
   ============================================================================ */

/* Punto 2D */
typedef struct {
    float x, y;
} RAY_Point;

/* ============================================================================
   THIN WALLS - Paredes delgadas (linedefs)
   ============================================================================ */

typedef struct RAY_ThickWall RAY_ThickWall; /* Forward declaration */

typedef struct {
    float x1, y1, x2, y2;           /* Coordenadas de inicio y fin */
    int wallType;                    /* Tipo de pared */
    int horizontal;                  /* 1 si es horizontal, 0 si vertical */
    float height;                    /* Altura de la pared */
    float z;                         /* Altura del suelo de la pared */
    float slope;                     /* Pendiente (para slopes) */
    int hidden;                      /* 1 si está oculta */
    RAY_ThickWall *thickWall;       /* Puntero al ThickWall padre */
} RAY_ThinWall;

/* ============================================================================
   THICK WALLS - Paredes gruesas (sectores cerrados)
   ============================================================================ */

typedef struct RAY_ThickWall {
    int type;                        /* RECT, TRIANGLE, QUAD */
    int slopeType;                   /* WEST_EAST, NORTH_SOUTH */
    
    /* Para RECT */
    float x, y, w, h;
    
    /* Para TRIANGLE y QUAD */
    RAY_Point *points;
    int num_points;
    
    /* ThinWalls que forman este ThickWall */
    RAY_ThinWall *thinWalls;
    int num_thin_walls;
    int thin_walls_capacity;
    
    /* Propiedades */
    float slope;
    int ceilingTextureID;
    int floorTextureID;
    float startHeight, endHeight;
    float tallerHeight;
    int invertedSlope;
    
    /* Altura y Z */
    float height;
    float z;
} RAY_ThickWall;

/* ============================================================================
   DOORS
   ============================================================================ */

typedef struct {
    int state;           /* 0 = cerrada, 1 = abierta */
    float offset;        /* 0.0 a 1.0 - progreso de animación */
    int animating;       /* 1 si está animándose */
    float anim_speed;    /* Velocidad de animación (unidades por segundo) */
} RAY_Door;

/* ============================================================================
   SPAWN FLAGS - Posiciones de spawn para sprites
   ============================================================================ */

typedef struct {
    int flag_id;          /* ID único de la flag (1, 2, 3...) */
    float x, y, z;        /* Posición de spawn en el mundo */
    int level;            /* Nivel del grid */
    int occupied;         /* 1 si ya hay un sprite en esta flag */
    INSTANCE *process_ptr; /* Puntero al proceso vinculado (NULL = libre) */
} RAY_SpawnFlag;

/* ============================================================================
   SPRITES
   ============================================================================ */

typedef struct {
    float x, y, z;
    int w, h;
    int level;                       /* Nivel del grid (0, 1, 2...) */
    int dir;                         /* -1 izquierda, 1 derecha */
    float rot;                       /* Rotación en radianes */
    int speed;                       /* 1 adelante, -1 atrás */
    int moveSpeed;
    float rotSpeed;
    float distance;                  /* Distancia al jugador (para z-buffer) */
    int textureID;                   /* ID de textura en el FPG (para sprites estáticos) */
    INSTANCE *process_ptr;           /* Puntero al proceso BennuGD vinculado (NULL = usar textureID) */
    int flag_id;                     /* ID de la flag de spawn asociada (-1 = sprite manual) */
    int cleanup;                     /* 1 si debe eliminarse */
    int frameRate;
    int frame;
    int hidden;                      /* 1 si está oculto */
    int jumping;
    float heightJumped;
    int rayhit;                      /* 1 si fue golpeado por un rayo */
} RAY_Sprite;

/* ============================================================================
   RAY HIT - Información de colisión de un rayo
   ============================================================================ */

typedef struct {
    float x, y;                      /* Posición del impacto en unidades de juego */
    int wallX, wallY;                /* Posición en grid (columna, fila) */
    int wallType;                    /* Tipo de pared golpeada */
    int strip;                       /* Columna de pantalla */
    float tileX;                     /* Coordenada X dentro del tile (para textura) */
    float squaredDistance;           /* Distancia al cuadrado */
    float distance;                  /* Distancia al impacto */
    float correctDistance;           /* Distancia corregida (fisheye) */
    int horizontal;                  /* 1 si golpeó pared horizontal */
    float rayAngle;                  /* Ángulo del rayo */
    RAY_Sprite *sprite;              /* Sprite golpeado (NULL si es pared) */
    int level;                       /* Nivel del grid */
    int right;                       /* 1 si el rayo va a la derecha */
    int up;                          /* 1 si el rayo va hacia arriba */
    RAY_ThinWall *thinWall;         /* ThinWall golpeado */
    float wallHeight;                /* Altura de la pared */
    float invertedZ;                 /* Z invertido (para slopes invertidos) */
    
    /* Sibling (para slopes) */
    float siblingWallHeight;
    float siblingDistance;
    float siblingCorrectDistance;
    float siblingThinWallZ;
    float siblingInvertedZ;
    
    /* Distancia de ordenamiento (para z-buffer) */
    float sortdistance;
} RAY_RayHit;

/* ============================================================================
   RAYCASTER - Motor principal
   ============================================================================ */

typedef struct {
    int **grids;                     /* Array de grids [nivel][offset] */
    int gridWidth;
    int gridHeight;
    int gridCount;                   /* Número de niveles */
    int tileSize;
} RAY_Raycaster;

/* ============================================================================
   CÁMARA
   ============================================================================ */

typedef struct {
    float x, y, z;
    float rot;                       /* Rotación en radianes */
    float pitch;                     /* Pitch (mirar arriba/abajo) */
    float moveSpeed;
    float rotSpeed;
    
    /* Jumping */
    int jumping;
    float heightJumped;
} RAY_Camera;

/* ============================================================================
   ESTADO DEL MOTOR
   ============================================================================ */

typedef struct {
    /* Configuración */
    int displayWidth, displayHeight;
    int stripWidth;
    int rayCount;
    int fovDegrees;
    float fovRadians;
    float viewDist;
    
    /* Ángulos precalculados */
    float *stripAngles;
    
    /* Raycaster */
    RAY_Raycaster raycaster;
    
    /* Cámara */
    RAY_Camera camera;
    
    /* Sprites */
    RAY_Sprite *sprites;
    int num_sprites;
    int sprites_capacity;
    
    /* ThinWalls */
    RAY_ThinWall **thinWalls;        /* Array de punteros */
    int num_thin_walls;
    int thin_walls_capacity;
    
    /* ThickWalls */
    RAY_ThickWall **thickWalls;      /* Array de punteros */
    int num_thick_walls;
    int thick_walls_capacity;
    
    
    /* Grids de suelo y techo - POR NIVEL (0, 1, 2) */
    int *floorGrids[3];                  /* Grids de suelo por nivel [level][x + y * width] */
    int *ceilingGrids[3];                /* Grids de techo por nivel [level][x + y * width] */
    
    /* Puertas */
    RAY_Door *doors;                 /* Estado de puertas [x + y * width] */
    
    /* Spawn Flags */
    RAY_SpawnFlag *spawn_flags;      /* Array de spawn flags */
    int num_spawn_flags;
    int spawn_flags_capacity;
    
    /* FPG de texturas */
    int fpg_id;
    
    /* Skybox */
    int skyTextureID;  /* ID de textura para el cielo (0 = color sólido) */
    
    /* Configuration */
    int drawMiniMap;
    int drawTexturedFloor;
    int drawCeiling;
    int drawWalls;
    int drawWeapon;
    int fogOn;
    int skipDrawnFloorStrips;
    int skipDrawnSkyboxStrips;
    int skipDrawnHighestCeilingStrips;
    
    /* Fog configuration */
    uint8_t fog_r, fog_g, fog_b;  /* Color del fog (RGB) */
    float fog_start_distance;     /* Distancia donde empieza el fog */
    float fog_end_distance;       /* Distancia donde el fog es completo */
    
    /* Minimapa configuration */
    int minimap_size;             /* Tamaño del minimapa en pixels */
    int minimap_x, minimap_y;     /* Posición en pantalla */
    float minimap_scale;          /* Escala del minimapa */
    
    /* Nivel más alto de techo */
    int highestCeilingLevel;
    
    /* Billboard - Sistema de sprites mirando a cámara */
    int billboard_enabled;      /* 1 = activo, 0 = desactivado */
    int billboard_directions;   /* Número de direcciones (típicamente 12) */
    
    /* Inicializado */
    int initialized;
} RAY_Engine;

/* ============================================================================
   FUNCIONES PÚBLICAS - Declaraciones
   ============================================================================ */

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
extern int64_t libmod_ray_jump(INSTANCE *my, int64_t *params);

/* Renderizado */
extern int64_t libmod_ray_render(INSTANCE *my, int64_t *params);

/* Configuración */
extern int64_t libmod_ray_set_fog(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_set_draw_minimap(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_set_draw_weapon(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_set_billboard(INSTANCE *my, int64_t *params);
extern int64_t libmod_ray_check_collision(INSTANCE *my, int64_t *params);

/* Puertas */
extern int64_t libmod_ray_toggle_door(INSTANCE *my, int64_t *params);

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
extern int64_t libmod_ray_set_minimap(INSTANCE *my, int64_t *params);

/* ============================================================================
   FUNCIONES INTERNAS - Declaraciones
   ============================================================================ */

/* Raycasting */
void ray_raycaster_create_grids(RAY_Raycaster *rc, int width, int height, int count, int tileSize);
void ray_raycaster_raycast(RAY_Raycaster *rc, RAY_RayHit *hits, int *num_hits,
                           int playerX, int playerY, float playerZ,
                           float playerRot, float stripAngle, int stripIdx,
                           RAY_Sprite *sprites, int num_sprites);
float ray_screen_distance(float screenWidth, float fovRadians);
float ray_strip_angle(float screenX, float screenDistance);
float ray_strip_screen_height(float screenDistance, float correctDistance, float tileSize);

/* Shape */
int ray_lines_intersect(float x1, float y1, float x2, float y2,
                        float x3, float y3, float x4, float y4,
                        float *ix, float *iy);
int ray_point_in_rect(float ptx, float pty, float x, float y, float w, float h);
float ray_sign(const RAY_Point *p1, const RAY_Point *p2, const RAY_Point *p3);
int ray_point_in_triangle(const RAY_Point *pt, const RAY_Point *v1,
                          const RAY_Point *v2, const RAY_Point *v3);
int ray_point_in_quad(const RAY_Point *pt, const RAY_Point *v1,
                      const RAY_Point *v2, const RAY_Point *v3,
                      const RAY_Point *v4);

/* ThinWall */
void ray_thin_wall_init(RAY_ThinWall *tw);
void ray_thin_wall_create(RAY_ThinWall *tw, float x1, float y1, float x2, float y2,
                          int wallType, RAY_ThickWall *thickWall, float wallHeight);
float ray_thin_wall_distance_to_origin(RAY_ThinWall *tw, float ix, float iy);

/* ThickWall */
void ray_thick_wall_init(RAY_ThickWall *tw);
void ray_thick_wall_free(RAY_ThickWall *tw);
void ray_thick_wall_create_rect(RAY_ThickWall *tw, float x, float y, float w, float h,
                                float z, float wallHeight);
void ray_thick_wall_create_triangle(RAY_ThickWall *tw, const RAY_Point *v1,
                                    const RAY_Point *v2, const RAY_Point *v3,
                                    float z, float wallHeight);
void ray_thick_wall_create_quad(RAY_ThickWall *tw, const RAY_Point *v1,
                                const RAY_Point *v2, const RAY_Point *v3,
                                const RAY_Point *v4, float z, float wallHeight);
void ray_thick_wall_create_rect_slope(RAY_ThickWall *tw, int slopeType,
                                      float x, float y, float w, float h, float z,
                                      float startHeight, float endHeight);
void ray_thick_wall_create_rect_inverted_slope(RAY_ThickWall *tw, int slopeType,
                                               float x, float y, float w, float h, float z,
                                               float startHeight, float endHeight);
void ray_thick_wall_set_z(RAY_ThickWall *tw, float z);
void ray_thick_wall_set_height(RAY_ThickWall *tw, float height);
void ray_thick_wall_set_thin_walls_type(RAY_ThickWall *tw, int wallType);
int ray_thick_wall_contains_point(RAY_ThickWall *tw, float x, float y);

/* Utilidades */
int ray_is_door(int wallType);
int ray_is_vertical_door(int wallType);
int ray_is_horizontal_door(int wallType);

#endif /* __LIBMOD_RAY_H */
