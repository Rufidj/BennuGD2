/*
 *  torrent.c - BitTorrent client engine for streaming, see torrent.h
 *
 *  Threads: one "engine" thread owns every peer socket (poll loop); trackers are asked from short-lived threads;
 *  a "server" thread accepts the local HTTP connections and each connection has its own thread. One mutex (g_mu)
 *  protects all the shared state; the engine thread holds it except while it waits in poll().
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <SDL.h>
#include <libavformat/avio.h>
#include <libavutil/error.h>



#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#include <direct.h>
#include <windows.h>
#include <winioctl.h>
#include <fcntl.h>
#define ftruncate( fd, n ) _chsize_s( ( fd ), ( n ) )
#define strtok_r strtok_s
#define poll WSAPoll
#define CLOSESOCK closesocket
#define SOCKERR WSAGetLastError()
#define E_INPROGRESS WSAEWOULDBLOCK
typedef SOCKET sock_t;
#define BAD_SOCK INVALID_SOCKET
#else
#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#define CLOSESOCK close
#define SOCKERR errno
#define E_INPROGRESS EINPROGRESS
typedef int sock_t;
#define BAD_SOCK (-1)
#endif

#include "torrent.h"

/* Size of the cache file in advance. On Linux ftruncate leaves a sparse file. On Windows _chsize writes zeros for the whole length
 * (slow, and it fails when the disk has less free space than the torrent), so the file is marked sparse and its end set instead.
 * If it cannot be done the file simply grows as pieces are written. */
static int cache_presize( int fd, int64_t total ) {
#ifdef _WIN32
    HANDLE h = ( HANDLE ) _get_osfhandle( fd ); DWORD br = 0; LARGE_INTEGER li;
    if ( h == INVALID_HANDLE_VALUE ) return -1;
    DeviceIoControl( h, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &br, NULL );
    li.QuadPart = total;
    if ( !SetFilePointerEx( h, li, NULL, FILE_BEGIN ) || !SetEndOfFile( h ) ) return -1;
    _lseeki64( fd, 0, SEEK_SET );
    return 0;
#else
    return ftruncate( fd, ( off_t ) total );
#endif
}

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#ifdef _WIN32
#define strcasecmp _stricmp
#define CLOSEFD _close
#else
#define CLOSEFD close
#endif

#define T_MAX        4
#define MAX_PEERS    64
#define MAX_CAND     600
#define BLOCK        16384
#define PIPELINE     32
#define MAX_TRK      64
#define DHT_MAXN     160
#define MAX_MSG      (1 << 20)
#define ORDER_MAX    4096
#define HEAD_BYTES   ( 3 << 20 )                   /* the start and the end of a file are needed by the containers */

/* ------------------------------------------------------------------------------------------------ small helpers */
static uint32_t now_ms( void ) {
    struct timespec t; clock_gettime( CLOCK_MONOTONIC, &t );
    return ( uint32_t ) ( t.tv_sec * 1000u + t.tv_nsec / 1000000u );
}
static void be32w( uint8_t * p, uint32_t v ) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
static uint32_t be32r( const uint8_t * p ) { return ( ( uint32_t ) p[0] << 24 ) | ( p[1] << 16 ) | ( p[2] << 8 ) | p[3]; }
static void be64w( uint8_t * p, uint64_t v ) { be32w( p, ( uint32_t ) ( v >> 32 ) ); be32w( p + 4, ( uint32_t ) v ); }

static int hexv( int c ) {
    if ( c >= '0' && c <= '9' ) return c - '0';
    if ( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
    if ( c >= 'A' && c <= 'F' ) return c - 'A' + 10;
    return -1;
}
static void to_hex( const uint8_t * in, int n, char * out ) {
    static const char d[] = "0123456789abcdef"; int i;
    for ( i = 0; i < n; i++ ) { out[i * 2] = d[in[i] >> 4]; out[i * 2 + 1] = d[in[i] & 15]; }
    out[n * 2] = 0;
}
static void url_decode( char * s ) {
    char * w = s;
    for ( ; *s; s++ ) {
        if ( *s == '%' && hexv( s[1] ) >= 0 && hexv( s[2] ) >= 0 ) { *w++ = ( char ) ( hexv( s[1] ) * 16 + hexv( s[2] ) ); s += 2; }
        else if ( *s == '+' ) *w++ = ' ';
        else *w++ = *s;
    }
    *w = 0;
}
static int b32_decode( const char * s, uint8_t * out20 ) {          /* 32 base32 characters -> 20 bytes */
    uint64_t acc = 0; int bits = 0, n = 0;
    for ( ; *s && n < 20; s++ ) {
        int v;
        if ( *s >= 'A' && *s <= 'Z' ) v = *s - 'A'; else if ( *s >= 'a' && *s <= 'z' ) v = *s - 'a'; else if ( *s >= '2' && *s <= '7' ) v = *s - '2' + 26; else return 0;
        acc = ( acc << 5 ) | v; bits += 5;
        if ( bits >= 8 ) { bits -= 8; out20[n++] = ( uint8_t ) ( acc >> bits ); }
    }
    return n == 20;
}

/* ------------------------------------------------------------------------------------------------ SHA-1 */
typedef struct { uint32_t h[5]; uint64_t len; uint8_t buf[64]; int n; } Sha1;
#define ROL( x, k ) ( ( ( x ) << ( k ) ) | ( ( x ) >> ( 32 - ( k ) ) ) )
static void sha1_block( Sha1 * s, const uint8_t * p ) {
    uint32_t w[80], a, b, c, d, e, t; int i;
    for ( i = 0; i < 16; i++ ) w[i] = be32r( p + i * 4 );
    for ( i = 16; i < 80; i++ ) w[i] = ROL( w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1 );
    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3]; e = s->h[4];
    for ( i = 0; i < 80; i++ ) {
        uint32_t f, k;
        if ( i < 20 ) { f = ( b & c ) | ( ~b & d ); k = 0x5A827999; }
        else if ( i < 40 ) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
        else if ( i < 60 ) { f = ( b & c ) | ( b & d ) | ( c & d ); k = 0x8F1BBCDC; }
        else { f = b ^ c ^ d; k = 0xCA62C1D6; }
        t = ROL( a, 5 ) + f + e + k + w[i]; e = d; d = c; c = ROL( b, 30 ); b = a; a = t;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d; s->h[4] += e;
}
static void sha1_init( Sha1 * s ) {
    s->h[0] = 0x67452301; s->h[1] = 0xEFCDAB89; s->h[2] = 0x98BADCFE; s->h[3] = 0x10325476; s->h[4] = 0xC3D2E1F0; s->len = 0; s->n = 0;
}
static void sha1_update( Sha1 * s, const uint8_t * p, size_t n ) {
    s->len += n;
    while ( n ) {
        size_t k = 64 - s->n; if ( k > n ) k = n;
        memcpy( s->buf + s->n, p, k ); s->n += ( int ) k; p += k; n -= k;
        if ( s->n == 64 ) { sha1_block( s, s->buf ); s->n = 0; }
    }
}
static void sha1_final( Sha1 * s, uint8_t out[20] ) {
    uint64_t bits = s->len * 8; uint8_t pad = 0x80, z = 0, lenb[8]; int i;
    sha1_update( s, &pad, 1 );
    while ( s->n != 56 ) sha1_update( s, &z, 1 );
    be64w( lenb, bits ); sha1_update( s, lenb, 8 );
    for ( i = 0; i < 5; i++ ) be32w( out + i * 4, s->h[i] );
}

/* ------------------------------------------------------------------------------------------------ bencode */
typedef struct BNode {
    int t;                                   /* 'i' integer, 's' string, 'l' list, 'd' dictionary */
    int64_t i;                               /* integer value */
    const uint8_t * s; size_t n;             /* string bytes */
    struct BNode * k; int nk;                /* list items / dictionary key,value,key,value... */
    const uint8_t * raw; size_t rawn;        /* the exact bytes this node was parsed from */
} BNode;

static const uint8_t * bparse( const uint8_t * p, const uint8_t * end, BNode * o, int depth ) {
    memset( o, 0, sizeof *o ); o->raw = p;
    if ( p >= end || depth > 24 ) return NULL;
    if ( *p == 'i' ) {
        int neg = 0; int64_t v = 0; p++;
        if ( p < end && *p == '-' ) { neg = 1; p++; }
        while ( p < end && *p >= '0' && *p <= '9' ) { v = v * 10 + ( *p - '0' ); p++; }
        if ( p >= end || *p != 'e' ) return NULL;
        o->t = 'i'; o->i = neg ? -v : v; p++;
    } else if ( *p >= '0' && *p <= '9' ) {
        size_t n = 0;
        while ( p < end && *p >= '0' && *p <= '9' ) { n = n * 10 + ( *p - '0' ); p++; if ( n > ( 1u << 30 ) ) return NULL; }
        if ( p >= end || *p != ':' ) return NULL;
        p++;
        if ( n > ( size_t ) ( end - p ) ) return NULL;
        o->t = 's'; o->s = p; o->n = n; p += n;
    } else if ( *p == 'l' || *p == 'd' ) {
        int cap = 0;
        o->t = *p; p++;
        while ( p < end && *p != 'e' ) {
            if ( o->nk >= cap ) {
                BNode * nk;
                cap = cap ? cap * 2 : 8; nk = ( BNode * ) realloc( o->k, cap * sizeof( BNode ) );
                if ( !nk ) return NULL;
                o->k = nk;
            }
            p = bparse( p, end, &o->k[o->nk], depth + 1 );
            if ( !p ) { o->nk++; return NULL; }
            o->nk++;
        }
        if ( p >= end ) return NULL;
        p++;
    } else return NULL;
    o->rawn = ( size_t ) ( p - o->raw );
    return p;
}
static void bfree( BNode * o ) {
    int i;
    for ( i = 0; i < o->nk; i++ ) bfree( &o->k[i] );
    free( o->k ); o->k = NULL; o->nk = 0;
}
static BNode * bget( BNode * d, const char * key ) {
    int i; size_t n = strlen( key );
    if ( !d || d->t != 'd' ) return NULL;
    for ( i = 0; i + 1 < d->nk; i += 2 )
        if ( d->k[i].t == 's' && d->k[i].n == n && !memcmp( d->k[i].s, key, n ) ) return &d->k[i + 1];
    return NULL;
}

/* ------------------------------------------------------------------------------------------------ model */
typedef struct { char * path; int64_t len, off; } TFile;

typedef struct Peer {
    sock_t sock; uint32_t ip; uint16_t port; int st;         /* 0 connecting, 1 handshake, 2 active, -1 closed */
    uint8_t * rbuf; size_t rlen, rcap;
    uint8_t * sbuf; size_t slen, soff, scap;
    int choked, am_interested, peer_interested, am_choking;   /* choked = the peer chokes us */
    uint8_t * bits; size_t bits_len;
    int ext_md, md_size;
    int nreq; int rp[PIPELINE], rb[PIPELINE]; uint32_t rt[PIPELINE];
    uint32_t t_conn, t_rx, t_tx; int cand;
} Peer;

typedef struct { uint32_t ip; uint16_t port; uint32_t next_ms; int used; int connected; } Cand;

typedef struct { uint8_t id[20]; uint32_t ip; uint16_t port; uint8_t st, known; uint32_t ts; } DNode;     /* DHT node: st 0 new, 1 asked, 2 answered, 3 no answer */

typedef struct Torrent Torrent;
typedef struct { Torrent * t; int idx; unsigned gen; } TrkArg;

struct Torrent {
    int used, dying; unsigned gen; int h;
    uint8_t ih[20]; char name[256]; char err[200]; int state;
    char * trk[MAX_TRK]; int ntrk; int trk_busy[MAX_TRK]; uint32_t trk_next[MAX_TRK]; uint8_t trk_extra[MAX_TRK];
    /* metadata */
    uint8_t * md; int md_size; uint8_t * md_have; int md_pieces; uint32_t * md_req; int md_got;
    int64_t plen, total; int npieces, bpp; uint8_t * hashes;
    TFile * files; int nfiles; int sel; int fp0, fp1; int64_t sel_off, sel_len;
    /* progress */
    uint8_t * have; uint8_t * mark; uint8_t * bst; uint32_t * btime;         /* piece done; block state 0 none 1 requested 2 received */
    int nhave; int * order; int norder;
    int fd; char path[1024]; int existing; int recheck;
    int64_t reader_abs; uint32_t reader_ms;
    int64_t downloaded, uploaded; int down_rate, up_rate; int64_t last_dl, last_ul; uint32_t last_stat;
    int up_tokens;
    Peer * peers[MAX_PEERS]; Cand cand[MAX_CAND]; int ncand;
    uint32_t last_tick, last_announce, t_start; int hashfails;
    char src[2048]; int loading;
    DNode dn[DHT_MAXN]; int ndn, dht_active, dht_sent, dht_ans, dht_found; uint32_t dht_t0;      /* DHT look-up of this torrent */
};

static SDL_mutex * g_mu; static SDL_cond * g_cv;
static Torrent g_t[T_MAX];
static int g_run, g_up_limit;
static char g_cache[900];
static SDL_Thread * g_eng_th, * g_srv_th;
static sock_t g_lsock = BAD_SOCK; static int g_port;
static uint8_t g_pid[20];
static unsigned g_gen_counter;
static int64_t g_ahead = ( int64_t ) 192 << 20;           /* how far past the play position we download (the player raises it when it needs a bigger buffer) */
static int g_extra_on = 1, g_dht_on = 1;                    /* options: public trackers added to every torrent, and DHT */
static sock_t g_psock = BAD_SOCK; static int g_pport, g_xport; static uint32_t g_myip;         /* incoming peers: listening socket, its port, the port announced (after NAT mapping), our public address */
typedef struct { int used; sock_t s; uint8_t buf[68]; int n; uint32_t t0, ip; uint16_t port; } Pend;          /* a connection that came in and has not sent its handshake yet */
static Pend g_pend[16];
static volatile int g_nat_ok;
static sock_t g_dsock = BAD_SOCK; static uint8_t g_nid[20];
static uint32_t g_boot_ip[16]; static uint16_t g_boot_port[16]; static int g_nboot;

#define DBG( ... ) do { if ( getenv( "TORRENT_DEBUG" ) ) { fprintf( stderr, "[torrent] " __VA_ARGS__ ); fputc( 10, stderr ); } } while ( 0 )
static char g_logpath[1100];
/* a small log next to the cache (torrent.log): what the trackers and the peers answered. Kept under 256 KB. */
static void tlog( const char * fmt, ... ) {
    FILE * f; va_list ap; time_t t = time( NULL ); struct tm * tm = localtime( &t ); char ts[16];
    if ( !g_logpath[0] ) return;
    f = fopen( g_logpath, "ab" );
    if ( !f ) return;
    if ( ftell( f ) > ( 256 << 10 ) ) { fclose( f ); f = fopen( g_logpath, "wb" ); if ( !f ) return; }
    strftime( ts, sizeof ts, "%H:%M:%S", tm ); fprintf( f, "%s ", ts );
    va_start( ap, fmt ); vfprintf( f, fmt, ap ); va_end( ap ); fputc( 10, f ); fclose( f );
}

#define LOCK() SDL_LockMutex( g_mu )
#define UNLOCK() SDL_UnlockMutex( g_mu )

/* bigger socket buffers: on wifi (high and variable latency) a connection needs more data in flight to keep its speed */
static void big_buffers( int s, int rcv, int snd ) {
    int v = rcv; setsockopt( s, SOL_SOCKET, SO_RCVBUF, ( const char * ) &v, sizeof v );
    v = snd;     setsockopt( s, SOL_SOCKET, SO_SNDBUF, ( const char * ) &v, sizeof v );
}

/* a time-out on a socket: seconds on POSIX, milliseconds in a DWORD on Windows */
static void sock_timeout( int s, int ms, int send_too ) {
#ifdef _WIN32
    DWORD v = ( DWORD ) ms;
    setsockopt( ( SOCKET ) s, SOL_SOCKET, SO_RCVTIMEO, ( const char * ) &v, sizeof v );
    if ( send_too ) setsockopt( ( SOCKET ) s, SOL_SOCKET, SO_SNDTIMEO, ( const char * ) &v, sizeof v );
#else
    struct timeval tv; tv.tv_sec = ms / 1000; tv.tv_usec = ( ms % 1000 ) * 1000;
    setsockopt( s, SOL_SOCKET, SO_RCVTIMEO, ( const char * ) &tv, sizeof tv );
    if ( send_too ) setsockopt( s, SOL_SOCKET, SO_SNDTIMEO, ( const char * ) &tv, sizeof tv );
#endif
}

static void tlog( const char * fmt, ... );
static void set_err( Torrent * t, const char * m ) { snprintf( t->err, sizeof t->err, "%s", m ); t->state = 2; tlog( "error: %s", m ); SDL_CondBroadcast( g_cv ); }

static int piece_len( const Torrent * t, int p ) {
    int64_t start = ( int64_t ) p * t->plen, rest = t->total - start;
    return ( int ) ( rest < t->plen ? rest : t->plen );
}
static int piece_blocks( const Torrent * t, int p ) { return ( piece_len( t, p ) + BLOCK - 1 ) / BLOCK; }

/* ------------------------------------------------------------------------------------------------ file i/o */
static int64_t fpread( int fd, void * b, size_t n, int64_t off ) {
#ifdef _WIN32
    static SDL_mutex * m; int64_t r; if ( !m ) m = SDL_CreateMutex();
    SDL_LockMutex( m ); _lseeki64( fd, off, SEEK_SET ); r = _read( fd, b, ( unsigned ) n ); SDL_UnlockMutex( m ); return r;
#elif defined( __SWITCH__ )          /* newlib (libnx) has no pread/pwrite: seek + read under a lock */
    static SDL_mutex * m; int64_t r; if ( !m ) m = SDL_CreateMutex();
    SDL_LockMutex( m ); lseek( fd, ( off_t ) off, SEEK_SET ); r = read( fd, b, n ); SDL_UnlockMutex( m ); return r;
#else
    return pread( fd, b, n, ( off_t ) off );
#endif
}
static int64_t fpwrite( int fd, const void * b, size_t n, int64_t off ) {
#ifdef _WIN32
    static SDL_mutex * m; int64_t r; if ( !m ) m = SDL_CreateMutex();
    SDL_LockMutex( m ); _lseeki64( fd, off, SEEK_SET ); r = _write( fd, b, ( unsigned ) n ); SDL_UnlockMutex( m ); return r;
#elif defined( __SWITCH__ )
    static SDL_mutex * m; int64_t r; if ( !m ) m = SDL_CreateMutex();
    SDL_LockMutex( m ); lseek( fd, ( off_t ) off, SEEK_SET ); r = write( fd, b, n ); SDL_UnlockMutex( m ); return r;
#else
    return pwrite( fd, b, n, ( off_t ) off );
#endif
}

/* ------------------------------------------------------------------------------------------------ http (through FFmpeg, so https works) */
typedef struct { double deadline; } HttpDl;
static int http_irq( void * o ) {
    struct timespec t; clock_gettime( CLOCK_MONOTONIC, &t );
    return ( ( HttpDl * ) o )->deadline < t.tv_sec + t.tv_nsec / 1e9;
}
static int http_get( const char * url, uint8_t ** out, size_t * outn, size_t maxn, int timeout_s ) {
    AVIOContext * io = NULL; AVDictionary * o = NULL; AVIOInterruptCB cb; HttpDl hd; struct timespec t;
    size_t cap = 32768, n = 0; uint8_t * buf; int r;
    clock_gettime( CLOCK_MONOTONIC, &t ); hd.deadline = t.tv_sec + t.tv_nsec / 1e9 + timeout_s;
    cb.callback = http_irq; cb.opaque = &hd;
    av_dict_set( &o, "user_agent", "BennuGD2-MediaPlayer/1.0", 0 );
    av_dict_set( &o, "rw_timeout", "12000000", 0 );
    r = avio_open2( &io, url, AVIO_FLAG_READ, &cb, &o );
    av_dict_free( &o );
    if ( r < 0 ) return -1;
    buf = ( uint8_t * ) malloc( cap );
    while ( buf ) {
        int k;
        if ( n + 16384 > cap ) {
            uint8_t * nb;
            if ( cap >= maxn ) break;
            cap *= 2; nb = ( uint8_t * ) realloc( buf, cap );
            if ( !nb ) { free( buf ); buf = NULL; break; }
            buf = nb;
        }
        k = avio_read( io, buf + n, 16384 );
        if ( k <= 0 ) break;
        n += ( size_t ) k;
    }
    avio_closep( &io );
    if ( !buf || !n ) { free( buf ); return -1; }
    *out = buf; *outn = n;
    return 0;
}

/* ------------------------------------------------------------------------------------------------ metadata ("info" dictionary) */
static const char * video_exts[] = { "mp4", "mkv", "avi", "webm", "mov", "m4v", "mpg", "mpeg", "ts", "wmv", "flv", "ogv", "mp3", "flac", "m4a", "ogg", "opus", "wav", NULL };
static int is_media_name( const char * nm ) {
    const char * e = strrchr( nm, '.' ); int i;
    if ( !e ) return 0;
    for ( i = 0; video_exts[i]; i++ ) if ( !strcasecmp( e + 1, video_exts[i] ) ) return 1;
    return 0;
}

static void select_range( Torrent * t, int fi ) {
    t->sel = fi; t->sel_off = t->files[fi].off; t->sel_len = t->files[fi].len;
    t->fp0 = ( int ) ( t->sel_off / t->plen );
    t->fp1 = t->sel_len > 0 ? ( int ) ( ( t->sel_off + t->sel_len - 1 ) / t->plen ) : t->fp0;
}

/* Fills the torrent from an "info" dictionary (bytes). Called with the lock held. */
static int set_info( Torrent * t, const uint8_t * info, size_t n ) {
    BNode d; BNode * v; int i, best = -1; int64_t bestlen = -1, off = 0;
    if ( !bparse( info, info + n, &d, 0 ) || d.t != 'd' ) { bfree( &d ); set_err( t, "bad torrent metadata" ); return 0; }
    v = bget( &d, "piece length" );
    if ( !v || v->t != 'i' || v->i <= 0 || v->i > ( 64 << 20 ) ) { bfree( &d ); set_err( t, "bad piece length" ); return 0; }
    t->plen = v->i;
    v = bget( &d, "pieces" );
    if ( !v || v->t != 's' || v->n % 20 || !v->n ) { bfree( &d ); set_err( t, "bad piece list" ); return 0; }
    t->npieces = ( int ) ( v->n / 20 );
    t->hashes = ( uint8_t * ) malloc( v->n ); memcpy( t->hashes, v->s, v->n );
    v = bget( &d, "name" );
    if ( v && v->t == 's' ) { size_t k = v->n < sizeof t->name - 1 ? v->n : sizeof t->name - 1; memcpy( t->name, v->s, k ); t->name[k] = 0; }
    v = bget( &d, "length" );
    if ( v && v->t == 'i' ) {
        t->nfiles = 1; t->files = ( TFile * ) calloc( 1, sizeof( TFile ) );
        t->files[0].path = strdup( t->name[0] ? t->name : "file" ); t->files[0].len = v->i; t->files[0].off = 0; t->total = v->i;
    } else {
        v = bget( &d, "files" );
        if ( !v || v->t != 'l' || !v->nk ) { bfree( &d ); set_err( t, "torrent has no files" ); return 0; }
        t->nfiles = v->nk; t->files = ( TFile * ) calloc( v->nk, sizeof( TFile ) );
        for ( i = 0; i < v->nk; i++ ) {
            BNode * f = &v->k[i], * len = bget( f, "length" ), * path = bget( f, "path" ); char nm[512] = ""; int j;
            if ( !len || len->t != 'i' || !path || path->t != 'l' ) continue;
            for ( j = 0; j < path->nk; j++ ) {
                if ( path->k[j].t != 's' ) continue;
                if ( j ) strncat( nm, "/", sizeof nm - strlen( nm ) - 1 );
                { size_t room = sizeof nm - strlen( nm ) - 1, k = path->k[j].n < room ? path->k[j].n : room; strncat( nm, ( const char * ) path->k[j].s, k ); }
            }
            t->files[i].path = strdup( nm ); t->files[i].len = len->i; t->files[i].off = off; off += len->i;
        }
        t->total = off;
    }
    bfree( &d );
    if ( t->total <= 0 || ( t->total + t->plen - 1 ) / t->plen != t->npieces ) { set_err( t, "inconsistent torrent" ); return 0; }
    t->bpp = ( int ) ( ( t->plen + BLOCK - 1 ) / BLOCK );
    if ( ( int64_t ) t->npieces * t->bpp > 60000000 ) { set_err( t, "torrent too large" ); return 0; }
    t->have = ( uint8_t * ) calloc( t->npieces, 1 );
    t->mark = ( uint8_t * ) calloc( t->npieces, 1 );
    t->bst = ( uint8_t * ) calloc( ( size_t ) t->npieces * t->bpp, 1 );
    t->btime = ( uint32_t * ) calloc( ( size_t ) t->npieces * t->bpp, sizeof( uint32_t ) );
    t->order = ( int * ) malloc( ORDER_MAX * sizeof( int ) );
    for ( i = 0; i < t->nfiles; i++ )                                    /* default file: the biggest media file, else the biggest */
        if ( is_media_name( t->files[i].path ) && t->files[i].len > bestlen ) { best = i; bestlen = t->files[i].len; }
    if ( best < 0 ) for ( i = 0; i < t->nfiles; i++ ) if ( t->files[i].len > bestlen ) { best = i; bestlen = t->files[i].len; }
    select_range( t, best < 0 ? 0 : best );
    /* the cache file (sparse) */
    { char hx[41]; struct stat st; to_hex( t->ih, 20, hx ); snprintf( t->path, sizeof t->path, "%s/%s.dat", g_cache, hx );
      t->existing = stat( t->path, &st ) == 0 && ( int64_t ) st.st_size == t->total;
#ifdef _WIN32
      t->fd = _open( t->path, _O_RDWR | _O_CREAT | _O_BINARY, 0666 );
#else
      t->fd = open( t->path, O_RDWR | O_CREAT, 0666 );
#endif
      if ( t->fd < 0 ) { set_err( t, "cannot write the cache" ); return 0; }
      if ( !t->existing && cache_presize( t->fd, t->total ) != 0 ) tlog( "cache: could not set the size (%lld MB) in advance; it will grow as pieces arrive", ( long long ) ( t->total >> 20 ) );
      t->recheck = t->existing ? t->fp0 : t->fp1 + 1; }
    t->state = 1;
    tlog( "ready: '%s' %d files, %d pieces of %d KB, %lld MB, %d trackers", t->name, t->nfiles, t->npieces, ( int ) ( t->plen / 1024 ), ( long long ) ( t->total >> 20 ), t->ntrk );
    SDL_CondBroadcast( g_cv );
    return 1;
}

/* ------------------------------------------------------------------------------------------------ pieces */
static int has_piece( const Peer * p, int i ) { return ( size_t ) ( i >> 3 ) < p->bits_len && ( p->bits[i >> 3] & ( 0x80 >> ( i & 7 ) ) ); }

static void send_raw( Peer * p, const uint8_t * d, size_t n ) {
    if ( p->st < 0 ) return;
    if ( p->soff && p->soff == p->slen ) { p->soff = p->slen = 0; }
    if ( p->slen + n > p->scap ) {
        size_t nc = p->scap ? p->scap : 4096; uint8_t * nb;
        while ( nc < p->slen + n ) nc *= 2;
        nb = ( uint8_t * ) realloc( p->sbuf, nc ); if ( !nb ) { p->st = -1; return; }
        p->sbuf = nb; p->scap = nc;
    }
    memcpy( p->sbuf + p->slen, d, n ); p->slen += n;
}
static void send_msg( Peer * p, int id, const uint8_t * pl, size_t n ) {
    uint8_t h[5]; be32w( h, ( uint32_t ) n + 1 ); h[4] = ( uint8_t ) id;
    send_raw( p, h, 5 ); if ( n ) send_raw( p, pl, n );
}

static void piece_done_check( Torrent * t, int pi ) {
    int b, nb = piece_blocks( t, pi ), len = piece_len( t, pi ); Sha1 s; uint8_t dig[20], * buf; int64_t done = 0, base = ( int64_t ) pi * t->plen; int i;
    for ( b = 0; b < nb; b++ ) if ( t->bst[( size_t ) pi * t->bpp + b] != 2 ) return;
    buf = ( uint8_t * ) malloc( 65536 ); if ( !buf ) return;
    sha1_init( &s );
    while ( done < len ) {
        int64_t k = len - done < 65536 ? len - done : 65536;
        if ( fpread( t->fd, buf, ( size_t ) k, base + done ) != k ) break;
        sha1_update( &s, buf, ( size_t ) k ); done += k;
    }
    free( buf ); sha1_final( &s, dig );
    if ( done == len && !memcmp( dig, t->hashes + ( size_t ) pi * 20, 20 ) ) {
        t->have[pi] = 1; t->nhave++;
        for ( i = 0; i < MAX_PEERS; i++ ) {
            Peer * p = t->peers[i]; uint8_t m[4];
            if ( p && p->st == 2 ) { be32w( m, ( uint32_t ) pi ); send_msg( p, 4, m, 4 ); }
        }
        SDL_CondBroadcast( g_cv );
    } else {
        t->hashfails++;
        for ( b = 0; b < nb; b++ ) t->bst[( size_t ) pi * t->bpp + b] = 0;
    }
}

/* the pieces we want, in the order we want them: the start and end of the file, then from the play position forward */
static void build_order( Torrent * t ) {
    int n = 0, rp, i, lim, hp, tp, e1;
    int64_t reader = t->reader_abs < t->sel_off ? t->sel_off : t->reader_abs;
    memset( t->mark, 0, t->npieces );
#define ADD( pc ) do { int q = ( pc ); if ( q >= t->fp0 && q <= t->fp1 && !t->have[q] && !t->mark[q] && n < ORDER_MAX ) { t->mark[q] = 1; t->order[n++] = q; } } while ( 0 )
    hp = ( int ) ( ( t->sel_off + HEAD_BYTES - 1 ) / t->plen ); tp = ( int ) ( ( t->sel_off + t->sel_len - HEAD_BYTES ) / t->plen );
    if ( hp > t->fp1 ) hp = t->fp1;
    if ( tp < t->fp0 ) tp = t->fp0;
    if ( t->reader_ms == 0 || reader - t->sel_off < HEAD_BYTES ) { for ( i = t->fp0; i <= hp && n < 120; i++ ) ADD( i ); }
    rp = ( int ) ( reader / t->plen ); if ( rp < t->fp0 ) rp = t->fp0;
    lim = rp + ( int ) ( g_ahead / t->plen ) + 1; if ( lim > t->fp1 ) lim = t->fp1;
    e1 = rp + ( int ) ( ( 6 << 20 ) / t->plen ) + 1; if ( e1 > lim ) e1 = lim;
    for ( i = rp; i <= e1 && n < 200; i++ ) ADD( i );                  /* what plays first */
    for ( i = tp; i <= t->fp1 && n < 300; i++ ) ADD( i );              /* the end of the file: mp4 keeps its index there */
    for ( i = rp; i <= lim && n < ORDER_MAX - 20; i++ ) ADD( i );      /* then everything ahead */
    for ( i = t->fp0; i <= hp && n < ORDER_MAX - 10; i++ ) ADD( i );
#undef ADD
    t->norder = n;
}

static void request_more( Torrent * t, Peer * p ) {
    int oi, b, nb, pi; uint32_t now = now_ms();
    if ( p->st != 2 || p->choked || t->state != 1 ) return;
    for ( oi = 0; oi < t->norder && p->nreq < PIPELINE; oi++ ) {
        pi = t->order[oi];
        if ( t->have[pi] || !has_piece( p, pi ) ) continue;
        nb = piece_blocks( t, pi );
        for ( b = 0; b < nb && p->nreq < PIPELINE; b++ ) {
            size_t bi = ( size_t ) pi * t->bpp + b; int k, mine = 0;
            if ( t->bst[bi] == 2 ) continue;
            if ( t->bst[bi] == 1 && now - t->btime[bi] < ( oi < 12 ? 2500u : 9000u ) ) continue;      /* someone asked recently: wait (what plays next is asked again sooner) */
            for ( k = 0; k < p->nreq; k++ ) if ( p->rp[k] == pi && p->rb[k] == b ) { mine = 1; break; }
            if ( mine ) continue;
            { uint8_t m[12]; int len = BLOCK; if ( b == nb - 1 ) len = piece_len( t, pi ) - b * BLOCK;
              be32w( m, ( uint32_t ) pi ); be32w( m + 4, ( uint32_t ) b * BLOCK ); be32w( m + 8, ( uint32_t ) len );
              send_msg( p, 6, m, 12 ); }
            t->bst[bi] = 1; t->btime[bi] = now;
            p->rp[p->nreq] = pi; p->rb[p->nreq] = b; p->rt[p->nreq] = now; p->nreq++;
        }
    }
}

static void drop_requests( Torrent * t, Peer * p ) {
    int k;
    for ( k = 0; k < p->nreq; k++ ) {
        size_t bi = ( size_t ) p->rp[k] * t->bpp + p->rb[k];
        if ( t->state == 1 && t->bst[bi] == 1 ) t->bst[bi] = 0;
    }
    p->nreq = 0;
}

/* ------------------------------------------------------------------------------------------------ peers */
static void peer_close( Torrent * t, Peer * p ) {
    if ( p->st == 2 || p->st == 1 || p->st == 0 ) drop_requests( t, p );
    if ( p->sock != BAD_SOCK ) CLOSESOCK( p->sock );
    p->sock = BAD_SOCK; p->st = -1;
    if ( p->cand >= 0 && p->cand < MAX_CAND ) { t->cand[p->cand].connected = 0; t->cand[p->cand].next_ms = now_ms() + 45000; }
}

static void peer_free( Torrent * t, int i ) {
    Peer * p = t->peers[i];
    if ( !p ) return;
    if ( p->st >= 0 ) peer_close( t, p );
    free( p->rbuf ); free( p->sbuf ); free( p->bits ); free( p );
    t->peers[i] = NULL;
}

static void send_ext_handshake( Torrent * t, Peer * p ) {
    char d[200]; uint8_t m[220]; int n;
    n = snprintf( d, sizeof d, "d1:md11:ut_metadatai2ee1:pi%de1:v14:BennuGD2 Playere", g_xport ? g_xport : g_port );
    (void) t;
    m[0] = 0; memcpy( m + 1, d, n );
    send_msg( p, 20, m, n + 1 );
}

static void ask_metadata( Torrent * t, Peer * p ) {
    int i; uint32_t now = now_ms();
    if ( !p->ext_md || !t->md || t->md_got >= t->md_pieces || p->md_size <= 0 ) return;
    for ( i = 0; i < t->md_pieces; i++ ) {
        if ( t->md_have[i] || now - t->md_req[i] < 4000 ) continue;
        { char d[64]; uint8_t m[80]; int n = snprintf( d, sizeof d, "d8:msg_typei0e5:piecei%dee", i );
          m[0] = ( uint8_t ) p->ext_md; memcpy( m + 1, d, n ); send_msg( p, 20, m, n + 1 ); }
        t->md_req[i] = now;
        break;
    }
}

static void on_extended( Torrent * t, Peer * p, const uint8_t * d, size_t n ) {
    BNode b; const uint8_t * e;
    if ( n < 1 ) return;
    if ( d[0] == 0 ) {                                                   /* extension handshake */
        BNode * m; BNode * v;
        e = bparse( d + 1, d + n, &b, 0 );
        if ( e && b.t == 'd' ) {
            m = bget( &b, "m" );
            if ( m && m->t == 'd' ) { v = bget( m, "ut_metadata" ); if ( v && v->t == 'i' ) p->ext_md = ( int ) v->i; }
            v = bget( &b, "metadata_size" ); if ( v && v->t == 'i' && v->i > 0 && v->i < ( 8 << 20 ) ) p->md_size = ( int ) v->i;
            DBG( "ext handshake: ut_metadata=%d size=%d", p->ext_md, p->md_size );
            if ( t->state == 0 && !t->md && p->md_size > 0 ) {            /* first peer that knows the size: prepare to receive it */
                t->md_size = p->md_size; t->md = ( uint8_t * ) calloc( 1, t->md_size ); t->md_pieces = ( t->md_size + BLOCK - 1 ) / BLOCK;
                t->md_have = ( uint8_t * ) calloc( t->md_pieces, 1 ); t->md_req = ( uint32_t * ) calloc( t->md_pieces, sizeof( uint32_t ) ); t->md_got = 0;
            }
            if ( t->state == 0 ) ask_metadata( t, p );
        }
        bfree( &b );
    } else if ( d[0] == 2 ) {                                            /* ut_metadata message (our id is 2) */
        BNode * mt, * pc;
        e = bparse( d + 1, d + n, &b, 0 );
        if ( e && b.t == 'd' ) {
            mt = bget( &b, "msg_type" ); pc = bget( &b, "piece" );
            if ( mt && pc && mt->t == 'i' && pc->t == 'i' && mt->i == 1 && t->md && t->state == 0 && pc->i >= 0 && pc->i < t->md_pieces && !t->md_have[pc->i] ) {
                size_t len = n - 1 - ( size_t ) ( e - ( d + 1 ) ); int64_t off = pc->i * ( int64_t ) BLOCK;
                if ( off + ( int64_t ) len <= t->md_size ) {
                    memcpy( t->md + off, e, len ); t->md_have[pc->i] = 1; t->md_got++;
                    if ( t->md_got >= t->md_pieces ) {
                        Sha1 s; uint8_t dig[20]; sha1_init( &s ); sha1_update( &s, t->md, t->md_size ); sha1_final( &s, dig );
                        if ( !memcmp( dig, t->ih, 20 ) ) set_info( t, t->md, t->md_size );
                        else { memset( t->md_have, 0, t->md_pieces ); t->md_got = 0; }      /* corrupt: start again */
                    }
                }
            } else if ( mt && mt->t == 'i' && mt->i == 0 && pc && pc->t == 'i' ) {       /* a peer asks us for the metadata: say we have none */
                char r[64]; uint8_t m[80]; int k = snprintf( r, sizeof r, "d8:msg_typei2e5:piecei%lldee", ( long long ) pc->i );
                m[0] = ( uint8_t ) ( p->ext_md ? p->ext_md : 1 ); memcpy( m + 1, r, k ); send_msg( p, 20, m, k + 1 );
            }
        }
        bfree( &b );
    }
}

static void on_message( Torrent * t, Peer * p, int id, const uint8_t * d, size_t n ) {
    switch ( id ) {
    case 0: p->choked = 1; drop_requests( t, p ); break;
    case 1: p->choked = 0; break;
    case 2: p->peer_interested = 1; break;
    case 3: p->peer_interested = 0; break;
    case 4:
        if ( n >= 4 ) {
            uint32_t i = be32r( d );
            if ( i < 4000000 ) {
                size_t need = ( i >> 3 ) + 1;
                if ( need > p->bits_len ) { uint8_t * nb = ( uint8_t * ) realloc( p->bits, need ); if ( nb ) { memset( nb + p->bits_len, 0, need - p->bits_len ); p->bits = nb; p->bits_len = need; } }
                if ( need <= p->bits_len ) p->bits[i >> 3] |= 0x80 >> ( i & 7 );
            }
        }
        break;
    case 5:
        free( p->bits ); p->bits = ( uint8_t * ) malloc( n ? n : 1 ); if ( p->bits ) { memcpy( p->bits, d, n ); p->bits_len = n; } else p->bits_len = 0;
        break;
    case 6:                                                              /* a request: only if uploading is allowed */
        if ( n >= 12 && g_up_limit > 0 && !p->am_choking && t->state == 1 ) {
            uint32_t pi = be32r( d ), off = be32r( d + 4 ), len = be32r( d + 8 );
            if ( pi < ( uint32_t ) t->npieces && t->have[pi] && len > 0 && len <= BLOCK && ( int64_t ) off + len <= piece_len( t, pi ) && t->up_tokens >= ( int ) len && p->slen - p->soff < ( 512 << 10 ) ) {
                uint8_t * m = ( uint8_t * ) malloc( 13 + len );
                if ( m ) {
                    be32w( m, 9 + len ); m[4] = 7; be32w( m + 5, pi ); be32w( m + 9, off );
                    if ( fpread( t->fd, m + 13, len, ( int64_t ) pi * t->plen + off ) == ( int64_t ) len ) { send_raw( p, m, 13 + len ); t->up_tokens -= ( int ) len; t->uploaded += len; }
                    free( m );
                }
            }
        }
        break;
    case 7:
        if ( n > 8 && t->state == 1 ) {
            uint32_t pi = be32r( d ), off = be32r( d + 4 ); size_t len = n - 8; int k, b;
            if ( pi < ( uint32_t ) t->npieces && off % BLOCK == 0 && ( int64_t ) off + len <= piece_len( t, pi ) ) {
                b = ( int ) ( off / BLOCK );
                for ( k = 0; k < p->nreq; k++ ) if ( p->rp[k] == ( int ) pi && p->rb[k] == b ) { p->rp[k] = p->rp[p->nreq - 1]; p->rb[k] = p->rb[p->nreq - 1]; p->rt[k] = p->rt[p->nreq - 1]; p->nreq--; break; }
                if ( t->bst[( size_t ) pi * t->bpp + b] != 2 && !t->have[pi] ) {
                    if ( fpwrite( t->fd, d + 8, len, ( int64_t ) pi * t->plen + off ) == ( int64_t ) len ) {
                        t->bst[( size_t ) pi * t->bpp + b] = 2; t->downloaded += ( int64_t ) len;
                        piece_done_check( t, ( int ) pi );
                    }
                }
            }
        }
        break;
    case 20: DBG( "ext message %d (%zu bytes)", n ? d[0] : -1, n ); on_extended( t, p, d, n ); break;
    default: break;
    }
}

static void peer_input( Torrent * t, Peer * p ) {
    for ( ;; ) {
        if ( p->st == 1 ) {                                              /* handshake: 68 bytes */
            if ( p->rlen < 68 ) return;
            DBG( "handshake from peer, %zu bytes", p->rlen );
            if ( p->rlen >= 68 && !memcmp( p->rbuf + 48, g_pid, 20 ) ) { int ci = p->cand; peer_close( t, p ); if ( ci >= 0 && ci < MAX_CAND ) t->cand[ci].next_ms = now_ms() + 3600000; return; }      /* that is us */
            if ( p->rbuf[0] != 19 || memcmp( p->rbuf + 1, "BitTorrent protocol", 19 ) || memcmp( p->rbuf + 28, t->ih, 20 ) ) { peer_close( t, p ); return; }
            { uint8_t ext = p->rbuf[25] & 0x10;
              memmove( p->rbuf, p->rbuf + 68, p->rlen - 68 ); p->rlen -= 68; p->st = 2;
              if ( ext ) send_ext_handshake( t, p ); }
            if ( t->state == 1 ) { send_msg( p, 2, NULL, 0 ); p->am_interested = 1; }
            continue;
        }
        if ( p->rlen < 4 ) return;
        { uint32_t len = be32r( p->rbuf );
          if ( len > MAX_MSG ) { peer_close( t, p ); return; }
          if ( p->rlen < 4 + ( size_t ) len ) return;
          if ( len > 0 ) on_message( t, p, p->rbuf[4], p->rbuf + 5, len - 1 );
          if ( p->st < 0 ) return;
          memmove( p->rbuf, p->rbuf + 4 + len, p->rlen - 4 - len ); p->rlen -= 4 + len; }
    }
}

static void peer_connect( Torrent * t, int ci ) {
    int i; Peer * p; struct sockaddr_in sa; sock_t s; int one = 1;
    for ( i = 0; i < MAX_PEERS; i++ ) if ( !t->peers[i] ) break;
    if ( i == MAX_PEERS ) return;
    s = socket( AF_INET, SOCK_STREAM, 0 );
    if ( s == BAD_SOCK ) return;
#ifdef _WIN32
    { u_long nb = 1; ioctlsocket( s, FIONBIO, &nb ); }
#else
    fcntl( s, F_SETFL, fcntl( s, F_GETFL, 0 ) | O_NONBLOCK );
#endif
    setsockopt( s, IPPROTO_TCP, TCP_NODELAY, ( const char * ) &one, sizeof one );
    big_buffers( ( int ) s, 1 << 20, 256 << 10 );
    memset( &sa, 0, sizeof sa ); sa.sin_family = AF_INET; sa.sin_port = htons( t->cand[ci].port ); sa.sin_addr.s_addr = t->cand[ci].ip;
    if ( connect( s, ( struct sockaddr * ) &sa, sizeof sa ) != 0 && SOCKERR != E_INPROGRESS ) { CLOSESOCK( s ); t->cand[ci].next_ms = now_ms() + 120000; return; }
    p = ( Peer * ) calloc( 1, sizeof *p );
    p->ip = t->cand[ci].ip; p->port = t->cand[ci].port; p->sock = s; p->st = 0; p->choked = 1; p->am_choking = 1; p->cand = ci; p->t_conn = p->t_rx = p->t_tx = now_ms();
    t->peers[i] = p; t->cand[ci].connected = 1; t->cand[ci].next_ms = now_ms() + 300000;
}

static void add_candidate( Torrent * t, uint32_t ip, uint16_t port ) {
    int i, free_i = -1;
    if ( !port || !ip || ( g_myip && ip == g_myip ) ) return;                  /* never ourselves */
    for ( i = 0; i < t->ncand; i++ ) if ( t->cand[i].used && t->cand[i].ip == ip && t->cand[i].port == port ) return;
    for ( i = 0; i < t->ncand; i++ ) if ( !t->cand[i].used ) { free_i = i; break; }
    if ( free_i < 0 ) { if ( t->ncand >= MAX_CAND ) return; free_i = t->ncand++; }
    t->cand[free_i].ip = ip; t->cand[free_i].port = port; t->cand[free_i].used = 1; t->cand[free_i].connected = 0; t->cand[free_i].next_ms = 0;
}

/* ------------------------------------------------------------------------------------------------ trackers */
static void parse_peers( Torrent * t, BNode * peers ) {
    if ( !peers ) return;
    if ( peers->t == 's' ) {
        size_t i;
        for ( i = 0; i + 6 <= peers->n; i += 6 ) {
            uint32_t ip; memcpy( &ip, peers->s + i, 4 );
            add_candidate( t, ip, ( uint16_t ) ( ( peers->s[i + 4] << 8 ) | peers->s[i + 5] ) );
        }
    } else if ( peers->t == 'l' ) {
        int i;
        for ( i = 0; i < peers->nk; i++ ) {
            BNode * ip = bget( &peers->k[i], "ip" ), * pt = bget( &peers->k[i], "port" ); char s[64]; struct in_addr a;
            if ( !ip || !pt || ip->t != 's' || pt->t != 'i' || ip->n > 60 ) continue;
            memcpy( s, ip->s, ip->n ); s[ip->n] = 0;
            if ( inet_pton( AF_INET, s, &a ) == 1 ) add_candidate( t, a.s_addr, ( uint16_t ) pt->i );
        }
    }
}

static void url_escape_bytes( const uint8_t * in, int n, char * out ) {
    int i; static const char d[] = "0123456789ABCDEF";
    for ( i = 0; i < n; i++ ) { *out++ = '%'; *out++ = d[in[i] >> 4]; *out++ = d[in[i] & 15]; }
    *out = 0;
}

static void announce_http( Torrent * t, const char * url, uint8_t ih[20], int64_t left ) {
    char ihe[70], pie[70], full[2400]; uint8_t * body = NULL; size_t n = 0; BNode b; const char * sep = strchr( url, '?' ) ? "&" : "?";
    url_escape_bytes( ih, 20, ihe ); url_escape_bytes( g_pid, 20, pie );
    snprintf( full, sizeof full, "%s%sinfo_hash=%s&peer_id=%s&port=%d&uploaded=0&downloaded=0&left=%lld&compact=1&numwant=60", url, sep, ihe, pie, g_xport ? g_xport : g_port, ( long long ) left );
    if ( http_get( full, &body, &n, 1 << 20, 15 ) != 0 ) { tlog( "tracker %s: no answer (failed to connect or error status)", url ); return; }
    if ( bparse( body, body + n, &b, 0 ) && b.t == 'd' ) {
        BNode * peers = bget( &b, "peers" ), * fail = bget( &b, "failure reason" ), * ext = bget( &b, "external ip" ); int before;
        if ( ext && ext->t == 's' && ext->n == 4 ) memcpy( &g_myip, ext->s, 4 );
        LOCK(); before = t->ncand; parse_peers( t, peers ); tlog( "tracker %s: %d new peers%s%.*s", url, t->ncand - before, fail && fail->t == 's' ? ", refused: " : "", fail && fail->t == 's' ? ( int ) fail->n : 0, fail && fail->t == 's' ? ( const char * ) fail->s : "" ); UNLOCK();
    } else tlog( "tracker %s: answer is not a valid reply (%zu bytes)", url, n );
    bfree( &b ); free( body );
}

static void announce_udp( Torrent * t, const char * url, uint8_t ih[20], int64_t left ) {
    char host[256]; int port = 80, k; struct addrinfo hints, * res = NULL; sock_t s; const char * p = url + 6;     /* after udp:// */
    char portstr[16]; uint8_t req[98], resp[2048]; uint32_t tid; int attempt;
    k = 0; while ( *p && *p != ':' && *p != '/' && k < 255 ) host[k++] = *p++; host[k] = 0;
    if ( *p == ':' ) port = atoi( p + 1 );
    snprintf( portstr, sizeof portstr, "%d", port );
    memset( &hints, 0, sizeof hints ); hints.ai_family = AF_INET; hints.ai_socktype = SOCK_DGRAM;
    { int gr = getaddrinfo( host, portstr, &hints, &res ); if ( gr != 0 || !res ) { tlog( "tracker %s: cannot resolve %s (code %d)", url, host, gr ); return; } }
    s = socket( AF_INET, SOCK_DGRAM, 0 );
    if ( s == BAD_SOCK ) { tlog( "tracker %s: cannot create a UDP socket (errno %d)", url, errno ); freeaddrinfo( res ); return; }
    sock_timeout( ( int ) s, 5000, 0 );
    tid = ( uint32_t ) rand() ^ now_ms();
    for ( attempt = 0; attempt < 2; attempt++ ) {
        int r; uint64_t cid; uint32_t act;
        be64w( req, 0x41727101980ULL ); be32w( req + 8, 0 ); be32w( req + 12, tid );
        { int sr = ( int ) sendto( s, ( const char * ) req, 16, 0, res->ai_addr, ( int ) res->ai_addrlen ); if ( sr < 0 ) tlog( "tracker %s: sendto failed (errno %d)", url, errno ); }
        r = ( int ) recv( s, ( char * ) resp, sizeof resp, 0 );
        if ( r < 16 || be32r( resp ) != 0 || be32r( resp + 4 ) != tid ) { tlog( "tracker %s: no connect answer (try %d, recv %d, errno %d)", url, attempt + 1, r, r < 0 ? errno : 0 ); continue; }
        cid = ( ( uint64_t ) be32r( resp + 8 ) << 32 ) | be32r( resp + 12 );
        be64w( req, cid ); be32w( req + 8, 1 ); be32w( req + 12, tid + 1 ); memcpy( req + 16, ih, 20 ); memcpy( req + 36, g_pid, 20 );
        be64w( req + 56, 0 ); be64w( req + 64, ( uint64_t ) left ); be64w( req + 72, 0 ); be32w( req + 80, 2 ); be32w( req + 84, 0 );
        be32w( req + 88, ( uint32_t ) rand() ); be32w( req + 92, 60 ); req[96] = ( uint8_t ) ( ( g_xport ? g_xport : g_port ) >> 8 ); req[97] = ( uint8_t ) ( g_xport ? g_xport : g_port );
        sendto( s, ( const char * ) req, 98, 0, res->ai_addr, ( int ) res->ai_addrlen );
        r = ( int ) recv( s, ( char * ) resp, sizeof resp, 0 );
        act = r >= 8 ? be32r( resp ) : 99;
        if ( r >= 20 && act == 1 && be32r( resp + 4 ) == tid + 1 ) {
            int i, before;
            LOCK(); before = t->ncand;
            for ( i = 20; i + 6 <= r; i += 6 ) { uint32_t ip; memcpy( &ip, resp + i, 4 ); add_candidate( t, ip, ( uint16_t ) ( ( resp[i + 4] << 8 ) | resp[i + 5] ) ); }
            tlog( "tracker %s: %d new peers (seeders %u, leechers %u)", url, t->ncand - before, be32r( resp + 16 ), be32r( resp + 12 ) );
            UNLOCK();
            break;
        }
        tlog( "tracker %s: no announce answer (try %d, recv %d, action %u)", url, attempt + 1, r, ( unsigned ) act );
    }
    CLOSESOCK( s ); freeaddrinfo( res );
}

static int trk_thread( void * arg ) {
    TrkArg a = * ( TrkArg * ) arg; char url[1100]; uint8_t ih[20]; int64_t left; Torrent * t = a.t;
    free( arg );
    LOCK();
    if ( !t->used || t->gen != a.gen || t->dying ) { UNLOCK(); return 0; }
    snprintf( url, sizeof url, "%s", t->trk[a.idx] ); memcpy( ih, t->ih, 20 );
    left = t->state == 1 ? t->total - ( int64_t ) t->nhave * t->plen : 1; if ( left < 1 ) left = 1;
    UNLOCK();
    tlog( "asking %s", url );
    if ( !strncmp( url, "udp://", 6 ) ) announce_udp( t, url, ih, left );
    else if ( !strncmp( url, "http://", 7 ) || !strncmp( url, "https://", 8 ) ) announce_http( t, url, ih, left );
    LOCK();
    if ( t->used && t->gen == a.gen ) { t->trk_busy[a.idx] = 0; t->trk_next[a.idx] = now_ms() + 180000; }
    UNLOCK();
    return 0;
}

static void start_announces( Torrent * t ) {
    int i, pass, busy = 0; uint32_t now = now_ms();
    for ( i = 0; i < t->ntrk; i++ ) if ( t->trk_busy[i] ) busy++;
    for ( pass = 0; pass < 2; pass++ )                               /* the public extra trackers first (they are alive), then the torrent's own */
        for ( i = 0; i < t->ntrk; i++ ) {
            if ( ( pass == 0 ) != ( t->trk_extra[i] != 0 ) ) continue;
            if ( t->trk_busy[i] || ( int32_t ) ( now - t->trk_next[i] ) < 0 ) continue;
            if ( !strncmp( t->trk[i], "udp://", 6 ) || !strncmp( t->trk[i], "http", 4 ) ) {
                TrkArg * a; SDL_Thread * th;
                if ( busy >= 10 ) return;                              /* never more than 10 at a time */
                a = ( TrkArg * ) malloc( sizeof *a );
                a->t = t; a->idx = i; a->gen = t->gen; t->trk_busy[i] = 1; t->trk_next[i] = now + 30000; busy++;
                th = SDL_CreateThread( trk_thread, "trk", a );
                if ( th ) SDL_DetachThread( th ); else { free( a ); t->trk_busy[i] = 0; busy--; }
            } else t->trk_next[i] = now + 3600000;                       /* wss:// and others: not supported */
        }
}

/* ------------------------------------------------------------------------------------------------ per-torrent tick */
static void add_tracker( Torrent * t, const char * u ) {
    int i;
    if ( !*u || t->ntrk >= MAX_TRK ) return;
    for ( i = 0; i < t->ntrk; i++ ) if ( !strcmp( t->trk[i], u ) ) return;
    t->trk[t->ntrk] = strdup( u ); t->trk_busy[t->ntrk] = 0; t->trk_next[t->ntrk] = 0; t->ntrk++;
}

/* ------------------------------------------------------------------------------------------------ extra public trackers */
static const char * g_extra_trk[] = {
    "udp://tracker.opentrackr.org:1337/announce", "udp://explodie.org:6969/announce", "udp://tracker.leechers-paradise.org:6969/announce",
    "udp://tracker.torrent.eu.org:451/announce", "udp://open.stealth.si:80/announce", "udp://exodus.desync.com:6969/announce", NULL };
static void add_extra_trackers( Torrent * t ) {
    int i;
    if ( !g_extra_on ) return;
    for ( i = 0; g_extra_trk[i]; i++ ) { int before = t->ntrk; add_tracker( t, g_extra_trk[i] ); if ( t->ntrk > before ) t->trk_extra[t->ntrk - 1] = 1; }
}

/* ------------------------------------------------------------------------------------------------ DHT (BEP 5, read-only: it only asks, it never answers or announces) */
static int dht_closer( const DNode * a, const DNode * b, const uint8_t * ih ) {       /* is a closer to ih than b? unknown ids count as far */
    int i;
    if ( !a->known ) return 0;
    if ( !b->known ) return 1;
    for ( i = 0; i < 20; i++ ) { int da = a->id[i] ^ ih[i], db = b->id[i] ^ ih[i]; if ( da != db ) return da < db; }
    return 0;
}

static void dht_add_node( Torrent * t, const uint8_t * id, uint32_t ip, uint16_t port, int known ) {
    int i;
    if ( !ip || !port || t->ndn >= DHT_MAXN ) return;
    for ( i = 0; i < t->ndn; i++ ) if ( t->dn[i].ip == ip && t->dn[i].port == port ) { if ( known && !t->dn[i].known ) { memcpy( t->dn[i].id, id, 20 ); t->dn[i].known = 1; } return; }
    memset( &t->dn[t->ndn], 0, sizeof( DNode ) );
    t->dn[t->ndn].ip = ip; t->dn[t->ndn].port = port; t->dn[t->ndn].known = known ? 1 : 0; if ( known ) memcpy( t->dn[t->ndn].id, id, 20 );
    t->ndn++;
}

static void dht_query( Torrent * t, int ti, int ni ) {
    uint8_t pkt[160]; int n = 0; struct sockaddr_in sa; DNode * d = &t->dn[ni];
    memcpy( pkt + n, "d1:ad2:id20:", 12 ); n += 12; memcpy( pkt + n, g_nid, 20 ); n += 20;
    memcpy( pkt + n, "9:info_hash20:", 14 ); n += 14; memcpy( pkt + n, t->ih, 20 ); n += 20;
    memcpy( pkt + n, "e1:q9:get_peers2:roi1e1:t2:", 27 ); n += 27; pkt[n++] = ( uint8_t ) ti; pkt[n++] = ( uint8_t ) ni;
    memcpy( pkt + n, "1:y1:qe", 7 ); n += 7;
    memset( &sa, 0, sizeof sa ); sa.sin_family = AF_INET; sa.sin_addr.s_addr = d->ip; sa.sin_port = htons( d->port );
    sendto( g_dsock, ( const char * ) pkt, n, 0, ( struct sockaddr * ) &sa, sizeof sa );
}

static void dht_tick( Torrent * t, int ti ) {
    uint32_t now = now_ms(); int i, out = 0, best;
    if ( !g_dht_on || g_dsock == BAD_SOCK || t->state == 2 ) return;
    if ( !t->dht_active ) {
        if ( g_nboot == 0 || ( t->dht_t0 && now - t->dht_t0 < 240000 ) ) return;
        t->ndn = 0; t->dht_sent = 0; t->dht_ans = 0; t->dht_found = 0; t->dht_t0 = now; t->dht_active = 1;
        for ( i = 0; i < g_nboot; i++ ) dht_add_node( t, NULL, g_boot_ip[i], g_boot_port[i], 0 );
        tlog( "dht: look-up started with %d bootstrap nodes", t->ndn );
    }
    for ( i = 0; i < t->ndn; i++ ) if ( t->dn[i].st == 1 ) { if ( now - t->dn[i].ts > 2500 ) t->dn[i].st = 3; else out++; }
    while ( out < 8 && t->dht_sent < 250 ) {                       /* always ask the closest node not asked yet */
        best = -1;
        for ( i = 0; i < t->ndn; i++ ) if ( t->dn[i].st == 0 && ( best < 0 || dht_closer( &t->dn[i], &t->dn[best], t->ih ) ) ) best = i;
        if ( best < 0 ) break;
        dht_query( t, ti, best ); t->dn[best].st = 1; t->dn[best].ts = now; out++; t->dht_sent++;
    }
    { int fresh = 0; for ( i = 0; i < t->ndn; i++ ) if ( t->dn[i].st == 0 ) fresh++;
      if ( ( out == 0 && fresh == 0 ) || now - t->dht_t0 > 60000 || t->dht_sent >= 250 ) {
          t->dht_active = 0; tlog( "dht: look-up finished: %d asked, %d answered, %d peers found", t->dht_sent, t->dht_ans, t->dht_found );
      } }
}

static void dht_recv( void ) {
    uint8_t buf[2048]; int k;
    for ( k = 0; k < 64; k++ ) {
        struct sockaddr_in from; socklen_t fl = sizeof from; int n = ( int ) recvfrom( g_dsock, ( char * ) buf, sizeof buf, 0, ( struct sockaddr * ) &from, &fl ); BNode b;
        if ( n <= 0 ) return;
        if ( !bparse( buf, buf + n, &b, 0 ) || b.t != 'd' ) { bfree( &b ); continue; }
        { BNode * y = bget( &b, "y" ), * tt = bget( &b, "t" ), * r = bget( &b, "r" );
          if ( y && y->t == 's' && y->n == 1 && y->s[0] == 'r' && tt && tt->t == 's' && tt->n == 2 && r && r->t == 'd' ) {
              int ti = tt->s[0], ni = tt->s[1];
              if ( ti < T_MAX && g_t[ti].used && g_t[ti].dht_active && ni < g_t[ti].ndn && g_t[ti].dn[ni].ip == from.sin_addr.s_addr && g_t[ti].dn[ni].port == ntohs( from.sin_port ) ) {
                  Torrent * t = &g_t[ti]; BNode * id = bget( r, "id" ), * nodes = bget( r, "nodes" ), * vals = bget( r, "values" ); size_t i;
                  t->dn[ni].st = 2; t->dht_ans++;
                  if ( id && id->t == 's' && id->n == 20 ) { memcpy( t->dn[ni].id, id->s, 20 ); t->dn[ni].known = 1; }
                  if ( nodes && nodes->t == 's' )
                      for ( i = 0; i + 26 <= nodes->n; i += 26 ) { uint32_t ip; memcpy( &ip, nodes->s + i + 20, 4 ); dht_add_node( t, nodes->s + i, ip, ( uint16_t ) ( ( nodes->s[i + 24] << 8 ) | nodes->s[i + 25] ), 1 ); }
                  if ( vals && vals->t == 'l' ) {
                      int j, got = 0;
                      for ( j = 0; j < vals->nk; j++ ) if ( vals->k[j].t == 's' && vals->k[j].n == 6 ) { uint32_t ip; memcpy( &ip, vals->k[j].s, 4 ); add_candidate( t, ip, ( uint16_t ) ( ( vals->k[j].s[4] << 8 ) | vals->k[j].s[5] ) ); got++; }
                      if ( got ) { t->dht_found += got; tlog( "dht: %u.%u.%u.%u gave %d peers", from.sin_addr.s_addr & 255, ( from.sin_addr.s_addr >> 8 ) & 255, ( from.sin_addr.s_addr >> 16 ) & 255, from.sin_addr.s_addr >> 24, got ); }
                  }
              }
          } }
        bfree( &b );
    }
}

static int dht_boot_thread( void * arg ) {
    static const struct { const char * host; int port; } B[] = { { "router.bittorrent.com", 6881 }, { "router.utorrent.com", 6881 }, { "dht.transmissionbt.com", 6881 }, { "dht.libtorrent.org", 25401 } };
    unsigned i; ( void ) arg;
    for ( i = 0; i < sizeof B / sizeof B[0]; i++ ) {
        struct addrinfo hints, * res = NULL; char ps[16]; int gr;
        memset( &hints, 0, sizeof hints ); hints.ai_family = AF_INET; hints.ai_socktype = SOCK_DGRAM; snprintf( ps, sizeof ps, "%d", B[i].port );
        gr = getaddrinfo( B[i].host, ps, &hints, &res );
        if ( gr == 0 && res ) {
            LOCK(); if ( g_nboot < 16 ) { g_boot_ip[g_nboot] = ( ( struct sockaddr_in * ) res->ai_addr )->sin_addr.s_addr; g_boot_port[g_nboot] = ( uint16_t ) B[i].port; g_nboot++; } UNLOCK();
            tlog( "dht: bootstrap %s resolved", B[i].host );
        } else tlog( "dht: bootstrap %s cannot be resolved (code %d)", B[i].host, gr );
        if ( res ) freeaddrinfo( res );
    }
    return 0;
}

static void torrent_free_data( Torrent * t, int del ) {
    int i;
    for ( i = 0; i < MAX_PEERS; i++ ) peer_free( t, i );
    if ( t->fd > 0 ) { CLOSEFD( t->fd ); t->fd = 0; }
    if ( del && t->path[0] ) remove( t->path );
    for ( i = 0; i < t->ntrk; i++ ) free( t->trk[i] );
    for ( i = 0; i < t->nfiles; i++ ) free( t->files[i].path );
    free( t->files ); free( t->hashes ); free( t->have ); free( t->mark ); free( t->md_req ); free( t->bst ); free( t->btime ); free( t->order ); free( t->md ); free( t->md_have );
    { unsigned g = t->gen; memset( t, 0, sizeof *t ); t->gen = g; }
}

static void tor_tick( Torrent * t ) {
    uint32_t now = now_ms(); int i, active = 0, connecting = 0;
    dht_tick( t, ( int ) ( t - g_t ) );
    /* statistics once a second */
    if ( now - t->last_stat >= 1000 ) {
        t->down_rate = ( int ) ( t->downloaded - t->last_dl ); t->up_rate = ( int ) ( t->uploaded - t->last_ul );
        t->last_dl = t->downloaded; t->last_ul = t->uploaded; t->last_stat = now;
    }
    t->up_tokens += g_up_limit / 4; if ( t->up_tokens > g_up_limit ) t->up_tokens = g_up_limit;
    { static uint32_t last_sum; if ( now - last_sum >= 10000 ) { int act = 0, con = 0, k; last_sum = now; for ( k = 0; k < MAX_PEERS; k++ ) if ( t->peers[k] && t->peers[k]->st >= 0 ) { if ( t->peers[k]->st == 2 ) act++; else con++; }
      tlog( "status: state %d, %d peers connected, %d connecting, %d known, %lld KB received", t->state, act, con, t->ncand, ( long long ) ( t->downloaded >> 10 ) ); } }
    /* trackers, when we need more peers */
    for ( i = 0; i < MAX_PEERS; i++ ) if ( t->peers[i] && t->peers[i]->st >= 0 ) { if ( t->peers[i]->st == 2 ) active++; else connecting++; }
    if ( active + connecting < 10 || now - t->last_announce > 120000 ) { start_announces( t ); if ( now - t->last_announce > 120000 ) t->last_announce = now; }
    /* peers */
    for ( i = 0; i < MAX_PEERS; i++ ) {
        Peer * p = t->peers[i];
        if ( !p ) continue;
        if ( p->st < 0 ) { peer_free( t, i ); continue; }
        if ( p->st == 0 && now - p->t_conn > 8000 ) { static int ntm; if ( ntm++ < 20 ) tlog( "peer %u.%u.%u.%u:%d: no answer (timeout)", p->ip & 255, ( p->ip >> 8 ) & 255, ( p->ip >> 16 ) & 255, p->ip >> 24, p->port ); peer_close( t, p ); continue; }
        if ( p->st == 1 && now - p->t_conn > 15000 ) { peer_close( t, p ); continue; }
        if ( p->st == 2 && now - p->t_rx > 90000 ) { peer_close( t, p ); continue; }
        if ( p->st == 2 && p->nreq > 0 && now - p->rt[0] > 25000 ) {          /* the oldest request is not answered: this peer is of no use now */
            drop_requests( t, p ); p->choked = 1; { uint8_t d = 0; (void) d; }
        }
        if ( p->st == 2 && now - p->t_tx > 60000 ) { uint8_t z[4] = { 0, 0, 0, 0 }; send_raw( p, z, 4 ); p->t_tx = now; }
    }
    if ( t->state == 0 ) {
        for ( i = 0; i < MAX_PEERS; i++ ) if ( t->peers[i] && t->peers[i]->st == 2 ) ask_metadata( t, t->peers[i] );
        if ( now - t->t_start > 150000 ) { set_err( t, "could not get the file list (no peers answered)" ); return; }
    }
    if ( t->state == 1 ) {
        /* verify what an earlier run left on disk (a few pieces per tick) */
        for ( i = 0; i < 8 && t->recheck <= t->fp1; i++, t->recheck++ ) {
            int pi = t->recheck, nb = piece_blocks( t, pi ), b, len = piece_len( t, pi ); uint8_t * buf = ( uint8_t * ) malloc( len ); Sha1 s; uint8_t dig[20];
            if ( !buf ) break;
            if ( fpread( t->fd, buf, len, ( int64_t ) pi * t->plen ) == len ) {
                sha1_init( &s ); sha1_update( &s, buf, len ); sha1_final( &s, dig );
                if ( !memcmp( dig, t->hashes + ( size_t ) pi * 20, 20 ) ) { t->have[pi] = 1; t->nhave++; for ( b = 0; b < nb; b++ ) t->bst[( size_t ) pi * t->bpp + b] = 2; }
            }
            free( buf );
        }
        build_order( t );
        for ( i = 0; i < MAX_PEERS; i++ ) {
            Peer * p = t->peers[i];
            if ( !p || p->st != 2 ) continue;
            if ( !p->am_interested ) { send_msg( p, 2, NULL, 0 ); p->am_interested = 1; }
            if ( g_up_limit > 0 && p->peer_interested && p->am_choking ) { send_msg( p, 1, NULL, 0 ); p->am_choking = 0; }
            request_more( t, p );
        }
    }
    /* connect to more peers */
    if ( active + connecting < MAX_PEERS - 4 ) {
        int tries = 0;
        for ( i = 0; i < t->ncand && tries < 6; i++ ) {
            if ( !t->cand[i].used || t->cand[i].connected || ( int32_t ) ( now - t->cand[i].next_ms ) < 0 ) continue;
            peer_connect( t, i ); tries++;
        }
    }
}

/* ------------------------------------------------------------------------------------------------ engine thread */
static void peer_flush( Torrent * t, Peer * p ) {
    while ( p->soff < p->slen ) {
        int n = ( int ) send( p->sock, ( const char * ) p->sbuf + p->soff, ( int ) ( p->slen - p->soff ), MSG_NOSIGNAL );
        if ( n > 0 ) { p->soff += n; p->t_tx = now_ms(); continue; }
        if ( n < 0 ) {
#ifdef _WIN32
            if ( SOCKERR == WSAEWOULDBLOCK ) return;
#else
            if ( errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR ) return;
#endif
        }
        peer_close( t, p ); return;
    }
    p->soff = p->slen = 0;
}

/* ------------------------------------------------------------------------------------------------ incoming peers */
static void set_nonblock( sock_t c ) {
#ifdef _WIN32
    u_long nb = 1; ioctlsocket( c, FIONBIO, &nb );
#else
    fcntl( c, F_SETFL, fcntl( c, F_GETFL, 0 ) | O_NONBLOCK );       /* explicit: on BSD an accepted socket inherits the mode, on Linux it does not */
#endif
}

static void pend_close( int k ) { if ( g_pend[k].used ) { CLOSESOCK( g_pend[k].s ); g_pend[k].used = 0; } }

static void accept_peers( void ) {
    int k;
    for ( k = 0; k < 8; k++ ) {
        struct sockaddr_in from; socklen_t fl = sizeof from; sock_t c = accept( g_psock, ( struct sockaddr * ) &from, &fl ); int j;
        if ( c == BAD_SOCK ) return;
        for ( j = 0; j < 16 && g_pend[j].used; j++ ) ;
        if ( j == 16 ) { CLOSESOCK( c ); continue; }
        set_nonblock( c ); big_buffers( ( int ) c, 1 << 20, 256 << 10 );
        g_pend[j].used = 1; g_pend[j].s = c; g_pend[j].n = 0; g_pend[j].t0 = now_ms(); g_pend[j].ip = from.sin_addr.s_addr; g_pend[j].port = ntohs( from.sin_port );
    }
}

/* the handshake of an incoming connection arrived: find its torrent and let the usual code take over */
static void pend_read( int k ) {
    Pend * q = &g_pend[k]; int r, i, j;
    if ( !q->used ) return;
    r = ( int ) recv( q->s, ( char * ) q->buf + q->n, 68 - q->n, 0 );
    if ( r == 0 ) { pend_close( k ); return; }
    if ( r < 0 ) {
#ifdef _WIN32
        if ( SOCKERR != WSAEWOULDBLOCK ) pend_close( k );
#else
        if ( errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR ) pend_close( k );
#endif
        return;
    }
    q->n += r;
    if ( q->n < 68 ) return;
    if ( q->buf[0] != 19 || memcmp( q->buf + 1, "BitTorrent protocol", 19 ) ) { pend_close( k ); return; }
    for ( i = 0; i < T_MAX; i++ ) {
        Torrent * t = &g_t[i];
        if ( !t->used || t->dying || t->loading || t->state == 2 || memcmp( t->ih, q->buf + 28, 20 ) ) continue;
        for ( j = 0; j < MAX_PEERS && t->peers[j]; j++ ) ;
        if ( j == MAX_PEERS ) break;
        { Peer * p = ( Peer * ) calloc( 1, sizeof *p ); uint8_t h[68];
          p->sock = q->s; p->ip = q->ip; p->port = q->port; p->st = 1; p->choked = 1; p->am_choking = 1; p->cand = -1; p->t_conn = p->t_rx = p->t_tx = now_ms();
          p->rcap = 8192; p->rbuf = ( uint8_t * ) malloc( p->rcap ); memcpy( p->rbuf, q->buf, 68 ); p->rlen = 68;
          h[0] = 19; memcpy( h + 1, "BitTorrent protocol", 19 ); memset( h + 20, 0, 8 ); h[25] = 0x10; memcpy( h + 28, t->ih, 20 ); memcpy( h + 48, g_pid, 20 );
          send_raw( p, h, 68 );
          t->peers[j] = p; q->used = 0;                                    /* the socket now belongs to the peer */
          tlog( "incoming peer %u.%u.%u.%u:%d", p->ip & 255, ( p->ip >> 8 ) & 255, ( p->ip >> 16 ) & 255, p->ip >> 24, p->port );
          peer_input( t, p ); }
        return;
    }
    pend_close( k );
}

/* ------------------------------------------------------------------------------------------------ opening the port in the router (UPnP, then NAT-PMP) */
static char g_gw_host[64], g_ctl_host[64], g_ctl_path[256], g_ctl_type[100]; static int g_ctl_port;

/* a small blocking http exchange (the router is on the local network) */
static int nat_http( const char * host, int port, const char * req, size_t rl, char * resp, size_t cap ) {
    struct addrinfo hints, * res = NULL; char ps[16]; sock_t c; int n = 0; struct pollfd pf; uint32_t t0 = now_ms();
    memset( &hints, 0, sizeof hints ); hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM; snprintf( ps, sizeof ps, "%d", port );
    if ( getaddrinfo( host, ps, &hints, &res ) != 0 || !res ) return -1;
    c = socket( AF_INET, SOCK_STREAM, 0 );
    if ( c == BAD_SOCK ) { freeaddrinfo( res ); return -1; }
    set_nonblock( c );
    if ( connect( c, res->ai_addr, ( int ) res->ai_addrlen ) != 0 ) {
        pf.fd = c; pf.events = POLLOUT; pf.revents = 0;
        if ( poll( &pf, 1, 3000 ) <= 0 ) { CLOSESOCK( c ); freeaddrinfo( res ); return -1; }
        { int err = 0; socklen_t el = sizeof err; getsockopt( c, SOL_SOCKET, SO_ERROR, ( char * ) &err, &el ); if ( err ) { CLOSESOCK( c ); freeaddrinfo( res ); return -1; } }
    }
    freeaddrinfo( res );
#ifndef _WIN32
    fcntl( c, F_SETFL, fcntl( c, F_GETFL, 0 ) & ~O_NONBLOCK );
#endif
    sock_timeout( ( int ) c, 3000, 1 );
    if ( send( c, req, ( int ) rl, MSG_NOSIGNAL ) != ( int ) rl ) { CLOSESOCK( c ); return -1; }
    while ( n < ( int ) cap - 1 && now_ms() - t0 < 6000 ) { int k = ( int ) recv( c, resp + n, ( int ) cap - 1 - n, 0 ); if ( k <= 0 ) break; n += k; }
    resp[n] = 0; CLOSESOCK( c );
    return n;
}

static const char * find_between( const char * s, const char * a, const char * b, char * out, size_t outn ) {
    const char * p = strstr( s, a ), * q;
    if ( !p ) return NULL;
    p += strlen( a ); q = strstr( p, b );
    if ( !q || ( size_t ) ( q - p ) >= outn ) return NULL;
    memcpy( out, p, ( size_t ) ( q - p ) ); out[q - p] = 0;
    return q;
}

static int upnp_soap( const char * action, const char * args, char * resp, size_t cap ) {
    char body[1400], req[2200]; int bl, rl;
    bl = snprintf( body, sizeof body, "<?xml version=\"1.0\"?><s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:%s xmlns:u=\"%s\">%s</u:%s></s:Body></s:Envelope>", action, g_ctl_type, args, action );
    rl = snprintf( req, sizeof req, "POST %s HTTP/1.1\r\nHOST: %s:%d\r\nContent-Type: text/xml; charset=\"utf-8\"\r\nSOAPAction: \"%s#%s\"\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s", g_ctl_path, g_ctl_host, g_ctl_port, g_ctl_type, action, bl, body );
    return nat_http( g_ctl_host, g_ctl_port, req, ( size_t ) rl, resp, cap );
}

static int nat_local_ip( char * out, size_t n ) {
    sock_t s = socket( AF_INET, SOCK_DGRAM, 0 ); struct sockaddr_in to, me; socklen_t l = sizeof me;
    out[0] = 0;
    if ( s == BAD_SOCK ) return 0;
    memset( &to, 0, sizeof to ); to.sin_family = AF_INET; to.sin_port = htons( 53 ); inet_pton( AF_INET, "192.0.2.1", &to.sin_addr );
    if ( connect( s, ( struct sockaddr * ) &to, sizeof to ) == 0 && getsockname( s, ( struct sockaddr * ) &me, &l ) == 0 ) inet_ntop( AF_INET, &me.sin_addr, out, ( socklen_t ) n );
    CLOSESOCK( s );
    return out[0] != 0;
}

/* reads the description of one device and looks for its WAN connection service; 1 = found (the control address is stored) */
static int upnp_try( const char * host, int port, const char * path ) {
    char req[500], * xml = ( char * ) malloc( 65536 ); int rl, n, i; const char * svc = NULL;
    static const char * types[] = { "urn:schemas-upnp-org:service:WANIPConnection:2", "urn:schemas-upnp-org:service:WANIPConnection:1", "urn:schemas-upnp-org:service:WANPPPConnection:1", NULL };
    if ( !xml ) return 0;
    rl = snprintf( req, sizeof req, "GET %s HTTP/1.1\r\nHOST: %s:%d\r\nConnection: close\r\n\r\n", path, host, port );
    n = nat_http( host, port, req, ( size_t ) rl, xml, 65536 );
    if ( n <= 0 ) { free( xml ); return 0; }
    for ( i = 0; types[i] && !svc; i++ ) { const char * q = strstr( xml, types[i] ); if ( q ) { svc = q; snprintf( g_ctl_type, sizeof g_ctl_type, "%s", types[i] ); } }
    if ( !svc || !find_between( svc, "<controlURL>", "</controlURL>", g_ctl_path, sizeof g_ctl_path ) ) { free( xml ); return 0; }
    free( xml );
    if ( !strncmp( g_ctl_path, "http://", 7 ) ) {
        const char * h = g_ctl_path + 7; size_t hl = strcspn( h, ":/" ); char tmp[256];
        memcpy( g_ctl_host, h, hl < 63 ? hl : 63 ); g_ctl_host[hl < 63 ? hl : 63] = 0; h += hl; g_ctl_port = 80;
        if ( *h == ':' ) { g_ctl_port = atoi( h + 1 ); h += strcspn( h, "/" ); }
        snprintf( tmp, sizeof tmp, "%s", *h ? h : "/" ); snprintf( g_ctl_path, sizeof g_ctl_path, "%s", tmp );
    } else {
        snprintf( g_ctl_host, sizeof g_ctl_host, "%s", host ); g_ctl_port = port;
        if ( g_ctl_path[0] != '/' ) { char tmp[256]; snprintf( tmp, sizeof tmp, "/%s", g_ctl_path ); snprintf( g_ctl_path, sizeof g_ctl_path, "%s", tmp ); }
    }
    snprintf( g_gw_host, sizeof g_gw_host, "%s", host );
    return 1;
}

/* SSDP: asks the network for routers; every device that answers is tried until one has a WAN connection service */
static int upnp_discover( void ) {
    static const char * st[] = { "urn:schemas-upnp-org:device:InternetGatewayDevice:1", "urn:schemas-upnp-org:device:InternetGatewayDevice:2", "upnp:rootdevice", NULL };
    sock_t s = socket( AF_INET, SOCK_DGRAM, 0 ); struct sockaddr_in to; char msg[400], buf[2048], loc[300]; int i, k, nloc = 0, tried = 0;
    struct { char host[64]; int port; char path[256]; int done; } L[12];
    if ( s == BAD_SOCK ) return 0;
    set_nonblock( s );
    memset( &to, 0, sizeof to ); to.sin_family = AF_INET; to.sin_port = htons( 1900 ); inet_pton( AF_INET, "239.255.255.250", &to.sin_addr );
    for ( i = 0; st[i]; i++ ) {
        int ml = snprintf( msg, sizeof msg, "M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\nMX: 2\r\nST: %s\r\n\r\n", st[i] );
        uint32_t t0 = now_ms();
        sendto( s, msg, ml, 0, ( struct sockaddr * ) &to, sizeof to );
        while ( now_ms() - t0 < 2200 && nloc < 12 ) {
            struct pollfd pf; int r; char * q; pf.fd = s; pf.events = POLLIN; pf.revents = 0;
            if ( poll( &pf, 1, 300 ) <= 0 ) continue;
            r = ( int ) recv( s, buf, sizeof buf - 1, 0 );
            if ( r <= 0 ) continue;
            buf[r] = 0;
            if ( !find_between( buf, "LOCATION:", "\r\n", loc, sizeof loc ) && !find_between( buf, "Location:", "\r\n", loc, sizeof loc ) && !find_between( buf, "location:", "\r\n", loc, sizeof loc ) ) continue;
            q = loc; while ( *q == ' ' ) q++;
            if ( strncmp( q, "http://", 7 ) ) continue;
            { const char * h = q + 7; size_t hl = strcspn( h, ":/" ); int port = 80, dup = 0; char host[64], path[256];
              if ( hl >= sizeof host ) continue;
              memcpy( host, h, hl ); host[hl] = 0; h += hl;
              if ( *h == ':' ) { port = atoi( h + 1 ); h += strcspn( h, "/" ); }
              snprintf( path, sizeof path, "%s", *h ? h : "/" );
              for ( k = 0; k < nloc; k++ ) if ( !strcmp( L[k].host, host ) && L[k].port == port && !strcmp( L[k].path, path ) ) dup = 1;
              if ( dup ) continue;
              snprintf( L[nloc].host, sizeof L[nloc].host, "%s", host ); L[nloc].port = port; snprintf( L[nloc].path, sizeof L[nloc].path, "%s", path ); L[nloc].done = 0; nloc++; }
        }
        for ( k = 0; k < nloc; k++ ) {                              /* the devices found so far, one by one */
            if ( L[k].done ) continue;
            L[k].done = 1; tried++;
            if ( upnp_try( L[k].host, L[k].port, L[k].path ) ) { CLOSESOCK( s ); tlog( "nat: router found at %s:%d (%d device(s) answered)", L[k].host, L[k].port, nloc ); return 1; }
        }
    }
    CLOSESOCK( s );
    tlog( "nat: %d UPnP device(s) answered, none is a router with a WAN service (UPnP may be off in the router)", nloc );
    return 0;
}

static int upnp_map( int port, const char * local_ip ) {
    char args[700], resp[3000], ext[64]; int lease;
    for ( lease = 7200; lease >= 0; lease -= 7200 ) {                  /* some routers only accept permanent leases */
        snprintf( args, sizeof args, "<NewRemoteHost></NewRemoteHost><NewExternalPort>%d</NewExternalPort><NewProtocol>TCP</NewProtocol><NewInternalPort>%d</NewInternalPort><NewInternalClient>%s</NewInternalClient><NewEnabled>1</NewEnabled><NewPortMappingDescription>BennuGD2 Media Player</NewPortMappingDescription><NewLeaseDuration>%d</NewLeaseDuration>", port, port, local_ip, lease );
        if ( upnp_soap( "AddPortMapping", args, resp, sizeof resp ) > 0 && !strncmp( resp, "HTTP/1.1 200", 12 ) ) {
            if ( upnp_soap( "GetExternalIPAddress", "", resp, sizeof resp ) > 0 && find_between( resp, "<NewExternalIPAddress>", "</NewExternalIPAddress>", ext, sizeof ext ) ) {
                struct in_addr a; if ( inet_pton( AF_INET, ext, &a ) == 1 && a.s_addr ) { g_myip = a.s_addr; tlog( "nat: public address %s", ext ); }
            }
            tlog( "nat: UPnP opened TCP port %d (lease %d s)", port, lease );
            return 1;
        }
        { char code[16] = ""; find_between( resp, "<errorCode>", "</errorCode>", code, sizeof code ); tlog( "nat: UPnP refused the mapping (lease %d, error %s)", lease, code[0] ? code : "?" ); if ( strcmp( code, "725" ) ) break; }
    }
    return 0;
}

static int natpmp_map( int port, const char * gw ) {
    sock_t s = socket( AF_INET, SOCK_DGRAM, 0 ); struct sockaddr_in to; uint8_t req[12], resp[32]; struct pollfd pf; int tries, r;
    if ( s == BAD_SOCK ) return 0;
    set_nonblock( s );
    memset( &to, 0, sizeof to ); to.sin_family = AF_INET; to.sin_port = htons( 5351 ); if ( inet_pton( AF_INET, gw, &to.sin_addr ) != 1 ) { CLOSESOCK( s ); return 0; }
    memset( req, 0, sizeof req ); req[1] = 2; req[4] = ( uint8_t ) ( port >> 8 ); req[5] = ( uint8_t ) port; req[6] = ( uint8_t ) ( port >> 8 ); req[7] = ( uint8_t ) port; be32w( req + 8, 7200 );
    for ( tries = 0; tries < 3; tries++ ) {
        sendto( s, ( const char * ) req, 12, 0, ( struct sockaddr * ) &to, sizeof to );
        pf.fd = s; pf.events = POLLIN; pf.revents = 0;
        if ( poll( &pf, 1, 1000 ) > 0 && ( r = ( int ) recv( s, ( char * ) resp, sizeof resp, 0 ) ) >= 16 && resp[1] == 130 ) {
            CLOSESOCK( s );
            if ( resp[2] == 0 && resp[3] == 0 ) { int ext = ( resp[10] << 8 ) | resp[11]; if ( ext > 0 ) g_xport = ext; tlog( "nat: NAT-PMP opened TCP port %d (external %d)", port, ext ); return 1; }
            tlog( "nat: NAT-PMP refused (code %d)", ( resp[2] << 8 ) | resp[3] ); return 0;
        }
    }
    CLOSESOCK( s ); tlog( "nat: no NAT-PMP answer from %s", gw );
    return 0;
}

static void nat_unmap( void ) {
    char args[300], resp[2000];
    if ( g_nat_ok != 1 || !g_ctl_type[0] ) return;
    snprintf( args, sizeof args, "<NewRemoteHost></NewRemoteHost><NewExternalPort>%d</NewExternalPort><NewProtocol>TCP</NewProtocol>", g_pport );
    upnp_soap( "DeletePortMapping", args, resp, sizeof resp );
    g_nat_ok = 0;
}

static int nat_thread( void * arg ) {
    char ip[64], gw[64]; int n, k; ( void ) arg;
    if ( g_pport <= 0 ) return 0;
    if ( !nat_local_ip( ip, sizeof ip ) ) { tlog( "nat: no local address" ); return 0; }
    for ( k = 0; k < 2 && g_run; k++ ) {                                 /* it is repeated every 30 minutes because the lease is 2 hours */
        g_nat_ok = 0;
        if ( upnp_discover() && upnp_map( g_pport, ip ) ) g_nat_ok = 1;
        else {
            if ( g_gw_host[0] ) snprintf( gw, sizeof gw, "%s", g_gw_host );
            else { snprintf( gw, sizeof gw, "%s", ip ); { char * d = strrchr( gw, '.' ); if ( d ) strcpy( d, ".1" ); } }
            if ( natpmp_map( g_pport, gw ) ) g_nat_ok = 2;
        }
        if ( !g_nat_ok ) { tlog( "nat: the port could not be opened (an open port in the router would let more peers connect)" ); return 0; }
        for ( n = 0; n < 1800 && g_run; n++ ) SDL_Delay( 1000 );
        if ( !g_run ) break;
        k = 0;
    }
    return 0;
}

static int engine_thread( void * arg ) {
    struct pollfd fds[T_MAX * MAX_PEERS + 20]; Torrent * ft[T_MAX * MAX_PEERS + 20]; int fi[T_MAX * MAX_PEERS + 20];
    ( void ) arg;
    LOCK();
    while ( g_run ) {
        int n = 0, i, j, r; uint32_t now;
        if ( g_dsock != BAD_SOCK ) { fds[n].fd = g_dsock; fds[n].events = POLLIN; fds[n].revents = 0; ft[n] = NULL; fi[n] = -1; n++; }
        if ( g_psock != BAD_SOCK ) { fds[n].fd = g_psock; fds[n].events = POLLIN; fds[n].revents = 0; ft[n] = NULL; fi[n] = -2; n++; }
        for ( j = 0; j < 16; j++ ) if ( g_pend[j].used ) {
            if ( now_ms() - g_pend[j].t0 > 10000 ) { pend_close( j ); continue; }
            fds[n].fd = g_pend[j].s; fds[n].events = POLLIN; fds[n].revents = 0; ft[n] = NULL; fi[n] = 100 + j; n++;
        }
        for ( i = 0; i < T_MAX; i++ ) {
            Torrent * t = &g_t[i];
            if ( !t->used ) continue;
            if ( t->dying ) { torrent_free_data( t, t->err[0] == 'D' ); continue; }
            for ( j = 0; j < MAX_PEERS; j++ ) {
                Peer * p = t->peers[j];
                if ( !p || p->st < 0 ) continue;
                fds[n].fd = p->sock; fds[n].revents = 0;
                fds[n].events = POLLIN | ( ( p->st == 0 || p->soff < p->slen ) ? POLLOUT : 0 );
                ft[n] = t; fi[n] = j; n++;
            }
        }
        UNLOCK();
        r = n ? poll( fds, n, 100 ) : ( SDL_Delay( 100 ), 0 );
        LOCK();
        if ( r > 0 ) {
            for ( i = 0; i < n; i++ ) {
                Torrent * t = ft[i]; Peer * p;
                if ( !t ) {
                    int code = fi[i];
                    if ( code == -1 ) { if ( fds[i].revents & POLLIN ) dht_recv(); }
                    else if ( code == -2 ) { if ( fds[i].revents & POLLIN ) accept_peers(); }
                    else if ( g_pend[code - 100].used && g_pend[code - 100].s == fds[i].fd && ( fds[i].revents & ( POLLIN | POLLERR | POLLHUP ) ) ) pend_read( code - 100 );
                    continue;
                }
                p = t->peers[fi[i]];
                if ( !t->used || !p || p->st < 0 || p->sock != fds[i].fd ) continue;
                if ( fds[i].revents & ( POLLERR | POLLHUP | POLLNVAL ) && !( fds[i].revents & POLLIN ) ) { peer_close( t, p ); continue; }
                if ( p->st == 0 && ( fds[i].revents & POLLOUT ) ) {
                    int err = 0; socklen_t el = sizeof err;
                    getsockopt( p->sock, SOL_SOCKET, SO_ERROR, ( char * ) &err, &el );
                    if ( err ) { static int nlog; if ( nlog++ < 20 ) tlog( "peer %u.%u.%u.%u:%d: connect failed (errno %d)", p->ip & 255, ( p->ip >> 8 ) & 255, ( p->ip >> 16 ) & 255, p->ip >> 24, p->port, err ); peer_close( t, p ); continue; }
                    { uint8_t h[68]; h[0] = 19; memcpy( h + 1, "BitTorrent protocol", 19 ); memset( h + 20, 0, 8 ); h[25] = 0x10; memcpy( h + 28, t->ih, 20 ); memcpy( h + 48, g_pid, 20 );
                      p->st = 1; p->t_conn = now_ms(); send_raw( p, h, 68 ); }
                }
                if ( p->st >= 1 && ( fds[i].revents & POLLIN ) ) {
                    if ( p->rcap - p->rlen < 4096 ) {
                        size_t nc = p->rcap ? p->rcap * 2 : 8192; uint8_t * nb = ( uint8_t * ) realloc( p->rbuf, nc );
                        if ( !nb ) { peer_close( t, p ); continue; }
                        p->rbuf = nb; p->rcap = nc;
                    }
                    { int k = ( int ) recv( p->sock, ( char * ) p->rbuf + p->rlen, ( int ) ( p->rcap - p->rlen ), 0 );
                      if ( k > 0 ) { p->rlen += k; p->t_rx = now_ms(); peer_input( t, p ); }
                      else if ( k == 0 ) { peer_close( t, p ); continue; }
                      else {
#ifdef _WIN32
                          if ( SOCKERR != WSAEWOULDBLOCK ) { peer_close( t, p ); continue; }
#else
                          if ( errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR ) { peer_close( t, p ); continue; }
#endif
                      } }
                }
                if ( p->st >= 1 && p->soff < p->slen ) peer_flush( t, p );
            }
        }
        now = now_ms();
        for ( i = 0; i < T_MAX; i++ ) {
            Torrent * t = &g_t[i];
            if ( !t->used || t->dying || t->loading ) continue;
            if ( now - t->last_tick >= 250 ) {
                t->last_tick = now; tor_tick( t );
                for ( j = 0; j < MAX_PEERS; j++ ) if ( t->peers[j] && t->peers[j]->st >= 1 && t->peers[j]->soff < t->peers[j]->slen ) peer_flush( t, t->peers[j] );
            }
        }
    }
    UNLOCK();
    return 0;
}

/* ------------------------------------------------------------------------------------------------ adding torrents */
static void parse_magnet( Torrent * t, const char * src ) {
    char * s = strdup( src + 8 ), * tok, * save = NULL;
    for ( tok = strtok_r( s, "&", &save ); tok; tok = strtok_r( NULL, "&", &save ) ) {
        if ( !strncmp( tok, "xt=urn:btih:", 12 ) ) {
            const char * h = tok + 12; size_t n = strlen( h ); int i;
            if ( n == 40 ) { for ( i = 0; i < 20; i++ ) t->ih[i] = ( uint8_t ) ( hexv( h[i * 2] ) * 16 + hexv( h[i * 2 + 1] ) ); }
            else if ( n == 32 ) b32_decode( h, t->ih );
        } else if ( !strncmp( tok, "dn=", 3 ) ) { url_decode( tok + 3 ); snprintf( t->name, sizeof t->name, "%s", tok + 3 ); }
        else if ( !strncmp( tok, "tr=", 3 ) ) { url_decode( tok + 3 ); add_tracker( t, tok + 3 ); }
    }
    free( s );
}

/* loads a .torrent (file or address) on its own thread, then the torrent becomes ready */
static int loader_thread( void * arg ) {
    int h = ( int ) ( intptr_t ) arg; Torrent * t = &g_t[h - 1]; uint8_t * data = NULL; size_t n = 0; char src[2048]; unsigned gen;
    LOCK(); snprintf( src, sizeof src, "%s", t->src ); gen = t->gen; UNLOCK();
    if ( !strncmp( src, "http://", 7 ) || !strncmp( src, "https://", 8 ) ) {
        if ( http_get( src, &data, &n, 16 << 20, 30 ) != 0 ) data = NULL;
    } else {
        FILE * f = fopen( src, "rb" );
        if ( f ) { fseek( f, 0, SEEK_END ); long sz = ftell( f ); fseek( f, 0, SEEK_SET ); if ( sz > 0 && sz < ( 16 << 20 ) ) { data = ( uint8_t * ) malloc( sz ); if ( data && fread( data, 1, sz, f ) == ( size_t ) sz ) n = sz; else { free( data ); data = NULL; } } fclose( f ); }
    }
    LOCK();
    if ( !t->used || t->gen != gen || t->dying ) { UNLOCK(); free( data ); return 0; }
    if ( data ) {
        BNode b;
        if ( bparse( data, data + n, &b, 0 ) && b.t == 'd' ) {
            BNode * info = bget( &b, "info" ), * an = bget( &b, "announce" ), * al = bget( &b, "announce-list" );
            if ( info && info->t == 'd' ) {
                Sha1 s; sha1_init( &s ); sha1_update( &s, info->raw, info->rawn ); sha1_final( &s, t->ih );
                if ( an && an->t == 's' && an->n < 1000 ) { char u[1024]; memcpy( u, an->s, an->n ); u[an->n] = 0; add_tracker( t, u ); }
                if ( al && al->t == 'l' ) {
                    int i, j;
                    for ( i = 0; i < al->nk; i++ ) if ( al->k[i].t == 'l' ) for ( j = 0; j < al->k[i].nk; j++ ) {
                        BNode * u = &al->k[i].k[j];
                        if ( u->t == 's' && u->n < 1000 ) { char x[1024]; memcpy( x, u->s, u->n ); x[u->n] = 0; add_tracker( t, x ); }
                    }
                }
                add_extra_trackers( t );
                set_info( t, info->raw, info->rawn );
            } else set_err( t, "not a torrent file" );
        } else set_err( t, "not a torrent file" );
        bfree( &b );
    } else set_err( t, "could not read the torrent file" );
    t->loading = 0;
    UNLOCK();
    free( data );
    return 0;
}

int torrent_add( const char * src ) {
    int i, h = -1; Torrent * t;
    if ( !g_mu || !src || !*src ) return -2;
    LOCK();
    for ( i = 0; i < T_MAX; i++ ) if ( !g_t[i].used ) { h = i + 1; break; }
    if ( h < 0 ) { UNLOCK(); return -1; }
    t = &g_t[h - 1]; { unsigned g = t->gen; memset( t, 0, sizeof *t ); t->gen = g; }
    t->used = 1; t->h = h; t->gen = ++g_gen_counter; t->state = 0; t->fd = 0; t->t_start = now_ms();
    snprintf( t->src, sizeof t->src, "%s", src );
    tlog( "add: %s", !strncmp( src, "magnet:?", 8 ) ? "magnet" : ( !strncmp( src, "http", 4 ) ? "torrent address" : "torrent file" ) );
    if ( !strncmp( src, "magnet:?", 8 ) ) {
        parse_magnet( t, src );
        add_extra_trackers( t );
        { int z = 1; for ( i = 0; i < 20; i++ ) if ( t->ih[i] ) z = 0; if ( z ) { t->used = 0; UNLOCK(); return -2; } }
        if ( !t->ntrk && !g_dht_on ) set_err( t, "this magnet link has no trackers (turn on the DHT or the extra trackers in the settings)" );
    } else {
        SDL_Thread * th;
        t->loading = 1;
        th = SDL_CreateThread( loader_thread, "torload", ( void * ) ( intptr_t ) h );
        if ( !th ) { t->used = 0; UNLOCK(); return -2; }
        SDL_DetachThread( th );
    }
    UNLOCK();
    return h;
}

void torrent_remove( int h, int delete_data ) {
    Torrent * t;
    if ( h < 1 || h > T_MAX ) return;
    t = &g_t[h - 1];
    LOCK();
    if ( t->used ) { t->dying = 1; if ( delete_data ) snprintf( t->err, sizeof t->err, "D" ); }
    UNLOCK();
}

/* ------------------------------------------------------------------------------------------------ queries */
#define GET( h ) Torrent * t = ( ( h ) >= 1 && ( h ) <= T_MAX && g_t[( h ) - 1].used ) ? &g_t[( h ) - 1] : NULL
int torrent_state( int h ) { GET( h ); int r = 2; if ( !t ) return 2; LOCK(); r = t->dying ? 2 : t->state; UNLOCK(); return r; }
const char * torrent_error( int h ) { GET( h ); return t ? t->err : "no torrent"; }
const char * torrent_name( int h ) { GET( h ); return t ? t->name : ""; }
int torrent_file_count( int h ) { GET( h ); return t && t->state == 1 ? t->nfiles : 0; }
const char * torrent_file_name( int h, int i ) { GET( h ); return t && t->state == 1 && i >= 0 && i < t->nfiles ? t->files[i].path : ""; }
int64_t torrent_file_size( int h, int i ) { GET( h ); return t && t->state == 1 && i >= 0 && i < t->nfiles ? t->files[i].len : 0; }
int torrent_main_file( int h ) {
    GET( h ); int i, best = -1; int64_t bl = -1;
    if ( !t || t->state != 1 ) return -1;
    for ( i = 0; i < t->nfiles; i++ ) if ( is_media_name( t->files[i].path ) && t->files[i].len > bl ) { best = i; bl = t->files[i].len; }
    return best;
}
int torrent_select( int h, int i ) {
    GET( h );
    if ( !t || t->state != 1 || i < 0 || i >= t->nfiles ) return -1;
    LOCK(); select_range( t, i ); t->reader_ms = 0; t->reader_abs = t->sel_off; UNLOCK();
    return 0;
}
int torrent_url( int h, int i, char * out, int outlen ) {
    GET( h ); const char * nm, * sl;
    if ( !t || t->state != 1 || i < 0 || i >= t->nfiles || g_port <= 0 ) return -1;
    nm = t->files[i].path; sl = strrchr( nm, '/' ); if ( sl ) nm = sl + 1;
    { int k = snprintf( out, outlen, "http://127.0.0.1:%d/%d/%d/", g_port, h, i ); const char * c;     /* the name goes percent-encoded */
      for ( c = nm; *c && k < outlen - 4; c++ ) {
          if ( ( *c >= '0' && *c <= '9' ) || ( *c >= 'a' && *c <= 'z' ) || ( *c >= 'A' && *c <= 'Z' ) || *c == '.' || *c == '-' || *c == '_' ) out[k++] = *c;
          else k += snprintf( out + k, outlen - k, "%%%02X", ( unsigned char ) *c );
      }
      out[k] = 0; }
    return 0;
}
int torrent_peers( int h ) { GET( h ); int i, n = 0; if ( !t ) return 0; LOCK(); for ( i = 0; i < MAX_PEERS; i++ ) if ( t->peers[i] && t->peers[i]->st == 2 ) n++; UNLOCK(); return n; }
int torrent_down_rate( int h ) { GET( h ); return t ? t->down_rate : 0; }
int torrent_up_rate( int h ) { GET( h ); return t ? t->up_rate : 0; }
int64_t torrent_downloaded( int h ) { GET( h ); return t ? t->downloaded : 0; }
int torrent_file_percent( int h ) {
    GET( h ); int i, n = 0;
    if ( !t || t->state != 1 ) return 0;
    LOCK(); for ( i = t->fp0; i <= t->fp1; i++ ) if ( t->have[i] ) n++; i = ( int ) ( ( int64_t ) n * 100 / ( t->fp1 - t->fp0 + 1 ) ); UNLOCK();
    return i;
}
int64_t torrent_buffered( int h ) {
    GET( h ); int p; int64_t n = 0, abs;
    if ( !t || t->state != 1 ) return 0;
    LOCK();
    abs = t->reader_abs < t->sel_off ? t->sel_off : t->reader_abs;
    for ( p = ( int ) ( abs / t->plen ); p <= t->fp1 && t->have[p]; p++ ) n += t->plen;
    n -= abs - ( int64_t ) ( abs / t->plen ) * t->plen;
    UNLOCK();
    return n < 0 ? 0 : n;
}
/* how much of what a player needs to start (the first and the last 3 MB of the file) is on disk, 0..100 */
int torrent_start_percent( int h ) {
    GET( h ); int i, hp, tp, have = 0, all = 0;
    if ( !t || t->state != 1 ) return 0;
    LOCK();
    hp = ( int ) ( ( t->sel_off + HEAD_BYTES - 1 ) / t->plen ); tp = ( int ) ( ( t->sel_off + t->sel_len - HEAD_BYTES ) / t->plen );
    if ( hp > t->fp1 ) hp = t->fp1;
    if ( tp < t->fp0 ) tp = t->fp0;
    for ( i = t->fp0; i <= hp; i++ ) { all++; if ( t->have[i] ) have++; }
    if ( tp > hp ) for ( i = tp; i <= t->fp1; i++ ) { all++; if ( t->have[i] ) have++; }
    UNLOCK();
    return all ? have * 100 / all : 100;
}
void torrent_set_upload_limit( int b ) { g_up_limit = b < 0 ? 0 : b; }
void torrent_set_ahead( int mb ) { g_ahead = ( int64_t ) ( mb < 64 ? 64 : ( mb > 8192 ? 8192 : mb ) ) << 20; }
void torrent_set_options( int extra_trackers, int dht ) { g_extra_on = extra_trackers != 0; g_dht_on = dht != 0; }

/* ------------------------------------------------------------------------------------------------ local http server */
static const char * mime_of( const char * nm ) {
    const char * e = strrchr( nm, '.' );
    if ( !e ) return "application/octet-stream";
    if ( !strcasecmp( e, ".mp4" ) || !strcasecmp( e, ".m4v" ) ) return "video/mp4";
    if ( !strcasecmp( e, ".mkv" ) ) return "video/x-matroska";
    if ( !strcasecmp( e, ".webm" ) ) return "video/webm";
    if ( !strcasecmp( e, ".avi" ) ) return "video/x-msvideo";
    if ( !strcasecmp( e, ".mp3" ) ) return "audio/mpeg";
    if ( !strcasecmp( e, ".flac" ) ) return "audio/flac";
    return "application/octet-stream";
}

static int send_all( sock_t s, const char * b, size_t n ) {
    while ( n ) { int k = ( int ) send( s, b, ( int ) n, MSG_NOSIGNAL ); if ( k <= 0 ) return -1; b += k; n -= k; }
    return 0;
}

static int conn_thread( void * arg ) {
    sock_t c = ( sock_t ) ( intptr_t ) arg; char req[4096]; int n = 0, h, fi; long long a = 0, b = -1; char * p;
    Torrent * t; TFile * f; int64_t size, pos, end, sent = 0, foff; uint8_t * buf; int partial, head; unsigned gen; char mime[64];
    { int v60 = 60000;
#ifdef _WIN32
      DWORD dv = ( DWORD ) v60; setsockopt( c, SOL_SOCKET, SO_SNDTIMEO, ( const char * ) &dv, sizeof dv );
#else
      struct timeval tv; tv.tv_sec = v60 / 1000; tv.tv_usec = 0; setsockopt( c, SOL_SOCKET, SO_SNDTIMEO, ( const char * ) &tv, sizeof tv );
#endif
    } big_buffers( ( int ) c, 64 << 10, 1 << 20 );
    while ( n < ( int ) sizeof req - 1 ) {
        int k = ( int ) recv( c, req + n, sizeof req - 1 - n, 0 );
        if ( k <= 0 ) { CLOSESOCK( c ); return 0; }
        n += k; req[n] = 0;
        if ( strstr( req, "\r\n\r\n" ) ) break;
    }
    head = !strncmp( req, "HEAD ", 5 );
    if ( strncmp( req, "GET /", 5 ) && !head ) { send_all( c, "HTTP/1.1 405 Method Not Allowed\r\nConnection: close\r\n\r\n", 55 ); CLOSESOCK( c ); return 0; }
    p = req + ( head ? 6 : 5 );
    h = atoi( p ); p = strchr( p, '/' ); fi = p ? atoi( p + 1 ) : -1;
    p = strstr( req, "\r\nRange:" ); if ( !p ) p = strstr( req, "\r\nrange:" );
    partial = 0;
    if ( p ) { p = strstr( p, "bytes=" ); if ( p ) { partial = 1; a = atoll( p + 6 ); p = strchr( p, '-' ); if ( p && p[1] >= '0' && p[1] <= '9' ) b = atoll( p + 1 ); } }
    if ( h < 1 || h > T_MAX ) { send_all( c, "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n", 45 ); CLOSESOCK( c ); return 0; }
    t = &g_t[h - 1];
    LOCK();
    if ( !t->used || t->dying || t->state != 1 || fi < 0 || fi >= t->nfiles ) { UNLOCK(); send_all( c, "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n", 45 ); CLOSESOCK( c ); return 0; }
    f = &t->files[fi]; size = f->len; foff = f->off; gen = t->gen; snprintf( mime, sizeof mime, "%s", mime_of( f->path ) );
    if ( fi != t->sel ) { select_range( t, fi ); t->reader_ms = 0; }
    UNLOCK();
    if ( a >= size ) { send_all( c, "HTTP/1.1 416 Range Not Satisfiable\r\nConnection: close\r\n\r\n", 58 ); CLOSESOCK( c ); return 0; }
    if ( b < 0 || b >= size ) b = size - 1;
    pos = a; end = b + 1;
    { char hd[400]; int k;
      if ( partial ) k = snprintf( hd, sizeof hd, "HTTP/1.1 206 Partial Content\r\nContent-Type: %s\r\nAccept-Ranges: bytes\r\nContent-Range: bytes %lld-%lld/%lld\r\nContent-Length: %lld\r\nConnection: close\r\n\r\n", mime, ( long long ) pos, ( long long ) b, ( long long ) size, ( long long ) ( end - pos ) );
      else k = snprintf( hd, sizeof hd, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nAccept-Ranges: bytes\r\nContent-Length: %lld\r\nConnection: close\r\n\r\n", mime, ( long long ) size );
      if ( send_all( c, hd, k ) != 0 || head ) { CLOSESOCK( c ); return 0; } }
    buf = ( uint8_t * ) malloc( 262144 );
    while ( buf && pos < end ) {
        int64_t abs = foff + pos, chunk = end - pos > 262144 ? 262144 : end - pos; int ready, p0, p1, i, stalled = 0, lasth;
        LOCK();
        for ( ;; ) {
            if ( !t->used || t->gen != gen || t->dying || !g_run ) { ready = -1; break; }
            t->reader_abs = abs; t->reader_ms = now_ms();
            p0 = ( int ) ( abs / t->plen ); p1 = ( int ) ( ( abs + chunk - 1 ) / t->plen ); ready = 1;
            for ( i = p0; i <= p1; i++ ) if ( !t->have[i] ) { ready = 0; break; }
            if ( ready ) break;
            lasth = t->nhave;
            SDL_CondWaitTimeout( g_cv, g_mu, 500 );
            if ( t->nhave == lasth ) { if ( ++stalled > 240 ) { ready = -1; break; } } else stalled = 0;        /* two minutes without a single new piece */
        }
        UNLOCK();
        if ( ready != 1 ) break;
        if ( fpread( t->fd, buf, ( size_t ) chunk, abs ) != chunk ) break;
        if ( send_all( c, ( const char * ) buf, ( size_t ) chunk ) != 0 ) break;
        pos += chunk; sent += chunk;
    }
    free( buf ); CLOSESOCK( c );
    return 0;
}

static int server_thread( void * arg ) {
    ( void ) arg;
    while ( g_run ) {
        struct pollfd pf; pf.fd = g_lsock; pf.events = POLLIN;
        if ( poll( &pf, 1, 300 ) > 0 ) {
            sock_t c = accept( g_lsock, NULL, NULL );
            if ( c != BAD_SOCK ) { SDL_Thread * th = SDL_CreateThread( conn_thread, "torhttp", ( void * ) ( intptr_t ) c ); if ( th ) SDL_DetachThread( th ); else CLOSESOCK( c ); }
        }
    }
    return 0;
}

/* ------------------------------------------------------------------------------------------------ init / shutdown */
int torrent_init( const char * cache_dir ) {
    struct sockaddr_in sa; socklen_t sl = sizeof sa; int i, one = 1;
    if ( g_run ) return 0;
#ifdef _WIN32
    { WSADATA w; WSAStartup( MAKEWORD( 2, 2 ), &w ); }
#endif
    snprintf( g_cache, sizeof g_cache, "%s", cache_dir && *cache_dir ? cache_dir : "." );
#ifdef _WIN32
    _mkdir( g_cache );
#else
    mkdir( g_cache, 0777 );
#endif
    snprintf( g_logpath, sizeof g_logpath, "%s/torrent.log", g_cache );
    tlog( "---- torrent engine started" );
    srand( ( unsigned ) time( NULL ) ^ now_ms() );
    memcpy( g_pid, "-BM0100-", 8 ); for ( i = 8; i < 20; i++ ) g_pid[i] = ( uint8_t ) ( 'a' + rand() % 26 );
    g_mu = SDL_CreateMutex(); g_cv = SDL_CreateCond();
    g_lsock = socket( AF_INET, SOCK_STREAM, 0 );
    if ( g_lsock == BAD_SOCK ) return -1;
    setsockopt( g_lsock, SOL_SOCKET, SO_REUSEADDR, ( const char * ) &one, sizeof one );
    memset( &sa, 0, sizeof sa ); sa.sin_family = AF_INET; sa.sin_addr.s_addr = htonl( INADDR_LOOPBACK ); sa.sin_port = 0;
    if ( bind( g_lsock, ( struct sockaddr * ) &sa, sizeof sa ) != 0 || listen( g_lsock, 8 ) != 0 ) { CLOSESOCK( g_lsock ); g_lsock = BAD_SOCK; return -1; }
    getsockname( g_lsock, ( struct sockaddr * ) &sa, &sl ); g_port = ntohs( sa.sin_port ); tlog( "local server on 127.0.0.1:%d", g_port );
    g_run = 1;
    { struct sockaddr_in da; sock_t ds = socket( AF_INET, SOCK_DGRAM, 0 );
      for ( i = 0; i < 20; i++ ) g_nid[i] = ( uint8_t ) rand();
      if ( ds != BAD_SOCK ) {
#ifdef _WIN32
          u_long nb = 1; ioctlsocket( ds, FIONBIO, &nb );
#else
          fcntl( ds, F_SETFL, fcntl( ds, F_GETFL, 0 ) | O_NONBLOCK );
#endif
          big_buffers( ( int ) ds, 512 << 10, 64 << 10 );
          memset( &da, 0, sizeof da ); da.sin_family = AF_INET; da.sin_addr.s_addr = htonl( INADDR_ANY ); da.sin_port = 0;
          if ( bind( ds, ( struct sockaddr * ) &da, sizeof da ) == 0 ) { g_dsock = ds; tlog( "dht: UDP socket ready" ); SDL_Thread * bt = SDL_CreateThread( dht_boot_thread, "dhtboot", NULL ); if ( bt ) SDL_DetachThread( bt ); }
          else { tlog( "dht: cannot bind a UDP socket (errno %d)", errno ); CLOSESOCK( ds ); }
      } else tlog( "dht: cannot create a UDP socket (errno %d)", errno ); }
    { sock_t ps = socket( AF_INET, SOCK_STREAM, 0 ); struct sockaddr_in pa; int one2 = 1, bp = 0, q;
      if ( ps != BAD_SOCK ) {
          setsockopt( ps, SOL_SOCKET, SO_REUSEADDR, ( const char * ) &one2, sizeof one2 ); set_nonblock( ps );
          for ( q = 0; q < 21 && !bp; q++ ) {
              memset( &pa, 0, sizeof pa ); pa.sin_family = AF_INET; pa.sin_addr.s_addr = htonl( INADDR_ANY ); pa.sin_port = htons( ( unsigned short ) ( q < 20 ? 6881 + q : 0 ) );
              if ( bind( ps, ( struct sockaddr * ) &pa, sizeof pa ) == 0 ) { socklen_t pl = sizeof pa; getsockname( ps, ( struct sockaddr * ) &pa, &pl ); bp = ntohs( pa.sin_port ); }
          }
          if ( bp && listen( ps, 16 ) == 0 ) { g_psock = ps; g_pport = bp; g_xport = bp; tlog( "peer port %d is listening", bp ); }
          else { tlog( "cannot listen for incoming peers (errno %d)", errno ); CLOSESOCK( ps ); }
      } }
    g_eng_th = SDL_CreateThread( engine_thread, "torengine", NULL );
    if ( g_psock != BAD_SOCK ) { SDL_Thread * nt = SDL_CreateThread( nat_thread, "nat", NULL ); if ( nt ) SDL_DetachThread( nt ); }
    g_srv_th = SDL_CreateThread( server_thread, "torserver", NULL );
    return 0;
}

void torrent_shutdown( void ) {
    int i;
    if ( !g_run ) return;
    g_run = 0;
    LOCK(); SDL_CondBroadcast( g_cv ); UNLOCK();
    SDL_WaitThread( g_eng_th, NULL ); SDL_WaitThread( g_srv_th, NULL );
    for ( i = 0; i < T_MAX; i++ ) if ( g_t[i].used ) torrent_free_data( &g_t[i], 0 );
    if ( g_lsock != BAD_SOCK ) CLOSESOCK( g_lsock );
    nat_unmap();
    if ( g_dsock != BAD_SOCK ) CLOSESOCK( g_dsock );
    if ( g_psock != BAD_SOCK ) CLOSESOCK( g_psock );
    { int k; for ( k = 0; k < 16; k++ ) pend_close( k ); }
    g_lsock = BAD_SOCK; g_dsock = BAD_SOCK; g_psock = BAD_SOCK; g_port = 0; g_pport = 0; g_xport = 0;
}
