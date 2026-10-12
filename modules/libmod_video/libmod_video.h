/*
 *  libmod_video - FFmpeg video playback module for BennuGD2.
 *
 *  Independent of BennuGD2's own sources: it only uses the public module API
 *  (bgddl.h) and libbggfx's graph/object hooks, the same ones libmod_3d uses.
 *
 *  Script API (all handles are small ints, 0 = error):
 *    VIDEO_OPEN(file)            -> handle     open a video (any format FFmpeg knows)
 *    VIDEO_GRAPH(handle)         -> graph      graph (file 0) that always shows the current frame
 *    VIDEO_PLAY(handle, loop)                  start / resume; loop=1 repeats forever
 *    VIDEO_PAUSE(handle, paused)
 *    VIDEO_STOP(handle)                        stop and rewind to the first frame
 *    VIDEO_SEEK(handle, ms)
 *    VIDEO_CLOSE(handle)
 *    VIDEO_WIDTH / VIDEO_HEIGHT(handle)
 *    VIDEO_DURATION / VIDEO_POSITION(handle)   milliseconds
 *    VIDEO_IS_PLAYING / VIDEO_IS_FINISHED(handle)
 *    VIDEO_VOLUME(handle, 0..128)
 *
 *  Subtitles (text tracks: SRT, ASS/SSA, WebVTT, mov_text; image tracks are ignored). The module
 *  does not draw them: the script shows VIDEO_SUBTITLE_TEXT() with its own fonts.
 *    VIDEO_SUBTITLE_TRACKS(handle)           -> number of tracks (embedded + external)
 *    VIDEO_SUBTITLE_LANG(handle, track)      -> "eng", "eng - Title", ... ("" if unknown)
 *    VIDEO_SUBTITLE_SELECT(handle, track)    -> select a track, -1 turns subtitles off (off by default)
 *    VIDEO_SUBTITLE_LOAD(handle, file)       -> load an external subtitle file as a new track (selected)
 *    VIDEO_SUBTITLE_TEXT(handle)             -> text to show right now ("" when none), lines joined by \n
 */
#ifndef __LIBMOD_VIDEO_H
#define __LIBMOD_VIDEO_H

#include "bgddl.h"
#include <stdint.h>

extern int64_t libmod_video_open( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_graph( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_play( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_pause( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_stop( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_seek( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_close( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_width( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_height( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_duration( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_position( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_is_playing( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_is_finished( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_volume( INSTANCE * my, int64_t * params );

extern int64_t libmod_video_subtitle_tracks( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_subtitle_lang( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_subtitle_select( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_subtitle_load( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_subtitle_text( INSTANCE * my, int64_t * params );

#endif
