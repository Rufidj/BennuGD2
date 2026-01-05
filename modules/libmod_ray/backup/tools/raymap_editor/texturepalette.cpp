#include "texturepalette.h"
#include <QVBoxLayout>
#include <QLabel>

TexturePalette::TexturePalette(QWidget *parent)
    : QWidget(parent)
    , m_selectedTexture(0)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    
    QLabel *label = new QLabel(tr("Textures (click or drag)"));
    layout->addWidget(label);
    
    m_listWidget = new QListWidget();
    m_listWidget->setViewMode(QListWidget::IconMode);
    m_listWidget->setIconSize(QSize(64, 64));
    m_listWidget->setResizeMode(QListWidget::Adjust);
    m_listWidget->setDragEnabled(true);
    m_listWidget->setDragDropMode(QAbstractItemView::DragOnly);
    
    connect(m_listWidget, &QListWidget::itemClicked, this, &TexturePalette::onItemClicked);
    
    layout->addWidget(m_listWidget);
    setLayout(layout);
}

void TexturePalette::setTextures(const QVector<TextureEntry> &textures)
{
    m_listWidget->clear();
    m_textureMap.clear();
    
    for (const TextureEntry &entry : textures) {
        // Create scaled pixmap for icon
        QPixmap scaledPixmap = entry.pixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        
        // Create list item
        QListWidgetItem *item = new QListWidgetItem();
        item->setIcon(QIcon(scaledPixmap));
        item->setText(QString::number(entry.id));
        item->setData(Qt::UserRole, entry.id);
        item->setToolTip(QString("ID: %1\nFile: %2\nSize: %3x%4")
                        .arg(entry.id)
                        .arg(entry.filename)
                        .arg(entry.pixmap.width())
                        .arg(entry.pixmap.height()));
        
        m_listWidget->addItem(item);
        m_textureMap[entry.id] = entry.pixmap;
    }
}

int TexturePalette::getSelectedTexture() const
{
    return m_selectedTexture;
}

void TexturePalette::onItemClicked(QListWidgetItem *item)
{
    if (!item) return;
    
    int textureId = item->data(Qt::UserRole).toInt();
    m_selectedTexture = textureId;
    emit textureSelected(textureId);
}

void TexturePalette::updateList()
{
    // Already handled in setTextures
}
