import "libmod_gfx";
import "libmod_input";
import "libmod_misc";
import "libmod_sound";
import "libmod_video";

global
    int vh;
    string last = "";
    string t;
begin
    set_mode(640, 480);
    vh = video_open("test_subs.mkv");
    say("SUBTEST: tracks=" + video_subtitle_tracks(vh) + " lang0=[" + video_subtitle_lang(vh, 0) + "]");
    say("SUBTEST: select(0)=" + video_subtitle_select(vh, 0));
    video_play(vh, 0);
    while (!video_is_finished(vh))
        t = video_subtitle_text(vh);
        if (t != last)
            say("SUBTEST @" + video_position(vh) + "ms: [" + t + "]");
            last = t;
        end
        frame;
    end
    // pista externa
    video_stop(vh);
    say("SUBTEST: load externo=" + video_subtitle_load(vh, "test_subs.srt") + " tracks=" + video_subtitle_tracks(vh));
    say("SUBTEST: fin");
end
