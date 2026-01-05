// inspect_map.prg - Inspeccionar contenido del mapa
import "libmod_misc";
import "libmod_ray";

PROCESS main()
BEGIN
    // Inicializar motor (necesario para las estructuras internas)
    if (RAY_INIT(800, 600, 90, 1) == 0)
        say("ERROR: No se pudo inicializar");
        exit();
    end
    
    // Cargar mapa
    if (RAY_LOAD_MAP("test.raymap", 0) == 0)
        say("ERROR: No se pudo cargar mapa");
        exit();
    end
    
    say("=== DEBUG DEL MAPA ===");
    // No podemos acceder a las estructuras internas desde BennuScript fácilmente
    // así que usaremos la consola de bgdc para ver el output de RAY_LOAD_MAP
    
    RAY_FREE_MAP();
    RAY_SHUTDOWN();
END
