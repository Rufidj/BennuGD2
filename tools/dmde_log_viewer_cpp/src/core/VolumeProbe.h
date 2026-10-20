#pragma once

#include <QString>
#include <cstdint>

#include "ScanProfile.h"

struct VolumeProbeResult {
    bool success = false;
    QString sourcePath;
    FileSystemKind detectedFileSystem = FileSystemKind::AutoDetect;
    QString detectedLabel;
    QString summary;
    QString details;
    std::uint64_t blockOrSectorSize = 0;
    std::uint64_t clusterSize = 0;
    std::uint64_t totalUnits = 0;
};

class VolumeProbe {
public:
    static VolumeProbeResult probe(const QString &filePath);
};
