#ifndef __LIBMOD_DOOM_H
#define __LIBMOD_DOOM_H

#include <stdint.h>
#include <math.h>
#include <string.h>

/* Inclusiones necesarias de BennuGD2 */
#include "bgddl.h"
#include "libbggfx.h"
#include "g_bitmap.h"
#include "g_blit.h"
#include "g_pixel.h"
#include "g_clear.h"
#include "g_grlib.h"
#include "xstrings.h"

// ============================================================================
// TIPOS BÁSICOS DE DOOM - Adaptados de doomtype.h
// ============================================================================

typedef uint8_t byte;
typedef int boolean;

#define false 0
#define true 1

// ============================================================================
// MATEMÁTICAS DE PUNTO FIJO - De m_fixed.h
// ============================================================================

// Fixed point, 32bit as 16.16.
#define FRACBITS        16
#define FRACUNIT        (1<<FRACBITS)

typedef int fixed_t;

fixed_t FixedMul(fixed_t a, fixed_t b);
fixed_t FixedDiv(fixed_t a, fixed_t b);

// ============================================================================
// TABLAS TRIGONOMÉTRICAS - De tables.h
// ============================================================================

#define FINEANGLES      8192
#define FINEMASK        (FINEANGLES-1)
#define ANGLETOFINESHIFT 19

// Binary Angle Measurement, BAM.
#define ANG45           0x20000000
#define ANG90           0x40000000
#define ANG180          0x80000000
#define ANG270          0xc0000000
#define ANG_MAX         0xffffffff
#define ANG1            (ANG45 / 45)
#define ANG60           (ANG180 / 3)

#define SLOPERANGE      2048
#define SLOPEBITS       11
#define DBITS           (FRACBITS-SLOPEBITS)

typedef unsigned angle_t;

// Tablas precalculadas
extern const fixed_t finesine[5*FINEANGLES/4];
extern const fixed_t *finecosine;
extern const fixed_t finetangent[FINEANGLES/2];
extern const angle_t tantoangle[SLOPERANGE+1];

int SlopeDiv(unsigned int num, unsigned int den);

// ============================================================================
// ESTRUCTURAS DE DATOS DEL MAPA - De r_defs.h y doomdata.h
// ============================================================================

// Vértice
typedef struct
{
    fixed_t x;
    fixed_t y;
} vertex_t;

// Forward declarations
struct line_s;
struct sector_s;

// Degenmobj para sonido de sectores
typedef struct
{
    fixed_t x;
    fixed_t y;
    fixed_t z;
} degenmobj_t;

// Sector
typedef struct sector_s
{
    fixed_t floorheight;
    fixed_t ceilingheight;
    short floorpic;
    short ceilingpic;
    short lightlevel;
    short special;
    short tag;
    
    int soundtraversed;
    void* soundtarget;
    int blockbox[4];
    degenmobj_t soundorg;
    int validcount;
    void* thinglist;
    void* specialdata;
    
    int linecount;
    struct line_s** lines;
} sector_t;

// SideDef
typedef struct
{
    fixed_t textureoffset;
    fixed_t rowoffset;
    short toptexture;
    short bottomtexture;
    short midtexture;
    sector_t* sector;
} side_t;

// Tipo de slope para LineDef
typedef enum
{
    ST_HORIZONTAL,
    ST_VERTICAL,
    ST_POSITIVE,
    ST_NEGATIVE
} slopetype_t;

// LineDef
typedef struct line_s
{
    vertex_t* v1;
    vertex_t* v2;
    fixed_t dx;
    fixed_t dy;
    short flags;
    short special;
    short tag;
    short sidenum[2];
    fixed_t bbox[4];
    slopetype_t slopetype;
    sector_t* frontsector;
    sector_t* backsector;
    int validcount;
    void* specialdata;
} line_t;

// Subsector
typedef struct subsector_s
{
    sector_t* sector;
    short numlines;
    short firstline;
} subsector_t;

// Seg (segmento de línea)
typedef struct
{
    vertex_t* v1;
    vertex_t* v2;
    fixed_t offset;
    angle_t angle;
    side_t* sidedef;
    line_t* linedef;
    sector_t* frontsector;
    sector_t* backsector;
} seg_t;

// Nodo BSP
typedef struct
{
    fixed_t x;
    fixed_t y;
    fixed_t dx;
    fixed_t dy;
    fixed_t bbox[2][4];
    unsigned short children[2];
} node_t;

// Drawseg para renderizado
#define MAXDRAWSEGS     256

typedef struct drawseg_s
{
    seg_t* curline;
    int x1;
    int x2;
    fixed_t scale1;
    fixed_t scale2;
    fixed_t scalestep;
    int silhouette;
    fixed_t bsilheight;
    fixed_t tsilheight;
    short* sprtopclip;
    short* sprbottomclip;
    short* maskedtexturecol;
} drawseg_t;

// ============================================================================
// ESTRUCTURAS DE CARGA DE MAPAS - De doomdata.h
// ============================================================================

#define PACKEDATTR __attribute__((packed))

// Estructuras del formato WAD
typedef struct
{
    short x;
    short y;
} PACKEDATTR mapvertex_t;

typedef struct
{
    short textureoffset;
    short rowoffset;
    char toptexture[8];
    char bottomtexture[8];
    char midtexture[8];
    short sector;
} PACKEDATTR mapsidedef_t;

typedef struct
{
    short v1;
    short v2;
    short flags;
    short special;
    short tag;
    short sidenum[2];
} PACKEDATTR maplinedef_t;

typedef struct
{
    short floorheight;
    short ceilingheight;
    char floorpic[8];
    char ceilingpic[8];
    short lightlevel;
    short special;
    short tag;
} PACKEDATTR mapsector_t;

typedef struct
{
    short numsegs;
    short firstseg;
} PACKEDATTR mapsubsector_t;

typedef struct
{
    short v1;
    short v2;
    short angle;
    short linedef;
    short side;
    short offset;
} PACKEDATTR mapseg_t;

typedef struct
{
    short x;
    short y;
    short dx;
    short dy;
    short bbox[2][4];
    unsigned short children[2];
} PACKEDATTR mapnode_t;

typedef struct
{
    short x;
    short y;
    short angle;
    short type;
    short options;
} PACKEDATTR mapthing_t;

// Flags de LineDef
#define ML_BLOCKING         1
#define ML_BLOCKMONSTERS    2
#define ML_TWOSIDED         4
#define ML_DONTPEGTOP       8
#define ML_DONTPEGBOTTOM    16
#define ML_SECRET           32
#define ML_SOUNDBLOCK       64
#define ML_DONTDRAW         128
#define ML_MAPPED           256

// Indicador de subsector en BSP
#define NF_SUBSECTOR        0x8000

// ============================================================================
// SISTEMA WAD
// ============================================================================

// NOTA: Las estructuras wadinfo_t, filelump_t y lumpinfo_t están definidas
// en doom_wad.c para mantener compatibilidad 1:1 con el motor original.
// No las redefinimos aquí para evitar conflictos.

// Forward declarations para las funciones WAD
struct wad_file_s;
typedef struct wad_file_s wad_file_t;

// ============================================================================
// VARIABLES GLOBALES DEL MOTOR
// ============================================================================

// Datos del mapa cargado
extern int numvertexes;
extern vertex_t* vertexes;

extern int numsegs;
extern seg_t* segs;

extern int numsectors;
extern sector_t* sectors;

extern int numlines;
extern line_t* lines;

extern int numsides;
extern side_t* sides;

extern int numsubsectors;
extern subsector_t* subsectors;

extern int numnodes;
extern node_t* nodes;

// Cámara/jugador
extern fixed_t viewx;
extern fixed_t viewy;
extern fixed_t viewz;
extern angle_t viewangle;
extern fixed_t viewcos;
extern fixed_t viewsin;

// Renderizado
extern int viewwidth;
extern int viewheight;
extern byte* viewimage;

extern GRAPH* doom_render_buffer;

// ============================================================================
// FUNCIONES PRINCIPALES DEL MOTOR
// ============================================================================

// Inicialización
void DOOM_Init(void);
void DOOM_Shutdown(void);

// Sistema WAD
boolean DOOM_LoadWAD(const char* filename);
wad_file_t* W_AddFile(char* filename);
void W_GenerateHashTable(void);
int W_CheckNumForName(char* name);
int W_GetNumForName(char* name);
int W_LumpLength(unsigned int lump);
void W_ReadLump(unsigned int lump, void* dest);
void* W_CacheLumpNum(int lump, int tag);
void* W_CacheLumpName(char* name, int tag);

// Carga de mapas
void P_SetupLevel(const char* mapname);

// Renderizado
void R_Init(void);
void R_RenderPlayerView(void);
void R_SetupFrame(void);

// Funciones de cámara (exportadas a BennuGD2)
void DOOM_SetCamera(fixed_t x, fixed_t y, fixed_t z, angle_t angle);
void DOOM_MoveForward(fixed_t speed);
void DOOM_MoveBackward(fixed_t speed);
void DOOM_StrafeLeft(fixed_t speed);
void DOOM_StrafeRight(fixed_t speed);
void DOOM_TurnLeft(angle_t angle);
void DOOM_TurnRight(angle_t angle);

// ============================================================================
// FUNCIONES EXPORTADAS A BENNUGD2
// ============================================================================

extern int64_t libmod_doom_init(INSTANCE *my, int64_t *params);
extern int64_t libmod_doom_shutdown(INSTANCE *my, int64_t *params);
extern int64_t libmod_doom_load_wad(INSTANCE *my, int64_t *params);
extern int64_t libmod_doom_load_map(INSTANCE *my, int64_t *params);
extern int64_t libmod_doom_render_frame(INSTANCE *my, int64_t *params);
extern int64_t libmod_doom_set_camera(INSTANCE *my, int64_t *params);
extern int64_t libmod_doom_move_forward(INSTANCE *my, int64_t *params);
extern int64_t libmod_doom_move_backward(INSTANCE *my, int64_t *params);
extern int64_t libmod_doom_strafe_left(INSTANCE *my, int64_t *params);
extern int64_t libmod_doom_strafe_right(INSTANCE *my, int64_t *params);
extern int64_t libmod_doom_turn_left(INSTANCE *my, int64_t *params);
extern int64_t libmod_doom_turn_right(INSTANCE *my, int64_t *params);

#endif // __LIBMOD_DOOM_H
