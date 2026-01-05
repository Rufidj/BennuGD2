#ifndef MAPDATA_H
#define MAPDATA_H

#include <QString>
#include <QPixmap>
#include <QVector>
#include <QPointF>
#include <cstdint>

/* ============================================================================
   TEXTURE ENTRY
   ============================================================================ */

struct TextureEntry {
    QString filename;
    uint32_t id;
    QPixmap pixmap;
    
    TextureEntry() : id(0) {}
    TextureEntry(const QString &fname, uint32_t tid) : filename(fname), id(tid) {}
};

/* ============================================================================
   WALL - Pared con texturas múltiples
   ============================================================================ */

struct Wall {
    int wall_id;
    float x1, y1, x2, y2;
    
    /* Texturas múltiples por altura */
    int texture_id_lower;
    int texture_id_middle;
    int texture_id_upper;
    float texture_split_z_lower;    /* Default: 64.0 */
    float texture_split_z_upper;    /* Default: 192.0 */
    
    int portal_id;                  /* -1 = sólida, >=0 = portal */
    int flags;
    
    Wall() : wall_id(0), x1(0), y1(0), x2(0), y2(0),
             texture_id_lower(0), texture_id_middle(0), texture_id_upper(0),
             texture_split_z_lower(64.0f), texture_split_z_upper(192.0f),
             portal_id(-1), flags(0) {}
};

/* ============================================================================
   SECTOR - Polígono convexo
   ============================================================================ */

struct Sector {
    int sector_id;
    
    /* Geometría del polígono (máx 16 vértices) */
    QVector<QPointF> vertices;
    
    /* Alturas */
    float floor_z;
    float ceiling_z;
    
    /* Texturas */
    int floor_texture_id;
    int ceiling_texture_id;
    
    /* Paredes */
    QVector<Wall> walls;
    
    /* Portales (IDs) */
    QVector<int> portal_ids;
    
    /* Iluminación */
    int light_level;
    
    /* ========================================================================
       SECTORES ANIDADOS (Nested Sectors) - Versión 9+
       ======================================================================== */
    
    /* Jerarquía */
    int parent_sector_id;           // -1 = sector raíz, >=0 = ID del sector padre
    QVector<int> child_sector_ids;  // IDs de sectores hijos
    
    /* Tipo de sector */
    enum SectorType {
        ROOT = 0,           // Sector raíz (sin padre)
        NESTED_ROOM = 1,    // Habitación dentro de otra (hueca)
        NESTED_COLUMN = 2,  // Columna/pilar (sólido)
        NESTED_BOX = 3      // Caja/objeto (sólido, altura limitada)
    };
    SectorType sector_type;
    
    /* Propiedades de anidamiento */
    bool is_solid;          // true = sólido (columna/caja), false = hueco (habitación)
    int nesting_level;      // Nivel de profundidad (0 = raíz, 1 = hijo directo, etc.)
    
    /* Constructor */
    Sector() : sector_id(0), floor_z(0.0f), ceiling_z(256.0f),
               floor_texture_id(0), ceiling_texture_id(0), light_level(255),
               parent_sector_id(-1), sector_type(ROOT), is_solid(false), nesting_level(0) {}
    
    /* Helper methods */
    bool isRoot() const { return parent_sector_id == -1; }
    bool hasChildren() const { return !child_sector_ids.isEmpty(); }
    bool isNested() const { return parent_sector_id >= 0; }
    bool isSolid() const { return is_solid; }
};

/* ============================================================================
   PORTAL - Conexión entre sectores
   ============================================================================ */

struct Portal {
    int portal_id;
    int sector_a, sector_b;
    int wall_id_a, wall_id_b;
    float x1, y1, x2, y2;
    
    Portal() : portal_id(0), sector_a(-1), sector_b(-1),
               wall_id_a(-1), wall_id_b(-1),
               x1(0), y1(0), x2(0), y2(0) {}
};

/* ============================================================================
   SPRITE
   ============================================================================ */

struct SpriteData {
    float x, y, z;
    int texture_id;
    int w, h;
    float rot;
    
    SpriteData() : x(0), y(0), z(0), texture_id(0), w(128), h(128), rot(0) {}
};

/* ============================================================================
   SPAWN FLAG
   ============================================================================ */

struct SpawnFlag {
    int flagId;
    float x, y, z;
    
    SpawnFlag() : flagId(1), x(384.0f), y(384.0f), z(0.0f) {}
    SpawnFlag(int id, float px, float py, float pz) 
        : flagId(id), x(px), y(py), z(pz) {}
};

/* ============================================================================
   CAMERA
   ============================================================================ */

struct CameraData {
    float x, y, z;
    float rotation;
    float pitch;
    bool enabled;
    
    CameraData() : x(384.0f), y(384.0f), z(0.0f), rotation(0.0f), pitch(0.0f), enabled(false) {}
};

/* ============================================================================
   MAP DATA - Estructura principal del mapa
   ============================================================================ */

struct MapData {
    /* Sectores y portales */
    QVector<Sector> sectors;
    QVector<Portal> portals;
    
    /* Sprites */
    QVector<SpriteData> sprites;
    
    /* Spawn Flags */
    QVector<SpawnFlag> spawnFlags;
    
    /* Cámara */
    CameraData camera;
    
    /* Skybox */
    int skyTextureID = 0;
    
    /* Texturas cargadas */
    QVector<TextureEntry> textures;
    
    MapData() {}
    
    /* Helper: Get next sector ID */
    int getNextSectorId() const {
        int maxId = -1;
        for (const Sector &s : sectors) {
            if (s.sector_id > maxId) maxId = s.sector_id;
        }
        return maxId + 1;
    }
    
    /* Helper: Get next wall ID */
    int getNextWallId() const {
        int maxId = -1;
        for (const Sector &s : sectors) {
            for (const Wall &w : s.walls) {
                if (w.wall_id > maxId) maxId = w.wall_id;
            }
        }
        return maxId + 1;
    }
    
    /* Helper: Get next portal ID */
    int getNextPortalId() const {
        int maxId = -1;
        for (const Portal &p : portals) {
            if (p.portal_id > maxId) maxId = p.portal_id;
        }
        return maxId + 1;
    }
    
    /* Helper: Find sector by ID */
    Sector* findSector(int sector_id) {
        for (Sector &s : sectors) {
            if (s.sector_id == sector_id) return &s;
        }
        return nullptr;
    }
    
    /* Helper: Find portal by ID */
    Portal* findPortal(int portal_id) {
        for (Portal &p : portals) {
            if (p.portal_id == portal_id) return &p;
        }
        return nullptr;
    }
};

#endif // MAPDATA_H
