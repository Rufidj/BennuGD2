// test_build.prg
// Programa de prueba para el módulo Build Engine
import "libmod_gfx";
import "libmod_input";
import "libmod_misc";
import "libmod_build";

GLOBAL
    int render;
    int screen_w = 640;
    int screen_h = 480;
END

PROCESS main()
PRIVATE
    int cam_x, cam_y, cam_z, cam_angle, cam_horiz;
BEGIN
    set_mode(screen_w, screen_h);
    set_fps(0, 0);
    window_set_title("Build Engine Test");
    
    // Cargar paleta del Build Engine
    // Necesitas PALETTE.DAT de Duke Nukem 3D, Shadow Warrior, etc.
    if (BUILD_LOAD_PALETTE("MAPS/PALETTE.DAT") != 0)
        say("ADVERTENCIA: No se pudo cargar la paleta");
        say("Las texturas no se verán correctamente");
    end
    
    // Cargar archivos de texturas .ART
    // Duke Nukem 3D usa TILES000.ART, TILES001.ART, etc.
    BUILD_LOAD_ART("MAPS/TILES000.ART");
    BUILD_LOAD_ART("MAPS/TILES001.ART");
    BUILD_LOAD_ART("MAPS/TILES002.ART");
    BUILD_LOAD_ART("MAPS/TILES003.ART");
    BUILD_LOAD_ART("MAPS/TILES004.ART");
    BUILD_LOAD_ART("MAPS/TILES005.ART");
    BUILD_LOAD_ART("MAPS/TILES006.ART");
    BUILD_LOAD_ART("MAPS/TILES007.ART");
    BUILD_LOAD_ART("MAPS/TILES008.ART");
    BUILD_LOAD_ART("MAPS/TILES009.ART");
    BUILD_LOAD_ART("MAPS/TILES010.ART");
    BUILD_LOAD_ART("MAPS/TILES011.ART");
    BUILD_LOAD_ART("MAPS/TILES012.ART");
    // Añade más archivos .ART si los tienes
    
    // Cargar mapa Build Engine (.MAP)
    // Necesitas un archivo .MAP de Duke Nukem 3D, Shadow Warrior, Blood, etc.
    if (BUILD_LOAD_MAP("MAPS/E1L1.MAP") != 0)
        say("ERROR: No se pudo cargar el mapa");
        say("Asegúrate de tener un archivo .MAP válido");
        exit();
    end
    
    // La cámara ya se configuró automáticamente con la posición inicial del mapa
    // Pero puedes ajustarla manualmente si quieres:
    // BUILD_SET_CAMERA(x, y, z, ang, horiz, sectnum)
    // Nota: ang va de 0-2047 (Build Engine usa 2048 ángulos, no 360 grados)
    //       horiz: 100 = recto, <100 = arriba, >100 = abajo
    
    // Iniciar proceso de renderizado
    build_display();
    
    LOOP
        // Controles de movimiento
        if (key(_w)) BUILD_MOVE_FORWARD(10); end
        if (key(_s)) BUILD_MOVE_BACKWARD(10); end
        if (key(_a)) BUILD_STRAFE_LEFT(8); end
        if (key(_d)) BUILD_STRAFE_RIGHT(8); end
        
        // Controles de cámara
        if (key(_left)) BUILD_LOOK_HORIZONTAL(-15); end
        if (key(_right)) BUILD_LOOK_HORIZONTAL(15); end
        if (key(_up)) BUILD_LOOK_VERTICAL(-10); end
        if (key(_down)) BUILD_LOOK_VERTICAL(10); end
        
        // Vertical movement (Flying)
        if (key(_q)) BUILD_MOVE_VERTICAL(-256); end // Up
        if (key(_e)) BUILD_MOVE_VERTICAL(256); end  // Down
        
        // Información en pantalla
        write(0, 10, 10, 0, "WASD: Mover | Flechas: Rotar cámara");
        write(0, 10, 30, 0, "ESC: Salir");
        write(0, 10, 50, 0, "FPS: " + frame_info.fps);
        
        if (key(_esc)) exit(); end
        
        FRAME;
        WRITE_DELETE(all_text);
    END
END

PROCESS build_display()
BEGIN
    LOOP
        // Renderizar el mapa Build Engine
        // Retorna el código del GRAPH para mostrarlo
        render = BUILD_RENDER(screen_w, screen_h);
        
        if (render)
            graph = render;
            x = screen_w / 2;
            y = screen_h / 2;
            // size = 200 significa 200% (el doble del tamaño)
            // Como el render es de 640x480 y la pantalla también, usamos 100
            size = 100;
        else
            say("ERROR: No se pudo renderizar");
        end
        
        FRAME;
    END
END
