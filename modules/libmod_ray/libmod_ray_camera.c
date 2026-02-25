#include "libmod_ray_camera.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// JSON parsing (simple implementation)
#include <ctype.h>

#define MAX_CAMERA_PATHS 16

static CameraPath g_camera_paths[MAX_CAMERA_PATHS];
static int g_active_path = -1;
static float g_current_time = 0.0f;
static int g_is_playing = 0;
static int g_is_paused = 0;

// Easing functions
float camera_ease(float t, int ease_type) {
    switch (ease_type) {
        case 0: return t; // Linear
        case 1: return t * t; // Ease In
        case 2: return t * (2.0f - t); // Ease Out
        case 3: return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t; // Ease In/Out
        case 4: return t * t * t; // Ease In Cubic
        case 5: { float f = t - 1.0f; return f * f * f + 1.0f; } // Ease Out Cubic
        case 6: return t < 0.5f ? 4.0f * t * t * t : 1.0f + (t - 1.0f) * (2.0f * (t - 2.0f)) * (2.0f * (t - 1.0f)); // Ease In/Out Cubic
        default: return t;
    }
}

// Catmull-Rom interpolation
float camera_interpolate_catmull_rom(float p0, float p1, float p2, float p3, float t) {
    float t2 = t * t;
    float t3 = t2 * t;
    
    return 0.5f * (
        (2.0f * p1) +
        (-p0 + p2) * t +
        (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
        (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
    );
}

// Linear interpolation
static float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

// Find keyframe indices for given time
static void find_keyframe_indices(const CameraPath *path, float time, int *i1, int *i2) {
    *i1 = 0;
    *i2 = 0;
    
    if (path->num_keyframes == 0) return;
    if (path->num_keyframes == 1) return;
    
    for (int i = 0; i < path->num_keyframes - 1; i++) {
        if (time >= path->keyframes[i].time && time <= path->keyframes[i + 1].time) {
            *i1 = i;
            *i2 = i + 1;
            return;
        }
    }
    
    // Time is beyond last keyframe
    *i1 = path->num_keyframes - 1;
    *i2 = path->num_keyframes - 1;
}

// Interpolate camera state at given time
void camera_interpolate_keyframe(const CameraPath *path, float time, CameraState *state) {
    // FORCE DEBUG PRINT to ensure we are running this code
    static int global_debug_counter = 0;
    if (global_debug_counter++ % 60 == 0) {
        fprintf(stderr, "DEBUG: interpolate called t=%.2f num=%d type=%d\n", 
                time, path->num_keyframes, path->interpolation_type);
        fflush(stderr);
    }

    if (path->num_keyframes == 0) return;
    
    if (path->num_keyframes == 1) {
        const CameraKeyframe *kf = &path->keyframes[0];
        state->x = kf->x; state->y = kf->y; state->z = kf->z;
        state->yaw = kf->yaw; state->pitch = kf->pitch; state->roll = kf->roll;
        state->fov = kf->fov;
        return;
    }
    
    // Handle loop
    if (path->loop && time > path->total_duration) {
        time = fmodf(time, path->total_duration);
    }
    
    // Clamp time checks - DEBUG
    if (time <= path->keyframes[0].time) {
        if (global_debug_counter % 60 == 1) fprintf(stderr, "DEBUG: Clamped BEFORE first keyframe\n");
        const CameraKeyframe *kf = &path->keyframes[0];
        state->x = kf->x; state->y = kf->y; state->z = kf->z;
        state->yaw = kf->yaw; state->pitch = kf->pitch; state->roll = kf->roll;
        state->fov = kf->fov;
        return;
    }
    
    if (time >= path->keyframes[path->num_keyframes - 1].time) {
        if (global_debug_counter % 60 == 1) fprintf(stderr, "DEBUG: Clamped AFTER last keyframe\n");
        const CameraKeyframe *kf = &path->keyframes[path->num_keyframes - 1];
        state->x = kf->x; state->y = kf->y; state->z = kf->z;
        state->yaw = kf->yaw; state->pitch = kf->pitch; state->roll = kf->roll;
        state->fov = kf->fov;
        return;
    }
    
    // Find surrounding keyframes
    int i1, i2;
    find_keyframe_indices(path, time, &i1, &i2);
    
    const CameraKeyframe *kf1 = &path->keyframes[i1];
    const CameraKeyframe *kf2 = &path->keyframes[i2];
    
    // Calculate interpolation factor
    float segment_duration = kf2->time - kf1->time;
    if (segment_duration <= 0.0f) {
        state->x = kf1->x; state->y = kf1->y; state->z = kf1->z;
        state->yaw = kf1->yaw; state->pitch = kf1->pitch; state->roll = kf1->roll;
        state->fov = kf1->fov;
        return;
    }
    
    float t = (time - kf1->time) / segment_duration;
    
    // Apply easing
    t = camera_ease(t, kf1->ease_out);
    
    // Apply speed multiplier
    t *= kf2->speed_multiplier;
    if (t > 1.0f) t = 1.0f;
    if (t < 0.0f) t = 0.0f;
    
    if (global_debug_counter % 60 == 1) {
         fprintf(stderr, "DEBUG: Indices %d->%d t=%.2f\n", i1, i2, t);
         fprintf(stderr, "DEBUG: P1(%.1f,%.1f) P2(%.1f,%.1f)\n", 
                 kf1->x, kf1->y, kf2->x, kf2->y);
    }
    
    // Interpolate based on type - FORCE CATMULL FOR DEBUG IF TYPE IS 0
    int type = path->interpolation_type;
    if (type == 0) type = 1; // Temporary force to Catmull
    
    if (type == 1) { // Catmull-Rom
        int i0 = (i1 > 0) ? i1 - 1 : i1;
        int i3 = (i2 < path->num_keyframes - 1) ? i2 + 1 : i2;
        
        const CameraKeyframe *kf0 = &path->keyframes[i0];
        const CameraKeyframe *kf3 = &path->keyframes[i3];
        
        state->x = camera_interpolate_catmull_rom(kf0->x, kf1->x, kf2->x, kf3->x, t);
        state->y = camera_interpolate_catmull_rom(kf0->y, kf1->y, kf2->y, kf3->y, t);
        state->z = camera_interpolate_catmull_rom(kf0->z, kf1->z, kf2->z, kf3->z, t);
        state->yaw = camera_interpolate_catmull_rom(kf0->yaw, kf1->yaw, kf2->yaw, kf3->yaw, t);
        state->pitch = camera_interpolate_catmull_rom(kf0->pitch, kf1->pitch, kf2->pitch, kf3->pitch, t);
        state->roll = camera_interpolate_catmull_rom(kf0->roll, kf1->roll, kf2->roll, kf3->roll, t);
        state->fov = camera_interpolate_catmull_rom(kf0->fov, kf1->fov, kf2->fov, kf3->fov, t);
        
        if (global_debug_counter % 60 == 1) {
             fprintf(stderr, "DEBUG: Result Catmull (%.1f, %.1f)\n", state->x, state->y);
        }
    } else { // Linear
        state->x = lerp(kf1->x, kf2->x, t);
        state->y = lerp(kf1->y, kf2->y, t);
        state->z = lerp(kf1->z, kf2->z, t);
        state->yaw = lerp(kf1->yaw, kf2->yaw, t);
        state->pitch = lerp(kf1->pitch, kf2->pitch, t);
        state->roll = lerp(kf1->roll, kf2->roll, t);
        state->fov = lerp(kf1->fov, kf2->fov, t);
        
        if (global_debug_counter % 60 == 1) {
             fprintf(stderr, "DEBUG: Result Linear (%.1f, %.1f)\n", state->x, state->y);
        }
    }
}

// Simple JSON parser helpers
static char* find_json_value(const char *json, const char *key) {
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);
    char *pos = strstr(json, search);
    if (!pos) return NULL;
    
    pos = strchr(pos + strlen(search), ':');
    if (!pos) return NULL;
    
    // Skip whitespace
    pos++;
    while (*pos && isspace(*pos)) pos++;
    
    return pos;
}

static float parse_json_float(const char *json, const char *key, float default_val) {
    char *pos = find_json_value(json, key);
    if (!pos) return default_val;
    return atof(pos);
}

static int parse_json_int(const char *json, const char *key, int default_val) {
    char *pos = find_json_value(json, key);
    if (!pos) return default_val;
    return atoi(pos);
}

static int parse_json_bool(const char *json, const char *key, int default_val) {
    char *pos = find_json_value(json, key);
    if (!pos) return default_val;
    return (strncmp(pos, "true", 4) == 0) ? 1 : 0;
}

static void parse_json_string(const char *json, const char *key, char *out, int max_len) {
    char *pos = find_json_value(json, key);
    if (!pos || *pos != '"') {
        out[0] = '\0';
        return;
    }
    
    pos++; // Skip opening quote
    int i = 0;
    while (*pos && *pos != '"' && i < max_len - 1) {
        out[i++] = *pos++;
    }
    out[i] = '\0';
}

static int ease_string_to_type(const char *str) {
    if (strcmp(str, "linear") == 0) return 0;
    if (strcmp(str, "ease_in") == 0) return 1;
    if (strcmp(str, "ease_out") == 0) return 2;
    if (strcmp(str, "ease_in_out") == 0) return 3;
    if (strcmp(str, "ease_in_cubic") == 0) return 4;
    if (strcmp(str, "ease_out_cubic") == 0) return 5;
    if (strcmp(str, "ease_in_out_cubic") == 0) return 6;
    return 0;
}

// Load camera path from .campath JSON file
int ray_camera_load_path(const char *filename) {
    // Find free slot
    int slot = -1;
    for (int i = 0; i < MAX_CAMERA_PATHS; i++) {
        if (g_camera_paths[i].num_keyframes == 0) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        fprintf(stderr, "ray_camera_load_path: No free slots\n");
        return -1;
    }
    
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "ray_camera_load_path: Failed to open %s\n", filename);
        return -1;
    }
    
    // Read entire file
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *json = (char*)malloc(fsize + 1);
    fread(json, 1, fsize, f);
    json[fsize] = '\0';
    fclose(f);
    
    // Parse path data
    CameraPath *path = &g_camera_paths[slot];
    
    parse_json_string(json, "name", path->name, sizeof(path->name));
    path->loop = parse_json_bool(json, "loop", 0);
    path->interpolation_type = 1; // FORCE CATMULL-ROM
    path->total_duration = parse_json_float(json, "totalDuration", 0.0f);
    
    // Find keyframes array
    char *keyframes_start = strstr(json, "\"keyframes\"");
    if (!keyframes_start) {
        fprintf(stderr, "ray_camera_load_path: No keyframes found\n");
        free(json);
        return -1;
    }
    
    keyframes_start = strchr(keyframes_start, '[');
    if (!keyframes_start) {
        fprintf(stderr, "ray_camera_load_path: Invalid keyframes array\n");
        free(json);
        return -1;
    }
    
    // Count keyframes
    int num_keyframes = 0;
    char *p = keyframes_start;
    while ((p = strchr(p + 1, '{')) != NULL) {
        // Find matching closing brace using brace counting
        char *end = p;
        int brace_count = 0;
        
        while (*end) {
            if (*end == '{') brace_count++;
            else if (*end == '}') {
                brace_count--;
                if (brace_count == 0) break;
            }
            end++;
        }
        
        if (!*end) break;
        
        // Only count if this brace block is directly inside the keyframes array
        // (This is a heuristic, but since we advance p to end, we skip nested objects implicitly)
        
        if (end < strstr(keyframes_start, "]")) {
            num_keyframes++;
            p = end; // Skip to end of this object
        } else {
            break;
        }
    }
    
    if (num_keyframes == 0) {
        fprintf(stderr, "ray_camera_load_path: No keyframes parsed\n");
        free(json);
        return -1;
    }
    
    // Allocate keyframes
    path->keyframes = (CameraKeyframe*)malloc(sizeof(CameraKeyframe) * num_keyframes);
    memset(path->keyframes, 0, sizeof(CameraKeyframe) * num_keyframes); // Initialize to zero
    path->num_keyframes = num_keyframes;
    
    // Parse each keyframe
    p = keyframes_start;
    int kf_idx = 0;
    while ((p = strchr(p + 1, '{')) != NULL && kf_idx < num_keyframes) {
        // Find matching closing brace
        char *end = p;
        int brace_count = 0;
        
        while (*end) {
            if (*end == '{') brace_count++;
            else if (*end == '}') {
                brace_count--;
                if (brace_count == 0) break;
            }
            end++;
        }
        
        if (!*end) break; // Malformed JSON
        
        // Extract this keyframe's JSON
        int kf_len = end - p + 1;
        char *kf_json = (char*)malloc(kf_len + 1);
        memcpy(kf_json, p, kf_len);
        kf_json[kf_len] = '\0';
        
        CameraKeyframe *kf = &path->keyframes[kf_idx];
        
        // Find position object
        char *pos_start = strstr(kf_json, "\"position\"");
        if (pos_start) {
            pos_start = strchr(pos_start, '{');
            if (pos_start) {
                char *pos_end = strchr(pos_start, '}');
                if (pos_end) {
                    int pos_len = pos_end - pos_start + 1;
                    char *pos_json = (char*)malloc(pos_len + 1);
                    memcpy(pos_json, pos_start, pos_len);
                    pos_json[pos_len] = '\0';
                    
                    kf->x = parse_json_float(pos_json, "x", 0.0f);
                    kf->y = parse_json_float(pos_json, "y", 0.0f);
                    kf->z = parse_json_float(pos_json, "z", 64.0f);
                    
                    free(pos_json);
                }
            }
        } else {
            fprintf(stderr, "WARNING: Keyframe %d has no 'position' object\n", kf_idx);
        }
        
        // Find rotation object
        char *rot_start = strstr(kf_json, "\"rotation\"");
        if (rot_start) {
            rot_start = strchr(rot_start, '{');
            if (rot_start) {
                char *rot_end = strchr(rot_start, '}');
                if (rot_end) {
                    int rot_len = rot_end - rot_start + 1;
                    char *rot_json = (char*)malloc(rot_len + 1);
                    memcpy(rot_json, rot_start, rot_len);
                    rot_json[rot_len] = '\0';
                    
                    // Convert degrees to radians
                    kf->yaw = parse_json_float(rot_json, "yaw", 0.0f) * M_PI / 180.0f;
                    kf->pitch = parse_json_float(rot_json, "pitch", 0.0f) * M_PI / 180.0f;
                    kf->roll = parse_json_float(rot_json, "roll", 0.0f) * M_PI / 180.0f;
                    
                    free(rot_json);
                }
            }
        }
        
        kf->fov = parse_json_float(kf_json, "fov", 90.0f);
        kf->time = parse_json_float(kf_json, "time", 0.0f);
        kf->duration = parse_json_float(kf_json, "duration", 0.0f);
        kf->speed_multiplier = parse_json_float(kf_json, "speedMultiplier", 1.0f);
        
        // Parse easing
        char ease_str[32];
        parse_json_string(kf_json, "easeIn", ease_str, sizeof(ease_str));
        kf->ease_in = ease_string_to_type(ease_str);
        
        parse_json_string(kf_json, "easeOut", ease_str, sizeof(ease_str));
        kf->ease_out = ease_string_to_type(ease_str);
        
        free(kf_json);
        kf_idx++;
        p = end;
    }
    
    free(json);
    
    fprintf(stderr, "ray_camera_load_path: Loaded '%s' with %d keyframes\n", 
            path->name, path->num_keyframes);
    
    // Debug: Print first 3 keyframes
    fprintf(stderr, "DEBUG: First keyframes:\n");
    for (int i = 0; i < path->num_keyframes && i < 3; i++) {
        CameraKeyframe *kf = &path->keyframes[i];
        fprintf(stderr, "  KF%d: pos=(%.1f,%.1f,%.1f) yaw=%.2f time=%.2f\n",
                i, kf->x, kf->y, kf->z, kf->yaw, kf->time);
    }
    
    return slot;
}

void ray_camera_free_path(int path_id) {
    if (path_id < 0 || path_id >= MAX_CAMERA_PATHS) return;
    
    if (g_camera_paths[path_id].keyframes) {
        free(g_camera_paths[path_id].keyframes);
        g_camera_paths[path_id].keyframes = NULL;
    }
    
    g_camera_paths[path_id].num_keyframes = 0;
}

void ray_camera_play_path(int path_id) {
    if (path_id < 0 || path_id >= MAX_CAMERA_PATHS) return;
    if (g_camera_paths[path_id].num_keyframes == 0) return;
    
    g_active_path = path_id;
    g_current_time = 0.0f;
    g_is_playing = 1;
    g_is_paused = 0;
}

void ray_camera_stop_path() {
    g_is_playing = 0;
    g_is_paused = 0;
    g_current_time = 0.0f;
}

void ray_camera_pause_path() {
    g_is_paused = 1;
}

void ray_camera_resume_path() {
    g_is_paused = 0;
}

int ray_camera_is_playing() {
    return g_is_playing && !g_is_paused;
}

float ray_camera_get_time() {
    return g_current_time;
}

void ray_camera_set_time(float time) {
    g_current_time = time;
}

void ray_camera_update(float delta_time) {
    if (!g_is_playing || g_is_paused) return;
    if (g_active_path < 0 || g_active_path >= MAX_CAMERA_PATHS) return;
    
    CameraPath *path = &g_camera_paths[g_active_path];
    if (path->num_keyframes == 0) return;
    
    g_current_time += delta_time;
    
    // Debug output every 60 frames
    static int debug_counter = 0;
    if (debug_counter++ % 60 == 0) {
        fprintf(stderr, "CAMERA DEBUG: time=%.2f/%.2f delta=%.4f playing=%d\n", 
                g_current_time, path->total_duration, delta_time, g_is_playing);
    }
    
    // Handle loop or stop
    if (g_current_time > path->total_duration) {
        if (path->loop) {
            g_current_time = fmodf(g_current_time, path->total_duration);
        } else {
            g_is_playing = 0;
            g_current_time = path->total_duration;
        }
    }
}

void ray_camera_get_state(CameraState *state) {
    if (g_active_path < 0 || g_active_path >= MAX_CAMERA_PATHS) {
        state->x = state->y = state->z = 0.0f;
        state->yaw = state->pitch = state->roll = 0.0f;
        state->fov = 90.0f;
        return;
    }
    
    camera_interpolate_keyframe(&g_camera_paths[g_active_path], g_current_time, state);
}

// Helper function to create a simple test path
int ray_camera_create_simple_path(int num_keyframes, const CameraKeyframe *keyframes) {
    int slot = -1;
    for (int i = 0; i < MAX_CAMERA_PATHS; i++) {
        if (g_camera_paths[i].num_keyframes == 0) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) return -1;
    
    CameraPath *path = &g_camera_paths[slot];
    path->num_keyframes = num_keyframes;
    path->keyframes = (CameraKeyframe*)malloc(sizeof(CameraKeyframe) * num_keyframes);
    memcpy(path->keyframes, keyframes, sizeof(CameraKeyframe) * num_keyframes);
    path->interpolation_type = 1; // Catmull-Rom
    path->loop = 0;
    
    // Calculate total duration
    path->total_duration = keyframes[num_keyframes - 1].time + keyframes[num_keyframes - 1].duration;
    
    strcpy(path->name, "Simple Path");
    
    return slot;
}
