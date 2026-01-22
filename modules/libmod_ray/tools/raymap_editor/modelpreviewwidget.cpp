#include "modelpreviewwidget.h"
#include <QMouseEvent>
#include <QtMath>
#include <QtGlobal>

ModelPreviewWidget::ModelPreviewWidget(QWidget *parent)
    : QOpenGLWidget(parent)
    , m_xRot(30.0f)
    , m_yRot(-45.0f)
    , m_zoom(-300.0f)
{
}

ModelPreviewWidget::~ModelPreviewWidget()
{
}

void ModelPreviewWidget::setMesh(const MD3Generator::MeshData &mesh)
{
    m_mesh = mesh;
    update();
}

void ModelPreviewWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE); // Disable culling to see all faces (helps debug geometry)
}

void ModelPreviewWidget::resizeGL(int w, int h)
{
    m_projection.setToIdentity();
    m_projection.perspective(45.0f, GLfloat(w) / h, 1.0f, 1000.0f);
}

void ModelPreviewWidget::setTexture(const QString &path)
{
    if (m_texturePath == path) return;
    m_texturePath = path;
    
    if (m_texture) {
        delete m_texture;
        m_texture = nullptr;
    }
    
    if (!path.isEmpty() && QFile::exists(path)) {
        QImage img(path);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        m_texture = new QOpenGLTexture(img.flipped(Qt::Vertical));
#else
        m_texture = new QOpenGLTexture(img.mirrored());
#endif
        m_texture->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
        m_texture->setMagnificationFilter(QOpenGLTexture::Linear);
    }
    update();
}

void ModelPreviewWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    m_view.setToIdentity();
    m_view.translate(0.0f, 0.0f, m_zoom);
    m_view.rotate(m_xRot, 1.0f, 0.0f, 0.0f);
    m_view.rotate(m_yRot, 0.0f, 1.0f, 0.0f);
    m_view.rotate(-90.0f, 1.0f, 0.0f, 0.0f);
    
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(m_projection.constData());
    
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(m_view.constData());
    
    // Draw Axis
    glDisable(GL_TEXTURE_2D);
    glBegin(GL_LINES);
    glColor3f(1, 0, 0); glVertex3f(0, 0, 0); glVertex3f(50, 0, 0);
    glColor3f(0, 1, 0); glVertex3f(0, 0, 0); glVertex3f(0, 50, 0);
    glColor3f(0, 0, 1); glVertex3f(0, 0, 0); glVertex3f(0, 0, 50);
    glEnd();
    
    // Draw Mesh
    if (m_mesh.vertices.isEmpty()) return;
    
    if (m_texture) {
        glEnable(GL_TEXTURE_2D);
        m_texture->bind();
        glColor3f(1, 1, 1);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    } else {
        glDisable(GL_TEXTURE_2D);
        glColor3f(0.8f, 0.8f, 0.8f);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // Wireframe if no texture
    }
    
    glBegin(GL_TRIANGLES);
    for (int idx : m_mesh.indices) {
        if (idx < 0 || idx >= m_mesh.vertices.size()) continue;
        const auto &v = m_mesh.vertices[idx];
        if (m_texture) glTexCoord2f(v.uv.x(), v.uv.y()); // Add UVs
        glVertex3f(v.pos.x(), v.pos.y(), v.pos.z());
    }
    glEnd();
    
    if (m_texture) {
        m_texture->release();
        glDisable(GL_TEXTURE_2D);
    }
}

void ModelPreviewWidget::mousePressEvent(QMouseEvent *event)
{
    m_lastPos = event->pos();
}

void ModelPreviewWidget::mouseMoveEvent(QMouseEvent *event)
{
    // Qt6 compatible way might be position().toPoint()
    QPoint currentPos = event->pos(); // pos() is still available in 6.0 but deprecated in newer? 
    // The warning said x() is deprecated.
    
    int dx = currentPos.x() - m_lastPos.x();
    int dy = currentPos.y() - m_lastPos.y();
    
    if (event->buttons() & Qt::LeftButton) {
        m_xRot += dy;
        m_yRot += dx;
        update();
    } else if (event->buttons() & Qt::RightButton) {
        m_zoom += dy;
        update();
    }
    
    m_lastPos = event->pos();
}
