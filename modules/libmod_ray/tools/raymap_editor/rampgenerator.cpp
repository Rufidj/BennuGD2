#include "rampgenerator.h"
#include <QtMath>
#include <QDebug>

RampGenerator::RampGenerator()
{
}

QVector<Sector> RampGenerator::generateRamp(const RampParameters &params)
{
    QVector<Sector> sectors;
    
    // Calculate direction vector
    QPointF direction = params.endPoint - params.startPoint;
    float length = qSqrt(direction.x() * direction.x() + direction.y() * direction.y());
    
    if (length < 0.1f) {
        qWarning() << "RampGenerator: Start and end points too close";
        return sectors;
    }
    
    qDebug() << "Generating REAL SLOPE ramp: length=" << length 
             << "height change=" << (params.endHeight - params.startHeight);
    
    // Normalize direction
    QPointF dirNorm = normalize(direction);
    
    // Calculate perpendicular for width
    QPointF perpVec = perpendicular(dirNorm);
    QPointF widthOffset = perpVec * (params.width / 2.0f);
    
    // Create 4 corners of the rectangular ramp sector
    QPointF p1 = params.startPoint + widthOffset;
    QPointF p2 = params.startPoint - widthOffset;
    QPointF p3 = params.endPoint - widthOffset;
    QPointF p4 = params.endPoint + widthOffset;
    
    // Create the single ramp sector
    Sector sector = createRectangularSector(p1, p2, p3, p4, 
                                                 params.startHeight, 
                                                 params.startHeight + params.ceilingHeight,
                                                 params.textureId,
                                                 params.ceilingTextureId,
                                                 params.wallTextureId);
    
    // REAL SLOPES: Calculate slope parameters (Build Engine Style)
    // Z = Base + (Heinum * Dist) / (Len * 32) ?? No, it's (Heinum * Cross) / (Len * 32)
    // Actually, let's look at the formula we derived: 
    // Z = Base + (heinum * perp_dist_from_wall0) / 32 ? No
    
    // In Build: tan(slope_angle) = heinum / 4096.0f (approx)
    // Let's use the simplest approximation:
    // heinum = (heightChange / length) * 4096.0f
    
    // Note: The slope pivot is implicitly Wall[0].
    // Our rectangular sector has walls:
    // Wall 0: p1 -> p2 (Start Width) - This is perpendicular to ramp direction! perfect.
    // The "distance" from Wall 0 increases along the ramp.
    
    float heightChange = params.endHeight - params.startHeight;
    int16_t heinum = (int16_t)((heightChange / length) * 4096.0f);
    
    // REAL SLOPES REMOVED - Pivoting to MD3
    // sector.floor_heinum = heinum;
    // sector.floor_slope_enabled = true;
    
    // Ceiling stays parallel
    // sector.ceiling_heinum = heinum;
    // sector.ceiling_slope_enabled = true;
    
    qDebug() << "Created slope sector: heinum=" << heinum << " for height change " << heightChange;
    // Pivot is implicitly Wall[0] (p1->p2)

    
    sectors.append(sector);
    
    qDebug() << "Generated 1 REAL SLOPE ramp sector (instead of" << params.segments << "flat sectors)";
    return sectors;
}

QVector<Sector> RampGenerator::generateStairs(const RampParameters &params)
{
    QVector<Sector> sectors;
    
    if (params.segments <= 0) {
        qWarning() << "RampGenerator: Invalid step count";
        return sectors;
    }
    
    // Calculate direction vector
    QPointF direction = params.endPoint - params.startPoint;
    float length = qSqrt(direction.x() * direction.x() + direction.y() * direction.y());
    
    if (length < 0.1f) {
        qWarning() << "RampGenerator: Start and end points too close";
        return sectors;
    }
    
    // Normalize direction
    QPointF dirNorm = normalize(direction);
    
    // Calculate perpendicular for width
    QPointF perpVec = perpendicular(dirNorm);
    QPointF widthOffset = perpVec * (params.width / 2.0f);
    
    // Step dimensions
    float stepLength = length / params.segments;
    float stepHeight = (params.endHeight - params.startHeight) / params.segments;
    
    qDebug() << "Generating stairs:" << params.segments << "steps, height per step:" << stepHeight;
    
    // Generate each step
    for (int i = 0; i < params.segments; i++) {
        // Calculate step position
        float t1 = (float)i / params.segments;
        float t2 = (float)(i + 1) / params.segments;
        
        QPointF stepStart = params.startPoint + direction * t1;
        QPointF stepEnd = params.startPoint + direction * t2;
        
        // Each step has constant height (discrete steps)
        float floorZ = params.startHeight + stepHeight * (i + 1);
        float ceilingZ = floorZ + params.ceilingHeight;
        
        // Create 4 corners
        QPointF p1 = stepStart + widthOffset;
        QPointF p2 = stepStart - widthOffset;
        QPointF p3 = stepEnd - widthOffset;
        QPointF p4 = stepEnd + widthOffset;
        
        // Create sector
        Sector sector = createRectangularSector(p1, p2, p3, p4,
                                                     floorZ, ceilingZ,
                                                     params.textureId,
                                                     params.ceilingTextureId,
                                                     params.wallTextureId);
        
        sectors.append(sector);
    }
    
    qDebug() << "Generated" << sectors.size() << "stair sectors";
    return sectors;
}

QVector<Sector> RampGenerator::generateSpiralRamp(QPointF center, float radius,
                                                        float startHeight, float endHeight,
                                                        int segments, float width)
{
    QVector<Sector> sectors;
    
    if (segments <= 0) return sectors;
    
    float angleStep = 360.0f / segments;
    float heightStep = (endHeight - startHeight) / segments;
    
    for (int i = 0; i < segments; i++) {
        float angle1 = qDegreesToRadians(angleStep * i);
        float angle2 = qDegreesToRadians(angleStep * (i + 1));
        
        float innerRadius = radius - width / 2.0f;
        float outerRadius = radius + width / 2.0f;
        
        // Calculate 4 corners
        QPointF p1(center.x() + outerRadius * qCos(angle1),
                   center.y() + outerRadius * qSin(angle1));
        QPointF p2(center.x() + innerRadius * qCos(angle1),
                   center.y() + innerRadius * qSin(angle1));
        QPointF p3(center.x() + innerRadius * qCos(angle2),
                   center.y() + innerRadius * qSin(angle2));
        QPointF p4(center.x() + outerRadius * qCos(angle2),
                   center.y() + outerRadius * qSin(angle2));
        
        float floorZ = startHeight + heightStep * i;
        float ceilingZ = floorZ + 128.0f; // Default ceiling height
        
        Sector sector = createRectangularSector(p1, p2, p3, p4,
                                                     floorZ, ceilingZ,
                                                     1, 1, 1);
        sectors.append(sector);
    }
    
    return sectors;
}

Sector RampGenerator::createRectangularSector(QPointF p1, QPointF p2, QPointF p3, QPointF p4,
                                                    float floorZ, float ceilingZ,
                                                    int floorTexId, int ceilingTexId, int wallTexId)
{
    Sector sector;
    sector.sector_id = -1; // Will be assigned by MapData
    sector.floor_z = floorZ;
    sector.ceiling_z = ceilingZ;
    sector.floor_texture_id = floorTexId;
    sector.ceiling_texture_id = ceilingTexId;
    sector.light_level = 100;
    
    // Add vertices in counter-clockwise order
    sector.vertices.append(p1);
    sector.vertices.append(p2);
    sector.vertices.append(p3);
    sector.vertices.append(p4);
    
    // Create walls
    for (int i = 0; i < 4; i++) {
        Wall wall;
        wall.wall_id = i;
        wall.x1 = sector.vertices[i].x();
        wall.y1 = sector.vertices[i].y();
        wall.x2 = sector.vertices[(i + 1) % 4].x();
        wall.y2 = sector.vertices[(i + 1) % 4].y();
        wall.texture_id_lower = wallTexId;
        wall.texture_id_middle = wallTexId;
        wall.texture_id_upper = wallTexId;
        wall.texture_split_z_lower = 0.0f;
        wall.texture_split_z_upper = 0.0f;
        wall.portal_id = -1;
        wall.flags = 0;
        
        sector.walls.append(wall);
    }
    
    return sector;
}

QPointF RampGenerator::perpendicular(const QPointF &vec)
{
    return QPointF(-vec.y(), vec.x());
}

QPointF RampGenerator::normalize(const QPointF &vec)
{
    float len = qSqrt(vec.x() * vec.x() + vec.y() * vec.y());
    if (len < 0.0001f) return QPointF(0, 0);
    return QPointF(vec.x() / len, vec.y() / len);
}
