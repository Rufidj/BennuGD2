#include <math.h>  
#include "bgddl.h"  
#include "libbggfx.h"  
#include <stdio.h> // Para printf de depuración  
  
static int basic_initialized = 0;  
  
// Estado efecto OutRun  
static float outrun_position = 0.0f;  
static float outrun_speed = 0.5f;  
  
// Colores para el efecto OutRun  
static uint32_t color_road = 0xFF444444;  
static uint32_t color_border = 0xFFFF0000;  
static uint32_t color_grass1 = 0xFF007700;  
static uint32_t color_grass2 = 0xFF003300;  
static uint32_t color_sky = 0xFF3399FF;  
  
// Variables de color globales del sistema de dibujo  
extern uint8_t drawing_color_r;  
extern uint8_t drawing_color_g;  
extern uint8_t drawing_color_b;  
extern uint8_t drawing_color_a;  
  
// Variables de pantalla globales  
extern int64_t scr_width;  
extern int64_t scr_height;  
  
// Funciones de dibujo externas  
extern void draw_rectangle_filled(GRAPH *dest, REGION *clip, int64_t x, int64_t y, int64_t w, int64_t h);  
extern GRAPH * bitmap_new(int64_t code, int64_t width, int64_t height, SDL_Surface * surface);  
extern void bitmap_destroy(GRAPH *gr);  
extern void gr_clear(GRAPH *dest);  
extern void gr_blit(GRAPH *dest, REGION *clip, double scrx, double scry, int64_t flags, int64_t angle, double scalex, double scaley, double centerx, double centery, GRAPH *gr, BGD_Rect *gr_clip, uint8_t alpha, uint8_t color_r, uint8_t color_g, uint8_t color_b, BLENDMODE blend_mode, CUSTOM_BLENDMODE * custom_blendmode);  
  
// GRAPH temporal para dibujar el efecto OutRun  
static GRAPH *outrun_render_target = NULL;  
  
// Función para dibujar rectángulos con color en un GRAPH específico  
static void draw_rect_color(GRAPH *dest_graph, int x, int y, int w, int h, uint32_t color) {  
    printf("draw_rect_color: Entrando. Dest: %p, x=%d, y=%d, w=%d, h=%d, color=0x%X\n", (void*)dest_graph, x, y, w, h, color);  
    // Extraer componentes RGBA  
    uint8_t r = (color >> 16) & 0xFF;  
    uint8_t g = (color >> 8) & 0xFF;  
    uint8_t b = color & 0xFF;  
    uint8_t a = (color >> 24) & 0xFF;  
      
    // Configurar color de dibujo  
    drawing_color_r = r;  
    drawing_color_g = g;  
    drawing_color_b = b;  
    drawing_color_a = a;  
  
    // Asegurarse de que el ancho y alto sean positivos  
    if (w < 0) w = 0;  
    if (h < 0) h = 0;  
      
    printf("draw_rect_color: Dibujando rect. x=%d, y=%d, w=%d, h=%d, r=%d, g=%d, b=%d, a=%d\n", x, y, w, h, r, g, b, a);  
      
    // Dibujar rectángulo relleno en el GRAPH de destino  
    draw_rectangle_filled(dest_graph, NULL, x, y, w, h);  
    printf("draw_rect_color: Saliendo.\n");  
}  
  
// Dibujar una línea de la carretera en un GRAPH específico  
static void draw_scanline(GRAPH *dest_graph, int y, int left_x, int right_x, int border_width) {  
    printf("draw_scanline: Entrando. Dest: %p, y=%d, left_x=%d, right_x=%d, border_width=%d\n", (void*)dest_graph, y, left_x, right_x, border_width);  
    int screen_width = (int)scr_width;  
      
    // Asegurarse de que los anchos sean positivos  
    int grass1_w = left_x;  
    int border1_w = border_width;  
    int road_w = right_x - left_x - 2 * border_width;  
    int border2_w = border_width;  
    int grass2_w = screen_width - right_x;  
  
    if (grass1_w < 0) grass1_w = 0;  
    if (border1_w < 0) border1_w = 0;  
    if (road_w < 0) road_w = 0;  
    if (border2_w < 0) border2_w = 0;  
    if (grass2_w < 0) grass2_w = 0;  
  
    // Césped izquierdo  
    draw_rect_color(dest_graph, 0, y, grass1_w, 1, color_grass1);  
      
    // Borde izquierdo  
    draw_rect_color(dest_graph, left_x, y, border1_w, 1, color_border);  
      
    // Carretera  
    draw_rect_color(dest_graph, left_x + border_width, y, road_w, 1, color_road);  
      
    // Borde derecho  
    draw_rect_color(dest_graph, right_x - border_width, y, border2_w, 1, color_border);  
      
    // Césped derecho  
    draw_rect_color(dest_graph, right_x, y, grass2_w, 1, color_grass2);  
    printf("draw_scanline: Saliendo.\n");  
}  
  
// Dibujar el cielo en un GRAPH específico  
static void draw_sky(GRAPH *dest_graph) {  
    printf("draw_sky: Entrando. Dest: %p\n", (void*)dest_graph);  
    draw_rect_color(dest_graph, 0, 0, (int)scr_width, (int)scr_height / 3, color_sky);  
    printf("draw_sky: Saliendo.\n");  
}  
  
// Dibujar la carretera con perspectiva y curvas en un GRAPH específico  
static void draw_road(GRAPH *dest_graph, float position) {  
    printf("draw_road: Entrando. Dest: %p, position=%f\n", (void*)dest_graph, position);  
    int horizon = (int)scr_height / 3;  
    float road_width = (float)scr_width * 0.8f;  
    float road_center = (float)scr_width / 2;  
  
    for (int y = horizon; y < (int)scr_height; y++) {  
        float perspective = (float)(y - horizon) / ((float)scr_height - horizon);  
          
        // Curvas sinusoidales para simular el efecto OutRun  
        float curve = sinf(position * 0.1f + perspective * 5.0f) * 100.0f;  
          
        // Colinas suaves  
        float hill = sinf(position * 0.05f + perspective * 3.0f) * 10.0f;  
  
        int left_x = (int)(road_center + curve * (1.0f - perspective) - road_width * perspective / 2);  
        int right_x = (int)(road_center + curve * (1.0f - perspective) + road_width * perspective / 2);  
        int border_width = (int)(20 * (1.0f - perspective));  
  
        // Asegurarse de que left_x no sea mayor que right_x  
        if (left_x > right_x) {  
            int temp = left_x;  
            left_x = right_x;  
            right_x = temp;  
        }  
  
        draw_scanline(dest_graph, y + (int)hill, left_x, right_x, border_width);  
    }  
    printf("draw_road: Saliendo.\n");  
}  
  
// Funciones exportadas  
int64_t libmod_basic_init_outrun(INSTANCE *my, int64_t *params) {  
    printf("libmod_basic_init_outrun: Entrando.\n");  
    (void)my; (void)params;  
    outrun_position = 0.0f;  
    printf("libmod_basic_init_outrun: Saliendo.\n");  
    return 1;  
}  
  
int64_t libmod_basic_update_outrun(INSTANCE *my, int64_t *params) {  
    printf("libmod_basic_update_outrun: Entrando.\n");  
    (void)my; (void)params;  
    outrun_position += outrun_speed;  
    if (outrun_position > 1000000.0f) outrun_position = 0.0f;  
    printf("libmod_basic_update_outrun: Saliendo.\n");  
    return 1;  
}  
  
int64_t libmod_basic_draw_outrun(INSTANCE *my, int64_t *params) {  
    printf("libmod_basic_draw_outrun: Entrando.\n");  
    (void)my; (void)params;  
      
    if (!outrun_render_target) {  
        printf("libmod_basic_draw_outrun: outrun_render_target es NULL. Intentando crear...\n");  
        if (scr_width > 0 && scr_height > 0) {  
            outrun_render_target = bitmap_new(0, scr_width, scr_height, NULL);  
            if (!outrun_render_target) {  
                printf("Error: No se pudo crear outrun_render_target en draw_outrun. Retornando 0.\n");  
                return 0;  
            }  
            printf("libmod_basic_draw_outrun: outrun_render_target creado exitosamente: %p\n", (void*)outrun_render_target);  
        } else {  
            printf("Advertencia: scr_width o scr_height no válidos (%lldx%lld) en draw_outrun. Retornando 0.\n", scr_width, scr_height);  
            return 0;  
        }  
    }  
  
    printf("libmod_basic_draw_outrun: Limpiando outrun_render_target: %p\n", (void*)outrun_render_target);  
    gr_clear(outrun_render_target);  
    printf("libmod_basic_draw_outrun: outrun_render_target limpiado.\n");  
  
    printf("libmod_basic_draw_outrun: Dibujando cielo y carretera en outrun_render_target.\n");  
    draw_sky(outrun_render_target);  
    draw_road(outrun_render_target, outrun_position);  
    printf("libmod_basic_draw_outrun: Cielo y carretera dibujados.\n");  
  
    printf("libmod_basic_draw_outrun: Bliteando outrun_render_target a pantalla principal.\n");  
    gr_blit(NULL, NULL, 0.0, 0.0, 0, 0, 100.0, 100.0, 0.0, 0.0, outrun_render_target, NULL, 255, 255, 255, 255, 0, NULL);
    printf("libmod_basic_draw_outrun: Blit completado. Saliendo.\n");  
      
    return 1;  
}  
  
int64_t libmod_basic_return_42(INSTANCE *my, int64_t *params) {  
    (void)my; (void)params;  
    return 42;  
}  
  
// Exportaciones del módulo  
DLSYSFUNCS __bgdexport(libmod_basic, functions_exports)[] = {  
    FUNC("INIT_OUTRUN", "", TYPE_QWORD, libmod_basic_init_outrun),  
    FUNC("UPDATE_OUTRUN", "", TYPE_QWORD, libmod_basic_update_outrun),  
    FUNC("DRAW_OUTRUN", "", TYPE_QWORD, libmod_basic_draw_outrun),  
    FUNC("RETURN_42", "", TYPE_QWORD, libmod_basic_return_42),  
    FUNC(0, 0, 0, 0)  
};  
  
char * __bgdexport(libmod_basic, modules_dependency)[] = {  
    "libbggfx",  
    NULL  
};  
  
void __bgdexport(libmod_basic, module_initialize)() {  
    printf("module_initialize: Entrando.\n");  
    if (!basic_initialized) {  
        basic_initialized = 1;  
        outrun_position = 0.0f;  
        outrun_speed = 0.5f;  
        printf("module_initialize: scr_width = %lld, scr_height = %lld\n", scr_width, scr_height);  
        if (scr_width > 0 && scr_height > 0) {  
            outrun_render_target = bitmap_new(0, scr_width, scr_height, NULL);  
              

                }
            
            }
        }