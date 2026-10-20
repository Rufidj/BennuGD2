#pragma once

#include <QString>
#include <QVector>

#include "RecoveryNode.h"

struct ParsedLog {
    QString sourceFile;
    QString detectedDevice;
    QString summaryText;
    QVector<RecoveryNode> roots;
    int pathEntries = 0;
    bool scanStateOnly = false;
};

class LogParser {
public:
    static ParsedLog loadFromFile(const QString &filePath);
};
