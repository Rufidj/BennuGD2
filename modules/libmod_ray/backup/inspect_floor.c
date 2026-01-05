#include <stdio.h>
#include <stdint.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <archivo.raymap>\n", argv[0]);
        return 1;
    }
    
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        printf("Error: No se pudo abrir %s\n", argv[1]);
        return 1;
    }
    
    // Ir a posición 9280 (donde deberían estar los floor grids)
    fseek(f, 9280, SEEK_SET);
    
    printf("Leyendo desde posición 9280 (floor grids):\n");
    printf("Primeros 40 ints (160 bytes):\n");
    
    for (int i = 0; i < 40; i++) {
        int value;
        if (fread(&value, sizeof(int), 1, f) == 1) {
            printf("  [%d] = %d\n", i, value);
        }
    }
    
    fclose(f);
    return 0;
}
