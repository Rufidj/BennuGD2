import "libmod_gfx";
import "libmod_misc";
import "libmod_sound";
import "libmod_video";

GLOBAL
    string files[2] = "test_h264.mp4", "test_vp9.webm", "test_theora.ogv";

PROCESS main()
PRIVATE
    int v; int n; int i; int t0;
BEGIN
    set_mode(1280, 720);
    set_fps(60, 0);
    for (n = 0; n < 3; n++)
        v = video_open(files[n]);
        say("open " + files[n] + " -> " + itoa(v) + " " + itoa(video_width(v)) + "x" + itoa(video_height(v)) + " dur_ms=" + itoa(video_duration(v)));
        if (v == 0) continue; end
        graph = video_graph(v);
        x = 640; y = 360;
        size = 200;
        video_play(v, 0);
        t0 = timer[0];
        i = 0;
        while (!video_is_finished(v) && i < 1200)
            i++;
            if (i % 60 == 0) say(files[n] + " pos_ms=" + itoa(video_position(v))); end
            frame;
        end
        say(files[n] + " finished=" + itoa(video_is_finished(v)) + " frames=" + itoa(i));
        video_close(v);
        graph = 0;
    end
    say("ALL DONE");
    loop frame; end
END
