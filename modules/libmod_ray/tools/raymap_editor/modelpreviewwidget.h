#ifndef MODELPREVIEWWIDGET_H
#define MODELPREVIEWWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLTexture>
#include <QVector3D>
#include <QMatrix4x4>
#include "md3generator.h"

class ModelPreviewWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit ModelPreviewWidget(QWidget *parent = nullptr);
    ~ModelPreviewWidget();
    
    void setMesh(const MD3Generator::MeshData &mesh);
    void setTexture(const QString &path);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    
private:
    MD3Generator::MeshData m_mesh;
    QOpenGLTexture *m_texture = nullptr;
    QString m_texturePath;
    
    QMatrix4x4 m_projection;
    QMatrix4x4 m_view;
    QMatrix4x4 m_model;
    
    QPoint m_lastPos;
    float m_xRot;
    float m_yRot;
    float m_zoom;
};

#endif // MODELPREVIEWWIDGET_H
