#pragma once

#include <QString>

#include "DiskDoctor.h"
#include "SmartProbe.h"
#include "SurfaceScanner.h"

struct DiskHealthReport {
    bool success = false;
    QString sourcePath;
    QString summary;
    QString details;
    QString healthBadge;
    DiskDoctorResult doctor;
    SmartProbeResult smart;
    SurfaceScanResult surfaceQuick;
};

class DiskHealthReportBuilder {
public:
    static DiskHealthReport build(const QString &sourcePath);
};
