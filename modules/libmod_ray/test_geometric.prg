// test_geometric.prg
// Programa de prueba para el motor geométrico (sectores Build Engine-style)
import "libmod_gfx";
import "libmod_input";
import "libmod_misc";
import "libmod_ray";

GLOBAL
    int screen_w = 640;
    int screen_h = 480;
    int fpg_textures;
    int fog_enabled = 0;
    int minimap_enabled = 1;
    int pitch;
    
    // IDs de sprites para jerarquía MD3
    int sprite_lower = -1;
    int sprite_upper = -1;
    int sprite_head = -1;
END

// Proceso para cargar y mostrar un modelo MD2
PROCESS MD2Model(float mx, float my, float mz)
PRIVATE
    int model;
    int skin;
    int sprite_id;
    float rot_angle = 0.0;
BEGIN
    model = RAY_LOAD_MD2("Models/md2/tris.md2");
    if (model == 0)
        say("Error cargando Models/md2/tris.md2");
        return;
    end
    
    skin = map_load("Models/md2/base.pcx");
    if (skin == 0)
        say("Warning: base.pcx no cargado, usando textura 1 del FPG");
        skin = 1;
    end
    
    sprite_id = RAY_ADD_SPRITE(mx, my, mz, 0, 96, 96);
    RAY_SET_SPRITE_MD2(sprite_id, model, skin);
    
    LOOP
        rot_angle += 2.0;
        if (rot_angle > 360.0) rot_angle -= 360.0; end
        RAY_SET_SPRITE_ANGLE(sprite_id, rot_angle);
        FRAME;
    END
END

// Proceso MD3 - Lower (Base, sin padre)
PROCESS MD3_Lower(string filename, string skin_filename, float mx, float my, float mz)
PRIVATE
    int model;
    int skin;
    float rot_angle = 0.0;
    
    // Animación
    int current_frame = 0;
    int next_frame = 1;
    float anim_speed = 0.1;  // Velocidad de animación
    float interp = 0.0;
    int max_frames = 191;    // Lower tiene 191 frames
BEGIN
    model = RAY_LOAD_MD3(filename);
    if (model == 0)
        say("Error cargando MD3: " + filename);
        return;
    end
    
    skin = map_load(skin_filename);
    if (skin == 0)
        say("Warning: skin no cargado, usando fallback");
        skin = 1;
    end
    
    sprite_lower = RAY_ADD_SPRITE(mx, my, mz, 0, 96, 96);
    RAY_SET_SPRITE_MD2(sprite_lower, model, skin);
    
    LOOP
        // Actualizar animación
        interp += anim_speed;
        if (interp >= 1.0)
            interp = 0.0;
            current_frame = next_frame;
            next_frame++;
            if (next_frame >= max_frames) next_frame = 0; end
        end
        
        RAY_SET_SPRITE_ANIM(sprite_lower, current_frame, next_frame, interp);
        
        // Rotación
        rot_angle += 1.0;
        if (rot_angle > 360.0) rot_angle -= 360.0; end
        RAY_SET_SPRITE_ANGLE(sprite_lower, rot_angle);
        FRAME;
    END
END

// Proceso MD3 - Upper (Sigue a Lower)
PROCESS MD3_Upper(string filename, string skin_filename)
PRIVATE
    int model;
    int skin;
    float tag_x, tag_y, tag_z;
BEGIN
    model = RAY_LOAD_MD3(filename);
    if (model == 0)
        say("Error cargando MD3: " + filename);
        return;
    end
    
    skin = map_load(skin_filename);
    if (skin == 0)
        skin = 1;
    end
    
    // Posición inicial temporal (será actualizada por tag)
    sprite_upper = RAY_ADD_SPRITE(0, 0, 0, 0, 96, 96);
    RAY_SET_SPRITE_MD2(sprite_upper, model, skin);
    
    LOOP
        // Seguir tag_torso de lower
        if (sprite_lower >= 0)
            if (RAY_GET_TAG_POINT(sprite_lower, "tag_torso", &tag_x, &tag_y, &tag_z))
                RAY_UPDATE_SPRITE_POSITION(sprite_upper, tag_x, tag_y, tag_z);
                // DEBUG: say("Upper: Tag encontrado en " + tag_x + "," + tag_y + "," + tag_z);
            else
                // DEBUG: say("Upper: Tag 'tag_torso' NO encontrado!");
            end
        end
        
        FRAME;
    END
END

// Proceso MD3 - Head (Sigue a Upper)
PROCESS MD3_Head(string filename, string skin_filename)
PRIVATE
    int model;
    int skin;
    float tag_x, tag_y, tag_z;
BEGIN
    model = RAY_LOAD_MD3(filename);
    if (model == 0)
        say("Error cargando MD3: " + filename);
        return;
    end
    
    skin = map_load(skin_filename);
    if (skin == 0)
        skin = 1;
    end
    
    sprite_head = RAY_ADD_SPRITE(0, 0, 0, 0, 96, 96);
    RAY_SET_SPRITE_MD2(sprite_head, model, skin);
    
    LOOP
        // Seguir tag_head de upper
        if (sprite_upper >= 0)
            if (RAY_GET_TAG_POINT(sprite_upper, "tag_head", &tag_x, &tag_y, &tag_z))
                RAY_UPDATE_SPRITE_POSITION(sprite_head, tag_x, tag_y, tag_z);
            end
        end
        
        FRAME;
    END
END

// Proceso Car Model (MD3 simple)
PROCESS CarModel(string filename, string skin_filename, float mx, float my, float mz)
PRIVATE
    int model;
    int skin;
    int sprite;
    float rot_angle = 0.0;
BEGIN
    model = RAY_LOAD_MD3(filename);
    if (model == 0)
        say("Error cargando Car MD3: " + filename);
        return;
    end
    
    skin = map_load(skin_filename);
    if (skin == 0)
        say("Warning: Car skin no cargado: " + skin_filename);
        skin = 1;
    else
        say("Car skin cargado OK: ID=" + skin + " File=" + skin_filename);
    end
    
    // Create sprite (Static car) - Increased size for visibility
    sprite = RAY_ADD_SPRITE(mx, my, mz, 0, 256, 256); 
    RAY_SET_SPRITE_MD2(sprite, model, skin);
    
    // Scale up the model 10x to make it visible
    say("Car sprite created: ID=" + sprite + " Scale=8.0");
    RAY_SET_SPRITE_SCALE(sprite, 5.0);
    
    LOOP
        // Simple rotation to show off the model
        FRAME;
    END
END

// Proceso Sonic Model (MD3)
PROCESS SonicModel(string filename, string skin_filename, float mx, float my, float mz)
PRIVATE
    int model;
    int skin;
    int sprite;
BEGIN
    model = RAY_LOAD_MD3(filename);
    if (model == 0)
        say("Error cargando Sonic MD3: " + filename);
        return;
    end
    
    skin = map_load(skin_filename);
    if (skin == 0)
        say("Warning: Sonic skin no cargado: " + skin_filename);
        skin = 1;
    else
        say("Sonic skin cargado OK: ID=" + skin + " File=" + skin_filename);
    end
    
    // Create sprite - Static model
    sprite = RAY_ADD_SPRITE(mx, my, mz, 0, 256, 256); 
    RAY_SET_SPRITE_MD2(sprite, model, skin);
    
    // Scale appropriately
    say("Sonic sprite created: ID=" + sprite);
    RAY_SET_SPRITE_SCALE(sprite, 0.8);
    
    RAY_SET_SPRITE_ANGLE(sprite, 90000); // Rotate 90 degrees left (miliangles)
    
    LOOP
        FRAME;
    END
END

// Proceso Lara Model (MD3)
PROCESS LaraModel(string filename, string skin_filename, float mx, float my, float mz)
PRIVATE
    int model;
    int skin;
    int sprite;
BEGIN
    model = RAY_LOAD_MD3(filename);
    if (model == 0)
        say("Error cargando Lara MD3: " + filename);
        return;
    end
    
    skin = map_load(skin_filename);
    if (skin == 0)
        say("Warning: Lara skin no cargado: " + skin_filename);
        skin = 1;
    else
        say("Lara skin cargado OK: ID=" + skin + " File=" + skin_filename);
    end
    
    // Create sprite - Static model
    sprite = RAY_ADD_SPRITE(mx, my, mz, 0, 256, 256); 
    RAY_SET_SPRITE_MD2(sprite, model, skin);
    
    // Scale appropriately (Lara usually needs scaling down if high res)
    say("Lara sprite created: ID=" + sprite);
    RAY_SET_SPRITE_SCALE(sprite, 2.5); // Increased from 0.3 to 2.5
    
    LOOP
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

PROCESS main()
PRIVATE
    float move_speed = 5.0;
    float rot_speed = 0.05;
    float pitch_speed = 0.02;
    float spawn_x, spawn_y, spawn_z;
    float car_x, car_y, car_z;
    float sonic_x, sonic_y, sonic_z;
    float lara_x, lara_y, lara_z;
BEGIN
    //screen.scale_resolution = 08000600; // 640*10000 + 480  
    set_mode(640, 480);
    set_fps(0, 0);
    window_set_title("Geometric Sector Engine Test - MD3 Tags & Car");

    fpg_textures = fpg_load("textures2.fpg");
    if (fpg_textures < 0)
        say("ERROR: No se pudo cargar textures.fpg");
        exit();
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
    
    // Spawning logic for Grunt (Prefer Flag 1 now)
    spawn_x = RAY_GET_FLAG_X(1);
    spawn_y = RAY_GET_FLAG_Y(1);
    spawn_z = RAY_GET_FLAG_Z(1);
    
    if (spawn_x == 0 && spawn_y == 0)
         // Fallback to 2 if 1 missing, but 2 is taken by car? Overlap risk.
         spawn_x = RAY_GET_FLAG_X(2);
         spawn_y = RAY_GET_FLAG_Y(2);
         spawn_z = RAY_GET_FLAG_Z(2);
    end
    if (spawn_x == 0 && spawn_y == 0)
        spawn_x = 500.0; spawn_y = 384.0;
        spawn_z = RAY_GET_FLOOR_HEIGHT(spawn_x, spawn_y);
    end
    
    say("Cargando personaje MD3 completo...");
    
    // Cargar partes del personaje (Grunt)
    MD3_Lower("Models/md3/lower.md3", "Models/md3/grunt.jpg", spawn_x, spawn_y, spawn_z);
    MD3_Upper("Models/md3/upper.md3", "Models/md3/grunt.jpg");
    MD3_Head("Models/md3/head.md3", "Models/md3/grunt.jpg");
    
    // Spawn Car at Flag 2
    car_x = RAY_GET_FLAG_X(2);
    car_y = RAY_GET_FLAG_Y(2);
    car_z = RAY_GET_FLAG_Z(2);
    
    if (car_x != 0 || car_y != 0)
        say("Spawning Car at Flag 2: " + car_x + "," + car_y);
        // Lower the car to ground level (model origin is at center, not bottom)
        car_z = car_z - 20.0;  // Adjust this value if needed
        CarModel("Models/md3/Car.md3", "Models/md3/Car.png", car_x, car_y, car_z);
    end
    
    // Spawn Sonic at Flag 3
    sonic_x = RAY_GET_FLAG_X(3);
    sonic_y = RAY_GET_FLAG_Y(3);
    sonic_z = RAY_GET_FLAG_Z(3);
    
    if (sonic_x != 0 || sonic_y != 0)
        say("Spawning Sonic at Flag 3: " + sonic_x + "," + sonic_y);
        // Lower Sonic to ground level (model origin is at center, not bottom)
        sonic_z = sonic_z - 15.0;  // Adjust if needed
        SonicModel("Models/md3/sonic.md3", "Models/md3/player00.png", sonic_x, sonic_y, sonic_z);
    end
    
    // Spawn Lara at Flag 4
    lara_x = RAY_GET_FLAG_X(4);
    lara_y = RAY_GET_FLAG_Y(4);
    lara_z = RAY_GET_FLAG_Z(4);
    
    if (lara_x != 0 || lara_y != 0)
        say("Spawning Lara at Flag 4: " + lara_x + "," + lara_y);
        // Raise Lara (scaled up model sinks down)
        lara_z = lara_z + 95.0;
        LaraModel("Models/md3/Lara.md3", "Models/md3/Lara.png", lara_x, lara_y, lara_z);
    end
    
    ray_display();
    
    LOOP
        if (key(_w)) RAY_MOVE_FORWARD(move_speed); end
        if (key(_s)) RAY_MOVE_BACKWARD(move_speed); end
        if (key(_a)) RAY_STRAFE_LEFT(move_speed); end
        if (key(_d)) RAY_STRAFE_RIGHT(move_speed); end
        
        // Movimiento vertical (Q = bajar, E = subir)
        if (key(_q)) RAY_MOVE_UP_DOWN(-move_speed); end
        if (key(_e)) RAY_MOVE_UP_DOWN(move_speed); end
        
        if (key(_left)) RAY_ROTATE(-rot_speed); end
        if (key(_right)) RAY_ROTATE(rot_speed); end
        if (key(_up)) RAY_LOOK_UP_DOWN(pitch_speed); end
        if (key(_down)) RAY_LOOK_UP_DOWN(-pitch_speed); end
        if (key(_space)) RAY_JUMP(); end
        
        write(0, 10, 90, 0, "FPS: " + frame_info.fps);
        
        if (key(_esc)) let_me_alone(); EXIT("Gracias por probar",0); end
        
        FRAME;
        WRITE_DELETE(all_text);
    END
    
    RAY_FREE_MAP();
    RAY_SHUTDOWN();
    fpg_unload(fpg_textures);
END
