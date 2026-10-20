#pragma once

#include <functional>
#include <QString>
#include <QStringList>
#include <cstdint>

#include "ScanProfile.h"

struct DiskDoctorResult {
    bool success = false;
    QString sourcePath;
    FileSystemKind detectedFileSystem = FileSystemKind::AutoDetect;
    QString detectedLabel;
    QString summary;
    QString details;
    QStringList warnings;
    QStringList repairPlan;
    int readSamplesOk = 0;
    int readSamplesFailed = 0;
    std::uint64_t estimatedSizeBytes = 0;
};

class DiskDoctor {
public:
    using ProgressCallback = std::function<bool(int percent, const QString &phase, const QString &detail)>;

    static DiskDoctorResult diagnose(const QString &sourcePath, ProgressCallback progress = {});
};
