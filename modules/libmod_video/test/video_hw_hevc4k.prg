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
    int maxgap;
    int last;
    int lastpos;
    int stalls;
begin
    set_mode(1280, 720);
    for (i = 1; i >= 0; i--)
        video_use_hw(i);
        vh = video_open("hevc4k10.mp4");
        say("HEVC4K: [" + i + "] hw_pedido=" + i + " handle=" + vh + " hw=" + video_hw(vh) + " gpu=" + video_gpu(vh) + " " + video_width(vh) + "x" + video_height(vh));
        if (vh != 0)
            g = video_graph(vh);
            shown = show(g);
            video_play(vh, 0);
            frames = 0; stalls = 0; lastpos = 0;
            t0 = get_timer();
            while (!video_is_finished(vh) and get_timer() - t0 < 9000)
                frames++;
                // fotogramas del video que se han "parado": la posicion no avanza durante varios frames
                if (frames % 10 == 0)
                    if (video_position(vh) == lastpos) stalls++; end
                    lastpos = video_position(vh);
                end
                frame;
            end
            say("HEVC4K: [" + i + "] fin motor_frames=" + frames + " ms=" + (get_timer() - t0) + " motor_fps=" + (frames * 1000 / (get_timer() - t0)) + " pos_video_ms=" + video_position(vh) + " de " + video_duration(vh) + " mostrados=" + video_frames(vh) + " paradas=" + stalls + " hw_final=" + video_hw(vh) + " terminado=" + video_is_finished(vh));
            signal(shown, s_kill);
            video_close(vh);
        end
        t0 = get_timer();
        while (get_timer() - t0 < 800) frame; end
    end
    say("HEVC4K: terminado");
    loop frame; end
end
process show(g)
begin
    x = 640; y = 360; size = 33;
    loop graph = g; frame; end
end
