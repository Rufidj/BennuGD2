Minimal SDL_image replacement for the PS5 port (see img_load_real.c's own
header comment). Only the entry points libmod_gfx/libmod_3d actually call -
decoding/encoding via this repo's own vendored stb_image/stb_image_write
(vendor/sdl-gpu/src/externals/), against real SDL2's public RWops/surface
API. No libpng/libjpeg dependency.
