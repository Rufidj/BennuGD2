/* Minimal SDL_image replacement for BennuGD2's libmod_gfx (m_map.c) against
 * real SDL2 - only the four entry points actually used:
 * IMG_Load_RW/IMG_GetError/IMG_SavePNG/IMG_SavePNG_RW. Decoding/encoding is
 * stb_image/stb_image_write (already vendored under sdl-gpu's externals/ and
 * linked into every BennuGD2 PS5 build already) instead of libpng/libjpeg -
 * PS5GL's own ps5gl_sdl_compat.c did the same thing, but poked at that
 * project's own fake SDL_RWops/SDL_Surface struct fields directly; this
 * version goes through real SDL2's own public RWops/surface API instead, so
 * surfaces it returns are completely ordinary SDL2 surfaces (safe to
 * SDL_FreeSurface/SDL_BlitSurface/SDL_LockSurface normally).
 *
 * Declarations only here (no *_IMPLEMENTATION): SDL_gpu's own build already
 * compiles stb_image.c/stb_image_write.c from this exact same vendored
 * header (vendor/sdl-gpu/src/externals/), and every PS5 app links SDL_gpu
 * too - a second implementation in this file would just be a duplicate
 * symbol at link time. */
#include <SDL.h>
#include <stdlib.h>
#include <string.h>

#define STBI_NO_STDIO
#include "stb_image.h"
#include "stb_image_write.h"

/* stbi_write_png_to_mem is a local addition to this vendored stb_image_write.h
 * (SDL_gpu's own copy, which this file's includes resolve to), added only
 * inside its STB_IMAGE_WRITE_IMPLEMENTATION block - so unlike the rest of the
 * stb_write API it has no public prototype above that block to pick up here.
 * SDL_gpu's build still compiles and exports the real definition; this just
 * declares it so this TU (which doesn't itself define
 * STB_IMAGE_WRITE_IMPLEMENTATION - see this file's own top comment) can call
 * it. */
extern unsigned char *stbi_write_png_to_mem(unsigned char *pixels, int stride_bytes,
                                             int x, int y, int n, int *out_len);
#include "stb_image_write.h"

static const char *s_img_error = "";

static SDL_Surface *surface_from_stb(unsigned char *pixels, int w, int h) {
    if (!pixels) return NULL;
    /* A real SDL2-constructed surface (every internal field - refcount,
     * clip_rect, BlitMap, locking state - properly initialized by SDL2
     * itself), then stb_image's buffer is copied in and freed immediately -
     * the returned surface owns nothing stb_image allocated, so ordinary
     * SDL_FreeSurface is always correct for it afterward. */
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ABGR8888);
    if (!s) { stbi_image_free(pixels); return NULL; }
    memcpy(s->pixels, pixels, (size_t)w * (size_t)h * 4);
    stbi_image_free(pixels);
    return s;
}

SDL_Surface *IMG_Load_RW(SDL_RWops *rw, int freesrc) {
    if (!rw) return NULL;
    Sint64 size = SDL_RWsize(rw);
    if (size <= 0) { if (freesrc) SDL_RWclose(rw); return NULL; }
    unsigned char *buf = (unsigned char *)malloc((size_t)size);
    if (!buf) { if (freesrc) SDL_RWclose(rw); return NULL; }
    size_t got = SDL_RWread(rw, buf, 1, (size_t)size);
    if (freesrc) SDL_RWclose(rw);
    if (got != (size_t)size) { free(buf); return NULL; }

    int w, h, channels;
    unsigned char *pixels = stbi_load_from_memory(buf, (int)size, &w, &h, &channels, 4);
    free(buf);
    if (!pixels) { s_img_error = stbi_failure_reason(); return NULL; }
    return surface_from_stb(pixels, w, h);
}

SDL_Surface *IMG_Load(const char *file) {
    SDL_RWops *rw = SDL_RWFromFile(file, "rb");
    if (!rw) { s_img_error = SDL_GetError(); return NULL; }
    return IMG_Load_RW(rw, 1);
}

const char *IMG_GetError(void) { return s_img_error; }
int IMG_Init(int flags) { return flags; }
void IMG_Quit(void) {}

int IMG_SavePNG_RW(SDL_Surface *surface, SDL_RWops *dst, int freedst) {
    if (!surface || !dst) { if (freedst && dst) SDL_RWclose(dst); return -1; }
    SDL_Surface *src = surface;
    SDL_Surface *converted = NULL;
    if (surface->format->format != SDL_PIXELFORMAT_ABGR8888) {
        converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ABGR8888, 0);
        if (!converted) { if (freedst) SDL_RWclose(dst); return -1; }
        src = converted;
    }
    int ok = 0;
    SDL_LockSurface(src);
    void *png_data;
    int png_len;
    png_data = stbi_write_png_to_mem((const unsigned char *)src->pixels, src->pitch,
                                      src->w, src->h, 4, &png_len);
    SDL_UnlockSurface(src);
    if (png_data) {
        ok = (SDL_RWwrite(dst, png_data, 1, (size_t)png_len) == (size_t)png_len);
        free(png_data);
    }
    if (converted) SDL_FreeSurface(converted);
    if (freedst) SDL_RWclose(dst);
    return ok ? 0 : -1;
}

int IMG_SavePNG(SDL_Surface *surface, const char *file) {
    SDL_RWops *dst = SDL_RWFromFile(file, "wb");
    if (!dst) return -1;
    return IMG_SavePNG_RW(surface, dst, 1);
}
