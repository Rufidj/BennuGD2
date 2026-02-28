import "mod_video";
import "mod_map";
import "mod_key";
import "mod_text";
import "libmod_ray";

Process main()
Begin
    set_mode(800, 600, 32);
    ray_init(800, 600, 60, 1);
    
    // Check if models exist and load them
    // For now we just want to compile the code
    Loop
        if (key(_esc)) break; end
        ray_render();
        frame;
    End
End
