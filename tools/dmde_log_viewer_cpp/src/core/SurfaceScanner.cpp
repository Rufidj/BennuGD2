#include "SurfaceScanner.h"

#include <QFile>

namespace {
SurfaceScanResult runScan(
    const QString &sourcePath,
    std::uint64_t estimatedTotalBytes,
    std::uint64_t readChunkBytes,
    std::uint64_t strideBytes,
    SurfaceScanner::ProgressCallback progress
) {
    SurfaceScanResult result;
    result.sourcePath = sourcePath;
    result.estimatedTotalBytes = estimatedTotalBytes;

    QFile file(sourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        result.summary = "No se pudo abrir el origen para escaneo de superficie.";
        result.details = file.errorString();
        return result;
    }

    std::uint64_t offset = 0;
    while (true) {
        if (estimatedTotalBytes > 0 && offset >= estimatedTotalBytes) {
            break;
        }

        if (progress) {
            const QString status =
                "Offset " + QString::number(static_cast<qulonglong>(offset)) +
                " | OK " + QString::number(result.chunksOk) +
                " | Fallos " + QString::number(result.chunksFailed);
            if (!progress(result.bytesProcessed, estimatedTotalBytes, result.chunksOk, result.chunksFailed, status)) {
                result.canceled = true;
                break;
            }
        }

        if (!file.seek(static_cast<qint64>(offset))) {
            ++result.chunksFailed;
            result.issueOffsets.append(QString::number(static_cast<qulonglong>(offset)));
            break;
        }

        const QByteArray data = file.read(static_cast<qint64>(readChunkBytes));
        if (data.isEmpty()) {
            break;
        }

        if (data.size() < static_cast<int>(readChunkBytes) &&
            estimatedTotalBytes > 0 &&
            offset + static_cast<std::uint64_t>(data.size()) < estimatedTotalBytes) {
            ++result.chunksFailed;
            result.issueOffsets.append(QString::number(static_cast<qulonglong>(offset)));
        } else {
            ++result.chunksOk;
        }

        result.bytesProcessed = offset + static_cast<std::uint64_t>(data.size());

        if (data.size() < static_cast<int>(readChunkBytes) && estimatedTotalBytes == 0) {
            break;
        }

        if (strideBytes == 0) {
            offset += static_cast<std::uint64_t>(data.size());
        } else {
            offset += strideBytes;
        }
    }

    result.success = !result.canceled && result.chunksFailed == 0 && result.chunksOk > 0;
    result.summary =
        QString(result.canceled ? "Escaneo de superficie cancelado." : "Escaneo de superficie completado.") +
        "\n\nChunks correctos: " + QString::number(result.chunksOk) +
        "\nChunks con incidencias: " + QString::number(result.chunksFailed) +
        "\nBytes procesados: " + QString::number(static_cast<qulonglong>(result.bytesProcessed));

    QString details;
    details += "Origen: " + sourcePath + "\n";
    details += "Estimacion total: " + QString::number(static_cast<qulonglong>(estimatedTotalBytes)) + " bytes\n";
    details += "Procesado: " + QString::number(static_cast<qulonglong>(result.bytesProcessed)) + " bytes\n";
    details += "Lecturas correctas: " + QString::number(result.chunksOk) + "\n";
    details += "Lecturas con incidencias: " + QString::number(result.chunksFailed);
    if (!result.issueOffsets.isEmpty()) {
        details += "\n\nOffsets con incidencia:\n- " + result.issueOffsets.join("\n- ");
    }
    result.details = details;
    return result;
}
} // namespace

SurfaceScanResult SurfaceScanner::scanQuick(
    const QString &sourcePath,
    std::uint64_t estimatedTotalBytes,
    ProgressCallback progress
) {
    return runScan(sourcePath, estimatedTotalBytes, 64ull * 1024ull, 16ull * 1024ull * 1024ull, std::move(progress));
}

SurfaceScanResult SurfaceScanner::scanFull(
    const QString &sourcePath,
    std::uint64_t estimatedTotalBytes,
    ProgressCallback progress
) {
    return runScan(sourcePath, estimatedTotalBytes, 1024ull * 1024ull, 0, std::move(progress));
}
