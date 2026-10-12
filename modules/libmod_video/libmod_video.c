/*
 *  libmod_video - FFmpeg video playback module for BennuGD2.  See libmod_video.h.
 *
 *  Design
 *    - One decoder thread per open video: demux + decode + convert.  Video frames are
 *      converted to RGBA into a small ring of buffers; audio is resampled to whatever
 *      SDL_mixer is running at and queued in a byte ring.
 *    - The main (GL) thread never decodes: a per-frame object hook (gr_new_object, the
 *      same one libmod_3d uses) picks the frame that is due and uploads it into the
 *      video's GRAPH with GPU_UpdateImageBytes().
 *    - Audio goes out through Mix_HookMusic(), so it shares the device SDL_mixer already
 *      opened (a second device is not guaranteed to work on every platform).  Only one
 *      video at a time owns the audio; the others run on a wall clock, silent.
 *    - The master clock is the audio clock when the video owns the audio, the wall
 *      clock otherwise.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <SDL.h>
#include <SDL_mixer.h>
#include "SDL_gpu.h"

#include "bgddl.h"
#include "dlvaracc.h"
#include "xstrings.h"
#include "libbggfx.h"

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>

#include "libmod_video.h"

/* ------------------------------------------------------------------------ */

#define MAX_VIDEOS   16
#define VQ_SIZE      8                 /* decoded video frames kept ahead      */
#define AUDIO_RING_S 2                 /* seconds of audio buffered            */

typedef struct {
    uint8_t * rgba;
    double    pts;                     /* seconds                               */
} VFRAME;

typedef struct {
    int             used;

    AVFormatContext * fmt;
    AVCodecContext  * vctx, * actx;
    int             vstream, astream;
    struct SwsContext * sws;
    struct SwrContext * swr;
    AVFrame         * frame;           /* decoder output (decoder thread only)  */
    double          vtb, atb;          /* time bases, seconds                   */

    int             w, h;
    double          duration;          /* seconds, <= 0 if unknown              */

    GRAPH           * graph;
    int64_t         graph_code;

    /* shared state, guarded by mx */
    SDL_mutex       * mx;
    SDL_cond        * cond;
    VFRAME          vq[VQ_SIZE];
    int             vq_r, vq_n;
    uint8_t         * abuf;            /* audio ring (interleaved, mixer format)*/
    size_t          acap, ar, an;
    double          a_pts_end;         /* pts of the end of the queued audio    */
    int             a_rate, a_ch, a_bps;   /* mixer spec in use                 */
    int             has_audio;         /* decoding audio for the mixer hook     */

    int             playing, paused, loop, eof, finished, quit;
    int             seek_req;          double seek_to;
    int             shown_first;
    double          volume;            /* 0..1                                  */

    /* wall clock fallback / pause bookkeeping */
    double          clk_base;          /* media time at clk_t0                  */
    Uint32          clk_t0;
    double          last_time;         /* last media time reported              */

    SDL_Thread      * thr;
} VIDEO;

static VIDEO       g_vid[MAX_VIDEOS + 1];       /* handle = index, 0 unused     */
static VIDEO *     g_audio_owner = NULL;

/* Graphs of closed videos are freed a few frames later, not immediately: on the PS5 the GPU runs
   draw calls after the CPU has queued them, and recreating a texture right after freeing one
   (VIDEO_CLOSE followed by VIDEO_OPEN) could leave the new video's texture black. */
#define ZOMBIE_FRAMES 30
#define MAX_ZOMBIES   32
static struct { int64_t code; int frames; } g_zombie[MAX_ZOMBIES];

static void zombie_add( int64_t code ) {
    for ( int i = 0; i < MAX_ZOMBIES; i++ ) if ( !g_zombie[i].code ) { g_zombie[i].code = code; g_zombie[i].frames = ZOMBIE_FRAMES; return; }
    grlib_unload_map( 0, code );                       /* table full: free now */
}
static void zombie_tick( int all ) {
    for ( int i = 0; i < MAX_ZOMBIES; i++ ) if ( g_zombie[i].code && ( all || --g_zombie[i].frames <= 0 ) ) {
        grlib_unload_map( 0, g_zombie[i].code ); g_zombie[i].code = 0;
    }
}
static int64_t     g_obj_id = 0;
static int         g_mix_ok = 0;
static int         g_mix_rate = 44100, g_mix_ch = 2;
static Uint16      g_mix_fmt = AUDIO_S16SYS;

/* ------------------------------------------------------------------------ */

static VIDEO * vid_get( int64_t h ) {
    if ( h < 1 || h > MAX_VIDEOS || !g_vid[h].used ) return NULL;
    return &g_vid[h];
}

/* media time "now" (call with mx held or not; reads are racy but monotonic enough) */
static double vid_clock( VIDEO * v ) {
    if ( v->has_audio && g_audio_owner == v ) {
        double queued = ( double ) v->an / ( double ) ( v->a_rate * v->a_ch * v->a_bps );
        double t = v->a_pts_end - queued;
        return t < 0 ? 0 : t;
    }
    if ( !v->playing || v->paused ) return v->clk_base;
    return v->clk_base + ( SDL_GetTicks() - v->clk_t0 ) / 1000.0;
}

static void vid_clock_freeze( VIDEO * v ) { v->clk_base = vid_clock( v ); v->clk_t0 = SDL_GetTicks(); }

/* ---- audio output (SDL_mixer music hook) ------------------------------- */

static void mix_hook( void * udata, Uint8 * stream, int len ) {
    VIDEO * v = g_audio_owner;
    ( void ) udata;
    memset( stream, 0, len );
    if ( !v || !v->has_audio ) return;

    SDL_LockMutex( v->mx );
    if ( v->playing && !v->paused ) {
        size_t n = ( size_t ) len < v->an ? ( size_t ) len : v->an;
        size_t first = v->acap - v->ar; if ( first > n ) first = n;
        memcpy( stream, v->abuf + v->ar, first );
        if ( n > first ) memcpy( stream + first, v->abuf, n - first );
        v->ar = ( v->ar + n ) % v->acap;
        v->an -= n;

        if ( v->volume < 0.999 && v->a_bps == 2 ) {
            int16_t * s = ( int16_t * ) stream; int cnt = ( int ) ( n / 2 );
            int vol = ( int ) ( v->volume * 256.0 );
            for ( int i = 0; i < cnt; i++ ) s[i] = ( int16_t ) ( ( s[i] * vol ) >> 8 );
        }
        SDL_CondSignal( v->cond );
    }
    SDL_UnlockMutex( v->mx );
}

static void audio_release( VIDEO * v ) {
    if ( g_audio_owner == v ) {
        Mix_HookMusic( NULL, NULL );
        g_audio_owner = NULL;
    }
}

/* ---- decoder thread ---------------------------------------------------- */

static void vq_clear( VIDEO * v ) {
    v->vq_r = 0; v->vq_n = 0;
    v->an = 0; v->ar = 0;
}

static int push_video_frame( VIDEO * v, AVFrame * f ) {
    double pts = ( f->best_effort_timestamp == AV_NOPTS_VALUE ) ? 0.0 : f->best_effort_timestamp * v->vtb;

    SDL_LockMutex( v->mx );
    while ( !v->quit && !v->seek_req && v->vq_n >= VQ_SIZE ) SDL_CondWaitTimeout( v->cond, v->mx, 10 );
    if ( v->quit || v->seek_req ) { SDL_UnlockMutex( v->mx ); return 0; }
    int slot = ( v->vq_r + v->vq_n ) % VQ_SIZE;
    SDL_UnlockMutex( v->mx );

    uint8_t * dst[4] = { v->vq[slot].rgba, NULL, NULL, NULL };
    int dst_ls[4] = { v->w * 4, 0, 0, 0 };
    sws_scale( v->sws, ( const uint8_t * const * ) f->data, f->linesize, 0, f->height, dst, dst_ls );
    v->vq[slot].pts = pts;

    SDL_LockMutex( v->mx );
    if ( !v->seek_req ) v->vq_n++;
    SDL_UnlockMutex( v->mx );
    return 1;
}

static void push_audio( VIDEO * v, AVFrame * f ) {
    if ( !v->has_audio || !v->swr ) return;

    int out_cap = swr_get_out_samples( v->swr, f->nb_samples );
    if ( out_cap < f->nb_samples ) out_cap = f->nb_samples * 2;
    size_t bytes_cap = ( size_t ) out_cap * v->a_ch * v->a_bps;
    uint8_t * tmp = ( uint8_t * ) malloc( bytes_cap );
    if ( !tmp ) return;
    uint8_t * out[1] = { tmp };
    int got = swr_convert( v->swr, out, out_cap, ( const uint8_t ** ) f->extended_data, f->nb_samples );
    if ( got <= 0 ) { free( tmp ); return; }
    size_t bytes = ( size_t ) got * v->a_ch * v->a_bps;

    double pts = ( f->best_effort_timestamp == AV_NOPTS_VALUE ) ? v->a_pts_end : f->best_effort_timestamp * v->atb;

    size_t off = 0;
    while ( off < bytes && !v->quit ) {
        SDL_LockMutex( v->mx );
        while ( !v->quit && !v->seek_req && v->acap - v->an == 0 ) SDL_CondWaitTimeout( v->cond, v->mx, 10 );
        if ( v->quit || v->seek_req ) { SDL_UnlockMutex( v->mx ); break; }
        size_t room = v->acap - v->an, n = bytes - off < room ? bytes - off : room;
        size_t w = ( v->ar + v->an ) % v->acap, first = v->acap - w; if ( first > n ) first = n;
        memcpy( v->abuf + w, tmp + off, first );
        if ( n > first ) memcpy( v->abuf, tmp + off + first, n - first );
        v->an += n; off += n;
        SDL_UnlockMutex( v->mx );
    }
    SDL_LockMutex( v->mx );
    v->a_pts_end = pts + ( double ) got / v->a_rate;
    SDL_UnlockMutex( v->mx );
    free( tmp );
}

static void drain_decoder( VIDEO * v, AVCodecContext * c, int is_video ) {
    int r;
    while ( !v->quit && !v->seek_req && ( r = avcodec_receive_frame( c, v->frame ) ) >= 0 ) {
        if ( is_video ) push_video_frame( v, v->frame ); else push_audio( v, v->frame );
        av_frame_unref( v->frame );
    }
}

static int decode_thread( void * arg ) {
    VIDEO * v = ( VIDEO * ) arg;
    AVPacket * pkt = av_packet_alloc();

    while ( !v->quit ) {
        /* seek requests */
        SDL_LockMutex( v->mx );
        if ( v->seek_req ) {
            double to = v->seek_to; v->seek_req = 0;
            SDL_UnlockMutex( v->mx );
            int64_t ts = ( int64_t ) ( to * AV_TIME_BASE );
            avformat_seek_file( v->fmt, -1, INT64_MIN, ts, ts, 0 );
            if ( v->vctx ) avcodec_flush_buffers( v->vctx );
            if ( v->actx ) avcodec_flush_buffers( v->actx );
            SDL_LockMutex( v->mx );
            vq_clear( v );
            v->eof = 0; v->finished = 0; v->a_pts_end = to;
            v->clk_base = to; v->clk_t0 = SDL_GetTicks();
            v->shown_first = 0;
            SDL_UnlockMutex( v->mx );
            continue;
        }
        /* nothing to do while stopped/paused and the first frame is already queued */
        if ( ( !v->playing || v->paused || v->eof ) && ( v->vq_n > 0 || v->eof ) ) {
            SDL_CondWaitTimeout( v->cond, v->mx, 20 );
            SDL_UnlockMutex( v->mx );
            continue;
        }
        SDL_UnlockMutex( v->mx );

        int r = av_read_frame( v->fmt, pkt );
        if ( r < 0 ) {
            /* flush decoders, then end / loop */
            if ( v->vctx ) { avcodec_send_packet( v->vctx, NULL ); drain_decoder( v, v->vctx, 1 ); avcodec_flush_buffers( v->vctx ); }
            if ( v->actx ) { avcodec_send_packet( v->actx, NULL ); drain_decoder( v, v->actx, 0 ); avcodec_flush_buffers( v->actx ); }
            SDL_LockMutex( v->mx );
            if ( v->loop && v->playing ) { v->seek_req = 1; v->seek_to = 0; }
            else v->eof = 1;
            SDL_UnlockMutex( v->mx );
            continue;
        }
        if ( pkt->stream_index == v->vstream && v->vctx ) {
            int sr = avcodec_send_packet( v->vctx, pkt );
            if ( sr >= 0 ) drain_decoder( v, v->vctx, 1 );

        } else if ( pkt->stream_index == v->astream && v->actx && v->has_audio ) {
            if ( avcodec_send_packet( v->actx, pkt ) >= 0 ) drain_decoder( v, v->actx, 0 );
        }
        av_packet_unref( pkt );
    }
    av_packet_free( &pkt );
    return 0;
}

/* ---- per-frame hook (main / GL thread) --------------------------------- */

static void vid_upload_slot( VIDEO * v, int slot ) {
    if ( v->graph && v->graph->tex )
        GPU_UpdateImageBytes( v->graph->tex, NULL, v->vq[slot].rgba, v->w * 4 );
}

static void vid_tick( VIDEO * v ) {
    int pick = -1;

    SDL_LockMutex( v->mx );
    double now = vid_clock( v );
    v->last_time = now;

    if ( !v->shown_first && v->vq_n > 0 ) {            /* poster frame before play() */
        pick = v->vq_r; v->shown_first = 1;
        vid_upload_slot( v, pick );
        pick = -1;
    }
    if ( v->playing && !v->paused ) {
        /* drop frames that are already late, keep the newest due one */
        while ( v->vq_n > 0 && v->vq[v->vq_r].pts <= now + 0.004 ) {
            pick = v->vq_r;
            v->vq_r = ( v->vq_r + 1 ) % VQ_SIZE; v->vq_n--;
            if ( v->vq_n == 0 ) break;
        }
        if ( pick >= 0 ) { vid_upload_slot( v, pick ); v->shown_first = 1; SDL_CondSignal( v->cond ); }

        if ( v->eof && v->vq_n == 0 && ( !v->has_audio || v->an == 0 ) ) {
            v->playing = 0; v->finished = 1;
            vid_clock_freeze( v );
        }
    }
    SDL_UnlockMutex( v->mx );
}

static int obj_info( void * what, REGION * clip, int64_t * key, int64_t * ready ) {
    ( void ) what; ( void ) clip;
    *key = 0; *ready = 0;                               /* never drawn by the object system */
    for ( int i = 1; i <= MAX_VIDEOS; i++ ) if ( g_vid[i].used ) vid_tick( &g_vid[i] );
    zombie_tick( 0 );
    return 1;
}

/* ---- open / close ------------------------------------------------------ */

static enum AVSampleFormat av_fmt_for( Uint16 f, int * bps ) {
    switch ( f ) {
        case AUDIO_F32SYS: *bps = 4; return AV_SAMPLE_FMT_FLT;
        case AUDIO_S32SYS: *bps = 4; return AV_SAMPLE_FMT_S32;
        default:           *bps = 2; return AV_SAMPLE_FMT_S16;
    }
}

static void vid_free( VIDEO * v ) {
    if ( v->thr ) {
        SDL_LockMutex( v->mx ); v->quit = 1; SDL_CondBroadcast( v->cond ); SDL_UnlockMutex( v->mx );
        SDL_WaitThread( v->thr, NULL );
        v->thr = NULL;
    }
    audio_release( v );
    for ( int i = 0; i < VQ_SIZE; i++ ) free( v->vq[i].rgba );
    free( v->abuf );
    if ( v->sws ) sws_freeContext( v->sws );
    if ( v->swr ) swr_free( &v->swr );
    if ( v->frame ) av_frame_free( &v->frame );
    if ( v->vctx ) avcodec_free_context( &v->vctx );
    if ( v->actx ) avcodec_free_context( &v->actx );
    if ( v->fmt ) avformat_close_input( &v->fmt );
    if ( v->graph_code ) zombie_add( v->graph_code );
    if ( v->cond ) SDL_DestroyCond( v->cond );
    if ( v->mx ) SDL_DestroyMutex( v->mx );
    memset( v, 0, sizeof( *v ) );
}

static AVCodecContext * open_codec( AVFormatContext * fmt, int idx ) {
    AVCodecParameters * par = fmt->streams[idx]->codecpar;
    const AVCodec * codec = avcodec_find_decoder( par->codec_id );
    if ( !codec ) return NULL;
    AVCodecContext * c = avcodec_alloc_context3( codec );
    if ( !c ) return NULL;
    if ( avcodec_parameters_to_context( c, par ) < 0 ) { avcodec_free_context( &c ); return NULL; }
    c->thread_count = 0;                               /* let FFmpeg pick */
    if ( avcodec_open2( c, codec, NULL ) < 0 ) { avcodec_free_context( &c ); return NULL; }
    return c;
}

int64_t libmod_video_open( INSTANCE * my, int64_t * params ) {
    const char * path = ( const char * ) string_get( params[0] );
    int h;
    for ( h = 1; h <= MAX_VIDEOS; h++ ) if ( !g_vid[h].used ) break;
    if ( h > MAX_VIDEOS ) { string_discard( params[0] ); fprintf( stderr, "VIDEO_OPEN: too many open videos\n" ); return 0; }

    VIDEO * v = &g_vid[h];
    memset( v, 0, sizeof( *v ) );
    v->used = 1; v->volume = 1.0; v->vstream = v->astream = -1;

    if ( avformat_open_input( &v->fmt, path, NULL, NULL ) < 0 ) { fprintf( stderr, "VIDEO_OPEN: cannot open %s\n", path ); goto fail; }
    if ( avformat_find_stream_info( v->fmt, NULL ) < 0 ) { fprintf( stderr, "VIDEO_OPEN: no stream info in %s\n", path ); goto fail; }

    v->vstream = av_find_best_stream( v->fmt, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0 );
    v->astream = av_find_best_stream( v->fmt, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0 );
    if ( v->vstream < 0 ) { fprintf( stderr, "VIDEO_OPEN: %s has no video stream\n", path ); goto fail; }

    v->vctx = open_codec( v->fmt, v->vstream );
    if ( !v->vctx ) { fprintf( stderr, "VIDEO_OPEN: no decoder for the video in %s\n", path ); goto fail; }
    v->w = v->vctx->width; v->h = v->vctx->height;
    v->vtb = av_q2d( v->fmt->streams[v->vstream]->time_base );
    v->duration = v->fmt->duration > 0 ? v->fmt->duration / ( double ) AV_TIME_BASE : 0;

    v->sws = sws_getContext( v->w, v->h, v->vctx->pix_fmt, v->w, v->h, AV_PIX_FMT_RGBA, SWS_BILINEAR, NULL, NULL, NULL );
    if ( !v->sws ) goto fail;
    for ( int i = 0; i < VQ_SIZE; i++ ) { v->vq[i].rgba = ( uint8_t * ) calloc( 1, ( size_t ) v->w * v->h * 4 ); if ( !v->vq[i].rgba ) goto fail; }
    v->frame = av_frame_alloc();

    /* audio, if SDL_mixer is up */
    g_mix_ok = Mix_QuerySpec( &g_mix_rate, &g_mix_fmt, &g_mix_ch ) ? 1 : 0;
    if ( g_mix_ok && v->astream >= 0 ) {
        v->actx = open_codec( v->fmt, v->astream );
        if ( v->actx ) {
            v->atb = av_q2d( v->fmt->streams[v->astream]->time_base );
            v->a_rate = g_mix_rate; v->a_ch = g_mix_ch;
            enum AVSampleFormat sf = av_fmt_for( g_mix_fmt, &v->a_bps );
            AVChannelLayout out_l; av_channel_layout_default( &out_l, v->a_ch );
            if ( swr_alloc_set_opts2( &v->swr, &out_l, sf, v->a_rate, &v->actx->ch_layout, v->actx->sample_fmt, v->actx->sample_rate, 0, NULL ) >= 0 && swr_init( v->swr ) >= 0 ) {
                v->acap = ( size_t ) v->a_rate * v->a_ch * v->a_bps * AUDIO_RING_S;
                v->abuf = ( uint8_t * ) malloc( v->acap );
                v->has_audio = v->abuf != NULL;
            }
            av_channel_layout_uninit( &out_l );
        }
    }

    /* the graph that shows the video */
    {
        SDL_Surface * s = SDL_CreateRGBSurfaceWithFormat( 0, v->w, v->h, 32, SDL_PIXELFORMAT_ABGR8888 );
        if ( !s ) goto fail;
        SDL_memset( s->pixels, 0, ( size_t ) s->pitch * s->h );
        v->graph = bitmap_new( 0, 0, 0, s );
        SDL_FreeSurface( s );
        if ( !v->graph ) goto fail;
        v->graph->code = bitmap_next_code();
        v->graph_code = v->graph->code;
        grlib_add_map( 0, v->graph );
    }

    v->mx = SDL_CreateMutex(); v->cond = SDL_CreateCond();
    if ( !g_obj_id ) g_obj_id = gr_new_object( 0, obj_info, NULL, NULL );
    v->thr = SDL_CreateThread( decode_thread, "video", v );
    if ( !v->thr ) goto fail;

    string_discard( params[0] );
    return h;

fail:
    string_discard( params[0] );
    vid_free( v );
    return 0;
}

int64_t libmod_video_graph( INSTANCE * my, int64_t * params ) { VIDEO * v = vid_get( params[0] ); return v ? v->graph_code : 0; }
int64_t libmod_video_width( INSTANCE * my, int64_t * params ) { VIDEO * v = vid_get( params[0] ); return v ? v->w : 0; }
int64_t libmod_video_height( INSTANCE * my, int64_t * params ) { VIDEO * v = vid_get( params[0] ); return v ? v->h : 0; }
int64_t libmod_video_duration( INSTANCE * my, int64_t * params ) { VIDEO * v = vid_get( params[0] ); return v ? ( int64_t ) ( v->duration * 1000.0 ) : 0; }
int64_t libmod_video_position( INSTANCE * my, int64_t * params ) { VIDEO * v = vid_get( params[0] ); return v ? ( int64_t ) ( v->last_time * 1000.0 ) : 0; }
int64_t libmod_video_is_playing( INSTANCE * my, int64_t * params ) { VIDEO * v = vid_get( params[0] ); return v && v->playing && !v->paused; }
int64_t libmod_video_is_finished( INSTANCE * my, int64_t * params ) { VIDEO * v = vid_get( params[0] ); return v ? v->finished : 0; }

int64_t libmod_video_volume( INSTANCE * my, int64_t * params ) {
    VIDEO * v = vid_get( params[0] ); if ( !v ) return 0;
    int64_t vol = params[1]; if ( vol < 0 ) vol = 0; if ( vol > 128 ) vol = 128;
    v->volume = vol / 128.0;
    return 1;
}

int64_t libmod_video_play( INSTANCE * my, int64_t * params ) {
    VIDEO * v = vid_get( params[0] ); if ( !v ) return 0;
    SDL_LockMutex( v->mx );
    if ( v->finished ) { v->seek_req = 1; v->seek_to = 0; v->finished = 0; v->eof = 0; }
    vid_clock_freeze( v );
    v->loop = params[1] ? 1 : 0;
    v->paused = 0; v->playing = 1;
    if ( v->has_audio && !g_audio_owner ) { g_audio_owner = v; Mix_HookMusic( mix_hook, NULL ); }
    vid_clock_freeze( v );
    SDL_CondBroadcast( v->cond );
    SDL_UnlockMutex( v->mx );
    return 1;
}

int64_t libmod_video_pause( INSTANCE * my, int64_t * params ) {
    VIDEO * v = vid_get( params[0] ); if ( !v ) return 0;
    SDL_LockMutex( v->mx );
    vid_clock_freeze( v );
    v->paused = params[1] ? 1 : 0;
    vid_clock_freeze( v );
    SDL_CondBroadcast( v->cond );
    SDL_UnlockMutex( v->mx );
    return 1;
}

int64_t libmod_video_stop( INSTANCE * my, int64_t * params ) {
    VIDEO * v = vid_get( params[0] ); if ( !v ) return 0;
    SDL_LockMutex( v->mx );
    v->playing = 0; v->paused = 0; v->loop = 0; v->finished = 0;
    v->seek_req = 1; v->seek_to = 0;
    SDL_CondBroadcast( v->cond );
    SDL_UnlockMutex( v->mx );
    audio_release( v );
    return 1;
}

int64_t libmod_video_seek( INSTANCE * my, int64_t * params ) {
    VIDEO * v = vid_get( params[0] ); if ( !v ) return 0;
    double to = params[1] / 1000.0;
    if ( to < 0 ) to = 0;
    if ( v->duration > 0 && to > v->duration ) to = v->duration;
    SDL_LockMutex( v->mx );
    v->seek_req = 1; v->seek_to = to; v->finished = 0;
    SDL_CondBroadcast( v->cond );
    SDL_UnlockMutex( v->mx );
    return 1;
}

int64_t libmod_video_close( INSTANCE * my, int64_t * params ) {
    VIDEO * v = vid_get( params[0] ); if ( !v ) return 0;
    vid_free( v );
    return 1;
}

/* ------------------------------------------------------------------------ */

void __bgdexport( libmod_video, module_initialize )() {
#if LIBAVFORMAT_VERSION_MAJOR < 58
    av_register_all();
#endif
    av_log_set_level( AV_LOG_ERROR );
}

void __bgdexport( libmod_video, module_finalize )() {
    for ( int i = 1; i <= MAX_VIDEOS; i++ ) if ( g_vid[i].used ) vid_free( &g_vid[i] );
    zombie_tick( 1 );
    if ( g_obj_id ) { gr_destroy_object( g_obj_id ); g_obj_id = 0; }
}

#include "libmod_video_exports.h"
