/* Minimal SDL_image replacement - only the entry points the ported modules call.
 * Real implementation in img_load_real.c, built from stb_image/stb_image_write
 * against real SDL2's own public RWops/surface API. */
#ifndef PS5_REALGL_SDL_IMAGE_H
#define PS5_REALGL_SDL_IMAGE_H

#include <SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IMG_INIT_JPG 0x00000001
#define IMG_INIT_PNG 0x00000002
#define IMG_INIT_TIF 0x00000004

int IMG_Init(int flags);
void IMG_Quit(void);
SDL_Surface *IMG_Load(const char *file);
SDL_Surface *IMG_Load_RW(SDL_RWops *src, int freesrc);
const char *IMG_GetError(void);
int IMG_SavePNG(SDL_Surface *surface, const char *file);
int IMG_SavePNG_RW(SDL_Surface *surface, SDL_RWops *dst, int freedst);

#ifdef __cplusplus
}
#endif

#endif
