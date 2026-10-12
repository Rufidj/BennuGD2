import "libmod_gfx";
import "libmod_input";
import "libmod_misc";
import "libmod_video";

global
    int vh;
    int g;
    int shown;
    int frames;
    int t0;
    int i;
    int usehw;
    string names[7];
    int hwflags[7];
begin
    set_mode(1280, 720);
    names[0] = "clip720.mp4";      hwflags[0] = 1;
    names[1] = "clip720.mp4";      hwflags[1] = 0;
    names[2] = "hevc1080.mp4";     hwflags[2] = 1;
    names[3] = "hevc1080.mp4";     hwflags[3] = 0;
    names[4] = "hevc1080_10.mp4";  hwflags[4] = 1;
    names[5] = "clip4k.mp4";       hwflags[5] = 1;
    names[6] = "clip4k.mp4";       hwflags[6] = 0;
    for (i = 0; i < 7; i++)
        video_use_hw(hwflags[i]);
        vh = video_open(names[i]);
        say("PS5HW: [" + i + "] " + names[i] + " hw_pedido=" + hwflags[i] + " handle=" + vh + " hw=" + video_hw(vh) + " gpu=" + video_gpu(vh) + " " + video_width(vh) + "x" + video_height(vh));
        if (vh != 0)
            g = video_graph(vh);
            shown = show(g);
            video_play(vh, 0);
            frames = 0;
            t0 = get_timer();
            while (!video_is_finished(vh) and get_timer() - t0 < 10000)
                frames++;
                frame;
            end
            say("PS5HW: [" + i + "] fin frames=" + frames + " ms=" + (get_timer() - t0) + " fps=" + (frames * 1000 / (get_timer() - t0)) + " pos_ms=" + video_position(vh) + " hw_final=" + video_hw(vh) + " terminado=" + video_is_finished(vh));
            signal(shown, s_kill);
            video_close(vh);
        end
        t0 = get_timer();
        while (get_timer() - t0 < 800) frame; end
    end
    say("PS5HW: terminado");
    loop frame; end
end
process show(g)
begin
    x = 640; y = 360;
    loop graph = g; frame; end
end
