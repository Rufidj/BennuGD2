import "libmod_gfx";
import "libmod_misc";
import "libmod_sound";
import "libmod_video";

GLOBAL
    string vfile = "test_h264.mp4";

PROCESS main()
PRIVATE
    int v; int i; int pos;
BEGIN
    set_mode(640, 360);
    set_fps(60, 0);
    v = video_open(vfile);
    say("open=" + itoa(v) + " w=" + itoa(video_width(v)) + " h=" + itoa(video_height(v)) + " dur_ms=" + itoa(video_duration(v)));
    if (v == 0) exit(); end
    graph = video_graph(v);
    x = 320; y = 180;
    video_play(v, 0);
    while (!video_is_finished(v) && i < 900)
        i++;
        if (i % 30 == 0) say("frame " + itoa(i) + " pos_ms=" + itoa(video_position(v)) + " playing=" + itoa(video_is_playing(v))); end
        frame;
    end
    say("finished=" + itoa(video_is_finished(v)) + " frames=" + itoa(i));
    video_close(v);
    exit();
END
