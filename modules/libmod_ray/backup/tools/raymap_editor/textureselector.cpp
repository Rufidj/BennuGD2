#include "textureselector.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>

TextureSelector::TextureSelector(const QMap<int, QPixmap> &textures, QWidget *parent)
    : QDialog(parent)
    , m_textures(textures)
    , m_selectedId(-1)
{
    setWindowTitle(tr("Seleccionar Textura"));
    setMinimumSize(600, 400);
    setupUI();
}

void TextureSelector::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Scroll area for texture grid
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    
    QWidget *gridWidget = new QWidget();
    QGridLayout *gridLayout = new QGridLayout(gridWidget);
    gridLayout->setSpacing(5);
    
    // Add "None" button
    QPushButton *noneBtn = new QPushButton(tr("Ninguna"));
    noneBtn->setFixedSize(80, 80);
    noneBtn->setStyleSheet("background-color: #404040;");
    connect(noneBtn, &QPushButton::clicked, this, &TextureSelector::onNoneClicked);
    gridLayout->addWidget(noneBtn, 0, 0);
    
    // Add texture buttons
    int row = 0;
    int col = 1;
    const int cols = 6;
    
    QList<int> textureIds = m_textures.keys();
    std::sort(textureIds.begin(), textureIds.end());
    
    for (int id : textureIds) {
        QPushButton *btn = new QPushButton();
        btn->setFixedSize(80, 80);
        btn->setIcon(QIcon(m_textures[id].scaled(76, 76, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        btn->setIconSize(QSize(76, 76));
        btn->setToolTip(tr("Textura %1").arg(id));
        
        connect(btn, &QPushButton::clicked, this, [this, id]() {
            onTextureClicked(id);
        });
        
        gridLayout->addWidget(btn, row, col);
        
        col++;
        if (col >= cols) {
            col = 0;
            row++;
        }
    }
    
    gridWidget->setLayout(gridLayout);
    scrollArea->setWidget(gridWidget);
    mainLayout->addWidget(scrollArea);
    
    // Button box
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

void TextureSelector::onTextureClicked(int textureId)
{
    m_selectedId = textureId;
    accept();
}

void TextureSelector::onNoneClicked()
{
    m_selectedId = 0;
    accept();
}
