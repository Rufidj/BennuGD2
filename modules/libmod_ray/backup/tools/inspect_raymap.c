#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    char magic[8];
    uint32_t version;
    uint32_t map_width;
    uint32_t map_height;
    uint32_t num_levels;
    uint32_t num_sprites;
    uint32_t num_thin_walls;
    uint32_t num_thick_walls;
    float camera_x;
    float camera_y;
    float camera_z;
    float camera_rot;
    float camera_pitch;
    int32_t skyTextureID;
} RAY_MapHeader;

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Uso: %s <archivo.raymap>\n", argv[0]);
        return 1;
    }
    
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        printf("Error: No se puede abrir %s\n", argv[1]);
        return 1;
    }
    
    RAY_MapHeader header;
    memset(&header, 0, sizeof(RAY_MapHeader));
    
    fread(header.magic, 1, 8, f);
    fread(&header.version, sizeof(uint32_t), 1, f);
    fread(&header.map_width, sizeof(uint32_t), 1, f);
    fread(&header.map_height, sizeof(uint32_t), 1, f);
    fread(&header.num_levels, sizeof(uint32_t), 1, f);
    fread(&header.num_sprites, sizeof(uint32_t), 1, f);
    fread(&header.num_thin_walls, sizeof(uint32_t), 1, f);
    fread(&header.num_thick_walls, sizeof(uint32_t), 1, f);
    
    if (header.version == 2) {
        fread(&header.camera_x, sizeof(float), 1, f);
        fread(&header.camera_y, sizeof(float), 1, f);
        fread(&header.camera_z, sizeof(float), 1, f);
        fread(&header.camera_rot, sizeof(float), 1, f);
        fread(&header.camera_pitch, sizeof(float), 1, f);
        fread(&header.skyTextureID, sizeof(int32_t), 1, f);
    }
    
    printf("=== HEADER ===\n");
    printf("Magic: %.7s\n", header.magic);
    printf("Version: %u\n", header.version);
    printf("Dimensiones: %u x %u\n", header.map_width, header.map_height);
    printf("Niveles: %u\n", header.num_levels);
    printf("Sprites: %u\n", header.num_sprites);
    printf("Thin walls: %u\n", header.num_thin_walls);
    printf("Thick walls: %u\n", header.num_thick_walls);
    
    if (header.version == 2) {
        printf("Cámara: (%.1f, %.1f, %.1f)\n", header.camera_x, header.camera_y, header.camera_z);
        printf("Skybox ID: %d\n", header.skyTextureID);
    }
    
    /* Leer grid nivel 0 */
    int grid_size = header.map_width * header.map_height;
    int *grid = (int*)malloc(grid_size * sizeof(int));
    
    /* DEBUG: Mostrar posición del archivo antes de leer grid */
    long pos_before = ftell(f);
    printf("\nDEBUG: Posición del archivo antes de leer grid: %ld bytes\n", pos_before);
    printf("DEBUG: Tamaño esperado del header: %zu bytes\n", 
           8 + 8*sizeof(uint32_t) + (header.version == 2 ? 5*sizeof(float) + sizeof(int32_t) : 0));
    
    if (fread(grid, sizeof(int), grid_size, f) != grid_size) {
        printf("Error leyendo grid\n");
        free(grid);
        fclose(f);
        return 1;
    }
    
    /* DEBUG: Mostrar primeros 20 valores leídos */
    printf("\nDEBUG: Primeros 20 valores leídos del grid:\n");
    for (int i = 0; i < 20 && i < grid_size; i++) {
        printf("  grid[%d] = %d\n", i, grid[i]);
    }
    
    printf("\n=== GRID NIVEL 0 ===\n");
    
    /* Contar celdas no vacías */
    int non_zero = 0;
    for (int i = 0; i < grid_size; i++) {
        if (grid[i] != 0) non_zero++;
    }
    printf("Celdas con datos: %d / %d\n", non_zero, grid_size);
    
    /* Imprimir bordes */
    printf("\nPrimera fila (y=0):\n");
    for (int x = 0; x < header.map_width; x++) {
        printf("%4d ", grid[x]);
        if ((x + 1) % 20 == 0) printf("\n");
    }
    printf("\n");
    
    printf("\nÚltima fila (y=%d):\n", header.map_height - 1);
    for (int x = 0; x < header.map_width; x++) {
        printf("%4d ", grid[x + (header.map_height - 1) * header.map_width]);
        if ((x + 1) % 20 == 0) printf("\n");
    }
    printf("\n");
    
    printf("\nPrimera columna (x=0):\n");
    for (int y = 0; y < header.map_height; y++) {
        printf("%4d ", grid[y * header.map_width]);
        if ((y + 1) % 20 == 0) printf("\n");
    }
    printf("\n");
    
    printf("\nÚltima columna (x=%d):\n", header.map_width - 1);
    for (int y = 0; y < header.map_height; y++) {
        printf("%4d ", grid[(header.map_width - 1) + y * header.map_width]);
        if ((y + 1) % 20 == 0) printf("\n");
    }
    printf("\n");
    
    /* Imprimir mapa completo */
    printf("\n=== MAPA COMPLETO (nivel 0) ===\n");
    printf("Leyenda: . = vacío, # = muro, D = puerta\n\n");
    for (int y = 0; y < header.map_height; y++) {
        for (int x = 0; x < header.map_width; x++) {
            int val = grid[x + y * header.map_width];
            if (val == 0) printf(". ");
            else if (val >= 1001) printf("D ");
            else printf("# ");
        }
        printf("\n");
    }
    
    free(grid);
    fclose(f);
    
    return 0;
}
