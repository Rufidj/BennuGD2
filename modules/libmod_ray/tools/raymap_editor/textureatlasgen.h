#ifndef TEXTUREATLASGEN_H
#define TEXTUREATLASGEN_H

#include <QImage>
#include <QVector>
#include <QRectF>
#include <QString>

class TextureAtlasGenerator
{
public:
    struct AtlasRegion {
        QRectF uvRect;  // UV coordinates in atlas (0.0-1.0)
        int textureIndex; // Original texture index
    };
    
    // Generate atlas from multiple textures
    // Returns the combined atlas image and fills uvRegions with UV coordinates for each texture
    static QImage createAtlas(const QVector<QImage> &textures, QVector<AtlasRegion> &uvRegions);
    
    // Helper: Load textures from file paths
    static QVector<QImage> loadTextures(const QStringList &paths);
    
    // Helper: Determine optimal atlas layout (2x2, 3x1, etc.)
    static void calculateLayout(int numTextures, int &cols, int &rows);
};

#endif // TEXTUREATLASGEN_H
