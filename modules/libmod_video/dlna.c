/*
 *  dlna.c - browse UPnP / DLNA media servers, see dlna.h
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <SDL.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define poll WSAPoll
#define CLOSESOCK closesocket
typedef SOCKET sock_t;
#define BAD_SOCK INVALID_SOCKET
#else
#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#define CLOSESOCK close
typedef int sock_t;
#define BAD_SOCK (-1)
#endif

#include "dlna.h"

/* case-insensitive helpers (strcasestr / strncasecmp are not everywhere) */
static int ci_ncmp( const char * a, const char * b, size_t n ) {
    for ( ; n; n--, a++, b++ ) { int x = *a, y = *b; if ( x >= 'A' && x <= 'Z' ) x += 32; if ( y >= 'A' && y <= 'Z' ) y += 32; if ( x != y ) return x - y; if ( !x ) return 0; }
    return 0;
}
static const char * ci_str( const char * h, const char * nd ) {
    size_t l = strlen( nd );
    for ( ; *h; h++ ) if ( !ci_ncmp( h, nd, l ) ) return h;
    return NULL;
}

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#define MAX_SRV 16
#define MAX_BODY ( 6 * 1024 * 1024 )

typedef struct { char name[128]; char host[64]; int port; char ctrl[300]; } Srv;

static SDL_mutex * g_mu;
static Srv g_srv[MAX_SRV];
static int g_nsrv, g_scan, g_br;                 /* g_scan: 0/1/2, g_br: 0 idle, 1 running, 2 done, 3 failed */
static char * g_out; static int g_total; static char g_err[200];
static int g_started;

typedef struct { int srv; char id[256]; int start, count; } BrowseArg;

static uint32_t now_ms( void ) { return SDL_GetTicks(); }

static void set_nonblock( sock_t s ) {
#ifdef _WIN32
    u_long one = 1; ioctlsocket( s, FIONBIO, &one );
#else
    fcntl( s, F_SETFL, fcntl( s, F_GETFL, 0 ) | O_NONBLOCK );
#endif
}

static void set_block( sock_t s ) {
#ifdef _WIN32
    u_long zero = 0; ioctlsocket( s, FIONBIO, &zero );
#else
    fcntl( s, F_SETFL, fcntl( s, F_GETFL, 0 ) & ~O_NONBLOCK );
#endif
}

static void set_error( const char * m ) { SDL_LockMutex( g_mu ); snprintf( g_err, sizeof g_err, "%s", m ); SDL_UnlockMutex( g_mu ); }

/* ---- a small HTTP/1.1 client (plain http, the local network): returns the status, the body is malloc'd and ends with a 0 ---- */
static int http_req( const char * host, int port, const char * method, const char * path, const char * extra, const char * body, char ** out ) {
    sock_t s; struct sockaddr_in sa; char req[2048], * buf; int n, blen = body ? ( int ) strlen( body ) : 0, status = -1, cap = 65536, len = 0;
    int hdr_end = -1, chunked = 0, clen = -1; uint32_t t0;
    *out = NULL;
    memset( &sa, 0, sizeof sa ); sa.sin_family = AF_INET; sa.sin_port = htons( ( unsigned short ) port );
    if ( inet_pton( AF_INET, host, &sa.sin_addr ) != 1 ) {
        struct addrinfo hints, * res = NULL; memset( &hints, 0, sizeof hints ); hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
        if ( getaddrinfo( host, NULL, &hints, &res ) != 0 || !res ) return -1;
        sa.sin_addr = ( ( struct sockaddr_in * ) res->ai_addr )->sin_addr; freeaddrinfo( res );
    }
    s = socket( AF_INET, SOCK_STREAM, 0 ); if ( s == BAD_SOCK ) return -1;
    set_nonblock( s );
    if ( connect( s, ( struct sockaddr * ) &sa, sizeof sa ) != 0 ) {
        struct pollfd pf; pf.fd = s; pf.events = POLLOUT; pf.revents = 0;
        if ( poll( &pf, 1, 5000 ) <= 0 ) { CLOSESOCK( s ); return -1; }
        { int e = 0; socklen_t el = sizeof e; getsockopt( s, SOL_SOCKET, SO_ERROR, ( char * ) &e, &el ); if ( e ) { CLOSESOCK( s ); return -1; } }
    }
    set_block( s );
#ifdef _WIN32
    { DWORD dv = 8000; setsockopt( s, SOL_SOCKET, SO_RCVTIMEO, ( const char * ) &dv, sizeof dv ); setsockopt( s, SOL_SOCKET, SO_SNDTIMEO, ( const char * ) &dv, sizeof dv ); }
#else
    { struct timeval tv; tv.tv_sec = 8; tv.tv_usec = 0; setsockopt( s, SOL_SOCKET, SO_RCVTIMEO, ( const char * ) &tv, sizeof tv ); setsockopt( s, SOL_SOCKET, SO_SNDTIMEO, ( const char * ) &tv, sizeof tv ); }
#endif
    n = snprintf( req, sizeof req, "%s %s HTTP/1.1\r\nHost: %s:%d\r\nUser-Agent: BennuGD2-MediaPlayer/1.0 UPnP/1.0\r\nConnection: close\r\n%sContent-Length: %d\r\n\r\n", method, path, host, port, extra ? extra : "", blen );
    if ( n >= ( int ) sizeof req ) { CLOSESOCK( s ); return -1; }
    if ( send( s, req, n, MSG_NOSIGNAL ) != n || ( blen && send( s, body, blen, MSG_NOSIGNAL ) != blen ) ) { CLOSESOCK( s ); return -1; }
    buf = ( char * ) malloc( cap + 1 ); if ( !buf ) { CLOSESOCK( s ); return -1; }
    t0 = now_ms();
    for ( ;; ) {
        int k;
        if ( len >= cap ) { if ( cap >= MAX_BODY ) break; cap *= 2; { char * nb = ( char * ) realloc( buf, cap + 1 ); if ( !nb ) break; buf = nb; } }
        k = ( int ) recv( s, buf + len, cap - len, 0 );
        if ( k <= 0 ) break;
        len += k; buf[len] = 0;
        if ( hdr_end < 0 ) {
            char * he = strstr( buf, "\r\n\r\n" );
            if ( he ) {
                char * p; hdr_end = ( int ) ( he - buf ) + 4;
                if ( !strncmp( buf, "HTTP/", 5 ) ) { p = strchr( buf, ' ' ); if ( p ) status = atoi( p + 1 ); }
                for ( p = buf; p < he; p++ ) {
                    if ( !ci_ncmp( p, "\r\nContent-Length:", 17 ) ) clen = atoi( p + 17 );
                    if ( !ci_ncmp( p, "\r\nTransfer-Encoding:", 20 ) ) { const char * c = ci_str( p, "chunked" ); if ( c && c < he ) chunked = 1; }
                }
            }
        }
        if ( hdr_end >= 0 && !chunked && clen >= 0 && len - hdr_end >= clen ) break;
        if ( hdr_end >= 0 && chunked && len >= 5 && !memcmp( buf + len - 5, "0\r\n\r\n", 5 ) ) break;
        if ( now_ms() - t0 > 20000 ) break;
    }
    CLOSESOCK( s );
    if ( hdr_end < 0 ) { free( buf ); return -1; }
    if ( chunked ) {                                                    /* join the chunks */
        char * w = buf + hdr_end, * r = buf + hdr_end;
        while ( r < buf + len ) {
            long sz = strtol( r, NULL, 16 ); char * nl = strstr( r, "\r\n" );
            if ( !nl || sz <= 0 ) break;
            r = nl + 2;
            if ( r + sz > buf + len ) sz = buf + len - r;
            memmove( w, r, ( size_t ) sz ); w += sz; r += sz + 2;
        }
        *w = 0; len = ( int ) ( w - buf );
    } else if ( clen >= 0 && len - hdr_end > clen ) { len = hdr_end + clen; buf[len] = 0; }
    memmove( buf, buf + hdr_end, ( size_t ) ( len - hdr_end ) + 1 );
    *out = buf;
    return status;
}

/* the text between two markers (a copy, at most n-1 bytes); 0 if the markers are not found */
static int between( const char * s, const char * a, const char * b, char * out, size_t n ) {
    const char * p = strstr( s, a ), * q; size_t l;
    if ( !p ) return 0;
    p += strlen( a ); q = strstr( p, b ); if ( !q ) return 0;
    l = ( size_t ) ( q - p ); if ( l >= n ) l = n - 1;
    memcpy( out, p, l ); out[l] = 0;
    return 1;
}

/* &lt; &gt; &amp; &quot; &apos; &#NN; in place */
static void unescape( char * s ) {
    char * w = s;
    while ( *s ) {
        if ( *s == '&' ) {
            if ( !strncmp( s, "&lt;", 4 ) ) { *w++ = '<'; s += 4; continue; }
            if ( !strncmp( s, "&gt;", 4 ) ) { *w++ = '>'; s += 4; continue; }
            if ( !strncmp( s, "&amp;", 5 ) ) { *w++ = '&'; s += 5; continue; }
            if ( !strncmp( s, "&quot;", 6 ) ) { *w++ = '"'; s += 6; continue; }
            if ( !strncmp( s, "&apos;", 6 ) ) { *w++ = '\''; s += 6; continue; }
            if ( s[1] == '#' ) {
                char * e; long c = s[2] == 'x' || s[2] == 'X' ? strtol( s + 3, &e, 16 ) : strtol( s + 2, &e, 10 );
                if ( *e == ';' && c > 0 ) {
                    if ( c < 0x80 ) *w++ = ( char ) c;
                    else if ( c < 0x800 ) { *w++ = ( char ) ( 0xC0 | ( c >> 6 ) ); *w++ = ( char ) ( 0x80 | ( c & 63 ) ); }
                    else if ( c < 0x10000 ) { *w++ = ( char ) ( 0xE0 | ( c >> 12 ) ); *w++ = ( char ) ( 0x80 | ( ( c >> 6 ) & 63 ) ); *w++ = ( char ) ( 0x80 | ( c & 63 ) ); }
                    else { *w++ = ( char ) ( 0xF0 | ( c >> 18 ) ); *w++ = ( char ) ( 0x80 | ( ( c >> 12 ) & 63 ) ); *w++ = ( char ) ( 0x80 | ( ( c >> 6 ) & 63 ) ); *w++ = ( char ) ( 0x80 | ( c & 63 ) ); }
                    s = e + 1; continue;
                }
            }
        }
        *w++ = *s++;
    }
    *w = 0;
}

/* "http://host:port/path" -> host, port, path */
static int split_url( const char * u, char * host, size_t hn, int * port, char * path, size_t pn ) {
    const char * h; size_t hl;
    if ( strncmp( u, "http://", 7 ) ) return 0;
    h = u + 7; hl = strcspn( h, ":/" ); if ( hl == 0 || hl >= hn ) return 0;
    memcpy( host, h, hl ); host[hl] = 0; h += hl; *port = 80;
    if ( *h == ':' ) { *port = atoi( h + 1 ); h += strcspn( h, "/" ); }
    snprintf( path, pn, "%s", *h ? h : "/" );
    return 1;
}

/* ---- finding the servers ---- */
static int learn_server( const char * host, int port, const char * path ) {
    char * xml = NULL, name[128] = "", ctrl[300] = "", * p; int st, i;
    SDL_LockMutex( g_mu ); for ( i = 0; i < g_nsrv; i++ ) if ( !strcmp( g_srv[i].host, host ) && g_srv[i].port == port ) { SDL_UnlockMutex( g_mu ); return 0; } SDL_UnlockMutex( g_mu );
    st = http_req( host, port, "GET", path, "", NULL, &xml );
    if ( st != 200 || !xml ) { free( xml ); return 0; }
    between( xml, "<friendlyName>", "</friendlyName>", name, sizeof name );
    unescape( name );
    p = strstr( xml, "service:ContentDirectory" );
    if ( !p || !between( p, "<controlURL>", "</controlURL>", ctrl, sizeof ctrl ) ) { free( xml ); return 0; }     /* not a server that lists content */
    free( xml );
    unescape( ctrl );
    SDL_LockMutex( g_mu );
    if ( g_nsrv < MAX_SRV ) {
        Srv * s = &g_srv[g_nsrv];
        snprintf( s->name, sizeof s->name, "%s", name[0] ? name : host );
        snprintf( s->host, sizeof s->host, "%s", host ); s->port = port;
        if ( !strncmp( ctrl, "http://", 7 ) ) { char h2[64], pa[300]; int po; if ( split_url( ctrl, h2, sizeof h2, &po, pa, sizeof pa ) ) { snprintf( s->host, sizeof s->host, "%s", h2 ); s->port = po; snprintf( s->ctrl, sizeof s->ctrl, "%s", pa ); } else s->ctrl[0] = 0; }
        else if ( ctrl[0] == '/' ) snprintf( s->ctrl, sizeof s->ctrl, "%s", ctrl );
        else snprintf( s->ctrl, sizeof s->ctrl, "/%s", ctrl );
        if ( s->ctrl[0] ) g_nsrv++;
    }
    SDL_UnlockMutex( g_mu );
    return 1;
}

static int scan_thread( void * arg ) {
    sock_t s = socket( AF_INET, SOCK_DGRAM, 0 ); struct sockaddr_in to; char msg[300], buf[2048], loc[300];
    struct { char host[64]; int port; char path[300]; } L[MAX_SRV * 2]; int nloc = 0, k, round;
    ( void ) arg;
    if ( s != BAD_SOCK ) {
        set_nonblock( s );
        memset( &to, 0, sizeof to ); to.sin_family = AF_INET; to.sin_port = htons( 1900 ); inet_pton( AF_INET, "239.255.255.250", &to.sin_addr );
        for ( round = 0; round < 2; round++ ) {                         /* two searches: a datagram can get lost */
            uint32_t t0 = now_ms(); int ml = snprintf( msg, sizeof msg, "M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\nMX: 2\r\nST: urn:schemas-upnp-org:device:MediaServer:1\r\n\r\n" );
            sendto( s, msg, ml, 0, ( struct sockaddr * ) &to, sizeof to );
            while ( now_ms() - t0 < 1500 && nloc < MAX_SRV * 2 ) {
                struct pollfd pf; int r; char * q; pf.fd = s; pf.events = POLLIN; pf.revents = 0;
                if ( poll( &pf, 1, 200 ) <= 0 ) continue;
                r = ( int ) recv( s, buf, sizeof buf - 1, 0 ); if ( r <= 0 ) continue; buf[r] = 0;
                { const char * lp = ci_str( buf, "\r\nLOCATION:" ); if ( !lp ) continue; lp += 11; while ( *lp == ' ' ) lp++; { size_t l = strcspn( lp, "\r\n" ); if ( l >= sizeof loc ) continue; memcpy( loc, lp, l ); loc[l] = 0; } }
                q = loc;
                { char h[64], pa[300]; int po, dup = 0;
                  if ( !split_url( q, h, sizeof h, &po, pa, sizeof pa ) ) continue;
                  for ( k = 0; k < nloc; k++ ) if ( !strcmp( L[k].host, h ) && L[k].port == po && !strcmp( L[k].path, pa ) ) dup = 1;
                  if ( dup ) continue;
                  snprintf( L[nloc].host, sizeof L[nloc].host, "%s", h ); L[nloc].port = po; snprintf( L[nloc].path, sizeof L[nloc].path, "%s", pa ); nloc++; }
            }
        }
        CLOSESOCK( s );
    }
    for ( k = 0; k < nloc; k++ ) learn_server( L[k].host, L[k].port, L[k].path );
    SDL_LockMutex( g_mu ); g_scan = 2; SDL_UnlockMutex( g_mu );
    return 0;
}

int dlna_scan( void ) {
    SDL_Thread * th;
    if ( !g_mu ) g_mu = SDL_CreateMutex();
#ifdef _WIN32
    { WSADATA w; WSAStartup( MAKEWORD( 2, 2 ), &w ); }
#endif
    SDL_LockMutex( g_mu );
    if ( g_scan == 1 ) { SDL_UnlockMutex( g_mu ); return -1; }
    g_scan = 1; g_nsrv = 0; g_started = 1;
    SDL_UnlockMutex( g_mu );
    th = SDL_CreateThread( scan_thread, "dlna-scan", NULL );
    if ( !th ) { SDL_LockMutex( g_mu ); g_scan = 2; SDL_UnlockMutex( g_mu ); return -1; }
    SDL_DetachThread( th );
    return 0;
}

typedef struct { char host[64]; int port; char path[300]; } AddArg;
static int add_thread( void * arg ) {
    AddArg a = * ( AddArg * ) arg; free( arg );
    learn_server( a.host, a.port, a.path );
    SDL_LockMutex( g_mu ); g_scan = 2; SDL_UnlockMutex( g_mu );
    return 0;
}

int dlna_add( const char * url ) {
    AddArg * a; SDL_Thread * th;
    if ( !g_mu ) g_mu = SDL_CreateMutex();
#ifdef _WIN32
    { WSADATA w; WSAStartup( MAKEWORD( 2, 2 ), &w ); }
#endif
    a = ( AddArg * ) malloc( sizeof *a );
    if ( !a || !url || !split_url( url, a->host, sizeof a->host, &a->port, a->path, sizeof a->path ) ) { free( a ); return -1; }
    SDL_LockMutex( g_mu );
    if ( g_scan == 1 ) { SDL_UnlockMutex( g_mu ); free( a ); return -1; }
    g_scan = 1; g_started = 1;
    SDL_UnlockMutex( g_mu );
    th = SDL_CreateThread( add_thread, "dlna-add", a );
    if ( !th ) { free( a ); SDL_LockMutex( g_mu ); g_scan = 2; SDL_UnlockMutex( g_mu ); return -1; }
    SDL_DetachThread( th );
    return 0;
}

int dlna_find( const char * url ) {
    char host[64], path[300]; int port, i, r = -1;
    if ( !g_mu || !split_url( url, host, sizeof host, &port, path, sizeof path ) ) return -1;
    SDL_LockMutex( g_mu );
    for ( i = 0; i < g_nsrv; i++ ) if ( !strcmp( g_srv[i].host, host ) && g_srv[i].port == port ) { r = i; break; }
    SDL_UnlockMutex( g_mu );
    return r;
}

int dlna_scan_state( void ) { return g_started ? g_scan : 0; }
int dlna_count( void ) { return g_nsrv; }
const char * dlna_name( int i ) { return ( i >= 0 && i < g_nsrv ) ? g_srv[i].name : ""; }

/* ---- browsing a folder ---- */
static void put_line( char ** out, size_t * len, size_t * cap, const char * s ) {
    size_t l = strlen( s );
    if ( *len + l + 2 > *cap ) { *cap = ( *len + l + 2 ) * 2; *out = ( char * ) realloc( *out, *cap ); }
    memcpy( *out + *len, s, l ); *len += l; ( *out )[*len] = 0;
}

static void clean_field( char * s ) { for ( ; *s; s++ ) if ( *s == '\t' || *s == '\n' || *s == '\r' ) *s = ' '; }

/* "0:42:01.500" -> seconds */
static long parse_dur( const char * d ) {
    int h = 0, m = 0; double sec = 0;
    if ( sscanf( d, "%d:%d:%lf", &h, &m, &sec ) == 3 ) return h * 3600L + m * 60L + ( long ) sec;
    return 0;
}

/* the DIDL-Lite document -> lines. returns how many entries */
static int parse_didl( char * x, char ** out, size_t * len, size_t * cap ) {
    int n = 0; char * p = x;
    while ( ( p = strpbrk( p, "<" ) ) != NULL ) {
        int is_c = !strncmp( p, "<container", 10 ), is_i = !strncmp( p, "<item", 5 );
        char * end; char saved, id[300] = "", title[512] = "", line[2400], cls[120] = "";
        if ( !is_c && !is_i ) { p++; continue; }
        end = strstr( p, is_c ? "</container>" : "</item>" ); if ( !end ) break;
        saved = *end; *end = 0;
        {
          { char * tag_end = strchr( p, '>' ); char attrs[800]; size_t al = tag_end ? ( size_t ) ( tag_end - p ) : 0; if ( al >= sizeof attrs ) al = sizeof attrs - 1; memcpy( attrs, p, al ); attrs[al] = 0;
            { char * ia = strstr( attrs, " id=\"" ); if ( ia ) between( ia, " id=\"", "\"", id, sizeof id ); }
            if ( is_c ) { char cc[40] = ""; char * ca = strstr( attrs, "childCount=\"" ); if ( ca ) between( ca, "childCount=\"", "\"", cc, sizeof cc ); snprintf( cls, sizeof cls, "%s", cc ); } }
          between( p, "<dc:title>", "</dc:title>", title, sizeof title );
          unescape( id ); unescape( title ); clean_field( title ); clean_field( id );
          if ( is_c ) {
              snprintf( line, sizeof line, "C\t%s\t%s\t%s\n", id, title[0] ? title : "?", cls[0] ? cls : "0" );
              put_line( out, len, cap, line ); n++;
          } else {
              char klass[120] = "", url[1600] = "", size[40] = "0", dur[40] = ""; char kind = 'v'; char * r = p; int found = 0;
              between( p, "<upnp:class>", "</upnp:class>", klass, sizeof klass );
              if ( strstr( klass, "audioItem" ) ) kind = 'a'; else if ( strstr( klass, "imageItem" ) ) kind = 'i';
              while ( !found && ( r = strstr( r, "<res " ) ) != NULL ) {                    /* the first resource that is the media itself (not a thumbnail) */
                  char * te = strchr( r, '>' ); char * re = te ? strstr( te, "</res>" ) : NULL; char proto[200] = "", attrs[800];
                  size_t al; if ( !te || !re ) break;
                  al = ( size_t ) ( te - r ); if ( al >= sizeof attrs ) al = sizeof attrs - 1; memcpy( attrs, r, al ); attrs[al] = 0;
                  between( attrs, "protocolInfo=\"", "\"", proto, sizeof proto );
                  { int ok = ( kind == 'v' && strstr( proto, ":video/" ) ) || ( kind == 'a' && strstr( proto, ":audio/" ) ) || ( kind == 'i' && strstr( proto, ":image/" ) );
                    if ( !ok && kind == 'v' && ( strstr( proto, "application/" ) || strstr( proto, "video" ) ) && !strstr( proto, ":image/" ) ) ok = 1;
                    if ( ok ) {
                        size_t ul = ( size_t ) ( re - ( te + 1 ) ); if ( ul >= sizeof url ) ul = sizeof url - 1; memcpy( url, te + 1, ul ); url[ul] = 0;
                        between( attrs, " size=\"", "\"", size, sizeof size ); between( attrs, "duration=\"", "\"", dur, sizeof dur );
                        found = 1;
                    } }
                  r = re + 6;
              }
              unescape( url );
              if ( found && !strncmp( url, "http", 4 ) ) {
                  clean_field( url );
                  snprintf( line, sizeof line, "I\t%s\t%s\t%s\t%ld\t%c\n", url, title[0] ? title : "?", size, parse_dur( dur ), kind );
                  put_line( out, len, cap, line ); n++;
              }
          } }
        *end = saved; p = end + 1;
    }
    return n;
}

static int browse_thread( void * arg ) {
    BrowseArg a = * ( BrowseArg * ) arg; char * soap = ( char * ) malloc( 2048 ), * resp = NULL, * result, * x; int st; Srv s; size_t len = 0, cap = 0, rl; char * out = NULL; char tot[40] = "0";
    free( arg );
    SDL_LockMutex( g_mu ); s = g_srv[a.srv]; SDL_UnlockMutex( g_mu );
    snprintf( soap, 2048,
        "<?xml version=\"1.0\"?><s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body>"
        "<u:Browse xmlns:u=\"urn:schemas-upnp-org:service:ContentDirectory:1\"><ObjectID>%s</ObjectID><BrowseFlag>BrowseDirectChildren</BrowseFlag><Filter>*</Filter>"
        "<StartingIndex>%d</StartingIndex><RequestedCount>%d</RequestedCount><SortCriteria></SortCriteria></u:Browse></s:Body></s:Envelope>", a.id, a.start, a.count );
    st = http_req( s.host, s.port, "POST", s.ctrl, "Content-Type: text/xml; charset=\"utf-8\"\r\nSOAPACTION: \"urn:schemas-upnp-org:service:ContentDirectory:1#Browse\"\r\n", soap, &resp );
    free( soap );
    if ( st != 200 || !resp ) { char m[120]; snprintf( m, sizeof m, st > 0 ? "the server answered %d" : "the server did not answer", st ); free( resp ); set_error( m ); SDL_LockMutex( g_mu ); g_br = 3; SDL_UnlockMutex( g_mu ); return 0; }
    between( resp, "<TotalMatches>", "</TotalMatches>", tot, sizeof tot );
    result = strstr( resp, "<Result>" );
    if ( result ) {
        result += 8; x = strstr( result, "</Result>" ); rl = x ? ( size_t ) ( x - result ) : strlen( result );
        x = ( char * ) malloc( rl + 1 ); memcpy( x, result, rl ); x[rl] = 0;
        if ( !strncmp( x, "<![CDATA[", 9 ) ) { size_t k = strlen( x ); memmove( x, x + 9, k - 8 ); if ( k >= 12 && !strcmp( x + k - 12, "]]>" ) ) x[k - 12] = 0; }
        else unescape( x );                                                          /* the DIDL-Lite document comes escaped inside the answer */
        parse_didl( x, &out, &len, &cap );
        free( x );
    }
    free( resp );
    SDL_LockMutex( g_mu );
    free( g_out ); g_out = out ? out : strdup( "" ); g_total = atoi( tot ); g_br = 2;
    SDL_UnlockMutex( g_mu );
    return 0;
}

int dlna_browse( int i, const char * id, int start, int count ) {
    BrowseArg * a; SDL_Thread * th;
    if ( !g_mu ) g_mu = SDL_CreateMutex();
    if ( i < 0 || i >= g_nsrv || !id || strlen( id ) > 250 ) return -1;
    for ( const char * c = id; *c; c++ ) if ( *c == '<' || *c == '>' || *c == '&' ) return -1;     /* the id goes inside XML: only ids that need no escaping */
    SDL_LockMutex( g_mu );
    if ( g_br == 1 ) { SDL_UnlockMutex( g_mu ); return -1; }
    g_br = 1; g_err[0] = 0;
    SDL_UnlockMutex( g_mu );
    a = ( BrowseArg * ) malloc( sizeof *a ); a->srv = i; snprintf( a->id, sizeof a->id, "%s", id ); a->start = start < 0 ? 0 : start; a->count = count < 1 ? 100 : ( count > 500 ? 500 : count );
    th = SDL_CreateThread( browse_thread, "dlna-browse", a );
    if ( !th ) { free( a ); SDL_LockMutex( g_mu ); g_br = 3; SDL_UnlockMutex( g_mu ); set_error( "cannot start" ); return -1; }
    SDL_DetachThread( th );
    return 0;
}

int dlna_browse_state( void ) { return g_br; }
const char * dlna_result( void ) { static char * keep = NULL; SDL_LockMutex( g_mu ); free( keep ); keep = g_out ? strdup( g_out ) : strdup( "" ); SDL_UnlockMutex( g_mu ); return keep; }
int dlna_total( void ) { return g_total; }
const char * dlna_error( void ) { static char e[200]; if ( !g_mu ) return ""; SDL_LockMutex( g_mu ); snprintf( e, sizeof e, "%s", g_err ); SDL_UnlockMutex( g_mu ); return e; }
