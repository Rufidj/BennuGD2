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

#endif
