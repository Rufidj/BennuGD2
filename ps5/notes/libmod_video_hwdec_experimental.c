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
#include <signal.h>
#include <pthread.h>
#include <ucontext.h>
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

/* static imports (libSceVideodec2 stub): bound by the loader once the library is loaded the sysmodule way */
extern int imp_QueryComputeMemoryInfo( VdecComputeMemory * info ) __asm__( "sceVideodec2QueryComputeMemoryInfo" );
extern int imp_AllocateComputeQueue( const VdecComputeConfig * config, const VdecComputeMemory * memory, void ** queue ) __asm__( "sceVideodec2AllocateComputeQueue" );
extern int imp_QueryDecoderMemoryInfo( const VdecConfig * config, VdecMemory * memory ) __asm__( "sceVideodec2QueryDecoderMemoryInfo" );
extern int imp_CreateDecoder( const VdecConfig * config, const VdecMemory * memory, void ** decoder ) __asm__( "sceVideodec2CreateDecoder" );
extern int imp_DeleteDecoder( void * decoder ) __asm__( "sceVideodec2DeleteDecoder" );
extern int imp_Decode( void * decoder, const VdecInput * input, VdecFrameBuffer * frame, VdecOutput * output ) __asm__( "sceVideodec2Decode" );
extern int imp_Flush( void * decoder, VdecFrameBuffer * frame, VdecOutput * output ) __asm__( "sceVideodec2Flush" );
extern int imp_Reset( void * decoder ) __asm__( "sceVideodec2Reset" );
int sceKernelLoadStartModuleForSysmodule( const char * path, size_t args, const void * argp, unsigned flags, const void * opt, int * res );

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

/* The console's decoder functions use a lot of stack; the interpreter's own threads have small ones
   (a call from there faulted on its own stack pointer). Everything that calls it runs on a thread
   with a big stack. */
#define BIG_STACK ( ( size_t ) 8 << 20 )

typedef struct { void * ( * fn )( void * ); void * arg; void * ret; } BIGCALL;

static void * big_trampoline( void * p ) {
    BIGCALL * c = ( BIGCALL * ) p;
    c->ret = c->fn( c->arg );
    return NULL;
}

static void * call_big( void * ( * fn )( void * ), void * arg ) {
    BIGCALL c = { fn, arg, NULL };
    pthread_attr_t at;
    pthread_t th;
    pthread_attr_init( &at );
    pthread_attr_setstacksize( &at, BIG_STACK );
    if ( pthread_create( &th, &at, big_trampoline, &c ) != 0 ) {
        pthread_attr_destroy( &at );
        LOG( "no big-stack thread, calling directly\n" );
        return fn( arg );
    }
    pthread_join( th, NULL );
    pthread_attr_destroy( &at );
    return c.ret;
}

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
        int ir = sceKernelGetModuleInfo( h[i], info );
        const char * name = ( const char * ) info + 8;
        LOG( "  module handle %d info=%#x name=[%.60s]\n", h[i], ( unsigned ) ir, ir == 0 ? name : "?" );
        if ( ir == 0 && strstr( name, "Videodec2" ) ) { LOG( "module %s (handle %d)\n", name, h[i] ); return h[i]; }
        if ( ir == 0 ) {                           /* does Dlsym work on this module for a function it exports? */
            void * q = NULL;
            int r1 = sceKernelDlsym( h[i], "malloc", &q );
            void * q2 = NULL;
            int r2 = sceKernelDlsym( h[i], "gQX+4GDQjpM", &q2 );
            void * q3 = NULL;
            int r3 = sceKernelDlsym( h[i], "gQX+4GDQjpM#libc#libc", &q3 );
            void * q4 = NULL;
            int r4 = sceKernelDlsym( h[i], "gQX+4GDQjpM#A#A", &q4 );
            LOG( "    Dlsym(malloc)=%#x %p NID=%#x %p NID#libc#libc=%#x %p NID#A#A=%#x %p\n", ( unsigned ) r1, q, ( unsigned ) r2, q2, ( unsigned ) r3, q3, ( unsigned ) r4, q4 );
        }
    }
    return -1;
}

static void module_map( int handle ) {
    uint8_t inf[0x160];
    memset( inf, 0, sizeof( inf ) );
    *( uint64_t * ) inf = sizeof( inf );
    if ( sceKernelGetModuleInfo( handle, inf ) != 0 ) { LOG( "module %d: no info\n", handle ); return; }
    LOG( "module %d [%.40s]:", handle, ( const char * ) inf + 8 );
    for ( int i = 0; i < 4; i++ ) {
        const uint8_t * sg = inf + 264 + i * 16;
        LOG( " seg%d %p+%#x prot%d", i, *( void * const * ) sg, *( const uint32_t * ) ( sg + 8 ), *( const int32_t * ) ( sg + 12 ) );
    }
    LOG( "\n" );
}

static void crash_report( int sig, siginfo_t * si, void * uc_ ) {
    ucontext_t * uc = ( ucontext_t * ) uc_;
    unsigned long rip = 0, rsp = 0;
#if defined( __FreeBSD__ ) || defined( __PROSPERO__ )
    rip = ( unsigned long ) uc->uc_mcontext.mc_rip; rsp = ( unsigned long ) uc->uc_mcontext.mc_rsp;
#endif
    fprintf( stderr, "libmod_video hw: CRASH signal %d fault addr %p rip %#lx rsp %#lx\n", sig, si ? si->si_addr : NULL, rip, rsp );
    if ( rsp >= 0x100000 && ( rsp & 7 ) == 0 ) {
        const unsigned long * sp = ( const unsigned long * ) rsp;
        for ( int i = 0, found = 0; i < 512 && found < 12; i++ )
            if ( sp[i] >= 0x400000 && sp[i] < 0x23f0000 ) { fprintf( stderr, "libmod_video hw:   stack[%d] %#lx\n", i, sp[i] ); found++; }
    }
    {   unsigned long long r15 = 0, rax = 0, rcx = 0, rdi = 0, rsi = 0, rbp = 0;
#if defined( __FreeBSD__ ) || defined( __PROSPERO__ )
        rax = uc->uc_mcontext.mc_rax; rcx = uc->uc_mcontext.mc_rcx; rdi = uc->uc_mcontext.mc_rdi;
        rsi = uc->uc_mcontext.mc_rsi; rbp = uc->uc_mcontext.mc_rbp; r15 = uc->uc_mcontext.mc_r15;
#endif
        fprintf( stderr, "libmod_video hw:   rax %#llx rcx %#llx rdi %#llx rsi %#llx rbp %#llx r15 %#llx\n", rax, rcx, rdi, rsi, rbp, r15 );
    }
    fflush( stderr );
    signal( sig, SIG_DFL );
    raise( sig );
}

static void crash_handler_install( void ) {
    struct sigaction sa;
    memset( &sa, 0, sizeof( sa ) );
    sa.sa_sigaction = crash_report;
    sa.sa_flags = SA_SIGINFO;
    sigaction( SIGSEGV, &sa, NULL );
    sigaction( SIGBUS, &sa, NULL );
    sigaction( SIGILL, &sa, NULL );
}

static int resolve_functions( void ) {
    char path[160] = "", priv[160] = "";
    const char * word = sceKernelGetFsSandboxRandomWord();
    if ( !word ) { LOG( "no sandbox word\n" ); return 0; }
    snprintf( path, sizeof( path ), "/%s/common/lib/libSceVideodec2.sprx", word );
    snprintf( priv, sizeof( priv ), "/%s/priv/lib/libSceVideodec2.sprx", word );
    int res = 0;
    int r = sceKernelLoadStartModuleForSysmodule( path, 0, NULL, 0, NULL, &res );
    LOG( "LoadStartModuleForSysmodule(%s) -> %#x (res %#x)\n", path, ( unsigned ) r, ( unsigned ) res );
    if ( r < 0 ) {
        r = sceKernelLoadStartModule( path, 0, NULL, 0, NULL, &res );
        LOG( "LoadStartModule(%s) -> %#x\n", path, ( unsigned ) r );
        if ( r < 0 ) return 0;
    }
    sceVideodec2QueryComputeMemoryInfo = imp_QueryComputeMemoryInfo;
    sceVideodec2AllocateComputeQueue   = imp_AllocateComputeQueue;
    sceVideodec2QueryDecoderMemoryInfo = imp_QueryDecoderMemoryInfo;
    sceVideodec2CreateDecoder          = imp_CreateDecoder;
    sceVideodec2DeleteDecoder          = imp_DeleteDecoder;
    sceVideodec2Decode                 = imp_Decode;
    sceVideodec2Flush                  = imp_Flush;
    sceVideodec2Reset                  = imp_Reset;
    LOG( "calling the imported QueryComputeMemoryInfo (a crash here = imports not bound)\n" );
    module_map( 0 ); module_map( 20 ); module_map( r );
    crash_handler_install();
    return 1;
}

static int hwdec_available_impl( void ) {
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

static void * available_thunk( void * a ) { ( void ) a; return ( void * ) ( intptr_t ) hwdec_available_impl(); }
int hwdec_available( void ) {
    if ( g_state ) return g_state > 0;
    return ( int ) ( intptr_t ) call_big( available_thunk, NULL );
}

static HWDEC * hwdec_open_impl( int hevc, int w, int h ) {
    if ( !hwdec_available_impl() ) return NULL;
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

typedef struct { int hevc, w, h; } OPENARGS;
static void * open_thunk( void * p ) { OPENARGS * a = ( OPENARGS * ) p; return hwdec_open_impl( a->hevc, a->w, a->h ); }
HWDEC * hwdec_open( int hevc, int w, int h ) {
    OPENARGS a = { hevc, w, h };
    return ( HWDEC * ) call_big( open_thunk, &a );
}

static void close_impl( HWDEC * d ) {
    if ( !d ) return;
    if ( d->dec ) sceVideodec2DeleteDecoder( d->dec );
    release_all( d );
    free( d );
}
static void * close_thunk( void * p ) { close_impl( ( HWDEC * ) p ); return NULL; }
void hwdec_close( HWDEC * d ) { if ( d ) call_big( close_thunk, d ); }

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
