#include "RecoveryExtractor.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

namespace {
struct Fat32ExtractContext {
    std::uint64_t bytesPerSector = 0;
    std::uint64_t sectorsPerCluster = 0;
    std::uint64_t firstDataSector = 0;
    QByteArray fatTable;
};

struct ExfatExtractContext {
    std::uint64_t bytesPerSector = 0;
    std::uint64_t sectorsPerCluster = 0;
    std::uint64_t clusterHeapOffset = 0;
    QByteArray fatTable;
};

struct ExtractContext {
    QFile sourceFile;
    QString destinationRoot;
    FileSystemKind fileSystem = FileSystemKind::AutoDetect;
    Fat32ExtractContext fat32;
    ExfatExtractContext exfat;
    std::uint64_t ntfsClusterSize = 0;
    std::uint64_t totalBytes = 0;
    int totalFiles = 0;
    RecoveryExtractResult result;
    RecoveryExtractor::ProgressCallback progress;
};

std::uint16_t readLe16(const QByteArray &data, int offset) {
    if (offset + 2 > data.size()) {
        return 0;
    }
    return static_cast<std::uint16_t>(
        static_cast<unsigned char>(data[offset]) |
        (static_cast<unsigned char>(data[offset + 1]) << 8)
    );
}

std::uint8_t readU8(const QByteArray &data, int offset) {
    if (offset >= data.size()) {
        return 0;
    }
    return static_cast<std::uint8_t>(static_cast<unsigned char>(data[offset]));
}

std::uint64_t readLe64(const QByteArray &data, int offset) {
    if (offset + 8 > data.size()) {
        return 0;
    }
    std::uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= (static_cast<std::uint64_t>(static_cast<unsigned char>(data[offset + i])) << (8 * i));
    }
    return value;
}

std::uint64_t ntfsClusterSize(QFile &file) {
    file.seek(0);
    const QByteArray boot = file.read(512);
    if (boot.size() < 16) {
        return 0;
    }
    const std::uint64_t bytesPerSector = readLe16(boot, 11);
    const std::uint64_t sectorsPerCluster = readU8(boot, 13);
    return bytesPerSector * sectorsPerCluster;
}

std::uint32_t readLe32(const QByteArray &data, int offset) {
    if (offset + 4 > data.size()) {
        return 0;
    }
    return static_cast<std::uint32_t>(static_cast<unsigned char>(data[offset])) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(data[offset + 1])) << 8) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(data[offset + 2])) << 16) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(data[offset + 3])) << 24);
}

std::uint64_t clusterOffset(
    std::uint32_t cluster,
    std::uint64_t firstDataSector,
    std::uint64_t sectorsPerCluster,
    std::uint64_t bytesPerSector
) {
    if (cluster < 2) {
        return 0;
    }
    return (firstDataSector + (static_cast<std::uint64_t>(cluster) - 2) * sectorsPerCluster) * bytesPerSector;
}

std::uint32_t fat32ClusterValue(const Fat32ExtractContext &context, std::uint32_t cluster) {
    const int offset = static_cast<int>(cluster * 4);
    if (offset + 4 > context.fatTable.size()) {
        return 0x0FFFFFFF;
    }
    return readLe32(context.fatTable, offset) & 0x0FFFFFFF;
}

std::uint32_t exfatClusterValue(const ExfatExtractContext &context, std::uint32_t cluster) {
    const int offset = static_cast<int>(cluster * 4);
    if (offset + 4 > context.fatTable.size()) {
        return 0xFFFFFFFFu;
    }
    return readLe32(context.fatTable, offset);
}

void accumulateNodeTotals(const RecoveryNode &node, std::uint64_t &bytes, int &files) {
    if (node.kind == RecoveryNodeKind::File) {
        bytes += node.sizeBytes;
        ++files;
        return;
    }
    for (const RecoveryNode &child : node.children) {
        accumulateNodeTotals(child, bytes, files);
    }
}

QString sanitizedName(const QString &name) {
    QString value = name;
    value.replace(" [borrado]", "");
    return value;
}

bool ensureDirectory(const QString &path, RecoveryExtractResult &result) {
    QDir dir;
    if (dir.mkpath(path)) {
        ++result.createdDirectories;
        return true;
    }
    result.errors.append("No se pudo crear la carpeta: " + path);
    return false;
}

QJsonObject parseInternalJson(const RecoveryNode &node) {
    if (node.internalDataJson.trimmed().isEmpty()) {
        return {};
    }
    const QJsonDocument doc = QJsonDocument::fromJson(node.internalDataJson.toUtf8());
    return doc.isObject() ? doc.object() : QJsonObject{};
}

bool reportProgress(ExtractContext &context, const QString &currentPath) {
    if (!context.progress) {
        return true;
    }
    return context.progress(
        currentPath,
        context.result.writtenBytes,
        context.totalBytes,
        context.result.recoveredFiles,
        context.totalFiles
    );
}

bool initializeFat32Context(ExtractContext &context) {
    context.sourceFile.seek(0);
    const QByteArray boot = context.sourceFile.read(512);
    if (boot.size() < 90) {
        context.result.errors.append("Cabecera FAT32 insuficiente.");
        return false;
    }

    context.fat32.bytesPerSector = readLe16(boot, 11);
    context.fat32.sectorsPerCluster = readU8(boot, 13);
    const std::uint64_t reservedSectors = readLe16(boot, 14);
    const std::uint64_t fatCount = readU8(boot, 16);
    const std::uint64_t sectorsPerFat = readLe32(boot, 36);
    context.fat32.firstDataSector = reservedSectors + fatCount * sectorsPerFat;
    const std::uint64_t fatOffset = reservedSectors * context.fat32.bytesPerSector;
    const std::uint64_t fatBytes = sectorsPerFat * context.fat32.bytesPerSector;

    context.sourceFile.seek(static_cast<qint64>(fatOffset));
    context.fat32.fatTable = context.sourceFile.read(static_cast<qint64>(fatBytes));
    if (context.fat32.fatTable.isEmpty()) {
        context.result.errors.append("No se pudo leer la FAT de FAT32.");
        return false;
    }
    return true;
}

bool initializeExfatContext(ExtractContext &context) {
    context.sourceFile.seek(0);
    const QByteArray boot = context.sourceFile.read(512);
    if (boot.size() < 120) {
        context.result.errors.append("Cabecera exFAT insuficiente.");
        return false;
    }

    const std::uint8_t sectorShift = readU8(boot, 108);
    const std::uint8_t clusterShift = readU8(boot, 109);
    context.exfat.bytesPerSector = 1ull << sectorShift;
    context.exfat.sectorsPerCluster = 1ull << clusterShift;
    const std::uint64_t fatOffsetSectors = readLe32(boot, 80);
    const std::uint64_t fatLengthSectors = readLe32(boot, 84);
    context.exfat.clusterHeapOffset = readLe32(boot, 88);
    const std::uint64_t fatOffsetBytes = fatOffsetSectors * context.exfat.bytesPerSector;
    const std::uint64_t fatBytes = fatLengthSectors * context.exfat.bytesPerSector;

    context.sourceFile.seek(static_cast<qint64>(fatOffsetBytes));
    context.exfat.fatTable = context.sourceFile.read(static_cast<qint64>(fatBytes));
    if (context.exfat.fatTable.isEmpty()) {
        context.result.errors.append("No se pudo leer la FAT de exFAT.");
        return false;
    }
    return true;
}

QByteArray readFat32FileData(ExtractContext &context, std::uint32_t startCluster, std::uint64_t sizeBytes) {
    QByteArray data;
    if (startCluster < 2 || sizeBytes == 0) {
        return data;
    }
    const std::uint64_t clusterSize = context.fat32.bytesPerSector * context.fat32.sectorsPerCluster;
    std::uint32_t cluster = startCluster;
    QSet<std::uint32_t> seen;

    while (cluster >= 2 && cluster < 0x0FFFFFF8u && !seen.contains(cluster) &&
           static_cast<std::uint64_t>(data.size()) < sizeBytes) {
        seen.insert(cluster);
        const std::uint64_t offset = clusterOffset(
            cluster,
            context.fat32.firstDataSector,
            context.fat32.sectorsPerCluster,
            context.fat32.bytesPerSector
        );
        std::uint64_t bytesToRead = clusterSize;
        if (static_cast<std::uint64_t>(data.size()) + bytesToRead > sizeBytes) {
            bytesToRead = sizeBytes - static_cast<std::uint64_t>(data.size());
        }
        context.sourceFile.seek(static_cast<qint64>(offset));
        const QByteArray chunk = context.sourceFile.read(static_cast<qint64>(bytesToRead));
        if (chunk.size() != static_cast<int>(bytesToRead)) {
            return {};
        }
        data.append(chunk);
        cluster = fat32ClusterValue(context.fat32, cluster);
    }
    return data;
}

QByteArray readExfatFileData(
    ExtractContext &context,
    std::uint32_t startCluster,
    std::uint64_t sizeBytes,
    bool contiguous
) {
    QByteArray data;
    if (startCluster < 2 || sizeBytes == 0) {
        return data;
    }
    const std::uint64_t clusterSize = context.exfat.bytesPerSector * context.exfat.sectorsPerCluster;
    const int clustersToRead = static_cast<int>((sizeBytes + clusterSize - 1) / clusterSize);

    if (contiguous) {
        for (int i = 0; i < clustersToRead; ++i) {
            const std::uint32_t cluster = startCluster + static_cast<std::uint32_t>(i);
            const std::uint64_t offset = clusterOffset(
                cluster,
                context.exfat.clusterHeapOffset,
                context.exfat.sectorsPerCluster,
                context.exfat.bytesPerSector
            );
            std::uint64_t bytesToRead = clusterSize;
            if (static_cast<std::uint64_t>(data.size()) + bytesToRead > sizeBytes) {
                bytesToRead = sizeBytes - static_cast<std::uint64_t>(data.size());
            }
            context.sourceFile.seek(static_cast<qint64>(offset));
            const QByteArray chunk = context.sourceFile.read(static_cast<qint64>(bytesToRead));
            if (chunk.size() != static_cast<int>(bytesToRead)) {
                return {};
            }
            data.append(chunk);
        }
        return data;
    }

    std::uint32_t cluster = startCluster;
    QSet<std::uint32_t> seen;
    while (cluster >= 2 && cluster < 0xFFFFFFF8u && !seen.contains(cluster) &&
           static_cast<std::uint64_t>(data.size()) < sizeBytes) {
        seen.insert(cluster);
        const std::uint64_t offset = clusterOffset(
            cluster,
            context.exfat.clusterHeapOffset,
            context.exfat.sectorsPerCluster,
            context.exfat.bytesPerSector
        );
        std::uint64_t bytesToRead = clusterSize;
        if (static_cast<std::uint64_t>(data.size()) + bytesToRead > sizeBytes) {
            bytesToRead = sizeBytes - static_cast<std::uint64_t>(data.size());
        }
        context.sourceFile.seek(static_cast<qint64>(offset));
        const QByteArray chunk = context.sourceFile.read(static_cast<qint64>(bytesToRead));
        if (chunk.size() != static_cast<int>(bytesToRead)) {
            return {};
        }
        data.append(chunk);
        cluster = exfatClusterValue(context.exfat, cluster);
    }
    return data;
}

bool recoverNtfsFile(ExtractContext &context, const RecoveryNode &node, const QString &outputPath) {
    const QJsonObject meta = parseInternalJson(node);
    if (meta.value("fs").toString() != "ntfs") {
        context.result.errors.append("Sin metadatos NTFS internos: " + node.path);
        ++context.result.skippedEntries;
        return false;
    }

    QFile out(outputPath);
    if (!out.open(QIODevice::WriteOnly)) {
        context.result.errors.append("No se pudo crear el archivo de salida: " + outputPath);
        ++context.result.skippedEntries;
        return false;
    }

    const bool resident = meta.value("resident").toBool(false);
    const std::uint64_t realSize = meta.value("real_size").toString().toULongLong();
    bool ok = true;

    if (resident) {
        const QByteArray data = QByteArray::fromBase64(meta.value("resident_data_b64").toString().toLatin1());
        if (out.write(data) != data.size()) {
            ok = false;
        }
    } else {
        const std::uint64_t clusterSize = context.ntfsClusterSize;
        if (clusterSize == 0) {
            context.result.errors.append("No se pudo determinar el tamano de cluster NTFS.");
            ++context.result.skippedEntries;
            return false;
        }

        std::uint64_t remaining = realSize;
        const QJsonArray runs = meta.value("runs").toArray();
        for (const QJsonValue &value : runs) {
            const QJsonObject run = value.toObject();
            const bool sparse = run.value("sparse").toBool(false);
            const std::uint64_t lcn = run.value("lcn").toString().toULongLong();
            const std::uint64_t clusters = run.value("clusters").toString().toULongLong();
            std::uint64_t byteCount = clusters * clusterSize;
            if (remaining < byteCount) {
                byteCount = remaining;
            }
            if (byteCount == 0) {
                continue;
            }

            QByteArray chunk;
            if (sparse) {
                chunk = QByteArray(static_cast<int>(byteCount), '\0');
            } else {
                context.sourceFile.seek(static_cast<qint64>(lcn * clusterSize));
                chunk = context.sourceFile.read(static_cast<qint64>(byteCount));
                if (chunk.size() != static_cast<int>(byteCount)) {
                    ok = false;
                    break;
                }
            }

            if (out.write(chunk) != chunk.size()) {
                ok = false;
                break;
            }
            remaining -= byteCount;
            if (remaining == 0) {
                break;
            }
        }

        if (remaining != 0) {
            ok = false;
        }
    }

    out.close();

    if (!ok) {
        QFile::remove(outputPath);
        context.result.errors.append("No se pudo reconstruir completamente: " + node.path);
        ++context.result.skippedEntries;
        return false;
    }

    out.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                       QFileDevice::ReadUser | QFileDevice::WriteUser |
                       QFileDevice::ReadGroup | QFileDevice::ReadOther);
    ++context.result.recoveredFiles;
    context.result.writtenBytes += node.sizeBytes;
    return reportProgress(context, node.path);
}

bool writeRecoveredBytes(ExtractContext &context, const RecoveryNode &node, const QString &outputPath, const QByteArray &data) {
    if (data.isEmpty() && node.sizeBytes > 0) {
        context.result.errors.append("No se pudieron leer datos para: " + node.path);
        ++context.result.skippedEntries;
        return false;
    }
    QFile out(outputPath);
    if (!out.open(QIODevice::WriteOnly)) {
        context.result.errors.append("No se pudo crear el archivo de salida: " + outputPath);
        ++context.result.skippedEntries;
        return false;
    }
    if (out.write(data) != data.size()) {
        out.close();
        QFile::remove(outputPath);
        context.result.errors.append("Error escribiendo el archivo recuperado: " + outputPath);
        ++context.result.skippedEntries;
        return false;
    }
    out.close();
    out.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                       QFileDevice::ReadUser | QFileDevice::WriteUser |
                       QFileDevice::ReadGroup | QFileDevice::ReadOther);
    ++context.result.recoveredFiles;
    context.result.writtenBytes += static_cast<std::uint64_t>(data.size());
    return reportProgress(context, node.path);
}

bool recoverFat32File(ExtractContext &context, const RecoveryNode &node, const QString &outputPath) {
    const QJsonObject meta = parseInternalJson(node);
    if (meta.value("fs").toString() != "fat32") {
        context.result.errors.append("Sin metadatos FAT32 internos: " + node.path);
        ++context.result.skippedEntries;
        return false;
    }
    const std::uint32_t startCluster = static_cast<std::uint32_t>(meta.value("start_cluster").toInt());
    const std::uint64_t sizeBytes = static_cast<std::uint64_t>(meta.value("size_bytes").toDouble());
    const QByteArray data = readFat32FileData(context, startCluster, sizeBytes);
    return writeRecoveredBytes(context, node, outputPath, data);
}

bool recoverExfatFile(ExtractContext &context, const RecoveryNode &node, const QString &outputPath) {
    const QJsonObject meta = parseInternalJson(node);
    if (meta.value("fs").toString() != "exfat") {
        context.result.errors.append("Sin metadatos exFAT internos: " + node.path);
        ++context.result.skippedEntries;
        return false;
    }
    const std::uint32_t startCluster = static_cast<std::uint32_t>(meta.value("start_cluster").toInt());
    const std::uint64_t sizeBytes = meta.value("size_bytes").toString().toULongLong();
    const bool contiguous = meta.value("contiguous").toBool(false);
    const QByteArray data = readExfatFileData(context, startCluster, sizeBytes, contiguous);
    return writeRecoveredBytes(context, node, outputPath, data);
}

bool recoverNodeRecursive(ExtractContext &context, const RecoveryNode &node, const QString &basePath) {
    const QString targetName = sanitizedName(node.name);
    const QString targetPath = basePath + "/" + targetName;

    if (node.kind == RecoveryNodeKind::Folder) {
        if (!ensureDirectory(targetPath, context.result)) {
            return false;
        }
        bool folderOk = true;
        for (const RecoveryNode &child : node.children) {
            if (!recoverNodeRecursive(context, child, targetPath)) {
                folderOk = false;
            }
        }
        return folderOk;
    }

    switch (context.fileSystem) {
    case FileSystemKind::NTFS:
        return recoverNtfsFile(context, node, targetPath);
    case FileSystemKind::FAT32:
        return recoverFat32File(context, node, targetPath);
    case FileSystemKind::ExFAT:
        return recoverExfatFile(context, node, targetPath);
    default:
        context.result.errors.append("Sistema aun no soportado para extraccion real: " + node.path);
        ++context.result.skippedEntries;
        return false;
    }
}

} // namespace

RecoveryExtractResult RecoveryExtractor::recover(
    const QString &sourcePath,
    FileSystemKind fileSystem,
    const QVector<RecoveryNode> &nodes,
    const QString &destinationRoot,
    ProgressCallback progress
) {
    ExtractContext context;
    context.destinationRoot = destinationRoot;
    context.fileSystem = fileSystem;
    context.progress = std::move(progress);

    for (const RecoveryNode &node : nodes) {
        accumulateNodeTotals(node, context.totalBytes, context.totalFiles);
    }

    if (fileSystem == FileSystemKind::NTFS || fileSystem == FileSystemKind::FAT32 || fileSystem == FileSystemKind::ExFAT) {
        context.sourceFile.setFileName(sourcePath);
        if (!context.sourceFile.open(QIODevice::ReadOnly)) {
            context.result.errors.append("No se pudo abrir el origen en solo lectura: " + sourcePath);
            return context.result;
        }
    }

    if (fileSystem == FileSystemKind::NTFS) {
        context.ntfsClusterSize = ntfsClusterSize(context.sourceFile);
        if (context.ntfsClusterSize == 0) {
            context.result.errors.append("No se pudo inicializar NTFS.");
            return context.result;
        }
    } else if (fileSystem == FileSystemKind::FAT32) {
        if (!initializeFat32Context(context)) {
            return context.result;
        }
    } else if (fileSystem == FileSystemKind::ExFAT) {
        if (!initializeExfatContext(context)) {
            return context.result;
        }
    }

    if (!reportProgress(context, "Preparando recuperacion")) {
        context.result.errors.append("Operacion cancelada por el usuario.");
        return context.result;
    }

    if (!ensureDirectory(destinationRoot, context.result)) {
        return context.result;
    }

    bool anyRecovered = false;
    for (const RecoveryNode &node : nodes) {
        if (recoverNodeRecursive(context, node, destinationRoot)) {
            if (context.result.recoveredFiles > 0) {
                anyRecovered = true;
            }
        } else if (context.result.recoveredFiles > 0) {
            anyRecovered = true;
        }
    }

    context.result.success = anyRecovered && context.result.recoveredFiles > 0;
    return context.result;
}
