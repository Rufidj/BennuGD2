//
// Doom Rendering System - Main Implementation
// Adaptado de r_main.c del motor Doom original
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "libmod_doom.h"
#include "doom_render.h"

// ============================================================================
// VARIABLES GLOBALES DE RENDERIZADO
// ============================================================================

// Dimensiones de la pantalla (definidas en libmod_doom.c)
extern int viewwidth;
extern int viewheight;
int scaledviewwidth = SCREENWIDTH;
int viewwindowx = 0;
int viewwindowy = 0;

// Tablas de lookup
int* viewangletox = NULL;
angle_t* xtoviewangle = NULL;

// Clipping arrays
short* screenheightarray = NULL;
short* negonearray = NULL;
short* floorclip = NULL;
short* ceilingclip = NULL;

// Proyección
fixed_t projection;
fixed_t projectiony;
fixed_t centerxfrac;
fixed_t centeryfrac;

int centerx;
int centery;

// Validcount para evitar procesar el mismo objeto múltiples veces
int validcount = 1;

// ============================================================================
// VARIABLES DE CLIPPING Y DIBUJO (Originales Doom)
// ============================================================================

drawseg_t   drawsegs[MAXDRAWSEGS];
drawseg_t*  ds_p;

cliprange_t solidsegs[MAXSEGS];
cliprange_t* newend;

// Angulos para clipping de frustum
angle_t     clipangle;
angle_t     doubleclipangle;

// Función R_InitSeam definida en r_segs.c original (aquí inline o helper)
// Ángulos de los bordes de la pantalla
angle_t     rw_normalangle;
angle_t     rw_angle1;

// Globales para renderizado de segmentos (addline -> storewallrange)
fixed_t     rw_distance;
fixed_t     rw_scale;
fixed_t     rw_scalestep;
fixed_t     rw_midtexturemid;
fixed_t     rw_toptexturemid;
fixed_t     rw_bottomtexturemid;
int         rw_x; // Inicio X del segmento actual en pantalla

seg_t*      curline; // La línea actual que se está procesando

// ============================================================================
// R_Init
// Inicializa el sistema de renderizado
// ============================================================================
void R_Init(void)
{
    printf("DOOM: Inicializando sistema de renderizado...\n");
    
    // Inicializar tablas
    R_InitTables();
    
    // Inicializar datos
    R_InitData();
    
    // Inicializar planos
    R_InitPlanes();
    
    // Inicializar tablas de luz
    R_InitLightTables();
    
    // Inicializar sky
    R_InitSkyMap();
    
    printf("DOOM: Sistema de renderizado inicializado\n");
}

// ============================================================================
// R_InitTables
// Inicializa tablas de lookup para renderizado
// ============================================================================
void R_InitTables(void)
{
    // Asignar memoria para tablas
    // viewangletox depende solo de FINEANGLES/SCREENWIDTH original (o podemos hacerlo dinámico después)
    // Por ahora lo dejamos fijo basado en SCREENWIDTH estándar para las tangentes precalculadas
    if (!viewangletox)
        viewangletox = malloc(sizeof(int) * (FINEANGLES / 2));
    
    // Calcular tabla viewangletox
    for (int i = 0; i < FINEANGLES / 2; i++)
    {
        // NOTA: Esto asume FOV de 90 grados horizontal estándar de Doom en proporción 320 ancho.
        // Si viewwidth cambia, esta tabla debería recalcularse también si queremos precisión perfecta,
        // pero funciona como aproximación lookup.
        fixed_t t = FixedMul(finetangent[i], FRACUNIT * SCREENWIDTH / 2);
        if (t > FRACUNIT * 2)
            t = FRACUNIT * 2;
        else if (t < -FRACUNIT * 2)
            t = -FRACUNIT * 2;
        viewangletox[i] = (SCREENWIDTH / 2) - (t >> FRACBITS);
    }
    
    // xtoviewangle se mueve a R_InitData porque depende de viewwidth
}

// ============================================================================
// R_InitData
// Inicializa datos de renderizado (texturas, flats, sprites)
// ============================================================================
void R_InitData(void)
{
    // TODO: Cargar texturas, flats y sprites del WAD
    // Por ahora solo inicializamos arrays básicos
    
    // Asegurar que viewwidth no exceda el máximo
    if (viewwidth > MAXWIDTH)
    {
        printf("WARNING: viewwidth %d > MAXWIDTH %d, clamping\n", viewwidth, MAXWIDTH);
        viewwidth = MAXWIDTH;
    }
    
    // Liberar memoria previa si existe
    if (screenheightarray) free(screenheightarray);
    if (negonearray) free(negonearray);
    if (floorclip) free(floorclip);
    if (ceilingclip) free(ceilingclip);
    if (xtoviewangle) free(xtoviewangle);
    
    // Asignar memoria basada en viewwidth real (o MAXWIDTH si preferimos buffer estático)
    screenheightarray = malloc(sizeof(short) * viewwidth);
    negonearray = malloc(sizeof(short) * viewwidth);
    floorclip = malloc(sizeof(short) * viewwidth);
    ceilingclip = malloc(sizeof(short) * viewwidth);
    xtoviewangle = malloc(sizeof(angle_t) * (viewwidth + 1));

    if (!screenheightarray || !negonearray || !floorclip || !ceilingclip || !xtoviewangle)
    {
        printf("ERROR: R_InitData: Failed to allocate memory for clipping arrays\n");
        return;
    }
    
    for (int i = 0; i < viewwidth; i++)
    {
        screenheightarray[i] = (short)viewheight;
        negonearray[i] = -1;
    }
    
    // Recalcular xtoviewangle basado en el nuevo ancho
    for (int i = 0; i <= viewwidth; i++)
    {
        int t = (i - viewwidth / 2) << FRACBITS;
        // CORRECCIÓN: Multiplicar antes de castear a angle_t
        // atan2 devuelve radianes float.
        double rads = atan2((double)t, (double)(FRACUNIT * viewwidth / 2));
        xtoviewangle[i] = (angle_t)(rads * (2147483648.0 / 3.14159265358979323846));
    }
}

// ============================================================================
// R_CheckResize
// Verifica si es necesario redimensionar los buffers
// ============================================================================
void R_CheckResize(int width, int height)
{
    if (viewwidth != width || viewheight != height)
    {
        viewwidth = width;
        viewheight = height;
        R_InitData(); // Reasignar arrays
        R_SetupFrame(); // Recalcular proyección
    }
}

// ============================================================================
// R_InitPlanes
// Inicializa sistema de renderizado de planos
// ============================================================================
void R_InitPlanes(void)
{
    // TODO: Inicializar visplanes
}

// ============================================================================
// R_InitLightTables
// Inicializa tablas de iluminación
// ============================================================================
void R_InitLightTables(void)
{
    // TODO: Generar tablas de luz basadas en distancia
}

// ============================================================================
// R_InitSkyMap
// Inicializa textura del cielo
// ============================================================================
void R_InitSkyMap(void)
{
    // TODO: Cargar textura del cielo
}

// ============================================================================
// R_SetupFrame
// Configura el frame para renderizado
// ============================================================================
void R_SetupFrame(void)
{
    // printf("DEBUG: R_SetupFrame start\n");
    validcount++;
    
    // Calcular centro de la pantalla
    centerx = viewwidth / 2;
    centery = viewheight / 2;
    centerxfrac = centerx << FRACBITS;
    centeryfrac = centery << FRACBITS;
    
    extern int debug_segs_count;
    debug_segs_count = 0; // Log EVERY frame for now
    
    // printf("DEBUG: R_SetupFrame center computed\n");
    
    // Calcular proyección
    // Calcular proyección
    // Doom usa un FOV de 90 grados por defecto.
    // Tan(45) = 1.0 (FRACUNIT).
    // Distancia al plano de proyección = centerx / tan(fov/2).
    // Si tan(fov/2) == 1.0, entonces projection = centerxfrac.
    
    // ERROR ANTERIOR: La división entera plano (/) eliminaba el shift de punto fijo.
    // (centerx<<16) / (1<<16) = centerx.
    // Necesitamos projection en formato 16.16 (ej: 160.0 * 65536).
    
    projection = centerxfrac;
    
    // Para Y (aspect ratio correction?), Doom estira los pixeles verticalmente (angulos).
    // Pero basicamente, projectiony suele ser similar escalado.
    // Original Doom: projectiony = (((viewheight<<FRACBITS)/2) * FRACUNIT) / finetangent[...]
    // Asumimos aspect ratio 1:1 de pixels ahora mismo o corrección Doom.
    // Simple:
    projectiony = (viewheight / 2) << FRACBITS;
    
    // Debug
    extern int debug_segs_count;
    if (debug_segs_count == 0) {
        printf("DEBUG: R_SetupFrame Projection=%d (Correct FixedPoint)\n", projection);
    }
    
    // Inicializar ángulos de clipping para AddLine
    // Definimos clipangle como el ángulo correspondiente al borde de la pantalla (+FOV/2)
    // xtoviewangle[0] debería darnos el ángulo relativo del borde izquierdo
    // xtoviewangle mapea X -> Angulo relativo
    clipangle = xtoviewangle[0];
    doubleclipangle = clipangle * 2;
    
    // Inicializar estructuras de renderizado para el nuevo frame
    R_ClearClipSegs();
    R_ClearDrawSegs();
    R_ClearPlanes();
    
    // printf("DEBUG: R_SetupFrame done, clipangle=%u\n", clipangle);
}

// ============================================================================
// R_ClearClipSegs
// Limpia los segmentos de clipping (inicializa solidsegs)
// ============================================================================
void R_ClearClipSegs(void)
{
    solidsegs[0].first = -0x7fffffff; // Menos infinito
    solidsegs[0].last = -1;
    
    solidsegs[1].first = viewwidth;
    solidsegs[1].last = 0x7fffffff; // Más infinito
    
    newend = &solidsegs[2];
}

// ============================================================================
// R_ClearDrawSegs
// Limpia los segmentos de dibujo
// ============================================================================
void R_ClearDrawSegs(void)
{
    ds_p = drawsegs;
}

// ============================================================================
// R_ClearPlanes
// Limpia los planos visibles
// ============================================================================
void R_ClearPlanes(void)
{
    // Resetear clipping arrays
    for (int i = 0; i < viewwidth; i++)
    {
        floorclip[i] = viewheight;
        ceilingclip[i] = -1;
    }
}

// ============================================================================
// R_ClearSprites
// Limpia la lista de sprites
// ============================================================================
void R_ClearSprites(void)
{
    // TODO: Limpiar vissprites
}

// ============================================================================
// R_RenderPlayerView
// Renderiza la vista del jugador
// ============================================================================
void R_RenderPlayerView(void)
{
    printf("DEBUG: R_RenderPlayerView start. Cam: x=%d y=%d z=%d ang=%u\n", viewx >> FRACBITS, viewy >> FRACBITS, viewz >> FRACBITS, viewangle);
    // Configurar frame
    R_SetupFrame();
    
    printf("DEBUG: R_SetupFrame done\n");
    
    // Limpiar buffers
    R_ClearClipSegs();
    R_ClearDrawSegs();
    R_ClearPlanes();
    R_ClearSprites();
    
    printf("DEBUG: Cleared buffers, numnodes=%d\n", numnodes);
    
    // Renderizar BSP desde el nodo raíz
    if (numnodes > 0)
    {
        printf("DEBUG: Calling R_RenderBSPNode(%d)\n", numnodes - 1);
        R_RenderBSPNode(numnodes - 1);
    }
    
    printf("DEBUG: BSP traversal done\n");
    
    // Dibujar planos (suelos y techos)
    R_DrawPlanes();
    
    // Dibujar sprites y masked textures
    R_DrawMasked();
    printf("DEBUG: R_RenderPlayerView end\n");
}

// ============================================================================
// R_RenderBSPNode
// Renderiza recursivamente el árbol BSP
// ============================================================================
void R_RenderBSPNode(int bspnum)
{
    // Si es un subsector, renderizarlo
    if (bspnum & NF_SUBSECTOR)
    {
        int subsector_idx = (bspnum == -1) ? 0 : (bspnum & (~NF_SUBSECTOR));
        // printf("DEBUG: RenderSubsector idx=%d (bspnum=0x%x)\n", subsector_idx, bspnum);
        R_Subsector(subsector_idx);
        return;
    }
    
    if (bspnum < 0 || bspnum >= numnodes)
    {
        printf("ERROR: R_RenderBSPNode: bspnum %d out of bounds (numnodes=%d)\n", bspnum, numnodes);
        return;
    }
    
    node_t* bsp = &nodes[bspnum];
    
    // Determinar qué lado del nodo estamos
    int side = R_PointOnSide(viewx, viewy, bsp);
    
    // Renderizar lado frontal primero
    R_RenderBSPNode(bsp->children[side]);
    
    // Verificar si el otro lado es potencialmente visible
    // if (R_CheckBBox(bsp->bbox[side ^ 1]))
    {
        R_RenderBSPNode(bsp->children[side ^ 1]);
    }
}

// ============================================================================
// R_PointOnSide
// Determina en qué lado de una línea de partición está un punto
// ============================================================================
int R_PointOnSide(fixed_t x, fixed_t y, node_t* node)
{
    fixed_t dx = x - node->x;
    fixed_t dy = y - node->y;
    
    // Producto cruz para determinar el lado
    if (!node->dx)
        return (x <= node->x) ? (node->dy > 0) : (node->dy < 0);
    
    if (!node->dy)
        return (y <= node->y) ? (node->dx < 0) : (node->dx > 0);
    
    fixed_t left = FixedMul(node->dy >> FRACBITS, dx);
    fixed_t right = FixedMul(dy, node->dx >> FRACBITS);
    
    return (right < left);
}

// ============================================================================
// R_PointToDist
// Calcula la distancia aproximada a un punto (x,y) desde (viewx,viewy)
// ============================================================================
fixed_t R_PointToDist(fixed_t x, fixed_t y)
{
    fixed_t dx = abs(x - viewx);
    fixed_t dy = abs(y - viewy);
    
    if (dx > dy)
        return dx + (dy >> 1);
    else
        return dy + (dx >> 1);
}

// ============================================================================
// R_PointToAngle
// Calcula el ángulo de un punto respecto a la cámara
// Usa la tabla tantoangle
// ============================================================================
angle_t R_PointToAngle(fixed_t x, fixed_t y)
{
    x -= viewx;
    y -= viewy;
    
    if ((!x) && (!y))
        return 0;
        
    if (x >= 0)
    {
        // Cuadrantes 1 y 4
        if (y >= 0)
        {
            // Cuadrante 1
            if (x > y)
                return tantoangle[SlopeDiv(y,x)]; // 0..45
            else
                return ANG90 - 1 - tantoangle[SlopeDiv(x,y)]; // 45..90
        }
        else
        {
            // Cuadrante 4
            y = -y;
            if (x > y)
                return -tantoangle[SlopeDiv(y,x)]; // 315..360 (negativo)
            else
                return ANG270 + tantoangle[SlopeDiv(x,y)]; // 270..315
        }
    }
    else
    {
        // Cuadrantes 2 y 3
        x = -x;
        if (y >= 0)
        {
            // Cuadrante 2
            if (x > y)
                return ANG180 - 1 - tantoangle[SlopeDiv(y,x)]; // 135..180
            else
                return ANG90 + tantoangle[SlopeDiv(x,y)]; // 90..135
        }
        else
        {
            // Cuadrante 3
            y = -y;
            if (x > y)
                return ANG180 + tantoangle[SlopeDiv(y,x)]; // 180..225
            else
                return ANG270 - 1 - tantoangle[SlopeDiv(x,y)]; // 225..270
        }
    }
}

// ============================================================================
// R_StoreWallRange
// Procesa un rango horizontal de pared visible
// Gestiona solidsegs y clipping
// ============================================================================
// ============================================================================
// R_ClipSolidWallSegment
// Helper para insertar rangos en solidsegs
// ============================================================================
//
// R_ClipSolidWallSegment
// Does handle solid walls,
//  e.g. single sided LineDefs (middle texture)
//  that entirely block the view.
//
void R_ClipSolidWallSegment(int first, int last)
{
    cliprange_t* next;
    cliprange_t* start;

    // Find the first range that touches the range
    //  (adjacent pixels are touching).
    start = solidsegs;
    while (start->last < first - 1)
        start++;

    if (first < start->first)
    {
        if (last < start->first - 1)
        {
            // Post is entirely visible (above start),
            //  so insert a new clippost.
            R_StoreWallRange(first, last);
            next = newend;
            newend++;
            
            while (next != start)
            {
                *next = *(next - 1);
                next--;
            }
            next->first = first;
            next->last = last;
            return;
        }
        
        // There is a fragment above *start.
        R_StoreWallRange(first, start->first - 1);
        // Now adjust the clip size.
        start->first = first;
    }

    // Bottom contained in start?
    if (last <= start->last)
        return;
        
    next = start;
    while (last >= (next + 1)->first - 1)
    {
        // There is a fragment between two posts.
        R_StoreWallRange(next->last + 1, (next + 1)->first - 1);
        next++;
        
        if (last <= next->last)
        {
            // Bottom is contained in next.
            // Adjust the clip size.
            start->last = next->last;
            goto crunch;
        }
    }
    
    // There is a fragment after *next.
    R_StoreWallRange(next->last + 1, last);
    // Adjust the clip size.
    start->last = last;
    
    // Remove start+1 to next from the clip list,
    // because start now covers their area.
  crunch:
    if (next == start)
    {
        // Post just extended past the bottom of one post.
        return;
    }
    
    while (next++ != newend)
    {
        // Remove a post.
        *++start = *next;
    }

    newend = start + 1;
}

// ============================================================================
void R_RenderSegLoop(int start, int stop);

//
// R_StoreWallRange
// A wall segment will be drawn between start and stop pixels (inclusive).
// Simplified version - just draws the segment
//
void R_StoreWallRange(int start, int stop)
{
    // Sanity check
    if (start < 0) start = 0;
    if (stop >= viewwidth) stop = viewwidth - 1;
    if (start > stop) return;
    
    // Calculate rw_distance for scale calculation
    rw_normalangle = curline->angle + ANG90;
    angle_t offsetangle = abs(rw_normalangle - rw_angle1);
    
    if (offsetangle > ANG90)
        offsetangle = ANG90;
    
    angle_t distangle = ANG90 - offsetangle;
    fixed_t hyp = R_PointToDist(curline->v1->x, curline->v1->y);
    fixed_t sineval = finesine[distangle >> ANGLETOFINESHIFT];
    fixed_t rw_distance = FixedMul(hyp, sineval);
    
    // Calculate scale at both ends
    // Using R_ScaleFromGlobalAngle would be ideal, but we'll use simple projection/distance
    // rw_scale = projection / distance
    if (rw_distance < FRACUNIT) rw_distance = FRACUNIT;
    
    rw_scale = FixedDiv(projection, rw_distance);
    
    // Calculate scale step (simplified - assume linear for now)
    if (stop > start)
    {
        // For now, use a simple step calculation
        // In full Doom, this uses R_ScaleFromGlobalAngle at both ends
        rw_scalestep = 0; // Constant scale for now (parallel walls)
    }
    else
    {
        rw_scalestep = 0;
    }
    
    rw_x = start;
    
    // Render it!
    R_RenderSegLoop(start, stop);
}
// ============================================================================
// R_CheckBBox
// Verifica si un bounding box es potencialmente visible
// ============================================================================
boolean R_CheckBBox(fixed_t* bspcoord)
{
    // TODO: Implementar frustum culling completo
    // Por ahora siempre retornamos true
    return true;
}

// ============================================================================
// R_Subsector
// Renderiza un subsector
// ============================================================================
void R_Subsector(int num)
{
    subsector_t* sub = &subsectors[num];
    
    // Marcar sector como visible
    sub->sector->validcount = validcount;
    
    // printf("DEBUG: Subsector #%d visible, lines: %d\n", num, sub->numlines);
    
    // Renderizar todos los segs del subsector
    // Desde el último hasta el primero (orden frontal)
    for (int i = 0; i < sub->numlines; i++)
    {
        R_AddLine(&segs[sub->firstline + i]);
    }
}

// ============================================================================
// R_Internal: Forward declarations para funciones internas de segs
// ============================================================================
// Globales para renderizado de segmentos declaradas arriba

// ============================================================================

// ============================================================================
// R_Internal: Forward declarations
// ============================================================================
void R_StoreWallRange(int start, int stop);

// ============================================================================
// R_AddLine
// Clips the given segment and adds any visible pieces to the line list
// Adaptación EXACTA de r_segs.c del Doom original
// ============================================================================
void R_AddLine(seg_t* line)
{
    int         x1;
    int         x2;
    angle_t     angle1;
    angle_t     angle2;
    angle_t     span;
    angle_t     tspan;
    extern int debug_segs_count;
    
    curline = line;
    
    // R_PointToAngle devuelve ángulo de mundo
    angle1 = R_PointToAngle(line->v1->x, line->v1->y);
    angle2 = R_PointToAngle(line->v2->x, line->v2->y);
    
    // Clip to view edges
    span = angle1 - angle2;
    
    // Back side? I.e. backface culling?
    if (span >= ANG180) return;
    
    // Global angle needed by segcalc
    rw_angle1 = angle1;
    angle1 -= viewangle;
    angle2 -= viewangle;
    
    tspan = angle1 + clipangle;
    if (tspan > doubleclipangle)
    {
        tspan -= doubleclipangle;
        // Totally off the left edge?
        if (tspan >= span) return;
        angle1 = clipangle;
    }
    
    tspan = clipangle - angle2;
    if (tspan > doubleclipangle)
    {
        tspan -= doubleclipangle;
        // Totally off the left edge?
        if (tspan >= span) return;
        angle2 = -clipangle;
    }
    
    // The seg is in the view range, but not necessarily visible
    angle1 = (angle1 + ANG90) >> ANGLETOFINESHIFT;
    angle2 = (angle2 + ANG90) >> ANGLETOFINESHIFT;
    x1 = viewangletox[angle1];
    x2 = viewangletox[angle2];
    
    // Does not cross a pixel?
    if (x1 == x2) return;
    
    // Single sided line?
    if (!curline->backsector)
    {
        R_ClipSolidWallSegment(x1, x2 - 1);
        return;
    }
    
    // For now, treat all lines as solid (we'll add portal logic later)
    R_ClipSolidWallSegment(x1, x2 - 1);
}

// Globales necesarias que faltaban
// Declared at top of file now

// ============================================================================
// R_DrawColumn
// Forward declaration
extern void R_DrawColumn(int x, int y1, int y2, int color);

// ============================================================================
// R_RenderSegLoop
// Dibuja un segmento de pared visible
// ============================================================================
// Debug counter global
int debug_segs_count = 0;

void R_RenderSegLoop(int start, int stop)
{
    fixed_t scale = rw_scale;

    // Corregir scale si start > viewangletox[angle1] debido a clipping en R_StoreWallRange recursivo
    extern int rw_x; // Debe ser definido y asignado en R_AddLine
    
    // Avanzar scale hasta start
    if (start > rw_x)
    {
        scale += (start - rw_x) * rw_scalestep;
    }
    
    // Obtener sector
    sector_t* frontsector = curline->frontsector;
    if (!frontsector) return; // Should not happen
    
    fixed_t floorheight = frontsector->floorheight;
    fixed_t ceilingheight = frontsector->ceilingheight;
    
    // Alturas relativas a la cámara
    fixed_t yz_top = ceilingheight - viewz;
    fixed_t yz_bottom = floorheight - viewz;
    
    // Debug eliminado (SegLoop start)

    for (int x = start; x <= stop; x++)
    {
        // Calcular Y top/bottom en pantalla
        // y = centery - (height * scale) >> FRACBITS
        // Calcular Y top/bottom en pantalla
        // y = centery - (height * scale)
        // height (yz_top) es 16.16
        // scale es 16.16 (o debería serlo, aunque logs dicen 33, asumimos que es fixed point small value 0.0005)
        // Result need to be integer pixels.
        // (yz_top * scale) is 32.32
        // >> 16 gives 32.16 (Fixed Point Pixels)
        // >> 16 again gives integer pixels.
        
        // CORRECCIÓN: Shift total 32 bits (16 para fixed->fixed mul, 16 para fixed->int)
        // Ojo: (int64_t) mantiene precisión.
        // Pero FixedMul de Doom es (a*b)>>16.
        // Si queremos INT pixels: (FixedMul(h, s)) >> 16.
        
    int y1 = centery - (int)(( (int64_t)yz_top * scale ) >> 32);
        int y2 = centery - (int)(( (int64_t)yz_bottom * scale ) >> 32);
        
        // Debug eliminado
        
        // Dibujar columna
        // R_DrawColumn recorta si se sale, pero better safe
        R_DrawColumn(x, y1, y2, 0xFFFFFFFF);
        
        scale += rw_scalestep;
    }
}

angle_t rw_angle1;


// ============================================================================
// R_DrawPlanes
// Dibuja todos los planos visibles (suelos y techos)
// ============================================================================
void R_DrawPlanes(void)
{
    // TODO: Renderizar visplanes
}

// ============================================================================
// R_DrawMasked
// Dibuja sprites y texturas masked
// ============================================================================
void R_DrawMasked(void)
{
    // TODO: Renderizar sprites y masked walls
}
