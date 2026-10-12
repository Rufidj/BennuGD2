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
#include <libavutil/imgutils.h>
#include <libavutil/dict.h>
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

#define MAX_SUBS     8                 /* embedded text subtitle tracks kept   */

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
    int             ypw, cpw, cw, ch;          /* packed Y / chroma widths (texels), chroma w,h */
    size_t          slot_bytes;
    GPU_Image       * img_y, * img_u, * img_v;
    GPU_Target      * tgt;
    float           gp[4];                     /* shader params: width, full range, bt709, -    */

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

    /* subtitles (text only: SRT / ASS / SSA / WebVTT / mov_text), guarded by mx */
    int             nsub;                      /* embedded text tracks          */
    int             sub_stream[MAX_SUBS];      /* stream index of each track    */
    AVCodecContext  * sub_ctx[MAX_SUBS];
    int             sub_sel;                   /* selected track, -1 = off      */
    int             sub_ext;                   /* 1 once an external file loaded*/
    CUE             * cues;
    int             ncues, capcues;

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
static Uint32 g_yuv_prog = 0;                  /* shared program, 0 = not built / failed        */
static int g_yuv_tried = 0;
static GPU_ShaderBlock g_yuv_block;
static int g_loc_tex, g_loc_u, g_loc_v, g_loc_p, g_loc_w;

#define YUV_FRAG_BODY \
    "uniform sampler2D tex;\n" \
    "uniform sampler2D tex_u;\n" \
    "uniform sampler2D tex_v;\n" \
    "uniform vec4 prm;\n"      /* x = width, y = full range, z = bt709 */ \
    "uniform vec4 wid;\n"      /* x = Y packed width, y = chroma packed width */ \
    "float pick(vec4 t, float c) { return c < 0.5 ? t.r : (c < 1.5 ? t.g : (c < 2.5 ? t.b : t.a)); }\n" \
    "vec3 yuv(vec2 uv) {\n" \
    "    float px = floor(uv.x * prm.x);\n" \
    "    float tx = floor(px / 4.0);\n" \
    "    float y = pick(TEX(tex, vec2((tx + 0.5) / wid.x, uv.y)), px - tx * 4.0);\n" \
    "    float cx = floor(px / 2.0);\n" \
    "    float ctx = floor(cx / 4.0);\n" \
    "    vec2 cuv = vec2((ctx + 0.5) / wid.y, uv.y);\n" \
    "    float cc = cx - ctx * 4.0;\n" \
    "    float u = pick(TEX(tex_u, cuv), cc) - 0.5019608;\n" \
    "    float v = pick(TEX(tex_v, cuv), cc) - 0.5019608;\n" \
    "    if (prm.y < 0.5) y = (y - 0.0627451) * 1.1643836;\n" \
    "    float cs = prm.y < 0.5 ? 1.1383929 : 1.0;\n" \
    "    u *= cs; v *= cs;\n" \
    "    vec3 c;\n" \
    "    if (prm.z > 0.5) c = vec3(y + 1.5748 * v, y - 0.1873 * u - 0.4681 * v, y + 1.8556 * u);\n" \
    "    else             c = vec3(y + 1.402 * v, y - 0.344136 * u - 0.714136 * v, y + 1.772 * u);\n" \
    "    return clamp(c, 0.0, 1.0);\n" \
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
    GPU_UpdateImageBytes( v->img_v, NULL, base + ( size_t ) v->ypw * 4 * v->h + ( size_t ) v->cpw * 4 * v->ch, v->cpw * 4 );

    float wid[4] = { ( float ) v->ypw, ( float ) v->cpw, 0, 0 };
    GPU_ActivateShaderProgram( g_yuv_prog, &g_yuv_block );
    GPU_SetShaderImage( v->img_u, g_loc_u, 1 );
    GPU_SetShaderImage( v->img_v, g_loc_v, 2 );
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

static int push_video_frame( VIDEO * v, AVFrame * f ) {
    double pts = ( f->best_effort_timestamp == AV_NOPTS_VALUE ) ? 0.0 : f->best_effort_timestamp * v->vtb;

    SDL_LockMutex( v->mx );
    while ( !v->quit && !v->seek_req && v->vq_n >= VQ_SIZE ) SDL_CondWaitTimeout( v->cond, v->mx, 10 );
    if ( v->quit || v->seek_req ) { SDL_UnlockMutex( v->mx ); return 0; }
    int slot = ( v->vq_r + v->vq_n ) % VQ_SIZE;
    SDL_UnlockMutex( v->mx );

    if ( v->gpu ) {
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
        sws_scale( v->sws, ( const uint8_t * const * ) f->data, f->linesize, 0, f->height, dst, dst_ls );
    }
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
    if ( !v->graph || !v->graph->tex ) return;
    if ( v->gpu ) yuv_render( v, v->vq[slot].rgba );
    else GPU_UpdateImageBytes( v->graph->tex, NULL, v->vq[slot].rgba, v->w * 4 );
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
    for ( int i = 0; i < MAX_SUBS; i++ ) if ( v->sub_ctx[i] ) avcodec_free_context( &v->sub_ctx[i] );
    for ( int i = 0; i < v->ncues; i++ ) free( v->cues[i].text );
    free( v->cues );
    if ( v->fmt ) avformat_close_input( &v->fmt );
    if ( v->graph_code ) zombie_add( v->graph_code );
    zimg_add( v->img_y, v->tgt ); zimg_add( v->img_u, NULL ); zimg_add( v->img_v, NULL );
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

    /* GPU conversion needs 8-bit 4:2:0 and row pitches that are a multiple of 4 */
    if ( g_use_gpu && ( v->vctx->pix_fmt == AV_PIX_FMT_YUV420P || v->vctx->pix_fmt == AV_PIX_FMT_YUVJ420P ) ) {
        if ( !g_yuv_tried ) yuv_program_build();
        if ( g_yuv_prog ) v->gpu = 1;
    }
    if ( v->gpu ) {
        v->ypw = ( v->w + 3 ) / 4; v->cw = ( v->w + 1 ) / 2; v->ch = ( v->h + 1 ) / 2; v->cpw = ( v->cw + 3 ) / 4;
        v->slot_bytes = ( size_t ) v->ypw * 4 * v->h + 2 * ( size_t ) v->cpw * 4 * v->ch;
        v->gp[0] = ( float ) v->w;
        v->gp[1] = ( v->vctx->pix_fmt == AV_PIX_FMT_YUVJ420P || v->vctx->color_range == AVCOL_RANGE_JPEG ) ? 1.0f : 0.0f;
        v->gp[2] = ( v->vctx->colorspace == AVCOL_SPC_BT709 || ( v->vctx->colorspace == AVCOL_SPC_UNSPECIFIED && v->h >= 720 ) ) ? 1.0f : 0.0f;
    } else {
        v->sws = sws_getContext( v->w, v->h, v->vctx->pix_fmt, v->w, v->h, AV_PIX_FMT_RGBA, SWS_BILINEAR, NULL, NULL, NULL );
        if ( !v->sws ) goto fail;
        v->slot_bytes = ( size_t ) v->w * v->h * 4;
    }
    for ( int i = 0; i < VQ_SIZE; i++ ) { v->vq[i].rgba = ( uint8_t * ) calloc( 1, v->slot_bytes ); if ( !v->vq[i].rgba ) goto fail; }
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
    if ( v->gpu ) {
        v->img_y = GPU_CreateImage( v->ypw, v->h,  GPU_FORMAT_RGBA );
        v->img_u = GPU_CreateImage( v->cpw, v->ch, GPU_FORMAT_RGBA );
        v->img_v = GPU_CreateImage( v->cpw, v->ch, GPU_FORMAT_RGBA );
        if ( v->img_y && v->img_u && v->img_v && v->graph->tex ) {
            GPU_SetImageFilter( v->img_y, GPU_FILTER_NEAREST );
            GPU_SetImageFilter( v->img_u, GPU_FILTER_NEAREST );
            GPU_SetImageFilter( v->img_v, GPU_FILTER_NEAREST );
            GPU_SetBlending( v->img_y, 0 );
            v->tgt = GPU_LoadTarget( v->graph->tex );
        }
        if ( !v->tgt ) {                              /* GPU path unavailable: back to the CPU converter */
            fprintf( stderr, "libmod_video: GPU conversion unavailable, using software\n" );
            zimg_add( v->img_y, NULL ); zimg_add( v->img_u, NULL ); zimg_add( v->img_v, NULL );
            v->img_y = v->img_u = v->img_v = NULL;
            v->gpu = 0;
            v->sws = sws_getContext( v->w, v->h, v->vctx->pix_fmt, v->w, v->h, AV_PIX_FMT_RGBA, SWS_BILINEAR, NULL, NULL, NULL );
            if ( !v->sws ) goto fail;
            for ( int i = 0; i < VQ_SIZE; i++ ) {
                free( v->vq[i].rgba );
                v->vq[i].rgba = ( uint8_t * ) calloc( 1, ( size_t ) v->w * v->h * 4 );
                if ( !v->vq[i].rgba ) goto fail;
            }
        }
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

/* VIDEO_USE_GPU(enable): colour conversion on the GPU for videos opened afterwards (default 1) */
int64_t libmod_video_use_gpu( INSTANCE * my, int64_t * params ) { g_use_gpu = params[0] ? 1 : 0; return g_use_gpu; }
/* VIDEO_GPU(handle): 1 if that video converts on the GPU */
int64_t libmod_video_gpu( INSTANCE * my, int64_t * params ) { VIDEO * v = vid_get( params[0] ); return v ? v->gpu : 0; }

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
            double now = vid_clock( v );
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
    zimg_tick( 1 );
    if ( g_yuv_prog ) { GPU_FreeShaderProgram( g_yuv_prog ); g_yuv_prog = 0; }
    if ( g_obj_id ) { gr_destroy_object( g_obj_id ); g_obj_id = 0; }
}

#include "libmod_video_exports.h"
