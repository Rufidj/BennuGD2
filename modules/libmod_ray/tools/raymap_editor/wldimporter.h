#ifndef WLDIMPORTER_H
#define WLDIMPORTER_H

#include <QString>
#include <QVector>
#include <QPointF>
#include <cstdint>
#include "mapdata.h"

/* ============================================================================
   WLD FORMAT STRUCTURES
   ============================================================================ */

#pragma pack(push, 1)

struct WLD_Point {
    int32_t active;
    int32_t x, y;
    int32_t links;
};

struct WLD_Wall {
    int32_t active;
    int32_t type;
    int32_t p1, p2;              // Indices to WLD_Point
    int32_t front_region, back_region;
    int32_t texture;
    int32_t texture_top;
    int32_t texture_bot;
    int32_t fade;
};

struct WLD_Region {
    int32_t active;
    int32_t type;
    int32_t floor_height, ceil_height;
    int32_t floor_tex;
    int32_t ceil_tex;
    int32_t fade;
};

struct WLD_Flag {
    int32_t sector_idx;
    float min_distance;
    float max_distance;
    int32_t visible;
    int32_t x, y;
    int32_t number;
};

#pragma pack(pop)

/* ============================================================================
   WLD IMPORTER CLASS
   ============================================================================ */

class WLDImporter {
public:
    /**
     * Main entry point: Import a WLD file into MapData
     * @param filename Path to .wld file
     * @param mapData MapData structure to populate
     * @return true on success, false on failure
     */
    static bool importWLD(const QString &filename, MapData &mapData);

private:
    /**
     * Read WLD file and parse structures
     */
    static bool readWLDFile(const QString &filename,
                           QVector<WLD_Point> &points,
                           QVector<WLD_Wall> &walls,
                           QVector<WLD_Region> &regions,
                           QVector<WLD_Flag> &flags);
    
    static void remapRegions(QVector<WLD_Region> &regions, QVector<WLD_Wall> &walls, QVector<WLD_Flag> &flags);
    
    /**
     * Fix connectivity by finding back-to-back walls and assigning back_region
     */
    static void fixConnectivity(QVector<WLD_Wall> &walls, const QVector<WLD_Point> &points, const QVector<WLD_Region> &regions);

    /**
     * Merge collinear walls to optimize geometry
     */
    static void optimizeWalls(QVector<WLD_Wall> &walls, const QVector<WLD_Point> &points, int32_t &num_walls);
    
    /**
     * Convert WLD regions to RayMap sectors
     */
    static void convertRegionsToSectors(const QVector<WLD_Point> &points,
                                       const QVector<WLD_Wall> &walls,
                                       const QVector<WLD_Region> &regions,
                                       MapData &mapData);
    
    /**
     * Build a closed polygon from region walls
     * @param regionWalls Walls belonging to this region
     * @param points All points in the map
     * @return Ordered vertices forming a closed polygon
     */
    static QVector<QPointF> buildSectorPolygon(const QVector<const WLD_Wall*> &regionWalls,
                                               const QVector<WLD_Point> &points);
    
    /**
     * Assign back_region to walls (portal detection)
     * Based on wld_assign_regions_simple from original WLD loader
     */
    static void assignWallRegions(QVector<WLD_Wall> &walls);
    
    /**
     * Detect and create portals from walls
     */
    static void detectPortals(const QVector<WLD_Point> &points,
                             const QVector<WLD_Wall> &walls,
                             MapData &mapData);
    
    /**
     * Convert WLD flags to spawn flags
     */
    static void convertFlags(const QVector<WLD_Flag> &wldFlags,
                            MapData &mapData);
    
    /**
     * Helper: Check if two points are equal
     */
    static bool pointsEqual(int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
        return x1 == x2 && y1 == y2;
    }
};

#endif // WLDIMPORTER_H
