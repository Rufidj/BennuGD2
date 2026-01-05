#ifndef RAYMAPFORMAT_H
#define RAYMAPFORMAT_H

#include <QString>
#include <cstdint>
#include <functional>
#include "mapdata.h"

// Header del formato .raymap
struct RAY_MapHeader {
    char magic[8];           // "RAYMAP\x1a"
    uint32_t version;        // 1, 2, 3, or 4
    uint32_t map_width;
    uint32_t map_height;
    uint32_t num_levels;
    uint32_t num_sprites;
    uint32_t num_thin_walls;
    uint32_t num_thick_walls;
    uint32_t num_spawn_flags;  // Versión 3+
    
    // Versión 2+: Datos de cámara y skybox
    float camera_x;
    float camera_y;
    float camera_z;
    float camera_rot;
    float camera_pitch;
    int32_t skyTextureID;    // ID de textura para el skybox (0 = sin skybox)
};

class RayMapFormat
{
public:
    RayMapFormat();
    
    // Cargar mapa desde archivo .raymap (soporta v1 y v2)
    static bool loadMap(const QString &filename, MapData &mapData,
                       std::function<void(const QString&)> progressCallback = nullptr);
    
    // Guardar mapa a archivo .raymap (versión 4)
    static bool saveMap(const QString &filename, const MapData &mapData,
                       std::function<void(const QString&)> progressCallback = nullptr);
    
    // Exportar a formato de texto (CSV)
    static bool exportToText(const QString &directory, const MapData &mapData);
    
    // Importar desde formato de texto (CSV)
    static bool importFromText(const QString &configFile, MapData &mapData);
    
private:
    static bool readGrid(QDataStream &in, QVector<int> &grid, int width, int height);
    static bool writeGrid(QDataStream &out, const QVector<int> &grid);
    static bool readFloatGrid(QDataStream &in, QVector<float> &grid, int width, int height);
    static bool writeFloatGrid(QDataStream &out, const QVector<float> &grid);
};

#endif // RAYMAPFORMAT_H
