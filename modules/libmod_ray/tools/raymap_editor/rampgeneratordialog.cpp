#include "rampgeneratordialog.h"
#include "textureselector.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QPainter>
#include <QtMath>
#include <QMessageBox>

RampGeneratorDialog::RampGeneratorDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Generador de Rampas y Escaleras"));
    resize(500, 600);
    
    setupUI();
    connectSignals();
    
    // Default values
    m_params.startPoint = QPointF(0, 0);
    m_params.endPoint = QPointF(200, 0);
    m_params.startHeight = 0.0f;
    m_params.endHeight = 64.0f;
    m_params.width = 100.0f;
    m_params.segments = 32;
    m_params.generateAsStairs = false;
    m_params.textureId = 1;
    m_params.ceilingTextureId = 1;
    m_params.wallTextureId = 1;
    m_params.ceilingHeight = 128.0f;
}

RampGeneratorDialog::~RampGeneratorDialog()
{
}

void RampGeneratorDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Type selection
    QGroupBox *typeGroup = new QGroupBox(tr("Tipo de Geometría"));
    QVBoxLayout *typeLayout = new QVBoxLayout(typeGroup);
    
    m_typeCombo = new QComboBox();
    m_typeCombo->addItem(tr("Rampa Suave"));
    m_typeCombo->addItem(tr("Escaleras"));
    m_typeCombo->addItem(tr("Rampa Espiral (Próximamente)"));
    m_typeCombo->setItemData(2, 0, Qt::UserRole - 1); // Disable spiral for now
    typeLayout->addWidget(m_typeCombo);
    
    mainLayout->addWidget(typeGroup);
    
    // Position parameters
    QGroupBox *posGroup = new QGroupBox(tr("Posición"));
    QFormLayout *posLayout = new QFormLayout(posGroup);
    
    m_startXSpin = new QDoubleSpinBox();
    m_startXSpin->setRange(-10000, 10000);
    m_startXSpin->setValue(0);
    m_startXSpin->setSuffix(" units");
    posLayout->addRow(tr("Inicio X:"), m_startXSpin);
    
    m_startYSpin = new QDoubleSpinBox();
    m_startYSpin->setRange(-10000, 10000);
    m_startYSpin->setValue(0);
    m_startYSpin->setSuffix(" units");
    posLayout->addRow(tr("Inicio Y:"), m_startYSpin);
    
    m_endXSpin = new QDoubleSpinBox();
    m_endXSpin->setRange(-10000, 10000);
    m_endXSpin->setValue(200);
    m_endXSpin->setSuffix(" units");
    posLayout->addRow(tr("Fin X:"), m_endXSpin);
    
    m_endYSpin = new QDoubleSpinBox();
    m_endYSpin->setRange(-10000, 10000);
    m_endYSpin->setValue(0);
    m_endYSpin->setSuffix(" units");
    posLayout->addRow(tr("Fin Y:"), m_endYSpin);
    
    mainLayout->addWidget(posGroup);
    
    // Dimensions
    QGroupBox *dimGroup = new QGroupBox(tr("Dimensiones"));
    QFormLayout *dimLayout = new QFormLayout(dimGroup);
    
    m_startHeightSpin = new QDoubleSpinBox();
    m_startHeightSpin->setRange(-1000, 1000);
    m_startHeightSpin->setValue(0);
    m_startHeightSpin->setSuffix(" units");
    dimLayout->addRow(tr("Altura Inicial:"), m_startHeightSpin);
    
    m_endHeightSpin = new QDoubleSpinBox();
    m_endHeightSpin->setRange(-1000, 1000);
    m_endHeightSpin->setValue(64);
    m_endHeightSpin->setSuffix(" units");
    dimLayout->addRow(tr("Altura Final:"), m_endHeightSpin);
    
    m_widthSpin = new QDoubleSpinBox();
    m_widthSpin->setRange(10, 1000);
    m_widthSpin->setValue(100);
    m_widthSpin->setSuffix(" units");
    dimLayout->addRow(tr("Ancho:"), m_widthSpin);
    
    m_ceilingHeightSpin = new QDoubleSpinBox();
    m_ceilingHeightSpin->setRange(32, 512);
    m_ceilingHeightSpin->setValue(128);
    m_ceilingHeightSpin->setSuffix(" units");
    dimLayout->addRow(tr("Altura Techo:"), m_ceilingHeightSpin);
    
    m_segmentsSpin = new QSpinBox();
    m_segmentsSpin->setRange(1, 100);
    m_segmentsSpin->setValue(32);  // More segments for smoother ramps
    dimLayout->addRow(tr("Segmentos:"), m_segmentsSpin);
    
    mainLayout->addWidget(dimGroup);
    
    // Textures
    QGroupBox *texGroup = new QGroupBox(tr("Texturas"));
    QFormLayout *texLayout = new QFormLayout(texGroup);
    
    // Floor texture
    QHBoxLayout *floorTexLayout = new QHBoxLayout();
    m_floorTextureSpin = new QSpinBox();
    m_floorTextureSpin->setRange(0, 9999);
    m_floorTextureSpin->setValue(1);
    floorTexLayout->addWidget(m_floorTextureSpin);
    QPushButton *floorTexButton = new QPushButton(tr("Seleccionar..."));
    connect(floorTexButton, &QPushButton::clicked, this, &RampGeneratorDialog::onSelectFloorTexture);
    floorTexLayout->addWidget(floorTexButton);
    texLayout->addRow(tr("Suelo:"), floorTexLayout);
    
    // Ceiling texture
    QHBoxLayout *ceilingTexLayout = new QHBoxLayout();
    m_ceilingTextureSpin = new QSpinBox();
    m_ceilingTextureSpin->setRange(0, 9999);
    m_ceilingTextureSpin->setValue(1);
    ceilingTexLayout->addWidget(m_ceilingTextureSpin);
    QPushButton *ceilingTexButton = new QPushButton(tr("Seleccionar..."));
    connect(ceilingTexButton, &QPushButton::clicked, this, &RampGeneratorDialog::onSelectCeilingTexture);
    ceilingTexLayout->addWidget(ceilingTexButton);
    texLayout->addRow(tr("Techo:"), ceilingTexLayout);
    
    // Wall texture
    QHBoxLayout *wallTexLayout = new QHBoxLayout();
    m_wallTextureSpin = new QSpinBox();
    m_wallTextureSpin->setRange(0, 9999);
    m_wallTextureSpin->setValue(1);
    wallTexLayout->addWidget(m_wallTextureSpin);
    QPushButton *wallTexButton = new QPushButton(tr("Seleccionar..."));
    connect(wallTexButton, &QPushButton::clicked, this, &RampGeneratorDialog::onSelectWallTexture);
    wallTexLayout->addWidget(wallTexButton);
    texLayout->addRow(tr("Paredes:"), wallTexLayout);
    
    mainLayout->addWidget(texGroup);
    
    // Preview
    QGroupBox *previewGroup = new QGroupBox(tr("Vista Previa (2D)"));
    QVBoxLayout *previewLayout = new QVBoxLayout(previewGroup);
    
    m_previewLabel = new QLabel();
    m_previewLabel->setMinimumSize(400, 150);
    m_previewLabel->setStyleSheet("QLabel { background-color: #2b2b2b; border: 1px solid #555; }");
    m_previewLabel->setAlignment(Qt::AlignCenter);
    previewLayout->addWidget(m_previewLabel);
    
    mainLayout->addWidget(previewGroup);
    
    // Buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox();
    m_generateButton = buttonBox->addButton(tr("Generar"), QDialogButtonBox::AcceptRole);
    m_cancelButton = buttonBox->addButton(tr("Cancelar"), QDialogButtonBox::RejectRole);
    
    mainLayout->addWidget(buttonBox);
    
    connect(buttonBox, &QDialogButtonBox::accepted, this, &RampGeneratorDialog::onGenerateClicked);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &RampGeneratorDialog::onCancelClicked);
}

void RampGeneratorDialog::connectSignals()
{
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RampGeneratorDialog::onTypeChanged);
    
    // Update preview when any parameter changes
    connect(m_startXSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &RampGeneratorDialog::updatePreview);
    connect(m_startYSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &RampGeneratorDialog::updatePreview);
    connect(m_endXSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &RampGeneratorDialog::updatePreview);
    connect(m_endYSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &RampGeneratorDialog::updatePreview);
    connect(m_widthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &RampGeneratorDialog::updatePreview);
    connect(m_segmentsSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RampGeneratorDialog::updatePreview);
}

void RampGeneratorDialog::onGenerateClicked()
{
    // Update parameters from UI
    m_params.startPoint = QPointF(m_startXSpin->value(), m_startYSpin->value());
    m_params.endPoint = QPointF(m_endXSpin->value(), m_endYSpin->value());
    m_params.startHeight = m_startHeightSpin->value();
    m_params.endHeight = m_endHeightSpin->value();
    m_params.width = m_widthSpin->value();
    m_params.segments = m_segmentsSpin->value();
    m_params.generateAsStairs = (m_typeCombo->currentIndex() == 1);
    m_params.textureId = m_floorTextureSpin->value();
    m_params.ceilingTextureId = m_ceilingTextureSpin->value();
    m_params.wallTextureId = m_wallTextureSpin->value();
    m_params.ceilingHeight = m_ceilingHeightSpin->value();
    
    accept();
}

void RampGeneratorDialog::onCancelClicked()
{
    reject();
}

void RampGeneratorDialog::onTypeChanged(int index)
{
    // Could enable/disable certain options based on type
    updatePreview();
}

void RampGeneratorDialog::updatePreview()
{
    // Create a simple 2D preview of the ramp
    QPixmap pixmap(m_previewLabel->width(), m_previewLabel->height());
    pixmap.fill(QColor(43, 43, 43));
    
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Get current parameters
    QPointF start(m_startXSpin->value(), m_startYSpin->value());
    QPointF end(m_endXSpin->value(), m_endYSpin->value());
    float width = m_widthSpin->value();
    int segments = m_segmentsSpin->value();
    
    // Calculate bounds for scaling
    float minX = qMin(start.x(), end.x()) - width;
    float maxX = qMax(start.x(), end.x()) + width;
    float minY = qMin(start.y(), end.y()) - width;
    float maxY = qMax(start.y(), end.y()) + width;
    
    float rangeX = maxX - minX;
    float rangeY = maxY - minY;
    float scale = qMin((pixmap.width() - 40) / rangeX, (pixmap.height() - 40) / rangeY);
    
    // Transform function
    auto transform = [&](const QPointF &p) -> QPointF {
        float x = (p.x() - minX) * scale + 20;
        float y = pixmap.height() - ((p.y() - minY) * scale + 20);
        return QPointF(x, y);
    };
    
    // Draw ramp segments
    QPointF dir = end - start;
    float len = qSqrt(dir.x() * dir.x() + dir.y() * dir.y());
    if (len > 0.1f) {
        QPointF dirNorm(dir.x() / len, dir.y() / len);
        QPointF perpVec(-dirNorm.y(), dirNorm.x());
        QPointF widthOffset = perpVec * (width / 2.0f);
        
        painter.setPen(QPen(QColor(100, 150, 255), 1));
        painter.setBrush(QBrush(QColor(50, 100, 200, 100)));
        
        for (int i = 0; i < segments; i++) {
            float t1 = (float)i / segments;
            float t2 = (float)(i + 1) / segments;
            
            QPointF segStart = start + dir * t1;
            QPointF segEnd = start + dir * t2;
            
            QPointF p1 = transform(segStart + widthOffset);
            QPointF p2 = transform(segStart - widthOffset);
            QPointF p3 = transform(segEnd - widthOffset);
            QPointF p4 = transform(segEnd + widthOffset);
            
            QPolygonF poly;
            poly << p1 << p2 << p3 << p4;
            painter.drawPolygon(poly);
        }
        
        // Draw direction arrow
        QPointF arrowStart = transform(start);
        QPointF arrowEnd = transform(end);
        painter.setPen(QPen(QColor(255, 200, 0), 2));
        painter.drawLine(arrowStart, arrowEnd);
        
        // Arrow head
        QPointF arrowDir = arrowEnd - arrowStart;
        float arrowLen = qSqrt(arrowDir.x() * arrowDir.x() + arrowDir.y() * arrowDir.y());
        if (arrowLen > 10) {
            QPointF arrowNorm = arrowDir / arrowLen;
            QPointF arrowPerp(-arrowNorm.y(), arrowNorm.x());
            QPointF tip = arrowEnd;
            QPointF left = tip - arrowNorm * 10 + arrowPerp * 5;
            QPointF right = tip - arrowNorm * 10 - arrowPerp * 5;
            
            painter.setBrush(QColor(255, 200, 0));
            QPolygonF arrowHead;
            arrowHead << tip << left << right;
            painter.drawPolygon(arrowHead);
        }
    }
    
    // Draw info text
    painter.setPen(QColor(200, 200, 200));
    painter.drawText(10, 20, tr("Segmentos: %1").arg(segments));
    
    m_previewLabel->setPixmap(pixmap);
}

RampParameters RampGeneratorDialog::getParameters() const
{
    return m_params;
}

void RampGeneratorDialog::setStartPoint(const QPointF &point)
{
    m_startXSpin->setValue(point.x());
    m_startYSpin->setValue(point.y());
    m_params.startPoint = point;
    updatePreview();
}

void RampGeneratorDialog::setEndPoint(const QPointF &point)
{
    m_endXSpin->setValue(point.x());
    m_endYSpin->setValue(point.y());
    m_params.endPoint = point;
    updatePreview();
}

void RampGeneratorDialog::setTextures(const QMap<int, QPixmap> &textures)
{
    m_textureMap = textures;
}

void RampGeneratorDialog::onSelectFloorTexture()
{
    if (m_textureMap.isEmpty()) {
        QMessageBox::warning(this, tr("Sin texturas"),
                           tr("No hay texturas cargadas. Carga un FPG primero."));
        return;
    }
    
    TextureSelector *selector = new TextureSelector(m_textureMap, this);
    
    if (selector->exec() == QDialog::Accepted) {
        int selectedId = selector->selectedTextureId();
        m_floorTextureSpin->setValue(selectedId);
    }
    
    delete selector;
}

void RampGeneratorDialog::onSelectCeilingTexture()
{
    if (m_textureMap.isEmpty()) {
        QMessageBox::warning(this, tr("Sin texturas"),
                           tr("No hay texturas cargadas. Carga un FPG primero."));
        return;
    }
    
    TextureSelector *selector = new TextureSelector(m_textureMap, this);
    
    if (selector->exec() == QDialog::Accepted) {
        int selectedId = selector->selectedTextureId();
        m_ceilingTextureSpin->setValue(selectedId);
    }
    
    delete selector;
}

void RampGeneratorDialog::onSelectWallTexture()
{
    if (m_textureMap.isEmpty()) {
        QMessageBox::warning(this, tr("Sin texturas"),
                           tr("No hay texturas cargadas. Carga un FPG primero."));
        return;
    }
    
    TextureSelector *selector = new TextureSelector(m_textureMap, this);
    
    if (selector->exec() == QDialog::Accepted) {
        int selectedId = selector->selectedTextureId();
        m_wallTextureSpin->setValue(selectedId);
    }
    
    delete selector;
}
