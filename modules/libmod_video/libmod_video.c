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
#include <stdarg.h>
#ifdef _WIN32
#include <io.h>
#define fsync _commit
#else
#include <unistd.h>
#endif
#include <math.h>

#include <SDL.h>
#include <SDL_mixer.h>
#include "SDL_gpu.h"

#include "bgddl.h"
#include "dlvaracc.h"
#include "xstrings.h"
#include "libbggfx.h"

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/bsf.h>
#include <libavutil/imgutils.h>
#include <libavutil/display.h>
#include <libavutil/dict.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/hwcontext.h>
#include <libavutil/samplefmt.h>
#include <libavutil/audio_fifo.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>

#include "libmod_video.h"
#include "torrent.h"
#include "upload.h"
#include "libmod_video_hwdec.h"
#include <time.h>
#include <pthread.h>

/* ------------------------------------------------------------------------ */

#define MAX_VIDEOS   16
#define VQ_MAX       8                 /* most decoded video frames kept ahead  */
#define VQ_BUDGET    ( ( size_t ) 96 << 20 )   /* memory the frame ring may take, per video */
#define AUDIO_RING_S 2                 /* seconds of audio buffered            */

typedef struct {
    uint8_t * rgba;
    double    pts;                     /* seconds                               */
} VFRAME;

#define MAX_SUBS     8                 /* embedded text subtitle tracks kept   */
#define MAX_ATRACKS  16                /* embedded audio tracks kept           */

typedef struct {
    double    start, end;              /* seconds                               */
    int       track;                   /* 0..nsub-1 embedded, nsub external     */
    char    * text;                    /* UTF-8, '\n' between lines             */
} CUE;

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

    /* GPU colour conversion (yuv420p only): the three planes are uploaded as packed RGBA8 textures
       (4 samples per texel) and a shader writes RGB straight into the graph's texture */
    int             gpu;
    int             down10;                    /* 10-bit hardware pictures reduced to 8 bits in the copy (SDR only) */
    int             hwmode;                    /* 0 planar yuv420p, 1 NV12, 2 16-bit (10 bit) NV12   */
    HWDEC           * hw;                      /* console decoder (PS5), NULL = libavcodec          */
    int             fake_hw;                   /* PC test: NV12/P010 made with swscale              */
    int             hwpc;                      /* PC: FFmpeg's hardware decoder (VAAPI, D3D11VA...)  */
    int             hwkind;                    /* 0 software, 1 NVDEC (cuvid), 2 hwaccel de FFmpeg  */
    AVBSFContext    * bsf;                     /* mp4 -> Annex B for the console decoder            */
    AVPacket        * bsf_pkt;
    int             hw_wait_key, hw_skip_rasl, hw_fail, hw_errors;
    struct SwsContext * sws_fake;
    uint8_t         * fk_y, * fk_uv;
    int             fk_ypitch, fk_uvpitch;
    int             ypw, cpw, cw, ch;          /* packed Y / chroma widths (texels), chroma w,h */
    int             vw, vh;                      /* tamano del fotograma tal cual viene del decodificador */
    int             vrot;                      /* rotacion: 0 ninguna, 1 90 a la derecha, 2 180, 3 90 a la izquierda */
    uint8_t         * rotbuf;                  /* fotograma girado (solo si vrot) */
    size_t          slot_bytes;
    GPU_Image       * img_y, * img_u, * img_v;
    GPU_Target      * tgt;
    float           gp[4];                     /* shader params: width, full range, matrix, layout */
    float           transfer;                  /* 1 = PQ (HDR10)                                */

    /* shared state, guarded by mx */
    SDL_mutex       * mx;
    SDL_cond        * cond;
    VFRAME          vq[VQ_MAX];
    int             vq_size;                   /* frames in the ring (3..VQ_MAX by frame size) */
    int             vq_r, vq_n;
    uint8_t         * abuf;            /* audio ring (interleaved, mixer format)*/
    size_t          acap, ar, an;
    double          a_pts_end;         /* pts of the end of the queued audio    */
    int             a_rate, a_ch, a_bps;   /* mixer spec in use                 */
    int             has_audio;         /* decoding audio for the mixer hook     */
    int             naudio;                    /* audio tracks found at open    */
    int             audio_stream[MAX_ATRACKS]; /* stream index of each track    */
    int             audio_switch_req;          /* VIDEO_AUDIO_SELECT pending    */
    int             audio_switch_to;           /* ...to this stream index       */
    int             audio_only;                /* no picture at all (music file)*/
    int16_t         vis[8192];                 /* last mixer chunk, mono, for the visualiser  */
    int             vis_len;                   /* samples valid in vis[]                       */
    double          vstart;                    /* media time of the first picture (a stream that starts mid-way, e.g. a trimmed list) */
    int             vpts_init;                 /* the wall clock was aligned with the first picture since the start / last seek */
    int             a_got;                     /* sound decoded since the start / last seek (else the wall clock is used) */
    Uint32          vis_t0;                    /* SDL ticks when that chunk was handed to the mixer */
    double          spec[32];                  /* spectrum cache (VIDEO_SPECTRUM)              */
    Uint32          spec_tick;

    int             playing, paused, loop, eof, finished, quit;
    int             seek_req;          double seek_to;
    int             shown_first;
    double          volume;            /* 0..1                                  */
    double          speed, swr_speed;  /* playback speed asked / the one the resampler is set for (1.0 = normal) */
    int             sub_delay_ms, av_off_ms;   /* subtitles later (+) / the picture later (+) than the sound, in ms */

    /* wall clock fallback / pause bookkeeping */
    double          clk_base;          /* media time at clk_t0                  */
    Uint32          clk_t0;
    double          clk_a;             /* last audio playout time (clock)       */
    Uint32          clk_a_t0;          /* SDL ticks when clk_a was taken        */
    double          mh_t, mh_wall;     /* audio clock at the last mixer callback and the wall time of that callback: the callbacks come every
                                          ~90 ms, so between them the clock is interpolated (else pictures are shown in bursts, ~11 per second) */
    double          clk_mono;          /* last value returned, so the interpolated clock never goes back */
    double          last_time;         /* last media time reported              */

    /* subtitles (text only: SRT / ASS / SSA / WebVTT / mov_text), guarded by mx */
    int             nsub;                      /* embedded text tracks          */
    int             sub_stream[MAX_SUBS];      /* stream index of each track    */
    AVCodecContext  * sub_ctx[MAX_SUBS];
    int             sub_sel;                   /* selected track, -1 = off      */
    int             sub_ext;                   /* 1 once an external file loaded*/
    CUE             * cues;
    int             ncues, capcues;

    SDL_Thread      * thr;
    double          wait_ms;                   /* decode thread waiting for a free ring slot (stats) */
    double          copy_ms;                   /* picture copy time (stats)                           */
    int             copy_n, copy_reports;
    int             shown_count;               /* pictures actually put on screen (stats)            */
    double          cpu0, wall0;               /* process CPU / wall seconds at open (stats)          */
} VIDEO;

static VIDEO       g_vid[MAX_VIDEOS + 1];       /* handle = index, 0 unused     */
static VIDEO *     g_audio_owner = NULL;
static char g_trace_path[512] = "";
static Uint32 g_open_t0 = 0;
static void vtrace( const char * fmt, ... );

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

static double now_s( clockid_t id ) {
    struct timespec t;
    clock_gettime( id, &t );
    return ( double ) t.tv_sec + t.tv_nsec / 1e9;
}

static VIDEO * vid_get( int64_t h ) {
    if ( h < 1 || h > MAX_VIDEOS || !g_vid[h].used ) return NULL;
    return &g_vid[h];
}

/* media time "now" (call with mx held or not; reads are racy but monotonic enough) */
static double vid_clock( VIDEO * v ) {
    if ( v->has_audio && g_audio_owner == v ) {
        /* no sound has been decoded yet (some streams are silent or have no audio for their first seconds):
           the audio clock cannot start, so keep time with the wall clock until the first sound arrives */
        if ( v->playing && !v->paused && !v->a_got )
            return v->clk_base + ( SDL_GetTicks() - v->clk_t0 ) / 1000.0 * v->speed;
        double queued = ( double ) v->an / ( double ) ( v->a_rate * v->a_ch * v->a_bps ) * v->swr_speed;   /* media seconds that are still queued */
        double t = v->a_pts_end - queued;
        if ( t < 0 ) t = 0;
        if ( v->playing && !v->paused ) {
            /* the audio ring ran dry (underrun, e.g. its track starts late or the demuxer is fetching
               the next segment): keep time from the last playout instant instead of freezing here.
               A frozen clock stalls the picture forever, because the decode thread is parked on a
               full video queue and that queue only drains when this clock advances. */
            if ( queued <= 0.0 )
                return v->clk_a + ( SDL_GetTicks() - v->clk_a_t0 ) / 1000.0 * v->swr_speed;
            v->clk_a = t; v->clk_a_t0 = SDL_GetTicks();
            if ( v->mh_wall > 0 ) {                            /* interpolate between mixer callbacks */
                double e = ( now_s( CLOCK_MONOTONIC ) - v->mh_wall ) * v->swr_speed;
                if ( e < 0 ) e = 0; if ( e > 0.25 ) e = 0.25;
                double ti = v->mh_t + e;
                if ( ti > v->a_pts_end ) ti = v->a_pts_end;
                if ( ti > t ) t = ti;
            }
            if ( t < v->clk_mono && v->clk_mono - t < 0.3 ) t = v->clk_mono; else v->clk_mono = t;
            if ( t > v->clk_mono ) v->clk_mono = t;
        }
        return t;
    }
    if ( !v->playing || v->paused ) return v->clk_base;
    return v->clk_base + ( SDL_GetTicks() - v->clk_t0 ) / 1000.0 * v->speed;
}

static void vid_clock_freeze( VIDEO * v ) { v->clk_base = vid_clock( v ); v->mh_wall = 0; v->clk_t0 = SDL_GetTicks(); v->clk_a = v->clk_base; v->clk_a_t0 = v->clk_t0; }

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
        v->mh_t = v->a_pts_end - ( double ) v->an / ( double ) ( v->a_rate * v->a_ch * v->a_bps ) * v->swr_speed; v->mh_wall = now_s( CLOCK_MONOTONIC );

        if ( v->a_bps == 2 && v->a_ch > 0 ) {            /* keep the chunk (mono) so VIDEO_SPECTRUM / VIDEO_WAVE can look at what is playing */
            const int16_t * sm = ( const int16_t * ) stream;
            int ns = ( int ) ( n / ( 2 * ( size_t ) v->a_ch ) );
            if ( ns > 8192 ) ns = 8192;
            for ( int i = 0; i < ns; i++ ) {
                int acc = 0;
                for ( int c = 0; c < v->a_ch; c++ ) acc += sm[i * v->a_ch + c];
                v->vis[i] = ( int16_t ) ( acc / v->a_ch );
            }
            v->vis_len = ns; v->vis_t0 = SDL_GetTicks();
        }

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

/* ---- subtitles ---------------------------------------------------------- */

/* strip ASS override blocks {...}, <tags> and turn \N / \n / \h into text; returns malloc'd UTF-8 */
static char * sub_clean( const char * in ) {
    size_t n = strlen( in );
    char * out = ( char * ) malloc( n + 1 );
    if ( !out ) return NULL;
    size_t o = 0;
    for ( size_t i = 0; i < n; i++ ) {
        char c = in[i];
        if ( c == '{' ) { while ( i < n && in[i] != '}' ) i++; continue; }
        if ( c == '<' ) { size_t j = i; while ( j < n && in[j] != '>' ) j++; if ( j < n ) { i = j; continue; } }
        if ( c == '\\' && i + 1 < n ) {
            if ( in[i+1] == 'N' || in[i+1] == 'n' ) { out[o++] = '\n'; i++; continue; }
            if ( in[i+1] == 'h' ) { out[o++] = ' '; i++; continue; }
        }
        if ( c == '\r' ) continue;
        out[o++] = c;
    }
    while ( o > 0 && ( out[o-1] == '\n' || out[o-1] == ' ' ) ) o--;
    out[o] = 0;
    return out;
}

static void cue_add( VIDEO * v, int track, double start, double end, char * text ) {
    SDL_LockMutex( v->mx );
    /* a looped / re-demuxed packet gives the same cue again */
    for ( int i = v->ncues - 1, k = 0; i >= 0 && k < 64; i--, k++ )
        if ( v->cues[i].track == track && fabs( v->cues[i].start - start ) < 0.001 && !strcmp( v->cues[i].text, text ) ) {
            SDL_UnlockMutex( v->mx ); free( text ); return;
        }
    if ( v->ncues == v->capcues ) {
        int nc = v->capcues ? v->capcues * 2 : 64;
        CUE * p = ( CUE * ) realloc( v->cues, ( size_t ) nc * sizeof( CUE ) );
        if ( !p ) { SDL_UnlockMutex( v->mx ); free( text ); return; }
        v->cues = p; v->capcues = nc;
    }
    v->cues[v->ncues].start = start; v->cues[v->ncues].end = end;
    v->cues[v->ncues].track = track; v->cues[v->ncues].text = text;
    v->ncues++;
    SDL_UnlockMutex( v->mx );
}

/* decode one subtitle packet into cues of 'track' */
static void sub_packet( VIDEO * v, AVCodecContext * c, AVPacket * pkt, double tb, int track ) {
    AVSubtitle sub; int got = 0;
    memset( &sub, 0, sizeof( sub ) );
    if ( avcodec_decode_subtitle2( c, &sub, &got, pkt ) < 0 || !got ) return;

    double base = pkt->pts != AV_NOPTS_VALUE ? pkt->pts * tb : ( sub.pts != AV_NOPTS_VALUE ? sub.pts / ( double ) AV_TIME_BASE : 0 );
    double start = base + sub.start_display_time / 1000.0;
    double end   = base + sub.end_display_time / 1000.0;
    if ( end <= start && pkt->duration > 0 ) end = start + pkt->duration * tb;
    if ( end <= start ) end = start + 3.0;

    for ( unsigned i = 0; i < sub.num_rects; i++ ) {
        AVSubtitleRect * r = sub.rects[i];
        const char * raw = NULL;
        if ( r->type == SUBTITLE_ASS && r->ass ) {
            raw = r->ass;                                   /* ReadOrder,Layer,Style,Name,ML,MR,MV,Effect,Text */
            for ( int k = 0; k < 8 && raw; k++ ) { raw = strchr( raw, ',' ); if ( raw ) raw++; }
        } else if ( r->type == SUBTITLE_TEXT && r->text ) raw = r->text;
        if ( !raw ) continue;
        char * t = sub_clean( raw );
        if ( t && *t ) cue_add( v, track, start, end, t ); else free( t );
    }
    avsubtitle_free( &sub );
}

static const char * sub_lang( AVFormatContext * f, int stream, char * buf, size_t n ) {
    AVDictionaryEntry * l = av_dict_get( f->streams[stream]->metadata, "language", NULL, 0 );
    AVDictionaryEntry * t = av_dict_get( f->streams[stream]->metadata, "title", NULL, 0 );
    if ( l && t ) snprintf( buf, n, "%s - %s", l->value, t->value );
    else if ( l ) snprintf( buf, n, "%s", l->value );
    else if ( t ) snprintf( buf, n, "%s", t->value );
    else buf[0] = 0;
    return buf;
}

/* ---- GPU yuv420p -> RGB -------------------------------------------------- */

static int g_use_gpu = 1;                      /* VIDEO_USE_GPU                                 */
static int g_use_hw = 1;                       /* VIDEO_USE_HW (console decoder, PS5)            */
static int g_pc_hw = 1;                        /* LIBMOD_VIDEO_PC_HW=0: en el PC, sin hardware  */
static int g_fake_hw = 0;                      /* PC test: LIBMOD_VIDEO_FAKE_HW=1 (NV12/P010 via swscale) */
static Uint32 g_yuv_prog = 0;                  /* shared program, 0 = not built / failed        */
static int g_yuv_tried = 0;
static GPU_ShaderBlock g_yuv_block;
static int g_loc_tex, g_loc_u, g_loc_v, g_loc_p, g_loc_w;

#define YUV_FRAG_BODY \
    "uniform sampler2D tex;\n" \
    "uniform sampler2D tex_u;\n" \
    "uniform sampler2D tex_v;\n" \
    "uniform vec4 prm;\n"      /* x = width, y = full range, z = matrix (0 601, 1 709, 2 2020), w = layout (0 planar, 1 NV12, 2 16-bit NV12) */ \
    "uniform vec4 wid;\n"      /* x = Y packed width, y = chroma packed width, z = transfer (1 = PQ) */ \
    "float pick(vec4 t, float c) { return c < 0.5 ? t.r : (c < 1.5 ? t.g : (c < 2.5 ? t.b : t.a)); }\n" \
    "vec3 yuv(vec2 uv) {\n" \
    "    float px = floor(uv.x * prm.x);\n" \
    "    float y, u, v;\n" \
    "    if (prm.w > 1.5) {\n"                                  /* 16-bit samples, 10 bits low-aligned */ \
    "        float tx = floor(px / 2.0);\n" \
    "        float s = px - tx * 2.0;\n" \
    "        vec4 t = TEX(tex, vec2((tx + 0.5) / wid.x, uv.y));\n" \
    "        float lo = s < 0.5 ? t.r : t.b;\n" \
    "        float hi = s < 0.5 ? t.g : t.a;\n" \
    "        y = lo * 255.0 + hi * 65280.0;\n" \
    "        float cx = floor(px / 2.0);\n" \
    "        vec4 c = TEX(tex_u, vec2((cx + 0.5) / wid.y, uv.y));\n" \
    "        u = c.r * 255.0 + c.g * 65280.0;\n" \
    "        v = c.b * 255.0 + c.a * 65280.0;\n" \
    "        if (prm.y < 0.5) { y = (y - 64.0) / 876.0; u = (u - 512.0) / 896.0; v = (v - 512.0) / 896.0; }\n" \
    "        else             { y = y / 1023.0;         u = (u - 512.0) / 1023.0; v = (v - 512.0) / 1023.0; }\n" \
    "    } else {\n" \
    "        float tx = floor(px / 4.0);\n" \
    "        y = pick(TEX(tex, vec2((tx + 0.5) / wid.x, uv.y)), px - tx * 4.0);\n" \
    "        float cx = floor(px / 2.0);\n" \
    "        if (prm.w > 0.5) {\n"                              /* NV12: U,V interleaved */ \
    "            float ctx = floor(cx / 2.0);\n" \
    "            float s = cx - ctx * 2.0;\n" \
    "            vec4 c = TEX(tex_u, vec2((ctx + 0.5) / wid.y, uv.y));\n" \
    "            u = s < 0.5 ? c.r : c.b;\n" \
    "            v = s < 0.5 ? c.g : c.a;\n" \
    "        } else {\n" \
    "            float ctx = floor(cx / 4.0);\n" \
    "            vec2 cuv = vec2((ctx + 0.5) / wid.y, uv.y);\n" \
    "            float cc = cx - ctx * 4.0;\n" \
    "            u = pick(TEX(tex_u, cuv), cc);\n" \
    "            v = pick(TEX(tex_v, cuv), cc);\n" \
    "        }\n" \
    "        u -= 0.5019608; v -= 0.5019608;\n" \
    "        if (prm.y < 0.5) { y = (y - 0.0627451) * 1.1643836; u *= 1.1383929; v *= 1.1383929; }\n" \
    "    }\n" \
    "    vec3 c;\n" \
    "    if (prm.z > 1.5)      c = vec3(y + 1.4746 * v, y - 0.16455 * u - 0.57135 * v, y + 1.8814 * u);\n" \
    "    else if (prm.z > 0.5) c = vec3(y + 1.5748 * v, y - 0.1873 * u - 0.4681 * v, y + 1.8556 * u);\n" \
    "    else                  c = vec3(y + 1.402 * v, y - 0.344136 * u - 0.714136 * v, y + 1.772 * u);\n" \
    "    c = clamp(c, 0.0, 1.0);\n" \
    "    if (wid.z > 0.5) {\n"                                  /* PQ (HDR10) -> SDR: EOTF, Reinhard, BT.2020 -> BT.709, gamma */ \
    "        vec3 p = pow(c, vec3(1.0 / 78.84375));\n" \
    "        vec3 l = pow(max(p - 0.8359375, 0.0) / (18.8515625 - 18.6875 * p), vec3(1.0 / 0.1593017578125));\n" \
    "        vec3 x = l * 10000.0 / 203.0;\n" \
    "        x = x * (1.0 + x / 24.0) / (1.0 + x);\n" \
    "        vec3 r = vec3(1.6605 * x.r - 0.5876 * x.g - 0.0728 * x.b,\n" \
    "                      -0.1246 * x.r + 1.1329 * x.g - 0.0083 * x.b,\n" \
    "                      -0.0182 * x.r - 0.1006 * x.g + 1.1187 * x.b);\n" \
    "        c = pow(clamp(r, 0.0, 1.0), vec3(1.0 / 2.2));\n" \
    "    }\n" \
    "    return c;\n" \
    "}\n"

static void yuv_program_build( void ) {
    g_yuv_tried = 1;
    GPU_Renderer * r = GPU_GetCurrentRenderer();
    if ( !r ) return;

    const char * vs, * fs;
    static const char vs_es[] =
        "#version 100\nprecision highp float;\nprecision mediump int;\n"
        "attribute vec2 gpu_Vertex; attribute vec2 gpu_TexCoord; attribute mediump vec4 gpu_Color;\n"
        "uniform mat4 gpu_ModelViewProjectionMatrix; varying vec2 texCoord;\n"
        "void main(void) { texCoord = gpu_TexCoord; gl_Position = gpu_ModelViewProjectionMatrix * vec4(gpu_Vertex, 0.0, 1.0); }\n";
    static const char fs_es[] =
        "#version 100\nprecision highp float;\nprecision mediump int;\n"
        "#define TEX texture2D\nvarying vec2 texCoord;\n" YUV_FRAG_BODY
        "void main(void) { gl_FragColor = vec4(yuv(texCoord), 1.0); }\n";
    static const char vs_gl[] =
        "#version 150\n"
        "in vec2 gpu_Vertex; in vec2 gpu_TexCoord; in vec4 gpu_Color;\n"
        "uniform mat4 gpu_ModelViewProjectionMatrix; out vec2 texCoord;\n"
        "void main(void) { texCoord = gpu_TexCoord; gl_Position = gpu_ModelViewProjectionMatrix * vec4(gpu_Vertex, 0.0, 1.0); }\n";
    static const char fs_gl[] =
        "#version 150\n"
        "#define TEX texture\nin vec2 texCoord; out vec4 fragColor;\n" YUV_FRAG_BODY
        "void main(void) { fragColor = vec4(yuv(texCoord), 1.0); }\n";
    static const char vs_gl130[] =
        "#version 130\n"
        "in vec2 gpu_Vertex; in vec2 gpu_TexCoord; in vec4 gpu_Color;\n"
        "uniform mat4 gpu_ModelViewProjectionMatrix; out vec2 texCoord;\n"
        "void main(void) { texCoord = gpu_TexCoord; gl_Position = gpu_ModelViewProjectionMatrix * vec4(gpu_Vertex, 0.0, 1.0); }\n";
    static const char fs_gl130[] =
        "#version 130\n"
        "#define TEX texture\nin vec2 texCoord; out vec4 fragColor;\n" YUV_FRAG_BODY
        "void main(void) { fragColor = vec4(yuv(texCoord), 1.0); }\n";

    if ( r->shader_language == GPU_LANGUAGE_GLSLES ) { vs = vs_es; fs = fs_es; }
    else if ( r->id.major_version > 3 || ( r->id.major_version == 3 && r->id.minor_version >= 2 ) ) { vs = vs_gl; fs = fs_gl; }
    else { vs = vs_gl130; fs = fs_gl130; }

    Uint32 v = GPU_CompileShader( GPU_VERTEX_SHADER, vs );
    if ( !v ) { fprintf( stderr, "libmod_video: GPU vertex shader: %s\n", GPU_GetShaderMessage() ); return; }
    Uint32 f = GPU_CompileShader( GPU_FRAGMENT_SHADER, fs );
    if ( !f ) { fprintf( stderr, "libmod_video: GPU fragment shader: %s\n", GPU_GetShaderMessage() ); GPU_FreeShader( v ); return; }
    Uint32 p = GPU_LinkShaders( v, f );
    if ( !p ) { fprintf( stderr, "libmod_video: GPU link: %s\n", GPU_GetShaderMessage() ); GPU_FreeShader( v ); GPU_FreeShader( f ); return; }
    g_yuv_block = GPU_LoadShaderBlock( p, "gpu_Vertex", "gpu_TexCoord", "gpu_Color", "gpu_ModelViewProjectionMatrix" );
    g_loc_tex = GPU_GetUniformLocation( p, "tex" );
    g_loc_u   = GPU_GetUniformLocation( p, "tex_u" );
    g_loc_v   = GPU_GetUniformLocation( p, "tex_v" );
    g_loc_p   = GPU_GetUniformLocation( p, "prm" );
    g_loc_w   = GPU_GetUniformLocation( p, "wid" );
    g_yuv_prog = p;
}

/* images / graph textures freed a few frames after VIDEO_CLOSE (see ZOMBIE_FRAMES) */
#define MAX_ZIMG 24
static struct { GPU_Image * img; GPU_Target * tgt; int frames; } g_zimg[MAX_ZIMG];

static void zimg_free( int i ) {
    if ( g_zimg[i].tgt ) GPU_FreeTarget( g_zimg[i].tgt );
    if ( g_zimg[i].img ) GPU_FreeImage( g_zimg[i].img );
    memset( &g_zimg[i], 0, sizeof( g_zimg[i] ) );
}
static void zimg_add( GPU_Image * img, GPU_Target * tgt ) {
    if ( !img && !tgt ) return;
    for ( int i = 0; i < MAX_ZIMG; i++ ) if ( !g_zimg[i].img && !g_zimg[i].tgt ) {
        g_zimg[i].img = img; g_zimg[i].tgt = tgt; g_zimg[i].frames = 30; return;
    }
    if ( tgt ) GPU_FreeTarget( tgt );
    if ( img ) GPU_FreeImage( img );
}
static void zimg_tick( int all ) {
    for ( int i = 0; i < MAX_ZIMG; i++ )
        if ( ( g_zimg[i].img || g_zimg[i].tgt ) && ( all || --g_zimg[i].frames <= 0 ) ) zimg_free( i );
}

/* convert one queued slot into the graph's texture, on the GPU */
static void yuv_render( VIDEO * v, const uint8_t * base ) {
    GPU_UpdateImageBytes( v->img_y, NULL, base, v->ypw * 4 );
    GPU_UpdateImageBytes( v->img_u, NULL, base + ( size_t ) v->ypw * 4 * v->h, v->cpw * 4 );
    if ( v->hwmode == 0 )
        GPU_UpdateImageBytes( v->img_v, NULL, base + ( size_t ) v->ypw * 4 * v->h + ( size_t ) v->cpw * 4 * v->ch, v->cpw * 4 );

    float wid[4] = { ( float ) v->ypw, ( float ) v->cpw, v->transfer, 0 };
    GPU_ActivateShaderProgram( g_yuv_prog, &g_yuv_block );
    GPU_SetShaderImage( v->img_u, g_loc_u, 1 );
    GPU_SetShaderImage( v->hwmode == 0 ? v->img_v : v->img_u, g_loc_v, 2 );
    GPU_SetUniformi( g_loc_tex, 0 );
    GPU_SetUniformfv( g_loc_p, 4, 1, v->gp );
    GPU_SetUniformfv( g_loc_w, 4, 1, wid );
    GPU_UnsetClip( v->tgt );
    GPU_BlitScale( v->img_y, NULL, v->tgt, v->w / 2.0f, v->h / 2.0f, v->w / ( float ) ( v->ypw ), 1.0f );
    GPU_FlushBlitBuffer();
    GPU_ActivateShaderProgram( 0, NULL );
}

static void vq_clear( VIDEO * v ) {
    v->vq_r = 0; v->vq_n = 0;
    v->an = 0; v->ar = 0;
}

/* NV12 / 16-bit NV12 rows into a slot (Y then interleaved UV); source pitches in bytes */
static void copy_rows_parallel( uint8_t * d, size_t dp, const uint8_t * s, size_t sp, size_t bytes, int rows, int down );

/* closes the current audio decoder/resampler and opens 'newstream' instead - called from
   decode_thread only, which is the thread that owns actx/swr (defined below av_fmt_for) */
static void audio_switch_track( VIDEO * v, int newstream );

static void pack_nv( VIDEO * v, uint8_t * d, const uint8_t * y, int ypitch, const uint8_t * uv, int uvpitch, int down ) {
    size_t ys = ( size_t ) v->ypw * 4, cs = ( size_t ) v->cpw * 4;
    size_t yb = v->hwmode == 2 ? ( size_t ) v->w * 2 : ( size_t ) v->w;
    size_t cb = ( size_t ) v->cw * ( v->hwmode == 2 ? 4 : 2 );
    uint8_t * du = d + ys * v->h;
    if ( v->hw ) {                                        /* source is GPU memory: copy in parallel */
        copy_rows_parallel( d, ys, y, ypitch, yb, v->h, down );
        copy_rows_parallel( du, cs, uv, uvpitch, cb, v->ch, down );
    } else {
        for ( int r = 0; r < v->h; r++ ) memcpy( d + r * ys, y + ( size_t ) r * ypitch, yb );
        for ( int r = 0; r < v->ch; r++ ) memcpy( du + r * cs, uv + ( size_t ) r * uvpitch, cb );
    }
}

/* lets a blocked network read return as soon as the video is being closed (otherwise closing could hang on a dead server) */
static int vid_interrupt( void * p ) { return ( ( VIDEO * ) p )->quit; }

static int vq_reserve( VIDEO * v ) {                      /* free slot, or -1 (quit / seek) */
    double tw = now_s( CLOCK_MONOTONIC );
    SDL_LockMutex( v->mx );
    while ( !v->quit && !v->seek_req && v->vq_n >= v->vq_size ) SDL_CondWaitTimeout( v->cond, v->mx, 10 );
    int slot = ( v->quit || v->seek_req ) ? -1 : ( v->vq_r + v->vq_n ) % v->vq_size;
    SDL_UnlockMutex( v->mx );
    v->wait_ms += ( now_s( CLOCK_MONOTONIC ) - tw ) * 1000.0;
    return slot;
}
/* streams that start late (a trimmed HLS list, a seek) have pictures at pts >> 0: align the wall clock with the first one,
   or the picture would wait for the clock to reach it (call with mx held) */
static void vclock_align( VIDEO * v, double pts ) {
    if ( v->vpts_init ) return;
    v->vpts_init = 1;
    if ( !v->a_got && v->clk_base + 0.5 < pts ) { v->clk_base = pts; v->clk_t0 = SDL_GetTicks(); v->clk_a = pts; v->clk_a_t0 = v->clk_t0; }
}
static void vq_commit( VIDEO * v, int slot, double pts ) {
    v->vq[slot].pts = pts;
    SDL_LockMutex( v->mx );
    vclock_align( v, pts );
    if ( !v->seek_req ) v->vq_n++;
    SDL_UnlockMutex( v->mx );
}

/* The decoder's frame memory is GPU memory: CPU reads from it are slow and latency bound, so one thread copying
   24 MB of a 4K 10-bit picture is the bottleneck. The rows are split over a few threads. */
#define COPY_THREADS 4
typedef struct { uint8_t * d; size_t dp; const uint8_t * s; size_t sp; size_t bytes; int rows; int down; } COPYJOB;

static void * copy_job( void * p ) {
    COPYJOB * j = ( COPYJOB * ) p;
    if ( !j->down ) {
        for ( int r = 0; r < j->rows; r++ ) memcpy( j->d + r * j->dp, j->s + ( size_t ) r * j->sp, j->bytes );
    } else {                                           /* 16-bit samples (10 bits, low aligned) -> 8 bits */
        for ( int r = 0; r < j->rows; r++ ) {
            const uint16_t * q = ( const uint16_t * ) ( j->s + ( size_t ) r * j->sp );
            uint8_t * o = j->d + r * j->dp;
            for ( size_t i = 0; i < j->bytes; i++ ) { unsigned x = ( q[i] + 2u ) >> 2; o[i] = ( uint8_t ) ( x > 255 ? 255 : x ); }
        }
    }
    return NULL;
}

static void copy_rows_parallel( uint8_t * d, size_t dp, const uint8_t * s, size_t sp, size_t bytes, int rows, int down ) {
    pthread_t th[COPY_THREADS - 1];
    COPYJOB job[COPY_THREADS];
    int n = rows >= 64 ? COPY_THREADS : 1, per = ( rows + n - 1 ) / n, started = 0;
    for ( int i = 0; i < n; i++ ) {
        int r0 = i * per, r1 = r0 + per > rows ? rows : r0 + per;
        job[i] = ( COPYJOB ) { d + r0 * dp, dp, s + ( size_t ) r0 * sp, sp, bytes, r1 - r0, down };
        if ( r1 <= r0 ) { job[i].rows = 0; continue; }
        if ( i < n - 1 && pthread_create( &th[i], NULL, copy_job, &job[i] ) == 0 ) started |= 1 << i;
        else if ( i < n - 1 ) copy_job( &job[i] );
    }
    copy_job( &job[n - 1] );
    for ( int i = 0; i < n - 1; i++ ) if ( started & ( 1 << i ) ) pthread_join( th[i], NULL );
}

/* a picture from the console's decoder */
static void hw_picture( void * ud, const HWPIC * p ) {
    VIDEO * v = ( VIDEO * ) ud;
    int slot = vq_reserve( v );
    if ( slot < 0 ) return;
    double t0 = now_s( CLOCK_MONOTONIC );
    pack_nv( v, v->vq[slot].rgba, p->y, p->pitch, p->uv, p->pitch, v->down10 && p->ten );
    v->copy_ms += ( now_s( CLOCK_MONOTONIC ) - t0 ) * 1000.0;
    if ( ++v->copy_n == 60 && v->copy_reports < 3 ) {
        v->copy_reports++;
        fprintf( stderr, "libmod_video: %dx%d picture copy %.1f ms each (%d threads), ring wait %.0f ms per 60 pictures\n", v->w, v->h, v->copy_ms / v->copy_n, COPY_THREADS, v->wait_ms );
        v->wait_ms = 0;
        v->copy_ms = 0; v->copy_n = 0;
    } else if ( v->copy_n >= 60 ) { v->copy_ms = 0; v->copy_n = 0; v->wait_ms = 0; }
    vq_commit( v, slot, p->pts == INT64_MIN ? 0.0 : p->pts * v->vtb );
}

/* HEVC: type of the first picture slice of an Annex B access unit (-1 none) */
static int hevc_slice_type( const uint8_t * p, size_t n ) {
    for ( size_t i = 0; i + 4 < n; i++ )
        if ( p[i] == 0 && p[i+1] == 0 && p[i+2] == 1 ) {
            unsigned t = ( p[i+3] >> 1 ) & 0x3f;
            if ( t < 32 ) return ( int ) t;
            i += 2;
        }
    return -1;
}

/* give up on the console decoder and go on in software from the current position */
static void hw_give_up( VIDEO * v, const char * why ) {
    fprintf( stderr, "libmod_video: hardware decoding off (%s), using software\n", why );
    v->hw_fail = 1;
}

static void hw_feed( VIDEO * v, AVPacket * pkt ) {
    if ( v->hw_wait_key && !( pkt->flags & AV_PKT_FLAG_KEY ) ) return;
    if ( av_bsf_send_packet( v->bsf, pkt ) < 0 ) return;
    while ( av_bsf_receive_packet( v->bsf, v->bsf_pkt ) >= 0 ) {
        AVPacket * o = v->bsf_pkt;
        int64_t ts = o->pts != AV_NOPTS_VALUE ? o->pts : o->dts;
        if ( ts == AV_NOPTS_VALUE ) ts = INT64_MIN;
        int skip = 0;
        if ( v->hw_skip_rasl && v->vctx->codec_id == AV_CODEC_ID_HEVC ) {
            int t = hevc_slice_type( o->data, o->size );
            if ( t == 8 || t == 9 ) skip = 1;                 /* RASL: refers to pictures before the restart point */
            else if ( t >= 0 && !( t >= 16 && t <= 21 ) ) v->hw_skip_rasl = 0;
        }
        if ( !skip ) {
            int rc = hwdec_decode( v->hw, o->data, o->size, ts, hw_picture, v );
            if ( rc < 0 ) {
                if ( rc <= -2 || ++v->hw_errors > 30 ) { hw_give_up( v, "decoder error" ); av_packet_unref( o ); return; }
                hwdec_reset( v->hw );                         /* stream error: restart from the next keyframe */
                v->hw_wait_key = 1; v->hw_skip_rasl = 1;
                av_packet_unref( o );
                return;
            } else v->hw_errors = 0;
        }
        av_packet_unref( o );
    }
}

/* Gira el fotograma RGBA (w x h). Para 90 grados el resultado es h x w (h filas
   de w pixeles).
   rot: 1 = 90 a la derecha, 2 = 180, 3 = 90 a la izquierda */
static void rotate_rgba( uint8_t * dst, const uint8_t * src, int w, int h, int rot )
{
    int x, y;
    if ( rot == 1 ) {
        for ( y = 0; y < w; y++ )
            for ( x = 0; x < h; x++ )
                memcpy( dst + ( size_t ) y * h * 4 + ( size_t ) x * 4, src + ( size_t ) y * w * 4 + ( size_t ) ( h - 1 - x ) * 4, 4 );
    } else if ( rot == 3 ) {
        for ( y = 0; y < w; y++ )
            for ( x = 0; x < h; x++ )
                memcpy( dst + ( size_t ) y * h * 4 + ( size_t ) x * 4, src + ( size_t ) ( w - 1 - y ) * w * 4 + ( size_t ) x * 4, 4 );
    } else {
        for ( y = 0; y < h; y++ )
            for ( x = 0; x < w; x++ )
                memcpy( dst + ( size_t ) y * w * 4 + ( size_t ) x * 4, src + ( size_t ) ( h - 1 - y ) * w * 4 + ( size_t ) ( w - 1 - x ) * 4, 4 );
    }
}

static int push_video_frame( VIDEO * v, AVFrame * f ) {
    double pts = ( f->best_effort_timestamp == AV_NOPTS_VALUE ) ? 0.0 : f->best_effort_timestamp * v->vtb;

    SDL_LockMutex( v->mx );
    while ( !v->quit && !v->seek_req && v->vq_n >= v->vq_size ) SDL_CondWaitTimeout( v->cond, v->mx, 10 );
    if ( v->quit || v->seek_req ) { SDL_UnlockMutex( v->mx ); return 0; }
    int slot = ( v->vq_r + v->vq_n ) % v->vq_size;
    SDL_UnlockMutex( v->mx );

    if ( v->fake_hw ) {
        /* PC test of the NV12 / 16-bit paths: swscale makes what the console decoder would give */
        uint8_t * dstp[4] = { v->fk_y, v->fk_uv, NULL, NULL };
        int dstl[4] = { v->fk_ypitch, v->fk_uvpitch, 0, 0 };
        sws_scale( v->sws_fake, ( const uint8_t * const * ) f->data, f->linesize, 0, f->height, dstp, dstl );
        if ( v->hwmode == 2 ) {                               /* P010 is high-aligned, the console gives low-aligned */
            for ( int r = 0; r < v->h; r++ ) { uint16_t * q = ( uint16_t * ) ( v->fk_y + ( size_t ) r * v->fk_ypitch ); for ( int i = 0; i < v->w; i++ ) q[i] >>= 6; }
            for ( int r = 0; r < v->ch; r++ ) { uint16_t * q = ( uint16_t * ) ( v->fk_uv + ( size_t ) r * v->fk_uvpitch ); for ( int i = 0; i < v->cw * 2; i++ ) q[i] >>= 6; }
        }
        pack_nv( v, v->vq[slot].rgba, v->fk_y, v->fk_ypitch, v->fk_uv, v->fk_uvpitch, 0 );
    } else if ( v->gpu ) {
        if ( v->hwkind == 1 || v->hwpc ) {
            /* NV12 (Y + UV intercalado) del decodificador de la GPU: los planos van tal cual */
            pack_nv( v, v->vq[slot].rgba, f->data[0], f->linesize[0], f->data[1], f->linesize[1], 0 );
            v->vq[slot].pts = pts;
            SDL_LockMutex( v->mx );
            vclock_align( v, pts );
            if ( !v->seek_req ) v->vq_n++;
            SDL_UnlockMutex( v->mx );
            return 1;
        }
        /* planes copied as they are; the GPU does the colour conversion */
        uint8_t * d = v->vq[slot].rgba;
        size_t ys = ( size_t ) v->ypw * 4, cs = ( size_t ) v->cpw * 4;
        for ( int y = 0; y < v->h; y++ ) memcpy( d + y * ys, f->data[0] + ( size_t ) y * f->linesize[0], v->w );
        uint8_t * du = d + ys * v->h, * dv = du + cs * v->ch;
        for ( int y = 0; y < v->ch; y++ ) {
            memcpy( du + y * cs, f->data[1] + ( size_t ) y * f->linesize[1], v->cw );
            memcpy( dv + y * cs, f->data[2] + ( size_t ) y * f->linesize[2], v->cw );
        }
    } else {
        uint8_t * dst[4] = { v->vq[slot].rgba, NULL, NULL, NULL };
        int dst_ls[4] = { v->w * 4, 0, 0, 0 };
        if ( v->vrot ) {
            /* el video va girado (los del movil grabados en vertical): se escala al
               tamano del fotograma y luego se gira, que es lo que espera el juego */
            uint8_t * tmp[4] = { v->rotbuf, NULL, NULL, NULL };
            int tmp_ls[4] = { v->vw * 4, 0, 0, 0 };
            sws_scale( v->sws, ( const uint8_t * const * ) f->data, f->linesize, 0, f->height, tmp, tmp_ls );
            rotate_rgba( v->vq[slot].rgba, v->rotbuf, v->vw, v->vh, v->vrot );
        } else
            sws_scale( v->sws, ( const uint8_t * const * ) f->data, f->linesize, 0, f->height, dst, dst_ls );
    }
    v->vq[slot].pts = pts;

    SDL_LockMutex( v->mx );
    vclock_align( v, pts );
    if ( !v->seek_req ) v->vq_n++;
    SDL_UnlockMutex( v->mx );
    return 1;
}

/* the resampler runs at (mixer rate / speed): the sound is played faster or slower and the pitch goes with it (simple, no extra libraries) */
static void swr_apply_speed( VIDEO * v ) {
    if ( !v->swr ) return;
    swr_close( v->swr );
    av_opt_set_int( v->swr, "out_sample_rate", ( int64_t ) ( v->a_rate / v->speed + 0.5 ), 0 );
    swr_init( v->swr );
    v->swr_speed = v->speed;
}

static void push_audio( VIDEO * v, AVFrame * f ) {
    if ( !v->has_audio || !v->swr ) return;
    if ( v->swr_speed != v->speed ) swr_apply_speed( v );

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
    if ( !v->a_got ) {            /* sound that ends before the picture starts (audio list starting a little early): skip it */
        double ref = vid_clock( v ); if ( ref < v->vstart ) ref = v->vstart;
        if ( pts + ( double ) got * v->swr_speed / v->a_rate < ref - 0.02 ) { free( tmp ); return; }
    }

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
    v->a_pts_end = pts + ( double ) got * v->swr_speed / v->a_rate;
    if ( !v->a_got ) vtrace( "first audio data %u ms after open (pts %.2f)", SDL_GetTicks() - g_open_t0, pts );
    v->a_got = 1;
    SDL_UnlockMutex( v->mx );
    free( tmp );
}

static void drain_decoder( VIDEO * v, AVCodecContext * c, int is_video ) {
    int r;
    while ( !v->quit && !v->seek_req && ( r = avcodec_receive_frame( c, v->frame ) ) >= 0 ) {
        if ( is_video && v->hwpc && v->frame->format != AV_PIX_FMT_NV12 ) {
            /* el fotograma esta en la GPU: se baja a la memoria (el shader hace el cambio de color) */
            if ( av_hwframe_transfer_data( v->frame, v->frame, 0 ) < 0 ) { av_frame_unref( v->frame ); continue; }
        }
        if ( is_video ) push_video_frame( v, v->frame ); else push_audio( v, v->frame );
        av_frame_unref( v->frame );
    }
}

/* the console decoder failed: go on with libavcodec + swscale (decode thread) */
static void hw_switch_software( VIDEO * v ) {
    SDL_LockMutex( v->mx );
    double at = v->last_time;
    vq_clear( v );
    for ( int i = 0; i < VQ_MAX; i++ ) { free( v->vq[i].rgba ); v->vq[i].rgba = NULL; }
    v->vq_size = ( int ) ( VQ_BUDGET / ( ( size_t ) v->w * v->h * 4 ) );
    if ( v->vq_size > VQ_MAX ) v->vq_size = VQ_MAX;
    if ( v->vq_size < 3 ) v->vq_size = 3;
    for ( int i = 0; i < v->vq_size; i++ ) v->vq[i].rgba = ( uint8_t * ) calloc( 1, ( size_t ) v->w * v->h * 4 );
    if ( v->sws ) sws_freeContext( v->sws );
    v->sws = sws_getContext( v->w, v->h, v->vctx->pix_fmt, v->w, v->h, AV_PIX_FMT_RGBA, SWS_BILINEAR, NULL, NULL, NULL );
    HWDEC * h = v->hw;
    v->gpu = 0; v->hwmode = 0; v->fake_hw = 0; v->hw = NULL;
    avcodec_flush_buffers( v->vctx );
    v->shown_first = 0;
    v->seek_req = 1; v->seek_to = at;                  /* decode again from where it was */
    SDL_UnlockMutex( v->mx );
    if ( h ) hwdec_close( h );
}

static int decode_thread( void * arg ) {
    VIDEO * v = ( VIDEO * ) arg;
    AVPacket * pkt = av_packet_alloc();
    int tr_pk = 0;
    vtrace( "decode_thread: running" );

    while ( !v->quit ) {
        if ( v->hw && v->hw_fail ) hw_switch_software( v );
        /* seek requests */
        SDL_LockMutex( v->mx );
        if ( v->seek_req ) {
            double to = v->seek_to; v->seek_req = 0;
            SDL_UnlockMutex( v->mx );
            int64_t ts = ( int64_t ) ( to * AV_TIME_BASE );
            avformat_seek_file( v->fmt, -1, INT64_MIN, ts, ts, 0 );
            if ( v->hw && !v->hw_fail ) { hwdec_reset( v->hw ); if ( v->bsf ) av_bsf_flush( v->bsf ); v->hw_skip_rasl = 1; }
            if ( v->vctx ) avcodec_flush_buffers( v->vctx );
            if ( v->actx ) avcodec_flush_buffers( v->actx );
            SDL_LockMutex( v->mx );
            vq_clear( v );
            v->eof = 0; v->finished = 0; v->a_pts_end = to; v->a_got = 0; v->mh_wall = 0; v->vpts_init = 0;
            v->clk_base = to; v->clk_t0 = SDL_GetTicks(); v->clk_a = to; v->clk_a_t0 = v->clk_t0;
            v->shown_first = 0;
            SDL_UnlockMutex( v->mx );
            continue;
        }
        /* audio track switch requests (VIDEO_AUDIO_SELECT) - same thread that owns actx/swr,
           so no cross-thread AVCodecContext mutation */
        if ( v->audio_switch_req ) {
            int newstream = v->audio_switch_to;
            v->audio_switch_req = 0;
            SDL_UnlockMutex( v->mx );
            audio_switch_track( v, newstream );
            continue;
        }
        /* nothing to do while stopped/paused and the first frame is already queued */
        if ( ( !v->playing || v->paused || v->eof ) && ( v->vq_n > 0 || v->eof || v->audio_only ) ) {
            SDL_CondWaitTimeout( v->cond, v->mx, 20 );
            SDL_UnlockMutex( v->mx );
            continue;
        }
        SDL_UnlockMutex( v->mx );

        int r = av_read_frame( v->fmt, pkt );
        if ( r >= 0 && tr_pk < 4 ) { tr_pk++; vtrace( "decode_thread: packet #%d stream=%d size=%d", tr_pk, pkt->stream_index, pkt->size ); }
        if ( r < 0 && v->quit ) break;                      /* closing: the read was interrupted, do not flush the decoders */
        if ( r < 0 ) {
            /* flush decoders, then end / loop */
            if ( v->hw && !v->hw_fail ) { hwdec_flush( v->hw, hw_picture, v ); hwdec_reset( v->hw ); if ( v->bsf ) av_bsf_flush( v->bsf ); v->hw_skip_rasl = 1; }
            else if ( v->vctx ) { avcodec_send_packet( v->vctx, NULL ); drain_decoder( v, v->vctx, 1 ); avcodec_flush_buffers( v->vctx ); }
            if ( v->actx ) { avcodec_send_packet( v->actx, NULL ); drain_decoder( v, v->actx, 0 ); avcodec_flush_buffers( v->actx ); }
            SDL_LockMutex( v->mx );
            if ( v->loop && v->playing ) { v->seek_req = 1; v->seek_to = 0; }
            else v->eof = 1;
            SDL_UnlockMutex( v->mx );
            continue;
        }
        if ( pkt->stream_index == v->vstream && v->hw && !v->hw_fail ) {
            hw_feed( v, pkt );
        } else if ( pkt->stream_index == v->vstream && v->vctx ) {
            if ( avcodec_send_packet( v->vctx, pkt ) >= 0 ) drain_decoder( v, v->vctx, 1 );
        } else if ( pkt->stream_index == v->astream && v->actx && v->has_audio ) {
            if ( avcodec_send_packet( v->actx, pkt ) >= 0 ) drain_decoder( v, v->actx, 0 );
        } else {
            for ( int i = 0; i < v->nsub; i++ )
                if ( pkt->stream_index == v->sub_stream[i] && v->sub_ctx[i] ) {
                    sub_packet( v, v->sub_ctx[i], pkt, av_q2d( v->fmt->streams[pkt->stream_index]->time_base ), i );
                    break;
                }
        }
        av_packet_unref( pkt );
    }
    av_packet_free( &pkt );
    return 0;
}

/* ---- per-frame hook (main / GL thread) --------------------------------- */

static void vid_upload_slot( VIDEO * v, int slot ) {
    v->shown_count++;
    if ( v->shown_count == 2 ) vtrace( "second picture shown %u ms after open", SDL_GetTicks() - g_open_t0 );
    if ( !v->graph || !v->graph->tex ) return;
    if ( v->gpu ) yuv_render( v, v->vq[slot].rgba );
    else GPU_UpdateImageBytes( v->graph->tex, NULL, v->vq[slot].rgba, v->w * 4 );
}

static void vid_tick( VIDEO * v ) {
    int pick = -1;

    SDL_LockMutex( v->mx );
    double now = vid_clock( v );
    v->last_time = now;
    now -= v->av_off_ms / 1000.0;                       /* picture offset (sound / picture sync): the pictures are chosen with this time */

    if ( !v->shown_first && v->vq_n > 0 ) {            /* poster frame before play() */
        pick = v->vq_r; v->shown_first = 1; vtrace( "first picture %u ms after open", SDL_GetTicks() - g_open_t0 );
        vid_upload_slot( v, pick );
        pick = -1;
    }
    if ( v->playing && !v->paused ) {
        /* drop frames that are already late, keep the newest due one */
        while ( v->vq_n > 0 && v->vq[v->vq_r].pts <= now + 0.004 ) {
            pick = v->vq_r;
            v->vq_r = ( v->vq_r + 1 ) % v->vq_size; v->vq_n--;
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
    zimg_tick( 0 );
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

/* temporary crash tracing: appends a line to a file and syncs it, so it survives a crash */
static void vtrace( const char * fmt, ... ) {
    if ( !g_trace_path[0] ) return;
    FILE * f = fopen( g_trace_path, "ab" );
    if ( !f ) return;
    va_list ap; va_start( ap, fmt ); vfprintf( f, fmt, ap ); va_end( ap );
    fputc( '\n', f ); fflush( f ); fsync( fileno( f ) ); fclose( f );
}
int64_t libmod_video_http_trace( INSTANCE * my, int64_t * params ) {
    const char * s = ( const char * ) string_get( params[0] );
    if ( s && !strncmp( s, "PATH:", 5 ) ) { snprintf( g_trace_path, sizeof g_trace_path, "%s", s + 5 ); }
    else vtrace( "%s", s ? s : "" );
    string_discard( params[0] );
    return 1;
}

static void vid_free( VIDEO * v ) {
    vtrace( "vid_free: start thr=%p hw=%p", ( void * ) v->thr, ( void * ) v->hw );
    if ( v->wall0 > 0 )
        fprintf( stderr, "libmod_video: %dx%d %s: process CPU %.2f s in %.2f s wall (%.0f%% of one core)\n", v->w, v->h,
                 v->hw ? "hardware decoder" : ( v->hwkind == 1 ? "hardware decoder (NVDEC)" : ( v->hwkind == 2 ? "hardware decoder (FFmpeg)" : "software decoder" ) ),
                 now_s( CLOCK_PROCESS_CPUTIME_ID ) - v->cpu0, now_s( CLOCK_MONOTONIC ) - v->wall0,
                 100.0 * ( now_s( CLOCK_PROCESS_CPUTIME_ID ) - v->cpu0 ) / ( now_s( CLOCK_MONOTONIC ) - v->wall0 ) );
    if ( v->thr ) {
        SDL_LockMutex( v->mx ); v->quit = 1; SDL_CondBroadcast( v->cond ); SDL_UnlockMutex( v->mx );
        SDL_WaitThread( v->thr, NULL );
        v->thr = NULL;
    }
    vtrace( "vid_free: thread joined" );
    audio_release( v );
    vtrace( "vid_free: audio released" );
    for ( int i = 0; i < VQ_MAX; i++ ) free( v->vq[i].rgba );
    free( v->abuf );
    free( v->rotbuf ); v->rotbuf = NULL;
    if ( v->sws ) sws_freeContext( v->sws );
    if ( v->sws_fake ) sws_freeContext( v->sws_fake );
    free( v->fk_y ); free( v->fk_uv );
    vtrace( "vid_free: before hwdec_close" );
    if ( v->hw ) { hwdec_close( v->hw ); SDL_Delay( 120 ); }   /* let the console release the decoder before another one is created */
    vtrace( "vid_free: after hwdec_close" );
    if ( v->bsf ) av_bsf_free( &v->bsf );
    if ( v->bsf_pkt ) av_packet_free( &v->bsf_pkt );
    if ( v->swr ) swr_free( &v->swr );
    if ( v->frame ) av_frame_free( &v->frame );
    if ( v->vctx ) avcodec_free_context( &v->vctx );
    if ( v->actx ) avcodec_free_context( &v->actx );
    for ( int i = 0; i < MAX_SUBS; i++ ) if ( v->sub_ctx[i] ) avcodec_free_context( &v->sub_ctx[i] );
    for ( int i = 0; i < v->ncues; i++ ) free( v->cues[i].text );
    free( v->cues );
    vtrace( "vid_free: before close_input" );
    if ( v->fmt ) avformat_close_input( &v->fmt );
    vtrace( "vid_free: after close_input" );
    if ( v->graph_code ) zombie_add( v->graph_code );
    /* the target belongs to the graph's texture: freeing that image frees it, never free it twice */
    zimg_add( v->img_y, NULL ); zimg_add( v->img_u, NULL ); zimg_add( v->img_v, NULL );
    if ( v->cond ) SDL_DestroyCond( v->cond );
    if ( v->mx ) SDL_DestroyMutex( v->mx );
    memset( v, 0, sizeof( *v ) );
    vtrace( "vid_free: done" );
}

/* ---- decodificacion por hardware en PC -------------------------------------------------------
   En la consola el motor tiene su propio decodificador (libSceVideodec2, ver libmod_video_hwdec.c);
   en el PC no hay ninguno, asi que se usa el de FFmpeg con la GPU del sistema: VAAPI en Linux,
   D3D11VA o NVDEC en Windows y VideoToolbox en macOS. Los fotogramas se descargan a la memoria
   (el shader hace el cambio de color, igual que en la consola) y si no hay ningun dispositivo
   disponible, o el codec no lo admite, se sigue con el decodificador de software. */
static const enum AVHWDeviceType pc_hw_kinds[] = {
    AV_HWDEVICE_TYPE_VAAPI, AV_HWDEVICE_TYPE_D3D11VA, AV_HWDEVICE_TYPE_VDPAU,
    AV_HWDEVICE_TYPE_CUDA, AV_HWDEVICE_TYPE_VIDEOTOOLBOX
};

static int pc_hw_open( AVFormatContext * fmt, int idx, AVCodecContext ** out, int * kind ) {
    AVCodecParameters * par = fmt->streams[idx]->codecpar;
    const AVCodec * codec = avcodec_find_decoder( par->codec_id );
    if ( !codec ) return 0;
    if ( par->codec_id != AV_CODEC_ID_H264 && par->codec_id != AV_CODEC_ID_HEVC
     && par->codec_id != AV_CODEC_ID_VP9 && par->codec_id != AV_CODEC_ID_AV1 ) return 0;
    /* NVIDIA: los decodificadores cuvid (NVDEC) no usan el marco de hwaccel, son decodificadores
       aparte que se abren por su identificador y entregan los fotogramas en memoria */
    {
        const AVCodec * cc = NULL;
        if ( par->codec_id == AV_CODEC_ID_H264 ) cc = avcodec_find_decoder_by_name( "h264_cuvid" );
        else if ( par->codec_id == AV_CODEC_ID_HEVC ) cc = avcodec_find_decoder_by_name( "hevc_cuvid" );
        else if ( par->codec_id == AV_CODEC_ID_VP9 ) cc = avcodec_find_decoder_by_name( "vp9_cuvid" );
        else if ( par->codec_id == AV_CODEC_ID_AV1 ) cc = avcodec_find_decoder_by_name( "av1_cuvid" );
        if ( cc ) {
            AVCodecContext * c = avcodec_alloc_context3( cc );
            if ( c ) {
                if ( avcodec_parameters_to_context( c, par ) >= 0 ) {
                    c->thread_count = 0;
                    if ( avcodec_open2( c, cc, NULL ) >= 0 ) {
                        vtrace( "video_open: hardware decoder cuvid (NVDEC)" );
                        *kind = 1;
                        *out = c;
                        return 1;
                    }
                }
                avcodec_free_context( &c );
            }
        }
    }
    for ( size_t i = 0; i < sizeof( pc_hw_kinds ) / sizeof( pc_hw_kinds[0] ); i++ ) {
        int has = 0;
        for ( int k = 0; ; k++ ) {                                  /* el codec acepta este dispositivo? */
            const AVCodecHWConfig * c2 = avcodec_get_hw_config( codec, k );
            if ( !c2 ) break;
            if ( c2->device_type == pc_hw_kinds[i] ) { has = 1; break; }
        }
        if ( !has ) continue;
        AVBufferRef * dev = NULL;
        if ( av_hwdevice_ctx_create( &dev, pc_hw_kinds[i], NULL, NULL, 0 ) < 0 ) { dev = NULL; continue; }
        AVCodecContext * c = avcodec_alloc_context3( codec );
        if ( !c ) { av_buffer_unref( &dev ); continue; }
        if ( avcodec_parameters_to_context( c, par ) < 0 ) { avcodec_free_context( &c ); av_buffer_unref( &dev ); continue; }
        c->thread_count = 0;
        c->hw_device_ctx = av_buffer_ref( dev );                      /* el decoder crea su contexto de fotogramas */
        if ( avcodec_open2( c, codec, NULL ) < 0 ) { avcodec_free_context( &c ); av_buffer_unref( &dev ); continue; }
        av_buffer_unref( &dev );
        vtrace( "video_open: hardware decoder %s", av_hwdevice_get_type_name( pc_hw_kinds[i] ) );
        *out = c;
        return 1;
    }
    return 0;
}

static AVCodecContext * open_codec( AVFormatContext * fmt, int idx ) {
    AVCodecParameters * par = fmt->streams[idx]->codecpar;
    const AVCodec * codec = avcodec_find_decoder( par->codec_id );
    if ( !codec ) return NULL;
    AVCodecContext * c = avcodec_alloc_context3( codec );
    if ( !c ) return NULL;
    if ( avcodec_parameters_to_context( c, par ) < 0 ) { avcodec_free_context( &c ); return NULL; }
    c->thread_count = 0;                               /* let FFmpeg pick */
    if ( c->codec_type == AVMEDIA_TYPE_VIDEO && ( long ) par->width * par->height > 2300000L )
        c->thread_count = 4;                           /* every frame thread keeps whole 4K pictures: bound the memory */
    if ( avcodec_open2( c, codec, NULL ) < 0 ) { avcodec_free_context( &c ); return NULL; }
    return c;
}

static void audio_switch_track( VIDEO * v, int newstream ) {
    if ( v->actx ) avcodec_free_context( &v->actx );
    if ( v->swr ) swr_free( &v->swr );
    v->astream = newstream;
    v->actx = open_codec( v->fmt, v->astream );
    int ok = 0;
    if ( v->actx ) {
        v->atb = av_q2d( v->fmt->streams[v->astream]->time_base );
        enum AVSampleFormat sf = av_fmt_for( g_mix_fmt, &v->a_bps );
        AVChannelLayout out_l; av_channel_layout_default( &out_l, v->a_ch );
        if ( swr_alloc_set_opts2( &v->swr, &out_l, sf, v->a_rate, &v->actx->ch_layout, v->actx->sample_fmt, v->actx->sample_rate, 0, NULL ) >= 0
             && swr_init( v->swr ) >= 0 ) { ok = 1; v->swr_speed = 1.0; if ( v->speed != 1.0 ) swr_apply_speed( v ); }
        else if ( v->swr ) swr_free( &v->swr );
        av_channel_layout_uninit( &out_l );
    }
    if ( !ok && v->actx ) avcodec_free_context( &v->actx );

    SDL_LockMutex( v->mx );
    vid_clock_freeze( v ); v->a_got = 0; v->mh_wall = 0;                /* keep time on the wall clock until the new track's sound arrives */
    v->an = 0; v->ar = 0;                              /* drop whatever was queued from the old track */
    v->has_audio = ok && v->abuf != NULL;
    SDL_UnlockMutex( v->mx );
}

static int64_t video_open_internal( const char * path, const char * headers ) {
    g_open_t0 = SDL_GetTicks();
    {   /* el usuario y la clave no se escriben en la traza */
        char shown[160]; char * at; char * sc;
        snprintf( shown, sizeof shown, "%s", path ? path : "" );
        at = strchr( shown, '@' ); sc = strstr( shown, "://" );
        if ( at && sc && at > sc ) memmove( sc + 3, at + 1, strlen( at + 1 ) + 1 );
        vtrace( "video_open: %.100s", shown );
    }
    int h;
    for ( h = 1; h <= MAX_VIDEOS; h++ ) if ( !g_vid[h].used ) break;
    if ( h > MAX_VIDEOS ) { fprintf( stderr, "VIDEO_OPEN: too many open videos\n" ); return 0; }

    VIDEO * v = &g_vid[h];
    memset( v, 0, sizeof( *v ) );
    v->used = 1; v->volume = 1.0; v->speed = v->swr_speed = 1.0; v->sub_delay_ms = v->av_off_ms = 0; v->vstream = v->astream = -1;

    {   /* network URLs: bounded waits (a dead server must not freeze the script) and automatic reconnects */
        AVDictionary * nopts = NULL;
        if ( strstr( path, "://" ) && strncmp( path, "file:", 5 ) ) {
            av_dict_set( &nopts, "rw_timeout", "10000000", 0 );          /* 10 s, in microseconds */
            av_dict_set( &nopts, "reconnect", "1", 0 );
            av_dict_set( &nopts, "reconnect_streamed", "1", 0 );
            av_dict_set( &nopts, "reconnect_delay_max", "5", 0 );
        }
        else {                                                              /* a local .m3u8 whose segments are on the network (saved by a script that needed headers) */
            size_t pl = strlen( path );
            if ( pl > 5 && !strcasecmp( path + pl - 5, ".m3u8" ) ) {
                av_dict_set( &nopts, "protocol_whitelist", "file,crypto,data,http,https,tls,tcp,udp", 0 );
                /* an edit list in the audio init segment makes the demuxer drop the first seconds of samples *it reads*; with a
                   playlist that starts mid-stream (after a seek) that would eat real sound */
                av_dict_set( &nopts, "seg_format_options", "ignore_editlist=1", 0 );
                /* the audio list may start a few seconds before the video one: probe long enough to see both */
                av_dict_set( &nopts, "analyzeduration", "15000000", 0 );
            }
        }
        v->fmt = avformat_alloc_context();
        if ( v->fmt ) { v->fmt->interrupt_callback.callback = vid_interrupt; v->fmt->interrupt_callback.opaque = v; }
        if ( headers && headers[0] ) av_dict_set( &nopts, "headers", headers, 0 );   /* cuenta de una lista (Authorization, Cookie...) */
        int orc = avformat_open_input( &v->fmt, path, NULL, &nopts );
        av_dict_free( &nopts );
        if ( orc < 0 ) { fprintf( stderr, "VIDEO_OPEN: cannot open %s\n", path ); goto fail; }
    }
    vtrace( "video_open: input opened" );
    if ( avformat_find_stream_info( v->fmt, NULL ) < 0 ) { fprintf( stderr, "VIDEO_OPEN: no stream info in %s\n", path ); goto fail; }
    vtrace( "video_open: stream info done (%u ms)", SDL_GetTicks() - g_open_t0 );

    v->vstream = av_find_best_stream( v->fmt, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0 );
    v->astream = av_find_best_stream( v->fmt, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0 );
    if ( v->vstream < 0 ) {
        if ( v->astream < 0 ) { fprintf( stderr, "VIDEO_OPEN: %s has no video or audio stream\n", path ); goto fail; }
        v->audio_only = 1;                              /* a music file: no picture, only sound (and the visualiser data) */
    }

    /* every audio track (VIDEO_AUDIO_TRACKS/_LANG/_SELECT); v->astream above is just the
       "best" one av_find_best_stream picked as the initial default */
    for ( unsigned i = 0; i < v->fmt->nb_streams && v->naudio < MAX_ATRACKS; i++ )
        if ( v->fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO )
            v->audio_stream[v->naudio++] = i;

    /* other video streams (HLS offers each quality as a stream): never read them, so the demuxer does not download them */
    for ( unsigned i = 0; i < v->fmt->nb_streams; i++ )
        if ( ( int ) i != v->vstream && v->fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO
             && !( v->fmt->streams[i]->disposition & AV_DISPOSITION_ATTACHED_PIC ) )
            v->fmt->streams[i]->discard = AVDISCARD_ALL;

    if ( v->audio_only ) {
        v->duration = v->fmt->duration > 0 ? v->fmt->duration / ( double ) AV_TIME_BASE : 0;
        goto after_video;
    }
    vtrace( "video_open: best streams v=%d a=%d, opening video codec", v->vstream, v->astream );
    v->vctx = open_codec( v->fmt, v->vstream );
    vtrace( "video_open: video codec opened (%p)", ( void * ) v->vctx );
    if ( !v->vctx ) { fprintf( stderr, "VIDEO_OPEN: no decoder for the video in %s\n", path ); goto fail; }
    if ( v->vctx->pix_fmt == AV_PIX_FMT_NONE || v->vctx->width <= 0 ) { fprintf( stderr, "VIDEO_OPEN: no picture format in %s (no video data found while probing)\n", path ); goto fail; }
    v->vw = v->vctx->width; v->vh = v->vctx->height;
    v->w = v->vw; v->h = v->vh;
    /* Los videos grabados en vertical (en el movil) traen una matriz de rotacion en
       el contenedor: el decodificador por software ya gira los fotogramas, asi que
       el ancho y el alto que se usan son los del fotograma girado (y no los del
       stream). Con el decodificador por hardware de la GPU los fotogramas no
       vienen girados, asi que ahi se dejan como estan. */
    int vrot = 0;
    {
        AVStream * st = v->fmt->streams[v->vstream];
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 60, 1, 0 )
        const AVPacketSideData * sd = st ? av_packet_side_data_get( st->codecpar->coded_side_data, st->codecpar->nb_coded_side_data, AV_PKT_DATA_DISPLAYMATRIX ) : NULL;
#elif LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 60, 0, 0 )
        const AVPacketSideData * sd = st ? av_packet_side_data_get( st->side_data, AV_PKT_DATA_DISPLAYMATRIX, NULL ) : NULL;
#else
        const AVPacketSideData * sd = st ? av_packet_side_data_get( st->coded_side, AV_PKT_DATA_DISPLAYMATRIX, NULL ) : NULL;
#endif
        uint8_t * mat = sd ? ( uint8_t * ) sd->data : NULL;
        double rot = mat ? av_display_rotation_get( ( int32_t * ) mat ) : 0.0;
        double a = fabs( rot );
        /* la rotacion es en sentido antihorario: los videos del movil grabados en
           vertical traen -90 (o 270). Se compara con margen porque el valor
           exacto depende de la matriz y no es siempre redondo */
        if ( a > 45.0 && a < 135.0 ) vrot = ( rot > 0 ) ? 3 : 1;
        else if ( a > 135.0 && a < 225.0 ) vrot = 2;
    }
    v->vrot = vrot;
    if ( vrot == 1 || vrot == 3 ) { v->w = v->vh; v->h = v->vw; }   /* el video se ve girado */
    if ( vrot ) vtrace( "video_open: el video va girado (%d)", vrot );
    v->vtb = av_q2d( v->fmt->streams[v->vstream]->time_base );
    { int64_t st = v->fmt->streams[v->vstream]->start_time; v->vstart = st != AV_NOPTS_VALUE ? st * v->vtb : 0; if ( v->vstart < 0 ) v->vstart = 0; v->clk_base = v->vstart; v->clk_a = v->vstart; v->clk_a_t0 = SDL_GetTicks(); }
    v->duration = v->fmt->duration > 0 ? v->fmt->duration / ( double ) AV_TIME_BASE : 0;

    /* colour conversion on the GPU: 8-bit 4:2:0 from libavcodec, or NV12 / 16-bit NV12 from the console's decoder */
    {
        enum AVPixelFormat pf = v->vctx->pix_fmt;
        int fmt8 = pf == AV_PIX_FMT_YUV420P || pf == AV_PIX_FMT_YUVJ420P;
        int fmt10 = pf == AV_PIX_FMT_YUV420P10LE;
        int is_h264 = v->vctx->codec_id == AV_CODEC_ID_H264, is_hevc = v->vctx->codec_id == AV_CODEC_ID_HEVC;
        int is_vp9 = v->vctx->codec_id == AV_CODEC_ID_VP9, is_av1 = v->vctx->codec_id == AV_CODEC_ID_AV1;
        int want_hw = g_use_hw && !vrot && !( v->fmt->streams[v->vstream]->disposition & AV_DISPOSITION_ATTACHED_PIC ) && ( ( is_h264 && fmt8 ) || ( is_hevc && ( fmt8 || fmt10 ) ) );
        if ( ( g_use_gpu && fmt8 ) || want_hw || ( g_fake_hw && ( fmt8 || fmt10 ) ) ) {
            if ( !g_yuv_tried ) yuv_program_build();
        }
        if ( g_yuv_prog ) {
            if ( want_hw && hwdec_available() ) {
                vtrace( "video_open: creating hw decoder %dx%d", v->w, v->h );
                v->hw = hwdec_open( is_hevc, v->w, v->h );
                vtrace( "video_open: hw decoder created (%p)", ( void * ) v->hw );
                if ( v->hw ) {
                    const AVBitStreamFilter * bf = av_bsf_get_by_name( is_hevc ? "hevc_mp4toannexb" : "h264_mp4toannexb" );
                    if ( bf && av_bsf_alloc( bf, &v->bsf ) >= 0 ) {
                        avcodec_parameters_copy( v->bsf->par_in, v->fmt->streams[v->vstream]->codecpar );
                        v->bsf->time_base_in = v->fmt->streams[v->vstream]->time_base;
                        if ( av_bsf_init( v->bsf ) < 0 ) av_bsf_free( &v->bsf );
                    }
                    v->bsf_pkt = av_packet_alloc();
                    if ( !v->bsf || !v->bsf_pkt ) {
                        if ( v->bsf ) av_bsf_free( &v->bsf );
                        if ( v->bsf_pkt ) av_packet_free( &v->bsf_pkt );
                        hwdec_close( v->hw ); v->hw = NULL;
                    } else {
                        v->hwmode = fmt10 ? 2 : 1;
                        /* the output is 8 bits anyway: SDR 10-bit goes up as 8-bit NV12 (half the bytes, a
                           shader pass over one byte per sample); HDR10 (PQ) keeps its 16 bits */
                        if ( fmt10 && v->vctx->color_trc != AVCOL_TRC_SMPTE2084 ) { v->hwmode = 1; v->down10 = 1; }
                        v->hw_skip_rasl = 1;
                    }
                }
            }
            /* PC: el decodificador por hardware de FFmpeg (VAAPI, D3D11VA, NVDEC, VideoToolbox) */
            if ( !v->hw && !v->fake_hw && g_pc_hw && fmt8 && ( is_h264 || is_hevc || is_vp9 || is_av1 )
                 && !( v->fmt->streams[v->vstream]->disposition & AV_DISPOSITION_ATTACHED_PIC )
                 && !vrot ) {      /* con el video girado los fotogramas de la GPU no vienen girados */
                AVCodecContext * hc = NULL;
                int kind = 0;
                if ( pc_hw_open( v->fmt, v->vstream, &hc, &kind ) ) {
                    avcodec_free_context( &v->vctx );
                    v->vctx = hc;
                    v->hwkind = kind;
                    if ( kind == 1 || kind == 2 ) v->hwmode = 1;       /* NV12 */
                    if ( kind == 2 ) v->hwpc = 1;                     /* el fotograma viene de la GPU */
                    v->gpu = 1;
                    if ( !g_yuv_tried ) yuv_program_build();
                }
            }
            if ( !v->hw && g_fake_hw && ( fmt8 || fmt10 ) ) { v->fake_hw = 1; v->hwmode = fmt10 ? 2 : 1; }
            if ( v->hw || v->fake_hw ) v->gpu = 1;
            else if ( !v->hwkind && g_use_gpu && fmt8 && !vrot ) { v->gpu = 1; v->hwmode = 0; }
        }
    }
    if ( v->gpu ) {
        v->cw = ( v->w + 1 ) / 2; v->ch = ( v->h + 1 ) / 2;
        if ( v->hwmode == 0 )      { v->ypw = ( v->w + 3 ) / 4;     v->cpw = ( v->cw + 3 ) / 4; }
        else if ( v->hwmode == 1 ) { v->ypw = ( v->w + 3 ) / 4;     v->cpw = ( v->cw * 2 + 3 ) / 4; }
        else                       { v->ypw = ( v->w * 2 + 3 ) / 4; v->cpw = v->cw; }
        v->slot_bytes = ( size_t ) v->ypw * 4 * v->h + ( size_t ) v->cpw * 4 * v->ch * ( v->hwmode == 0 ? 2 : 1 );
        v->gp[0] = ( float ) v->w;
        v->gp[1] = ( v->vctx->pix_fmt == AV_PIX_FMT_YUVJ420P || v->vctx->color_range == AVCOL_RANGE_JPEG ) ? 1.0f : 0.0f;
        v->gp[2] = ( v->vctx->colorspace == AVCOL_SPC_BT2020_NCL || v->vctx->colorspace == AVCOL_SPC_BT2020_CL ) ? 2.0f
                 : ( v->vctx->colorspace == AVCOL_SPC_BT709 || ( v->vctx->colorspace == AVCOL_SPC_UNSPECIFIED && v->h >= 720 ) ) ? 1.0f : 0.0f;
        v->gp[3] = ( float ) v->hwmode;
        v->transfer = v->vctx->color_trc == AVCOL_TRC_SMPTE2084 ? 1.0f : 0.0f;
        if ( v->fake_hw ) {
            enum AVPixelFormat dst = v->hwmode == 2 ? AV_PIX_FMT_P010LE : AV_PIX_FMT_NV12;
            v->sws_fake = sws_getContext( v->vw, v->vh, v->vctx->pix_fmt, v->vw, v->vh, dst, SWS_BILINEAR, NULL, NULL, NULL );
            v->fk_ypitch = ( v->hwmode == 2 ? v->w * 2 : v->w ) + 64;
            v->fk_uvpitch = v->cw * ( v->hwmode == 2 ? 4 : 2 ) + 64;
            v->fk_y = ( uint8_t * ) calloc( 1, ( size_t ) v->fk_ypitch * v->h );
            v->fk_uv = ( uint8_t * ) calloc( 1, ( size_t ) v->fk_uvpitch * v->ch );
            if ( !v->sws_fake || !v->fk_y || !v->fk_uv ) goto fail;
        }
    } else {
        v->sws = sws_getContext( v->vw, v->vh, v->vctx->pix_fmt, v->vw, v->vh, AV_PIX_FMT_RGBA, SWS_BILINEAR, NULL, NULL, NULL );
        if ( !v->sws ) goto fail;
        v->slot_bytes = ( size_t ) v->w * v->h * 4;
    }
    v->vq_size = ( int ) ( VQ_BUDGET / ( v->slot_bytes ? v->slot_bytes : 1 ) );
    if ( v->vq_size > VQ_MAX ) v->vq_size = VQ_MAX;
    if ( v->vq_size < 3 ) v->vq_size = 3;
    for ( int i = 0; i < v->vq_size; i++ ) { v->vq[i].rgba = ( uint8_t * ) calloc( 1, v->slot_bytes ); if ( !v->vq[i].rgba ) goto fail; }
    if ( v->vrot && !v->rotbuf ) {
        v->rotbuf = ( uint8_t * ) calloc( 1, ( size_t ) v->w * v->h * 4 );
        if ( !v->rotbuf ) goto fail;
    }
after_video:
    vtrace( "video_open: after_video (hw=%p)", ( void * ) v->hw );
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

    /* embedded text subtitle tracks (decoded as the demuxer reaches them) */
    v->sub_sel = -1;
    for ( unsigned i = 0; i < v->fmt->nb_streams && v->nsub < MAX_SUBS; i++ ) {
        AVCodecParameters * par = v->fmt->streams[i]->codecpar;
        if ( par->codec_type != AVMEDIA_TYPE_SUBTITLE ) continue;
        const AVCodecDescriptor * d = avcodec_descriptor_get( par->codec_id );
        if ( !d || !( d->props & AV_CODEC_PROP_TEXT_SUB ) ) continue;
        AVCodecContext * c = open_codec( v->fmt, i );
        if ( !c ) continue;
        v->sub_stream[v->nsub] = i; v->sub_ctx[v->nsub] = c; v->nsub++;
    }

    /* the graph that shows the video */
    {
        SDL_Surface * s = SDL_CreateRGBSurfaceWithFormat( 0, v->audio_only ? 1 : v->w, v->audio_only ? 1 : v->h, 32, SDL_PIXELFORMAT_ABGR8888 );
        if ( !s ) goto fail;
        SDL_memset( s->pixels, 0, ( size_t ) s->pitch * s->h );
        v->graph = bitmap_new( 0, 0, 0, s );
        SDL_FreeSurface( s );
        if ( !v->graph ) goto fail;
        v->graph->code = bitmap_next_code();
        v->graph_code = v->graph->code;
        grlib_add_map( 0, v->graph );
    }
    if ( v->gpu ) {
        vtrace( "video_open: creating gpu images %dx%d", v->ypw, v->h );
        v->img_y = GPU_CreateImage( v->ypw, v->h,  GPU_FORMAT_RGBA );
        v->img_u = GPU_CreateImage( v->cpw, v->ch, GPU_FORMAT_RGBA );
        if ( v->hwmode == 0 ) v->img_v = GPU_CreateImage( v->cpw, v->ch, GPU_FORMAT_RGBA );
        if ( v->img_y && v->img_u && ( v->hwmode != 0 || v->img_v ) && v->graph->tex ) {
            GPU_SetImageFilter( v->img_y, GPU_FILTER_NEAREST );
            GPU_SetImageFilter( v->img_u, GPU_FILTER_NEAREST );
            if ( v->img_v ) GPU_SetImageFilter( v->img_v, GPU_FILTER_NEAREST );
            GPU_SetBlending( v->img_y, 0 );
            vtrace( "video_open: loading gpu target" );
            v->tgt = GPU_LoadTarget( v->graph->tex );
            vtrace( "video_open: gpu target loaded" );
        }
        if ( !v->tgt ) {                              /* GPU path unavailable: back to the CPU converter */
            fprintf( stderr, "libmod_video: GPU conversion unavailable, using software\n" );
            zimg_add( v->img_y, NULL ); zimg_add( v->img_u, NULL ); zimg_add( v->img_v, NULL );
            v->img_y = v->img_u = v->img_v = NULL;
            if ( v->hw ) { HWDEC * h = v->hw; v->hw = NULL; hwdec_close( h ); }
            v->fake_hw = 0; v->hwmode = 0;
            v->gpu = 0;
            v->sws = sws_getContext( v->w, v->h, v->vctx->pix_fmt, v->w, v->h, AV_PIX_FMT_RGBA, SWS_BILINEAR, NULL, NULL, NULL );
            if ( !v->sws ) goto fail;
            for ( int i = 0; i < VQ_MAX; i++ ) { free( v->vq[i].rgba ); v->vq[i].rgba = NULL; }
            v->vq_size = ( int ) ( VQ_BUDGET / ( ( size_t ) v->w * v->h * 4 ) );
            if ( v->vq_size > VQ_MAX ) v->vq_size = VQ_MAX;
            if ( v->vq_size < 3 ) v->vq_size = 3;
            for ( int i = 0; i < v->vq_size; i++ ) {
                v->vq[i].rgba = ( uint8_t * ) calloc( 1, ( size_t ) v->w * v->h * 4 );
                if ( !v->vq[i].rgba ) goto fail;
            }
        }
    }

    v->mx = SDL_CreateMutex(); v->cond = SDL_CreateCond();
    if ( !g_obj_id ) g_obj_id = gr_new_object( 0, obj_info, NULL, NULL );
    /* cover art (an mp3 / flac / m4a with an embedded picture): the demuxer never delivers that "stream"
       as packets, so decode the stored picture once and show it as the poster frame */
    if ( v->vctx && ( v->fmt->streams[v->vstream]->disposition & AV_DISPOSITION_ATTACHED_PIC ) ) {
        AVPacket * ap = &v->fmt->streams[v->vstream]->attached_pic;
        if ( ap->size > 0 && avcodec_send_packet( v->vctx, ap ) >= 0 ) {
            avcodec_send_packet( v->vctx, NULL );
            drain_decoder( v, v->vctx, 1 );
        }
    }
    #if SDL_VERSION_ATLEAST( 2, 0, 9 )
    v->cpu0 = now_s( CLOCK_PROCESS_CPUTIME_ID ); v->wall0 = now_s( CLOCK_MONOTONIC );
    vtrace( "video_open: starting decode thread" );
    v->thr = SDL_CreateThreadWithStackSize( decode_thread, "video", ( size_t ) 8 << 20, v );   /* the console's decoder needs a big stack */
#else
    v->thr = SDL_CreateThread( decode_thread, "video", v );
#endif
    if ( !v->thr ) goto fail;

    return h;

fail:
    vid_free( v );
    return 0;
}

/* VIDEO_OPEN(path): opens a video (the headers are not needed) */
int64_t libmod_video_open( INSTANCE * my, int64_t * params ) {
    const char * path = ( const char * ) string_get( params[0] );
    int64_t r = video_open_internal( path, "" );
    string_discard( params[0] );
    return r;
}

/* VIDEO_OPEN_H(path, headers): opens a video sending extra headers ("Cabecera: valor\r\n"),
   needed by the lists that ask for an account (Authorization, Cookie...) */
int64_t libmod_video_open_h( INSTANCE * my, int64_t * params ) {
    const char * path = ( const char * ) string_get( params[0] );
    const char * hdr = ( const char * ) string_get( params[1] );
    int64_t r = video_open_internal( path, hdr ? hdr : "" );
    string_discard( params[0] );
    string_discard( params[1] );
    return r;
}

/* VIDEO_USE_GPU(enable): colour conversion on the GPU for videos opened afterwards (default 1) */
int64_t libmod_video_use_gpu( INSTANCE * my, int64_t * params ) { g_use_gpu = params[0] ? 1 : 0; return g_use_gpu; }
/* VIDEO_FRAMES(handle): pictures shown so far (late ones are dropped to keep the clock) */
int64_t libmod_video_frames( INSTANCE * my, int64_t * params ) { VIDEO * v = vid_get( params[0] ); return v ? v->shown_count : 0; }
/* VIDEO_USE_HW(enable): console video decoder (PS5) for videos opened afterwards (default 1) */
int64_t libmod_video_use_hw( INSTANCE * my, int64_t * params ) { g_use_hw = params[0] ? 1 : 0; return g_use_hw; }
/* VIDEO_HW(handle): 1 if that video is decoded by the console's decoder block */
int64_t libmod_video_hw( INSTANCE * my, int64_t * params ) { VIDEO * v = vid_get( params[0] ); return v ? ( v->hw ? 1 : ( v->fake_hw ? 2 : 0 ) ) : 0; }
/* VIDEO_GPU(handle): 1 if that video converts on the GPU */
int64_t libmod_video_gpu( INSTANCE * my, int64_t * params ) { VIDEO * v = vid_get( params[0] ); return v ? v->gpu : 0; }

/* ---- visualiser data ------------------------------------------------------ */

#define SPEC_N 1024

static void fft_inplace( double * re, double * im, int n ) {
    for ( int i = 1, j = 0; i < n; i++ ) {                 /* bit reversal */
        int bit = n >> 1;
        for ( ; j & bit; bit >>= 1 ) j ^= bit;
        j ^= bit;
        if ( i < j ) { double t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for ( int len = 2; len <= n; len <<= 1 ) {
        double ang = -2.0 * M_PI / len, wr = cos( ang ), wi = sin( ang );
        for ( int i = 0; i < n; i += len ) {
            double cr = 1.0, ci = 0.0;
            for ( int k = 0; k < len / 2; k++ ) {
                int a = i + k, b = i + k + len / 2;
                double xr = re[b] * cr - im[b] * ci, xi = re[b] * ci + im[b] * cr;
                re[b] = re[a] - xr; im[b] = im[a] - xi;
                re[a] += xr;        im[a] += xi;
                double nr = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = nr;
            }
        }
    }
}

/* window of the mixer chunk that is being heard right now (the chunk is handed over in one piece, so the
   position inside it is estimated from the time elapsed since) */
static int vis_window( VIDEO * v, int16_t * out, int n ) {
    int ok = 0;
    SDL_LockMutex( v->mx );
    if ( v->vis_len > 0 && v->a_rate > 0 && v->playing && !v->paused ) {
        Uint32 el = SDL_GetTicks() - v->vis_t0;
        int end = ( int ) ( ( double ) el * v->a_rate / 1000.0 );
        if ( end > v->vis_len ) end = v->vis_len;
        if ( end < n ) end = n < v->vis_len ? n : v->vis_len;
        for ( int i = 0; i < n; i++ ) { int idx = end - n + i; out[i] = ( idx >= 0 && idx < v->vis_len ) ? v->vis[idx] : 0; }
        ok = 1;
    }
    SDL_UnlockMutex( v->mx );
    return ok;
}

static void spec_update( VIDEO * v ) {
    Uint32 now = SDL_GetTicks();
    if ( v->spec_tick == now ) return;
    v->spec_tick = now;
    int16_t win[SPEC_N];
    if ( !vis_window( v, win, SPEC_N ) ) { for ( int b = 0; b < 32; b++ ) v->spec[b] = 0; return; }
    double re[SPEC_N], im[SPEC_N];
    for ( int i = 0; i < SPEC_N; i++ ) {
        double w = 0.5 - 0.5 * cos( 2.0 * M_PI * i / ( SPEC_N - 1 ) );          /* Hann */
        re[i] = win[i] / 32768.0 * w; im[i] = 0;
    }
    fft_inplace( re, im, SPEC_N );
    double lo = 40.0, hi = 16000.0; if ( hi > v->a_rate / 2.0 - 100 ) hi = v->a_rate / 2.0 - 100;
    for ( int b = 0; b < 32; b++ ) {
        double f0 = lo * pow( hi / lo, b / 32.0 ), f1 = lo * pow( hi / lo, ( b + 1 ) / 32.0 );
        int b0 = ( int ) ( f0 * SPEC_N / v->a_rate ), b1 = ( int ) ( f1 * SPEC_N / v->a_rate );
        if ( b0 < 1 ) b0 = 1;
        if ( b1 <= b0 ) b1 = b0 + 1;
        if ( b1 > SPEC_N / 2 ) b1 = SPEC_N / 2;
        double m = 0;
        for ( int k = b0; k < b1; k++ ) { double mag = sqrt( re[k] * re[k] + im[k] * im[k] ) / ( SPEC_N / 4.0 ); if ( mag > m ) m = mag; }
        double db = 20.0 * log10( m + 1e-9 );
        double lvl = ( db + 62.0 ) / 62.0;
        v->spec[b] = lvl < 0 ? 0 : ( lvl > 1 ? 1 : lvl );
    }
}

/* VIDEO_SPECTRUM(handle, band): level 0..255 of one of 32 log-spaced bands (40 Hz - 16 kHz) of what is playing; 0 when paused */
int64_t libmod_video_spectrum( INSTANCE * my, int64_t * params ) {
    VIDEO * v = vid_get( params[0] ); if ( !v || params[1] < 0 || params[1] > 31 ) return 0;
    spec_update( v );
    return ( int64_t ) ( v->spec[params[1]] * 255.0 );
}

/* VIDEO_WAVE(handle, i): i = 0..255, signed sample -128..127 of the waveform that is playing (oscilloscope) */
int64_t libmod_video_wave( INSTANCE * my, int64_t * params ) {
    VIDEO * v = vid_get( params[0] ); if ( !v || params[1] < 0 || params[1] > 255 ) return 0;
    int16_t win[SPEC_N];
    if ( !vis_window( v, win, SPEC_N ) ) return 0;
    return win[params[1] * 4] / 256;
}

/* VIDEO_IS_AUDIO_ONLY(handle): 1 when the file has no picture (music) */
int64_t libmod_video_is_audio_only( INSTANCE * my, int64_t * params ) { VIDEO * v = vid_get( params[0] ); return v ? v->audio_only : 0; }

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

/* ---- subtitle API ------------------------------------------------------- */

int64_t libmod_video_subtitle_tracks( INSTANCE * my, int64_t * params ) {
    VIDEO * v = vid_get( params[0] ); if ( !v ) return 0;
    return v->nsub + ( v->sub_ext ? 1 : 0 );
}

int64_t libmod_video_subtitle_lang( INSTANCE * my, int64_t * params ) {
    VIDEO * v = vid_get( params[0] );
    char buf[256] = "";
    int i = ( int ) params[1];
    if ( v && i >= 0 && i < v->nsub ) sub_lang( v->fmt, v->sub_stream[i], buf, sizeof( buf ) );
    else if ( v && v->sub_ext && i == v->nsub ) snprintf( buf, sizeof( buf ), "external" );
    int64_t str = string_new( buf );
    string_use( str );
    return str;
}

int64_t libmod_video_subtitle_select( INSTANCE * my, int64_t * params ) {
    VIDEO * v = vid_get( params[0] ); if ( !v ) return 0;
    int t = ( int ) params[1];
    int max = v->nsub + ( v->sub_ext ? 1 : 0 );
    SDL_LockMutex( v->mx );
    v->sub_sel = ( t >= 0 && t < max ) ? t : -1;
    SDL_UnlockMutex( v->mx );
    return v->sub_sel >= 0;
}

/* ---- audio track API ----------------------------------------------------- */

int64_t libmod_video_audio_tracks( INSTANCE * my, int64_t * params ) {
    VIDEO * v = vid_get( params[0] ); if ( !v ) return 0;
    return v->naudio;
}

int64_t libmod_video_audio_lang( INSTANCE * my, int64_t * params ) {
    VIDEO * v = vid_get( params[0] );
    char buf[256] = "";
    int i = ( int ) params[1];
    if ( v && i >= 0 && i < v->naudio ) sub_lang( v->fmt, v->audio_stream[i], buf, sizeof( buf ) );
    int64_t str = string_new( buf );
    string_use( str );
    return str;
}

/* switch to audio track 't' (0-based, see VIDEO_AUDIO_TRACKS/VIDEO_AUDIO_LANG). The actual
   decoder/resampler swap happens in decode_thread (audio_switch_track()) - this just posts
   the request, since only that thread may touch actx/swr. May be heard as a brief gap. */
int64_t libmod_video_audio_select( INSTANCE * my, int64_t * params ) {
    VIDEO * v = vid_get( params[0] ); if ( !v ) return 0;
    int t = ( int ) params[1];
    if ( t < 0 || t >= v->naudio ) return 0;
    SDL_LockMutex( v->mx );
    if ( v->audio_stream[t] == v->astream ) { SDL_UnlockMutex( v->mx ); return 1; }   /* already selected */
    v->audio_switch_req = 1;
    v->audio_switch_to = v->audio_stream[t];
    SDL_UnlockMutex( v->mx );
    return 1;
}

/* load an external subtitle file (.srt .ass .ssa .vtt ...) as an extra track, which becomes selected */
int64_t libmod_video_subtitle_load( INSTANCE * my, int64_t * params ) {
    VIDEO * v = vid_get( params[0] );
    const char * path = ( const char * ) string_get( params[1] );
    int ok = 0;
    AVFormatContext * f = NULL;
    AVCodecContext * c = NULL;
    AVPacket * pkt = NULL;

    if ( !v ) goto done;
    if ( avformat_open_input( &f, path, NULL, NULL ) < 0 ) { fprintf( stderr, "VIDEO_SUBTITLE_LOAD: cannot open %s\n", path ); goto done; }
    if ( avformat_find_stream_info( f, NULL ) < 0 ) goto done;
    int si = av_find_best_stream( f, AVMEDIA_TYPE_SUBTITLE, -1, -1, NULL, 0 );
    if ( si < 0 ) { fprintf( stderr, "VIDEO_SUBTITLE_LOAD: no subtitle stream in %s\n", path ); goto done; }
    c = open_codec( f, si );
    if ( !c ) goto done;

    /* replace a previously loaded external track */
    SDL_LockMutex( v->mx );
    int w = 0;
    for ( int i = 0; i < v->ncues; i++ ) {
        if ( v->cues[i].track == v->nsub ) free( v->cues[i].text ); else v->cues[w++] = v->cues[i];
    }
    v->ncues = w;
    SDL_UnlockMutex( v->mx );

    pkt = av_packet_alloc();
    double tb = av_q2d( f->streams[si]->time_base );
    while ( av_read_frame( f, pkt ) >= 0 ) {
        if ( pkt->stream_index == si ) sub_packet( v, c, pkt, tb, v->nsub );
        av_packet_unref( pkt );
    }
    SDL_LockMutex( v->mx );
    v->sub_ext = 1;
    v->sub_sel = v->nsub;
    SDL_UnlockMutex( v->mx );
    ok = 1;

done:
    if ( pkt ) av_packet_free( &pkt );
    if ( c ) avcodec_free_context( &c );
    if ( f ) avformat_close_input( &f );
    string_discard( params[1] );
    return ok;
}

/* text due at the current playback time ("" if none) */
int64_t libmod_video_subtitle_text( INSTANCE * my, int64_t * params ) {
    VIDEO * v = vid_get( params[0] );
    char * out = NULL;
    size_t n = 0;
    if ( v ) {
        SDL_LockMutex( v->mx );
        if ( v->sub_sel >= 0 ) {
            double now = vid_clock( v ) - v->sub_delay_ms / 1000.0;
            for ( int i = 0; i < v->ncues; i++ ) {
                CUE * q = &v->cues[i];
                if ( q->track != v->sub_sel || now < q->start || now >= q->end ) continue;
                size_t l = strlen( q->text );
                char * p = ( char * ) realloc( out, n + l + 2 );
                if ( !p ) break;
                out = p;
                if ( n ) out[n++] = '\n';
                memcpy( out + n, q->text, l ); n += l; out[n] = 0;
            }
        }
        SDL_UnlockMutex( v->mx );
    }
    int64_t str = string_new( out ? out : "" );
    free( out );
    string_use( str );
    return str;
}

/* VIDEO_SPEED(handle, percent): 25..300 (100 = normal). The position is kept (the sound is restarted from it) */
int64_t libmod_video_speed( INSTANCE * my, int64_t * params ) {
    VIDEO * v = vid_get( params[0] ); if ( !v ) return 0;
    double sp = ( double ) params[1] / 100.0; if ( sp < 0.25 ) sp = 0.25; if ( sp > 3.0 ) sp = 3.0;
    SDL_LockMutex( v->mx );
    if ( sp != v->speed ) {
        double at = vid_clock( v );
        vid_clock_freeze( v );
        v->speed = sp;
        if ( v->has_audio && v->playing ) { v->seek_req = 1; v->seek_to = at; }      /* flushes the sound that was already converted at the old speed */
        vid_clock_freeze( v );
        SDL_CondBroadcast( v->cond );
    }
    SDL_UnlockMutex( v->mx );
    return 1;
}
int64_t libmod_video_get_speed( INSTANCE * my, int64_t * params ) { VIDEO * v = vid_get( params[0] ); return v ? ( int64_t ) ( v->speed * 100.0 + 0.5 ) : 100; }
/* VIDEO_SUB_DELAY(handle, ms): subtitles appear that many ms later (negative: earlier) */
int64_t libmod_video_sub_delay( INSTANCE * my, int64_t * params ) { VIDEO * v = vid_get( params[0] ); if ( !v ) return 0; v->sub_delay_ms = ( int ) params[1]; return 1; }
/* VIDEO_AV_OFFSET(handle, ms): the picture is shown that many ms later than normal (negative: earlier), to match a sound that is out of sync */
int64_t libmod_video_av_offset( INSTANCE * my, int64_t * params ) { VIDEO * v = vid_get( params[0] ); if ( !v ) return 0; v->av_off_ms = ( int ) params[1]; return 1; }

/* ------------------------------------------------------------------------ */

void __bgdexport( libmod_video, module_initialize )() {
#if LIBAVFORMAT_VERSION_MAJOR < 58
    av_register_all();
#endif
    av_log_set_level( AV_LOG_ERROR );
#ifndef __PROSPERO__
    { const char * e = getenv( "LIBMOD_VIDEO_FAKE_HW" ); g_fake_hw = e && *e == '1'; }
    { const char * e = getenv( "LIBMOD_VIDEO_PC_HW" ); g_pc_hw = !e || *e != '0'; }
#endif
}

void __bgdexport( libmod_video, module_finalize )() {
    torrent_shutdown();
    upload_stop();
    for ( int i = 1; i <= MAX_VIDEOS; i++ ) if ( g_vid[i].used ) vid_free( &g_vid[i] );
    zombie_tick( 1 );
    zimg_tick( 1 );
    if ( g_yuv_prog ) { GPU_FreeShaderProgram( g_yuv_prog ); g_yuv_prog = 0; }
    if ( g_obj_id ) { gr_destroy_object( g_obj_id ); g_obj_id = 0; }
}


/* ---- HTTP(S) requests for scripts ---------------------------------------- */
/* Downloads a whole answer through FFmpeg's network layer (the same one that plays https streams), so
   scripts get TLS without any extra library. Synchronous, bounded by a timeout. The body is kept in a
   module buffer until the next call: HTTP_FETCH() -> HTTP_LENGTH() -> HTTP_READ(). */

static char * g_http_body = NULL;
static size_t g_http_len = 0;
static char g_http_err[256] = "";

static int http_hex( const char * s, size_t n, char * out ) {
    static const char * h = "0123456789abcdef";
    size_t i;
    for ( i = 0; i < n; i++ ) { out[i * 2] = h[( unsigned char ) s[i] >> 4]; out[i * 2 + 1] = h[( unsigned char ) s[i] & 15]; }
    out[n * 2] = 0;
    return 1;
}

typedef struct { double deadline; } HttpInt;
static int http_interrupt( void * p ) {
    struct timespec t; clock_gettime( CLOCK_MONOTONIC, &t );
    return ( t.tv_sec + t.tv_nsec / 1e9 ) > ( ( HttpInt * ) p )->deadline;
}

/* progress of the request in course (read by HTTP_PROGRESS / HTTP_TOTAL while HTTP_FETCH_BEGIN's request runs) */
static volatile int64_t g_http_prog = 0, g_http_tot = 0;

/* the request itself: returns the status (200 ok, 400/401/403/404/500 http errors, 0 = no connection) */
static int http_do( const char * url, const char * hdr, const char * method, const char * body ) {
    size_t blen = strlen( body );
    int status = 0;
    AVDictionary * o = NULL;
    AVIOContext * io = NULL;
    struct timespec t;
    HttpInt hi;
    AVIOInterruptCB cb;
    int r;
    static int net_ready = 0;

    free( g_http_body ); g_http_body = NULL; g_http_len = 0; g_http_err[0] = 0; g_http_prog = 0; g_http_tot = 0;
    if ( strncmp( url, "http://", 7 ) && strncmp( url, "https://", 8 ) ) { snprintf( g_http_err, sizeof g_http_err, "only http:// and https:// addresses" ); return 0; }
    if ( !net_ready ) { avformat_network_init(); net_ready = 1; }

    av_dict_set( &o, "rw_timeout", "15000000", 0 );
    av_dict_set( &o, "user_agent", "BennuGD2-MediaPlayer-Addon/1.0", 0 );
    av_dict_set( &o, "seekable", "0", 0 );
    if ( hdr[0] ) av_dict_set( &o, "headers", hdr, 0 );
    if ( method[0] && strcmp( method, "GET" ) ) av_dict_set( &o, "method", method, 0 );
    if ( blen ) {
        char * hx = ( char * ) malloc( blen * 2 + 1 );
        if ( !hx ) { av_dict_free( &o ); snprintf( g_http_err, sizeof g_http_err, "out of memory" ); return 0; }
        http_hex( body, blen, hx );
        av_dict_set( &o, "post_data", hx, 0 );                  /* binary options are given as hex text */
        free( hx );
        if ( !method[0] || !strcmp( method, "GET" ) ) av_dict_set( &o, "method", "POST", 0 );
    }

    clock_gettime( CLOCK_MONOTONIC, &t );
    hi.deadline = t.tv_sec + t.tv_nsec / 1e9 + 40.0;            /* hard limit for the whole request */
    cb.callback = http_interrupt; cb.opaque = &hi;
    r = avio_open2( &io, url, AVIO_FLAG_READ, &cb, &o );
    av_dict_free( &o );
    if ( r < 0 ) {
        if ( r == AVERROR_HTTP_BAD_REQUEST ) status = 400;
        else if ( r == AVERROR_HTTP_UNAUTHORIZED ) status = 401;
        else if ( r == AVERROR_HTTP_FORBIDDEN ) status = 403;
        else if ( r == AVERROR_HTTP_NOT_FOUND ) status = 404;
        else if ( r == AVERROR_HTTP_OTHER_4XX ) status = 400;
        else if ( r == AVERROR_HTTP_SERVER_ERROR ) status = 500;
        else { char e[128]; av_strerror( r, e, sizeof e ); snprintf( g_http_err, sizeof g_http_err, "%s", e ); }
        return status;
    }
    {
        size_t cap = 64 << 10, n = 0, max = 8u << 20;
        char * buf = ( char * ) malloc( cap + 1 );
        int64_t total = avio_size( io );
        g_http_tot = total > 0 ? total : 0;                     /* 0 = the server did not say how big it is */
        while ( buf ) {
            int k;
            if ( n + 16384 > cap ) {
                char * nb;
                if ( cap >= max ) break;
                cap *= 2; nb = ( char * ) realloc( buf, cap + 1 );
                if ( !nb ) { free( buf ); buf = NULL; break; }
                buf = nb;
            }
            k = avio_read( io, ( unsigned char * ) buf + n, 16384 );
            if ( k <= 0 ) break;
            n += ( size_t ) k;
            g_http_prog = ( int64_t ) n;
        }
        avio_closep( &io );
        if ( !buf ) { snprintf( g_http_err, sizeof g_http_err, "out of memory" ); return 0; }
        buf[n] = 0; g_http_body = buf; g_http_len = n; status = 200;
    }
    return status;
}

/* HTTP_FETCH(url, headers, method, body): the request, waiting for it */
int64_t libmod_video_http_fetch( INSTANCE * my, int64_t * params ) {
    char * url = strdup( ( const char * ) string_get( params[0] ) );
    char * hdr = strdup( ( const char * ) string_get( params[1] ) );
    char * method = strdup( ( const char * ) string_get( params[2] ) );
    char * body = strdup( ( const char * ) string_get( params[3] ) );
    int status = 0;
    string_discard( params[0] ); string_discard( params[1] ); string_discard( params[2] ); string_discard( params[3] );
    if ( url && hdr && method && body ) status = http_do( url, hdr, method, body );
    else snprintf( g_http_err, sizeof g_http_err, "out of memory" );
    free( url ); free( hdr ); free( method ); free( body );
    return status;
}

/* HTTP_FETCH_BEGIN(url, headers, method, body): the same request in the background (0 = started); then HTTP_FETCH_POLL() every frame
   (-1 while it runs, the status once it is done), with HTTP_PROGRESS() / HTTP_TOTAL() to draw a progress bar. The answer is read with
   HTTP_LENGTH / HTTP_READ / HTTP_SAVE as usual. One request at a time. */
typedef struct { char * url, * hdr, * method, * body; } HttpJob;
static SDL_atomic_t g_hasync_state;           /* 0 idle, 1 running, 2 done */
static volatile int g_hasync_status = 0;

static int http_thread( void * arg ) {
    HttpJob * j = ( HttpJob * ) arg;
    int st = http_do( j->url, j->hdr, j->method, j->body );
    free( j->url ); free( j->hdr ); free( j->method ); free( j->body ); free( j );
    g_hasync_status = st;
    SDL_AtomicSet( &g_hasync_state, 2 );
    return 0;
}

int64_t libmod_video_http_fetch_begin( INSTANCE * my, int64_t * params ) {
    HttpJob * j;
    SDL_Thread * th;
    int64_t ret = 0;
    char * url = strdup( ( const char * ) string_get( params[0] ) );
    char * hdr = strdup( ( const char * ) string_get( params[1] ) );
    char * method = strdup( ( const char * ) string_get( params[2] ) );
    char * body = strdup( ( const char * ) string_get( params[3] ) );
    string_discard( params[0] ); string_discard( params[1] ); string_discard( params[2] ); string_discard( params[3] );
    if ( SDL_AtomicGet( &g_hasync_state ) == 1 ) ret = -1;                      /* one at a time */
    else if ( !url || !hdr || !method || !body || !( j = ( HttpJob * ) malloc( sizeof( *j ) ) ) ) ret = -2;
    else {
        j->url = url; j->hdr = hdr; j->method = method; j->body = body;
        g_http_prog = 0; g_http_tot = 0;
        SDL_AtomicSet( &g_hasync_state, 1 );
        th = SDL_CreateThread( http_thread, "http", j );
        if ( !th ) {
            SDL_AtomicSet( &g_hasync_state, 0 );
            free( url ); free( hdr ); free( method ); free( body ); free( j );
            return -3;
        }
        SDL_DetachThread( th );
        return 0;
    }
    free( url ); free( hdr ); free( method ); free( body );
    return ret;
}

int64_t libmod_video_http_fetch_poll( INSTANCE * my, int64_t * params ) {
    int st = SDL_AtomicGet( &g_hasync_state );
    if ( st == 1 ) return -1;
    if ( st == 2 ) { SDL_AtomicSet( &g_hasync_state, 0 ); return g_hasync_status; }
    return -3;                                                                  /* nothing was started */
}

int64_t libmod_video_http_progress( INSTANCE * my, int64_t * params ) { return g_http_prog; }
int64_t libmod_video_http_total( INSTANCE * my, int64_t * params ) { return g_http_tot; }

int64_t libmod_video_http_length( INSTANCE * my, int64_t * params ) { return ( int64_t ) g_http_len; }

/* HTTP_READ(pointer, max): copies up to max bytes of the last answer; returns the bytes copied */
int64_t libmod_video_http_read( INSTANCE * my, int64_t * params ) {
    size_t n = g_http_len, mx = ( size_t ) params[1];
    if ( !g_http_body || !params[0] ) return 0;
    if ( n > mx ) n = mx;
    memcpy( ( void * )( intptr_t ) params[0], g_http_body, n );
    return ( int64_t ) n;
}

/* HTTP_SAVE(path): writes the last answer to a real file (the same view of the disk FFmpeg uses); 1 = ok */
int64_t libmod_video_http_save( INSTANCE * my, int64_t * params ) {
    const char * path = ( const char * ) string_get( params[0] );
    int ok = 0;
    if ( g_http_body && path && path[0] ) {
        FILE * f = fopen( path, "wb" );
        if ( f ) { ok = fwrite( g_http_body, 1, g_http_len, f ) == g_http_len; if ( fclose( f ) != 0 ) ok = 0; }
    }
    string_discard( params[0] );
    return ok;
}

/* ---- DOWNLOAD: copies a video (HLS list, data: list or file) into one .mp4, in the background --------------------------- */
/* VIDEO_DOWNLOAD_BEGIN(source, headers, out_path) -> 0 started; VIDEO_DOWNLOAD_POLL() -> -1 running, 0 done, -2 cancelled, -10 failed (VIDEO_DOWNLOAD_ERROR()
   says why); VIDEO_DOWNLOAD_PERCENT() -> 0..100; VIDEO_DOWNLOAD_CANCEL(). The best video and the best audio are copied as they
   are (no re-encoding), like `ffmpeg -c copy` does. One download at a time. */

typedef struct { char * src, * hdr, * out; int audio; char conv[16]; } DlJob;   /* conv: "" = copy; "mp3" "opus" "flac" "wav" "aac" = convert the audio */
static SDL_atomic_t g_dl_state;               /* 0 idle, 1 running, 2 done */
static volatile int g_dl_result = 0, g_dl_percent = 0, g_dl_cancel = 0;
static char g_dl_err[256] = "";
static char g_dl_out[1024] = "";                /* the file really written (in audio mode the extension follows the codec) */

static int dl_interrupt( void * p ) { ( void ) p; return g_dl_cancel; }

/* opens the source (network options, local .m3u8 whitelist, headers) and reads its stream info; 0 = ok, else g_dl_err says why */
static int dl_open( DlJob * j, AVFormatContext ** icp ) {
    AVFormatContext * ic = avformat_alloc_context();
    AVDictionary * o = NULL;
    if ( !ic ) { snprintf( g_dl_err, sizeof g_dl_err, "out of memory" ); return -1; }
    ic->interrupt_callback.callback = dl_interrupt;
    if ( strstr( j->src, "://" ) && strncmp( j->src, "file:", 5 ) ) {
        av_dict_set( &o, "rw_timeout", "15000000", 0 );
        av_dict_set( &o, "reconnect", "1", 0 );
        av_dict_set( &o, "reconnect_streamed", "1", 0 );
        av_dict_set( &o, "reconnect_delay_max", "5", 0 );
    } else {
        size_t pl = strlen( j->src );
        if ( pl > 5 && !strcasecmp( j->src + pl - 5, ".m3u8" ) ) {
            av_dict_set( &o, "protocol_whitelist", "file,crypto,data,http,https,tls,tcp,udp", 0 );
            av_dict_set( &o, "seg_format_options", "ignore_editlist=1", 0 );
        }
    }
    if ( j->hdr[0] ) av_dict_set( &o, "headers", j->hdr, 0 );
    { int orc = avformat_open_input( &ic, j->src, NULL, &o );
      av_dict_free( &o );
      if ( orc < 0 ) { char e[96]; av_strerror( orc, e, sizeof e ); snprintf( g_dl_err, sizeof g_dl_err, "could not open the source (%s)", e ); return -1; } }
    if ( avformat_find_stream_info( ic, NULL ) < 0 ) { snprintf( g_dl_err, sizeof g_dl_err, "no stream info" ); avformat_close_input( &ic ); return -1; }
    *icp = ic;
    return 0;
}

#if defined( __PROSPERO__ ) || defined( DL_LAME_DIRECT )
#include <lame/lame.h>
/* MP3 with LAME called directly: the console's FFmpeg is built without libmp3lame, but the library is there.
   Decode and resample with FFmpeg to 16-bit interleaved, feed LAME, write the stream raw (with an ID3v2 tag). */
static int dl_convert_lame( DlJob * j ) {
    AVFormatContext * ic = NULL;
    AVCodecContext * dec = NULL;
    SwrContext * swr = NULL;
    AVFrame * fr = NULL;
    AVPacket * pkt = NULL;
    AVIOContext * pb = NULL;
    lame_t g = NULL;
    unsigned char * mp3 = NULL;
    short * pcm = NULL;
    int pcm_cap = 0, mp3_cap = 0;
    const AVCodec * dc;
    AVChannelLayout lay = { 0 };
    AVDictionaryEntry * t;
    int as, ret = -10, rate, ch, flushing = 0, written = 0;
    double dur;
    static const int ok_rates[] = { 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000, 0 };

    g_dl_err[0] = 0;
    if ( dl_open( j, &ic ) != 0 ) goto end;
    as = av_find_best_stream( ic, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0 );
    if ( as < 0 ) { snprintf( g_dl_err, sizeof g_dl_err, "no audio to save" ); goto end; }
    for ( int i = 0; i < ( int ) ic->nb_streams; i++ ) if ( i != as ) ic->streams[i]->discard = AVDISCARD_ALL;
    dc = avcodec_find_decoder( ic->streams[as]->codecpar->codec_id );
    if ( !dc ) { snprintf( g_dl_err, sizeof g_dl_err, "no decoder for the audio" ); goto end; }
    dec = avcodec_alloc_context3( dc );
    if ( !dec || avcodec_parameters_to_context( dec, ic->streams[as]->codecpar ) < 0 || avcodec_open2( dec, dc, NULL ) < 0 ) { snprintf( g_dl_err, sizeof g_dl_err, "could not open the audio decoder" ); goto end; }

    ch = dec->ch_layout.nb_channels == 1 ? 1 : 2;
    rate = 44100;
    for ( const int * p = ok_rates; *p; p++ ) if ( *p == dec->sample_rate ) rate = *p;
    av_channel_layout_default( &lay, ch );
    if ( swr_alloc_set_opts2( &swr, &lay, AV_SAMPLE_FMT_S16, rate, &dec->ch_layout, dec->sample_fmt, dec->sample_rate, 0, NULL ) < 0 || swr_init( swr ) < 0 ) { snprintf( g_dl_err, sizeof g_dl_err, "could not set up the resampler" ); goto end; }

    g = lame_init();
    if ( !g ) { snprintf( g_dl_err, sizeof g_dl_err, "could not start LAME" ); goto end; }
    lame_set_in_samplerate( g, rate );
    lame_set_num_channels( g, ch );
    lame_set_mode( g, ch == 1 ? MONO : JOINT_STEREO );
    lame_set_brate( g, 192 );
    lame_set_quality( g, 5 );
    id3tag_init( g ); id3tag_add_v2( g );
    if ( ( t = av_dict_get( ic->metadata, "title", NULL, 0 ) ) ) id3tag_set_title( g, t->value );
    if ( ( t = av_dict_get( ic->metadata, "artist", NULL, 0 ) ) ) id3tag_set_artist( g, t->value );
    if ( ( t = av_dict_get( ic->metadata, "album", NULL, 0 ) ) ) id3tag_set_album( g, t->value );
    if ( lame_init_params( g ) < 0 ) { snprintf( g_dl_err, sizeof g_dl_err, "LAME refused the settings" ); goto end; }

    snprintf( g_dl_out, sizeof g_dl_out, "%s", j->out );
    { char * dot = strrchr( g_dl_out, '.' ), * sl = strrchr( g_dl_out, '/' );
      if ( dot && ( !sl || dot > sl ) ) *dot = 0;
      if ( strlen( g_dl_out ) + 4 < sizeof g_dl_out ) strcat( g_dl_out, ".mp3" ); }
    if ( avio_open( &pb, g_dl_out, AVIO_FLAG_WRITE ) < 0 ) { snprintf( g_dl_err, sizeof g_dl_err, "could not write %s", g_dl_out ); goto end; }

    pkt = av_packet_alloc(); fr = av_frame_alloc();
    if ( !pkt || !fr ) { snprintf( g_dl_err, sizeof g_dl_err, "out of memory" ); goto end; }
    dur = ic->duration > 0 ? ic->duration / ( double ) AV_TIME_BASE : 0.0;

    for ( ;; ) {
        int got = 0;
        if ( g_dl_cancel ) { ret = -2; snprintf( g_dl_err, sizeof g_dl_err, "cancelled" ); goto end; }
        if ( !flushing ) {
            int r = av_read_frame( ic, pkt );
            if ( r < 0 ) { avcodec_send_packet( dec, NULL ); flushing = 1; }
            else {
                if ( pkt->stream_index == as ) {
                    if ( dur > 0 && pkt->pts != AV_NOPTS_VALUE ) {
                        AVStream * ist = ic->streams[as];
                        int pc = ( int ) ( 100.0 * ( ( pkt->pts - ( ist->start_time != AV_NOPTS_VALUE ? ist->start_time : 0 ) ) * av_q2d( ist->time_base ) ) / dur );
                        g_dl_percent = pc < 0 ? 0 : ( pc > 99 ? 99 : pc );
                    }
                    avcodec_send_packet( dec, pkt );
                }
                av_packet_unref( pkt );
            }
        }
        while ( avcodec_receive_frame( dec, fr ) >= 0 ) {
            int cap = swr_get_out_samples( swr, fr->nb_samples ) + 64, n, bytes;
            if ( cap > pcm_cap ) { free( pcm ); pcm = ( short * ) malloc( sizeof( short ) * ch * cap ); pcm_cap = pcm ? cap : 0; }
            if ( 5 * cap / 4 + 7200 > mp3_cap ) { free( mp3 ); mp3_cap = 5 * cap / 4 + 7200; mp3 = ( unsigned char * ) malloc( mp3_cap ); if ( !mp3 ) mp3_cap = 0; }
            if ( !pcm || !mp3 ) { snprintf( g_dl_err, sizeof g_dl_err, "out of memory" ); goto end; }
            n = swr_convert( swr, ( uint8_t ** ) &pcm, cap, ( const uint8_t ** ) fr->extended_data, fr->nb_samples );
            if ( n > 0 ) {
                bytes = ch == 1 ? lame_encode_buffer( g, pcm, pcm, n, mp3, mp3_cap ) : lame_encode_buffer_interleaved( g, pcm, n, mp3, mp3_cap );
                if ( bytes < 0 ) { snprintf( g_dl_err, sizeof g_dl_err, "LAME error %d", bytes ); goto end; }
                if ( bytes > 0 ) { avio_write( pb, mp3, bytes ); written = 1; }
            }
            av_frame_unref( fr );
            got = 1;
        }
        if ( flushing && !got ) break;
    }
    if ( !mp3 ) { mp3_cap = 7200; mp3 = ( unsigned char * ) malloc( mp3_cap ); }
    if ( mp3 ) { int bytes = lame_encode_flush( g, mp3, mp3_cap ); if ( bytes > 0 ) { avio_write( pb, mp3, bytes ); written = 1; } }
    if ( !written ) { snprintf( g_dl_err, sizeof g_dl_err, "no audio could be decoded" ); goto end; }
    avio_flush( pb );
    g_dl_percent = 100;
    ret = 0;
end:
    if ( g_dl_cancel && ret != 0 ) { ret = -2; snprintf( g_dl_err, sizeof g_dl_err, "cancelled" ); }
    free( pcm ); free( mp3 );
    if ( g ) lame_close( g );
    if ( pb ) avio_closep( &pb );
    av_packet_free( &pkt ); av_frame_free( &fr );
    if ( swr ) swr_free( &swr );
    if ( dec ) avcodec_free_context( &dec );
    av_channel_layout_uninit( &lay );
    if ( ic ) avformat_close_input( &ic );
    return ret;
}
#endif

/* audio converted to another codec: decode, resample, encode and write (mp3, opus, flac, wav, aac) */
static int dl_convert( DlJob * j ) {
    AVFormatContext * ic = NULL, * oc = NULL;
    AVCodecContext * dec = NULL, * enc = NULL;
    SwrContext * swr = NULL;
    AVAudioFifo * fifo = NULL;
    AVFrame * fr = NULL, * cv = NULL, * ef = NULL;
    AVPacket * pkt = NULL, * op = NULL;
    const AVCodec * dc, * ec;
    const char * ename = "aac", * fmt = "mp4", * ext = ".m4a";
    int as, ret = -10, header = 0, rate, nb, r, flushing = 0;
    int64_t next_pts = 0;
    double dur;
    AVChannelLayout lay = { 0 };

    g_dl_err[0] = 0;
#if defined( __PROSPERO__ ) || defined( DL_LAME_DIRECT )
    if ( !strcmp( j->conv, "mp3" ) ) return dl_convert_lame( j );
#endif
    if ( dl_open( j, &ic ) != 0 ) goto end;
    as = av_find_best_stream( ic, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0 );
    if ( as < 0 ) { snprintf( g_dl_err, sizeof g_dl_err, "no audio to save" ); goto end; }
    for ( int i = 0; i < ( int ) ic->nb_streams; i++ ) if ( i != as ) ic->streams[i]->discard = AVDISCARD_ALL;

    if      ( !strcmp( j->conv, "mp3" ) )  { ename = "libmp3lame"; fmt = "mp3";  ext = ".mp3"; }
    else if ( !strcmp( j->conv, "opus" ) ) { ename = "libopus";    fmt = "ogg";  ext = ".opus"; }
    else if ( !strcmp( j->conv, "flac" ) ) { ename = "flac";       fmt = "flac"; ext = ".flac"; }
    else if ( !strcmp( j->conv, "wav" ) )  { ename = "pcm_s16le";  fmt = "wav";  ext = ".wav"; }
    ec = avcodec_find_encoder_by_name( ename );
    if ( !ec && !strcmp( ename, "libopus" ) ) { ename = "opus"; ec = avcodec_find_encoder_by_name( ename ); }      /* builds without libopus: FFmpeg's own (experimental) encoder */
    if ( !ec ) { snprintf( g_dl_err, sizeof g_dl_err, "this build cannot encode %s", j->conv ); goto end; }
    dc = avcodec_find_decoder( ic->streams[as]->codecpar->codec_id );
    if ( !dc ) { snprintf( g_dl_err, sizeof g_dl_err, "no decoder for the audio" ); goto end; }
    dec = avcodec_alloc_context3( dc );
    if ( !dec || avcodec_parameters_to_context( dec, ic->streams[as]->codecpar ) < 0 || avcodec_open2( dec, dc, NULL ) < 0 ) { snprintf( g_dl_err, sizeof g_dl_err, "could not open the audio decoder" ); goto end; }

    /* output format: mono stays mono, anything else is stereo; the rate is the source's unless the encoder wants another */
    av_channel_layout_default( &lay, dec->ch_layout.nb_channels == 1 ? 1 : 2 );
    rate = dec->sample_rate > 0 ? dec->sample_rate : 44100;
    if ( !strcmp( ename, "libopus" ) || !strcmp( ename, "opus" ) ) rate = 48000;
    else if ( ec->supported_samplerates ) {
        int best = ec->supported_samplerates[0];
        for ( const int * p = ec->supported_samplerates; *p; p++ ) if ( abs( *p - rate ) < abs( best - rate ) ) best = *p;
        rate = best;
    }
    enc = avcodec_alloc_context3( ec );
    if ( !enc ) goto end;
    enc->sample_fmt = ec->sample_fmts ? ec->sample_fmts[0] : AV_SAMPLE_FMT_S16;
    enc->sample_rate = rate;
    av_channel_layout_copy( &enc->ch_layout, &lay );
    enc->time_base = ( AVRational ){ 1, rate };
    if ( !strcmp( ename, "libmp3lame" ) ) enc->bit_rate = 192000;
    else if ( !strcmp( ename, "libopus" ) || !strcmp( ename, "opus" ) ) enc->bit_rate = 128000;
    else if ( !strcmp( ename, "aac" ) ) enc->bit_rate = 192000;
    if ( avformat_alloc_output_context2( &oc, NULL, fmt, j->out ) < 0 || !oc ) { snprintf( g_dl_err, sizeof g_dl_err, "no %s muxer in this build", fmt ); goto end; }
    if ( oc->oformat->flags & AVFMT_GLOBALHEADER ) enc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    enc->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;           /* the native opus / vorbis encoders are marked experimental */
    if ( avcodec_open2( enc, ec, NULL ) < 0 ) { snprintf( g_dl_err, sizeof g_dl_err, "could not open the %s encoder", j->conv ); goto end; }
    { AVStream * ost = avformat_new_stream( oc, NULL );
      if ( !ost || avcodec_parameters_from_context( ost->codecpar, enc ) < 0 ) { snprintf( g_dl_err, sizeof g_dl_err, "stream setup failed" ); goto end; }
      ost->time_base = enc->time_base; }
    av_dict_copy( &oc->metadata, ic->metadata, 0 );                      /* title, artist... */

    snprintf( g_dl_out, sizeof g_dl_out, "%s", j->out );                 /* the extension follows the format asked for */
    { char * dot = strrchr( g_dl_out, '.' ), * sl = strrchr( g_dl_out, '/' );
      if ( dot && ( !sl || dot > sl ) ) *dot = 0;
      if ( strlen( g_dl_out ) + strlen( ext ) < sizeof g_dl_out ) strcat( g_dl_out, ext ); }
    if ( avio_open( &oc->pb, g_dl_out, AVIO_FLAG_WRITE ) < 0 ) { snprintf( g_dl_err, sizeof g_dl_err, "could not write %s", g_dl_out ); goto end; }
    if ( avformat_write_header( oc, NULL ) < 0 ) { snprintf( g_dl_err, sizeof g_dl_err, "could not start the file" ); goto end; }
    header = 1;

    if ( swr_alloc_set_opts2( &swr, &enc->ch_layout, enc->sample_fmt, enc->sample_rate, &dec->ch_layout, dec->sample_fmt, dec->sample_rate, 0, NULL ) < 0 || swr_init( swr ) < 0 ) { snprintf( g_dl_err, sizeof g_dl_err, "could not set up the resampler" ); goto end; }
    fifo = av_audio_fifo_alloc( enc->sample_fmt, enc->ch_layout.nb_channels, 1 );
    nb = enc->frame_size > 0 ? enc->frame_size : 1024;                   /* fixed-size encoders (mp3, aac, opus, flac) need exactly this many samples */
    pkt = av_packet_alloc(); op = av_packet_alloc(); fr = av_frame_alloc();
    if ( !fifo || !pkt || !op || !fr ) { snprintf( g_dl_err, sizeof g_dl_err, "out of memory" ); goto end; }
    dur = ic->duration > 0 ? ic->duration / ( double ) AV_TIME_BASE : 0.0;

    for ( ;; ) {
        int got_in = 0;
        if ( g_dl_cancel ) { ret = -2; snprintf( g_dl_err, sizeof g_dl_err, "cancelled" ); goto end; }
        if ( !flushing ) {                                         /* feed the decoder */
            r = av_read_frame( ic, pkt );
            if ( r < 0 ) { avcodec_send_packet( dec, NULL ); flushing = 1; }
            else {
                if ( pkt->stream_index == as ) {
                    if ( dur > 0 && pkt->pts != AV_NOPTS_VALUE ) {
                        AVStream * ist = ic->streams[as];
                        int pc = ( int ) ( 100.0 * ( ( pkt->pts - ( ist->start_time != AV_NOPTS_VALUE ? ist->start_time : 0 ) ) * av_q2d( ist->time_base ) ) / dur );
                        g_dl_percent = pc < 0 ? 0 : ( pc > 99 ? 99 : pc );
                    }
                    avcodec_send_packet( dec, pkt );
                }
                av_packet_unref( pkt );
            }
        }
        while ( avcodec_receive_frame( dec, fr ) >= 0 ) {          /* decoded -> resampled -> fifo */
            int64_t maxout = swr_get_out_samples( swr, fr->nb_samples );
            uint8_t ** buf = NULL; int lsz = 0, n;
            if ( maxout < 1 ) maxout = fr->nb_samples + 256;
            if ( av_samples_alloc_array_and_samples( &buf, &lsz, enc->ch_layout.nb_channels, ( int ) maxout, enc->sample_fmt, 0 ) < 0 ) { av_frame_unref( fr ); continue; }
            n = swr_convert( swr, buf, ( int ) maxout, ( const uint8_t ** ) fr->extended_data, fr->nb_samples );
            if ( n > 0 ) av_audio_fifo_write( fifo, ( void ** ) buf, n );
            av_freep( &buf[0] ); av_freep( &buf );
            av_frame_unref( fr );
            got_in = 1;
        }
        if ( flushing && !got_in ) {                               /* decoder drained: what is left in the resampler */
            uint8_t ** buf = NULL; int lsz = 0, n;
            if ( av_samples_alloc_array_and_samples( &buf, &lsz, enc->ch_layout.nb_channels, 4096, enc->sample_fmt, 0 ) >= 0 ) {
                n = swr_convert( swr, buf, 4096, NULL, 0 );
                if ( n > 0 ) av_audio_fifo_write( fifo, ( void ** ) buf, n );
                av_freep( &buf[0] ); av_freep( &buf );
            }
        }
        /* encode every full frame in the fifo (and, once the input is over, the last partial one) */
        while ( av_audio_fifo_size( fifo ) >= nb || ( flushing && av_audio_fifo_size( fifo ) > 0 ) ) {
            int take = av_audio_fifo_size( fifo ) < nb ? av_audio_fifo_size( fifo ) : nb;
            ef = av_frame_alloc();
            if ( !ef ) goto end;
            ef->nb_samples = take; ef->format = enc->sample_fmt; ef->sample_rate = enc->sample_rate;
            av_channel_layout_copy( &ef->ch_layout, &enc->ch_layout );
            if ( av_frame_get_buffer( ef, 0 ) < 0 ) { av_frame_free( &ef ); goto end; }
            av_audio_fifo_read( fifo, ( void ** ) ef->data, take );
            ef->pts = next_pts; next_pts += take;
            if ( avcodec_send_frame( enc, ef ) < 0 ) { av_frame_free( &ef ); snprintf( g_dl_err, sizeof g_dl_err, "encoder error" ); goto end; }
            av_frame_free( &ef );
            while ( avcodec_receive_packet( enc, op ) >= 0 ) {
                av_packet_rescale_ts( op, enc->time_base, oc->streams[0]->time_base );
                op->stream_index = 0;
                if ( av_interleaved_write_frame( oc, op ) < 0 ) { snprintf( g_dl_err, sizeof g_dl_err, "write failed (disk full?)" ); goto end; }
            }
        }
        if ( flushing && !got_in && av_audio_fifo_size( fifo ) == 0 ) break;
    }
    avcodec_send_frame( enc, NULL );                               /* flush the encoder */
    while ( avcodec_receive_packet( enc, op ) >= 0 ) {
        av_packet_rescale_ts( op, enc->time_base, oc->streams[0]->time_base );
        op->stream_index = 0;
        if ( av_interleaved_write_frame( oc, op ) < 0 ) { snprintf( g_dl_err, sizeof g_dl_err, "write failed (disk full?)" ); goto end; }
    }
    if ( av_write_trailer( oc ) < 0 ) { snprintf( g_dl_err, sizeof g_dl_err, "could not finish the file" ); header = 0; goto end; }
    header = 0;
    g_dl_percent = 100;
    ret = 0;
end:
    if ( g_dl_cancel && ret != 0 ) { ret = -2; snprintf( g_dl_err, sizeof g_dl_err, "cancelled" ); }
    if ( header && oc ) av_write_trailer( oc );
    av_packet_free( &pkt ); av_packet_free( &op ); av_frame_free( &fr ); av_frame_free( &cv );
    if ( fifo ) av_audio_fifo_free( fifo );
    if ( swr ) swr_free( &swr );
    if ( dec ) avcodec_free_context( &dec );
    if ( enc ) avcodec_free_context( &enc );
    av_channel_layout_uninit( &lay );
    if ( oc ) { if ( oc->pb ) avio_closep( &oc->pb ); avformat_free_context( oc ); }
    if ( ic ) avformat_close_input( &ic );
    return ret;
}

static int dl_run( DlJob * j ) {
    AVFormatContext * ic = NULL, * oc = NULL;
    AVBSFContext * bsf = NULL;
    AVPacket * pkt = NULL;
    int * map = NULL;
    int vs, as, ret = -10, header = 0, i;
    double dur;

    g_dl_err[0] = 0;
    if ( j->conv[0] ) return dl_convert( j );
    if ( dl_open( j, &ic ) != 0 ) goto end;

    vs = j->audio ? -1 : av_find_best_stream( ic, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0 );
    as = av_find_best_stream( ic, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0 );
    if ( vs < 0 && as < 0 ) { snprintf( g_dl_err, sizeof g_dl_err, j->audio ? "no audio to save" : "no video or audio to save" ); goto end; }

    snprintf( g_dl_out, sizeof g_dl_out, "%s", j->out );
    { const char * fmt = "mp4", * ext = NULL;
      if ( j->audio && as >= 0 ) {                        /* audio only: the container follows the codec, so the file is a plain .m4a / .mp3 / .opus ... */
          switch ( ic->streams[as]->codecpar->codec_id ) {
              case AV_CODEC_ID_MP3:    fmt = "mp3";  ext = ".mp3";  break;
              case AV_CODEC_ID_OPUS:   fmt = "ogg";  ext = ".opus"; break;
              case AV_CODEC_ID_VORBIS: fmt = "ogg";  ext = ".oga";  break;
              case AV_CODEC_ID_FLAC:   fmt = "flac"; ext = ".flac"; break;
              default: break;
          }
      }
      if ( ext ) {                                       /* replace the extension the caller gave */
          char * dot = strrchr( g_dl_out, '.' ), * sl = strrchr( g_dl_out, '/' );
          if ( dot && ( !sl || dot > sl ) ) *dot = 0;
          if ( strlen( g_dl_out ) + strlen( ext ) < sizeof g_dl_out ) strcat( g_dl_out, ext );
      }
      if ( avformat_alloc_output_context2( &oc, NULL, fmt, g_dl_out ) < 0 || !oc ) {
          if ( ext ) { snprintf( g_dl_out, sizeof g_dl_out, "%s", j->out ); }          /* that muxer is not in this build: .m4a instead */
          if ( avformat_alloc_output_context2( &oc, NULL, "mp4", j->out ) < 0 || !oc ) { snprintf( g_dl_err, sizeof g_dl_err, "no mp4 muxer" ); goto end; }
      }
    }
    map = ( int * ) malloc( sizeof( int ) * ic->nb_streams );
    if ( !map ) { snprintf( g_dl_err, sizeof g_dl_err, "out of memory" ); goto end; }
    for ( i = 0; i < ( int ) ic->nb_streams; i++ ) {
        map[i] = -1;
        if ( i == vs || i == as ) {
            AVStream * ist = ic->streams[i], * ost = avformat_new_stream( oc, NULL );
            if ( !ost || avcodec_parameters_copy( ost->codecpar, ist->codecpar ) < 0 ) { snprintf( g_dl_err, sizeof g_dl_err, "stream copy failed" ); goto end; }
            ost->codecpar->codec_tag = 0;
            ost->time_base = ist->time_base;
            map[i] = ost->index;
            if ( i == as && ist->codecpar->codec_id == AV_CODEC_ID_AAC && ist->codecpar->extradata_size == 0 ) {   /* ADTS AAC (from a TS list) */
                const AVBitStreamFilter * f = av_bsf_get_by_name( "aac_adtstoasc" );
                if ( f && av_bsf_alloc( f, &bsf ) >= 0 ) {
                    avcodec_parameters_copy( bsf->par_in, ist->codecpar );
                    bsf->time_base_in = ist->time_base;
                    if ( av_bsf_init( bsf ) < 0 ) av_bsf_free( &bsf );
                    else avcodec_parameters_copy( ost->codecpar, bsf->par_out );
                }
            }
        } else ic->streams[i]->discard = AVDISCARD_ALL;                 /* other qualities: never downloaded */
    }
    if ( avio_open( &oc->pb, g_dl_out, AVIO_FLAG_WRITE ) < 0 ) { snprintf( g_dl_err, sizeof g_dl_err, "could not write %s", g_dl_out ); goto end; }
    if ( avformat_write_header( oc, NULL ) < 0 ) { snprintf( g_dl_err, sizeof g_dl_err, "could not start the file" ); goto end; }
    header = 1;

    dur = ic->duration > 0 ? ic->duration / ( double ) AV_TIME_BASE : 0.0;
    pkt = av_packet_alloc();
    for ( ;; ) {
        int r = av_read_frame( ic, pkt );
        AVStream * ist; AVStream * ost;
        if ( g_dl_cancel ) { ret = -2; snprintf( g_dl_err, sizeof g_dl_err, "cancelled" ); goto end; }
        if ( r < 0 ) break;                                           /* end of the video (or an error after the data: the file keeps what arrived) */
        if ( pkt->stream_index >= ( int ) ic->nb_streams || map[pkt->stream_index] < 0 ) { av_packet_unref( pkt ); continue; }
        ist = ic->streams[pkt->stream_index];
        ost = oc->streams[map[pkt->stream_index]];
        if ( pkt->stream_index == ( vs >= 0 ? vs : as ) && dur > 0 && pkt->pts != AV_NOPTS_VALUE ) {       /* progress: by the video, or by the audio when there is none */
            int ps = vs >= 0 ? vs : as;
            int pc = ( int ) ( 100.0 * ( ( pkt->pts - ( ic->streams[ps]->start_time != AV_NOPTS_VALUE ? ic->streams[ps]->start_time : 0 ) ) * av_q2d( ist->time_base ) ) / dur );
            g_dl_percent = pc < 0 ? 0 : ( pc > 99 ? 99 : pc );
        }
        if ( pkt->stream_index == as && bsf ) {
            if ( av_bsf_send_packet( bsf, pkt ) < 0 ) { av_packet_unref( pkt ); continue; }
            while ( av_bsf_receive_packet( bsf, pkt ) >= 0 ) {
                av_packet_rescale_ts( pkt, ist->time_base, ost->time_base );
                pkt->stream_index = ost->index; pkt->pos = -1;
                if ( av_interleaved_write_frame( oc, pkt ) < 0 ) { snprintf( g_dl_err, sizeof g_dl_err, "write failed (disk full?)" ); goto end; }
            }
            continue;
        }
        av_packet_rescale_ts( pkt, ist->time_base, ost->time_base );
        pkt->stream_index = ost->index; pkt->pos = -1;
        if ( av_interleaved_write_frame( oc, pkt ) < 0 ) { snprintf( g_dl_err, sizeof g_dl_err, "write failed (disk full?)" ); goto end; }
    }
    if ( av_write_trailer( oc ) < 0 ) { snprintf( g_dl_err, sizeof g_dl_err, "could not finish the file" ); header = 0; goto end; }
    header = 0;
    g_dl_percent = 100;
    ret = 0;
end:
    if ( g_dl_cancel && ret != 0 ) { ret = -2; snprintf( g_dl_err, sizeof g_dl_err, "cancelled" ); }      /* a cancel during the opening is a cancel too */
    if ( header && oc ) av_write_trailer( oc );                          /* a partial file: still closed properly */
    av_packet_free( &pkt );
    if ( bsf ) av_bsf_free( &bsf );
    free( map );
    if ( oc ) { if ( oc->pb ) avio_closep( &oc->pb ); avformat_free_context( oc ); }
    if ( ic ) avformat_close_input( &ic );
    return ret;
}

static int dl_thread( void * arg ) {
    DlJob * j = ( DlJob * ) arg;
    int r = dl_run( j );
    free( j->src ); free( j->hdr ); free( j->out ); free( j );
    g_dl_result = r;
    SDL_AtomicSet( &g_dl_state, 2 );
    return 0;
}

static int64_t dl_begin( int64_t * params, int audio, const char * conv ) {
    DlJob * j;
    SDL_Thread * th;
    char * src = strdup( ( const char * ) string_get( params[0] ) );
    char * hdr = strdup( ( const char * ) string_get( params[1] ) );
    char * out = strdup( ( const char * ) string_get( params[2] ) );
    static int net_ready = 0;
    string_discard( params[0] ); string_discard( params[1] ); string_discard( params[2] );
    if ( SDL_AtomicGet( &g_dl_state ) == 1 ) { free( src ); free( hdr ); free( out ); return -1; }
    if ( !src || !hdr || !out || !( j = ( DlJob * ) malloc( sizeof( *j ) ) ) ) { free( src ); free( hdr ); free( out ); return -2; }
    if ( !net_ready ) { avformat_network_init(); net_ready = 1; }
    j->src = src; j->hdr = hdr; j->out = out; j->audio = audio; snprintf( j->conv, sizeof j->conv, "%s", conv ? conv : "" );
    g_dl_percent = 0; g_dl_cancel = 0; g_dl_result = 0; g_dl_err[0] = 0; snprintf( g_dl_out, sizeof g_dl_out, "%s", out );
    SDL_AtomicSet( &g_dl_state, 1 );
    th = SDL_CreateThread( dl_thread, "download", j );
    if ( !th ) { SDL_AtomicSet( &g_dl_state, 0 ); free( src ); free( hdr ); free( out ); free( j ); return -3; }
    SDL_DetachThread( th );
    return 0;
}

int64_t libmod_video_download_begin( INSTANCE * my, int64_t * params ) { return dl_begin( params, 0, NULL ); }
/* VIDEO_DOWNLOAD_AUDIO_BEGIN(source, headers, path): only the audio, saved as .m4a/.mp3/.opus/... (VIDEO_DOWNLOAD_PATH tells which) */
int64_t libmod_video_download_audio_begin( INSTANCE * my, int64_t * params ) { return dl_begin( params, 1, NULL ); }
/* VIDEO_DOWNLOAD_CONVERT_BEGIN(source, headers, path, format): the audio re-encoded as "mp3", "opus", "flac", "wav" or "aac" */
int64_t libmod_video_download_convert_begin( INSTANCE * my, int64_t * params ) {
    char fmt[16];
    snprintf( fmt, sizeof fmt, "%s", ( const char * ) string_get( params[3] ) );
    string_discard( params[3] );
    return dl_begin( params, 1, fmt );
}

int64_t libmod_video_download_poll( INSTANCE * my, int64_t * params ) {
    int st = SDL_AtomicGet( &g_dl_state );
    if ( st == 1 ) return -1;
    if ( st == 2 ) { SDL_AtomicSet( &g_dl_state, 0 ); return g_dl_result; }
    return -3;
}
int64_t libmod_video_download_percent( INSTANCE * my, int64_t * params ) { return g_dl_percent; }
int64_t libmod_video_download_cancel( INSTANCE * my, int64_t * params ) { g_dl_cancel = 1; return 0; }
int64_t libmod_video_download_path( INSTANCE * my, int64_t * params ) {
    int64_t str = string_new( g_dl_out );
    string_use( str );
    return str;
}
int64_t libmod_video_download_error( INSTANCE * my, int64_t * params ) {
    int64_t str = string_new( g_dl_err );
    string_use( str );
    return str;
}

/* HTTP_WRITE(path, text): writes a text to a real file (same view of the disk as FFmpeg); 1 = ok */
int64_t libmod_video_http_write( INSTANCE * my, int64_t * params ) {
    const char * path = ( const char * ) string_get( params[0] );
    const char * text = ( const char * ) string_get( params[1] );
    int ok = 0;
    if ( path && path[0] && text ) {
        FILE * f = fopen( path, "wb" );
        if ( f ) { size_t n = strlen( text ); ok = fwrite( text, 1, n, f ) == n; if ( fclose( f ) != 0 ) ok = 0; }
    }
    string_discard( params[0] ); string_discard( params[1] );
    return ok;
}

int64_t libmod_video_http_error( INSTANCE * my, int64_t * params ) {
    int64_t str = string_new( g_http_err );
    string_use( str );
    return str;
}

/* ---- BitTorrent (see torrent.h): the files of a torrent are served on a local address that VIDEO_OPEN can play ---- */
static char g_tor_cache[900] = "";
static int tor_ready( void ) {
    if ( torrent_init( g_tor_cache[0] ? g_tor_cache : "torrent_cache" ) != 0 ) return 0;
    return 1;
}
static int64_t tor_str( const char * t ) { int64_t str = string_new( t ? t : "" ); string_use( str ); return str; }

/* TORRENT_CACHE(folder): where the downloaded data is kept (call before the first TORRENT_ADD) */
int64_t libmod_video_torrent_cache( INSTANCE * my, int64_t * params ) {
    snprintf( g_tor_cache, sizeof g_tor_cache, "%s", string_get( params[0] ) ); string_discard( params[0] ); return 0;
}
int64_t libmod_video_torrent_add( INSTANCE * my, int64_t * params ) {
    int h; if ( !tor_ready() ) { string_discard( params[0] ); return -3; }
    h = torrent_add( string_get( params[0] ) ); string_discard( params[0] ); return h;
}
int64_t libmod_video_torrent_remove( INSTANCE * my, int64_t * params ) { torrent_remove( ( int ) params[0], ( int ) params[1] ); return 0; }
int64_t libmod_video_torrent_state( INSTANCE * my, int64_t * params ) { return torrent_state( ( int ) params[0] ); }
int64_t libmod_video_torrent_error( INSTANCE * my, int64_t * params ) { return tor_str( torrent_error( ( int ) params[0] ) ); }
int64_t libmod_video_torrent_name( INSTANCE * my, int64_t * params ) { return tor_str( torrent_name( ( int ) params[0] ) ); }
int64_t libmod_video_torrent_files( INSTANCE * my, int64_t * params ) { return torrent_file_count( ( int ) params[0] ); }
int64_t libmod_video_torrent_file_name( INSTANCE * my, int64_t * params ) { return tor_str( torrent_file_name( ( int ) params[0], ( int ) params[1] ) ); }
int64_t libmod_video_torrent_file_size( INSTANCE * my, int64_t * params ) { return torrent_file_size( ( int ) params[0], ( int ) params[1] ); }
int64_t libmod_video_torrent_main( INSTANCE * my, int64_t * params ) { return torrent_main_file( ( int ) params[0] ); }
int64_t libmod_video_torrent_select( INSTANCE * my, int64_t * params ) { return torrent_select( ( int ) params[0], ( int ) params[1] ); }
int64_t libmod_video_torrent_url( INSTANCE * my, int64_t * params ) {
    char u[1200]; if ( torrent_url( ( int ) params[0], ( int ) params[1], u, sizeof u ) != 0 ) u[0] = 0; return tor_str( u );
}
int64_t libmod_video_torrent_peers( INSTANCE * my, int64_t * params ) { return torrent_peers( ( int ) params[0] ); }
int64_t libmod_video_torrent_rate( INSTANCE * my, int64_t * params ) { return torrent_down_rate( ( int ) params[0] ); }
int64_t libmod_video_torrent_percent( INSTANCE * my, int64_t * params ) { return torrent_file_percent( ( int ) params[0] ); }
int64_t libmod_video_torrent_buffered( INSTANCE * my, int64_t * params ) { return torrent_buffered( ( int ) params[0] ); }
int64_t libmod_video_torrent_ready( INSTANCE * my, int64_t * params ) { return torrent_start_percent( ( int ) params[0] ); }
int64_t libmod_video_torrent_downloaded( INSTANCE * my, int64_t * params ) { return torrent_downloaded( ( int ) params[0] ); }
int64_t libmod_video_torrent_options( INSTANCE * my, int64_t * params ) { torrent_set_options( ( int ) params[0], ( int ) params[1] ); return 0; }
int64_t libmod_video_torrent_ahead( INSTANCE * my, int64_t * params ) { torrent_set_ahead( ( int ) params[0] ); return 0; }
int64_t libmod_video_torrent_upload_limit( INSTANCE * my, int64_t * params ) { torrent_set_upload_limit( ( int ) params[0] ); return 0; }

/* ---- receive files from another computer through a web page (see upload.h) ---- */
int64_t libmod_video_upload_start( INSTANCE * my, int64_t * params ) {
    int r = upload_start( string_get( params[0] ), ( int ) params[1], string_get( params[2] ) );
    string_discard( params[0] ); string_discard( params[2] ); return r;
}
int64_t libmod_video_upload_stop( INSTANCE * my, int64_t * params ) { upload_stop(); return 0; }
int64_t libmod_video_upload_files( INSTANCE * my, int64_t * params ) { return upload_file_count(); }
int64_t libmod_video_upload_bytes( INSTANCE * my, int64_t * params ) { return upload_total_bytes(); }
int64_t libmod_video_upload_percent( INSTANCE * my, int64_t * params ) { return upload_percent(); }
int64_t libmod_video_upload_current( INSTANCE * my, int64_t * params ) { return tor_str( upload_current() ); }
int64_t libmod_video_upload_url( INSTANCE * my, int64_t * params ) { return tor_str( upload_take_url() ); }
int64_t libmod_video_upload_ip( INSTANCE * my, int64_t * params ) { return tor_str( upload_local_ip() ); }
int64_t libmod_video_upload_error( INSTANCE * my, int64_t * params ) { return tor_str( upload_last_error() ); }

#include "libmod_video_exports.h"
