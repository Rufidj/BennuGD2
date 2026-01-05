//
// Doom Rendering System - Main Header
// Adaptado de r_main.h y r_defs.h del motor Doom original
//

#ifndef __DOOM_RENDER_H
#define __DOOM_RENDER_H

#include "libmod_doom.h"

// ============================================================================
// CONSTANTES DE RENDERIZADO
// ============================================================================

#define SCREENWIDTH  320
#define SCREENHEIGHT 200

#define MAXWIDTH  1120
#define MAXHEIGHT 832

#define FIELDOFVIEW 2048    // Fineangles en el campo de visión DOOM

// ============================================================================
// VARIABLES GLOBALES DE RENDERIZADO
// ============================================================================

// Dimensiones de la pantalla
extern int viewwidth;
extern int viewheight;
extern int scaledviewwidth;
extern int viewwindowx;
extern int viewwindowy;

// Tablas de lookup para renderizado
extern int* viewangletox;
extern angle_t* xtoviewangle;

// Clipping
extern short* screenheightarray;
extern short* negonearray;
extern short* floorclip;
extern short* ceilingclip;

// ============================================================================
// ESTRUCTURAS DE RENDERIZADO (Originales Doom)
// ============================================================================

// ESTRUCTURAS DE RENDERIZADO (Originales Doom)
// ============================================================================

// MAXDRAWSEGS definido en r_defs.h original
// drawseg_t ya definido en libmod_doom.h
#define MAXSEGS         32

extern drawseg_t    drawsegs[MAXDRAWSEGS];
extern drawseg_t*   ds_p; // Puntero al drawseg actual

// Clipping de segmentos - Solidsegs
// (Rangos horizontales ocluidos)
typedef struct
{
    int     first;
    int     last;
} cliprange_t;

#define MAXSEGS 32 // Definido arriba

extern cliprange_t  solidsegs[MAXSEGS];
extern cliprange_t* newend; // Puntero al final de solidsegs

// Angulos de clip
extern angle_t      clipangle;
extern angle_t      doubleclipangle;

// Funciones helper
angle_t R_PointToAngle(fixed_t x, fixed_t y);
int R_PointOnSide(fixed_t x, fixed_t y, node_t* node);
void R_StoreWallRange(int start, int stop);
boolean R_CheckBBox(fixed_t* bspcoord);

// Distancias de proyección
extern fixed_t projection;
extern fixed_t projectiony;
extern fixed_t centerxfrac;
extern fixed_t centeryfrac;

// ============================================================================
// FUNCIONES DE RENDERIZADO
// ============================================================================

// Inicialización
void R_Init(void);
void R_InitData(void);
void R_InitTables(void);
void R_InitPlanes(void);
void R_InitLightTables(void);
void R_InitSkyMap(void);

// Renderizado principal
void R_RenderPlayerView(void);
void R_SetupFrame(void);
void R_CheckResize(int width, int height);

// BSP
void R_RenderBSPNode(int bspnum);
void R_Subsector(int num);
void R_AddLine(seg_t* line);

// Clipping
void R_ClearClipSegs(void);
void R_ClearDrawSegs(void);

// Planos (suelos y techos)
void R_ClearPlanes(void);
void R_DrawPlanes(void);

// Sprites
void R_ClearSprites(void);
void R_DrawMasked(void);

#endif // __DOOM_RENDER_H
