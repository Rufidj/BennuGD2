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

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bgddl.h"

#include <SDL.h>

#include "libbggfx.h"

#include "dlvaracc.h"

/* --------------------------------------------------------------------------- */

#define FPS_INTIAL_VALUE    60
#define FPS_INTIAL_SKIP     2

/* --------------------------------------------------------------------------- */

int64_t fps_value = FPS_INTIAL_VALUE;
int64_t max_jump = FPS_INTIAL_SKIP;
double frame_ms = 1000.0 / FPS_INTIAL_VALUE; /* 40.0; */

uint64_t frames_count = 0;
int64_t last_frame_ticks = 0;
int64_t jump = 0;

int64_t FPS_count = 0;
int64_t FPS_init = 0;

int64_t FPS_count_sync = 0;
int64_t FPS_init_sync = 0;

double ticks_per_frame = 0.0;
double fps_partial = 0.0;

/* --------------------------------------------------------------------------- */
/* Inicializaci�n y controles de tiempo                                        */
/* --------------------------------------------------------------------------- */

/*
 *  FUNCTION : gr_set_fps
 *
 *  Change the game fps and frameskip values
 *
 *  PARAMS :
 *      fps         New number of frames per second
 *      jump        New value of maximum frameskip
 *
 *  RETURN VALUE :
 *      None
 */

void gr_set_fps( int64_t fps, int64_t skip ) {
    if ( fps == fps_value && skip == max_jump ) return;

    frame_ms = fps ? 1000.0 / ( double ) fps : 0.0;
    max_jump = skip;
    fps_value = ( int64_t ) fps;

    FPS_init_sync = FPS_init = 0;
    FPS_count_sync = FPS_count = 0;

    jump = 0;
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : gr_wait_frame
 *
 *  Wait for the next frame start.
 *
 *  PARAMS :
 *      None
 *
 *  RETURN VALUE :
 *      None
 */

void gr_wait_frame() {
    int64_t frame_ticks;

    GLOQWORD( libbggfx, FRAMES_COUNT ) = ++frames_count;

    /* -------------- */

    /* Tomo Tick actual */
#if defined(TARGET_GP2X_WIZ) || defined(TARGET_CAANOO)
    frame_ticks = bgdrtm_ptimer_get_ticks_us() / 1000L;
#else
    frame_ticks = SDL_GetTicks();
#endif
    if ( !FPS_init_sync ) {
#if defined(TARGET_GP2X_WIZ) || defined(TARGET_CAANOO)
        FPS_init_sync = FPS_init = bgdrtm_ptimer_get_ticks_us() / 1000L;
#else
        FPS_init_sync = FPS_init = SDL_GetTicks();
#endif
        FPS_count_sync = FPS_count = 0;
        jump = 0;

        /* Tiempo inicial del nuevo frame */
        last_frame_ticks = frame_ticks;

        return;
    }

    /* Tiempo transcurrido total del ejecucion del ultimo frame (Frame time en ms) */
    * ( double * ) &GLOQWORD( libbggfx, FRAME_TIME ) = ( frame_ticks - last_frame_ticks ) / 1000.0f;

    /* -------------- */

    FPS_count++;

    /* -------------- */

    if ( fps_value ) {
        FPS_count_sync++;

        ticks_per_frame = ( ( double ) ( frame_ticks - FPS_init_sync ) ) / ( double ) FPS_count_sync;
        fps_partial = 1000.0 / ticks_per_frame;

        if ( fps_partial == fps_value ) {
            FPS_init_sync = frame_ticks;
            FPS_count_sync = 0;
            jump = 0;
        }
        else if ( fps_partial > fps_value ) {
            int32_t delay = FPS_count_sync * frame_ms - ( frame_ticks - FPS_init_sync );

            if ( delay > 0 ) {
#if defined(TARGET_GP2X_WIZ) || defined(TARGET_CAANOO)
            {
                    unsigned long ta = bgdrtm_ptimer_get_ticks_us(), te = ta + delay * 1000;
                    if ( ta > te ) while ( bgdrtm_ptimer_get_ticks_us() > te );
                    while ( bgdrtm_ptimer_get_ticks_us() < te );
                }
#else
                SDL_Delay( delay );
#endif
                /* Reajust after delay */
#if defined(TARGET_GP2X_WIZ) || defined(TARGET_CAANOO)
                frame_ticks = bgdrtm_ptimer_get_ticks_us() / 1000L;
#else
                frame_ticks = SDL_GetTicks();
#endif
                ticks_per_frame = ( ( double ) ( frame_ticks - FPS_init_sync ) ) / ( double ) FPS_count_sync;
                fps_partial = 1000.0 / ticks_per_frame;
            }

            jump = 0;
        } else {
            if ( jump < max_jump ) /* Como no me alcanza el tiempo, voy a hacer skip */
                jump++; /* No dibujar el frame */
            else {
                FPS_init_sync = frame_ticks;
                FPS_count_sync = 0;
                jump = 0;
            }
        }
    }

    /* Si paso 1 segundo o mas desde la ultima lectura */
    if ( frame_ticks - FPS_init >= 1000 ) {
        if ( fps_value ) {
            GLOQWORD( libbggfx, SPEED_GAUGE ) = FPS_count /*fps_partial*/ * 100.0 / fps_value;
        } else {
            GLOQWORD( libbggfx, SPEED_GAUGE ) = 100;
        }

        GLOQWORD( libbggfx, FPS ) = FPS_count;

        FPS_init = frame_ticks;
        FPS_count = 0;
    }

    /* Tiempo inicial del nuevo frame */
    last_frame_ticks = frame_ticks;
}

/* --------------------------------------------------------------------------- */

#ifdef __PROSPERO__
#include <time.h>
/* PS5 bring-up profiling: where does one 2D frame go? Printed every 120 frames. */
static double gp_now( void ) { struct timespec t; clock_gettime( CLOCK_MONOTONIC, &t ); return ( double ) t.tv_sec * 1000.0 + ( double ) t.tv_nsec / 1e6; }
static double gp_acc[5], gp_last; static int gp_frames, gp_i;
static double gp_t0 = 0;
#define GPROF_START() do { gp_t0 = gp_now(); gp_i = 0; } while (0)
#define GPROF(n) do { double t_ = gp_now(); gp_acc[gp_i++] += t_ - gp_t0; gp_t0 = t_; } while (0)
static void gp_report( void ) {
    if ( ++gp_frames % 120 ) return;
    printf( "BGGFX per frame ms: clear=%.2f update_objects=%.2f draw_objects(3d+2d)=%.2f flip=%.2f\n", gp_acc[0] / 120, gp_acc[1] / 120, gp_acc[2] / 120, gp_acc[3] / 120 );
    for ( int i = 0; i < 5; i++ ) gp_acc[i] = 0;
}
#else
#define GPROF_START() do { } while (0)
#define GPROF(n) do { } while (0)
#define gp_report() do { } while (0)
#endif

/* PS5 bring-up: tiny tuning file /app0/data/gfx.cfg ("key=value" lines, read once).
   Keys: linear (bilinear image filter), intscale (integer pixel scale + letterbox),
   noclip (ignore GPU clip rects), flushblit (flush the blit batch after every blit). */
int gfx_dev_cfg( const char *key, int def ) {
    static int loaded = 0, n = 0;
    static char keys[8][16]; static int vals[8];
    if ( !loaded ) {
        loaded = 1;
        FILE *f = fopen( "/app0/data/gfx.cfg", "rb" );
        if ( f ) {
            char line[64];
            while ( n < 8 && fgets( line, sizeof line, f ) ) {
                char *eq = strchr( line, '=' );
                if ( !eq || line[0] == '#' ) continue;
                *eq = 0;
                snprintf( keys[n], sizeof keys[n], "%s", line );
                vals[n] = atoi( eq + 1 );
                printf( "BGGFX cfg %s=%d\n", keys[n], vals[n] );
                n++;
            }
            fclose( f );
        }
    }
    for ( int i = 0; i < n; i++ ) if ( !strcmp( keys[i], key ) ) return vals[i];
    return def;
}

/* PS5: the first touch of the screen framebuffer blocks until the previous
   frame's present completes (~13 ms), and doing it first thing serialises that wait
   with the whole frame's CPU work. A module that draws its own full-screen image
   (libmod_3d's resolve) can ask for the screen clear to be done by itself, late:
   it sets gr_defer_clear at the end of its draw; gr_draw_frame then skips the clear
   for the next frame and records that in gr_clear_was_deferred. */
extern void gr_clip_reset( void );
extern void glFinish( void );
int gr_defer_clear = 0;
int gr_clear_was_deferred = 0;

void gr_draw_frame() {
    if ( jump ) return;
    GPROF_START();

    /* Set Viewport */
//    SDL_RenderSetViewport( gRenderer, NULL );

    /* Clear screen */
#ifdef USE_SDL2
    SDL_SetRenderDrawColor( gRenderer, 0x00, 0x00, 0x00, 0xFF );
    SDL_RenderClear( gRenderer );
#endif
#ifdef USE_SDL2_GPU
    /* GPU_Clear() scopes to the target's clip rect if one is still active
       (GPU_SetClip()/GPU_SetClipRect() set it for a scrolled/clipped region
       - a starfield, a parallax layer - and something along the way skips
       the matching GPU_UnsetClip()). The clear for a brand new frame must
       always cover the whole target: a clip left over from last frame means
       everything outside it (letterbox/pillarbox bars on a scaled display,
       most visibly) never gets cleared and old content just piles up there
       frame after frame. */
    gr_clear_was_deferred = gr_defer_clear;
    gr_defer_clear = 0;
    if ( !gr_clear_was_deferred ) {
        GPU_UnsetClip( gRenderer );
        GPU_Clear( gRenderer );
    }
    gr_clip_reset();
#endif
    GPROF("clear");

    GRAPH * background = NULL;
    int64_t background_graph = GLOQWORD( libbggfx, BACKGROUND_GRAPH );

    /* Put background */
    if ( background_graph && ( background = bitmap_get( GLOQWORD( libbggfx, BACKGROUND_FILE ), background_graph ) ) ) {
        double sizex = GLODOUBLE( libbggfx, BACKGROUND_SIZEX ),
               sizey = GLODOUBLE( libbggfx, BACKGROUND_SIZEY );
        if ( sizex == 100.0 && sizey == 100.0 ) sizex = sizey = GLODOUBLE( libbggfx, BACKGROUND_SIZE );

        shader_activate( * ( BGD_SHADER ** ) GLOADDR( libbggfx, BACKGROUND_SHADER_ID ) );
        BGD_SHADER_PARAMETERS * shader_params = * ( BGD_SHADER_PARAMETERS ** ) GLOADDR( libbggfx, BACKGROUND_SHADER_PARAMS );
        if ( shader_params ) shader_apply_parameters( shader_params );

        gr_blit( NULL,
                 NULL,
                 scr_width / 2.0,
                 scr_height / 2.0,
                 GLOQWORD( libbggfx, BACKGROUND_FLAGS ),
                 GLOQWORD( libbggfx, BACKGROUND_ANGLE ),
                 sizex,
                 sizey,
                 POINT_UNDEFINED,
                 POINT_UNDEFINED,
                 background,
                 NULL,
                 255,
                 GLOBYTE( libbggfx, BACKGROUND_COLORR ),
                 GLOBYTE( libbggfx, BACKGROUND_COLORG ),
                 GLOBYTE( libbggfx, BACKGROUND_COLORB ),
                 GLOINT64( libbggfx, BACKGROUND_BLEND_MODE ),
                 GLOADDR( libbggfx, BACKGROUND_CUSTOM_BLEND_MODE)
                );
    }

    /* Update the object list */
    gr_update_objects();
    GPROF("update");

    /* Dump everything */
    gr_draw_objects();
    GPROF("draw");

//    if ( fade_on || fade_set ) gr_fade_step();

    //Update screen
#ifdef USE_SDL2
    SDL_RenderPresent( gRenderer );
    SDL_RenderSetClipRect( gRenderer, NULL );
#endif
#ifdef USE_SDL2_GPU
    if ( gfx_dev_cfg( "finish", 0 ) ) glFinish();   /* test: present only after the GPU completed the frame */
    GPU_Flip( gRenderer );
#endif
    GPROF("flip");
    gp_report();
}

/* --------------------------------------------------------------------------- */

void frame_init() {
#ifndef TARGET_DINGUX_A320
    if ( !SDL_WasInit( SDL_INIT_TIMER ) ) SDL_InitSubSystem( SDL_INIT_TIMER );
#endif
}

/* --------------------------------------------------------------------------- */

void frame_exit() {
#ifndef TARGET_DINGUX_A320
    if ( SDL_WasInit( SDL_INIT_TIMER ) ) SDL_QuitSubSystem( SDL_INIT_TIMER );
#endif
}

/* --------------------------------------------------------------------------- */
