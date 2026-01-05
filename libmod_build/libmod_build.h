/*
 * libmod_build.h
 * Build Engine Module for BennuGD2 - Main Header
 */

#ifndef __LIBMOD_BUILD_H
#define __LIBMOD_BUILD_H

#include <stdint.h>
#include "bgddl.h"
#include "g_bitmap.h"  // For GRAPH type

#define BUILD_MAXSECTORS 4096
#define BUILD_MAXWALLS 16384
#define BUILD_MAXSPRITES 16384

// Tile information (Moved from libmod_build_art.c to be accessible by renderer)
typedef struct {
    int16_t xsize;
    int16_t ysize;
    int32_t picanm;
    uint8_t *data;          // Pixel data (palette indices)
    GRAPH *graph;           // Converted to BennuGD2 GRAPH (cache for simple rendering)
} BUILD_Tile;

// Palette and Shading Globals
extern uint8_t g_build_palette[768];  // Raw RGB palette
extern uint8_t *g_palookup;           // Shading lookup table (numshades * 256)
extern int g_numshades;               // Number of shade levels
extern uint32_t g_palette_cache[256]; // Pre-calculated 32-bit colors for fast rendering

// Sector structure (40 bytes) - adapted from sectortype
typedef struct {
    int16_t wallptr, wallnum;
    int32_t ceilingz, floorz;
    int16_t ceilingstat, floorstat;
    int16_t ceilingpicnum, ceilingheinum;
    int8_t ceilingshade;
    uint8_t ceilingpal, ceilingxpanning, ceilingypanning;
    int16_t floorpicnum, floorheinum;
    int8_t floorshade;
    uint8_t floorpal, floorxpanning, floorypanning;
    uint8_t visibility, filler;
    int16_t lotag, hitag, extra;
} BUILD_Sector;

// Wall structure (32 bytes) - adapted from walltype
typedef struct {
    int32_t x, y;
    int16_t point2, nextwall, nextsector, cstat;
    int16_t picnum, overpicnum;
    int8_t shade;
    uint8_t pal, xrepeat, yrepeat, xpanning, ypanning;
    int16_t lotag, hitag, extra;
} BUILD_Wall;

// Sprite structure (44 bytes) - adapted from spritetype
typedef struct {
    int32_t x, y, z;
    int16_t cstat, picnum;
    int8_t shade;
    uint8_t pal, clipdist, filler;
    uint8_t xrepeat, yrepeat;
    int8_t xoffset, yoffset;
    int16_t sectnum, statnum;
    int16_t ang, owner, xvel, yvel, zvel;
    int16_t lotag, hitag, extra;
} BUILD_Sprite;

// Camera structure
typedef struct {
    int32_t x, y, z;      // Position
    int16_t ang;          // Angle (0-2047, Build Engine uses 2048 angles instead of 360 degrees)
    int32_t horiz;        // Horizon (100 = looking straight, <100 = looking up, >100 = looking down)
    int16_t cursectnum;   // Current sector number
} BUILD_Camera;

// Map structure
typedef struct {
    BUILD_Sector *sectors;
    BUILD_Wall *walls;
    BUILD_Sprite *sprites;
    int16_t numsectors;
    int16_t numwalls;
    int16_t numsprites;
    int32_t version;      // Map version
} BUILD_Map;

// Global variables (extern, defined in libmod_build.c)
extern BUILD_Map *g_build_map;
extern BUILD_Camera g_build_camera;

// API Functions - Map Loading
int64_t libmod_build_load_map(INSTANCE *my, int64_t *params);

// API Functions - Texture Loading
int64_t libmod_build_load_palette(INSTANCE *my, int64_t *params);
int64_t libmod_build_load_art(INSTANCE *my, int64_t *params);

// API Functions - Rendering
int64_t libmod_build_render(INSTANCE *my, int64_t *params);

// API Functions - Camera
int64_t libmod_build_set_camera(INSTANCE *my, int64_t *params);

// API Functions - Movement
int64_t libmod_build_move_forward(INSTANCE *my, int64_t *params);
int64_t libmod_build_move_backward(INSTANCE *my, int64_t *params);
int64_t libmod_build_strafe_left(INSTANCE *my, int64_t *params);
int64_t libmod_build_strafe_right(INSTANCE *my, int64_t *params);
int64_t libmod_build_move_vertical(INSTANCE *my, int64_t *params);

// API Functions - Look
int64_t libmod_build_look_horizontal(INSTANCE *my, int64_t *params);
int64_t libmod_build_look_vertical(INSTANCE *my, int64_t *params);

// Internal functions
void build_init_tables(void);
void build_free_map(BUILD_Map *map);
BUILD_Map* build_load_map_file(const char *filename, int32_t *start_x, int32_t *start_y, 
                                int32_t *start_z, int16_t *start_ang, int16_t *start_sectnum);
void build_render_frame(BUILD_Map *map, BUILD_Camera *camera, int width, int height);
int build_get_render_buffer_code(void);
void build_cleanup_render(void);

// ART texture system
int build_load_palette(const char *filename);
int build_load_art(const char *filename);
GRAPH* build_get_tile_graph(int tile_num);
void build_free_tiles(void);
int build_get_tile_width(int tile_num);
int build_get_tile_height(int tile_num);
// Physics and Sector Updates
int build_inside_sector(BUILD_Map *map, int sectnum, int32_t x, int32_t y);
int16_t build_updatesector(BUILD_Map *map, int32_t x, int32_t y, int16_t last_sectnum);

#endif // __LIBMOD_BUILD_H
