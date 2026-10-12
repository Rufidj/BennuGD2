import "libmod_gfx";
import "libmod_input";
import "libmod_misc";
import "libmod_video";

global
    int vh;
    int g;
    int usegpu = 1;
begin
    set_mode(640, 480);
    usegpu = atoi(argv[1]);
    video_use_gpu(usegpu);
    vh = video_open("plain.mp4");
    say("GPUTEST: usegpu=" + usegpu + " handle=" + vh + " gpu=" + video_gpu(vh));
    g = video_graph(vh);
    show(g);
    video_play(vh, 1);
    while (timer[0] < 450) frame; end
    say("GPUTEST: fin");
end
process show(g)
begin
    x = 320; y = 240;
    loop graph = g; frame; end
end
