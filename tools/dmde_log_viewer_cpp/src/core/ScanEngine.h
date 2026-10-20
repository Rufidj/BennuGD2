#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include "ScanProfile.h"

struct ScanPhase {
    QString name;
    QString description;
    int estimatedWeight = 0;
    bool optional = false;
};

struct ScanEngineDescriptor {
    QString engineId;
    QString displayName;
    QString targetFileSystem;
    QString strategyName;
    QString modeName;
    QString summary;
    QString strengths;
    QString risks;
    bool available = false;
    QVector<ScanPhase> phases;
    QStringList outputs;
};

class ScanEngineRegistry {
public:
    static ScanEngineDescriptor describe(const ScanProfile &profile);
};
