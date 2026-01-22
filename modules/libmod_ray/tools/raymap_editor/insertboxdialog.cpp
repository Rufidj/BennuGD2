#include "insertboxdialog.h"
#include <QLabel>
#include <QVBoxLayout>

InsertBoxDialog::InsertBoxDialog(const QMap<int, QPixmap> &textureCache, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Insertar Caja"));
    setModal(true);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Size group
    QGroupBox *sizeGroup = new QGroupBox(tr("Tamaño"));
    QFormLayout *sizeLayout = new QFormLayout(sizeGroup);
    
    m_widthSpin = new QDoubleSpinBox();
    m_widthSpin->setRange(10.0, 1000.0);
    m_widthSpin->setValue(100.0);
    m_widthSpin->setSuffix(" units");
    m_widthSpin->setToolTip(tr("Ancho de la caja en unidades del mapa"));
    sizeLayout->addRow(tr("Ancho:"), m_widthSpin);
    
    m_heightSpin = new QDoubleSpinBox();
    m_heightSpin->setRange(10.0, 1000.0);
    m_heightSpin->setValue(100.0);
    m_heightSpin->setSuffix(" units");
    m_heightSpin->setToolTip(tr("Alto de la caja en unidades del mapa"));
    sizeLayout->addRow(tr("Alto:"), m_heightSpin);
    
    mainLayout->addWidget(sizeGroup);
    
    // Height group
    QGroupBox *heightGroup = new QGroupBox(tr("Altura"));
    QFormLayout *heightLayout = new QFormLayout(heightGroup);
    
    m_floorZSpin = new QDoubleSpinBox();
    m_floorZSpin->setRange(-100000.0, 100000.0);
    m_floorZSpin->setValue(0.0);
    m_floorZSpin->setSuffix(" units");
    m_floorZSpin->setToolTip(tr("Altura del suelo (Z)"));
    heightLayout->addRow(tr("Suelo Z:"), m_floorZSpin);
    
    m_ceilingZSpin = new QDoubleSpinBox();
    m_ceilingZSpin->setRange(-100000.0, 100000.0);
    m_ceilingZSpin->setValue(256.0);
    m_ceilingZSpin->setSuffix(" units");
    m_ceilingZSpin->setToolTip(tr("Altura del techo (Z)"));
    heightLayout->addRow(tr("Techo Z:"), m_ceilingZSpin);
    
    mainLayout->addWidget(heightGroup);
    
    // Texture group with visual previews
    QGroupBox *textureGroup = new QGroupBox(tr("Texturas"));
    QFormLayout *textureLayout = new QFormLayout(textureGroup);
    
    // Wall texture combo
    m_wallTextureCombo = new QComboBox();
    m_wallTextureCombo->setIconSize(QSize(64, 64));
    m_wallTextureCombo->setToolTip(tr("Textura para las paredes"));
    for (auto it = textureCache.constBegin(); it != textureCache.constEnd(); ++it) {
        QPixmap scaled = it.value().scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_wallTextureCombo->addItem(QIcon(scaled), QString::number(it.key()), it.key());
    }
    textureLayout->addRow(tr("Paredes:"), m_wallTextureCombo);
    
    // Floor texture combo
    m_floorTextureCombo = new QComboBox();
    m_floorTextureCombo->setIconSize(QSize(64, 64));
    m_floorTextureCombo->setToolTip(tr("Textura para el suelo"));
    for (auto it = textureCache.constBegin(); it != textureCache.constEnd(); ++it) {
        QPixmap scaled = it.value().scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_floorTextureCombo->addItem(QIcon(scaled), QString::number(it.key()), it.key());
    }
    textureLayout->addRow(tr("Suelo:"), m_floorTextureCombo);
    
    // Ceiling texture combo
    m_ceilingTextureCombo = new QComboBox();
    m_ceilingTextureCombo->setIconSize(QSize(64, 64));
    m_ceilingTextureCombo->setToolTip(tr("Textura para el techo"));
    for (auto it = textureCache.constBegin(); it != textureCache.constEnd(); ++it) {
        QPixmap scaled = it.value().scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_ceilingTextureCombo->addItem(QIcon(scaled), QString::number(it.key()), it.key());
    }
    textureLayout->addRow(tr("Techo:"), m_ceilingTextureCombo);
    
    mainLayout->addWidget(textureGroup);
    
    // Info label
    QLabel *infoLabel = new QLabel(tr(
        "<b>Instrucciones:</b><br>"
        "Después de hacer clic en OK, la caja se creará<br>"
        "con un portal automático al sector padre."));
    infoLabel->setWordWrap(true);
    mainLayout->addWidget(infoLabel);
    
    // Buttons
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(m_buttonBox);
}
