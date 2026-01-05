/*
 * libmod_build_map.c
 * Build Engine Module - Map Loading
 * 
 * Implements loading of Build Engine .MAP files (versions 5-8)
 */

#include "libmod_build.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Map file header structure
typedef struct {
    int32_t version;
    int32_t posx, posy, posz;
    int16_t ang, cursectnum;
} MAP_Header;

// ============================================================================
// Internal Helper Functions
// ============================================================================

static int read_int16(FILE *f, int16_t *val) {
    unsigned char buf[2];
    if (fread(buf, 1, 2, f) != 2) return -1;
    *val = (int16_t)(buf[0] | (buf[1] << 8));
    return 0;
}

static int read_int32(FILE *f, int32_t *val) {
    unsigned char buf[4];
    if (fread(buf, 1, 4, f) != 4) return -1;
    *val = (int32_t)(buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24));
    return 0;
}

static int read_uint8(FILE *f, uint8_t *val) {
    return (fread(val, 1, 1, f) == 1) ? 0 : -1;
}

static int read_int8(FILE *f, int8_t *val) {
    return (fread(val, 1, 1, f) == 1) ? 0 : -1;
}

// ============================================================================
// Map Loading Functions
// ============================================================================

static int load_sectors(FILE *f, BUILD_Map *map) {
    for (int i = 0; i < map->numsectors; i++) {
        BUILD_Sector *sec = &map->sectors[i];
        
        if (read_int16(f, &sec->wallptr) < 0) return -1;
        if (read_int16(f, &sec->wallnum) < 0) return -1;
        if (read_int32(f, &sec->ceilingz) < 0) return -1;
        if (read_int32(f, &sec->floorz) < 0) return -1;
        if (read_int16(f, &sec->ceilingstat) < 0) return -1;
        if (read_int16(f, &sec->floorstat) < 0) return -1;
        if (read_int16(f, &sec->ceilingpicnum) < 0) return -1;
        if (read_int16(f, &sec->ceilingheinum) < 0) return -1;
        if (read_int8(f, &sec->ceilingshade) < 0) return -1;
        if (read_uint8(f, &sec->ceilingpal) < 0) return -1;
        if (read_uint8(f, &sec->ceilingxpanning) < 0) return -1;
        if (read_uint8(f, &sec->ceilingypanning) < 0) return -1;
        if (read_int16(f, &sec->floorpicnum) < 0) return -1;
        if (read_int16(f, &sec->floorheinum) < 0) return -1;
        if (read_int8(f, &sec->floorshade) < 0) return -1;
        if (read_uint8(f, &sec->floorpal) < 0) return -1;
        if (read_uint8(f, &sec->floorxpanning) < 0) return -1;
        if (read_uint8(f, &sec->floorypanning) < 0) return -1;
        if (read_uint8(f, &sec->visibility) < 0) return -1;
        if (read_uint8(f, &sec->filler) < 0) return -1;
        if (read_int16(f, &sec->lotag) < 0) return -1;
        if (read_int16(f, &sec->hitag) < 0) return -1;
        if (read_int16(f, &sec->extra) < 0) return -1;
    }
    
    return 0;
}

static int load_walls(FILE *f, BUILD_Map *map) {
    for (int i = 0; i < map->numwalls; i++) {
        BUILD_Wall *wall = &map->walls[i];
        
        if (read_int32(f, &wall->x) < 0) return -1;
        if (read_int32(f, &wall->y) < 0) return -1;
        if (read_int16(f, &wall->point2) < 0) return -1;
        if (read_int16(f, &wall->nextwall) < 0) return -1;
        if (read_int16(f, &wall->nextsector) < 0) return -1;
        if (read_int16(f, &wall->cstat) < 0) return -1;
        if (read_int16(f, &wall->picnum) < 0) return -1;
        if (read_int16(f, &wall->overpicnum) < 0) return -1;
        if (read_int8(f, &wall->shade) < 0) return -1;
        if (read_uint8(f, &wall->pal) < 0) return -1;
        if (read_uint8(f, &wall->xrepeat) < 0) return -1;
        if (read_uint8(f, &wall->yrepeat) < 0) return -1;
        if (read_uint8(f, &wall->xpanning) < 0) return -1;
        if (read_uint8(f, &wall->ypanning) < 0) return -1;
        if (read_int16(f, &wall->lotag) < 0) return -1;
        if (read_int16(f, &wall->hitag) < 0) return -1;
        if (read_int16(f, &wall->extra) < 0) return -1;
    }
    
    return 0;
}

static int load_sprites(FILE *f, BUILD_Map *map) {
    for (int i = 0; i < map->numsprites; i++) {
        BUILD_Sprite *spr = &map->sprites[i];
        
        if (read_int32(f, &spr->x) < 0) return -1;
        if (read_int32(f, &spr->y) < 0) return -1;
        if (read_int32(f, &spr->z) < 0) return -1;
        if (read_int16(f, &spr->cstat) < 0) return -1;
        if (read_int16(f, &spr->picnum) < 0) return -1;
        if (read_int8(f, &spr->shade) < 0) return -1;
        if (read_uint8(f, &spr->pal) < 0) return -1;
        if (read_uint8(f, &spr->clipdist) < 0) return -1;
        if (read_uint8(f, &spr->filler) < 0) return -1;
        if (read_uint8(f, &spr->xrepeat) < 0) return -1;
        if (read_uint8(f, &spr->yrepeat) < 0) return -1;
        if (read_int8(f, &spr->xoffset) < 0) return -1;
        if (read_int8(f, &spr->yoffset) < 0) return -1;
        if (read_int16(f, &spr->sectnum) < 0) return -1;
        if (read_int16(f, &spr->statnum) < 0) return -1;
        if (read_int16(f, &spr->ang) < 0) return -1;
        if (read_int16(f, &spr->owner) < 0) return -1;
        if (read_int16(f, &spr->xvel) < 0) return -1;
        if (read_int16(f, &spr->yvel) < 0) return -1;
        if (read_int16(f, &spr->zvel) < 0) return -1;
        if (read_int16(f, &spr->lotag) < 0) return -1;
        if (read_int16(f, &spr->hitag) < 0) return -1;
        if (read_int16(f, &spr->extra) < 0) return -1;
    }
    
    return 0;
}

// ============================================================================
// Public Map Loading Function
// ============================================================================

BUILD_Map* build_load_map_file(const char *filename, int32_t *start_x, int32_t *start_y, 
                                int32_t *start_z, int16_t *start_ang, int16_t *start_sectnum) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "[libmod_build] Error: Cannot open map file: %s\n", filename);
        return NULL;
    }
    
    // Read map version
    int32_t version;
    if (read_int32(f, &version) < 0) {
        fprintf(stderr, "[libmod_build] Error: Cannot read map version\n");
        fclose(f);
        return NULL;
    }
    
    // Check version (support v5-v8)
    if (version < 5 || version > 8) {
        fprintf(stderr, "[libmod_build] Error: Unsupported map version %d (expected 5-8)\n", version);
        fclose(f);
        return NULL;
    }
    
    // Read starting position
    int32_t posx, posy, posz;
    int16_t ang, cursectnum;
    
    if (read_int32(f, &posx) < 0 || read_int32(f, &posy) < 0 || read_int32(f, &posz) < 0 ||
        read_int16(f, &ang) < 0 || read_int16(f, &cursectnum) < 0) {
        fprintf(stderr, "[libmod_build] Error: Cannot read starting position\n");
        fclose(f);
        return NULL;
    }
    
    // Return starting position if requested
    if (start_x) *start_x = posx;
    if (start_y) *start_y = posy;
    if (start_z) *start_z = posz;
    if (start_ang) *start_ang = ang;
    if (start_sectnum) *start_sectnum = cursectnum;
    
    // Allocate map structure
    BUILD_Map *map = (BUILD_Map *)calloc(1, sizeof(BUILD_Map));
    if (!map) {
        fprintf(stderr, "[libmod_build] Error: Cannot allocate map structure\n");
        fclose(f);
        return NULL;
    }
    
    map->version = version;
    
    // Read number of sectors
    if (read_int16(f, &map->numsectors) < 0) {
        fprintf(stderr, "[libmod_build] Error: Cannot read sector count\n");
        free(map);
        fclose(f);
        return NULL;
    }
    

    
    // Allocate and load sectors
    map->sectors = (BUILD_Sector *)calloc(map->numsectors, sizeof(BUILD_Sector));
    if (!map->sectors || load_sectors(f, map) < 0) {
        fprintf(stderr, "[libmod_build] Error: Cannot load sectors\n");
        build_free_map(map);
        fclose(f);
        return NULL;
    }
    
    // Read number of walls
    if (read_int16(f, &map->numwalls) < 0) {
        fprintf(stderr, "[libmod_build] Error: Cannot read wall count\n");
        build_free_map(map);
        fclose(f);
        return NULL;
    }
    

    
    // Allocate and load walls
    map->walls = (BUILD_Wall *)calloc(map->numwalls, sizeof(BUILD_Wall));
    if (!map->walls || load_walls(f, map) < 0) {
        fprintf(stderr, "[libmod_build] Error: Cannot load walls\n");
        build_free_map(map);
        fclose(f);
        return NULL;
    }
    
    // Read number of sprites
    if (read_int16(f, &map->numsprites) < 0) {
        fprintf(stderr, "[libmod_build] Error: Cannot read sprite count\n");
        build_free_map(map);
        fclose(f);
        return NULL;
    }
    

    
    // Allocate and load sprites
    map->sprites = (BUILD_Sprite *)calloc(map->numsprites, sizeof(BUILD_Sprite));
    if (!map->sprites || load_sprites(f, map) < 0) {
        fprintf(stderr, "[libmod_build] Error: Cannot load sprites\n");
        build_free_map(map);
        fclose(f);
        return NULL;
    }
    
    fclose(f);
    

    return map;
}
