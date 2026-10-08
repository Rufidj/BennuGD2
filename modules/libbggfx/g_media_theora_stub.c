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

/* PS5: the real decoder (g_media_theora.c, over the vendored theoraplay/
 * libogg/libvorbis/libtheora chain) isn't ported - that chain needs its own
 * cross-compile pass, separate from porting the rest of the engine. This
 * stand-in compiles against the exact same g_media_theora.h interface, only
 * for __PROSPERO__ (see this module's CMakeLists.txt).
 *
 * Every caller in g_media.c already treats a NULL THR_ID pointer or a -1
 * return as
 * "media unavailable" and degrades gracefully (see media_load's NULL check),
 * so this is safe: a PRG that calls MEDIA_PLAY and checks the result just
 * never gets video, exactly as validated by hand in the PS5 integration
 * tree's own stub_fake_dl_missing.c before this module was wired into CMake.
 */
#include "g_media_theora.h"

THR_ID *thr_open(const char *fname, const uint32_t timeout) { (void)fname; (void)timeout; return NULL; }
int thr_update(THR_ID *ctx) { (void)ctx; return 0; }
void thr_close(THR_ID *ctx) { (void)ctx; }
int thr_get_video_size(THR_ID *ctx, int *w, int *h) { (void)ctx; (void)w; (void)h; return -1; }
void thr_set_shadow_surface(THR_ID *ctx, SDL_Surface *shadow) { (void)ctx; (void)shadow; }
int thr_get_mute(THR_ID *ctx) { (void)ctx; return -1; }
void thr_set_mute(THR_ID *ctx, int status) { (void)ctx; (void)status; }
int thr_get_volume(THR_ID *ctx) { (void)ctx; return -1; }
int thr_set_volume(THR_ID *ctx, int volume) { (void)ctx; (void)volume; return -1; }
void thr_pause(THR_ID *ctx, int action) { (void)ctx; (void)action; }
int thr_get_state(THR_ID *ctx) { (void)ctx; return 0; /* MEDIA_STATUS_ERROR */ }
int thr_get_time(THR_ID *ctx) { (void)ctx; return 0; }
