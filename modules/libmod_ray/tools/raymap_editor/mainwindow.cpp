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
#include "fpgeditor.h"
#include "effectgeneratordialog.h"
#include "camerapatheditor.h"
#include "meshgeneratordialog.h"
#include "textureatlasgen.h"
#include "md3generator.h"
#include "fpgeditor.h"
#include "effectgeneratordialog.h"
#include "camerapatheditor.h"
#include "meshgeneratordialog.h"
#include "textureatlasgen.h"
#include "md3generator.h"
#include "assetbrowser.h" 
#include "buildmanager.h"
#include "projectmanager.h"
#include <QToolBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QSettings>
#include <QFileInfo>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
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
#include "objimportdialog.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_currentFPG(0)
    , m_selectedSectorId(-1)
    , m_selectedWallId(-1)
    , m_selectedDecalId(-1)
    , m_visualModeWidget(nullptr)
    , m_fpgEditor(nullptr)
    , m_buildManager(nullptr)
    , m_consoleWidget(nullptr)
    , m_projectManager(nullptr)
    , m_assetBrowser(nullptr)
    , m_codeEditorDialog(nullptr)
{
    m_projectManager = new ProjectManager(this); // Initialize ProjectManager
    
    setWindowTitle("RayMap Editor - Geometric Sectors");
    resize(1280, 800);
    
    // Create grid editor
    // START TABBED INTERFACE SETUP
    // Create TabWidget as central widget
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabsClosable(true);
    m_tabWidget->setMovable(true);
    setCentralWidget(m_tabWidget);
    
    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::onTabCloseRequested);
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);

    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::onTabCloseRequested);
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);

    // Initial Map (Empty) - MOVED TO END
    // END TABBED INTERFACE SETUP
    
    // Connect signals
    /* Signals connected in onNewMap/openMap etc */
    
    // Create UI
    createActions();
    createMenus();
    createToolbars();
    createDockWindows();
    createStatusBar();
    
    updateWindowTitle();
    
    createStatusBar();
    
    updateWindowTitle();
    
    updateWindowTitle();
    
    // Initialize Build System (m_buildManager, m_consoleWidget)
    setupBuildSystem();
    
    // Initial Map (Empty) - Call after UI init to prevent crashes
    onNewMap();

    // Load settings (Dark mode, window geometry) AFTER creating all UI
    loadSettings();
}

MainWindow::~MainWindow()
{
    saveSettings();
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
    
    m_viewGridAction = new QAction(tr("Ver Cuadrícula"), this);
    m_viewGridAction->setCheckable(true);
    m_viewGridAction->setChecked(true);
    connect(m_viewGridAction, &QAction::toggled, this, [this](bool checked) {
        GridEditor *editor = getCurrentEditor();
        if (editor) editor->showGrid(checked);
    });
    
    m_visualModeAction = new QAction(tr("Modo &Visual"), this);
    m_visualModeAction->setShortcut(QKeySequence(tr("F3")));
    connect(m_visualModeAction, &QAction::triggered, this, &MainWindow::onToggleVisualMode);
}

void MainWindow::createMenus()
{
    // === PROJECT MENU ===
    QMenu *projectMenu = menuBar()->addMenu(tr("&Proyecto"));
    
    QAction *newProjectAction = new QAction(tr("Nuevo Proyecto..."), this);
    connect(newProjectAction, &QAction::triggered, this, &MainWindow::onNewProject);
    projectMenu->addAction(newProjectAction);
    
    QAction *openProjectAction = new QAction(tr("Abrir Proyecto..."), this);
    connect(openProjectAction, &QAction::triggered, this, &MainWindow::onOpenProject);
    projectMenu->addAction(openProjectAction);
    
    QAction *closeProjectAction = new QAction(tr("Cerrar Proyecto"), this);
    connect(closeProjectAction, &QAction::triggered, this, &MainWindow::onCloseProject);
    projectMenu->addAction(closeProjectAction);
    
    projectMenu->addSeparator();
    
    QAction *projectSettingsAction = new QAction(tr("Configuración..."), this);
    connect(projectSettingsAction, &QAction::triggered, this, &MainWindow::onProjectSettings);
    projectMenu->addAction(projectSettingsAction);
    
    projectMenu->addSeparator();
    
    QAction *publishAction = new QAction(tr("Publicar..."), this);
    publishAction->setToolTip(tr("Publicar proyecto para distribución"));
    connect(publishAction, &QAction::triggered, this, &MainWindow::onPublishProject);
    projectMenu->addAction(publishAction);

    // === FILE MENU ===
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
    viewMenu->addAction(m_viewGridAction);
    viewMenu->addSeparator();
    
    // Dark Mode Toggle
    QAction *darkModeAction = new QAction(tr("Modo &Oscuro"), this);
    darkModeAction->setCheckable(true);
    darkModeAction->setChecked(true); // Default
    connect(darkModeAction, &QAction::toggled, this, &MainWindow::onToggleDarkMode);
    // Connect settings load to update this checkbox state? 
    // Ideally loadSettings would update this action, but for now let's just add it.
    viewMenu->addAction(darkModeAction);
    
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
    
    // Tools Menu
    QMenu *toolsMenu = menuBar()->addMenu(tr("&Tools"));
    QAction *fpgEditorAction = new QAction(tr("FPG Editor..."), this);
    fpgEditorAction->setShortcut(QKeySequence(tr("Ctrl+F")));
    fpgEditorAction->setStatusTip(tr("Open FPG texture editor"));
    connect(fpgEditorAction, &QAction::triggered, this, &MainWindow::onOpenFPGEditor);
    toolsMenu->addAction(fpgEditorAction);
    
    QAction *effectGenAction = new QAction(tr("Generador de Efectos..."), this);
    effectGenAction->setShortcut(QKeySequence(tr("Ctrl+E")));
    connect(effectGenAction, &QAction::triggered, this, &MainWindow::onOpenEffectGenerator);
    toolsMenu->addAction(effectGenAction);
    
    QAction *cameraPathAction = new QAction(tr("Editor de Cámaras..."), this);
    cameraPathAction->setShortcut(QKeySequence(tr("Ctrl+Shift+C")));
    connect(cameraPathAction, &QAction::triggered, this, &MainWindow::onOpenCameraPathEditor);
    toolsMenu->addAction(cameraPathAction);
    
    // Ramp Generator
    // Ramp Generator REMOVED (Pivoting to MD3)
    // ...
    // QAction *rampGenAction = ...
    
    QAction *meshGenAction = new QAction(tr("Generador de Modelos MD3..."), this);
    meshGenAction->setShortcut(QKeySequence(tr("Ctrl+Shift+M")));
    connect(meshGenAction, &QAction::triggered, this, &MainWindow::onOpenMeshGenerator);
    toolsMenu->addAction(meshGenAction);
    
    QAction *objImportAction = new QAction(tr("Conversor OBJ a MD3..."), this);
    objImportAction->setShortcut(QKeySequence(tr("Ctrl+Shift+O")));
    connect(objImportAction, &QAction::triggered, this, &MainWindow::openObjConverter);
    toolsMenu->addAction(objImportAction);
    
    // === BUILD MENU ===
    QMenu *buildMenu = menuBar()->addMenu(tr("&Compilar"));
    
    QAction *buildAction = new QAction(tr("Compilar Proyecto"), this);
    buildAction->setShortcut(QKeySequence("F5"));
    connect(buildAction, &QAction::triggered, this, &MainWindow::onBuildProject);
    buildMenu->addAction(buildAction);
    
    QAction *runAction = new QAction(tr("Ejecutar"), this);
    runAction->setShortcut(QKeySequence("F9"));
    connect(runAction, &QAction::triggered, this, &MainWindow::onRunProject);
    buildMenu->addAction(runAction);
    
    QAction *buildRunAction = new QAction(tr("Compilar y Ejecutar"), this);
    buildRunAction->setShortcut(QKeySequence("Ctrl+R"));
    connect(buildRunAction, &QAction::triggered, this, &MainWindow::onBuildAndRun);
    buildMenu->addAction(buildRunAction);
    
    buildMenu->addSeparator();
    
    QAction *stopAction = new QAction(tr("Detener Ejecución"), this);
    stopAction->setShortcut(QKeySequence("Shift+F9"));
    connect(stopAction, &QAction::triggered, this, &MainWindow::onStopRunning);
    buildMenu->addAction(stopAction);
    
    buildMenu->addSeparator();
    
    QAction *configAction = new QAction(tr("Configurar BennuGD2..."), this);
    connect(configAction, &QAction::triggered, this, &MainWindow::onConfigureBennuGD2);
    buildMenu->addAction(configAction);
    
    QAction *installBennuAction = new QAction(tr("Instalar BennuGD2..."), this);
    connect(installBennuAction, &QAction::triggered, this, &MainWindow::onInstallBennuGD2);
    buildMenu->addAction(installBennuAction);
    
    buildMenu->addSeparator();
    
    QAction *genAction = new QAction(tr("Generar Código"), this);
    genAction->setShortcut(QKeySequence("Ctrl+Shift+G"));
    connect(genAction, &QAction::triggered, this, &MainWindow::onGenerateCode);
    buildMenu->addAction(genAction);
    
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
    m_modeCombo->addItem("Dibujar Sector", GridEditor::MODE_DRAW_SECTOR);
    m_modeCombo->addItem("Editar Vértices", GridEditor::MODE_EDIT_VERTICES);
    m_modeCombo->addItem("Seleccionar Pared", GridEditor::MODE_SELECT_WALL);
    m_modeCombo->addItem("Seleccionar Entidad", GridEditor::MODE_SELECT_ENTITY); // Added here for visibility
    m_modeCombo->addItem("Seleccionar Sector", GridEditor::MODE_SELECT_SECTOR);
    m_modeCombo->addItem("Colocar Sprite", GridEditor::MODE_PLACE_SPRITE);
    m_modeCombo->addItem("Colocar Spawn", GridEditor::MODE_PLACE_SPAWN);
    m_modeCombo->addItem("Colocar Cámara", GridEditor::MODE_PLACE_CAMERA);
    m_modeCombo->addItem("Portal Manual", GridEditor::MODE_MANUAL_PORTAL); // Also beneficial to expose this

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
    
    toolbar->addSeparator();
    
    // Build & Run
    QPushButton *buildBtn = new QPushButton(QIcon::fromTheme("system-run"), tr("Compilar (F5)"));
    buildBtn->setIcon(QIcon::fromTheme("media-playback-start")); // Using media icon as generic 'start' if system-run fails
    // actually let's use standard icons if possible, or text if not.
    // User requested: "compilar y no solo compilar y ejecutar, y si le puedes añadir iconos mejor que mejor"
    
    // Build Only
    QAction *buildToolbarAction = new QAction(QIcon::fromTheme("system-run"), tr("Compilar"), this);
    buildToolbarAction->setToolTip(tr("Compilar Proyecto (F5)"));
    connect(buildToolbarAction, &QAction::triggered, this, &MainWindow::onBuildProject);
    toolbar->addAction(buildToolbarAction);
    
    // Build & Run
    QAction *buildRunToolbarAction = new QAction(QIcon::fromTheme("media-playback-start"), tr("Compilar y Ejecutar"), this);
    buildRunToolbarAction->setToolTip(tr("Compilar y Ejecutar (Ctrl+F9)"));
    connect(buildRunToolbarAction, &QAction::triggered, this, &MainWindow::onBuildAndRun);
    toolbar->addAction(buildRunToolbarAction);
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
    m_sectorFloorZSpin->setRange(-100000, 100000);
    m_sectorFloorZSpin->setValue(0);
    connect(m_sectorFloorZSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onSectorFloorZChanged);
    floorLayout->addWidget(m_sectorFloorZSpin);
    heightLayout->addLayout(floorLayout);
    
    QHBoxLayout *ceilingLayout = new QHBoxLayout();
    ceilingLayout->addWidget(new QLabel(tr("Techo Z:")));
    m_sectorCeilingZSpin = new QDoubleSpinBox();
    m_sectorCeilingZSpin->setRange(-100000, 100000);
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
    
    // === CONSOLE DOCK ===
    // Console Dock is created in setupBuildSystem() to ensure BuildManager is also created
    // So we don't create it here to avoid duplication.
    // However, we need to ensure setupBuildSystem() is called in constructor.
    
    // === CODE PREVIEW DOCK ===
    m_codePreviewDock = new QDockWidget(tr("Vista Previa de Código"), this);
    m_codePreviewDock->setObjectName("CodePreviewDock");
    m_codePreviewPanel = new CodePreviewPanel(this);
    connect(m_codePreviewPanel, &CodePreviewPanel::openInEditorRequested, 
            this, &MainWindow::onCodePreviewOpenRequested);
            
    m_codePreviewDock->setWidget(m_codePreviewPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_codePreviewDock);

    // --- ASSET BROWSER ---
    // Created here to ensure correct tab order/stacking
    m_assetBrowser = new AssetBrowser(this);
    m_assetDock = new QDockWidget(tr("Explorador de Archivos"), this);
    m_assetDock->setObjectName("AssetBrowserDock_v2"); // Unique name to avoid old settings
    m_assetDock->setWidget(m_assetBrowser);
    m_assetDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, m_assetDock);
    
    // Connect Asset Browser signals
    connect(m_assetBrowser, &AssetBrowser::mapFileRequested, this, [this](const QString &path){
         openMapFile(path); // Use openMapFile instead of QMessageBox
    });
    connect(m_assetBrowser, &AssetBrowser::fileClicked, this, [this](const QString &path){
        if (path.endsWith(".prg") || path.endsWith(".inc") || path.endsWith(".h") || path.endsWith(".c")) {
            m_codePreviewPanel->showFile(path);
            if (m_codePreviewDock->isHidden()) m_codePreviewDock->show();
        }
    });
    connect(m_assetBrowser, &AssetBrowser::fpgEditorRequested, this, [this](const QString &path){
        qDebug() << "Opening FPG Editor for:" << path;
        
        // Open FPG Editor window
        if (!m_fpgEditor) {
            m_fpgEditor = new FPGEditor(this);
            connect(m_fpgEditor, &FPGEditor::fpgReloaded, this, &MainWindow::onFPGReloaded);
        }
        
        // Load the selected FPG file
        m_fpgEditor->setFPGPath(path);
        m_fpgEditor->loadFPG();
        
        // Show and activate the editor
        m_fpgEditor->show();
        m_fpgEditor->raise();
        m_fpgEditor->activateWindow();
    });
    connect(m_assetBrowser, &AssetBrowser::fileDoubleClicked, this, [this](const QString &path){
        QFileInfo info(path);
        QString ext = info.suffix().toLower();
        
        // Open .prg and .h files in code editor
        if (ext == "prg" || ext == "h" || ext == "inc" || ext == "c") {
            onOpenCodeEditor(path);
        }
    });
    
    // Sector tree dock (hierarchical with groups)
    m_sectorListDock = new QDockWidget(tr("Sectores"), this);
    m_sectorListDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    
    m_sectorTree = new QTreeWidget();
    m_sectorTree->setHeaderHidden(true);  // Hide header to avoid duplication with dock title
    m_sectorTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_sectorTree, &QTreeWidget::itemClicked,
            this, &MainWindow::onSectorTreeItemClicked);
    connect(m_sectorTree, &QTreeWidget::itemDoubleClicked,
            this, &MainWindow::onSectorTreeItemDoubleClicked);
    connect(m_sectorTree, &QTreeWidget::customContextMenuRequested,
            this, &MainWindow::onSectorTreeContextMenu);
    m_sectorListDock->setWidget(m_sectorTree);
    addDockWidget(Qt::LeftDockWidgetArea, m_sectorListDock);
    
    // Entity Properties Dock
    QDockWidget *entityDock = new QDockWidget(tr("Propiedades de Entidad"), this);
    m_entityPanel = new EntityPropertyPanel();
    connect(m_entityPanel, &EntityPropertyPanel::entityChanged, this, &MainWindow::onEntityChanged);
    entityDock->setWidget(m_entityPanel);
    addDockWidget(Qt::RightDockWidgetArea, entityDock);
    
    /* DECAL DOCK DISABLED - Replaced by FPG Editor
    // Decal Properties Dock
    m_decalDock = new QDockWidget(tr("Propiedades del Decal"), this);
    QWidget *decalWidget = new QWidget();
    QFormLayout *decalLayout = new QFormLayout();
    
    m_decalIdLabel = new QLabel(tr("Ninguno"));
    decalLayout->addRow(tr("ID:"), m_decalIdLabel);
    
    m_decalXSpin = new QDoubleSpinBox();
    m_decalXSpin->setRange(-100000, 100000);
    m_decalXSpin->setDecimals(1);
    connect(m_decalXSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onDecalXChanged);
    decalLayout->addRow(tr("Posición X:"), m_decalXSpin);
    
    m_decalYSpin = new QDoubleSpinBox();
    m_decalYSpin->setRange(-100000, 100000);
    m_decalYSpin->setDecimals(1);
    connect(m_decalYSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onDecalYChanged);
    decalLayout->addRow(tr("Posición Y:"), m_decalYSpin);
    
    m_decalWidthSpin = new QDoubleSpinBox();
    m_decalWidthSpin->setRange(1, 10000);
    m_decalWidthSpin->setValue(100);
    m_decalWidthSpin->setDecimals(1);
    connect(m_decalWidthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onDecalWidthChanged);
    decalLayout->addRow(tr("Ancho:"), m_decalWidthSpin);
    
    m_decalHeightSpin = new QDoubleSpinBox();
    m_decalHeightSpin->setRange(1, 10000);
    m_decalHeightSpin->setValue(100);
    m_decalHeightSpin->setDecimals(1);
    connect(m_decalHeightSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onDecalHeightChanged);
    decalLayout->addRow(tr("Alto:"), m_decalHeightSpin);
    
    m_decalRotationSpin = new QDoubleSpinBox();
    m_decalRotationSpin->setRange(0, 360);
    m_decalRotationSpin->setSuffix("°");
    m_decalRotationSpin->setDecimals(1);
    connect(m_decalRotationSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onDecalRotationChanged);
    decalLayout->addRow(tr("Rotación:"), m_decalRotationSpin);
    
    QHBoxLayout *decalTexLayout = new QHBoxLayout();
    m_decalTextureSpin = new QSpinBox();
    m_decalTextureSpin->setRange(0, 9999);
    connect(m_decalTextureSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onDecalTextureChanged);
    decalTexLayout->addWidget(m_decalTextureSpin);
    
    QPushButton *selectDecalTexBtn = new QPushButton(tr("..."));
    selectDecalTexBtn->setMaximumWidth(30);
    connect(selectDecalTexBtn, &QPushButton::clicked,
            this, &MainWindow::onSelectDecalTexture);
    decalTexLayout->addWidget(selectDecalTexBtn);
    decalLayout->addRow(tr("Textura:"), decalTexLayout);
    
    m_decalAlphaSpin = new QDoubleSpinBox();
    m_decalAlphaSpin->setRange(0.0, 1.0);
    m_decalAlphaSpin->setValue(1.0);
    m_decalAlphaSpin->setSingleStep(0.1);
    m_decalAlphaSpin->setDecimals(2);
    connect(m_decalAlphaSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onDecalAlphaChanged);
    decalLayout->addRow(tr("Opacidad:"), m_decalAlphaSpin);
    
    m_decalRenderOrderSpin = new QSpinBox();
    m_decalRenderOrderSpin->setRange(-100, 100);
    m_decalRenderOrderSpin->setValue(0);
    connect(m_decalRenderOrderSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onDecalRenderOrderChanged);
    decalLayout->addRow(tr("Orden:"), m_decalRenderOrderSpin);
    
    m_deleteDecalButton = new QPushButton(tr("Eliminar Decal"));
    connect(m_deleteDecalButton, &QPushButton::clicked,
            this, &MainWindow::onDeleteDecal);
    decalLayout->addRow(m_deleteDecalButton);
    
    decalWidget->setLayout(decalLayout);
    m_decalDock->setWidget(decalWidget);
    addDockWidget(Qt::RightDockWidgetArea, m_decalDock);
    m_decalDock->hide(); // Hidden by default
    */
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
    // Create new GridEditor and MapData
    GridEditor *editor = new GridEditor(this);
    // Note: GridEditor now owns its MapData (initialized in its constructor)
    
    editor->setEditMode(static_cast<GridEditor::EditMode>(m_modeCombo->currentIndex()));
    editor->showGrid(m_viewGridAction->isChecked());
    
    // Connect signals
    connect(editor, &GridEditor::statusMessage, this, [this](const QString &msg) {
        m_statusLabel->setText(msg);
    });
    connect(editor, &GridEditor::wallSelected, this, &MainWindow::onWallSelected);
    connect(editor, &GridEditor::sectorSelected, this, &MainWindow::onSectorSelected);
    connect(editor, &GridEditor::decalPlaced, this, &MainWindow::onDecalPlaced); 
    connect(editor, &GridEditor::cameraPlaced, this, &MainWindow::onCameraPlaced);
    connect(editor, &GridEditor::entitySelected, this, &MainWindow::onEntitySelected);
    connect(editor, &GridEditor::entityMoved, this, &MainWindow::onEntityChanged); // Live visual update
    
    // Apply current textures to it
    editor->setTextures(m_textureCache);
    
    // Add to tab
    int index = m_tabWidget->addTab(editor, tr("Sin Título %1").arg(m_tabWidget->count() + 1));
    m_tabWidget->setCurrentIndex(index);
    
    updateSectorList();
    updateWindowTitle();
    m_statusLabel->setText(tr("Nuevo mapa creado"));
}

void MainWindow::onOpenMap()
{
    QString filename = QFileDialog::getOpenFileName(this, tr("Abrir Mapa"),
                                                    "", tr("RayMap Files (*.rmap *.raymap);;All Files (*)"));
    if (filename.isEmpty()) return;
    
    openMapFile(filename);
}

void MainWindow::onSaveMap()
{
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;

    if (editor->fileName().isEmpty()) {
        onSaveMapAs();
        return;
    }
    
    // Check camera
    if (editor->mapData()->camera.z < 1.0f) {
        editor->mapData()->camera.z = 32.0f; // Default height
    }
    
    if (RayMapFormat::saveMap(editor->fileName(), *editor->mapData())) {
        m_statusLabel->setText(tr("Mapa guardado: %1").arg(editor->fileName()));
    } else {
        QMessageBox::critical(this, tr("Error"), tr("No se pudo guardar el mapa."));
    }
}

void MainWindow::onSaveMapAs()
{
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;

    // Determine default save location
    QString defaultPath;
    if (m_projectManager && !m_projectManager->getProjectPath().isEmpty()) {
        // If project is open, suggest project's maps folder
        QString mapsDir = m_projectManager->getProjectPath() + "/assets/maps";
        QDir dir;
        if (!dir.exists(mapsDir)) {
            dir.mkpath(mapsDir);  // Create if doesn't exist
        }
        
        // Use existing filename or suggest "new_map.raymap"
        if (!editor->fileName().isEmpty()) {
            QFileInfo info(editor->fileName());
            defaultPath = mapsDir + "/" + info.fileName();
        } else {
            defaultPath = mapsDir + "/new_map.raymap";
        }
    } else {
        // No project, use current file or empty
        defaultPath = editor->fileName();
    }

    QString filename = QFileDialog::getSaveFileName(this, tr("Guardar Mapa Como"), 
                                                    defaultPath, 
                                                    tr("RayMap (*.rmap *.raymap)"));
    if (filename.isEmpty()) return;
    
    if (!filename.endsWith(".rmap") && !filename.endsWith(".raymap")) {
        filename += ".raymap";  // Use .raymap as default
    }
    
    // Check camera
    if (editor->mapData()->camera.z < 1.0f) {
        editor->mapData()->camera.z = 32.0f; // Default height
    }
    
    if (RayMapFormat::saveMap(filename, *editor->mapData())) {
        editor->setFileName(filename);
        updateWindowTitle();
        m_tabWidget->setTabText(m_tabWidget->currentIndex(), QFileInfo(filename).fileName());
        addToRecentMaps(filename);
        m_statusLabel->setText(tr("Mapa guardado: %1").arg(filename));
    } else {
        QMessageBox::critical(this, tr("Error"), tr("No se pudo guardar el mapa."));
    }
}


void MainWindow::onCameraPlaced(float x, float y)
{
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;
    
    editor->mapData()->camera.x = x;
    editor->mapData()->camera.y = y;
    editor->mapData()->camera.enabled = true;
    m_statusLabel->setText(tr("Cámara colocada en (%1, %2)").arg(x).arg(y));
}

void MainWindow::onImportWLD()
{
    QString filename = QFileDialog::getOpenFileName(this, tr("Importar WLD"), "", tr("WLD Files (*.wld)"));
    if (filename.isEmpty()) return;
    
    // Create new tab for import
    GridEditor *editor = new GridEditor(this);
    
    if (WLDImporter::importWLD(filename, *editor->mapData())) {
        // Setup editor
        editor->setFileName(""); // Imported, so no filename yet
        editor->setEditMode(static_cast<GridEditor::EditMode>(m_modeCombo->currentIndex()));
        editor->showGrid(m_viewGridAction->isChecked());
        editor->setTextures(m_textureCache);
        
        // Connect signals
        connect(editor, &GridEditor::statusMessage, this, [this](const QString &msg) {
            m_statusLabel->setText(msg);
        });
        connect(editor, &GridEditor::wallSelected, this, &MainWindow::onWallSelected);
        connect(editor, &GridEditor::sectorSelected, this, &MainWindow::onSectorSelected);
        connect(editor, &GridEditor::decalPlaced, this, &MainWindow::onDecalPlaced);
        connect(editor, &GridEditor::cameraPlaced, this, &MainWindow::onCameraPlaced);
        connect(editor, &GridEditor::entitySelected, this, &MainWindow::onEntitySelected);
        connect(editor, &GridEditor::entityMoved, this, &MainWindow::onEntityChanged);
        
        m_tabWidget->addTab(editor, tr("Importado %1").arg(QFileInfo(filename).fileName()));
        m_tabWidget->setCurrentWidget(editor);
        
        updateSectorList();
        updateWindowTitle();
        updateVisualMode();
        m_statusLabel->setText(tr("WLD Importado: %1").arg(filename));
    } else {
        delete editor;
        QMessageBox::critical(this, tr("Error"), tr("No se pudo importar el archivo WLD."));
    }
}

void MainWindow::onLoadFPG()
{
    QString filename = QFileDialog::getOpenFileName(this, tr("Cargar FPG"), "", tr("FPG Files (*.fpg *.map)"));
    if (filename.isEmpty()) return;
    
    QVector<TextureEntry> textures; // Declared here
    
    // Load FPG with progress callback
    // FPGLoader is static, use class name
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
        // m_mapData accessed via current editor or we need to update all open editors?
        // Let's assume global FPG for now like before
        
        // m_mapData.textures.clear(); // NO, we don't have m_mapData member anymore
        
        m_textureCache.clear();
        for (const TextureEntry &entry : textures) {
            m_textureCache[entry.id] = entry.pixmap;
        }
        
        // Update Grid Editor
        // Update all editors
         for (int i=0; i<m_tabWidget->count(); i++) {
            GridEditor *ed = qobject_cast<GridEditor*>(m_tabWidget->widget(i));
            if (ed) {
                ed->setTextures(m_textureCache);
                ed->update();
            }
        }
        
        // Cache textures for selector
        m_textureCache.clear();
        for (const TextureEntry &entry : textures) {
            m_textureCache[entry.id] = entry.pixmap;
        }
        
        addToRecentFPGs(filename);
        m_currentFPGPath = filename;  // Store FPG path using correct member
        m_statusLabel->setText(tr("FPG loaded: %1 textures from %2")
                              .arg(textures.size()).arg(filename));
    } else {
        QMessageBox::critical(this, tr("Error"), tr("No se pudo cargar el archivo FPG.").arg(filename));
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
    QAction *action = qobject_cast<QAction *>(sender());
    if (action) {
        openMapFile(action->data().toString());
    }
}

void MainWindow::openRecentFPG()
{
    QAction *action = qobject_cast<QAction *>(sender());
    if (action) {
        QString filename = action->data().toString();
        
        QVector<TextureEntry> textures;
        bool success = FPGLoader::loadFPG(filename, textures, nullptr);
        
        if (success) {
            m_currentFPGPath = filename; // Consistent variable
            addToRecentFPGs(filename);
            
            QMap<int, QPixmap> textureMap = FPGLoader::getTextureMap(textures);
            m_textureCache.clear();
            for (auto it = textureMap.begin(); it != textureMap.end(); ++it) {
                 m_textureCache.insert(it.key(), it.value());
            }
            // Also populate cache from Vector if needed, but getTextureMap does it.
            // Wait, previous code used loop over map
            m_textureCache = textureMap; // Simple assignment
            
            m_statusLabel->setText(tr("FPG cargado: %1 (%2 texturas)").arg(filename).arg(m_textureCache.size()));
            
             for (int i=0; i<m_tabWidget->count(); i++) {
                GridEditor *ed = qobject_cast<GridEditor*>(m_tabWidget->widget(i));
                if (ed) {
                    ed->setTextures(m_textureCache);
                    ed->update();
                }
            }
        }
    }
}

/* ============================================================================
   VIEW OPERATIONS
   ============================================================================ */

void MainWindow::onZoomIn()
{
    GridEditor *editor = getCurrentEditor();
    if (editor)
        editor->setZoom(editor->property("zoom").toFloat() * 1.2f);
}

void MainWindow::onZoomOut()
{
    GridEditor *editor = getCurrentEditor();
    if (editor)
        editor->setZoom(editor->property("zoom").toFloat() / 1.2f);
}

void MainWindow::onZoomReset()
{
    GridEditor *editor = getCurrentEditor();
    if (editor)
        editor->setZoom(1.0f);
}

void MainWindow::onToggleVisualMode()
{
    if (!m_visualModeWidget) {
        m_visualModeWidget = new VisualModeWidget();
    }
    
    if (m_visualModeWidget->isVisible()) {
        m_visualModeWidget->hide();
    } else {
        GridEditor *editor = getCurrentEditor();
        if (editor) {
             m_visualModeWidget->setMapData(*editor->mapData());
             // Load textures to visual mode... this might be heavy if done every time
             // ideally VisualModeWidget should share texture cache or similar
             for (auto it = m_textureCache.begin(); it != m_textureCache.end(); ++it) {
                 m_visualModeWidget->loadTexture(it.key(), it.value().toImage());
             }
             
             m_visualModeWidget->show();
             m_visualModeWidget->raise();
             m_visualModeWidget->activateWindow();
        }
    }
}


/* ============================================================================
   EDIT MODE
   ============================================================================ */

void MainWindow::onModeChanged(int index)
{
    GridEditor *editor = getCurrentEditor();
    if (editor) {
        int modeId = m_modeCombo->itemData(index).toInt();
        editor->setEditMode(static_cast<GridEditor::EditMode>(modeId));
    }
}

void MainWindow::onTextureSelected(int textureId)
{
    GridEditor *editor = getCurrentEditor();
    if (editor) {
        editor->setSelectedTexture(textureId);
    }
}

/* ============================================================================
   SECTOR EDITING
   ============================================================================ */

void MainWindow::onSectorSelected(int sectorId)
{
    m_selectedSectorId = sectorId;
    updateSectorPanel();
    
    // Sync list selection
    if (m_sectorTree) {
        m_sectorTree->blockSignals(true); // Prevent loop
        
        // Find and select the item in the tree
        QTreeWidgetItemIterator it(m_sectorTree);
        while (*it) {
            if ((*it)->data(0, Qt::UserRole).toInt() == sectorId) {
                m_sectorTree->setCurrentItem(*it);
                m_sectorTree->scrollToItem(*it);
                break;
            }
            ++it;
        }
        
        m_sectorTree->blockSignals(false);
    }
}

void MainWindow::onVertexSelected(int sectorId, int vertexIndex)
{
    m_selectedSectorId = sectorId;
    // Could add vertex editing panel here in the future
}

void MainWindow::onSectorFloorZChanged(double value)
{
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;
    
    if (m_selectedSectorId != -1) {
        for (Sector &sector : editor->mapData()->sectors) {
            if (sector.sector_id == m_selectedSectorId) {
                sector.floor_z = value;
                editor->update();
                updateVisualMode();
                break;
            }
        }
    }
}

void MainWindow::onSectorCeilingZChanged(double value)
{
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;

    if (m_selectedSectorId != -1) {
        for (Sector &sector : editor->mapData()->sectors) {
            if (sector.sector_id == m_selectedSectorId) {
                sector.ceiling_z = value;
                editor->update();
                updateVisualMode();
                break;
            }
        }
    }
}

void MainWindow::onSectorFloorTextureChanged(int value)
{
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;

    if (m_selectedSectorId != -1) {
        for (Sector &sector : editor->mapData()->sectors) {
            if (sector.sector_id == m_selectedSectorId) {
                sector.floor_texture_id = value;
                editor->update();
                updateVisualMode();
                break;
            }
        }
    }
}

void MainWindow::onSectorCeilingTextureChanged(int value)
{
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;

    if (m_selectedSectorId != -1) {
        for (Sector &sector : editor->mapData()->sectors) {
            if (sector.sector_id == m_selectedSectorId) {
                sector.ceiling_texture_id = value;
                editor->update();
                updateVisualMode();
                break;
            }
        }
    }
}

void MainWindow::onSelectSectorFloorTexture()
{
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;
    auto *map = editor->mapData();

    int sectorIndex = -1;
    for (int i = 0; i < map->sectors.size(); i++) {
        if (map->sectors[i].sector_id == m_selectedSectorId) {
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
        
        // Direct update
        map->sectors[sectorIndex].floor_texture_id = textureId;
        editor->update();
        updateVisualMode();
        
        // Update UI preventing signal loop
        m_sectorFloorTextureSpin->blockSignals(true);
        m_sectorFloorTextureSpin->setValue(textureId);
        m_sectorFloorTextureSpin->blockSignals(false);
    }
}

void MainWindow::onSelectSectorCeilingTexture()
{
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;
    auto *map = editor->mapData();

    int sectorIndex = -1;
    for (int i = 0; i < map->sectors.size(); i++) {
        if (map->sectors[i].sector_id == m_selectedSectorId) {
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
        
        // Direct update
        map->sectors[sectorIndex].ceiling_texture_id = textureId;
        editor->update();
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
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;
    auto *map = editor->mapData();

    if (m_selectedSectorId >= 0 && m_selectedSectorId < map->sectors.size() &&
        m_selectedWallId >= 0 && m_selectedWallId < map->sectors[m_selectedSectorId].walls.size()) {
        map->sectors[m_selectedSectorId].walls[m_selectedWallId].texture_id_lower = value;
        editor->update();
        updateVisualMode();
    }
}

void MainWindow::onWallTextureMiddleChanged(int value)
{
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;
    auto *map = editor->mapData();

    if (m_selectedSectorId >= 0 && m_selectedSectorId < map->sectors.size() &&
        m_selectedWallId >= 0 && m_selectedWallId < map->sectors[m_selectedSectorId].walls.size()) {
        map->sectors[m_selectedSectorId].walls[m_selectedWallId].texture_id_middle = value;
        editor->update();
        updateVisualMode();
    }
}



void MainWindow::onWallTextureUpperChanged(int value)
{
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;
    auto *map = editor->mapData();

    if (m_selectedSectorId >= 0 && m_selectedSectorId < map->sectors.size() &&
        m_selectedWallId >= 0 && m_selectedWallId < map->sectors[m_selectedSectorId].walls.size()) {
        map->sectors[m_selectedSectorId].walls[m_selectedWallId].texture_id_upper = value;
        editor->update();
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
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;
    auto *map = editor->mapData();

    if (m_selectedSectorId < 0 || m_selectedSectorId >= map->sectors.size()) {
        QMessageBox::warning(this, tr("Advertencia"), 
                           tr("Selecciona una pared primero para identificar el sector"));
        return;
    }
    
    // Get current middle texture value
    int textureId = m_wallTextureMiddleSpin->value();
    
    
    // Apply to all walls in the sector
    Sector &sector = map->sectors[m_selectedSectorId];
    for (Wall &wall : sector.walls) {
        wall.texture_id_middle = textureId;
    }
    
    editor->update();
    updateVisualMode();
    
    QMessageBox::information(this, tr("Éxito"), 
                           tr("Textura %1 aplicada a %2 paredes del sector %3")
                           .arg(textureId)
                           .arg(sector.walls.size())
                           .arg(m_selectedSectorId));
}

void MainWindow::onWallSplitLowerChanged(double value)
{
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;

    for (Sector &sector : editor->mapData()->sectors) {
        for (Wall &wall : sector.walls) {
            if (wall.wall_id == m_selectedWallId) {
                wall.texture_split_z_lower = value;
                editor->update();
                updateVisualMode();
                return;
            }
        }
    }
}

void MainWindow::onWallSplitUpperChanged(double value)
{
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;

    for (Sector &sector : editor->mapData()->sectors) {
        for (Wall &wall : sector.walls) {
            if (wall.wall_id == m_selectedWallId) {
                wall.texture_split_z_upper = value;
                editor->update();
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
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;

    if (checked) {
        editor->setEditMode(GridEditor::MODE_MANUAL_PORTAL);
        m_statusLabel->setText(tr("Modo Portal Manual: Selecciona la PRIMERA pared para el portal..."));
        m_pendingPortalSector = -1;
        m_pendingPortalWall = -1;
    } else {
        editor->setEditMode(GridEditor::MODE_SELECT_SECTOR); // Default back to generic selection
        m_statusLabel->setText(tr("Modo Portal Manual desactivado"));
        m_pendingPortalSector = -1;
        m_pendingPortalWall = -1;
    }
}

void MainWindow::onManualPortalWallSelected(int sectorIndex, int wallIndex)
{
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;
    auto *map = editor->mapData();

    if (sectorIndex < 0 || wallIndex < 0) return;
    
    // Check if we are selecting Source or Target
    if (m_pendingPortalSector == -1) {
        // --- STEP 1: SOURCE SELECTION ---
        
        // Validation: Verify wall exists
        if (sectorIndex >= map->sectors.size()) return;
        Sector &sector = map->sectors[sectorIndex];
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
        Sector &secA = map->sectors[m_pendingPortalSector];
        Wall &wallA = secA.walls[m_pendingPortalWall];
        
        // Get Target Wall
        Sector &secB = map->sectors[sectorIndex];
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
        portal.portal_id = map->getNextPortalId();
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
        
        map->portals.append(portal);
        
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
        
        editor->update(); // Redraw
        
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
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;
    auto *map = editor->mapData();

    if (map->sectors.size() < 2) {
        QMessageBox::information(this, tr("Portal Detection"),
                               tr("Need at least 2 sectors to detect portals"));
        return;
    }
    
    QMessageBox::StandardButton reply = QMessageBox::question(this, tr("Auto-Detect Portals"),
        tr("This will CLEAR all existing portals (including manual ones) and auto-detect them.\nContinue?"),
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    // Clear existing portals
    map->portals.clear();
    
    // Reset portal IDs in all walls
    for (Sector &sector : map->sectors) {
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
        
        for (int i = 0; i < map->sectors.size(); i++) {
            Sector &sectorA = map->sectors[i];
            
            for (int k = 0; k < map->sectors.size(); k++) {
                if (i == k) continue;
                Sector &sectorB = map->sectors[k];
                
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
                                    newWall.wall_id = map->getNextWallId();
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
    
    for (int i = 0; i < map->sectors.size(); i++) {
        Sector &sectorA = map->sectors[i];
        
        for (int j = i + 1; j < map->sectors.size(); j++) {
            Sector &sectorB = map->sectors[j];
            
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
                        portal.portal_id = map->getNextPortalId();
                        portal.sector_a = sectorA.sector_id;
                        portal.sector_b = sectorB.sector_id;
                        portal.wall_id_a = wallA.wall_id;
                        portal.wall_id_b = wallB.wall_id;
                        portal.x1 = wallA.x1;
                        portal.y1 = wallA.y1;
                        portal.x2 = wallA.x2;
                        portal.y2 = wallA.y2;
                        
                        map->portals.append(portal);
                        
                        wallA.portal_id = portal.portal_id;
                        wallB.portal_id = portal.portal_id;
                        
                        portalsCreated++;
                        break;
                    }
                }
            }
        }
    }
    
    
    editor->update();
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
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;
    auto *map = editor->mapData();

    if (sectorIndex < 0 || sectorIndex >= map->sectors.size()) return;
    Sector &sector = map->sectors[sectorIndex];
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
    for (int i = 0; i < map->portals.size(); i++) {
        if (map->portals[i].portal_id == portalId) {
            map->portals.removeAt(i);
            removed = true;
            break;
        }
    }
    
    // Reset walls referencing this portal
    int wallsUpdated = 0;
    for (Sector &s : map->sectors) {
        for (Wall &w : s.walls) {
            if (w.portal_id == portalId) {
                w.portal_id = -1;
                wallsUpdated++;
            }
        }
    }
    
    editor->update();
    updateVisualMode();
    m_statusLabel->setText(tr("Portal %1 eliminado (referencias limpiadas: %2)").arg(portalId).arg(wallsUpdated));
}

/* ============================================================================
   HELPERS
   ============================================================================ */

void MainWindow::updateWindowTitle()
{
    QString title = "RayMap Editor - Geometric Sectors";
    GridEditor *editor = getCurrentEditor();
    if (editor && !editor->fileName().isEmpty()) {
        title += " - " + QFileInfo(editor->fileName()).fileName();
    }
    setWindowTitle(title);
}

void MainWindow::updateSectorPanel()
{
    GridEditor *editor = getCurrentEditor();
    if (!editor) {
        m_sectorIdLabel->setText(tr("No sector selected"));
        return;
    }
    auto *map = editor->mapData();

    int sectorIndex = -1;
    for (int i = 0; i < map->sectors.size(); i++) {
        if (map->sectors[i].sector_id == m_selectedSectorId) {
            sectorIndex = i;
            break;
        }
    }

    if (sectorIndex != -1) {
        const Sector &sector = map->sectors[sectorIndex];
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
    GridEditor *editor = getCurrentEditor();
    if (!editor) {
        m_wallIdLabel->setText(tr("No wall selected"));
        m_portalTexGroup->setVisible(false);
        return;
    }
    auto *map = editor->mapData();

    if (m_selectedSectorId >= 0 && m_selectedSectorId < map->sectors.size() &&
        m_selectedWallId >= 0 && m_selectedWallId < map->sectors[m_selectedSectorId].walls.size()) {
        
        const Wall &wall = map->sectors[m_selectedSectorId].walls[m_selectedWallId];
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
    if (!m_sectorTree) return;
    
    m_sectorTree->clear();
    
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;
    auto *map = editor->mapData();
    
    // Create set of grouped sectors
    QSet<int> groupedSectors;
    for (const SectorGroup &group : map->sectorGroups) {
        for (int id : group.sector_ids) {
            groupedSectors.insert(id);
        }
    }
    
    // Add groups as parent nodes
    for (const SectorGroup &group : map->sectorGroups) {
        QTreeWidgetItem *groupItem = new QTreeWidgetItem(m_sectorTree);
        groupItem->setText(0, QString("📁 %1 (%2 sectores)")
                          .arg(group.name)
                          .arg(group.sector_ids.size()));
        groupItem->setData(0, Qt::UserRole, -group.group_id - 1); // Negative = group
        groupItem->setFlags(groupItem->flags() | Qt::ItemIsEditable);
        groupItem->setExpanded(true); // Expand by default
        
        // Add child sectors
        for (int sectorId : group.sector_ids) {
            const Sector *sector = map->findSector(sectorId);
            if (sector) {
                QTreeWidgetItem *sectorItem = new QTreeWidgetItem(groupItem);
                sectorItem->setText(0, QString("  Sector %1").arg(sectorId));
                sectorItem->setData(0, Qt::UserRole, sectorId);
            }
        }
    }
    
    // Add ungrouped sectors
    for (const Sector &sector : map->sectors) {
        if (!groupedSectors.contains(sector.sector_id)) {
            QTreeWidgetItem *item = new QTreeWidgetItem(m_sectorTree);
            item->setText(0, QString("Sector %1").arg(sector.sector_id));
            item->setData(0, Qt::UserRole, sector.sector_id);
        }
    }
}

void MainWindow::onSectorTreeItemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    if (!item) return;
    
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;

    int data = item->data(0, Qt::UserRole).toInt();
    
    // Negative values are groups, positive are sector IDs
    if (data >= 0) {
        // It's a sector
        int sectorId = data;
        editor->setSelectedSector(sectorId);
        onSectorSelected(sectorId);
    }
}

void MainWindow::onSectorTreeItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    if (!item) return;
    
    int data = item->data(0, Qt::UserRole).toInt();
    
    if (data < 0) {
        // It's a group - toggle expand/collapse
        item->setExpanded(!item->isExpanded());
    }
}



/* ============================================================================
   SECTOR CONTEXT MENU & OPERATIONS
   ============================================================================ */

void MainWindow::onSectorTreeContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_sectorTree->itemAt(pos);
    if (!item) return;
    
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;
    auto *map = editor->mapData();
    
    QMenu menu;
    int data = item->data(0, Qt::UserRole).toInt();
    
    if (data < 0) {
        // It's a group
        int groupId = -data - 1;
        
        QAction *renameAction = menu.addAction(tr("Renombrar grupo"));
        QAction *setParentAction = menu.addAction(tr("Establecer sector padre"));
        QAction *moveAction = menu.addAction(tr("Mover grupo"));
        menu.addSeparator();
        QAction *deleteAction = menu.addAction(tr("Eliminar grupo (mantener sectores)"));
        
        QAction *selected = menu.exec(m_sectorTree->mapToGlobal(pos));
        
        if (selected == renameAction) {
            item->setFlags(item->flags() | Qt::ItemIsEditable);
            m_sectorTree->editItem(item, 0);
        } else if (selected == setParentAction) {
            // Show dialog to select parent sector
            QStringList sectorNames;
            QList<int> sectorIds;
            
            for (const Sector &sector : map->sectors) {
                // Only ungrouped sectors can be parents for now
                // A sector is ungrouped if its group_id is -1 (or not part of any group in m_mapData.sectorGroups)
                bool isUngrouped = true;
                for (const SectorGroup &group : map->sectorGroups) {
                    if (group.sector_ids.contains(sector.sector_id)) {
                        isUngrouped = false;
                        break;
                    }
                }

                if (isUngrouped) {
                    sectorNames.append(QString("Sector %1").arg(sector.sector_id));
                    sectorIds.append(sector.sector_id);
                }
            }
            
            if (sectorNames.isEmpty()) {
                QMessageBox::information(this, tr("Sin sectores"),
                                       tr("No hay sectores disponibles para ser padres.\n"
                                          "Solo los sectores no agrupados pueden ser padres."));
                return;
            }
            
            bool ok;
            QString selectedName = QInputDialog::getItem(this, tr("Seleccionar sector padre"),
                                                        tr("Sector padre:"), sectorNames, 0, false, &ok);
            if (ok) {
                int idx = sectorNames.indexOf(selectedName);
                int parentId = sectorIds[idx];
                
                qDebug() << "Assigning parent sector" << parentId << "to group" << groupId;
                
                // Set parent for all sectors in group
                SectorGroup *group = map->findGroup(groupId);
                if (group) {
                    qDebug() << "Found group with" << group->sector_ids.size() << "sectors";
                    for (int sectorId : group->sector_ids) {
                        Sector *sector = map->findSector(sectorId);
                        if (sector) {
                            sector->parent_sector_id = parentId;
                            qDebug() << "  Set sector" << sectorId << "parent_sector_id =" << parentId;
                        }
                    }
                    m_statusLabel->setText(tr("Sector padre asignado al grupo '%1'").arg(group->name));
                    editor->update();
                } else {
                    qDebug() << "ERROR: Group" << groupId << "not found!";
                }
            }
        } else if (selected == moveAction) {
            QMessageBox::information(this, tr("Mover Grupo"),
                tr("Haz clic y arrastra en el mapa para mover todo el grupo.\n"
                   "Presiona ESC para cancelar."));
            editor->setGroupMoveMode(groupId);
        } else if (selected == deleteAction) {
            // Remove group but keep sectors
            for (int i = 0; i < map->sectorGroups.size(); i++) {
                if (map->sectorGroups[i].group_id == groupId) {
                    map->sectorGroups.removeAt(i);
                    updateSectorList();
                    break;
                }
            }
        }
    } else {
        // It's a sector
        int sectorId = data;
        menu.addAction(tr("Eliminar sector"), this, [this, sectorId]() {
            GridEditor *editor = getCurrentEditor();
            if (!editor) return;
            auto *map = editor->mapData();
            
            // Find and remove sector
            for (int i = 0; i < map->sectors.size(); i++) {
                if (map->sectors[i].sector_id == sectorId) {
                    map->sectors.removeAt(i);
                    
                    // Remove from any groups
                    for (SectorGroup &group : map->sectorGroups) {
                        group.sector_ids.removeAll(sectorId);
                    }
                    
                    updateSectorList();
                    editor->update();
                }
            }
        }); 
    }
    menu.exec(m_sectorTree->mapToGlobal(pos));
}

/* ============================================================================
   SECTOR OPERATIONS
   ============================================================================ */

void MainWindow::deleteSelectedSector()
{
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;
    auto *map = editor->mapData();

    if (m_selectedSectorId < 0) return;
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Eliminar Sector"),
        tr("¿Estás seguro de que deseas eliminar el sector %1?").arg(m_selectedSectorId),
        QMessageBox::Yes | QMessageBox::No);
        
    if (reply != QMessageBox::Yes) return;
    
    // Find sector sectorIndex
    int sectorIndex = -1;
    for (int i = 0; i < map->sectors.size(); i++) {
        if (map->sectors[i].sector_id == m_selectedSectorId) {
            sectorIndex = i;
            break;
        }
    }
    
    if (sectorIndex >= 0) {
        map->sectors.removeAt(sectorIndex);
        m_selectedSectorId = -1;
        updateSectorList();
        editor->update();
        updateVisualMode();
        m_statusLabel->setText(tr("Sector eliminado"));
    }
}

void MainWindow::copySelectedSector()
{
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;
    auto *map = editor->mapData();

    if (m_selectedSectorId < 0) return;
    
    // Find sector
    for (const Sector &sec : map->sectors) {
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

    GridEditor *editor = getCurrentEditor();
    if (!editor) return;
    auto *map = editor->mapData();
    
    // Create new sector from clipboard
    Sector newSector = m_clipboardSector;
    
    // Assign new unique ID
    int infoMaxId = 0;
    for (const Sector &s : map->sectors) {
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
    map->sectors.append(newSector);
    
    updateSectorList();
    editor->update();
    updateVisualMode();
    m_statusLabel->setText(tr("Sector pegado como ID %1").arg(newSector.sector_id));
    
    // Select the new sector
    m_selectedSectorId = newSector.sector_id;
    // Highlight in list?
}

void MainWindow::moveSelectedSector()
{
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;
    auto *map = editor->mapData();

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
        for (int i = 0; i < map->sectors.size(); i++) {
            if (map->sectors[i].sector_id == m_selectedSectorId) {
                Sector &sec = map->sectors[i];
                
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
                
                editor->update();
                updateVisualMode();
                m_statusLabel->setText(tr("Sector movido"));
                break;
            }
        }
    }
}

void MainWindow::onCreateRectangle()
{
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;
    auto *map = editor->mapData();

    // Ask for size
    bool ok;
    int size = QInputDialog::getInt(this, tr("Crear Rectángulo"),
                                    tr("Tamaño del rectángulo (ancho y alto):"),
                                    512, 64, 4096, 64, &ok);
    if (!ok) return;
    
    // Create rectangle centered at origin
    Sector newSector;
    newSector.sector_id = map->getNextSectorId();
    
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
        wall.wall_id = map->getNextWallId();
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
    
    map->sectors.append(newSector);
    updateSectorList();
    editor->update();
    updateVisualMode();
    
    m_statusLabel->setText(tr("Rectángulo %1x%1 creado (Sector %2)")
                          .arg(size).arg(newSector.sector_id));
}

void MainWindow::onCreateCircle()
{
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;
    auto *map = editor->mapData();

    // Ask for radius
    bool ok;
    int radius = QInputDialog::getInt(this, tr("Crear Círculo"),
                                      tr("Radio del círculo:"),
                                      256, 32, 2048, 32, &ok);
    if (!ok) return;
    
    // Create circle with 16 segments
    int segments = 16;
    Sector newSector;
    newSector.sector_id = map->getNextSectorId();
    
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
        wall.wall_id = map->getNextWallId();
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
    
    map->sectors.append(newSector);
    updateSectorList();
    editor->update();
    updateVisualMode();
    
    m_statusLabel->setText(tr("Círculo de radio %1 creado (Sector %2)")
                          .arg(radius).arg(newSector.sector_id));
}

/* ============================================================================
   INSERT TOOLS (HIGH-LEVEL GEOMETRY CREATION)
   ============================================================================ */

void MainWindow::onInsertBox()
{
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;
    auto *map = editor->mapData();

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
    newSector.sector_id = map->getNextSectorId();
    newSector.floor_z = floorZ;
    newSector.ceiling_z = ceilingZ;
    newSector.floor_texture_id = floorTexture;
    newSector.ceiling_texture_id = ceilingTexture;
    
    // Create 4 walls (rectangle)
    float hw = width / 2.0f;   // half width
    float hh = height / 2.0f;  // half height
    
    // Wall 0: Bottom (left to right)
    Wall wall0;
    wall0.wall_id = map->getNextWallId(); // Use unique ID generator
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
    wall1.wall_id = map->getNextWallId();
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
    wall2.wall_id = map->getNextWallId();
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
    wall3.wall_id = map->getNextWallId();
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
    map->sectors.append(newSector);
    int newSectorIndex = map->sectors.size() - 1;
    
    // Auto-detect parent sector (sector containing the box center)
    int parentSectorIndex = -1;
    for (int i = 0; i < newSectorIndex; i++) {
        const Sector &sector = map->sectors[i];
        
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
        map->sectors[newSectorIndex].parent_sector_id = map->sectors[parentSectorIndex].sector_id;
        
        // Add this sector as child of parent
        map->sectors[parentSectorIndex].child_sector_ids.append(newSector.sector_id);
        
        m_statusLabel->setText(tr("Caja creada (Sector %1) como hijo del Sector %2")
                              .arg(newSector.sector_id)
                              .arg(map->sectors[parentSectorIndex].sector_id));
    } else {
        map->sectors[newSectorIndex].parent_sector_id = -1;  // Root sector
        m_statusLabel->setText(tr("Caja creada (Sector %1) como sector raíz")
                              .arg(newSector.sector_id));
    }
    
    // NOTE: We do NOT create portals here
    // The motor will automatically detect this as a nested sector using AABB checks
    // in ray_detect_nested_sectors() and create portals if needed
    
    // Update UI
    updateSectorList();
    editor->update();
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
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;
    auto *map = editor->mapData();

    // Get currently selected item from tree
    QTreeWidgetItem *selectedItem = m_sectorTree->currentItem();
    if (!selectedItem) {
        QMessageBox::warning(this, tr("Sin selección"),
                           tr("Por favor selecciona un sector de la lista."));
        return;
    }
    
    int data = selectedItem->data(0, Qt::UserRole).toInt();
    if (data < 0) {
        QMessageBox::warning(this, tr("Selección inválida"),
                           tr("Por favor selecciona un sector, no un grupo."));
        return;
    }
    
    int selectedSectorId = data;
    
    int selectedIndex = -1;
    for (int i = 0; i < map->sectors.size(); i++) {
        if (map->sectors[i].sector_id == selectedSectorId) {
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
    
    for (int i = 0; i < map->sectors.size(); i++) {
        if (i == selectedIndex) continue;
        const Sector &sector = map->sectors[i];
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
        int oldParentId = map->sectors[selectedIndex].parent_sector_id;
        
        // Remove from old parent's children list
        if (oldParentId >= 0) {
            bool foundOldParent = false;
            for (int i = 0; i < map->sectors.size(); i++) {
                if (map->sectors[i].sector_id == oldParentId) {
                    map->sectors[i].child_sector_ids.removeAll(selectedSectorId);
                    foundOldParent = true;
                    break;
                }
            }
            if (!foundOldParent) {
                qWarning() << "Warning: Old parent sector" << oldParentId << "not found!";
            }
        }
        
        map->sectors[selectedIndex].parent_sector_id = newParentId;
        
        // Add to new parent's children list
        if (newParentId >= 0) {
            bool foundNewParent = false;
            for (int i = 0; i < map->sectors.size(); i++) {
                if (map->sectors[i].sector_id == newParentId) {
                    if (!map->sectors[i].child_sector_ids.contains(selectedSectorId)) {
                        map->sectors[i].child_sector_ids.append(selectedSectorId);
                    }
                    foundNewParent = true;
                    break;
                }
            }
            if (!foundNewParent) {
                QMessageBox::critical(this, tr("Error"),
                                    tr("No se pudo encontrar el sector padre %1!").arg(newParentId));
                // Revert the change
                map->sectors[selectedIndex].parent_sector_id = oldParentId;
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
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;
    auto *map = editor->mapData();

    // Update data directly and sync the other spinbox
    if (m_selectedSectorId >= 0 && m_selectedSectorId < map->sectors.size() &&
        m_selectedWallId >= 0 && m_selectedWallId < map->sectors[m_selectedSectorId].walls.size()) {
        
        map->sectors[m_selectedSectorId].walls[m_selectedWallId].texture_id_upper = val;
        
        // Sync the standard upper spinbox
        m_wallTextureUpperSpin->blockSignals(true);
        m_wallTextureUpperSpin->setValue(val);
        m_wallTextureUpperSpin->blockSignals(false);
        
        editor->update();
        updateVisualMode();
    }
}

void MainWindow::onPortalLowerChanged(int val)
{
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;
    auto *map = editor->mapData();

    if (m_selectedSectorId >= 0 && m_selectedSectorId < map->sectors.size() &&
        m_selectedWallId >= 0 && m_selectedWallId < map->sectors[m_selectedSectorId].walls.size()) {
        
        map->sectors[m_selectedSectorId].walls[m_selectedWallId].texture_id_lower = val;
        
        // Sync the standard lower spinbox
        m_wallTextureLowerSpin->blockSignals(true);
        m_wallTextureLowerSpin->setValue(val);
        m_wallTextureLowerSpin->blockSignals(false);
        
        editor->update();
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
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;
    editor->mapData()->skyTextureID = value;
    updateVisualMode();
}

void MainWindow::updateVisualMode()
{
    GridEditor *editor = getCurrentEditor();
    if (m_visualModeWidget && m_visualModeWidget->isVisible() && editor) {
        m_visualModeWidget->setMapData(*editor->mapData(), false); // false = Don't reset camera
    }
}

/* ============================================================================
   DECAL EDITING
   ============================================================================ */

void MainWindow::onDecalPlaced(float x, float y)
{
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;
    auto *map = editor->mapData();

    int mode = m_modeCombo->currentIndex();
    bool isFloor = (mode == 7); // "Colocar Decal Suelo"
    
    // Find which sector contains this point
    int targetSectorId = -1;
    for (int i = 0; i < map->sectors.size(); i++) {
        const Sector &sector = map->sectors[i];
        
        // Check if point is inside sector using ray casting algorithm
        bool inside = false;
        int j = sector.vertices.size() - 1;
        for (int k = 0; k < sector.vertices.size(); k++) {
            if (((sector.vertices[k].y() > y) != (sector.vertices[j].y() > y)) &&
                (x < (sector.vertices[j].x() - sector.vertices[k].x()) * 
                     (y - sector.vertices[k].y()) / 
                     (sector.vertices[j].y() - sector.vertices[k].y()) + 
                     sector.vertices[k].x())) {
                inside = !inside;
            }
            j = k;
        }
        
        if (inside) {
            targetSectorId = sector.sector_id;
            break;
        }
    }
    
    // Check if we found a sector
    if (targetSectorId < 0) {
        m_statusLabel->setText(tr("Error: Click dentro de un sector para colocar el decal"));
        return;
    }
    
    // Create new decal
    Decal decal;
    decal.id = map->getNextDecalId();
    decal.sector_id = targetSectorId;
    decal.is_floor = isFloor;
    decal.x = x;
    decal.y = y;
    decal.width = m_decalWidthSpin->value();
    decal.height = m_decalHeightSpin->value();
    decal.rotation = m_decalRotationSpin->value() * M_PI / 180.0f; // Degrees to radians
    decal.texture_id = m_decalTextureSpin->value();
    decal.alpha = m_decalAlphaSpin->value();
    decal.render_order = m_decalRenderOrderSpin->value();
    
    map->decals.append(decal);
    editor->update();
    
    // Auto-select the newly created decal and show properties panel
    m_selectedDecalId = decal.id;
    updateDecalPanel();
    m_decalDock->show();
    
    m_statusLabel->setText(tr("Decal %1 colocado en sector %2 en (%3, %4)")
                          .arg(decal.id).arg(targetSectorId).arg(x).arg(y));
}

void MainWindow::onDecalSelected(int decalId)
{
    m_selectedDecalId = decalId;
    updateDecalPanel();
    m_decalDock->show();
}

void MainWindow::onDecalXChanged(double value)
{
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;
    auto *map = editor->mapData();

    Decal *decal = map->findDecal(m_selectedDecalId);
    if (decal) {
        decal->x = value;
        editor->update();
    }
}

void MainWindow::onDecalYChanged(double value)
{
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;
    auto *map = editor->mapData();

    Decal *decal = map->findDecal(m_selectedDecalId);
    if (decal) {
        decal->y = value;
        editor->update();
    }
}

void MainWindow::onDecalWidthChanged(double value)
{
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;
    auto *map = editor->mapData();

    Decal *decal = map->findDecal(m_selectedDecalId);
    if (decal) {
        decal->width = value;
        editor->update();
    }
}

void MainWindow::onDecalHeightChanged(double value)
{
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;
    auto *map = editor->mapData();

    Decal *decal = map->findDecal(m_selectedDecalId);
    if (decal) {
        decal->height = value;
        editor->update();
    }
}

void MainWindow::onDecalRotationChanged(double value)
{
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;
    auto *map = editor->mapData();

    Decal *decal = map->findDecal(m_selectedDecalId);
    if (decal) {
        decal->rotation = value * M_PI / 180.0f; // Degrees to radians
        editor->update();
    }
}

void MainWindow::onDecalTextureChanged(int value)
{
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;
    auto *map = editor->mapData();

    Decal *decal = map->findDecal(m_selectedDecalId);
    if (decal) {
        decal->texture_id = value;
        editor->update();
    }
}

void MainWindow::onSelectDecalTexture()
{
    TextureSelector selector(m_textureCache, this);
    if (selector.exec() == QDialog::Accepted) {
        int textureId = selector.selectedTextureId();
        m_decalTextureSpin->setValue(textureId);
    }
}

void MainWindow::onDecalAlphaChanged(double value)
{
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;
    auto *map = editor->mapData();

    Decal *decal = map->findDecal(m_selectedDecalId);
    if (decal) {
        decal->alpha = value;
        editor->update();
    }
}

void MainWindow::onDecalRenderOrderChanged(int value)
{
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;
    auto *map = editor->mapData();

    Decal *decal = map->findDecal(m_selectedDecalId);
    if (decal) {
        decal->render_order = value;
        editor->update();
    }
}

void MainWindow::onDeleteDecal()
{
    GridEditor *editor = getCurrentEditor();
    if (!editor) return;
    auto *map = editor->mapData();

    for (int i = 0; i < map->decals.size(); i++) {
        if (map->decals[i].id == m_selectedDecalId) {
            map->decals.removeAt(i);
            m_selectedDecalId = -1;
            m_decalDock->hide();
            editor->update();
            m_statusLabel->setText(tr("Decal eliminado"));
            break;
        }
    }
}

void MainWindow::updateDecalPanel()
{
    GridEditor *editor = getCurrentEditor();
    if (!editor) {
        m_decalIdLabel->setText(tr("Ninguno"));
        return;
    }
    auto *map = editor->mapData();

    Decal *decal = map->findDecal(m_selectedDecalId);
    if (!decal) {
        m_decalIdLabel->setText(tr("Ninguno"));
        return;
    }
    
    // Block signals to avoid triggering updates
    m_decalXSpin->blockSignals(true);
    m_decalYSpin->blockSignals(true);
    m_decalWidthSpin->blockSignals(true);
    m_decalHeightSpin->blockSignals(true);
    m_decalRotationSpin->blockSignals(true);
    m_decalTextureSpin->blockSignals(true);
    m_decalAlphaSpin->blockSignals(true);
    m_decalRenderOrderSpin->blockSignals(true);
    
    m_decalIdLabel->setText(QString::number(decal->id));
    m_decalXSpin->setValue(decal->x);
    m_decalYSpin->setValue(decal->y);
    m_decalWidthSpin->setValue(decal->width);
    m_decalHeightSpin->setValue(decal->height);
    m_decalRotationSpin->setValue(decal->rotation * 180.0f / M_PI); // Radians to degrees
    m_decalTextureSpin->setValue(decal->texture_id);
    m_decalAlphaSpin->setValue(decal->alpha);
    m_decalRenderOrderSpin->setValue(decal->render_order);
    
    // Unblock signals
    m_decalXSpin->blockSignals(false);
    m_decalYSpin->blockSignals(false);
    m_decalWidthSpin->blockSignals(false);
    m_decalHeightSpin->blockSignals(false);
    m_decalRotationSpin->blockSignals(false);
    m_decalTextureSpin->blockSignals(false);
    m_decalAlphaSpin->blockSignals(false);
    m_decalRenderOrderSpin->blockSignals(false);
}

// FPG Editor slots
void MainWindow::onOpenFPGEditor()
{
    if (!m_fpgEditor) {
        m_fpgEditor = new FPGEditor(this);
        connect(m_fpgEditor, &FPGEditor::fpgReloaded, this, &MainWindow::onFPGReloaded);
    }
    
    // If we have a current FPG, load it
    if (!m_currentFPGPath.isEmpty()) {
        m_fpgEditor->setFPGPath(m_currentFPGPath);
        m_fpgEditor->loadFPG();
    }
    
    m_fpgEditor->show();
    m_fpgEditor->raise();
    m_fpgEditor->activateWindow();
}

void MainWindow::onFPGReloaded()
{
    // Reload FPG in main editor
    if (m_currentFPGPath.isEmpty()) return;
    
    QVector<TextureEntry> textures;
    bool success = FPGLoader::loadFPG(m_currentFPGPath, textures);
    
    if (success) {
        QMap<int, QPixmap> textureMap = FPGLoader::getTextureMap(textures);
        
        m_textureCache.clear();
        for (const TextureEntry &entry : textures) {
            m_textureCache[entry.id] = entry.pixmap;
        }

        // Apply to all editors
        for (int i=0; i<m_tabWidget->count(); i++) {
            GridEditor *editor = qobject_cast<GridEditor*>(m_tabWidget->widget(i));
            if (editor) {
                editor->mapData()->textures.clear();
                for (const TextureEntry &entry : textures) {
                    editor->mapData()->textures.append(entry);
                }
                editor->setTextures(textureMap);
            }
        }
        
        m_statusLabel->setText(tr("FPG reloaded: %1 textures").arg(textures.size()));
    }
}

void MainWindow::onOpenEffectGenerator()
{
    EffectGeneratorDialog *dialog = new EffectGeneratorDialog(this);
    dialog->exec();
    delete dialog;
}

void MainWindow::onOpenCameraPathEditor()
{
    GridEditor *editor = getCurrentEditor();
    if (editor) {
        CameraPathEditor *pathEditor = new CameraPathEditor(*editor->mapData(), this);
        pathEditor->exec();
        delete pathEditor;
    }
}

void MainWindow::onOpenMeshGenerator()
{
    MeshGeneratorDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        MeshGeneratorDialog::MeshParams params = dlg.getParameters();
        
        qDebug() << "=== MD3 Export Debug ===";
        qDebug() << "Texture paths count:" << params.texturePaths.size();
        for (int i = 0; i < params.texturePaths.size(); i++) {
            qDebug() << "  Texture" << i << ":" << params.texturePaths[i];
        }
        
        bool success = false;
        
        // Check if we have multiple textures - use atlas generation
        if (params.texturePaths.size() > 1) {
            qDebug() << "Using multi-texture atlas generation";
            
            // Generate mesh with multi-texture UVs
            MD3Generator::MeshData mesh = MD3Generator::generateMesh(
                (MD3Generator::MeshType)params.type,
                params.width, params.height, params.depth, params.segments,
                params.hasRailings, params.hasArch, (int)params.roofType
            );
            
            qDebug() << "Mesh generated with" << mesh.vertices.size() << "vertices";
            
            // Generate texture atlas
            QVector<QImage> textures = TextureAtlasGenerator::loadTextures(params.texturePaths);
            qDebug() << "Loaded" << textures.size() << "textures for atlas";
            
            if (!textures.isEmpty()) {
                QVector<TextureAtlasGenerator::AtlasRegion> uvRegions;
                QImage atlas = TextureAtlasGenerator::createAtlas(textures, uvRegions);
                
                qDebug() << "Atlas created:" << atlas.width() << "x" << atlas.height();
                
                // Save atlas as the main texture (same name as MD3 but .png)
                QFileInfo fi(params.exportPath);
                QString texturePath = fi.absolutePath() + "/" + fi.baseName() + ".png";
                
                qDebug() << "Attempting to save atlas to:" << texturePath;
                
                if (atlas.save(texturePath)) {
                    qDebug() << "✓ Atlas texture saved successfully";
                    // Save MD3
                    success = MD3Generator::saveMD3(mesh, params.exportPath);
                    qDebug() << "MD3 save result:" << success;
                } else {
                    qWarning() << "✗ Failed to save atlas texture to:" << texturePath;
                    success = false;
                }
            } else {
                qWarning() << "✗ Failed to load textures for atlas generation";
                success = false;
            }
        } else {
            qDebug() << "Using single texture export";
            // Single texture - use original method
            success = MD3Generator::generateAndSave(
                (MD3Generator::MeshType)params.type,
                params.width, params.height, params.depth, params.segments,
                params.texturePath, params.exportPath
            );
        }
        
        if (success) {
            QMessageBox::information(this, "Generador MD3", 
                                     QString("Modelo exportado correctamente a:\n%1").arg(params.exportPath));
        } else {
             QMessageBox::warning(this, "Generador MD3", 
                                  QString("Error al exportar modelo. Verifique la ruta y permisos."));
        }
    }
}

// Build System Slots
// Build System Slots are implemented in mainwindow_build.cpp

// void MainWindow::onGenerateCode() ... REMOVED logic or commented out
// Function body removed

/* ============================================================================
   TABBED INTERFACE IMPLEMENTATION
   ============================================================================ */

void MainWindow::onTabCloseRequested(int index)
{
    GridEditor *editor = qobject_cast<GridEditor*>(m_tabWidget->widget(index));
    if (!editor) return;

    // Check for unsaved changes?
    // For now, just close
    
    // If it's the last tab, maybe create a new empty one?
    m_tabWidget->removeTab(index);
    delete editor;
    
    if (m_tabWidget->count() == 0) {
        onNewMap(); // Create default new map
    }
}

void MainWindow::onTabChanged(int index)
{
    Q_UNUSED(index);
    updateWindowTitle();
    updateSectorList();
    updateSectorPanel();
    updateWallPanel();
    
    GridEditor *editor = getCurrentEditor();
    if (editor) {
        // Update visual mode widget if needed
    }
}

GridEditor* MainWindow::getCurrentEditor() const
{
    return qobject_cast<GridEditor*>(m_tabWidget->currentWidget());
}

void MainWindow::openMapFile(const QString &filename)
{
    // Check if already open?
    for (int i=0; i<m_tabWidget->count(); i++) {
        GridEditor *ed = qobject_cast<GridEditor*>(m_tabWidget->widget(i));
        if (ed && ed->fileName() == filename) {
             m_tabWidget->setCurrentIndex(i);
             return;
        }
    }

    GridEditor *editor = new GridEditor(const_cast<MainWindow*>(this)); // Parent is this
    if (RayMapFormat::loadMap(filename, *editor->mapData())) {
        editor->setFileName(filename);
        
        // Setup editor
        editor->setEditMode(static_cast<GridEditor::EditMode>(m_modeCombo->currentIndex()));
        if (m_viewGridAction) {
             editor->showGrid(m_viewGridAction->isChecked());
        }
        editor->setTextures(m_textureCache);
        
        // Connect signals
        connect(editor, &GridEditor::statusMessage, this, [this](const QString &msg) {
            m_statusLabel->setText(msg);
        });
        connect(editor, &GridEditor::wallSelected, this, &MainWindow::onWallSelected);
        connect(editor, &GridEditor::sectorSelected, this, &MainWindow::onSectorSelected);
        connect(editor, &GridEditor::decalPlaced, this, &MainWindow::onDecalPlaced);
        connect(editor, &GridEditor::cameraPlaced, this, &MainWindow::onCameraPlaced);
        connect(editor, &GridEditor::entitySelected, this, &MainWindow::onEntitySelected);
        connect(editor, &GridEditor::entityMoved, this, &MainWindow::onEntityChanged);
        
        m_tabWidget->addTab(editor, QFileInfo(filename).fileName());
        m_tabWidget->setCurrentWidget(editor);
        
        addToRecentMaps(filename);
        updateSectorList();
        updateWindowTitle();
        updateVisualMode();
        
        // Auto-load FPG with same base name if it exists
        QFileInfo mapInfo(filename);
        QString baseName = mapInfo.completeBaseName();
        QString mapDir = mapInfo.absolutePath();
        
        QStringList fpgPaths;
        fpgPaths << mapDir + "/" + baseName + ".fpg";
        fpgPaths << mapDir + "/" + baseName + ".map";
        
        if (m_projectManager && !m_projectManager->getProjectPath().isEmpty()) {
            QString projectPath = m_projectManager->getProjectPath();
            fpgPaths << projectPath + "/assets/fpg/" + baseName + ".fpg";
            fpgPaths << projectPath + "/assets/fpg/" + baseName + ".map";
        }
        
        bool fpgLoaded = false;
        for (const QString &fpgPath : fpgPaths) {
            if (QFile::exists(fpgPath)) {
                qDebug() << "Auto-loading FPG:" << fpgPath;
                
                QVector<TextureEntry> textures;
                bool success = FPGLoader::loadFPG(fpgPath, textures, 
                    [this](int current, int total, const QString &name) {
                        m_statusLabel->setText(tr("Loading FPG: %1/%2 - %3")
                                              .arg(current).arg(total).arg(name));
                        qApp->processEvents();
                    });
                
                if (success) {
                    m_textureCache.clear();
                    for (const TextureEntry &entry : textures) {
                        m_textureCache[entry.id] = entry.pixmap;
                    }
                    
                    editor->setTextures(m_textureCache);
                    
                    addToRecentFPGs(fpgPath);
                    m_currentFPGPath = fpgPath;
                    fpgLoaded = true;
                    qDebug() << "Auto-loaded FPG:" << fpgPath << "with" << textures.size() << "textures";
                }
                break;
            }
        }
        
        if (fpgLoaded) {
            m_statusLabel->setText(tr("Mapa y FPG cargados: %1").arg(filename));
        } else {
            m_statusLabel->setText(tr("Mapa cargado: %1").arg(filename));
        }
    } else {
        delete editor;
        QMessageBox::critical(const_cast<MainWindow*>(this), tr("Error"), tr("No se pudo cargar el mapa %1").arg(filename));
    }
}

/* ============================================================================
   CODE EDITOR INTEGRATION
   ============================================================================ */

void MainWindow::onOpenCodeEditor(const QString &filePath)
{
    if (!m_codeEditorDialog) {
        m_codeEditorDialog = new CodeEditorDialog(this);
    }
    
    m_codeEditorDialog->show();
    m_codeEditorDialog->raise();
    m_codeEditorDialog->activateWindow();
    
    if (!filePath.isEmpty()) {
        m_codeEditorDialog->openFile(filePath);
    }
}

void MainWindow::onCodePreviewOpenRequested(const QString &filePath)
{
    onOpenCodeEditor(filePath);
}

void MainWindow::onEntitySelected(int index, EntityInstance entity)
{
    if (m_entityPanel) {
        m_entityPanel->setEntity(index, entity);
        // Ensure entity dock is visible?
        // m_entityDock->show(); 
    }
}

void MainWindow::onEntityChanged(int index, EntityInstance entity)
{
    GridEditor *editor = getCurrentEditor();
    if (editor) {
        editor->updateEntity(index, entity);
        editor->update(); // Redraw grid
        
        // Update visual mode
        if (m_visualModeWidget && m_visualModeWidget->isVisible()) {
             m_visualModeWidget->setMapData(*editor->mapData(), false); // false = don't reset camera
        }
    }
}


void MainWindow::openObjConverter()
{
    ObjImportDialog dialog(this);
    dialog.exec();
}
