/*
 * libmod_ray_slopes.c - Build Engine Style Slopes
 * 
 * NOTE: Slopes have been DEPRECATED and replaced by MD3 models.
 * These functions are kept as stubs to maintain API compatibility.
 */

#include "libmod_ray.h"
#include <math.h>

/* ============================================================================
   SLOPE HEIGHT CALCULATION (STUBBED - Returns flat geometry)
   ============================================================================ */

float calculate_slope_z_build_style(const RAY_Sector *sector, float x, float y, int is_ceiling) {
    // DEPRECATED: Slopes replaced by MD3 models
    // Return flat floor/ceiling height
    return is_ceiling ? sector->ceiling_z : sector->floor_z;
}

/* ============================================================================
   PUBLIC API (STUBBED)
   ============================================================================ */

int ray_sector_has_floor_slope(const RAY_Sector *sector) {
    // DEPRECATED: Always return false (no slopes)
    return 0;
}

int ray_sector_has_ceiling_slope(const RAY_Sector *sector) {
    // DEPRECATED: Always return false (no slopes)
    return 0;
}

float ray_get_floor_height_at(const RAY_Sector *sector, float x, float y) {
    // DEPRECATED: Return flat floor height
    if (!sector) return 0.0f;
    return sector->floor_z;
}

float ray_get_ceiling_height_at(const RAY_Sector *sector, float x, float y) {
    // DEPRECATED: Return flat ceiling height
    if (!sector) return 0.0f;
    return sector->ceiling_z;
}
