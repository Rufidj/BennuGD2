#ifndef TEXTUREPALETTE_H
#define TEXTUREPALETTE_H

#include <QWidget>
#include <QListWidget>
#include <QMap>
#include <QPixmap>
#include "mapdata.h"

class TexturePalette : public QWidget
{
    Q_OBJECT
    
public:
    explicit TexturePalette(QWidget *parent = nullptr);
    
    // Cargar texturas desde FPG
    void setTextures(const QVector<TextureEntry> &textures);
    
    // Obtener textura seleccionada
    int getSelectedTexture() const;
    
signals:
    void textureSelected(int textureId);
    
private slots:
    void onItemClicked(QListWidgetItem *item);
    
private:
    QListWidget *m_listWidget;
    QMap<int, QPixmap> m_textureMap;
    int m_selectedTexture;
    
    void updateList();
};

#endif // TEXTUREPALETTE_H
