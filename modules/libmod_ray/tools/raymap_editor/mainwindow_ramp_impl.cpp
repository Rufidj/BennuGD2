void MainWindow::onOpenCameraPathEditor()
{
    // Create and show camera path editor
    CameraPathEditor *editor = new CameraPathEditor(this);
    editor->setMapData(&m_mapData);
    editor->setAttribute(Qt::WA_DeleteOnClose);
    editor->show();
}

void MainWindow::onGenerateRamp()
{
    // Create and show ramp generator dialog
    RampGeneratorDialog *dialog = new RampGeneratorDialog(this);
    
    if (dialog->exec() == QDialog::Accepted) {
        RampParameters params = dialog->getParameters();
        
        // Generate sectors based on type
        QVector<SectorData> generatedSectors;
        if (params.generateAsStairs) {
            generatedSectors = RampGenerator::generateStairs(params);
        } else {
            generatedSectors = RampGenerator::generateRamp(params);
        }
        
        if (generatedSectors.isEmpty()) {
            QMessageBox::warning(this, tr("Error"),
                               tr("No se pudieron generar sectores. Verifica los parámetros."));
            delete dialog;
            return;
        }
        
        // Add generated sectors to map
        for (SectorData &sector : generatedSectors) {
            m_mapData.addSector(sector);
        }
        
        // Update UI
        m_gridEditor->update();
        updateSectorList();
        
        QMessageBox::information(this, tr("Éxito"),
                               tr("¡Generados %1 sectores correctamente!").arg(generatedSectors.size()));
    }
    
    delete dialog;
}
