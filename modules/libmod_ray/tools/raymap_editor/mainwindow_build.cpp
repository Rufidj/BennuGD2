#include "mainwindow.h"
#include "bennugdinstaller.h"
#include "buildmanager.h"
#include "consolewidget.h"
#include <QMessageBox>
#include <QToolBar>
#include <QFileDialog>
#include <QInputDialog>
#include <QDebug>
#include <QDir>
#include "projectmanager.h"
#include "codegenerator.h"
#include "processgenerator.h"

void MainWindow::setupBuildSystem()
{
    // Create Build Manager
    m_buildManager = new BuildManager(this);
    
    // Create Console Widget
    m_consoleWidget = new ConsoleWidget(this);
    m_consoleDock = new QDockWidget(tr("Consola / Salida"), this);
    m_consoleDock->setWidget(m_consoleWidget);
    addDockWidget(Qt::BottomDockWidgetArea, m_consoleDock);
    m_consoleDock->show();  // Show by default for debugging
    
    // Connect signals
    connect(m_buildManager, &BuildManager::buildStarted,
            this, [this]() {
        //m_consoleWidget->clear(); // Don't clear on build, let user decide or history persist
        m_consoleWidget->setBuildMode();
        m_consoleDock->show();
    });
    
    // Connect executeInTerminal signal
    connect(m_buildManager, &BuildManager::executeInTerminal,
            this, [this](const QString &command) {
        m_consoleWidget->sendText(command);
    });
    
    // Obsolete signals (buildFinished, runOutput, etc) are removed 
    // because we now use a real terminal that handles its own output.
    
    connect(m_buildManager, &BuildManager::runStarted,
            this, [this]() {
        m_consoleWidget->setRunMode();
        m_consoleDock->show();
    });
    
    connect(m_consoleWidget, &ConsoleWidget::stopRequested,
            m_buildManager, &BuildManager::stopRunning);
    
    // Check if BennuGD2 is installed

    if (!m_buildManager->isBennuGD2Installed()) {
        QMessageBox::StandardButton reply = QMessageBox::question(this,
            tr("BennuGD2 no encontrado"),
            tr("BennuGD2 no está instalado en tu sistema.\n"
               "¿Deseas descargarlo e instalarlo automáticamente?"),
            QMessageBox::Yes | QMessageBox::No);
        
        if (reply == QMessageBox::Yes) {
            onInstallBennuGD2();
        }
    }
}

void MainWindow::onBuildProject()
{
    QString projectPath;
    
    if (m_projectManager && m_projectManager->hasProject()) {
        projectPath = m_projectManager->getProjectPath();
    } else {
        // Fallback: Use current map directory
        // We need to access the tab widget and get the current file
        // Since mainwindow_build.cpp includes mainwindow.h, we can access members if they are protected/public or we are in MainWindow scope (we are).
        // But we need to ensure we can get the filename.
        // We previously used getCurrentEditor()->fileName()
        // We need to verify if getCurrentEditor is available. It is a member function.
        GridEditor *editor = getCurrentEditor();
        if (editor) {
             QFileInfo fi(editor->fileName());
             if (fi.exists()) projectPath = fi.absolutePath();
        }
        
        if (projectPath.isEmpty()) {
             projectPath = QDir::currentPath();
        }
    }
    
    // Auto-generate code before building to ensure it's up to date
    onGenerateCode();
    
    m_buildManager->buildProject(projectPath);
    m_buildManager->buildProject(projectPath);
}

void MainWindow::onRunProject()
{
    QString projectPath;
    
    if (m_projectManager && m_projectManager->hasProject()) {
        projectPath = m_projectManager->getProjectPath();
    } else {
        GridEditor *editor = getCurrentEditor();
        if (editor) {
             QFileInfo fi(editor->fileName());
             if (fi.exists()) projectPath = fi.absolutePath();
        }
        
        if (projectPath.isEmpty()) {
             projectPath = QDir::currentPath();
        }
    }
    
    m_buildManager->runProject(projectPath);
}

void MainWindow::onBuildAndRun()
{
    QString projectPath;
    
    if (m_projectManager && m_projectManager->hasProject()) {
        projectPath = m_projectManager->getProjectPath();
    } else {
        GridEditor *editor = getCurrentEditor();
        if (editor) {
             QFileInfo fi(editor->fileName());
             if (fi.exists()) projectPath = fi.absolutePath();
        }
        
        if (projectPath.isEmpty()) {
             projectPath = QDir::currentPath();
        }
    }
    
    // Build and Run combined
    m_buildManager->buildAndRunProject(projectPath);
}

void MainWindow::onStopRunning()
{
    m_buildManager->stopRunning();
}

void MainWindow::onInstallBennuGD2()
{
    BennuGDInstaller *installer = new BennuGDInstaller(this);
    
    connect(installer, &BennuGDInstaller::installationFinished,
            this, [this, installer](bool success) {
        if (success) {
            // Re-detect BennuGD2
            m_buildManager->detectBennuGD2();
            QMessageBox::information(this, tr("Success"),
                tr("BennuGD2 installed successfully!"));
        }
        installer->deleteLater();
    });
    
    installer->show();
    installer->startInstallation();
}

void MainWindow::onConfigureBennuGD2()
{
    QString dir = QFileDialog::getExistingDirectory(this,
        tr("Select BennuGD2 Installation Directory (bin folder)"),
        QDir::homePath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!dir.isEmpty()) {
        m_buildManager->setCustomBennuGDPath(dir);
        if (m_buildManager->isBennuGD2Installed()) {
             QMessageBox::information(this, tr("Configuration Saved"),
                tr("BennuGD2 path updated to:\n%1").arg(dir));
        } else {
             QMessageBox::warning(this, tr("Invalid Path"),
                tr("Could not find bgdc/bgdi in the selected directory:\n%1").arg(dir));
        }
    }
}

void MainWindow::onGenerateCode()
{
    if (!m_projectManager || !m_projectManager->hasProject()) return;

    CodeGenerator generator;
    
    // Load project configuration from file (or use defaults if not found)
    QString projectPath = m_projectManager->getProjectPath();
    ProjectData projectData = ProjectManager::loadProjectData(projectPath);
    
    // Update name and path from current project
    const Project *proj = m_projectManager->getProject();
    projectData.name = proj->name;
    projectData.path = proj->path;
    
    generator.setProjectData(projectData);
    
    // Get current map and FPG paths
    GridEditor *editor = getCurrentEditor();
    QString mapPath = "assets/maps/map.raymap";  // Default
    QString fpgPath = "assets/fpg/textures.fpg";  // Default
    
    if (editor && !editor->fileName().isEmpty()) {
        // Get relative path from project root
        QString projectPath = m_projectManager->getProjectPath();
        QFileInfo mapInfo(editor->fileName());
        
        // Calculate relative path
        QDir projectDir(projectPath);
        mapPath = projectDir.relativeFilePath(mapInfo.absoluteFilePath());
        
        qDebug() << "Map path:" << mapPath;
    }
    
    // Get FPG path if one is loaded
    if (!m_currentFPGPath.isEmpty()) {
        QString projectPath = m_projectManager->getProjectPath();
        QFileInfo fpgInfo(m_currentFPGPath);
        QDir projectDir(projectPath);
        fpgPath = projectDir.relativeFilePath(fpgInfo.absoluteFilePath());
        
        qDebug() << "FPG path:" << fpgPath;
    }
    
    // Set paths in generator
    generator.setVariable("INITIAL_MAP", mapPath);
    generator.setVariable("FPG_PATH", fpgPath);
    
    // Get entities from current map
    QVector<EntityInstance> entities;
    if (editor && editor->mapData()) {
        entities = editor->mapData()->entities;
    }
    
    // Generate entity process files
    int entityFilesGenerated = 0;
    if (!entities.isEmpty()) {
        QStringList uniqueProcesses = ProcessGenerator::getUniqueProcessNames(entities);
        
        for (const QString &processName : uniqueProcesses) {
            // Find first entity with this process name to get asset info
            for (const EntityInstance &entity : entities) {
                if (entity.processName == processName) {
                    QString code = ProcessGenerator::generateProcessCode(
                        processName, 
                        entity.assetPath, 
                        entity.type
                    );
                    
                    if (ProcessGenerator::saveProcessFile(
                        m_projectManager->getProjectPath(), 
                        processName, 
                        code)) {
                        entityFilesGenerated++;
                        if (m_consoleWidget)
                            m_consoleWidget->sendText("  Generated: src/includes/" + processName + ".h\n");
                    }
                    break;
                }
            }
        }
    }

    // Generate main.prg with entities
    QString code;
    if (!entities.isEmpty()) {
        code = generator.generateMainPrgWithEntities(entities);
    } else {
        code = generator.generateMainPrg();
    }

    // Save to file
    QString srcDir = m_projectManager->getProjectPath() + "/src";
    QDir().mkpath(srcDir);
    
    QFile file(srcDir + "/main.prg");
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(code.toUtf8());
        file.close();
        if (m_consoleWidget) {
            m_consoleWidget->sendText("Code generated successfully!\n");
            m_consoleWidget->sendText("  Main: src/main.prg\n");
            m_consoleWidget->sendText("  Map: " + mapPath + "\n");
            m_consoleWidget->sendText("  FPG: " + fpgPath + "\n");
            if (entityFilesGenerated > 0) {
                m_consoleWidget->sendText("  Entity types: " + QString::number(entityFilesGenerated) + "\n");
                m_consoleWidget->sendText("  Entity instances: " + QString::number(entities.size()) + "\n");
            }
        }
    } else {
        if (m_consoleWidget)
            m_consoleWidget->sendText("Error generating code: Could not open " + file.fileName() + "\n");
    }
}
