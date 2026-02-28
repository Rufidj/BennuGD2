/*
 * test_camera_load.prg
 * Test para cargar archivos .campath creados en el editor
 */

import "libmod_gfx";
import "libmod_input";
import "libmod_misc";
import "libmod_ray";

GLOBAL
    int camera_path_id = -1;
    int is_playing = 0;
    int screen_w = 640;
    int screen_h = 480;
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
    float delta_time = 0.016; // ~60 FPS
    int info_text;
    string campath_file = "template.campath";
END
BEGIN
    set_mode(640, 480, 32);
    set_fps(60, 0);
    
    say("=== TEST: Cargar archivo .campath ===");
    say("");
    
    // Cargar texturas
    say("Cargando texturas...");
    int fpg_textures = fpg_load("textures2.fpg");
    if (fpg_textures < 0)
        say("ERROR: No se pudo cargar textures2.fpg");
        exit();
    END
    
    // Inicializar motor
    say("Inicializando motor raycasting...");
    ray_init(640, 480, 90, 1);
    
    // Cargar mapa
    say("Cargando mapa...");
    if (ray_load_map("test.raymap", fpg_textures) == 0)
        say("ERROR: No se pudo cargar el mapa");
        say("Asegúrate de tener test.raymap en el directorio");
        RETURN;
    END
    
    say("Mapa cargado correctamente");
    say("");
    
    // Posicionar cámara inicial (Usar la del mapa)
    say("Posicionando cámara inicial (desde mapa)...");
    // ray_set_camera(384.0, 32.0, 384.0, 0.0, 0.0); // COMENTADO: Usar posición del mapa
    
    // Debug posición inicial
    float cx = ray_get_camera_x();
    float cy = ray_get_camera_y();
    float cz = ray_get_camera_z();
    say("Posición inicial: (" + cx + ", " + cy + ", " + cz + ")");
    
    // Cargar path de cámara desde archivo
    say("Cargando path de cámara desde: " + campath_file);
    say("(Crea este archivo en el Editor de Cámaras del raymap_editor)");
    say("");
    
    camera_path_id = ray_camera_load(campath_file);
    
    if (camera_path_id < 0)
        say("ERROR: No se pudo cargar el archivo .campath");
        say("");
        say("INSTRUCCIONES:");
        say("1. Abre raymap_editor");
        say("2. Carga un mapa");
        say("3. Tools → Editor de Cámaras (Ctrl+Shift+C)");
        say("4. Dibuja un path clickeando en el mapa 2D");
        say("5. Ajusta propiedades de keyframes");
        say("6. Guarda como 'template.campath'");
        say("7. Ejecuta este programa de nuevo");
        say("");
        say("MODO MANUAL ACTIVADO - Usa WASD para moverte");
        say("Presiona ESC para salir");
        
        // Texto en pantalla
        write(0, 10, 10, 0, "ERROR: No se pudo cargar " + campath_file);
        write(0, 10, 30, 0, "MODO MANUAL - WASD: Mover | Flechas: Rotar | ESC: Salir");
        
        // Modo manual
        LOOP
            // Movimiento manual
            if (key(_W)) ray_move_forward(5.0); END
            if (key(_S)) ray_move_backward(5.0); END
            if (key(_A)) ray_strafe_left(5.0); END
            if (key(_D)) ray_strafe_right(5.0); END
            if (key(_LEFT)) ray_rotate(-0.05); END
            if (key(_RIGHT)) ray_rotate(0.05); END
            if (key(_UP)) ray_look_up_down(0.05); END
            if (key(_DOWN)) ray_look_up_down(-0.05); END
            
            if (key(_ESC)) BREAK; END
            
            FRAME;
        END
        
        ray_shutdown();
        RETURN;
    END
    
    say("Path de cámara cargado correctamente!");
    say("ID del path: " + camera_path_id);
    say("");
    say("CONTROLES:");
    say("ESPACIO - Play/Pause");
    say("R - Reiniciar");
    say("S - Detener");
    say("M - Modo manual (WASD)");
    say("ESC - Salir");
    say("");
    
    // Texto en pantalla
    write(0, 10, 10, 0, "Sistema de Cámaras - Cargando archivo .campath");
    write(0, 10, 30, 0, "Archivo: " + campath_file);
    write(0, 10, 50, 0, "ESPACIO: Play | R: Reiniciar | S: Stop | M: Manual | ESC: Salir");
    info_text = write(0, 10, 450, 0, "Estado: DETENIDO - Presiona ESPACIO para reproducir");
    
    int manual_mode = 0;
    
    // Iniciar proceso de renderizado
    ray_display();
    
    LOOP
        // Controles
        if (key(_SPACE))
            if (is_playing)
                say("Pausando...");
                ray_camera_pause();
                is_playing = 0;
            ELSE
                say("Reproduciendo...");
                ray_camera_play(camera_path_id);
                is_playing = 1;
                manual_mode = 0;
            END
            WHILE (key(_SPACE)) FRAME; END
        END
        
        if (key(_R))
            say("Reiniciando...");
            ray_camera_stop();
            ray_camera_play(camera_path_id);
            is_playing = 1;
            manual_mode = 0;
            WHILE (key(_R)) FRAME; END
        END
        
        if (key(_S))
            say("Deteniendo...");
            ray_camera_stop();
            is_playing = 0;
            WHILE (key(_S)) FRAME; END
        END
        
        if (key(_M))
            say("Modo manual activado");
            ray_camera_stop();
            is_playing = 0;
            manual_mode = 1;
            WHILE (key(_M)) FRAME; END
        END
        
        if (key(_ESC))
            say("Saliendo...");
            BREAK;
        END
        
        // Actualizar cámara
        if (ray_camera_is_playing())
            ray_camera_update(delta_time);
            WRITE_DELETE(info_text);
            info_text = write(0, 10, 450, 0, "Estado: REPRODUCIENDO - La cámara sigue el path");
        ELSE
            if (manual_mode)
                // Movimiento manual
                if (key(_W)) ray_move_forward(5.0); END
                if (key(_S)) ray_move_backward(5.0); END
                if (key(_A)) ray_strafe_left(5.0); END
                if (key(_D)) ray_strafe_right(5.0); END
                if (key(_LEFT)) ray_rotate(-0.05); END
                if (key(_RIGHT)) ray_rotate(0.05); END
                
                WRITE_DELETE(info_text);
                info_text = write(0, 10, 450, 0, "Estado: MANUAL - WASD para mover, Flechas para rotar");
            ELSE
                WRITE_DELETE(info_text);
                info_text = write(0, 10, 450, 0, "Estado: PAUSADO - Presiona ESPACIO para continuar");
            END
        END
        
        FRAME;
    END
    
    // Limpiar
    say("Limpiando...");
    ray_shutdown();
    
    let_me_alone();
END
