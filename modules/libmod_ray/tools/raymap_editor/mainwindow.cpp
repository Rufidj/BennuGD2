/*
 * mainwindow.cpp - Main window for geometric sector map editor
 * Simplified version for Build Engine-style editing
 */

#include "mainwindow.h"
#include "raymapformat.h"
#include "fpgloader.h"
#include "grideditor.h" // Added based on instruction
#include "wldimporter.h"  // WLD import support
#include "insertboxdialog.h"  // Insert Box dialog
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QSettings>
#include <QFileInfo>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QApplication>
#include <cmath>
#include <QMenu> // Added based on instruction
#include <QColorDialog> // Added based on instruction
#include <QDebug> // Added based on instruction
#include <algorithm> // for std::max/min // Added based on instruction
#include <QDialog>
#include <QDialogButtonBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_currentFPG(0)
    , m_selectedSectorId(-1)
    , m_selectedWallId(-1)
    , m_visualModeWidget(nullptr)
{
    setWindowTitle("RayMap Editor - Geometric Sectors");
    resize(1280, 800);
    
    // Create grid editor
    m_gridEditor = new GridEditor(this);
    m_gridEditor->setMapData(&m_mapData);
    setCentralWidget(m_gridEditor);
    
    // Connect signals
    connect(m_gridEditor, &GridEditor::sectorSelected, this, &MainWindow::onSectorSelected);
    connect(m_gridEditor, &GridEditor::sectorCreated, this, &MainWindow::updateSectorList);  // NEW
    connect(m_gridEditor, &GridEditor::wallSelected, this, &MainWindow::onWallSelected);
    connect(m_gridEditor, &GridEditor::vertexSelected, this, &MainWindow::onVertexSelected);
    connect(m_gridEditor, &GridEditor::cameraPlaced, this, &MainWindow::onCameraPlaced);
    connect(m_gridEditor, &GridEditor::portalWallSelected, this, &MainWindow::onManualPortalWallSelected); 
    connect(m_gridEditor, &GridEditor::requestDeletePortal, this, &MainWindow::onDeletePortal); // NEW
    connect(m_gridEditor, &GridEditor::mapChanged, this, &MainWindow::updateVisualMode);
    
    // Create UI
    createActions();
    createMenus();
    createToolbars();
    createDockWindows();
    createStatusBar();
    
    updateWindowTitle();
}

MainWindow::~MainWindow()
{
}

/* ============================================================================
   UI CREATION
   ============================================================================ */

void MainWindow::createActions()
{
    // File actions
    m_newAction = new QAction(tr("&Nuevo"), this);
    m_newAction->setShortcut(QKeySequence::New);
    connect(m_newAction, &QAction::triggered, this, &MainWindow::onNewMap);
    
    m_openAction = new QAction(tr("&Abrir..."), this);
    m_openAction->setShortcut(QKeySequence::Open);
    connect(m_openAction, &QAction::triggered, this, &MainWindow::onOpenMap);
    
    m_saveAction = new QAction(tr("&Guardar"), this);
    m_saveAction->setShortcut(QKeySequence::Save);
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::onSaveMap);
    
    m_saveAsAction = new QAction(tr("Guardar &como..."), this);
    m_saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::onSaveMapAs);
    
    m_loadFPGAction = new QAction(tr("Cargar &FPG..."), this);
    connect(m_loadFPGAction, &QAction::triggered, this, &MainWindow::onLoadFPG);
    
    m_exitAction = new QAction(tr("&Salir"), this);
    m_exitAction->setShortcut(QKeySequence::Quit);
    connect(m_exitAction, &QAction::triggered, this, &MainWindow::onExit);
    
    // View actions
    m_zoomInAction = new QAction(tr("Acercar"), this);
    m_zoomInAction->setShortcut(QKeySequence::ZoomIn);
    connect(m_zoomInAction, &QAction::triggered, this, &MainWindow::onZoomIn);
    
    m_zoomOutAction = new QAction(tr("Alejar"), this);
    m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(m_zoomOutAction, &QAction::triggered, this, &MainWindow::onZoomOut);
    
    m_zoomResetAction = new QAction(tr("Restablecer zoom"), this);
    connect(m_zoomResetAction, &QAction::triggered, this, &MainWindow::onZoomReset);
    
    m_visualModeAction = new QAction(tr("Modo &Visual"), this);
    m_visualModeAction->setShortcut(QKeySequence(tr("F3")));
    connect(m_visualModeAction, &QAction::triggered, this, &MainWindow::onToggleVisualMode);
}

void MainWindow::createMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&Archivo"));
    fileMenu->addAction(m_newAction);
    fileMenu->addAction(m_openAction);
    fileMenu->addAction(m_saveAction);
    fileMenu->addAction(m_saveAsAction);
    fileMenu->addSeparator();
    
    // Import WLD action
    QAction *importWLDAction = new QAction(tr("Importar WLD..."), this);
    importWLDAction->setToolTip(tr("Importar mapa desde formato WLD"));
    connect(importWLDAction, &QAction::triggered, this, &MainWindow::onImportWLD);
    fileMenu->addAction(importWLDAction);
    
    fileMenu->addSeparator();
    fileMenu->addAction(m_loadFPGAction);
    
    // Recent files submenus
    fileMenu->addSeparator();
    m_recentMapsMenu = fileMenu->addMenu(tr("Mapas Recientes"));
    m_recentFPGsMenu = fileMenu->addMenu(tr("FPGs Recientes"));
    updateRecentMapsMenu();
    updateRecentFPGsMenu();
    
    fileMenu->addSeparator();
    fileMenu->addAction(m_exitAction);
    
    QMenu *viewMenu = menuBar()->addMenu(tr("&Ver"));
    viewMenu->addAction(m_zoomInAction);
    viewMenu->addAction(m_zoomOutAction);
    viewMenu->addAction(m_zoomResetAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_visualModeAction);
    
    // Insert Menu
    QMenu *insertMenu = menuBar()->addMenu(tr("&Insertar"));
    
    // Insert Box
    m_insertBoxAction = new QAction(QIcon::fromTheme("insert-object"), tr("Caja"), this);
    m_insertBoxAction->setShortcut(QKeySequence(tr("Ctrl+B")));
    m_insertBoxAction->setToolTip(tr("Insertar una caja rectangular dentro del sector actual.\nCrea automáticamente el sector y los portales necesarios."));
    m_insertBoxAction->setStatusTip(tr("Insertar caja con portales automáticos"));
    connect(m_insertBoxAction, &QAction::triggered, this, &MainWindow::onInsertBox);
    insertMenu->addAction(m_insertBoxAction);
    
    // Insert Column
    m_insertColumnAction = new QAction(QIcon::fromTheme("insert-object"), tr("Columna"), this);
    m_insertColumnAction->setShortcut(QKeySequence(tr("Ctrl+L")));
    m_insertColumnAction->setToolTip(tr("Insertar una columna (caja pequeña) dentro del sector actual.\nÚtil para pilares y soportes."));
    m_insertColumnAction->setStatusTip(tr("Insertar columna"));
    connect(m_insertColumnAction, &QAction::triggered, this, &MainWindow::onInsertColumn);
    insertMenu->addAction(m_insertColumnAction);
    
    // Insert Platform
    m_insertPlatformAction = new QAction(QIcon::fromTheme("go-up"), tr("Plataforma"), this);
    m_insertPlatformAction->setShortcut(QKeySequence(tr("Ctrl+P")));
    m_insertPlatformAction->setToolTip(tr("Insertar una plataforma elevada dentro del sector actual.\nCrea un sector con suelo más alto."));
    m_insertPlatformAction->setStatusTip(tr("Insertar plataforma elevada"));
    connect(m_insertPlatformAction, &QAction::triggered, this, &MainWindow::onInsertPlatform);
    insertMenu->addAction(m_insertPlatformAction);
    
    insertMenu->addSeparator();
    
    // Sector Menu
    QMenu *sectorMenu = menuBar()->addMenu(tr("&Sector"));
    QAction *setParentAction = new QAction(tr("Asignar Sector Padre..."), this);
    setParentAction->setShortcut(QKeySequence(tr("Ctrl+P")));
    connect(setParentAction, &QAction::triggered, this, &MainWindow::onSetParentSector);
    sectorMenu->addAction(setParentAction);
    
    // Future tools (disabled for now)
    m_insertDoorAction = new QAction(QIcon::fromTheme("door-open"), tr("Puerta (Próximamente)"), this);
    m_insertDoorAction->setEnabled(false);
    m_insertDoorAction->setToolTip(tr("Insertar una puerta deslizante o giratoria.\n[Función en desarrollo]"));
    insertMenu->addAction(m_insertDoorAction);
    
    m_insertElevatorAction = new QAction(QIcon::fromTheme("go-jump"), tr("Ascensor (Próximamente)"), this);
    m_insertElevatorAction->setEnabled(false);
    m_insertElevatorAction->setToolTip(tr("Insertar un ascensor o plataforma móvil.\n[Función en desarrollo]"));
    insertMenu->addAction(m_insertElevatorAction);
    
    m_insertStairsAction = new QAction(QIcon::fromTheme("go-up"), tr("Escalera (Próximamente)"), this);
    m_insertStairsAction->setEnabled(false);
    m_insertStairsAction->setToolTip(tr("Insertar una escalera con múltiples escalones.\n[Función en desarrollo]"));
    insertMenu->addAction(m_insertStairsAction);
}

void MainWindow::createToolbars()
{
    QToolBar *toolbar = addToolBar(tr("Principal"));
    
    // Edit mode selector
    toolbar->addWidget(new QLabel(tr(" Modo: ")));
    m_modeCombo = new QComboBox();
    m_modeCombo->addItem("Dibujar Sector");
    m_modeCombo->addItem("Editar Vértices");
    m_modeCombo->addItem("Seleccionar Pared");
    m_modeCombo->addItem("Colocar Sprite");
    m_modeCombo->addItem("Colocar Spawn");
    m_modeCombo->addItem("Colocar Cámara");
    m_modeCombo->addItem("Seleccionar Sector"); // New option
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onModeChanged);
    toolbar->addWidget(m_modeCombo);
    
    toolbar->addSeparator();
    
    // Texture selector
    toolbar->addWidget(new QLabel(tr(" Textura: ")));
    m_selectedTextureSpin = new QSpinBox();
    m_selectedTextureSpin->setRange(0, 9999);
    m_selectedTextureSpin->setValue(1);
    connect(m_selectedTextureSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onTextureSelected);
    toolbar->addWidget(m_selectedTextureSpin);
    
    toolbar->addSeparator();
    
    // Skybox selector
    toolbar->addWidget(new QLabel(tr(" Cielo: ")));
    m_skyboxSpin = new QSpinBox();
    m_skyboxSpin->setRange(0, 9999);
    m_skyboxSpin->setValue(0);
    m_skyboxSpin->setToolTip(tr("ID de textura del cielo (0 = azul sólido)"));
    connect(m_skyboxSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onSkyboxTextureChanged);
    toolbar->addWidget(m_skyboxSpin);
    
    toolbar->addSeparator();
    
    // Tools
    // Tools
    m_manualPortalsButton = new QPushButton(tr("P. Manual"));
    m_manualPortalsButton->setCheckable(true);
    m_manualPortalsButton->setToolTip(tr("Modo Manual: Click en Pared A -> Click en Pared B para crear portal"));
    connect(m_manualPortalsButton, &QPushButton::toggled, this, &MainWindow::onToggleManualPortals);
    toolbar->addWidget(m_manualPortalsButton);
    
    QPushButton *detectPortalsBtn = new QPushButton(tr("Auto Portales"));
    detectPortalsBtn->setToolTip(tr("Borrar todo y detectar portales automáticamente"));
    connect(detectPortalsBtn, &QPushButton::clicked, this, &MainWindow::onDetectPortals);
    toolbar->addWidget(detectPortalsBtn);
    
    toolbar->addSeparator();
    
    // Shape tools
    toolbar->addWidget(new QLabel(tr(" Formas: ")));
    
    QPushButton *rectBtn = new QPushButton(tr("Rectángulo"));
    rectBtn->setToolTip(tr("Crear un sector rectangular de 512x512"));
    connect(rectBtn, &QPushButton::clicked, this, &MainWindow::onCreateRectangle);
    toolbar->addWidget(rectBtn);
    
    QPushButton *circleBtn = new QPushButton(tr("Círculo"));
    circleBtn->setToolTip(tr("Crear un sector circular de radio 256"));
    connect(circleBtn, &QPushButton::clicked, this, &MainWindow::onCreateCircle);
    toolbar->addWidget(circleBtn);
}

void MainWindow::createDockWindows()
{
    // Sector properties dock
    m_sectorDock = new QDockWidget(tr("Propiedades del Sector"), this);
    QWidget *sectorWidget = new QWidget();
    QVBoxLayout *sectorLayout = new QVBoxLayout();
    
    m_sectorIdLabel = new QLabel(tr("Ningún sector seleccionado"));
    sectorLayout->addWidget(m_sectorIdLabel);
    
    QGroupBox *heightGroup = new QGroupBox(tr("Alturas"));
    QVBoxLayout *heightLayout = new QVBoxLayout();
    
    QHBoxLayout *floorLayout = new QHBoxLayout();
    floorLayout->addWidget(new QLabel(tr("Suelo Z:")));
    m_sectorFloorZSpin = new QDoubleSpinBox();
    m_sectorFloorZSpin->setRange(-1000, 1000);
    m_sectorFloorZSpin->setValue(0);
    connect(m_sectorFloorZSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onSectorFloorZChanged);
    floorLayout->addWidget(m_sectorFloorZSpin);
    heightLayout->addLayout(floorLayout);
    
    QHBoxLayout *ceilingLayout = new QHBoxLayout();
    ceilingLayout->addWidget(new QLabel(tr("Techo Z:")));
    m_sectorCeilingZSpin = new QDoubleSpinBox();
    m_sectorCeilingZSpin->setRange(-1000, 1000);
    m_sectorCeilingZSpin->setValue(256);
    connect(m_sectorCeilingZSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onSectorCeilingZChanged);
    ceilingLayout->addWidget(m_sectorCeilingZSpin);
    heightLayout->addLayout(ceilingLayout);
    
    heightGroup->setLayout(heightLayout);
    sectorLayout->addWidget(heightGroup);
    
    QGroupBox *textureGroup = new QGroupBox(tr("Texturas"));
    QVBoxLayout *textureLayout = new QVBoxLayout();
    
    QHBoxLayout *floorTexLayout = new QHBoxLayout();
    floorTexLayout->addWidget(new QLabel(tr("Suelo:")));
    m_sectorFloorTextureSpin = new QSpinBox();
    m_sectorFloorTextureSpin->setRange(0, 9999);
    connect(m_sectorFloorTextureSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onSectorFloorTextureChanged);
    floorTexLayout->addWidget(m_sectorFloorTextureSpin);
    QPushButton *selectFloorBtn = new QPushButton(tr("..."));
    selectFloorBtn->setMaximumWidth(30);
    selectFloorBtn->setToolTip(tr("Seleccionar textura"));
    connect(selectFloorBtn, &QPushButton::clicked, this, &MainWindow::onSelectSectorFloorTexture);
    floorTexLayout->addWidget(selectFloorBtn);
    textureLayout->addLayout(floorTexLayout);
    
    QHBoxLayout *ceilingTexLayout = new QHBoxLayout();
    ceilingTexLayout->addWidget(new QLabel(tr("Techo:")));
    m_sectorCeilingTextureSpin = new QSpinBox();
    m_sectorCeilingTextureSpin->setRange(0, 9999);
    connect(m_sectorCeilingTextureSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onSectorCeilingTextureChanged);
    ceilingTexLayout->addWidget(m_sectorCeilingTextureSpin);
    QPushButton *selectCeilingBtn = new QPushButton(tr("..."));
    selectCeilingBtn->setMaximumWidth(30);
    selectCeilingBtn->setToolTip(tr("Seleccionar textura"));
    connect(selectCeilingBtn, &QPushButton::clicked, this, &MainWindow::onSelectSectorCeilingTexture);
    ceilingTexLayout->addWidget(selectCeilingBtn);
    textureLayout->addLayout(ceilingTexLayout);
    
    textureGroup->setLayout(textureLayout);
    sectorLayout->addWidget(textureGroup);
    

    
    sectorLayout->addStretch();
    sectorWidget->setLayout(sectorLayout);
    m_sectorDock->setWidget(sectorWidget);
    addDockWidget(Qt::RightDockWidgetArea, m_sectorDock);
    
    // Wall properties dock
    m_wallDock = new QDockWidget(tr("Propiedades de la Pared"), this);
    QWidget *wallWidget = new QWidget();
    QVBoxLayout *wallLayout = new QVBoxLayout();
    
    m_wallIdLabel = new QLabel(tr("Ninguna pared seleccionada"));
    wallLayout->addWidget(m_wallIdLabel);
    
    QGroupBox *wallTexGroup = new QGroupBox(tr("Texturas (Inferior/Media/Superior)"));
    QVBoxLayout *wallTexLayout = new QVBoxLayout();
    
    QHBoxLayout *lowerLayout = new QHBoxLayout();
    lowerLayout->addWidget(new QLabel(tr("Inferior:")));
    m_wallTextureLowerSpin = new QSpinBox();
    m_wallTextureLowerSpin->setRange(0, 9999);
    connect(m_wallTextureLowerSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onWallTextureLowerChanged);
    lowerLayout->addWidget(m_wallTextureLowerSpin);
    QPushButton *selectLowerBtn = new QPushButton(tr("..."));
    selectLowerBtn->setMaximumWidth(30);
    selectLowerBtn->setToolTip(tr("Seleccionar textura"));
    connect(selectLowerBtn, &QPushButton::clicked, this, &MainWindow::onSelectWallTextureLower);
    lowerLayout->addWidget(selectLowerBtn);
    wallTexLayout->addLayout(lowerLayout);
    
    QHBoxLayout *middleLayout = new QHBoxLayout();
    middleLayout->addWidget(new QLabel(tr("Media:")));
    m_wallTextureMiddleSpin = new QSpinBox();
    m_wallTextureMiddleSpin->setRange(0, 9999);
    connect(m_wallTextureMiddleSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onWallTextureMiddleChanged);
    middleLayout->addWidget(m_wallTextureMiddleSpin);
    QPushButton *selectMiddleBtn = new QPushButton(tr("..."));
    selectMiddleBtn->setMaximumWidth(30);
    selectMiddleBtn->setToolTip(tr("Seleccionar textura"));
    connect(selectMiddleBtn, &QPushButton::clicked, this, &MainWindow::onSelectWallTextureMiddle);
    middleLayout->addWidget(selectMiddleBtn);
    wallTexLayout->addLayout(middleLayout);
    
    QHBoxLayout *upperLayout = new QHBoxLayout();
    upperLayout->addWidget(new QLabel(tr("Superior:")));
    m_wallTextureUpperSpin = new QSpinBox();
    m_wallTextureUpperSpin->setRange(0, 9999);
    connect(m_wallTextureUpperSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onWallTextureUpperChanged);
    upperLayout->addWidget(m_wallTextureUpperSpin);
    QPushButton *selectUpperBtn = new QPushButton(tr("..."));
    selectUpperBtn->setMaximumWidth(30);
    selectUpperBtn->setToolTip(tr("Seleccionar textura"));
    connect(selectUpperBtn, &QPushButton::clicked, this, &MainWindow::onSelectWallTextureUpper);
    upperLayout->addWidget(selectUpperBtn);
    wallTexLayout->addLayout(upperLayout);
    
    wallTexGroup->setLayout(wallTexLayout);
    wallLayout->addWidget(wallTexGroup);
    
    // Button to apply texture to all walls
    QPushButton *applyAllBtn = new QPushButton(tr("Aplicar textura media a TODAS las paredes del sector"));
    applyAllBtn->setToolTip(tr("Aplica la textura media actual a todas las paredes de este sector"));
    connect(applyAllBtn, &QPushButton::clicked, this, &MainWindow::onApplyTextureToAllWalls);
    wallLayout->addWidget(applyAllBtn);
    
    QGroupBox *splitGroup = new QGroupBox(tr("Divisiones de Textura (Z)"));
    QVBoxLayout *splitLayout = new QVBoxLayout();
    
    QHBoxLayout *splitLowerLayout = new QHBoxLayout();
    splitLowerLayout->addWidget(new QLabel(tr("Inferior:")));
    m_wallSplitLowerSpin = new QDoubleSpinBox();
    m_wallSplitLowerSpin->setRange(0, 1000);
    m_wallSplitLowerSpin->setValue(64);
    connect(m_wallSplitLowerSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onWallSplitLowerChanged);
    splitLowerLayout->addWidget(m_wallSplitLowerSpin);
    splitLayout->addLayout(splitLowerLayout);
    
    QHBoxLayout *splitUpperLayout = new QHBoxLayout();
    splitUpperLayout->addWidget(new QLabel(tr("Superior:")));
    m_wallSplitUpperSpin = new QDoubleSpinBox();
    m_wallSplitUpperSpin->setRange(0, 1000);
    m_wallSplitUpperSpin->setValue(192);
    connect(m_wallSplitUpperSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onWallSplitUpperChanged);
    splitUpperLayout->addWidget(m_wallSplitUpperSpin);
    splitLayout->addLayout(splitUpperLayout);
    
    splitGroup->setLayout(splitLayout);
    wallLayout->addWidget(splitGroup);
    
    // --- SPECIAL PORTAL TEXTURE GROUP (User Request) ---
    m_portalTexGroup = new QGroupBox(tr("Texturas de Portal"));
    QVBoxLayout *portalTexLayout = new QVBoxLayout();
    
    QHBoxLayout *portalUpperLayout = new QHBoxLayout();
    portalUpperLayout->addWidget(new QLabel(tr("Escalón Superior:")));
    m_portalUpperSpin = new QSpinBox();
    m_portalUpperSpin->setRange(0, 9999);
    connect(m_portalUpperSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onPortalUpperChanged);
    portalUpperLayout->addWidget(m_portalUpperSpin);
    QPushButton *selPortalUpBtn = new QPushButton(tr("..."));
    selPortalUpBtn->setMaximumWidth(30);
    connect(selPortalUpBtn, &QPushButton::clicked, this, &MainWindow::onSelectPortalUpper);
    portalUpperLayout->addWidget(selPortalUpBtn);
    portalTexLayout->addLayout(portalUpperLayout);
    
    QHBoxLayout *portalLowerLayout = new QHBoxLayout();
    portalLowerLayout->addWidget(new QLabel(tr("Escalón Inferior:")));
    m_portalLowerSpin = new QSpinBox();
    m_portalLowerSpin->setRange(0, 9999);
    connect(m_portalLowerSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onPortalLowerChanged);
    portalLowerLayout->addWidget(m_portalLowerSpin);
    QPushButton *selPortalDownBtn = new QPushButton(tr("..."));
    selPortalDownBtn->setMaximumWidth(30);
    connect(selPortalDownBtn, &QPushButton::clicked, this, &MainWindow::onSelectPortalLower);
    portalLowerLayout->addWidget(selPortalDownBtn);
    portalTexLayout->addLayout(portalLowerLayout);
    
    m_portalTexGroup->setLayout(portalTexLayout);
    m_portalTexGroup->setVisible(false); // Hidden by default
    wallLayout->addWidget(m_portalTexGroup);
    // ----------------------------------------------------
    
    wallLayout->addStretch();
    wallWidget->setLayout(wallLayout);
    m_wallDock->setWidget(wallWidget);
    addDockWidget(Qt::RightDockWidgetArea, m_wallDock);
    
    // Sector List Dock (NEW - on the left)
    m_sectorListDock = new QDockWidget(tr("Lista de Sectores"), this);
    m_sectorListWidget = new QListWidget();
    m_sectorListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_sectorListWidget, &QListWidget::itemClicked,
            this, &MainWindow::onSectorListItemClicked);
    connect(m_sectorListWidget, &QListWidget::customContextMenuRequested,
            this, &MainWindow::onSectorListContextMenu);
    m_sectorListDock->setWidget(m_sectorListWidget);
    addDockWidget(Qt::LeftDockWidgetArea, m_sectorListDock);
}

void MainWindow::createStatusBar()
{
    m_statusLabel = new QLabel(tr("Ready"));
    statusBar()->addWidget(m_statusLabel);
}

/* ============================================================================
   FILE OPERATIONS
   ============================================================================ */

void MainWindow::onNewMap()
{
    m_mapData = MapData();
    m_currentMapFile.clear();
    m_gridEditor->setMapData(&m_mapData);
    updateWindowTitle();
    
    // Clear sector list UI
    m_sectorListWidget->clear();
    
    // Sync UI
    m_skyboxSpin->blockSignals(true);
    m_skyboxSpin->setValue(0);
    m_skyboxSpin->blockSignals(false);
    
    m_statusLabel->setText(tr("New map created"));
}

void MainWindow::onOpenMap()
{
    QString filename = QFileDialog::getOpenFileName(this, tr("Open Map"),
                                                    "", tr("RayMap Files (*.raymap)"));
    if (filename.isEmpty()) return;
    
    if (RayMapFormat::loadMap(filename, m_mapData)) {
        m_currentMapFile = filename;
        updateWindowTitle();
        m_gridEditor->setMapData(&m_mapData);
        updateSectorList();  // NEW: Update sector list
        
        // Sync UI
        m_skyboxSpin->blockSignals(true);
        m_skyboxSpin->setValue(m_mapData.skyTextureID);
        m_skyboxSpin->blockSignals(false);
        
        m_statusLabel->setText(tr("Map loaded: %1").arg(filename));
    } else {
        QMessageBox::critical(this, tr("Error"), tr("Failed to load map"));
    }
}

void MainWindow::onSaveMap()
{
    if (m_currentMapFile.isEmpty()) {
        onSaveMapAs();
        return;
    }
    
    /* Update map data with current editor state */
    /* Note: Camera X/Y should be updated via onCameraPlaced signal */
    /* If Z is 0 (default/missing), set it to player height (32) so floor is visible */
    if (m_mapData.camera.z < 1.0f) {
        m_mapData.camera.z = 32.0f;
    }
    
    if (RayMapFormat::saveMap(m_currentMapFile, m_mapData)) {
        addToRecentMaps(m_currentMapFile);
        m_statusLabel->setText(tr("Map saved: %1").arg(m_currentMapFile));
    } else {
        QMessageBox::critical(this, tr("Error"), tr("Failed to save map"));
    }
}

void MainWindow::onSaveMapAs()
{
    QString filename = QFileDialog::getSaveFileName(this, tr("Save Map As"),
                                                    m_currentMapFile,
                                                    tr("Ray Maps (*.raymap)"));
    if (filename.isEmpty())
        return;
        
    if (!filename.endsWith(".raymap")) {
        filename += ".raymap";
    }
    
    /* Update map data with current editor state */
    /* Note: Camera X/Y should be updated via onCameraPlaced signal */
    if (m_mapData.camera.z < 1.0f) {
        m_mapData.camera.z = 32.0f;
    }
    
    if (RayMapFormat::saveMap(filename, m_mapData)) {
        m_currentMapFile = filename;
        updateWindowTitle();
        addToRecentMaps(filename);
        m_statusLabel->setText(tr("Map saved: %1").arg(filename));
    } else {
        QMessageBox::critical(this, tr("Error"), tr("Failed to save map"));
    }
}

void MainWindow::onCameraPlaced(float x, float y)
{
    m_mapData.camera.x = x;
    m_mapData.camera.y = y;
    // Keep existing Z/rotation
    
    qDebug() << "DEBUG: Camera placed and saved at:" << x << "," << y;
    
    m_statusLabel->setText(tr("Camera placed at (%1, %2)").arg(x).arg(y));
}

void MainWindow::onImportWLD()
{
    QString filename = QFileDialog::getOpenFileName(this, tr("Importar Archivo WLD"),
                                                    "", tr("WLD Files (*.wld)"));
    if (filename.isEmpty()) return;
    
    // Show confirmation dialog
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, 
        tr("Importar WLD"),
        tr("¿Importar el archivo WLD?\n\nEsto reemplazará el mapa actual."),
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply != QMessageBox::Yes) return;
    
    // Import WLD file
    m_statusLabel->setText(tr("Importando WLD..."));
    qApp->processEvents();
    
    if (WLDImporter::importWLD(filename, m_mapData)) {
        // Update editor
        m_gridEditor->setMapData(&m_mapData);
        updateSectorList();
        
        // Clear current file (force Save As)
        m_currentMapFile.clear();
        updateWindowTitle();
        
        m_statusLabel->setText(tr("WLD importado: %1 sectores, %2 portales")
                              .arg(m_mapData.sectors.size())
                              .arg(m_mapData.portals.size()));
        
        QMessageBox::information(this, tr("Importación Exitosa"),
                               tr("Archivo WLD importado correctamente.\n\n"
                                  "Sectores: %1\n"
                                  "Portales: %2\n"
                                  "Spawn Flags: %3\n\n"
                                  "Usa 'Guardar como...' para guardar en formato .raymap")
                               .arg(m_mapData.sectors.size())
                               .arg(m_mapData.portals.size())
                               .arg(m_mapData.spawnFlags.size()));
    } else {
        m_statusLabel->setText(tr("Error al importar WLD"));
        QMessageBox::critical(this, tr("Error"),
                            tr("No se pudo importar el archivo WLD.\n\n"
                               "Verifica que el archivo sea válido."));
    }
}

void MainWindow::onLoadFPG()
{
    QString filename = QFileDialog::getOpenFileName(this, tr("Load FPG"),
                                                    "", tr("FPG Files (*.fpg)"));
    if (filename.isEmpty()) return;
    
    QVector<TextureEntry> textures;
    
    // Load FPG with progress callback
    bool success = FPGLoader::loadFPG(filename, textures, 
        [this](int current, int total, const QString &name) {
            m_statusLabel->setText(tr("Loading FPG: %1/%2 - %3")
                                  .arg(current).arg(total).arg(name));
            qApp->processEvents();
        });
    
    if (success) {
        // Convert to texture map
        QMap<int, QPixmap> textureMap = FPGLoader::getTextureMap(textures);
        
        // Store textures in map data
        m_mapData.textures.clear();
        for (const TextureEntry &entry : textures) {
            m_mapData.textures.append(entry);
        }
        
        // Update grid editor
        m_gridEditor->setTextures(textureMap);
        
        // Cache textures for selector
        m_textureCache.clear();
        for (const TextureEntry &entry : textures) {
            m_textureCache[entry.id] = entry.pixmap;
        }
        
        addToRecentFPGs(filename);
        m_statusLabel->setText(tr("FPG loaded: %1 textures from %2")
                              .arg(textures.size()).arg(filename));
    } else {
        QMessageBox::critical(this, tr("Error"), 
                            tr("Failed to load FPG file: %1").arg(filename));
        m_statusLabel->setText(tr("Failed to load FPG"));
    }
}

void MainWindow::onExit()
{
    close();
}

// ============================================================================
// RECENT FILES
// ============================================================================

void MainWindow::updateRecentMapsMenu()
{
    m_recentMapsMenu->clear();
    
    QSettings settings("BennuGD", "RayMapEditor");
    QStringList recentMaps = settings.value("recentMaps").toStringList();
    
    for (const QString &file : recentMaps) {
        if (QFile::exists(file)) {
            QAction *action = m_recentMapsMenu->addAction(QFileInfo(file).fileName());
            action->setData(file);
            action->setToolTip(file);
            connect(action, &QAction::triggered, this, &MainWindow::openRecentMap);
        }
    }
    
    if (m_recentMapsMenu->isEmpty()) {
        m_recentMapsMenu->addAction(tr("(Ninguno)"))->setEnabled(false);
    }
}

void MainWindow::updateRecentFPGsMenu()
{
    m_recentFPGsMenu->clear();
    
    QSettings settings("BennuGD", "RayMapEditor");
    QStringList recentFPGs = settings.value("recentFPGs").toStringList();
    
    for (const QString &file : recentFPGs) {
        if (QFile::exists(file)) {
            QAction *action = m_recentFPGsMenu->addAction(QFileInfo(file).fileName());
            action->setData(file);
            action->setToolTip(file);
            connect(action, &QAction::triggered, this, &MainWindow::openRecentFPG);
        }
    }
    
    if (m_recentFPGsMenu->isEmpty()) {
        m_recentFPGsMenu->addAction(tr("(Ninguno)"))->setEnabled(false);
    }
}

void MainWindow::addToRecentMaps(const QString &filename)
{
    QSettings settings("BennuGD", "RayMapEditor");
    QStringList recentMaps = settings.value("recentMaps").toStringList();
    
    recentMaps.removeAll(filename);  // Remove if already exists
    recentMaps.prepend(filename);    // Add to front
    
    while (recentMaps.size() > 10) {  // Keep only 10 most recent
        recentMaps.removeLast();
    }
    
    settings.setValue("recentMaps", recentMaps);
    updateRecentMapsMenu();
}

void MainWindow::addToRecentFPGs(const QString &filename)
{
    QSettings settings("BennuGD", "RayMapEditor");
    QStringList recentFPGs = settings.value("recentFPGs").toStringList();
    
    recentFPGs.removeAll(filename);
    recentFPGs.prepend(filename);
    
    while (recentFPGs.size() > 10) {
        recentFPGs.removeLast();
    }
    
    settings.setValue("recentFPGs", recentFPGs);
    updateRecentFPGsMenu();
}

void MainWindow::openRecentMap()
{
    QAction *action = qobject_cast<QAction*>(sender());
    if (!action) return;
    
    QString filename = action->data().toString();
    if (RayMapFormat::loadMap(filename, m_mapData)) {
        m_currentMapFile = filename;
        updateWindowTitle();
        m_gridEditor->setMapData(&m_mapData);
        updateSectorList();
        updateVisualMode();
        addToRecentMaps(filename);
        m_statusLabel->setText(tr("Mapa cargado: %1").arg(QFileInfo(filename).fileName()));
    }
}

void MainWindow::openRecentFPG()
{
    QAction *action = qobject_cast<QAction*>(sender());
    if (!action) return;
    
    QString filename = action->data().toString();
    
    // Reload FPG using the same logic as onLoadFPG
    QVector<TextureEntry> textures;
    bool success = FPGLoader::loadFPG(filename, textures, nullptr);
    
    if (success) {
        addToRecentFPGs(filename);
        m_statusLabel->setText(tr("FPG cargado: %1").arg(QFileInfo(filename).fileName()));
        updateVisualMode();
    }
}

/* ============================================================================
   VIEW OPERATIONS
   ============================================================================ */

void MainWindow::onZoomIn()
{
    m_gridEditor->setZoom(m_gridEditor->property("zoom").toFloat() * 1.2f);
}

void MainWindow::onZoomOut()
{
    m_gridEditor->setZoom(m_gridEditor->property("zoom").toFloat() / 1.2f);
}

void MainWindow::onZoomReset()
{
    m_gridEditor->setZoom(1.0f);
}

void MainWindow::onToggleVisualMode()
{
    // Create widget if it doesn't exist
    if (!m_visualModeWidget) {
        m_visualModeWidget = new VisualModeWidget();
        
        // Set map data
        m_visualModeWidget->setMapData(m_mapData);
        
        // Load textures
        qDebug() << "Loading" << m_mapData.textures.size() << "textures to Visual Mode...";
        for (const TextureEntry &entry : m_mapData.textures) {
            qDebug() << "  Loading texture ID" << entry.id << "size:" << entry.pixmap.size();
            m_visualModeWidget->loadTexture(entry.id, entry.pixmap.toImage());
        }
        
        qDebug() << "Visual Mode widget created with" << m_mapData.textures.size() << "textures";
    }
    
    // Toggle visibility
    if (m_visualModeWidget->isVisible()) {
        m_visualModeWidget->hide();
    } else {
        m_visualModeWidget->show();
        m_visualModeWidget->raise();
        m_visualModeWidget->activateWindow();
    }
    
    m_statusLabel->setText(tr("Modo Visual %1").arg(m_visualModeWidget->isVisible() ? "activado" : "desactivado"));
}


/* ============================================================================
   EDIT MODE
   ============================================================================ */

void MainWindow::onModeChanged(int index)
{
    m_gridEditor->setEditMode(static_cast<GridEditor::EditMode>(index));
}

void MainWindow::onTextureSelected(int textureId)
{
    m_gridEditor->setSelectedTexture(textureId);
}

/* ============================================================================
   SECTOR EDITING
   ============================================================================ */

void MainWindow::onSectorSelected(int sectorId)
{
    m_selectedSectorId = sectorId;
    updateSectorPanel();
    
    // Sync list selection
    if (m_sectorListWidget) {
        m_sectorListWidget->blockSignals(true); // Prevent loop
        for (int i = 0; i < m_sectorListWidget->count(); i++) {
            QListWidgetItem *item = m_sectorListWidget->item(i);
            if (item->data(Qt::UserRole).toInt() == sectorId) {
                item->setSelected(true);
                m_sectorListWidget->scrollToItem(item);
            } else {
                item->setSelected(false);
            }
        }
        m_sectorListWidget->blockSignals(false);
    }
}

void MainWindow::onVertexSelected(int sectorId, int vertexIndex)
{
    m_selectedSectorId = sectorId;
    // Could add vertex editing panel here in the future
}

void MainWindow::onSectorFloorZChanged(double value)
{
    // Find sector by ID, not by index
    for (Sector &sector : m_mapData.sectors) {
        if (sector.sector_id == m_selectedSectorId) {
            sector.floor_z = value;
            qDebug() << "Updated sector" << m_selectedSectorId << "floor_z to" << value;
            m_gridEditor->update();
            updateVisualMode();
            return;
        }
    }
    qDebug() << "WARNING: Sector" << m_selectedSectorId << "not found for floor_z update";
}

void MainWindow::onSectorCeilingZChanged(double value)
{
    // Find sector by ID, not by index
    for (Sector &sector : m_mapData.sectors) {
        if (sector.sector_id == m_selectedSectorId) {
            sector.ceiling_z = value;
            qDebug() << "Updated sector" << m_selectedSectorId << "ceiling_z to" << value;
            m_gridEditor->update();
            updateVisualMode();
            return;
        }
    }
    qDebug() << "WARNING: Sector" << m_selectedSectorId << "not found for ceiling_z update";
}

void MainWindow::onSectorFloorTextureChanged(int value)
{
    // Find sector by ID, not by index
    for (Sector &sector : m_mapData.sectors) {
        if (sector.sector_id == m_selectedSectorId) {
            sector.floor_texture_id = value;
            qDebug() << "Updated sector" << m_selectedSectorId << "floor_texture_id to" << value;
            m_gridEditor->update();
            updateVisualMode();
            return;
        }
    }
    qDebug() << "WARNING: Sector" << m_selectedSectorId << "not found for floor_texture update";
}

void MainWindow::onSectorCeilingTextureChanged(int value)
{
    // Find sector by ID, not by index
    for (Sector &sector : m_mapData.sectors) {
        if (sector.sector_id == m_selectedSectorId) {
            sector.ceiling_texture_id = value;
            qDebug() << "Updated sector" << m_selectedSectorId << "ceiling_texture_id to" << value;
            m_gridEditor->update();
            updateVisualMode();
            return;
        }
    }
    qDebug() << "WARNING: Sector" << m_selectedSectorId << "not found for ceiling_texture update";
}

void MainWindow::onSelectSectorFloorTexture()
{
    int sectorIndex = -1;
    for (int i = 0; i < m_mapData.sectors.size(); i++) {
        if (m_mapData.sectors[i].sector_id == m_selectedSectorId) {
            sectorIndex = i;
            break;
        }
    }

    if (sectorIndex == -1) {
        QMessageBox::warning(this, "Aviso", "Selecciona un sector primero");
        return;
    }

    TextureSelector selector(m_textureCache, this);
    if (selector.exec() == QDialog::Accepted) {
        int textureId = selector.selectedTextureId();
        
        // DEBUG
        QMessageBox::information(this, "DEBUG", 
            QString("Selector: Aplicando textura SUELO %1 al sector %2").arg(textureId).arg(m_selectedSectorId));
            
        // Direct update
        m_mapData.sectors[sectorIndex].floor_texture_id = textureId;
        m_gridEditor->update();
        updateVisualMode();
        
        // Update UI preventing signal loop
        m_sectorFloorTextureSpin->blockSignals(true);
        m_sectorFloorTextureSpin->setValue(textureId);
        m_sectorFloorTextureSpin->blockSignals(false);
    }
}

void MainWindow::onSelectSectorCeilingTexture()
{
    int sectorIndex = -1;
    for (int i = 0; i < m_mapData.sectors.size(); i++) {
        if (m_mapData.sectors[i].sector_id == m_selectedSectorId) {
            sectorIndex = i;
            break;
        }
    }

    if (sectorIndex == -1) {
        QMessageBox::warning(this, "Aviso", "Selecciona un sector primero");
        return;
    }

    TextureSelector selector(m_textureCache, this);
    if (selector.exec() == QDialog::Accepted) {
        int textureId = selector.selectedTextureId();
        
        // DEBUG
        QMessageBox::information(this, "DEBUG", 
            QString("Selector: Aplicando textura TECHO %1 al sector %2").arg(textureId).arg(m_selectedSectorId));
            
        // Direct update
        m_mapData.sectors[sectorIndex].ceiling_texture_id = textureId;
        m_gridEditor->update();
        updateVisualMode();
        
        // Update UI preventing signal loop
        m_sectorCeilingTextureSpin->blockSignals(true);
        m_sectorCeilingTextureSpin->setValue(textureId);
        m_sectorCeilingTextureSpin->blockSignals(false);
    }
}

/* ============================================================================
   WALL EDITING
   ============================================================================ */

void MainWindow::onWallSelected(int sectorIndex, int wallIndex)
{
    m_selectedSectorId = sectorIndex;
    m_selectedWallId = wallIndex;
    updateWallPanel();
}

void MainWindow::onWallTextureLowerChanged(int value)
{
    if (m_selectedSectorId >= 0 && m_selectedSectorId < m_mapData.sectors.size() &&
        m_selectedWallId >= 0 && m_selectedWallId < m_mapData.sectors[m_selectedSectorId].walls.size()) {
        m_mapData.sectors[m_selectedSectorId].walls[m_selectedWallId].texture_id_lower = value;
        m_gridEditor->update();
        updateVisualMode();
    }
}

void MainWindow::onWallTextureMiddleChanged(int value)
{
    if (m_selectedSectorId >= 0 && m_selectedSectorId < m_mapData.sectors.size() &&
        m_selectedWallId >= 0 && m_selectedWallId < m_mapData.sectors[m_selectedSectorId].walls.size()) {
        m_mapData.sectors[m_selectedSectorId].walls[m_selectedWallId].texture_id_middle = value;
        m_gridEditor->update();
        updateVisualMode();
    }
}

void MainWindow::onWallTextureUpperChanged(int value)
{
    if (m_selectedSectorId >= 0 && m_selectedSectorId < m_mapData.sectors.size() &&
        m_selectedWallId >= 0 && m_selectedWallId < m_mapData.sectors[m_selectedSectorId].walls.size()) {
        m_mapData.sectors[m_selectedSectorId].walls[m_selectedWallId].texture_id_upper = value;
        m_gridEditor->update();
        updateVisualMode();
    }
}

void MainWindow::onSelectWallTextureLower()
{
    TextureSelector selector(m_textureCache, this);
    if (selector.exec() == QDialog::Accepted) {
        int textureId = selector.selectedTextureId();
        m_wallTextureLowerSpin->setValue(textureId);
    }
}

void MainWindow::onSelectWallTextureMiddle()
{
    TextureSelector selector(m_textureCache, this);
    if (selector.exec() == QDialog::Accepted) {
        int textureId = selector.selectedTextureId();
        m_wallTextureMiddleSpin->setValue(textureId);
    }
}

void MainWindow::onSelectWallTextureUpper()
{
    TextureSelector selector(m_textureCache, this);
    if (selector.exec() == QDialog::Accepted) {
        int textureId = selector.selectedTextureId();
        m_wallTextureUpperSpin->setValue(textureId);
    }
}

void MainWindow::onApplyTextureToAllWalls()
{
    if (m_selectedSectorId < 0 || m_selectedSectorId >= m_mapData.sectors.size()) {
        QMessageBox::warning(this, tr("Advertencia"), 
                           tr("Selecciona una pared primero para identificar el sector"));
        return;
    }
    
    // Get current middle texture value
    int textureId = m_wallTextureMiddleSpin->value();
    
    
    // Apply to all walls in the sector
    Sector &sector = m_mapData.sectors[m_selectedSectorId];
    for (Wall &wall : sector.walls) {
        wall.texture_id_middle = textureId;
    }
    
    m_gridEditor->update();
    updateVisualMode();
    
    QMessageBox::information(this, tr("Éxito"), 
                           tr("Textura %1 aplicada a %2 paredes del sector %3")
                           .arg(textureId)
                           .arg(sector.walls.size())
                           .arg(m_selectedSectorId));
}

void MainWindow::onWallSplitLowerChanged(double value)
{
    for (Sector &sector : m_mapData.sectors) {
        for (Wall &wall : sector.walls) {
            if (wall.wall_id == m_selectedWallId) {
                wall.texture_split_z_lower = value;
                m_gridEditor->update();
                updateVisualMode();
                return;
            }
        }
    }
}

void MainWindow::onWallSplitUpperChanged(double value)
{
    for (Sector &sector : m_mapData.sectors) {
        for (Wall &wall : sector.walls) {
            if (wall.wall_id == m_selectedWallId) {
                wall.texture_split_z_upper = value;
                m_gridEditor->update();
                return;
            }
        }
    }
}

/* ============================================================================
   TOOLS
   ============================================================================ */

void MainWindow::onToggleManualPortals(bool checked)
{
    if (checked) {
        m_gridEditor->setEditMode(GridEditor::MODE_MANUAL_PORTAL);
        m_statusLabel->setText(tr("Modo Portal Manual: Selecciona la PRIMERA pared para el portal..."));
        m_pendingPortalSector = -1;
        m_pendingPortalWall = -1;
    } else {
        m_gridEditor->setEditMode(GridEditor::MODE_SELECT_SECTOR); // Default back to generic selection
        m_statusLabel->setText(tr("Modo Portal Manual desactivado"));
        m_pendingPortalSector = -1;
        m_pendingPortalWall = -1;
    }
}

void MainWindow::onManualPortalWallSelected(int sectorIndex, int wallIndex)
{
    if (sectorIndex < 0 || wallIndex < 0) return;
    
    // Check if we are selecting Source or Target
    if (m_pendingPortalSector == -1) {
        // --- STEP 1: SOURCE SELECTION ---
        
        // Validation: Verify wall exists
        if (sectorIndex >= m_mapData.sectors.size()) return;
        Sector &sector = m_mapData.sectors[sectorIndex];
        if (wallIndex >= sector.walls.size()) return;
        
        Wall &wall = sector.walls[wallIndex];
        
        // NOTE: Build Engine allows multiple portals per wall
        // We allow overwriting/adding portals without warning
        /* REMOVED: Single portal per wall restriction
        if (wall.portal_id >= 0) {
            int ret = QMessageBox::question(this, tr("Portal Existente"), 
                tr("Esta pared ya tiene un portal (ID %1). ¿Quieres reemplazarlo?").arg(wall.portal_id),
                QMessageBox::Yes | QMessageBox::No);
            if (ret == QMessageBox::No) return;
        }
        */

        m_pendingPortalSector = sectorIndex;
        m_pendingPortalWall = wallIndex;
        
        // Feedback
        m_statusLabel->setText(tr("Pared ORIGEN seleccionada (Sector %1, Pared %2). Ahora selecciona la pared DESTINO...")
                               .arg(sector.sector_id).arg(wallIndex));
        
        QMessageBox::information(this, tr("Portal Manual"), tr("Origen seleccionado via CLICK.\nAhora haz CLICK en la pared del OTRO sector para unir."));
        
    } else {
        // --- STEP 2: TARGET SELECTION ---
        
        if (sectorIndex == m_pendingPortalSector) {
             QMessageBox::warning(this, tr("Error"), tr("No puedes crear un portal en el mismo sector. Selecciona una pared de OTRO sector."));
             return;
        }
        
        // Create Portal!
        // Get Source Wall
        Sector &secA = m_mapData.sectors[m_pendingPortalSector];
        Wall &wallA = secA.walls[m_pendingPortalWall];
        
        // Get Target Wall
        Sector &secB = m_mapData.sectors[sectorIndex];
        Wall &wallB = secB.walls[wallIndex];
        
        // NOTE: Build Engine allows multiple portals per wall
        /* REMOVED: Single portal per wall restriction
        if (wallB.portal_id >= 0) {
            int ret = QMessageBox::question(this, tr("Portal Existente"), 
                tr("La pared destino ya tiene un portal (ID %1). ¿Quieres reemplazarlo?").arg(wallB.portal_id),
                QMessageBox::Yes | QMessageBox::No);
             if (ret == QMessageBox::No) return;
        }
        */
        
        // Create Portal Data
        Portal portal;
        portal.portal_id = m_mapData.getNextPortalId();
        portal.sector_a = secA.sector_id;
        portal.sector_b = secB.sector_id;
        portal.wall_id_a = wallA.wall_id;
        portal.wall_id_b = wallB.wall_id;
        // Use geometry of Wall A as "primary", but engine uses vertex lists anyway.
        // It's good practice to ensure they match roughly.
        portal.x1 = wallA.x1;
        portal.y1 = wallA.y1;
        portal.x2 = wallA.x2;
        portal.y2 = wallA.y2;
        
        m_mapData.portals.append(portal);
        
        // Assign IDs
        wallA.portal_id = portal.portal_id;
        wallB.portal_id = portal.portal_id;
        
        // Auto-assign Upper/Lower textures from Middle texture
        // This makes the portal "steps" blend in with the wall that was there.
        if (wallA.texture_id_middle > 0) {
            wallA.texture_id_upper = wallA.texture_id_middle;
            wallA.texture_id_lower = wallA.texture_id_middle;
        }
        if (wallB.texture_id_middle > 0) {
            wallB.texture_id_upper = wallB.texture_id_middle;
            wallB.texture_id_lower = wallB.texture_id_middle;
        }
        
        m_gridEditor->update(); // Redraw
        
        QMessageBox::information(this, tr("Portal Creado"), 
            tr("Portal creado correctamente entre Sector %1 y Sector %2.").arg(secA.sector_id).arg(secB.sector_id));
            
        // Reset state loop to allow creating more portals
        m_pendingPortalSector = -1;
        m_pendingPortalWall = -1;
        m_statusLabel->setText(tr("Portal Creado. Selecciona nueva pared ORIGEN o desactiva modo manual."));
    }
}

void MainWindow::onDetectPortals()
{
    if (m_mapData.sectors.size() < 2) {
        QMessageBox::information(this, tr("Portal Detection"),
                               tr("Need at least 2 sectors to detect portals"));
        return;
    }
    
    // Warn user that this might reset MANUAL portals on the same wall?
    // The original logic clears ALL portals: m_mapData.portals.clear();
    // Maybe we should CHANGE this to only ADD portals or ask?
    // User asked "volver a poner... lo de asignar portales automaticamente".
    // I will restore it as it was (destructive clear), but maybe add a warning?
    // Or better: Let's restore it exactly as it was for now, but assume it clears existing portals.
    
    QMessageBox::StandardButton reply = QMessageBox::question(this, tr("Auto-Detect Portals"),
        tr("This will CLEAR all existing portals (including manual ones) and auto-detect them.\nContinue?"),
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    // Clear existing portals
    m_mapData.portals.clear();
    
    // Reset portal IDs in all walls
    for (Sector &sector : m_mapData.sectors) {
        for (Wall &wall : sector.walls) {
            wall.portal_id = -1;
        }
    }
    
    const float EPSILON = 5.0f;  
    int portalsCreated = 0;
    int geometryFixes = 0;
    
    // Auto-fix geometry loop
    bool geologyChanged = true;
    while (geologyChanged) {
        geologyChanged = false;
        if (geometryFixes > 100) break; 
        
        for (int i = 0; i < m_mapData.sectors.size(); i++) {
            Sector &sectorA = m_mapData.sectors[i];
            
            for (int k = 0; k < m_mapData.sectors.size(); k++) {
                if (i == k) continue;
                Sector &sectorB = m_mapData.sectors[k];
                
                for (const QPointF &vB : sectorB.vertices) {
                    for (int wA = 0; wA < sectorA.walls.size(); wA++) {
                        Wall &wallA = sectorA.walls[wA];
                        
                        float dx = wallA.x2 - wallA.x1;
                        float dy = wallA.y2 - wallA.y1;
                        if (dx*dx + dy*dy < 0.1f) continue; 
                        
                        float t = ((vB.x() - wallA.x1) * dx + (vB.y() - wallA.y1) * dy) / (dx*dx + dy*dy);
                        
                        if (t > 0.05f && t < 0.95f) { 
                            float px = wallA.x1 + t * dx;
                            float py = wallA.y1 + t * dy;
                            float distSq = (vB.x()-px)*(vB.x()-px) + (vB.y()-py)*(vB.y()-py);
                            
                            if (distSq < EPSILON * EPSILON) {
                                int insertIdx = (wA + 1) % (sectorA.vertices.size() + 1);
                                if (wA == sectorA.vertices.size() - 1) insertIdx = sectorA.vertices.size();
                                else insertIdx = wA + 1;
                                
                                QPointF splitPoint(px, py); 
                                sectorA.vertices.insert(insertIdx, splitPoint);
                                
                                QVector<Wall> oldWalls = sectorA.walls;
                                sectorA.walls.clear();
                                for (int v = 0; v < sectorA.vertices.size(); v++) {
                                    int next = (v + 1) % sectorA.vertices.size();
                                    Wall newWall;
                                    newWall.wall_id = m_mapData.getNextWallId();
                                    newWall.x1 = sectorA.vertices[v].x();
                                    newWall.y1 = sectorA.vertices[v].y();
                                    newWall.x2 = sectorA.vertices[next].x();
                                    newWall.y2 = sectorA.vertices[next].y();
                                    
                                    int sourceWallIdx = v;
                                    if (v > wA) sourceWallIdx = v - 1;
                                    if (sourceWallIdx < oldWalls.size()) {
                                        const Wall &old = oldWalls[sourceWallIdx];
                                        newWall.texture_id_lower = old.texture_id_lower;
                                        newWall.texture_id_middle = old.texture_id_middle;
                                        newWall.texture_id_upper = old.texture_id_upper;
                                        newWall.texture_split_z_lower = old.texture_split_z_lower;
                                        newWall.texture_split_z_upper = old.texture_split_z_upper;
                                        newWall.flags = old.flags;
                                    }
                                    sectorA.walls.append(newWall);
                                }
                                
                                geometryFixes++;
                                geologyChanged = true;
                                goto restart_loops;
                            }
                        }
                    }
                }
            }
        }
        restart_loops:;
    }

    float minDistanceFound = 99999.0f;
    int closestSectorA = -1;
    int closestSectorB = -1;
    
    for (int i = 0; i < m_mapData.sectors.size(); i++) {
        Sector &sectorA = m_mapData.sectors[i];
        
        for (int j = i + 1; j < m_mapData.sectors.size(); j++) {
            Sector &sectorB = m_mapData.sectors[j];
            
            for (int wallIdxA = 0; wallIdxA < sectorA.walls.size(); wallIdxA++) {
                Wall &wallA = sectorA.walls[wallIdxA];
                if (wallA.portal_id >= 0) continue;
                
                for (int wallIdxB = 0; wallIdxB < sectorB.walls.size(); wallIdxB++) {
                    Wall &wallB = sectorB.walls[wallIdxB];
                    if (wallB.portal_id >= 0) continue;
                    
                    float distNormal = std::max({
                        (float)fabs(wallA.x1 - wallB.x1), (float)fabs(wallA.y1 - wallB.y1),
                        (float)fabs(wallA.x2 - wallB.x2), (float)fabs(wallA.y2 - wallB.y2)
                    });
                    
                    float distReversed = std::max({
                        (float)fabs(wallA.x1 - wallB.x2), (float)fabs(wallA.y1 - wallB.y2),
                        (float)fabs(wallA.x2 - wallB.x1), (float)fabs(wallA.y2 - wallB.y1)
                    });
                    
                    float currentMinDist = std::min(distNormal, distReversed);
                    if (currentMinDist < minDistanceFound) {
                        minDistanceFound = currentMinDist;
                        closestSectorA = sectorA.sector_id;
                        closestSectorB = sectorB.sector_id;
                    }
                    
                    bool normalMatch = (distNormal < EPSILON);
                    bool reversedMatch = (distReversed < EPSILON);
                    
                    if (normalMatch || reversedMatch) {
                        Portal portal;
                        portal.portal_id = m_mapData.getNextPortalId();
                        portal.sector_a = sectorA.sector_id;
                        portal.sector_b = sectorB.sector_id;
                        portal.wall_id_a = wallA.wall_id;
                        portal.wall_id_b = wallB.wall_id;
                        portal.x1 = wallA.x1;
                        portal.y1 = wallA.y1;
                        portal.x2 = wallA.x2;
                        portal.y2 = wallA.y2;
                        
                        m_mapData.portals.append(portal);
                        
                        wallA.portal_id = portal.portal_id;
                        wallB.portal_id = portal.portal_id;
                        
                        portalsCreated++;
                        break;
                    }
                }
            }
        }
    }
    
    
    m_gridEditor->update();
    updateVisualMode();
    
    QString resultMsg;
    if (geometryFixes > 0) {
        resultMsg += tr("Auto-fixed %1 geometry mismatch(es) (T-junctions).\n").arg(geometryFixes);
    }
    
    if (portalsCreated > 0) {
        resultMsg += tr("Portal detection completed. %1 portal(s) created.").arg(portalsCreated);
        QMessageBox::information(this, tr("Portal Detection"), resultMsg);
    } else {
        QString details = tr("No portals detected.");
        if (minDistanceFound < 1000.0f) {
            details += tr("\n\nClosest match found:\nDistance: %1\n").arg(minDistanceFound);
        }
        QMessageBox::warning(this, tr("Portal Detection Failed"), resultMsg + "\n" + details);
    }
}



void MainWindow::onDeletePortal(int sectorIndex, int wallIndex)
{
    if (sectorIndex < 0 || sectorIndex >= m_mapData.sectors.size()) return;
    Sector &sector = m_mapData.sectors[sectorIndex];
    if (wallIndex < 0 || wallIndex >= sector.walls.size()) return;
    
    Wall &wall = sector.walls[wallIndex];
    int portalId = wall.portal_id;
    
    if (portalId < 0) return;
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Eliminar Portal"),
        tr("¿Estás seguro de que deseas eliminar el portal %1?").arg(portalId),
        QMessageBox::Yes | QMessageBox::No);
        
    if (reply != QMessageBox::Yes) return;
    
    // Remove portal from list
    bool removed = false;
    for (int i = 0; i < m_mapData.portals.size(); i++) {
        if (m_mapData.portals[i].portal_id == portalId) {
            m_mapData.portals.removeAt(i);
            removed = true;
            break;
        }
    }
    
    // Reset walls referencing this portal
    int wallsUpdated = 0;
    for (Sector &s : m_mapData.sectors) {
        for (Wall &w : s.walls) {
            if (w.portal_id == portalId) {
                w.portal_id = -1;
                wallsUpdated++;
            }
        }
    }
    
    m_gridEditor->update();
    updateVisualMode();
    m_statusLabel->setText(tr("Portal %1 eliminado (referencias limpiadas: %2)").arg(portalId).arg(wallsUpdated));
}

/* ============================================================================
   HELPERS
   ============================================================================ */

void MainWindow::updateWindowTitle()
{
    QString title = "RayMap Editor - Geometric Sectors";
    if (!m_currentMapFile.isEmpty()) {
        title += " - " + QFileInfo(m_currentMapFile).fileName();
    }
    setWindowTitle(title);
}

void MainWindow::updateSectorPanel()
{
    int sectorIndex = -1;
    for (int i = 0; i < m_mapData.sectors.size(); i++) {
        if (m_mapData.sectors[i].sector_id == m_selectedSectorId) {
            sectorIndex = i;
            break;
        }
    }

    if (sectorIndex != -1) {
        const Sector &sector = m_mapData.sectors[sectorIndex];
        m_sectorIdLabel->setText(tr("Sector %1").arg(sector.sector_id));
        
        m_sectorFloorZSpin->blockSignals(true);
        m_sectorCeilingZSpin->blockSignals(true);
        m_sectorFloorTextureSpin->blockSignals(true);
        m_sectorCeilingTextureSpin->blockSignals(true);
        
        m_sectorFloorZSpin->setValue(sector.floor_z);
        m_sectorCeilingZSpin->setValue(sector.ceiling_z);
        m_sectorFloorTextureSpin->setValue(sector.floor_texture_id);
        m_sectorCeilingTextureSpin->setValue(sector.ceiling_texture_id);
        
        m_sectorFloorZSpin->blockSignals(false);
        m_sectorCeilingZSpin->blockSignals(false);
        m_sectorFloorTextureSpin->blockSignals(false);
        m_sectorCeilingTextureSpin->blockSignals(false);
        
        // Removed nested sector controls update (v9 format is flat)
        
    } else {
        m_sectorIdLabel->setText(tr("No sector selected"));
    }
}

void MainWindow::updateWallPanel()
{
    if (m_selectedSectorId >= 0 && m_selectedSectorId < m_mapData.sectors.size() &&
        m_selectedWallId >= 0 && m_selectedWallId < m_mapData.sectors[m_selectedSectorId].walls.size()) {
        
        const Wall &wall = m_mapData.sectors[m_selectedSectorId].walls[m_selectedWallId];
        // Show Wall Index (0..N) instead of internal wall_id which might be uninitialized/duplicate
        m_wallIdLabel->setText(tr("Wall %1").arg(m_selectedWallId));
        m_wallTextureLowerSpin->setValue(wall.texture_id_lower);
        m_wallTextureMiddleSpin->setValue(wall.texture_id_middle);
        m_wallTextureUpperSpin->setValue(wall.texture_id_upper);
        m_wallSplitLowerSpin->setValue(wall.texture_split_z_lower);
        m_wallSplitUpperSpin->setValue(wall.texture_split_z_upper);
        
        // Portal UI
        if (wall.portal_id >= 0) {
            m_portalTexGroup->setVisible(true);
            m_portalTexGroup->setTitle(tr("Propiedades de Portal (ID %1)").arg(wall.portal_id));
            m_portalUpperSpin->blockSignals(true);
            m_portalLowerSpin->blockSignals(true);
            m_portalUpperSpin->setValue(wall.texture_id_upper);
            m_portalLowerSpin->setValue(wall.texture_id_lower);
            m_portalUpperSpin->blockSignals(false);
            m_portalLowerSpin->blockSignals(false);
        } else {
            m_portalTexGroup->setVisible(false);
        }
        
    } else {
        m_wallIdLabel->setText(tr("No wall selected"));
        m_portalTexGroup->setVisible(false);
    }
}

/* ============================================================================
   NESTED SECTOR SLOTS
   ============================================================================ */



/* ============================================================================
   SECTOR LIST
   ============================================================================ */

void MainWindow::updateSectorList()
{
    if (!m_sectorListWidget) return;
    
    m_sectorListWidget->clear();
    
    for (const Sector &sector : m_mapData.sectors) {
        QString itemText;
        
        // Indent based on nesting level - REMOVED (v9 is flat)
        // for (int i = 0; i < sector.nesting_level; i++) {
        //     itemText += "  ";
        // }
        
        itemText += QString("Sector %1").arg(sector.sector_id);
        
        // Removed sector type suffix (v9 is flat)
        
        QListWidgetItem *item = new QListWidgetItem(itemText);
        item->setData(Qt::UserRole, sector.sector_id);
        m_sectorListWidget->addItem(item);
    }
}

void MainWindow::onSectorListItemClicked(QListWidgetItem *item)
{
    if (!item) return;
    
    int sectorId = item->data(Qt::UserRole).toInt();
    
    for (int i = 0; i < m_mapData.sectors.size(); i++) {
        if (m_mapData.sectors[i].sector_id == sectorId) {
            m_selectedSectorId = sectorId;
            m_gridEditor->setSelectedSector(i);
            updateSectorPanel();
            break;
        }
    }
}

/* ============================================================================
   SECTOR CONTEXT MENU & OPERATIONS
   ============================================================================ */

void MainWindow::onSectorListContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_sectorListWidget->itemAt(pos);
    if (!item) return;
    
    int sectorId = item->data(Qt::UserRole).toInt();
    m_selectedSectorId = sectorId; // Ensure selection matches click
    
    QMenu menu(this);
    
    QAction *moveAction = menu.addAction(tr("Mover Sector..."));
    connect(moveAction, &QAction::triggered, this, &MainWindow::moveSelectedSector);
    
    menu.addSeparator();
    
    QAction *copyAction = menu.addAction(tr("Copiar"));
    connect(copyAction, &QAction::triggered, this, &MainWindow::copySelectedSector);
    
    QAction *pasteAction = menu.addAction(tr("Pegar"));
    pasteAction->setEnabled(m_hasClipboard);
    connect(pasteAction, &QAction::triggered, this, &MainWindow::pasteSector);
    
    menu.addSeparator();
    
    QAction *deleteAction = menu.addAction(tr("Eliminar"));
    connect(deleteAction, &QAction::triggered, this, &MainWindow::deleteSelectedSector);
    
    menu.exec(m_sectorListWidget->mapToGlobal(pos));
}

void MainWindow::deleteSelectedSector()
{
    if (m_selectedSectorId < 0) return;
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Eliminar Sector"),
        tr("¿Estás seguro de que deseas eliminar el sector %1?").arg(m_selectedSectorId),
        QMessageBox::Yes | QMessageBox::No);
        
    if (reply != QMessageBox::Yes) return;
    
    // Find sector index
    int index = -1;
    for (int i = 0; i < m_mapData.sectors.size(); i++) {
        if (m_mapData.sectors[i].sector_id == m_selectedSectorId) {
            index = i;
            break;
        }
    }
    
    if (index >= 0) {
        m_mapData.sectors.removeAt(index);
        m_selectedSectorId = -1;
        updateSectorList();
        m_gridEditor->update();
        updateVisualMode();
        m_statusLabel->setText(tr("Sector eliminado"));
    }
}

void MainWindow::copySelectedSector()
{
    if (m_selectedSectorId < 0) return;
    
    // Find sector
    for (const Sector &sec : m_mapData.sectors) {
        if (sec.sector_id == m_selectedSectorId) {
            m_clipboardSector = sec;
            m_hasClipboard = true;
            m_statusLabel->setText(tr("Sector %1 copiado al portapapeles").arg(sec.sector_id));
            return;
        }
    }
}

void MainWindow::pasteSector()
{
    if (!m_hasClipboard) return;
    
    // Create new sector from clipboard
    Sector newSector = m_clipboardSector;
    
    // Assign new unique ID
    int infoMaxId = 0;
    for (const Sector &s : m_mapData.sectors) {
        if (s.sector_id > infoMaxId) infoMaxId = s.sector_id;
    }
    newSector.sector_id = infoMaxId + 1;
    
    // Reset geometry/topology relations to avoid conflicts
    newSector.portal_ids.clear(); // Remove existing portals
    
    // Apply offset to geometry so it's visible (user can move it later)
    float offsetX = 64.0f;
    float offsetY = 64.0f;
    
    for (int i = 0; i < newSector.vertices.size(); i++) {
        newSector.vertices[i].rx() += offsetX;
        newSector.vertices[i].ry() += offsetY;
    }
    
    // Update walls: reset portals and update coordinates
    // Note: Walls storing coordinates is redundant if vertices exist, but MapData struct has both.
    // We must update wall coordinates to match vertices.
    for (int i = 0; i < newSector.walls.size(); i++) {
        Wall &w = newSector.walls[i];
        w.portal_id = -1; // Reset portal connection
        w.x1 += offsetX; 
        w.y1 += offsetY;
        w.x2 += offsetX;
        w.y2 += offsetY;
    }
    
    // Add to map
    m_mapData.sectors.append(newSector);
    
    updateSectorList();
    m_gridEditor->update();
    updateVisualMode();
    m_statusLabel->setText(tr("Sector pegado como ID %1").arg(newSector.sector_id));
    
    // Select the new sector
    m_selectedSectorId = newSector.sector_id;
    // Highlight in list?
}

void MainWindow::moveSelectedSector()
{
    if (m_selectedSectorId < 0) return;
    
    // Create dialog
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Mover Sector"));
    
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    
    QHBoxLayout *xLayout = new QHBoxLayout();
    xLayout->addWidget(new QLabel("Delta X:"));
    QDoubleSpinBox *xSpin = new QDoubleSpinBox();
    xSpin->setRange(-10000, 10000);
    xSpin->setValue(0);
    xLayout->addWidget(xSpin);
    layout->addLayout(xLayout);
    
    QHBoxLayout *yLayout = new QHBoxLayout();
    yLayout->addWidget(new QLabel("Delta Y:"));
    QDoubleSpinBox *ySpin = new QDoubleSpinBox();
    ySpin->setRange(-10000, 10000);
    ySpin->setValue(0);
    yLayout->addWidget(ySpin);
    layout->addLayout(yLayout);
    
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    
    if (dialog.exec() == QDialog::Accepted) {
        float dx = xSpin->value();
        float dy = ySpin->value();
        
        if (dx == 0 && dy == 0) return;
        
        // Find and update sector
        for (int i = 0; i < m_mapData.sectors.size(); i++) {
            if (m_mapData.sectors[i].sector_id == m_selectedSectorId) {
                Sector &sec = m_mapData.sectors[i];
                
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
                    
                    // Invalidate portals because geometry changed position
                    // Ideally we should check if it still aligns, but safest is to break links
                    sec.walls[w].portal_id = -1;
                }
                sec.portal_ids.clear(); 
                
                m_gridEditor->update();
                updateVisualMode();
                m_statusLabel->setText(tr("Sector movido"));
                break;
            }
        }
    }
}

void MainWindow::onCreateRectangle()
{
    // Ask for size
    bool ok;
    int size = QInputDialog::getInt(this, tr("Crear Rectángulo"),
                                    tr("Tamaño del rectángulo (ancho y alto):"),
                                    512, 64, 4096, 64, &ok);
    if (!ok) return;
    
    // Create rectangle centered at origin
    Sector newSector;
    newSector.sector_id = m_mapData.getNextSectorId();
    
    float half = size / 2.0f;
    newSector.vertices.append(QPointF(-half, -half));  // Bottom-left
    newSector.vertices.append(QPointF(half, -half));   // Bottom-right
    newSector.vertices.append(QPointF(half, half));    // Top-right
    newSector.vertices.append(QPointF(-half, half));   // Top-left
    
    newSector.floor_z = 0.0f;
    newSector.ceiling_z = 256.0f;
    newSector.floor_texture_id = m_selectedTextureSpin->value();
    newSector.ceiling_texture_id = m_selectedTextureSpin->value();
    newSector.light_level = 255;
    
    // Create walls
    for (int i = 0; i < 4; i++) {
        int next = (i + 1) % 4;
        Wall wall;
        wall.wall_id = m_mapData.getNextWallId();
        wall.x1 = newSector.vertices[i].x();
        wall.y1 = newSector.vertices[i].y();
        wall.x2 = newSector.vertices[next].x();
        wall.y2 = newSector.vertices[next].y();
        wall.texture_id_middle = m_selectedTextureSpin->value();
        wall.texture_split_z_lower = 64.0f;
        wall.texture_split_z_upper = 192.0f;
        wall.portal_id = -1;
        newSector.walls.append(wall);
    }
    
    m_mapData.sectors.append(newSector);
    updateSectorList();
    m_gridEditor->update();
    updateVisualMode();
    
    m_statusLabel->setText(tr("Rectángulo %1x%1 creado (Sector %2)")
                          .arg(size).arg(newSector.sector_id));
}

void MainWindow::onCreateCircle()
{
    // Ask for radius
    bool ok;
    int radius = QInputDialog::getInt(this, tr("Crear Círculo"),
                                      tr("Radio del círculo:"),
                                      256, 32, 2048, 32, &ok);
    if (!ok) return;
    
    // Create circle with 16 segments
    int segments = 16;
    Sector newSector;
    newSector.sector_id = m_mapData.getNextSectorId();
    
    for (int i = 0; i < segments; i++) {
        float angle = (float)i / segments * 2.0f * M_PI;
        float x = radius * cosf(angle);
        float y = radius * sinf(angle);
        newSector.vertices.append(QPointF(x, y));
    }
    
    newSector.floor_z = 0.0f;
    newSector.ceiling_z = 256.0f;
    newSector.floor_texture_id = m_selectedTextureSpin->value();
    newSector.ceiling_texture_id = m_selectedTextureSpin->value();
    newSector.light_level = 255;
    
    // Create walls
    for (int i = 0; i < segments; i++) {
        int next = (i + 1) % segments;
        Wall wall;
        wall.wall_id = m_mapData.getNextWallId();
        wall.x1 = newSector.vertices[i].x();
        wall.y1 = newSector.vertices[i].y();
        wall.x2 = newSector.vertices[next].x();
        wall.y2 = newSector.vertices[next].y();
        wall.texture_id_middle = m_selectedTextureSpin->value();
        wall.texture_split_z_lower = 64.0f;
        wall.texture_split_z_upper = 192.0f;
        wall.portal_id = -1;
        newSector.walls.append(wall);
    }
    
    m_mapData.sectors.append(newSector);
    updateSectorList();
    m_gridEditor->update();
    updateVisualMode();
    
    m_statusLabel->setText(tr("Círculo de radio %1 creado (Sector %2)")
                          .arg(radius).arg(newSector.sector_id));
}

/* ============================================================================
   INSERT TOOLS (HIGH-LEVEL GEOMETRY CREATION)
   ============================================================================ */

void MainWindow::onInsertBox()
{
    // Show configuration dialog with texture previews
    InsertBoxDialog dialog(m_textureCache, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    
    // Get configuration
    float width = dialog.getWidth();
    float height = dialog.getHeight();
    float floorZ = dialog.getFloorZ();
    float ceilingZ = dialog.getCeilingZ();
    int wallTexture = dialog.getWallTexture();
    int floorTexture = dialog.getFloorTexture();
    int ceilingTexture = dialog.getCeilingTexture();
    
    // Ask user to click where to place the box
    QMessageBox::information(this, tr("Colocar Caja"),
        tr("Haz clic en el mapa donde quieres colocar el centro de la caja."));
    
    // TODO: Implement click-to-place workflow
    // For now, place at origin as a test
    float centerX = 0.0f;
    float centerY = 0.0f;
    
    // Create new sector
    Sector newSector;
    newSector.sector_id = m_mapData.getNextSectorId();
    newSector.floor_z = floorZ;
    newSector.ceiling_z = ceilingZ;
    newSector.floor_texture_id = floorTexture;
    newSector.ceiling_texture_id = ceilingTexture;
    
    // Create 4 walls (rectangle)
    float hw = width / 2.0f;   // half width
    float hh = height / 2.0f;  // half height
    
    // Wall 0: Bottom (left to right)
    Wall wall0;
    wall0.wall_id = 0;
    wall0.x1 = centerX - hw;
    wall0.y1 = centerY - hh;
    wall0.x2 = centerX + hw;
    wall0.y2 = centerY - hh;
    wall0.texture_id_lower = wallTexture;
    wall0.texture_id_middle = wallTexture;
    wall0.texture_id_upper = wallTexture;
    wall0.portal_id = -1;
    newSector.walls.append(wall0);
    
    // Wall 1: Right (bottom to top)
    Wall wall1;
    wall1.wall_id = 1;
    wall1.x1 = centerX + hw;
    wall1.y1 = centerY - hh;
    wall1.x2 = centerX + hw;
    wall1.y2 = centerY + hh;
    wall1.texture_id_lower = wallTexture;
    wall1.texture_id_middle = wallTexture;
    wall1.texture_id_upper = wallTexture;
    wall1.portal_id = -1;
    newSector.walls.append(wall1);
    
    // Wall 2: Top (right to left)
    Wall wall2;
    wall2.wall_id = 2;
    wall2.x1 = centerX + hw;
    wall2.y1 = centerY + hh;
    wall2.x2 = centerX - hw;
    wall2.y2 = centerY + hh;
    wall2.texture_id_lower = wallTexture;
    wall2.texture_id_middle = wallTexture;
    wall2.texture_id_upper = wallTexture;
    wall2.portal_id = -1;
    newSector.walls.append(wall2);
    
    // Wall 3: Left (top to bottom)
    Wall wall3;
    wall3.wall_id = 3;
    wall3.x1 = centerX - hw;
    wall3.y1 = centerY + hh;
    wall3.x2 = centerX - hw;
    wall3.y2 = centerY - hh;
    wall3.texture_id_lower = wallTexture;
    wall3.texture_id_middle = wallTexture;
    wall3.texture_id_upper = wallTexture;
    wall3.portal_id = -1;
    newSector.walls.append(wall3);
    
    // Create vertices for the sector (needed for moving/editing)
    newSector.vertices.append(QPointF(centerX - hw, centerY - hh));  // Bottom-left
    newSector.vertices.append(QPointF(centerX + hw, centerY - hh));  // Bottom-right
    newSector.vertices.append(QPointF(centerX + hw, centerY + hh));  // Top-right
    newSector.vertices.append(QPointF(centerX - hw, centerY + hh));  // Top-left
    
    // Add sector to map
    m_mapData.sectors.append(newSector);
    int newSectorIndex = m_mapData.sectors.size() - 1;
    
    // Auto-detect parent sector (sector containing the box center)
    int parentSectorIndex = -1;
    for (int i = 0; i < newSectorIndex; i++) {
        const Sector &sector = m_mapData.sectors[i];
        
        // Point-in-polygon test
        bool inside = false;
        int j = sector.vertices.size() - 1;
        for (int k = 0; k < sector.vertices.size(); j = k++) {
            const QPointF &vj = sector.vertices[j];
            const QPointF &vk = sector.vertices[k];
            
            if (((vk.y() > centerY) != (vj.y() > centerY)) &&
                (centerX < (vj.x() - vk.x()) * (centerY - vk.y()) / (vj.y() - vk.y()) + vk.x())) {
                inside = !inside;
            }
        }
        
        if (inside) {
            parentSectorIndex = i;
            break;
        }
    }
    
    // Assign parent-child relationship
    if (parentSectorIndex >= 0) {
        m_mapData.sectors[newSectorIndex].parent_sector_id = m_mapData.sectors[parentSectorIndex].sector_id;
        
        // Add this sector as child of parent
        m_mapData.sectors[parentSectorIndex].child_sector_ids.append(newSector.sector_id);
        
        m_statusLabel->setText(tr("Caja creada (Sector %1) como hijo del Sector %2")
                              .arg(newSector.sector_id)
                              .arg(m_mapData.sectors[parentSectorIndex].sector_id));
    } else {
        m_mapData.sectors[newSectorIndex].parent_sector_id = -1;  // Root sector
        m_statusLabel->setText(tr("Caja creada (Sector %1) como sector raíz")
                              .arg(newSector.sector_id));
    }
    
    // NOTE: We do NOT create portals here
    // The motor will automatically detect this as a nested sector using AABB checks
    // in ray_detect_nested_sectors() and create portals if needed
    
    // Update UI
    updateSectorList();
    m_gridEditor->update();
    updateVisualMode();
    
    m_statusLabel->setText(tr("Caja creada (Sector %1) en (%2, %3)")
                          .arg(newSector.sector_id)
                          .arg(centerX)
                          .arg(centerY));
    
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Caja Creada"));
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setText(tr("Caja creada correctamente (Sector %1).").arg(newSector.sector_id));
    msgBox.setInformativeText(tr(
        "Para que la caja se renderice en el motor, necesitas crear un portal:\n\n"
        "1. Activa el modo 'Portal Manual' en la barra de herramientas\n"
        "2. Haz clic en una pared de la habitación\n"
        "3. Haz clic en una pared de la caja\n"
        "4. El portal se creará automáticamente\n\n"
        "¿Quieres activar el modo Portal Manual ahora?"));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::Yes);
    
    if (msgBox.exec() == QMessageBox::Yes) {
        // Enable manual portal mode
        m_manualPortalsButton->setChecked(true);
        onToggleManualPortals(true);
    }
}

void MainWindow::onInsertColumn()
{
    QMessageBox::information(this, tr("Insertar Columna"),
        tr("Función 'Insertar Columna' en desarrollo.\n\n"
           "Similar a 'Insertar Caja' pero con tamaño más pequeño\n"
           "para pilares y soportes."));
}

void MainWindow::onInsertPlatform()
{
    QMessageBox::information(this, tr("Insertar Plataforma"),
        tr("Función 'Insertar Plataforma' en desarrollo.\n\n"
           "Esta herramienta creará:\n"
           "• Un sector con suelo elevado\n"
           "• Portales al sector padre\n"
           "• Altura configurable"));
}

void MainWindow::onInsertDoor()
{
    // Future implementation
}

void MainWindow::onInsertElevator()
{
    // Future implementation
}

void MainWindow::onInsertStairs()
{
    QMessageBox::information(this, tr("Insertar Escaleras"), 
                           tr("Esta función estará disponible próximamente."));
}

void MainWindow::onSetParentSector()
{
    if (m_sectorListWidget->selectedItems().isEmpty()) {
        QMessageBox::warning(this, tr("Asignar Sector Padre"),
                           tr("Por favor, selecciona un sector primero."));
        return;
    }
    
    // Use selectedItems()[0] instead of currentItem() because currentItem() can be NULL
    QListWidgetItem *selectedItem = m_sectorListWidget->selectedItems()[0];
    int selectedSectorId = selectedItem->data(Qt::UserRole).toInt();
    
    int selectedIndex = -1;
    for (int i = 0; i < m_mapData.sectors.size(); i++) {
        if (m_mapData.sectors[i].sector_id == selectedSectorId) {
            selectedIndex = i;
            break;
        }
    }
    
    if (selectedIndex < 0) {
        return;
    }
    
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Asignar Sector Padre"));
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    
    layout->addWidget(new QLabel(tr("Selecciona el sector padre para el Sector %1:").arg(selectedSectorId)));
    
    QListWidget *parentList = new QListWidget();
    QListWidgetItem *noneItem = new QListWidgetItem(tr("(Ninguno - Sector Raíz)"));
    noneItem->setData(Qt::UserRole, -1);
    parentList->addItem(noneItem);
    
    for (int i = 0; i < m_mapData.sectors.size(); i++) {
        if (i == selectedIndex) continue;
        const Sector &sector = m_mapData.sectors[i];
        QListWidgetItem *item = new QListWidgetItem(tr("Sector %1").arg(sector.sector_id));
        item->setData(Qt::UserRole, sector.sector_id);
        parentList->addItem(item);
    }
    layout->addWidget(parentList);
    
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);
    
    if (dialog.exec() == QDialog::Accepted && parentList->currentItem()) {
        int newParentId = parentList->currentItem()->data(Qt::UserRole).toInt();
        int oldParentId = m_mapData.sectors[selectedIndex].parent_sector_id;
        
        // Remove from old parent's children list
        if (oldParentId >= 0) {
            bool foundOldParent = false;
            for (int i = 0; i < m_mapData.sectors.size(); i++) {
                if (m_mapData.sectors[i].sector_id == oldParentId) {
                    m_mapData.sectors[i].child_sector_ids.removeAll(selectedSectorId);
                    foundOldParent = true;
                    break;
                }
            }
            if (!foundOldParent) {
                qWarning() << "Warning: Old parent sector" << oldParentId << "not found!";
            }
        }
        
        m_mapData.sectors[selectedIndex].parent_sector_id = newParentId;
        
        // Add to new parent's children list
        if (newParentId >= 0) {
            bool foundNewParent = false;
            for (int i = 0; i < m_mapData.sectors.size(); i++) {
                if (m_mapData.sectors[i].sector_id == newParentId) {
                    if (!m_mapData.sectors[i].child_sector_ids.contains(selectedSectorId)) {
                        m_mapData.sectors[i].child_sector_ids.append(selectedSectorId);
                    }
                    foundNewParent = true;
                    break;
                }
            }
            if (!foundNewParent) {
                QMessageBox::critical(this, tr("Error"),
                                    tr("No se pudo encontrar el sector padre %1!").arg(newParentId));
                // Revert the change
                m_mapData.sectors[selectedIndex].parent_sector_id = oldParentId;
                return;
            }
        }
        
        updateSectorList();
        m_statusLabel->setText(tr("Sector %1: Padre = %2")
                              .arg(selectedSectorId)
                              .arg(newParentId >= 0 ? QString::number(newParentId) : tr("Ninguno")));
    }
}


/* ============================================================================
   PORTAL TEXTURE SLOTS (User Request)
   ============================================================================ */

void MainWindow::onPortalUpperChanged(int val)
{
    // Update data directly and sync the other spinbox
    if (m_selectedSectorId >= 0 && m_selectedSectorId < m_mapData.sectors.size() &&
        m_selectedWallId >= 0 && m_selectedWallId < m_mapData.sectors[m_selectedSectorId].walls.size()) {
        
        m_mapData.sectors[m_selectedSectorId].walls[m_selectedWallId].texture_id_upper = val;
        
        // Sync the standard upper spinbox
        m_wallTextureUpperSpin->blockSignals(true);
        m_wallTextureUpperSpin->setValue(val);
        m_wallTextureUpperSpin->blockSignals(false);
        
        m_gridEditor->update();
        updateVisualMode();
    }
}

void MainWindow::onPortalLowerChanged(int val)
{
    if (m_selectedSectorId >= 0 && m_selectedSectorId < m_mapData.sectors.size() &&
        m_selectedWallId >= 0 && m_selectedWallId < m_mapData.sectors[m_selectedSectorId].walls.size()) {
        
        m_mapData.sectors[m_selectedSectorId].walls[m_selectedWallId].texture_id_lower = val;
        
        // Sync the standard lower spinbox
        m_wallTextureLowerSpin->blockSignals(true);
        m_wallTextureLowerSpin->setValue(val);
        m_wallTextureLowerSpin->blockSignals(false);
        
        m_gridEditor->update();
        updateVisualMode();
    }
}

void MainWindow::onSelectPortalUpper()
{
    TextureSelector selector(m_textureCache, this);
    if (selector.exec() == QDialog::Accepted) {
        int textureId = selector.selectedTextureId();
        m_portalUpperSpin->setValue(textureId); // Triggers onPortalUpperChanged
    }
}

void MainWindow::onSelectPortalLower()
{
    TextureSelector selector(m_textureCache, this);
    if (selector.exec() == QDialog::Accepted) {
        int textureId = selector.selectedTextureId();
        m_portalLowerSpin->setValue(textureId); // Triggers onPortalLowerChanged
    }
}

void MainWindow::onSkyboxTextureChanged(int value)
{
    m_mapData.skyTextureID = value;
    updateVisualMode();
}

void MainWindow::updateVisualMode()
{
    if (m_visualModeWidget && m_visualModeWidget->isVisible()) {
        m_visualModeWidget->setMapData(m_mapData, false); // false = Don't reset camera
    }
}
