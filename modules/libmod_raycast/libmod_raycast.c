/*  
 *  Copyright (C) SplinterGU (Fenix/BennuGD) (Since 2006)  
 *  
 *  This file is part of Bennu Game Development  
 */  
  
#include "bgddl.h"  
#include "libbggfx.h"  
#include "libmod_raycast.h"  
  
// Variables globales del motor  
static RaycastEngine engine;  
static GRAPH * raycast_bitmap = NULL;  
static int64_t raycast_file_id = 0;  
static int64_t raycast_graph_id = 1;  
  
// Funciones auxiliares internas  
static float deg_to_rad(float degrees) {  
    return degrees * PI / 180.0f;  
}  
  
static float rad_to_deg(float radians) {  
    return radians * 180.0f / PI;  
}  
  
static uint32_t rgb_to_color(int r, int g, int b) {  
    return (0xFF << 24) | (r << 16) | (g << 8) | b;  
}  
  
static void cast_ray(RaycastEngine *engine, float ray_angle, int strip_id) {  
    float ray_cos = cosf(ray_angle);  
    float ray_sin = sinf(ray_angle);  
      
    // Variables para el algoritmo DDA  
    int map_x = (int)engine->player.x;  
    int map_y = (int)engine->player.y;  
      
    float delta_dist_x = fabsf(1.0f / ray_cos);  
    float delta_dist_y = fabsf(1.0f / ray_sin);  
      
    float perp_wall_dist;  
    int step_x, step_y;  
    int hit = 0;  
    int side;  
      
    float side_dist_x, side_dist_y;  
      
    if (ray_cos < 0) {  
        step_x = -1;  
        side_dist_x = (engine->player.x - map_x) * delta_dist_x;  
    } else {  
        step_x = 1;  
        side_dist_x = (map_x + 1.0f - engine->player.x) * delta_dist_x;  
    }  
      
    if (ray_sin < 0) {  
        step_y = -1;  
        side_dist_y = (engine->player.y - map_y) * delta_dist_y;  
    } else {  
        step_y = 1;  
        side_dist_y = (map_y + 1.0f - engine->player.y) * delta_dist_y;  
    }  
      
    // Algoritmo DDA  
    while (hit == 0) {  
        if (side_dist_x < side_dist_y) {  
            side_dist_x += delta_dist_x;  
            map_x += step_x;  
            side = 0;  
        } else {  
            side_dist_y += delta_dist_y;  
            map_y += step_y;  
            side = 1;  
        }  
          
        if (map_x < 0 || map_x >= engine->map_width ||   
            map_y < 0 || map_y >= engine->map_height ||  
            engine->map_data[map_y * engine->map_width + map_x] > 0) {  
            hit = 1;  
        }  
    }  
      
    // Calcular distancia  
    if (side == 0) {  
        perp_wall_dist = (map_x - engine->player.x + (1 - step_x) / 2) / ray_cos;  
    } else {  
        perp_wall_dist = (map_y - engine->player.y + (1 - step_y) / 2) / ray_sin;  
    }  
      
    // Calcular altura de la línea a dibujar  
    int line_height = (int)(engine->height / perp_wall_dist);  
      
    // Calcular píxeles de inicio y fin  
    int draw_start = -line_height / 2 + engine->height / 2;  
    if (draw_start < 0) draw_start = 0;  
      
    int draw_end = line_height / 2 + engine->height / 2;  
    if (draw_end >= engine->height) draw_end = engine->height - 1;  
      
    // Determinar color de la pared  
    uint32_t color;  
    int wall_type = 0;  
    if (map_x >= 0 && map_x < engine->map_width &&   
        map_y >= 0 && map_y < engine->map_height) {  
        wall_type = engine->map_data[map_y * engine->map_width + map_x];  
    }  
      
    switch (wall_type) {  
        case 1: color = rgb_to_color(255, 0, 0); break;    // Rojo  
        case 2: color = rgb_to_color(0, 255, 0); break;    // Verde  
        case 3: color = rgb_to_color(0, 0, 255); break;    // Azul  
        case 4: color = rgb_to_color(255, 255, 0); break;  // Amarillo  
        default: color = rgb_to_color(128, 128, 128); break; // Gris  
    }  
      
    // Aplicar sombreado según el lado  
    if (side == 1) {  
        int r = (color >> 16) & 0xFF;  
        int g = (color >> 8) & 0xFF;  
        int b = color & 0xFF;  
        color = rgb_to_color(r/2, g/2, b/2);  
    }  
      
    // Dibujar la línea vertical  
    for (int y = 0; y < engine->height; y++) {  
        if (y < draw_start) {  
            // Cielo  
            engine->framebuffer[y * engine->width + strip_id] = rgb_to_color(135, 206, 235);  
        } else if (y >= draw_start && y <= draw_end) {  
            // Pared  
            engine->framebuffer[y * engine->width + strip_id] = color;  
        } else {  
            // Suelo  
            engine->framebuffer[y * engine->width + strip_id] = rgb_to_color(34, 139, 34);  
        }  
    }  
      
    // Guardar distancia en z-buffer  
    engine->z_buffer[strip_id] = perp_wall_dist;  
}  
  
// Funciones del motor de raycasting  
int raycast_init(RaycastEngine *engine, int width, int height) {  
    if (!engine || width <= 0 || height <= 0) return -1;  
      
    // Inicializar framebuffer  
    engine->framebuffer = (uint32_t*)malloc(width * height * sizeof(uint32_t));  
    if (!engine->framebuffer) return -1;  
      
    // Inicializar z-buffer  
    engine->z_buffer = (float*)malloc(width * sizeof(float));  
    if (!engine->z_buffer) {  
        free(engine->framebuffer);  
        return -1;  
    }  
      
    engine->width = width;  
    engine->height = height;  
      
    // Inicializar jugador  
    engine->player.x = 2.0f;  
    engine->player.y = 2.0f;  
    engine->player.angle = 0.0f;  
    engine->player.fov = FOV;  
      
    // Inicializar mapa por defecto  
    engine->map_width = 8;  
    engine->map_height = 8;  
    engine->map_data = (int*)malloc(64 * sizeof(int));  
    if (!engine->map_data) {  
        free(engine->framebuffer);  
        free(engine->z_buffer);  
        return -1;  
    }  
      
    // Mapa de prueba  
    int default_map[64] = {  
        1,1,1,1,1,1,1,1,  
        1,0,0,0,0,0,0,1,  
        1,0,1,0,0,1,0,1,  
        1,0,0,0,0,0,0,1,  
        1,0,0,2,2,0,0,1,  
        1,0,0,0,0,0,0,1,  
        1,0,0,0,0,0,0,1,  
        1,1,1,1,1,1,1,1  
    };  
    memcpy(engine->map_data, default_map, 64 * sizeof(int));  
      
    engine->texture_count = 0;  
    engine->initialized = 1;  
      
    return 0;  
}  
  
void raycast_cleanup(RaycastEngine *engine) {  
    if (!engine) return;  
      
    if (engine->framebuffer) {  
        free(engine->framebuffer);  
        engine->framebuffer = NULL;  
    }  
      
    if (engine->z_buffer) {  
        free(engine->z_buffer);  
        engine->z_buffer = NULL;  
    }  
      
    if (engine->map_data) {  
        free(engine->map_data);  
        engine->map_data = NULL;  
    }  
      
    // Limpiar texturas  
    for (int i = 0; i < engine->texture_count; i++) {  
        if (engine->textures[i].data) {  
            free(engine->textures[i].data);  
            engine->textures[i].data = NULL;  
        }  
    }  
      
    engine->initialized = 0;  
}  
  
void raycast_render(RaycastEngine *engine) {  
    if (!engine || !engine->initialized) return;  
      
    float ray_angle_start = engine->player.angle - deg_to_rad(engine->player.fov / 2.0f);  
    float ray_angle_step = deg_to_rad(engine->player.fov) / engine->width;  
      
    for (int x = 0; x < engine->width; x++) {  
        float ray_angle = ray_angle_start + x * ray_angle_step;  
        cast_ray(engine, ray_angle, x);  
    }  
}  
  
int raycast_load_map(RaycastEngine *engine, int width, int height, int *data) {  
    if (!engine || !data || width <= 0 || height <= 0) return -1;  
      
    if (engine->map_data) {  
        free(engine->map_data);  
    }  
      
    engine->map_data = (int*)malloc(width * height * sizeof(int));  
    if (!engine->map_data) return -1;  
      
    memcpy(engine->map_data, data, width * height * sizeof(int));  
    engine->map_width = width;  
    engine->map_height = height;  
      
    return 0;  
}  
  
int raycast_set_wall_texture(RaycastEngine *engine, int wall_id, int texture_id) {  
    if (!engine || wall_id < 0 || texture_id < 0 || texture_id >= engine->texture_count) {  
        return -1;  
    }  
      
    // Esta función se puede expandir para mapear tipos de pared a texturas  
    return 0;  
}  
  
void raycast_move_player(RaycastEngine *engine, float distance) {  
    if (!engine || !engine->initialized) return;  
      
    float new_x = engine->player.x + cosf(engine->player.angle) * distance;  
    float new_y = engine->player.y + sinf(engine->player.angle) * distance;  
      
    // Verificar colisiones  
    int map_x = (int)new_x;  
    int map_y = (int)new_y;  
      
    if (map_x >= 0 && map_x < engine->map_width &&   
        map_y >= 0 && map_y < engine->map_height &&  
        engine->map_data[map_y * engine->map_width + map_x] == 0) {  
        engine->player.x = new_x;  
        engine->player.y = new_y;  
    }  
}  
  
void raycast_rotate_player(RaycastEngine *engine, float angle) {  
    if (!engine || !engine->initialized) return;  
      
    engine->player.angle += angle;  
      
    // Normalizar ángulo  
    while (engine->player.angle < 0) engine->player.angle += 2 * PI;  
    while (engine->player.angle >= 2 * PI) engine->player.angle -= 2 * PI;  
}  
  
// Funciones de inicialización del módulo  
void __bgdexport(libmod_raycast, module_initialize)() {  
    // Inicialización básica del motor  
    memset(&engine, 0, sizeof(RaycastEngine));  
    engine.initialized = 0;  
}  
  
void __bgdexport(libmod_raycast, module_finalize)() {  
    if (engine.initialized) {  
        raycast_cleanup(&engine);  
    }  
    if (raycast_bitmap) {  
        bitmap_destroy(raycast_bitmap);  
        raycast_bitmap = NULL;  
    }  
}  
  
// Función de inicialización del motor de raycasting  
int64_t libmod_raycast_init_engine(INSTANCE * my, int64_t * params) {  
    int width = params[0];  
    int height = params[1];  
      
    if (width <= 0 || height <= 0) return 0;  
      
    // Crear el bitmap donde renderizaremos el raycasting  
    raycast_bitmap = bitmap_new(raycast_graph_id, width, height, NULL);  
    if (!raycast_bitmap) return 0;  
      
    // Inicializar el motor de raycasting  
    if (raycast_init(&engine, width, height) != 0) {  
        bitmap_destroy(raycast_bitmap);  
        raycast_bitmap = NULL;  
        return 0;  
    }  
      
    // Registrar el bitmap en el sistema de BennuGD2  
    grlib_add_map(raycast_file_id, raycast_bitmap);  
      
    engine.initialized = 1;  
    return raycast_graph_id; // Retorna el ID del gráfico creado  
}  
  
// Función principal de renderizado  
int64_t libmod_raycast_render_frame(INSTANCE * my, int64_t * params) {  
    if (!engine.initialized || !raycast_bitmap) return 0;  
      
    // Preparar el renderer para dibujar en nuestro bitmap  
    BLENDMODE blend_mode = BLEND_DISABLED;  
    if (gr_prepare_renderer(raycast_bitmap, NULL, 0, &blend_mode)) return 0;  
      
    // Limpiar el bitmap antes de renderizar  
    gr_clear(raycast_bitmap);  
      
    // Renderizar el raycasting  
    raycast_render(&engine);  
      
    // Copiar los datos del raycasting al bitmap de BennuGD2  
    for (int y = 0; y < engine.height; y++) {  
        for (int x = 0; x < engine.width; x++) {  
            uint32_t color = engine.framebuffer[y * engine.width + x];  
            gr_put_pixel(raycast_bitmap, x, y, color);  
        }  
    }  
      
    // Marcar el bitmap como "sucio" para que se actualice la  
      // Marcar el bitmap como "sucio" para que se actualice la textura  
    raycast_bitmap->dirty = 1;  
      
    return 1;  
}  
  
// Funciones auxiliares del raycasting  
int64_t libmod_raycast_set_player_pos(INSTANCE * my, int64_t * params) {  
    if (!engine.initialized) return 0;  
      
    float x = *(float*)&params[0];  
    float y = *(float*)&params[1];  
    float angle = *(float*)&params[2];  
      
    engine.player.x = x;  
    engine.player.y = y;  
    engine.player.angle = angle;  
      
    return 1;  
}  
  
int64_t libmod_raycast_load_map(INSTANCE * my, int64_t * params) {  
    if (!engine.initialized) return 0;  
      
    int width = params[0];  
    int height = params[1];  
    int * map_data = (int*)params[2];  
      
    return raycast_load_map(&engine, width, height, map_data);  
}  
  
int64_t libmod_raycast_set_wall_texture(INSTANCE * my, int64_t * params) {  
    if (!engine.initialized) return 0;  
      
    int wall_id = params[0];  
    int texture_id = params[1];  
      
    return raycast_set_wall_texture(&engine, wall_id, texture_id);  
}  
  
int64_t libmod_raycast_move_player(INSTANCE * my, int64_t * params) {  
    if (!engine.initialized) return 0;  
      
    float distance = *(float*)&params[0];  
    raycast_move_player(&engine, distance);  
      
    return 1;  
}  
  
int64_t libmod_raycast_rotate_player(INSTANCE * my, int64_t * params) {  
    if (!engine.initialized) return 0;  
      
    float angle = *(float*)&params[0];  
    raycast_rotate_player(&engine, angle);  
      
    return 1;  
}  
  
// Funciones adicionales  
int64_t libmod_raycast_load_texture(INSTANCE * my, int64_t * params) {  
    if (!engine.initialized) return 0;  
      
    int texture_id = params[0];  
    int file_id = params[1];  
    int graph_id = params[2];  
      
    if (texture_id < 0 || texture_id >= MAX_TEXTURES) return 0;  
      
    GRAPH * texture_graph = bitmap_get(file_id, graph_id);  
    if (!texture_graph) return 0;  
      
    // Copiar datos de textura (simplificado)  
    engine.textures[texture_id].width = texture_graph->width;  
    engine.textures[texture_id].height = texture_graph->height;  
      
    if (texture_id >= engine.texture_count) {  
        engine.texture_count = texture_id + 1;  
    }  
      
    return 1;  
}  
  
int64_t libmod_raycast_get_player_pos(INSTANCE * my, int64_t * params) {  
    if (!engine.initialized) return 0;  
      
    float *x_ptr = (float*)params[0];  
    float *y_ptr = (float*)params[1];  
    float *angle_ptr = (float*)params[2];  
      
    if (x_ptr) *x_ptr = engine.player.x;  
    if (y_ptr) *y_ptr = engine.player.y;  
    if (angle_ptr) *angle_ptr = engine.player.angle;  
      
    return 1;  
}