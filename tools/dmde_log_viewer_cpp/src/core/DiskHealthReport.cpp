#include "DiskHealthReport.h"

#include <QFileInfo>

#include "VolumeProbe.h"

namespace {
std::uint64_t estimateSourceBytes(const QString &sourcePath) {
    const VolumeProbeResult probe = VolumeProbe::probe(sourcePath);
    if (probe.blockOrSectorSize > 0 && probe.totalUnits > 0) {
        return probe.blockOrSectorSize * probe.totalUnits;
    }
    QFileInfo info(sourcePath);
    if (info.exists() && info.isFile()) {
        return static_cast<std::uint64_t>(info.size());
    }
    return 0;
}

QString computeBadge(const DiskDoctorResult &doctor, const SmartProbeResult &smart, const SurfaceScanResult &surface) {
    if (doctor.readSamplesFailed > 0 || surface.chunksFailed > 0) {
        return "Riesgo alto";
    }
    if (!smart.warnings.isEmpty()) {
        return "Atencion";
    }
    if (doctor.success || smart.success || surface.success) {
        return "Estable";
    }
    return "Desconocido";
}
} // namespace

DiskHealthReport DiskHealthReportBuilder::build(const QString &sourcePath) {
    DiskHealthReport report;
    report.sourcePath = sourcePath;
    report.doctor = DiskDoctor::diagnose(sourcePath);
    report.smart = SmartProbe::probe(sourcePath);
    report.surfaceQuick = SurfaceScanner::scanQuick(sourcePath, estimateSourceBytes(sourcePath));

    report.healthBadge = computeBadge(report.doctor, report.smart, report.surfaceQuick);
    report.success = report.doctor.success || report.smart.success || report.surfaceQuick.success;

    QString summary;
    summary += "Panel de salud del disco\n\n";
    summary += "Origen: " + sourcePath + "\n";
    summary += "Estado general: " + report.healthBadge + "\n";
    summary += "Sistema detectado: " + (
        report.doctor.detectedLabel.isEmpty() ? QString("Desconocido") : report.doctor.detectedLabel
    ) + "\n";
    summary += "SMART: " + (report.smart.healthLabel.isEmpty() ? QString("No disponible") : report.smart.healthLabel) + "\n";
    summary += "Temperatura: " + (report.smart.temperatureLabel.isEmpty() ? QString("No disponible") : report.smart.temperatureLabel) + "\n";
    summary += "Lecturas de muestra fallidas: " + QString::number(report.doctor.readSamplesFailed) + "\n";
    summary += "Fallos en superficie rapida: " + QString::number(report.surfaceQuick.chunksFailed);
    report.summary = summary;

    QString details;
    details += "[Diagnostico base]\n" + report.doctor.details + "\n\n";
    details += "[SMART]\n" + report.smart.details + "\n\n";
    details += "[Superficie rapida]\n" + report.surfaceQuick.details;
    report.details = details.trimmed();
    return report;
}
