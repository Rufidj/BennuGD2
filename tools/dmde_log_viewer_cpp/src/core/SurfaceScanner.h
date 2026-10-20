#pragma once

#include <QString>
#include <QStringList>
#include <functional>
#include <cstdint>

struct SurfaceScanResult {
    bool success = false;
    bool canceled = false;
    QString sourcePath;
    std::uint64_t estimatedTotalBytes = 0;
    std::uint64_t bytesProcessed = 0;
    int chunksOk = 0;
    int chunksFailed = 0;
    QString summary;
    QString details;
    QStringList issueOffsets;
};

class SurfaceScanner {
public:
    using ProgressCallback = std::function<bool(
        std::uint64_t processedBytes,
        std::uint64_t totalBytes,
        int okChunks,
        int failedChunks,
        const QString &currentStatus
    )>;

    static SurfaceScanResult scanQuick(
        const QString &sourcePath,
        std::uint64_t estimatedTotalBytes,
        ProgressCallback progress = {}
    );

    static SurfaceScanResult scanFull(
        const QString &sourcePath,
        std::uint64_t estimatedTotalBytes,
        ProgressCallback progress = {}
    );
};
