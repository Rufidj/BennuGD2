/*
 * grideditor.cpp - Vector-based map editor for geometric sectors
 * Complete rewrite for Build Engine-style sector editing
 */

#include "grideditor.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPen>
#include <QBrush>
#include <QMessageBox>
#include <QMenu>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>
#include <cmath>
#include <cfloat>

GridEditor::GridEditor(QWidget *parent)
    : QWidget(parent)
    , m_mapData(new MapData())
    , m_editMode(MODE_SELECT_SECTOR)
    , m_selectedTexture(1)
    , m_selectedSector(-1)
    , m_selectedWall(-1)
    , m_selectedWallSector(-1)
    , m_selectedWallIndex(-1)
    , m_zoom(1.0f)
    , m_panX(0.0f)
    , m_panY(0.0f)
    , m_isDrawing(false)
    , m_isDraggingSector(false)
    , m_isDraggingEntity(false)
    , m_draggedVertex(-1)
    , m_hasCameraPosition(false)
    , m_cameraX(0.0f)
    , m_cameraY(0.0f)
    , m_manualPortalMode(false)
    , m_isMovingGroup(false)
    , m_movingGroupId(-1)
    , m_showGrid(true) // Default true
{
    setMinimumSize(800, 600);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAcceptDrops(true); // Enable Drag & Drop
}

GridEditor::~GridEditor()
{
    if (m_mapData) {
        delete m_mapData;
    }
}

void GridEditor::newMap()
{
    if (m_mapData) delete m_mapData;
    m_mapData = new MapData();
    update();
}

void GridEditor::setTextures(const QMap<int, QPixmap> &textures)
{
    m_textures = textures;
    update();
}

void GridEditor::setEditMode(EditMode mode)
{
    m_editMode = mode;
    m_isDrawing = false;
    m_currentPolygon.clear();
    m_draggedVertex = -1;
    update();
}

void GridEditor::setSelectedTexture(int textureId)
{
    m_selectedTexture = textureId;
}

void GridEditor::setSelectedSector(int sectorId)
{
    m_selectedSector = sectorId;
    update();
}

void GridEditor::setSelectedWall(int wallId)
{
    m_selectedWall = wallId;
    update();
}

void GridEditor::setZoom(float zoom)
{
    m_zoom = qBound(0.1f, zoom, 10.0f);
    update();
}

void GridEditor::showGrid(bool show)
{
    m_showGrid = show;
    update();
}

void GridEditor::panView(int dx, int dy)
{
    m_panX += dx / m_zoom;
    m_panY += dy / m_zoom;
    update();
}

void GridEditor::setCameraPosition(float x, float y)
{
    m_hasCameraPosition = true;
    m_cameraX = x;
    m_cameraY = y;
    update();
}

void GridEditor::getCameraPosition(float &x, float &y) const
{
    x = m_cameraX;
    y = m_cameraY;
}

/* ============================================================================
   COORDINATE CONVERSION
   ============================================================================ */

QPointF GridEditor::screenToWorld(const QPoint &screenPos) const
{
    float worldX = (screenPos.x() - width() / 2.0f) / m_zoom + m_panX;
    float worldY = (screenPos.y() - height() / 2.0f) / m_zoom + m_panY;
    return QPointF(worldX, worldY);
}

QPoint GridEditor::worldToScreen(const QPointF &worldPos) const
{
    int screenX = (int)((worldPos.x() - m_panX) * m_zoom + width() / 2.0f);
    int screenY = (int)((worldPos.y() - m_panY) * m_zoom + height() / 2.0f);
    return QPoint(screenX, screenY);
}

/* ============================================================================
   HIT TESTING
   ============================================================================ */

int GridEditor::findSectorAt(const QPointF &worldPos)
{
    if (!m_mapData) return -1;
    
    // Find ALL sectors that contain this point
    QVector<int> candidateSectors;
    
    for (int i = 0; i < m_mapData->sectors.size(); i++) {
        const Sector &sector = m_mapData->sectors[i];
        
        // Ray casting algorithm
        int intersections = 0;
        for (int j = 0; j < sector.vertices.size(); j++) {
            int next = (j + 1) % sector.vertices.size();
            QPointF v1 = sector.vertices[j];
            QPointF v2 = sector.vertices[next];
            
            if ((v1.y() > worldPos.y()) != (v2.y() > worldPos.y())) {
                float x_intersect = (v2.x() - v1.x()) * (worldPos.y() - v1.y()) / 
                                   (v2.y() - v1.y()) + v1.x();
                if (worldPos.x() < x_intersect) {
                    intersections++;
                }
            }
        }
        
        if (intersections % 2 == 1) {
            candidateSectors.append(i);
        }
    }
    
    // If no sectors found, return -1
    if (candidateSectors.isEmpty()) {
        return -1;
    }
    
    // If only one sector found, return it
    if (candidateSectors.size() == 1) {
        return candidateSectors[0];
    }
    
    // Multiple sectors contain this point (nested sectors)
    // Strategy: Return the sector with highest nesting level
    // If tied, return the one with smallest area (most specific)
    
    int bestSector = candidateSectors[0];
    // Removed: nesting level check (v9 format is flat)
    
    // Calculate area of first candidate
    float minArea = 0.0f;
    const Sector &firstSector = m_mapData->sectors[bestSector];
    for (int j = 0; j < firstSector.vertices.size(); j++) {
        int next = (j + 1) % firstSector.vertices.size();
        minArea += firstSector.vertices[j].x() * firstSector.vertices[next].y();
        minArea -= firstSector.vertices[next].x() * firstSector.vertices[j].y();
    }
    minArea = std::abs(minArea) / 2.0f;
    
    for (int i = 1; i < candidateSectors.size(); i++) {
        int sectorIdx = candidateSectors[i];
        const Sector &sector = m_mapData->sectors[sectorIdx];
        // Removed: nestingLevel check
        
        // Calculate area using shoelace formula
        float area = 0.0f;
        for (int j = 0; j < sector.vertices.size(); j++) {
            int next = (j + 1) % sector.vertices.size();
            area += sector.vertices[j].x() * sector.vertices[next].y();
            area -= sector.vertices[next].x() * sector.vertices[j].y();
        }
        area = std::abs(area) / 2.0f;
        
        // Prefer smaller area if tied (most specific sector)
        if (area < minArea) {
            bestSector = sectorIdx;
            minArea = area;
        }
    }
    
    return bestSector;
}



int GridEditor::findWallAt(const QPointF &worldPos, float tolerance)
{
    if (!m_mapData) return -1;
    
    // Increased tolerance for easier wall selection
    float minDist = tolerance * 3.0f;  // 30 units instead of 10
    int closestWall = -1;
    int closestSector = -1;
    
    // FIRST PASS: Check only the currently selected sector (priority)
    if (m_selectedSector >= 0 && m_selectedSector < m_mapData->sectors.size()) {
        const Sector &sector = m_mapData->sectors[m_selectedSector];
        for (int w = 0; w < sector.walls.size(); w++) {
            const Wall &wall = sector.walls[w];
            QPointF p1(wall.x1, wall.y1);
            QPointF p2(wall.x2, wall.y2);
            float dist = pointToLineDistance(worldPos, p1, p2);
            
            if (dist < minDist) {
                // Found a wall in the selected sector! Select it immediately.
                // We prioritize staying in the current sector context.
                m_selectedSector = m_selectedSector; // Keep same sector
                return w; // Return wall index directly
            }
        }
    }
    
    // SECOND PASS: If no wall found in current sector, check all sectors
    // This allows selecting walls in other sectors if we click far from current sector walls
    for (int s = 0; s < m_mapData->sectors.size(); s++) {
        // Skip current sector as we already checked it
        if (s == m_selectedSector) continue;
        
        const Sector &sector = m_mapData->sectors[s];
        for (int w = 0; w < sector.walls.size(); w++) {
            const Wall &wall = sector.walls[w];
            
            QPointF p1(wall.x1, wall.y1);
            QPointF p2(wall.x2, wall.y2);
            
            float dist = pointToLineDistance(worldPos, p1, p2);
            
            if (dist < minDist) {
                minDist = dist;
                closestWall = w;
                closestSector = s;
            }
        }
    }
    
    // Store the sector index for later use
    if (closestWall >= 0) {
        m_selectedSector = closestSector;
    }
    
    return closestWall;
}

int GridEditor::findVertexAt(const QPointF &worldPos, int &sectorId, float tolerance)
{
    if (!m_mapData) return -1;
    
    float minDist = tolerance / m_zoom;
    int closestVertex = -1;
    sectorId = -1;
    
    for (int i = 0; i < m_mapData->sectors.size(); i++) {
        const Sector &sector = m_mapData->sectors[i];
        for (int j = 0; j < sector.vertices.size(); j++) {
            QPointF v = sector.vertices[j];
            float dx = worldPos.x() - v.x();
            float dy = worldPos.y() - v.y();
            float dist = std::sqrt(dx * dx + dy * dy);
            
            if (dist < minDist) {
                minDist = dist;
                closestVertex = j;
                sectorId = i;
            }
        }
    }
    
    return closestVertex;
}

int GridEditor::findSpawnFlagAt(const QPointF &worldPos, float tolerance)
{
    if (!m_mapData) return -1;
    
    float minDist = tolerance / m_zoom;
    int closestFlag = -1;
    
    for (int i = 0; i < m_mapData->spawnFlags.size(); i++) {
        const SpawnFlag &flag = m_mapData->spawnFlags[i];
        float dx = worldPos.x() - flag.x;
        float dy = worldPos.y() - flag.y;
        float dist = std::sqrt(dx * dx + dy * dy);
        
        if (dist < minDist) {
            minDist = dist;
            closestFlag = i;
        }
    }
    
    return closestFlag;
}

int GridEditor::findEntityAt(const QPointF &worldPos, float tolerance)
{
    if (!m_mapData) return -1;
    
    float minDist = tolerance / m_zoom;
    int closestEntity = -1;
    
    for (int i = 0; i < m_mapData->entities.size(); i++) {
        const EntityInstance &ent = m_mapData->entities[i];
        float dx = worldPos.x() - ent.x;
        float dy = worldPos.y() - ent.y;
        float dist = std::sqrt(dx * dx + dy * dy);
        
        if (dist < minDist) {
            minDist = dist;
            closestEntity = i;
        }
    }
    
    return closestEntity;
}

float GridEditor::pointToLineDistance(const QPointF &point, const QPointF &lineStart, const QPointF &lineEnd) const
{
    QPointF v = lineEnd - lineStart;
    QPointF w = point - lineStart;
    
    float c1 = QPointF::dotProduct(w, v);
    if (c1 <= 0) {
        return QLineF(point, lineStart).length();
    }
    
    float c2 = QPointF::dotProduct(v, v);
    if (c1 >= c2) {
        return QLineF(point, lineEnd).length();
    }
    
    float b = c1 / c2;
    QPointF pb = lineStart + b * v;
    return QLineF(point, pb).length();
}

/* ============================================================================
   RENDERING
   ============================================================================ */

void GridEditor::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Clear background
    painter.fillRect(rect(), QColor(40, 40, 40));
    
    // Draw grid
    drawGrid(painter);
    
    if (!m_mapData) return;
    
    // Draw sectors
    drawSectors(painter);
    
    // Draw walls
    drawWalls(painter);
    
    // Draw portals
    drawPortals(painter);
    
    // Draw sprites
    drawSprites(painter);
    
    // Draw spawn flags
    drawSpawnFlags(painter);

    // Draw entities
    drawEntities(painter);
    
    // Draw camera
    drawCamera(painter);
    
    // Draw current polygon being drawn
    if (m_editMode == MODE_DRAW_SECTOR && !m_currentPolygon.isEmpty()) {
        drawCurrentPolygon(painter);
    }
    
    // Draw group selection highlight
    if (m_isMovingGroup && m_movingGroupId >= 0) {
        const SectorGroup *group = m_mapData->findGroup(m_movingGroupId);
        if (group) {
            // Calculate bounding box of all sectors in group
            float minX = FLT_MAX, minY = FLT_MAX;
            float maxX = -FLT_MAX, maxY = -FLT_MAX;
            
            for (int sectorId : group->sector_ids) {
                const Sector *sector = m_mapData->findSector(sectorId);
                if (sector) {
                    for (const QPointF &v : sector->vertices) {
                        minX = qMin(minX, (float)v.x());
                        minY = qMin(minY, (float)v.y());
                        maxX = qMax(maxX, (float)v.x());
                        maxY = qMax(maxY, (float)v.y());
                    }
                }
            }
            
            // Draw bounding rectangle
            QPoint topLeft = worldToScreen(QPointF(minX, minY));
            QPoint bottomRight = worldToScreen(QPointF(maxX, maxY));
            QRect boundingRect(topLeft, bottomRight);
            
            painter.setPen(QPen(QColor(255, 200, 0), 3, Qt::DashLine));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(boundingRect);
            
            // Draw group name
            painter.setPen(QPen(QColor(255, 255, 255), 1));
            painter.setFont(QFont("Arial", 12, QFont::Bold));
            painter.drawText(topLeft + QPoint(5, -5), group->name);
        }
    }
    
    // Draw cursor coordinates
    drawCursorInfo(painter);
}

void GridEditor::drawGrid(QPainter &painter)
{
    painter.setPen(QPen(QColor(60, 60, 60), 1));
    
    // Draw grid lines every 64 units
    int gridSize = 64;
    
    // Calculate visible range
    QPointF topLeft = screenToWorld(QPoint(0, 0));
    QPointF bottomRight = screenToWorld(QPoint(width(), height()));
    
    int startX = ((int)topLeft.x() / gridSize) * gridSize;
    int endX = ((int)bottomRight.x() / gridSize + 1) * gridSize;
    int startY = ((int)topLeft.y() / gridSize) * gridSize;
    int endY = ((int)bottomRight.y() / gridSize + 1) * gridSize;
    
    // Vertical lines
    for (int x = startX; x <= endX; x += gridSize) {
        QPoint p1 = worldToScreen(QPointF(x, topLeft.y()));
        QPoint p2 = worldToScreen(QPointF(x, bottomRight.y()));
        painter.drawLine(p1, p2);
    }
    
    // Horizontal lines
    for (int y = startY; y <= endY; y += gridSize) {
        QPoint p1 = worldToScreen(QPointF(topLeft.x(), y));
        QPoint p2 = worldToScreen(QPointF(bottomRight.x(), y));
        painter.drawLine(p1, p2);
    }
    
    // Draw origin
    painter.setPen(QPen(QColor(100, 100, 100), 2));
    QPoint origin = worldToScreen(QPointF(0, 0));
    painter.drawLine(origin.x() - 10, origin.y(), origin.x() + 10, origin.y());
    painter.drawLine(origin.x(), origin.y() - 10, origin.x(), origin.y() + 10);
}

void GridEditor::drawSectors(QPainter &painter)
{
    for (int i = 0; i < m_mapData->sectors.size(); i++) {
        const Sector &sector = m_mapData->sectors[i];
        
        if (sector.vertices.size() < 3) continue;
        
        // Convert to screen coordinates
        QPolygonF polygon;
        for (const QPointF &v : sector.vertices) {
            polygon << worldToScreen(v);
        }
        
        // Fill sector - only highlight if in sector selection mode (not wall selection)
        QColor fillColor;
        if (i == m_selectedSector && m_selectedWall < 0) {
            // Only highlight sector if no wall is selected
            fillColor = QColor(80, 120, 180, 100);
        } else {
            fillColor = QColor(60, 80, 100, 80);
        }
        painter.setBrush(QBrush(fillColor));
        painter.setPen(Qt::NoPen);
        painter.drawPolygon(polygon);
        
        // Draw sector ID and Slope Info
        QPointF center(0, 0);
        for (const QPointF &v : sector.vertices) {
            center += v;
        }
        center /= sector.vertices.size();
        QPoint screenCenter = worldToScreen(center);
        
        painter.setPen(QPen(QColor(200, 200, 200), 1));
        painter.drawText(screenCenter, QString("S%1").arg(sector.sector_id));
        
        // SLOPE INDICATOR REMOVED
    }
}

void GridEditor::drawWalls(QPainter &painter)
{
    for (int s = 0; s < m_mapData->sectors.size(); s++) {
        const Sector &sector = m_mapData->sectors[s];
        for (int w = 0; w < sector.walls.size(); w++) {
            const Wall &wall = sector.walls[w];
            QPoint p1 = worldToScreen(QPointF(wall.x1, wall.y1));
            QPoint p2 = worldToScreen(QPointF(wall.x2, wall.y2));
            
            // Color based on selection and portal status
            QColor color;
            if (s == m_selectedWallSector && w == m_selectedWallIndex) {
                color = QColor(255, 255, 0);  // Yellow for selected
            } else if (wall.portal_id >= 0) {
                color = QColor(0, 255, 0);    // Green for portals
            } else {
                color = QColor(150, 150, 150); // Gray for solid walls
            }
            
            painter.setPen(QPen(color, 2));
            painter.drawLine(p1, p2);
        }
    }
}

void GridEditor::drawPortals(QPainter &painter)
{
    painter.setPen(QPen(QColor(0, 255, 0, 128), 1, Qt::DashLine));
    
    for (const Portal &portal : m_mapData->portals) {
        QPoint p1 = worldToScreen(QPointF(portal.x1, portal.y1));
        QPoint p2 = worldToScreen(QPointF(portal.x2, portal.y2));
        painter.drawLine(p1, p2);
    }
}

void GridEditor::drawSprites(QPainter &painter)
{
    painter.setBrush(QBrush(QColor(255, 128, 0)));
    painter.setPen(QPen(QColor(255, 200, 0), 2));
    
    for (const SpriteData &sprite : m_mapData->sprites) {
        QPoint pos = worldToScreen(QPointF(sprite.x, sprite.y));
        painter.drawEllipse(pos, 5, 5);
    }
}

void GridEditor::drawSpawnFlags(QPainter &painter)
{
    painter.setBrush(QBrush(QColor(255, 0, 255)));
    painter.setPen(QPen(QColor(255, 128, 255), 2));
    
    for (const SpawnFlag &flag : m_mapData->spawnFlags) {
        QPoint pos = worldToScreen(QPointF(flag.x, flag.y));
        painter.drawRect(pos.x() - 5, pos.y() - 5, 10, 10);
        painter.drawText(pos + QPoint(8, 0), QString::number(flag.flagId));
    }
}

void GridEditor::drawCamera(QPainter &painter)
{
    if (!m_hasCameraPosition) return;
    
    QPoint pos = worldToScreen(QPointF(m_cameraX, m_cameraY));
    
    painter.setBrush(QBrush(QColor(255, 0, 0)));
    painter.setPen(QPen(QColor(255, 128, 128), 2));
    painter.drawEllipse(pos, 8, 8);
    
    // Draw direction indicator
    float rot = m_mapData ? m_mapData->camera.rotation : 0.0f;
    QPoint dir = worldToScreen(QPointF(m_cameraX + std::cos(rot) * 32,
                                       m_cameraY - std::sin(rot) * 32));
    painter.drawLine(pos, dir);
}

void GridEditor::drawCurrentPolygon(QPainter &painter)
{
    if (m_currentPolygon.size() < 2) return;
    
    painter.setPen(QPen(QColor(255, 255, 0), 2));
    painter.setBrush(QBrush(QColor(255, 255, 0, 50)));
    
    QPolygonF screenPoly;
    for (const QPointF &p : m_currentPolygon) {
        screenPoly << worldToScreen(p);
    }
    
    painter.drawPolyline(screenPoly);
    
    // Draw vertices
    for (const QPointF &p : m_currentPolygon) {
        QPoint sp = worldToScreen(p);
        painter.drawEllipse(sp, 4, 4);
    }
}

void GridEditor::drawCursorInfo(QPainter &painter)
{
    // Draw cursor coordinates in top-left corner
    painter.setPen(QPen(QColor(255, 255, 255), 1));
    painter.setFont(QFont("Monospace", 10));
    
    QString coordText = QString("Cursor: (%.1f, %.1f)").arg(m_lastCursorPos.x()).arg(m_lastCursorPos.y());
    painter.drawText(10, 20, coordText);
    
    // If drawing a polygon, show dimensions
    if (m_editMode == MODE_DRAW_SECTOR && m_currentPolygon.size() >= 2) {
        // Calculate bounding box
        float minX = m_currentPolygon[0].x(), maxX = m_currentPolygon[0].x();
        float minY = m_currentPolygon[0].y(), maxY = m_currentPolygon[0].y();
        
        for (const QPointF &p : m_currentPolygon) {
            if (p.x() < minX) minX = p.x();
            if (p.x() > maxX) maxX = p.x();
            if (p.y() < minY) minY = p.y();
            if (p.y() > maxY) maxY = p.y();
        }
        
        float width = maxX - minX;
        float height = maxY - minY;
        
        QString sizeText = QString("Size: %.1f x %.1f").arg(width).arg(height);
        painter.drawText(10, 40, sizeText);
    }
}


/* ============================================================================
   MOUSE EVENTS
   ============================================================================ */

void GridEditor::mousePressEvent(QMouseEvent *event)
{
    m_lastMousePos = event->pos(); // Store for panning
    
    QPointF worldPos = screenToWorld(event->pos());
    
    // Middle button panning
    if (event->button() == Qt::MiddleButton) {
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    
    // Handle group movement mode
    if (m_isMovingGroup && event->button() == Qt::LeftButton) {
        m_groupMoveStart = worldPos;
        return;
    }
    
    if (event->button() == Qt::LeftButton) {
        switch (m_editMode) {
            case MODE_DRAW_SECTOR:
                m_currentPolygon.append(worldPos);
                update();
                break;
                
            case MODE_EDIT_VERTICES: {
                int sectorId = -1;
                int vertexIdx = findVertexAt(worldPos, sectorId);
                if (vertexIdx >= 0) {
                    m_draggedVertex = vertexIdx;
                    m_selectedSector = sectorId;
                    emit vertexSelected(sectorId, vertexIdx);
                }
                break;
            }
                
            case MODE_SELECT_WALL: {
                // Check entities first
                int entityIdx = findEntityAt(worldPos);
                if (entityIdx >= 0) {
                    m_selectedEntity = entityIdx;
                    emit entitySelected(entityIdx, m_mapData->entities[entityIdx]);
                    update();
                    break;
                }

                int wallIdx = findWallAt(worldPos);
                if (wallIdx >= 0 && m_selectedSector >= 0 && m_selectedSector < m_mapData->sectors.size()) {
                    // findWallAt sets m_selectedSector to the sector containing the wall
                    // and returns the wall index
                    m_selectedWallSector = m_selectedSector;
                    m_selectedWallIndex = wallIdx;  // This is actually the wall index
                    emit wallSelected(m_selectedWallSector, m_selectedWallIndex);
                    update();
                } else {
                    // No wall found - check if clicked inside a sector
                    int sectorIdx = findSectorAt(worldPos);
                    if (sectorIdx >= 0) {
                        // Check if this sector belongs to a group
                        int sectorId = m_mapData->sectors[sectorIdx].sector_id;
                        int groupId = m_mapData->findGroupForSector(sectorId);
                        
                        if (groupId >= 0) {
                            // Sector belongs to a group - auto-select group for movement
                            setGroupMoveMode(groupId);
                            m_groupMoveStart = worldPos;
                            QMessageBox::information(this, tr("Grupo Seleccionado"),
                                tr("Grupo seleccionado. Arrastra para mover todos los sectores del grupo.\n"
                                   "Presiona ESC para cancelar."));
                            update();
                            return;
                        }
                        
                        // Check if we are clicking inside the ALREADY SELECTED sector
                        if (sectorIdx == m_selectedSector) {
                            // Start Dragging Sector
                            m_isDraggingSector = true;
                            m_dragStartPos = worldPos;
                            setCursor(Qt::SizeAllCursor);
                        } else {
                            // Select new sector
                            m_selectedSector = sectorIdx;
                            m_selectedWallSector = -1;
                            m_selectedWallIndex = -1;
                            emit sectorSelected(m_mapData->sectors[sectorIdx].sector_id);
                        }
                        update();
                    } else {
                        // Deselect if clicked in void?
                        // m_selectedSector = -1;
                        // update();
                    }
                }
                break;
            }
                
            case MODE_PLACE_CAMERA:
                setCameraPosition(worldPos.x(), worldPos.y());
                if (m_mapData) {
                    m_mapData->camera.x = worldPos.x();
                    m_mapData->camera.y = worldPos.y();
                    m_mapData->camera.enabled = true;
                }
                emit cameraPlaced(worldPos.x(), worldPos.y());
                break;
                
            case MODE_SELECT_SECTOR: {
                int sectorIdx = findSectorAt(worldPos);
                if (sectorIdx >= 0) {
                    m_selectedSector = sectorIdx;
                    m_selectedWallSector = -1;
                    m_selectedWallIndex = -1;
                    emit sectorSelected(m_mapData->sectors[sectorIdx].sector_id);
                } else {
                    // Start dragging if no sector found? Or just deselect?
                    // For now, let's allow dragging if we click on void but are in logic for panning? 
                    // No, panning is middle click.
                    
                    // Maybe deselect?
                    // m_selectedSector = -1;
                    // emit sectorSelected(-1);
                }
                update();
                break;
            }
                
            case MODE_PLACE_SPAWN: {
                int flagId = m_mapData ? m_mapData->getNextSpawnEntityId() : 1;
                if (m_mapData) {
                    SpawnFlag flag;
                    flag.flagId = flagId;
                    flag.x = worldPos.x();
                    flag.y = worldPos.y();
                    flag.z = 0.0f;
                    m_mapData->spawnFlags.append(flag);
                }
                emit spawnFlagPlaced(flagId, worldPos.x(), worldPos.y());
                update();
                break;
            }
                
            case MODE_PLACE_DECAL_FLOOR:
            case MODE_PLACE_DECAL_CEILING:
                emit decalPlaced(worldPos.x(), worldPos.y());
                update();
                break;

            case MODE_MANUAL_PORTAL: {
                int wallIdx = findWallAt(worldPos);
                if (wallIdx >= 0 && m_selectedSector >= 0 && m_selectedSector < m_mapData->sectors.size()) {
                    // findWallAt updates m_selectedSector
                    emit portalWallSelected(m_selectedSector, wallIdx);
                    // Force update to show potential highlight if we add it later
                    update();
                }
                break;
            }
            
            case MODE_SELECT_ENTITY: {
                int entityIdx = findEntityAt(worldPos);
                if (entityIdx >= 0) {
                    m_selectedEntity = entityIdx;
                    emit entitySelected(entityIdx, m_mapData->entities[entityIdx]);
                    
                    m_isDraggingEntity = true;
                    m_dragStartPos = worldPos;
                    setCursor(Qt::SizeAllCursor);
                } else {
                    m_selectedEntity = -1;
                    EntityInstance empty; // Dummy for deselection
                    emit entitySelected(-1, empty);
                }
                update();
                break;
            }
                
            default:
                break;
        }
    } else if (event->button() == Qt::RightButton) {
        // Right click to finish polygon
        if (m_editMode == MODE_DRAW_SECTOR && m_currentPolygon.size() >= 3) {
            // Create new sector
            if (m_mapData) {
                Sector newSector;
                newSector.sector_id = m_mapData->getNextSectorId();
                newSector.vertices = m_currentPolygon;
                newSector.floor_z = 0.0f;
                newSector.ceiling_z = 256.0f;
                newSector.floor_texture_id = m_selectedTexture;
                newSector.ceiling_texture_id = m_selectedTexture;
                newSector.light_level = 255;
                
                // Create walls from vertices
                for (int i = 0; i < newSector.vertices.size(); i++) {
                    int next = (i + 1) % newSector.vertices.size();
                    Wall wall;
                    wall.wall_id = m_mapData->getNextWallId();
                    wall.x1 = newSector.vertices[i].x();
                    wall.y1 = newSector.vertices[i].y();
                    wall.x2 = newSector.vertices[next].x();
                    wall.y2 = newSector.vertices[next].y();
                    wall.texture_id_middle = m_selectedTexture;
                    wall.texture_split_z_lower = 64.0f;
                    wall.texture_split_z_upper = 192.0f;
                    wall.portal_id = -1;
                    newSector.walls.append(wall);
                }
                
                m_mapData->sectors.append(newSector);
                emit sectorCreated(newSector.sector_id);  // NEW: Notify that sector was created
            }
            
            m_currentPolygon.clear();
            update();
        }
    }
}

void GridEditor::mouseMoveEvent(QMouseEvent *event)
{
    // Track cursor position
    m_lastCursorPos = screenToWorld(event->pos());
    update();  // Redraw to show updated coordinates
    
    // Handle panning (Middle button OR Space+LeftDrag)
    if (event->buttons() & Qt::MiddleButton) {
        float dx = event->pos().x() - m_lastMousePos.x();
        float dy = event->pos().y() - m_lastMousePos.y();
        
        panView(dx, dy);
        
        m_lastMousePos = event->pos();
        update();
        return;
    }
    
    QPointF worldPos = screenToWorld(event->pos());
    
    // Handle Group Movement
    if (m_isMovingGroup && (event->buttons() & Qt::LeftButton) && m_movingGroupId >= 0 && m_mapData) {
        // Calculate total offset from the start position
        QPointF totalOffset = worldPos - m_groupMoveStart;
        
        // Find the group
        const SectorGroup *group = m_mapData->findGroup(m_movingGroupId);
        if (group) {
            // Move all sectors in the group
            for (int sectorId : group->sector_ids) {
                Sector *sector = m_mapData->findSector(sectorId);
                if (sector && m_originalGroupPositions.contains(sectorId)) {
                    const QVector<QPointF> &originalVertices = m_originalGroupPositions[sectorId];
                    
                    // Update vertices from original positions + total offset
                    for (int i = 0; i < sector->vertices.size() && i < originalVertices.size(); i++) {
                        sector->vertices[i] = originalVertices[i] + totalOffset;
                    }
                    
                    // Update walls from vertices
                    for (int w = 0; w < sector->walls.size(); w++) {
                        int v1 = w;
                        int v2 = (w + 1) % sector->vertices.size();
                        if (v1 < sector->vertices.size() && v2 < sector->vertices.size()) {
                            sector->walls[w].x1 = sector->vertices[v1].x();
                            sector->walls[w].y1 = sector->vertices[v1].y();
                            sector->walls[w].x2 = sector->vertices[v2].x();
                            sector->walls[w].y2 = sector->vertices[v2].y();
                        }
                    }
                }
            }
        }
        
        update();
        return;
    }
    
    // Handle Sector Dragging
    if (m_isDraggingSector && m_selectedSector >= 0 && m_selectedSector < m_mapData->sectors.size()) {
        float dx = worldPos.x() - m_dragStartPos.x();
        float dy = worldPos.y() - m_dragStartPos.y();
        
        Sector &sec = m_mapData->sectors[m_selectedSector];
        
        // Update vertices
        for (int v = 0; v < sec.vertices.size(); v++) {
            sec.vertices[v].rx() += dx;
            sec.vertices[v].ry() += dy;
        }
        
        // Update walls
        for (int w = 0; w < sec.walls.size(); w++) {
            sec.walls[w].x1 += dx;
            sec.walls[w].y1 += dy;
            sec.walls[w].x2 += dx;
            sec.walls[w].y2 += dy;
            
            // Portals might become invalid if connections break.
            // For smooth dragging, we keep them, but user might need to re-detect if they misalign.
            // Or we could break them. Let's keep them for now as it's less destructive.
        }
        
        m_dragStartPos = worldPos;
        update();
        emit mapChanged();
        return; // Consume event
    }
    
    if (m_editMode == MODE_EDIT_VERTICES && m_draggedVertex >= 0 && 
        m_selectedSector >= 0 && m_selectedSector < m_mapData->sectors.size()) {
        
        Sector &sector = m_mapData->sectors[m_selectedSector];
        if (m_draggedVertex < sector.vertices.size()) {
            sector.vertices[m_draggedVertex] = worldPos;
            
            // Update walls
            for (int i = 0; i < sector.walls.size(); i++) {
                int v1 = i;
                int v2 = (i + 1) % sector.vertices.size();
                
                if (v1 < sector.vertices.size() && v2 < sector.vertices.size()) {
                    sector.walls[i].x1 = sector.vertices[v1].x();
                    sector.walls[i].y1 = sector.vertices[v1].y();
                    sector.walls[i].x2 = sector.vertices[v2].x();
                    sector.walls[i].y2 = sector.vertices[v2].y();
                }
            }
            
            update();
            emit mapChanged();
        }
    }
    
    // Entity dragging
    if (m_isDraggingEntity && m_selectedEntity >= 0 && m_selectedEntity < m_mapData->entities.size()) {
        float dx = worldPos.x() - m_dragStartPos.x();
        float dy = worldPos.y() - m_dragStartPos.y();
        
        EntityInstance &ent = m_mapData->entities[m_selectedEntity];
        ent.x += dx;
        ent.y += dy;
        
        m_dragStartPos = worldPos;
        update();
        // Emit updated entity to panel (and visual mode via MainWindow)
        // We reuse entitySelected to update panel values, OR create a new signal if needed.
        // But MainWindow::onEntityChanged handles updateEntity (which loops back here).
        // Let's check: MainWindow::onEntityChanged calls this->updateEntity.
        // We want to notify MainWindow that entity changed.
        
        // Actually we need a signal "entityModified" from here to MainWindow.
        // We have `entitySelected` which updates the panel. Let's use that to update the numbers.
        // But we also want to update the Visual Mode.
        // Ideally we emit `updateEntity` signal? No, `updateEntity` method is incoming.
        
        // Let's emit `entitySelected` again to update panel numbers
        emit entitySelected(m_selectedEntity, ent);
        // And emit dragged for visual update
        emit entityMoved(m_selectedEntity, ent);
        
        return;
    }
}

void GridEditor::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_draggedVertex = -1;
        if (m_isDraggingSector) {
            m_isDraggingSector = false;
            setCursor(Qt::ArrowCursor);
            emit mapChanged(); // Ensure final position is synced
        }
        if (m_isDraggingEntity) {
            m_isDraggingEntity = false;
            setCursor(Qt::ArrowCursor);
            emit mapChanged(); // Important to notify changes
        }
    }
    else if (event->button() == Qt::MiddleButton) {
        setCursor(Qt::CrossCursor);
    }
}

void GridEditor::keyPressEvent(QKeyEvent *event)
{
    // ESC to cancel group movement
    if (event->key() == Qt::Key_Escape) {
        if (m_isMovingGroup) {
            cancelGroupMove();
            QMessageBox::information(this, tr("Cancelado"),
                                   tr("Movimiento de grupo cancelado."));
            return;
        }
    }
    
    // Slope Editing Keys REMOVED
    
    QWidget::keyPressEvent(event);
}


void GridEditor::contextMenuEvent(QContextMenuEvent *event)
{
    QPointF worldPos = screenToWorld(event->pos());
    
    // Check for entity first (highest priority)
    int entityIdx = findEntityAt(worldPos, 15.0f);
    if (entityIdx >= 0 && m_mapData) {
        const EntityInstance &ent = m_mapData->entities[entityIdx];
        
        QMenu menu(this);
        QFileInfo info(ent.assetPath);
        QAction *deleteAction = menu.addAction(tr("Eliminar Entidad '%1' (ID %2)").arg(info.fileName()).arg(ent.spawn_id));
        
        QAction *selectedItem = menu.exec(event->globalPos());
        if (selectedItem == deleteAction) {
            m_mapData->entities.removeAt(entityIdx);
            emit mapChanged();
            update();
        }
        return;
    }

    // Check for spawn flag
    int flagIdx = findSpawnFlagAt(worldPos, 15.0f);
    if (flagIdx >= 0 && m_mapData) {
        const SpawnFlag &flag = m_mapData->spawnFlags[flagIdx];
        
        QMenu menu(this);
        QAction *deleteAction = menu.addAction(tr("Eliminar Spawn Flag (ID %1)").arg(flag.flagId));
        
        QAction *selectedItem = menu.exec(event->globalPos());
        if (selectedItem == deleteAction) {
            m_mapData->spawnFlags.removeAt(flagIdx);
            emit mapChanged();
            update();
        }
        return;
    }
    
    // Check for wall/portal
    int wallIdx = findWallAt(worldPos);
    
    // Check if we hit a wall in any sector (findWallAt updates m_selectedSector if found)
    if (wallIdx >= 0 && m_selectedSector >= 0 && m_selectedSector < m_mapData->sectors.size()) {
        const Wall &wall = m_mapData->sectors[m_selectedSector].walls[wallIdx];
        
        if (wall.portal_id >= 0) {
            QMenu menu(this);
            QAction *deleteAction = menu.addAction(tr("Eliminar Portal (ID %1)").arg(wall.portal_id));
            
            // Execute menu
            QAction *selectedItem = menu.exec(event->globalPos());
            if (selectedItem == deleteAction) {
                emit requestDeletePortal(m_selectedSector, wallIdx);
            }
        }
    }
}

void GridEditor::wheelEvent(QWheelEvent *event)
{
    // Zoom with mouse wheel
    float delta = event->angleDelta().y() / 120.0f;
    float newZoom = m_zoom * (1.0f + delta * 0.1f);
    setZoom(newZoom);
}

void GridEditor::mouseDoubleClickEvent(QMouseEvent *event)
{
    QPointF worldPos = screenToWorld(event->pos());
    
    if (m_editMode == MODE_EDIT_VERTICES && event->button() == Qt::LeftButton) {
        int wallIdx = findWallAt(worldPos);
        if (wallIdx >= 0 && m_selectedSector >= 0 && m_selectedSector < m_mapData->sectors.size()) {
            
            Sector &sector = m_mapData->sectors[m_selectedSector];
            
            // Calculate insertion index
            // Wall i connects vertex i to i+1
            // So we want to insert at vertex i+1
            int insertIdx = (wallIdx + 1) % (sector.vertices.size() + 1); // +1 because we are adding one
            if (wallIdx == sector.vertices.size() - 1) insertIdx = sector.vertices.size();
            else insertIdx = wallIdx + 1;

            sector.vertices.insert(insertIdx, worldPos);
            
            // Rebuild walls for this sector
            // Store old walls to preserve properties
            QVector<Wall> oldWalls = sector.walls;
            sector.walls.clear();
            
            for (int i = 0; i < sector.vertices.size(); i++) {
                int next = (i + 1) % sector.vertices.size();
                Wall newWall;
                newWall.wall_id = m_mapData->getNextWallId();
                newWall.x1 = sector.vertices[i].x();
                newWall.y1 = sector.vertices[i].y();
                newWall.x2 = sector.vertices[next].x();
                newWall.y2 = sector.vertices[next].y();
                
                // Try to inherit properties from old wall
                // If this new wall corresponds to an old wall segment
                // Logic: if new wall is part of old wall 'k', use 'k' props
                
                // Simplification for split: 
                // The split wall 'wallIdx' becomes two new walls: i and i+1?
                // Actually wallIdx becomes wallIdx and wallIdx+1
                
                int sourceWallIdx = i; 
                if (i > wallIdx) sourceWallIdx = i - 1; // Shift back for walls after split
                
                if (sourceWallIdx < oldWalls.size()) {
                    Wall &oldWall = oldWalls[sourceWallIdx];
                    newWall.texture_id_lower = oldWall.texture_id_lower;
                    newWall.texture_id_middle = oldWall.texture_id_middle;
                    newWall.texture_id_upper = oldWall.texture_id_upper;
                    newWall.texture_split_z_lower = oldWall.texture_split_z_lower;
                    newWall.texture_split_z_upper = oldWall.texture_split_z_upper;
                    newWall.flags = oldWall.flags;
                    
                    // Reset portal for the split wall segments because geometry changed
                    if (sourceWallIdx == wallIdx) newWall.portal_id = -1;
                    else newWall.portal_id = oldWall.portal_id; 
                }
                
                sector.walls.append(newWall);
            }
            
            m_selectedWall = -1; // Deselect to avoid index errors
            update();
            QMessageBox::information(this, tr("Split Wall"), tr("Wall split successfully!"));
        }
    }
}

/* ============================================================================
   GROUP MOVEMENT
   ============================================================================ */

void GridEditor::setGroupMoveMode(int groupId)
{
    m_isMovingGroup = true;
    m_movingGroupId = groupId;
    m_originalGroupPositions.clear();
    
    // Store original positions of all sectors in the group
    if (m_mapData) {
        const SectorGroup *group = m_mapData->findGroup(groupId);
        if (group) {
            for (int sectorId : group->sector_ids) {
                const Sector *sector = m_mapData->findSector(sectorId);
                if (sector) {
                    m_originalGroupPositions[sectorId] = sector->vertices;
                }
            }
        }
    }
    
    setCursor(Qt::SizeAllCursor);
    update();
}

void GridEditor::cancelGroupMove()
{
    m_isMovingGroup = false;
    m_movingGroupId = -1;
    m_originalGroupPositions.clear();
    setCursor(Qt::ArrowCursor);
    update();
}



/* ============================================================================
   DRAG AND DROP
   ============================================================================ */

void GridEditor::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void GridEditor::dragMoveEvent(QDragMoveEvent *event)
{
    event->acceptProposedAction();
}

void GridEditor::dropEvent(QDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    
    if (mimeData->hasUrls()) {
        QList<QUrl> urlList = mimeData->urls();
        if (urlList.isEmpty()) return;
        
        QString filePath = urlList.first().toLocalFile();
        QFileInfo info(filePath);
        QString ext = info.suffix().toLower();
        
        if (ext == "md3") {
            // Calculate drop position in world coordinates
            QPointF dropPos = screenToWorld(event->position().toPoint());
            
            // Create Entity Instance
            EntityInstance entity;
            entity.assetPath = filePath;
            entity.type = "model";
            
            // Generate process name from filename (e.g., "caja.md3" -> "caja")
            QString baseName = info.completeBaseName();  // Gets filename without extension
            entity.processName = baseName;
            
            entity.x = dropPos.x();
            entity.y = dropPos.y();
            entity.z = 0.0f;
            entity.spawn_id = m_mapData->getNextSpawnEntityId(); // Unified ID generation
            
            m_mapData->entities.append(entity);
            
            // QMessageBox removed as per user request
                
            update();
            event->acceptProposedAction();
        }
    }
}

void GridEditor::drawEntities(QPainter &painter)
{
    if (!m_mapData) return;

    painter.setBrush(QBrush(QColor(0, 200, 255))); // Cyan for entities
    painter.setPen(QPen(QColor(0, 100, 200), 2));
    
    for (const EntityInstance &entity : m_mapData->entities) {
        QPoint pos = worldToScreen(QPointF(entity.x, entity.y));
        
        // Draw diamond shape
        QPolygon diamond;
        diamond << QPoint(pos.x(), pos.y() - 6)
                << QPoint(pos.x() + 6, pos.y())
                << QPoint(pos.x(), pos.y() + 6)
                << QPoint(pos.x() - 6, pos.y());
        
        painter.drawPolygon(diamond);
        
        // Draw entity name/asset
        painter.setPen(QPen(QColor(200, 255, 255), 1));
        painter.setFont(QFont("Arial", 8));
        QFileInfo info(entity.assetPath);
        painter.drawText(pos + QPoint(8, 0), info.fileName());
    }
}

void GridEditor::updateEntity(int index, const EntityInstance &entity)
{
    if (index >= 0 && index < m_mapData->entities.size()) {
        m_mapData->entities[index] = entity;
        emit mapChanged();
        update();
    }
}
