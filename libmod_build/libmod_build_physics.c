/*
 * libmod_build_physics.c
 * Build Engine Module - Physics & Geometrics
 */

#include "libmod_build.h"
#include <math.h>
#include <stdlib.h>

// Check if point (x,y) is inside a sector (Ray Casting Algorithm for Point in Polygon)
int build_inside_sector(BUILD_Map *map, int sectnum, int32_t x, int32_t y) {
    if (sectnum < 0 || sectnum >= map->numsectors) return 0;
    
    BUILD_Sector *sec = &map->sectors[sectnum];
    if (sec->wallnum <= 0) return 0;
    
    int inside = 0;
    
    for (int i = 0; i < sec->wallnum; i++) {
        int wall_idx = sec->wallptr + i;
        if (wall_idx >= map->numwalls) break;
        
        BUILD_Wall *wal = &map->walls[wall_idx];
        BUILD_Wall *wal2 = &map->walls[wal->point2];
        
        int32_t x1 = wal->x;
        int32_t y1 = wal->y;
        int32_t x2 = wal2->x;
        int32_t y2 = wal2->y;
        
        // Ray casting algorithm - cast ray to the right
        if (((y1 > y) != (y2 > y))) {
            // Calculate intersection point
            int64_t intersect_x = x1 + (int64_t)(x2 - x1) * (y - y1) / (y2 - y1);
            if (x < intersect_x) {
                inside = !inside;
            }
        }
    }
    
    return inside;
}

// Update current sector for a moving point (x,y) starting from last_sectnum
// Returns the new sector number, or -1 if outside map
int16_t build_updatesector(BUILD_Map *map, int32_t x, int32_t y, int16_t last_sectnum) {
    if (!map) return -1;
    
    // 1. Check if still in current sector
    if (build_inside_sector(map, last_sectnum, x, y)) {
        return last_sectnum;
    }
    
    // 2. Check neighbors (portals)
    BUILD_Sector *sec = &map->sectors[last_sectnum];
    for (int i = 0; i < sec->wallnum; i++) {
        BUILD_Wall *wal = &map->walls[sec->wallptr + i];
        if (wal->nextsector >= 0) {
            if (build_inside_sector(map, wal->nextsector, x, y)) {
                return wal->nextsector;
            }
        }
    }
    
    // 3. Fallback: Brute force search (slow, but necessary if we skipped a sector)
    // Only do this if really necessary, or maybe search neighbors of neighbors?
    // For now, let's try brute force if neighbors fail (robustness)
    for (int i = 0; i < map->numsectors; i++) {
        if (build_inside_sector(map, i, x, y)) {
            return i;
        }
    }
    
    return -1; // Outside map
}
