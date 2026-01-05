#ifndef VISUALMODEWIDGET_H
#define VISUALMODEWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QTimer>
#include <QElapsedTimer>
#include <QSet>
#include <QPoint>
#include "visualrenderer.h"
#include "mapdata.h"

/**
 * VisualModeWidget - 3D view widget for the Visual Mode
 * 
 * Provides a first-person 3D view of the map with WASD+mouse camera controls.
 * Similar to Doom Builder's Visual Mode.
 */
class VisualModeWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
    
public:
    explicit VisualModeWidget(QWidget *parent = nullptr);
    ~VisualModeWidget();
    
    // Map data
    void setMapData(const MapData &mapData, bool resetCamera = true);
    void loadTexture(int id, const QImage &image);
    
    // Camera control
    void setCameraPosition(float x, float y, float z);
    void setCameraRotation(float yaw, float pitch);
    
protected:
    // QOpenGLWidget overrides
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    
    // Event handlers
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    
private slots:
    void updateFrame();
    
private:
    void updateCamera(float deltaTime);
    void captureMouse();
    void releaseMouse();
    
    // Renderer
    VisualRenderer *m_renderer;
    
    // Update timer
    QTimer *m_updateTimer;
    QElapsedTimer m_frameTimer;
    
    // Camera state
    float m_cameraX;
    float m_cameraY;
    float m_cameraZ;
    float m_cameraYaw;    // Rotation around Y axis (left/right)
    float m_cameraPitch;  // Rotation around X axis (up/down)
    
    // Camera movement speeds
    float m_moveSpeed;      // Units per second
    float m_strafeSpeed;    // Units per second
    float m_verticalSpeed;  // Units per second
    float m_mouseSensitivity; // Radians per pixel
    
    // Input state
    QSet<int> m_keysPressed;
    QPoint m_lastMousePos;
    bool m_mouseCaptured;
    bool m_firstMouse;
    
    // Map data reference
    MapData m_mapData;
};

#endif // VISUALMODEWIDGET_H
