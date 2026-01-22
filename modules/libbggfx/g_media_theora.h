/*
 *  Copyright (C) SplinterGU (Fenix/BennuGD) (Since 2006)
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

#ifndef __MEDIA_THEORA_H
#define __MEDIA_THEORA_H

#ifndef NO_THEORA
#include "SDL.h"

// ... (contenido original) ...
// Copiaré todo el contenido original aquí para mantenerlo
typedef struct THR_ID {
    void *decoder;               /**< Decoder handle for media playback. */
    const void *video;           /**< Current video frame. */
    const void *audio;           /**< Current audio data. */
    SDL_Surface *shadow;         /**< SDL_Surface used for video rendering. */
    int has_audio;               /**< Flag indicating if audio is present. */
    int has_video;               /**< Flag indicating if video is present. */
    Uint32 framems;             /**< Frame duration in milliseconds. */
    int opened_audio;           /**< Flag indicating if the audio device is opened. */
    Uint32 baseticks;           /**< Base time for playback. */
    SDL_AudioDeviceID audio_devid; /**< ID of the audio device. */
    void *audio_queue;          /**< Queue for audio data. */
    void *audio_queue_tail;     /**< Tail of the audio queue. */
    int muted;                  /**< Flag indicating if audio is muted. */
    int volume;                 /**< Current volume level (0-128). */
    int paused;                 /**< Flag indicating if playback is paused. */
    Uint32 paused_ticks;        /**< Time when playback was paused. */
    unsigned int playms;        /**< Playback start time in milliseconds. */
} THR_ID;

extern THR_ID* thr_open(const char *fname, const uint32_t timeout);
extern int thr_update(THR_ID *ctx);
extern void thr_close(THR_ID *ctx);
extern int thr_get_video_size(THR_ID *ctx, int *w, int *h);
extern void thr_set_shadow_surface(THR_ID *ctx, SDL_Surface *shadow);
extern int thr_get_mute(THR_ID *ctx);
extern void thr_set_mute(THR_ID *ctx, int status);
extern int thr_get_volume(THR_ID *ctx);
extern int thr_set_volume(THR_ID *ctx, int volume);
extern void thr_pause(THR_ID *ctx, int action);
extern int thr_get_state(THR_ID *ctx);
extern int thr_get_time(THR_ID *ctx);

#else
// Stubs for NO_THEORA
#include <SDL.h>

typedef struct THR_ID {
    int dummy;
} THR_ID;

static inline THR_ID* thr_open(const char *fname, const uint32_t timeout) { return NULL; }
static inline int thr_update(THR_ID *ctx) { return 0; }
static inline void thr_close(THR_ID *ctx) {}
static inline int thr_get_video_size(THR_ID *ctx, int *w, int *h) { return -1; }
static inline void thr_set_shadow_surface(THR_ID *ctx, SDL_Surface *shadow) {}
static inline int thr_get_mute(THR_ID *ctx) { return 0; }
static inline void thr_set_mute(THR_ID *ctx, int status) {}
static inline int thr_get_volume(THR_ID *ctx) { return 0; }
static inline int thr_set_volume(THR_ID *ctx, int volume) { return 0; }
static inline void thr_pause(THR_ID *ctx, int action) {}
static inline int thr_get_state(THR_ID *ctx) { return 0; /* MEDIA_STATUS_STOPPED/ERROR */ }
static inline int thr_get_time(THR_ID *ctx) { return 0; }

#endif

#endif
