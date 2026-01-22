#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QCheckBox>
#include <QGroupBox>
#include <QListWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QDockWidget>
#include "mapdata.h"
#include "grideditor.h"
#include "codeeditordialog.h"
#include "codepreviewpanel.h"
#include "consolewidget.h"
#include "textureselector.h"
#include "visualmodewidget.h"
#include "fpgeditor.h"
#include "entitypropertypanel.h"

class BuildManager;
class ProjectManager;
class AssetBrowser;

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    
private slots:
    // ... existing slots ...

    // Code Editor Integration
    void onOpenCodeEditor(const QString &filePath = QString());
    void onCodePreviewOpenRequested(const QString &filePath);
    
    // File menu
    void onNewMap();
    void onOpenMap();
    void onSaveMap();
    void onSaveMapAs();
    void onImportWLD();  // Import WLD file
    void onLoadFPG();
    void onExit();
    void openRecentMap();
    void openRecentFPG();
    
    // Tabs
    void onTabCloseRequested(int index);
    void onTabChanged(int index);
    void openMapFile(const QString &filePath); // Renamed from onOpenMap to avoid ambiguity
    
    // Tools menu
    void onOpenFPGEditor();
    void onFPGReloaded();
    void onOpenEffectGenerator();
    void onOpenCameraPathEditor();
    void onOpenMeshGenerator(); // NEW: MD3 Generator
    
    // Project Management
    void onNewProject();
    void onOpenProject();
    void onCloseProject();
    void onProjectSettings();
    void onPublishProject();

    // Build System
    void onBuildProject();
    void onRunProject();
    void onBuildAndRun();
    void onStopRunning();
    void onInstallBennuGD2();
    void onConfigureBennuGD2();
    void setupBuildSystem();
    void onGenerateCode();

    // View menu
    void onZoomIn();
    void onZoomOut();
    void onZoomReset();
    void onToggleVisualMode();
    
    // Edit mode
    void onModeChanged(int index);
    
    // Sector editing
    void onSectorSelected(int sectorId);
    void onWallSelected(int sectorIndex, int wallIndex);
    void onVertexSelected(int sectorId, int vertexIndex);
    void onSectorFloorZChanged(double value);
    void onSectorCeilingZChanged(double value);
    void onSectorFloorTextureChanged(int value);
    void onSectorCeilingTextureChanged(int value);
    void onSelectSectorFloorTexture();
    void onSelectSectorCeilingTexture();
    

    
    // Wall editing
    void onWallTextureLowerChanged(int value);
    void onWallTextureMiddleChanged(int value);
    void onWallTextureUpperChanged(int value);
    void onSelectWallTextureLower();
    void onSelectWallTextureMiddle();
    void onSelectWallTextureUpper();
    void onApplyTextureToAllWalls();  // Apply current wall texture to all walls in sector
    void onWallSplitLowerChanged(double value);
    void onWallSplitUpperChanged(double value);
    
    // Tools
    void onDetectPortals(); // RESTORED
    void onToggleManualPortals(bool checked); 
    void onManualPortalWallSelected(int sectorIndex, int wallIndex);
    void onDeletePortal(int sectorIndex, int wallIndex); // NEW
    void onCameraPlaced(float x, float y);
    void onTextureSelected(int textureId);
    void onCreateRectangle();
    void onCreateCircle();
    
    // Insert Tools
    void onInsertBox();
    void onInsertColumn();
    void onInsertPlatform();
    void onInsertDoor();        // Future
    void onInsertElevator();    // Future
    void onInsertStairs();      // Future
    // void onGenerateRamp();      // REMOVED
    
    void onSetParentSector();  // Assign parent sector to selected sector    // Future
    
    // Sector tree (hierarchical)
    void onSectorTreeItemClicked(QTreeWidgetItem *item, int column);
    void onSectorTreeItemDoubleClicked(QTreeWidgetItem *item, int column);
    
    // Entity selection
    void onEntitySelected(int index, EntityInstance entity); // NEW
    void onEntityChanged(int index, EntityInstance entity); // NEW
    
    void updateSectorList();  // Refresh the sector tree
    void onSectorTreeContextMenu(const QPoint &pos); // Context menu slot
    
    // Sector operations
    void deleteSelectedSector();
    void copySelectedSector();
    void pasteSector();
    void moveSelectedSector();
    
    // Visual Mode Update
    void updateVisualMode();
    
    // Recent files helpers
    void updateRecentMapsMenu();
    void updateRecentFPGsMenu();
    void addToRecentMaps(const QString &filename);
    void addToRecentFPGs(const QString &filename);
    
    // Decal editing
    void onDecalPlaced(float x, float y);
    void onDecalSelected(int decalId);
    void onDecalXChanged(double value);
    void onDecalYChanged(double value);
    void onDecalWidthChanged(double value);
    void onDecalHeightChanged(double value);
    void onDecalRotationChanged(double value);
    void onDecalTextureChanged(int value);
    void onSelectDecalTexture();
    void onDecalAlphaChanged(double value);
    void onDecalRenderOrderChanged(int value);
    void onDeleteDecal();

    // Dark Mode
    void onToggleDarkMode(bool checked);
    
    // Tools
    void openObjConverter();
    void loadSettings();
    void saveSettings();
    
private:
    // UI Components
    QTabWidget *m_tabWidget; // Replaces m_gridEditor
    GridEditor* getCurrentEditor() const; // Helper to get current tab
    
    VisualModeWidget *m_visualModeWidget;

    // Console
    ConsoleWidget *m_consoleWidget;
    QDockWidget *m_consoleDock;

    // Code Preview
    CodePreviewPanel *m_codePreviewPanel;
    QDockWidget *m_codePreviewDock;
    
    // Code Editor Window
    CodeEditorDialog *m_codeEditorDialog;
    
    // Build System
    BuildManager *m_buildManager;

    // Project System
    ProjectManager *m_projectManager;
    AssetBrowser *m_assetBrowser;
    QDockWidget *m_assetDock;

    // Texture cache for selector
    QMap<int, QPixmap> m_textureCache;
    
    // Property panels
    QWidget *m_sectorPanel;
    QWidget *m_wallPanel;
    EntityPropertyPanel *m_entityPanel; // NEW
    
    // Dock widgets
    QDockWidget *m_sectorDock;
    QDockWidget *m_wallDock;
    QDockWidget *m_sectorListDock;  // NEW: List of all sectors
    
    // Sector tree (hierarchical with groups)
    QTreeWidget *m_sectorTree;  // NEW: Hierarchical sector tree
    
    // Sector controls
    QLabel *m_sectorIdLabel;
    QDoubleSpinBox *m_sectorFloorZSpin;
    QDoubleSpinBox *m_sectorCeilingZSpin;
    QSpinBox *m_sectorFloorTextureSpin;
    QSpinBox *m_sectorCeilingTextureSpin;
    

    
    // Wall controls
    QLabel *m_wallIdLabel;
    QSpinBox *m_wallTextureLowerSpin;
    QSpinBox *m_wallTextureMiddleSpin;
    QSpinBox *m_wallTextureUpperSpin;
    QDoubleSpinBox *m_wallSplitLowerSpin;
    QDoubleSpinBox *m_wallSplitUpperSpin;
    
    // Decal controls
    QDockWidget *m_decalDock;
    QLabel *m_decalIdLabel;
    QDoubleSpinBox *m_decalXSpin;
    QDoubleSpinBox *m_decalYSpin;
    QDoubleSpinBox *m_decalWidthSpin;
    QDoubleSpinBox *m_decalHeightSpin;
    QDoubleSpinBox *m_decalRotationSpin;
    QSpinBox *m_decalTextureSpin;
    QDoubleSpinBox *m_decalAlphaSpin;
    QSpinBox *m_decalRenderOrderSpin;
    QPushButton *m_deleteDecalButton;
    
    // Toolbar
    QComboBox *m_modeCombo;
    QSpinBox *m_selectedTextureSpin;
    QSpinBox *m_skyboxSpin;
    QPushButton *m_manualPortalsButton; // REPLACED
    
    // Status bar
    QLabel *m_statusLabel;
    
    // Tools
    FPGEditor *m_fpgEditor;
    
    // Data
    // MapData m_mapData; // MOVED TO GRIDEDITOR
    // QString m_currentMapFile; // MOVED TO TAB PROPERTY
    QString m_currentFPGPath;
    int m_currentFPG;
    int m_selectedSectorId;
    int m_selectedWallId;
    int m_selectedDecalId;
    
    // Slots for Skybox
    void onSkyboxTextureChanged(int value);
    
    // Manual Portal State
    int m_pendingPortalSector = -1;
    int m_pendingPortalWall = -1;
    
    // Portal UI Elements (Special Request)
    QGroupBox *m_portalTexGroup;
    QSpinBox *m_portalUpperSpin;
    QSpinBox *m_portalLowerSpin;
    
private slots:
    void onPortalUpperChanged(int val);
    void onPortalLowerChanged(int val);
    void onSelectPortalUpper();
    void onSelectPortalLower();
    
private:
    // Clipboard
    Sector m_clipboardSector;
    bool m_hasClipboard = false;
    
    // UI Setup
    void createActions();
    void createMenus();
    void createToolbars();
    void createDockWindows();
    void createStatusBar();
    
    // Actions
    QAction *m_newAction;
    QAction *m_openAction;
    QAction *m_saveAction;
    QAction *m_saveAsAction;
    QAction *m_loadFPGAction;
    QAction *m_exitAction;
    
    // Recent files menus
    QMenu *m_recentMapsMenu;
    QMenu *m_recentFPGsMenu;
    
    QAction *m_zoomInAction;
    QAction *m_zoomOutAction;
    QAction *m_zoomResetAction;
    QAction *m_viewGridAction; // NEW: Replaces m_newAction confusion? Or was it missing?
    QAction *m_visualModeAction;
    
    // Insert Tools
    QAction *m_insertBoxAction;
    QAction *m_insertColumnAction;
    QAction *m_insertPlatformAction;
    QAction *m_insertDoorAction;        // Future
    QAction *m_insertElevatorAction;    // Future
    QAction *m_insertStairsAction;      // Future
    
    // Helpers
    void updateWindowTitle();
    void updateSectorPanel();
    void updateWallPanel();
    void updateDecalPanel();
};

#endif // MAINWINDOW_H
