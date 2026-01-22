#include "textureatlasgen.h"
#include <QDebug>
#include <QtMath>
#include <QPainter>
#include <QFile>

QImage TextureAtlasGenerator::createAtlas(const QVector<QImage> &textures, QVector<AtlasRegion> &uvRegions)
{
    if (textures.isEmpty()) {
        return QImage();
    }
    
    // Single texture - no atlas needed
    if (textures.size() == 1) {
        uvRegions.clear();
        uvRegions.append({QRectF(0, 0, 1, 1), 0});
        return textures[0];
    }
    
    // Calculate layout
    int cols, rows;
    calculateLayout(textures.size(), cols, rows);
    
    // Find max texture size to determine cell size
    int maxWidth = 0, maxHeight = 0;
    for (const QImage &tex : textures) {
        maxWidth = qMax(maxWidth, tex.width());
        maxHeight = qMax(maxHeight, tex.height());
    }
    
    // Create atlas image
    int atlasWidth = maxWidth * cols;
    int atlasHeight = maxHeight * rows;
    QImage atlas(atlasWidth, atlasHeight, QImage::Format_RGBA8888);
    atlas.fill(Qt::transparent);
    
    // Paint textures into atlas and calculate UV regions
    uvRegions.clear();
    QPainter painter(&atlas);
    
    for (int i = 0; i < textures.size(); i++) {
        int col = i % cols;
        int row = i / cols;
        
        int x = col * maxWidth;
        int y = row * maxHeight;
        
        // Draw texture (scaled to fit cell if needed)
        QImage scaledTex = textures[i].scaled(maxWidth, maxHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        painter.drawImage(x, y, scaledTex);
        
        // Calculate UV region (normalized 0-1)
        AtlasRegion region;
        region.textureIndex = i;
        region.uvRect = QRectF(
            (float)x / atlasWidth,
            (float)y / atlasHeight,
            (float)scaledTex.width() / atlasWidth,
            (float)scaledTex.height() / atlasHeight
        );
        uvRegions.append(region);
        
        qDebug() << "Texture" << i << "placed at (" << x << "," << y << ")";
        qDebug() << "  UV region:" << region.uvRect;
    }
    
    painter.end();
    
    qDebug() << "Atlas created:" << atlasWidth << "x" << atlasHeight;
    qDebug() << "Layout:" << cols << "cols x" << rows << "rows";
    qDebug() << "Total UV regions:" << uvRegions.size();
    
    return atlas;
}

QVector<QImage> TextureAtlasGenerator::loadTextures(const QStringList &paths)
{
    QVector<QImage> textures;
    for (const QString &path : paths) {
        if (!path.isEmpty() && QFile::exists(path)) {
            QImage img(path);
            if (!img.isNull()) {
                textures.append(img);
            } else {
                qWarning() << "Failed to load texture:" << path;
            }
        }
    }
    return textures;
}

void TextureAtlasGenerator::calculateLayout(int numTextures, int &cols, int &rows)
{
    if (numTextures <= 0) {
        cols = rows = 0;
        return;
    }
    
    // Prefer square or near-square layouts
    cols = qCeil(qSqrt(numTextures));
    rows = qCeil((float)numTextures / cols);
    
    // Special cases for common counts
    if (numTextures == 2) {
        cols = 2; rows = 1; // 2x1
    } else if (numTextures == 3) {
        cols = 3; rows = 1; // 3x1 or 2x2 with empty slot
    }
}
