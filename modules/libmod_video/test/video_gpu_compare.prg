import "libmod_gfx";
import "libmod_input";
import "libmod_misc";
import "libmod_video";

global
    int vh;
    int g;
    int phase;
    int frames;
    int t0;
    int shown;
begin
    set_mode(1280, 720);
    for (phase = 1; phase >= 0; phase--)
        video_use_gpu(phase);
        vh = video_open("clip720.mp4");
        say("PS5GPU: fase gpu=" + phase + " handle=" + vh + " usa_gpu=" + video_gpu(vh) + " " + video_width(vh) + "x" + video_height(vh));
        g = video_graph(vh);
        shown = show(g);
        video_play(vh, 0);
        frames = 0;
        t0 = get_timer();
        while (!video_is_finished(vh) and get_timer() - t0 < 9000)
            frames++;
            if (frames % 60 == 0) say("PS5GPU: gpu=" + phase + " frames=" + frames + " fps=" + (frames * 1000 / (get_timer() - t0)) + " pos=" + video_position(vh)); end
            frame;
        end
        say("PS5GPU: fin fase gpu=" + phase + " frames=" + frames + " ms=" + (get_timer() - t0) + " fps_medio=" + (frames * 1000 / (get_timer() - t0)) + " pos=" + video_position(vh));
        signal(shown, s_kill);
        video_close(vh);
        t0 = get_timer();
        while (get_timer() - t0 < 700) frame; end
    end
    say("PS5GPU: terminado");
    loop frame; end
end
process show(g)
begin
    x = 640; y = 360;
    loop graph = g; frame; end
end
