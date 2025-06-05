import "libmod_doom3d"

PROGRAM main;
BEGIN
    doom3d_init(320, 200);
    WHILE (!key(_esc))
        doom3d_render();
        FRAME;
    END
    doom3d_finish();
END