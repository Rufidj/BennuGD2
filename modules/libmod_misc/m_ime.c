/*
 *  m_ime.c - system on-screen keyboard (PS5 IME dialog), see m_ime.h
 *
 *  The console's libSceImeDialog / libSceCommonDialog are imported statically against link stubs (like libSceVideodec2 in
 *  libmod_video): the stubs are built by ps5/tools/build-commondialog-stub.sh and the SDK's own libSceImeDialog.so.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "bgddl.h"
#include "dlvaracc.h"
#include "xstrings.h"
#include "m_ime.h"

#ifdef __PROSPERO__

/* ---- the console's API (UTF-16 text, see SharpProspero's ImeDialog.cs) ---------------------------------------- */
typedef struct {
    int32_t         userId;
    int32_t         type;                   /* 0 default, 1 basic latin, 2 url, 3 mail, 4 number */
    uint64_t        supportedLanguages;
    int32_t         enterLabel;
    int32_t         inputMethod;
    void *          filter;
    uint32_t        option;                 /* 4 = password */
    uint32_t        maxTextLength;
    uint16_t *      inputTextBuffer;
    float           posx, posy;
    int32_t         halign, valign;
    const uint16_t * placeholder;
    const uint16_t * title;
    int8_t          reserved[16];
} ImeParam;

typedef struct { int32_t outcome; int8_t reserved[12]; } ImeResult;   /* 0 ok, 1 user canceled, 2 aborted */

_Static_assert( sizeof( ImeParam ) == 96, "SceImeDialogParam" );
_Static_assert( sizeof( ImeResult ) == 16, "SceImeDialogResult" );

int sceSysmoduleLoadModule( uint16_t id );
int sceCommonDialogInitialize( void );
int sceUserServiceInitialize( void * params );
int sceUserServiceGetForegroundUser( int * user );
int sceUserServiceGetInitialUser( int * user );
int sceImeDialogInit( ImeParam * param, void * extended );
int sceImeDialogGetStatus( void );
int sceImeDialogGetResult( ImeResult * result );
int sceImeDialogAbort( void );
int sceImeDialogTerm( void );
int sceImeDialogGetPanelSizeExtended( ImeParam * param, void * extended, uint32_t * width, uint32_t * height );

#define SYSMODULE_IME_DIALOG    0x0096
#define IME_MAX                 2048

static int       g_inited = 0, g_open = 0, g_read = 0, g_ok = 0;
static uint16_t  g_buf[ IME_MAX + 2 ];
static uint16_t  g_title[ 128 ];
static char      g_text[ IME_MAX * 4 + 8 ];

/* a line per step in /data/ime.log (opened and closed each time, so it survives a crash): says where it stopped */
void ps5_dialog_log( const char * fmt, ... ) {
    __builtin_va_list ap;
    FILE * f = fopen( "/data/ime.log", "ab" );
    if ( !f ) return;
    __builtin_va_start( ap, fmt );
    vfprintf( f, fmt, ap );
    __builtin_va_end( ap );
    fputc( '\n', f );
    fclose( f );
}

static size_t utf8_to_utf16( const char * s, uint16_t * out, size_t cap ) {
    size_t n = 0;
    const unsigned char * p = ( const unsigned char * ) s;
    while ( *p && n + 2 < cap ) {
        uint32_t c = *p++, extra = 0;
        if ( c >= 0xF0 ) { c &= 0x07; extra = 3; } else if ( c >= 0xE0 ) { c &= 0x0F; extra = 2; } else if ( c >= 0xC0 ) { c &= 0x1F; extra = 1; }
        while ( extra-- && ( *p & 0xC0 ) == 0x80 ) c = ( c << 6 ) | ( *p++ & 0x3F );
        if ( c >= 0x10000 ) { c -= 0x10000; out[ n++ ] = ( uint16_t ) ( 0xD800 + ( c >> 10 ) ); out[ n++ ] = ( uint16_t ) ( 0xDC00 + ( c & 0x3FF ) ); }
        else out[ n++ ] = ( uint16_t ) c;
    }
    out[ n ] = 0;
    return n;
}

static void utf16_to_utf8( const uint16_t * s, char * out, size_t cap ) {
    size_t n = 0;
    while ( *s && n + 5 < cap ) {
        uint32_t c = *s++;
        if ( c >= 0xD800 && c < 0xDC00 && *s >= 0xDC00 && *s < 0xE000 ) c = 0x10000 + ( ( c - 0xD800 ) << 10 ) + ( *s++ - 0xDC00 );
        if ( c < 0x80 ) out[ n++ ] = ( char ) c;
        else if ( c < 0x800 ) { out[ n++ ] = ( char ) ( 0xC0 | ( c >> 6 ) ); out[ n++ ] = ( char ) ( 0x80 | ( c & 0x3F ) ); }
        else if ( c < 0x10000 ) { out[ n++ ] = ( char ) ( 0xE0 | ( c >> 12 ) ); out[ n++ ] = ( char ) ( 0x80 | ( ( c >> 6 ) & 0x3F ) ); out[ n++ ] = ( char ) ( 0x80 | ( c & 0x3F ) ); }
        else { out[ n++ ] = ( char ) ( 0xF0 | ( c >> 18 ) ); out[ n++ ] = ( char ) ( 0x80 | ( ( c >> 12 ) & 0x3F ) ); out[ n++ ] = ( char ) ( 0x80 | ( ( c >> 6 ) & 0x3F ) ); out[ n++ ] = ( char ) ( 0x80 | ( c & 0x3F ) ); }
    }
    out[ n ] = 0;
}

/* Loads the dialog's own system module (always first: its functions are static imports that only answer once the module is
   loaded), then brings up the shared dialog subsystem (libSceCommonDialog) and the user service once. */
int ps5_dialogs_init( int sysmodule ) {
    int rc;
    rc = sceSysmoduleLoadModule( ( uint16_t ) sysmodule );
    ps5_dialog_log( "sceSysmoduleLoadModule(%#x) -> %#x", sysmodule, ( unsigned ) rc );       /* an already loaded module answers an error too: go on */
    if ( g_inited ) return 0;
    rc = sceCommonDialogInitialize();
    ps5_dialog_log( "sceCommonDialogInitialize -> %#x", ( unsigned ) rc );
    if ( rc < 0 && ( unsigned ) rc != 0x80B80002u ) return -3;
    rc = sceUserServiceInitialize( 0 );
    ps5_dialog_log( "sceUserServiceInitialize -> %#x", ( unsigned ) rc );          /* already initialised is fine */
    g_inited = 1;
    return 0;
}

int64_t libmod_misc_syskb_open( INSTANCE * my, int64_t * params ) {
    const char * title = ( const char * ) string_get( params[0] );
    const char * initial = ( const char * ) string_get( params[1] );
    int64_t maxlen = params[2], flags = params[3];
    ImeParam p;
    uint32_t w = 0, h = 0;
    int rc, user = 0;
    int64_t ret = 0;

    if ( g_open ) { ret = -2; goto done; }
    if ( maxlen < 1 ) maxlen = 1;
    if ( maxlen > IME_MAX ) maxlen = IME_MAX;

    if ( ps5_dialogs_init( SYSMODULE_IME_DIALOG ) != 0 ) { ret = -3; goto done; }

    rc = sceUserServiceGetForegroundUser( &user );
    if ( rc != 0 ) rc = sceUserServiceGetInitialUser( &user );
    ps5_dialog_log( "user -> %#x (%d)", ( unsigned ) rc, user );
    if ( rc != 0 ) { ret = -4; goto done; }

    memset( &p, 0, sizeof p );
    memset( g_buf, 0, sizeof g_buf );
    utf8_to_utf16( initial ? initial : "", g_buf, ( size_t ) maxlen + 1 );
    utf8_to_utf16( title ? title : "", g_title, sizeof g_title / sizeof g_title[0] );
    p.userId = user;
    p.type = ( flags & 2 ) ? 2 : 0;
    p.option = ( flags & 1 ) ? 4u : 0u;
    p.maxTextLength = ( uint32_t ) maxlen;
    p.inputTextBuffer = g_buf;
    p.title = g_title;
    if ( sceImeDialogGetPanelSizeExtended( &p, 0, &w, &h ) == 0 && w && h ) {      /* centred on a 1080p screen */
        p.posx = ( 1920.0f - ( float ) w ) / 2.0f; p.posy = ( 1080.0f - ( float ) h ) / 2.0f;
        p.halign = 0; p.valign = 0;
    }
    rc = sceImeDialogInit( &p, 0 );
    ps5_dialog_log( "sceImeDialogInit -> %#x (panel %ux%u, flags %d)", ( unsigned ) rc, w, h, ( int ) flags );
    if ( rc != 0 ) { ret = -5; goto done; }
    g_open = 1; g_read = 0; g_ok = 0; g_text[0] = 0;
done:
    string_discard( params[0] ); string_discard( params[1] );
    return ret;
}

int64_t libmod_misc_syskb_status( INSTANCE * my, int64_t * params ) {
    int st;
    if ( !g_open ) return 0;
    st = sceImeDialogGetStatus();
    if ( st == 2 && !g_read ) {
        ImeResult r;
        memset( &r, 0, sizeof r );
        if ( sceImeDialogGetResult( &r ) == 0 ) g_ok = ( r.outcome == 0 );
        if ( g_ok ) utf16_to_utf8( g_buf, g_text, sizeof g_text ); else g_text[0] = 0;
        g_read = 1;
        ps5_dialog_log( "result: outcome %d, text %d bytes", ( int ) r.outcome, ( int ) strlen( g_text ) );
    }
    return st;
}

int64_t libmod_misc_syskb_ok( INSTANCE * my, int64_t * params ) { return g_read ? g_ok : 0; }

int64_t libmod_misc_syskb_text( INSTANCE * my, int64_t * params ) {
    int64_t str = string_new( g_read ? g_text : "" );
    string_use( str );
    return str;
}

int64_t libmod_misc_syskb_close( INSTANCE * my, int64_t * params ) {
    if ( !g_open ) return 0;
    if ( sceImeDialogGetStatus() != 2 ) sceImeDialogAbort();
    sceImeDialogTerm();
    g_open = 0;
    ps5_dialog_log( "closed" );
    return 0;
}

#else   /* no system keyboard on this platform: the script keeps its own */

int ps5_dialogs_init( int sysmodule ) { ( void ) sysmodule; return -100; }
void ps5_dialog_log( const char * fmt, ... ) { ( void ) fmt; }

/* for testing a script on a PC: MP_FAKE_SYSKB="text" makes the keyboard "open", stay up for a while and answer that text */
static int g_fake = 0, g_fake_polls = 0;
static char g_fake_text[ 256 ];

int64_t libmod_misc_syskb_open( INSTANCE * my, int64_t * params ) {
    const char * e = getenv( "MP_FAKE_SYSKB" );
    string_discard( params[0] ); string_discard( params[1] );
    if ( !e ) return -100;
    snprintf( g_fake_text, sizeof g_fake_text, "%s", e );
    g_fake = 1; g_fake_polls = 0;
    return 0;
}
int64_t libmod_misc_syskb_status( INSTANCE * my, int64_t * params ) {
    if ( !g_fake ) return 0;
    return ++g_fake_polls < 30 ? 1 : 2;
}
int64_t libmod_misc_syskb_ok( INSTANCE * my, int64_t * params ) { return g_fake && g_fake_polls >= 30 && strcmp( g_fake_text, "<cancel>" ) != 0; }
int64_t libmod_misc_syskb_text( INSTANCE * my, int64_t * params ) {
    int64_t str = string_new( g_fake ? g_fake_text : "" );
    string_use( str );
    return str;
}
int64_t libmod_misc_syskb_close( INSTANCE * my, int64_t * params ) { g_fake = 0; return 0; }

#endif
