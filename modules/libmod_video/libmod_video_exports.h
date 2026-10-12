#ifndef __LIBMOD_VIDEO_EXPORTS
#define __LIBMOD_VIDEO_EXPORTS

/* ----------------------------------------------------------------- */

#include "bgddl.h"
#include "libmod_video.h"

/* ----------------------------------------------------------------- */

DLSYSFUNCS __bgdexport( libmod_video, functions_exports )[] = {
    FUNC( "VIDEO_OPEN"        , "S"   , TYPE_INT, libmod_video_open        ),
    FUNC( "VIDEO_GRAPH"       , "I"   , TYPE_INT, libmod_video_graph       ),
    FUNC( "VIDEO_PLAY"        , "II"  , TYPE_INT, libmod_video_play        ),
    FUNC( "VIDEO_PAUSE"       , "II"  , TYPE_INT, libmod_video_pause       ),
    FUNC( "VIDEO_STOP"        , "I"   , TYPE_INT, libmod_video_stop        ),
    FUNC( "VIDEO_SEEK"        , "II"  , TYPE_INT, libmod_video_seek        ),
    FUNC( "VIDEO_CLOSE"       , "I"   , TYPE_INT, libmod_video_close       ),
    FUNC( "VIDEO_WIDTH"       , "I"   , TYPE_INT, libmod_video_width       ),
    FUNC( "VIDEO_HEIGHT"      , "I"   , TYPE_INT, libmod_video_height      ),
    FUNC( "VIDEO_DURATION"    , "I"   , TYPE_INT, libmod_video_duration    ),
    FUNC( "VIDEO_POSITION"    , "I"   , TYPE_INT, libmod_video_position    ),
    FUNC( "VIDEO_IS_PLAYING"  , "I"   , TYPE_INT, libmod_video_is_playing  ),
    FUNC( "VIDEO_IS_FINISHED" , "I"   , TYPE_INT, libmod_video_is_finished ),
    FUNC( "VIDEO_VOLUME"      , "II"  , TYPE_INT, libmod_video_volume      ),

    FUNC( "VIDEO_USE_GPU"     , "I"   , TYPE_INT, libmod_video_use_gpu     ),
    FUNC( "VIDEO_GPU"         , "I"   , TYPE_INT, libmod_video_gpu         ),
    FUNC( "VIDEO_SUBTITLE_TRACKS", "I"  , TYPE_INT   , libmod_video_subtitle_tracks ),
    FUNC( "VIDEO_SUBTITLE_LANG"  , "II" , TYPE_STRING, libmod_video_subtitle_lang   ),
    FUNC( "VIDEO_SUBTITLE_SELECT", "II" , TYPE_INT   , libmod_video_subtitle_select ),
    FUNC( "VIDEO_SUBTITLE_LOAD"  , "IS" , TYPE_INT   , libmod_video_subtitle_load   ),
    FUNC( "VIDEO_SUBTITLE_TEXT"  , "I"  , TYPE_STRING, libmod_video_subtitle_text   ),

    FUNC( 0                   , 0     , 0       , 0                        )
};

/* ----------------------------------------------------------------- */

void __bgdexport( libmod_video, module_initialize )();
void __bgdexport( libmod_video, module_finalize )();

/* ----------------------------------------------------------------- */

#endif
