#ifndef FPGLOADER_H
#define FPGLOADER_H

#include <QString>
#include <QVector>
#include <QMap>
#include <QPixmap>
#include <functional>
#include "mapdata.h"

// Estructuras para formato FPG de BennuGD2
typedef struct {
    int code;          // Código del mapa
    int regsize;       // Tamaño del registro (normalmente 0)
    char name[32];     // Nombre del map
    char filename[12]; // Nombre del archivo
    int width;         // Ancho
    int height;        // Alto
    int flags;         // Flags (incluye cantidad de control points)
} FPG_CHUNK;

typedef struct {
    uint16_t x;
    uint16_t y;
} FPG_CONTROL_POINT;

class FPGLoader
{
public:
    FPGLoader();
    
    // Cargar archivo FPG y retornar lista de texturas
    // progressCallback: función opcional que recibe (current, total, textureName)
    static bool loadFPG(const QString &filename, QVector<TextureEntry> &textures,
                       std::function<void(int, int, const QString&)> progressCallback = nullptr);
    
    // Obtener mapa de texturas por ID
    static QMap<int, QPixmap> getTextureMap(const QVector<TextureEntry> &textures);
    
private:
    // No se permite instanciar (clase estática)
};

#endif // FPGLOADER_H
