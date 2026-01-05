#ifndef GRIDEDITOR_H
#define GRIDEDITOR_H

#include <QWidget>
#include <QPixmap>
#include <QMap>
#include <QPoint>
#include <QVector>
#include "mapdata.h"

class GridEditor : public QWidget
{
    Q_OBJECT
    
public:
    explicit GridEditor(QWidget *parent = nullptr);
    
    // Map data
    void setMapData(MapData *data);
    
    // Textures
    void setTextures(const QMap<int, QPixmap> &textures);
    
    // Edit modes
    enum EditMode {
        MODE_DRAW_SECTOR,      // Draw new sector polygon
        MODE_EDIT_VERTICES,    // Move/add/delete vertices
        MODE_SELECT_WALL,      // Select wall for texture assignment
        MODE_PLACE_SPRITE,     // Place sprites
        MODE_PLACE_SPAWN,      // Place spawn flags
        MODE_PLACE_CAMERA,     // Place camera
        MODE_SELECT_SECTOR,    // Select sector
        MODE_MANUAL_PORTAL     // Link portals manually
    };
    void setEditMode(EditMode mode);
    
    // Current selection
    void setSelectedTexture(int textureId);
    void setSelectedSector(int sectorId);
    void setSelectedWall(int wallId);
    
    // Zoom and pan
    void setZoom(float zoom);
    void panView(int dx, int dy);
    
    // Camera
    void setCameraPosition(float x, float y);
    void getCameraPosition(float &x, float &y) const;
    bool hasCameraPosition() const { return m_hasCameraPosition; }
    
signals:
    void sectorSelected(int sectorId);
    void sectorCreated(int sectorId);  // NEW: Emitted when a new sector is created
    void wallSelected(int sectorIndex, int wallIndex);
    void portalWallSelected(int sectorIndex, int wallIndex); // NEW: For manual portal linking
    void requestDeletePortal(int sectorIndex, int wallIndex); // NEW: Context menu request
    void vertexSelected(int sectorId, int vertexIndex);
    void mapChanged(); // Emitted when geometry changes (dragging)
    void cameraPlaced(float x, float y);
    void spawnFlagPlaced(int flagId, float x, float y);
    
protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override; // NEW
    
private:
    MapData *m_mapData;
    QMap<int, QPixmap> m_textures;
    EditMode m_editMode;
    int m_selectedTexture;
    int m_selectedSector;
    int m_selectedWall;  // wall_id (legacy)
    int m_selectedWallSector;  // Sector index of selected wall
    int m_selectedWallIndex;   // Wall index within sector
    
    // View transform
    float m_zoom;
    float m_panX, m_panY;
    
    // Drawing state
    bool m_isDrawing;
    bool m_isDraggingSector;            // NEW: Dragging whole sector
    bool m_isDraggingWall;              // Future expansion?
    QVector<QPointF> m_currentPolygon;  // For MODE_DRAW_SECTOR
    int m_draggedVertex;                // For MODE_EDIT_VERTICES
    QPoint m_lastMousePos;              // For panning
    QPointF m_dragStartPos;             // NEW: For calculating delta
    
    // Camera
    bool m_hasCameraPosition;
    float m_cameraX, m_cameraY;
    
    // Cursor tracking
    QPointF m_lastCursorPos;
    
    // Rendering helpers
    void drawGrid(QPainter &painter);
    void drawSectors(QPainter &painter);
    void drawWalls(QPainter &painter);
    void drawPortals(QPainter &painter);
    void drawSprites(QPainter &painter);
    void drawSpawnFlags(QPainter &painter);
    void drawCamera(QPainter &painter);
    void drawCurrentPolygon(QPainter &painter);
    void drawCursorInfo(QPainter &painter);
    
    // Coordinate conversion
    QPointF screenToWorld(const QPoint &screenPos) const;
    QPoint worldToScreen(const QPointF &worldPos) const;
    
    // Helper for wall selection
    float pointToLineDistance(const QPointF &point, const QPointF &lineStart, const QPointF &lineEnd) const;
    
    // Hit testing
    int findSectorAt(const QPointF &worldPos);
    int findWallAt(const QPointF &worldPos, float tolerance = 10.0f);
    int findVertexAt(const QPointF &worldPos, int &sectorId, float tolerance = 10.0f);
    int findSpawnFlagAt(const QPointF &worldPos, float tolerance = 10.0f);
};

#endif // GRIDEDITOR_H
