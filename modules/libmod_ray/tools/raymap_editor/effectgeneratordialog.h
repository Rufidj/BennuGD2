#ifndef EFFECTGENERATORDIALOG_H
#define EFFECTGENERATORDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QSlider>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QGroupBox>
#include <QFormLayout>
#include <QColorDialog>
#include <QCheckBox>
#include "effectgenerator.h"

class EffectGeneratorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EffectGeneratorDialog(QWidget *parent = nullptr);
    ~EffectGeneratorDialog();

private slots:
    void onEffectTypeChanged(int index);
    void onParameterChanged();
    void onRegenerateClicked();
    void onPlayClicked();
    void onStopClicked();
    void onAnimationTick();
    void onExportClicked();
    void onPresetChanged(int index);
    void onSavePresetClicked();
    void onColorPicker1();
    void onColorPicker2();

private:
    void setupUI();
    void updateParameterControls();
    void generateEffect();
    void loadPreset(int index);
    void savePreset();
    
    // UI Components
    QComboBox *m_effectTypeCombo;
    QComboBox *m_presetCombo;
    QComboBox *m_sizeCombo;
    QSpinBox *m_framesSpinBox;
    QSpinBox *m_fpsSpinBox;
    QSpinBox *m_seedSpinBox;
    
    // Dynamic parameters
    QGroupBox *m_paramsGroup;
    QFormLayout *m_paramsLayout;
    QSlider *m_intensitySlider;
    QSlider *m_speedSlider;
    QSlider *m_radiusSlider;
    QSlider *m_turbulenceSlider;
    QSlider *m_gravitySlider;
    QSlider *m_dispersionSlider;
    QSlider *m_fadeRateSlider;
    QSpinBox *m_particleCountSpinBox;
    QPushButton *m_color1Button;
    QPushButton *m_color2Button;
    QCheckBox *m_debrisCheckBox;
    QCheckBox *m_sparksCheckBox;
    QCheckBox *m_trailsCheckBox;
    
    // Preview
    QLabel *m_previewLabel;
    QLabel *m_frameLabel;
    QPushButton *m_playButton;
    QPushButton *m_stopButton;
    QSlider *m_frameSlider;
    
    // Generator
    EffectGenerator m_generator;
    QVector<QImage> m_frames;
    QTimer *m_animationTimer;
    int m_currentFrame;
    bool m_isPlaying;
    
    // Current params
    EffectParams m_params;
    QColor m_color1;
    QColor m_color2;
};

#endif // EFFECTGENERATORDIALOG_H
