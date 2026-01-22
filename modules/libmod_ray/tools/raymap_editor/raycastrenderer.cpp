#include "raycastrenderer.h"
#include <QDebug>
#include <QtMath>
#include <cmath>

RaycastRenderer::RaycastRenderer()
    : m_shaderProgram(nullptr)
    , m_stripVBO(nullptr)
    , m_stripVAO(nullptr)
    , m_defaultTexture(nullptr)
    , m_cameraX(0.0f)
    , m_cameraY(0.0f)
    , m_cameraZ(32.0f)
    , m_cameraYaw(0.0f)
    , m_cameraPitch(0.0f)
    , m_screenWidth(800)
    , m_screenHeight(600)
    , m_fov(M_PI / 3.0f) // 60 degrees
    , m_currentSector(-1)
    , m_initialized(false)
{
}

RaycastRenderer::~RaycastRenderer()
{
    cleanup();
}

bool RaycastRenderer::initialize()
{
    if (m_initialized) {
        return true;
    }
    
    initializeOpenGLFunctions();
    
    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    // Disable back-face culling
    glDisable(GL_CULL_FACE);
    
    // Create shaders
    if (!createShaders()) {
        qWarning() << "Failed to create shaders";
        return false;
    }
    
    // Create VBO for strip rendering
    m_stripVBO = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    m_stripVBO->create();
    m_stripVBO->bind();
    m_stripVBO->setUsagePattern(QOpenGLBuffer::DynamicDraw);
    // Allocate space for one vertical strip (6 vertices * 4 floats each)
    m_stripVBO->allocate(6 * 4 * sizeof(float));
    
    // Create VAO
    m_stripVAO = new QOpenGLVertexArrayObject();
    m_stripVAO->create();
    m_stripVAO->bind();
    
    // Position attribute (x, y)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    
    // TexCoord attribute (u, v)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    
    m_stripVAO->release();
    m_stripVBO->release();
    
    // Create default texture
    QImage defaultImage(1, 1, QImage::Format_RGB888);
    defaultImage.fill(Qt::white);
    m_defaultTexture = new QOpenGLTexture(defaultImage);
    m_defaultTexture->setMinificationFilter(QOpenGLTexture::Nearest);
    m_defaultTexture->setMagnificationFilter(QOpenGLTexture::Nearest);
    
    m_initialized = true;
    qDebug() << "RaycastRenderer initialized successfully";
    return true;
}

void RaycastRenderer::cleanup()
{
    if (!m_initialized) {
        return;
    }
    
    destroyShaders();
    
    // Clean up OpenGL resources
    delete m_stripVBO;
    delete m_stripVAO;
    
    for (QOpenGLTexture *texture : m_textures) {
        delete texture;
    }
    m_textures.clear();
    
    if (m_defaultTexture) {
        delete m_defaultTexture;
        m_defaultTexture = nullptr;
    }
    
    m_initialized = false;
}

bool RaycastRenderer::createShaders()
{
    m_shaderProgram = new QOpenGLShaderProgram();
    
    // Vertex shader - simple 2D screen-space rendering
    const char *vertexShaderSource = R"(
        #version 330 core
        layout(location = 0) in vec2 position;
        layout(location = 1) in vec2 texCoord;
        
        uniform mat4 projection;
        
        out vec2 fragTexCoord;
        
        void main() {
            gl_Position = projection * vec4(position, 0.0, 1.0);
            fragTexCoord = texCoord;
        }
    )";
    
    // Fragment shader
    const char *fragmentShaderSource = R"(
        #version 330 core
        in vec2 fragTexCoord;
        
        uniform sampler2D textureSampler;
        
        out vec4 outColor;
        
        void main() {
            outColor = texture(textureSampler, fragTexCoord);
        }
    )";
    
    if (!m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource)) {
        qWarning() << "Vertex shader compilation failed:" << m_shaderProgram->log();
        return false;
    }
    
    if (!m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource)) {
        qWarning() << "Fragment shader compilation failed:" << m_shaderProgram->log();
        return false;
    }
    
    if (!m_shaderProgram->link()) {
        qWarning() << "Shader program linking failed:" << m_shaderProgram->log();
        return false;
    }
    
    // Get uniform locations
    m_uniformProjection = m_shaderProgram->uniformLocation("projection");
    m_uniformTexture = m_shaderProgram->uniformLocation("textureSampler");
    
    qDebug() << "Shaders created successfully";
    return true;
}

void RaycastRenderer::destroyShaders()
{
    if (m_shaderProgram) {
        delete m_shaderProgram;
        m_shaderProgram = nullptr;
    }
}

void RaycastRenderer::setCamera(float x, float y, float z, float yaw, float pitch)
{
    m_cameraX = x;
    m_cameraY = y;  // Height (vertical)
    m_cameraZ = z;  // Depth (2D map coordinate)
    m_cameraYaw = yaw;
    m_cameraPitch = pitch;
    
    // Find current sector using X and Z (2D map plane)
    m_currentSector = findSectorAt(x, z);
    
    qDebug() << "Camera set to" << x << y << z << "- sector" << m_currentSector;
}

void RaycastRenderer::loadTexture(int id, const QImage &image)
{
    if (!m_initialized) {
        qWarning() << "Cannot load texture: renderer not initialized";
        return;
    }
    
    if (image.isNull()) {
        qWarning() << "Cannot load texture" << id << ": image is null";
        return;
    }
    
    // Delete existing texture if present
    if (m_textures.contains(id)) {
        delete m_textures[id];
    }
    
    // Create new texture
    QOpenGLTexture *texture = new QOpenGLTexture(image.flipped(Qt::Vertical));
    texture->setMinificationFilter(QOpenGLTexture::Linear);
    texture->setMagnificationFilter(QOpenGLTexture::Linear);
    texture->setWrapMode(QOpenGLTexture::Repeat);
    
    m_textures[id] = texture;
}

void RaycastRenderer::setMapData(const MapData &mapData)
{
    qDebug() << "========================================";
    qDebug() << "RaycastRenderer::setMapData() CALLED";
    qDebug() << "========================================";
    
    if (!m_initialized) {
        qWarning() << "Cannot set map data: renderer not initialized";
        return;
    }
    
    m_mapData = mapData;
    m_currentSector = findSectorAt(m_cameraX, m_cameraZ);  // Fixed: use Z not Y
    
    // Debug: Show bounds of first few sectors to understand coordinate system
    if (!mapData.sectors.isEmpty()) {
        qDebug() << "=== Sector coordinate ranges (first 5 sectors) ===";
        for (int i = 0; i < qMin(5, mapData.sectors.size()); i++) {
            const Sector &s = mapData.sectors[i];
            if (s.vertices.isEmpty()) {
                qDebug() << "  Sector" << i << ": NO VERTICES!";
                continue;
            }
            
            float minX = s.vertices[0].x(), maxX = s.vertices[0].x();
            float minZ = s.vertices[0].y(), maxZ = s.vertices[0].y();
            
            for (const QPointF &v : s.vertices) {
                minX = qMin(minX, (float)v.x());
                maxX = qMax(maxX, (float)v.x());
                minZ = qMin(minZ, (float)v.y());
                maxZ = qMax(maxZ, (float)v.y());
            }
            
            qDebug() << "  Sector" << i << ": X [" << minX << "," << maxX << "]"
                     << "Z [" << minZ << "," << maxZ << "]"
                     << "floor=" << s.floor_z << "ceiling=" << s.ceiling_z;
        }
        
        // If camera is not in any sector, auto-position it in the first sector
        if (m_currentSector < 0) {
            const Sector &firstSector = mapData.sectors[0];
            if (!firstSector.vertices.isEmpty()) {
                float centerX = 0, centerZ = 0;
                for (const QPointF &v : firstSector.vertices) {
                    centerX += v.x();
                    centerZ += v.y();
                }
                centerX /= firstSector.vertices.size();
                centerZ /= firstSector.vertices.size();
                float cameraY = firstSector.floor_z + 32.0f; // 32 units above floor
                
                qDebug() << "*** AUTO-POSITIONING CAMERA ***";
                qDebug() << "*** Moving from (" << m_cameraX << "," << m_cameraY << "," << m_cameraZ << ")";
                qDebug() << "*** to (" << centerX << "," << cameraY << "," << centerZ << ") ***";
                
                // Update camera position
                m_cameraX = centerX;
                m_cameraY = cameraY;
                m_cameraZ = centerZ;
                
                // Re-find sector with new position
                m_currentSector = findSectorAt(m_cameraX, m_cameraZ);
                
                qDebug() << "*** Camera now in sector" << m_currentSector << "***";
            }
        }
    }
    
    qDebug() << "Map data loaded:" << mapData.sectors.size() << "sectors";
}

void RaycastRenderer::render(int width, int height)
{
    if (!m_initialized) {
        return;
    }
    
    m_screenWidth = width;
    m_screenHeight = height;
    
    // Set up orthographic projection for 2D screen-space rendering
    m_projectionMatrix.setToIdentity();
    m_projectionMatrix.ortho(0, width, height, 0, -1, 1);
    
    // Clear screen
    glClearColor(0.2f, 0.3f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Use shader program
    m_shaderProgram->bind();
    m_shaderProgram->setUniformValue(m_uniformProjection, m_projectionMatrix);
    m_shaderProgram->setUniformValue(m_uniformTexture, 0);
    
    // Render frame using raycasting
    renderFrame();
    
    m_shaderProgram->release();
}

void RaycastRenderer::renderFrame()
{
    if (m_currentSector < 0 || m_currentSector >= m_mapData.sectors.size()) {
        static bool once = true;
        if (once) {
            qDebug() << "RaycastRenderer: Not in a valid sector! currentSector=" << m_currentSector 
                     << "total sectors=" << m_mapData.sectors.size()
                     << "camera at X=" << m_cameraX << "Z=" << m_cameraZ << "(Y=" << m_cameraY << ")";
            once = false;
        }
        return; // Not in a valid sector
    }
    
    static bool debugOnce = true;
    if (debugOnce) {
        qDebug() << "RaycastRenderer: Starting render - sector" << m_currentSector 
                 << "camera at" << m_cameraX << m_cameraY << m_cameraZ
                 << "yaw" << m_cameraYaw;
        qDebug() << "Screen size:" << m_screenWidth << "x" << m_screenHeight;
        qDebug() << "Sector has" << m_mapData.sectors[m_currentSector].walls.size() << "walls";
        debugOnce = false;
    }
    
    int totalHits = 0;
    
    // Cast one ray per screen column
    for (int x = 0; x < m_screenWidth; x++) {
        // Calculate ray angle
        float rayAngle = m_cameraYaw - m_fov / 2.0f + (x / (float)m_screenWidth) * m_fov;
        
        // Cast ray and get hits
        QVector<RayHit> hits;
        castRay(rayAngle, x, hits);
        
        if (!hits.isEmpty()) {
            totalHits++;
        }
        
        // Render this strip
        renderStrip(x, hits);
    }
    
    static bool debugHits = true;
    if (debugHits) {
        qDebug() << "Total strips with hits:" << totalHits << "out of" << m_screenWidth;
        debugHits = false;
    }
}

void RaycastRenderer::castRay(float angle, int stripX, QVector<RayHit> &hits)
{
    // Ray direction in XZ plane (2D top-down)
    float rayDirX = cosf(angle);
    float rayDirZ = sinf(angle);
    
    // Ray origin and end in XZ plane
    QPointF rayStart(m_cameraX, m_cameraZ);
    QPointF rayEnd(m_cameraX + rayDirX * 10000.0f, m_cameraZ + rayDirZ * 10000.0f);
    
    // Check all walls in current sector
    if (m_currentSector < 0 || m_currentSector >= m_mapData.sectors.size()) {
        return;
    }
    
    const Sector &sector = m_mapData.sectors[m_currentSector];
    
    static bool debugFirstRay = true;
    
    for (const Wall &wall : sector.walls) {
        QPointF wallStart(wall.x1, wall.y1);
        QPointF wallEnd(wall.x2, wall.y2);
        QPointF intersection;
        
        if (lineIntersect(rayStart, rayEnd, wallStart, wallEnd, intersection)) {
            if (debugFirstRay && stripX == m_screenWidth / 2) {
                qDebug() << "Center ray hit wall at" << intersection << "distance from camera";
            }
            
            RayHit hit;
            
            // Calculate distance (with fisheye correction) in XZ plane
            float dx = intersection.x() - m_cameraX;
            float dz = intersection.y() - m_cameraZ;  // QPointF.y() is our Z coordinate
            hit.distance = dx * cosf(m_cameraYaw) + dz * sinf(m_cameraYaw);
            
            if (hit.distance < 0.1f) continue; // Too close
            
            hit.hitX = intersection.x();
            hit.hitY = intersection.y();
            
            // Calculate texture U coordinate
            float wallLength = sqrtf(powf(wall.x2 - wall.x1, 2) + powf(wall.y2 - wall.y1, 2));
            float hitDist = sqrtf(powf(intersection.x() - wall.x1, 2) + powf(intersection.y() - wall.y1, 2));
            hit.texU = hitDist / wallLength;
            
            // Wall height
            hit.wallHeight = sector.ceiling_z - sector.floor_z;
            
            // Texture
            hit.textureId = wall.texture_id_middle;
            
            // Portal info
            hit.isPortal = (wall.portal_id >= 0);
            if (hit.isPortal && wall.portal_id < m_mapData.portals.size()) {
                const Portal &portal = m_mapData.portals[wall.portal_id];
                hit.portalSectorId = (portal.sector_a == m_currentSector) ? portal.sector_b : portal.sector_a;
            } else {
                hit.portalSectorId = -1;
            }
            
            hits.append(hit);
        }
    }
    
    if (debugFirstRay && stripX == m_screenWidth / 2) {
        qDebug() << "Center ray found" << hits.size() << "hits";
        debugFirstRay = false;
    }
    
    // Sort hits by distance
    std::sort(hits.begin(), hits.end(), [](const RayHit &a, const RayHit &b) {
        return a.distance < b.distance;
    });
}

void RaycastRenderer::renderStrip(int x, const QVector<RayHit> &hits)
{
    const Sector &sector = m_mapData.sectors[m_currentSector];
    
    if (hits.isEmpty()) {
        return;
    }
    
    // Render closest hit
    const RayHit &hit = hits.first();
    
    // Calculate screen distance for perspective
    float screenDist = (m_screenWidth / 2.0f) / tanf(m_fov / 2.0f);
    
    // Calculate wall heights relative to camera
    float wallFloorHeight = sector.floor_z - m_cameraY;
    float wallCeilingHeight = sector.ceiling_z - m_cameraY;
    
    // Project heights to screen space
    float wallFloorScreen = (screenDist / hit.distance) * wallFloorHeight;
    float wallCeilingScreen = (screenDist / hit.distance) * wallCeilingHeight;
    
    // Apply pitch offset (looking up/down)
    float pitchOffset = m_screenHeight * tanf(m_cameraPitch);
    
    // Calculate final screen Y coordinates
    float wallTop = m_screenHeight / 2.0f - wallCeilingScreen + pitchOffset;
    float wallBottom = m_screenHeight / 2.0f - wallFloorScreen + pitchOffset;
    
    // Calculate ray angle for this column
    float columnAngle = m_cameraYaw - m_fov / 2.0f + (x / (float)m_screenWidth) * m_fov;
    float rayDirX = cosf(columnAngle);
    float rayDirZ = sinf(columnAngle);
    
    // Render ceiling with proper perspective
    if (wallTop > 0 && sector.ceiling_texture_id > 0) {
        for (int y = 0; y < wallTop && y < m_screenHeight; y++) {
            // Calculate distance to ceiling at this screen Y
            float screenY = y - m_screenHeight / 2.0f - pitchOffset;
            float ceilingDist = screenDist * wallCeilingHeight / screenY;
            
            if (ceilingDist > 0 && ceilingDist < 10000.0f) {
                // Calculate world position
                float worldX = m_cameraX + rayDirX * ceilingDist;
                float worldZ = m_cameraZ + rayDirZ * ceilingDist;
                
                // Calculate texture coordinates (tile every 64 units)
                float texU = fmodf(worldX / 64.0f, 1.0f);
                float texV = fmodf(worldZ / 64.0f, 1.0f);
                if (texU < 0) texU += 1.0f;
                if (texV < 0) texV += 1.0f;
                
                // Render single pixel
                renderWallStrip(x, y, y + 1, texU, sector.ceiling_texture_id);
            }
        }
    }
    
    // Render wall strip
    if (!hit.isPortal || hit.portalSectorId < 0) {
        renderWallStrip(x, wallTop, wallBottom, hit.texU, hit.textureId);
    } else {
        renderWallStrip(x, wallTop, wallBottom, hit.texU, hit.textureId);
    }
    
    // Render floor with proper perspective
    if (wallBottom < m_screenHeight && sector.floor_texture_id > 0) {
        for (int y = qMax(0, (int)wallBottom); y < m_screenHeight; y++) {
            // Calculate distance to floor at this screen Y
            float screenY = y - m_screenHeight / 2.0f - pitchOffset;
            float floorDist = screenDist * wallFloorHeight / screenY;
            
            if (floorDist > 0 && floorDist < 10000.0f) {
                // Calculate world position
                float worldX = m_cameraX + rayDirX * floorDist;
                float worldZ = m_cameraZ + rayDirZ * floorDist;
                
                // Calculate texture coordinates (tile every 64 units)
                float texU = fmodf(worldX / 64.0f, 1.0f);
                float texV = fmodf(worldZ / 64.0f, 1.0f);
                if (texU < 0) texU += 1.0f;
                if (texV < 0) texV += 1.0f;
                
                // Render single pixel
                renderWallStrip(x, y, y + 1, texU, sector.floor_texture_id);
            }
        }
    }
}

void RaycastRenderer::renderWallStrip(int x, float y1, float y2, float texU, int textureId)
{
    // Create quad for this vertical strip
    float vertices[] = {
        // x, y, u, v
        (float)x,     y1, texU, 0.0f,
        (float)x + 1, y1, texU, 0.0f,
        (float)x + 1, y2, texU, 1.0f,
        
        (float)x,     y1, texU, 0.0f,
        (float)x + 1, y2, texU, 1.0f,
        (float)x,     y2, texU, 1.0f
    };
    
    // Update VBO
    m_stripVBO->bind();
    m_stripVBO->write(0, vertices, sizeof(vertices));
    
    // Bind texture
    QOpenGLTexture *tex = m_textures.value(textureId, m_defaultTexture);
    if (tex) tex->bind(0);
    
    // Draw
    m_stripVAO->bind();
    glDrawArrays(GL_TRIANGLES, 0, 6);
    m_stripVAO->release();
}

// Geometry helper functions

bool RaycastRenderer::lineIntersect(QPointF p1, QPointF p2, QPointF p3, QPointF p4, QPointF &intersection)
{
    float x1 = p1.x(), y1 = p1.y();
    float x2 = p2.x(), y2 = p2.y();
    float x3 = p3.x(), y3 = p3.y();
    float x4 = p4.x(), y4 = p4.y();
    
    float denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
    if (fabs(denom) < 0.0001f) {
        return false; // Parallel lines
    }
    
    float t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / denom;
    float u = -((x1 - x2) * (y1 - y3) - (y1 - y2) * (x1 - x3)) / denom;
    
    if (t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f) {
        intersection.setX(x1 + t * (x2 - x1));
        intersection.setY(y1 + t * (y2 - y1));
        return true;
    }
    
    return false;
}

float RaycastRenderer::pointToLineDistance(QPointF point, QPointF lineStart, QPointF lineEnd)
{
    float dx = lineEnd.x() - lineStart.x();
    float dy = lineEnd.y() - lineStart.y();
    float lengthSq = dx * dx + dy * dy;
    
    if (lengthSq < 0.0001f) {
        // Line is actually a point
        dx = point.x() - lineStart.x();
        dy = point.y() - lineStart.y();
        return sqrtf(dx * dx + dy * dy);
    }
    
    float t = ((point.x() - lineStart.x()) * dx + (point.y() - lineStart.y()) * dy) / lengthSq;
    t = qMax(0.0f, qMin(1.0f, t));
    
    float projX = lineStart.x() + t * dx;
    float projY = lineStart.y() + t * dy;
    
    dx = point.x() - projX;
    dy = point.y() - projY;
    
    return sqrtf(dx * dx + dy * dy);
}

int RaycastRenderer::findSectorAt(float x, float y)
{
    for (int i = 0; i < m_mapData.sectors.size(); i++) {
        if (pointInPolygon(x, y, m_mapData.sectors[i].vertices)) {
            return i;
        }
    }
    return -1;
}

bool RaycastRenderer::pointInPolygon(float x, float y, const QVector<QPointF> &polygon)
{
    if (polygon.size() < 3) {
        return false;
    }
    
    bool inside = false;
    for (int i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        float xi = polygon[i].x(), yi = polygon[i].y();
        float xj = polygon[j].x(), yj = polygon[j].y();
        
        if (((yi > y) != (yj > y)) &&
            (x < (xj - xi) * (y - yi) / (yj - yi) + xi)) {
            inside = !inside;
        }
    }
    
    return inside;
}
