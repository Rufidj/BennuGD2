#include "meshgeneratordialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include "md3generator.h"
#include "textureatlasgen.h"
#include <QSettings>
#include <QGroupBox>
#include <QDialogButtonBox>

MeshGeneratorDialog::MeshGeneratorDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Generador de Modelos MD3"));
    resize(800, 600); // Larger size for preview
    
    QHBoxLayout *topLayout = new QHBoxLayout(this); // New top level
    
    // Left Parameter Panel
    QWidget *paramWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(paramWidget);
    
    // ... (rest of parameter layout construction)
    
    // Form Layout for Parameters

    QFormLayout *formLayout = new QFormLayout();
    
    // Type Selection
    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem(tr("Rampa (Cuña)"), MeshParams::RAMP);
    m_typeCombo->addItem(tr("Escalera Lineal"), MeshParams::STAIRS);
    m_typeCombo->addItem(tr("Cilindro / Columna"), MeshParams::CYLINDER);
    m_typeCombo->addItem(tr("Caja / Bloque"), MeshParams::BOX);
    m_typeCombo->addItem(tr("Puente"), MeshParams::BRIDGE);
    m_typeCombo->addItem(tr("Casa Simple"), MeshParams::HOUSE);
    m_typeCombo->addItem(tr("Arco"), MeshParams::ARCH);
    connect(m_typeCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onTypeChanged(int)));
    formLayout->addRow(tr("Tipo de Malla:"), m_typeCombo);
    
    // Dimensions
    m_widthSpin = new QDoubleSpinBox(this);
    m_widthSpin->setRange(1.0, 10000.0);
    m_widthSpin->setValue(64.0);
    m_widthSpin->setSuffix(" .u");
    formLayout->addRow(tr("Ancho (X):"), m_widthSpin);
    
    m_depthSpin = new QDoubleSpinBox(this); // Depth is usually Y or Z depending on orientation, let's say "Length/Depth"
    m_depthSpin->setRange(1.0, 10000.0);
    m_depthSpin->setValue(128.0);
    m_depthSpin->setSuffix(" .u");
    formLayout->addRow(tr("Profundidad (Y):"), m_depthSpin);
    
    m_heightSpin = new QDoubleSpinBox(this);
    m_heightSpin->setRange(1.0, 10000.0);
    m_heightSpin->setValue(64.0);
    m_heightSpin->setSuffix(" .u");
    formLayout->addRow(tr("Altura (Z):"), m_heightSpin);
    
    // segments (contextual)
    m_segmentsSpin = new QSpinBox(this);
    m_segmentsSpin->setRange(1, 100);
    m_segmentsSpin->setValue(8);
    m_segmentsLabel = new QLabel(tr("Escalones:"), this);
    formLayout->addRow(m_segmentsLabel, m_segmentsSpin);
    m_segmentsLabel->setVisible(false); // Default is Ramp (hidden)
    m_segmentsSpin->setVisible(false);
    
    // Type-specific options
    // Railings (for Bridge/Platform)
    m_railingsCheck = new QCheckBox(tr("Con barandillas"), this);
    m_railingsCheck->setChecked(true);
    m_railingsLabel = new QLabel(tr("Opciones:"), this);
    formLayout->addRow(m_railingsLabel, m_railingsCheck);
    m_railingsLabel->setVisible(false);
    m_railingsCheck->setVisible(false);
    connect(m_railingsCheck, &QCheckBox::checkStateChanged, this, &MeshGeneratorDialog::updatePreview);
    
    // Arch underneath (for Bridge)
    m_archCheck = new QCheckBox(tr("Con arco por debajo"), this);
    m_archCheck->setChecked(false);
    m_archLabel = new QLabel(tr(""), this);
    formLayout->addRow(m_archLabel, m_archCheck);
    m_archLabel->setVisible(false);
    m_archCheck->setVisible(false);
    connect(m_archCheck, &QCheckBox::checkStateChanged, this, &MeshGeneratorDialog::updatePreview);
    
    // Roof type (for House)
    m_roofTypeCombo = new QComboBox(this);
    m_roofTypeCombo->addItem(tr("Techo Plano"), 0);
    m_roofTypeCombo->addItem(tr("Techo Inclinado"), 1);
    m_roofTypeCombo->addItem(tr("Techo a Dos Aguas"), 2);
    m_roofTypeCombo->setCurrentIndex(2); // Default gabled
    m_roofTypeLabel = new QLabel(tr("Tipo de Techo:"), this);
    formLayout->addRow(m_roofTypeLabel, m_roofTypeCombo);
    m_roofTypeLabel->setVisible(false);
    m_roofTypeCombo->setVisible(false);
    connect(m_roofTypeCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(updatePreview()));
    
    mainLayout->addLayout(formLayout);
    
    // Paths
    QGroupBox *pathsGroup = new QGroupBox(tr("Recursos"), this);
    QVBoxLayout *pathsLayout = new QVBoxLayout(pathsGroup);
    
    // Multi-texture widget (dynamic based on type)
    m_texturesWidget = new QWidget(this);
    m_texturesLayout = new QVBoxLayout(m_texturesWidget);
    m_texturesLayout->setContentsMargins(0, 0, 0, 0);
    
    // Create texture slots (will be shown/hidden dynamically)
    for (int i = 0; i < 3; i++) {
        QHBoxLayout *texLayout = new QHBoxLayout();
        QLabel *label = new QLabel(this);
        QLineEdit *edit = new QLineEdit(this);
        QPushButton *btn = new QPushButton("...", this);
        btn->setMaximumWidth(30);
        
        texLayout->addWidget(label);
        texLayout->addWidget(edit);
        texLayout->addWidget(btn);
        
        m_textureLabels.append(label);
        m_textureEdits.append(edit);
        m_textureBrowseBtns.append(btn);
        
        m_texturesLayout->addLayout(texLayout);
        
        // Connect browse button
        connect(btn, &QPushButton::clicked, this, [this, i]() {
            QString path = QFileDialog::getOpenFileName(this, tr("Seleccionar Textura"), "", 
                                                       tr("Imágenes (*.png *.jpg *.jpeg *.tga *.bmp)"));
            if (!path.isEmpty()) {
                m_textureEdits[i]->setText(path);
                updatePreview();
            }
        });
        
        // Connect text change
        connect(edit, &QLineEdit::textChanged, this, &MeshGeneratorDialog::updatePreview);
    }
    
    pathsLayout->addWidget(m_texturesWidget);
    
    // Legacy single texture (kept for compatibility, hidden by default)
    m_texturePathEdit = new QLineEdit(this);
    m_texturePathEdit->setVisible(false);
    
    // Export
    // Export
    QHBoxLayout *expLayout = new QHBoxLayout();
    QLabel *expLabel = new QLabel(tr("Exportar (.md3):"), this);
    m_exportPathEdit = new QLineEdit(this);
    QPushButton *expBrowseBtn = new QPushButton("...", this);
    connect(expBrowseBtn, &QPushButton::clicked, this, &MeshGeneratorDialog::onBrowseExport);
    expLayout->addWidget(expLabel);
    expLayout->addWidget(m_exportPathEdit);
    expLayout->addWidget(expBrowseBtn);
    pathsLayout->addLayout(expLayout);
    
    mainLayout->addWidget(pathsGroup);
    
    // Buttons
    QDialogButtonBox *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(btnBox);
    
    // Finalize Layout
    topLayout->addWidget(paramWidget, 1); // 1/3 width
    
    // Preview Widget
    m_previewWidget = new ModelPreviewWidget(this);
    topLayout->addWidget(m_previewWidget, 2); // 2/3 width
    
    // Connect signals for live preview
    connect(m_widthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MeshGeneratorDialog::updatePreview);
    connect(m_heightSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MeshGeneratorDialog::updatePreview);
    connect(m_depthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MeshGeneratorDialog::updatePreview);
    connect(m_segmentsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MeshGeneratorDialog::updatePreview);
    connect(m_typeCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(updatePreview()));
    
    // Trigger initial state
    onTypeChanged(0);
    updatePreview();
}

void MeshGeneratorDialog::onTypeChanged(int index) {
    MeshParams::Type type = (MeshParams::Type)m_typeCombo->currentData().toInt();
    
    // Hide all type-specific controls first
    m_segmentsLabel->setVisible(false);
    m_segmentsSpin->setVisible(false);
    m_railingsLabel->setVisible(false);
    m_railingsCheck->setVisible(false);
    m_archLabel->setVisible(false);
    m_archCheck->setVisible(false);
    m_roofTypeLabel->setVisible(false);
    m_roofTypeCombo->setVisible(false);
    
    // Hide all texture slots initially
    for (int i = 0; i < m_textureLabels.size(); i++) {
        m_textureLabels[i]->setVisible(false);
        m_textureEdits[i]->setVisible(false);
        m_textureBrowseBtns[i]->setVisible(false);
    }
    
    // Configure texture slots based on type
    auto setupTextures = [this](const QStringList &labels) {
        for (int i = 0; i < labels.size() && i < m_textureLabels.size(); i++) {
            m_textureLabels[i]->setText(labels[i]);
            m_textureLabels[i]->setVisible(true);
            m_textureEdits[i]->setVisible(true);
            m_textureBrowseBtns[i]->setVisible(true);
        }
    };
    
    // Show relevant controls based on type
    switch(type) {
        case MeshParams::RAMP:
        case MeshParams::BOX:
        case MeshParams::CYLINDER:
            // Single texture
            setupTextures({tr("Textura:")});
            break;
            
        case MeshParams::STAIRS:
            m_segmentsLabel->setText(tr("Escalones:"));
            m_segmentsLabel->setVisible(true);
            m_segmentsSpin->setVisible(true);
            m_segmentsSpin->setValue(8);
            setupTextures({tr("Textura:")});
            break;
            
        case MeshParams::BRIDGE:
            m_railingsLabel->setVisible(true);
            m_railingsCheck->setVisible(true);
            m_archLabel->setVisible(true);
            m_archCheck->setVisible(true);
            setupTextures({tr("Superficie:"), tr("Muros/Barandillas:")});
            break;
            
        case MeshParams::HOUSE:
            m_roofTypeLabel->setVisible(true);
            m_roofTypeCombo->setVisible(true);
            setupTextures({tr("Techo:"), tr("Fachada:"), tr("Base:")});
            break;
            
        case MeshParams::ARCH:
            m_segmentsLabel->setText(tr("Segmentos del Arco:"));
            m_segmentsLabel->setVisible(true);
            m_segmentsSpin->setVisible(true);
            m_segmentsSpin->setValue(12);
            setupTextures({tr("Piedra/Material:")});
            break;
    }
}

void MeshGeneratorDialog::onBrowseTexture() {
    QString path = QFileDialog::getOpenFileName(this, tr("Seleccionar Textura"), "", tr("Imágenes (*.png *.jpg *.jpeg *.tga *.bmp)"));
    if (!path.isEmpty()) {
        m_texturePathEdit->setText(path);
        updatePreview(); // Trigger update which sets texture
    }
}

void MeshGeneratorDialog::onBrowseExport() {
    QString path = QFileDialog::getSaveFileName(this, tr("Exportar MD3"), "model.md3", tr("Modelos MD3 (*.md3)"));
    if (!path.isEmpty()) {
        m_exportPathEdit->setText(path);
    }
}

MeshGeneratorDialog::MeshParams MeshGeneratorDialog::getParameters() const {
    MeshParams params;
    params.type = (MeshParams::Type)m_typeCombo->currentData().toInt();
    params.width = (float)m_widthSpin->value();
    params.height = (float)m_heightSpin->value();
    params.depth = (float)m_depthSpin->value();
    params.segments = m_segmentsSpin->value();
    
    // Collect textures from visible slots
    params.texturePaths.clear();
    qDebug() << "=== getParameters() texture collection ===";
    qDebug() << "m_textureEdits.size():" << m_textureEdits.size();
    
    for (int i = 0; i < m_textureEdits.size(); i++) {
        QString text = m_textureEdits[i]->text();
        qDebug() << "  Slot" << i << "- text:" << text;
        
        // Don't check visibility - just check if there's a texture path
        if (!text.isEmpty()) {
            params.texturePaths.append(text);
            qDebug() << "    -> Added to texturePaths";
        }
    }
    
    qDebug() << "Final texturePaths.size():" << params.texturePaths.size();
    
    // Legacy single texture (use first texture if available)
    params.texturePath = params.texturePaths.isEmpty() ? QString() : params.texturePaths.first();
    params.exportPath = m_exportPathEdit->text();
    
    // Type-specific options
    params.hasRailings = m_railingsCheck->isChecked();
    params.hasArch = m_archCheck->isChecked();
    params.roofType = (MeshParams::RoofType)m_roofTypeCombo->currentData().toInt();
    
    return params;
}

QString MeshGeneratorDialog::getExportPath() const {
    return m_exportPathEdit->text();
}

void MeshGeneratorDialog::updatePreview()
{
    MeshParams params = getParameters();
    MD3Generator::MeshData mesh = MD3Generator::generateMesh(
        (MD3Generator::MeshType)params.type,
        params.width, params.height, params.depth, params.segments,
        params.hasRailings, params.hasArch, (int)params.roofType
    );
    
    // Handle multi-texture: generate atlas if multiple textures
    QString textureToShow;
    if (params.texturePaths.size() > 1) {
        // Load all textures
        QVector<QImage> textures = TextureAtlasGenerator::loadTextures(params.texturePaths);
        
        if (!textures.isEmpty()) {
            // Generate atlas
            QVector<TextureAtlasGenerator::AtlasRegion> uvRegions;
            QImage atlas = TextureAtlasGenerator::createAtlas(textures, uvRegions);
            
            // Save atlas temporarily for preview
            QString tempAtlasPath = QDir::temp().filePath("preview_atlas.png");
            atlas.save(tempAtlasPath);
            textureToShow = tempAtlasPath;
            
            // TODO: Adjust mesh UVs to use atlas regions (for now, just show the atlas)
        } else {
            textureToShow = params.texturePath; // Fallback to first texture
        }
    } else {
        textureToShow = params.texturePath; // Single texture
    }
    
    m_previewWidget->setTexture(textureToShow);
    m_previewWidget->setMesh(mesh);
}
