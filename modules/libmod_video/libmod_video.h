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
 *    VIDEO_USE_GPU(enable)                   colour conversion (yuv420p -> RGB) on the GPU for videos opened
 *                                            afterwards; default 1, falls back to software when unavailable
 *    VIDEO_GPU(handle)                       1 if that video is converted on the GPU
 *    VIDEO_FRAMES(handle)                    pictures actually shown so far (late ones are dropped)
 *    VIDEO_USE_HW(enable)                    PS5: decode H.264/HEVC on the console's video decoder (default 1)
 *    VIDEO_HW(handle)                        1 if that video is decoded by the console's decoder
 *
 *  Subtitles (text tracks: SRT, ASS/SSA, WebVTT, mov_text; image tracks are ignored). The module
 *  does not draw them: the script shows VIDEO_SUBTITLE_TEXT() with its own fonts.
 *    VIDEO_SUBTITLE_TRACKS(handle)           -> number of tracks (embedded + external)
 *    VIDEO_SUBTITLE_LANG(handle, track)      -> "eng", "eng - Title", ... ("" if unknown)
 *    VIDEO_SUBTITLE_SELECT(handle, track)    -> select a track, -1 turns subtitles off (off by default)
 *    VIDEO_SUBTITLE_LOAD(handle, file)       -> load an external subtitle file as a new track (selected)
 *    VIDEO_SUBTITLE_TEXT(handle)             -> text to show right now ("" when none), lines joined by \n
 *
 *  Audio tracks (multiple language tracks in the same file). Switching re-opens the audio
 *  decoder/resampler and may be heard as a brief gap; the video keeps playing.
 *    VIDEO_AUDIO_TRACKS(handle)              -> number of audio tracks
 *    HTTP_FETCH(url, headers, method, body) -> status (200 ok, 400/401/403/404/500 http errors, 0 = no connection; see HTTP_ERROR())
 *    HTTP_FETCH_BEGIN(url, headers, method, body) -> 0 started (in the background); HTTP_FETCH_POLL() -> -1 running, else the status;
 *    HTTP_PROGRESS() -> bytes received so far; HTTP_TOTAL() -> bytes expected (0 = unknown)
 *    VIDEO_DOWNLOAD_BEGIN(source, headers, out_path) -> 0 started (copies the best video + audio into one .mp4 in the background);
 *    VIDEO_DOWNLOAD_POLL() -> -1 running, 0 done, -2 cancelled, -10 failed; VIDEO_DOWNLOAD_PERCENT(); VIDEO_DOWNLOAD_CANCEL(); VIDEO_DOWNLOAD_ERROR()
 *    HTTP_LENGTH() -> bytes of the answer; HTTP_READ(pointer, max) -> copies them; HTTP_ERROR() -> text of the last failure
 *    HTTP_WRITE(path, text) -> 1 if the text was written to that file
 *    HTTP_SAVE(path) -> 1 if the last answer was written to that file
 *    (http:// and https://, redirects followed, 15 s timeouts; headers = "Name: value\r\n" lines)
 *    VIDEO_AUDIO_LANG(handle, track)         -> "eng", "eng - Title", ... ("" if unknown)
 *    VIDEO_AUDIO_SELECT(handle, track)       -> switch to that track
 *
 *  Music: files with no picture open as audio-only (VIDEO_GRAPH is then a 1x1 graph). For visualisers:
 *    VIDEO_IS_AUDIO_ONLY(handle)             -> 1 if there is no video stream
 *    VIDEO_SPECTRUM(handle, band)            -> 0..255 level of band 0..31 (40 Hz - 16 kHz, log spaced); 0 when paused
 *    VIDEO_WAVE(handle, i)                   -> -128..127 waveform sample, i = 0..255
 */
#ifndef __LIBMOD_VIDEO_H
#define __LIBMOD_VIDEO_H

#include "bgddl.h"
#include <stdint.h>

extern int64_t libmod_video_spectrum( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_wave( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_is_audio_only( INSTANCE * my, int64_t * params );
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
extern int64_t libmod_video_use_gpu( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_frames( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_use_hw( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_hw( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_gpu( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_volume( INSTANCE * my, int64_t * params );

extern int64_t libmod_video_subtitle_tracks( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_subtitle_lang( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_subtitle_select( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_subtitle_load( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_subtitle_text( INSTANCE * my, int64_t * params );

extern int64_t libmod_video_audio_tracks( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_audio_lang( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_http_fetch( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_http_fetch_begin( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_http_fetch_poll( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_http_progress( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_download_begin( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_download_convert_begin( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_download_audio_begin( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_download_path( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_download_poll( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_download_percent( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_download_cancel( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_download_error( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_http_total( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_http_length( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_http_read( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_http_error( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_http_save( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_http_write( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_http_trace( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_audio_select( INSTANCE * my, int64_t * params );

extern int64_t libmod_video_torrent_cache( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_torrent_add( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_torrent_remove( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_torrent_state( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_torrent_error( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_torrent_name( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_torrent_files( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_torrent_file_name( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_torrent_file_size( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_torrent_main( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_torrent_select( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_torrent_url( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_torrent_peers( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_torrent_rate( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_torrent_percent( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_torrent_buffered( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_torrent_ready( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_torrent_downloaded( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_torrent_options( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_torrent_ahead( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_torrent_upload_limit( INSTANCE * my, int64_t * params );

extern int64_t libmod_video_upload_start( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_upload_stop( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_upload_files( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_upload_bytes( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_upload_percent( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_upload_current( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_upload_ip( INSTANCE * my, int64_t * params );
extern int64_t libmod_video_upload_error( INSTANCE * my, int64_t * params );

#endif
