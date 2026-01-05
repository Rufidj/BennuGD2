/*
 * libmod_build_art.c
 * Build Engine Module - ART Texture Loading
 * 
 * Loads .ART files (TILES000.ART, etc.) from Build Engine games
 */

#include "libmod_build.h"
#include "libbggfx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

// External pixel format from BennuGD2
extern SDL_PixelFormat *gPixelFormat;

// ART file header structure
typedef struct {
    int32_t version;        // Art file version
    int32_t numtiles;       // Number of tiles in this file
    int32_t localtilestart; // First tile number in this file
    int32_t localtileend;   // Last tile number in this file
} ART_Header;

// Global tile storage
#define MAX_TILES 9216
static BUILD_Tile *tiles[MAX_TILES] = {NULL};
static int tiles_loaded = 0;

// Build Engine palette globals
uint8_t g_build_palette[768];       // Raw RGB palette (0-255 range)
uint8_t *g_palookup = NULL;         // Shading lookup table
int g_numshades = 0;                // Number of shade levels
uint32_t g_palette_cache[256];      // Pre-calculated 32-bit colors
static int palette_loaded = 0;

// ============================================================================
// Palette Loading
// ============================================================================

int build_load_palette(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "[libmod_build] Cannot open palette file: %s\n", filename);
        return -1;
    }
    
    // 1. Read 256 RGB triplets
    if (fread(g_build_palette, 1, 768, f) != 768) {
        fprintf(stderr, "[libmod_build] Error reading palette RGB\n");
        fclose(f);
        return -1;
    }
    
    // Build Engine palette uses 0-63 range, convert to 0-255
    for (int i = 0; i < 768; i++) {
        g_build_palette[i] = (g_build_palette[i] << 2) | (g_build_palette[i] >> 4);
    }
    
    // 2. Read Number of Shades (Little Endian Short)
    int16_t numshades_le;
    if (fread(&numshades_le, sizeof(int16_t), 1, f) != 1) {
        // Some palettes might not have shades (e.g. simple editors), but game palette should.
        fprintf(stderr, "[libmod_build] Warning: Palette file ends early (no shades)\n");
        g_numshades = 0;
    } else {
        g_numshades = (int)numshades_le;
    }
    
    // 3. Read Lookup Table (numshades * 256)
    if (g_numshades > 0) {
        if (g_palookup) free(g_palookup);
        g_palookup = (uint8_t *)malloc(g_numshades * 256);
        
        if (fread(g_palookup, 1, g_numshades * 256, f) != (size_t)(g_numshades * 256)) {
            fprintf(stderr, "[libmod_build] Error reading palette lookup table\n");
            free(g_palookup);
            g_palookup = NULL;
            g_numshades = 0;
        }
    }
    
    fclose(f);
    
    // 4. Generate 32-bit Color Cache
    // This allows fast mapping from final 8-bit index -> SDL Surface Color
    for (int i = 0; i < 256; i++) {
        uint8_t r = g_build_palette[i * 3 + 0];
        uint8_t g = g_build_palette[i * 3 + 1];
        uint8_t b = g_build_palette[i * 3 + 2];
        
        // Build Engine index 255 is transparent
        uint8_t a = (i == 255) ? 0 : 255;
        
        g_palette_cache[i] = SDL_MapRGBA(gPixelFormat, r, g, b, a);
    }
    
    palette_loaded = 1;
    printf("[libmod_build] Palette loaded from %s. Shades: %d. Cache generated.\n", filename, g_numshades);
    return 0;
}

// ============================================================================
// ART File Loading
// ============================================================================

int build_load_art(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "[libmod_build] Cannot open ART file: %s\n", filename);
        return -1;
    }
    
    // Read header
    ART_Header header;
    if (fread(&header, sizeof(ART_Header), 1, f) != 1) {
        fprintf(stderr, "[libmod_build] Error reading ART header\n");
        fclose(f);
        return -1;
    }
    
    if (header.localtileend < header.localtilestart) {
        printf("[libmod_build] SKIPPING ART: %s (Empty range: %d-%d)\n", 
               filename, header.localtilestart, header.localtileend);
        fclose(f);
        return 0; 
    }

    printf("[libmod_build] Loading ART: %s (tiles START: %d - END: %d)\n", 
           filename, header.localtilestart, header.localtileend);
    
    int numtiles = header.localtileend - header.localtilestart + 1;
    
    // Read tile sizes
    int16_t *tilesizx = (int16_t *)malloc(numtiles * sizeof(int16_t));
    int16_t *tilesizy = (int16_t *)malloc(numtiles * sizeof(int16_t));
    int32_t *picanm = (int32_t *)malloc(numtiles * sizeof(int32_t));
    
    if (fread(tilesizx, sizeof(int16_t), numtiles, f) != numtiles ||
        fread(tilesizy, sizeof(int16_t), numtiles, f) != numtiles ||
        fread(picanm, sizeof(int32_t), numtiles, f) != numtiles) {
        fprintf(stderr, "[libmod_build] Error reading tile info\n");
        free(tilesizx);
        free(tilesizy);
        free(picanm);
        fclose(f);
        return -1;
    }
    
    // Load tile data
    for (int i = 0; i < numtiles; i++) {
        int tile_num = header.localtilestart + i;
        if (tile_num >= MAX_TILES) continue;
        
        int xsize = tilesizx[i];
        int ysize = tilesizy[i];
        
        if (xsize > 4096 || ysize > 4096) {
             printf("[libmod_build] SKIP Tile %d (Invalid Size: %dx%d) in %s\n", 
                    tile_num, xsize, ysize, filename);
            continue;
        }
        
        BUILD_Tile *tile = (BUILD_Tile *)calloc(1, sizeof(BUILD_Tile));
        tile->xsize = xsize;
        tile->ysize = ysize;
        tile->picanm = picanm[i];
        
        int datasize = xsize * ysize;
        if (datasize > 0) {
            tile->data = (uint8_t *)malloc(datasize);
            if (fread(tile->data, 1, datasize, f) != datasize) {
                fprintf(stderr, "[libmod_build] Error reading tile %d data\n", tile_num);
                free(tile->data);
                free(tile);
                continue;
            }
        } else {
            tile->data = NULL;
        }
        
        tiles[tile_num] = tile;
    }
    
    free(tilesizx);
    free(tilesizy);
    free(picanm);
    fclose(f);
    
    tiles_loaded = 1;
    printf("[libmod_build] Loaded %d tiles from %s\n", numtiles, filename);
    return 0;
}

// ============================================================================
// Tile Access
// ============================================================================

BUILD_Tile* build_get_tile(int tile_num) {
    if (tile_num < 0 || tile_num >= MAX_TILES) return NULL;
    BUILD_Tile *t = tiles[tile_num];
    if (!t || !t->data) return NULL; // Only return valid tiles with data
    return t;
}

GRAPH* build_get_tile_graph(int tile_num) {
    if (tile_num < 0 || tile_num >= MAX_TILES || !tiles[tile_num]) {
        static int warned[MAX_TILES] = {0};
        if (tile_num >= 0 && tile_num < MAX_TILES && !warned[tile_num]) {
            int art_file_idx = tile_num / 256;
            fprintf(stderr, "[libmod_build] WARNING: Missing texture tile %d (Expected in TILES%03d.ART)\n", 
                    tile_num, art_file_idx);
            warned[tile_num] = 1;
        }
        return NULL;
    }

    BUILD_Tile *tile = tiles[tile_num];
    
    if (tile->xsize <= 0 || tile->ysize <= 0) return NULL;

    // Create GRAPH if not already created (Cache)
    if (!tile->graph && palette_loaded) {
        tile->graph = bitmap_new_syslib(tile->xsize, tile->ysize);
        if (!tile->graph) return NULL;
        
        for (int y = 0; y < tile->ysize; y++) {
            for (int x = 0; x < tile->xsize; x++) {
                int idx = y * tile->xsize + x;
                uint8_t pal_idx = tile->data[idx];
                
                // Use cached 32-bit color
                // IMPORTANT: This graph is "full bright" (shader 0)
                // Used for UI or simple 2D rendering, not for 3D engine loops
                gr_put_pixel(tile->graph, x, y, g_palette_cache[pal_idx]);
            }
        }
    }
    
    return tile->graph;
}

// ============================================================================
// Cleanup
// ============================================================================

void build_free_tiles(void) {
    for (int i = 0; i < MAX_TILES; i++) {
        if (tiles[i]) {
            if (tiles[i]->data) free(tiles[i]->data);
            if (tiles[i]->graph) bitmap_destroy(tiles[i]->graph);
            free(tiles[i]);
            tiles[i] = NULL;
        }
    }
    if (g_palookup) {
        free(g_palookup);
        g_palookup = NULL;
    }
    tiles_loaded = 0;
    palette_loaded = 0;
    g_numshades = 0;
}

// ============================================================================
// Utility Functions
// ============================================================================

int build_get_tile_width(int tile_num) {
    if (tile_num < 0 || tile_num >= MAX_TILES || !tiles[tile_num]) return 0;
    return tiles[tile_num]->xsize;
}

int build_get_tile_height(int tile_num) {
    if (tile_num < 0 || tile_num >= MAX_TILES || !tiles[tile_num]) return 0;
    return tiles[tile_num]->ysize;
}

int build_is_tile_loaded(int tile_num) {
    return (tile_num >= 0 && tile_num < MAX_TILES && tiles[tile_num] != NULL);
}
