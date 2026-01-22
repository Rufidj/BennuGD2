#ifndef RAMPGENERATORDIALOG_H
#define RAMPGENERATORDIALOG_H

#include <QDialog>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include "rampgenerator.h"

class RampGeneratorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RampGeneratorDialog(QWidget *parent = nullptr);
    ~RampGeneratorDialog();
    
    RampParameters getParameters() const;
    void setStartPoint(const QPointF &point);
    void setEndPoint(const QPointF &point);
    void setTextures(const QMap<int, QPixmap> &textures);

private slots:
    void onGenerateClicked();
    void onCancelClicked();
    void onTypeChanged(int index);
    void updatePreview();
    void onSelectFloorTexture();
    void onSelectCeilingTexture();
    void onSelectWallTexture();

private:
    void setupUI();
    void connectSignals();
    
    // UI Components
    QComboBox *m_typeCombo;
    QDoubleSpinBox *m_startXSpin;
    QDoubleSpinBox *m_startYSpin;
    QDoubleSpinBox *m_endXSpin;
    QDoubleSpinBox *m_endYSpin;
    QDoubleSpinBox *m_startHeightSpin;
    QDoubleSpinBox *m_endHeightSpin;
    QDoubleSpinBox *m_widthSpin;
    QSpinBox *m_segmentsSpin;
    QDoubleSpinBox *m_ceilingHeightSpin;
    QSpinBox *m_floorTextureSpin;
    QSpinBox *m_ceilingTextureSpin;
    QSpinBox *m_wallTextureSpin;
    QCheckBox *m_stairsCheckbox;
    
    QLabel *m_previewLabel;
    QPushButton *m_generateButton;
    QPushButton *m_cancelButton;
    
    RampParameters m_params;
    QMap<int, QPixmap> m_textureMap;
};

#endif // RAMPGENERATORDIALOG_H
