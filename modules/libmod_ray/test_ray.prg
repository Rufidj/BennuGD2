// test_ray.prg
// Programa de prueba para el módulo Raycasting
import "libmod_gfx";
import "libmod_input";
import "libmod_misc";
import "libmod_ray";

GLOBAL
    int screen_w = 800;
    int screen_h = 600;
    int fpg_textures;
    int fog_enabled = 0;
    int minimap_enabled = 1;
END

PROCESS main()
PRIVATE
    float move_speed = 5.0;
    float rot_speed = 0.05;
    float pitch_speed = 0.02;
    int fog_key_pressed = 0;
    int minimap_key_pressed = 0;
    int enter_key_pressed = 0;  // Añadido para debounce de Enter
    float cam_x, cam_y;
BEGIN
    set_mode(screen_w, screen_h);
    set_fps(0, 0);
    window_set_title("Raycasting Engine Test");

    // Cargar FPG con texturas
    fpg_textures = fpg_load("textures.fpg");
    if (fpg_textures < 0)
        say("ERROR: No se pudo cargar textures.fpg");
        say("Ejecuta crear_texturas.prg para generar el FPG");
        exit();
    end
    
    // Inicializar motor de raycasting
    if (RAY_INIT(screen_w, screen_h, 90, 1) == 0)
        say("ERROR: No se pudo inicializar el motor");
        exit();
    end
    
    // Cargar mapa .raymap
    if (RAY_LOAD_MAP("test.raymap", fpg_textures) == 0)
        say("ERROR: No se pudo cargar el mapa");
        RAY_SHUTDOWN();
        exit();
    end
    
    // Configurar posición inicial de la cámara
    // NOTA: El editor aún no guarda posición de cámara, así que la configuramos manualmente
    RAY_SET_CAMERA(8.0 * 128.0 + 64.0, 8.0 * 128.0 + 64.0, 0.0, 0.0, 0.0);
    
    // Activar efectos
    RAY_SET_FOG(fog_enabled);
    RAY_SET_DRAW_MINIMAP(minimap_enabled);
    RAY_SET_DRAW_WEAPON(0);
    
    // Iniciar proceso de renderizado
    ray_display();
    
    LOOP
        // ===== CONTROLES DE MOVIMIENTO =====
        if (key(_w)) 
            RAY_MOVE_FORWARD(move_speed); 
        end
        
        if (key(_s)) 
            RAY_MOVE_BACKWARD(move_speed); 
        end
        
        if (key(_a)) 
            RAY_STRAFE_LEFT(move_speed); 
        end
        
        if (key(_d)) 
            RAY_STRAFE_RIGHT(move_speed); 
        end
        
        // ===== CONTROLES DE CÁMARA =====
        if (key(_left)) 
            RAY_ROTATE(-rot_speed); 
        end
        
        if (key(_right)) 
            RAY_ROTATE(rot_speed); 
        end
        
        if (key(_up)) 
            RAY_LOOK_UP_DOWN(pitch_speed); 
        end
        
        if (key(_down)) 
            RAY_LOOK_UP_DOWN(-pitch_speed); 
        end
        
        // ===== SALTO =====
        if (key(_space)) 
            RAY_JUMP(1); 
        end
        
        // ===== INTERACCIÓN CON PUERTAS =====
        if (key(_enter))
            if (enter_key_pressed == 0)
                RAY_TOGGLE_DOOR();
                enter_key_pressed = 1;
            end
        else
            enter_key_pressed = 0;
        end
        
        // ===== OPCIONES DE VISUALIZACIÓN =====
        if (key(_f))
            if (fog_key_pressed == 0)
                fog_enabled = !fog_enabled;
                RAY_SET_FOG(fog_enabled);
                fog_key_pressed = 1;
            end
        else
            fog_key_pressed = 0;
        end
        
        if (key(_m))
            if (minimap_key_pressed == 0)
                minimap_enabled = !minimap_enabled;
                RAY_SET_DRAW_MINIMAP(minimap_enabled);
                minimap_key_pressed = 1;
            end
        else
            minimap_key_pressed = 0;
        end
        
        // ===== INFORMACIÓN EN PANTALLA =====
        write(0, 10, 10, 0, "WASD: Mover | Flechas: Rotar cámara | Espacio: Saltar");
        write(0, 10, 30, 0, "Enter: Abrir puerta | F: Niebla | M: Minimapa");
        write(0, 10, 50, 0, "ESC: Salir");
        
        // Mostrar posición (útil para debug)
        cam_x = RAY_GET_CAMERA_X();
        cam_y = RAY_GET_CAMERA_Y();
        write(0, 10, 70, 0, "Pos: (" + (cam_x / 128) + ", " + (cam_y / 128) + ")");
        write(0, 10, 90, 0, "Fog: " + fog_enabled + " | Minimap: " + minimap_enabled);
        
        if (key(_esc)) 
            break; 
        end
        
        FRAME;
        WRITE_DELETE(all_text);
    END
    
    // Limpiar recursos
    RAY_FREE_MAP();
    RAY_SHUTDOWN();
    fpg_unload(fpg_textures);
END

PROCESS ray_display()
BEGIN
    LOOP
        // Renderizar el frame del raycasting
        graph = RAY_RENDER();
        
        if (graph)
            x = screen_w / 2;
            y = screen_h / 2;
            size = 100;
        else
            say("ERROR: No se pudo renderizar el frame");
        end
        
        FRAME;
    END
END
