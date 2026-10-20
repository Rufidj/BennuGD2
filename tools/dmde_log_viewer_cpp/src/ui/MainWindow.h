#pragma once

#include <QMainWindow>
#include <QModelIndex>
#include <QFileInfo>
#include <QVector>

#include "core/DiskDoctor.h"
#include "core/DiskHealthReport.h"
#include "core/RecoveryNode.h"
#include "core/ScanProfile.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QResizeEvent;
class QSortFilterProxyModel;
class QStandardItem;
class QTextEdit;
class QTreeView;
class RecoveryTreeModel;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void loadDemoData();
    void openDiskDevice();
    void openLogDocument();
    void openJsonDocument();
    void saveSessionDocument();
    void analyzeVolumeImage();
    void diagnoseCurrentSource();
    void diagnoseAnotherSource();
    void showRepairAdvisor();
    void showDiskHealthDashboard();
    void exportDiskHealthJson();
    void exportDiskHealthHtml();
    void runQuickSurfaceScan();
    void runFullSurfaceScan();
    void runSmartProbe();
    void runSuperFastScan();
    void exploreDetectedVolume();
    void startMetadataScan();
    void startSignatureScan();
    void recoverCheckedNodes();
    void showSupportMatrix();
    void showEnginePlan();
    void updateDetailsFromSelection();
    void filterChanged(const QString &text);
    void toggleBranchFromContextMenu(bool checked);
    void updateScanProfile();

private:
    RecoveryTreeModel *treeModel_ = nullptr;
    QSortFilterProxyModel *proxyModel_ = nullptr;
    QTreeView *treeView_ = nullptr;
    QLineEdit *filterEdit_ = nullptr;
    QComboBox *libraryCombo_ = nullptr;
    QComboBox *statusCombo_ = nullptr;
    QComboBox *modeCombo_ = nullptr;
    QComboBox *fileSystemCombo_ = nullptr;
    QLabel *selectionLabel_ = nullptr;
    QLabel *profileLabel_ = nullptr;
    QLabel *previewLabel_ = nullptr;
    QString sourcePath_;
    FileSystemKind detectedFileSystem_ = FileSystemKind::AutoDetect;
    QStringList checkedPaths_;
    DiskHealthReport lastDiskHealthReport_;
    QTextEdit *summaryText_ = nullptr;
    QTextEdit *detailText_ = nullptr;
    QModelIndex contextIndex_;

    void buildUi();
    void buildMenuAndToolbar();
    void populateDemo();
    QVector<RecoveryNode> demoNodes() const;
    ScanProfile currentProfile(ScanStrategy strategy = ScanStrategy::Metadata) const;
    FileSystemKind currentFileSystemKind() const;
    ScanMode currentScanMode() const;
    void applyDetectedFileSystem(FileSystemKind fileSystem);
    QString currentScanModeLabel() const;
    QString currentFileSystemLabel() const;
    QString profileSummaryText(ScanStrategy strategy = ScanStrategy::Metadata) const;
    QString enginePlanText(const ScanProfile &profile) const;
    QVector<RecoveryNode> collectCurrentRoots() const;
    QStringList collectCheckedPaths() const;
    void updatePreviewForNode(const RecoveryNode &node);
    void showPreviewText(const QString &title, const QString &body);
    QString buildUserFacingFileFacts(const RecoveryNode &node, const QFileInfo &info) const;
    void updateSelectionCount();
    void setBranchCheckState(QStandardItem *item, bool checked);
    int setDirectFilesCheckState(QStandardItem *item, bool checked);
    void markRecoveredNodes(const QVector<RecoveryNode> &nodes);
    void restoreCheckedPaths();
    bool offerAdminRelaunch(const QString &devicePath, const QString &reason);
    void runDiagnosisWorkflow(const QString &sourcePath, bool updateCurrentSource);
    void showDiagnosisNextSteps(const DiskDoctorResult &result);
    void resizeEvent(QResizeEvent *event) override;
};

