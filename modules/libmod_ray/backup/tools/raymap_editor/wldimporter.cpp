#include "wldimporter.h"
#include <QFile>
#include <QDataStream>
#include <QDebug>
#include <QMap>
#include <cmath>
#include <QPolygonF>

/* ============================================================================
   MAIN IMPORT FUNCTION
   ============================================================================ */

bool WLDImporter::importWLD(const QString &filename, MapData &mapData) {
    qDebug() << "WLDImporter: Starting import of" << filename;
    
    // Read WLD file
    QVector<WLD_Point> points;
    QVector<WLD_Wall> walls;
    QVector<WLD_Region> regions;
    QVector<WLD_Flag> flags;
    
    if (!readWLDFile(filename, points, walls, regions, flags)) {
        qWarning() << "WLDImporter: Failed to read WLD file";
        return false;
    }
    
    qDebug() << "WLDImporter: Read" << points.size() << "points,"
             << walls.size() << "walls,"
             << regions.size() << "regions,"
             << flags.size() << "flags";
    
    // Clear existing map data
    mapData.sectors.clear();
    mapData.portals.clear();
    mapData.spawnFlags.clear();
    
    // Remap regions to ensure contiguous IDs and valid references
    remapRegions(regions, walls, flags);
    
    // Assign back_region to walls (portal detection)
    // assignWallRegions(walls);
    
    // Convert structures
    convertRegionsToSectors(points, walls, regions, mapData);
    detectPortals(points, walls, mapData);
    convertFlags(flags, mapData);
    
    qDebug() << "WLDImporter: Created" << mapData.sectors.size() << "sectors,"
             << mapData.portals.size() << "portals,"
             << mapData.spawnFlags.size() << "spawn flags";
    
    return true;
}

/* ============================================================================
   FILE READING
   ============================================================================ */

bool WLDImporter::readWLDFile(const QString &filename,
                              QVector<WLD_Point> &points,
                              QVector<WLD_Wall> &walls,
                              QVector<WLD_Region> &regions,
                              QVector<WLD_Flag> &flags) {
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "WLDImporter: Cannot open file" << filename;
        return false;
    }
    
    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);
    
    // Read and verify WLD magic header (8 bytes: "wld\x1a\x0d\x0a\x01\x00")
    char magic[9];
    in.readRawData(magic, 8);
    magic[8] = '\0';
    
    if (memcmp(magic, "wld\x1a\x0d\x0a\x01", 8) != 0) {
        qWarning() << "WLDImporter: Invalid WLD magic header";
        file.close();
        return false;
    }
    
    // Read total size (4 bytes)
    int32_t total_size;
    in >> total_size;
    
    qDebug() << "WLDImporter: Total size:" << total_size;
    
    // Skip metadata (548 bytes)
    file.seek(8 + 4 + 548);
    
    // Read POINTS (count + data)
    int32_t num_points;
    in >> num_points;
    
    qDebug() << "WLDImporter: Reading" << num_points << "points";
    
    if (num_points < 0 || num_points > 8192) {
        qWarning() << "WLDImporter: Invalid point count:" << num_points;
        file.close();
        return false;
    }
    
    points.resize(num_points);
    for (int i = 0; i < num_points; i++) {
        in >> points[i].active >> points[i].x >> points[i].y >> points[i].links;
    }
    
    // Normalize coordinates: find min coordinates and shift everything to near (0,0)
    // This fixes floating point precision issues (gaps between walls) caused by large coordinates
    float minX = 1000000.0f;
    float minZ = 1000000.0f;
    bool hasActivePoints = false;
    
    for (int i = 0; i < num_points; i++) {
        // WlD format uses X,Y for 2D plane (which maps to X,Z in 3D)
        // Check active flag, although we might want to normalize all just in case
        if (points[i].x < minX) minX = points[i].x;
        if (points[i].y < minZ) minZ = points[i].y;
        hasActivePoints = true;
    }
    
    if (hasActivePoints && (minX > 100.0f || minZ > 100.0f)) {
        qDebug() << "WLDImporter: Normalizing map coordinates. Shift by X=" << -minX << "Z=" << -minZ;
        for (int i = 0; i < num_points; i++) {
            points[i].x -= minX;
            points[i].y -= minZ;
        }
    }
    
    // Read WALLS (count + data)
    int32_t num_walls;
    in >> num_walls;
    
    qDebug() << "WLDImporter: Reading" << num_walls << "walls";
    
    if (num_walls < 0 || num_walls > 8192) {
        qWarning() << "WLDImporter: Invalid wall count:" << num_walls;
        file.close();
        return false;
    }
    
    walls.resize(num_walls);
    for (int i = 0; i < num_walls; i++) {
        in >> walls[i].active >> walls[i].type
           >> walls[i].p1 >> walls[i].p2
           >> walls[i].front_region >> walls[i].back_region
           >> walls[i].texture >> walls[i].texture_top >> walls[i].texture_bot
           >> walls[i].fade;
        
        // Mark wall as active if it has valid data
        // The 'active' field from file is not reliable (like regions)
        if (walls[i].p1 >= 0 && walls[i].p2 >= 0 && walls[i].front_region >= 0) {
            walls[i].active = 1;
        } else {
            walls[i].active = 0;
        }
    }
    
    // Fix connectivity: find missing portal links based on shared geometry
    fixConnectivity(walls, points, regions);
    
    // Optimize walls: merge collinear segments
    // DISABLED: Causes geometry gaps (missing lines) in some maps.
    // Correctness is improved by keeping original splits.
    // optimizeWalls(walls, points, num_walls);
    
    // Debug: Show first few walls as read from file
    qDebug() << "WLDImporter: First 10 active walls from file (after optimization):";
    int shown = 0;
    for (int i = 0; i < walls.size() && shown < 10; i++) {
        if (!walls[i].active) continue;
        qDebug() << "  Wall" << i << "p1:" << walls[i].p1 << "p2:" << walls[i].p2
                 << "front_region:" << walls[i].front_region
                 << "back_region:" << walls[i].back_region;
        shown++;
    }

    
    // Read REGIONS (count + data)
    int32_t num_regions;
    in >> num_regions;
    
    qDebug() << "WLDImporter: Reading" << num_regions << "regions";
    
    if (num_regions < 0 || num_regions > 4096) {
        qWarning() << "WLDImporter: Invalid region count:" << num_regions;
        file.close();
        return false;
    }
    
    regions.resize(num_regions);
    for (int i = 0; i < num_regions; i++) {
        in >> regions[i].active >> regions[i].type
           >> regions[i].floor_height >> regions[i].ceil_height
           >> regions[i].floor_tex >> regions[i].ceil_tex
           >> regions[i].fade;
           
        // Mark region as active if it has valid heights (not -1, -1)
        // The 'active' field from file is not reliable, so we override it
        if (regions[i].floor_height == -1 && regions[i].ceil_height == -1) {
            regions[i].active = 0;  // Invalid region
        } else {
            regions[i].active = 1;  // Valid region
        }
    }
    
    // Read FLAGS (count + data)
    int32_t num_flags;
    in >> num_flags;
    
    qDebug() << "WLDImporter: Reading" << num_flags << "flags";
    
    if (num_flags < 0 || num_flags > 1000) {
        qWarning() << "WLDImporter: Invalid flag count:" << num_flags;
        file.close();
        return false;
    }
    
    flags.resize(num_flags);
    for (int i = 0; i < num_flags; i++) {
        in >> flags[i].sector_idx
           >> flags[i].min_distance >> flags[i].max_distance
           >> flags[i].visible
           >> flags[i].x >> flags[i].y
           >> flags[i].number;
    }
    
    qDebug() << "WLDImporter: Successfully read WLD file";
    
    file.close();
    return true;
}

/* ============================================================================
   WALL REGION ASSIGNMENT (PORTAL DETECTION)
   ============================================================================ */

void WLDImporter::assignWallRegions(QVector<WLD_Wall> &walls) {
    qDebug() << "WLDImporter: Assigning back_region to walls (portal detection)";
    
    // Based on map_asignregions() from original DIV editor
    
    // Create wall table with IDs
    struct WallInfo {
        int64_t id;      // Unique ID based on vertices
        int tipo;        // 0 or 1 based on vertex order
        int front;       // Front region
        int inside;      // Region type (not used in import, set to 0)
        int n;           // Number of walls sharing same vertices
    };
    
    QVector<WallInfo> wallInfo(walls.size());
    
    // Step 1: Create wall IDs and count shared vertices
    for (int i = 0; i < walls.size(); i++) {
        if (!walls[i].active) continue;
        
        // Create unique ID based on vertices
        if (walls[i].p1 > walls[i].p2) {
            wallInfo[i].id = (int64_t)walls[i].p1 * 32000 + walls[i].p2;
            wallInfo[i].tipo = 0;
        } else {
            wallInfo[i].id = (int64_t)walls[i].p2 * 32000 + walls[i].p1;
            wallInfo[i].tipo = 1;
        }
        
        wallInfo[i].front = walls[i].front_region;
        wallInfo[i].inside = 0;  // Not used in import
        wallInfo[i].n = 0;
    }
    
    // Count walls sharing same vertices
    for (int i = 0; i < walls.size(); i++) {
        if (!walls[i].active) continue;
        for (int j = i + 1; j < walls.size(); j++) {
            if (!walls[j].active) continue;
            if (wallInfo[i].id == wallInfo[j].id) {
                wallInfo[i].n++;
                wallInfo[j].n++;
            }
        }
    }
    
    // Step 2: Assign back_regions
    int portals_found = 0;
    
    for (int i = 0; i < walls.size(); i++) {
        if (!walls[i].active) continue;
        
        // Initialize
        walls[i].texture_top = 0;
        walls[i].texture_bot = 0;
        walls[i].back_region = -1;
        walls[i].type = 2;  // Solid wall by default
        
        // If wall shares vertices with other walls
        if (wallInfo[i].n > 0) {
            for (int j = 0; j < walls.size(); j++) {
                if (j == i || !walls[j].active) continue;
                
                if (wallInfo[i].id == wallInfo[j].id) {
                    // Same vertices, opposite orientation = portal
                    if (wallInfo[i].tipo != wallInfo[j].tipo) {
                        walls[i].back_region = wallInfo[j].front;
                        walls[i].type = 1;  // Portal
                        
                        // Set textures for portal
                        walls[i].texture_top = walls[i].texture;
                        walls[i].texture_bot = walls[i].texture;
                        
                        portals_found++;
                        break;  // Found portal, stop searching
                    }
                }
            }
        }
        // Note: We skip the "no shared vertices" case (map_findregion2)
        // because it requires point-in-polygon testing which is complex
        // Most portals should be detected by shared vertices
    }
    
    // Update front_region from wallInfo (like original does at the end)
    for (int i = 0; i < walls.size(); i++) {
        if (!walls[i].active) continue;
        walls[i].front_region = wallInfo[i].front;
    }
    
    // Debug: Show first few walls
    qDebug() << "WLDImporter: First 10 walls after assignment:";
    for (int i = 0; i < qMin(10, walls.size()); i++) {
        if (!walls[i].active) continue;
        qDebug() << "  Wall" << i << "- p1:" << walls[i].p1 << "p2:" << walls[i].p2
                 << "front:" << walls[i].front_region << "back:" << walls[i].back_region
                 << "id:" << wallInfo[i].id << "tipo:" << wallInfo[i].tipo << "n:" << wallInfo[i].n;
    }
    
    qDebug() << "WLDImporter: Found" << portals_found << "portals";
}

/* ============================================================================
   REGION TO SECTOR CONVERSION
   ============================================================================ */

void WLDImporter::convertRegionsToSectors(const QVector<WLD_Point> &points,
                                         const QVector<WLD_Wall> &walls,
                                         const QVector<WLD_Region> &regions,
                                         MapData &mapData) {
    int skipped_inactive = 0;
    int skipped_no_walls = 0;
    int skipped_invalid_polygon = 0;
    int created = 0;
    
    for (int r = 0; r < regions.size(); r++) {
        const WLD_Region &wldRegion = regions[r];
        
        // Debug first few regions
        if (r < 5) {
            qDebug() << "WLDImporter: Region" << r << "- active:" << wldRegion.active
                     << "floor:" << wldRegion.floor_height << "ceil:" << wldRegion.ceil_height;
        }
        
        // Skip inactive regions
        if (!wldRegion.active) {
            skipped_inactive++;
            continue;
        }
        
        // Find all walls belonging to this region
        QVector<const WLD_Wall*> regionWalls;
        for (int w = 0; w < walls.size(); w++) {
            const WLD_Wall &wall = walls[w];
            if (!wall.active) continue;
            
            // Debug first few walls for first region
            if (r == 0 && w < 10) {
                qDebug() << "  Wall" << w << "- active:" << wall.active 
                         << "front_region:" << wall.front_region 
                         << "back_region:" << wall.back_region;
            }
            
            // Wall belongs to region if front_region or back_region matches
            if (wall.front_region == r || wall.back_region == r) {
                regionWalls.append(&wall);
            }
        }
        
        if (r < 5) {
            qDebug() << "  Region" << r << "has" << regionWalls.size() << "walls";
        }
        
        if (regionWalls.isEmpty()) {
            if (r < 5) {
                qWarning() << "WLDImporter: Region" << r << "has no walls, skipping";
            }
            skipped_no_walls++;
            continue;
        }
        
        // Build polygon from walls
        QVector<QPointF> vertices = buildSectorPolygon(regionWalls, points);
        
        if (vertices.size() < 3) {
            qWarning() << "WLDImporter: Region" << r << "has invalid polygon (" << vertices.size() << "vertices), skipping";
            skipped_invalid_polygon++;
            continue;
        }
        
        created++;
        
        // Create sector
        Sector sector;
        sector.sector_id = r;
        sector.vertices = vertices;
        sector.floor_z = (float)wldRegion.floor_height;
        sector.ceiling_z = (float)wldRegion.ceil_height;
        sector.floor_texture_id = wldRegion.floor_tex;
        sector.ceiling_texture_id = wldRegion.ceil_tex;
        sector.light_level = 255;
        
        // Initialize nested sector fields (WLD doesn't have nesting)
        sector.parent_sector_id = -1;  // Root sector
        sector.sector_type = Sector::ROOT;
        sector.is_solid = false;
        
        // Create walls for this sector
        // Create walls for this sector from WLD walls
        // This ensures we include islands/holes/pillars that are not part of the main polygon perimeter
        int wall_id = 0;
        
        for (const WLD_Wall *wldWall : regionWalls) {
            Wall wall;
            wall.wall_id = wall_id++;
            
            const WLD_Point &p1 = points[wldWall->p1];
            const WLD_Point &p2 = points[wldWall->p2];
            
            // Determine orientation based on which side this sector is on
            if (wldWall->front_region == r) {
                // Front face belongs to this sector: p1 -> p2
                wall.x1 = (float)p1.x;
                wall.y1 = (float)p1.y;
                wall.x2 = (float)p2.x;
                wall.y2 = (float)p2.y;
            } 
            else if (wldWall->back_region == r) {
                // Back face belongs to this sector: p2 -> p1 (winding must be reversed for raycaster)
                wall.x1 = (float)p2.x;
                wall.y1 = (float)p2.y;
                wall.x2 = (float)p1.x;
                wall.y2 = (float)p1.y;
            } 
            else {
                // Should not happen given how regionWalls is populated
                continue;
            }
            
            // Textures
            wall.texture_id_middle = wldWall->texture;
            wall.texture_id_upper = wldWall->texture_top;
            wall.texture_id_lower = wldWall->texture_bot;

            
            // Initialize portal_id to -1 (solid). 
            // Valid portals will be assigned in detectPortals() later.
            wall.portal_id = -1;
            
            // Default split values
            wall.texture_split_z_lower = sector.floor_z;
            wall.texture_split_z_upper = sector.ceiling_z;

            sector.walls.append(wall);
        }
        
        mapData.sectors.append(sector);
    }
    
    // Calculate nested sectors based on portal relationships and wall counts
    // The region with MORE walls is the parent, the one with FEWER walls is the nested child
    QMap<QPair<int, int>, bool> processedPairs;
    
    for (const Portal &portal : mapData.portals) {
        int sectorA = portal.sector_a;
        int sectorB = portal.sector_b;
        
        // Avoid processing the same pair twice
        QPair<int, int> pair1(sectorA, sectorB);
        QPair<int, int> pair2(sectorB, sectorA);
        if (processedPairs.contains(pair1) || processedPairs.contains(pair2)) {
            continue;
        }
        processedPairs[pair1] = true;
        
        // Find both sectors
        Sector *secA = nullptr;
        Sector *secB = nullptr;
        for (Sector &s : mapData.sectors) {
            if (s.sector_id == sectorA) secA = &s;
            if (s.sector_id == sectorB) secB = &s;
        }
        
        if (!secA || !secB) continue;
        
        // Determine parent/child based on wall count
        int wallsA = secA->walls.size();
        int wallsB = secB->walls.size();
        
        Sector *parent = nullptr;
        Sector *child = nullptr;
        
        if (wallsA > wallsB) {
            parent = secA;
            child = secB;
        } else if (wallsB > wallsA) {
            parent = secB;
            child = secA;
        } else {
            // Same number of walls, use sectorB as parent (matches WLD logic)
            parent = secB;
            child = secA;
        }
        
        // Set nested relationship
        child->parent_sector_id = parent->sector_id;
        child->sector_type = Sector::NESTED_ROOM;
        // Parent remains as ROOT (already set during initialization)
    }
    
    qDebug() << "WLDImporter: Calculated nested sector relationships";
    
    // Second pass: Calculate wall height splits for portals
    for (Sector &sector : mapData.sectors) {
        for (Wall &wall : sector.walls) {
            if (wall.portal_id >= 0 && wall.portal_id < mapData.portals.size()) {
                // This is a portal wall - calculate height splits
                const Portal &portal = mapData.portals[wall.portal_id];
                
                // Find the neighbor sector (the one on the other side of the portal)
                int neighbor_id = (portal.sector_a == sector.sector_id) ? portal.sector_b : portal.sector_a;
                
                // Find neighbor sector object
                Sector *neighbor = nullptr;
                for (Sector &s : mapData.sectors) {
                    if (s.sector_id == neighbor_id) {
                        neighbor = &s;
                        break;
                    }
                }
                
                if (neighbor) {
                    // For portals, the splits are where the neighbor sector's floor/ceiling differ
                    // Lower split = max of the two floors (where lower texture ends)
                    // Upper split = min of the two ceilings (where upper texture starts)
                    wall.texture_split_z_lower = qMax(sector.floor_z, neighbor->floor_z);
                    wall.texture_split_z_upper = qMin(sector.ceiling_z, neighbor->ceiling_z);
                } else {
                    // Fallback if neighbor not found
                    wall.texture_split_z_lower = sector.floor_z;
                    wall.texture_split_z_upper = sector.ceiling_z;
                }
            } else {
                // Solid wall - spans from floor to ceiling
                wall.texture_split_z_lower = sector.floor_z;
                wall.texture_split_z_upper = sector.ceiling_z;
            }
        }
    }
    
    qDebug() << "WLDImporter: Conversion summary -"
             << "Created:" << created
             << "Skipped inactive:" << skipped_inactive
             << "Skipped no walls:" << skipped_no_walls
             << "Skipped invalid polygon:" << skipped_invalid_polygon;
}

/* ============================================================================
   POLYGON BUILDING ALGORITHM
   ============================================================================ */

QVector<QPointF> WLDImporter::buildSectorPolygon(const QVector<const WLD_Wall*> &regionWalls,
                                                 const QVector<WLD_Point> &points) {
    QVector<QPointF> vertices;
    
    if (regionWalls.isEmpty()) {
        return vertices;
    }
    
    // Build connectivity map: point_index -> list of connected walls
    QMap<int32_t, QVector<const WLD_Wall*>> pointToWalls;
    
    for (const WLD_Wall *wall : regionWalls) {
        pointToWalls[wall->p1].append(wall);
        pointToWalls[wall->p2].append(wall);
    }
    
    // Start with first wall
    QVector<const WLD_Wall*> orderedWalls;
    QSet<const WLD_Wall*> usedWalls;
    
    const WLD_Wall *currentWall = regionWalls[0];
    orderedWalls.append(currentWall);
    usedWalls.insert(currentWall);
    
    int32_t currentPoint = currentWall->p2;
    int32_t startPoint = currentWall->p1;
    
    // Follow the chain of connected walls
    int maxIterations = regionWalls.size() * 2; // Safety limit
    int iterations = 0;
    
    while (currentPoint != startPoint && iterations < maxIterations) {
        iterations++;
        
        // Find next wall connected to currentPoint
        const WLD_Wall *nextWall = nullptr;
        
        for (const WLD_Wall *wall : pointToWalls[currentPoint]) {
            if (usedWalls.contains(wall)) continue;
            
            // This wall connects to currentPoint
            nextWall = wall;
            break;
        }
        
        if (!nextWall) {
            qWarning() << "WLDImporter: Cannot find next wall at point" << currentPoint;
            break;
        }
        
        orderedWalls.append(nextWall);
        usedWalls.insert(nextWall);
        
        // Move to next point
        if (nextWall->p1 == currentPoint) {
            currentPoint = nextWall->p2;
        } else {
            currentPoint = nextWall->p1;
        }
    }
    
    // Extract vertices from ordered walls
    for (const WLD_Wall *wall : orderedWalls) {
        if (wall->p1 < 0 || wall->p1 >= points.size()) continue;
        
        const WLD_Point &pt = points[wall->p1];
        vertices.append(QPointF((float)pt.x, (float)pt.y));
    }
    
    // Remove duplicate last vertex if it equals first
    if (vertices.size() > 1) {
        QPointF first = vertices.first();
        QPointF last = vertices.last();
        if (qAbs(first.x() - last.x()) < 0.1f && qAbs(first.y() - last.y()) < 0.1f) {
            vertices.removeLast();
        }
    }
    
    qDebug() << "WLDImporter: Built polygon with" << vertices.size() << "vertices";
    
    return vertices;
}

/* ============================================================================
   PORTAL DETECTION
   ============================================================================ */

void WLDImporter::detectPortals(const QVector<WLD_Point> &points,
                                const QVector<WLD_Wall> &walls,
                                MapData &mapData) {
    qDebug() << "WLDImporter: Detecting portals and assigning to sector walls";
    
    // First pass: Create portal structures and map WLD walls to portal IDs
    QMap<int, int> wldWallIndexToPortalId;  // WLD wall index -> portal ID
    int portal_id = 0;
    
    for (int w = 0; w < walls.size(); w++) {
        const WLD_Wall &wall = walls[w];
        
        if (!wall.active) continue;
        
        // Check if this is a portal wall
        if (wall.front_region >= 0 && wall.back_region >= 0 &&
            wall.front_region != wall.back_region) {
            
            // Create portal
            Portal portal;
            portal.portal_id = portal_id;
            portal.sector_a = wall.front_region;
            portal.sector_b = wall.back_region;
            
            mapData.portals.append(portal);
            wldWallIndexToPortalId[w] = portal_id;
            
            portal_id++;
        }
    }
    
    qDebug() << "WLDImporter: Created" << mapData.portals.size() << "portal structures";
    
    // Second pass: Assign portal IDs to sector walls by matching coordinates
    int portals_assigned = 0;
    
    for (Sector &sector : mapData.sectors) {
        for (Wall &sectorWall : sector.walls) {
            // Try to find matching WLD wall by coordinates
            for (int w = 0; w < walls.size(); w++) {
                if (!wldWallIndexToPortalId.contains(w)) continue;
                
                const WLD_Wall &wldWall = walls[w];
                
                // Check if this WLD wall belongs to current sector
                if (wldWall.front_region != sector.sector_id &&
                    wldWall.back_region != sector.sector_id) {
                    continue;
                }
                
                // Get WLD wall coordinates
                if (wldWall.p1 < 0 || wldWall.p1 >= points.size() ||
                    wldWall.p2 < 0 || wldWall.p2 >= points.size()) {
                    continue;
                }
                
                const WLD_Point &wldP1 = points[wldWall.p1];
                const WLD_Point &wldP2 = points[wldWall.p2];
                
                // Match by coordinates (check both directions)
                bool matches = (pointsEqual(wldP1.x, wldP1.y, (int32_t)sectorWall.x1, (int32_t)sectorWall.y1) &&
                               pointsEqual(wldP2.x, wldP2.y, (int32_t)sectorWall.x2, (int32_t)sectorWall.y2)) ||
                              (pointsEqual(wldP2.x, wldP2.y, (int32_t)sectorWall.x1, (int32_t)sectorWall.y1) &&
                               pointsEqual(wldP1.x, wldP1.y, (int32_t)sectorWall.x2, (int32_t)sectorWall.y2));
                
                if (matches) {
                    sectorWall.portal_id = wldWallIndexToPortalId[w];
                    
                    if (!sector.portal_ids.contains(wldWallIndexToPortalId[w])) {
                        sector.portal_ids.append(wldWallIndexToPortalId[w]);
                    }
                    
                    portals_assigned++;
                    break;  // Found match, move to next sector wall
                }
            }
        }
    }
    
    qDebug() << "WLDImporter: Assigned" << portals_assigned << "portal IDs to sector walls";
    
    // Third pass: Fix textures for portal walls (match original WLD behavior)
    for (Sector &sector : mapData.sectors) {
        for (Wall &wall : sector.walls) {
            if (wall.portal_id >= 0) {
                // This is a portal - copy middle texture to upper/lower, then make middle transparent
                
                // Copy middle to upper and lower
                wall.texture_id_upper = wall.texture_id_middle;
                wall.texture_id_lower = wall.texture_id_middle;
                
                // If texture is still 0, try to find one from other walls in same sector
                if (wall.texture_id_upper <= 0) {
                    for (const Wall &otherWall : sector.walls) {
                        if (otherWall.texture_id_middle > 0) {
                            wall.texture_id_upper = otherWall.texture_id_middle;
                            wall.texture_id_lower = otherWall.texture_id_middle;
                            break;
                        }
                    }
                }
                
                // If still 0, use sector's ceiling/floor textures as fallback
                if (wall.texture_id_upper <= 0) {
                    if (sector.ceiling_texture_id > 0) {
                        wall.texture_id_upper = sector.ceiling_texture_id;
                    }
                    if (sector.floor_texture_id > 0) {
                        wall.texture_id_lower = sector.floor_texture_id;
                    }
                }
                
                // Ensure lower has something if upper has something
                if (wall.texture_id_lower <= 0 && wall.texture_id_upper > 0) {
                    wall.texture_id_lower = wall.texture_id_upper;
                }
                
                // Make middle transparent (the portal opening)
                wall.texture_id_middle = 0;
            }
        }
    }
    
    qDebug() << "WLDImporter: Fixed portal textures";
}

/* ============================================================================
   FLAG CONVERSION
   ============================================================================ */

void WLDImporter::convertFlags(const QVector<WLD_Flag> &wldFlags, MapData &mapData) {
    for (const WLD_Flag &wldFlag : wldFlags) {
        if (!wldFlag.visible) continue;
        
        SpawnFlag flag;
        flag.flagId = wldFlag.number;
        flag.x = (float)wldFlag.x;
        flag.y = (float)wldFlag.y;
        flag.z = 0.0f; // WLD doesn't have Z for flags
        
        mapData.spawnFlags.append(flag);
    }
    
    qDebug() << "WLDImporter: Converted" << mapData.spawnFlags.size() << "spawn flags";
}

// Helper to optimize walls by merging collinear segments
void WLDImporter::optimizeWalls(QVector<WLD_Wall> &walls, const QVector<WLD_Point> &points, int32_t &num_walls)
{
    qDebug() << "WLDImporter: Optimizing walls... Initial active count:" << num_walls;
    int mergedCount = 0;
    
    // Group active walls by front_region for faster processing
    QMap<int, QVector<int>> regionWalls;
    for(int i = 0; i < walls.size(); i++) {
        if(walls[i].active && walls[i].front_region >= 0) {
            regionWalls[walls[i].front_region].append(i);
        }
    }
    
    // Process each region
    // Using iterators to avoid copying vectors
    QMapIterator<int, QVector<int>> i(regionWalls);
    while (i.hasNext()) {
        i.next();
        const QVector<int> &rw = i.value();
        
        bool changed = true;
        
        // Multi-pass merging until no more merges found
        while(changed) {
            changed = false;
            
            for(int idx1_pos = 0; idx1_pos < rw.size(); idx1_pos++) {
                int idx1 = rw[idx1_pos];
                if(!walls[idx1].active) continue;
                
                // Find wall starting where wall1 ends (p2 -> p1 of next wall)
                for(int idx2_pos = 0; idx2_pos < rw.size(); idx2_pos++) {
                    int idx2 = rw[idx2_pos];
                    if(idx1 == idx2 || !walls[idx2].active) continue;
                    
                    // Check physical connection: w1.p2 == w2.p1
                    if(walls[idx1].p2 == walls[idx2].p1) {
                         // Check properties match
                         if(walls[idx1].type != walls[idx2].type ||
                            walls[idx1].back_region != walls[idx2].back_region ||
                            walls[idx1].texture != walls[idx2].texture ||
                            walls[idx1].texture_top != walls[idx2].texture_top ||
                            walls[idx1].texture_bot != walls[idx2].texture_bot ||
                            walls[idx1].fade != walls[idx2].fade) {
                             continue;
                         }
                         
                         // Check collinearity
                         const WLD_Point &p1 = points[walls[idx1].p1];
                         const WLD_Point &p2 = points[walls[idx1].p2]; // Shared point
                         const WLD_Point &p3 = points[walls[idx2].p2];
                         
                         // Vector 1
                         float dx1 = (float)p2.x - (float)p1.x;
                         float dy1 = (float)p2.y - (float)p1.y;
                         
                         // Vector 2
                         float dx2 = (float)p3.x - (float)p2.x;
                         float dy2 = (float)p3.y - (float)p2.y;
                         
                         // Cross product (2D)
                         float cross = dx1 * dy2 - dy1 * dx2;
                         
                         // Dot product to ensure direction is same (not folding back)
                         float dot = dx1 * dx2 + dy1 * dy2;
                         
                         // If cross product near zero and dot product positive => collinear
                         // Use substantial tolerance because WLD coordinates can be large integers but effective grid is small
                         if(std::abs(cross) < 1000.0f && dot > 0) {
                             // MERGE!
                             // Extend wall1 to cover wall2
                             walls[idx1].p2 = walls[idx2].p2;
                             
                             // Deactivate wall2
                             walls[idx2].active = 0;
                             
                             mergedCount++;
                             changed = true;
                             break;
                         }
                    }
                }
                if(changed) break;
            }
        }
    }
    
    // Update active count
    int activeCount = 0;
    for(const WLD_Wall &w : walls) {
        if(w.active) activeCount++;
    }
    num_walls = activeCount; // Update the reference
    
    qDebug() << "WLDImporter: Optimization complete. Merged" << mergedCount << "walls. Final active:" << num_walls;
}

void WLDImporter::remapRegions(QVector<WLD_Region> &regions, QVector<WLD_Wall> &walls, QVector<WLD_Flag> &flags) {
    QVector<WLD_Region> activeRegions;
    QMap<int, int> idMap;
    int nextId = 0;

    // 1. Filter active regions and build map
    for(int i=0; i<regions.size(); i++) {
        if(regions[i].active) {
            idMap[i] = nextId;
            activeRegions.append(regions[i]);
            nextId++;
        }
    }
    
    qDebug() << "WLDImporter: Remapping" << regions.size() << "regions to" << activeRegions.size() << "active sectors.";

    // 2. Update walls
    for(int i=0; i<walls.size(); i++) {
        // Remap front
        if(walls[i].front_region >= 0) {
            if(idMap.contains(walls[i].front_region)) {
                walls[i].front_region = idMap[walls[i].front_region];
            } else {
                walls[i].front_region = -1; // Invalid/Deleted
            }
        }
        
        // Remap back
        if(walls[i].back_region >= 0) {
            if(idMap.contains(walls[i].back_region)) {
                walls[i].back_region = idMap[walls[i].back_region];
            } else {
                walls[i].back_region = -1; // Invalid/Deleted -> Solid wall
            }
        }
    }

    // 3. Update flags
    for(int i=0; i<flags.size(); i++) {
        if(flags[i].sector_idx >= 0) {
            if(idMap.contains(flags[i].sector_idx)) {
                flags[i].sector_idx = idMap[flags[i].sector_idx];
            } else {
                flags[i].sector_idx = -1; 
            }
        }
    }

    // 4. Replace regions vector
    regions = activeRegions;
}

// Helper to fix missing back_region links based on shared geometry
void WLDImporter::fixConnectivity(QVector<WLD_Wall> &walls, const QVector<WLD_Point> &points, const QVector<WLD_Region> &regions) {
    qDebug() << "WLDImporter: Fixing connectivity (Hybrid: Geometric + Coincident)...";
    int fixedCoincident = 0;
    int fixedGeometric = 0;
    
    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------
    // PHASE 1: Group Walls by Region (Optimization)
    // -------------------------------------------------------------------------
    QVector<QVector<const WLD_Wall*>> regionWallsMap(regions.size());
    for(const WLD_Wall &w : walls) {
        if(w.active && w.front_region >= 0 && w.front_region < regions.size()) {
            regionWallsMap[w.front_region].append(&w);
        }
    }
    
    // Helper Lambda for Point-In-Region (Ray-Casting)
    auto isPointInRegion = [&](int regionIdx, float px, float py) -> bool {
        if(regionIdx < 0 || regionIdx >= regionWallsMap.size()) return false;
        const auto &rWalls = regionWallsMap[regionIdx];
        if(rWalls.isEmpty()) return false;
        
        bool inside = false;
        for(const WLD_Wall *w : rWalls) {
            float x1 = points[w->p1].x;
            float y1 = points[w->p1].y;
            float x2 = points[w->p2].x;
            float y2 = points[w->p2].y;
            
            if (((y1 > py) != (y2 > py)) &&
                (px < (x2 - x1) * (py - y1) / (float)(y2 - y1) + x1)) {
                inside = !inside;
            }
        }
        return inside;
    };

    // -------------------------------------------------------------------------
    // PHASE 2: Coincident Walls (Vertex/Coordinate Match)
    // -------------------------------------------------------------------------
    const float EPSILON_SQ = 1.0f; // 1 unit tolerance
    
    for (int i = 0; i < walls.size(); i++) {
        if (!walls[i].active) continue;
        
        WLD_Point p1_i = points[walls[i].p1];
        WLD_Point p2_i = points[walls[i].p2];
        
        for (int j = i + 1; j < walls.size(); j++) {
            if (!walls[j].active) continue;
            // Optimization: Skip if both already have back_region
            if (walls[i].back_region != -1 && walls[j].back_region != -1) continue;
            
            WLD_Point p1_j = points[walls[j].p1];
            WLD_Point p2_j = points[walls[j].p2];
            
            bool matchNormal = (pow(p1_i.x - p1_j.x, 2) + pow(p1_i.y - p1_j.y, 2) < EPSILON_SQ) &&
                               (pow(p2_i.x - p2_j.x, 2) + pow(p2_i.y - p2_j.y, 2) < EPSILON_SQ);
                               
            bool matchReversed = (pow(p1_i.x - p2_j.x, 2) + pow(p1_i.y - p2_j.y, 2) < EPSILON_SQ) &&
                                 (pow(p2_i.x - p1_j.x, 2) + pow(p2_i.y - p1_j.y, 2) < EPSILON_SQ);
            
            if (matchNormal || matchReversed) {
                if (walls[i].front_region != walls[j].front_region) {
                    if (walls[i].back_region == -1) {
                        walls[i].back_region = walls[j].front_region;
                        fixedCoincident++;
                    }
                    if (walls[j].back_region == -1) {
                        walls[j].back_region = walls[i].front_region;
                        fixedCoincident++;
                    }
                }
            }
        }
    }
    
    // -------------------------------------------------------------------------
    // PHASE 3: Geometric Region Search for Orphans
    // -------------------------------------------------------------------------
    // Use conservative probe distances.
    // 0.1: Touching walls.
    // 4.0: Standard thick walls.
    // Large values avoided to prevent linking through empty space (false portals).
    QVector<float> probeDistances;
    probeDistances << 0.1f << 1.0f << 4.0f;
    
    int orphansChecked = 0;
    
    for(int i=0; i<walls.size(); i++) {
        if(!walls[i].active) continue;
        if(walls[i].back_region != -1) continue; // Already linked
        
        orphansChecked++;
        
        WLD_Point p1 = points[walls[i].p1];
        WLD_Point p2 = points[walls[i].p2];
        float midX = (p1.x + p2.x) * 0.5f;
        float midY = (p1.y + p2.y) * 0.5f;
        
        float dx = p2.x - p1.x;
        float dy = p2.y - p1.y;
        float len = sqrt(dx*dx + dy*dy);
        if(len < 0.1f) continue;
        
        // Normal (-dy, dx) - Normalized
        float nx = -dy / len;
        float ny = dx / len;
        
        bool found = false;
        
        // Try each probe distance
        for(float dist : probeDistances) {
            float probeX = midX + nx * dist;
            float probeY = midY + ny * dist;
            
            float backX1 = midX - nx * dist;
            float backY1 = midY - ny * dist;
            float backX2 = midX + nx * dist;
            float backY2 = midY + ny * dist;
            
            // Heuristic: Check active regions only
            for(int r=0; r<regions.size(); r++) {
                if(!regions[r].active) continue;
                if(r == walls[i].front_region) continue;
                
                // Check Midpoint (Nested) check first (once)
                bool checkMid = (dist == 1.0f) && isPointInRegion(r, midX, midY);
                
                if(checkMid || 
                   isPointInRegion(r, backX1, backY1) || 
                   isPointInRegion(r, backX2, backY2)) {
                    
                    walls[i].back_region = r;
                    walls[i].type = 1; 
                    fixedGeometric++;
                    found = true;
                    // qDebug() << "Fixed Orphan Wall" << i << "linked to Region" << r << "at dist" << dist;
                    break;
                }
            }
            if(found) break; // Stop trying distances if found
        }
    }
    
    qDebug() << "WLDImporter: Connectivity Check:" << orphansChecked << "orphans checked.";
    qDebug() << "WLDImporter: Fixed" << fixedCoincident << "coincident links and" << fixedGeometric << "geometric links.";
}

