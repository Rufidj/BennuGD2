#pragma once

#include <QString>
#include <QStringList>

#include "ScanProfile.h"

struct RepairAdvisorResult {
    QString operatingSystemLabel;
    FileSystemKind fileSystem = FileSystemKind::AutoDetect;
    QString sourcePath;
    bool hasDirectCommandSupport = false;
    QString summary;
    QString safetyNotes;
    QStringList suggestedCommands;
    QStringList guidedSteps;
};

class RepairAdvisor {
public:
    static RepairAdvisorResult build(const QString &sourcePath, FileSystemKind fileSystem);
};
