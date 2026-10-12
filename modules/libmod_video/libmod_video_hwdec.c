/* See libmod_video_hwdec.h. */
#include "libmod_video_hwdec.h"

#ifndef __PROSPERO__

int      hwdec_available( void ) { return 0; }
HWDEC *  hwdec_open( int hevc, int w, int h ) { ( void ) hevc; ( void ) w; ( void ) h; return NULL; }
void     hwdec_close( HWDEC * d ) { ( void ) d; }
int      hwdec_decode( HWDEC * d, const uint8_t * au, size_t size, int64_t pts, hwdec_cb cb, void * ud ) { ( void ) d; ( void ) au; ( void ) size; ( void ) pts; ( void ) cb; ( void ) ud; return -1; }
void     hwdec_flush( HWDEC * d, hwdec_cb cb, void * ud ) { ( void ) d; ( void ) cb; ( void ) ud; }
void     hwdec_reset( HWDEC * d ) { ( void ) d; }

#else /* __PROSPERO__ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <libavutil/sha.h>
#include <libavutil/mem.h>

/* ---- the console's API ---------------------------------------------------- */

enum { CODEC_AVC = 1, CODEC_HEVC = 974921 };

typedef struct {
    uint64_t this_size;
    uint32_t resource_type;       /* 1: decode on a compute queue */
    uint32_t codec_type;
    uint32_t profile;
    uint32_t max_level;
    int32_t  max_frame_width;
    int32_t  max_frame_height;
    int32_t  max_dpb_frame_count;
    uint32_t decode_pipeline_depth;
    void   * compute_queue;
    uint64_t cpu_affinity_mask;
    int32_t  cpu_thread_priority;
    bool     optimize_progressive_video;
    bool     check_memory_type;
    uint8_t  reserved0, reserved1;
    void   * extra_config_info;
} VdecConfig;

typedef struct {
    uint64_t this_size;
    uint64_t cpu_memory_size;      void * cpu_memory;
    uint64_t gpu_memory_size;      void * gpu_memory;
    uint64_t cpu_gpu_memory_size;  void * cpu_gpu_memory;
    uint64_t max_frame_buffer_size;
    uint32_t frame_buffer_alignment;
    uint32_t reserved0;
} VdecMemory;

typedef struct {
    uint64_t this_size;
    void   * au_data;
    uint64_t au_size;
    uint64_t pts_data, dts_data, attached_data;
} VdecInput;

typedef struct {
    uint64_t this_size;
    bool     is_valid, is_error_frame;
    uint8_t  picture_count;
    uint32_t codec_type;
    uint32_t frame_width, frame_pitch, frame_height;
    void   * frame_buffer;
    uint64_t frame_buffer_size;
    uint32_t frame_format;
    uint32_t frame_pitch_in_bytes;
} VdecOutput;

typedef struct {
    uint64_t this_size;
    void   * frame_buffer;
    uint64_t frame_buffer_size;
    bool     is_accepted;
} VdecFrameBuffer;

typedef struct { uint64_t this_size; uint64_t cpu_gpu_memory_size; void * cpu_gpu_memory; } VdecComputeMemory;
typedef struct { uint64_t this_size; uint16_t compute_pipe_id, compute_queue_id; bool check_memory_type; uint8_t reserved0; uint16_t reserved1; } VdecComputeConfig;

_Static_assert( sizeof( VdecConfig ) == 0x48, "VdecConfig" );
_Static_assert( sizeof( VdecMemory ) == 0x48, "VdecMemory" );
_Static_assert( sizeof( VdecInput ) == 0x30, "VdecInput" );
_Static_assert( sizeof( VdecOutput ) == 0x38, "VdecOutput" );
_Static_assert( sizeof( VdecFrameBuffer ) == 0x20, "VdecFrameBuffer" );

/* libSceVideodec2 is NOT linked and no sysmodule library is imported: the title must start without them.
   The module is loaded on request with sceKernelLoadStartModule and its functions are looked up in it. */
static int ( * sceVideodec2QueryComputeMemoryInfo )( VdecComputeMemory * info );
static int ( * sceVideodec2AllocateComputeQueue )( const VdecComputeConfig * config, const VdecComputeMemory * memory, void ** queue );
static int ( * sceVideodec2QueryDecoderMemoryInfo )( const VdecConfig * config, VdecMemory * memory );
static int ( * sceVideodec2CreateDecoder )( const VdecConfig * config, const VdecMemory * memory, void ** decoder );
static int ( * sceVideodec2DeleteDecoder )( void * decoder );
static int ( * sceVideodec2Decode )( void * decoder, const VdecInput * input, VdecFrameBuffer * frame, VdecOutput * output );
static int ( * sceVideodec2Flush )( void * decoder, VdecFrameBuffer * frame, VdecOutput * output );
static int ( * sceVideodec2Reset )( void * decoder );

int sceKernelGetModuleList( int * handles, size_t max, size_t * num );
int sceKernelGetModuleInfo( int handle, void * info );
int sceKernelDlsym( int handle, const char * name, void ** addr );
const char * sceKernelGetFsSandboxRandomWord( void );
int sceKernelLoadStartModule( const char * path, size_t args, const void * argp, unsigned flags, const void * opt, int * res );

int     sceKernelAllocateDirectMemory( int64_t search_start, int64_t search_end, size_t len, size_t alignment, int type, int64_t * start );
int     sceKernelMapDirectMemory( void ** addr, size_t len, int prot, int flags, int64_t start, size_t alignment );
int     sceKernelReleaseDirectMemory( int64_t start, size_t len );
int64_t sceKernelGetDirectMemorySize( void );
int     sceKernelMapFlexibleMemory( void ** addr, size_t len, int prot, int flags );
int     sceKernelReleaseFlexibleMemory( void * addr, size_t len );
int     sceKernelMunmap( void * addr, size_t len );

#define PROT_CPU_RW   0x03
#define PROT_GPU_RW   0x30
#define DIRECT_TYPE   12            /* the memory type the decoder is known to accept */
#define PAGE_16K      ( ( size_t ) 0x4000 )

#define AU_SLOTS      8             /* access units in turn (the decoder may still read one after Decode) */
#define AU_SLOT_SIZE  ( ( size_t ) 4 << 20 )
#define MAX_FRAMES    16
#define MAX_PENDING   64

#define LOG( ... ) do { fprintf( stderr, "libmod_video hw: " __VA_ARGS__ ); fflush( stderr ); } while ( 0 )

/* ---- memory --------------------------------------------------------------- */

typedef struct { void * addr; size_t len; int64_t start; } MEM;      /* start = -1: flexible */

static size_t round16k( size_t v ) { return ( v + PAGE_16K - 1 ) & ~( PAGE_16K - 1 ); }

static int mem_direct( MEM * m, size_t len, size_t align, int prot ) {
    m->len = round16k( len ); m->start = -1; m->addr = NULL;
    if ( align < PAGE_16K ) align = PAGE_16K;
    if ( sceKernelAllocateDirectMemory( 0, sceKernelGetDirectMemorySize(), m->len, align, DIRECT_TYPE, &m->start ) != 0 ) { m->start = -1; return 0; }
    if ( sceKernelMapDirectMemory( &m->addr, m->len, prot, 0, m->start, align ) != 0 ) {
        sceKernelReleaseDirectMemory( m->start, m->len ); m->start = -1; m->addr = NULL; return 0;
    }
    return 1;
}
static int mem_flexible( MEM * m, size_t len ) {
    m->len = round16k( len ); m->start = -1; m->addr = NULL;
    return sceKernelMapFlexibleMemory( &m->addr, m->len, PROT_CPU_RW, 0 ) == 0;
}
static void mem_free( MEM * m ) {
    if ( !m->addr ) return;
    if ( m->start >= 0 ) { sceKernelMunmap( m->addr, m->len ); sceKernelReleaseDirectMemory( m->start, m->len ); }
    else sceKernelReleaseFlexibleMemory( m->addr, m->len );
    m->addr = NULL; m->start = -1;
}

/* ---- library + shared compute queue ---------------------------------------- */

static int   g_state = 0;          /* 0 untried, 1 usable, -1 not available */
static void * g_queue = NULL;
static MEM   g_queue_mem = { NULL, 0, -1 };

/* NID of a function name: first 8 bytes of SHA-1(name + salt), read little-endian, base64 big-endian ('/' -> '-') */
static void nid_of( const char * name, char out[16] ) {
    static const uint8_t salt[16] = { 0x51, 0x8D, 0x64, 0xA6, 0x35, 0xDE, 0xD8, 0xC1, 0xE6, 0xB0, 0x39, 0xB1, 0xC3, 0xE5, 0x52, 0x30 };
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-";
    uint8_t d[20]; uint64_t v = 0;
    struct AVSHA * c = av_sha_alloc();
    av_sha_init( c, 160 );
    av_sha_update( c, ( const uint8_t * ) name, strlen( name ) );
    av_sha_update( c, salt, sizeof( salt ) );
    av_sha_final( c, d );
    av_free( c );
    for ( int i = 7; i >= 0; i-- ) v = ( v << 8 ) | d[i];            /* little-endian read */
    uint8_t be[8];
    for ( int i = 0; i < 8; i++ ) be[i] = ( uint8_t ) ( v >> ( 56 - 8 * i ) );   /* written big-endian */
    int o = 0;
    for ( int i = 0; i < 8; i += 3 ) {
        uint32_t n = ( uint32_t ) be[i] << 16 | ( i + 1 < 8 ? ( uint32_t ) be[i+1] << 8 : 0 ) | ( i + 2 < 8 ? be[i+2] : 0 );
        out[o++] = b64[( n >> 18 ) & 63]; out[o++] = b64[( n >> 12 ) & 63];
        if ( i + 1 < 8 ) out[o++] = b64[( n >> 6 ) & 63];
        if ( i + 2 < 8 ) out[o++] = b64[n & 63];
    }
    out[o] = 0;
}

/* handle of the loaded libSceVideodec2 module, or -1 */
static int find_module( void ) {
    int h[256]; size_t n = 0;
    if ( sceKernelGetModuleList( h, 256, &n ) != 0 ) { LOG( "module list unavailable\n" ); return -1; }
    LOG( "%zu modules loaded\n", n );
    for ( size_t i = 0; i < n && i < 256; i++ ) {
        uint8_t info[0x160];                       /* SceKernelModuleInfo: size, char name[256], ... */
        memset( info, 0, sizeof( info ) );
        *( uint64_t * ) info = sizeof( info );
        if ( sceKernelGetModuleInfo( h[i], info ) != 0 ) continue;
        const char * name = ( const char * ) info + 8;
        if ( strstr( name, "Videodec2" ) ) { LOG( "module %s (handle %d)\n", name, h[i] ); return h[i]; }
    }
    return -1;
}

static int resolve_functions( void ) {
    int h = find_module();
    if ( h < 0 ) {
        /* titles are sandboxed: system paths are "/<random word>/common/lib/..." */
        char sandboxed[160] = "", sandboxed_priv[160] = "";
        const char * word = sceKernelGetFsSandboxRandomWord();
        if ( word ) {
            snprintf( sandboxed, sizeof( sandboxed ), "/%s/common/lib/libSceVideodec2.sprx", word );
            snprintf( sandboxed_priv, sizeof( sandboxed_priv ), "/%s/priv/lib/libSceVideodec2.sprx", word );
        }
        const char * paths[] = { sandboxed, sandboxed_priv, "/system/common/lib/libSceVideodec2.sprx", "libSceVideodec2.sprx" };
        for ( unsigned i = 0; i < sizeof( paths ) / sizeof( paths[0] ) && h < 0; i++ ) {
            if ( !paths[i][0] ) continue;
            int res = 0;
            int r = sceKernelLoadStartModule( paths[i], 0, NULL, 0, NULL, &res );
            LOG( "LoadStartModule(%s) -> %#x\n", paths[i], ( unsigned ) r );
            if ( r > 0 ) h = r;
        }
        if ( h < 0 ) return 0;
    }
    struct { const char * name; void ** slot; } f[] = {
        { "sceVideodec2QueryComputeMemoryInfo", ( void ** ) &sceVideodec2QueryComputeMemoryInfo },
        { "sceVideodec2AllocateComputeQueue",   ( void ** ) &sceVideodec2AllocateComputeQueue },
        { "sceVideodec2QueryDecoderMemoryInfo", ( void ** ) &sceVideodec2QueryDecoderMemoryInfo },
        { "sceVideodec2CreateDecoder",          ( void ** ) &sceVideodec2CreateDecoder },
        { "sceVideodec2DeleteDecoder",          ( void ** ) &sceVideodec2DeleteDecoder },
        { "sceVideodec2Decode",                 ( void ** ) &sceVideodec2Decode },
        { "sceVideodec2Flush",                  ( void ** ) &sceVideodec2Flush },
        { "sceVideodec2Reset",                  ( void ** ) &sceVideodec2Reset },
    };
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-";
    int suffix_a = -1, suffix_b = -1;            /* library / module id characters that worked, -1 = none */
    int have_suffix = 0;
    for ( unsigned i = 0; i < sizeof( f ) / sizeof( f[0] ); i++ ) {
        void * p = NULL;
        char nid[16], key[64];
        nid_of( f[i].name, nid );
        int rc = sceKernelDlsym( h, f[i].name, &p );                   /* plain name */
        if ( rc != 0 || !p ) { p = NULL; rc = sceKernelDlsym( h, nid, &p ); }          /* NID alone */
        if ( ( rc != 0 || !p ) && have_suffix ) {
            snprintf( key, sizeof( key ), "%s#%c#%c", nid, suffix_a < 0 ? 'A' : b64[suffix_a], suffix_b < 0 ? 'A' : b64[suffix_b] );
            p = NULL; rc = sceKernelDlsym( h, key, &p );
        }
        if ( ( rc != 0 || !p ) && !have_suffix ) {                      /* NID#lib#mod: search the ids once */
            for ( int la = 0; la < 64 && !have_suffix; la++ )
                for ( int lb = 0; lb < 64 && !have_suffix; lb++ ) {
                    snprintf( key, sizeof( key ), "%s#%c#%c", nid, b64[la], b64[lb] );
                    p = NULL;
                    if ( sceKernelDlsym( h, key, &p ) == 0 && p ) { suffix_a = la; suffix_b = lb; have_suffix = 1; rc = 0; LOG( "symbol form %s#%c#%c\n", "NID", b64[la], b64[lb] ); }
                }
        }
        if ( rc != 0 || !p ) { LOG( "%s (%s) not found (%#x)\n", f[i].name, nid, ( unsigned ) rc ); return 0; }
        *f[i].slot = p;
        LOG( "%s -> %p\n", f[i].name, p );
    }
    return 1;
}

int hwdec_available( void ) {
    if ( g_state ) return g_state > 0;
    g_state = -1;
    if ( !resolve_functions() ) { LOG( "libSceVideodec2 not available\n" ); return 0; }
    int rc;

    VdecComputeMemory info = { .this_size = sizeof( info ) };
    rc = sceVideodec2QueryComputeMemoryInfo( &info );
    if ( rc == 0 && mem_direct( &g_queue_mem, info.cpu_gpu_memory_size, PAGE_16K, PROT_CPU_RW | PROT_GPU_RW ) ) {
        info.cpu_gpu_memory = g_queue_mem.addr;
        info.cpu_gpu_memory_size = g_queue_mem.len;
        VdecComputeConfig cfg = { .this_size = sizeof( cfg ) };
        rc = sceVideodec2AllocateComputeQueue( &cfg, &info, &g_queue );
        if ( rc != 0 ) { g_queue = NULL; mem_free( &g_queue_mem ); }
    }
    LOG( "compute queue %s (%#x)\n", g_queue ? "ready" : "FAILED", ( unsigned ) rc );
    if ( g_queue ) g_state = 1;
    return g_state > 0;
}

/* ---- decoder -------------------------------------------------------------- */

struct HWDEC {
    void       * dec;
    MEM          cpu, gpu, cpu_gpu, frames, au[AU_SLOTS];
    size_t       frame_size;
    unsigned     frame_count;
    bool         held[MAX_FRAMES];     /* with the decoder, not given back yet */
    unsigned     au_next;
    int64_t      pending[MAX_PENDING]; /* pts of access units inside, output is in display order */
    unsigned     npending;
    unsigned     decoded, errors, shown;
};

static void pending_add( HWDEC * d, int64_t pts ) {
    if ( d->npending < MAX_PENDING ) d->pending[d->npending++] = pts;
}
static int64_t pending_take( HWDEC * d ) {            /* earliest still waiting */
    if ( !d->npending ) return 0;
    unsigned best = 0;
    for ( unsigned i = 1; i < d->npending; i++ ) if ( d->pending[i] < d->pending[best] ) best = i;
    int64_t v = d->pending[best];
    d->pending[best] = d->pending[--d->npending];
    return v;
}

static void frames_release( HWDEC * d, const void * buf ) {
    if ( !buf || !d->frames.addr ) return;
    size_t off = ( size_t ) ( ( const uint8_t * ) buf - ( const uint8_t * ) d->frames.addr );
    unsigned i = ( unsigned ) ( off / d->frame_size );
    if ( i < d->frame_count ) d->held[i] = false;
}

static int next_frame( HWDEC * d, VdecFrameBuffer * fb ) {
    for ( unsigned i = 0; i < d->frame_count; i++ ) if ( !d->held[i] ) {
        memset( fb, 0, sizeof( *fb ) );
        fb->this_size = sizeof( *fb );
        fb->frame_buffer = ( uint8_t * ) d->frames.addr + i * d->frame_size;
        fb->frame_buffer_size = d->frame_size;
        return ( int ) i;
    }
    return -1;
}

static void deliver( HWDEC * d, const VdecOutput * out, hwdec_cb cb, void * ud ) {
    int64_t pts = pending_take( d );
    if ( out->is_error_frame || !out->frame_buffer ) { d->errors++; frames_release( d, out->frame_buffer ); return; }
    HWPIC p;
    p.pitch = ( int ) out->frame_pitch_in_bytes;
    p.w = ( int ) out->frame_width; p.h = ( int ) out->frame_height;
    p.ten = out->frame_pitch_in_bytes >= 2 * out->frame_pitch;
    p.y = ( const uint8_t * ) out->frame_buffer;
    p.uv = p.y + ( size_t ) p.pitch * out->frame_height;     /* chroma follows the luma rows */
    p.pts = pts;
    if ( cb ) cb( ud, &p );
    frames_release( d, out->frame_buffer );
    d->shown++;
}

static void release_all( HWDEC * d ) {
    for ( int i = 0; i < AU_SLOTS; i++ ) mem_free( &d->au[i] );
    mem_free( &d->frames ); mem_free( &d->cpu_gpu ); mem_free( &d->gpu ); mem_free( &d->cpu );
}

static int bring_up( HWDEC * d, const VdecConfig * cfg, VdecMemory * mem ) {
    memset( mem, 0, sizeof( *mem ) );
    mem->this_size = sizeof( *mem );
    int rc = sceVideodec2QueryDecoderMemoryInfo( cfg, mem );
    if ( rc != 0 ) { LOG( "memory query %#x\n", ( unsigned ) rc ); return rc; }
    size_t align = mem->frame_buffer_alignment > PAGE_16K ? mem->frame_buffer_alignment : PAGE_16K;
    d->frame_size = ( mem->max_frame_buffer_size + align - 1 ) & ~( align - 1 );

    int ok = ( !mem->cpu_memory_size || mem_flexible( &d->cpu, mem->cpu_memory_size ) )
          && ( !mem->gpu_memory_size || mem_direct( &d->gpu, mem->gpu_memory_size, PAGE_16K, PROT_CPU_RW | PROT_GPU_RW ) )
          && ( !mem->cpu_gpu_memory_size || mem_direct( &d->cpu_gpu, mem->cpu_gpu_memory_size, PAGE_16K, PROT_CPU_RW | PROT_GPU_RW ) )
          && mem_direct( &d->frames, d->frame_size * d->frame_count, align, PROT_CPU_RW | PROT_GPU_RW );
    for ( int i = 0; ok && i < AU_SLOTS; i++ ) ok = mem_direct( &d->au[i], AU_SLOT_SIZE, PAGE_16K, PROT_CPU_RW | PROT_GPU_RW );
    if ( ok ) {
        mem->cpu_memory = d->cpu.addr; mem->gpu_memory = d->gpu.addr; mem->cpu_gpu_memory = d->cpu_gpu.addr;
        rc = sceVideodec2CreateDecoder( cfg, mem, &d->dec );
        if ( rc == 0 ) return 0;
        LOG( "create decoder %#x\n", ( unsigned ) rc );
        d->dec = NULL;
    } else {
        LOG( "no memory (cpu %llu gpu %llu cpu+gpu %llu KiB, %u frames x %zu KiB)\n",
             ( unsigned long long ) ( mem->cpu_memory_size >> 10 ), ( unsigned long long ) ( mem->gpu_memory_size >> 10 ),
             ( unsigned long long ) ( mem->cpu_gpu_memory_size >> 10 ), d->frame_count, d->frame_size >> 10 );
        rc = -1;
    }
    release_all( d );
    return rc;
}

HWDEC * hwdec_open( int hevc, int w, int h ) {
    if ( !hwdec_available() ) return NULL;
    if ( w > 3840 || h > 2176 ) { LOG( "%dx%d is bigger than 4K\n", w, h ); return NULL; }

    HWDEC * d = ( HWDEC * ) calloc( 1, sizeof( *d ) );
    if ( !d ) return NULL;
    for ( int i = 0; i < AU_SLOTS; i++ ) d->au[i].start = -1;
    d->cpu.start = d->gpu.start = d->cpu_gpu.start = d->frames.start = -1;

    VdecConfig cfg;
    memset( &cfg, 0, sizeof( cfg ) );
    cfg.this_size = sizeof( cfg );
    cfg.resource_type = 1;
    cfg.codec_type = hevc ? CODEC_HEVC : CODEC_AVC;
    cfg.profile = hevc ? 2 : 100;
    cfg.max_level = hevc ? 156 : 52;
    cfg.max_frame_width = ( int32_t ) ( ( ( unsigned ) w + 15 ) & ~15u );
    cfg.max_frame_height = ( int32_t ) ( ( ( unsigned ) h + 15 ) & ~15u );
    cfg.max_dpb_frame_count = -1;
    cfg.decode_pipeline_depth = 4;
    cfg.compute_queue = g_queue;
    cfg.cpu_affinity_mask = 0x3f;
    cfg.cpu_thread_priority = 700;
    cfg.optimize_progressive_video = true;

    d->frame_count = ( w <= 1920 && h <= 1088 ) ? MAX_FRAMES : 10;
    VdecMemory mem;
    int rc = bring_up( d, &cfg, &mem );
    if ( rc != 0 && cfg.decode_pipeline_depth > 1 ) {          /* depth 4 costs memory: one at a time */
        LOG( "pipeline depth 4 failed (%#x), trying 1\n", ( unsigned ) rc );
        cfg.decode_pipeline_depth = 1;
        rc = bring_up( d, &cfg, &mem );
    }
    if ( rc != 0 && hevc && cfg.max_level == 156 ) {           /* level 5.2 refused: 5.1 */
        LOG( "level 5.2 failed (%#x), trying 5.1\n", ( unsigned ) rc );
        cfg.max_level = 153;
        rc = bring_up( d, &cfg, &mem );
    }
    if ( rc != 0 ) { free( d ); return NULL; }
    LOG( "%s %dx%d ready (level %u, pipeline %u, frames %u x %zu KiB)\n", hevc ? "HEVC" : "H.264", w, h,
         cfg.max_level, cfg.decode_pipeline_depth, d->frame_count, d->frame_size >> 10 );
    return d;
}

void hwdec_close( HWDEC * d ) {
    if ( !d ) return;
    if ( d->dec ) sceVideodec2DeleteDecoder( d->dec );
    release_all( d );
    free( d );
}

int hwdec_decode( HWDEC * d, const uint8_t * au, size_t size, int64_t pts, hwdec_cb cb, void * ud ) {
    MEM * slot = &d->au[d->au_next];
    if ( size > slot->len ) {                                  /* a big keyframe: grow this slot */
        mem_free( slot );
        if ( !mem_direct( slot, size + ( ( size_t ) 1 << 20 ), PAGE_16K, PROT_CPU_RW | PROT_GPU_RW ) ) { LOG( "no memory for a %zu byte access unit\n", size ); return -2; }
    }
    memcpy( slot->addr, au, size );

    VdecFrameBuffer fb;
    int fi = next_frame( d, &fb );
    if ( fi < 0 ) { LOG( "no free picture buffer\n" ); return -3; }

    VdecInput in;
    memset( &in, 0, sizeof( in ) );
    in.this_size = sizeof( in ); in.au_data = slot->addr; in.au_size = size;
    in.pts_data = ( uint64_t ) pts; in.dts_data = UINT64_MAX;
    VdecOutput out;
    memset( &out, 0, sizeof( out ) );
    out.this_size = sizeof( out );

    int rc = sceVideodec2Decode( d->dec, &in, &fb, &out );
    d->au_next = ( d->au_next + 1 ) % AU_SLOTS;
    if ( fb.is_accepted ) d->held[fi] = true;
    if ( rc != 0 ) {
        d->errors++;
        if ( d->errors <= 5 ) LOG( "decode %#x (access unit %u, %zu bytes)\n", ( unsigned ) rc, d->decoded, size );
        return rc < 0 ? rc : -1;
    }
    d->decoded++;
    pending_add( d, pts );
    if ( out.is_valid ) deliver( d, &out, cb, ud );
    return 0;
}

void hwdec_flush( HWDEC * d, hwdec_cb cb, void * ud ) {
    for ( int i = 0; i < 32; i++ ) {
        VdecFrameBuffer fb;
        int fi = next_frame( d, &fb );
        if ( fi < 0 ) break;
        VdecOutput out;
        memset( &out, 0, sizeof( out ) );
        out.this_size = sizeof( out );
        int rc = sceVideodec2Flush( d->dec, &fb, &out );
        if ( fb.is_accepted ) d->held[fi] = true;
        if ( rc != 0 || !out.is_valid ) break;
        if ( cb ) deliver( d, &out, cb, ud ); else { pending_take( d ); frames_release( d, out.frame_buffer ); }
    }
}

void hwdec_reset( HWDEC * d ) {
    hwdec_flush( d, NULL, NULL );
    int rc = sceVideodec2Reset( d->dec );
    if ( rc != 0 ) LOG( "reset %#x\n", ( unsigned ) rc );
    d->npending = 0;
    memset( d->held, 0, sizeof( d->held ) );
}

#endif /* __PROSPERO__ */
