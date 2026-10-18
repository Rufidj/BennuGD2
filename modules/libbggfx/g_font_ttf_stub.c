/*
 * No-op stand-in for g_font_ttf.c (FreeType2 TTF support) on targets where
 * FreeType2 isn't vendored - currently PROSPERO (PS5): the payload SDK has no
 * freetype package. gr_font_ttf_is_font() always returning 0 is what matters;
 * every caller in g_text.c/g_font.c checks it first and falls back to the
 * existing bitmap-font path, so TTF_LOAD() on this target simply fails
 * instead of crashing or needing a port of FreeType2 to the PS5 toolchain.
 */

#include "g_font_ttf.h"

int64_t gr_font_ttf_load( const char * filename ) { (void) filename; return -1; }
int64_t gr_font_ttf_load_from_memory( const void * data, size_t data_size ) { (void) data; (void) data_size; return -1; }
int64_t gr_font_ttf_set_size( int64_t fontid, int64_t pixels ) { (void) fontid; (void) pixels; return 0; }
int64_t gr_font_ttf_is_font( int64_t fontid ) { (void) fontid; return 0; }
int64_t gr_font_ttf_get_size( int64_t fontid ) { (void) fontid; return 0; }
int64_t gr_font_ttf_get_line_height( int64_t fontid ) { (void) fontid; return 0; }
int64_t gr_font_ttf_get_kerning( int64_t fontid, uint32_t left, uint32_t right ) { (void) fontid; (void) left; (void) right; return 0; }
int gr_font_ttf_get_glyph( int64_t fontid, uint32_t codepoint, TTF_GLYPH_INFO * glyph ) { (void) fontid; (void) codepoint; (void) glyph; return -1; }

uint32_t gr_font_ttf_utf8_next( const unsigned char ** text ) {
    if ( !text || !*text || !**text ) return 0;
    (*text)++;
    return 0xFFFD;
}

const char * gr_font_ttf_get_family( int64_t fontid ) { (void) fontid; return NULL; }
const char * gr_font_ttf_get_style( int64_t fontid ) { (void) fontid; return NULL; }

void gr_font_ttf_forget( int64_t fontid ) { (void) fontid; }
void gr_font_ttf_shutdown( void ) { }
