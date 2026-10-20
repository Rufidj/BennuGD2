#!/usr/bin/env python3
"""Traducción de símbolos de BennuGD1 a BennuGD2.

Nada de esto está inventado: los renombrados se derivan o se comprueban contra
las tablas de símbolos extraídas de los dos árboles de fuentes
(`build_symtab.py`), y sólo se aplica un renombrado si el nombre destino existe
de verdad en BennuGD2 con una aridad compatible.

Lo que no tiene equivalente NO se traduce a la ligera: se deja el nombre
original y se marca con un TODO, porque una traducción inventada es peor que
una marca honesta.
"""
import json, os, re

HERE = os.path.dirname(os.path.abspath(__file__))

# Renombrados comprobados uno a uno contra la tabla de BennuGD2 (nombre y nº de
# argumentos). Los que el heurístico ya deduce solo no hace falta listarlos.
CURATED = {
    "FILE_EXISTS":        "FEXISTS",
    "DRAW_RECT":          "DRAW_RECTANGLE",
    "WRITE_INT":          "WRITE_VAR",
    "LOAD_PNG":           "MAP_LOAD",
    "SAVE_PNG":           "MAP_SAVE",
    "IS_PLAYING_SONG":    "MUSIC_IS_PLAYING",
    "IS_PLAYING_WAV":     "SOUND_IS_PLAYING",
    "SET_CHANNEL_VOLUME": "CHANNEL_SET_VOLUME",
    "SET_SONG_VOLUME":    "MUSIC_SET_VOLUME",
    "SET_ICON":           "WINDOW_SET_ICON",
    "GET_DESKTOP_SIZE":   "DESKTOP_GET_SIZE",
}

# Globales sueltas de BennuGD1 que en BennuGD2 pasaron a campos de struct.
# Sólo las que NO se deducen del nombre del campo (ahí el nombre cambió).
# El resto (scale_resolution, fullscreen, ...) las resuelve `global_fields`.
GLOBAL_MAP = {
    "SOUND_FREQ":     "sound.freq",
    "SOUND_MODE":     "sound.mode",
    "SOUND_CHANNELS": "sound.channels",
    "FULL_SCREEN":    "screen.fullscreen",
    "SCALE_MODE":     "screen.scale_resolution",
    "SCALE_RESOLUTION_ORIENTATION": None,   # no existe en BennuGD2
}

# Sin equivalente: BennuGD2 dejó de trabajar con paletas de 8 bits.
NO_EQUIV_NOTE = {
    "LOAD_PAL": "BennuGD2 no usa paletas de 8 bits",
    "NEW_PAL": "BennuGD2 no usa paletas de 8 bits",
    "UNLOAD_PAL": "BennuGD2 no usa paletas de 8 bits",
    "PAL_GET": "BennuGD2 no usa paletas de 8 bits",
    "PAL_SET": "BennuGD2 no usa paletas de 8 bits",
    "PAL_CLONE": "BennuGD2 no usa paletas de 8 bits",
    "PAL_REFRESH": "BennuGD2 no usa paletas de 8 bits",
    "PAL_MAP_ASSIGN": "ver MAP_SET_PALETTE",
    "PAL_MAP_REMOVE": "ver MAP_SET_PALETTE",
    "GRAPH_MODE": "modo de vídeo: BennuGD2 lo resuelve en SET_MODE",
    "RESTORE_TYPE": "el pintado por regiones ya no existe",
    "DUMP_TYPE": "el pintado por regiones ya no existe",
    "ALPHA_STEPS": "el blending es por ALPHA directo",
}

# Funciones que existen en las dos versiones pero con firma o semántica distinta.
# Se redirigen a un shim generado, en vez de reescribir las llamadas a ciegas.
# (nombre v1, nº args v1) -> (nombre del shim, código del shim)
SHIMS = {
    ("SET_TITLE", 1): ("bgd1_set_title", [
        "FUNCTION bgd1_set_title(STRING t)",
        "BEGIN",
        "    // SDL 1.2 admitia el titulo ANTES de crear la ventana y lo conservaba.",
        "    // En BennuGD2, gr_set_caption() hace SDL_SetWindowTitle(gWindow, ...) y",
        "    // sin ventana no hace nada; y ademas gr_set_mode() lo reescribe con el",
        "    // titulo del motor al crearla. Casi todos los juegos ponen el titulo",
        "    // antes del set_mode, asi que se perdia siempre. Se guarda para poder",
        "    // reponerlo despues de cada set_mode.",
        "    BGD1_TITULO = t;",
        "    window_set_title(t);",
        "    RETURN 0;",
        "END",
    ]),
    ("SET_MODE", 3): ("bgd1_set_mode", [
        "FUNCTION bgd1_set_mode(INT w, INT h, INT f)",
        "PRIVATE",
        "    INT r;",
        "BEGIN",
        "    // set_mode() crea la ventana y le pone el titulo del motor, borrando el",
        "    // que hubiera puesto el juego: hay que reponerlo.",
        "    r = set_mode(w, h, f);",
        "    IF (BGD1_TITULO != \"\") window_set_title(BGD1_TITULO); END",
        "    RETURN r;",
        "END",
    ]),
    ("SET_MODE", 2): ("bgd1_set_mode2", [
        "FUNCTION bgd1_set_mode2(INT w, INT h)",
        "PRIVATE",
        "    INT r;",
        "BEGIN",
        "    r = set_mode(w, h);",
        "    IF (BGD1_TITULO != \"\") window_set_title(BGD1_TITULO); END",
        "    RETURN r;",
        "END",
    ]),
    ("JOY_GETHAT", 2): ("bgd1_joy_gethat", [
        "FUNCTION bgd1_joy_gethat(INT j, INT h)",
        "PRIVATE",
        "    INT v;",
        "BEGIN",
        "    // SDL 1.2 exponia la cruceta como un HAT; SDL2 la expone como BOTONES",
        "    // (11=arriba, 12=abajo, 13=izquierda, 14=derecha, el orden estandar de",
        "    // SDL_GameController). Con el mismo mando, BennuGD1 devuelve valores de",
        "    // hat 1/2/4/8 y BennuGD2 devuelve SIEMPRE 0, asi que la cruceta queda",
        "    // muerta en el portado. Aqui se reconstruye el hat a partir de los",
        "    // botones, con la mascara de SDL 1.2: 1=arriba 2=derecha 4=abajo 8=izq.",
        "    v = joy_gethat(j, h);",
        "    IF (v != 0) RETURN v; END",
        "    IF (get_joy_button(j, 11) == 1) v = v | 1; END",
        "    IF (get_joy_button(j, 14) == 1) v = v | 2; END",
        "    IF (get_joy_button(j, 12) == 1) v = v | 4; END",
        "    IF (get_joy_button(j, 13) == 1) v = v | 8; END",
        "    RETURN v;",
        "END",
    ]),
    ("JOY_GETHAT", 1): ("bgd1_joy_gethat1", [
        "FUNCTION bgd1_joy_gethat1(INT h)",
        "PRIVATE",
        "    INT v;",
        "BEGIN",
        "    // misma reconstruccion que bgd1_joy_gethat, para el joystick 0",
        "    v = joy_gethat(0, h);",
        "    IF (v != 0) RETURN v; END",
        "    IF (get_joy_button(0, 11) == 1) v = v | 1; END",
        "    IF (get_joy_button(0, 14) == 1) v = v | 2; END",
        "    IF (get_joy_button(0, 12) == 1) v = v | 4; END",
        "    IF (get_joy_button(0, 13) == 1) v = v | 8; END",
        "    RETURN v;",
        "END",
    ]),
    ("LOAD_FPG", 1): ("bgd1_load_fpg", [
        "FUNCTION bgd1_load_fpg(STRING fichero)",
        "PRIVATE",
        "    INT f;",
        "BEGIN",
        "    // Ademas de cargar el FPG se guarda SU paleta original: es la que el",
        "    // shader necesita como punto de partida para intercambiar colores,",
        "    // porque BennuGD2 entrega los graficos ya convertidos a RGBA.",
        "    f = fpg_load(fichero);",
        "    IF (f >= 0 && f <= 255) BGD1_BASE[f] = load_pal(fichero); END",
        "    RETURN f;",
        "END",
    ]),
    ("MAP_GET_PIXEL", 4): ("bgd1_map_get_pixel", [
        "FUNCTION bgd1_map_get_pixel(INT fich, INT graf, INT px, INT py)",
        "PRIVATE",
        "    INT col, cr, cv, cb;",
        "BEGIN",
        "    // map_get_pixel() devuelve el color en el formato del grafico. En",
        "    // BennuGD1 un grafico de 16 bits daba RGB565 (blanco = 65535); en",
        "    // BennuGD2 todo se maneja en 32 bits y devuelve RGBA8888. Un juego",
        "    // de BennuGD1 que compare pixeles contra literales (mascaras,",
        "    // comprobaciones de integridad, deteccion de colisiones por color)",
        "    // falla si no se convierte de vuelta.",
        "    col = map_get_pixel(fich, graf, px, py);",
        "    // BennuGD2 convierte todo a 32 bits al cargar, asi que su G_DEPTH",
        "    // siempre dice 32: la profundidad de la que hay que convertir es la",
        "    // que usaba el programa original, detectada al portar.",
        "    IF (BGD1_MAP_DEPTH != 16) RETURN col; END",
        "    cr = (col >> 24) & 255;",
        "    cv = (col >> 16) & 255;",
        "    cb = (col >>  8) & 255;",
        "    RETURN ((cr >> 3) << 11) | ((cv >> 2) << 5) | (cb >> 3);",
        "END",
    ]),
    ("FADE_ON", 0): ("bgd1_fade_on", [
        "FUNCTION bgd1_fade_on()",
        "BEGIN",
        "    // BennuGD1: gr_fade_init(100,100,100,16) -- speed, no milisegundos",
        "    RETURN fade_on(BGD1_FADE_MS);",
        "END",
    ]),
    ("FADE_OFF", 0): ("bgd1_fade_off", [
        "FUNCTION bgd1_fade_off()",
        "BEGIN",
        "    // BennuGD1: gr_fade_init(0,0,0,16) -- speed, no milisegundos",
        "    RETURN fade_off(BGD1_FADE_MS);",
        "END",
    ]),
    ("FADE", 4): ("bgd1_fade", [
        "FUNCTION bgd1_fade(INT r, INT g, INT b, INT vel)",
        "PRIVATE",
        "    INT medio, alfa, ms;",
        "BEGIN",
        "    // En BennuGD1 fade() ajustaba el BRILLO de la paleta: r,g,b van de 0",
        "    // a 200 y **100 es el brillo normal**, o sea que fade(100,100,100,v)",
        "    // significa \"no cambies nada\". BennuGD2 no tiene brillo de paleta:",
        "    // fade() pinta un OVERLAY de color con alpha. Traducirlo como",
        "    // alpha = 255 - media pintaba un velo gris permanente sobre todo.",
        "    medio = (r + g + b) / 3;",
        "    // vel era un incremento por frame sobre 100 pasos; a ~25 fps eso son",
        "    // unos 4000/vel milisegundos.",
        "    ms = BGD1_FADE_MS;",
        "    IF (vel > 0) ms = 4000 / vel; END",
        "    IF (medio >= 100)",
        "        alfa = (medio - 100) * 255 / 100;      // mas claro: velo blanco",
        "        IF (alfa > 255) alfa = 255; END",
        "        RETURN fade(255, 255, 255, alfa, ms);",
        "    END",
        "    alfa = (100 - medio) * 255 / 100;          // mas oscuro: velo negro",
        "    RETURN fade(0, 0, 0, alfa, ms);",
        "END",
    ]),
}

# Duración de los fundidos al portar. BennuGD1 usaba un incremento por frame;
# BennuGD2 quiere milisegundos, así que este valor hay que ajustarlo a ojo.
FADE_MS_DEFAULT = 1000


# Implementación real de la API de paletas de BennuGD1 sobre la de BennuGD2.
# BennuGD2 no tiene objetos-paleta, sólo MAP_GET_PALETTE/MAP_SET_PALETTE por
# gráfico, así que aquí se mantienen las paletas en arrays del propio juego y
# se aplican a los gráficos asignados. El formato de v1 es RGB de 3 bytes; el
# de v2 es SDL_Color de 4, de ahí la conversión.
# Globales que necesita cada shim. Se emiten solo si el shim se usa.
SHIM_GLOBALS = {
    "bgd1_set_title": ["    STRING BGD1_TITULO;   // titulo puesto por el juego"],
    "bgd1_set_mode":  ["    STRING BGD1_TITULO;   // titulo puesto por el juego"],
    "bgd1_set_mode2": ["    STRING BGD1_TITULO;   // titulo puesto por el juego"],
}

PAL_GLOBALS = [
    "    // --- paletas: emulación de la API de BennuGD1 ---",
    "    // BennuGD2 no tiene objetos-paleta: se guardan aquí, en el propio juego.",
    "    BYTE BGD1_PAL[1023][767];    // 1024 paletas x 256 colores x 3 bytes",
    "    INT  BGD1_PAL_USO[1023];",
    "    BYTE BGD1_PAL_TMP[767];      // buffer de lectura de fichero",
    "    BYTE BGD1_CAB[7];            // cabecera de 8 bytes del MAP/FPG/PAL",
    "    BYTE BGD1_SDL[1023];         // buffer SDL_Color: 256 x 4 bytes",
    "    INT  BGD1_ASG_F[1023];       // gráficos con paleta asignada",
    "    INT  BGD1_ASG_G[1023];",
    "    INT  BGD1_ASG_P[1023];",
    "    // shader de intercambio de paleta y sus texturas (256x1 por paleta)",
    "    INT  BGD1_SH = 0;           // el shader, creado una sola vez",
    "    INT  BGD1_TEX[1023];        // grafico 256x1 con cada paleta",
    "    INT  BGD1_PARAMS[1023];     // parametros del shader por paleta",
    "    INT  BGD1_BASE[255];        // paleta original de cada FPG cargado, por id",
]

PAL_HELPERS = [
        "FUNCTION bgd1_pal_libre()",
        "PRIVATE",
        "    INT i;",
        "BEGIN",
        "    FOR (i = 0; i <= 1023; i++)",
        "        IF (BGD1_PAL_USO[i] == 0)",
        "            BGD1_PAL_USO[i] = 1;",
        "            RETURN i + 1;",
        "        END",
        "    END",
        "    RETURN 0;",
        "END",
        "",
        "FUNCTION bgd1_pal_aplica(INT f, INT g, INT p)",
        "PRIVATE",
        "    INT i;",
        "BEGIN",
        "    IF (p < 1) RETURN -1; END",
        "    FOR (i = 0; i <= 255; i++)",
        "        BGD1_SDL[i * 4]     = BGD1_PAL[p - 1][i * 3];",
        "        BGD1_SDL[i * 4 + 1] = BGD1_PAL[p - 1][i * 3 + 1];",
        "        BGD1_SDL[i * 4 + 2] = BGD1_PAL[p - 1][i * 3 + 2];",
        "        BGD1_SDL[i * 4 + 3] = 255;",
        "    END",
        "    RETURN map_set_palette(f, g, 0, 256, &BGD1_SDL[0]);",
        "END",
        "",
        "",
        "// --- intercambio de paleta por shader ---",
        "// BennuGD2 no aplica paletas de 8 bits: los graficos llegan ya en RGBA.",
        "// El shader recibe la paleta ORIGINAL del grafico y la paleta DESTINO,",
        "// las dos como texturas de 256x1, busca el color de entrada en la",
        "// original y saca el de la misma posicion en la destino.",
        "// Los nombres bgd_Vertex/bgd_TexCoord/bgd_Color/bgd_ModelViewProjectionMatrix",
        "// los exige el motor (ver shader_create() en modules/libbggfx/g_shaders.c);",
        "// con los gpu_* por defecto de SDL_gpu compila pero no pinta nada.",
        "FUNCTION bgd1_pal_shader()",
        "PRIVATE",
        "    STRING vs, fs;",
        "BEGIN",
        "    IF (BGD1_SH != 0) RETURN BGD1_SH; END",
        "    vs = \"#version 120\\n\""
        "       + \"attribute vec2 bgd_Vertex;\\nattribute vec2 bgd_TexCoord;\\n\""
        "       + \"attribute vec4 bgd_Color;\\n\""
        "       + \"uniform mat4 bgd_ModelViewProjectionMatrix;\\n\""
        "       + \"varying vec4 color;\\nvarying vec2 texCoord;\\n\""
        "       + \"void main(void) {\\n  color = bgd_Color;\\n\""
        "       + \"  texCoord = vec2(bgd_TexCoord);\\n\""
        "       + \"  gl_Position = bgd_ModelViewProjectionMatrix * vec4(bgd_Vertex, 0.0, 1.0);\\n}\\n\";",
        "    fs = \"#version 120\\n\""
        "       + \"varying vec4 color;\\nvarying vec2 texCoord;\\n\""
        "       + \"uniform sampler2D tex;\\nuniform sampler2D palOrig;\\nuniform sampler2D palNueva;\\n\""
        "       + \"void main(void) {\\n\""
        "       + \"  vec4 c = texture2D(tex, texCoord);\\n\""
        "       + \"  if (c.a < 0.01) { gl_FragColor = c; return; }\\n\""
        "       + \"  float mejor = 1e9; vec3 salida = c.rgb;\\n\""
        "       + \"  for (int i = 0; i < 256; i++) {\\n\""
        "       + \"    vec3 o = texture2D(palOrig, vec2((float(i)+0.5)/256.0, 0.5)).rgb;\\n\""
        "       + \"    vec3 d = o - c.rgb; float dist = dot(d, d);\\n\""
        "       + \"    if (dist < mejor) { mejor = dist;\\n\""
        "       + \"      salida = texture2D(palNueva, vec2((float(i)+0.5)/256.0, 0.5)).rgb; }\\n\""
        "       + \"  }\\n  gl_FragColor = vec4(salida, c.a) * color;\\n}\\n\";",
        "    BGD1_SH = shader_create(vs, fs);",
        "    RETURN BGD1_SH;",
        "END",
        "",
        "FUNCTION bgd1_pal_textura(INT p)",
        "PRIVATE",
        "    INT m, i;",
        "BEGIN",
        "    IF (p < 1 || p > 1023) RETURN 0; END",
        "    IF (BGD1_TEX[p - 1] != 0) RETURN BGD1_TEX[p - 1]; END",
        "    m = map_new(256, 1, 32);",
        "    FOR (i = 0; i <= 255; i++)",
        "        map_put_pixel(0, m, i, 0, rgb(BGD1_PAL[p - 1][i * 3],",
        "                                      BGD1_PAL[p - 1][i * 3 + 1],",
        "                                      BGD1_PAL[p - 1][i * 3 + 2]));",
        "    END",
        "    BGD1_TEX[p - 1] = m;",
        "    RETURN m;",
        "END",
        "",
        "// Devuelve los parametros de shader para dibujar con la paleta p, usando",
        "// como paleta de partida la del propio grafico (base).",
        "FUNCTION bgd1_pal_params(INT base, INT p)",
        "PRIVATE",
        "    INT pr, loc, tb, tn;",
        "BEGIN",
        "    IF (p < 1 || p > 1023) RETURN 0; END",
        "    IF (BGD1_PARAMS[p - 1] != 0) RETURN BGD1_PARAMS[p - 1]; END",
        "    IF (bgd1_pal_shader() == 0) RETURN 0; END",
        "    tb = bgd1_pal_textura(base);",
        "    tn = bgd1_pal_textura(p);",
        "    IF (tb == 0 || tn == 0) RETURN 0; END",
        "    pr = shader_create_params(2);",
        "    loc = shader_get_param_location(BGD1_SH, \"palOrig\");",
        "    shader_set_param(pr, SHADER_IMAGE, loc, 0, tb, 1);",
        "    loc = shader_get_param_location(BGD1_SH, \"palNueva\");",
        "    shader_set_param(pr, SHADER_IMAGE, loc, 0, tn, 2);",
        "    BGD1_PARAMS[p - 1] = pr;",
        "    RETURN pr;",
        "END",
]

PAL_SHIMS = {
    "NEW_PAL": [
        "FUNCTION new_pal()",
        "BEGIN",
        "    RETURN bgd1_pal_libre();",
        "END",
    ],
    "LOAD_PAL": [
        "FUNCTION load_pal(STRING fichero)",
        "PRIVATE",
        "    INT p, f, i;",
        "BEGIN",
        "    // BennuGD2 no tiene objetos-paleta, asi que la paleta se lee del",
        "    // propio fichero. Formato de BennuGD (vale para MAP, FPG y PAL de 8",
        "    // bits): cabecera de 8 bytes y despues 768 bytes RGB de 6 bits.",
        "    // Ver gr_read_pal_with_gamma() en modules/libmod_gfx/file_div.c.",
        "    f = fopen(fichero, O_READ);",
        "    IF (f == 0) RETURN 0; END",
        "    fread(f, BGD1_CAB);",
        "    IF (BGD1_CAB[0] == 102 && BGD1_CAB[1] == 49 && BGD1_CAB[2] == 54)",
        "        fclose(f);",
        "        RETURN 0;                  // \"f16\": 16 bits, no lleva paleta",
        "    END",
        "    IF (BGD1_CAB[3] != 26)",
        "        fseek(f, 0, SEEK_SET);     // paleta cruda de 768 bytes, sin cabecera",
        "    END",
        "    p = bgd1_pal_libre();",
        "    IF (p == 0) fclose(f); RETURN 0; END",
        "    fread(f, BGD1_PAL_TMP);",
        "    fclose(f);",
        "    FOR (i = 0; i <= 767; i++)",
        "        BGD1_PAL[p - 1][i] = BGD1_PAL_TMP[i] * 4;   // 6 bits -> 8 bits",
        "    END",
        "    RETURN p;",
        "END",
    ],
    "UNLOAD_PAL": [
        "FUNCTION unload_pal(INT p)",
        "BEGIN",
        "    IF (p >= 1 && p <= 1024) BGD1_PAL_USO[p - 1] = 0; END",
        "    RETURN 0;",
        "END",
    ],
    "PAL_GET": [
        "FUNCTION pal_get(INT p, INT primero, INT n, BYTE * buf)",
        "PRIVATE",
        "    INT i;",
        "BEGIN",
        "    IF (p < 1) RETURN -1; END",
        "    FOR (i = 0; i < n; i++)",
        "        buf[i * 3]     = BGD1_PAL[p - 1][(primero + i) * 3];",
        "        buf[i * 3 + 1] = BGD1_PAL[p - 1][(primero + i) * 3 + 1];",
        "        buf[i * 3 + 2] = BGD1_PAL[p - 1][(primero + i) * 3 + 2];",
        "    END",
        "    RETURN 0;",
        "END",
    ],
    "PAL_SET": [
        "FUNCTION pal_set(INT p, INT primero, INT n, BYTE * buf)",
        "PRIVATE",
        "    INT i;",
        "BEGIN",
        "    IF (p < 1) RETURN -1; END",
        "    FOR (i = 0; i < n; i++)",
        "        BGD1_PAL[p - 1][(primero + i) * 3]     = buf[i * 3];",
        "        BGD1_PAL[p - 1][(primero + i) * 3 + 1] = buf[i * 3 + 1];",
        "        BGD1_PAL[p - 1][(primero + i) * 3 + 2] = buf[i * 3 + 2];",
        "    END",
        "    RETURN 0;",
        "END",
    ],
    "PAL_CLONE": [
        "FUNCTION pal_clone(INT p)",
        "PRIVATE",
        "    INT n, i;",
        "BEGIN",
        "    IF (p < 1) RETURN 0; END",
        "    n = bgd1_pal_libre();",
        "    IF (n == 0) RETURN 0; END",
        "    FOR (i = 0; i <= 767; i++)",
        "        BGD1_PAL[n - 1][i] = BGD1_PAL[p - 1][i];",
        "    END",
        "    RETURN n;",
        "END",
    ],
    "PAL_MAP_ASSIGN": [
        "FUNCTION pal_map_assign(INT f, INT g, INT p)",
        "PRIVATE",
        "    INT i;",
        "BEGIN",
        "    FOR (i = 0; i <= 1023; i++)",
        "        IF (BGD1_ASG_F[i] == f && BGD1_ASG_G[i] == g)",
        "            BGD1_ASG_P[i] = p;",
        "            RETURN bgd1_pal_aplica(f, g, p);",
        "        END",
        "    END",
        "    FOR (i = 0; i <= 1023; i++)",
        "        IF (BGD1_ASG_P[i] == 0)",
        "            BGD1_ASG_F[i] = f;",
        "            BGD1_ASG_G[i] = g;",
        "            BGD1_ASG_P[i] = p;",
        "            RETURN bgd1_pal_aplica(f, g, p);",
        "        END",
        "    END",
        "    RETURN -1;",
        "END",
    ],
    "PAL_MAP_REMOVE": [
        "FUNCTION pal_map_remove(INT f, INT g)",
        "PRIVATE",
        "    INT i;",
        "BEGIN",
        "    FOR (i = 0; i <= 1023; i++)",
        "        IF (BGD1_ASG_F[i] == f && BGD1_ASG_G[i] == g)",
        "            BGD1_ASG_P[i] = 0;",
        "        END",
        "    END",
        "    RETURN 0;",
        "END",
    ],
    "PAL_REFRESH": [
        "FUNCTION pal_refresh(INT p)",
        "PRIVATE",
        "    INT i;",
        "BEGIN",
        "    FOR (i = 0; i <= 1023; i++)",
        "        IF (BGD1_ASG_P[i] == p && p > 0)",
        "            bgd1_pal_aplica(BGD1_ASG_F[i], BGD1_ASG_G[i], p);",
        "        END",
        "    END",
        "    RETURN 0;",
        "END",
    ],
}

DOMAIN = {"SONG": "MUSIC", "WAV": "SOUND", "TEXT": "WRITE", "WINDOW": "WINDOW"}
_PREFIX_VERBS = ("LOAD", "UNLOAD", "NEW", "SAVE", "PLAY", "STOP", "DELETE",
                 "SET", "GET", "MOVE", "OUT")


class Porter:
    def __init__(self, tab1=None, tab2=None):
        self.v1 = self._load(tab1 or os.path.join(HERE, "symtab_bgd1.json"))
        self.v2 = self._load(tab2 or os.path.join(HERE, "symtab_bgd2.json"))
        self.f2 = self.v2.get("functions", {})
        self._cache = {}
        self.unresolved = {}      # nombre -> motivo
        self.stub_vars = {}       # variables sin equivalente que hay que declarar
        self.shims_used = {}      # shims de compatibilidad realmente necesarios
        self.stub_funcs = {}      # funciones sin equivalente: nombre -> nº args
        self.renamed = {}         # v1 -> v2
        self.modules_needed = set()
        self.pal_usado = False    # el juego usa la API de paletas de BennuGD1
        self.map_depth = 16       # profundidad del programa original (v1: 16 por defecto)

    @staticmethod
    def _load(path):
        try:
            with open(path) as fh:
                return json.load(fh)
        except OSError:
            return {}

    # ---------- funciones ----------
    def _candidates(self, name):
        t = name.split("_")
        out = []
        if name in CURATED:
            out.append(CURATED[name])
        if len(t) > 1:
            out.append("_".join(reversed(t)))
            d = [DOMAIN.get(x, x) for x in t]
            out += ["_".join(d), "_".join(reversed(d))]
            if t[0] in _PREFIX_VERBS:
                out.append("_".join(DOMAIN.get(x, x) for x in t[1:] + [t[0]]))
        return list(dict.fromkeys(out))

    def map_func(self, name, nargs=None):
        """Devuelve el nombre en BennuGD2, o el original si no hay equivalente."""
        key = (name.upper(), nargs)
        if key in self._cache:
            return self._cache[key]
        up = name.upper()
        # firma o semántica distinta: redirigir al shim de compatibilidad
        # la API de paletas de BennuGD1 se emula en el propio .prg: no existe en
        # BennuGD2 y devolver 0 (como un stub) cambiaría las ramas que toma el juego
        if up in PAL_SHIMS:
            self.shims_used[up.lower()] = PAL_SHIMS[up]
            self.pal_usado = True
            self._cache[key] = up.lower()
            self.modules_needed.add("libmod_misc")
            self.modules_needed.add("libmod_gfx")
            return up.lower()
        sh = SHIMS.get((up, nargs))
        if sh:
            self.shims_used[sh[0]] = sh[1]
            self._cache[key] = sh[0]
            self.modules_needed.add("libmod_gfx")
            return sh[0]
        res = up
        if up in self.f2:
            pass                                   # el nombre ya vale en BennuGD2
        else:
            # 1ª pasada: candidato cuya aridad encaja exactamente
            # 2ª pasada: candidato que existe aunque la firma no cuadre
            #            (p.ej. firmas variádicas tipo "IIIIV++"), avisando
            exact = fallback = None
            for c in self._candidates(up):
                sigs = self.f2.get(c)
                if not sigs:
                    continue
                if fallback is None:
                    fallback = c
                if nargs is None or any(len(s["params"]) == nargs for s in sigs):
                    exact = c
                    break
            if exact:
                res = exact
                self.renamed[up] = exact
            elif fallback:
                res = fallback
                self.renamed[up] = fallback
                self.unresolved[up] = ("renombrado a %s, pero la firma no coincide: "
                                       "revisar argumentos" % fallback)
            else:
                self.unresolved[up] = NO_EQUIV_NOTE.get(
                    up, "sin equivalente directo en BennuGD2")
                self.stub_funcs[up] = nargs or 0
        if res in self.f2:
            self.modules_needed.add(self.f2[res][0]["module"])
        self._cache[key] = res
        return res

    # ---------- variables ----------
    def map_global(self, name):
        up = name.upper()
        if up in GLOBAL_MAP:
            tgt = GLOBAL_MAP[up]
            if tgt is None:
                self.unresolved[up] = NO_EQUIV_NOTE.get(up, "global sin equivalente")
                return name
            self.renamed[up] = tgt
            return tgt
        g2 = {k.upper() for k in self.v2.get("globals", {})}
        g1 = {k.upper() for k in self.v1.get("globals", {})}
        if up in g2:
            return name
        if up in g1:
            # muchas globales sueltas de v1 son ahora campos de un struct de v2:
            # el nombre del campo se busca en la tabla, no se adivina
            f = self.v2.get("global_fields", {}).get(up)
            if f:
                self.renamed[up] = f
                return f
            self.unresolved[up] = NO_EQUIV_NOTE.get(up, "global del motor eliminada")
            self.stub_vars[up] = self.unresolved[up]
        return name

    def map_local(self, name):
        up = name.upper()
        l1 = {k.upper() for k in self.v1.get("locals", {})}
        l2 = {k.upper() for k in self.v2.get("locals", {})}
        if up in l2:
            return name
        if up in l1:
            f = self.v2.get("local_fields", {}).get(up)
            if f:
                self.renamed[up] = f
                return f
            self.unresolved[up] = "local del proceso eliminada en BennuGD2"
            self.stub_vars[up] = self.unresolved[up]
        return name

    # ---------- salida ----------
    def prelude(self):
        """Shims y stubs de compatibilidad que el fuente portado necesita."""
        if not self.shims_used and not self.stub_funcs:
            return []
        out = ["// --- compatibilidad BennuGD1 -> BennuGD2 ---",
               "#define BGD1_FADE_MS %d" % FADE_MS_DEFAULT,
               "// profundidad de color del programa original: de ella depende el",
               "// valor que devolvia map_get_pixel() (16 bits -> RGB565)",
               "#define BGD1_MAP_DEPTH %d" % self.map_depth, ""]
        if self.pal_usado:
            out += PAL_HELPERS + [""]
        for _nm, body in sorted(self.shims_used.items()):
            out += body + [""]
        out += self.func_stubs()
        return out

    def func_stubs(self):
        """Stubs vacíos de las funciones que BennuGD2 ya no tiene.

        Sin ellos el fuente ni siquiera compila. Devuelven 0 y llevan su TODO:
        el comportamiento hay que rehacerlo, no está portado.
        """
        out = []
        for nm, n in sorted(self.stub_funcs.items()):
            args = ", ".join("INT p%d" % i for i in range(n))
            out += ["// TODO portar: %s" % self.unresolved.get(nm, ""),
                    "FUNCTION %s(%s)" % (nm.lower(), args),
                    "BEGIN",
                    "    RETURN 0;",
                    "END",
                    ""]
        return out

    def pal_globals(self):
        """Globales de apoyo: de las paletas y de los shims, sólo si se usan."""
        out = list(PAL_GLOBALS) if self.pal_usado else []
        vistas = set()
        for nm in sorted(self.shims_used):
            for linea in SHIM_GLOBALS.get(nm, []):
                if linea not in vistas:
                    vistas.add(linea)
                    out.append(linea)
        return out

    def missing_locals(self, nombres):
        """De las locales del motor de BennuGD1, las que BennuGD2 no aporta.

        Son las que hay que declarar en un bloque LOCAL del propio programa: si
        se declaran como GLOBAL, todas las instancias comparten el valor y la
        logica del juego se rompe (en Streets of Rage, PALETTE como global hacia
        que todos los enemigos compartiesen paleta y fallasen las comparaciones
        del tipo "IF (PALETTE == PAL_SOMBRA)").
        """
        hay = {k.upper() for k in self.v2.get("locals", {})}
        return [n for n in nombres if n.upper() not in hay]

    def stub_decls(self):
        """Declaraciones de relleno para los símbolos del motor que ya no existen.

        Permiten que el fuente portado compile; cada una lleva su TODO para que
        quede claro que ese comportamiento hay que rehacerlo, no que ya funcione.
        """
        out = []
        for nm, why in sorted(self.stub_vars.items()):
            out.append("    // TODO portar: %s" % why)
            out.append("    INT %s;" % nm)
        return out

    def int_locals(self):
        """Locales que eran enteras en BennuGD1 y son reales en BennuGD2.

        Importa porque el código de v1 asume truncación: patrones como
        "y = pos_float; ... IF (y == 780)" sólo funcionan si la asignación
        trunca. Con y en DOUBLE la igualdad no se cumple nunca y el juego se
        queda colgado esperando algo que no llega.
        """
        t1 = self.v1.get("local_types", {})
        t2 = self.v2.get("local_types", {})
        return {k.upper() for k in self.v1.get("locals", {})
                if t2.get(k.upper()) in ("DOUBLE", "FLOAT")
                and t1.get(k.upper(), "INT") not in ("DOUBLE", "FLOAT")}

    def param_types(self, name, nargs):
        """ParamTypes de BennuGD2 para la función ya renombrada.

        Hace falta porque al renombrar puede cambiar la clase del parámetro:
        WRITE_INT recibía 'P' (dirección) y WRITE_VAR recibe 'V' (variable),
        así que el "&" corresponde en un caso y estorba en el otro.
        """
        sigs = self.f2.get(name.upper())
        if not sigs:
            return None
        for s in sigs:
            if len(s["params"].replace("+", "")) == nargs:
                return s["params"]
        return sigs[0]["params"]

    def local_type(self, name):
        """Tipo BennuGD2 de una local reservada, o None si no lo es.

        Hace falta porque BennuGD1 las tenía todas enteras y BennuGD2 pasó
        varias a DOUBLE (x, y, z, size). Un parámetro sin tipo que sea una
        local reservada obliga a bgdc a inferir, y si el proceso se llamó antes
        de declararse el tipo ya quedó fijado y choca (c_main.c:918).
        """
        return self.v2.get("local_types", {}).get(name.upper())

    def engine_globals(self):
        """Globales que declara el propio motor: el juego no debe redeclararlas."""
        return ({k.upper() for k in self.v1.get("globals", {})} |
                {k.upper() for k in self.v2.get("globals", {})})

    def imports(self):
        """Módulos de BennuGD2 que hay que importar, según lo realmente usado."""
        mods = {m for m in self.modules_needed if m.startswith("libmod_")}
        return sorted(mods)

    def report(self):
        lines = []
        if self.renamed:
            lines.append("// %d funciones renombradas automáticamente "
                         "(verificadas contra BennuGD2):" % len(self.renamed))
            for a, b in sorted(self.renamed.items()):
                lines.append("//     %-22s -> %s" % (a, b))
        if self.unresolved:
            lines.append("//")
            lines.append("// %d símbolos SIN equivalente: revisar a mano." %
                         len(self.unresolved))
            for a, why in sorted(self.unresolved.items()):
                lines.append("//     %-22s %s" % (a, why))
        return lines


# ---------------------------------------------------------------------------
# Portar DESDE Fenix
# ---------------------------------------------------------------------------
# Fenix lo lleva todo dentro del interprete: no tiene modulos ni "import". Los
# .dcb de Fenix, por eso, traen NImports = 0. BennuGD reparte esas mismas
# funciones en modulos, asi que al portar hay que averiguar cuales hacen falta y
# escribir sus "import" -- sin ellos no compila nada.

def _tabla(nombre):
    import json, os
    ruta = os.path.join(os.path.dirname(os.path.abspath(__file__)), nombre)
    return json.load(open(ruta)) if os.path.exists(ruta) else {}


def portabilidad(usadas, globales, locales, destino, firmas=None):
    """Que hace falta para llevar un programa de Fenix a BennuGD1 o BennuGD2.

    Devuelve (modulos, funciones_sin_equivalente, globales_sin, locales_sin,
    firmas_distintas). `usadas` son los nombres de funcion del sistema que el
    programa llama; el resto, las variables del motor que toca.

    `firmas` es {nombre: nº de parametros} en el ORIGEN. Que una funcion exista
    con el mismo nombre no basta: "put" de Fenix lleva 4 parametros y el
    "map_put" de BennuGD2 lleva mas, asi que la llamada portada no compila. Eso
    sale en `firmas_distintas`, que es trabajo a mano.
    """
    tab = _tabla("symtab_bgd1.json" if destino == "BennuGD1" else "symtab_bgd2.json")
    fn, gl, lo = tab.get("functions", {}), tab.get("globals", {}), tab.get("locals", {})
    mods, faltan, otras = set(), [], []
    firmas = firmas or {}
    for f in sorted(usadas):
        ent = fn.get(f.upper())
        if not ent:
            faltan.append(f)
            continue
        n = firmas.get(f)
        if n is not None:
            # los '+' son argumentos que genera el compilador, no se escriben
            cuenta = {len(e.get("params", "").replace("+", "")) for e in ent}
            if cuenta and n not in cuenta:
                otras.append("%s (%d -> %s)"
                             % (f, n, "/".join(str(c) for c in sorted(cuenta))))
        # una funcion puede estar en varios modulos; vale cualquiera, se coge
        # el primero que sea un modulo importable
        for e in ent:
            m = e.get("module", "")
            if m.startswith(("mod_", "libmod_")):
                mods.add(m)
                break
    # Las VARIABLES del motor tambien vienen de un modulo: "timer" lo declara
    # mod_timers, y sin su import no compila aunque no se llame a ninguna de sus
    # funciones. Las que vienen de una lib interna (libkey, librender...) se
    # arrastran solas y no se importan.
    sin_g, sin_l = [], []
    for nombre, tabla, sin in ((globales, gl, sin_g), (locales, lo, sin_l)):
        for v in sorted(nombre):
            m = tabla.get(v.upper())
            if m is None:
                sin.append(v)
            elif str(m).startswith(("mod_", "libmod_")):
                mods.add(m)
    return sorted(mods), faltan, sin_g, sin_l, otras
