#include "visualrenderer.h"
#include <QDebug>
#include <QtMath>

VisualRenderer::VisualRenderer()
    : m_shaderProgram(nullptr)
    , m_defaultTexture(nullptr)
    , m_cameraX(0.0f)
    , m_cameraY(0.0f)
    , m_cameraZ(32.0f)
    , m_cameraYaw(0.0f)
    , m_cameraPitch(0.0f)
    , m_skyTextureId(-1)
    , m_initialized(false)
{
}

VisualRenderer::~VisualRenderer()
{
    cleanup();
}

bool VisualRenderer::initialize()
{
    if (m_initialized) {
        return true;
    }
    
    initializeOpenGLFunctions();
    
    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    // Disable back-face culling so walls are visible from both sides
    glDisable(GL_CULL_FACE);
    
    // Create shaders
    if (!createShaders()) {
        qWarning() << "Failed to create shaders";
        return false;
    }
    
    // Create default texture (white 1x1)
    QImage defaultImage(1, 1, QImage::Format_RGB888);
    defaultImage.fill(Qt::white);
    m_defaultTexture = new QOpenGLTexture(defaultImage);
    m_defaultTexture->setMinificationFilter(QOpenGLTexture::Nearest);
    m_defaultTexture->setMagnificationFilter(QOpenGLTexture::Nearest);
    
    // Set default projection
    setProjection(90.0f, 4.0f/3.0f, 0.1f, 10000.0f);
    
    m_initialized = true;
    qDebug() << "VisualRenderer initialized successfully";
    return true;
}

void VisualRenderer::cleanup()
{
    if (!m_initialized) {
        return;
    }
    
    clearGeometry();
    destroyShaders();
    
    // Clean up textures
    for (QOpenGLTexture *texture : m_textures) {
        delete texture;
    }
    m_textures.clear();
    
    if (m_defaultTexture) {
        delete m_defaultTexture;
        m_defaultTexture = nullptr;
    }
    
    // Clean up models
    for (MD3Loader *loader : m_models) {
        delete loader;
    }
    m_models.clear();
    
    m_initialized = false;
}

bool VisualRenderer::createShaders()
{
    m_shaderProgram = new QOpenGLShaderProgram();
    
    // Vertex shader
    const char *vertexShaderSource = R"(
        #version 330 core
        layout(location = 0) in vec3 position;
        layout(location = 1) in vec2 texCoord;
        layout(location = 2) in vec3 normal;
        
        uniform mat4 mvp;
        
        out vec2 fragTexCoord;
        out vec3 fragNormal;
        out float fragDepth;
        
        void main() {
            gl_Position = mvp * vec4(position, 1.0);
            fragTexCoord = texCoord;
            fragNormal = normal;
            fragDepth = gl_Position.z;
        }
    )";
    
    // Fragment shader
    const char *fragmentShaderSource = R"(
        #version 330 core
        in vec2 fragTexCoord;
        in vec3 fragNormal;
        in float fragDepth;
        
        uniform sampler2D textureSampler;
        uniform float lightLevel;
        
        out vec4 color;
        
        void main() {
            vec4 texColor = texture(textureSampler, fragTexCoord);
            
            // Simple lighting based on normal
            float lighting = max(abs(dot(fragNormal, vec3(0.0, 1.0, 0.0))), 0.5);
            
            // Apply light level (ensure it's at least 0.5 for visibility)
            float finalLight = max(lighting * lightLevel, 0.5);
            
            color = vec4(texColor.rgb * finalLight, texColor.a);
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
    m_uniformMVP = m_shaderProgram->uniformLocation("mvp");
    m_uniformTexture = m_shaderProgram->uniformLocation("textureSampler");
    m_uniformLightLevel = m_shaderProgram->uniformLocation("lightLevel");
    
    qDebug() << "Shaders created successfully";
    return true;
}

void VisualRenderer::destroyShaders()
{
    if (m_shaderProgram) {
        delete m_shaderProgram;
        m_shaderProgram = nullptr;
    }
}

void VisualRenderer::setMapData(const MapData &mapData)
{
    if (!m_initialized) {
        qWarning() << "Cannot set map data: renderer not initialized";
        return;
    }
    
    // Store map data for portal lookups
    m_mapData = mapData;
    
    // Store sky texture ID
    m_skyTextureId = mapData.skyTextureID;
    
    // Clear existing geometry
    clearGeometry();
    
    // Generate new geometry
    generateGeometry(mapData);
    
    qDebug() << "Map data loaded:" << mapData.sectors.size() << "sectors," 
             << mapData.entities.size() << "entities, sky texture:" << m_skyTextureId;
}

void VisualRenderer::generateGeometry(const MapData &mapData)
{
    qDebug() << "=== Generating geometry for" << mapData.sectors.size() << "sectors ===";
    
    // Generate geometry for each sector
    for (const Sector &sector : mapData.sectors) {
        qDebug() << "Sector" << sector.sector_id << ": vertices=" << sector.vertices.size() 
                 << "walls=" << sector.walls.size()
                 << "floor_z=" << sector.floor_z << "ceiling_z=" << sector.ceiling_z
                 << "light=" << sector.light_level
                 << "floor_tex=" << sector.floor_texture_id << "ceiling_tex=" << sector.ceiling_texture_id;
        generateSectorGeometry(sector);
    }
    
    qDebug() << "Generated:" << m_wallBuffers.size() << "walls," 
             << m_floorBuffers.size() << "floors," 
             << m_ceilingBuffers.size() << "ceilings";
    
    // Debug: Show texture IDs being used
    QSet<int> usedTextures;
    for (const GeometryBuffer &buf : m_wallBuffers) usedTextures.insert(buf.textureId);
    for (const GeometryBuffer &buf : m_floorBuffers) usedTextures.insert(buf.textureId);
    for (const GeometryBuffer &buf : m_ceilingBuffers) usedTextures.insert(buf.textureId);
    
    qDebug() << "Texture IDs used by geometry:" << usedTextures;
    qDebug() << "Texture IDs loaded in renderer:" << m_textures.keys();
    
    // Generate entity billboards or 3D models
    int entityIndex = 0;
    for (const EntityInstance &entity : mapData.entities) {
 

        bool modelLoaded = false;
        int entityTextureId = 1000 + entityIndex;
        
        // Try to find/load texture first (for both model and billboard)
        QString texturePath = entity.assetPath;
        if (texturePath.endsWith(".md3", Qt::CaseInsensitive)) {
            texturePath.replace(".md3", ".png", Qt::CaseInsensitive);
        } else {
            texturePath += ".png";
        }
        
        QImage entityImage(texturePath);
        if (!entityImage.isNull()) {
            loadTexture(entityTextureId, entityImage);
        } else {
            // Placeholder texture
            QImage placeholder(64, 64, QImage::Format_RGB888);
            QColor colors[] = {Qt::red, Qt::green, Qt::blue, Qt::yellow, Qt::cyan, Qt::magenta};
            placeholder.fill(colors[entityIndex % 6]);
            loadTexture(entityTextureId, placeholder);
        }

        // Try to load and render MD3 model
        if (entity.assetPath.endsWith(".md3", Qt::CaseInsensitive)) {
            MD3Loader *loader = nullptr;
            
            // Check cache
            if (m_models.contains(entity.assetPath)) {
                loader = m_models[entity.assetPath];
            } else {
                // Load new
                loader = new MD3Loader();
                if (loader->load(entity.assetPath)) {
                    m_models[entity.assetPath] = loader;
                    qDebug() << "Loaded MD3 model:" << entity.assetPath;
                } else {
                    delete loader;
                    loader = nullptr;
                    qWarning() << "Failed to load MD3:" << entity.assetPath;
                }
            }
            
            if (loader) {
                // Generate geometry for each surface
                const QVector<RenderSurface> &surfaces = loader->getSurfaces();
                for (const RenderSurface &surf : surfaces) {
                    QVector<float> vertices;
                    
                    for (unsigned int idx : surf.indices) {
                        if ((int)idx < surf.vertices.size()) {
                            QVector3D p = surf.vertices[idx];
                            QVector2D uv = surf.texCoords[idx];
                            
                            // Transform MD3 (Z-up) to World (Y-up for OpenGL rendering of RayMap)
                            // Map X -> GL X
                            // Map Y -> GL Z
                            // Map Z (Height) -> GL Y
                            
                            // MD3 Standard: X=Forward, Y=Left, Z=Up
                            // We need testing for orientation, but let's try mapping MD3 Z to Map Height (GL Y)
                            
                            float wx = entity.x + p.x();
                            float wy = entity.z + p.z(); // Height
                            float wz = entity.y + p.y();
                            
                            vertices << wx << wy << wz;
                            vertices << uv.x() << uv.y();
                            vertices << 0.0f << 1.0f << 0.0f; // Dummy normal
                        }
                    }
                    
                    if (!vertices.isEmpty()) {
                        GeometryBuffer buffer;
                        buffer.vbo = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
                        buffer.vbo->create();
                        buffer.vbo->bind();
                        buffer.vbo->allocate(vertices.constData(), vertices.size() * sizeof(float));
                        
                        buffer.vao = new QOpenGLVertexArrayObject();
                        buffer.vao->create();
                        buffer.vao->bind();
                        
                        glEnableVertexAttribArray(0);
                        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
                        glEnableVertexAttribArray(1);
                        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
                        glEnableVertexAttribArray(2);
                        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
                        
                        buffer.vao->release();
                        buffer.vbo->release();
                        
                        buffer.vertexCount = vertices.size() / 8;
                        buffer.textureId = entityTextureId; // Use the entity texture we loaded
                        buffer.lightLevel = 1.0f;
                        
                        m_entityBuffers.append(buffer);
                    }
                }
                modelLoaded = true;
            }
        }
        
        // Fallback to Billboard if model failed to load
        if (!modelLoaded) {
            QVector<float> vertices;
            float size = 32.0f; 
            
            // Quad logic here...
            vertices << entity.x - size << entity.z << entity.y;
            vertices << 0.0f << 0.0f;
            vertices << 1.0f << 0.0f << 0.0f;
            
            vertices << entity.x + size << entity.z << entity.y;
            vertices << 1.0f << 0.0f;
            vertices << 1.0f << 0.0f << 0.0f;
            
            vertices << entity.x + size << entity.z + size*2 << entity.y;
            vertices << 1.0f << 1.0f;
            vertices << 1.0f << 0.0f << 0.0f;
            
            vertices << entity.x - size << entity.z << entity.y;
            vertices << 0.0f << 0.0f;
            vertices << 1.0f << 0.0f << 0.0f;
            
            vertices << entity.x + size << entity.z + size*2 << entity.y;
            vertices << 1.0f << 1.0f;
            vertices << 1.0f << 0.0f << 0.0f;
            
            vertices << entity.x - size << entity.z + size*2 << entity.y;
            vertices << 0.0f << 1.0f;
            vertices << 1.0f << 0.0f << 0.0f;
            
            if (!vertices.isEmpty()) {
                GeometryBuffer buffer;
                buffer.vbo = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
                buffer.vbo->create();
                buffer.vbo->bind();
                buffer.vbo->allocate(vertices.constData(), vertices.size() * sizeof(float));
                
                buffer.vao = new QOpenGLVertexArrayObject();
                buffer.vao->create();
                buffer.vao->bind();
                
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
                glEnableVertexAttribArray(2);
                glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
                
                buffer.vao->release();
                buffer.vbo->release();
                
                buffer.vertexCount = 6;
                buffer.textureId = entityTextureId;
                buffer.lightLevel = 1.0f;
                
                m_entityBuffers.append(buffer);
            }
        }
        
        entityIndex++;
    }
    
    qDebug() << "Generated" << m_entityBuffers.size() << "entity billboards";
}

void VisualRenderer::generateSectorGeometry(const Sector &sector)
{
    if (sector.vertices.size() < 3) {
        return; // Invalid sector
    }
    
    // Generate floor geometry (triangulated polygon)
    {
        QVector<float> vertices;
        
        // Simple fan triangulation from first vertex
        for (int i = 1; i < sector.vertices.size() - 1; i++) {
            // Triangle: 0, i, i+1
            
            // Vertex 0
            vertices << sector.vertices[0].x() << sector.floor_z << sector.vertices[0].y();
            vertices << sector.vertices[0].x() / 128.0f << sector.vertices[0].y() / 128.0f; // UV
            vertices << 0.0f << 1.0f << 0.0f; // Normal (up)
            
            // Vertex i
            vertices << sector.vertices[i].x() << sector.floor_z << sector.vertices[i].y();
            vertices << sector.vertices[i].x() / 128.0f << sector.vertices[i].y() / 128.0f;
            vertices << 0.0f << 1.0f << 0.0f;
            
            // Vertex i+1
            vertices << sector.vertices[i+1].x() << sector.floor_z << sector.vertices[i+1].y();
            vertices << sector.vertices[i+1].x() / 128.0f << sector.vertices[i+1].y() / 128.0f;
            vertices << 0.0f << 1.0f << 0.0f;
        }
        
        if (!vertices.isEmpty()) {
            GeometryBuffer buffer;
            buffer.vbo = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
            buffer.vbo->create();
            buffer.vbo->bind();
            buffer.vbo->allocate(vertices.constData(), vertices.size() * sizeof(float));
            
            buffer.vao = new QOpenGLVertexArrayObject();
            buffer.vao->create();
            buffer.vao->bind();
            
            // Position attribute
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
            
            // TexCoord attribute
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
            
            // Normal attribute
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
            
            buffer.vao->release();
            buffer.vbo->release();
            
            buffer.vertexCount = vertices.size() / 8;
            buffer.textureId = sector.floor_texture_id;
            buffer.lightLevel = sector.light_level > 0 ? sector.light_level / 255.0f : 1.0f;
            
            m_floorBuffers.append(buffer);
        }
    }
    
    // Generate ceiling geometry (triangulated polygon, facing down)
    // Only generate if not Sky (ID > 0)
    if (sector.ceiling_texture_id > 0) {
        QVector<float> vertices;
        
        // Simple fan triangulation from first vertex (reversed winding for downward face)
        for (int i = 1; i < sector.vertices.size() - 1; i++) {
            // Triangle: 0, i+1, i (reversed)
            
            // Vertex 0
            vertices << sector.vertices[0].x() << sector.ceiling_z << sector.vertices[0].y();
            vertices << sector.vertices[0].x() / 128.0f << sector.vertices[0].y() / 128.0f;
            vertices << 0.0f << -1.0f << 0.0f; // Normal (down)
            
            // Vertex i+1
            vertices << sector.vertices[i+1].x() << sector.ceiling_z << sector.vertices[i+1].y();
            vertices << sector.vertices[i+1].x() / 128.0f << sector.vertices[i+1].y() / 128.0f;
            vertices << 0.0f << -1.0f << 0.0f;
            
            // Vertex i
            vertices << sector.vertices[i].x() << sector.ceiling_z << sector.vertices[i].y();
            vertices << sector.vertices[i].x() / 128.0f << sector.vertices[i].y() / 128.0f;
            vertices << 0.0f << -1.0f << 0.0f;
        }
        
        if (!vertices.isEmpty()) {
            GeometryBuffer buffer;
            buffer.vbo = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
            buffer.vbo->create();
            buffer.vbo->bind();
            buffer.vbo->allocate(vertices.constData(), vertices.size() * sizeof(float));
            
            buffer.vao = new QOpenGLVertexArrayObject();
            buffer.vao->create();
            buffer.vao->bind();
            
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
            
            buffer.vao->release();
            buffer.vbo->release();
            
            buffer.vertexCount = vertices.size() / 8;
            buffer.textureId = sector.ceiling_texture_id;
            buffer.lightLevel = sector.light_level > 0 ? sector.light_level / 255.0f : 1.0f;
            
            m_ceilingBuffers.append(buffer);
        }
    }
    
    // Generate wall geometry
    for (const Wall &wall : sector.walls) {
        float dx = wall.x2 - wall.x1;
        float dy = wall.y2 - wall.y1;
        float length = qSqrt(dx*dx + dy*dy);
        if (length < 0.001f) continue;
        
        float nx = -dy / length;
        float ny = 0.0f;
        float nz = dx / length;
        
        // Check if this wall is a portal
        if (wall.portal_id >= 0) {
            qDebug() << "Portal wall detected: portal_id=" << wall.portal_id 
                     << "upper_tex=" << wall.texture_id_upper 
                     << "lower_tex=" << wall.texture_id_lower;
            
            // This is a portal - render upper and lower sections if needed
            // We need to find the connected sector to get its floor/ceiling heights
            // For now, we'll skip rendering the middle part but render upper/lower
            
            // Find the portal to get the neighbor sector
            const Portal *portal = nullptr;
            for (const Portal &p : m_mapData.portals) {
                if (p.portal_id == wall.portal_id) {
                    portal = &p;
                    break;
                }
            }
            
            if (!portal) {
                qWarning() << "Portal" << wall.portal_id << "not found in portal list!";
                continue;
            }
            
            qDebug() << "Found portal:" << portal->portal_id 
                     << "connecting sectors" << portal->sector_a << "and" << portal->sector_b;
            
            // Find neighbor sector
            int neighborSectorId = (portal->sector_a == sector.sector_id) ? portal->sector_b : portal->sector_a;
            const Sector *neighborSector = nullptr;
            for (const Sector &s : m_mapData.sectors) {
                if (s.sector_id == neighborSectorId) {
                    neighborSector = &s;
                    break;
                }
            }
            
            if (!neighborSector) {
                // Neighbor sector doesn't exist (was skipped during import)
                // Render this as a solid wall instead
                qDebug() << "Neighbor sector" << neighborSectorId << "not found - rendering as solid wall";
                
                // Fall through to solid wall rendering below
            } else {
                qDebug() << "Current sector" << sector.sector_id << ": floor=" << sector.floor_z << "ceiling=" << sector.ceiling_z;
                qDebug() << "Neighbor sector" << neighborSector->sector_id << ": floor=" << neighborSector->floor_z << "ceiling=" << neighborSector->ceiling_z;
                
                // Render UPPER wall (if neighbor ceiling is lower than current ceiling)
                if (neighborSector->ceiling_z < sector.ceiling_z) {
                    qDebug() << "Should render UPPER wall: texture_id=" << wall.texture_id_upper;
                    
                    if (wall.texture_id_upper > 0) {
                            QVector<float> vertices;
                            float u1 = 0.0f;
                            float u2 = length / 128.0f;
                            float v1 = 0.0f;
                            float v2 = 1.0f;
                            
                            // Upper section: from neighbor ceiling to current ceiling
                            vertices << wall.x1 << neighborSector->ceiling_z << wall.y1;
                            vertices << u1 << v1;
                            vertices << nx << ny << nz;
                            
                            vertices << wall.x2 << neighborSector->ceiling_z << wall.y2;
                            vertices << u2 << v1;
                            vertices << nx << ny << nz;
                            
                            vertices << wall.x2 << sector.ceiling_z << wall.y2;
                            vertices << u2 << v2;
                            vertices << nx << ny << nz;
                            
                            vertices << wall.x1 << neighborSector->ceiling_z << wall.y1;
                            vertices << u1 << v1;
                            vertices << nx << ny << nz;
                            
                            vertices << wall.x2 << sector.ceiling_z << wall.y2;
                            vertices << u2 << v2;
                            vertices << nx << ny << nz;
                            
                            vertices << wall.x1 << sector.ceiling_z << wall.y1;
                            vertices << u1 << v2;
                            vertices << nx << ny << nz;
                            
                            if (!vertices.isEmpty()) {
                                GeometryBuffer buffer;
                                buffer.vbo = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
                                buffer.vbo->create();
                                buffer.vbo->bind();
                                buffer.vbo->allocate(vertices.constData(), vertices.size() * sizeof(float));
                                
                                buffer.vao = new QOpenGLVertexArrayObject();
                                buffer.vao->create();
                                buffer.vao->bind();
                                
                                glEnableVertexAttribArray(0);
                                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
                                glEnableVertexAttribArray(1);
                                glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
                                glEnableVertexAttribArray(2);
                                glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
                                
                                buffer.vao->release();
                                buffer.vbo->release();
                                
                                buffer.vertexCount = vertices.size() / 8;
                                buffer.textureId = wall.texture_id_upper;
                                buffer.lightLevel = sector.light_level > 0 ? sector.light_level / 255.0f : 1.0f;
                                
                                m_wallBuffers.append(buffer);
                            }
                        }
                        
                        // Render LOWER wall (if current floor is lower than neighbor floor)
                        // This shows the step UP from current sector to neighbor sector
                        if (sector.floor_z < neighborSector->floor_z && wall.texture_id_lower > 0) {
                            QVector<float> vertices;
                            float u1 = 0.0f;
                            float u2 = length / 128.0f;
                            float v1 = 0.0f;
                            float v2 = 1.0f;
                            
                            qDebug() << "Rendering LOWER wall: sector" << sector.sector_id 
                                     << "floor=" << sector.floor_z << "neighbor floor=" << neighborSector->floor_z
                                     << "texture=" << wall.texture_id_lower;
                            
                            // Lower section: from current floor to neighbor floor
                            vertices << wall.x1 << sector.floor_z << wall.y1;
                            vertices << u1 << v1;
                            vertices << nx << ny << nz;
                            
                            vertices << wall.x2 << sector.floor_z << wall.y2;
                            vertices << u2 << v1;
                            vertices << nx << ny << nz;
                            
                            vertices << wall.x2 << neighborSector->floor_z << wall.y2;
                            vertices << u2 << v2;
                            vertices << nx << ny << nz;
                            
                            vertices << wall.x1 << sector.floor_z << wall.y1;
                            vertices << u1 << v1;
                            vertices << nx << ny << nz;
                            
                            vertices << wall.x2 << neighborSector->floor_z << wall.y2;
                            vertices << u2 << v2;
                            vertices << nx << ny << nz;
                            
                            vertices << wall.x1 << neighborSector->floor_z << wall.y1;
                            vertices << u1 << v2;
                            vertices << nx << ny << nz;
                            
                            if (!vertices.isEmpty()) {
                                GeometryBuffer buffer;
                                buffer.vbo = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
                                buffer.vbo->create();
                                buffer.vbo->bind();
                                buffer.vbo->allocate(vertices.constData(), vertices.size() * sizeof(float));
                                
                                buffer.vao = new QOpenGLVertexArrayObject();
                                buffer.vao->create();
                                buffer.vao->bind();
                                
                                glEnableVertexAttribArray(0);
                                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
                                glEnableVertexAttribArray(1);
                                glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
                                glEnableVertexAttribArray(2);
                                glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
                                
                                buffer.vao->release();
                                buffer.vbo->release();
                                
                                buffer.vertexCount = vertices.size() / 8;
                                buffer.textureId = wall.texture_id_lower;
                                buffer.lightLevel = sector.light_level > 0 ? sector.light_level / 255.0f : 1.0f;
                                
                                m_wallBuffers.append(buffer);
                            }
                        }
                }
                
                qDebug() << "Portal wall - skipping middle texture (texture_id_middle=" << wall.texture_id_middle << ")";
                continue; // Skip rendering middle texture for valid portals
            }
        }
        
        // This is a solid wall (or a portal to a non-existent sector) - render the full middle texture
        QVector<float> vertices;
        
        // Texture coordinates: stretch vertically to fit wall height
        float u1 = 0.0f;
        float u2 = length / 128.0f;  // Horizontal tiling based on wall length
        float v1 = 0.0f;
        float v2 = 1.0f;  // Stretch vertically to fit wall height (no tiling)
        
        float wallHeight = sector.ceiling_z - sector.floor_z;
        
        // Triangle 1
        vertices << wall.x1 << sector.floor_z << wall.y1;
        vertices << u1 << v1;
        vertices << nx << ny << nz;
        
        vertices << wall.x2 << sector.floor_z << wall.y2;
        vertices << u2 << v1;
        vertices << nx << ny << nz;
        
        vertices << wall.x2 << sector.ceiling_z << wall.y2;
        vertices << u2 << v2;
        vertices << nx << ny << nz;
        
        // Triangle 2
        vertices << wall.x1 << sector.floor_z << wall.y1;
        vertices << u1 << v1;
        vertices << nx << ny << nz;
        
        vertices << wall.x2 << sector.ceiling_z << wall.y2;
        vertices << u2 << v2;
        vertices << nx << ny << nz;
        
        vertices << wall.x1 << sector.ceiling_z << wall.y1;
        vertices << u1 << v2;
        vertices << nx << ny << nz;
        
        if (!vertices.isEmpty()) {
            GeometryBuffer buffer;
            buffer.vbo = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
            buffer.vbo->create();
            buffer.vbo->bind();
            buffer.vbo->allocate(vertices.constData(), vertices.size() * sizeof(float));
            
            buffer.vao = new QOpenGLVertexArrayObject();
            buffer.vao->create();
            buffer.vao->bind();
            
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
            
            buffer.vao->release();
            buffer.vbo->release();
            
            buffer.vertexCount = vertices.size() / 8;
            buffer.textureId = wall.texture_id_middle;
            buffer.lightLevel = sector.light_level > 0 ? sector.light_level / 255.0f : 1.0f;
            
            m_wallBuffers.append(buffer);
        }
    }
}

void VisualRenderer::clearGeometry()
{
    for (GeometryBuffer &buffer : m_wallBuffers) {
        delete buffer.vbo;
        delete buffer.vao;
    }
    m_wallBuffers.clear();
    
    for (GeometryBuffer &buffer : m_floorBuffers) {
        delete buffer.vbo;
        delete buffer.vao;
    }
    m_floorBuffers.clear();
    
    for (GeometryBuffer &buffer : m_ceilingBuffers) {
        delete buffer.vbo;
        delete buffer.vao;
    }
    m_ceilingBuffers.clear();
    
    for (GeometryBuffer &buffer : m_entityBuffers) {
        delete buffer.vbo;
        delete buffer.vao;
    }
    m_entityBuffers.clear();
}

void VisualRenderer::loadTexture(int id, const QImage &image)
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
    qDebug() << "Loaded texture ID" << id << "size:" << image.size() << "format:" << image.format();
}

void VisualRenderer::setCamera(float x, float y, float z, float yaw, float pitch)
{
    m_cameraX = x;
    m_cameraY = y;
    m_cameraZ = z;
    m_cameraYaw = yaw;
    m_cameraPitch = pitch;
    
    // Update view matrix
    m_viewMatrix.setToIdentity();
    
    // Apply pitch and yaw rotations
    m_viewMatrix.rotate(qRadiansToDegrees(-pitch), 1.0f, 0.0f, 0.0f);
    m_viewMatrix.rotate(qRadiansToDegrees(-yaw), 0.0f, 1.0f, 0.0f);
    
    // Apply camera translation
    m_viewMatrix.translate(-x, -y, -z);
}

void VisualRenderer::setProjection(float fov, float aspect, float nearPlane, float farPlane)
{
    m_projectionMatrix.setToIdentity();
    m_projectionMatrix.perspective(fov, aspect, nearPlane, farPlane);
}

// Redefined render to include Skybox
void VisualRenderer::render(int width, int height)
{
    if (!m_initialized) {
        return;
    }
    
    static int frameCount = 0;
    if (frameCount++ % 60 == 0) {
        qDebug() << "Rendering: camera=(" << m_cameraX << m_cameraY << m_cameraZ << ")"
                 << "yaw=" << qRadiansToDegrees(m_cameraYaw) << "pitch=" << qRadiansToDegrees(m_cameraPitch);
    }
    
    // Update projection aspect ratio
    setProjection(90.0f, (float)width / (float)height, 0.1f, 10000.0f);
    
    // Clear buffers
    glClearColor(0.4f, 0.6f, 0.9f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Use shader program
    m_shaderProgram->bind();
    
    // Draw Skybox (if available)
    if (m_skyTextureId > 0 && m_textures.contains(m_skyTextureId)) {
        drawSkybox(QVector3D(m_cameraX, m_cameraY, m_cameraZ));
        
        // Restore state
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
    }
    
    // Calculate MVP matrix for world
    QMatrix4x4 mvp = m_projectionMatrix * m_viewMatrix;
    m_shaderProgram->setUniformValue(m_uniformMVP, mvp);
    m_shaderProgram->setUniformValue(m_uniformTexture, 0);
    
    // Helper lambda for rendering buffers
    auto renderBuffers = [&](const QVector<GeometryBuffer> &buffers) {
        for (const GeometryBuffer &buffer : buffers) {
            QOpenGLTexture *texture = m_textures.value(buffer.textureId, m_defaultTexture);
            if (texture) texture->bind(0);
            
            m_shaderProgram->setUniformValue(m_uniformLightLevel, buffer.lightLevel);
            
            buffer.vao->bind();
            glDrawArrays(GL_TRIANGLES, 0, buffer.vertexCount);
            buffer.vao->release();
        }
    };
    
    // Disable culling for floors/ceilings to handle arbitrary winding order
    glDisable(GL_CULL_FACE);
    renderBuffers(m_floorBuffers);
    renderBuffers(m_ceilingBuffers);
    glEnable(GL_CULL_FACE);
    
    renderBuffers(m_wallBuffers);
    
    // Render entities (billboards / md3)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    renderBuffers(m_entityBuffers);
    glDisable(GL_BLEND);
    
    m_shaderProgram->release();
}

// 2D Parallax Skybox
void VisualRenderer::drawSkybox(const QVector3D &cameraPos)
{
    if (m_skyTextureId <= 0 || !m_textures.contains(m_skyTextureId)) return;
    
    QOpenGLTexture *tex = m_textures[m_skyTextureId];
    if (!tex) return;
    
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    
    tex->bind();
    
    m_shaderProgram->setUniformValue(m_uniformMVP, QMatrix4x4()); // Screen space
    m_shaderProgram->setUniformValue(m_uniformLightLevel, 1.0f); // Full bright
    
    // Parallax calculation
    float fovRatio = 90.0f / 360.0f; 
    float yawNorm = qRadiansToDegrees(m_cameraYaw) / 360.0f;
    float uStart = -yawNorm; 
    float uEnd = uStart + fovRatio;
    float pitchNorm = qRadiansToDegrees(m_cameraPitch) / 90.0f;
    float vShift = pitchNorm * 0.8f; 
    
    float v1 = 0.0f - vShift; // Bottom
    float v2 = 1.0f - vShift; // Top
    
    // Create quad data dynamic
    float quadData[] = {
        // X, Y, Z,   U, V,   NX, NY, NZ
        -1.0f, -1.0f, 0.99f,   uStart, v1,   0,0,1,
         1.0f, -1.0f, 0.99f,   uEnd, v1,     0,0,1,
         1.0f,  1.0f, 0.99f,   uEnd, v2,     0,0,1,
         
         1.0f,  1.0f, 0.99f,   uEnd, v2,     0,0,1,
        -1.0f,  1.0f, 0.99f,   uStart, v2,   0,0,1,
        -1.0f, -1.0f, 0.99f,   uStart, v1,   0,0,1
    };
    
    static QOpenGLBuffer vbo(QOpenGLBuffer::VertexBuffer);
    static QOpenGLVertexArrayObject vao;
    static bool init = false;
    
    if (!init) {
        if (vao.create() && vbo.create()) {
            init = true;
        }
    }
    
    if (init) {
        vao.bind();
        vbo.bind();
        vbo.allocate(quadData, sizeof(quadData)); // Dynamic update
        
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3*sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5*sizeof(float)));
        
        glDrawArrays(GL_TRIANGLES, 0, 6);
        
        vao.release();
        vbo.release();
    }
    
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
}
