// test_doom.prg
// Programa de prueba para el módulo Doom Engine
import "libmod_gfx";
import "libmod_input";
import "libmod_misc";
import "libmod_doom";

GLOBAL
    int render;
    int screen_w = 640;
    int screen_h = 480;
END

PROCESS main()
PRIVATE
    int cam_x, cam_y, cam_z, cam_angle;
BEGIN
    set_mode(screen_w, screen_h);
    set_fps(0, 0);
    window_set_title("Doom Engine Test");
    
    // Inicializar el motor Doom
    DOOM_INIT();
    
    // Cargar archivo WAD
    // Necesitas DOOM1.WAD (shareware) o DOOM.WAD (completo)
    if (DOOM_LOAD_WAD("DOOM.WAD") != 0)
        say("ERROR: No se pudo cargar el WAD");
        say("Asegúrate de tener DOOM1.WAD o DOOM.WAD");
        exit();
    end
    
    // Cargar mapa
    // E1M1 = Episode 1, Map 1
    if (DOOM_LOAD_MAP("E1M1") != 0)
        say("ERROR: No se pudo cargar el mapa E1M1");
        exit();
    end
    
    // Configurar cámara inicial
    // Posición inicial típica de E1M1
    // x, y, z (en unidades de punto fijo FRACUNIT)
    // angle (0-4294967295, donde ANG90 = 1073741824)
    // Posición inicial de la cámara
    // Spawn point exacto del jugador en E1M1
    cam_x = 1056;      // X position (spawn point E1M1)
    cam_y = -3616;     // Y position (spawn point E1M1)
    cam_z = 0;         // Z position (nivel del suelo)
    cam_angle = 1073741824;  // ANG90 = mirando al Norte
    
    DOOM_SET_CAMERA(cam_x, cam_y, cam_z, cam_angle);
    
    // Iniciar proceso de renderizado
    doom_display();
    LOOP
        // Controles de movimiento
        // Velocidad de movimiento (en unidades de punto fijo)
        if (key(_w)) DOOM_MOVE_FORWARD(8 * 65536); end
        if (key(_s)) DOOM_MOVE_BACKWARD(8 * 65536); end
        if (key(_a)) DOOM_STRAFE_LEFT(6 * 65536); end
        if (key(_d)) DOOM_STRAFE_RIGHT(6 * 65536); end
        
        // Controles de rotación
        // Ángulo de rotación (ANG1 = 11930465, aproximadamente 1 grado)
        if (key(_left)) DOOM_TURN_LEFT(11930465 * 3); end   // ~3 grados
        if (key(_right)) DOOM_TURN_RIGHT(11930465 * 3); end // ~3 grados
        
        // Información en pantalla
        write(0, 10, 10, 0, "WASD: Mover | Flechas: Rotar");
        write(0, 10, 30, 0, "ESC: Salir");
        write(0, 10, 50, 0, "FPS: " + frame_info.fps);
        
        if (key(_esc)) 
            DOOM_SHUTDOWN();
            exit(); 
        end
        
        FRAME;
        WRITE_DELETE(all_text);
    END
END

PROCESS doom_display()
BEGIN
    LOOP
        // Renderizar el frame del motor Doom
        // Retorna el código del GRAPH para mostrarlo
        render = DOOM_RENDER_FRAME(screen_w, screen_h);
        
        if (render)
            graph = render;
            x = screen_w / 2;
            y = screen_h / 2;
            size = 100;
        else
            say("ERROR: No se pudo renderizar");
        end
        
        FRAME;
    END
END
