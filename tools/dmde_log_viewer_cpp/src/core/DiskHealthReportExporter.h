#pragma once

#include <QString>

#include "DiskHealthReport.h"

class DiskHealthReportExporter {
public:
    static bool exportJson(const DiskHealthReport &report, const QString &filePath, QString *errorMessage = nullptr);
    static bool exportHtml(const DiskHealthReport &report, const QString &filePath, QString *errorMessage = nullptr);
};
