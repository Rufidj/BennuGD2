
/* Set Sprite Rotation (Angle) */
int64_t libmod_ray_set_sprite_angle(INSTANCE *my, int64_t *params) {
    if (!g_engine.initialized) return 0;
    
    int sprite_id = (int)params[0];
    float angle = *(float*)&params[1];
    
    if (sprite_id < 0 || sprite_id >= g_engine.num_sprites) return 0;
    
    // Normalize angle to [0, 2*PI] or [-PI, PI]
    // The engine uses radians internally for rotation.
    // Assuming input is in DEGREES for ease of use in BennuGD?
    // Usually standard Bennu uses Degrees * 1000.
    // But here we are using float params...
    // Let's assume input is FLOAT DEGREES for now, as consistent with other params.
    // Wait, RAY_CAM_ROTATE uses degrees?
    // libmod_ray_cam_rotate uses: g_engine.camera.rot += (float)angle * M_PI / 180.0f;
    // So yes, let's take degrees.
    
    g_engine.sprites[sprite_id].rot = angle * M_PI / 180.0f;
    
    return 1;
}
