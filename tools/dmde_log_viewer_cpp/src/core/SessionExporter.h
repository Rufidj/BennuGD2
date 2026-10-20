#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include "RecoveryNode.h"

class SessionExporter {
public:
    static bool saveToFile(
        const QString &filePath,
        const QString &sourceRoot,
        const QVector<RecoveryNode> &roots,
        const QStringList &checkedPaths,
        QString *errorMessage = nullptr
    );
};
