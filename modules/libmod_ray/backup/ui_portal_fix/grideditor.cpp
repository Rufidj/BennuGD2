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
#include <cmath>

GridEditor::GridEditor(QWidget *parent)
    : QWidget(parent)
    , m_mapData(nullptr)
    , m_editMode(MODE_DRAW_SECTOR)
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
    , m_draggedVertex(-1)
    , m_hasCameraPosition(false)
    , m_cameraX(384.0f)
    , m_cameraY(384.0f)
{
    setMinimumSize(800, 600);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void GridEditor::setMapData(MapData *data)
{
    m_mapData = data;
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
    int maxNestingLevel = m_mapData->sectors[bestSector].nesting_level;
    
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
        int nestingLevel = sector.nesting_level;
        
        // Calculate area using shoelace formula
        float area = 0.0f;
        for (int j = 0; j < sector.vertices.size(); j++) {
            int next = (j + 1) % sector.vertices.size();
            area += sector.vertices[j].x() * sector.vertices[next].y();
            area -= sector.vertices[next].x() * sector.vertices[j].y();
        }
        area = std::abs(area) / 2.0f;
        
        // Prefer higher nesting level, or smaller area if tied
        if (nestingLevel > maxNestingLevel || 
            (nestingLevel == maxNestingLevel && area < minArea)) {
            bestSector = sectorIdx;
            maxNestingLevel = nestingLevel;
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
    
    // Draw camera
    drawCamera(painter);
    
    // Draw current polygon being drawn
    if (m_editMode == MODE_DRAW_SECTOR && !m_currentPolygon.isEmpty()) {
        drawCurrentPolygon(painter);
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
        
        // Draw sector ID
        QPointF center(0, 0);
        for (const QPointF &v : sector.vertices) {
            center += v;
        }
        center /= sector.vertices.size();
        QPoint screenCenter = worldToScreen(center);
        
        painter.setPen(QPen(QColor(200, 200, 200), 1));
        painter.drawText(screenCenter, QString("S%1").arg(sector.sector_id));
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
                int flagId = m_mapData ? m_mapData->spawnFlags.size() + 1 : 1;
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
        }
    }
}

void GridEditor::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_draggedVertex = -1;
        if (m_isDraggingSector) {
            m_isDraggingSector = false;
            setCursor(Qt::ArrowCursor);
            // Maybe notify mainwindow that map changed?
        }
    }
    else if (event->button() == Qt::MiddleButton) {
        setCursor(Qt::CrossCursor);
    }
}

void GridEditor::wheelEvent(QWheelEvent *event)
{
    float delta = event->angleDelta().y() > 0 ? 1.1f : 0.9f;
    setZoom(m_zoom * delta);
}



void GridEditor::contextMenuEvent(QContextMenuEvent *event)
{
    QPointF worldPos = screenToWorld(event->pos());
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
