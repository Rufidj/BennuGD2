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
#include <QListWidget>
#include <QDockWidget>
#include "mapdata.h"
#include "grideditor.h"
#include "textureselector.h"
#include "visualmodewidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    
private slots:
    // File menu
    void onNewMap();
    void onOpenMap();
    void onSaveMap();
    void onSaveMapAs();
    void onImportWLD();  // Import WLD file
    void onLoadFPG();
    void onExit();
    
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
    
    // Nested sector editing (NEW)
    void onSectorParentChanged(int index);
    void onSectorTypeChanged(int index);
    void onSectorSolidChanged(bool checked);
    
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
    void onDetectPortals();
    void onCameraPlaced(float x, float y);
    void onTextureSelected(int textureId);
    void onCreateRectangle();
    void onCreateCircle();
    
    // Sector list (NEW)
    void onSectorListItemClicked(QListWidgetItem *item);
    void updateSectorList();  // Refresh the sector list
    void onSectorListContextMenu(const QPoint &pos); // Context menu slot
    
    // Sector operations
    void deleteSelectedSector();
    void copySelectedSector();
    void pasteSector();
    void moveSelectedSector();
    
private:
    // UI Components
    GridEditor *m_gridEditor;
    VisualModeWidget *m_visualModeWidget;
    // Texture cache for selector
    QMap<int, QPixmap> m_textureCache;
    
    // Dock widgets
    QDockWidget *m_sectorDock;
    QDockWidget *m_wallDock;
    QDockWidget *m_sectorListDock;  // NEW: List of all sectors
    
    // Sector list
    QListWidget *m_sectorListWidget;  // NEW
    
    // Sector controls
    QLabel *m_sectorIdLabel;
    QDoubleSpinBox *m_sectorFloorZSpin;
    QDoubleSpinBox *m_sectorCeilingZSpin;
    QSpinBox *m_sectorFloorTextureSpin;
    QSpinBox *m_sectorCeilingTextureSpin;
    
    // Nested sector controls (NEW)
    QComboBox *m_sectorParentCombo;
    QComboBox *m_sectorTypeCombo;
    QCheckBox *m_sectorSolidCheckbox;
    QLabel *m_sectorNestingLevelLabel;
    
    // Wall controls
    QLabel *m_wallIdLabel;
    QSpinBox *m_wallTextureLowerSpin;
    QSpinBox *m_wallTextureMiddleSpin;
    QSpinBox *m_wallTextureUpperSpin;
    QDoubleSpinBox *m_wallSplitLowerSpin;
    QDoubleSpinBox *m_wallSplitUpperSpin;
    
    // Toolbar
    QComboBox *m_modeCombo;
    QSpinBox *m_selectedTextureSpin;
    QSpinBox *m_skyboxSpin;
    QPushButton *m_detectPortalsButton;
    
    // Status bar
    QLabel *m_statusLabel;
    
    // Data
    MapData m_mapData;
    QString m_currentMapFile;
    int m_currentFPG;
    int m_selectedSectorId;
    int m_selectedWallId;
    
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
    
    QAction *m_zoomInAction;
    QAction *m_zoomOutAction;
    QAction *m_zoomResetAction;
    QAction *m_visualModeAction;
    
    // Helpers
    void updateWindowTitle();
    void updateSectorPanel();
    void updateWallPanel();
};

#endif // MAINWINDOW_H
