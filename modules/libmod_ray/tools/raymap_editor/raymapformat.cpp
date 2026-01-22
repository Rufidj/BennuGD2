/*
 * raymapformat.cpp - Map Loading/Saving for Editor (Format v9)
 * v9: Added nested sectors support
 * v8: Geometric sectors only
 */

#include "raymapformat.h"
#include "mapdata.h"
#include <QFile>
#include <QDataStream>
#include <QDebug>
#include <cstring>
#include <QMap>
#include <QFileInfo>

RayMapFormat::RayMapFormat()
{
}

/* ============================================================================
   MAP LOADING
   ============================================================================ */

bool RayMapFormat::loadMap(const QString &filename, MapData &mapData,
                           std::function<void(const QString&)> progressCallback)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "No se pudo abrir el archivo:" << filename;
        return false;
    }
    
    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);
    
    /* Read header */
    char magic[8];
    uint32_t version, num_sectors, num_portals, num_sprites, num_spawn_flags;
    float camera_x, camera_y, camera_z, camera_rot, camera_pitch;
    int32_t skyTextureID;
    
    in.readRawData(magic, 8);
    in.readRawData(reinterpret_cast<char*>(&version), sizeof(uint32_t));
    in.readRawData(reinterpret_cast<char*>(&num_sectors), sizeof(uint32_t));
    in.readRawData(reinterpret_cast<char*>(&num_portals), sizeof(uint32_t));
    in.readRawData(reinterpret_cast<char*>(&num_sprites), sizeof(uint32_t));
    in.readRawData(reinterpret_cast<char*>(&num_spawn_flags), sizeof(uint32_t));
    in.readRawData(reinterpret_cast<char*>(&camera_x), sizeof(float));
    in.readRawData(reinterpret_cast<char*>(&camera_y), sizeof(float));
    in.readRawData(reinterpret_cast<char*>(&camera_z), sizeof(float));
    in.readRawData(reinterpret_cast<char*>(&camera_rot), sizeof(float));
    in.readRawData(reinterpret_cast<char*>(&camera_pitch), sizeof(float));
    in.readRawData(reinterpret_cast<char*>(&skyTextureID), sizeof(int32_t));
    
    /* Verify magic number */
    if (memcmp(magic, "RAYMAP\x1a", 7) != 0) {
        qWarning() << "Formato de archivo inválido";
        file.close();
        return false;
    }
    
    /* Verify version */
    if (version < 8 || version > 9) {
        qWarning() << "Versión no soportada:" << version << "(solo v8-v9)";
        file.close();
        return false;
    }
    
    qDebug() << "Cargando mapa v" << version << ":" << num_sectors << "sectores," << num_portals << "portales";
    
    if (progressCallback) progressCallback("Cargando sectores...");
    
    /* Set camera */
    mapData.camera.x = camera_x;
    mapData.camera.y = camera_y;
    mapData.camera.z = camera_z;
    mapData.camera.rotation = camera_rot;
    mapData.camera.pitch = camera_pitch;
    mapData.camera.enabled = true;
    mapData.skyTextureID = skyTextureID;
    
    /* Load sectors */
    mapData.sectors.clear();
    for (uint32_t i = 0; i < num_sectors; i++) {
        Sector sector;
        
        in.readRawData(reinterpret_cast<char*>(&sector.sector_id), sizeof(int));
        in.readRawData(reinterpret_cast<char*>(&sector.floor_z), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&sector.ceiling_z), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&sector.floor_texture_id), sizeof(int));
        in.readRawData(reinterpret_cast<char*>(&sector.ceiling_texture_id), sizeof(int));
        in.readRawData(reinterpret_cast<char*>(&sector.light_level), sizeof(int));
        
        /* Read vertices */
        uint32_t num_vertices;
        in.readRawData(reinterpret_cast<char*>(&num_vertices), sizeof(uint32_t));
        
        for (uint32_t v = 0; v < num_vertices; v++) {
            float x, y;
            in.readRawData(reinterpret_cast<char*>(&x), sizeof(float));
            in.readRawData(reinterpret_cast<char*>(&y), sizeof(float));
            sector.vertices.append(QPointF(x, y));
        }
        
        /* Read walls */
        uint32_t num_walls;
        in.readRawData(reinterpret_cast<char*>(&num_walls), sizeof(uint32_t));
        
        for (uint32_t w = 0; w < num_walls; w++) {
            Wall wall;
            
            in.readRawData(reinterpret_cast<char*>(&wall.wall_id), sizeof(int));
            in.readRawData(reinterpret_cast<char*>(&wall.x1), sizeof(float));
            in.readRawData(reinterpret_cast<char*>(&wall.y1), sizeof(float));
            in.readRawData(reinterpret_cast<char*>(&wall.x2), sizeof(float));
            in.readRawData(reinterpret_cast<char*>(&wall.y2), sizeof(float));
            in.readRawData(reinterpret_cast<char*>(&wall.texture_id_lower), sizeof(int));
            in.readRawData(reinterpret_cast<char*>(&wall.texture_id_middle), sizeof(int));
            in.readRawData(reinterpret_cast<char*>(&wall.texture_id_upper), sizeof(int));
            in.readRawData(reinterpret_cast<char*>(&wall.texture_split_z_lower), sizeof(float));
            in.readRawData(reinterpret_cast<char*>(&wall.texture_split_z_upper), sizeof(float));
            in.readRawData(reinterpret_cast<char*>(&wall.portal_id), sizeof(int));
            in.readRawData(reinterpret_cast<char*>(&wall.flags), sizeof(int));
            
            sector.walls.append(wall);
        }
        
        // Load hierarchy fields (parent and children)
        in.readRawData(reinterpret_cast<char*>(&sector.parent_sector_id), sizeof(int));
        int numChildren;
        in.readRawData(reinterpret_cast<char*>(&numChildren), sizeof(int));
        for (int c = 0; c < numChildren; c++) {
            int childId;
            in.readRawData(reinterpret_cast<char*>(&childId), sizeof(int));
            sector.child_sector_ids.append(childId);
        }
        
        mapData.sectors.append(sector);
    }
    
    if (progressCallback) progressCallback("Cargando portales...");
    
    /* Load portals */
    mapData.portals.clear();
    for (uint32_t i = 0; i < num_portals; i++) {
        Portal portal;
        
        in.readRawData(reinterpret_cast<char*>(&portal.portal_id), sizeof(int));
        in.readRawData(reinterpret_cast<char*>(&portal.sector_a), sizeof(int));
        in.readRawData(reinterpret_cast<char*>(&portal.sector_b), sizeof(int));
        in.readRawData(reinterpret_cast<char*>(&portal.wall_id_a), sizeof(int));
        in.readRawData(reinterpret_cast<char*>(&portal.wall_id_b), sizeof(int));
        in.readRawData(reinterpret_cast<char*>(&portal.x1), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&portal.y1), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&portal.x2), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&portal.y2), sizeof(float));
        
        /* Add portal IDs to sectors */
        for (Sector &sector : mapData.sectors) {
            if (sector.sector_id == portal.sector_a || sector.sector_id == portal.sector_b) {
                if (!sector.portal_ids.contains(portal.portal_id)) {
                    sector.portal_ids.append(portal.portal_id);
                }
            }
        }
        
        mapData.portals.append(portal);
    }
    
    if (progressCallback) progressCallback("Cargando sprites...");
    
    /* Load sprites */
    mapData.sprites.clear();
    for (uint32_t i = 0; i < num_sprites; i++) {
        SpriteData sprite;
        
        in.readRawData(reinterpret_cast<char*>(&sprite.texture_id), sizeof(int));
        in.readRawData(reinterpret_cast<char*>(&sprite.x), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&sprite.y), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&sprite.z), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&sprite.w), sizeof(int));
        in.readRawData(reinterpret_cast<char*>(&sprite.h), sizeof(int));
        in.readRawData(reinterpret_cast<char*>(&sprite.rot), sizeof(float));
        
        mapData.sprites.append(sprite);
    }
    
    if (progressCallback) progressCallback("Cargando spawn flags...");
    
    /* Load spawn flags */
    mapData.spawnFlags.clear();
    for (uint32_t i = 0; i < num_spawn_flags; i++) {
        SpawnFlag flag;
        
        in.readRawData(reinterpret_cast<char*>(&flag.flagId), sizeof(int));
        in.readRawData(reinterpret_cast<char*>(&flag.x), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&flag.y), sizeof(float));
        in.readRawData(reinterpret_cast<char*>(&flag.z), sizeof(float));
        
        mapData.spawnFlags.append(flag);
    }
    
    /* Load entities (appended at end for compatibility) */
    mapData.entities.clear();
    
    // Check if there is more data (entities appended)
    if (!in.atEnd()) {
        uint32_t num_entities;
        in.readRawData(reinterpret_cast<char*>(&num_entities), sizeof(uint32_t));
        
        for (uint32_t i = 0; i < num_entities; i++) {
            EntityInstance entity;
            
            in.readRawData(reinterpret_cast<char*>(&entity.spawn_id), sizeof(int));
            in.readRawData(reinterpret_cast<char*>(&entity.x), sizeof(float));
            in.readRawData(reinterpret_cast<char*>(&entity.y), sizeof(float));
            in.readRawData(reinterpret_cast<char*>(&entity.z), sizeof(float));
            
            // Asset path
            uint32_t pathLen;
            in.readRawData(reinterpret_cast<char*>(&pathLen), sizeof(uint32_t));
            QByteArray pathBytes;
            pathBytes.resize(pathLen);
            in.readRawData(pathBytes.data(), pathLen);
            entity.assetPath = QString::fromUtf8(pathBytes);
            
            // Type
            uint32_t typeLen;
            in.readRawData(reinterpret_cast<char*>(&typeLen), sizeof(uint32_t));
            QByteArray typeBytes;
            typeBytes.resize(typeLen);
            in.readRawData(typeBytes.data(), typeLen);
            entity.type = QString::fromUtf8(typeBytes);
            
            // Generate processName from asset path if not set
            // (for backward compatibility with old maps)
            QFileInfo assetInfo(entity.assetPath);
            entity.processName = assetInfo.completeBaseName();
            
            mapData.entities.append(entity);
        }
    }
    
    file.close();
    
    qDebug() << "Mapa cargado:" << mapData.sectors.size() << "sectores,"
             << mapData.portals.size() << "portales,"
             << mapData.sprites.size() << "sprites,"
             << mapData.spawnFlags.size() << "spawn flags,"
             << mapData.entities.size() << "entidades";
    
    return true;
}

/* ============================================================================
   MAP SAVING
   ============================================================================ */

bool RayMapFormat::saveMap(const QString &filename, const MapData &mapData,
                           std::function<void(const QString&)> progressCallback)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "No se pudo crear el archivo:" << filename;
        return false;
    }
    
    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);
    
    /* Prepare header */
    char magic[8];
    memcpy(magic, "RAYMAP\x1a", 7);
    magic[7] = 0;
    
    // --- PORTAL RENUMBERING (Defragmentation) ---
    // Engine expects portals to be contiguous 0..N-1 matching array index.
    // Editor IDs might have gaps due to deletions. We must map them.
    QMap<int, int> portalIdMap;
    int nextPortalId = 0;
    for (const Portal &p : mapData.portals) {
        portalIdMap[p.portal_id] = nextPortalId++;
    }
    // --------------------------------------------
    
    uint32_t version = 9;  // Updated to v9 for nested sectors
    uint32_t num_sectors = mapData.sectors.size();
    uint32_t num_portals = mapData.portals.size();
    uint32_t num_sprites = mapData.sprites.size();
    uint32_t num_spawn_flags = mapData.spawnFlags.size() + mapData.entities.size(); // Include entities!
    
    float camera_x = mapData.camera.x;
    float camera_y = mapData.camera.y;
    float camera_z = mapData.camera.z;
    float camera_rot = mapData.camera.rotation;
    float camera_pitch = mapData.camera.pitch;
    int32_t skyTextureID = mapData.skyTextureID;
    
    qDebug() << "DEBUG SAVE: Writing camera position:" << camera_x << "," << camera_y << "," << camera_z;
    
    /* Write header */
    out.writeRawData(magic, 8);
    out.writeRawData(reinterpret_cast<const char*>(&version), sizeof(uint32_t));
    out.writeRawData(reinterpret_cast<const char*>(&num_sectors), sizeof(uint32_t));
    out.writeRawData(reinterpret_cast<const char*>(&num_portals), sizeof(uint32_t));
    out.writeRawData(reinterpret_cast<const char*>(&num_sprites), sizeof(uint32_t));
    out.writeRawData(reinterpret_cast<const char*>(&num_spawn_flags), sizeof(uint32_t));
    out.writeRawData(reinterpret_cast<const char*>(&camera_x), sizeof(float));
    out.writeRawData(reinterpret_cast<const char*>(&camera_y), sizeof(float));
    out.writeRawData(reinterpret_cast<const char*>(&camera_z), sizeof(float));
    out.writeRawData(reinterpret_cast<const char*>(&camera_rot), sizeof(float));
    out.writeRawData(reinterpret_cast<const char*>(&camera_pitch), sizeof(float));
    out.writeRawData(reinterpret_cast<const char*>(&skyTextureID), sizeof(int32_t));
    
    if (progressCallback) progressCallback("Guardando sectores...");
    
    /* Write sectors */
    for (const Sector &sector : mapData.sectors) {
        out.writeRawData(reinterpret_cast<const char*>(&sector.sector_id), sizeof(int));
        out.writeRawData(reinterpret_cast<const char*>(&sector.floor_z), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&sector.ceiling_z), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&sector.floor_texture_id), sizeof(int));
        out.writeRawData(reinterpret_cast<const char*>(&sector.ceiling_texture_id), sizeof(int));
        out.writeRawData(reinterpret_cast<const char*>(&sector.light_level), sizeof(int));
        
        /* Write vertices */
        uint32_t num_vertices = sector.vertices.size();
        out.writeRawData(reinterpret_cast<const char*>(&num_vertices), sizeof(uint32_t));
        
        for (const QPointF &vertex : sector.vertices) {
            float x = vertex.x();
            float y = vertex.y();
            out.writeRawData(reinterpret_cast<const char*>(&x), sizeof(float));
            out.writeRawData(reinterpret_cast<const char*>(&y), sizeof(float));
        }
        
        /* Write walls */
        uint32_t num_walls = sector.walls.size();
        out.writeRawData(reinterpret_cast<const char*>(&num_walls), sizeof(uint32_t));
        
        for (const Wall &wall : sector.walls) {
            out.writeRawData(reinterpret_cast<const char*>(&wall.wall_id), sizeof(int));
            out.writeRawData(reinterpret_cast<const char*>(&wall.x1), sizeof(float));
            out.writeRawData(reinterpret_cast<const char*>(&wall.y1), sizeof(float));
            out.writeRawData(reinterpret_cast<const char*>(&wall.x2), sizeof(float));
            out.writeRawData(reinterpret_cast<const char*>(&wall.y2), sizeof(float));
            out.writeRawData(reinterpret_cast<const char*>(&wall.texture_id_lower), sizeof(int));
            out.writeRawData(reinterpret_cast<const char*>(&wall.texture_id_middle), sizeof(int));
            out.writeRawData(reinterpret_cast<const char*>(&wall.texture_id_upper), sizeof(int));
            out.writeRawData(reinterpret_cast<const char*>(&wall.texture_split_z_lower), sizeof(float));
            out.writeRawData(reinterpret_cast<const char*>(&wall.texture_split_z_upper), sizeof(float));
            
            // USE MAPPED ID
            int savedPortalId = -1;
            if (wall.portal_id >= 0 && portalIdMap.contains(wall.portal_id)) {
                savedPortalId = portalIdMap[wall.portal_id];
            } else if (wall.portal_id >= 0) {
                 qWarning() << "Warning: Wall points to non-existent portal ID:" << wall.portal_id;
            }
            out.writeRawData(reinterpret_cast<const char*>(&savedPortalId), sizeof(int));
            
            out.writeRawData(reinterpret_cast<const char*>(&wall.flags), sizeof(int));
        }
        
        // Save hierarchy (parent and children)
        out.writeRawData(reinterpret_cast<const char*>(&sector.parent_sector_id), sizeof(int));
        int numChildren = sector.child_sector_ids.size();
        out.writeRawData(reinterpret_cast<const char*>(&numChildren), sizeof(int));
        for (int childId : sector.child_sector_ids) {
            out.writeRawData(reinterpret_cast<const char*>(&childId), sizeof(int));
        }
    }
    
    if (progressCallback) progressCallback("Guardando portales...");
    
    /* Write portals */
    for (const Portal &portal : mapData.portals) {
        // USE MAPPED ID (which is sequential 0..N-1)
        int savedPortalId = portalIdMap[portal.portal_id];
        
        out.writeRawData(reinterpret_cast<const char*>(&savedPortalId), sizeof(int));
        out.writeRawData(reinterpret_cast<const char*>(&portal.sector_a), sizeof(int));
        out.writeRawData(reinterpret_cast<const char*>(&portal.sector_b), sizeof(int));
        out.writeRawData(reinterpret_cast<const char*>(&portal.wall_id_a), sizeof(int));
        out.writeRawData(reinterpret_cast<const char*>(&portal.wall_id_b), sizeof(int));
        out.writeRawData(reinterpret_cast<const char*>(&portal.x1), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&portal.y1), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&portal.x2), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&portal.y2), sizeof(float));
    }
    
    if (progressCallback) progressCallback("Guardando sprites...");
    
    /* Write sprites */
    for (const SpriteData &sprite : mapData.sprites) {
        out.writeRawData(reinterpret_cast<const char*>(&sprite.texture_id), sizeof(int));
        out.writeRawData(reinterpret_cast<const char*>(&sprite.x), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&sprite.y), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&sprite.z), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&sprite.w), sizeof(int));
        out.writeRawData(reinterpret_cast<const char*>(&sprite.h), sizeof(int));
        out.writeRawData(reinterpret_cast<const char*>(&sprite.rot), sizeof(float));
    }
    
    if (progressCallback) progressCallback("Guardando spawn flags...");
    
    /* Write spawn flags (including entities) */
    // First write regular spawn flags
    for (const SpawnFlag &flag : mapData.spawnFlags) {
        out.writeRawData(reinterpret_cast<const char*>(&flag.flagId), sizeof(int));
        out.writeRawData(reinterpret_cast<const char*>(&flag.x), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&flag.y), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&flag.z), sizeof(float));
    }
    
    // Then write entities as spawn flags (so motor can load them)
    for (const EntityInstance &entity : mapData.entities) {
        out.writeRawData(reinterpret_cast<const char*>(&entity.spawn_id), sizeof(int));
        out.writeRawData(reinterpret_cast<const char*>(&entity.x), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&entity.y), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&entity.z), sizeof(float));
    }
    
    if (progressCallback) progressCallback("Guardando entidades...");
    
    /* Write entities (appended at end for compatibility) */
    uint32_t num_entities = mapData.entities.size();
    out.writeRawData(reinterpret_cast<const char*>(&num_entities), sizeof(uint32_t));
    
    for (const EntityInstance &entity : mapData.entities) {
        out.writeRawData(reinterpret_cast<const char*>(&entity.spawn_id), sizeof(int));
        out.writeRawData(reinterpret_cast<const char*>(&entity.x), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&entity.y), sizeof(float));
        out.writeRawData(reinterpret_cast<const char*>(&entity.z), sizeof(float));
        
        // Asset path
        QByteArray pathBytes = entity.assetPath.toUtf8();
        uint32_t pathLen = pathBytes.size();
        out.writeRawData(reinterpret_cast<const char*>(&pathLen), sizeof(uint32_t));
        out.writeRawData(pathBytes.data(), pathLen);
        
        // Type
        QByteArray typeBytes = entity.type.toUtf8();
        uint32_t typeLen = typeBytes.size();
        out.writeRawData(reinterpret_cast<const char*>(&typeLen), sizeof(uint32_t));
        out.writeRawData(typeBytes.data(), typeLen);
    }
    
    /* Flush */
    out.device()->waitForBytesWritten(-1);
    file.flush();
    file.close();
    
    qDebug() << "Mapa guardado:" << mapData.sectors.size() << "sectores,"
             << mapData.portals.size() << "portales,"
             << mapData.entities.size() << "entidades";
    
    return true;
}
