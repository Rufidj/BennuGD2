/*
 * libmod_build_render.c - Complete Build Engine Renderer
 * Phase B: Full textured rendering implementation
 */

#include "libmod_build.h"
#include "libbggfx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <SDL.h>

extern SDL_PixelFormat *gPixelFormat;
extern BUILD_Tile* build_get_tile(int tile_num);

static GRAPH *render_buffer = NULL;
static BUILD_Map *g_map = NULL;

// Macros from original engine.c
#define mulscale(a,b,c) (((int64_t)(a) * (int64_t)(b)) >> (c))
#define dmulscale(a,b,c,d,e) (((int64_t)(a)*(int64_t)(b) + (int64_t)(c)*(int64_t)(d)) >> (e))
#define tmulscale(a,b,c,d,e,f,g) (((int64_t)(a)*(int64_t)(b) + (int64_t)(c)*(int64_t)(d) + (int64_t)(e)*(int64_t)(f)) >> (g))

#define mulscale1(a,b) mulscale(a,b,1)
#define mulscale2(a,b) mulscale(a,b,2)
#define mulscale3(a,b) mulscale(a,b,3)
#define mulscale4(a,b) mulscale(a,b,4)
#define mulscale5(a,b) mulscale(a,b,5)
#define mulscale6(a,b) mulscale(a,b,6)
#define mulscale8(a,b) mulscale(a,b,8)
#define mulscale10(a,b) mulscale(a,b,10)
#define mulscale11(a,b) mulscale(a,b,11)
#define mulscale12(a,b) mulscale(a,b,12)
#define mulscale14(a,b) mulscale(a,b,14)
#define mulscale16(a,b) mulscale(a,b,16)
#define mulscale18(a,b) mulscale(a,b,18)
#define mulscale19(a,b) mulscale(a,b,19)
#define mulscale20(a,b) mulscale(a,b,20)
#define mulscale21(a,b) mulscale(a,b,21)
#define mulscale24(a,b) mulscale(a,b,24)
#define mulscale27(a,b) mulscale(a,b,27)
#define mulscale28(a,b) mulscale(a,b,28)
#define mulscale30(a,b) mulscale(a,b,30)

#define dmulscale1(a,b,c,d) dmulscale(a,b,c,d,1)
#define dmulscale2(a,b,c,d) dmulscale(a,b,c,d,2)
#define dmulscale6(a,b,c,d) dmulscale(a,b,c,d,6)
#define dmulscale32(a,b,c,d) dmulscale(a,b,c,d,32)

#define divscale12(a,b) (((int64_t)(a) << 12) / (int64_t)(b))
#define divscale30(a,b) (((int64_t)(a) << 30) / (int64_t)(b))
#define divscale32(a,b) (((int64_t)(a) << 32) / (int64_t)(b))
#define scale(a,b,c) (((int64_t)(a) * (int64_t)(b)) / (int64_t)(c))
#define pow2char(x) (1<<(x))
#define pow2long(x) (1<<(x))

#define MAXWALLSB 8192
#define MAXTILES 9216

// Data Structures
static short xb1[MAXWALLSB], xb2[MAXWALLSB];
static int yb1[MAXWALLSB], yb2[MAXWALLSB];
static int rx1[MAXWALLSB], ry1[MAXWALLSB], rx2[MAXWALLSB], ry2[MAXWALLSB];
static short p2[MAXWALLSB], thewall[MAXWALLSB], thesector[MAXWALLSB];
static short bunchfirst[MAXWALLSB], bunchlast[MAXWALLSB];
static unsigned char gotsector[(BUILD_MAXSECTORS+7)>>3];
static short sectorborder[BUILD_MAXSECTORS];
static int sectorbordercnt, numscans, numbunches, numhits;

static short *umost = NULL, *dmost = NULL, *uplc = NULL, *dplc = NULL;
static int *swall = NULL, *lwall = NULL;
static short *dwall = NULL;

// Tile management
static short tilesizx[MAXTILES];
static short tilesizy[MAXTILES];
static int picsiz[MAXTILES];
static intptr_t waloff[MAXTILES];
static int picanm[MAXTILES];

// Global rendering state
static int32_t globalposx, globalposy, globalposz;
static int16_t globalang;
static int32_t globalhoriz, xdimenscale, xdimscale, halfxdimen;
static int32_t cosglobalang, singlobalang, cosviewingrangeglobalang, sinviewingrangeglobalang;
static int32_t xdimenrecip, viewingrangerecip;  // Reciprocals for perspective calculations
static int16_t globalcursectnum;
static int32_t globaluclip, globaldclip;
static int xdimen, ydimen;

// Phase B: Additional globals for textured rendering
static int32_t globalorientation;
static int32_t globalpicnum;
static int32_t globalshade;
static int32_t globalpal;
static int32_t globalxpanning, globalypanning;
static int32_t globalx1, globaly1, globalx2, globaly2;
static int32_t globalzd, globalyscale, globalshiftval;
static int32_t globaltilesizy;  // log2 of tile height for V coordinate calculation
static int globvis = 524288;
static int globalvisibility = 512;
static intptr_t globalbufplc;  // Pointer to tile data
static uint32_t globalvplc, globalvinc;  // V coordinate and increment

// Phase C: Additional globals for floor/ceiling rendering (grouscan)
static int32_t globalx, globaly, globalz, globalzx;  // Perspective calculation
static int32_t globalx3, globaly3;  // Additional perspective vars
static int32_t asm1, asm3;  // ASM helper vars

// ============================================================================
// Phase B: Lookup Tables for Math Functions (from EDuke32)
// ============================================================================
static uint16_t sqrtable[4096];
static uint16_t shlookup[4096+256];
static int32_t reciptable[2048];

// Phase C: Slopalookup table for floor/ceiling rendering
#define SLOPALOOKUPSIZ 16384
static intptr_t slopalookup[SLOPALOOKUPSIZ];  // Palette lookup for each Y coordinate

// Phase C: Sloptable for perspective calculations
#define SLOPTABLESIZ 16384
#define HALFSLOPTABLESIZ (SLOPTABLESIZ>>1)
static int32_t sloptable[SLOPTABLESIZ];

// Phase C: Additional globals for slopevlin
static int32_t gpinc;  // Pitch increment for vertical drawing
static int32_t glogx, glogy;  // Log2 of texture dimensions
static char *gbuf;  // Texture buffer pointer

// ============================================================================
// Phase B: zint_t typedef for slope calculations (from engine.cpp:2532-2555)
// ============================================================================
#ifdef CLASSIC_Z_DIFF_64
typedef int64_t zint_t;
#define mulscale16z(a,d) (((int64_t)(a) * (d))>>16)
#define mulscale20z(a,d) (((int64_t)(a) * (d))>>20)
#define dmulscale24z(a,b,S,D) ((((int64_t)(a)*(b)) + ((int64_t)(S)*(D))) >> 24)
#else
typedef int32_t zint_t;
#define mulscale16z mulscale16
#define mulscale20z mulscale20
#define dmulscale24z(a,b,S,D) dmulscale((a),(b),(S),(D),24)
#endif

// ============================================================================
// Phase B: Math Helper Functions (from EDuke32)
// ============================================================================

// Math Helpers
static inline int klabs(int a) { return (a < 0) ? -a : a; }
static inline int ksgn(int a) { return (a > 0) ? 1 : ((a < 0) ? -1 : 0); }

// uhypsq - Unsigned hypotenuse squared (from build.h:1391)
static inline uint32_t uhypsq(int32_t const dx, int32_t const dy)
{
    return (uint32_t)dx*dx + (uint32_t)dy*dy;
}

// nsqrtasm - Fast square root (from engine_priv.h:138-145)
// Generic C version (non-ASM)
static inline int32_t nsqrtasm(uint32_t a)
{
    // JBF 20030901: This was a damn lot simpler to reverse engineer than
    // msqrtasm was. Really, it was just like simplifying an algebra equation.
    uint16_t const c = shlookup[(a & 0xff000000) ? ((a >> 24) + 4096):(a>>12)];
    a >>= c&0xff;
    return ((a&0xffff0000)|(sqrtable[a])) >> ((c&0xff00) >> 8);
}

// krecipasm - Fast reciprocal (from pragmas.h:365-372)
// Ken Silverman's original implementation
static inline int32_t krecipasm(int32_t i)
{
    // Ken did this
    union { int32_t i; float f; } x;
    x.f = (float)i;
    i = x.i;
    return ((reciptable[(i >> 12) & 2047] >> (((i - 0x3f800000) >> 23) & 31)) ^ (i >> 31));
}

// ============================================================================
// Phase B: Table Initialization (from engine.cpp:8456-8518)
// ============================================================================

// msqrtasm - Helper for table initialization (from engine.cpp:8456-8476)
static uint32_t msqrtasm(uint32_t c)
{
    uint32_t a = 0x40000000l, b = 0x20000000l;

    do
    {
        if (c >= a)
        {
            c -= a;
            a += b*4;
        }
        a -= b;
        a >>= 1;
        b >>= 2;
    } while (b);

    if (c >= a)
        a++;

    return a >> 1;
}

// initksqrt - Initialize sqrt and recip tables (from engine.cpp:8481-8518)
static void initksqrt(void)
{
    int32_t i, j, k;

    j = 1; k = 0;
    for (i=0; i<4096; i++)
    {
        if (i >= j) { j <<= 2; k++; }
        sqrtable[i] = (uint16_t)(msqrtasm((i<<18)+131072)<<1);
        shlookup[i] = (k<<1)+((10-k)<<8);
        if (i < 256) shlookup[i+4096] = ((k+6)<<1)+((10-(k+6))<<8);
    }
}

// init_build_tables - Initialize all lookup tables
static void init_build_tables(void)
{
    static int tables_initialized = 0;
    if (tables_initialized) return;
    
    initksqrt();
    
    // Initialize reciptable
    for (int i = 0; i < 2048; i++)
        reciptable[i] = divscale30(2048, i + 2048);
    
    // Initialize sloptable for floor/ceiling perspective
    for (int i = 0; i < SLOPTABLESIZ; i++)
        sloptable[i] = krecipasm(i - HALFSLOPTABLESIZ);
    
    tables_initialized = 1;
}

static void qinterpolatedown16short(short *bufptr, int num, int val, int add) {
    for (int i = 0; i < num; i++) {
        bufptr[i] = (short)(val >> 16);
        val += add;
    }
}

// getpalookup - simplified palette lookup
static inline int getpalookup(int davis, int dashade) {
    int palookup_index = dashade + (davis >> 8);
    if (palookup_index < 0) palookup_index = 0;
    if (palookup_index > 63) palookup_index = 63;
    return palookup_index;
}

// Initialize tile tables
static void init_tile_tables(void) {
    for (int i = 0; i < MAXTILES; i++) {
        BUILD_Tile *tile = build_get_tile(i);
        if (tile && tile->data) {
            tilesizx[i] = tile->xsize;
            tilesizy[i] = tile->ysize;
            int xlog = 0, ylog = 0;
            int tx = tile->xsize, ty = tile->ysize;
            while (tx > 1) { tx >>= 1; xlog++; }
            while (ty > 1) { ty >>= 1; ylog++; }
            picsiz[i] = (ylog << 4) | xlog;
            waloff[i] = (intptr_t)tile->data;
            picanm[i] = tile->picanm;
        } else {
            tilesizx[i] = 0; tilesizy[i] = 0;
            picsiz[i] = 0; waloff[i] = 0; picanm[i] = 0;
        }
    }
}

// Stubs for compatibility
static inline void setgotpic(int picnum) {}
static inline void loadtile(short tilenume) {}
static inline int animateoffs(short tilenum, short fakevar) { return 0; }

// ============================================================================
// Phase C: slopevlin() - Floor/Ceiling column rendering (from a-c.cpp:96-113)
// ============================================================================

// Helper macro for high part of int64
#define inthi_t(x) ((int32_t)((x) >> 32))

// slopevlin - Draw a vertical column for sloped floor/ceiling
// Draws from y2 (bottom) upward to y1 (top)
static void slopevlin(int32_t screen_x, int32_t screen_y_bottom, int32_t i, intptr_t slopaloffs, int32_t cnt, int32_t bx, int32_t by)
{
    intptr_t *slopalptr;
    int32_t bz, bzinc;
    uint32_t u, v;
    int32_t current_y;
    
    bz = asm3;
    bzinc = (asm1 >> 3);
    slopalptr = (intptr_t *)slopaloffs;
    
    // Start from bottom (y2) and draw upward
    current_y = screen_y_bottom;
    
    for (; cnt > 0; cnt--) {
        i = sloptable[(bz >> 6) + HALFSLOPTABLESIZ];
        bz += bzinc;
        u = bx + inthi_t((int64_t)globalx3 * i);
        v = by + inthi_t((int64_t)globaly3 * i);
        
        // Get pixel from texture
        int32_t texel_offset = ((u >> (32 - glogx)) << glogy) + (v >> (32 - glogy));
        uint8_t texel = gbuf[texel_offset];
        
        // Apply palette lookup with shading
        // slopalptr[0] is a pointer to the shaded palette for this scanline
        uint8_t palindex = *((uint8_t *)slopalptr[0] + texel);
        
        // Draw pixel using gr_put_pixel
        uint32_t color = g_palette_cache[palindex];
        gr_put_pixel(render_buffer, screen_x, current_y, color);
        
        slopalptr--;
        current_y--;  // Move up one pixel (toward y1)
    }
}

// ============================================================================
// Phase C: Slope Z Calculation Functions (from engine.cpp:14246-14282)
// ============================================================================

// getceilzofslope - Get ceiling Z at position (x,y) with slope
static int32_t getceilzofslope(int16_t sectnum, int32_t dax, int32_t day)
{
    BUILD_Sector *sec = &g_map->sectors[sectnum];
    
    if (!(sec->ceilingstat & 2))
        return sec->ceilingz;
    
    BUILD_Wall *wal = &g_map->walls[sec->wallptr];
    BUILD_Wall *wal2 = &g_map->walls[wal->point2];
    
    int32_t dx = wal2->x - wal->x;
    int32_t dy = wal2->y - wal->y;
    
    int32_t i = nsqrtasm(uhypsq(dx, dy)) << 5;
    if (i == 0) return sec->ceilingz;
    
    int32_t j = dmulscale(dx, day - wal->y, -dy, dax - wal->x, 3);
    return sec->ceilingz + scale(sec->ceilingheinum, j, i);
}

// getflorzofslope - Get floor Z at position (x,y) with slope
static int32_t getflorzofslope(int16_t sectnum, int32_t dax, int32_t day)
{
    BUILD_Sector *sec = &g_map->sectors[sectnum];
    
    if (!(sec->floorstat & 2))
        return sec->floorz;
    
    BUILD_Wall *wal = &g_map->walls[sec->wallptr];
    BUILD_Wall *wal2 = &g_map->walls[wal->point2];
    
    int32_t dx = wal2->x - wal->x;
    int32_t dy = wal2->y - wal->y;
    
    int32_t i = nsqrtasm(uhypsq(dx, dy)) << 5;
    if (i == 0) return sec->floorz;
    
    int32_t j = dmulscale(dx, day - wal->y, -dy, dax - wal->x, 3);
    return sec->floorz + scale(sec->floorheinum, j, i);
}

// ============================================================================
// Phase B: wallscan() Helper Functions (from engine.cpp:1998-2051)
// ============================================================================

// calc_bufplc - Calculate buffer pointer for texture column (engine.cpp:1998-2020)
static void calc_bufplc(intptr_t *bufplc, int32_t lw, int32_t tsizx_bits, int32_t tsizy_bits)
{
    // CAUTION: lw can be negative!
    int32_t i = lw + globalxpanning;

    // Wrap horizontally
    if (tsizx_bits >= 0)
        i &= ((1 << tsizx_bits) - 1);  // Power of 2: use mask
    else
        i = (uint32_t)i % (1 << (-tsizx_bits));  // Non-power of 2: use modulo

    // Multiply by height (column-major storage)
    if (tsizy_bits >= 0)
        i <<= tsizy_bits;  // Power of 2: shift
    else
        i *= (1 << (-tsizy_bits));  // Non-power of 2: multiply

    // Address is at the first row of tile storage (which is column-major)
    *bufplc = waloff[globalpicnum] + i;
}

// calc_vplcinc_wall - Calculate V coordinate and increment (engine.cpp:2022-2026)
static void calc_vplcinc_wall(uint32_t *vplc, int32_t *vinc, int32_t sw, int32_t y1v)
{
    *vinc = sw * globalyscale;
    *vplc = globalzd + (uint32_t)(*vinc) * (y1v - globalhoriz + 1);
}

// getpalookupsh - Get palette lookup with shading
static inline int32_t getpalookupsh(int32_t davis)
{
    int32_t shade = davis >> 8;
    if (shade < 0) shade = 0;
    if (shade > (g_numshades - 1)) shade = g_numshades - 1;
    return shade * 256;
}

// ============================================================================
// Phase B: wallmost() Helper Functions (from engine.cpp)
// ============================================================================

// Forward declaration for owallmost (defined later)
static int owallmost(short *mostbuf, int w, int z);

// wallmosts_finish - Finish wallmost calculation (from engine.cpp:2510-2530)
static inline void wallmosts_finish(int16_t *mostbuf, int32_t z1, int32_t z2,
                                    int32_t ix1, int32_t iy1, int32_t ix2, int32_t iy2)
{
    const int32_t y = scale(z1, xdimenscale, iy1)<<4;

#if 0
    // enable for paranoia:
    ix1 = clamp(ix1, 0, xdim-1);
    ix2 = clamp(ix2, 0, xdim-1);
    if (ix2-ix1 < 0)
        swaplong(&ix1, &ix2);
#endif
    // PK 20110423: a bit consistency checking is a good thing:
    int32_t const tmp = (ix2 - ix1 >= 0) ? (ix2 - ix1 + 1) : 1;
    int32_t const yinc = ((scale(z2, xdimenscale, iy2) << 4) - y) / tmp;

    qinterpolatedown16short((intptr_t)&mostbuf[ix1], tmp, y + (globalhoriz << 16), yinc);

    if (mostbuf[ix1] < 0) mostbuf[ix1] = 0;
    if (mostbuf[ix1] > ydimen) mostbuf[ix1] = ydimen;
    if (mostbuf[ix2] < 0) mostbuf[ix2] = 0;
    if (mostbuf[ix2] > ydimen) mostbuf[ix2] = ydimen;
}

// wallmost_getz - Calculate Z for sloped surfaces (from engine.cpp:2629-2642)
static inline zint_t wallmost_getz(int32_t fw, int32_t t, zint_t z,
                                   int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                                   int32_t xv, int32_t yv, int32_t dx, int32_t dy)
{
    // XXX: OVERFLOW with huge sectors and sloped ceilngs/floors!
    int32_t i = xv*(y1-globalposy) - yv*(x1-globalposx);
    const int32_t j = yv*x2 - xv*y2;

    if (klabs(j) > klabs(i>>3))
        i = divscale30(i,j) >> 2;

    return dmulscale24z(dx*t, mulscale20z(y2,i)+((y1-g_map->walls[fw].y)<<8),
                        -dy*t, mulscale20z(x2,i)+((x1-g_map->walls[fw].x)<<8)) + ((z-globalposz)<<7);
}

// ============================================================================
// scansector - Complete from original (already working)
// ============================================================================
static void scansector(short sectnum)
{
    BUILD_Wall *wal, *wal2;
    int x1, y1, x2, y2, xp1, yp1, xp2, yp2, templong;
    short z, startwall, endwall, numscansbefore, scanfirst, bunchfrst, nextsectnum;

    if (sectnum < 0) return;
    sectorborder[0] = sectnum;
    sectorbordercnt = 1;
    
    do {
        sectnum = sectorborder[--sectorbordercnt];
        gotsector[sectnum>>3] |= pow2char(sectnum&7);
        bunchfrst = numbunches;
        numscansbefore = numscans;
        startwall = g_map->sectors[sectnum].wallptr;
        endwall = startwall + g_map->sectors[sectnum].wallnum;
        scanfirst = numscans;
        
        for(z=startwall, wal=&g_map->walls[z]; z<endwall; z++, wal++) {
            nextsectnum = wal->nextsector;
            wal2 = &g_map->walls[wal->point2];
            x1 = wal->x - globalposx; y1 = wal->y - globalposy;
            x2 = wal2->x - globalposx; y2 = wal2->y - globalposy;

            if ((nextsectnum >= 0) && ((wal->cstat&32) == 0))
                if ((gotsector[nextsectnum>>3] & pow2char(nextsectnum&7)) == 0) {
                    templong = x1*y2 - x2*y1;
                    if (((unsigned)templong+262144) < 524288)
                        if (mulscale5(templong,templong) <= (x2-x1)*(x2-x1)+(y2-y1)*(y2-y1))
                            sectorborder[sectorbordercnt++] = nextsectnum;
                }

            if ((z == startwall) || (g_map->walls[z-1].point2 != z)) {
                xp1 = dmulscale6(y1,cosglobalang,-x1,singlobalang);
                yp1 = dmulscale6(x1,cosviewingrangeglobalang,y1,sinviewingrangeglobalang);
            } else {
                xp1 = xp2; yp1 = yp2;
            }
            xp2 = dmulscale6(y2,cosglobalang,-x2,singlobalang);
            yp2 = dmulscale6(x2,cosviewingrangeglobalang,y2,sinviewingrangeglobalang);
            
            if ((yp1 < 256) && (yp2 < 256)) goto skipitaddwall;
            if (dmulscale32(xp1,yp2,-xp2,yp1) >= 0) goto skipitaddwall;

            if (xp1 >= -yp1) {
                if ((xp1 > yp1) || (yp1 == 0)) goto skipitaddwall;
                xb1[numscans] = halfxdimen + scale(xp1,halfxdimen,yp1);
                if (xp1 >= 0) xb1[numscans]++;
                if (xb1[numscans] >= xdimen) xb1[numscans] = xdimen-1;
                yb1[numscans] = yp1;
            } else {
                if (xp2 < -yp2) goto skipitaddwall;
                xb1[numscans] = 0;
                templong = yp1-yp2+xp1-xp2;
                if (templong == 0) goto skipitaddwall;
                yb1[numscans] = yp1 + scale(yp2-yp1,xp1+yp1,templong);
            }
            if (yb1[numscans] < 256) goto skipitaddwall;

            if (xp2 <= yp2) {
                if ((xp2 < -yp2) || (yp2 == 0)) goto skipitaddwall;
                xb2[numscans] = halfxdimen + scale(xp2,halfxdimen,yp2) - 1;
                if (xp2 >= 0) xb2[numscans]++;
                if (xb2[numscans] >= xdimen) xb2[numscans] = xdimen-1;
                yb2[numscans] = yp2;
            } else {
                if (xp1 > yp1) goto skipitaddwall;
                xb2[numscans] = xdimen-1;
                templong = xp2-xp1+yp1-yp2;
                if (templong == 0) goto skipitaddwall;
                yb2[numscans] = yp1 + scale(yp2-yp1,yp1-xp1,templong);
            }
            if ((yb2[numscans] < 256) || (xb1[numscans] > xb2[numscans])) goto skipitaddwall;

            thesector[numscans] = sectnum;
            thewall[numscans] = z;
            rx1[numscans] = xp1; ry1[numscans] = yp1;
            rx2[numscans] = xp2; ry2[numscans] = yp2;
            p2[numscans] = numscans+1;
            numscans++;

skipitaddwall:
            if ((g_map->walls[z].point2 < z) && (scanfirst < numscans))
                p2[numscans-1] = scanfirst, scanfirst = numscans;
        }

        for(z=numscansbefore; z<numscans; z++)
            if ((g_map->walls[thewall[z]].point2 != thewall[p2[z]]) || (xb2[z] >= xb1[p2[z]]))
                bunchfirst[numbunches++] = p2[z], p2[z] = -1;

        for(z=bunchfrst; z<numbunches; z++) {
            int zz;
            for(zz=bunchfirst[z]; p2[zz]>=0; zz=p2[zz]);
            bunchlast[z] = zz;
        }
    } while (sectorbordercnt > 0);
}

// wallfront, bunchfront (already working)
static int wallfront(int l1, int l2)
{
    BUILD_Wall *wal;
    int x11, y11, x21, y21, x12, y12, x22, y22, dx, dy, t1, t2;
    wal = &g_map->walls[thewall[l1]]; x11 = wal->x; y11 = wal->y;
    wal = &g_map->walls[wal->point2]; x21 = wal->x; y21 = wal->y;
    wal = &g_map->walls[thewall[l2]]; x12 = wal->x; y12 = wal->y;
    wal = &g_map->walls[wal->point2]; x22 = wal->x; y22 = wal->y;
    dx = x21-x11; dy = y21-y11;
    t1 = dmulscale2(x12-x11,dy,-dx,y12-y11);
    t2 = dmulscale2(x22-x11,dy,-dx,y22-y11);
    if (t1 == 0) { t1 = t2; if (t1 == 0) return -1; }
    if (t2 == 0) t2 = t1;
    if ((t1^t2) >= 0) {
        t2 = dmulscale2(globalposx-x11,dy,-dx,globalposy-y11);
        return ((t2^t1) >= 0);
    }
    dx = x22-x12; dy = y22-y12;
    t1 = dmulscale2(x11-x12,dy,-dx,y11-y12);
    t2 = dmulscale2(x21-x12,dy,-dx,y21-y12);
    if (t1 == 0) { t1 = t2; if (t1 == 0) return -1; }
    if (t2 == 0) t2 = t1;
    if ((t1^t2) >= 0) {
        t2 = dmulscale2(globalposx-x12,dy,-dx,globalposy-y12);
        return ((t2^t1) < 0);
    }
    return -2;
}

static int bunchfront(int b1, int b2)
{
    int x1b1, x2b1, x1b2, x2b2, b1f, b2f, i;
    b1f = bunchfirst[b1]; x1b1 = xb1[b1f]; x2b2 = xb2[bunchlast[b2]]+1;
    if (x1b1 >= x2b2) return -1;
    b2f = bunchfirst[b2]; x1b2 = xb1[b2f]; x2b1 = xb2[bunchlast[b1]]+1;
    if (x1b2 >= x2b1) return -1;
    if (x1b1 >= x1b2) {
        for(i=b2f; xb2[i]<x1b1; i=p2[i]);
        return wallfront(b1f,i);
    }
    for(i=b1f; xb2[i]<x1b2; i=p2[i]);
    return wallfront(i,b2f);
}

// ============================================================================
// Phase B: wallmost() - Complete from EDuke32 (engine.cpp:2647-2781)
// ============================================================================
static int32_t wallmost(int16_t *mostbuf, int32_t w, int32_t sectnum, char dastat)
{
    int32_t t, z;
    int32_t xv, yv;

    if (dastat == 0)
    {
        z = g_map->sectors[sectnum].ceilingz-globalposz;
        if ((g_map->sectors[sectnum].ceilingstat&2) == 0)
            return owallmost(mostbuf,w,z);
    }
    else
    {
        z = g_map->sectors[sectnum].floorz-globalposz;
        if ((g_map->sectors[sectnum].floorstat&2) == 0)
            return owallmost(mostbuf,w,z);
    }

    const int wi = thewall[w];
    if (wi == g_map->sectors[sectnum].wallptr)
        return owallmost(mostbuf,w,z);

    BUILD_Wall *wal = &g_map->walls[wi];
    const int32_t x1 = wal->x, x2 = g_map->walls[wal->point2].x-x1;
    const int32_t y1 = wal->y, y2 = g_map->walls[wal->point2].y-y1;

    const int w1 = g_map->sectors[sectnum].wallptr, w2 = g_map->walls[w1].point2;
    const int32_t dx = g_map->walls[w2].x-g_map->walls[w1].x, dy = g_map->walls[w2].y-g_map->walls[w1].y;
    const int32_t dasqr = krecipasm(nsqrtasm(uhypsq(dx,dy)));

    if (dastat == 0)
    {
        t = mulscale16(g_map->sectors[sectnum].ceilingheinum, dasqr) >> 1;
        z = g_map->sectors[sectnum].ceilingz;
    }
    else
    {
        t = mulscale16(g_map->sectors[sectnum].floorheinum,dasqr) >> 1;
        z = g_map->sectors[sectnum].floorz;
    }


    if (xb1[w] == 0)
        { xv = cosglobalang+sinviewingrangeglobalang; yv = singlobalang-cosviewingrangeglobalang; }
    else
        { xv = x1-globalposx; yv = y1-globalposy; }
    zint_t z1 = wallmost_getz(w1, t, z, x1, y1, x2, y2, xv, yv, dx, dy);


    if (xb2[w] == xdimen-1)
        { xv = cosglobalang-sinviewingrangeglobalang; yv = singlobalang+cosviewingrangeglobalang; }
    else
        { xv = (x2+x1)-globalposx; yv = (y2+y1)-globalposy; }
    zint_t z2 = wallmost_getz(w1, t, z, x1, y1, x2, y2, xv, yv, dx, dy);


    const zint_t s1 = mulscale20(globaluclip,yb1[w]), s2 = mulscale20(globaluclip,yb2[w]);
    const zint_t s3 = mulscale20(globaldclip,yb1[w]), s4 = mulscale20(globaldclip,yb2[w]);
    const int32_t bad = (z1<s1)+((z2<s2)<<1)+((z1>s3)<<2)+((z2>s4)<<3);

    int32_t ix1 = xb1[w], ix2 = xb2[w];
    int32_t iy1 = yb1[w], iy2 = yb2[w];

    if ((bad&3) == 3)
    {
        for (int i=ix1; i<=ix2; i++)
            mostbuf[i] = 0;
        return bad;
    }

    if ((bad&12) == 12)
    {
        for (int i=ix1; i<=ix2; i++)
            mostbuf[i] = ydimen;
        return bad;
    }

    const int32_t oz1 = z1, oz2 = z2;

    if (bad&3)
    {
        //inty = intz / (globaluclip>>16)
        t = divscale30(oz1-s1,s2-s1+oz1-oz2);
        int32_t inty = yb1[w] + mulscale30(yb2[w]-yb1[w],t);
        int32_t intz = oz1 + mulscale30(oz2-oz1,t);
        int32_t xcross = xb1[w] + scale(mulscale30(yb2[w],t),xb2[w]-xb1[w],inty);

        //t = divscale30((x1<<4)-xcross*yb1[w],xcross*(yb2[w]-yb1[w])-((x2-x1)<<4));
        //inty = yb1[w] + mulscale30(yb2[w]-yb1[w],t);
        //intz = z1 + mulscale30(z2-z1,t);

        if ((bad&3) == 2)
        {
            if (xb1[w] <= xcross) { z2 = intz; iy2 = inty; ix2 = xcross; }
            for (int i=xcross+1; i<=xb2[w]; i++)
                mostbuf[i] = 0;
        }
        else
        {
            if (xcross <= xb2[w]) { z1 = intz; iy1 = inty; ix1 = xcross; }
            for (int i=xb1[w]; i<=xcross; i++)
                mostbuf[i] = 0;
        }
    }

    if (bad&12)
    {
        //inty = intz / (globaldclip>>16)
        t = divscale30(oz1-s3,s4-s3+oz1-oz2);
        int32_t inty = yb1[w] + mulscale30(yb2[w]-yb1[w],t);
        int32_t intz = oz1 + mulscale30(oz2-oz1,t);
        int32_t xcross = xb1[w] + scale(mulscale30(yb2[w],t),xb2[w]-xb1[w],inty);

        //t = divscale30((x1<<4)-xcross*yb1[w],xcross*(yb2[w]-yb1[w])-((x2-x1)<<4));
        //inty = yb1[w] + mulscale30(yb2[w]-yb1[w],t);
        //intz = z1 + mulscale30(z2-z1,t);

        if ((bad&12) == 8)
        {
            if (xb1[w] <= xcross) { z2 = intz; iy2 = inty; ix2 = xcross; }
            for (int i=xcross+1; i<=xb2[w]; i++)
                mostbuf[i] = ydimen;
        }
        else
        {
            if (xcross <= xb2[w]) { z1 = intz; iy1 = inty; ix1 = xcross; }
            for (int i=xb1[w]; i<=xcross; i++)
                mostbuf[i] = ydimen;
        }
    }

    wallmosts_finish(mostbuf, z1, z2, ix1, iy1, ix2, iy2);

    return bad;
}

// owallmost (simple, for flat sectors) - kept for fallback
static int owallmost(short *mostbuf, int w, int z)
{
    int bad, inty, xcross, y, yinc, s1, s2, s3, s4, ix1, ix2, iy1, iy2, t, i;
    z <<= 7;
    s1 = mulscale20(globaluclip,yb1[w]); s2 = mulscale20(globaluclip,yb2[w]);
    s3 = mulscale20(globaldclip,yb1[w]); s4 = mulscale20(globaldclip,yb2[w]);
    bad = (z<s1)+((z<s2)<<1)+((z>s3)<<2)+((z>s4)<<3);
    ix1 = xb1[w]; iy1 = yb1[w]; ix2 = xb2[w]; iy2 = yb2[w];
    if ((bad&3) == 3) { for (i=ix1; i<=ix2; i++) mostbuf[i] = 0; return bad; }
    if ((bad&12) == 12) { for (i=ix1; i<=ix2; i++) mostbuf[i] = ydimen; return bad; }
    if (bad&3) {
        t = divscale30(z-s1,s2-s1);
        inty = yb1[w] + mulscale30(yb2[w]-yb1[w],t);
        xcross = xb1[w] + scale(mulscale30(yb2[w],t),xb2[w]-xb1[w],inty);
        if ((bad&3) == 2) {
            if (xb1[w] <= xcross) { iy2 = inty; ix2 = xcross; }
            for (i=xcross+1; i<=xb2[w]; i++) mostbuf[i] = 0;
        } else {
            if (xcross <= xb2[w]) { iy1 = inty; ix1 = xcross; }
            for (i=xb1[w]; i<=xcross; i++) mostbuf[i] = 0;
        }
    }
    if (bad&12) {
        t = divscale30(z-s3,s4-s3);
        inty = yb1[w] + mulscale30(yb2[w]-yb1[w],t);
        xcross = xb1[w] + scale(mulscale30(yb2[w],t),xb2[w]-xb1[w],inty);
        if ((bad&12) == 8) {
            if (xb1[w] <= xcross) { iy2 = inty; ix2 = xcross; }
            for (i=xcross+1; i<=xb2[w]; i++) mostbuf[i] = ydimen;
        } else {
            if (xcross <= xb2[w]) { iy1 = inty; ix1 = xcross; }
            for (i=xb1[w]; i<=xcross; i++) mostbuf[i] = ydimen;
        }
    }
    y = (scale(z,xdimenscale,iy1)<<4);
    yinc = ((scale(z,xdimenscale,iy2)<<4)-y) / (ix2-ix1+1);
    qinterpolatedown16short(&mostbuf[ix1],ix2-ix1+1,y+(globalhoriz<<16),yinc);
    if (mostbuf[ix1] < 0) mostbuf[ix1] = 0;
    if (mostbuf[ix1] > ydimen) mostbuf[ix1] = ydimen;
    if (mostbuf[ix2] < 0) mostbuf[ix2] = 0;
    if (mostbuf[ix2] > ydimen) mostbuf[ix2] = ydimen;
    return bad;
}

// prepwall - Complete original from Build Engine (engine.cpp:2390-2478)
static void prepwall(int z, BUILD_Wall *wal)
{
    int32_t l=0, ol=0, x;

    int32_t walxrepeat = (wal->xrepeat<<3);
    // Note: globalht (hightile) not used in this port

    //lwall calculation
    int32_t tmpx = xb1[z]-halfxdimen;

    const int32_t topinc = -(ry1[z]>>2);
    const int32_t botinc = (ry2[z]-ry1[z])>>8;
    int32_t top = mulscale5(rx1[z],xdimen) + mulscale2(topinc,tmpx);
    int32_t bot = mulscale11(rx1[z]-rx2[z],xdimen) + mulscale2(botinc,tmpx);

    const int32_t splc = mulscale19(ry1[z],xdimscale);
    const int32_t sinc = mulscale16(ry2[z]-ry1[z],xdimscale);

    x = xb1[z];
    if (bot != 0)
    {
        l = divscale12(top,bot);
        swall[x] = mulscale21(l,sinc)+splc;
        l *= walxrepeat;
        lwall[x] = (l>>18);
    }
    while (x+4 <= xb2[z])
    {
        int32_t i;

        top += topinc; bot += botinc;
        if (bot != 0)
        {
            ol = l; l = divscale12(top,bot);
            swall[x+4] = mulscale21(l,sinc)+splc;
            l *= walxrepeat;
            lwall[x+4] = (l>>18);
        }

        i = (ol+l)>>1;

        lwall[x+2] = i>>18;
        lwall[x+1] = (ol+i)>>19;
        lwall[x+3] = (l+i)>>19;

        swall[x+2] = (swall[x]+swall[x+4])>>1;
        swall[x+1] = (swall[x]+swall[x+2])>>1;
        swall[x+3] = (swall[x+4]+swall[x+2])>>1;

        x += 4;
    }
    if (x+2 <= xb2[z])
    {
        top += (topinc>>1); bot += (botinc>>1);
        if (bot != 0)
        {
            ol = l; l = divscale12(top,bot);
            swall[x+2] = mulscale21(l,sinc)+splc;
            l *= walxrepeat;
            lwall[x+2] = (l>>18);
        }
        lwall[x+1] = (l+ol)>>19;
        swall[x+1] = (swall[x]+swall[x+2])>>1;
        x += 2;
    }
    if (x+1 <= xb2[z])
    {
        bot += (botinc>>2);
        if (bot != 0)
        {
            l = divscale12(top+(topinc>>2),bot);
            swall[x+1] = mulscale21(l,sinc)+splc;
            lwall[x+1] = mulscale18(l,walxrepeat);
        }
    }

    if (lwall[xb1[z]] < 0)
        lwall[xb1[z]] = 0;
    if (lwall[xb2[z]] >= walxrepeat && walxrepeat)
        lwall[xb2[z]] = walxrepeat-1;

    if (wal->cstat&8)
    {
        walxrepeat--;
        for (x=xb1[z]; x<=xb2[z]; x++)
            lwall[x] = walxrepeat-lwall[x];
    }
}

// ============================================================================
// Phase B: wallscan() - Complete C version (based on engine.cpp:3088-3234)
// ============================================================================
static void wallscan(int x1, int x2, short *uwal, short *dwal, int *swal, int *lwal)
{
    if (globalpicnum < 0 || globalpicnum >= MAXTILES) return;
    
    BUILD_Tile *tile = build_get_tile(globalpicnum);
    if (!tile || !tile->data) {
        // Fallback to gray for missing tiles
        uint32_t gray = SDL_MapRGB(gPixelFormat, 128, 128, 128);
        for (int x = x1; x <= x2; x++) {
            int y1 = (uwal[x] > umost[x]) ? uwal[x] : umost[x];
            int y2 = (dwal[x] < dmost[x]) ? dwal[x] : dmost[x];
            for (int y = y1; y < y2 && y < ydimen; y++)
                gr_put_pixel(render_buffer, x, y, gray);
        }
        return;
    }
    
    int tsizx = tile->xsize;
    int tsizy = tile->ysize;
    if (tsizx <= 0 || tsizy <= 0) return;
    
    // Check if any pixels will be drawn
    if ((uwal[x1] > ydimen) && (uwal[x2] > ydimen)) return;
    if ((dwal[x1] < 0) && (dwal[x2] < 0)) return;
    
    // Calculate log2 of texture dimensions
    int tsizx_bits = picsiz[globalpicnum] & 15;
    int tsizy_bits = (picsiz[globalpicnum] >> 4);
    
    // Main rendering loop - process each column
    int x = x1;
    while ((x <= x2) && (umost[x] > dmost[x]))
        x++;
    
    for (; x <= x2; x++) {
        int y1 = (uwal[x] > umost[x]) ? uwal[x] : umost[x];
        int y2 = (dwal[x] < dmost[x]) ? dwal[x] : dmost[x];
        if (y2 <= y1) continue;
        
        // Calculate palette lookup offset for shading
        int palookup_offset = getpalookupsh(mulscale16(swal[x], globvis));
        
        // Calculate buffer pointer (U coordinate)
        intptr_t bufplc;
        calc_bufplc(&bufplc, lwal[x], tsizx_bits, tsizy_bits);
        
        // Calculate V coordinate and increment
        uint32_t vplc;
        int32_t vinc;
        calc_vplcinc_wall(&vplc, &vinc, swal[x], y1);
        
        // Draw vertical column
        uint8_t *tile_column = (uint8_t *)bufplc;
        for (int y = y1; y < y2 && y < ydimen; y++) {
            // Get V coordinate (texture Y)
            int v = (vplc >> globalshiftval);
            if (v < 0) v = 0;
            if (v >= tsizy) v = tsizy - 1;
            
            // Get palette index from tile data (column-major storage)
            int palindex = tile_column[v];
            
            // Apply shading if we have palookup
            if (g_palookup && palookup_offset >= 0 && palookup_offset < g_numshades * 256) {
                palindex = g_palookup[palookup_offset + palindex];
            }
            
            // Draw pixel
            uint32_t color = g_palette_cache[palindex];
            gr_put_pixel(render_buffer, x, y, color);
            
            // Increment V coordinate
            vplc += vinc;
        }
    }
}
// ============================================================================
// Phase C: grouscan() - Floor/Ceiling rendering (C version)
// Based on EDuke32 engine.cpp:3950-4171
// ============================================================================

static void grouscan(int32_t dax1, int32_t dax2, int32_t sectnum, char dastat)
{
    int32_t i, l, x, y, dx, dy, wx, wy, y1, y2, daz;
    int32_t daslope, dasqr;
    int32_t shoffs, m1, m2, shy1, shy2;
    intptr_t *mptr1, *mptr2, j;
    
    BUILD_Sector *sec = &g_map->sectors[sectnum];
    BUILD_Wall *wal;
    
    // Setup - get sector info based on ceiling (0) or floor (1)
    if (dastat == 0) {
        // Ceiling
        if (globalposz <= getceilzofslope(sectnum, globalposx, globalposy))
            return;  // Back-face culling
        
        globalorientation = sec->ceilingstat;
        globalpicnum = sec->ceilingpicnum;
        globalshade = sec->ceilingshade;
        globalpal = sec->ceilingpal;
        daslope = sec->ceilingheinum;
        daz = sec->ceilingz;
    } else {
        // Floor
        if (globalposz >= getflorzofslope(sectnum, globalposx, globalposy))
            return;  // Back-face culling
        
        globalorientation = sec->floorstat;
        globalpicnum = sec->floorpicnum;
        globalshade = sec->floorshade;
        globalpal = sec->floorpal;
        daslope = sec->floorheinum;
        daz = sec->floorz;
    }
    
    // Check if tile exists
    if ((unsigned)globalpicnum >= (unsigned)MAXTILES) return;
    if (tilesizx[globalpicnum] <= 0 || tilesizy[globalpicnum] <= 0) return;
    
    // Load tile
    if (waloff[globalpicnum] == 0) loadtile(globalpicnum);
    globalbufplc = waloff[globalpicnum];
    
    // Get first wall for slope calculations
    wal = &g_map->walls[sec->wallptr];
    BUILD_Wall *wal2 = &g_map->walls[wal->point2];
    wx = wal2->x - wal->x;
    wy = wal2->y - wal->y;
    dasqr = krecipasm(nsqrtasm(uhypsq(wx, wy)));
    i = mulscale21(daslope, dasqr);
    wx *= i;
    wy *= i;
    
    // Setup perspective globals
    globalx = -mulscale19(singlobalang, xdimenrecip);
    globaly = mulscale19(cosglobalang, xdimenrecip);
    globalx1 = (globalposx << 8);
    globaly1 = -(globalposy << 8);
    i = (dax1 - halfxdimen) * xdimenrecip;
    globalx2 = mulscale16(cosglobalang << 4, viewingrangerecip) - mulscale27(singlobalang, i);
    globaly2 = mulscale16(singlobalang << 4, viewingrangerecip) + mulscale27(cosglobalang, i);
    globalzd = (int64_t)xdimscale << 9;
    globalzx = -dmulscale(wx, globaly2, -wy, globalx2, 17) + mulscale10(1 - globalhoriz, globalzd);
    globalz = -dmulscale(wx, globaly, -wy, globalx, 25);
    
    // Handle relative alignment
    if (globalorientation & 64) {
        dx = mulscale14(wal2->x - wal->x, dasqr);
        dy = mulscale14(wal2->y - wal->y, dasqr);
        i = nsqrtasm(daslope * daslope + 16777216);
        
        int32_t tempx = globalx, tempy = globaly;
        globalx = dmulscale(tempx, dx, tempy, dy, 16);
        globaly = mulscale12(dmulscale(-tempy, dx, tempx, dy, 16), i);
        
        x = ((wal->x - globalposx) << 8);
        y = ((wal->y - globalposy) << 8);
        globalx1 = dmulscale(-x, dx, -y, dy, 16);
        globaly1 = mulscale12(dmulscale(-y, dx, x, dy, 16), i);
        
        tempx = globalx2; tempy = globaly2;
        globalx2 = dmulscale(tempx, dx, tempy, dy, 16);
        globaly2 = mulscale12(dmulscale(-tempy, dx, tempx, dy, 16), i);
    }
    
    // Handle flipping
    if (globalorientation & 0x4) {
        i = globalx; globalx = -globaly; globaly = -i;
        i = globalx1; globalx1 = globaly1; globaly1 = i;
        i = globalx2; globalx2 = -globaly2; globaly2 = -i;
    }
    if (globalorientation & 0x10) { globalx1 = -globalx1; globalx2 = -globalx2; globalx = -globalx; }
    if (globalorientation & 0x20) { globaly1 = -globaly1; globaly2 = -globaly2; globaly = -globaly; }
    
    daz = dmulscale(wx, globalposy - wal->y, -wy, globalposx - wal->x, 9) + ((daz - globalposz) << 8);
    globalx2 = mulscale20(globalx2, daz);
    globalx = mulscale28(globalx, daz);
    globaly2 = mulscale20(globaly2, -daz);
    globaly = mulscale28(globaly, -daz);
    
    // Texture coordinate setup
    i = 8 - (picsiz[globalpicnum] & 15);
    int jj = 8 - (picsiz[globalpicnum] >> 4);
    if (globalorientation & 8) { i++; jj++; }
    
    globalx1 <<= (i + 12);
    globaly1 <<= (jj + 12);
    
    if (i >= 0) { globalx2 <<= i; globalx <<= i; }
    else { globalx2 >>= -i; globalx >>= -i; }
    
    if (jj >= 0) { globaly2 <<= jj; globaly <<= jj; }
    else { globaly2 >>= -jj; globaly >>= -jj; }
    
    // Apply panning
    if (dastat == 0) {
        globalx1 += (uint32_t)sec->ceilingxpanning << 24;
        globaly1 += (uint32_t)sec->ceilingypanning << 24;
    } else {
        globalx1 += (uint32_t)sec->floorxpanning << 24;
        globaly1 += (uint32_t)sec->floorypanning << 24;
    }
    
    // Visibility calculation
    asm1 = -(globalzd >> (16 - 3));  // BITSOFPRECISION = 3
    
    int32_t vis = (sec->visibility != 0) ? 
        mulscale4(globalvisibility, (uint8_t)(sec->visibility + 16)) : globalvisibility;
    globvis = ((((int64_t)(vis * (int64_t)daz)) >> 13) * xdimscale) >> 16;
    
    j = (intptr_t)&g_palookup[globalpal * 256];
    
    // Setup texture info for slopevlin
    glogx = picsiz[globalpicnum] & 15;
    glogy = picsiz[globalpicnum] >> 4;
    gbuf = (char *)globalbufplc;
    
    l = (globalzd >> 16);
    
    int32_t shinc = mulscale16(globalz, xdimenscale);
    
    shoffs = (shinc > 0) ? (4 << 15) : ((16380 - ydimen) << 15);
    y1 = (dastat == 0) ? umost[dax1] : ((umost[dax1] > dplc[dax1]) ? umost[dax1] : dplc[dax1]);
    
    m1 = mulscale16(y1, globalzd) + (globalzx >> 6);
    m1 += klabs((int32_t)(globalzd >> 16));
    m2 = m1 + l;
    shy1 = y1 + (shoffs >> 15);
    
    if ((unsigned)shy1 >= SLOPALOOKUPSIZ - 1) return;
    
    mptr1 = &slopalookup[shy1];
    mptr2 = mptr1 + 1;
    
    // Main rendering loop
    for (x = dax1; x <= dax2; x++) {
        if (dastat == 0) {
            y1 = umost[x];
            y2 = (dmost[x] < uplc[x]) ? dmost[x] : uplc[x];
            y2--;
        } else {
            y1 = (umost[x] > dplc[x]) ? umost[x] : dplc[x];
            y2 = dmost[x] - 1;
        }
        
        if (y1 <= y2) {
            shy1 = y1 + (shoffs >> 15);
            shy2 = y2 + (shoffs >> 15);
            
            if ((unsigned)shy1 >= SLOPALOOKUPSIZ) goto next_most;
            if ((unsigned)shy2 >= SLOPALOOKUPSIZ) goto next_most;
            
            intptr_t *nptr1 = &slopalookup[shy1];
            intptr_t *nptr2 = &slopalookup[shy2];
            
            // Update slopalookup with shading
            while (nptr1 <= mptr1) {
                *mptr1-- = j + getpalookupsh(mulscale24(krecipasm(m1), globvis));
                m1 -= l;
            }
            while (nptr2 >= mptr2) {
                *mptr2++ = j + getpalookupsh(mulscale24(krecipasm(m2), globvis));
                m2 += l;
            }
            
            globalx3 = (globalx2 >> 10);
            globaly3 = (globaly2 >> 10);
            asm3 = mulscale16(y2, globalzd) + (globalzx >> 6);
            
            // Draw the column using slopevlin (pass screen coordinates)
            slopevlin(x, y2, krecipasm(asm3 >> 3), (intptr_t)nptr2, y2 - y1 + 1, globalx1, globaly1);
        }
        
next_most:
        globalx2 += globalx;
        globaly2 += globaly;
        globalzx += globalz;
        shoffs += shinc;
    }
}


// ============================================================================
// drawalls - Updated for Phase B textured rendering
// ============================================================================
static void drawalls(int bunch)
{
    BUILD_Sector *sec;
    BUILD_Wall *wal;
    int z, x, x1, x2, sectnum, wallnum;
    unsigned char andwstat1, andwstat2;

    z = bunchfirst[bunch];
    sectnum = thesector[z];
    sec = &g_map->sectors[sectnum];

    // Calculate ceiling/floor heights using wallmost() for slope support
    andwstat1 = 0xff; andwstat2 = 0xff;
    for(; z>=0; z=p2[z]) {
        andwstat1 &= wallmost(uplc,z,sectnum,0);  // 0 = ceiling
        andwstat2 &= wallmost(dplc,z,sectnum,1);  // 1 = floor
    }

    // Phase C: Draw textured ceiling and floor using grouscan()
    // Find the X range for this bunch
    int bunch_x1 = xdimen, bunch_x2 = 0;
    for(z=bunchfirst[bunch]; z>=0; z=p2[z]) {
        if (xb1[z] < bunch_x1) bunch_x1 = xb1[z];
        if (xb2[z] > bunch_x2) bunch_x2 = xb2[z];
    }
    if (bunch_x1 < 0) bunch_x1 = 0;
    if (bunch_x2 >= xdimen) bunch_x2 = xdimen - 1;
    
    // Draw ceiling (dastat = 0)
    if ((sec->ceilingstat & 1) == 0) {  // Not parallax sky
        grouscan(bunch_x1, bunch_x2, sectnum, 0);
    }
    
    // Draw floor (dastat = 1)
    grouscan(bunch_x1, bunch_x2, sectnum, 1);
    
    // Draw textured walls
    for(z=bunchfirst[bunch]; z>=0; z=p2[z]) {
        x1 = xb1[z]; x2 = xb2[z];
        if (x1 < 0) x1 = 0;
        if (x2 >= xdimen) x2 = xdimen - 1;
        
        wallnum = thewall[z];
        wal = &g_map->walls[wallnum];
        
        // Setup texture globals
        globalorientation = (int)wal->cstat;
        globalpicnum = wal->picnum;
        if ((unsigned)globalpicnum >= (unsigned)MAXTILES) globalpicnum = 0;
        globalxpanning = (int)wal->xpanning;
        globalypanning = (int)wal->ypanning;
        
        // Calculate globalshiftval (from engine.cpp:4406-4426)
        int logtilesizy = (picsiz[globalpicnum]>>4);
        if (logtilesizy > 0 && pow2long(logtilesizy) != tilesizy[globalpicnum]) logtilesizy++;
        
        globaltilesizy = logtilesizy;  // Save for V coordinate calculation in wallscan
        
        // For power-of-two textures:
        globalshiftval = 32-logtilesizy;  // This is used in globalyscale calculation
        globalyscale = wal->yrepeat<<(globalshiftval-19);
        
        globalshade = (int)wal->shade;
        globvis = globalvisibility;
        globalpal = (int)wal->pal;
        
        // globalzd calculation (engine.cpp:4428-4433)
        if ((globalorientation&4) == 0)
            globalzd = (((globalposz-sec->ceilingz)*globalyscale)<<8);
        else  // bottom-aligned
            globalzd = (((globalposz-sec->floorz)*globalyscale)<<8);
        
        globalzd += (globalypanning<<24);
        
        if (globalorientation&256) globalyscale = -globalyscale, globalzd = -globalzd;
        
        // Calculate texture coordinates
        prepwall(z, wal);
        
        // Render textured wall
        wallscan(x1, x2, uplc, dplc, swall, lwall);
    }
    
    // Update occlusion
    for(z=bunchfirst[bunch]; z>=0; z=p2[z]) {
        x1 = xb1[z]; x2 = xb2[z];
        if (x1 < 0) x1 = 0;
        if (x2 >= xdimen) x2 = xdimen - 1;
        
        for(x=x1; x<=x2; x++) {
            if (uplc[x] > umost[x]) {
                umost[x] = uplc[x];
                if (umost[x] > dmost[x]) numhits--;
            }
            if (dplc[x] < dmost[x]) {
                dmost[x] = dplc[x];
                if (umost[x] > dmost[x]) numhits--;
            }
        }
    }
}

// ============================================================================
// build_render_frame - Main entry
// ============================================================================
void build_render_frame(BUILD_Map *map, BUILD_Camera *camera, int width, int height)
{
    if (!map || !camera) return;
    g_map = map;

    // Initialize tile tables once
    static int tiles_initialized = 0;
    if (!tiles_initialized) {
        init_build_tables();  // Initialize lookup tables for math functions
        init_tile_tables();
        tiles_initialized = 1;
    }

    if (!render_buffer || render_buffer->width != width || render_buffer->height != height) {
        if (render_buffer) bitmap_destroy(render_buffer);
        render_buffer = bitmap_new_syslib(width, height);
        if (!render_buffer) return;
    }

    static int last_width = 0;
    if (!umost || last_width != width) {
        if (umost) free(umost); if (dmost) free(dmost);
        if (uplc) free(uplc); if (dplc) free(dplc);
        if (swall) free(swall); if (lwall) free(lwall); if (dwall) free(dwall);
        umost = (short *)malloc(width * sizeof(short));
        dmost = (short *)malloc(width * sizeof(short));
        uplc = (short *)malloc(width * sizeof(short));
        dplc = (short *)malloc(width * sizeof(short));
        swall = (int *)malloc(width * sizeof(int));
        lwall = (int *)malloc(width * sizeof(int));
        dwall = (short *)malloc(width * sizeof(short));
        last_width = width;
    }

    xdimen = width; ydimen = height; halfxdimen = (xdimen >> 1);
    globalposx = camera->x; globalposy = camera->y; globalposz = camera->z;
    globalang = camera->ang & 2047; globalcursectnum = camera->cursectnum;
    
    int32_t yxaspect = 65536;
    xdimenscale = scale(xdimen, yxaspect, 320);
    xdimscale = scale(320, yxaspect, xdimen);
    
    // Initialize reciprocals for perspective calculations
    xdimenrecip = divscale32(1, xdimen);
    viewingrangerecip = divscale32(1, xdimen);  // Simplified, should be viewingrange but using xdimen for now
    
    globalhoriz = mulscale16(camera->horiz - 100, xdimenscale) + (ydimen >> 1);
    globaluclip = (0-globalhoriz)*xdimscale;
    globaldclip = (ydimen-globalhoriz)*xdimscale;
    
    cosglobalang = (int32_t)(cos(globalang * M_PI / 1024.0) * 16383.0);
    singlobalang = (int32_t)(sin(globalang * M_PI / 1024.0) * 16383.0);
    int viewingrange = 65536;
    cosviewingrangeglobalang = mulscale16(cosglobalang, viewingrange);
    sinviewingrangeglobalang = mulscale16(singlobalang, viewingrange);
    
    uint32_t clear_color = SDL_MapRGB(gPixelFormat, 0, 0, 0);
    for (int y = 0; y < height; y++)
        for(int x = 0; x < width; x++)
            gr_put_pixel(render_buffer, x, y, clear_color);
    
    for (int i = 0; i < xdimen; i++) { umost[i] = 0; dmost[i] = ydimen-1; }
    memset(gotsector, 0, sizeof(gotsector));
    numscans = 0; numbunches = 0; numhits = xdimen;
    
    scansector(globalcursectnum);
    
    while ((numbunches > 0) && (numhits > 0)) {
        int closest = 0;
        for(int i=1; i<numbunches; i++) {
            int j = bunchfront(i,closest);
            if (j < 0) continue;
            if (j == 0) closest = i;
        }
        drawalls(closest);
        numbunches--;
        bunchfirst[closest] = bunchfirst[numbunches];
        bunchlast[closest] = bunchlast[numbunches];
    }
    
    int cx = width / 2, cy = height / 2;
    uint32_t cross_col = SDL_MapRGB(gPixelFormat, 255, 255, 255);
    gr_put_pixel(render_buffer, cx, cy, cross_col);
    gr_put_pixel(render_buffer, cx-1, cy, cross_col);
    gr_put_pixel(render_buffer, cx+1, cy, cross_col);
    gr_put_pixel(render_buffer, cx, cy-1, cross_col);
    gr_put_pixel(render_buffer, cx, cy+1, cross_col);
}

int build_get_render_buffer_code(void) {
    return render_buffer ? render_buffer->code : 0;
}

void build_cleanup_render(void) {
    if (render_buffer) { bitmap_destroy(render_buffer); render_buffer = NULL; }
    if (umost) { free(umost); umost = NULL; }
    if (dmost) { free(dmost); dmost = NULL; }
    if (uplc) { free(uplc); uplc = NULL; }
    if (dplc) { free(dplc); dplc = NULL; }
    if (swall) { free(swall); swall = NULL; }
    if (lwall) { free(lwall); lwall = NULL; }
    if (dwall) { free(dwall); dwall = NULL; }
}
