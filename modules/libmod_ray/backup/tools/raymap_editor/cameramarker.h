#ifndef CAMERAMARKER_H
#define CAMERAMARKER_H

#include <QWidget>
#include "mapdata.h"

// Stub para camera marker - implementación completa más adelante
class CameraMarker : public QWidget
{
    Q_OBJECT
    
public:
    explicit CameraMarker(QWidget *parent = nullptr);
    void setMapData(MapData *data) { m_mapData = data; }
    
private:
    MapData *m_mapData;
};

#endif // CAMERAMARKER_H
