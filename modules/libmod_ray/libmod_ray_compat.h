/* libmod_ray_compat.h - Compatibility layer for Build Engine migration
 * 
 * Helper functions to abstract hierarchy access, enabling gradual migration
 * from artificial hierarchy to Build Engine's nextsector system.
 */

#ifndef __LIBMOD_RAY_COMPAT_H
#define __LIBMOD_RAY_COMPAT_H

#include "libmod_ray.h"

/* ============================================================================
   PORTAL/NEXTSECTOR HELPERS
   ============================================================================ */

/**
 * Get connected sector through a wall's portal
 * Returns sector ID if portal exists, -1 otherwise
 * 
 * BUILD_ENGINE: wall->portal_id IS the nextsector (connected sector ID)
 * Legacy: wall->portal_id is index into portals array
 */
static inline int ray_wall_get_nextsector(RAY_Wall *wall) {
    if (!wall) return -1;
    
    // BUILD_ENGINE: portal_id directly contains connected sector ID
    // For now, return portal_id as-is (works for both systems)
    return wall->portal_id;
}

/**
 * Check if wall is a portal
 */
static inline int ray_wall_is_portal(RAY_Wall *wall) {
    return wall && wall->portal_id >= 0;
}

/* ============================================================================
   SECTOR PROPERTY HELPERS
   ============================================================================ */

/**
 * Check if sector is solid (without using hierarchy field)
 * 
 * BUILD_ENGINE: In pure Build Engine, there's no "solid" concept
 * Sectors are just spaces connected by portals
 * For now: Always return 0 (not solid)
 */
static inline int ray_sector_is_solid(RAY_Sector *sector) {
    if (!sector) return 0;
    
    // RESTORED: A sector is "solid" (rendered as a block) if:
    // 1. It has NO portals (it's isolated geometry)
    // 2. It has NO children (it's a leaf node)
    // 3. Parent check removed to handle inconsistent map data
    if (sector->num_portals == 0 && sector->num_children == 0) return 1;
    
    return 0;
}

/**
 * Get sector's parent (if nested)
 */
static inline int ray_sector_get_parent(RAY_Sector *sector) {
    if (!sector) return -1;
    return sector->parent_sector_id;
}

/**
 * Check if sector has children
 */
static inline int ray_sector_has_children(RAY_Sector *sector) {
    if (!sector) return 0;
    return (sector->num_children > 0);
}

/**
 * Get number of child sectors
 */
static inline int ray_sector_get_num_children(RAY_Sector *sector) {
    if (!sector) return 0;
    return sector->num_children;
}

/**
 * Get child sector ID by index
 */
static inline int ray_sector_get_child(RAY_Sector *sector, int index) {
    if (!sector) return -1;
    if (index < 0 || index >= sector->num_children) return -1;
    return sector->child_sector_ids[index];
}

#endif /* __LIBMOD_RAY_COMPAT_H */
