#include "effectgeneratordialog.h"
#include "fpgloader.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMessageBox>
#include <QFileDialog>
#include <QScrollArea>
#include <QLineEdit>

EffectGeneratorDialog::EffectGeneratorDialog(QWidget *parent)
    : QDialog(parent)
    , m_animationTimer(new QTimer(this))
    , m_currentFrame(0)
    , m_isPlaying(false)
    , m_color1(Qt::white)
    , m_color2(Qt::black)
{
    setupUI();
    setWindowTitle(tr("Generador de Efectos"));
    resize(1000, 700);
    
    connect(m_animationTimer, &QTimer::timeout, this, &EffectGeneratorDialog::onAnimationTick);
    
    // Initialize with default effect (Explosion)
    updateParameterControls();
    generateEffect();
}

EffectGeneratorDialog::~EffectGeneratorDialog()
{
}

void EffectGeneratorDialog::setupUI()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    
    // Left panel - Controls
    QVBoxLayout *leftLayout = new QVBoxLayout();
    
    // Effect type
    QGroupBox *typeGroup = new QGroupBox(tr("Tipo de Efecto"));
    QVBoxLayout *typeLayout = new QVBoxLayout(typeGroup);
    
    m_effectTypeCombo = new QComboBox();
    m_effectTypeCombo->addItem(tr("Explosión"), EFFECT_EXPLOSION);
    m_effectTypeCombo->addItem(tr("Humo"), EFFECT_SMOKE);
    m_effectTypeCombo->addItem(tr("Fuego"), EFFECT_FIRE);
    m_effectTypeCombo->addItem(tr("Partículas"), EFFECT_PARTICLES);
    m_effectTypeCombo->addItem(tr("Agua"), EFFECT_WATER);
    m_effectTypeCombo->addItem(tr("Energía"), EFFECT_ENERGY);
    m_effectTypeCombo->addItem(tr("Impacto"), EFFECT_IMPACT);
    connect(m_effectTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EffectGeneratorDialog::onEffectTypeChanged);
    typeLayout->addWidget(m_effectTypeCombo);
    
    leftLayout->addWidget(typeGroup);
    
    // General parameters
    QGroupBox *generalGroup = new QGroupBox(tr("Parámetros Generales"));
    QFormLayout *generalLayout = new QFormLayout(generalGroup);
    
    m_framesSpinBox = new QSpinBox();
    m_framesSpinBox->setRange(1, 120);
    m_framesSpinBox->setValue(30);
    connect(m_framesSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &EffectGeneratorDialog::onParameterChanged);
    generalLayout->addRow(tr("Frames:"), m_framesSpinBox);
    
    m_sizeCombo = new QComboBox();
    m_sizeCombo->addItem("32x32", 32);
    m_sizeCombo->addItem("64x64", 64);
    m_sizeCombo->addItem("128x128", 128);
    m_sizeCombo->addItem("256x256", 256);
    m_sizeCombo->addItem("512x512", 512);
    m_sizeCombo->setCurrentIndex(2); // 128x128
    connect(m_sizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EffectGeneratorDialog::onParameterChanged);
    generalLayout->addRow(tr("Tamaño:"), m_sizeCombo);
    
    m_fpsSpinBox = new QSpinBox();
    m_fpsSpinBox->setRange(1, 60);
    m_fpsSpinBox->setValue(12);
    generalLayout->addRow(tr("FPS Preview:"), m_fpsSpinBox);
    
    m_seedSpinBox = new QSpinBox();
    m_seedSpinBox->setRange(0, 99999);
    m_seedSpinBox->setValue(0);
    m_seedSpinBox->setSpecialValueText(tr("Aleatorio"));
    connect(m_seedSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &EffectGeneratorDialog::onParameterChanged);
    generalLayout->addRow(tr("Semilla:"), m_seedSpinBox);
    
    leftLayout->addWidget(generalGroup);
    
    // Dynamic parameters
    m_paramsGroup = new QGroupBox(tr("Parámetros del Efecto"));
    m_paramsLayout = new QFormLayout(m_paramsGroup);
    leftLayout->addWidget(m_paramsGroup);
    
    // Presets
    QGroupBox *presetGroup = new QGroupBox(tr("Presets"));
    QVBoxLayout *presetLayout = new QVBoxLayout(presetGroup);
    
    m_presetCombo = new QComboBox();
    m_presetCombo->addItem(tr("Personalizado"));
    m_presetCombo->addItem(tr("Explosión Pequeña"));
    m_presetCombo->addItem(tr("Explosión Grande"));
    m_presetCombo->addItem(tr("Humo Denso"));
    m_presetCombo->addItem(tr("Humo Ligero"));
    m_presetCombo->addItem(tr("Fuego Pequeño"));
    m_presetCombo->addItem(tr("Fuego Grande"));
    m_presetCombo->addItem(tr("Chispas"));
    m_presetCombo->addItem(tr("Salpicadura"));
    m_presetCombo->addItem(tr("Rayo Mágico"));
    m_presetCombo->addItem(tr("Polvo"));
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EffectGeneratorDialog::onPresetChanged);
    presetLayout->addWidget(m_presetCombo);
    
    leftLayout->addWidget(presetGroup);
    leftLayout->addStretch();
    
    QScrollArea *scrollArea = new QScrollArea();
    QWidget *scrollWidget = new QWidget();
    scrollWidget->setLayout(leftLayout);
    scrollArea->setWidget(scrollWidget);
    scrollArea->setWidgetResizable(true);
    scrollArea->setMinimumWidth(350);
    
    mainLayout->addWidget(scrollArea);
    
    // Right panel - Preview
    QVBoxLayout *rightLayout = new QVBoxLayout();
    
    QGroupBox *previewGroup = new QGroupBox(tr("Vista Previa"));
    QVBoxLayout *previewLayout = new QVBoxLayout(previewGroup);
    
    m_previewLabel = new QLabel();
    m_previewLabel->setMinimumSize(400, 400);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet("QLabel { background-color: #2b2b2b; }");
    previewLayout->addWidget(m_previewLabel);
    
    m_frameLabel = new QLabel(tr("Frame: 0/0"));
    previewLayout->addWidget(m_frameLabel);
    
    m_frameSlider = new QSlider(Qt::Horizontal);
    m_frameSlider->setRange(0, 0);
    connect(m_frameSlider, &QSlider::valueChanged, [this](int value) {
        if (!m_isPlaying && !m_frames.isEmpty()) {
            m_currentFrame = value;
            m_previewLabel->setPixmap(QPixmap::fromImage(m_frames[m_currentFrame]));
            m_frameLabel->setText(tr("Frame: %1/%2").arg(m_currentFrame + 1).arg(m_frames.size()));
        }
    });
    previewLayout->addWidget(m_frameSlider);
    
    QHBoxLayout *controlsLayout = new QHBoxLayout();
    m_playButton = new QPushButton(tr("▶ Reproducir"));
    m_playButton->setEnabled(false);
    connect(m_playButton, &QPushButton::clicked, this, &EffectGeneratorDialog::onPlayClicked);
    controlsLayout->addWidget(m_playButton);
    
    m_stopButton = new QPushButton(tr("⏹ Detener"));
    m_stopButton->setEnabled(false);
    connect(m_stopButton, &QPushButton::clicked, this, &EffectGeneratorDialog::onStopClicked);
    controlsLayout->addWidget(m_stopButton);
    
    previewLayout->addLayout(controlsLayout);
    
    rightLayout->addWidget(previewGroup);
    
    // Action buttons
    QHBoxLayout *actionLayout = new QHBoxLayout();
    
    QPushButton *regenerateButton = new QPushButton(tr("Regenerar"));
    connect(regenerateButton, &QPushButton::clicked, this, &EffectGeneratorDialog::onRegenerateClicked);
    actionLayout->addWidget(regenerateButton);
    
    QPushButton *exportButton = new QPushButton(tr("Exportar FPG..."));
    connect(exportButton, &QPushButton::clicked, this, &EffectGeneratorDialog::onExportClicked);
    actionLayout->addWidget(exportButton);
    
    rightLayout->addLayout(actionLayout);
    
    QPushButton *closeButton = new QPushButton(tr("Cerrar"));
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    rightLayout->addWidget(closeButton);
    
    mainLayout->addLayout(rightLayout);
}

void EffectGeneratorDialog::updateParameterControls()
{
    qDebug() << "=== updateParameterControls START ===";
    
    // Clear existing controls
    while (m_paramsLayout->count() > 0) {
        QLayoutItem *item = m_paramsLayout->takeAt(0);
        if (item->widget()) {
            qDebug() << "Deleting widget:" << item->widget()->metaObject()->className();
            item->widget()->deleteLater();
        }
        delete item;
    }
    
    qDebug() << "Cleared existing controls";
    
    // Initialize all pointers to nullptr to avoid dangling pointers
    m_particleCountSpinBox = nullptr;
    m_intensitySlider = nullptr;
    m_speedSlider = nullptr;
    m_radiusSlider = nullptr;
    m_turbulenceSlider = nullptr;
    m_gravitySlider = nullptr;
    m_dispersionSlider = nullptr;
    m_fadeRateSlider = nullptr;
    m_debrisCheckBox = nullptr;
    m_sparksCheckBox = nullptr;
    m_trailsCheckBox = nullptr;
    m_color1Button = nullptr;
    m_color2Button = nullptr;
    
    qDebug() << "Initialized all pointers to nullptr";
    
    EffectType type = (EffectType)m_effectTypeCombo->currentData().toInt();
    qDebug() << "Effect type:" << type;
    
    // Particle count (all effects)
    m_particleCountSpinBox = new QSpinBox();
    m_particleCountSpinBox->setRange(10, 5000);
    m_particleCountSpinBox->setValue(100);
    connect(m_particleCountSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &EffectGeneratorDialog::onParameterChanged);
    m_paramsLayout->addRow(tr("Partículas:"), m_particleCountSpinBox);
    
    // Intensity
    m_intensitySlider = new QSlider(Qt::Horizontal);
    m_intensitySlider->setRange(0, 100);
    m_intensitySlider->setValue(50);
    connect(m_intensitySlider, &QSlider::valueChanged,
            this, &EffectGeneratorDialog::onParameterChanged);
    m_paramsLayout->addRow(tr("Intensidad:"), m_intensitySlider);
    
    // Speed
    m_speedSlider = new QSlider(Qt::Horizontal);
    m_speedSlider->setRange(1, 50);
    m_speedSlider->setValue(10);
    connect(m_speedSlider, &QSlider::valueChanged,
            this, &EffectGeneratorDialog::onParameterChanged);
    m_paramsLayout->addRow(tr("Velocidad:"), m_speedSlider);
    
    // Colors
    m_color1Button = new QPushButton();
    m_color1Button->setStyleSheet(QString("background-color: %1").arg(m_color1.name()));
    connect(m_color1Button, &QPushButton::clicked, this, &EffectGeneratorDialog::onColorPicker1);
    m_paramsLayout->addRow(tr("Color 1:"), m_color1Button);
    
    m_color2Button = new QPushButton();
    m_color2Button->setStyleSheet(QString("background-color: %1").arg(m_color2.name()));
    connect(m_color2Button, &QPushButton::clicked, this, &EffectGeneratorDialog::onColorPicker2);
    m_paramsLayout->addRow(tr("Color 2:"), m_color2Button);
    
    // Type-specific parameters
    switch (type) {
        case EFFECT_EXPLOSION:
            m_radiusSlider = new QSlider(Qt::Horizontal);
            m_radiusSlider->setRange(10, 200);
            m_radiusSlider->setValue(50);
            connect(m_radiusSlider, &QSlider::valueChanged,
                    this, &EffectGeneratorDialog::onParameterChanged);
            m_paramsLayout->addRow(tr("Radio:"), m_radiusSlider);
            
            m_debrisCheckBox = new QCheckBox(tr("Escombros"));
            connect(m_debrisCheckBox, &QCheckBox::toggled,
                    this, &EffectGeneratorDialog::onParameterChanged);
            m_paramsLayout->addRow("", m_debrisCheckBox);
            break;
            
        case EFFECT_SMOKE:
            m_turbulenceSlider = new QSlider(Qt::Horizontal);
            m_turbulenceSlider->setRange(0, 100);
            m_turbulenceSlider->setValue(50);
            connect(m_turbulenceSlider, &QSlider::valueChanged,
                    this, &EffectGeneratorDialog::onParameterChanged);
            m_paramsLayout->addRow(tr("Turbulencia:"), m_turbulenceSlider);
            
            m_dispersionSlider = new QSlider(Qt::Horizontal);
            m_dispersionSlider->setRange(0, 100);
            m_dispersionSlider->setValue(50);
            connect(m_dispersionSlider, &QSlider::valueChanged,
                    this, &EffectGeneratorDialog::onParameterChanged);
            m_paramsLayout->addRow(tr("Dispersión:"), m_dispersionSlider);
            
            m_fadeRateSlider = new QSlider(Qt::Horizontal);
            m_fadeRateSlider->setRange(1, 100);
            m_fadeRateSlider->setValue(5);
            connect(m_fadeRateSlider, &QSlider::valueChanged,
                    this, &EffectGeneratorDialog::onParameterChanged);
            m_paramsLayout->addRow(tr("Desvanecimiento:"), m_fadeRateSlider);
            break;
            
        case EFFECT_FIRE:
            m_sparksCheckBox = new QCheckBox(tr("Chispas"));
            connect(m_sparksCheckBox, &QCheckBox::toggled,
                    this, &EffectGeneratorDialog::onParameterChanged);
            m_paramsLayout->addRow("", m_sparksCheckBox);
            break;
            
        case EFFECT_PARTICLES:
            m_gravitySlider = new QSlider(Qt::Horizontal);
            m_gravitySlider->setRange(-50, 50);
            m_gravitySlider->setValue(0);
            connect(m_gravitySlider, &QSlider::valueChanged,
                    this, &EffectGeneratorDialog::onParameterChanged);
            m_paramsLayout->addRow(tr("Gravedad:"), m_gravitySlider);
            
            m_trailsCheckBox = new QCheckBox(tr("Estelas"));
            connect(m_trailsCheckBox, &QCheckBox::toggled,
                    this, &EffectGeneratorDialog::onParameterChanged);
            m_paramsLayout->addRow("", m_trailsCheckBox);
            break;
            
        case EFFECT_WATER:
        case EFFECT_ENERGY:
            m_radiusSlider = new QSlider(Qt::Horizontal);
            m_radiusSlider->setRange(10, 200);
            m_radiusSlider->setValue(50);
            connect(m_radiusSlider, &QSlider::valueChanged,
                    this, &EffectGeneratorDialog::onParameterChanged);
            m_paramsLayout->addRow(tr("Radio:"), m_radiusSlider);
            break;
            
        case EFFECT_IMPACT:
            m_debrisCheckBox = new QCheckBox(tr("Escombros"));
            m_debrisCheckBox->setChecked(true);
            connect(m_debrisCheckBox, &QCheckBox::toggled,
                    this, &EffectGeneratorDialog::onParameterChanged);
            m_paramsLayout->addRow("", m_debrisCheckBox);
            break;
    }
    
    qDebug() << "=== updateParameterControls END ===";
    qDebug() << "m_particleCountSpinBox:" << m_particleCountSpinBox;
    qDebug() << "m_intensitySlider:" << m_intensitySlider;
    qDebug() << "m_speedSlider:" << m_speedSlider;
    qDebug() << "m_radiusSlider:" << m_radiusSlider;
    qDebug() << "m_sparksCheckBox:" << m_sparksCheckBox;
}

// Continued in next message due to length...
// Continuation of effectgeneratordialog.cpp

void EffectGeneratorDialog::onEffectTypeChanged(int index)
{
    updateParameterControls();
    
    // Set default colors per effect type
    EffectType type = (EffectType)m_effectTypeCombo->currentData().toInt();
    switch (type) {
        case EFFECT_EXPLOSION:
            m_color1 = QColor(255, 255, 200);
            m_color2 = QColor(200, 50, 0);
            break;
        case EFFECT_SMOKE:
            m_color1 = QColor(100, 100, 100);
            m_color2 = QColor(50, 50, 50);
            break;
        case EFFECT_FIRE:
            m_color1 = QColor(255, 255, 100);
            m_color2 = QColor(200, 50, 0);
            break;
        case EFFECT_PARTICLES:
            m_color1 = QColor(100, 150, 255);
            m_color2 = QColor(255, 100, 150);
            break;
        case EFFECT_WATER:
            m_color1 = QColor(100, 150, 255);
            m_color2 = QColor(50, 100, 200);
            break;
        case EFFECT_ENERGY:
            m_color1 = QColor(150, 100, 255);
            m_color2 = QColor(255, 100, 255);
            break;
        case EFFECT_IMPACT:
            m_color1 = QColor(150, 130, 100);
            m_color2 = QColor(80, 70, 60);
            break;
    }
    
    if (m_color1Button) {
        m_color1Button->setStyleSheet(QString("background-color: %1").arg(m_color1.name()));
    }
    if (m_color2Button) {
        m_color2Button->setStyleSheet(QString("background-color: %1").arg(m_color2.name()));
    }
    
    generateEffect();
}

void EffectGeneratorDialog::onParameterChanged()
{
    m_presetCombo->setCurrentIndex(0); // Custom
    // Don't auto-regenerate, user clicks Regenerate button
}

void EffectGeneratorDialog::onRegenerateClicked()
{
    generateEffect();
}

void EffectGeneratorDialog::generateEffect()
{
    // Safety checks
    if (!m_framesSpinBox || !m_sizeCombo || !m_seedSpinBox) {
        return; // UI not ready yet
    }
    
    // Collect parameters
    m_params.frames = m_framesSpinBox->value();
    m_params.imageSize = m_sizeCombo->currentData().toInt();
    m_params.seed = m_seedSpinBox->value();
    m_params.particleCount = m_particleCountSpinBox ? m_particleCountSpinBox->value() : 100;
    m_params.intensity = m_intensitySlider ? m_intensitySlider->value() : 50;
    m_params.speed = m_speedSlider ? m_speedSlider->value() : 10;
    m_params.color1 = m_color1;
    m_params.color2 = m_color2;
    
    if (m_radiusSlider) m_params.radius = m_radiusSlider->value();
    if (m_turbulenceSlider) m_params.turbulence = m_turbulenceSlider->value() / 100.0f;
    if (m_gravitySlider) m_params.gravity = m_gravitySlider->value() / 10.0f;
    if (m_dispersionSlider) m_params.dispersion = m_dispersionSlider->value() / 50.0f;
    if (m_fadeRateSlider) m_params.fadeRate = m_fadeRateSlider->value() / 1000.0f;
    if (m_debrisCheckBox) m_params.debris = m_debrisCheckBox->isChecked();
    if (m_sparksCheckBox) m_params.sparks = m_sparksCheckBox->isChecked();
    if (m_trailsCheckBox) m_params.trails = m_trailsCheckBox->isChecked();
    
    // Generate
    EffectType type = (EffectType)m_effectTypeCombo->currentData().toInt();
    m_generator.setType(type);
    m_generator.setParams(m_params);
    
    m_frames = m_generator.generateAnimation();
    
    // Update UI
    if (m_frameSlider) {
        m_frameSlider->setRange(0, m_frames.size() - 1);
        m_frameSlider->setValue(0);
    }
    m_currentFrame = 0;
    
    if (!m_frames.isEmpty() && m_previewLabel && m_frameLabel && m_playButton) {
        m_previewLabel->setPixmap(QPixmap::fromImage(m_frames[0]));
        m_frameLabel->setText(tr("Frame: 1/%1").arg(m_frames.size()));
        m_playButton->setEnabled(true);
    }
}

void EffectGeneratorDialog::onPlayClicked()
{
    if (m_frames.isEmpty()) return;
    
    if (m_isPlaying) {
        // Pause
        m_animationTimer->stop();
        m_isPlaying = false;
        m_playButton->setText(tr("▶ Reproducir"));
        // Re-enable controls
        m_effectTypeCombo->setEnabled(true);
        m_presetCombo->setEnabled(true);
    } else {
        // Play
        int fps = m_fpsSpinBox->value();
        m_animationTimer->setInterval(1000 / fps);
        m_animationTimer->start();
        m_isPlaying = true;
        m_playButton->setText(tr("⏸ Pausar"));
        m_stopButton->setEnabled(true);
        // Disable controls during playback
        m_effectTypeCombo->setEnabled(false);
        m_presetCombo->setEnabled(false);
    }
}

void EffectGeneratorDialog::onStopClicked()
{
    m_animationTimer->stop();
    m_isPlaying = false;
    m_currentFrame = 0;
    m_playButton->setText(tr("▶ Reproducir"));
    m_stopButton->setEnabled(false);
    
    // Re-enable controls
    m_effectTypeCombo->setEnabled(true);
    m_presetCombo->setEnabled(true);
    
    if (!m_frames.isEmpty()) {
        m_previewLabel->setPixmap(QPixmap::fromImage(m_frames[0]));
        m_frameLabel->setText(tr("Frame: 1/%1").arg(m_frames.size()));
        if (m_frameSlider) m_frameSlider->setValue(0);
    }
}

void EffectGeneratorDialog::onAnimationTick()
{
    if (m_frames.isEmpty()) return;
    
    m_currentFrame = (m_currentFrame + 1) % m_frames.size();
    m_previewLabel->setPixmap(QPixmap::fromImage(m_frames[m_currentFrame]));
    m_frameLabel->setText(tr("Frame: %1/%2").arg(m_currentFrame + 1).arg(m_frames.size()));
    m_frameSlider->setValue(m_currentFrame);
}

void EffectGeneratorDialog::onExportClicked()
{
    if (m_frames.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("No hay efecto generado para exportar"));
        return;
    }
    
    // Export dialog
    QDialog exportDialog(this);
    exportDialog.setWindowTitle(tr("Exportar a FPG"));
    QVBoxLayout *layout = new QVBoxLayout(&exportDialog);
    
    QFormLayout *formLayout = new QFormLayout();
    
    QSpinBox *startIDSpinBox = new QSpinBox();
    startIDSpinBox->setRange(1, 9999);
    startIDSpinBox->setValue(1);
    formLayout->addRow(tr("ID Inicial:"), startIDSpinBox);
    
    QLineEdit *baseNameEdit = new QLineEdit();
    baseNameEdit->setText(m_effectTypeCombo->currentText().toLower());
    formLayout->addRow(tr("Nombre Base:"), baseNameEdit);
    
    QCheckBox *compressCheckBox = new QCheckBox(tr("Comprimir con gzip"));
    formLayout->addRow("", compressCheckBox);
    
    layout->addLayout(formLayout);
    
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, &exportDialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &exportDialog, &QDialog::reject);
    layout->addWidget(buttonBox);
    
    if (exportDialog.exec() != QDialog::Accepted) {
        return;
    }
    
    // Get export path
    QString filename = QFileDialog::getSaveFileName(this,
        tr("Guardar FPG"),
        "",
        tr("Archivos FPG (*.fpg)"));
    
    if (filename.isEmpty()) return;
    
    if (!filename.endsWith(".fpg", Qt::CaseInsensitive)) {
        filename += ".fpg";
    }
    
    // Create texture entries
    QVector<TextureEntry> textures;
    int startID = startIDSpinBox->value();
    QString baseName = baseNameEdit->text();
    
    for (int i = 0; i < m_frames.size(); i++) {
        TextureEntry tex;
        tex.id = startID + i;
        tex.pixmap = QPixmap::fromImage(m_frames[i]);
        tex.filename = QString("%1_%2").arg(baseName).arg(i, 3, 10, QChar('0'));
        textures.append(tex);
    }
    
    // Save FPG
    bool success = FPGLoader::saveFPG(filename, textures, compressCheckBox->isChecked());
    
    if (success) {
        QMessageBox::information(this, tr("Éxito"),
            tr("¡Efecto exportado correctamente!\n%1 frames guardados en %2")
            .arg(m_frames.size()).arg(filename));
    } else {
        QMessageBox::warning(this, tr("Error"),
            tr("Error al exportar el efecto"));
    }
}

void EffectGeneratorDialog::onPresetChanged(int index)
{
    if (index == 0) return; // Custom - do nothing
    loadPreset(index);
    generateEffect();
}

void EffectGeneratorDialog::loadPreset(int index)
{
    qDebug() << "=== loadPreset START ===" << index;
    
    // Disconnect to avoid triggering onEffectTypeChanged multiple times
    disconnect(m_effectTypeCombo, nullptr, this, nullptr);
    
    qDebug() << "Disconnected signals";
    
    switch (index) {
        case 1: // Explosión Pequeña
            qDebug() << "Loading preset: Explosión Pequeña";
            m_effectTypeCombo->setCurrentIndex(0);
            qDebug() << "Set effect type to 0";
            updateParameterControls();
            qDebug() << "Updated parameter controls";
            m_framesSpinBox->setValue(20);
            m_sizeCombo->setCurrentIndex(1); // 64x64
            m_particleCountSpinBox->setValue(80);
            m_intensitySlider->setValue(40);
            m_speedSlider->setValue(8);
            m_radiusSlider->setValue(30);
            qDebug() << "Set all parameters";
            break;
            
        case 2: // Explosión Grande
            m_effectTypeCombo->setCurrentIndex(0);
            updateParameterControls();
            m_framesSpinBox->setValue(40);
            m_sizeCombo->setCurrentIndex(3); // 256x256
            m_particleCountSpinBox->setValue(300);
            m_intensitySlider->setValue(80);
            m_speedSlider->setValue(15);
            m_radiusSlider->setValue(100);
            break;
            
        case 3: // Humo Denso
            m_effectTypeCombo->setCurrentIndex(1);
            updateParameterControls();
            m_framesSpinBox->setValue(50);
            m_particleCountSpinBox->setValue(150);
            m_turbulenceSlider->setValue(70);
            m_dispersionSlider->setValue(40);
            m_color1 = QColor(60, 60, 60);
            m_color1Button->setStyleSheet(QString("background-color: %1").arg(m_color1.name()));
            break;
            
        case 4: // Humo Ligero
            m_effectTypeCombo->setCurrentIndex(1);
            updateParameterControls();
            m_framesSpinBox->setValue(60);
            if (m_particleCountSpinBox) m_particleCountSpinBox->setValue(80);
            if (m_turbulenceSlider) m_turbulenceSlider->setValue(30);
            if (m_dispersionSlider) m_dispersionSlider->setValue(70);
            m_color1 = QColor(180, 180, 180);
            if (m_color1Button) m_color1Button->setStyleSheet(QString("background-color: %1").arg(m_color1.name()));
            break;
            
        case 5: // Fuego Pequeño
            m_effectTypeCombo->setCurrentIndex(2);
            updateParameterControls();
            m_framesSpinBox->setValue(30);
            m_sizeCombo->setCurrentIndex(1); // 64x64
            if (m_particleCountSpinBox) m_particleCountSpinBox->setValue(100);
            if (m_speedSlider) m_speedSlider->setValue(12);
            // No sparks for small fire
            break;
            
        case 6: // Fuego Grande
            m_effectTypeCombo->setCurrentIndex(2);
            updateParameterControls();
            m_framesSpinBox->setValue(40);
            m_sizeCombo->setCurrentIndex(3); // 256x256
            if (m_particleCountSpinBox) m_particleCountSpinBox->setValue(250);
            if (m_speedSlider) m_speedSlider->setValue(18);
            if (m_sparksCheckBox) m_sparksCheckBox->setChecked(true);
            break;
            
        case 7: // Chispas
            m_effectTypeCombo->setCurrentIndex(3);
            updateParameterControls();
            m_framesSpinBox->setValue(25);
            if (m_particleCountSpinBox) m_particleCountSpinBox->setValue(50);
            if (m_speedSlider) m_speedSlider->setValue(20);
            if (m_gravitySlider) m_gravitySlider->setValue(10);
            m_color1 = QColor(255, 255, 150);
            m_color2 = QColor(255, 100, 0);
            if (m_color1Button) m_color1Button->setStyleSheet(QString("background-color: %1").arg(m_color1.name()));
            if (m_color2Button) m_color2Button->setStyleSheet(QString("background-color: %1").arg(m_color2.name()));
            break;
            
        case 8: // Salpicadura
            m_effectTypeCombo->setCurrentIndex(4);
            updateParameterControls();
            m_framesSpinBox->setValue(30);
            if (m_particleCountSpinBox) m_particleCountSpinBox->setValue(120);
            if (m_speedSlider) m_speedSlider->setValue(15);
            break;
            
        case 9: // Rayo Mágico
            m_effectTypeCombo->setCurrentIndex(5);
            updateParameterControls();
            m_framesSpinBox->setValue(35);
            if (m_particleCountSpinBox) m_particleCountSpinBox->setValue(200);
            if (m_radiusSlider) m_radiusSlider->setValue(60);
            m_color1 = QColor(150, 100, 255);
            m_color2 = QColor(255, 100, 255);
            if (m_color1Button) m_color1Button->setStyleSheet(QString("background-color: %1").arg(m_color1.name()));
            if (m_color2Button) m_color2Button->setStyleSheet(QString("background-color: %1").arg(m_color2.name()));
            break;
            
        case 10: // Polvo
            m_effectTypeCombo->setCurrentIndex(6);
            updateParameterControls();
            m_framesSpinBox->setValue(40);
            if (m_particleCountSpinBox) m_particleCountSpinBox->setValue(150);
            if (m_debrisCheckBox) m_debrisCheckBox->setChecked(true);
            break;
    }
    
    qDebug() << "=== loadPreset END ===";
    qDebug() << "Reconnecting signals";
    
    // Reconnect
    connect(m_effectTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EffectGeneratorDialog::onEffectTypeChanged);
    
    qDebug() << "=== loadPreset COMPLETE ===";
}

void EffectGeneratorDialog::onSavePresetClicked()
{
    // TODO: Implement custom preset saving
    QMessageBox::information(this, tr("Info"),
        tr("Guardar presets personalizados se implementará próximamente"));
}

void EffectGeneratorDialog::onColorPicker1()
{
    QColor color = QColorDialog::getColor(m_color1, this, tr("Seleccionar Color 1"));
    if (color.isValid()) {
        m_color1 = color;
        m_color1Button->setStyleSheet(QString("background-color: %1").arg(m_color1.name()));
        m_presetCombo->setCurrentIndex(0); // Custom
    }
}

void EffectGeneratorDialog::onColorPicker2()
{
    QColor color = QColorDialog::getColor(m_color2, this, tr("Seleccionar Color 2"));
    if (color.isValid()) {
        m_color2 = color;
        m_color2Button->setStyleSheet(QString("background-color: %1").arg(m_color2.name()));
        m_presetCombo->setCurrentIndex(0); // Custom
    }
}
