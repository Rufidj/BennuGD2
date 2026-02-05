// Project Main Entry Point
// Configured by RayMap Editor to start with scene: menu

import "libmod_gfx";
import "libmod_input";
import "libmod_misc";
// import "libmod_ray"; // Uncomment if using 3D

include "includes/scene_commons.prg"
include "includes/scenes_list.prg"

process main()
begin
    set_mode(640, 480, 32);
    set_fps(60, 0);

    // Start Initial Scene
    menu();

    // Global Exit Loop
    loop
        if (key(_esc)) exit("", 0); end
        frame;
    end
end
