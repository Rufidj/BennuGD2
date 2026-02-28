import "libmod_ray";
import "libmod_misc";

Process Main()
Begin
    say("Intentando cargar libmod_ray...");
    // Solo llamamos a funciones básicas si la carga tiene éxito
    if (RAY_INIT(640, 480, 60, 1))
        say("RAY_INIT exitoso");
        RAY_SHUTDOWN();
    else
        say("RAY_INIT fallo");
    end
End
