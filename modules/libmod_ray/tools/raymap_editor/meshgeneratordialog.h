#ifndef MESHGENERATORDIALOG_H
#define MESHGENERATORDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QVBoxLayout>
#include "modelpreviewwidget.h"

class MeshGeneratorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MeshGeneratorDialog(QWidget *parent = nullptr);
    
    struct MeshParams {
        enum Type { RAMP, STAIRS, CYLINDER, BOX, BRIDGE, HOUSE, ARCH };
        Type type;
        
        float width;
        float height;
        float depth;
        int segments; // For stairs (steps) or cylinder (sides)
        
        // Multi-texture support
        QStringList texturePaths; // Multiple textures (roof, walls, floor, etc.)
        QString texturePath;      // Legacy single texture (for compatibility)
        QString exportPath;
        
        // Type-specific options
        bool hasRailings = false;      // For bridges
        bool hasArch = false;          // For bridges (arch underneath)
        enum RoofType { FLAT, SLOPED, GABLED } roofType = FLAT; // For houses
    };
    
    MeshParams getParameters() const;
    QString getExportPath() const;

private slots:
    void onBrowseTexture();
    void onBrowseExport();
    void onTypeChanged(int index);
    void updatePreview(); // New slot

private:
    ModelPreviewWidget *m_previewWidget; // New widget
    QComboBox *m_typeCombo;
    QDoubleSpinBox *m_widthSpin;
    QDoubleSpinBox *m_heightSpin;
    QDoubleSpinBox *m_depthSpin;
    QSpinBox *m_segmentsSpin;
    QLabel *m_segmentsLabel;
    
    QLineEdit *m_texturePathEdit;
    QLineEdit *m_exportPathEdit;
    
    // Type-specific controls (shown/hidden dynamically)
    QCheckBox *m_railingsCheck;
    QLabel *m_railingsLabel;
    QCheckBox *m_archCheck;
    QLabel *m_archLabel;
    QComboBox *m_roofTypeCombo;
    QLabel *m_roofTypeLabel;
    
    // Multi-texture support
    QVector<QLineEdit*> m_textureEdits;
    QVector<QPushButton*> m_textureBrowseBtns;
    QVector<QLabel*> m_textureLabels;
    QWidget *m_texturesWidget;
    QVBoxLayout *m_texturesLayout;
};

#endif // MESHGENERATORDIALOG_H
