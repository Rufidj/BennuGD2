#ifndef RAYCASTRENDERER_H
#define RAYCASTRENDERER_H

#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLTexture>
#include <QMatrix4x4>
#include <QMap>
#include <QVector>
#include <QPointF>
#include "mapdata.h"

/**
 * Pure OpenGL raycasting renderer - no BennuGD2 dependencies
 * Implements Build Engine-style portal rendering
 */
class RaycastRenderer : protected QOpenGLFunctions
{
public:
    RaycastRenderer();
    ~RaycastRenderer();
    
    bool initialize();
    void cleanup();
    
    void setMapData(const MapData &mapData);
    void loadTexture(int id, const QImage &image);
    
    void setCamera(float x, float y, float z, float yaw, float pitch);
    void render(int width, int height);
    
    // Camera getters (for retrieving auto-positioned camera)
    float getCameraX() const { return m_cameraX; }
    float getCameraY() const { return m_cameraY; }
    float getCameraZ() const { return m_cameraZ; }
    
private:
    // Raycasting structures
    struct RayHit {
        float distance;
        float hitX, hitY;
        float wallHeight;
        float texU;
        int textureId;
        bool isPortal;
        int portalSectorId;
    };
    
    // Rendering functions
    bool createShaders();
    void destroyShaders();
    
    void renderFrame();
    void castRay(float angle, int stripX, QVector<RayHit> &hits);
    void renderStrip(int x, const QVector<RayHit> &hits);
    void renderWallStrip(int x, float y1, float y2, float texU, int textureId);
    
    // Geometry helpers
    bool lineIntersect(QPointF p1, QPointF p2, QPointF p3, QPointF p4, QPointF &intersection);
    float pointToLineDistance(QPointF point, QPointF lineStart, QPointF lineEnd);
    int findSectorAt(float x, float y);
    bool pointInPolygon(float x, float y, const QVector<QPointF> &polygon);
    
    // OpenGL resources
    QOpenGLShaderProgram *m_shaderProgram;
    QOpenGLBuffer *m_stripVBO;
    QOpenGLVertexArrayObject *m_stripVAO;
    QMap<int, QOpenGLTexture*> m_textures;
    QOpenGLTexture *m_defaultTexture;
    
    // Shader uniforms
    int m_uniformProjection;
    int m_uniformTexture;
    
    // Camera state
    float m_cameraX;
    float m_cameraY;
    float m_cameraZ;
    float m_cameraYaw;
    float m_cameraPitch;
    
    // Rendering state
    QMatrix4x4 m_projectionMatrix;
    int m_screenWidth;
    int m_screenHeight;
    float m_fov;
    
    // Map data
    MapData m_mapData;
    int m_currentSector;
    
    // State
    bool m_initialized;
};

#endif // RAYCASTRENDERER_H
