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
    
    /* Paredes */
    QVector<Wall> walls;
    
    /* Alturas */
    float floor_z;
    float ceiling_z;
    
    /* Slopes - REMOVED (Replaced by MD3 Models) */
    // Pivot is implicitly wall[0]
    
    /* Texturas */
    int floor_texture_id;
    int ceiling_texture_id;
    
    /* Iluminación */
    int light_level;
    
    /* Portales (IDs) */
    QVector<int> portal_ids;
    
    int group_id;  // NEW: ID of the group this sector belongs to (-1 if ungrouped)
    
    /* Jerarquía de sectores anidados */
    int parent_sector_id;              // -1 = root, >=0 = parent sector ID
    QVector<int> child_sector_ids;     // List of child sector IDs
    
    /* ========================================================================
       SECTORES ANIDADOS (Nested Sectors) - REMOVED (v9 is flat)
       ======================================================================== */
    
    /* Constructor */
    Sector() : sector_id(0), floor_z(0.0f), ceiling_z(256.0f),
               floor_texture_id(0), ceiling_texture_id(0), light_level(255),
               group_id(-1), parent_sector_id(-1) {}
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
   ENTITY INSTANCE - For process generation
   ============================================================================ */

struct EntityInstance {
    QString processName;
    QString assetPath;
    QString type;
    int spawn_id;
    float x, y, z;
    
    EntityInstance() : spawn_id(0), x(0), y(0), z(0) {}
    EntityInstance(const QString &pname, const QString &asset, const QString &t, int id, float px, float py, float pz)
        : processName(pname), assetPath(asset), type(t), spawn_id(id), x(px), y(py), z(pz) {}
};

/* ============================================================================
   DECAL - Overlay texture for floors/ceilings
   ============================================================================ */

struct Decal {
    int id;                     // Unique decal ID
    int sector_id;              // Sector this decal belongs to
    bool is_floor;              // true = floor, false = ceiling
    
    // Position and size (world coordinates)
    float x, y;                 // Center position
    float width, height;        // Dimensions
    float rotation;             // Rotation in radians
    
    // Texture
    int texture_id;             // Texture ID from FPG
    
    // Rendering
    float alpha;                // Transparency (0.0 = invisible, 1.0 = opaque)
    int render_order;           // Render order (higher = on top)
    
    Decal() : id(0), sector_id(-1), is_floor(true),
              x(0), y(0), width(100), height(100), rotation(0),
              texture_id(0), alpha(1.0f), render_order(0) {}
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
   SECTOR GROUP - Agrupación de sectores relacionados
   ============================================================================ */

struct SectorGroup {
    int group_id;
    QString name;
    QVector<int> sector_ids;  // IDs de sectores en este grupo
    
    SectorGroup() : group_id(-1), name("Grupo") {}
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
    
    /* Decals */
    QVector<Decal> decals;

    /* Entities (MD3 Models) */
    QVector<EntityInstance> entities;
    
    /* Sector Groups */
    QVector<SectorGroup> sectorGroups;
    
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
    
    /* Helper: Get next group ID */
    int getNextGroupId() const {
        int maxId = -1;
        for (const SectorGroup &g : sectorGroups) {
            if (g.group_id > maxId) maxId = g.group_id;
        }
        return maxId + 1;
    }
    
    /* Helper: Find sector by ID */
    Sector* findSector(int sectorId) {
        for (Sector &s : sectors) {
            if (s.sector_id == sectorId) return &s;
        }
        return nullptr;
    }
    
    const Sector* findSector(int sectorId) const {
        for (const Sector &s : sectors) {
            if (s.sector_id == sectorId) return &s;
        }
        return nullptr;
    }
    
    /* Helper: Find group by ID */
    SectorGroup* findGroup(int groupId) {
        for (SectorGroup &g : sectorGroups) {
            if (g.group_id == groupId) return &g;
        }
        return nullptr;
    }
    
    const SectorGroup* findGroup(int groupId) const {
        for (const SectorGroup &g : sectorGroups) {
            if (g.group_id == groupId) return &g;
        }
        return nullptr;
    }
    
    /* Helper: Find which group contains a sector */
    int findGroupForSector(int sectorId) const {
        for (const SectorGroup &g : sectorGroups) {
            if (g.sector_ids.contains(sectorId)) {
                return g.group_id;
            }
        }
        return -1;
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
    
    /* Helper: Get next decal ID */
    int getNextDecalId() const {
        int maxId = -1;
        for (const Decal &d : decals) {
            if (d.id > maxId) maxId = d.id;
        }
        return maxId + 1;
    }

    /* Helper: Get next unified ID for SpawnFlags and Entities */
    int getNextSpawnEntityId() const {
        int maxId = 0;
        for (const SpawnFlag &f : spawnFlags) {
            if (f.flagId > maxId) maxId = f.flagId;
        }
        for (const EntityInstance &e : entities) {
            if (e.spawn_id > maxId) maxId = e.spawn_id;
        }
        return maxId + 1;
    }
    
    /* Helper: Find portal by ID */
    Portal* findPortal(int portal_id) {
        for (Portal &p : portals) {
            if (p.portal_id == portal_id) return &p;
        }
        return nullptr;
    }
    
    /* Helper: Find decal by ID */
    Decal* findDecal(int decal_id) {
        for (Decal &d : decals) {
            if (d.id == decal_id) return &d;
        }
        return nullptr;
    }
};

#endif // MAPDATA_H
