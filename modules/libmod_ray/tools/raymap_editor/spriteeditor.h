#ifndef SPRITEEDITOR_H
#define SPRITEEDITOR_H

#include <QWidget>
#include "mapdata.h"

// Stub para sprite editor - implementación completa más adelante
class SpriteEditor : public QWidget
{
    Q_OBJECT
    
public:
    explicit SpriteEditor(QWidget *parent = nullptr);
    void setMapData(MapData *data) { m_mapData = data; }
    
private:
    MapData *m_mapData;
};

#endif // SPRITEEDITOR_H
