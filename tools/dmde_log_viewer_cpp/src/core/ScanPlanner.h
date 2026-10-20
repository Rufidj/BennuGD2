#pragma once

#include <QString>

#include "ScanProfile.h"

struct FileSystemCapabilities {
    QString summary;
    QString strengths;
    QString limitations;
    QString recommendedOrder;
    bool supportsMetadataScan = false;
    bool supportsSignatureScan = true;
    bool keepsFolderStructureWell = false;
    bool deletedFileRecovery = false;
    bool runlistOrExtentRecovery = false;
};

class ScanPlanner {
public:
    static FileSystemCapabilities capabilitiesFor(FileSystemKind fileSystem);
    static QString modeDescription(ScanMode mode);
    static QString profileSummary(const ScanProfile &profile);
    static QString strategySummary(const ScanProfile &profile);
};
