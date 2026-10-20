#include "DiskHealthReportExporter.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

namespace {
QJsonArray toJsonArray(const QStringList &values) {
    QJsonArray array;
    for (const QString &value : values) {
        array.append(value);
    }
    return array;
}

QJsonObject doctorToJson(const DiskDoctorResult &doctor) {
    QJsonObject object;
    object.insert("success", doctor.success);
    object.insert("source_path", doctor.sourcePath);
    object.insert("detected_label", doctor.detectedLabel);
    object.insert("summary", doctor.summary);
    object.insert("details", doctor.details);
    object.insert("warnings", toJsonArray(doctor.warnings));
    object.insert("repair_plan", toJsonArray(doctor.repairPlan));
    object.insert("read_samples_ok", doctor.readSamplesOk);
    object.insert("read_samples_failed", doctor.readSamplesFailed);
    object.insert("estimated_size_bytes", QString::number(static_cast<qulonglong>(doctor.estimatedSizeBytes)));
    return object;
}

QJsonObject smartToJson(const SmartProbeResult &smart) {
    QJsonObject object;
    object.insert("success", smart.success);
    object.insert("source_path", smart.sourcePath);
    object.insert("summary", smart.summary);
    object.insert("details", smart.details);
    object.insert("health_label", smart.healthLabel);
    object.insert("temperature_label", smart.temperatureLabel);
    object.insert("warnings", toJsonArray(smart.warnings));
    return object;
}

QJsonObject surfaceToJson(const SurfaceScanResult &surface) {
    QJsonObject object;
    object.insert("success", surface.success);
    object.insert("canceled", surface.canceled);
    object.insert("source_path", surface.sourcePath);
    object.insert("estimated_total_bytes", QString::number(static_cast<qulonglong>(surface.estimatedTotalBytes)));
    object.insert("bytes_processed", QString::number(static_cast<qulonglong>(surface.bytesProcessed)));
    object.insert("chunks_ok", surface.chunksOk);
    object.insert("chunks_failed", surface.chunksFailed);
    object.insert("summary", surface.summary);
    object.insert("details", surface.details);
    object.insert("issue_offsets", toJsonArray(surface.issueOffsets));
    return object;
}

QString htmlEscaped(QString value) {
    value.replace('&', "&amp;");
    value.replace('<', "&lt;");
    value.replace('>', "&gt;");
    return value;
}

QString htmlList(const QStringList &values) {
    if (values.isEmpty()) {
        return "<p>Sin elementos.</p>";
    }
    QString html = "<ul>";
    for (const QString &value : values) {
        html += "<li>" + htmlEscaped(value) + "</li>";
    }
    html += "</ul>";
    return html;
}

bool writeTextFile(const QString &filePath, const QString &text, QString *errorMessage) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = "No se pudo escribir el archivo: " + filePath;
        }
        return false;
    }
    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    stream << text;
    return true;
}
} // namespace

bool DiskHealthReportExporter::exportJson(const DiskHealthReport &report, const QString &filePath, QString *errorMessage) {
    QJsonObject root;
    root.insert("success", report.success);
    root.insert("source_path", report.sourcePath);
    root.insert("summary", report.summary);
    root.insert("details", report.details);
    root.insert("health_badge", report.healthBadge);
    root.insert("doctor", doctorToJson(report.doctor));
    root.insert("smart", smartToJson(report.smart));
    root.insert("surface_quick", surfaceToJson(report.surfaceQuick));

    const QString text = QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return writeTextFile(filePath, text, errorMessage);
}

bool DiskHealthReportExporter::exportHtml(const DiskHealthReport &report, const QString &filePath, QString *errorMessage) {
    QString html;
    html += "<!doctype html><html><head><meta charset=\"utf-8\">";
    html += "<title>Informe de salud del disco</title>";
    html += "<style>";
    html += "body{font-family:Segoe UI,Arial,sans-serif;background:#111;color:#eee;margin:24px;}";
    html += "h1,h2{color:#7ee787;} .badge{display:inline-block;padding:6px 12px;border-radius:999px;background:#1f6f43;color:#fff;font-weight:bold;}";
    html += "pre{white-space:pre-wrap;background:#1a1a1a;border:1px solid #333;padding:12px;border-radius:8px;}";
    html += "section{margin:18px 0;padding:16px;border:1px solid #2a2a2a;border-radius:10px;background:#151515;}";
    html += "</style></head><body>";
    html += "<h1>Informe de salud del disco</h1>";
    html += "<p><strong>Origen:</strong> " + htmlEscaped(report.sourcePath) + "</p>";
    html += "<p><strong>Estado general:</strong> <span class=\"badge\">" + htmlEscaped(report.healthBadge) + "</span></p>";
    html += "<section><h2>Resumen</h2><pre>" + htmlEscaped(report.summary) + "</pre></section>";
    html += "<section><h2>Diagnostico base</h2><pre>" + htmlEscaped(report.doctor.details) + "</pre>";
    html += "<h3>Alertas</h3>" + htmlList(report.doctor.warnings);
    html += "<h3>Plan sugerido</h3>" + htmlList(report.doctor.repairPlan) + "</section>";
    html += "<section><h2>SMART</h2><pre>" + htmlEscaped(report.smart.details) + "</pre>";
    html += "<h3>Alertas</h3>" + htmlList(report.smart.warnings) + "</section>";
    html += "<section><h2>Superficie rapida</h2><pre>" + htmlEscaped(report.surfaceQuick.details) + "</pre>";
    html += "<h3>Offsets con incidencia</h3>" + htmlList(report.surfaceQuick.issueOffsets) + "</section>";
    html += "<section><h2>Detalle consolidado</h2><pre>" + htmlEscaped(report.details) + "</pre></section>";
    html += "</body></html>";
    return writeTextFile(filePath, html, errorMessage);
}
