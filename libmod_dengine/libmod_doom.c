/*
 * Doom Engine Module for BennuGD2
 * Main implementation file
 */

#include "libmod_doom.h"
#include "libmod_doom_exports.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include "libbggfx.h"  
#include <inttypes.h>  
#include <limits.h>
#include "doom_zone.h"
#include "doom_file.h"
#include "doom_system.h"
#include "doom_render.h"// ============================================================================
// VARIABLES GLOBALES DEL MOTOR
// ============================================================================

// Datos del mapa cargado
int numvertexes = 0;
vertex_t* vertexes = NULL;

int numsegs = 0;
seg_t* segs = NULL;

int numsectors = 0;
sector_t* sectors = NULL;

int numlines = 0;
line_t* lines = NULL;

int numsides = 0;
side_t* sides = NULL;

int numsubsectors = 0;
subsector_t* subsectors = NULL;

int numnodes = 0;
node_t* nodes = NULL;

// Cámara/jugador
fixed_t viewx = 0;
fixed_t viewy = 0;
fixed_t viewz = 41 * FRACUNIT;  // Altura por defecto del jugador
angle_t viewangle = 0;
fixed_t viewcos = FRACUNIT;
fixed_t viewsin = 0;

// Renderizado
int viewwidth = 320;
int viewheight = 200;
byte* viewimage = NULL;

GRAPH* doom_render_buffer = NULL;

// Estado del motor
static boolean doom_initialized = false;
static boolean wad_loaded = false;
static boolean map_loaded = false;

// ============================================================================
// FUNCIONES PRINCIPALES DEL MOTOR
// ============================================================================

//
// DOOM_Init
// Inicializa el motor Doom
//
void DOOM_Init(void)
{
    if (doom_initialized)
    {
        printf("DOOM: Motor ya inicializado\n");
        return;
    }
    
    printf("DOOM: Inicializando motor Doom...\n");
    
    // Inicializar sistema de memoria Z_Zone
    int size;
    byte* zonemem = I_ZoneBase(&size);
    Z_Init();
    
    // Inicializar tablas (ya están precalculadas en doom_tables.c)
    // No necesitamos hacer nada aquí
    
    // Inicializar buffer de renderizado
    viewimage = NULL;
    doom_render_buffer = NULL;
    
    doom_initialized = true;
    printf("DOOM: Motor inicializado correctamente\n");
}

//
// DOOM_Shutdown
// Libera todos los recursos del motor
//
void DOOM_Shutdown(void)
{
    if (!doom_initialized)
        return;
        
    printf("DOOM: Cerrando motor Doom...\n");
    
    // Liberar datos del mapa
    if (vertexes) free(vertexes);
    if (segs) free(segs);
    if (sectors) free(sectors);
    if (lines) free(lines);
    if (sides) free(sides);
    if (subsectors) free(subsectors);
    if (nodes) free(nodes);
    
    vertexes = NULL;
    segs = NULL;
    sectors = NULL;
    lines = NULL;
    sides = NULL;
    subsectors = NULL;
    nodes = NULL;
    
    numvertexes = 0;
    numsegs = 0;
    numsectors = 0;
    numlines = 0;
    numsides = 0;
    numsubsectors = 0;
    numnodes = 0;
    
    // Liberar buffer de renderizado
    if (viewimage) free(viewimage);
    viewimage = NULL;
    
    doom_initialized = false;
    wad_loaded = false;
    map_loaded = false;
    
    printf("DOOM: Motor cerrado\n");
}

//
// DOOM_SetCamera
// Establece la posición y ángulo de la cámara
//
void DOOM_SetCamera(fixed_t x, fixed_t y, fixed_t z, angle_t angle)
{
    viewx = x;
    viewy = y;
    viewz = z;
    viewangle = angle;
    
    // Precalcular seno y coseno del ángulo
    viewcos = finecosine[angle >> ANGLETOFINESHIFT];
    viewsin = finesine[angle >> ANGLETOFINESHIFT];
}

//
// DOOM_MoveForward
// Mueve la cámara hacia adelante
//
void DOOM_MoveForward(fixed_t speed)
{
    fixed_t old_x = viewx;
    fixed_t old_y = viewy;
    
    viewx += FixedMul(speed, viewcos);
    viewy += FixedMul(speed, viewsin);
    
    printf("MOVE FWD: speed=%d angle=%u cos=%d sin=%d | pos (%d,%d)->(%d,%d)\n",
           speed >> FRACBITS, viewangle >> 20, viewcos >> FRACBITS, viewsin >> FRACBITS,
           old_x >> FRACBITS, old_y >> FRACBITS, viewx >> FRACBITS, viewy >> FRACBITS);
}

//
// DOOM_MoveBackward
// Mueve la cámara hacia atrás
//
void DOOM_MoveBackward(fixed_t speed)
{
    viewx -= FixedMul(speed, viewcos);
    viewy -= FixedMul(speed, viewsin);
}

//
// DOOM_StrafeLeft
// Mueve la cámara a la izquierda (strafe)
//
void DOOM_StrafeLeft(fixed_t speed)
{
    viewx -= FixedMul(speed, viewsin);
    viewy += FixedMul(speed, viewcos);
}

//
// DOOM_StrafeRight
// Mueve la cámara a la derecha (strafe)
//
void DOOM_StrafeRight(fixed_t speed)
{
    viewx += FixedMul(speed, viewsin);
    viewy -= FixedMul(speed, viewcos);
}

//
// DOOM_TurnLeft
// Gira la cámara a la izquierda
//
void DOOM_TurnLeft(angle_t angle)
{
    viewangle += angle;
    viewcos = finecosine[viewangle >> ANGLETOFINESHIFT];
    viewsin = finesine[viewangle >> ANGLETOFINESHIFT];
}

//
// DOOM_TurnRight
// Gira la cámara a la derecha
//
void DOOM_TurnRight(angle_t angle)
{
    angle_t old_angle = viewangle;
    viewangle -= angle;
    viewcos = finecosine[viewangle >> ANGLETOFINESHIFT];
    viewsin = finesine[viewangle >> ANGLETOFINESHIFT];
    
    printf("TURN RIGHT: delta=%u | angle %u->%u\n", 
           angle >> 20, old_angle >> 20, viewangle >> 20);
}

// ============================================================================
// FUNCIONES EXPORTADAS A BENNUGD2
// ============================================================================

//
// libmod_doom_init
// Función exportada: DOOM_INIT()
//
int64_t libmod_doom_init(INSTANCE *my, int64_t *params)
{
    DOOM_Init();
    return 0;
}

//
// libmod_doom_shutdown
// Función exportada: DOOM_SHUTDOWN()
//
int64_t libmod_doom_shutdown(INSTANCE *my, int64_t *params)
{
    DOOM_Shutdown();
    return 0;
}

//
// libmod_doom_load_wad
// Función exportada: DOOM_LOAD_WAD(string filename)
//
int64_t libmod_doom_load_wad(INSTANCE *my, int64_t *params)
{
    const char* filename = string_get(params[0]);
    
    if (!doom_initialized)
    {
        printf("DOOM: Error - Motor no inicializado\n");
        return -1;
    }
    
    printf("DOOM: Cargando WAD: %s\n", filename);
    
    // Cargar archivo WAD usando el sistema del motor original
    wad_file_t* wadfile = W_AddFile((char*)filename);
    
    if (wadfile == NULL)
    {
        printf("DOOM: Error - No se pudo cargar el WAD\n");
        return -1;
    }
    
    // Generar tabla hash para búsquedas rápidas
    W_GenerateHashTable();
    
    wad_loaded = true;
    printf("DOOM: WAD cargado correctamente\n");
    
    return 0;
}

//
// libmod_doom_load_map
// Función exportada: DOOM_LOAD_MAP(string mapname)
//
int64_t libmod_doom_load_map(INSTANCE *my, int64_t *params)
{
    const char* mapname = string_get(params[0]);
    
    if (!doom_initialized)
    {
        printf("DOOM: Error - Motor no inicializado\n");
        return -1;
    }
    
    if (!wad_loaded)
    {
        printf("DOOM: Error - No hay WAD cargado\n");
        return -1;
    }
    
    printf("DOOM: Cargando mapa: %s\n", mapname);
    
    // Buscar el lump del mapa
    int lumpnum = W_CheckNumForName((char*)mapname);
    if (lumpnum < 0)
    {
        printf("DOOM: Error - Mapa '%s' no encontrado en el WAD\n", mapname);
        return -1;
    }
    
    // Los datos del mapa están en lumps consecutivos después del marcador del mapa:
    // MAPNAME, THINGS, LINEDEFS, SIDEDEFS, VERTEXES, SEGS, SSECTORS, NODES, SECTORS, REJECT, BLOCKMAP
    
    // Cargar VERTEXES (lump +4)
    int vertexes_lump = lumpnum + 4;
    numvertexes = W_LumpLength(vertexes_lump) / sizeof(mapvertex_t);
    
    if (vertexes) free(vertexes);
    vertexes = malloc(numvertexes * sizeof(vertex_t));
    
    mapvertex_t* mapverts = W_CacheLumpNum(vertexes_lump, 0);
    for (int i = 0; i < numvertexes; i++)
    {
        vertexes[i].x = mapverts[i].x << FRACBITS;
        vertexes[i].y = mapverts[i].y << FRACBITS;
    }
    printf("DOOM: Cargados %d vértices\n", numvertexes);
    
    // Calcular bounding box del mapa para diagnóstico
    if (numvertexes > 0) {
        fixed_t min_x = vertexes[0].x;
        fixed_t max_x = vertexes[0].x;
        fixed_t min_y = vertexes[0].y;
        fixed_t max_y = vertexes[0].y;
        
        for (int i = 1; i < numvertexes; i++) {
            if (vertexes[i].x < min_x) min_x = vertexes[i].x;
            if (vertexes[i].x > max_x) max_x = vertexes[i].x;
            if (vertexes[i].y < min_y) min_y = vertexes[i].y;
            if (vertexes[i].y > max_y) max_y = vertexes[i].y;
        }
        
        printf("DOOM: Mapa bounds: X[%d .. %d] Y[%d .. %d]\n", 
               min_x >> FRACBITS, max_x >> FRACBITS,
               min_y >> FRACBITS, max_y >> FRACBITS);
        printf("DOOM: Centro aproximado: X=%d Y=%d\n",
               ((min_x + max_x) >> 1) >> FRACBITS,
               ((min_y + max_y) >> 1) >> FRACBITS);
    }
    
    // Cargar SECTORS (lump +8)
    int sectors_lump = lumpnum + 8;
    numsectors = W_LumpLength(sectors_lump) / sizeof(mapsector_t);
    
    if (sectors) free(sectors);
    sectors = malloc(numsectors * sizeof(sector_t));
    memset(sectors, 0, numsectors * sizeof(sector_t));
    
    mapsector_t* mapsecs = W_CacheLumpNum(sectors_lump, 0);
    for (int i = 0; i < numsectors; i++)
    {
        sectors[i].floorheight = mapsecs[i].floorheight << FRACBITS;
        sectors[i].ceilingheight = mapsecs[i].ceilingheight << FRACBITS;
        sectors[i].lightlevel = mapsecs[i].lightlevel;
        sectors[i].special = mapsecs[i].special;
        sectors[i].tag = mapsecs[i].tag;
        sectors[i].thinglist = NULL;
        sectors[i].specialdata = NULL;
        sectors[i].linecount = 0;
        sectors[i].lines = NULL;
    }
    printf("DOOM: Cargados %d sectores\n", numsectors);
    
    // Cargar SIDEDEFS (lump +3)
    int sidedefs_lump = lumpnum + 3;
    numsides = W_LumpLength(sidedefs_lump) / sizeof(mapsidedef_t);
    
    if (sides) free(sides);
    sides = malloc(numsides * sizeof(side_t));
    memset(sides, 0, numsides * sizeof(side_t));
    
    mapsidedef_t* mapsides = W_CacheLumpNum(sidedefs_lump, 0);
    for (int i = 0; i < numsides; i++)
    {
        sides[i].textureoffset = mapsides[i].textureoffset << FRACBITS;
        sides[i].rowoffset = mapsides[i].rowoffset << FRACBITS;
        sides[i].toptexture = 0;    // TODO: R_TextureNumForName
        sides[i].bottomtexture = 0; // TODO: R_TextureNumForName
        sides[i].midtexture = 0;    // TODO: R_TextureNumForName
        sides[i].sector = &sectors[mapsides[i].sector];
    }
    printf("DOOM: Cargados %d lados\n", numsides);
    
    // Cargar LINEDEFS (lump +2)
    int linedefs_lump = lumpnum + 2;
    numlines = W_LumpLength(linedefs_lump) / sizeof(maplinedef_t);
    
    if (lines) free(lines);
    lines = malloc(numlines * sizeof(line_t));
    memset(lines, 0, numlines * sizeof(line_t));
    
    maplinedef_t* maplines = W_CacheLumpNum(linedefs_lump, 0);
    for (int i = 0; i < numlines; i++)
    {
        lines[i].flags = maplines[i].flags;
        lines[i].special = maplines[i].special;
        lines[i].tag = maplines[i].tag;
        lines[i].v1 = &vertexes[maplines[i].v1];
        lines[i].v2 = &vertexes[maplines[i].v2];
        lines[i].dx = lines[i].v2->x - lines[i].v1->x;
        lines[i].dy = lines[i].v2->y - lines[i].v1->y;
        
        // Calcular slope type
        if (!lines[i].dx)
            lines[i].slopetype = ST_VERTICAL;
        else if (!lines[i].dy)
            lines[i].slopetype = ST_HORIZONTAL;
        else if (FixedDiv(lines[i].dy, lines[i].dx) > 0)
            lines[i].slopetype = ST_POSITIVE;
        else
            lines[i].slopetype = ST_NEGATIVE;
        
        lines[i].sidenum[0] = maplines[i].sidenum[0];
        lines[i].sidenum[1] = maplines[i].sidenum[1];
        
        if (lines[i].sidenum[0] != -1)
            lines[i].frontsector = sides[lines[i].sidenum[0]].sector;
        else
            lines[i].frontsector = NULL;
            
        if (lines[i].sidenum[1] != -1)
            lines[i].backsector = sides[lines[i].sidenum[1]].sector;
        else
            lines[i].backsector = NULL;
    }
    printf("DOOM: Cargados %d líneas\n", numlines);
    
    // Cargar SEGS (lump +5)
    int segs_lump = lumpnum + 5;
    numsegs = W_LumpLength(segs_lump) / sizeof(mapseg_t);
    
    if (segs) free(segs);
    segs = malloc(numsegs * sizeof(seg_t));
    memset(segs, 0, numsegs * sizeof(seg_t));
    
    mapseg_t* mapsegs = W_CacheLumpNum(segs_lump, 0);
    for (int i = 0; i < numsegs; i++)
    {
        segs[i].v1 = &vertexes[mapsegs[i].v1];
        segs[i].v2 = &vertexes[mapsegs[i].v2];
        segs[i].angle = mapsegs[i].angle << 16;
        segs[i].offset = mapsegs[i].offset << 16;
        
        int linedef = mapsegs[i].linedef;
        segs[i].linedef = &lines[linedef];
        
        int side = mapsegs[i].side;
        segs[i].sidedef = &sides[lines[linedef].sidenum[side]];
        segs[i].frontsector = sides[lines[linedef].sidenum[side]].sector;
        
        if (lines[linedef].flags & ML_TWOSIDED)
        {
            int sidenum = lines[linedef].sidenum[side ^ 1];
            if (sidenum >= 0 && sidenum < numsides)
                segs[i].backsector = sides[sidenum].sector;
            else
                segs[i].backsector = NULL;
        }
        else
        {
            segs[i].backsector = NULL;
        }
    }
    printf("DOOM: Cargados %d segmentos\n", numsegs);
    
    // Cargar SSECTORS (lump +6)
    int ssectors_lump = lumpnum + 6;
    numsubsectors = W_LumpLength(ssectors_lump) / sizeof(mapsubsector_t);
    
    if (subsectors) free(subsectors);
    subsectors = malloc(numsubsectors * sizeof(subsector_t));
    memset(subsectors, 0, numsubsectors * sizeof(subsector_t));
    
    mapsubsector_t* mapss = W_CacheLumpNum(ssectors_lump, 0);
    for (int i = 0; i < numsubsectors; i++)
    {
        subsectors[i].numlines = mapss[i].numsegs;
        subsectors[i].firstline = mapss[i].firstseg;
        // Asignar sector desde el primer seg
        if (subsectors[i].firstline < numsegs)
            subsectors[i].sector = segs[subsectors[i].firstline].sidedef->sector;
    }
    printf("DOOM: Cargados %d subsectores\n", numsubsectors);
    
    // Cargar NODES (lump +7)
    int nodes_lump = lumpnum + 7;
    numnodes = W_LumpLength(nodes_lump) / sizeof(mapnode_t);
    
    if (nodes) free(nodes);
    nodes = malloc(numnodes * sizeof(node_t));
    
    mapnode_t* mapnodes = W_CacheLumpNum(nodes_lump, 0);
    for (int i = 0; i < numnodes; i++)
    {
        nodes[i].x = mapnodes[i].x << FRACBITS;
        nodes[i].y = mapnodes[i].y << FRACBITS;
        nodes[i].dx = mapnodes[i].dx << FRACBITS;
        nodes[i].dy = mapnodes[i].dy << FRACBITS;
        
        for (int j = 0; j < 2; j++)
        {
            nodes[i].children[j] = mapnodes[i].children[j];
            for (int k = 0; k < 4; k++)
                nodes[i].bbox[j][k] = mapnodes[i].bbox[j][k] << FRACBITS;
        }
    }
    printf("DOOM: Cargados %d nodos BSP\n", numnodes);
    
    map_loaded = true;
    printf("DOOM: Mapa cargado correctamente\n");
    
    return 0;
}

//
// libmod_doom_render_frame
// Función exportada: DOOM_RENDER_FRAME(int screen_w, int screen_h)
//
int64_t libmod_doom_render_frame(INSTANCE *my, int64_t *params)
{
    int screen_w = (int)params[0];
    int screen_h = (int)params[1];
    
    if (!doom_initialized)
    {
        printf("DOOM: Error - Motor no inicializado\n");
        return 0;
    }
    
    if (!map_loaded)
    {
        printf("DOOM: Error - No hay mapa cargado\n");
        return 0;
    }
    
    // Inicializar sistema de renderizado si es necesario
    static boolean render_initialized = false;
    if (!render_initialized)
    {
        R_Init();
        render_initialized = true;
    }
    
    

    if (doom_render_buffer == NULL || 
        doom_render_buffer->width != screen_w || 
        doom_render_buffer->height != screen_h)
    {
        if (doom_render_buffer != NULL)
        {
            bitmap_destroy(doom_render_buffer);
        }
        
        // Usar bitmap_new_syslib como en el módulo OLD para evitar crash
        doom_render_buffer = bitmap_new_syslib(screen_w, screen_h);
        
        if (doom_render_buffer == NULL)
        {
            printf("DOOM: Error - No se pudo crear buffer de renderizado\n");
            return 0;
        }
        
        // Redimensionar buffers internos del motor Doom
        R_CheckResize(screen_w, screen_h);
        
        // Recrear buffer de imagen si es necesario
        if (viewimage) free(viewimage);
        viewimage = malloc(screen_w * screen_h);
        memset(viewimage, 0, screen_w * screen_h);
    }
    
    // Asegurarse de que el motor sepa el tamaño actual
    if (viewwidth != screen_w || viewheight != screen_h)
    {
         R_CheckResize(screen_w, screen_h);
    }
    
    // Limpiar buffer (Fondo para wireframe)
    // Usamos gr_clear_as si es posible, o memset si es raw, pero como es GRAPH de Bennu:
    // Pinta fondo negro/gris antes de renderizar
    if (doom_render_buffer) {
        // Limpiar a transparente/negro (ColorKey 0 usualmente en Bennu para gráficos 32bpp si alpha=0)
        // O simplemente negro. 
        // Si usamos SDL_MapRGBA(..., 0, 0, 0, 0), debería ser transparente.
         extern SDL_PixelFormat * gPixelFormat;
         // Intentar transparente absoluto
         uint32_t clear_color = SDL_MapRGBA(gPixelFormat, 0, 0, 0, 0);
         
         // Método de limpieza seguro usando gr_put_pixel
         for (int y = 0; y < screen_h; y++) {
             for (int x = 0; x < screen_w; x++) {
                 // Usamos clear_color que ya se calculó arriba (debería ser 0 para transparente)
                 gr_put_pixel(doom_render_buffer, x, y, clear_color);
             }
         }
    }
    
    // Renderizar vista del jugador
    R_RenderPlayerView();
    
    // Retornar el gráfico actualizado
    return doom_render_buffer->code;
}

//
// libmod_doom_set_camera
// Función exportada: DOOM_SET_CAMERA(int x, int y, int z, int angle)
//
int64_t libmod_doom_set_camera(INSTANCE *my, int64_t *params)
{
    // Convertir a fixed point
    fixed_t x = (fixed_t)params[0] << FRACBITS;
    fixed_t y = (fixed_t)params[1] << FRACBITS;
    fixed_t z = (fixed_t)params[2] << FRACBITS;
    angle_t angle = (angle_t)params[3];
    
    DOOM_SetCamera(x, y, z, angle);
    return 0;
}

//
// libmod_doom_move_forward
// Función exportada: DOOM_MOVE_FORWARD(int speed)
//
int64_t libmod_doom_move_forward(INSTANCE *my, int64_t *params)
{
    // Convertir a fixed point (asumiendo input en unidades de mapa enteras)
    fixed_t speed = (fixed_t)params[0] << FRACBITS;
    DOOM_MoveForward(speed);
    return 0;
}

//
// libmod_doom_move_backward
// Función exportada: DOOM_MOVE_BACKWARD(int speed)
//
int64_t libmod_doom_move_backward(INSTANCE *my, int64_t *params)
{
    fixed_t speed = (fixed_t)params[0] << FRACBITS;
    DOOM_MoveBackward(speed);
    return 0;
}

//
// libmod_doom_strafe_left
// Función exportada: DOOM_STRAFE_LEFT(int speed)
//
int64_t libmod_doom_strafe_left(INSTANCE *my, int64_t *params)
{
    fixed_t speed = (fixed_t)params[0] << FRACBITS;
    DOOM_StrafeLeft(speed);
    return 0;
}

//
// libmod_doom_strafe_right
// Función exportada: DOOM_STRAFE_RIGHT(int speed)
//
int64_t libmod_doom_strafe_right(INSTANCE *my, int64_t *params)
{
    fixed_t speed = (fixed_t)params[0] << FRACBITS;
    DOOM_StrafeRight(speed);
    return 0;
}

//
// libmod_doom_turn_left
// Función exportada: DOOM_TURN_LEFT(int angle)
//
int64_t libmod_doom_turn_left(INSTANCE *my, int64_t *params)
{
    angle_t angle = (angle_t)params[0];
    DOOM_TurnLeft(angle);
    return 0;
}

//
// libmod_doom_turn_right
// Función exportada: DOOM_TURN_RIGHT(int angle)
//
int64_t libmod_doom_turn_right(INSTANCE *my, int64_t *params)
{
    angle_t angle = (angle_t)params[0];
    DOOM_TurnRight(angle);
    return 0;
}

// ============================================================================
// HOOKS DEL MÓDULO
// ============================================================================

void __bgdexport(libmod_doom, module_initialize)()
{
    printf("DOOM: Módulo cargado\n");
}

void __bgdexport(libmod_doom, module_finalize)()
{
    DOOM_Shutdown();
    printf("DOOM: Módulo descargado\n");
}
