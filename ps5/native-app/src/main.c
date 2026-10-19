/*
 *  Copyright (C) SplinterGU (Fenix/BennuGD) (Since 2006)
 *  Copyright (C) 2002-2006 Fenix Team (Fenix)
 *  Copyright (C) 1999-2002 José Luis Cebrián Pagüe (Fenix)
 *
 *  This file is part of Bennu Game Development
 *
 *  This software is provided 'as-is', without any express or implied
 *  warranty. In no event will the authors be held liable for any damages
 *  arising from the use of this software.
 *
 *  Permission is granted to anyone to use this software for any purpose,
 *  including commercial applications, and to alter it and redistribute it
 *  freely, subject to the following restrictions:
 *
 *     1. The origin of this software must not be misrepresented; you must not
 *     claim that you wrote the original software. If you use this software
 *     in a product, an acknowledgment in the product documentation would be
 *     appreciated but is not required.
 *
 *     2. Altered source versions must be plainly marked as such, and must not be
 *     misrepresented as being the original software.
 *
 *     3. This notice may not be removed or altered from any source
 *     distribution.
 *
 */

/*
 * INCLUDES
 */

#ifdef _WIN32
#define  _WIN32_WINNT 0x0500
#include <windows.h>
#endif

#ifdef __SWITCH__
#include <switch.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __ANDROID__
#include "SDL.h"
#include "jni.h"
#include <android/log.h>
#define LOG_TAG "bgdi-native"
#endif

#if defined(__PROSPERO__) && !defined(USE_REAL_PS5_OPENGL)
#include "ps5gl.h"
#endif
#if defined(__PROSPERO__) && defined(USE_REAL_PS5_OPENGL)
#include "SDL.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#endif
#if defined(__PROSPERO__)
#include <fcntl.h>   /* open() - declaration (not just under USE_REAL_PS5_OPENGL) */
#include <errno.h>   /* errno/strerror for the jailbreak-write diagnostics */
#endif

#include "bgdi.h"
#include "bgdrtm.h"
#include "xstrings.h"
#include "dirs.h"
#include "messages.h"

/* ---------------------------------------------------------------------- */

static char * dcb_exts[] = { ".dcb", ".dat", ".bin", NULL };

#if !defined( __SWITCH__ ) && !defined( PS3_PPU ) && !defined( __ANDROID__ ) && !defined( __PROSPERO__ )
static int standalone  = 0;  /* 1 only if this is an standalone interpreter   */
static int embedded    = 0;  /* 1 only if this is a stub with an embedded DCB */
#endif

/* ---------------------------------------------------------------------- */

// Function to check if a path is relative or absolute and convert it to absolute if necessary

char* get_executable_full_path(const char* path) {
    // Calculate the length of the full path
    int full_path_len = 0;
    int is_relative = path[0] != '/' && (path[1] != ':' || path[2] != '\\');

    // Check if the path is relative or absolute
    if ( is_relative ) { // Check if it's a relative path
        #ifdef _WIN32
            full_path_len = GetCurrentDirectory(0, NULL); // Get the required buffer size
            full_path_len += strlen(path) + 2 + 4; // Additional space for '\\' and null terminator and ".exe"
        #else
#ifdef PS3_PPU
            full_path_len = 2048;
#else
            full_path_len = pathconf(".", _PC_PATH_MAX);
            full_path_len += strlen(path) + 2; // Additional space for '/' and null terminator
#endif
        #endif
    } else { // It's an absolute path
        #ifdef _WIN32
            full_path_len = strlen(path) + 1 + 4; // Length of the path plus null terminator and ".exe"
        #else
            full_path_len = strlen(path) + 1; // Length of the path plus null terminator
        #endif
    }

    // Allocate memory for the full path
    char* full_path = (char*)malloc(full_path_len * sizeof(char));
    if (full_path == NULL) {
        perror("Error allocating memory");
        exit(EXIT_FAILURE);
    }

    // Construct the full path
    if ( is_relative ) { // Check if it's a relative path
        #ifdef _WIN32
            GetCurrentDirectory(full_path_len, full_path);
            strcat(full_path, "\\");
        #else
            if ( !getcwd(full_path, full_path_len) )
                ; // ignore return
            strcat(full_path, "/");
        #endif
        strcat(full_path, path);
    } else { // It's an absolute path
        strcpy(full_path, path);
    }

    // Convert path separators to Unix format
    char* p;
    for (p = full_path; *p != '\0'; p++) {
        if (*p == '\\') {
            *p = '/';
        }
    }

    // If we're on Windows and the path doesn't end with ".exe", append ".exe" to the end
    #ifdef _WIN32
        int len = strlen(full_path);
        if (len < 4 || strcmp(full_path + len - 4, ".exe") != 0) {
            strcat(full_path, ".exe");
        }
    #endif

    return full_path;
}

// non destructive basename
// fullpath already have a separator
char* get_executable_name(const char* full_path) {
    char * separator = strrchr(full_path, '/');
    if ( !separator ) return strdup( full_path );
    return strdup( separator + 1);
}

// non destructive dirname
// fullpath already have a separator
char* get_base_directory(const char* full_path) {
    // Find the last occurrence of path separator
    const char* last_separator = strrchr(full_path, '/');

    if ( !last_separator ) return strdup( "" );

    // Calculate the length of the base directory
    size_t base_dir_len = last_separator - full_path + 1;

    // Allocate memory for the base directory
    char* base_directory = (char*)malloc((base_dir_len + 1) * sizeof(char));
    if (base_directory == NULL) {
        perror("Error allocating memory");
        exit(EXIT_FAILURE);
    }

    // Copy the base directory into the newly allocated memory
    strncpy(base_directory, full_path, base_dir_len);
    base_directory[base_dir_len] = '\0'; // Null terminate the string

    return base_directory;
}


char* remove_extension(const char* path) {
    char *result = strdup( path );
    char *last_dot = strrchr(result, '.');
    char *last_slash = strrchr(result, '/');

    if ( last_dot && ( !last_slash || last_dot > last_slash ) ) {
        *last_dot = '\0';
    }

    return result;
}

char* remove_app_extension(const char* path) {
    char *result = strdup( path );
    char *last_dot = strrchr(result, '.');
    char *last_slash = strrchr(result, '/');

    if ( last_dot && ( !last_slash || last_dot > last_slash ) ) {
        const char *ext = last_dot + 1;
        
        // Check if extension is .exe
        if ( strcasecmp(ext, "exe") == 0 ) {
            *last_dot = '\0';
            return result;
        }
        
        // Check if extension matches any dcb_exts
        char **dcbext = dcb_exts;
        while ( dcbext && *dcbext ) {
            // Skip the leading dot in dcb_exts for comparison
            if ( strcasecmp(ext, (*dcbext) + 1) == 0 ) {
                *last_dot = '\0';
                return result;
            }
            dcbext++;
        }
    }

    return result;
}


int file_has_extension(const char *path) {
    const char *last_dot = strrchr(path, '.');
    const char *last_slash = strrchr(path, '/');

    if ( !last_dot ) return 0;
    if ( last_slash && last_dot < last_slash ) return 0;
    
    int len = strlen(last_dot + 1);
    if ( len >= 1 && len <= 4 ) return 1;
    return 0;
}

/*
 *  FUNCTION : main
 *
 *  Main function
 *
 *  PARAMS:
 *      INT n: ERROR LEVEL to return to OS
 *
 *  RETURN VALUE:
 *      No value
 *
 */

#if defined(__PROSPERO__) && !defined(USE_REAL_PS5_OPENGL)
extern void ps5_init_stdio_globals(void);
#endif

#include <pthread.h>
#include <unistd.h>
static int ps5_cmp_int( const void *a, const void *b ) { int x = *( const int * ) a, y = *( const int * ) b; return ( x > y ) - ( x < y ); }
static volatile int ps5_flusher_quit = 0;
static void *ps5_flusher( void *arg ) { (void) arg; while ( !ps5_flusher_quit ) { for ( int i = 0; i < 20 && !ps5_flusher_quit; i++ ) usleep( 100000 ); if ( !ps5_flusher_quit ) { fflush( stdout ); fflush( stderr ); } } return NULL; }

int main( int argc, char *argv[] ) {

#if defined(__PROSPERO__) && !defined(USE_REAL_PS5_OPENGL)
    /* Must run before anything can call fprintf(stderr,...)/fflush(stdout)/
     * etc. This custom crt1_ps5.o does not run .init_array, so this can't be
     * a __attribute__((constructor)) - see stub_libc_gaps.c's own comment
     * for the hang this fixes. Not needed with USE_REAL_PS5_OPENGL: that
     * build's native-app-boilerplate CRT (app_crt.cpp) already brings up a
     * normal, working stdout/stderr - BennuGD2's own fopen-based debug
     * logging this session confirmed no special stdio setup is needed
     * there. */
    ps5_init_stdio_globals();
#endif

#if defined( __PROSPERO__ )
/* Jailbreak on-demand via lapy_jb_daemon.elf (v1.2, ArkSama). El log se
   mantiene ABIERTO toda la vida del proceso (el fd sigue valiendo tras el
   cambio de raiz) y vive en /data: es escribible con y sin sandbox y
   SOBREVIVE a la retirada del sandbox tras un crash (/download0 muere con
   el sandbox). /app0/data es solo lectura en el pkg, por eso ahi no se
   escribia nada nunca. */
static FILE *ps5_probe_log = NULL;
extern char ps5_app_base[256];        /* base conmutable de rutas (ps5_paths.c) */
#define PLOG( ... ) do { if ( ps5_probe_log ) { fprintf( ps5_probe_log, __VA_ARGS__ ); fflush( ps5_probe_log ); } } while ( 0 )
#endif
#if defined( __PROSPERO__ )
    {
        // 1. PID del proceso actual
        int my_pid = (int)getpid();

        /* El daemon v1.2 (ArkSama/PS5-Lapy-JB-Daemon, source/main.cpp) parsea
         * el contenido como JSON: strstr(buf, "\"PID\"") busca "PID" ENTRE
         * COMILLAS -> ':' -> salta espacios/comillas -> atoi(). El unlink() es
         * SIEMPRE, parseado o no: que el archivo "desaparezca" no acredita
         * nada. Formato valido: {"PID":"<pid>"} (por eso fallaban 4 bytes
         * binarios, "152\n" y "PID:167\n"). */
        ps5_probe_log = fopen( "/data/bgdi_probe.log", "ab" );
        if ( !ps5_probe_log ) ps5_probe_log = fopen( "/download0/bgdi_probe.log", "ab" );
        PLOG( "\n=== pid=%d\n", my_pid );

        /* TID del titulo desde /app0/sce_sys/param.json. Hace falta para
         * encontrar el sandbox real tras el escape: dentro del sandbox no
         * se ve /mnt/sandbox y opendir() esta rechazado (SIGSYS). Clave
         * directa "titleId":"PPSA00090"; si no esta, del contentId
         * "UP9000-PPSA00090_000-..." extraemos "PPSA00090". */
        char tid[ 32 ] = "";
        {
            FILE *pf = fopen( "/app0/sce_sys/param.json", "rb" );
            if ( !pf ) {
                PLOG( "param.json: fopen errno=%d (%s)\n", errno, strerror( errno ) );
            } else {
                char js[ 65536 ];
                size_t n = fread( js, 1, sizeof( js ) - 1, pf );
                fclose( pf );
                js[ n ] = 0;
                const char *c = strstr( js, "\"titleId\"" );
                if ( c && ( c = strchr( c + 9, ':' ) ) != NULL && ( c = strchr( c, '"' ) ) != NULL ) {
                    c++;
                    const char *e = strchr( c, '"' );
                    size_t L = e ? (size_t)( e - c ) : strlen( c );
                    if ( L >= sizeof( tid ) ) L = sizeof( tid ) - 1;
                    memcpy( tid, c, L ); tid[ L ] = 0;
                }
                if ( !tid[ 0 ] && ( c = strstr( js, "\"contentId\"" ) ) != NULL &&
                     ( c = strchr( c + 11, '-' ) ) != NULL ) {
                    c++;
                    const char *e = strchr( c, '-' );
                    size_t L = e ? (size_t)( e - c ) : strlen( c );
                    if ( L >= 4 && c[ L - 4 ] == '_' ) L -= 4;   /* quitar "_000" */
                    if ( L >= sizeof( tid ) ) L = sizeof( tid ) - 1;
                    memcpy( tid, c, L ); tid[ L ] = 0;
                }
                PLOG( "param.json: %zu bytes, TID=%s\n", n, tid[ 0 ] ? tid : "?" );
            }
        }

        /* Peticion al daemon con descriptores nativos:
         * O_WRONLY | O_CREAT | O_TRUNC. Ruta unica: dentro de la app lo
         * autentico es /download0/ (las variantes relativas no aportan). */
        char pidbuf[ 64 ];
        int pidlen = snprintf( pidbuf, sizeof( pidbuf ), "{\"PID\":\"%d\"}\n", my_pid );
        int fdj = open( "/download0/etahen_jailbreak", 0x0001 | 0x0200 | 0x0400, 0777 );
        if ( fdj >= 0 ) {
            ssize_t w = write( fdj, pidbuf, (size_t) pidlen );
            close( fdj );
            PLOG( "peticion %s", ( w == (ssize_t) pidlen ) ? "escrita OK\n" : "write FALLO\n" );
        } else {
            PLOG( "open(etahen_jailbreak) errno=%d (%s)\n", errno, strerror( errno ) );
        }

        /* Espera activa al daemon: su ciclo es de 250 ms y en cuanto el
         * parcheo cuaja /system pasa a abrir (sondeo directo cada 50 ms: la
         * fuente de verdad; el unlink del daemon no acredita nada y varia
         * entre versiones). Con daemon se detecta en ~250-350 ms. SIN daemon
         * (usuarios que no lo usen: otro exploit, etaHen, nada) la ventana
         * completa son ~500 ms y la app sigue en modo sandbox, su
         * funcionamiento normal. */
        int unsandboxed = 0;
        for ( int i = 0; i < 10 && !unsandboxed; i++ ) {
            usleep( 50000 );
            int fs = open( "/system", 0 /* O_RDONLY */, 0 );
            if ( fs >= 0 ) { close( fs ); unsandboxed = 1; }
        }
        PLOG( "tras espera: uid=%d euid=%d unsandboxed=%d\n",
              (int) getuid(), (int) geteuid(), unsandboxed );

        /* Comprobacion del resultado: Hijacker::jailbreak() parchea ucred
         * (uid->root, caps, authid) y cambia la raiz del proceso al root
         * vnode REAL, observable sin depender de sceNotificationSend(): el
         * root real tiene /system (verificado por FTP) y el sandbox no. */
        if ( !unsandboxed ) {
            /* Sin daemon: vaciar la peticion la deja inofensiva. Un daemon
             * que arranque mas tarde hara unlink sin parsear nada (su
             * comportamiento ante un archivo vacio) y nunca parcheara a
             * media partida una app cuya base de rutas ya no se va a
             * re-mapear — si lo hiciera, /app0 desapareceria y las cargas
             * posteriores darian ENOENT. O_WRONLY|O_TRUNC: solo syscalls que
             * ya sabemos funcionar en el sandbox. */
            int fz = open( "/download0/etahen_jailbreak",
                           0x0001 | 0x0400 /* O_WRONLY | O_TRUNC */, 0777 );
            if ( fz >= 0 ) close( fz );
            PLOG( "sigue sandboxeado; peticion vaciada\n" );
        } else {
            int found = 0;
            /* /app0 YA NO EXISTE en la raiz nueva: todo lo que el motor abra
             * ("/app0/data/game.dcb" y los recursos via los wrappers de
             * ps5_paths.c) daria ENOENT. El contenido sigue en
             * /mnt/sandbox/<TID>_<NNN>/app0: lo localizamos con open()
             * (opendir() sigue rechazado) y cambiamos la base de rutas del
             * motor para que todo siga resolviendo. */
            if ( tid[ 0 ] ) {
                for ( int n = 0; n < 10 && !found; n++ ) {
                    char cand[ 128 ], dcb[ 160 ];
                    snprintf( cand, sizeof( cand ), "/mnt/sandbox/%s_%03d/app0", tid, n );
                    snprintf( dcb, sizeof( dcb ), "%s/data/game.dcb", cand );
                    int f = open( dcb, 0, 0 );
                    if ( f >= 0 ) {
                        close( f );
                        found = 1;
                        snprintf( ps5_app_base, sizeof( ps5_app_base ), "%s", cand );
                    }
                }
            }
            if ( !found ) {
                /* fallback de este titulo si param.json no se pudo leer */
                snprintf( ps5_app_base, sizeof( ps5_app_base ), "/mnt/sandbox/%s_000/app0",
                          tid[ 0 ] ? tid : "PPSA00090" );
            }
            PLOG( "UNSBOXED: ps5_app_base=%s (%s)\n", ps5_app_base,
                  found ? "verificado con open()" : "fallback sin verificar" );
        }
        /* El log NO se cierra: el fd sigue valiendo tras el cambio de raiz. */
    }
#endif

#ifdef __SWITCH__
//    consoleInit(NULL);
    romfsInit();
    fsdevMountSdmc();
    chdir("romfs:/");
#endif

    char * filename = NULL, dcbname[ __MAX_PATH ];
    int i, j, ret = -1;
    file * fp = NULL;
    dcb_signature dcb_signature = { 0 };

    /* disable stdout buffering */
#ifndef __PROSPERO__
    /* stdout/stdin/stderr are DATA symbols exported by libc.prx, not
     * functions - ps5link's NID catalog and dynwriter only resolve function
     * imports, so a reference to the `stdout` global here loads a
     * still-zero GOT slot and setvbuf() dereferences a NULL FILE* (SIGSEGV,
     * fault address 0, confirmed on hardware in bgdi's first PS5 boot
     * attempt). No TTY exists on PS5 anyway, so there is nothing to
     * unbuffer. */
    setvbuf( stdout, NULL, _IONBF, BUFSIZ );
#endif

    /* get executable full pathname  */
#if defined ( __SWITCH__ ) || defined ( PS3_PPU ) || defined ( __ANDROID__ ) || defined ( __PROSPERO__ )
    appexefullpath = strdup( argv[0] );
#else
    appexefullpath = get_executable_full_path( argv[ 0 ] );
#endif

    appexename = get_executable_name( appexefullpath );

#if !defined ( __SWITCH__ ) && !defined ( PS3_PPU ) && !defined( __ANDROID__ ) && !defined( __PROSPERO__ )
    if ( ( !strchr( appexefullpath, '\\' ) && !strchr( appexefullpath, '/' ) ) ) {
        struct stat st;
        if ( stat( appexefullpath, &st ) || !S_ISREG( st.st_mode ) ) {
            char *p = whereis( appexename );
            if ( p ) {
                char * tmp = calloc( 1, strlen( p ) + strlen( appexename ) + 2 );
                free( appexefullpath );
                sprintf( tmp, "%s/%s", p, appexename );
                free( p );
                appexefullpath = getfullpath( tmp );
                free( tmp );
            }
        }
    }
#endif

    /* get pathname of executable */
    appexepath = get_base_directory( appexefullpath );

    /* add binary path */
    file_addp( appexepath );

#if !defined( __SWITCH__ ) && !defined( PS3_PPU ) && !defined( __ANDROID__ ) && !defined( __PROSPERO__ )
    standalone = strncmpi( appexename, "bgdi", 4 );
    if ( standalone ) {
        /* Hand-made interpreter: search for DCB at EOF */
        fp = file_open( appexefullpath, "rb0" );
        if ( fp ) {
            file_seek( fp, -( int )sizeof( dcb_signature ), SEEK_END );
            file_read( fp, &dcb_signature, sizeof( dcb_signature ) );

            if ( strcmp( dcb_signature.magic, DCB_STUB_MAGIC ) == 0 ) {
                ARRANGE_QWORD( &dcb_signature.dcb_offset );
                embedded = 1;
            }
        }

        filename = appexefullpath;
    }

    if ( !standalone ) {
        /* Calling BGDI.EXE so we must get all command line params */

        for ( i = 1 ; i < argc ; i++ ) {
            if ( argv[i][0] == '-' ) {
                j = 1 ;
                while ( argv[i][j] ) {
                    if ( argv[i][j] == 'd' ) debug++;
                    if ( argv[i][j] == 'i' ) {
                        if ( argv[i][j+1] == 0 ) {
                            if ( i == argc - 1 ) {
                                fprintf( stderr, MSG_DIRECTORY_MISSING "\n" ) ;
                                exit( 0 );
                            }
                            file_addp( argv[i+1] );
                            i++ ;
                            break ;
                        }
                        file_addp( &argv[i][j + 1] ) ;
                        break ;
                    }
                    j++ ;
                }
            } else {
                if ( !filename ) {
                    filename = argv[i] ;
                    if ( i < argc - 1 ) memmove( &argv[i], &argv[i+1], sizeof( char * ) * ( argc - i - 1 ) ) ;
                    argc-- ;
                    i-- ;
                }
            }
        }

        if ( !filename ) {
            printf( BGDI_VERSION "\n"
                    "Bennu Game Development Interpreter\n\n"
                    MSG_COPYRIGHT "\n" );

            printf( MSG_USAGE
                    MSG_OPTIONS
                    MSG_LICENSE, appexename ) ;
            return -1 ;
        }
    }
#endif

    /* Initialization (modules needed before dcb_load) */

    string_init() ;
    init_c_type() ;

    /* Init application title for windowed modes */

#if defined( __PROSPERO__ )
    /* ps5link titles run with data/ packaged next to the eboot under
     * /app0 - see this project's other PS5 test titles (e.g. the libc
     * catalog test's /app0/data/libc_catalog.txt log path). */
    filename = "/app0/data/game.dcb";
    /* Con la base ya re-mapeada, fopen("/app0/data/game.dcb") resuelve en
     * /mnt/sandbox/<TID>_<NNN>/app0/data/game.dcb automaticamente. */
    PLOG( "antes de dcb_load: filename=%s ps5_app_base=%s\n", filename, ps5_app_base );
#elif defined ( __SWITCH__ ) || defined ( PS3_PPU ) || defined( __ANDROID__ )
    filename = "game.dcb";
#endif

    strcpy( dcbname, filename );

    char * en = get_executable_name( filename );
    appname = remove_app_extension( en );
    free( en );

#if !defined ( __SWITCH__ ) && !defined ( PS3_PPU ) && !defined( __ANDROID__ ) && !defined( __PROSPERO__ )
    if ( !embedded ) {
#endif
        /* First try to load directly (we expect myfile.dcb) */
        if ( !dcb_load( dcbname ) ) {
            int dcbloaded = 0;

            /* Check if we should try to add an extension */
#if !defined( __SWITCH__ ) && !defined( PS3_PPU ) && !defined( __ANDROID__ ) && !defined( __PROSPERO__ )
            if ( standalone || !file_has_extension( filename ) || filename[strlen(filename)-1] == '.' ) {
#else
            if ( !file_has_extension( filename ) || filename[strlen(filename)-1] == '.' ) {
#endif
                char ** dcbext = dcb_exts;
                char * base = strdup( filename );
                if ( base[strlen(base)-1] == '.' ) base[strlen(base)-1] = '\0';

                while ( dcbext && *dcbext ) {
                    strcpy( dcbname, base ) ;
                    strcat( dcbname, *dcbext ) ;
                    if (( dcbloaded = dcb_load( dcbname ) ) ) break;
                    dcbext++;
                }
                free( base );
            }

            if ( !dcbloaded ) {
                fprintf( stderr, MSG_DCB_ERROR "\n", filename, DCB_VERSION >> 8 ) ;
#ifdef __ANDROID__
                __android_log_print(ANDROID_LOG_INFO, LOG_TAG, MSG_DCB_ERROR, filename, DCB_VERSION >> 8);
#endif
#ifdef __PROSPERO__
                /* NUNCA hacer return de main en Prospero: el syscall de
                 * salida del proceso está rechazado (SIGSYS 12, el crash que
                 * se ve en la consola). Pedir al sistema que cierre el titulo. */
                PLOG( "dcb_load FALLO con filename=%s ps5_app_base=%s -> salida segura\n",
                      filename, ps5_app_base );
                fflush( stderr );
                {
                    extern int sceSystemServiceLoadExec( const char * path, const char ** arguments );
                    sceSystemServiceLoadExec( "exit", NULL );
                }
                for ( ;; ) usleep( 1000000 );
#endif
                return -1 ;
            }
        }
#if !defined ( __SWITCH__ ) && !defined ( PS3_PPU ) && !defined( __ANDROID__ ) && !defined( __PROSPERO__ )
    } else {
        dcb_load_from( fp, ( const char * ) dcbname, dcb_signature.dcb_offset );
    }
#endif
    /* If the dcb is not in debug mode */

    if ( dcb.data.NSourceFiles == 0 ) debug = 0;

#if defined(__PROSPERO__) && !defined(USE_REAL_PS5_OPENGL)
    /* Real AGC/GPU bring-up for PS5GL (this project's own hand-rolled
     * GL-shaped shim backed directly by AGC, superseded for new work by
     * blackbearreloaded/ps5-opengl - see memory's
     * project_ps5_pivot_to_real_opengl.md). Every other PS5 test in this
     * project's history that touches the GPU calls this explicitly in its
     * own main() before any GL call - libbggfx's gr_set_mode()/sdl-gpu's
     * GPU_Init() have no way to trigger it themselves (SDL_Init() is a
     * no-op here, see ps5gl_sdl_compat.c), so the first SET_MODE() from
     * script would hang making GL calls against an AGC context that was
     * never initialized. Not needed with USE_REAL_PS5_OPENGL: that driver's
     * real SDL2 video backend (ps5-g19) brings up EGL/GL itself inside
     * SDL_Init()/SDL_CreateWindow(SDL_WINDOW_OPENGL), the same way any
     * desktop SDL2 GL app works - nothing extra to call here. */
    ps5gl_init();
    /* Opens the very first frame. Every later one is opened by
     * SDL_GL_SwapWindow's end_frame()+begin_frame() pair (see
     * ps5gl_sdl_compat.c) - without this one, nothing between here and the
     * first GPU_Flip() would run inside a valid open frame either. */
    ps5gl_begin_frame();
#endif

    /* Initialization (modules needed after dcb_load) */

    sysproc_init() ;

#ifdef _WIN32
    HWND hWnd = GetConsoleWindow();
    DWORD dwProcessId;
    GetWindowThreadProcessId( hWnd, &dwProcessId );
    if ( dwProcessId == GetCurrentProcessId() ) ShowWindow( hWnd, SW_HIDE );
#endif

//#ifdef __SWITCH__
//    consoleExit(NULL);
//#endif

    argv[0] = filename;

#ifdef __PROSPERO__
    /* First-boot proof on real hardware: no module calls the trivial test
     * DCB can make (it has zero imports) can show anything on screen, so
     * this is the only way to tell "the interpreter loop ran a real
     * instance to completion" from "the title never got that far". */
    /* /data en vez de /app0/data: ese directorio es solo lectura en el pkg
       (todos estos fopen() fallaban en silencio) y /data sobrevive a la
       retirada del sandbox tras un crash. */
    FILE *bootlog = fopen( "/data/bgdi_boot.log", "ab" );
    if ( bootlog ) { fprintf( bootlog, "dcb loaded, NImports=%lld, mainproc=%p\n", (long long) dcb.data.NImports, (void *) mainproc ); fflush( bootlog ); }
#endif


#if defined(__PROSPERO__) && defined(USE_REAL_PS5_OPENGL)
    freopen( "/data/stderr.log", "a", stderr );
    freopen( "/data/stdout.log", "a", stdout );
    /* Each write to a log file on this console costs ~60 ms (measured), so an
       unbuffered stdout turned engine startup logging into ~40 s of waiting.
       Keep stdout fully buffered and flush it from a thread every 2 s. */
    { static char big[1 << 20], ebig[1 << 18]; setvbuf( stdout, big, _IOFBF, sizeof big ); setvbuf( stderr, ebig, _IOFBF, sizeof ebig ); }
    { pthread_t th; pthread_create( &th, NULL, ps5_flusher, NULL ); }
    {   /* libc sanity: is qsort() correct here? (the scroll draw order depends on it) */
        enum { N = 300 }; static int v[N]; unsigned int r = 12345; int bad = 0;
        for ( int i = 0; i < N; i++ ) { r = r * 1103515245u + 12345u; v[i] = (int)( ( r >> 8 ) % 1000 ); }
        qsort( v, N, sizeof( int ), ps5_cmp_int );
        for ( int i = 1; i < N; i++ ) if ( v[i-1] > v[i] ) bad++;
        printf( "LIBC qsort(300 ints): %s (%d out-of-order)\n", bad ? "BROKEN" : "ok", bad );
    }
#endif
#if defined(__PROSPERO__) && defined(USE_REAL_PS5_OPENGL)
/* OJO: nombre distinto de PLOG (el de arriba, que escribe en el log del
   jailbreak mantenido abierto): redefinir PLOG aqui seria un choque de
   macros. BOOTLOG abre/cierra su propio archivo en cada marca. */
#define BOOTLOG(...) do { FILE *_pl = fopen( "/data/bgdi_boot.log", "ab" ); if ( _pl ) { fprintf( _pl, __VA_ARGS__ ); fclose( _pl ); } } while ( 0 )
    {
        extern void GPU_SetInitWindow( Uint32 windowID );
        extern SDL_GLContext gpu_ps5_shared_gl_context;
        int vok = SDL_Init( SDL_INIT_VIDEO );
        BOOTLOG( "EARLY init=%d err=[%s]\n", vok, SDL_GetError() );
        SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 3 );
        SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 3 );
        SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );
        SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
        SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 16 );
        SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8 );
        SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8 );
        SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8 );
        SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 8 );
        SDL_Window *ew = SDL_CreateWindow( "", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN );
        BOOTLOG( "EARLY window=%p err=[%s]\n", (void *) ew, SDL_GetError() );
        if ( ew ) {
            gpu_ps5_shared_gl_context = SDL_GL_CreateContext( ew );
            BOOTLOG( "EARLY ctx=%p err=[%s]\n", (void *) gpu_ps5_shared_gl_context, SDL_GetError() );
            if ( gpu_ps5_shared_gl_context ) GPU_SetInitWindow( SDL_GetWindowID( ew ) );
        }
    }
#undef BOOTLOG
#endif

    bgdrtm_entry( argc, argv );
    { FILE *g = fopen( "/data/bgdi_boot.log", "ab" ); if ( g ) { fprintf( g, "after bgdrtm_entry, mainproc=%p\n", (void *) mainproc ); fclose( g ); } }

    if ( mainproc ) {
        ( void ) instance_new( mainproc, NULL ) ;
        { FILE *g = fopen( "/data/bgdi_boot.log", "ab" ); if ( g ) { fprintf( g, "instance_new ok, entering instance_go_all\n" ); fclose( g ); } }
        ret = instance_go_all() ;
    }

#ifdef __PROSPERO__
    if ( bootlog ) { fprintf( bootlog, "instance_go_all() returned %d\n", ret ); fclose( bootlog ); }
#endif

    #define EXITMARK(t) do { FILE *_x = fopen( "/data/exit_marks.log", "ab" ); if ( _x ) { fprintf( _x, "%s\n", t ); fclose( _x ); } } while ( 0 )
    EXITMARK( "A: instance_go_all returned, calling bgdrtm_exit" );
    bgdrtm_exit();
    EXITMARK( "B: bgdrtm_exit returned" );

    free( appexename        );
    free( appexepath        );
    free( appexefullpath    );
    free( appname           );

#ifdef __SWITCH__
    romfsExit();
#endif

#if defined(__PROSPERO__) && defined(USE_REAL_PS5_OPENGL)
    /* Leaving through the C library's exit() ran the static destructors / atexit handlers of
       the libc, the GL driver and libcxx, and that is where the process crashed on every quit.
       Everything BennuGD owns has been released above: flush the logs and end the process. */
    EXITMARK( "C: asking the system to close the title" );
    ps5_flusher_quit = 1;
    usleep( 150000 );
    fflush( stdout ); fflush( stderr );
    /* A native title must not call exit()/_exit() itself: the process-exit syscall is not allowed
       in this sandbox (SIGSYS, a crash report on every quit). The system closes the title when asked. */
    {
        extern int sceSystemServiceLoadExec( const char * path, const char ** arguments );
        sceSystemServiceLoadExec( "exit", NULL );
    }
    for ( ;; ) usleep( 1000000 );
#endif
    return ret;
}
