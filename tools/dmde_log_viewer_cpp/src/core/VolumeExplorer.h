#pragma once

#include <functional>
#include <QString>
#include <QVector>

#include "RecoveryNode.h"
#include "ScanProfile.h"

struct VolumeExploreResult {
    bool success = false;
    QString sourcePath;
    FileSystemKind fileSystem = FileSystemKind::AutoDetect;
    QString summary;
    QString details;
    QVector<RecoveryNode> roots;
};

struct VolumeExploreOptions {
    int maxEntries = 256;
    int maxDepth = 2;
    bool includeDeleted = true;
};

class VolumeExplorer {
public:
    using FastScanProgressCallback = std::function<void(int scannedRecords, int totalRecords, int foundActive, int foundDeleted)>;

    static VolumeExploreResult explore(
        const QString &filePath,
        FileSystemKind fileSystem,
        int maxEntries = 256,
        int maxDepth = 2,
        bool includeDeleted = true
    );

    static VolumeExploreResult exploreFastNtfsMft(
        const QString &filePath,
        int maxEntries = 12000,
        int maxRecords = 120000,
        bool includeDeleted = false,
        FastScanProgressCallback progressCallback = {}
    );
};
