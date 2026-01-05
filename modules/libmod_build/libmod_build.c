/*
 * libmod_build.c
 * Build Engine Module for BennuGD2 - Main Implementation
 */

#include "libmod_build.h"
#include "libmod_build_exports.h"
#include "xstrings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Global variables
BUILD_Map *g_build_map = NULL;
BUILD_Camera g_build_camera = {0};

// Sine table (Build Engine uses 2048 angles)
static int16_t sintable[2048];
static int initialized = 0;

// ============================================================================
// Module Initialization and Finalization
// ============================================================================

void __bgdexport(libmod_build, module_initialize)() {

    
    // Initialize sine table
    build_init_tables();
    
    // Initialize camera to default values
    g_build_camera.x = 0;
    g_build_camera.y = 0;
    g_build_camera.z = 0;
    g_build_camera.ang = 0;
    g_build_camera.horiz = 100;  // 100 = looking straight
    g_build_camera.cursectnum = 0;
    
    initialized = 1;
}

void __bgdexport(libmod_build, module_finalize)() {

    
    // Free map if loaded
    if (g_build_map) {
        build_free_map(g_build_map);
        g_build_map = NULL;
    }
    
    // Cleanup render buffer
    build_cleanup_render();
    
    // Cleanup tiles
    build_free_tiles();
    
    initialized = 0;
}

// ============================================================================
// Internal Functions
// ============================================================================

void build_init_tables(void) {
    // Initialize sine table (Build Engine uses 2048 angles, multiplied by 16383)
    for (int i = 0; i < 2048; i++) {
        sintable[i] = (int16_t)(sin(i * M_PI / 1024.0) * 16383.0);
    }
}

void build_free_map(BUILD_Map *map) {
    if (!map) return;
    
    if (map->sectors) free(map->sectors);
    if (map->walls) free(map->walls);
    if (map->sprites) free(map->sprites);
    free(map);
}

// ============================================================================
// API Functions - Map Loading
// ============================================================================

int64_t libmod_build_load_map(INSTANCE *my, int64_t *params) {
    // Get string from BennuGD2 (like WLD module does)
    const char *filename = string_get(params[0]);
    
    if (!initialized) {
        fprintf(stderr, "[libmod_build] Error: Module not initialized\n");
        string_discard(params[0]);
        return -1;
    }
    

    
    // Free previous map if exists
    if (g_build_map) {
        build_free_map(g_build_map);
        g_build_map = NULL;
    }
    
    // Load the map file
    int32_t start_x, start_y, start_z;
    int16_t start_ang, start_sectnum;
    
    g_build_map = build_load_map_file(filename, &start_x, &start_y, &start_z, 
                                       &start_ang, &start_sectnum);
    
    if (!g_build_map) {
        fprintf(stderr, "[libmod_build] Error: Failed to load map\n");
        string_discard(params[0]);
        return -1;
    }
    
    // Set camera to starting position
    g_build_camera.x = start_x;
    g_build_camera.y = start_y;
    g_build_camera.z = start_z;
    g_build_camera.ang = start_ang;
    g_build_camera.horiz = 100;  // Default horizon
    g_build_camera.cursectnum = start_sectnum;
    

    
    // Discard string (cleanup)
    string_discard(params[0]);
    return 0;
}

// ============================================================================
// API Functions - Texture Loading
// ============================================================================

int64_t libmod_build_load_palette(INSTANCE *my, int64_t *params) {
    const char *filename = string_get(params[0]);
    
    if (!initialized) {
        fprintf(stderr, "[libmod_build] Error: Module not initialized\n");
        string_discard(params[0]);
        return -1;
    }
    
    int result = build_load_palette(filename);
    string_discard(params[0]);
    return result;
}

int64_t libmod_build_load_art(INSTANCE *my, int64_t *params) {
    const char *filename = string_get(params[0]);
    
    if (!initialized) {
        fprintf(stderr, "[libmod_build] Error: Module not initialized\n");
        string_discard(params[0]);
        return -1;
    }
    
    int result = build_load_art(filename);
    string_discard(params[0]);
    return result;
}

// ============================================================================
// API Functions - Rendering
// ============================================================================

int64_t libmod_build_render(INSTANCE *my, int64_t *params) {
    if (!initialized) {
        fprintf(stderr, "[libmod_build] Error: Module not initialized\n");
        return -1;
    }
    
    if (!g_build_map) {
        fprintf(stderr, "[libmod_build] Error: No map loaded\n");
        return -1;
    }
    
    // Get render dimensions from parameters
    int width = (int)params[0];
    int height = (int)params[1];
    
    // Call rendering function with dimensions
    build_render_frame(g_build_map, &g_build_camera, width, height);
    
    // Return render buffer code (like WLD module)
    return build_get_render_buffer_code();
}

// ============================================================================
// API Functions - Camera
// ============================================================================

int64_t libmod_build_set_camera(INSTANCE *my, int64_t *params) {
    if (!initialized) {
        fprintf(stderr, "[libmod_build] Error: Module not initialized\n");
        return -1;
    }
    
    g_build_camera.x = (int32_t)params[0];
    g_build_camera.y = (int32_t)params[1];
    g_build_camera.z = (int32_t)params[2];
    g_build_camera.ang = (int16_t)params[3];
    g_build_camera.horiz = (int32_t)params[4];
    g_build_camera.cursectnum = (int16_t)params[5];
    
    return 0;
}

// ============================================================================
// API Functions - Movement
// ============================================================================

int64_t libmod_build_move_forward(INSTANCE *my, int64_t *params) {
    if (!initialized) return -1;
    
    int32_t speed = (int32_t)params[0];
    
    // Calculate movement vector based on camera angle
    // Build Engine: ang 0 = East, 512 = North, 1024 = West, 1536 = South
    int ang = g_build_camera.ang & 2047;
    int32_t dx = (sintable[(ang + 512) & 2047] * speed) >> 14;
    int32_t dy = (sintable[ang] * speed) >> 14;
    
    // TODO: Use clipmove for collision detection
    g_build_camera.x += dx;
    g_build_camera.y += dy;
    
    // Update sector
    int16_t new_sect = build_updatesector(g_build_map, g_build_camera.x, g_build_camera.y, g_build_camera.cursectnum);
    if (new_sect >= 0) {
        g_build_camera.cursectnum = new_sect;
    }
    
    return 0;
}

int64_t libmod_build_move_backward(INSTANCE *my, int64_t *params) {
    if (!initialized) return -1;
    
    int32_t speed = (int32_t)params[0];
    
    int ang = g_build_camera.ang & 2047;
    int32_t dx = (sintable[(ang + 512) & 2047] * speed) >> 14;
    int32_t dy = (sintable[ang] * speed) >> 14;
    
    g_build_camera.x -= dx;
    g_build_camera.y -= dy;
    
    // Update sector
    int16_t new_sect = build_updatesector(g_build_map, g_build_camera.x, g_build_camera.y, g_build_camera.cursectnum);
    if (new_sect >= 0) {
        g_build_camera.cursectnum = new_sect;
    }
    
    return 0;
}

int64_t libmod_build_strafe_left(INSTANCE *my, int64_t *params) {
    if (!initialized) return -1;
    
    int32_t speed = (int32_t)params[0];
    
    // Strafe is perpendicular to forward direction
    int ang = g_build_camera.ang & 2047;
    int32_t dx = (sintable[ang] * speed) >> 14;
    int32_t dy = -(sintable[(ang + 512) & 2047] * speed) >> 14;
    
    g_build_camera.x += dx;
    g_build_camera.y += dy;
    
    // Update sector
    int16_t new_sect = build_updatesector(g_build_map, g_build_camera.x, g_build_camera.y, g_build_camera.cursectnum);
    if (new_sect >= 0) {
        g_build_camera.cursectnum = new_sect;
    }
    
    return 0;
}

int64_t libmod_build_strafe_right(INSTANCE *my, int64_t *params) {
    if (!initialized) return -1;
    
    int32_t speed = (int32_t)params[0];
    
    int ang = g_build_camera.ang & 2047;
    int32_t dx = (sintable[ang] * speed) >> 14;
    int32_t dy = -(sintable[(ang + 512) & 2047] * speed) >> 14;
    
    g_build_camera.x -= dx;
    g_build_camera.y -= dy;
    
    // Update sector
    int16_t new_sect = build_updatesector(g_build_map, g_build_camera.x, g_build_camera.y, g_build_camera.cursectnum);
    if (new_sect >= 0) {
        g_build_camera.cursectnum = new_sect;
    }
    
    return 0;
}

int64_t libmod_build_move_vertical(INSTANCE *my, int64_t *params) {
    if (!initialized) return -1;
    
    int32_t speed = (int32_t)params[0];
    
    // Z axis in Build Engine: negative is UP, positive is DOWN
    // Unit is usually roughly 16 per pixel? 
    // Let's just add the speed directly to Z.
    g_build_camera.z += speed;
    
    return 0;
}

// ============================================================================
// API Functions - Look
// ============================================================================

int64_t libmod_build_look_horizontal(INSTANCE *my, int64_t *params) {
    if (!initialized) return -1;
    
    int32_t delta = (int32_t)params[0];
    
    // Update angle and wrap to 0-2047 range
    g_build_camera.ang = (g_build_camera.ang + delta) & 2047;
    
    return 0;
}

int64_t libmod_build_look_vertical(INSTANCE *my, int64_t *params) {
    if (!initialized) return -1;
    
    int32_t delta = (int32_t)params[0];
    
    // Update horizon (clamp to reasonable values)
    g_build_camera.horiz += delta;
    
    // Clamp horizon (typical range: 0-200, 100 = straight)
    if (g_build_camera.horiz < 0) g_build_camera.horiz = 0;
    if (g_build_camera.horiz > 200) g_build_camera.horiz = 200;
    
    return 0;
}
