/*
 *  m_progress.c - system progress bar (PS5 message dialog, progress mode), see m_progress.h
 *
 *  libSceMsgDialog is not reachable from a title (see the comment on resolve_msgdialog).
 *  Struct layouts as in SharpProspero's MessageDialog.cs.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "bgddl.h"
#include "dlvaracc.h"
#include "xstrings.h"
#include "m_ime.h"
#include "m_progress.h"

#ifdef __PROSPERO__

typedef struct { uint64_t size; uint8_t reserved[36]; uint32_t magic; } DlgBase;                       /* SceCommonDialogBaseParam, 48 bytes */
typedef struct { int32_t barType; int32_t pad; const char * msg; uint8_t reserved[64]; } DlgProgBar;  /* 80 bytes */
typedef struct {
    DlgBase     base;
    uint64_t    size;
    int32_t     mode;               /* 2 = progress bar */
    int32_t     pad0;
    void *      userMsg;
    DlgProgBar * progBar;
    void *      sysMsg;
    int32_t     userId;
    uint8_t     reserved[40];
    int32_t     pad1;
} DlgParam;                         /* 136 bytes */

_Static_assert( sizeof( DlgBase ) == 48, "base" );
_Static_assert( sizeof( DlgProgBar ) == 80, "bar" );
_Static_assert( sizeof( DlgParam ) == 136, "param" );

/* The system progress bar (libSceMsgDialog) cannot be reached from a title on the consoles tested so far:
 *  - a static import against a link stub is never bound by the console's loader (the first call jumps to address 0),
 *    whatever the stub's soname ("libSceMsgDialog.sprx" or "libSceMsgDialog.native.sprx");
 *  - sysmodule 0xA4 only loads libSceMsgDialog.native.sprx, a 64 KB helper that holds no public functions (its only
 *    strings are "SceMsgDialog" and "SceCdlgMsgUti"; no NID or suffix variant is found by sceKernelDlsym);
 *  - loading libSceMsgDialog.sprx itself by path answers "not found" / "not executable".
 * So OPEN answers -6 here and the player keeps its own on-screen progress text. The calls below stay as the shape of the API. */
static int ( * sceMsgDialogInitialize )( void );
static int ( * sceMsgDialogTerminate )( void );
static int ( * sceMsgDialogOpen )( DlgParam * param );
static int ( * sceMsgDialogUpdateStatus )( void );
static int ( * sceMsgDialogClose )( void );
static int ( * sceMsgDialogProgressBarSetValue )( int target, uint32_t rate );
static int ( * sceMsgDialogProgressBarSetMsg )( int target, const char * msg );

static int resolve_msgdialog( void ) {
    ps5_dialog_log( "the system progress bar is not available on this console" );
    return 0;
}

#define USER_SYSTEM             0xFF
#define MAGIC_NUMBER            0xC0D1A109ull

static int          g_open = 0;
static char         g_msg[ 512 ];
static DlgProgBar   g_bar;          /* kept alive for as long as the dialog is open */
static DlgParam     g_param;

int64_t libmod_misc_progress_open( INSTANCE * my, int64_t * params ) {
    const char * caption = ( const char * ) string_get( params[0] );
    int64_t flags = params[1];
    int rc;
    int64_t ret = 0;

    if ( g_open ) { ret = -2; goto done; }
    if ( !resolve_msgdialog() ) { ret = -6; goto done; }

    rc = sceMsgDialogInitialize();
    ps5_dialog_log( "sceMsgDialogInitialize -> %#x", ( unsigned ) rc );
    if ( rc != 0 ) { ret = -4; goto done; }

    snprintf( g_msg, sizeof g_msg, "%s", caption ? caption : "" );
    memset( &g_bar, 0, sizeof g_bar );
    g_bar.barType = ( flags & 1 ) ? 1 : 0;          /* percentage; with a cancel button when asked (pressing it makes UPDATE answer 3) */
    g_bar.msg = g_msg;
    memset( &g_param, 0, sizeof g_param );
    g_param.base.size = sizeof( DlgBase );
    g_param.base.magic = ( uint32_t ) ( MAGIC_NUMBER + ( uint64_t ) ( uintptr_t ) &g_param.base );
    g_param.size = sizeof( DlgParam );
    g_param.mode = 2;
    g_param.userId = USER_SYSTEM;
    g_param.progBar = &g_bar;
    rc = sceMsgDialogOpen( &g_param );
    ps5_dialog_log( "sceMsgDialogOpen(progress) -> %#x", ( unsigned ) rc );
    if ( rc != 0 ) { sceMsgDialogTerminate(); ret = -5; goto done; }
    g_open = 1;
done:
    string_discard( params[0] );
    return ret;
}

int64_t libmod_misc_progress_set( INSTANCE * my, int64_t * params ) {
    int64_t p = params[0];
    if ( !g_open ) return -1;
    if ( p < 0 ) p = 0;
    if ( p > 100 ) p = 100;
    return sceMsgDialogProgressBarSetValue( 0, ( uint32_t ) p );
}

int64_t libmod_misc_progress_message( INSTANCE * my, int64_t * params ) {
    const char * text = ( const char * ) string_get( params[0] );
    int64_t rc = -1;
    if ( g_open ) {
        snprintf( g_msg, sizeof g_msg, "%s", text ? text : "" );
        rc = sceMsgDialogProgressBarSetMsg( 0, g_msg );
    }
    string_discard( params[0] );
    return rc;
}

int64_t libmod_misc_progress_update( INSTANCE * my, int64_t * params ) {
    if ( !g_open ) return 0;
    return sceMsgDialogUpdateStatus();
}

int64_t libmod_misc_progress_close( INSTANCE * my, int64_t * params ) {
    if ( !g_open ) return 0;
    if ( sceMsgDialogUpdateStatus() != 3 ) sceMsgDialogClose();
    sceMsgDialogTerminate();
    g_open = 0;
    ps5_dialog_log( "progress closed" );
    return 0;
}

#else   /* no system progress bar on this platform */

static int g_fake = 0;

int64_t libmod_misc_progress_open( INSTANCE * my, int64_t * params ) {
    const char * caption = ( const char * ) string_get( params[0] );
    int64_t ret = -100;
    if ( getenv( "MP_FAKE_PROGRESS" ) ) { g_fake = 1; fprintf( stderr, "PROGRESS open \"%s\"%s\n", caption ? caption : "", ( params[1] & 1 ) ? " (with cancel)" : "" ); ret = 0; }
    string_discard( params[0] );
    return ret;
}
int64_t libmod_misc_progress_set( INSTANCE * my, int64_t * params ) {
    if ( g_fake ) fprintf( stderr, "PROGRESS set %d\n", ( int ) params[0] );
    return g_fake ? 0 : -1;
}
int64_t libmod_misc_progress_message( INSTANCE * my, int64_t * params ) {
    const char * text = ( const char * ) string_get( params[0] );
    if ( g_fake ) fprintf( stderr, "PROGRESS message \"%s\"\n", text ? text : "" );
    string_discard( params[0] );
    return g_fake ? 0 : -1;
}
int64_t libmod_misc_progress_update( INSTANCE * my, int64_t * params ) { return g_fake ? 2 : 0; }
int64_t libmod_misc_progress_close( INSTANCE * my, int64_t * params ) {
    if ( g_fake ) fprintf( stderr, "PROGRESS close\n" );
    g_fake = 0;
    return 0;
}

#endif
