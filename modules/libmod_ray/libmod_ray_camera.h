#ifndef LIBMOD_RAY_CAMERA_H
#define LIBMOD_RAY_CAMERA_H

#ifdef __cplusplus
extern "C" {
#endif

// Camera keyframe structure
typedef struct {
    float x, y, z;           // Position
    float yaw, pitch, roll;  // Rotation (radians)
    float fov;               // Field of view
    float time;              // Time in seconds
    float duration;          // Pause duration
    float speed_multiplier;  // Speed multiplier
    int ease_in;             // Ease in type
    int ease_out;            // Ease out type
} CameraKeyframe;

// Camera path structure
typedef struct {
    char name[256];
    int num_keyframes;
    CameraKeyframe *keyframes;
    int interpolation_type;  // 0=linear, 1=catmull-rom
    int loop;
    float total_duration;
} CameraPath;

// Camera state
typedef struct {
    float x, y, z;
    float yaw, pitch, roll;
    float fov;
} CameraState;

// API Functions
int ray_camera_load_path(const char *filename);
void ray_camera_free_path(int path_id);
void ray_camera_play_path(int path_id);
void ray_camera_stop_path();
void ray_camera_pause_path();
void ray_camera_resume_path();
int ray_camera_is_playing();
float ray_camera_get_time();
void ray_camera_set_time(float time);
void ray_camera_update(float delta_time);
void ray_camera_get_state(CameraState *state);

// Internal functions
float camera_interpolate_catmull_rom(float p0, float p1, float p2, float p3, float t);
float camera_ease(float t, int ease_type);
void camera_interpolate_keyframe(const CameraPath *path, float time, CameraState *state);

#ifdef __cplusplus
}
#endif

#endif // LIBMOD_RAY_CAMERA_H
