#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bgddl.h"
#include "libmod.h"
#include "SDL2/SDL.h"
#include "SDL2/SDL2_gfxPrimitives.h"

#define MAP_WIDTH 24
#define MAP_HEIGHT 24
#define TEX_WIDTH 64
#define TEX_HEIGHT 64

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static Uint32 *pixels = NULL;

static int screen_width = 320;
static int screen_height = 200;
static int map[MAP_WIDTH][MAP_HEIGHT];
static SDL_Surface *tex_surface = NULL;

// Jugador
float posX = 22, posY = 12;
float dirX = -1, dirY = 0;
float planeX = 0, planeY = 0.66;

void load_map(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) return;
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            fscanf(file, "%d", &map[x][y]);
        }
    }
    fclose(file);
}

void render_scene() {
    for (int x = 0; x < screen_width; x++) {
        float cameraX = 2 * x / (float)screen_width - 1;
        float rayDirX = dirX + planeX * cameraX;
        float rayDirY = dirY + planeY * cameraX;

        int mapX = (int)posX;
        int mapY = (int)posY;

        float sideDistX, sideDistY;

        float deltaDistX = fabs(1 / rayDirX);
        float deltaDistY = fabs(1 / rayDirY);
        float perpWallDist;

        int stepX, stepY, hit = 0, side;

        if (rayDirX < 0) {
            stepX = -1;
            sideDistX = (posX - mapX) * deltaDistX;
        } else {
            stepX = 1;
            sideDistX = (mapX + 1.0 - posX) * deltaDistX;
        }
        if (rayDirY < 0) {
            stepY = -1;
            sideDistY = (posY - mapY) * deltaDistY;
        } else {
            stepY = 1;
            sideDistY = (mapY + 1.0 - posY) * deltaDistY;
        }

        while (!hit) {
            if (sideDistX < sideDistY) {
                sideDistX += deltaDistX;
                mapX += stepX;
                side = 0;
            } else {
                sideDistY += deltaDistY;
                mapY += stepY;
                side = 1;
            }
            if (map[mapX][mapY] > 0) hit = 1;
        }

        if (side == 0)
            perpWallDist = (mapX - posX + (1 - stepX) / 2) / rayDirX;
        else
            perpWallDist = (mapY - posY + (1 - stepY) / 2) / rayDirY;

        int lineHeight = (int)(screen_height / perpWallDist);
        int drawStart = -lineHeight / 2 + screen_height / 2;
        int drawEnd = lineHeight / 2 + screen_height / 2;

        if (drawStart < 0) drawStart = 0;
        if (drawEnd >= screen_height) drawEnd = screen_height - 1;

        int texNum = map[mapX][mapY] % 4;
        float wallX = (side == 0) ? (posY + perpWallDist * rayDirY) : (posX + perpWallDist * rayDirX);
        wallX -= floor(wallX);
        int texX = (int)(wallX * TEX_WIDTH);
        if ((side == 0 && rayDirX > 0) || (side == 1 && rayDirY < 0))
            texX = TEX_WIDTH - texX - 1;

        for (int y = drawStart; y < drawEnd; y++) {
            int d = y * 256 - screen_height * 128 + lineHeight * 128;
            int texY = ((d * TEX_HEIGHT) / lineHeight) / 256;
            Uint32 *texPixels = (Uint32 *)tex_surface->pixels;
            Uint32 color = texPixels[texNum * TEX_WIDTH + texX + texY * tex_surface->w];
            if (side == 1) color = (color >> 1) & 0x7F7F7F;
            pixels[y * screen_width + x] = color;
        }
    }
}

int moddoom3d_init(INSTANCE *my, int *params) {
    screen_width = params[0];
    screen_height = params[1];
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow("Doom3D", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, screen_width, screen_height, 0);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, screen_width, screen_height);
    pixels = malloc(screen_width * screen_height * sizeof(Uint32));
    load_map("map.txt");
    tex_surface = SDL_LoadBMP("textures.bmp");
    return 1;
}

int moddoom3d_render(INSTANCE *my, int *params) {
    render_scene();
    SDL_UpdateTexture(texture, NULL, pixels, screen_width * sizeof(Uint32));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    return 1;
}

int moddoom3d_finish(INSTANCE *my, int *params) {
    if (tex_surface) SDL_FreeSurface(tex_surface);
    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    if (pixels) free(pixels);
    SDL_Quit();
    return 1;
}

DLCONSTANT constants[] = { { NULL, 0 } };
DLVARFIXUP libdoom3d_var[] = { { NULL, -1, -1 } };
DLSYSFUNCS functions_exports[] = {
    { "DOOM3D_INIT", "II", TYPE_INT, moddoom3d_init },
    { "DOOM3D_RENDER", "", TYPE_INT, moddoom3d_render },
    { "DOOM3D_FINISH", "", TYPE_INT, moddoom3d_finish },
    { NULL, NULL, 0, NULL }
};