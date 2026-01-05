/*    
 * Creador de mapas WLD - Versión que escribe archivos válidos para libmod_heightmap  
 * Genera una sala cuadrada de 1024x1024 con texturas desde TEX    
 */    
  
#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include <stdint.h>  
  
#pragma pack(push, 1)  
  
typedef struct {  
    int active;  
    int x, y;  
    int links;  
} WLD_Point;  
  
typedef struct {  
    int active;  
    int type;  
    int p1, p2;  
    int front_region, back_region;  
    int texture;  
    int texture_top;  
    int texture_bot;  
    int fade;  
} WLD_Wall;  
  
typedef struct {  
    int active;  
    int type;  
    int floor_height, ceil_height;  
    int floor_tex;  
    int ceil_tex;  
    int fade;  
} WLD_Region;  
  
#pragma pack(pop)  
  
void create_simple_room(const char *filename) {  
    FILE *file = fopen(filename, "wb");  
    if (!file) {  
        printf("Error: No se pudo crear %s\n", filename);  
        return;  
    }  
      
    // Magic header WLD  
    fwrite("wld\x1a\x0d\x0a\x01\x00", 8, 1, file);  
      
    // Total size  
    int total_size = 548 + 4 + 4*16 + 4 + 4*36 + 4 + 1*32 + 4 + 4;  
    fwrite(&total_size, 4, 1, file);  
      
    // Metadata del editor (548 bytes)  
    char editor_metadata[548];  
    memset(editor_metadata, 0, 548);  
    fwrite(editor_metadata, 1, 548, file);  
      
    // Puntos (4 vértices de un cuadrado)  
    int num_points = 4;  
    fwrite(&num_points, 4, 1, file);  
      
    WLD_Point points[4] = {  
        {1, -100, -100, 0},  // Esquina inferior izquierda  
        {1,  100, -100, 0},  // Esquina inferior derecha  
        {1,  100,  100, 0},  // Esquina superior derecha  
        {1, -100,  100, 0}   // Esquina superior izquierda  
    };  
    fwrite(points, sizeof(WLD_Point), 4, file);  
      
    // Paredes (4 paredes conectando los puntos)  
    int num_walls = 4;  
    fwrite(&num_walls, 4, 1, file);  
      
    WLD_Wall walls[4] = {  
        {1, 0, 0, 1, 0, -1, 1, 0, 0, 0},  // Pared sur  
        {1, 0, 1, 2, 0, -1, 1, 0, 0, 0},  // Pared este  
        {1, 0, 2, 3, 0, -1, 1, 0, 0, 0},  // Pared norte  
        {1, 0, 3, 0, 0, -1, 1, 0, 0, 0}   // Pared oeste  
    };  
    fwrite(walls, sizeof(WLD_Wall), 4, file);  
      
    // Región (1 sala)  
    int num_regions = 1;  
    fwrite(&num_regions, 4, 1, file);  
      
    WLD_Region region = {  
        1, 0, 0, 200,    // active, type, floor_height=0, ceil_height=200  
        2, 3, 0         // floor_tex=2, ceil_tex=3, fade=0  
    };  
    fwrite(&region, sizeof(WLD_Region), 1, file);  
      
    // Flags (ninguno)  
    int num_flags = 0;  
    fwrite(&num_flags, 4, 1, file);  
      
    // Background color  
    int fondo = 0;  
    fwrite(&fondo, 4, 1, file);  
      
    fclose(file);  
      
    printf("Mapa simple creado: %s\n", filename);  
    printf("  - Sala cuadrada de 200x200 unidades\n");  
    printf("  - Paredes con textura índice 1\n");  
    printf("  - Suelo con textura índice 2\n");  
    printf("  - Techo con textura índice 3\n");  
}  
  
int main() {  
    create_simple_room("test.wld");  
    return 0;  
}