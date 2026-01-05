import "libmod_gfx";        
import "libmod_input";        
import "libmod_misc";        
import "libmod_heightmap";        
        
GLOBAL        
    int wld_loaded = 0;        
    string base_path;        
    string assets_path;        
    int fpg_id;      
END        
        
PROCESS main()        
PRIVATE        
    int cam_x = 5938;        
    int cam_y = 5236;        
    int cam_z = 1100;        
    int cam_angle = 0;        
    int cam_pitch = 0;        
    int cam_fov = 60000;        
BEGIN        
    set_mode(640, 480);        
    set_fps(0, 0);        
    window_set_title("WLD Map Renderer");        
            
    // 1. Cargar FPG con verificación de errores    
    say("Cargando archivo FPG...");        
    fpg_id = FPG_LOAD("assets/textures.fpg");     
    say("FPG_ID obtenido: " + fpg_id);        
      
    // Verificar si el FPG se cargó correctamente    
    if (fpg_id == -1)        
        say("ERROR: No se pudo cargar textures.fpg - archivo corrupto o formato inválido");        
        return;        
    end        
            
    file = fpg_id;      
    say("FPG cargado correctamente con ID: " + fpg_id);               
    // 2. Cargar mapa WLD solo si el FPG es válido    
    say("Cargando mapa WLD...");        
    wld_loaded = LOAD_WLD("assets/test.wld", fpg_id);        
    say("LOAD_WLD retornó: " + wld_loaded);        
        
    if (!wld_loaded)        
        say("ERROR: No se pudo cargar test.wld");        
        return;        
    end        
    say("Mapa WLD cargado correctamente");      
            
    // 3. Configurar cámara inicial        
    HEIGHTMAP_SET_CAMERA(cam_x, cam_y, cam_z, cam_angle, cam_pitch, cam_fov);        
    say("Cámara configurada en (" + cam_x + "," + cam_y + "," + cam_z + ")");        
            
    // 4. Iniciar procesos de renderizado e info      
    wld_display();        
    display_wld_info();        
            
    // 5. Loop principal con controles        
    LOOP        
        if (key(_left)) HEIGHTMAP_LOOK_HORIZONTAL(-5); end        
        if (key(_right)) HEIGHTMAP_LOOK_HORIZONTAL(5); end        
        if (key(_up)) HEIGHTMAP_LOOK_VERTICAL(5); end        
        if (key(_down)) HEIGHTMAP_LOOK_VERTICAL(-5); end        
        if (key(_w)) HEIGHTMAP_MOVE_FORWARD(50); end        
        if (key(_s)) HEIGHTMAP_MOVE_BACKWARD(50); end        
        if (key(_a)) HEIGHTMAP_STRAFE_LEFT(50); end        
        if (key(_d)) HEIGHTMAP_STRAFE_RIGHT(50); end        
        if (key(_q)) HEIGHTMAP_ADJUST_HEIGHT(10); end        
        if (key(_e)) HEIGHTMAP_ADJUST_HEIGHT(-10); end        
        if (key(_esc)) exit(); end        
        FRAME;        
    END        
END        
        
PROCESS wld_display()        
PRIVATE        
    int frame_count = 0;        
BEGIN        
    LOOP        
        graph = HEIGHTMAP_RENDER_WLD_3D(640, 480);        
        if (!graph)        
            say("ERROR: Falló el renderizado WLD");        
            write(0, 320, 240, 4, "ERROR DE RENDERIZADO WLD");        
        else        
            x = 320;        
            y = 240;        
            size = 100;        
        end        
        frame_count++;        
        if (frame_count >= 60)        
            say("Renderizado OK - frame: " + frame_count);        
            frame_count = 0;        
        end        
        FRAME;        
    END        
END        
        
PROCESS display_wld_info()        
PRIVATE        
    int cam_x, cam_y, cam_z, cam_angle, cam_pitch;        
BEGIN        
    LOOP        
        HEIGHTMAP_GET_CAMERA_POSITION(&cam_x, &cam_y, &cam_z, &cam_angle, &cam_pitch);        
        write(0, 10, 10, 0, "FPS: " + frame_info.fps);        
        write(0, 10, 30, 0, "Posición cámara: " + cam_x + "," + cam_y + "," + cam_z);        
        write(0, 10, 50, 0, "Ángulo: " + cam_angle + " Pitch: " + cam_pitch);        
        write(0, 10, 70, 0, "Controles: WASD= mover, Flechas= rotar, Q/E= altura");        
        write(0, 10, 90, 0, "FPG ID: " + fpg_id + " | ESC= salir");        
        FRAME;        
        write_delete(all_text);        
    END        
END