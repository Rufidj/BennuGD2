#pragma once

#include <QString>
#include <QStringList>

struct SmartProbeResult {
    bool success = false;
    QString sourcePath;
    QString summary;
    QString details;
    QString healthLabel;
    QString temperatureLabel;
    QStringList warnings;
};

class SmartProbe {
public:
    static SmartProbeResult probe(const QString &sourcePath);
};
