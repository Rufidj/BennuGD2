// test_fps_weapon.prg
// Demo de arma FPS estilo Quake 3 con MD3
import "libmod_gfx";
import "libmod_input";
import "libmod_misc";
import "libmod_ray";

GLOBAL
    int screen_w = 640;
    int screen_h = 480;
    int fpg_textures;
    int fog_enabled = 0;
    int minimap_enabled = 0;
    
    // Weapon sprite
    int weapon_sprite = -1;
    int muzzle_flash_graph;
    int bullet_graph;
    
    // Flash state
    int flash_active = 0;
    int flash_timer = 0;
END


// Proceso para el arma FPS (renderizada en 3D)
PROCESS FPS_Weapon(string weapon_model, string weapon_skin)
PRIVATE
    int model;
    int skin;
    float bob_time = 0.0;
    float bob_offset_y = 0.0;
    float recoil = 0.0;
    int can_fire = 1;
    int fire_cooldown = 0;
BEGIN
    // Cargar modelo MD3
    model = RAY_LOAD_MD3(weapon_model);
    if (model == 0)
        say("Error cargando arma MD3: " + weapon_model);
        return;
    end
    
    // Cargar skin
    skin = map_load(weapon_skin);
    if (skin == 0)
        say("Warning: skin no cargado, usando textura por defecto");
        skin = 1;
    end
    
    // Crear sprite del arma
    weapon_sprite = RAY_ADD_SPRITE(
        0.0, 0.0, 0.0,  // Posición temporal
        0,   // fileID
        0,   // textureID
        64,  // width
        64,  // height
        0    // flags
    );
    
    RAY_SET_SPRITE_MD2(weapon_sprite, model, skin);
    
    LOOP
        // Obtener posición de cámara
        float cam_x = RAY_GET_CAMERA_X();
        float cam_y = RAY_GET_CAMERA_Y();
        float cam_z = RAY_GET_CAMERA_Z();
        float cam_rot = RAY_GET_CAMERA_ROT();
        
        // Offsets para posicionar el arma (ajusta estos valores)
        float offset_right = 0.4;      // Derecha
        float offset_forward = 0.5;    // Adelante (cerca de la cámara)
        float offset_down = -30.0;     // MUY ABAJO (a nivel del suelo o más)
        
        // Weapon bobbing
        bob_time += 0.1;
        bob_offset_y = sin(bob_time * 180.0) * 0.02;
        
        // Recoil
        if (recoil > 0.0)
            recoil -= 0.05;
            if (recoil < 0.0) recoil = 0.0; end
        end
        
        // Cooldown del disparo
        if (fire_cooldown > 0)
            fire_cooldown--;
        else
            can_fire = 1;
        end
        
        // Calcular posición del arma en el mundo
        float weapon_x = cam_x + cos(cam_rot) * offset_forward - sin(cam_rot) * offset_right;
        float weapon_y = cam_y + sin(cam_rot) * offset_forward + cos(cam_rot) * offset_right;
        float weapon_z = cam_z + offset_down + bob_offset_y - recoil;
        
        RAY_UPDATE_SPRITE_POSITION(weapon_sprite, weapon_x, weapon_y, weapon_z);
        RAY_SET_SPRITE_ANGLE(weapon_sprite, cam_rot * 57.2958);
        
        // Disparo
        if (mouse.left && can_fire)
            recoil = 0.15;
            can_fire = 0;
            fire_cooldown = 15;
            
            say("DISPARO! Rot (Rad): " + cam_rot);
            
            // CONVERSION CRITICA: Radiants (C) -> Miliangles (Bennu)
            // 1 Rad = 57.2957795 Deg = 57295.7795 Miliangles
            float shoot_angle = cam_rot * 57295.78;
            
            // Calcular offset del cañón
            // En Bennu cos/sin usan miliangulos
            float flash_offset_fwd = 40.0;
            float flash_offset_right = 10.0;
            
            // Nota: X=cos, Y=sin es estandar. Bennu suele usar seno/coseno normal
            float flash_x = cam_x + cos(shoot_angle) * flash_offset_fwd;
            float flash_y = cam_y + sin(shoot_angle) * flash_offset_fwd;
            float flash_z = cam_z - 5.0; 

             // Activar muzzle flash
            flash_active = 1;
            flash_timer = 5;
            
            // Muzzle Flash
            MuzzleFlash(flash_x, flash_y, flash_z);
            
            // Proyectil - Pasamos el ángulo YA CONVERTIDO a miliangulos
            Bullet(flash_x, flash_y, flash_z, shoot_angle);
        end
        
        FRAME;
    END
END

PROCESS ray_display()
BEGIN
    LOOP
        graph = RAY_RENDER(0);
        x = screen_w / 2;
        y = screen_h / 2;
        FRAME;
    END
END

// Proceso para el muzzle flash (Sprite 3D)
PROCESS MuzzleFlash(float pos_x, float pos_y, float pos_z)
PRIVATE
    int sprite_id;
    int frames_alive = 0;
BEGIN
    // Crear sprite 3D (Billboard)
    sprite_id = RAY_ADD_SPRITE(
        pos_x, pos_y, pos_z,
        0,  // fileID
        muzzle_flash_graph,
        32, 32,
        0   // flags
    );
    
    LOOP
        frames_alive++;
        if (frames_alive > 5) break; end
        FRAME;
    END
    
    RAY_REMOVE_SPRITE(sprite_id);
END

// Proceso para la bala (Sprite 3D)
// direction: En Miliangulos (Bennu standard)
PROCESS Bullet(float start_x, float start_y, float start_z, float direction)
PRIVATE
    float speed = 15.0; // Velocidad de bala
    float vel_x;
    float vel_y;
    int sprite_id;
    int frames_alive = 0;
BEGIN
    // cos(miliangulos) funciona correctamente
    vel_x = cos(direction) * speed;
    vel_y = sin(direction) * speed;
    
    // Crear sprite 3D
    sprite_id = RAY_ADD_SPRITE(
        start_x, start_y, start_z,
        0,  // fileID
        bullet_graph,
        32, 32,
        0   // flags
    );
    
    LOOP
        frames_alive++;
        if (frames_alive > 100) break; end
        
        // Mover bala
        start_x += vel_x;
        start_y += vel_y;
        
        // Actualizar posición del sprite 3D
        RAY_UPDATE_SPRITE_POSITION(sprite_id, start_x, start_y, start_z);
        
        // Opcional: Chequeo de colisión simple (distancia al suelo/pared)
        // Por ahora solo movimiento
        
        FRAME;
    END
    
    RAY_REMOVE_SPRITE(sprite_id);
END

PROCESS main()
PRIVATE
    float move_speed = 5.0;
    float rot_speed = 0.05;
    float mouse_sensitivity = 0.001;  // Sensibilidad más baja para movimiento suave
    float spawn_x, spawn_y, spawn_z;
BEGIN
    set_mode(640, 480);
    set_fps(0, 0);
    window_set_title("FPS Weapon Test - Quake 3 Style");

    fpg_textures = fpg_load("textures2.fpg");
    if (fpg_textures < 0)
        say("ERROR: No se pudo cargar textures.fpg");
        exit();
    end
    
    // Cargar gráficos de disparo
    say("Intentando cargar fire.png...");
    muzzle_flash_graph = map_load("Models/weapons/fire.png");
    say("muzzle_flash_graph = " + muzzle_flash_graph);
    if (muzzle_flash_graph == 0)
        say("ERROR: fire.png no cargado - usando gráfico temporal NARANJA");
        // Crear un gráfico temporal naranja brillante
        muzzle_flash_graph = map_new(64, 64, 32);
        map_clear(0, muzzle_flash_graph, rgb(255, 100, 0));
        say("Gráfico temporal creado, ID = " + muzzle_flash_graph);
    end
    
    say("Intentando cargar bullet.png...");
    bullet_graph = map_load("Models/weapons/bullet.png");
    say("bullet_graph = " + bullet_graph);
    if (bullet_graph == 0)
        say("ERROR: bullet.png no cargado - usando gráfico temporal AMARILLO");
        // Crear un gráfico temporal amarillo brillante
        bullet_graph = map_new(32, 32, 32);
        map_clear(0, bullet_graph, rgb(255, 255, 0));
        say("Gráfico temporal creado, ID = " + bullet_graph);
    end

    if (RAY_INIT(screen_w, screen_h, 110, 12) == 0)
        say("ERROR: No se pudo inicializar el motor");
        exit();
    end
    
    if (RAY_LOAD_MAP("test.raymap", fpg_textures) == 0)
        say("ERROR: No se pudo cargar el mapa");
        RAY_SHUTDOWN();
        exit();
    end
    
    RAY_SET_FOG(fog_enabled, 150, 150, 180, 512.0, 1280.0);
    RAY_SET_DRAW_MINIMAP(minimap_enabled);
    // RAY_SET_TEXTURE_QUALITY(1); // Disabled - causes vertical artifacts in column-based raycaster
    
    // Spawning logic
    spawn_x = RAY_GET_FLAG_X(1);
    spawn_y = RAY_GET_FLAG_Y(1);
    spawn_z = RAY_GET_FLAG_Z(1);
    
    if (spawn_x == 0 && spawn_y == 0)
        spawn_x = 500.0; spawn_y = 384.0;
    //    spawn_z = RAY_GET_FLOOR_HEIGHT(spawn_x, spawn_y);
    end
    
    say("Cargando arma Railgun...");
    
    // Cargar arma (ajusta la ruta según donde tengas los archivos)
    FPS_Weapon("Models/weapons/railgun.md3", "Models/weapons/railgun1.jpg");
    
    ray_display();
    
    LOOP
        // Movimiento
        if (key(_w)) RAY_MOVE_FORWARD(move_speed); end
        if (key(_s)) RAY_MOVE_BACKWARD(move_speed); end
        if (key(_a)) RAY_STRAFE_LEFT(move_speed); end
        if (key(_d)) RAY_STRAFE_RIGHT(move_speed); end
        
        // Rotación con mouse (SUAVIZADA)
        int mouse_delta_x = mouse.x - screen_w/2;
        RAY_ROTATE(mouse_delta_x * mouse_sensitivity);
        mouse.x = screen_w / 2;
        
        // Pitch con teclado
        if (key(_up)) RAY_LOOK_UP_DOWN(0.02); end
        if (key(_down)) RAY_LOOK_UP_DOWN(-0.02); end
        
        // Info
        write(0, 10, 10, 0, "FPS: " + frame_info.fps);
        write(0, 10, 30, 0, "Click: Disparar");
        write(0, 10, 50, 0, "WASD: Movimiento");
        write(0, 10, 70, 0, "Mouse: Mirar (Sens: " + mouse_sensitivity + ")");
        
        // Ajustar sensibilidad con +/-
        if (key(_plus) || key(_equals))
            mouse_sensitivity += 0.0005;
            if (mouse_sensitivity > 0.01) mouse_sensitivity = 0.01; end
        end
        if (key(_minus))
            mouse_sensitivity -= 0.0005;
            if (mouse_sensitivity < 0.0005) mouse_sensitivity = 0.0005; end
        end
        if(key(_esc))LET_ME_ALONE();exit("",0);END
        
        // Dibujar muzzle flash si está activo
        if (flash_active)
            say("Dibujando flash!");
            map_put(0, 0, 0, muzzle_flash_graph, 480, 400);
            flash_timer--;
            if (flash_timer <= 0)
                flash_active = 0;
            end
        end
        
        FRAME;
    END
END
