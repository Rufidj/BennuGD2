#include "MainWindow.h"

#include <algorithm>
#include <QAction>
#include <QCoreApplication>
#include <QComboBox>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QImageReader>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPixmap>
#include <QProgressDialog>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QResizeEvent>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStandardPaths>
#include <QStandardItem>
#include <QStatusBar>
#include <QSet>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidget>

#ifdef Q_OS_LINUX
#include <unistd.h>
#endif

#include "core/RecoveryTreeModel.h"
#include "core/DiskDoctor.h"
#include "core/DiskHealthReport.h"
#include "core/DiskHealthReportExporter.h"
#include "core/RecoveryExtractor.h"
#include "core/RepairAdvisor.h"
#include "core/ScanEngine.h"
#include "core/LogParser.h"
#include "core/ScanPlanner.h"
#include "core/SessionLoader.h"
#include "core/SessionExporter.h"
#include "core/SmartProbe.h"
#include "core/SurfaceScanner.h"
#include "core/VolumeExplorer.h"
#include "core/VolumeProbe.h"

namespace {
class PathFilterProxyModel final : public QSortFilterProxyModel {
public:
    explicit PathFilterProxyModel(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {}

    void setLibraryFilter(const QString &library) {
        libraryFilter_ = library.trimmed();
        invalidateFilter();
    }

    void setStatusFilter(const QString &status) {
        statusFilter_ = status.trimmed();
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override {
        if (!sourceModel()) {
            return false;
        }

        const QModelIndex pathIndex = sourceModel()->index(sourceRow, 0, sourceParent);
        const QString text = sourceModel()->data(pathIndex).toString();
        const QString typeText = sourceModel()->data(sourceModel()->index(sourceRow, 1, sourceParent)).toString();
        const QString needle = filterRegularExpression().pattern().trimmed();
        const RecoveryNode node = sourceModel()->data(pathIndex, Qt::UserRole + 1).value<RecoveryNode>();

        const bool libraryMatches = matchesLibrary(node);
        const bool statusMatches = matchesStatus(node);

        const bool textMatches =
            needle.isEmpty() ||
            text.contains(needle, Qt::CaseInsensitive) ||
            typeText.contains(needle, Qt::CaseInsensitive) ||
            node.path.contains(needle, Qt::CaseInsensitive);

        if (libraryMatches && statusMatches && textMatches) {
            return true;
        }

        const int childCount = sourceModel()->rowCount(pathIndex);
        for (int row = 0; row < childCount; ++row) {
            if (filterAcceptsRow(row, pathIndex)) {
                return true;
            }
        }
        return false;
    }

    bool lessThan(const QModelIndex &sourceLeft, const QModelIndex &sourceRight) const override {
        const RecoveryNode leftNode = sourceModel()->data(sourceLeft.sibling(sourceLeft.row(), 0), Qt::UserRole + 1).value<RecoveryNode>();
        const RecoveryNode rightNode = sourceModel()->data(sourceRight.sibling(sourceRight.row(), 0), Qt::UserRole + 1).value<RecoveryNode>();

        switch (sourceLeft.column()) {
        case RecoveryTreeModel::PathColumn:
            return QString::localeAwareCompare(leftNode.name, rightNode.name) < 0;
        case RecoveryTreeModel::TypeColumn:
            return QString::localeAwareCompare(leftNode.typeLabel, rightNode.typeLabel) < 0;
        case RecoveryTreeModel::StatusColumn:
            if (leftNode.deleted != rightNode.deleted) {
                return !leftNode.deleted && rightNode.deleted;
            }
            return QString::localeAwareCompare(leftNode.name, rightNode.name) < 0;
        case RecoveryTreeModel::SizeColumn:
            if (leftNode.sizeBytes != rightNode.sizeBytes) {
                return leftNode.sizeBytes < rightNode.sizeBytes;
            }
            return QString::localeAwareCompare(leftNode.name, rightNode.name) < 0;
        case RecoveryTreeModel::LinesColumn:
            if (leftNode.lineCount != rightNode.lineCount) {
                return leftNode.lineCount < rightNode.lineCount;
            }
            return QString::localeAwareCompare(leftNode.name, rightNode.name) < 0;
        default:
            return QSortFilterProxyModel::lessThan(sourceLeft, sourceRight);
        }
    }

private:
    QString libraryFilter_ = "Todos";
    QString statusFilter_ = "Todos";

    static QString detectCategory(const RecoveryNode &node) {
        const QString lower = node.name.toLower();
        if (lower.endsWith(".jpg") || lower.endsWith(".jpeg") || lower.endsWith(".png") ||
            lower.endsWith(".gif") || lower.endsWith(".bmp") || lower.endsWith(".webp")) {
            return "Imagenes";
        }
        if (lower.endsWith(".mp4") || lower.endsWith(".mov") || lower.endsWith(".avi") ||
            lower.endsWith(".mkv") || lower.endsWith(".mts") || lower.endsWith(".mpeg") ||
            lower.endsWith(".mp3") || lower.endsWith(".wav") || lower.endsWith(".flac") ||
            lower.endsWith(".ogg")) {
            return "Videos";
        }
        if (lower.endsWith(".pdf") || lower.endsWith(".doc") || lower.endsWith(".docx") ||
            lower.endsWith(".txt") || lower.endsWith(".xml") || lower.endsWith(".json") ||
            lower.endsWith(".csv") || lower.endsWith(".md")) {
            return "Documentos";
        }
        if (lower.endsWith(".zip") || lower.endsWith(".rar") || lower.endsWith(".7z") ||
            lower.endsWith(".gz") || lower.endsWith(".tar")) {
            return "Comprimidos";
        }
        return "Otros";
    }

    bool matchesLibrary(const RecoveryNode &node) const {
        if (libraryFilter_.isEmpty() || libraryFilter_ == "Todos") {
            return true;
        }
        if (node.kind == RecoveryNodeKind::Folder) {
            return false;
        }
        return detectCategory(node) == libraryFilter_;
    }

    bool matchesStatus(const RecoveryNode &node) const {
        if (statusFilter_.isEmpty() || statusFilter_ == "Todos") {
            return true;
        }
        if (node.kind == RecoveryNodeKind::Folder) {
            return false;
        }
        if (statusFilter_ == "Activos") {
            return !node.deleted;
        }
        if (statusFilter_ == "Borrados") {
            return node.deleted;
        }
        if (statusFilter_ == "Recuperables") {
            return node.kind == RecoveryNodeKind::File && node.sizeBytes > 0;
        }
        return true;
    }
};

bool isTransparentFolderNode(const RecoveryNode &node) {
    return node.kind == RecoveryNodeKind::Folder &&
           (node.name == "." ||
            node.name.compare("Root", Qt::CaseInsensitive) == 0 ||
            node.name.compare("NTFS reconstruido", Qt::CaseInsensitive) == 0 ||
            node.name.compare("Archivos y carpetas", Qt::CaseInsensitive) == 0 ||
            node.name.compare("Biblioteca de recuperacion", Qt::CaseInsensitive) == 0);
}

void collectCheckedTopLevelNodes(QStandardItem *item, QVector<RecoveryNode> &nodes) {
    if (!item) {
        return;
    }
    const RecoveryNode node = item->data(Qt::UserRole + 1).value<RecoveryNode>();
    const bool transparentFolder = isTransparentFolderNode(node);

    if (item->isCheckable() && item->checkState() == Qt::Checked && !transparentFolder) {
        nodes.append(node);
        return;
    }
    for (int row = 0; row < item->rowCount(); ++row) {
        collectCheckedTopLevelNodes(item->child(row, 0), nodes);
    }
}

bool buildVisibleCheckedNodeFromProxy(
    QSortFilterProxyModel *proxyModel,
    RecoveryTreeModel *treeModel,
    const QModelIndex &proxyIndex,
    RecoveryNode &outNode
) {
    if (!proxyModel || !treeModel || !proxyIndex.isValid()) {
        return false;
    }

    const QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
    QStandardItem *item = treeModel->itemFromIndex(sourceIndex);
    if (!item) {
        return false;
    }

    const RecoveryNode sourceNode = item->data(Qt::UserRole + 1).value<RecoveryNode>();
    const bool checked = item->isCheckable() && item->checkState() == Qt::Checked;
    const bool transparent = isTransparentFolderNode(sourceNode);

    QVector<RecoveryNode> visibleChildren;
    const int rowCount = proxyModel->rowCount(proxyIndex);
    for (int row = 0; row < rowCount; ++row) {
        const QModelIndex childProxyIndex = proxyModel->index(row, RecoveryTreeModel::PathColumn, proxyIndex);
        RecoveryNode childNode;
        if (buildVisibleCheckedNodeFromProxy(proxyModel, treeModel, childProxyIndex, childNode)) {
            visibleChildren.append(childNode);
        }
    }

    if (sourceNode.kind == RecoveryNodeKind::File) {
        if (!checked || transparent) {
            return false;
        }
        outNode = sourceNode;
        outNode.children.clear();
        return true;
    }

    if (transparent) {
        if (visibleChildren.isEmpty()) {
            return false;
        }
        outNode = sourceNode;
        outNode.children = visibleChildren;
        return true;
    }

    if (!checked && visibleChildren.isEmpty()) {
        return false;
    }

    outNode = sourceNode;
    outNode.children = visibleChildren;
    return true;
}

std::uint64_t estimateSourceBytes(const QString &sourcePath) {
    const VolumeProbeResult probe = VolumeProbe::probe(sourcePath);
    if (probe.blockOrSectorSize > 0 && probe.totalUnits > 0) {
        return probe.blockOrSectorSize * probe.totalUnits;
    }
    QFileInfo info(sourcePath);
    if (info.exists() && info.isFile()) {
        return static_cast<std::uint64_t>(info.size());
    }
    return 0;
}

QString formatBytesProgress(std::uint64_t sizeBytes) {
    static const char *units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    double value = static_cast<double>(sizeBytes);
    int unitIndex = 0;
    while (value >= 1024.0 && unitIndex < 5) {
        value /= 1024.0;
        ++unitIndex;
    }
    if (unitIndex == 0) {
        return QString::number(static_cast<qulonglong>(value)) + " " + units[unitIndex];
    }
    return QString::number(value, 'f', 1) + " " + units[unitIndex];
}

QString formatDurationCompact(qint64 totalSeconds) {
    if (totalSeconds < 0) {
        totalSeconds = 0;
    }
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;
    if (hours > 0) {
        return QString("%1h %2m %3s").arg(hours).arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
    }
    if (minutes > 0) {
        return QString("%1m %2s").arg(minutes).arg(seconds, 2, 10, QChar('0'));
    }
    return QString::number(seconds) + "s";
}

struct BlockDeviceChoice {
    QString path;
    QString label;
    QString fsType;
    QString mountPoint;
    QString type;
};

QList<BlockDeviceChoice> availableBlockDevices() {
    QList<BlockDeviceChoice> devices;
    QProcess process;
    process.start("lsblk", {"-ln", "-b", "-o", "PATH,SIZE,FSTYPE,TYPE,MOUNTPOINT,MODEL"});
    if (!process.waitForFinished(2500) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return devices;
    }

    const QStringList lines =
        QString::fromUtf8(process.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        const QStringList parts = trimmed.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() < 4) {
            continue;
        }
        const QString path = parts.at(0);
        const qulonglong sizeBytes = parts.at(1).toULongLong();
        const QString fsType = parts.at(2);
        const QString type = parts.at(3);
        if (!path.startsWith("/dev/")) {
            continue;
        }
        if (type != "disk" && type != "part") {
            continue;
        }
        QString mountPoint = parts.size() >= 5 ? parts.at(4) : QString();
        QString model;
        if (parts.size() >= 6) {
            QStringList modelParts = parts.mid(5);
            model = modelParts.join(' ');
        }

        QString label = path + " | " + formatBytesProgress(sizeBytes) + " | " + type;
        if (!fsType.isEmpty()) {
            label += " | " + fsType;
        } else {
            label += " | sin FS detectado";
        }
        if (!mountPoint.isEmpty() && mountPoint != "-") {
            label += " | " + mountPoint;
        } else {
            label += " | no montado";
        }
        if (!model.isEmpty()) {
            label += " | " + model;
        }
        devices.append({path, label, fsType, mountPoint, type});
    }

    std::sort(devices.begin(), devices.end(), [](const BlockDeviceChoice &left, const BlockDeviceChoice &right) {
        const bool leftDisk = left.label.contains("| disk");
        const bool rightDisk = right.label.contains("| disk");
        if (leftDisk != rightDisk) {
            return leftDisk && !rightDisk;
        }
        return left.path < right.path;
    });
    return devices;
}

bool isImageFileName(const QString &name) {
    const QString lower = name.toLower();
    return lower.endsWith(".jpg") || lower.endsWith(".jpeg") || lower.endsWith(".png") ||
           lower.endsWith(".gif") || lower.endsWith(".bmp") || lower.endsWith(".webp");
}

bool isTextLikeFileName(const QString &name) {
    const QString lower = name.toLower();
    return lower.endsWith(".txt") || lower.endsWith(".log") || lower.endsWith(".json") ||
           lower.endsWith(".xml") || lower.endsWith(".csv") || lower.endsWith(".md");
}

QString expectedFsToken(FileSystemKind kind) {
    switch (kind) {
    case FileSystemKind::NTFS:
        return "ntfs";
    case FileSystemKind::FAT32:
        return "vfat";
    case FileSystemKind::ExFAT:
        return "exfat";
    case FileSystemKind::EXT4:
        return "ext4";
    case FileSystemKind::APFS:
        return "apfs";
    case FileSystemKind::HFSPlus:
        return "hfsplus";
    default:
        return QString();
    }
}

QString resolveRecoverySourcePath(const QString &sourcePath, FileSystemKind fileSystem) {
    if (!sourcePath.startsWith("/dev/")) {
        return sourcePath;
    }

    const QString fileName = QFileInfo(sourcePath).fileName();
    if (!fileName.isEmpty() && fileName.back().isDigit()) {
        return sourcePath;
    }

    QProcess process;
    process.start("lsblk", {"-ln", "-o", "PATH,FSTYPE,TYPE", sourcePath});
    if (!process.waitForFinished(1500) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return sourcePath;
    }

    const QString desiredFs = expectedFsToken(fileSystem);
    const QStringList lines =
        QString::fromUtf8(process.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts);

    QString firstPartition;
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        const QStringList parts = trimmed.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() < 3) {
            continue;
        }
        const QString path = parts.at(0);
        const QString fs = parts.at(1).toLower();
        const QString type = parts.at(2).toLower();
        if (path == sourcePath || type != "part") {
            continue;
        }
        if (firstPartition.isEmpty()) {
            firstPartition = path;
        }
        if (!desiredFs.isEmpty() && fs == desiredFs) {
            return path;
        }
    }

    return firstPartition.isEmpty() ? sourcePath : firstPartition;
}

struct EffectiveScanSource {
    QString path;
    FileSystemKind fileSystem = FileSystemKind::AutoDetect;
    QString note;
};

EffectiveScanSource determineEffectiveScanSource(const QString &sourcePath, FileSystemKind preferredFileSystem) {
    EffectiveScanSource result;
    result.path = sourcePath;
    result.fileSystem = preferredFileSystem;

    auto tryProbe = [&](const QString &candidatePath) -> bool {
        if (candidatePath.isEmpty()) {
            return false;
        }
        const VolumeProbeResult probe = VolumeProbe::probe(candidatePath);
        if (!probe.success || probe.detectedFileSystem == FileSystemKind::AutoDetect) {
            return false;
        }
        result.path = candidatePath;
        result.fileSystem = probe.detectedFileSystem;
        result.note = "Detectado automaticamente: " + toDisplayString(probe.detectedFileSystem);
        return true;
    };

    if (!sourcePath.startsWith("/dev/")) {
        tryProbe(sourcePath);
        return result;
    }

    const QString resolvedPreferred = resolveRecoverySourcePath(sourcePath, preferredFileSystem);
    if (tryProbe(resolvedPreferred)) {
        return result;
    }

    if (tryProbe(sourcePath)) {
        return result;
    }

    const QList<BlockDeviceChoice> devices = availableBlockDevices();
    for (const BlockDeviceChoice &device : devices) {
        if (device.type != "part") {
            continue;
        }
        if (!device.path.startsWith(sourcePath)) {
            continue;
        }
        if (tryProbe(device.path)) {
            return result;
        }
    }

    return result;
}

bool isReadableSourcePath(const QString &path) {
    if (path.trimmed().isEmpty()) {
        return false;
    }
    QFile file(path);
    return file.open(QIODevice::ReadOnly);
}

FileSystemKind inferFileSystemFromRoots(const QVector<RecoveryNode> &roots) {
    QList<RecoveryNode> stack;
    for (const RecoveryNode &root : roots) {
        stack.append(root);
    }

    while (!stack.isEmpty()) {
        const RecoveryNode node = stack.takeLast();
        if (node.path.startsWith("/NTFS reconstruido")) {
            return FileSystemKind::NTFS;
        }
        const QString lowerMeta = node.metadataText.toLower();
        if (lowerMeta.contains("\"data_runlist\"") || lowerMeta.contains("\"record_index\"") ||
            lowerMeta.contains("\"parent_ref\"")) {
            return FileSystemKind::NTFS;
        }
        if (lowerMeta.contains("\"first_cluster\"") || lowerMeta.contains("\"short_name\"")) {
            return FileSystemKind::FAT32;
        }
        if (lowerMeta.contains("\"no_fat_chain\"") || lowerMeta.contains("\"valid_data_length\"")) {
            return FileSystemKind::ExFAT;
        }
        for (const RecoveryNode &child : node.children) {
            stack.append(child);
        }
    }

    return FileSystemKind::AutoDetect;
}

QString friendlyKindLabel(const RecoveryNode &node) {
    if (node.kind == RecoveryNodeKind::Folder) {
        return "Carpeta";
    }
    const QString lower = node.name.toLower();
    if (isImageFileName(lower)) {
        return "Imagen";
    }
    if (lower.endsWith(".mp4") || lower.endsWith(".mov") || lower.endsWith(".avi") ||
        lower.endsWith(".mkv") || lower.endsWith(".mts") || lower.endsWith(".mpeg")) {
        return "Video";
    }
    if (lower.endsWith(".mp3") || lower.endsWith(".wav") || lower.endsWith(".flac") || lower.endsWith(".ogg")) {
        return "Audio";
    }
    if (isTextLikeFileName(lower)) {
        return "Texto";
    }
    return node.typeLabel.isEmpty() ? "Archivo" : node.typeLabel;
}
} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("Recovery Studio");
    buildUi();
    buildMenuAndToolbar();
    treeModel_->setNodes({});
    checkedPaths_.clear();
    summaryText_->setPlainText(
        "Recovery Studio lista.\n\n"
        "Abre un disco, un log o una sesion JSON para empezar."
    );
    detailText_->clear();
    previewLabel_->setText("Sin vista previa");
    updateScanProfile();
    updateSelectionCount();
    statusBar()->showMessage("Recovery Studio lista.");
}

void MainWindow::buildUi() {
    auto *central = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(8);

    auto *filterRow = new QHBoxLayout();
    filterRow->setSpacing(8);
    filterRow->addWidget(new QLabel("Filtro rapido:", central));

    filterEdit_ = new QLineEdit(central);
    filterEdit_->setPlaceholderText("Nombre, ruta o extension...");
    connect(filterEdit_, &QLineEdit::textChanged, this, &MainWindow::filterChanged);
    filterRow->addWidget(filterEdit_, 2);

    filterRow->addWidget(new QLabel("Biblioteca:", central));
    libraryCombo_ = new QComboBox(central);
    libraryCombo_->addItems({"Todos", "Imagenes", "Videos", "Documentos", "Comprimidos", "Otros"});
    filterRow->addWidget(libraryCombo_);

    filterRow->addWidget(new QLabel("Estado:", central));
    statusCombo_ = new QComboBox(central);
    statusCombo_->addItems({"Todos", "Activos", "Borrados", "Recuperables"});
    filterRow->addWidget(statusCombo_);

    filterRow->addWidget(new QLabel("Modo:", central));
    modeCombo_ = new QComboBox(central);
    modeCombo_->addItems({"Normal", "Turbo"});
    filterRow->addWidget(modeCombo_);

    filterRow->addWidget(new QLabel("Sistema de archivos:", central));
    fileSystemCombo_ = new QComboBox(central);
    fileSystemCombo_->addItems({"Auto", "NTFS", "exFAT", "FAT32", "ext4", "APFS", "HFS+", "Raw genérico"});
    filterRow->addWidget(fileSystemCombo_);

    selectionLabel_ = new QLabel("0 marcados", central);
    filterRow->addWidget(selectionLabel_);
    filterRow->addStretch(1);
    rootLayout->addLayout(filterRow);

    profileLabel_ = new QLabel(central);
    profileLabel_->setWordWrap(true);
    rootLayout->addWidget(profileLabel_);

    auto *splitter = new QSplitter(Qt::Horizontal, central);

    treeModel_ = new RecoveryTreeModel(this);
    proxyModel_ = new PathFilterProxyModel(this);
    proxyModel_->setSourceModel(treeModel_);
    proxyModel_->setRecursiveFilteringEnabled(true);

    treeView_ = new QTreeView(splitter);
    treeView_->setModel(proxyModel_);
    treeView_->setAlternatingRowColors(true);
    treeView_->setRootIsDecorated(true);
    treeView_->setUniformRowHeights(false);
    treeView_->setAnimated(false);
    treeView_->setAllColumnsShowFocus(false);
    treeView_->setIndentation(18);
    treeView_->setIconSize(QSize(20, 20));
    treeView_->setStyleSheet("QTreeView::item { padding-top: 3px; padding-bottom: 3px; }");
    treeView_->header()->setStretchLastSection(false);
    treeView_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    treeView_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    treeView_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    treeView_->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    treeView_->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    treeView_->setSortingEnabled(true);
    treeView_->sortByColumn(RecoveryTreeModel::PathColumn, Qt::AscendingOrder);
    treeView_->expandToDepth(1);
    treeView_->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(treeView_->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::updateDetailsFromSelection);
    connect(treeModel_, &QStandardItemModel::itemChanged, this, [this](QStandardItem *) {
        updateSelectionCount();
    });
    connect(treeView_, &QTreeView::customContextMenuRequested, this, [this](const QPoint &pos) {
        contextIndex_ = treeView_->indexAt(pos);
        if (!contextIndex_.isValid()) {
            return;
        }

        QMenu menu(this);
        QAction *expandAction = menu.addAction("Expandir rama");
        QAction *collapseAction = menu.addAction("Contraer rama");
        menu.addSeparator();
        QAction *markAction = menu.addAction("Marcar rama");
        QAction *unmarkAction = menu.addAction("Desmarcar rama");
        QAction *markDirectFilesAction = menu.addAction("Marcar solo archivos directos");
        QAction *unmarkDirectFilesAction = menu.addAction("Desmarcar solo archivos directos");
        QAction *picked = menu.exec(treeView_->viewport()->mapToGlobal(pos));

        if (picked == expandAction) {
            treeView_->expandRecursively(contextIndex_);
        } else if (picked == collapseAction) {
            treeView_->collapse(contextIndex_);
        } else if (picked == markAction) {
            toggleBranchFromContextMenu(true);
        } else if (picked == unmarkAction) {
            toggleBranchFromContextMenu(false);
        } else if (picked == markDirectFilesAction || picked == unmarkDirectFilesAction) {
            const QModelIndex sourceIndex = proxyModel_->mapToSource(contextIndex_);
            auto *item = treeModel_->itemFromIndex(sourceIndex.sibling(sourceIndex.row(), 0));
            if (item) {
                const int changedCount = setDirectFilesCheckState(item, picked == markDirectFilesAction);
                updateSelectionCount();
                statusBar()->showMessage(
                    (picked == markDirectFilesAction ? "Marcados " : "Desmarcados ") +
                    QString::number(changedCount) + " archivo(s) directos."
                );
            }
        }
    });

    auto *rightPane = new QWidget(splitter);
    auto *rightLayout = new QVBoxLayout(rightPane);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(8);

    auto *summaryLabel = new QLabel("Vista previa", rightPane);
    rightLayout->addWidget(summaryLabel);

    previewLabel_ = new QLabel("Base nativa Qt/C++ iniciada.\n\nAqui ira la previsualizacion de imagen, video y estado del archivo.", rightPane);
    previewLabel_->setAlignment(Qt::AlignCenter);
    previewLabel_->setMinimumHeight(280);
    previewLabel_->setWordWrap(true);
    rightLayout->addWidget(previewLabel_);

    rightLayout->addWidget(new QLabel("Resumen", rightPane));
    summaryText_ = new QTextEdit(rightPane);
    summaryText_->setReadOnly(true);
    rightLayout->addWidget(summaryText_, 2);

    rightLayout->addWidget(new QLabel("Detalles del nodo", rightPane));
    detailText_ = new QTextEdit(rightPane);
    detailText_->setReadOnly(true);
    rightLayout->addWidget(detailText_, 3);

    splitter->addWidget(treeView_);
    splitter->addWidget(rightPane);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    rootLayout->addWidget(splitter, 1);
    setCentralWidget(central);

    connect(modeCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::updateScanProfile);
    connect(fileSystemCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::updateScanProfile);
    connect(libraryCombo_, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        auto *pathProxy = static_cast<PathFilterProxyModel *>(proxyModel_);
        pathProxy->setLibraryFilter(text);
        treeView_->expandToDepth(1);
    });
    connect(statusCombo_, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        auto *pathProxy = static_cast<PathFilterProxyModel *>(proxyModel_);
        pathProxy->setStatusFilter(text);
        treeView_->expandToDepth(1);
    });
}

void MainWindow::buildMenuAndToolbar() {
    auto *fileMenu = menuBar()->addMenu("Archivo");
    auto *openDiskAction = fileMenu->addAction("Abrir disco...");
    auto *analyzeVolumeAction = fileMenu->addAction("Analizar volumen o imagen");
    fileMenu->addSeparator();
    auto *openLogAction = fileMenu->addAction("Importar log heredado");
    auto *openSessionAction = fileMenu->addAction("Importar sesion");
    auto *saveSessionAction = fileMenu->addAction("Guardar sesion");
    fileMenu->addSeparator();
    auto *quitAction = fileMenu->addAction("Salir");

    auto *scanMenu = menuBar()->addMenu("Escaneo");
    auto *superFastScanAction = scanMenu->addAction("Escaneo super rapido");
    auto *exploreVolumeAction = scanMenu->addAction("Explorar volumen detectado");
    scanMenu->addSeparator();
    auto *metadataScanAction = scanMenu->addAction("Escaneo por metadatos");
    auto *signatureScanAction = scanMenu->addAction("Escaneo por firmas");
    auto *enginePlanAction = scanMenu->addAction("Plan del motor");
    scanMenu->addSeparator();
    auto *supportMatrixAction = scanMenu->addAction("Matriz de compatibilidad");

    auto *recoverMenu = menuBar()->addMenu("Recuperacion");
    auto *recoverMarkedAction = recoverMenu->addAction("Recuperar marcados");

    auto *toolsMenu = menuBar()->addMenu("Herramientas");
    auto *diagnoseCurrentAction = toolsMenu->addAction("Diagnosticar origen actual");
    auto *diagnoseOtherAction = toolsMenu->addAction("Diagnosticar otro disco o imagen");
    auto *repairAdvisorAction = toolsMenu->addAction("Asistente de reparacion");
    auto *diskHealthAction = toolsMenu->addAction("Panel de salud del disco");
    toolsMenu->addSeparator();
    auto *exportHealthJsonAction = toolsMenu->addAction("Exportar salud a JSON");
    auto *exportHealthHtmlAction = toolsMenu->addAction("Exportar salud a HTML");
    auto *quickSurfaceScanAction = toolsMenu->addAction("Escaneo rapido de superficie");
    auto *fullSurfaceScanAction = toolsMenu->addAction("Escaneo completo de superficie");
    auto *smartProbeAction = toolsMenu->addAction("Leer estado SMART");

    auto *viewMenu = menuBar()->addMenu("Vista");
    auto *expandAction = viewMenu->addAction("Expandir todo");
    auto *collapseAction = viewMenu->addAction("Contraer todo");

    connect(openDiskAction, &QAction::triggered, this, &MainWindow::openDiskDevice);
    connect(analyzeVolumeAction, &QAction::triggered, this, &MainWindow::analyzeVolumeImage);
    connect(diagnoseCurrentAction, &QAction::triggered, this, &MainWindow::diagnoseCurrentSource);
    connect(diagnoseOtherAction, &QAction::triggered, this, &MainWindow::diagnoseAnotherSource);
    connect(repairAdvisorAction, &QAction::triggered, this, &MainWindow::showRepairAdvisor);
    connect(diskHealthAction, &QAction::triggered, this, &MainWindow::showDiskHealthDashboard);
    connect(exportHealthJsonAction, &QAction::triggered, this, &MainWindow::exportDiskHealthJson);
    connect(exportHealthHtmlAction, &QAction::triggered, this, &MainWindow::exportDiskHealthHtml);
    connect(quickSurfaceScanAction, &QAction::triggered, this, &MainWindow::runQuickSurfaceScan);
    connect(fullSurfaceScanAction, &QAction::triggered, this, &MainWindow::runFullSurfaceScan);
    connect(smartProbeAction, &QAction::triggered, this, &MainWindow::runSmartProbe);
    connect(openLogAction, &QAction::triggered, this, &MainWindow::openLogDocument);
    connect(openSessionAction, &QAction::triggered, this, &MainWindow::openJsonDocument);
    connect(saveSessionAction, &QAction::triggered, this, &MainWindow::saveSessionDocument);
    connect(superFastScanAction, &QAction::triggered, this, &MainWindow::runSuperFastScan);
    connect(exploreVolumeAction, &QAction::triggered, this, &MainWindow::exploreDetectedVolume);
    connect(metadataScanAction, &QAction::triggered, this, &MainWindow::startMetadataScan);
    connect(signatureScanAction, &QAction::triggered, this, &MainWindow::startSignatureScan);
    connect(recoverMarkedAction, &QAction::triggered, this, &MainWindow::recoverCheckedNodes);
    connect(enginePlanAction, &QAction::triggered, this, &MainWindow::showEnginePlan);
    connect(supportMatrixAction, &QAction::triggered, this, &MainWindow::showSupportMatrix);
    connect(expandAction, &QAction::triggered, treeView_, &QTreeView::expandAll);
    connect(collapseAction, &QAction::triggered, treeView_, &QTreeView::collapseAll);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    auto *toolbar = addToolBar("Principal");
    toolbar->setMovable(false);
    toolbar->addAction(openDiskAction);
    toolbar->addAction(analyzeVolumeAction);
    toolbar->addAction(diagnoseCurrentAction);
    toolbar->addAction(repairAdvisorAction);
    toolbar->addAction(diskHealthAction);
    toolbar->addAction(exportHealthJsonAction);
    toolbar->addAction(exportHealthHtmlAction);
    toolbar->addAction(quickSurfaceScanAction);
    toolbar->addAction(fullSurfaceScanAction);
    toolbar->addAction(smartProbeAction);
    toolbar->addAction(openLogAction);
    toolbar->addAction(openSessionAction);
    toolbar->addAction(saveSessionAction);
    toolbar->addSeparator();
    toolbar->addAction(superFastScanAction);
    toolbar->addAction(exploreVolumeAction);
    toolbar->addAction(metadataScanAction);
    toolbar->addAction(signatureScanAction);
    toolbar->addAction(recoverMarkedAction);
    toolbar->addAction(enginePlanAction);
    toolbar->addAction(supportMatrixAction);
    toolbar->addSeparator();
    toolbar->addAction(expandAction);
    toolbar->addAction(collapseAction);
}

void MainWindow::populateDemo() {
    treeModel_->setNodes(demoNodes());
    treeView_->expandToDepth(1);
    checkedPaths_.clear();
    summaryText_->setPlainText(
        "Recovery Studio Qt/C++ creada.\n\n"
        "- Arbol nativo con checkboxes\n"
        "- Tema oscuro\n"
        "- Panel de detalles\n"
        "- Arquitectura separada entre core y UI\n"
        "- Perfil de escaneo con modo normal/turbo\n"
        "- Preparada para NTFS, exFAT, FAT32, ext4, APFS, HFS+ y raw\n\n"
        "Siguiente paso: conectar este perfil al motor nativo real de escaneo."
    );
    updateScanProfile();
    updateSelectionCount();
}

void MainWindow::loadDemoData() {
    populateDemo();
    statusBar()->showMessage("Datos demo cargados.");
}

void MainWindow::openDiskDevice() {
    const QList<BlockDeviceChoice> devices = availableBlockDevices();
    if (devices.isEmpty()) {
        QMessageBox::warning(this, "Abrir disco", "No se pudieron listar discos o particiones con lsblk.");
        return;
    }

    QStringList labels;
    labels.reserve(devices.size());
    for (const BlockDeviceChoice &device : devices) {
        labels.append(device.label);
    }

    bool ok = false;
    const QString selectedLabel = QInputDialog::getItem(
        this,
        "Abrir disco",
        "Elige un disco o particion para trabajar:",
        labels,
        0,
        false,
        &ok
    );
    if (!ok || selectedLabel.isEmpty()) {
        return;
    }

    QString selectedPath;
    BlockDeviceChoice selectedDevice;
    for (const BlockDeviceChoice &device : devices) {
        if (device.label == selectedLabel) {
            selectedPath = device.path;
            selectedDevice = device;
            break;
        }
    }
    if (selectedPath.isEmpty()) {
        return;
    }

    const VolumeProbeResult result = VolumeProbe::probe(selectedPath);
    sourcePath_ = selectedPath;
    detectedFileSystem_ = result.detectedFileSystem;

    if (fileSystemCombo_ && fileSystemCombo_->currentText() == "Auto" &&
        detectedFileSystem_ != FileSystemKind::AutoDetect) {
        applyDetectedFileSystem(detectedFileSystem_);
    }

    QString summary = "Disco seleccionado\n\n";
    summary += "Origen: " + selectedPath + "\n";
    summary += "Tipo de bloque: " + (selectedDevice.type.isEmpty() ? QString("desconocido") : selectedDevice.type) + "\n";
    summary += "Montaje: " +
               ((selectedDevice.mountPoint.isEmpty() || selectedDevice.mountPoint == "-")
                    ? QString("no montado")
                    : selectedDevice.mountPoint) + "\n";
    summary += "FS lsblk: " + (selectedDevice.fsType.isEmpty() ? QString("sin detectar") : selectedDevice.fsType) + "\n";
    summary += "Sistema detectado: " +
               (result.detectedLabel.isEmpty() ? QString("Desconocido") : result.detectedLabel) + "\n\n";
    summary += result.summary;
    summary += "\n\n";
    summary += result.details;
    summary += "\n\n";
    summary += profileSummaryText();

    summaryText_->setPlainText(summary.trimmed());
    detailText_->setPlainText(
        "Origen: " + selectedPath + "\n"
        "Deteccion correcta: " + QString(result.success ? "si" : "no") + "\n"
        "Sistema: " + (result.detectedLabel.isEmpty() ? QString("Desconocido") : result.detectedLabel) + "\n"
        "Tamano de sector/bloque: " + QString::number(static_cast<qulonglong>(result.blockOrSectorSize)) + "\n"
        "Tamano de cluster: " + QString::number(static_cast<qulonglong>(result.clusterSize)) + "\n"
        "Unidades totales: " + QString::number(static_cast<qulonglong>(result.totalUnits))
    );
    previewLabel_->setText(
        result.success
            ? "Disco listo.\n\nYa puedes lanzar escaneos, explorar el volumen o diagnosticarlo."
            : "Disco abierto.\n\nAunque no se monte o no se reconozca el sistema de archivos, aun puedes usar firmas, diagnostico o forzarlo manualmente."
    );
    statusBar()->showMessage("Disco abierto: " + selectedPath);
    updateScanProfile();
}

void MainWindow::analyzeVolumeImage() {
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        "Analizar volumen o imagen",
        sourcePath_,
        "Todos (*.*)"
    );
    if (filePath.isEmpty()) {
        return;
    }

    const VolumeProbeResult result = VolumeProbe::probe(filePath);
    sourcePath_ = filePath;
    detectedFileSystem_ = result.detectedFileSystem;

    if (fileSystemCombo_ && fileSystemCombo_->currentText() == "Auto" && detectedFileSystem_ != FileSystemKind::AutoDetect) {
        applyDetectedFileSystem(detectedFileSystem_);
    }

    QString summary = "Analisis de volumen\n\n";
    summary += "Origen: " + filePath + "\n";
    summary += "Sistema detectado: " + (result.detectedLabel.isEmpty() ? QString("Desconocido") : result.detectedLabel) + "\n\n";
    summary += result.summary;
    summary += "\n\n";
    summary += result.details;
    summary += "\n\n";
    summary += profileSummaryText();

    summaryText_->setPlainText(summary.trimmed());
    detailText_->setPlainText(
        "Origen: " + filePath + "\n"
        "Deteccion correcta: " + QString(result.success ? "si" : "no") + "\n"
        "Sistema: " + (result.detectedLabel.isEmpty() ? QString("Desconocido") : result.detectedLabel) + "\n"
        "Tamano de sector/bloque: " + QString::number(static_cast<qulonglong>(result.blockOrSectorSize)) + "\n"
        "Tamano de cluster: " + QString::number(static_cast<qulonglong>(result.clusterSize)) + "\n"
        "Unidades totales: " + QString::number(static_cast<qulonglong>(result.totalUnits))
    );
    previewLabel_->setText(
        result.success
            ? "Volumen reconocido.\n\nLa herramienta ya puede orientar el motor correcto para este origen."
            : "No se reconocio el volumen.\n\nAun asi podras usar firmas o forzar manualmente un sistema de archivos."
    );
    statusBar()->showMessage(
        result.success
            ? "Volumen analizado: " + result.detectedLabel
            : "No se pudo identificar el volumen."
    );
    updateScanProfile();
}

void MainWindow::diagnoseCurrentSource() {
    if (sourcePath_.isEmpty()) {
        QMessageBox::information(this, "Diagnostico", "Primero selecciona o analiza un disco, particion o imagen.");
        return;
    }
    runDiagnosisWorkflow(sourcePath_, false);
}

void MainWindow::diagnoseAnotherSource() {
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        "Diagnosticar disco, particion o imagen",
        sourcePath_,
        "Todos (*.*)"
    );
    if (filePath.isEmpty()) {
        return;
    }
    runDiagnosisWorkflow(filePath, true);
}

void MainWindow::runDiagnosisWorkflow(const QString &diagnosticSourcePath, bool updateCurrentSource) {
    QProgressDialog progress("Preparando diagnostico completo...", "Cancelar", 0, 100, this);
    progress.setWindowTitle("Diagnostico completo del disco");
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(0);
    progress.setAutoClose(false);
    progress.setAutoReset(false);
    progress.setMinimumWidth(620);
    progress.setValue(0);
    progress.show();
    QCoreApplication::processEvents();

    const DiskDoctorResult result = DiskDoctor::diagnose(
        diagnosticSourcePath,
        [&progress](int percent, const QString &phase, const QString &detail) {
            progress.setValue(std::clamp(percent, 0, 100));
            progress.setLabelText(
                "Diagnostico completo en progreso...\n\n"
                "Fase actual: " + phase +
                "\nDetalle: " + detail +
                "\n\nConsejo: este modo no escribe en el disco. Solo inspecciona cabecera, bloque, lecturas de muestra y metadatos base."
            );
            QCoreApplication::processEvents();
            return !progress.wasCanceled();
        }
    );

    progress.setValue(100);
    progress.close();

    if (updateCurrentSource) {
        sourcePath_ = diagnosticSourcePath;
    }
    if (result.detectedFileSystem != FileSystemKind::AutoDetect) {
        detectedFileSystem_ = result.detectedFileSystem;
        if (fileSystemCombo_ && fileSystemCombo_->currentText() == "Auto") {
            applyDetectedFileSystem(detectedFileSystem_);
        }
    }

    summaryText_->setPlainText(result.summary);
    detailText_->setPlainText(result.details);
    previewLabel_->setText(
        result.readSamplesFailed > 0
            ? "Diagnostico completado.\n\nHay indicios de errores de lectura o metadatos dañados. Prioriza clonado, imagen o recuperacion antes de reparar."
            : "Diagnostico completado.\n\nEl disco responde razonablemente bien. Ya puedes decidir si explorar, recuperar o validar logicamente."
    );
    statusBar()->showMessage("Diagnostico completado: " + diagnosticSourcePath);
    updateScanProfile();
    showDiagnosisNextSteps(result);
}

void MainWindow::showDiagnosisNextSteps(const DiskDoctorResult &result) {
    QMessageBox box(this);
    box.setWindowTitle("Diagnostico finalizado");
    box.setIcon(result.readSamplesFailed > 0 ? QMessageBox::Warning : QMessageBox::Information);

    QString text;
    text += "Origen: " + result.sourcePath + "\n";
    text += "Sistema detectado: " +
            (result.detectedLabel.isEmpty() ? QString("Desconocido") : result.detectedLabel) + "\n";
    text += "Lecturas correctas: " + QString::number(result.readSamplesOk) + "\n";
    text += "Lecturas fallidas: " + QString::number(result.readSamplesFailed) + "\n\n";
    text += "Opciones recomendadas ahora:\n";
    if (result.readSamplesFailed > 0) {
        text += "- Recuperar o clonar antes de reparar.\n";
        text += "- Usar firmas si el sistema de archivos falla.\n";
        text += "- Abrir el asistente de reparacion solo despues de asegurar los datos.\n";
    } else {
        text += "- Explorar metadatos o estructura reconstruida.\n";
        text += "- Escaneo super rapido para ver carpetas y archivos pronto.\n";
        text += "- Asistente de reparacion si el problema es solo logico/montaje.\n";
    }
    box.setText(text.trimmed());

    QPushButton *repairButton = box.addButton("Asistente de reparacion", QMessageBox::ActionRole);
    QPushButton *metadataButton = box.addButton("Escaneo por metadatos", QMessageBox::ActionRole);
    QPushButton *signatureButton = box.addButton("Escaneo por firmas", QMessageBox::ActionRole);
    QPushButton *closeButton = box.addButton("Cerrar", QMessageBox::RejectRole);
    Q_UNUSED(closeButton);

    box.exec();
    if (box.clickedButton() == repairButton) {
        showRepairAdvisor();
    } else if (box.clickedButton() == metadataButton) {
        startMetadataScan();
    } else if (box.clickedButton() == signatureButton) {
        startSignatureScan();
    }
}

void MainWindow::showRepairAdvisor() {
    if (sourcePath_.isEmpty()) {
        QMessageBox::information(this, "Asistente de reparacion", "Primero selecciona o analiza un disco, particion o imagen.");
        return;
    }

    const RepairAdvisorResult result = RepairAdvisor::build(sourcePath_, currentFileSystemKind());

    QString text;
    text += result.summary + "\n\n";
    text += "Notas de seguridad:\n- " + result.safetyNotes + "\n\n";
    text += "Pasos guiados:\n- " + result.guidedSteps.join("\n- ");
    if (!result.suggestedCommands.isEmpty()) {
        text += "\n\nComandos sugeridos para " + result.operatingSystemLabel + ":\n- " +
                result.suggestedCommands.join("\n- ");
    }

    summaryText_->setPlainText(text.trimmed());
    detailText_->setPlainText(
        "Asistente de reparacion\n"
        "SO actual: " + result.operatingSystemLabel + "\n"
        "Sistema de archivos: " + toDisplayString(result.fileSystem) + "\n"
        "Origen: " + result.sourcePath + "\n"
        "Comandos directos: " + QString(result.hasDirectCommandSupport ? "si" : "no")
    );
    previewLabel_->setText(
        result.hasDirectCommandSupport
            ? "Asistente listo.\n\nEmpieza por el comando de verificacion, no por el de reparacion."
            : "Asistente listo.\n\nEn este sistema aun no hay comandos nativos preconfigurados."
    );
    statusBar()->showMessage("Asistente de reparacion preparado.");

    QString dialogText;
    dialogText += result.summary + "\n\n";
    dialogText += "Notas de seguridad:\n- " + result.safetyNotes + "\n\n";
    dialogText += "Pasos guiados:\n- " + result.guidedSteps.join("\n- ");
    if (!result.suggestedCommands.isEmpty()) {
        dialogText += "\n\nComandos sugeridos para " + result.operatingSystemLabel + ":\n";
        for (int i = 0; i < result.suggestedCommands.size(); ++i) {
            const QString label =
                i == 0 ? "Verificacion/reparacion inicial"
                       : QString("Alternativa %1").arg(i);
            dialogText += "- " + label + ": " + result.suggestedCommands.at(i) + "\n";
        }
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Asistente de reparacion");
    dialog.setModal(true);
    dialog.resize(760, 520);

    auto *layout = new QVBoxLayout(&dialog);
    auto *introLabel = new QLabel(
        "Asistente de reparacion del montaje\n\n"
        "Aqui tienes los pasos recomendados y los comandos sugeridos para intentar arreglar el montaje sin ir a ciegas.",
        &dialog
    );
    introLabel->setWordWrap(true);
    layout->addWidget(introLabel);

    auto *textView = new QPlainTextEdit(&dialog);
    textView->setReadOnly(true);
    textView->setPlainText(dialogText.trimmed());
    layout->addWidget(textView, 1);

    auto *buttonRow = new QHBoxLayout();
    auto *copyButton = new QPushButton("Copiar comandos", &dialog);
    auto *metadataButton = new QPushButton("Escaneo por metadatos", &dialog);
    auto *signatureButton = new QPushButton("Escaneo por firmas", &dialog);
    auto *closeButton = new QPushButton("Cerrar", &dialog);
    buttonRow->addWidget(copyButton);
    buttonRow->addWidget(metadataButton);
    buttonRow->addWidget(signatureButton);
    buttonRow->addStretch(1);
    buttonRow->addWidget(closeButton);
    layout->addLayout(buttonRow);

    connect(copyButton, &QPushButton::clicked, &dialog, [&result, this]() {
        QString commandText;
        if (result.suggestedCommands.isEmpty()) {
            commandText = "No hay comandos sugeridos para este sistema.";
        } else {
            commandText = result.suggestedCommands.join("\n");
        }
        QGuiApplication::clipboard()->setText(commandText);
        statusBar()->showMessage("Comandos del asistente copiados al portapapeles.");
    });
    connect(metadataButton, &QPushButton::clicked, &dialog, [this, &dialog]() {
        dialog.accept();
        startMetadataScan();
    });
    connect(signatureButton, &QPushButton::clicked, &dialog, [this, &dialog]() {
        dialog.accept();
        startSignatureScan();
    });
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);

    dialog.exec();
}

void MainWindow::showDiskHealthDashboard() {
    if (sourcePath_.isEmpty()) {
        QMessageBox::information(this, "Panel de salud del disco", "Primero selecciona o analiza un disco, particion o imagen.");
        return;
    }

    summaryText_->setPlainText("Construyendo panel de salud del disco...");
    QCoreApplication::processEvents();

    const DiskHealthReport report = DiskHealthReportBuilder::build(sourcePath_);
    lastDiskHealthReport_ = report;
    summaryText_->setPlainText(report.summary);
    detailText_->setPlainText(report.details);

    if (report.healthBadge == "Riesgo alto") {
        previewLabel_->setText(
            "Salud del disco: Riesgo alto\n\n"
            "Se han detectado fallos de lectura o sintomas fisicos. Lo mas prudente es clonar o recuperar antes de reparar."
        );
    } else if (report.healthBadge == "Atencion") {
        previewLabel_->setText(
            "Salud del disco: Atencion\n\n"
            "La lectura parece estable, pero SMART muestra alertas que conviene vigilar."
        );
    } else if (report.healthBadge == "Estable") {
        previewLabel_->setText(
            "Salud del disco: Estable\n\n"
            "No se han detectado alertas evidentes en esta revision rapida."
        );
    } else {
        previewLabel_->setText(
            "Salud del disco: Desconocida\n\n"
            "No hubo suficientes datos para valorar el estado general con confianza."
        );
    }

    statusBar()->showMessage("Panel de salud del disco actualizado.");
}

void MainWindow::exportDiskHealthJson() {
    if (lastDiskHealthReport_.sourcePath.isEmpty()) {
        QMessageBox::information(this, "Exportar salud a JSON", "Primero genera el panel de salud del disco.");
        return;
    }

    const QString defaultName =
        QFileInfo(lastDiskHealthReport_.sourcePath).fileName().replace('/', '_') + "_salud.json";
    const QString filePath = QFileDialog::getSaveFileName(
        this,
        "Exportar informe de salud a JSON",
        defaultName,
        "JSON (*.json)"
    );
    if (filePath.isEmpty()) {
        return;
    }

    QString errorMessage;
    if (!DiskHealthReportExporter::exportJson(lastDiskHealthReport_, filePath, &errorMessage)) {
        QMessageBox::warning(this, "Exportar salud a JSON", errorMessage);
        return;
    }
    statusBar()->showMessage("Informe de salud exportado a JSON.");
}

void MainWindow::exportDiskHealthHtml() {
    if (lastDiskHealthReport_.sourcePath.isEmpty()) {
        QMessageBox::information(this, "Exportar salud a HTML", "Primero genera el panel de salud del disco.");
        return;
    }

    const QString defaultName =
        QFileInfo(lastDiskHealthReport_.sourcePath).fileName().replace('/', '_') + "_salud.html";
    const QString filePath = QFileDialog::getSaveFileName(
        this,
        "Exportar informe de salud a HTML",
        defaultName,
        "HTML (*.html)"
    );
    if (filePath.isEmpty()) {
        return;
    }

    QString errorMessage;
    if (!DiskHealthReportExporter::exportHtml(lastDiskHealthReport_, filePath, &errorMessage)) {
        QMessageBox::warning(this, "Exportar salud a HTML", errorMessage);
        return;
    }
    statusBar()->showMessage("Informe de salud exportado a HTML.");
}

void MainWindow::runQuickSurfaceScan() {
    if (sourcePath_.isEmpty()) {
        QMessageBox::information(this, "Escaneo rapido de superficie", "Primero selecciona o analiza un disco, particion o imagen.");
        return;
    }

    const std::uint64_t estimatedTotalBytes = estimateSourceBytes(sourcePath_);

    QProgressDialog progress("Preparando escaneo rapido...", "Cancelar", 0, 1000, this);
    progress.setWindowTitle("Escaneo rapido de superficie");
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(0);
    progress.setValue(0);

    const SurfaceScanResult result = SurfaceScanner::scanQuick(
        sourcePath_,
        estimatedTotalBytes,
        [&progress](std::uint64_t processedBytes, std::uint64_t totalBytes, int okChunks, int failedChunks, const QString &currentStatus) {
            const int value = totalBytes > 0
                ? static_cast<int>((processedBytes * 1000ull) / totalBytes)
                : 0;
            progress.setValue(value);
            progress.setLabelText(
                "Escaneando superficie...\n\n" + currentStatus +
                "\n\nProcesado: " + QString::number(static_cast<qulonglong>(processedBytes)) +
                " / " + QString::number(static_cast<qulonglong>(totalBytes)) +
                "\nCorrectos: " + QString::number(okChunks) +
                " | Fallos: " + QString::number(failedChunks)
            );
            QCoreApplication::processEvents();
            return !progress.wasCanceled();
        }
    );

    progress.setValue(1000);
    summaryText_->setPlainText(result.summary);
    detailText_->setPlainText(result.details);
    previewLabel_->setText(
        result.chunksFailed > 0
            ? "Escaneo rapido completado.\n\nSe han detectado incidencias de lectura: conviene clonar antes de reparar."
            : "Escaneo rapido completado.\n\nNo se han visto fallos en las muestras leidas."
    );
    statusBar()->showMessage("Escaneo rapido de superficie finalizado.");
}

void MainWindow::runFullSurfaceScan() {
    if (sourcePath_.isEmpty()) {
        QMessageBox::information(this, "Escaneo completo de superficie", "Primero selecciona o analiza un disco, particion o imagen.");
        return;
    }

    const auto answer = QMessageBox::question(
        this,
        "Escaneo completo de superficie",
        "Este escaneo recorre el origen completo y puede tardar bastante.\n\n¿Quieres continuar?"
    );
    if (answer != QMessageBox::Yes) {
        return;
    }

    const std::uint64_t estimatedTotalBytes = estimateSourceBytes(sourcePath_);

    QProgressDialog progress("Preparando escaneo completo...", "Cancelar", 0, 1000, this);
    progress.setWindowTitle("Escaneo completo de superficie");
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(0);
    progress.setValue(0);

    const SurfaceScanResult result = SurfaceScanner::scanFull(
        sourcePath_,
        estimatedTotalBytes,
        [&progress](std::uint64_t processedBytes, std::uint64_t totalBytes, int okChunks, int failedChunks, const QString &currentStatus) {
            const int value = totalBytes > 0
                ? static_cast<int>((processedBytes * 1000ull) / totalBytes)
                : 0;
            progress.setValue(value);
            progress.setLabelText(
                "Escaneando toda la superficie...\n\n" + currentStatus +
                "\n\nProcesado: " + QString::number(static_cast<qulonglong>(processedBytes)) +
                " / " + QString::number(static_cast<qulonglong>(totalBytes)) +
                "\nCorrectos: " + QString::number(okChunks) +
                " | Fallos: " + QString::number(failedChunks)
            );
            QCoreApplication::processEvents();
            return !progress.wasCanceled();
        }
    );

    progress.setValue(1000);
    summaryText_->setPlainText(result.summary);
    detailText_->setPlainText(result.details);
    previewLabel_->setText(
        result.canceled
            ? "Escaneo completo cancelado.\n\nSe muestran los resultados parciales obtenidos hasta el momento."
            : (result.chunksFailed > 0
                ? "Escaneo completo terminado.\n\nSe han detectado zonas con incidencias de lectura."
                : "Escaneo completo terminado.\n\nNo se han detectado fallos de lectura en los bloques revisados.")
    );
    statusBar()->showMessage(
        result.canceled
            ? "Escaneo completo de superficie cancelado."
            : "Escaneo completo de superficie finalizado."
    );
}

void MainWindow::runSmartProbe() {
    if (sourcePath_.isEmpty()) {
        QMessageBox::information(this, "Estado SMART", "Primero selecciona o analiza un disco.");
        return;
    }

    const SmartProbeResult result = SmartProbe::probe(sourcePath_);
    summaryText_->setPlainText(result.summary);
    detailText_->setPlainText(result.details);
    previewLabel_->setText(
        !result.warnings.isEmpty()
            ? "SMART leido.\n\nHay alertas que conviene revisar antes de seguir usando o reparando el disco."
            : "SMART leido.\n\nNo se han detectado alertas evidentes en los atributos clave revisados."
    );
    statusBar()->showMessage("Lectura SMART finalizada.");
}

void MainWindow::runSuperFastScan() {
    if (sourcePath_.isEmpty()) {
        QMessageBox::information(this, "Escaneo super rapido", "Primero abre o selecciona un disco, particion o imagen.");
        return;
    }

    const FileSystemKind requestedFileSystem = currentFileSystemKind();
    const EffectiveScanSource effectiveSource = determineEffectiveScanSource(sourcePath_, requestedFileSystem);
    const FileSystemKind fileSystem =
        effectiveSource.fileSystem != FileSystemKind::AutoDetect ? effectiveSource.fileSystem : requestedFileSystem;
    const QString effectiveSourcePath = effectiveSource.path;
    int maxEntries = 180;
    int maxDepth = 1;
    int maxRecords = 0;
    bool includeDeleted = false;

    switch (fileSystem) {
    case FileSystemKind::NTFS:
        maxEntries = 12000;
        maxDepth = 1;
        maxRecords = 120000;
        includeDeleted = false;
        break;
    case FileSystemKind::FAT32:
    case FileSystemKind::ExFAT:
        maxEntries = 260;
        maxDepth = 1;
        includeDeleted = false;
        break;
    default:
        maxEntries = 160;
        maxDepth = 1;
        includeDeleted = false;
        break;
    }

    QElapsedTimer timer;
    timer.start();

    QProgressDialog progress("Preparando escaneo super rapido...", "Cancelar", 0, 100, this);
    progress.setWindowTitle("Escaneo super rapido");
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(0);
    progress.setAutoClose(false);
    progress.setAutoReset(false);
    progress.setValue(0);
    progress.show();
    QCoreApplication::processEvents();
    if (progress.wasCanceled()) {
        return;
    }

    progress.setLabelText(
        "Preparando escaneo super rapido...\n\n"
        "Origen seleccionado: " + sourcePath_ + "\n"
        "Origen usado: " + effectiveSourcePath + "\n"
        "Sistema de archivos: " + toDisplayString(fileSystem) +
        (effectiveSource.note.isEmpty() ? QString() : "\n" + effectiveSource.note)
    );
    QCoreApplication::processEvents();

    const VolumeExploreResult result =
        fileSystem == FileSystemKind::NTFS
            ? VolumeExplorer::exploreFastNtfsMft(
                  effectiveSourcePath,
                  maxEntries,
                  maxRecords,
                  includeDeleted,
                  [this, &progress, &timer, &effectiveSourcePath](int scannedRecords, int totalRecords, int foundActive, int foundDeleted) {
                      const int percent =
                          totalRecords > 0
                              ? std::clamp((scannedRecords * 100) / totalRecords, 0, 100)
                              : 0;
                      const qint64 elapsedMs = timer.elapsed();
                      const qint64 remainingMs =
                          (scannedRecords > 0 && totalRecords > 0)
                              ? static_cast<qint64>((static_cast<double>(elapsedMs) / scannedRecords) * (totalRecords - scannedRecords))
                              : 0;
                      progress.setValue(percent);
                      progress.setLabelText(
                          "Escaneo super rapido NTFS en progreso...\n\n"
                          "Origen: " + effectiveSourcePath + "\n"
                          "Progreso: " + QString::number(percent) + "%\n"
                          "Registros MFT: " + QString::number(scannedRecords) + "/" + QString::number(totalRecords) + "\n"
                          "Activos detectados: " + QString::number(foundActive) + "\n"
                          "Borrados detectados: " + QString::number(foundDeleted) + "\n"
                          "Transcurrido: " + formatDurationCompact(elapsedMs / 1000) + "\n"
                          "Queda aprox.: " + formatDurationCompact(remainingMs / 1000)
                      );
                      QCoreApplication::processEvents();
                      Q_UNUSED(this);
                  })
            : VolumeExplorer::explore(effectiveSourcePath, fileSystem, maxEntries, maxDepth, includeDeleted);

    progress.setValue(100);
    progress.close();

    if (!result.roots.isEmpty()) {
        treeModel_->setNodes(result.roots);
        treeView_->expandToDepth(1);
        checkedPaths_.clear();
        updateSelectionCount();
    }

    summaryText_->setPlainText(
        profileSummaryText() +
        "\n\nEscaneo super rapido completado.\n\n" +
        result.summary + "\n\n" + result.details +
        "\n\nParametros usados:\n"
        "- origen efectivo: " + effectiveSourcePath + "\n"
        "- maxEntries: " + QString::number(maxEntries) + "\n"
        "- maxDepth: " + QString::number(maxDepth) + "\n"
        "- maxRecords: " + QString::number(maxRecords) + "\n"
        "- borrados incluidos: " + QString(includeDeleted ? "si" : "no")
    );
    detailText_->setPlainText(
        "Origen seleccionado: " + sourcePath_ + "\n"
        "Origen usado: " + effectiveSourcePath + "\n"
        "Sistema: " + toDisplayString(fileSystem) + "\n"
        "Modo: super rapido\n"
        "Exploracion correcta: " + QString(result.success ? "si" : "no") + "\n"
        "Raices cargadas: " + QString::number(result.roots.size()) + "\n"
        "Entradas maximas: " + QString::number(maxEntries) + "\n"
        "Profundidad maxima: " + QString::number(maxDepth) + "\n"
        "Registros maximos: " + QString::number(maxRecords)
    );
    previewLabel_->setText(
        result.success
            ? "Escaneo super rapido completado.\n\nSe muestra un arbol parcial recuperable para empezar a trabajar ya."
            : "El escaneo super rapido no ha encontrado estructura suficiente.\n\nPrueba exploracion normal o escaneo por firmas."
    );
    statusBar()->showMessage(
        result.success
            ? "Escaneo super rapido completado."
            : "Escaneo super rapido sin resultados utiles."
    );
}

void MainWindow::exploreDetectedVolume() {
    if (sourcePath_.isEmpty()) {
        QMessageBox::information(this, "Explorar volumen", "Primero analiza un volumen o imagen.");
        return;
    }

    const FileSystemKind fileSystem = currentFileSystemKind();
    const VolumeExploreResult result = VolumeExplorer::explore(sourcePath_, fileSystem, 400);
    if (!result.roots.isEmpty()) {
        treeModel_->setNodes(result.roots);
        treeView_->expandToDepth(1);
        checkedPaths_.clear();
        updateSelectionCount();
    }

    summaryText_->setPlainText(
        profileSummaryText() + "\n\n" + result.summary + "\n\n" + result.details
    );
    detailText_->setPlainText(
        "Origen: " + sourcePath_ + "\n"
        "Sistema: " + toDisplayString(fileSystem) + "\n"
        "Exploracion correcta: " + QString(result.success ? "si" : "no") + "\n"
        "Raices cargadas: " + QString::number(result.roots.size())
    );
    previewLabel_->setText(
        result.success
            ? "Exploracion completada.\n\nSe muestran entradas reales del volumen en el arbol."
            : "Exploracion no disponible o sin resultados.\n\nEste primer explorador solo lista raiz real en FAT32 y exFAT."
    );
    statusBar()->showMessage(
        result.success
            ? "Exploracion de volumen completada."
            : "No se pudo explorar el volumen con el explorador actual."
    );
}

void MainWindow::openLogDocument() {
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        "Importar log heredado",
        QString(),
        "Logs (*.log *.txt);;Todos (*.*)"
    );
    if (filePath.isEmpty()) {
        return;
    }

    try {
        const ParsedLog parsed = LogParser::loadFromFile(filePath);
        treeModel_->setNodes(parsed.roots);
        checkedPaths_.clear();
        treeView_->expandToDepth(1);
        summaryText_->setPlainText(profileSummaryText() + "\n\n" + parsed.summaryText);
        detailText_->setPlainText(
            "Archivo cargado: " + filePath + "\n"
            "Dispositivo detectado: " + (parsed.detectedDevice.isEmpty() ? QString("no encontrado") : parsed.detectedDevice) + "\n"
            "Entradas detectadas: " + QString::number(parsed.pathEntries)
        );
        previewLabel_->setText(
            parsed.scanStateOnly
                ? "Log heredado cargado.\n\nEste archivo no contiene rutas claras suficientes para reconstruir un arbol completo."
                : "Log heredado cargado.\n\nEl arbol de rutas detectadas ya esta disponible en la parte izquierda."
        );
        updateSelectionCount();
        statusBar()->showMessage("Importacion completada: " + filePath);
    } catch (const std::exception &exc) {
        summaryText_->setPlainText(QString::fromUtf8(exc.what()));
        detailText_->clear();
        previewLabel_->setText("No se pudo cargar el log.");
        statusBar()->showMessage("No se pudo importar el log.");
    }
}

void MainWindow::openJsonDocument() {
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        "Importar sesion o manifiesto JSON",
        QString(),
        "JSON (*.json);;Todos (*.*)"
    );
    if (filePath.isEmpty()) {
        return;
    }

    try {
        const LoadedSession session = SessionLoader::loadFromFile(filePath);
        treeModel_->setNodes({});
        checkedPaths_.clear();
        sourcePath_ = session.sourceRoot.trimmed();

        if (session.roots.isEmpty()) {
            summaryText_->setPlainText(
                profileSummaryText() + "\n\n" + session.summaryText +
                "\n\nNo se encontraron nodos importables en este JSON."
            );
            detailText_->setPlainText(session.rawJsonText.left(20000));
            previewLabel_->setText("La sesion JSON no contiene un arbol visible compatible todavia.");
            if (sourcePath_.isEmpty()) {
                detectedFileSystem_ = FileSystemKind::AutoDetect;
            }
            updateSelectionCount();
            statusBar()->showMessage("Sesion JSON abierta, pero sin nodos importables.");
            QMessageBox::warning(
                this,
                "Sesion sin arbol",
                "El JSON se ha abierto, pero no contiene nodos importables para el arbol.\n\n"
                "La demo ya no se mantiene en pantalla, asi que si el arbol queda vacio el problema es el formato del JSON."
            );
            return;
        }

        if (!sourcePath_.isEmpty()) {
            const VolumeProbeResult probe = VolumeProbe::probe(sourcePath_);
            if (probe.detectedFileSystem != FileSystemKind::AutoDetect) {
                detectedFileSystem_ = probe.detectedFileSystem;
            }
        }
        if (detectedFileSystem_ == FileSystemKind::AutoDetect) {
            detectedFileSystem_ = inferFileSystemFromRoots(session.roots);
        }
        if (fileSystemCombo_ && fileSystemCombo_->currentText() == "Auto" &&
            detectedFileSystem_ != FileSystemKind::AutoDetect) {
            applyDetectedFileSystem(detectedFileSystem_);
        }

        treeModel_->setNodes(session.roots);
        checkedPaths_ = session.checkedPaths;
        restoreCheckedPaths();
        treeView_->expandToDepth(1);
        summaryText_->setPlainText(profileSummaryText() + "\n\n" + session.summaryText);
        detailText_->clear();
        previewLabel_->setText(
            "Sesion/manifiesto cargado."
        );
        const QString resolvedSourcePath = resolveRecoverySourcePath(sourcePath_, currentFileSystemKind());
        if (resolvedSourcePath.startsWith("/dev/") && !isReadableSourcePath(resolvedSourcePath)) {
            if (offerAdminRelaunch(
                    resolvedSourcePath,
                    "La sesion se ha cargado, pero el dispositivo no se puede leer con los permisos actuales.")) {
                return;
            }
        }
        updateSelectionCount();
        statusBar()->showMessage("Sesion cargada: " + filePath);
    } catch (const std::exception &exc) {
        treeModel_->setNodes({});
        checkedPaths_.clear();
        sourcePath_.clear();
        summaryText_->setPlainText(QString::fromUtf8(exc.what()));
        detailText_->clear();
        previewLabel_->setText("No se pudo cargar la sesion JSON.");
        updateSelectionCount();
        statusBar()->showMessage("No se pudo cargar la sesion JSON.");
    }
}

void MainWindow::saveSessionDocument() {
    const QVector<RecoveryNode> roots = collectCurrentRoots();
    if (roots.isEmpty()) {
        QMessageBox::information(this, "Guardar sesion", "No hay ningun arbol cargado para guardar.");
        return;
    }

    const QString defaultName =
        (sourcePath_.isEmpty() ? QString("recovery_studio_sesion.json")
                               : QFileInfo(sourcePath_).fileName().replace('/', '_') + "_sesion.json");
    const QString filePath = QFileDialog::getSaveFileName(
        this,
        "Guardar sesion",
        defaultName,
        "JSON (*.json)"
    );
    if (filePath.isEmpty()) {
        return;
    }

    QString errorMessage;
    if (!SessionExporter::saveToFile(filePath, sourcePath_, roots, collectCheckedPaths(), &errorMessage)) {
        QMessageBox::warning(this, "Guardar sesion", errorMessage);
        return;
    }

    statusBar()->showMessage("Sesion guardada correctamente.");
}

QVector<RecoveryNode> MainWindow::demoNodes() const {
    const QString fsLabel = currentFileSystemLabel() == "Auto" ? "NTFS" : currentFileSystemLabel();

    RecoveryNode cruiseFolder;
    cruiseFolder.name = "CRUCERO 11-10-19";
    cruiseFolder.path = "/" + fsLabel + " reconstruido/Archivos y carpetas/CRUCERO 11-10-19";
    cruiseFolder.typeLabel = "Carpeta";
    cruiseFolder.statusLabel = "Activo";
    cruiseFolder.kind = RecoveryNodeKind::Folder;
    cruiseFolder.sizeBytes = 8ull * 1024ull * 1024ull * 1024ull;
    cruiseFolder.lineCount = 1;

    RecoveryNode imageFile;
    imageFile.name = "IMG_3485.JPG";
    imageFile.path = cruiseFolder.path + "/IMG_3485.JPG";
    imageFile.typeLabel = "JPG";
    imageFile.statusLabel = "Activo";
    imageFile.kind = RecoveryNodeKind::File;
    imageFile.sizeBytes = 2967090;
    imageFile.lineCount = 1;

    RecoveryNode videoFile;
    videoFile.name = "video_fiesta.mp4";
    videoFile.path = cruiseFolder.path + "/video_fiesta.mp4";
    videoFile.typeLabel = "MP4";
    videoFile.statusLabel = "Activo";
    videoFile.kind = RecoveryNodeKind::File;
    videoFile.sizeBytes = 148ull * 1024ull * 1024ull;
    videoFile.lineCount = 1;

    RecoveryNode deletedFile;
    deletedFile.name = "foto_borrada.jpg [borrado]";
    deletedFile.path = "/" + fsLabel + " reconstruido/Borrados detectados/foto_borrada.jpg [borrado]";
    deletedFile.typeLabel = "JPG";
    deletedFile.statusLabel = "Borrado";
    deletedFile.kind = RecoveryNodeKind::File;
    deletedFile.sizeBytes = 1882400;
    deletedFile.lineCount = 1;
    deletedFile.deleted = true;

    cruiseFolder.children = {imageFile, videoFile};

    RecoveryNode activeRoot;
    activeRoot.name = "Archivos y carpetas";
    activeRoot.path = "/" + fsLabel + " reconstruido/Archivos y carpetas";
    activeRoot.typeLabel = "Carpeta";
    activeRoot.statusLabel = "Activo";
    activeRoot.kind = RecoveryNodeKind::Folder;
    activeRoot.sizeBytes = cruiseFolder.sizeBytes;
    activeRoot.children = {cruiseFolder};

    RecoveryNode deletedRoot;
    deletedRoot.name = "Borrados detectados";
    deletedRoot.path = "/" + fsLabel + " reconstruido/Borrados detectados";
    deletedRoot.typeLabel = "Carpeta";
    deletedRoot.statusLabel = "Borrado";
    deletedRoot.kind = RecoveryNodeKind::Folder;
    deletedRoot.deleted = true;
    deletedRoot.children = {deletedFile};

    RecoveryNode root;
    root.name = fsLabel + " reconstruido";
    root.path = "/" + fsLabel + " reconstruido";
    root.typeLabel = "Carpeta";
    root.statusLabel = "Activo";
    root.kind = RecoveryNodeKind::Folder;
    root.children = {activeRoot, deletedRoot};

    if (fsLabel == "exFAT" || fsLabel == "FAT32") {
        RecoveryNode cameraFolder;
        cameraFolder.name = "DCIM";
        cameraFolder.path = activeRoot.path + "/DCIM";
        cameraFolder.typeLabel = "Carpeta";
        cameraFolder.statusLabel = "Activo";
        cameraFolder.kind = RecoveryNodeKind::Folder;
        cameraFolder.sizeBytes = 3ull * 1024ull * 1024ull * 1024ull;
        cameraFolder.lineCount = 1;

        RecoveryNode cameraImage;
        cameraImage.name = "DSC_0001.JPG";
        cameraImage.path = cameraFolder.path + "/DSC_0001.JPG";
        cameraImage.typeLabel = "JPG";
        cameraImage.statusLabel = "Activo";
        cameraImage.kind = RecoveryNodeKind::File;
        cameraImage.sizeBytes = 5242880;
        cameraImage.lineCount = 1;
        cameraFolder.children = {cameraImage};
        activeRoot.children.append(cameraFolder);
    } else if (fsLabel == "ext4" || fsLabel == "APFS" || fsLabel == "HFS+") {
        RecoveryNode userFolder;
        userFolder.name = "Usuarios";
        userFolder.path = activeRoot.path + "/Usuarios";
        userFolder.typeLabel = "Carpeta";
        userFolder.statusLabel = "Activo";
        userFolder.kind = RecoveryNodeKind::Folder;
        userFolder.sizeBytes = 12ull * 1024ull * 1024ull * 1024ull;
        userFolder.lineCount = 1;

        RecoveryNode exportFile;
        exportFile.name = "biblioteca_fotos.mov";
        exportFile.path = userFolder.path + "/biblioteca_fotos.mov";
        exportFile.typeLabel = "MOV";
        exportFile.statusLabel = "Activo";
        exportFile.kind = RecoveryNodeKind::File;
        exportFile.sizeBytes = 420ull * 1024ull * 1024ull;
        exportFile.lineCount = 1;
        userFolder.children = {exportFile};
        activeRoot.children.append(userFolder);
    }

    root.children = {activeRoot, deletedRoot};
    return {root};
}

QString MainWindow::currentScanModeLabel() const {
    return modeCombo_ ? modeCombo_->currentText() : "Normal";
}

QString MainWindow::currentFileSystemLabel() const {
    return fileSystemCombo_ ? fileSystemCombo_->currentText() : "Auto";
}

ScanMode MainWindow::currentScanMode() const {
    if (currentScanModeLabel() == "Turbo") {
        return ScanMode::Turbo;
    }
    return ScanMode::Normal;
}

FileSystemKind MainWindow::currentFileSystemKind() const {
    if (fileSystemCombo_ && fileSystemCombo_->currentText() == "Auto" && detectedFileSystem_ != FileSystemKind::AutoDetect) {
        return detectedFileSystem_;
    }
    const QString fs = currentFileSystemLabel();
    if (fs == "NTFS") {
        return FileSystemKind::NTFS;
    }
    if (fs == "exFAT") {
        return FileSystemKind::ExFAT;
    }
    if (fs == "FAT32") {
        return FileSystemKind::FAT32;
    }
    if (fs == "ext4") {
        return FileSystemKind::EXT4;
    }
    if (fs == "APFS") {
        return FileSystemKind::APFS;
    }
    if (fs == "HFS+") {
        return FileSystemKind::HFSPlus;
    }
    if (fs == "Raw genérico") {
        return FileSystemKind::RawGeneric;
    }
    return FileSystemKind::AutoDetect;
}

void MainWindow::applyDetectedFileSystem(FileSystemKind fileSystem) {
    if (!fileSystemCombo_) {
        return;
    }
    const QString label = toDisplayString(fileSystem);
    const int index = fileSystemCombo_->findText(label);
    if (index >= 0) {
        fileSystemCombo_->setCurrentIndex(index);
    }
}

ScanProfile MainWindow::currentProfile(ScanStrategy strategy) const {
    ScanProfile profile;
    profile.mode = currentScanMode();
    profile.fileSystem = currentFileSystemKind();
    profile.strategy = strategy;
    return profile;
}

QString MainWindow::profileSummaryText(ScanStrategy strategy) const {
    return ScanPlanner::profileSummary(currentProfile(strategy));
}

QString MainWindow::enginePlanText(const ScanProfile &profile) const {
    const ScanEngineDescriptor engine = ScanEngineRegistry::describe(profile);
    QString text;
    text += "Motor seleccionado:\n";
    text += "- ID: " + engine.engineId + "\n";
    text += "- Nombre: " + engine.displayName + "\n";
    text += "- Disponible: " + QString(engine.available ? "si" : "no") + "\n";
    text += "- Sistema de archivos: " + engine.targetFileSystem + "\n";
    text += "- Estrategia: " + engine.strategyName + "\n";
    text += "- Modo: " + engine.modeName + "\n\n";
    text += engine.summary + "\n";
    text += "Fortalezas: " + engine.strengths + "\n";
    text += "Riesgos: " + engine.risks + "\n\n";
    text += "Fases:\n";
    int totalWeight = 0;
    for (const ScanPhase &phase : engine.phases) {
        totalWeight += phase.estimatedWeight;
        text += "- " + phase.name + " [" + QString::number(phase.estimatedWeight) + "%]";
        if (phase.optional) {
            text += " opcional";
        }
        text += ": " + phase.description + "\n";
    }
    text += "\nSalidas esperadas:\n";
    for (const QString &output : engine.outputs) {
        text += "- " + output + "\n";
    }
    text += "\nPeso total estimado: " + QString::number(totalWeight) + "%";
    return text.trimmed();
}

QVector<RecoveryNode> MainWindow::collectCurrentRoots() const {
    QVector<RecoveryNode> roots;
    roots.reserve(treeModel_->rowCount());
    for (int row = 0; row < treeModel_->rowCount(); ++row) {
        QStandardItem *item = treeModel_->item(row, 0);
        if (!item) {
            continue;
        }
        roots.append(item->data(Qt::UserRole + 1).value<RecoveryNode>());
    }
    return roots;
}

QStringList MainWindow::collectCheckedPaths() const {
    QStringList paths;
    QList<QStandardItem *> stack;
    for (int row = 0; row < treeModel_->rowCount(); ++row) {
        stack.append(treeModel_->item(row, 0));
    }

    while (!stack.isEmpty()) {
        QStandardItem *item = stack.takeLast();
        if (!item) {
            continue;
        }
        const RecoveryNode node = item->data(Qt::UserRole + 1).value<RecoveryNode>();
        if (item->isCheckable() && item->checkState() == Qt::Checked && !node.path.isEmpty()) {
            paths.append(node.path);
        }
        for (int row = 0; row < item->rowCount(); ++row) {
            stack.append(item->child(row, 0));
        }
    }
    return paths;
}

void MainWindow::startMetadataScan() {
    const ScanProfile profile = currentProfile(ScanStrategy::Metadata);
    summaryText_->setPlainText(
        ScanPlanner::profileSummary(profile) +
        "\n\nEscaneo por metadatos preparado.\n"
        + ScanPlanner::strategySummary(profile) +
        "\n\n" + enginePlanText(profile)
    );
    previewLabel_->setText(
        "Escaneo por metadatos listo.\n\n"
        "Cuando conectemos el motor nativo, este modo sera el principal para mantener estructura de carpetas."
    );
    statusBar()->showMessage("Perfil listo para escaneo por metadatos.");
}

void MainWindow::startSignatureScan() {
    const ScanProfile profile = currentProfile(ScanStrategy::Signatures);
    summaryText_->setPlainText(
        ScanPlanner::profileSummary(profile) +
        "\n\nEscaneo por firmas preparado.\n"
        + ScanPlanner::strategySummary(profile) +
        "\n\n" + enginePlanText(profile)
    );
    previewLabel_->setText(
        "Escaneo por firmas listo.\n\n"
        "Ideal como plan B cuando la particion no se deja montar o la estructura esta muy dañada."
    );
    statusBar()->showMessage("Perfil listo para escaneo por firmas.");
}

void MainWindow::recoverCheckedNodes() {
    if (sourcePath_.isEmpty()) {
        QMessageBox::information(this, "Recuperar marcados", "Primero analiza o abre un volumen real.");
        return;
    }

    QVector<RecoveryNode> selectedNodes;
    std::function<void(const QModelIndex &)> collectVisibleCheckedNodes = [&](const QModelIndex &proxyParent) {
        const int rows = proxyModel_->rowCount(proxyParent);
        for (int row = 0; row < rows; ++row) {
            const QModelIndex proxyIndex = proxyModel_->index(row, RecoveryTreeModel::PathColumn, proxyParent);
            if (!proxyIndex.isValid()) {
                continue;
            }
            RecoveryNode visibleNode;
            if (buildVisibleCheckedNodeFromProxy(proxyModel_, treeModel_, proxyIndex, visibleNode)) {
                if (isTransparentFolderNode(visibleNode)) {
                    for (const RecoveryNode &child : visibleNode.children) {
                        selectedNodes.append(child);
                    }
                } else {
                    selectedNodes.append(visibleNode);
                }
                continue;
            }
            collectVisibleCheckedNodes(proxyIndex);
        }
    };

    collectVisibleCheckedNodes(QModelIndex());

    if (selectedNodes.isEmpty()) {
        QMessageBox::information(this, "Recuperar marcados", "No hay archivos o carpetas marcados.");
        return;
    }

    const QString destinationRoot = QFileDialog::getExistingDirectory(
        this,
        "Elegir carpeta de destino para la recuperacion",
        QString()
    );
    if (destinationRoot.isEmpty()) {
        return;
    }

    const QString recoverySourcePath = resolveRecoverySourcePath(sourcePath_, currentFileSystemKind());
    if (recoverySourcePath.startsWith("/dev/") && !isReadableSourcePath(recoverySourcePath)) {
        if (offerAdminRelaunch(
                recoverySourcePath,
                "La recuperacion necesita acceso de lectura al dispositivo en bruto.")) {
            return;
        }
    }

    QProgressDialog progress("Preparando recuperacion...", "Cancelar", 0, 1000, this);
    progress.setWindowTitle("Recuperando archivos");
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(0);
    progress.setAutoClose(false);
    progress.setAutoReset(false);
    progress.setValue(0);
    progress.setMinimumWidth(560);

    QElapsedTimer timer;
    timer.start();

    const RecoveryExtractResult result = RecoveryExtractor::recover(
        recoverySourcePath,
        currentFileSystemKind(),
        selectedNodes,
        destinationRoot,
        [&progress, &timer](const QString &currentPath, std::uint64_t writtenBytes, std::uint64_t totalBytes, int recoveredFiles, int totalFiles) {
            const int value = totalBytes > 0
                ? static_cast<int>((writtenBytes * 1000ull) / totalBytes)
                : 0;
            const double ratio = totalBytes > 0
                ? static_cast<double>(writtenBytes) / static_cast<double>(totalBytes)
                : 0.0;
            const int percent = static_cast<int>(ratio * 100.0);
            const qint64 elapsedSeconds = timer.elapsed() / 1000;
            qint64 remainingSeconds = 0;
            if (writtenBytes > 0 && totalBytes > writtenBytes && elapsedSeconds > 0) {
                const double bytesPerSecond = static_cast<double>(writtenBytes) / static_cast<double>(elapsedSeconds);
                if (bytesPerSecond > 0.0) {
                    remainingSeconds = static_cast<qint64>(
                        static_cast<double>(totalBytes - writtenBytes) / bytesPerSecond
                    );
                }
            }

            progress.setLabelText(
                "Recuperando archivos...\n\n"
                "Elemento actual: " + QFileInfo(currentPath).fileName() +
                "\nRuta: " + currentPath +
                "\n\nProgreso: " + QString::number(percent) + "%" +
                "\nArchivos: " + QString::number(recoveredFiles) + "/" + QString::number(totalFiles) +
                "\nCopiado: " + formatBytesProgress(writtenBytes) + " / " + formatBytesProgress(totalBytes) +
                "\nTranscurrido: " + formatDurationCompact(elapsedSeconds) +
                "\nRestante aprox.: " + formatDurationCompact(remainingSeconds)
            );
            progress.setValue(value);
            QCoreApplication::processEvents();
            return !progress.wasCanceled();
        }
    );

    progress.setValue(1000);
    if (result.recoveredFiles > 0) {
        markRecoveredNodes(selectedNodes);
        updateSelectionCount();
    }

    QString message;
    message += "Archivos recuperados: " + QString::number(result.recoveredFiles) + "\n";
    message += "Carpetas creadas: " + QString::number(result.createdDirectories) + "\n";
    message += "Saltados: " + QString::number(result.skippedEntries) + "\n";
    message += "Bytes escritos: " + QString::number(static_cast<qulonglong>(result.writtenBytes));
    if (recoverySourcePath != sourcePath_) {
        message += "\nOrigen usado: " + recoverySourcePath + " (resuelto desde " + sourcePath_ + ")";
    }
    if (!result.errors.isEmpty()) {
        message += "\n\nIncidencias:\n- " + result.errors.join("\n- ");
        if (sourcePath_.startsWith("/dev/")) {
            bool permissionDenied = false;
            for (const QString &error : result.errors) {
                if (error.contains("No se pudo abrir el origen en solo lectura", Qt::CaseInsensitive) ||
                    error.contains("perm", Qt::CaseInsensitive)) {
                    permissionDenied = true;
                    break;
                }
            }
            if (permissionDenied) {
                message +=
                    "\n\nSugerencia:\n"
                    "La app no tiene permisos para leer el dispositivo en bruto.\n"
                    "Prueba a ejecutarla con sudo/pkexec o da permisos de lectura al /dev correspondiente.";
            }
        }
    }

    QMessageBox::information(
        this,
        result.success ? "Recuperacion completada" : "Recuperacion finalizada con incidencias",
        message
    );
    statusBar()->showMessage(
        "Recuperacion terminada: " + QString::number(result.recoveredFiles) + " archivo(s)."
    );
}

void MainWindow::showSupportMatrix() {
    const FileSystemCapabilities caps = ScanPlanner::capabilitiesFor(currentFileSystemKind());
    QString matrix;
    matrix += "Compatibilidad actual para " + currentFileSystemLabel() + "\n\n";
    matrix += "Metadatos: " + QString(caps.supportsMetadataScan ? "si" : "no") + "\n";
    matrix += "Firmas: " + QString(caps.supportsSignatureScan ? "si" : "no") + "\n";
    matrix += "Mantener carpetas: " + QString(caps.keepsFolderStructureWell ? "si" : "no") + "\n";
    matrix += "Recuperar borrados: " + QString(caps.deletedFileRecovery ? "si" : "no") + "\n";
    matrix += "Runlists/extents: " + QString(caps.runlistOrExtentRecovery ? "si" : "no") + "\n\n";
    matrix += "Fortalezas: " + caps.strengths + "\n";
    matrix += "Limites: " + caps.limitations + "\n";
    matrix += "Orden recomendado: " + caps.recommendedOrder + "\n";

    QMessageBox::information(this, "Matriz de compatibilidad", matrix);
}

void MainWindow::showEnginePlan() {
    const ScanProfile metadataProfile = currentProfile(ScanStrategy::Metadata);
    const ScanProfile signatureProfile = currentProfile(ScanStrategy::Signatures);
    QString text;
    text += "Plan de motor para el perfil activo\n\n";
    text += enginePlanText(metadataProfile);
    text += "\n\n";
    text += enginePlanText(signatureProfile);
    QMessageBox::information(this, "Plan del motor", text);
}

void MainWindow::updateDetailsFromSelection() {
    const QModelIndex proxyIndex = treeView_->currentIndex();
    if (!proxyIndex.isValid()) {
        detailText_->clear();
        return;
    }

    const QModelIndex sourceIndex = proxyModel_->mapToSource(proxyIndex.sibling(proxyIndex.row(), 0));
    const RecoveryNode node = treeModel_->nodeFromIndex(sourceIndex);

    const QString statusText =
        node.statusLabel.trimmed().isEmpty() ? (node.deleted ? QString("Borrado") : QString("Activo")) : node.statusLabel;
    QString details;
    details += "Nombre: " + node.name + "\n";
    details += "Ruta: " + node.path + "\n";
    details += "Tipo: " + node.typeLabel + "\n";
    details += "Estado: " + statusText + "\n";
    details += "Tamano: " + treeModel_->data(
        treeModel_->index(sourceIndex.row(), RecoveryTreeModel::SizeColumn, sourceIndex.parent())
    ).toString() + "\n";
    details += "Hijos: " + QString::number(node.children.size()) + "\n";
    details += "Lineas: " + QString::number(node.lineCount) + "\n";
    if (!node.metadataText.trimmed().isEmpty()) {
        details += "\nMetadatos:\n" + node.metadataText.trimmed() + "\n";
    }

    detailText_->setPlainText(details);
    updatePreviewForNode(node);
}

void MainWindow::filterChanged(const QString &text) {
    proxyModel_->setFilterFixedString(text);
    treeView_->expandToDepth(1);
}

void MainWindow::updateScanProfile() {
    const ScanProfile profile = currentProfile();
    if (profileLabel_) {
        QString sourceSuffix;
        if (!sourcePath_.isEmpty()) {
            sourceSuffix = " | Origen: " + sourcePath_;
        }
        profileLabel_->setText(
            "Perfil activo: " + toDisplayString(profile.mode) + " | " + toDisplayString(profile.fileSystem) +
            " | " + ScanPlanner::modeDescription(profile.mode) + sourceSuffix
        );
    }

    if (summaryText_ && treeModel_ && treeModel_->rowCount() > 0) {
        statusBar()->showMessage(
            "Perfil actualizado: " + toDisplayString(profile.mode) + " / " + toDisplayString(profile.fileSystem)
        );
    }
}

void MainWindow::toggleBranchFromContextMenu(bool checked) {
    if (!contextIndex_.isValid()) {
        return;
    }
    const QModelIndex sourceIndex = proxyModel_->mapToSource(contextIndex_);
    auto *item = treeModel_->itemFromIndex(sourceIndex.sibling(sourceIndex.row(), 0));
    if (!item) {
        return;
    }
    setBranchCheckState(item, checked);
    updateSelectionCount();
}

void MainWindow::updateSelectionCount() {
    int checkedCount = 0;
    QList<QStandardItem *> stack;
    for (int row = 0; row < treeModel_->rowCount(); ++row) {
        stack.append(treeModel_->item(row, 0));
    }

    while (!stack.isEmpty()) {
        QStandardItem *item = stack.takeLast();
        if (!item) {
            continue;
        }
        if (item->isCheckable() && item->checkState() == Qt::Checked) {
            ++checkedCount;
        }
        for (int row = 0; row < item->rowCount(); ++row) {
            stack.append(item->child(row, 0));
        }
    }

    selectionLabel_->setText(QString::number(checkedCount) + " marcados");
}

void MainWindow::setBranchCheckState(QStandardItem *item, bool checked) {
    if (!item) {
        return;
    }
    item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    for (int row = 0; row < item->rowCount(); ++row) {
        setBranchCheckState(item->child(row, 0), checked);
    }
}

int MainWindow::setDirectFilesCheckState(QStandardItem *item, bool checked) {
    if (!item) {
        return 0;
    }

    auto isTransparentFolder = [](const RecoveryNode &node) {
        if (node.kind != RecoveryNodeKind::Folder) {
            return false;
        }
        return node.name == "." ||
               node.name.compare("Root", Qt::CaseInsensitive) == 0 ||
               node.name.compare("NTFS reconstruido", Qt::CaseInsensitive) == 0 ||
               node.name.compare("Archivos y carpetas", Qt::CaseInsensitive) == 0 ||
               node.name.compare("Biblioteca de recuperacion", Qt::CaseInsensitive) == 0;
    };

    std::function<int(QStandardItem *, bool)> markLogicalDirectFiles = [&](QStandardItem *currentItem, bool currentChecked) -> int {
        if (!currentItem) {
            return 0;
        }

        int localChanged = 0;
        for (int row = 0; row < currentItem->rowCount(); ++row) {
            QStandardItem *child = currentItem->child(row, 0);
            if (!child) {
                continue;
            }
            const RecoveryNode childNode = child->data(Qt::UserRole + 1).value<RecoveryNode>();
            if (childNode.kind == RecoveryNodeKind::File) {
                child->setCheckState(currentChecked ? Qt::Checked : Qt::Unchecked);
                ++localChanged;
            }
        }

        if (localChanged > 0) {
            return localChanged;
        }

        for (int row = 0; row < currentItem->rowCount(); ++row) {
            QStandardItem *child = currentItem->child(row, 0);
            if (!child) {
                continue;
            }
            const RecoveryNode childNode = child->data(Qt::UserRole + 1).value<RecoveryNode>();
            if (isTransparentFolder(childNode)) {
                localChanged += markLogicalDirectFiles(child, currentChecked);
            }
        }
        return localChanged;
    };

    return markLogicalDirectFiles(item, checked);
}

void MainWindow::markRecoveredNodes(const QVector<RecoveryNode> &nodes) {
    QSet<QString> recoveredPaths;
    QList<RecoveryNode> stack;
    for (const RecoveryNode &node : nodes) {
        stack.append(node);
    }
    while (!stack.isEmpty()) {
        const RecoveryNode node = stack.takeLast();
        recoveredPaths.insert(node.path);
        for (const RecoveryNode &child : node.children) {
            stack.append(child);
        }
    }

    QList<QStandardItem *> itemStack;
    for (int row = 0; row < treeModel_->rowCount(); ++row) {
        itemStack.append(treeModel_->item(row, 0));
    }

    while (!itemStack.isEmpty()) {
        QStandardItem *item = itemStack.takeLast();
        if (!item) {
            continue;
        }
        RecoveryNode node = item->data(Qt::UserRole + 1).value<RecoveryNode>();
        if (recoveredPaths.contains(node.path) && node.kind == RecoveryNodeKind::File) {
            node.statusLabel = "Recuperado";
            item->setData(QVariant::fromValue(node), Qt::UserRole + 1);
            if (QStandardItem *typeItem = item->parent()
                    ? item->parent()->child(item->row(), RecoveryTreeModel::TypeColumn)
                    : treeModel_->item(item->row(), RecoveryTreeModel::TypeColumn)) {
                typeItem->setData(QVariant::fromValue(node), Qt::UserRole + 1);
            }
            if (QStandardItem *statusItem = item->parent()
                    ? item->parent()->child(item->row(), RecoveryTreeModel::StatusColumn)
                    : treeModel_->item(item->row(), RecoveryTreeModel::StatusColumn)) {
                statusItem->setText(node.statusLabel);
                statusItem->setData(QVariant::fromValue(node), Qt::UserRole + 1);
                statusItem->setForeground(QBrush(QColor("#7ec8ff")));
            }
            item->setCheckState(Qt::Unchecked);
        }
        for (int row = 0; row < item->rowCount(); ++row) {
            itemStack.append(item->child(row, 0));
        }
    }
}

void MainWindow::restoreCheckedPaths() {
    QList<QStandardItem *> stack;
    for (int row = 0; row < treeModel_->rowCount(); ++row) {
        stack.append(treeModel_->item(row, 0));
    }

    while (!stack.isEmpty()) {
        QStandardItem *item = stack.takeLast();
        if (!item) {
            continue;
        }
        const RecoveryNode node = item->data(Qt::UserRole + 1).value<RecoveryNode>();
        item->setCheckState(checkedPaths_.contains(node.path) ? Qt::Checked : Qt::Unchecked);
        for (int row = 0; row < item->rowCount(); ++row) {
            stack.append(item->child(row, 0));
        }
    }
}

bool MainWindow::offerAdminRelaunch(const QString &devicePath, const QString &reason) {
#ifdef Q_OS_LINUX
    if (geteuid() == 0) {
        return false;
    }

    QMessageBox messageBox(this);
    messageBox.setIcon(QMessageBox::Warning);
    messageBox.setWindowTitle("Permisos necesarios");
    messageBox.setText("Hace falta acceso de administrador para leer el dispositivo.");
    messageBox.setInformativeText(
        "Origen: " + devicePath + "\n\n" +
        reason + "\n\n" +
        "Puedes relanzar ahora la aplicacion como administrador usando pkexec."
    );
    QPushButton *relaunchButton = messageBox.addButton("Reabrir como administrador", QMessageBox::AcceptRole);
    messageBox.addButton(QMessageBox::Cancel);
    messageBox.exec();

    if (messageBox.clickedButton() != relaunchButton) {
        return false;
    }

    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString display = env.value("DISPLAY");
    const QString xauthority = env.value("XAUTHORITY");
    const QString appPath = QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath();
    const QString pkexecPath = QStandardPaths::findExecutable("pkexec");
    if (pkexecPath.isEmpty() || appPath.isEmpty()) {
        QMessageBox::warning(
            this,
            "No se pudo relanzar",
            "No se encontro pkexec o la ruta del ejecutable no es valida.\n\n"
            "Prueba a ejecutar la aplicacion manualmente como administrador."
        );
        return false;
    }

    const QStringList appArgs = QCoreApplication::arguments();
    QStringList forwardedArgs;
    for (int index = 1; index < appArgs.size(); ++index) {
        forwardedArgs << appArgs.at(index);
    }

    auto shellQuoted = [](const QString &value) {
        QString escaped = value;
        escaped.replace('\'', "'\"'\"'");
        return "'" + escaped + "'";
    };

    QStringList candidateCommands;
    QString command =
        "export QT_QPA_PLATFORM=xcb;";
    if (!display.isEmpty()) {
        command += " export DISPLAY=" + shellQuoted(display) + ";";
    }
    if (!xauthority.isEmpty()) {
        command += " export XAUTHORITY=" + shellQuoted(xauthority) + ";";
    }
    command += " exec " + shellQuoted(appPath);
    for (const QString &arg : forwardedArgs) {
        command += " " + shellQuoted(arg);
    }
    candidateCommands << command;

    QStringList pkexecEnvArgs;
    pkexecEnvArgs << "env" << "QT_QPA_PLATFORM=xcb";
    if (!display.isEmpty()) {
        pkexecEnvArgs << ("DISPLAY=" + display);
    }
    if (!xauthority.isEmpty()) {
        pkexecEnvArgs << ("XAUTHORITY=" + xauthority);
    }
    pkexecEnvArgs << appPath;
    pkexecEnvArgs.append(forwardedArgs);

    bool started = false;
    if (QProcess::startDetached(pkexecPath, {"sh", "-c", command})) {
        started = true;
    } else if (QProcess::startDetached(pkexecPath, pkexecEnvArgs)) {
        started = true;
    }

    if (!started) {
        QMessageBox::warning(
            this,
            "No se pudo relanzar",
            "No se pudo iniciar pkexec automaticamente.\n\n"
            "Prueba a ejecutar la aplicacion con sudo o pkexec manualmente."
        );
        return false;
    }

    QMessageBox::information(
        this,
        "Solicitud enviada",
        "Se ha solicitado abrir una nueva instancia como administrador.\n\n"
        "Si aparece la ventana autenticada, ya puedes cerrar esta instancia manualmente.\n"
        "Si no aparece nada, prueba a lanzarla desde terminal con:\n\n"
        "pkexec " + appPath
    );
    return true;
#else
    Q_UNUSED(devicePath);
    Q_UNUSED(reason);
    return false;
#endif
}

void MainWindow::updatePreviewForNode(const RecoveryNode &node) {
    const QFileInfo info(node.path);

    if (node.kind == RecoveryNodeKind::Folder) {
        showPreviewText(
            "Carpeta",
            "Nombre: " + node.name + "\n"
            "Elementos conocidos: " + QString::number(node.children.size()) + "\n"
            "Tamano acumulado estimado: " + QString::number(static_cast<qulonglong>(node.sizeBytes)) + " bytes\n"
            "Ruta: " + node.path
        );
        return;
    }

    if (info.exists() && info.isFile() && isImageFileName(node.name)) {
        QImageReader reader(info.absoluteFilePath());
        const QSize imageSize = reader.size();
        const QImage image = reader.read();
        if (!image.isNull()) {
            const int maxWidth = qMax(220, previewLabel_->width() - 24);
            const int maxHeight = qMax(180, previewLabel_->height() - 24);
            const QPixmap pixmap = QPixmap::fromImage(image).scaled(
                maxWidth,
                maxHeight,
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation
            );
            previewLabel_->setAlignment(Qt::AlignCenter);
            previewLabel_->setPixmap(pixmap);
            previewLabel_->setText(
                "Imagen real\n" + node.name + "\n" +
                QString::number(imageSize.width()) + "x" + QString::number(imageSize.height()) + " px"
            );
            previewLabel_->setToolTip(node.path);
            return;
        }
    }

    if (info.exists() && info.isFile() && isTextLikeFileName(node.name)) {
        QFile file(info.absoluteFilePath());
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString text = QString::fromUtf8(file.read(4096));
            if (text.trimmed().isEmpty()) {
                text = "[El archivo existe pero no muestra texto legible en el primer bloque leido]";
            }
            showPreviewText("Texto real", text);
            previewLabel_->setToolTip(node.path);
            return;
        }
    }

    showPreviewText(friendlyKindLabel(node), buildUserFacingFileFacts(node, info));
    previewLabel_->setToolTip(node.path);
}

void MainWindow::showPreviewText(const QString &title, const QString &body) {
    previewLabel_->setPixmap(QPixmap());
    previewLabel_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    previewLabel_->setText(title + "\n\n" + body.trimmed());
}

QString MainWindow::buildUserFacingFileFacts(const RecoveryNode &node, const QFileInfo &info) const {
    QString text;
    text += "Nombre: " + node.name + "\n";
    text += "Tipo: " + friendlyKindLabel(node) + "\n";
    text += "Tamano estimado: " + QString::number(static_cast<qulonglong>(node.sizeBytes)) + " bytes\n";
    text += "Estado: " + QString(node.deleted ? "Borrado" : "Activo/Detectado") + "\n";
    if (info.exists()) {
        text += "Archivo real accesible: si\n";
        text += "Ruta local: " + info.absoluteFilePath() + "\n";
    } else {
        text += "Archivo real accesible: no\n";
        text += "Ruta virtual de recuperacion: " + node.path + "\n";
    }
    if (!node.metadataText.trimmed().isEmpty()) {
        text += "\nDatos tecnicos utiles:\n" + node.metadataText.trimmed();
    } else {
        text += "\nNo hay metadatos ampliados para este elemento todavia.";
    }
    return text.trimmed();
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    if (!treeView_ || !proxyModel_ || !treeModel_) {
        return;
    }
    const QModelIndex proxyIndex = treeView_->currentIndex();
    if (!proxyIndex.isValid()) {
        return;
    }
    const QModelIndex sourceIndex = proxyModel_->mapToSource(proxyIndex.sibling(proxyIndex.row(), 0));
    const RecoveryNode node = treeModel_->nodeFromIndex(sourceIndex);
    if (!node.name.isEmpty()) {
        updatePreviewForNode(node);
    }
}

