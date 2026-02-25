/*
 * libmod_ray_render_slopes.c
 * 
 * Este archivo ha sido vaciado ya que el soporte para slopes geométricos
 * ha sido reemplazado por el uso de modelos MD3 instanciados.
 * 
 * Se mantiene el archivo vacío para evitar errores de compilación en
 * scripts de build existentes hasta que se actualice CMakeLists.txt.
 */

#include "libmod_ray.h"

/* Stub function to satisfy any potential lingering references (though unlikely) */
void draw_slope_column(GRAPH *dest, int x, int y_start, int y_end, 
                       const RAY_Sector *sector, GRAPH *texture, int is_ceiling)
{
    /* No-op */
}
