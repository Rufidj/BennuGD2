/*
 * Herramienta para crear mapas .raymap desde archivos de texto
 * Compila con: gcc -o map_builder map_builder.c
 * Uso: ./map_builder config.txt output.raymap
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE 1024
#define MAX_SPRITES 1000

typedef struct {
    char magic[8];
    uint32_t version;
    uint32_t map_width;
    uint32_t map_height;
    uint32_t num_levels;
    uint32_t num_sprites;
    uint32_t num_thin_walls;
    uint32_t num_thick_walls;
} RAY_MapHeader;

typedef struct {
    float x, y, z;
    int texture_id;
    int w, h;
    int level;
    float rot;
} SpriteData;

/* Eliminar espacios al inicio y final */
char* trim(char *str) {
    char *end;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

/* Obtener directorio del archivo de configuración */
void get_config_dir(const char *config_path, char *dir_out, size_t size) {
    const char *last_slash = strrchr(config_path, '/');
    if (last_slash) {
        size_t len = last_slash - config_path + 1;
        if (len >= size) len = size - 1;
        strncpy(dir_out, config_path, len);
        dir_out[len] = '\0';
    } else {
        dir_out[0] = '\0';
    }
}

/* Resolver ruta relativa al directorio del config */
void resolve_path(const char *config_dir, const char *relative_path, char *out, size_t size) {
    if (relative_path[0] == '/') {
        /* Ruta absoluta */
        strncpy(out, relative_path, size - 1);
        out[size - 1] = '\0';
    } else {
        /* Ruta relativa */
        snprintf(out, size, "%s%s", config_dir, relative_path);
    }
}

/* Leer un grid desde archivo */
int read_grid(const char *filename, int **grid, int *width, int *height) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Error: No se puede abrir %s\n", filename);
        return 0;
    }
    
    char line[MAX_LINE];
    int rows = 0;
    int cols = 0;
    
    /* Primera pasada: contar filas y columnas */
    while (fgets(line, sizeof(line), f)) {
        char *trimmed = trim(line);
        if (strlen(trimmed) == 0 || trimmed[0] == '#') continue;
        
        if (rows == 0) {
            /* Contar columnas en primera fila */
            char *token = strtok(trimmed, ",");
            while (token) {
                cols++;
                token = strtok(NULL, ",");
            }
        }
        rows++;
    }
    
    if (rows == 0 || cols == 0) {
        fprintf(stderr, "Error: Grid vacío en %s\n", filename);
        fclose(f);
        return 0;
    }
    
    *width = cols;
    *height = rows;
    *grid = (int*)malloc(rows * cols * sizeof(int));
    
    /* Segunda pasada: leer datos */
    rewind(f);
    int row = 0;
    while (fgets(line, sizeof(line), f) && row < rows) {
        char *trimmed = trim(line);
        if (strlen(trimmed) == 0 || trimmed[0] == '#') continue;
        
        int col = 0;
        char *token = strtok(trimmed, ",");
        while (token && col < cols) {
            (*grid)[row * cols + col] = atoi(trim(token));
            col++;
            token = strtok(NULL, ",");
        }
        row++;
    }
    
    fclose(f);
    printf("Grid cargado: %s (%dx%d)\n", filename, cols, rows);
    return 1;
}

/* Leer múltiples grids desde un archivo con secciones por nivel */
/* Formato esperado:
   # Nivel 0
   1,2,3,...
   ...
   # Nivel 1
   4,5,6,...
   ...
*/
int read_multi_level_grid(const char *filename, int ***grids_out, int *width, int *height, int num_levels) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Error: No se puede abrir %s\n", filename);
        return 0;
    }
    
    char line[MAX_LINE];
    int current_level = -1;
    int rows_per_level = 0;
    int cols = 0;
    
    /* Primera pasada: determinar dimensiones */
    while (fgets(line, sizeof(line), f)) {
        char *trimmed = trim(line);
        if (strlen(trimmed) == 0) continue;
        
        /* Detectar comentario de nivel - debe ser "# Nivel X" */
        if (trimmed[0] == '#') {
            /* Buscar patrón "Nivel" seguido de un número */
            char *nivel_pos = strstr(trimmed, "Nivel");
            if (!nivel_pos) nivel_pos = strstr(trimmed, "nivel");
            if (!nivel_pos) nivel_pos = strstr(trimmed, "NIVEL");
            
            if (nivel_pos) {
                /* Verificar que después de "Nivel" hay un espacio y un número */
                char *after = nivel_pos + 5; /* Saltar "Nivel" */
                while (*after == ' ') after++;
                if (isdigit(*after)) {
                    current_level++;
                }
            }
            continue;
        }
        
        if (current_level < 0) continue; /* Ignorar datos antes del primer nivel */
        
        /* Contar columnas en primera fila del primer nivel */
        if (current_level == 0 && cols == 0) {
            char temp[MAX_LINE];
            strcpy(temp, trimmed);
            char *token = strtok(temp, ",");
            while (token) {
                cols++;
                token = strtok(NULL, ",");
            }
        }
        
        if (current_level == 0) {
            rows_per_level++;
        }
    }
    
    if (rows_per_level == 0 || cols == 0) {
        fprintf(stderr, "Error: No se encontraron datos válidos en %s\n", filename);
        fclose(f);
        return 0;
    }
    
    *width = cols;
    *height = rows_per_level;
    
    /* Allocar arrays */
    *grids_out = (int**)malloc(num_levels * sizeof(int*));
    for (int i = 0; i < num_levels; i++) {
        (*grids_out)[i] = (int*)calloc(rows_per_level * cols, sizeof(int));
    }
    
    /* Segunda pasada: leer datos */
    rewind(f);
    current_level = -1;
    int current_row = 0;
    
    while (fgets(line, sizeof(line), f)) {
        char *trimmed = trim(line);
        if (strlen(trimmed) == 0) continue;
        
        /* Detectar comentario de nivel */
        if (trimmed[0] == '#') {
            if (strstr(trimmed, "Nivel") || strstr(trimmed, "nivel") || strstr(trimmed, "NIVEL")) {
                current_level++;
                current_row = 0;
            }
            continue;
        }
        
        if (current_level < 0 || current_level >= num_levels) continue;
        if (current_row >= rows_per_level) continue;
        
        /* Leer fila */
        int col = 0;
        char *token = strtok(trimmed, ",");
        while (token && col < cols) {
            (*grids_out)[current_level][current_row * cols + col] = atoi(trim(token));
            col++;
            token = strtok(NULL, ",");
        }
        current_row++;
    }
    
    fclose(f);
    printf("Multi-level grid cargado: %s (%dx%d x %d niveles)\n", filename, cols, rows_per_level, num_levels);
    return 1;
}

/* Leer sprites desde archivo */
int read_sprites(const char *filename, SpriteData *sprites, int *count) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Advertencia: No se puede abrir %s\n", filename);
        *count = 0;
        return 1; /* No es error crítico */
    }
    
    char line[MAX_LINE];
    *count = 0;
    
    while (fgets(line, sizeof(line), f) && *count < MAX_SPRITES) {
        char *trimmed = trim(line);
        if (strlen(trimmed) == 0 || trimmed[0] == '#') continue;
        
        /* Formato: x,y,z,texture_id,w,h,level,rot */
        float x, y, z, rot;
        int tex_id, w, h, level;
        
        if (sscanf(trimmed, "%f,%f,%f,%d,%d,%d,%d,%f",
                   &x, &y, &z, &tex_id, &w, &h, &level, &rot) == 8) {
            sprites[*count].x = x;
            sprites[*count].y = y;
            sprites[*count].z = z;
            sprites[*count].texture_id = tex_id;
            sprites[*count].w = w;
            sprites[*count].h = h;
            sprites[*count].level = level;
            sprites[*count].rot = rot;
            (*count)++;
        }
    }
    
    fclose(f);
    printf("Sprites cargados: %d\n", *count);
    return 1;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Uso: %s <config.txt> <output.raymap>\n", argv[0]);
        printf("\nFormato del archivo de configuración:\n");
        printf("  grid0=nivel0.txt\n");
        printf("  grid1=nivel1.txt\n");
        printf("  grid2=nivel2.txt\n");
        printf("  floor=floor.txt\n");
        printf("  ceiling=ceiling.txt\n");
        printf("  sprites=sprites.txt\n");
        return 1;
    }
    
    const char *config_file = argv[1];
    const char *output_file = argv[2];
    
    /* Obtener directorio del archivo de configuración */
    char config_dir[512];
    get_config_dir(config_file, config_dir, sizeof(config_dir));
    printf("Directorio de configuración: %s\n", strlen(config_dir) > 0 ? config_dir : "(actual)");
    
    /* Leer archivo de configuración */
    FILE *cfg = fopen(config_file, "r");
    if (!cfg) {
        fprintf(stderr, "Error: No se puede abrir %s\n", config_file);
        return 1;
    }
    
    char grid0_file[256] = "";
    char grid1_file[256] = "";
    char grid2_file[256] = "";
    char floor_file[256] = "";
    char ceiling_file[256] = "";
    char sprites_file[256] = "";
    
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), cfg)) {
        char *trimmed = trim(line);
        if (strlen(trimmed) == 0 || trimmed[0] == '#') continue;
        
        char *eq = strchr(trimmed, '=');
        if (!eq) continue;
        
        *eq = '\0';
        char *key = trim(trimmed);
        char *value = trim(eq + 1);
        
        /* Resolver rutas relativas al directorio del config */
        char resolved[512];
        resolve_path(config_dir, value, resolved, sizeof(resolved));
        
        if (strcmp(key, "grid0") == 0) strcpy(grid0_file, resolved);
        else if (strcmp(key, "grid1") == 0) strcpy(grid1_file, resolved);
        else if (strcmp(key, "grid2") == 0) strcpy(grid2_file, resolved);
        else if (strcmp(key, "floor") == 0) strcpy(floor_file, resolved);
        else if (strcmp(key, "ceiling") == 0) strcpy(ceiling_file, resolved);
        else if (strcmp(key, "sprites") == 0) strcpy(sprites_file, resolved);
    }
    fclose(cfg);
    
    /* Validar archivos requeridos */
    if (strlen(grid0_file) == 0) {
        fprintf(stderr, "Error: Falta grid0 en configuración\n");
        return 1;
    }
    
    /* Leer grids */
    int *grid0 = NULL, *grid1 = NULL, *grid2 = NULL;
    int width = 0, height = 0;
    
    if (!read_grid(grid0_file, &grid0, &width, &height)) {
        return 1;
    }
    
    /* Grid1 (opcional) */
    if (strlen(grid1_file) > 0) {
        int w, h;
        if (!read_grid(grid1_file, &grid1, &w, &h)) {
            free(grid0);
            return 1;
        }
        if (w != width || h != height) {
            fprintf(stderr, "Error: grid1 tiene dimensiones diferentes\n");
            free(grid0);
            free(grid1);
            return 1;
        }
    } else {
        grid1 = (int*)calloc(width * height, sizeof(int));
    }
    
    /* Grid2 (opcional) */
    if (strlen(grid2_file) > 0) {
        int w, h;
        if (!read_grid(grid2_file, &grid2, &w, &h)) {
            free(grid0);
            free(grid1);
            return 1;
        }
        if (w != width || h != height) {
            fprintf(stderr, "Error: grid2 tiene dimensiones diferentes\n");
            free(grid0);
            free(grid1);
            free(grid2);
            return 1;
        }
    } else {
        grid2 = (int*)calloc(width * height, sizeof(int));
    }
    
    /* Floor grids (multi-nivel) */
    int **floor_grids = NULL;
    if (strlen(floor_file) > 0) {
        int w, h;
        if (!read_multi_level_grid(floor_file, &floor_grids, &w, &h, 3)) {
            free(grid0);
            free(grid1);
            free(grid2);
            return 1;
        }
        if (w != width || h != height) {
            fprintf(stderr, "Error: floor tiene dimensiones diferentes\n");
            free(grid0);
            free(grid1);
            free(grid2);
            for (int i = 0; i < 3; i++) free(floor_grids[i]);
            free(floor_grids);
            return 1;
        }
    } else {
        floor_grids = (int**)malloc(3 * sizeof(int*));
        for (int i = 0; i < 3; i++) {
            floor_grids[i] = (int*)calloc(width * height, sizeof(int));
        }
    }
    
    /* Ceiling grids (multi-nivel) */
    int **ceiling_grids = NULL;
    if (strlen(ceiling_file) > 0) {
        int w, h;
        if (!read_multi_level_grid(ceiling_file, &ceiling_grids, &w, &h, 3)) {
            free(grid0);
            free(grid1);
            free(grid2);
            for (int i = 0; i < 3; i++) free(floor_grids[i]);
            free(floor_grids);
            return 1;
        }
        if (w != width || h != height) {
            fprintf(stderr, "Error: ceiling tiene dimensiones diferentes\n");
            free(grid0);
            free(grid1);
            free(grid2);
            for (int i = 0; i < 3; i++) free(floor_grids[i]);
            free(floor_grids);
            for (int i = 0; i < 3; i++) free(ceiling_grids[i]);
            free(ceiling_grids);
            return 1;
        }
    } else {
        ceiling_grids = (int**)malloc(3 * sizeof(int*));
        for (int i = 0; i < 3; i++) {
            ceiling_grids[i] = (int*)calloc(width * height, sizeof(int));
        }
    }
    
    /* Leer sprites (opcional) */
    SpriteData sprites[MAX_SPRITES];
    int num_sprites = 0;
    if (strlen(sprites_file) > 0) {
        read_sprites(sprites_file, sprites, &num_sprites);
    }
    
    /* Escribir archivo .raymap */
    /* Eliminar archivo anterior si existe para evitar datos corruptos */
    remove(output_file);
    
    FILE *out = fopen(output_file, "wb");
    if (!out) {
        fprintf(stderr, "Error: No se puede crear %s\n", output_file);
        free(grid0);
        free(grid1);
        free(grid2);
        for (int i = 0; i < 3; i++) {
            free(floor_grids[i]);
            free(ceiling_grids[i]);
        }
        free(floor_grids);
        free(ceiling_grids);
        return 1;
    }
    
    /* Header */
    RAY_MapHeader header;
    memcpy(header.magic, "RAYMAP\x1a", 7);
    header.magic[7] = 0;
    header.version = 1;
    header.map_width = width;
    header.map_height = height;
    header.num_levels = 3;
    header.num_sprites = num_sprites;
    header.num_thin_walls = 0;
    header.num_thick_walls = 0;
    
    fwrite(&header, sizeof(RAY_MapHeader), 1, out);
    
    /* Grids */
    fwrite(grid0, sizeof(int), width * height, out);
    fwrite(grid1, sizeof(int), width * height, out);
    fwrite(grid2, sizeof(int), width * height, out);
    
    /* Sprites */
    for (int i = 0; i < num_sprites; i++) {
        fwrite(&sprites[i].texture_id, sizeof(int), 1, out);
        fwrite(&sprites[i].x, sizeof(float), 1, out);
        fwrite(&sprites[i].y, sizeof(float), 1, out);
        fwrite(&sprites[i].z, sizeof(float), 1, out);
        fwrite(&sprites[i].w, sizeof(int), 1, out);
        fwrite(&sprites[i].h, sizeof(int), 1, out);
        fwrite(&sprites[i].level, sizeof(int), 1, out);
        fwrite(&sprites[i].rot, sizeof(float), 1, out);
    }
    
    /* Floor y ceiling - 3 grids de cada uno */
    for (int level = 0; level < 3; level++) {
        fwrite(floor_grids[level], sizeof(int), width * height, out);
    }
    for (int level = 0; level < 3; level++) {
        fwrite(ceiling_grids[level], sizeof(int), width * height, out);
    }
    
    fclose(out);
    
    /* Limpiar */
    free(grid0);
    free(grid1);
    free(grid2);
    for (int i = 0; i < 3; i++) {
        free(floor_grids[i]);
        free(ceiling_grids[i]);
    }
    free(floor_grids);
    free(ceiling_grids);
    
    printf("\n=== Mapa creado exitosamente ===\n");
    printf("Archivo: %s\n", output_file);
    printf("Dimensiones: %dx%d\n", width, height);
    printf("Niveles: 3\n");
    printf("Sprites: %d\n", num_sprites);
    
    return 0;
}
