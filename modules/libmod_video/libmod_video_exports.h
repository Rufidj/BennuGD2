#ifndef __LIBMOD_VIDEO_EXPORTS
#define __LIBMOD_VIDEO_EXPORTS

/* ----------------------------------------------------------------- */

#include "bgddl.h"
#include "libmod_video.h"

/* ----------------------------------------------------------------- */

DLSYSFUNCS __bgdexport( libmod_video, functions_exports )[] = {
    FUNC( "VIDEO_OPEN"        , "S"   , TYPE_INT, libmod_video_open        ),
    FUNC( "VIDEO_OPEN_H"      , "SS"  , TYPE_INT, libmod_video_open_h      ),
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
    FUNC( "VIDEO_FRAMES"      , "I"   , TYPE_INT, libmod_video_frames      ),
    FUNC( "VIDEO_USE_HW"      , "I"   , TYPE_INT, libmod_video_use_hw      ),
    FUNC( "VIDEO_HW"          , "I"   , TYPE_INT, libmod_video_hw          ),
    FUNC( "VIDEO_SUBTITLE_TRACKS", "I"  , TYPE_INT   , libmod_video_subtitle_tracks ),
    FUNC( "VIDEO_SUBTITLE_LANG"  , "II" , TYPE_STRING, libmod_video_subtitle_lang   ),
    FUNC( "VIDEO_SUBTITLE_SELECT", "II" , TYPE_INT   , libmod_video_subtitle_select ),
    FUNC( "VIDEO_SUBTITLE_LOAD"  , "IS" , TYPE_INT   , libmod_video_subtitle_load   ),
    FUNC( "VIDEO_SUBTITLE_TEXT"  , "I"  , TYPE_STRING, libmod_video_subtitle_text   ),

    FUNC( "VIDEO_AUDIO_TRACKS"   , "I"  , TYPE_INT   , libmod_video_audio_tracks    ),
    FUNC( "VIDEO_AUDIO_LANG"     , "II" , TYPE_STRING, libmod_video_audio_lang      ),
    FUNC( "VIDEO_AUDIO_SELECT"   , "II" , TYPE_INT   , libmod_video_audio_select    ),

    FUNC( "VIDEO_IS_AUDIO_ONLY"  , "I"  , TYPE_INT   , libmod_video_is_audio_only  ),
    FUNC( "VIDEO_SPECTRUM"       , "II" , TYPE_INT   , libmod_video_spectrum        ),
    FUNC( "VIDEO_WAVE"           , "II" , TYPE_INT   , libmod_video_wave            ),

    FUNC( "HTTP_FETCH"           , "SSSS", TYPE_INT   , libmod_video_http_fetch     ),
    FUNC( "HTTP_FETCH_BEGIN"     , "SSSS", TYPE_INT   , libmod_video_http_fetch_begin ),
    FUNC( "HTTP_FETCH_POLL"      , ""   , TYPE_INT   , libmod_video_http_fetch_poll  ),
    FUNC( "HTTP_PROGRESS"        , ""   , TYPE_INT   , libmod_video_http_progress    ),
    FUNC( "HTTP_TOTAL"           , ""   , TYPE_INT   , libmod_video_http_total       ),
    FUNC( "VIDEO_DOWNLOAD_BEGIN"   , "SSS", TYPE_INT    , libmod_video_download_begin   ),
    FUNC( "VIDEO_DOWNLOAD_CONVERT_BEGIN", "SSSS", TYPE_INT , libmod_video_download_convert_begin ),
    FUNC( "VIDEO_DOWNLOAD_AUDIO_BEGIN", "SSS", TYPE_INT  , libmod_video_download_audio_begin ),
    FUNC( "VIDEO_DOWNLOAD_PATH"    , ""   , TYPE_STRING , libmod_video_download_path    ),
    FUNC( "VIDEO_DOWNLOAD_POLL"    , ""   , TYPE_INT    , libmod_video_download_poll    ),
    FUNC( "VIDEO_DOWNLOAD_PERCENT" , ""   , TYPE_INT    , libmod_video_download_percent ),
    FUNC( "VIDEO_DOWNLOAD_CANCEL"  , ""   , TYPE_INT    , libmod_video_download_cancel  ),
    FUNC( "VIDEO_DOWNLOAD_ERROR"   , ""   , TYPE_STRING , libmod_video_download_error   ),
    FUNC( "HTTP_LENGTH"          , ""   , TYPE_INT   , libmod_video_http_length    ),
    FUNC( "HTTP_READ"            , "PI" , TYPE_INT   , libmod_video_http_read      ),
    FUNC( "HTTP_SAVE"            , "S"  , TYPE_INT   , libmod_video_http_save      ),
    FUNC( "HTTP_WRITE"           , "SS" , TYPE_INT   , libmod_video_http_write     ),
    FUNC( "HTTP_TRACE"           , "S"  , TYPE_INT   , libmod_video_http_trace     ),
    FUNC( "HTTP_ERROR"           , ""   , TYPE_STRING, libmod_video_http_error     ),

    FUNC( 0                   , 0     , 0       , 0                        )
};

/* ----------------------------------------------------------------- */

void __bgdexport( libmod_video, module_initialize )();
void __bgdexport( libmod_video, module_finalize )();

/* ----------------------------------------------------------------- */

#endif
