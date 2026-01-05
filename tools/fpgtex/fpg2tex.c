// fpg2tex.c - Convertidor de archivos FPG a TEX  
#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include <stdint.h>  
  
#pragma pack(push, 1)  
  
// Estructuras FPG (basadas en divmap3d.c) [19-cite-0](#19-cite-0)   
typedef struct {  
    char magic1[3];  
    char magic2[4];  
    char version;  
    uint8_t pal[768];  
    uint8_t gamas[576];  
} FPG_header;  
  
typedef struct {  
    int  cod;  
    int  tam;  
    char descrip[32];  
    char filename[12];  
    int  ancho;  
    int  alto;  
    int  puntos;  
} FPG_info;  
  
typedef struct {  
    uint16_t x;  
    uint16_t y;  
} FPG_points;  
  
// Estructuras TEX (basadas en tex/tex.c) [19-cite-1](#19-cite-1)   
typedef struct {  
    char magic[4];  
    uint32_t version;  
    uint16_t num_images;  
    uint8_t reserved[6];  
} TEX_HEADER;  
  
typedef struct {  
    uint16_t index;  
    uint16_t width;  
    uint16_t height;  
    uint16_t format;  
    uint8_t reserved[250];  
} TEX_ENTRY;  
  
#pragma pack(pop)  
  
void dump_file_header(const char *filename, int bytes)  
{  
    FILE *f = fopen(filename, "rb");  
    if (!f) return;  
      
    unsigned char buffer[bytes];  
    fread(buffer, 1, bytes, f);  
    fclose(f);  
      
    printf("Header hex dump (%s):\n", filename);  
    for (int i = 0; i < bytes; i++) {  
        printf("%02X ", buffer[i]);  
        if ((i + 1) % 16 == 0) printf("\n");  
    }  
    printf("\n");  
      
    printf("Header ASCII (%s):\n", filename);  
    for (int i = 0; i < bytes; i++) {  
        if (buffer[i] >= 32 && buffer[i] <= 126) {  
            printf("%c", buffer[i]);  
        } else {  
            printf(".");  
        }  
    }  
    printf("\n\n");  
}  
  
int convert_fpg_to_tex(const char *fpg_file, const char *tex_file)  
{  
    printf("Analizando archivo: %s\n", fpg_file);  
      
    // Mostrar primeros 32 bytes del archivo  
    dump_file_header(fpg_file, 32);  
      
    FILE *fpg = fopen(fpg_file, "rb");  
    if (!fpg) {  
        printf("ERROR: No se puede abrir FPG: %s\n", fpg_file);  
        return 0;  
    }  
  
    // Leer header FPG  
    FPG_header fpg_header;  
    if (fread(&fpg_header, sizeof(FPG_header), 1, fpg) != 1) {  
        printf("ERROR: No se puede leer header FPG\n");  
        fclose(fpg);  
        return 0;  
    }  
  
    // Mostrar magic bytes encontrados  
    printf("Magic bytes encontrados: %.3s%.4s\n", fpg_header.magic1, fpg_header.magic2);  
    printf("Versión: %d\n", fpg_header.version);  
  
    // Validar magic FPG  
    if (memcmp(fpg_header.magic1, "FPG", 3) != 0) {  
        printf("ERROR: No es un archivo FPG válido\n");  
        printf("Se esperaba 'FPG' en los primeros 3 bytes, se encontró: %.3s\n", fpg_header.magic1);  
          
        // Intentar otros formatos conocidos  
        if (memcmp(fpg_header.magic1, "FPG", 3) == 0) {  
            printf("El archivo parece ser FPG pero con diferente formato\n");  
        } else if (fpg_header.magic1[0] == 0xFF && fpg_header.magic1[1] == 0xFF) {  
            printf("El archivo parece ser un MAP o DAT de BennuGD\n");  
        } else {  
            printf("Formato desconocido\n");  
        }  
          
        fclose(fpg);  
        return 0;  
    }  
  
    // Contar imágenes en el FPG  
    int num_images = 0;  
    FPG_info fpg_info;  
    long current_pos = ftell(fpg);  
      
    while (fread(&fpg_info, sizeof(FPG_info), 1, fpg) == 1) {  
        // Saltar puntos de control e imagen  
        fseek(fpg, sizeof(FPG_points) * fpg_info.puntos, SEEK_CUR);  
        fseek(fpg, fpg_info.ancho * fpg_info.alto, SEEK_CUR);  
        num_images++;  
    }  
      
    // Volver al inicio de las imágenes  
    fseek(fpg, current_pos, SEEK_SET);  
  
    // Crear archivo TEX  
    FILE *tex = fopen(tex_file, "wb");  
    if (!tex) {  
        printf("ERROR: No se puede crear TEX: %s\n", tex_file);  
        fclose(fpg);  
        return 0;  
    }  
  
    // Escribir header TEX  
    TEX_HEADER tex_header;  
    memcpy(tex_header.magic, "TEX\0", 4);  
    tex_header.version = 1;  
    tex_header.num_images = num_images;  
    memset(tex_header.reserved, 0, 6);  
    fwrite(&tex_header, sizeof(TEX_HEADER), 1, tex);  
  
    // Procesar cada imagen del FPG  
    int image_index = 1;  
    while (fread(&fpg_info, sizeof(FPG_info), 1, fpg) == 1) {  
        // Saltar puntos de control  
        fseek(fpg, sizeof(FPG_points) * fpg_info.puntos, SEEK_CUR);  
  
        // Leer imagen indexada  
        uint8_t *indexed_image = malloc(fpg_info.ancho * fpg_info.alto);  
        if (!indexed_image) {  
            printf("ERROR: Sin memoria para imagen %d\n", image_index);  
            break;  
        }  
  
        if (fread(indexed_image, 1, fpg_info.ancho * fpg_info.alto, fpg) !=   
            fpg_info.ancho * fpg_info.alto) {  
            printf("ERROR: No se puede leer imagen %d\n", image_index);  
            free(indexed_image);  
            break;  
        }  
  
        // Convertir a RGBA  
        uint8_t *rgba_image = malloc(fpg_info.ancho * fpg_info.alto * 4);  
        if (!rgba_image) {  
            printf("ERROR: Sin memoria para RGBA %d\n", image_index);  
            free(indexed_image);  
            break;  
        }  
  
        // Convertir paleta a RGBA  
        for (int i = 0; i < fpg_info.ancho * fpg_info.alto; i++) {  
            uint8_t pal_index = indexed_image[i];  
            int rgba_offset = i * 4;  
              
            rgba_image[rgba_offset] = fpg_header.pal[pal_index * 3];     // R  
            rgba_image[rgba_offset + 1] = fpg_header.pal[pal_index * 3 + 1]; // G  
            rgba_image[rgba_offset + 2] = fpg_header.pal[pal_index * 3 + 2]; // B  
            rgba_image[rgba_offset + 3] = (pal_index == 0) ? 0 : 255;     // Alpha (transparente para color 0)  
        }  
  
        // Escribir entrada TEX  
        TEX_ENTRY tex_entry;  
        tex_entry.index = image_index;  
        tex_entry.width = fpg_info.ancho;  
        tex_entry.height = fpg_info.alto;  
        tex_entry.format = 1; // RGBA32  
        memset(tex_entry.reserved, 0, 250);  
        fwrite(&tex_entry, sizeof(TEX_ENTRY), 1, tex);  
  
        // Escribir datos RGBA  
        fwrite(rgba_image, 1, fpg_info.ancho * fpg_info.alto * 4, tex);  
  
        printf("Convertida imagen %d: código %d (%dx%d)\n",   
               image_index, fpg_info.cod, fpg_info.ancho, fpg_info.alto);  
  
        free(indexed_image);  
        free(rgba_image);  
        image_index++;  
    }  
  
    fclose(fpg);  
    fclose(tex);  
  
    printf("Conversión completada: %d imágenes convertidas\n", num_images - 1);  
    return 1;  
}  
  
int main(int argc, char *argv[])  
{  
    if (argc != 3) {  
        printf("Uso: %s <archivo.fpg> <archivo.tex>\n", argv[0]);  
        return 1;  
    }  
  
    printf("=== Convertidor FPG a TEX ===\n");  
      
    if (convert_fpg_to_tex(argv[1], argv[2])) {  
        printf("¡Conversión exitosa!\n");  
        return 0;  
    }  
      
    printf("Error en la conversión\n");  
    return 1;  
}