#include "visualmodewidget.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QCursor>
#include <QApplication>
#include <QtMath>
#include <QDebug>

VisualModeWidget::VisualModeWidget(QWidget *parent)
    : QOpenGLWidget(parent)
    , m_renderer(nullptr)
    , m_updateTimer(new QTimer(this))
    , m_cameraX(384.0f)
    , m_cameraY(32.0f)
    , m_cameraZ(384.0f)
    , m_cameraYaw(0.0f)
    , m_cameraPitch(0.0f)
    , m_moveSpeed(200.0f)
    , m_strafeSpeed(200.0f)
    , m_verticalSpeed(150.0f)
    , m_mouseSensitivity(0.002f)
    , m_mouseCaptured(false)
    , m_firstMouse(true)
{
    setWindowTitle(tr("Modo Visual - RayMap Editor"));
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    resize(800, 600);
    
    // Enable keyboard focus
    setFocusPolicy(Qt::StrongFocus);
    
    // Set up update timer (60 FPS)
    connect(m_updateTimer, &QTimer::timeout, this, &VisualModeWidget::updateFrame);
    m_updateTimer->start(1000 / 60);
    
    m_frameTimer.start();
}

VisualModeWidget::~VisualModeWidget()
{
    makeCurrent();
    if (m_renderer) {
        m_renderer->cleanup();
        delete m_renderer;
    }
    doneCurrent();
}

void VisualModeWidget::initializeGL()
{
    initializeOpenGLFunctions();
    
    // Create renderer
    m_renderer = new VisualRenderer();
    if (!m_renderer->initialize()) {
        qWarning() << "Failed to initialize renderer";
        return;
    }
    
    qDebug() << "OpenGL initialized";
    qDebug() << "OpenGL Version:" << (const char*)glGetString(GL_VERSION);
    qDebug() << "GLSL Version:" << (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
    
    // Load map data if it was set before initialization
    if (!m_mapData.sectors.isEmpty()) {
        qDebug() << "Loading deferred map data...";
        
        // IMPORTANT: Load textures FIRST, before generating geometry
        qDebug() << "Loading textures first...";
        for (const TextureEntry &entry : m_mapData.textures) {
            m_renderer->loadTexture(entry.id, entry.pixmap.toImage());
        }
        qDebug() << "Loaded" << m_mapData.textures.size() << "textures";
        
        // NOW generate geometry (textures are already loaded)
        m_renderer->setMapData(m_mapData);
    }
}

void VisualModeWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void VisualModeWidget::paintGL()
{
    if (!m_renderer) {
        return;
    }
    
    // Update camera in renderer
    m_renderer->setCamera(m_cameraX, m_cameraY, m_cameraZ, m_cameraYaw, m_cameraPitch);
    
    // Render frame
    m_renderer->render(width(), height());
}

void VisualModeWidget::setMapData(const MapData &mapData, bool resetCamera)
{
    m_mapData = mapData;
    
    qDebug() << "setMapData called with" << mapData.sectors.size() << "sectors";
    
    if (m_renderer) {
        // Renderer already exists, load immediately
        makeCurrent();
        m_renderer->setMapData(mapData);
        doneCurrent();
    } else {
        // Renderer doesn't exist yet, data will be loaded in initializeGL()
        qDebug() << "Renderer not initialized yet, deferring map data load";
    }
    
    // Set camera to map's camera position if available and requested
    if (resetCamera && mapData.camera.enabled) {
        m_cameraX = mapData.camera.x;
        m_cameraY = mapData.camera.z > 0 ? mapData.camera.z : 32.0f;
        m_cameraZ = mapData.camera.y;
        m_cameraYaw = mapData.camera.rotation;
        m_cameraPitch = mapData.camera.pitch;
    }
    
    qDebug() << "Visual Mode: Map data set, camera at" << m_cameraX << m_cameraY << m_cameraZ;
}

void VisualModeWidget::loadTexture(int id, const QImage &image)
{
    // Store texture in map data
    bool found = false;
    for (TextureEntry &entry : m_mapData.textures) {
        if (entry.id == id) {
            entry.pixmap = QPixmap::fromImage(image);
            found = true;
            break;
        }
    }
    
    if (!found) {
        TextureEntry entry;
        entry.id = id;
        entry.pixmap = QPixmap::fromImage(image);
        m_mapData.textures.append(entry);
    }
    
    // Load to renderer if it exists
    if (m_renderer) {
        makeCurrent();
        m_renderer->loadTexture(id, image);
        doneCurrent();
    }
}

void VisualModeWidget::setCameraPosition(float x, float y, float z)
{
    m_cameraX = x;
    m_cameraY = y;
    m_cameraZ = z;
}

void VisualModeWidget::setCameraRotation(float yaw, float pitch)
{
    m_cameraYaw = yaw;
    m_cameraPitch = pitch;
}

void VisualModeWidget::updateFrame()
{
    // Calculate delta time
    float deltaTime = m_frameTimer.elapsed() / 1000.0f;
    m_frameTimer.restart();
    
    // Clamp delta time to avoid huge jumps
    if (deltaTime > 0.1f) {
        deltaTime = 0.1f;
    }
    
    // Update camera based on input
    updateCamera(deltaTime);
    
    // Trigger repaint
    update();
}

void VisualModeWidget::updateCamera(float deltaTime)
{
    // Calculate forward and right vectors (matching raycasting coordinate system)
    // In raycasting: angle 0 = looking along +X axis
    float forwardX = qCos(m_cameraYaw);
    float forwardZ = qSin(m_cameraYaw);
    // Right vector should be -90 degrees (Clockwise) relative to forward if Y is Down?
    // Let's use standard -PI/2 for Right vector in a math system where angles increase CCW.
    float rightX = qCos(m_cameraYaw - M_PI/2.0f);
    float rightZ = qSin(m_cameraYaw - M_PI/2.0f);
    
    // Movement
    if (m_keysPressed.contains(Qt::Key_W)) {
        m_cameraX += forwardX * m_moveSpeed * deltaTime;
        m_cameraZ += forwardZ * m_moveSpeed * deltaTime;
    }
    if (m_keysPressed.contains(Qt::Key_S)) {
        m_cameraX -= forwardX * m_moveSpeed * deltaTime;
        m_cameraZ -= forwardZ * m_moveSpeed * deltaTime;
    }
    if (m_keysPressed.contains(Qt::Key_A)) {
        m_cameraX -= rightX * m_strafeSpeed * deltaTime;
        m_cameraZ -= rightZ * m_strafeSpeed * deltaTime;
    }
    if (m_keysPressed.contains(Qt::Key_D)) {
        m_cameraX += rightX * m_strafeSpeed * deltaTime;
        m_cameraZ += rightZ * m_strafeSpeed * deltaTime;
    }
    
    // Vertical movement - Q to go up, E to go down
    if (m_keysPressed.contains(Qt::Key_Q)) {
        m_cameraY += m_verticalSpeed * deltaTime;
        static int debugQ = 0;
        if (debugQ++ % 60 == 0) qDebug() << "Q pressed - moving up, Y=" << m_cameraY;
    }
    if (m_keysPressed.contains(Qt::Key_E)) {
        m_cameraY -= m_verticalSpeed * deltaTime;
        static int debugE = 0;
        if (debugE++ % 60 == 0) qDebug() << "E pressed - moving down, Y=" << m_cameraY;
    }
    
    // Clamp pitch to avoid gimbal lock
    if (m_cameraPitch > M_PI / 2.0f - 0.01f) {
        m_cameraPitch = M_PI / 2.0f - 0.01f;
    }
    if (m_cameraPitch < -M_PI / 2.0f + 0.01f) {
        m_cameraPitch = -M_PI / 2.0f + 0.01f;
    }
}

void VisualModeWidget::keyPressEvent(QKeyEvent *event)
{
    m_keysPressed.insert(event->key());
    
    // ESC to release mouse
    if (event->key() == Qt::Key_Escape) {
        releaseMouse();
    }
    
    // F11 to toggle fullscreen
    if (event->key() == Qt::Key_F11) {
        if (isFullScreen()) {
            showNormal();
        } else {
            showFullScreen();
        }
    }
    
    QOpenGLWidget::keyPressEvent(event);
}

void VisualModeWidget::keyReleaseEvent(QKeyEvent *event)
{
    m_keysPressed.remove(event->key());
    QOpenGLWidget::keyReleaseEvent(event);
}

void VisualModeWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_mouseCaptured) {
        QOpenGLWidget::mouseMoveEvent(event);
        return;
    }
    
    if (m_firstMouse) {
        m_lastMousePos = event->pos();
        m_firstMouse = false;
        return;
    }
    
    // Calculate mouse delta
    int deltaX = event->pos().x() - m_lastMousePos.x();
    int deltaY = event->pos().y() - m_lastMousePos.y();
    
    // Update camera rotation
    m_cameraYaw -= deltaX * m_mouseSensitivity; // Inverted X axis
    m_cameraPitch -= deltaY * m_mouseSensitivity;
    
    // Wrap yaw to [0, 2*PI]
    while (m_cameraYaw < 0) m_cameraYaw += 2.0f * M_PI;
    while (m_cameraYaw >= 2.0f * M_PI) m_cameraYaw -= 2.0f * M_PI;
    
    // Recenter cursor
    QPoint center = QPoint(width() / 2, height() / 2);
    QCursor::setPos(mapToGlobal(center));
    m_lastMousePos = center;
    
    QOpenGLWidget::mouseMoveEvent(event);
}

void VisualModeWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !m_mouseCaptured) {
        captureMouse();
    }
    
    QOpenGLWidget::mousePressEvent(event);
}

void VisualModeWidget::focusInEvent(QFocusEvent *event)
{
    QOpenGLWidget::focusInEvent(event);
}

void VisualModeWidget::focusOutEvent(QFocusEvent *event)
{
    // Release mouse when losing focus
    releaseMouse();
    m_keysPressed.clear();
    
    QOpenGLWidget::focusOutEvent(event);
}

void VisualModeWidget::captureMouse()
{
    if (m_mouseCaptured) {
        return;
    }
    
    setCursor(Qt::BlankCursor);
    m_mouseCaptured = true;
    m_firstMouse = true;
    
    // Center cursor
    QPoint center = QPoint(width() / 2, height() / 2);
    QCursor::setPos(mapToGlobal(center));
    m_lastMousePos = center;
    
    qDebug() << "Mouse captured - use ESC to release";
}

void VisualModeWidget::releaseMouse()
{
    if (!m_mouseCaptured) {
        return;
    }
    
    setCursor(Qt::ArrowCursor);
    m_mouseCaptured = false;
    
    qDebug() << "Mouse released";
}
