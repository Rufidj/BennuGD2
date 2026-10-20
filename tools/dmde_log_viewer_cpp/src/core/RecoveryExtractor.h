#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>
#include <cstdint>

#include "RecoveryNode.h"
#include "ScanProfile.h"

struct RecoveryExtractResult {
    bool success = false;
    int recoveredFiles = 0;
    int createdDirectories = 0;
    int skippedEntries = 0;
    std::uint64_t writtenBytes = 0;
    QStringList errors;
};

class RecoveryExtractor {
public:
    using ProgressCallback = std::function<bool(
        const QString &currentPath,
        std::uint64_t writtenBytes,
        std::uint64_t totalBytes,
        int recoveredFiles,
        int totalFiles
    )>;

    static RecoveryExtractResult recover(
        const QString &sourcePath,
        FileSystemKind fileSystem,
        const QVector<RecoveryNode> &nodes,
        const QString &destinationRoot,
        ProgressCallback progress = {}
    );
};
