// test_ray_simple.prg - Simple raycasting test without MD3 models
import "libmod_gfx";
import "libmod_input";
import "libmod_misc";
import "libmod_ray";

GLOBAL
    int screen_w = 320;
    int screen_h = 240;
    int fpg_textures;
    int fog_enabled = 0;
    int minimap_enabled = 1;
END

PROCESS main()
PRIVATE
    float move_speed = 5.0;
    float rot_speed = 0.05;
    float pitch_speed = 0.02;
BEGIN
    set_mode(320, 240);
    set_fps(0, 0);
    window_set_title("Simple Raycasting Test - Vita Controls");

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
    
    ray_display();
    
    LOOP
        // Keyboard controls (PC)
        if (key(_w)) RAY_MOVE_FORWARD(move_speed); end
        if (key(_s)) RAY_MOVE_BACKWARD(move_speed); end
        if (key(_a)) RAY_STRAFE_LEFT(move_speed); end
        if (key(_d)) RAY_STRAFE_RIGHT(move_speed); end
        if (key(_q)) RAY_MOVE_UP_DOWN(-move_speed); end
        if (key(_e)) RAY_MOVE_UP_DOWN(move_speed); end
        if (key(_left)) RAY_ROTATE(-rot_speed); end
        if (key(_right)) RAY_ROTATE(rot_speed); end
        if (key(_up)) RAY_LOOK_UP_DOWN(pitch_speed); end
        if (key(_down)) RAY_LOOK_UP_DOWN(-pitch_speed); end
        if (key(_space)) RAY_JUMP(); end
        
        // PS Vita gamepad controls (same mapping as test_geometric.prg, with validation)
        // Left analog stick: Movement
        if (joy_getaxis(0, 1) < -5000 && joy_getaxis(0, 1) != -32768) RAY_MOVE_FORWARD(move_speed); end      // Up
        if (joy_getaxis(0, 1) > 5000) RAY_MOVE_BACKWARD(move_speed); end       // Down
        if (joy_getaxis(0, 0) < -5000 && joy_getaxis(0, 0) != -32768) RAY_STRAFE_LEFT(move_speed); end        // Left
        if (joy_getaxis(0, 0) > 5000) RAY_STRAFE_RIGHT(move_speed); end        // Right
        
        // Right analog stick: Look around (reject only exact -32768 which indicates broken axis)
        if (joy_getaxis(0, 2) < -5000 && joy_getaxis(0, 2) != -32768) RAY_ROTATE(-rot_speed); end             // Left
        if (joy_getaxis(0, 2) > 5000) RAY_ROTATE(rot_speed); end               // Right
        if (joy_getaxis(0, 3) < -5000 && joy_getaxis(0, 3) != -32768) RAY_LOOK_UP_DOWN(pitch_speed); end      // Up
        if (joy_getaxis(0, 3) > 5000) RAY_LOOK_UP_DOWN(-pitch_speed); end      // Down
        
        // Buttons
        if (joy_getbutton(0, 0)) RAY_JUMP(); end                                 // Cross (X)
        if (joy_getbutton(0, 4)) RAY_MOVE_UP_DOWN(move_speed); end              // L1 - Up
        if (joy_getbutton(0, 5)) RAY_MOVE_UP_DOWN(-move_speed); end             // R1 - Down
        
        write(0, 10, 90, 0, "FPS: " + frame_info.fps);
        write(0, 10, 100, 0, "Joy0: " + joy_getaxis(0, 0) + " Joy1: " + joy_getaxis(0, 1));
        write(0, 10, 110, 0, "Joy2: " + joy_getaxis(0, 2) + " Joy3: " + joy_getaxis(0, 3));
        write(0, 10, 120, 0, "Btn0: " + joy_getbutton(0, 0) + " Num: " + joy_number());
        
        // Debug: Press F1 to print joystick values to console
        if (key(_f1))
            say("=== JOYSTICK DEBUG ===");
            say("Joy0 (axis 0): " + joy_getaxis(0, 0));
            say("Joy1 (axis 1): " + joy_getaxis(0, 1));
            say("Joy2 (axis 2): " + joy_getaxis(0, 2));
            say("Joy3 (axis 3): " + joy_getaxis(0, 3));
            say("Joy4 (axis 4): " + joy_getaxis(0, 4));
            say("Joy5 (axis 5): " + joy_getaxis(0, 5));
            say("Buttons: " + joy_getbutton(0, 0) + " " + joy_getbutton(0, 1) + " " + joy_getbutton(0, 2));
            say("Num joysticks: " + joy_number());
            say("=====================");
            while(key(_f1)) frame; end // Wait for key release
        end
        
        if (key(_esc)) let_me_alone(); EXIT("Gracias por probar",0); end
        if (joy_getbutton(0, 6)) let_me_alone(); EXIT("Gracias por probar",0); end
        
        FRAME;
        WRITE_DELETE(all_text);
    END
    
    RAY_FREE_MAP();
    RAY_SHUTDOWN();
    fpg_unload(fpg_textures);
END

PROCESS ray_display()
BEGIN
    // Create persistent bitmap for rendering
    graph = map_new(320, 240, 32);
    
    // Center on screen
    x = screen_w / 2;
    y = screen_h / 2;
    
    LOOP
        // Render raycasting view onto the bitmap
        RAY_RENDER(graph);
        
        // The graph is assigned to the process, so it draws automatically
        FRAME;
    END
END

