#include "SmartProbe.h"

#include <QProcess>
#include <QStringList>

namespace {
QString extractAfterColon(const QString &line) {
    const int idx = line.indexOf(':');
    if (idx < 0) {
        return {};
    }
    return line.mid(idx + 1).trimmed();
}

QString findLineValue(const QStringList &lines, const QStringList &prefixes) {
    for (const QString &line : lines) {
        for (const QString &prefix : prefixes) {
            if (line.startsWith(prefix, Qt::CaseInsensitive)) {
                return extractAfterColon(line);
            }
        }
    }
    return {};
}

QString findAttributeRaw(const QStringList &lines, const QStringList &attributeNames) {
    for (const QString &line : lines) {
        for (const QString &attribute : attributeNames) {
            if (line.contains(attribute, Qt::CaseInsensitive)) {
                const QStringList parts = line.simplified().split(' ', Qt::SkipEmptyParts);
                if (!parts.isEmpty()) {
                    return parts.last();
                }
            }
        }
    }
    return {};
}
} // namespace

SmartProbeResult SmartProbe::probe(const QString &sourcePath) {
    SmartProbeResult result;
    result.sourcePath = sourcePath;

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    QProcess process;
    process.start("smartctl", {"-H", "-A", sourcePath});
    if (!process.waitForFinished(8000) || process.exitStatus() != QProcess::NormalExit) {
        result.summary = "No se pudo ejecutar smartctl.";
        result.details = "Comprueba que smartmontools este instalado y que el disco permita SMART.";
        result.warnings << "SMART no disponible o comando no encontrado.";
        return result;
    }

    const QString stdoutText = QString::fromUtf8(process.readAllStandardOutput());
    const QString stderrText = QString::fromUtf8(process.readAllStandardError());
    const QString combined = stdoutText + (stderrText.isEmpty() ? QString() : ("\n" + stderrText));
    const QStringList lines = combined.split('\n', Qt::SkipEmptyParts);

    result.healthLabel = findLineValue(lines, {
        "SMART overall-health self-assessment test result",
        "SMART Health Status",
        "SMART overall-health"
    });
    result.temperatureLabel = findAttributeRaw(lines, {"Temperature_Celsius", "Airflow_Temperature_Cel", "Temperature_Internal"});
    const QString reallocated = findAttributeRaw(lines, {"Reallocated_Sector_Ct"});
    const QString pending = findAttributeRaw(lines, {"Current_Pending_Sector"});
    const QString offline = findAttributeRaw(lines, {"Offline_Uncorrectable"});

    if (result.healthLabel.isEmpty()) {
        result.healthLabel = "No disponible";
    }
    if (result.temperatureLabel.isEmpty()) {
        result.temperatureLabel = "No disponible";
    }

    if (!reallocated.isEmpty() && reallocated != "0") {
        result.warnings << "Sectores reasignados detectados: " + reallocated;
    }
    if (!pending.isEmpty() && pending != "0") {
        result.warnings << "Sectores pendientes detectados: " + pending;
    }
    if (!offline.isEmpty() && offline != "0") {
        result.warnings << "Errores no corregibles offline: " + offline;
    }
    if (result.healthLabel.contains("FAIL", Qt::CaseInsensitive) || result.healthLabel.contains("BAD", Qt::CaseInsensitive)) {
        result.warnings << "El estado SMART general no es bueno.";
    }

    result.success = !combined.trimmed().isEmpty();
    result.summary =
        "SMART consultado para " + sourcePath + "\n\n"
        "Salud: " + result.healthLabel + "\n"
        "Temperatura: " + result.temperatureLabel;

    QString details;
    details += "Origen: " + sourcePath + "\n";
    details += "Estado SMART: " + result.healthLabel + "\n";
    details += "Temperatura: " + result.temperatureLabel + "\n";
    if (!reallocated.isEmpty()) {
        details += "Reallocated_Sector_Ct: " + reallocated + "\n";
    }
    if (!pending.isEmpty()) {
        details += "Current_Pending_Sector: " + pending + "\n";
    }
    if (!offline.isEmpty()) {
        details += "Offline_Uncorrectable: " + offline + "\n";
    }
    if (!result.warnings.isEmpty()) {
        details += "\nAlertas:\n- " + result.warnings.join("\n- ") + "\n";
    }
    details += "\nSalida SMART:\n" + combined.trimmed();
    result.details = details.trimmed();
    return result;
#else
    result.summary = "SMART aun no esta integrado en este sistema operativo.";
    result.details = "La base de la app sigue siendo multiplataforma, pero la lectura SMART aun no esta implementada aqui.";
    result.warnings << "SMART no soportado en esta plataforma por ahora.";
    return result;
#endif
}
