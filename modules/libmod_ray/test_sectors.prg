// Programa de prueba para el sistema de PORTALES
import "libmod_gfx";
import "libmod_input";
import "libmod_misc";
import "libmod_ray";

GLOBAL
    int screen_w = 800;
    int screen_h = 600;
END

PROCESS main()
PRIVATE
    int fpg_result;
    int fpg_id = 0;
    int sector_a, sector_b;
BEGIN
    set_mode(screen_w, screen_h);
    set_fps(120, 0);
    window_set_title("Raycaster - Test de Portales");
    
    // Cargar texturas
    fpg_result = fpg_load("textures.fpg");
    if (fpg_result == -1)
        say("ADVERTENCIA: No se pudo cargar textures.fpg");
        fpg_id = -1;
    else
        say("Texturas cargadas correctamente");
    end
    
    // Crear mapa 16x10 (habitaciones más grandes)
    if (ray_create_map(16, 10, 1, 64, fpg_id) == 0)
        say("ERROR: No se pudo crear el mapa");
        exit();
    end
    
    say("Creando mapa con 2 habitaciones conectadas por portal...");
    
    // HABITACIÓN A (izquierda): tiles 0-7, 0-9 (8x10)
    // Paredes exteriores
    for(x=0; x<8; x++)
        ray_set_cell(x, 0, 0, 1);  // Norte
        ray_set_cell(x, 9, 0, 1);  // Sur
    end
    for(y=0; y<10; y++)
        ray_set_cell(0, y, 0, 1);  // Oeste
    end
    // Pared este con apertura para portal (dejar tiles 7,4 y 7,5 vacíos)
    for(y=0; y<10; y++)
        if (y != 4 && y != 5)  // Portal de 2 tiles
            ray_set_cell(7, y, 0, 1);
        end
    end
    
    // HABITACIÓN B (derecha): tiles 8-15, 0-9 (8x10)
    // Pared oeste con apertura para portal (dejar tiles 8,4 y 8,5 vacíos)
    for(y=0; y<10; y++)
        if (y != 4 && y != 5)  // Portal de 2 tiles
            ray_set_cell(8, y, 0, 2);
        end
    end
    // Paredes exteriores
    for(x=8; x<16; x++)
        ray_set_cell(x, 0, 0, 2);  // Norte
        ray_set_cell(x, 9, 0, 2);  // Sur
    end
    for(y=0; y<10; y++)
        ray_set_cell(15, y, 0, 2);  // Este
    end
    
    // Crear sectores (el sistema detectará el portal automáticamente)
    sector_a = ray_add_sector(0, 0, 7, 9);  // Habitación A (8x10)
    ray_set_sector_floor(sector_a, 0.0, 2);
    ray_set_sector_ceiling(sector_a, 64.0, 3, 1);  // Techo normal
    say("Sector A (habitación baja): Techo a 64");
    
    sector_b = ray_add_sector(8, 0, 15, 9);  // Habitación B (8x10)
    ray_set_sector_floor(sector_b, 0.0, 1);
    ray_set_sector_ceiling(sector_b, 128.0, 3, 1);  // Techo ALTO (doble altura)
    say("Sector B (habitación alta): Techo a 128");
    
    say("¡Portal detectado automáticamente entre sectores!");
    say("Habitación A: baja (64) | Habitación B: alta (128)");
    say("Muévete entre las habitaciones para ver el cambio de altura");
    
    // Configurar cámara en centro de habitación A
    ray_set_camera(256.0, 320.0, 0.0, 0.0, 60.0);
    
    // Iniciar renderizado
    raycaster_display();
    
    LOOP
        // Controles
        if (key(_w)) ray_move_forward(5.0); end
        if (key(_s)) ray_move_backward(5.0); end
        if (key(_left)) ray_rotate(-0.05); end
        if (key(_right)) ray_rotate(0.05); end
        if (key(_a)) ray_strafe_left(5.0); end
        if (key(_d)) ray_strafe_right(5.0); end
        
        // Info
        write(0, 10, 10, 0, "WS: Adelante/Atrás | Flechas: Rotar | ESC: Salir");
        write(0, 10, 30, 0, "FPS: " + frame_info.fps);
        write(0, 10, 50, 0, "TEST: 2 habitaciones con portal automático");
        
        // DEBUG
        write(0, 10, 70, 0, "Pos: " + ray_get_camera_x() + ", " + ray_get_camera_y());
        write(0, 10, 90, 0, "Angle: " + ray_get_camera_angle());
        
        if (key(_esc))
            ray_unload_map();
            exit();
        end
        
        FRAME;
        WRITE_DELETE(all_text);
    END
END

PROCESS raycaster_display()
BEGIN
    LOOP
        graph = ray_render(screen_w, screen_h);
        x = screen_w / 2;
        y = screen_h / 2;
        FRAME;
    END
END
