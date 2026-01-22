#ifndef RAMPGENERATOR_H
#define RAMPGENERATOR_H

#include <QPointF>
#include <QVector>
#include "mapdata.h"

struct RampParameters {
    QPointF startPoint;
    QPointF endPoint;
    float startHeight;
    float endHeight;
    float width;
    int segments;
    bool generateAsStairs;
    int textureId;
    int ceilingTextureId;
    int wallTextureId;
    float ceilingHeight;
};

class RampGenerator
{
public:
    RampGenerator();
    
    // Generate a ramp as multiple sectors with interpolated heights
    static QVector<Sector> generateRamp(const RampParameters &params);
    
    // Generate stairs as discrete steps
    static QVector<Sector> generateStairs(const RampParameters &params);
    
    // Generate a circular ramp (spiral)
    static QVector<Sector> generateSpiralRamp(QPointF center, float radius, 
                                                    float startHeight, float endHeight,
                                                    int segments, float width);
    
private:
    // Helper: Create a rectangular sector
    static Sector createRectangularSector(QPointF p1, QPointF p2, QPointF p3, QPointF p4,
                                               float floorZ, float ceilingZ,
                                               int floorTexId, int ceilingTexId, int wallTexId);
    
    // Helper: Calculate perpendicular vector
    static QPointF perpendicular(const QPointF &vec);
    
    // Helper: Normalize vector
    static QPointF normalize(const QPointF &vec);
};

#endif // RAMPGENERATOR_H
