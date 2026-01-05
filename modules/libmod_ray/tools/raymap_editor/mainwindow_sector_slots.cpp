
// Sector control slots
void MainWindow::onSectorIdChanged(int value)
{
    m_gridEditor->setCurrentSectorId(value);
    updateStatusBar(QString("Sector ID: %1").arg(value));
}

void MainWindow::onSectorFloorZChanged(double value)
{
    m_gridEditor->setCurrentSectorFloorZ(static_cast<float>(value));
    updateStatusBar(QString("Altura suelo: %1").arg(value));
}

void MainWindow::onSectorCeilingZChanged(double value)
{
    m_gridEditor->setCurrentSectorCeilingZ(static_cast<float>(value));
    updateStatusBar(QString("Altura techo: %1").arg(value));
}

void MainWindow::onSectorFloorTextureChanged(int value)
{
    m_gridEditor->setCurrentSectorFloorTexture(value);
    updateStatusBar(QString("Textura suelo: %1").arg(value));
}

void MainWindow::onSectorCeilingTextureChanged(int value)
{
    m_gridEditor->setCurrentSectorCeilingTexture(value);
    updateStatusBar(QString("Textura techo: %1").arg(value));
}

void MainWindow::onDetectPortalsClicked()
{
    // TODO: Implementar detección de portales
    // Por ahora, solo crear portales basándose en el sectorGrid
    
    m_mapData.portals.clear();
    int portal_id = 0;
    
    // Recorrer el sectorGrid y buscar bordes entre sectores
    for (int y = 0; y < m_mapData.height; y++) {
        for (int x = 0; x < m_mapData.width; x++) {
            int idx = x + y * m_mapData.width;
            int sector_id = m_mapData.sectorGrid[idx];
            
            if (sector_id < 0) continue;  // Sin sector asignado
            
            // Verificar vecino derecho
            if (x < m_mapData.width - 1) {
                int right_idx = (x + 1) + y * m_mapData.width;
                int right_sector = m_mapData.sectorGrid[right_idx];
                
                if (right_sector >= 0 && right_sector != sector_id) {
                    // Crear portal vertical
                    Portal portal;
                    portal.portal_id = portal_id++;
                    portal.sector_a = sector_id;
                    portal.sector_b = right_sector;
                    portal.x1 = (x + 1) * 128.0f;  // RAY_TILE_SIZE = 128
                    portal.y1 = y * 128.0f;
                    portal.x2 = (x + 1) * 128.0f;
                    portal.y2 = (y + 1) * 128.0f;
                    portal.is_horizontal = 0;
                    m_mapData.portals.append(portal);
                }
            }
            
            // Verificar vecino abajo
            if (y < m_mapData.height - 1) {
                int bottom_idx = x + (y + 1) * m_mapData.width;
                int bottom_sector = m_mapData.sectorGrid[bottom_idx];
                
                if (bottom_sector >= 0 && bottom_sector != sector_id) {
                    // Crear portal horizontal
                    Portal portal;
                    portal.portal_id = portal_id++;
                    portal.sector_a = sector_id;
                    portal.sector_b = bottom_sector;
                    portal.x1 = x * 128.0f;
                    portal.y1 = (y + 1) * 128.0f;
                    portal.x2 = (x + 1) * 128.0f;
                    portal.y2 = (y + 1) * 128.0f;
                    portal.is_horizontal = 1;
                    m_mapData.portals.append(portal);
                }
            }
        }
    }
    
    updateStatusBar(QString("Detectados %1 portales").arg(m_mapData.portals.size()));
    QMessageBox::information(this, tr("Portales Detectados"),
        QString("Se detectaron %1 portales entre sectores").arg(m_mapData.portals.size()));
}

void MainWindow::onAutoAssignSectorsClicked()
{
    // TODO: Implementar asignación automática de sectores
    // Por ahora, crear un sector por defecto
    
    m_mapData.sectors.clear();
    Sector defaultSector;
    defaultSector.sector_id = 0;
    defaultSector.floor_z = 0.0f;
    defaultSector.ceiling_z = 256.0f;
    defaultSector.floor_texture_id = 0;
    defaultSector.ceiling_texture_id = 0;
    defaultSector.light_level = 255;
    
    // Asignar todos los tiles al sector 0
    for (int i = 0; i < m_mapData.sectorGrid.size(); i++) {
        m_mapData.sectorGrid[i] = 0;
        defaultSector.tiles.append(i);
    }
    
    m_mapData.sectors.append(defaultSector);
    
    m_gridEditor->update();
    updateStatusBar("Sector por defecto creado");
    QMessageBox::information(this, tr("Auto-Asignar Sectores"),
        tr("Se creó un sector por defecto (ID 0) con todos los tiles"));
}
