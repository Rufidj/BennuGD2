import "libmod_gfx";      
import "libmod_input";      
import "libmod_misc";      
   
      
GLOBAL      
    int wld_loaded = 0;      
    string base_path;      
    string assets_path;      
    int fpg_id;  // Nuevo: ID del archivo FPG cargado  
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
    window_set_title("fpg load");      
    say("Cargando archivo FPG...");        
    fpg_id = FPG_LOAD("textures.fpg");     
    say("FPG_ID obtenido: " + fpg_id);        
      
    // Verificar si el FPG se cargó correctamente    
    if (fpg_id == -1)        
        say("ERROR: No se pudo cargar textures.fpg - archivo corrupto o formato inválido");        
        return;        
    end        
    file = fpg_id;
    graph = 1;
    loop
    if(key(_esc)); exit("",0);end 
    frame;
    end
    END
