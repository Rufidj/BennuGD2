#include "VolumeExplorer.h"

#include <algorithm>
#include <QByteArray>
#include <QDate>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSet>
#include <QStringList>
#include <QTime>

namespace {
std::uint8_t readU8(const QByteArray &data, int offset) {
    if (offset >= data.size()) {
        return 0;
    }
    return static_cast<std::uint8_t>(static_cast<unsigned char>(data[offset]));
}

std::uint16_t readLe16(const QByteArray &data, int offset) {
    if (offset + 2 > data.size()) {
        return 0;
    }
    return static_cast<std::uint16_t>(
        static_cast<unsigned char>(data[offset]) |
        (static_cast<unsigned char>(data[offset + 1]) << 8)
    );
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

QString decodeFatShortName(const QByteArray &entry) {
    QString base = QString::fromLatin1(entry.mid(0, 8)).trimmed();
    QString ext = QString::fromLatin1(entry.mid(8, 3)).trimmed();
    base.remove(' ');
    ext.remove(' ');
    if (base.isEmpty()) {
        base = "SIN_NOMBRE";
    }
    return ext.isEmpty() ? base : base + "." + ext;
}

QString decodeFatLfnPart(const QByteArray &entry) {
    QString result;
    const int offsets[] = {1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30};
    for (int offset : offsets) {
        const std::uint16_t value = readLe16(entry, offset);
        if (value == 0x0000 || value == 0xFFFF) {
            break;
        }
        result.append(QChar(value));
    }
    return result;
}

RecoveryNode makeNode(const QString &name, const QString &path, bool isDir, std::uint64_t sizeBytes = 0) {
    RecoveryNode node;
    node.name = name;
    node.path = path;
    node.kind = isDir ? RecoveryNodeKind::Folder : RecoveryNodeKind::File;
    node.typeLabel = isDir ? "Carpeta" : (name.contains('.') ? name.section('.', -1).toUpper() : "Archivo");
    node.statusLabel = "Detectado";
    node.sizeBytes = sizeBytes;
    node.lineCount = 1;
    return node;
}

QString formatDosDateTime(std::uint16_t dateValue, std::uint16_t timeValue) {
    if (dateValue == 0 || dateValue == 0xFFFF) {
        return {};
    }
    const int day = dateValue & 0x1F;
    const int month = (dateValue >> 5) & 0x0F;
    const int year = ((dateValue >> 9) & 0x7F) + 1980;
    const int second = (timeValue & 0x1F) * 2;
    const int minute = (timeValue >> 5) & 0x3F;
    const int hour = (timeValue >> 11) & 0x1F;
    const QDate date(year, month, day);
    const QTime time(hour, minute, second);
    if (!date.isValid()) {
        return {};
    }
    return QDateTime(date, time).toString("yyyy-MM-dd HH:mm:ss");
}

QString formatNtfsFileTime(std::uint64_t fileTime) {
    if (fileTime == 0) {
        return {};
    }
    static const qint64 ticksPerSecond = 10000000ll;
    static const qint64 secondsBetween1601And1970 = 11644473600ll;
    const qint64 seconds = static_cast<qint64>(fileTime / ticksPerSecond) - secondsBetween1601And1970;
    const qint64 millis = seconds * 1000 + static_cast<qint64>((fileTime % ticksPerSecond) / 10000);
    const QDateTime dt = QDateTime::fromMSecsSinceEpoch(millis, Qt::UTC);
    return dt.isValid() ? dt.toString("yyyy-MM-dd HH:mm:ss 'UTC'") : QString();
}

QString buildFat32InternalNodeJson(std::uint32_t startCluster, std::uint32_t sizeBytes, bool isDir, bool deleted) {
    QJsonObject root;
    root.insert("fs", "fat32");
    root.insert("start_cluster", static_cast<int>(startCluster));
    root.insert("size_bytes", static_cast<qint64>(sizeBytes));
    root.insert("is_dir", isDir);
    root.insert("deleted", deleted);
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

QString buildExfatInternalNodeJson(
    std::uint32_t startCluster,
    std::uint64_t sizeBytes,
    bool isDir,
    bool deleted,
    bool noFatChain
) {
    QJsonObject root;
    root.insert("fs", "exfat");
    root.insert("start_cluster", static_cast<int>(startCluster));
    root.insert("size_bytes", QString::number(static_cast<qulonglong>(sizeBytes)));
    root.insert("is_dir", isDir);
    root.insert("deleted", deleted);
    root.insert("contiguous", noFatChain);
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

struct Fat32Context {
    std::uint64_t bytesPerSector = 0;
    std::uint64_t sectorsPerCluster = 0;
    std::uint64_t reservedSectors = 0;
    std::uint64_t fatCount = 0;
    std::uint64_t sectorsPerFat = 0;
    std::uint64_t firstDataSector = 0;
    std::uint32_t rootCluster = 0;
    QByteArray fatTable;
    int maxEntries = 0;
    int parsedEntries = 0;
    int deletedEntries = 0;
};

struct ExfatContext {
    std::uint64_t bytesPerSector = 0;
    std::uint64_t sectorsPerCluster = 0;
    std::uint64_t clusterHeapOffset = 0;
    std::uint32_t rootCluster = 0;
    QByteArray fatTable;
    int maxEntries = 0;
    int parsedEntries = 0;
    int deletedEntries = 0;
};

struct NtfsContext {
    std::uint64_t bytesPerSector = 0;
    std::uint64_t sectorsPerCluster = 0;
    std::uint64_t clusterSize = 0;
    std::uint64_t mftLcn = 0;
    std::uint64_t mftOffset = 0;
    std::uint32_t recordSize = 0;
    std::uint32_t indexBufferSize = 0;
    int maxEntries = 0;
    int parsedEntries = 0;
    int deletedEntries = 0;
    QSet<std::uint64_t> visitedDirectories;
};

std::uint32_t fat32ClusterValue(const Fat32Context &context, std::uint32_t cluster) {
    const int offset = static_cast<int>(cluster * 4);
    if (offset + 4 > context.fatTable.size()) {
        return 0x0FFFFFFF;
    }
    return readLe32(context.fatTable, offset) & 0x0FFFFFFF;
}

QByteArray readFat32Cluster(QFile &file, const Fat32Context &context, std::uint32_t cluster) {
    const std::uint64_t offset = clusterOffset(
        cluster,
        context.firstDataSector,
        context.sectorsPerCluster,
        context.bytesPerSector
    );
    const std::uint64_t bytes = context.bytesPerSector * context.sectorsPerCluster;
    file.seek(static_cast<qint64>(offset));
    return file.read(static_cast<qint64>(bytes));
}

QByteArray readFat32Chain(QFile &file, const Fat32Context &context, std::uint32_t startCluster, int maxClusters = 64) {
    QByteArray data;
    QSet<std::uint32_t> seen;
    std::uint32_t cluster = startCluster;
    int count = 0;

    while (cluster >= 2 && cluster < 0x0FFFFFF8 && !seen.contains(cluster) && count < maxClusters) {
        seen.insert(cluster);
        data.append(readFat32Cluster(file, context, cluster));
        cluster = fat32ClusterValue(context, cluster);
        ++count;
    }
    return data;
}

std::uint32_t fat32StartCluster(const QByteArray &entry) {
    const std::uint32_t high = readLe16(entry, 20);
    const std::uint32_t low = readLe16(entry, 26);
    return (high << 16) | low;
}

void finalizeFolderNode(RecoveryNode &node) {
    if (node.kind != RecoveryNodeKind::Folder) {
        return;
    }
    std::uint64_t totalSize = 0;
    int totalLines = 0;
    for (RecoveryNode &child : node.children) {
        finalizeFolderNode(child);
        totalSize += child.sizeBytes;
        totalLines += child.lineCount;
    }
    node.sizeBytes = totalSize;
    node.lineCount = totalLines > 0 ? totalLines : static_cast<int>(node.children.size());
    node.typeLabel = "Carpeta";
}

std::uint32_t ntfsRecordSize(const QByteArray &boot, std::uint64_t clusterSize) {
    const qint8 raw = static_cast<qint8>(readU8(boot, 64));
    if (raw > 0) {
        return static_cast<std::uint32_t>(static_cast<std::uint64_t>(raw) * clusterSize);
    }
    if (raw < 0) {
        return static_cast<std::uint32_t>(1u << static_cast<unsigned>(-raw));
    }
    return 1024;
}

std::uint32_t ntfsSizedStructure(const QByteArray &boot, int offset, std::uint64_t clusterSize, std::uint32_t fallback) {
    const qint8 raw = static_cast<qint8>(readU8(boot, offset));
    if (raw > 0) {
        return static_cast<std::uint32_t>(static_cast<std::uint64_t>(raw) * clusterSize);
    }
    if (raw < 0) {
        return static_cast<std::uint32_t>(1u << static_cast<unsigned>(-raw));
    }
    return fallback;
}

bool ntfsApplyFixup(QByteArray &record) {
    if (record.size() < 8) {
        return false;
    }
    const std::uint16_t usaOffset = readLe16(record, 4);
    const std::uint16_t usaCount = readLe16(record, 6);
    if (usaOffset == 0 || usaCount < 2 || usaOffset + usaCount * 2 > record.size()) {
        return false;
    }

    const QByteArray usa = record.mid(usaOffset, usaCount * 2);
    const std::uint16_t sequenceValue = readLe16(usa, 0);
    for (std::uint16_t i = 1; i < usaCount; ++i) {
        const int sectorEnd = static_cast<int>(i * 512 - 2);
        if (sectorEnd + 2 > record.size()) {
            return false;
        }
        if (readLe16(record, sectorEnd) != sequenceValue) {
            return false;
        }
        record[sectorEnd] = usa[2 * i];
        record[sectorEnd + 1] = usa[2 * i + 1];
    }
    return true;
}

qint64 readLeSigned(const QByteArray &data, int offset, int lengthBytes) {
    if (lengthBytes <= 0 || offset + lengthBytes > data.size() || lengthBytes > 8) {
        return 0;
    }
    qint64 value = 0;
    for (int i = 0; i < lengthBytes; ++i) {
        value |= (static_cast<qint64>(static_cast<unsigned char>(data[offset + i])) << (8 * i));
    }
    const int bits = lengthBytes * 8;
    const qint64 signBit = static_cast<qint64>(1) << (bits - 1);
    if (value & signBit) {
        if (bits < 64) {
            value |= ~((static_cast<qint64>(1) << bits) - 1);
        }
    }
    return value;
}

struct NtfsRun {
    std::uint64_t lcn = 0;
    std::uint64_t clusterCount = 0;
    bool sparse = false;
};

struct NtfsDataDescriptor {
    bool valid = false;
    bool resident = false;
    std::uint64_t realSize = 0;
    std::uint64_t allocatedSize = 0;
    QByteArray residentData;
    QVector<NtfsRun> runs;
};

QVector<NtfsRun> ntfsParseDataRuns(const QByteArray &record, int runOffset) {
    QVector<NtfsRun> runs;
    qint64 currentLcn = 0;
    int cursor = runOffset;
    while (cursor < record.size()) {
        const std::uint8_t header = readU8(record, cursor);
        if (header == 0x00) {
            break;
        }
        const int lenSize = header & 0x0F;
        const int offSize = (header >> 4) & 0x0F;
        if (lenSize == 0 || cursor + 1 + lenSize + offSize > record.size()) {
            break;
        }
        const std::uint64_t clusterCount = static_cast<std::uint64_t>(readLeSigned(record, cursor + 1, lenSize));
        const bool sparse = offSize == 0;
        qint64 lcnDelta = 0;
        if (!sparse) {
            lcnDelta = readLeSigned(record, cursor + 1 + lenSize, offSize);
            currentLcn += lcnDelta;
        }
        if (clusterCount > 0) {
            runs.append(NtfsRun{
                sparse ? 0ull : static_cast<std::uint64_t>(currentLcn),
                clusterCount,
                sparse
            });
        }
        cursor += 1 + lenSize + offSize;
    }
    return runs;
}

QByteArray ntfsReadRuns(QFile &file, const NtfsContext &context, const QVector<NtfsRun> &runs, std::uint64_t maxBytes) {
    QByteArray data;
    for (const NtfsRun &run : runs) {
        std::uint64_t byteCount = run.clusterCount * context.clusterSize;
        if (static_cast<std::uint64_t>(data.size()) + byteCount > maxBytes) {
            byteCount = maxBytes - static_cast<std::uint64_t>(data.size());
        }
        if (byteCount == 0) {
            break;
        }
        if (run.sparse) {
            data.append(QByteArray(static_cast<int>(byteCount), '\0'));
        } else {
            const std::uint64_t byteOffset = run.lcn * context.clusterSize;
            file.seek(static_cast<qint64>(byteOffset));
            data.append(file.read(static_cast<qint64>(byteCount)));
        }
        if (static_cast<std::uint64_t>(data.size()) >= maxBytes) {
            break;
        }
    }
    return data;
}

QByteArray readNtfsRecord(QFile &file, const NtfsContext &context, std::uint64_t recordNumber) {
    const std::uint64_t offset = context.mftOffset + recordNumber * context.recordSize;
    file.seek(static_cast<qint64>(offset));
    QByteArray record = file.read(static_cast<qint64>(context.recordSize));
    if (record.size() < static_cast<int>(context.recordSize) || record.mid(0, 4) != "FILE") {
        return {};
    }
    if (!ntfsApplyFixup(record)) {
        return {};
    }
    return record;
}

NtfsDataDescriptor ntfsParseDataDescriptor(const QByteArray &record) {
    NtfsDataDescriptor descriptor;
    if (record.size() < 24) {
        return descriptor;
    }

    const std::uint16_t attrOffset = readLe16(record, 20);
    int offset = attrOffset;

    while (offset + 16 <= record.size()) {
        const std::uint32_t type = readLe32(record, offset);
        if (type == 0xFFFFFFFFu) {
            break;
        }
        const std::uint32_t length = readLe32(record, offset + 4);
        if (length == 0 || offset + static_cast<int>(length) > record.size()) {
            break;
        }

        if (type == 0x80u) {
            const bool nonResident = readU8(record, offset + 8) != 0;
            descriptor.valid = true;
            descriptor.resident = !nonResident;
            if (!nonResident) {
                const std::uint32_t valueLength = readLe32(record, offset + 16);
                const std::uint16_t valueOffset = readLe16(record, offset + 20);
                descriptor.realSize = valueLength;
                descriptor.allocatedSize = descriptor.realSize;
                if (valueOffset > 0 && offset + valueOffset + static_cast<int>(valueLength) <= record.size()) {
                    descriptor.residentData = record.mid(offset + valueOffset, valueLength);
                }
            } else {
                descriptor.allocatedSize = readLe64(record, offset + 40);
                descriptor.realSize = readLe64(record, offset + 48);
                const std::uint16_t runOffset = readLe16(record, offset + 32);
                const QByteArray attrRecord = record.mid(offset, length);
                descriptor.runs = ntfsParseDataRuns(attrRecord, runOffset);
            }
            return descriptor;
        }

        offset += static_cast<int>(length);
    }

    return descriptor;
}

QString ntfsBuildDataMetadata(const NtfsDataDescriptor &descriptor, std::uint64_t clusterSize) {
    if (!descriptor.valid) {
        return "DATA: no localizado";
    }

    QString metadata;
    metadata += "DATA: " + QString(descriptor.resident ? "residente" : "no residente") + "\n";
    metadata += "Tamano real: " + QString::number(static_cast<qulonglong>(descriptor.realSize)) + " bytes\n";
    metadata += "Tamano asignado: " + QString::number(static_cast<qulonglong>(descriptor.allocatedSize)) + " bytes";

    if (!descriptor.resident) {
        metadata += "\nFragmentos fisicos: " + QString::number(descriptor.runs.size());
        if (!descriptor.runs.isEmpty()) {
            metadata += "\nPrimer LCN: " + QString::number(static_cast<qulonglong>(descriptor.runs.first().lcn));
            std::uint64_t totalClusters = 0;
            for (const NtfsRun &run : descriptor.runs) {
                totalClusters += run.clusterCount;
            }
            metadata += "\nClusters cubiertos: " + QString::number(static_cast<qulonglong>(totalClusters));
            metadata += "\nCobertura estimada: " +
                        QString::number(static_cast<qulonglong>(totalClusters * clusterSize)) +
                        " bytes";
        }
    }

    return metadata;
}

QString ntfsBuildInternalNodeJson(
    std::uint64_t recordNumber,
    bool isDir,
    const NtfsDataDescriptor &descriptor
) {
    QJsonObject root;
    root.insert("fs", "ntfs");
    root.insert("record_number", QString::number(static_cast<qulonglong>(recordNumber)));
    root.insert("is_dir", isDir);
    root.insert("has_data", descriptor.valid);
    if (descriptor.valid) {
        root.insert("resident", descriptor.resident);
        root.insert("real_size", QString::number(static_cast<qulonglong>(descriptor.realSize)));
        root.insert("allocated_size", QString::number(static_cast<qulonglong>(descriptor.allocatedSize)));
        if (descriptor.resident) {
            root.insert("resident_data_b64", QString::fromLatin1(descriptor.residentData.toBase64()));
        } else {
            QJsonArray runsArray;
            for (const NtfsRun &run : descriptor.runs) {
                QJsonObject runObject;
                runObject.insert("lcn", QString::number(static_cast<qulonglong>(run.lcn)));
                runObject.insert("clusters", QString::number(static_cast<qulonglong>(run.clusterCount)));
                runObject.insert("sparse", run.sparse);
                runsArray.append(runObject);
            }
            root.insert("runs", runsArray);
        }
    }
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

QString decodeNtfsFileNameFromIndexEntry(const QByteArray &stream) {
    if (stream.size() < 66) {
        return {};
    }
    const int nameLength = readU8(stream, 64);
    QString name;
    for (int i = 0; i < nameLength; ++i) {
        const int offset = 66 + i * 2;
        if (offset + 2 > stream.size()) {
            break;
        }
        name.append(QChar(readLe16(stream, offset)));
    }
    return name;
}

QString decodeNtfsUtf16Name(const QByteArray &data, int nameOffset, int nameLength) {
    QString name;
    for (int i = 0; i < nameLength; ++i) {
        const int offset = nameOffset + i * 2;
        if (offset + 2 > data.size()) {
            break;
        }
        name.append(QChar(readLe16(data, offset)));
    }
    return name;
}

struct NtfsDeletedCandidate {
    bool valid = false;
    QString name;
    bool isDir = false;
    std::uint64_t parentRef = 5;
    std::uint64_t realSize = 0;
    QString metadata;
};

struct NtfsMftEntry {
    std::uint64_t recordNumber = 0;
    QString name;
    std::uint64_t parentRef = 5;
    bool isDir = false;
    bool active = false;
    std::uint64_t sizeBytes = 0;
    QString metadata;
    NtfsDataDescriptor dataDescriptor;
};

struct NtfsBootScanInfo {
    bool valid = false;
    QString error;
    std::uint64_t bytesPerSector = 0;
    std::uint64_t sectorsPerCluster = 0;
    std::uint64_t clusterSize = 0;
    std::uint64_t mftLcn = 0;
    std::uint64_t mftOffset = 0;
    std::uint32_t recordSize = 0;
    std::uint64_t declaredVolumeBytes = 0;
};

struct NtfsFileNameAttr {
    bool valid = false;
    QString name;
    std::uint64_t parentRef = 5;
    std::uint64_t realSize = 0;
    std::uint32_t fileFlags = 0;
    std::uint8_t nameSpace = 0;
    std::uint64_t created = 0;
    std::uint64_t modified = 0;
    std::uint64_t changed = 0;
    std::uint64_t accessed = 0;
};

NtfsBootScanInfo ntfsParseBootScanInfo(const QByteArray &boot, std::uint64_t deviceSize) {
    NtfsBootScanInfo info;
    if (boot.size() < 90 || boot.mid(3, 8) != "NTFS    ") {
        info.error = "El origen seleccionado no parece un volumen NTFS valido.";
        return info;
    }

    info.bytesPerSector = readLe16(boot, 11);
    info.sectorsPerCluster = readU8(boot, 13);
    info.clusterSize = info.bytesPerSector * info.sectorsPerCluster;
    info.mftLcn = readLe64(boot, 48);
    info.recordSize = ntfsRecordSize(boot, info.clusterSize);
    info.mftOffset = info.mftLcn * info.clusterSize;
    info.declaredVolumeBytes = readLe64(boot, 40) * info.bytesPerSector;

    if (info.bytesPerSector == 0 || info.clusterSize == 0 || info.recordSize == 0) {
        info.error = "No se pudieron interpretar los parametros basicos de NTFS.";
        return info;
    }
    const std::uint64_t effectiveDeviceSize = std::max(deviceSize, info.declaredVolumeBytes);
    if (info.mftOffset == 0 || (effectiveDeviceSize > 0 && info.mftOffset >= effectiveDeviceSize)) {
        info.error = "La posicion de la MFT parece invalida.";
        return info;
    }

    info.valid = true;
    return info;
}

std::uint64_t exactDeviceSizeBytes(const QString &filePath) {
    if (filePath.startsWith("/dev/")) {
        QProcess blockdev;
        blockdev.start("blockdev", {"--getsize64", filePath});
        if (blockdev.waitForFinished(1500) &&
            blockdev.exitStatus() == QProcess::NormalExit &&
            blockdev.exitCode() == 0) {
            const QString output = QString::fromUtf8(blockdev.readAllStandardOutput()).trimmed();
            bool ok = false;
            const std::uint64_t size = output.toULongLong(&ok);
            if (ok && size > 0) {
                return size;
            }
        }

        QProcess lsblk;
        lsblk.start("lsblk", {"-b", "-n", "-o", "SIZE", filePath});
        if (lsblk.waitForFinished(1500) &&
            lsblk.exitStatus() == QProcess::NormalExit &&
            lsblk.exitCode() == 0) {
            const QStringList lines = QString::fromUtf8(lsblk.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts);
            if (!lines.isEmpty()) {
                bool ok = false;
                const std::uint64_t size = lines.first().trimmed().toULongLong(&ok);
                if (ok && size > 0) {
                    return size;
                }
            }
        }
    }

    QFileInfo info(filePath);
    if (info.exists() && info.isFile()) {
        return static_cast<std::uint64_t>(info.size());
    }
    return 0;
}

NtfsFileNameAttr ntfsParseFileNameAttr(const QByteArray &value) {
    NtfsFileNameAttr attr;
    if (value.size() < 66) {
        return attr;
    }

    attr.parentRef = readLe64(value, 0) & 0x0000FFFFFFFFFFFFull;
    attr.created = readLe64(value, 8);
    attr.modified = readLe64(value, 16);
    attr.changed = readLe64(value, 24);
    attr.accessed = readLe64(value, 32);
    attr.realSize = readLe64(value, 48);
    attr.fileFlags = readLe32(value, 56);
    const int nameLength = readU8(value, 64);
    attr.nameSpace = readU8(value, 65);
    attr.name = decodeNtfsUtf16Name(value, 66, nameLength);
    attr.valid = !attr.name.isEmpty();
    return attr;
}

QByteArray ntfsFindAttributeRaw(const QByteArray &record, std::uint32_t attributeType, bool *resident = nullptr) {
    if (record.size() < 24) {
        return {};
    }

    const std::uint16_t attrOffset = readLe16(record, 20);
    int offset = attrOffset;
    while (offset + 16 <= record.size()) {
        const std::uint32_t type = readLe32(record, offset);
        if (type == 0xFFFFFFFFu) {
            break;
        }
        const std::uint32_t length = readLe32(record, offset + 4);
        if (length == 0 || offset + static_cast<int>(length) > record.size()) {
            break;
        }

        if (type == attributeType) {
            const bool isResident = readU8(record, offset + 8) == 0;
            if (resident) {
                *resident = isResident;
            }
            return record.mid(offset, length);
        }
        offset += static_cast<int>(length);
    }
    return {};
}

QByteArray ntfsReadRecordFromRuns(
    QFile &file,
    const NtfsBootScanInfo &scan,
    const QVector<NtfsRun> &runs,
    std::uint64_t recordIndex
) {
    const std::uint64_t streamOffset = recordIndex * scan.recordSize;
    std::uint64_t currentStreamOffset = 0;

    for (const NtfsRun &run : runs) {
        const std::uint64_t runBytes = run.clusterCount * scan.clusterSize;
        if (currentStreamOffset + runBytes <= streamOffset) {
            currentStreamOffset += runBytes;
            continue;
        }

        if (run.sparse) {
            return {};
        }

        const std::uint64_t insideRun = streamOffset - currentStreamOffset;
        const std::uint64_t diskOffset = run.lcn * scan.clusterSize + insideRun;
        file.seek(static_cast<qint64>(diskOffset));
        QByteArray record = file.read(static_cast<qint64>(scan.recordSize));
        if (record.size() < static_cast<int>(scan.recordSize)) {
            return {};
        }
        if (!ntfsApplyFixup(record) || record.mid(0, 4) != "FILE") {
            return {};
        }
        return record;
    }

    return {};
}

bool ntfsParseMftEntryRecord(
    const QByteArray &record,
    std::uint64_t recordNumber,
    std::uint64_t clusterSize,
    NtfsMftEntry &entry
) {
    if (record.size() < 24 || record.mid(0, 4) != "FILE") {
        return false;
    }

    const std::uint16_t flags = readLe16(record, 22);
    const bool active = (flags & 0x0001u) != 0;
    const bool isDir = (flags & 0x0002u) != 0;

    NtfsFileNameAttr bestName;
    NtfsDataDescriptor dataDescriptor = ntfsParseDataDescriptor(record);

    const std::uint16_t attrOffset = readLe16(record, 20);
    int offset = attrOffset;
    while (offset + 16 <= record.size()) {
        const std::uint32_t type = readLe32(record, offset);
        if (type == 0xFFFFFFFFu) {
            break;
        }
        const std::uint32_t length = readLe32(record, offset + 4);
        if (length == 0 || offset + static_cast<int>(length) > record.size()) {
            break;
        }

        const bool nonResident = readU8(record, offset + 8) != 0;
        if (type == 0x30u && !nonResident) {
            const std::uint32_t valueLength = readLe32(record, offset + 16);
            const std::uint16_t valueOffset = readLe16(record, offset + 20);
            const int valueStart = offset + valueOffset;
            if (valueLength >= 66 && valueStart + static_cast<int>(valueLength) <= record.size()) {
                const NtfsFileNameAttr candidate = ntfsParseFileNameAttr(record.mid(valueStart, valueLength));
                if (candidate.valid && (bestName.name.isEmpty() || candidate.nameSpace == 0x01 || candidate.nameSpace == 0x03)) {
                    bestName = candidate;
                }
            }
        }
        offset += static_cast<int>(length);
    }

    if (!bestName.valid) {
        if (recordNumber == 5) {
            bestName.valid = true;
            bestName.name = "Root";
            bestName.parentRef = 5;
        } else {
            return false;
        }
    }

    std::uint64_t effectiveSize = bestName.realSize;
    if (!isDir && dataDescriptor.valid) {
        effectiveSize = dataDescriptor.realSize;
    }

    QString metadata;
    metadata += "Registro MFT: " + QString::number(static_cast<qulonglong>(recordNumber)) + "\n";
    const QString created = formatNtfsFileTime(bestName.created);
    const QString modified = formatNtfsFileTime(bestName.modified);
    const QString changed = formatNtfsFileTime(bestName.changed);
    const QString accessed = formatNtfsFileTime(bestName.accessed);
    if (!created.isEmpty()) {
        metadata += "Creado: " + created + "\n";
    }
    if (!modified.isEmpty()) {
        metadata += "Modificado: " + modified + "\n";
    }
    if (!changed.isEmpty()) {
        metadata += "Cambio MFT: " + changed + "\n";
    }
    if (!accessed.isEmpty()) {
        metadata += "Accedido: " + accessed + "\n";
    }
    metadata += "Flags NTFS: 0x" + QString::number(bestName.fileFlags, 16).toUpper() + "\n";
    metadata += ntfsBuildDataMetadata(dataDescriptor, clusterSize) + "\n";
    metadata += active ? "Estado de registro: activo" : "Estado de registro: no activo";

    entry.recordNumber = recordNumber;
    entry.name = bestName.name;
    entry.parentRef = bestName.parentRef;
    entry.isDir = isDir;
    entry.active = active;
    entry.sizeBytes = effectiveSize;
    entry.metadata = metadata;
    entry.dataDescriptor = dataDescriptor;
    return true;
}

void ntfsBuildMftTreeBranch(
    std::uint64_t parentRef,
    const QString &basePath,
    const QHash<std::uint64_t, QVector<std::uint64_t>> &childrenByParent,
    const QHash<std::uint64_t, NtfsMftEntry> &entries,
    QVector<RecoveryNode> &target,
    QHash<std::uint64_t, QString> *pathByRecord = nullptr
) {
    auto it = childrenByParent.find(parentRef);
    if (it == childrenByParent.end()) {
        return;
    }

    QVector<std::uint64_t> childRecords = it.value();
    std::sort(childRecords.begin(), childRecords.end(), [&entries](std::uint64_t left, std::uint64_t right) {
        const NtfsMftEntry a = entries.value(left);
        const NtfsMftEntry b = entries.value(right);
        if (a.isDir != b.isDir) {
            return a.isDir && !b.isDir;
        }
        return a.name.toLower() < b.name.toLower();
    });

    for (std::uint64_t recordNumber : childRecords) {
        const NtfsMftEntry entry = entries.value(recordNumber);
        if (entry.recordNumber == 5 || entry.name.isEmpty()) {
            continue;
        }

        const QString safeName = QString(entry.name).replace('/', '_');
        RecoveryNode node = makeNode(
            safeName,
            basePath + "/" + safeName,
            entry.isDir,
            entry.isDir ? 0 : entry.sizeBytes
        );
        node.deleted = !entry.active;
        node.statusLabel = entry.active ? "Activo" : "Borrado";
        node.metadataText = entry.metadata;
        node.internalDataJson = ntfsBuildInternalNodeJson(entry.recordNumber, entry.isDir, entry.dataDescriptor);
        if (pathByRecord) {
            pathByRecord->insert(entry.recordNumber, node.path);
        }
        if (entry.isDir) {
            ntfsBuildMftTreeBranch(recordNumber, node.path, childrenByParent, entries, node.children, pathByRecord);
        }
        target.append(node);
    }
}

RecoveryNode *findRecoveryNodeByPath(RecoveryNode &node, const QString &path) {
    if (node.path == path) {
        return &node;
    }
    for (RecoveryNode &child : node.children) {
        if (RecoveryNode *found = findRecoveryNodeByPath(child, path)) {
            return found;
        }
    }
    return nullptr;
}

VolumeExploreResult buildNtfsMftExploreResult(
    const QString &filePath,
    const QHash<std::uint64_t, NtfsMftEntry> &activeEntries,
    const QHash<std::uint64_t, NtfsMftEntry> &deletedEntries,
    bool includeDeleted,
    int scannedRecords,
    std::uint64_t mftStreamSize
) {
    VolumeExploreResult result;
    result.success = !activeEntries.isEmpty() || (includeDeleted && !deletedEntries.isEmpty());
    result.sourcePath = filePath;
    result.fileSystem = FileSystemKind::NTFS;

    RecoveryNode root = makeNode("NTFS reconstruido", "/NTFS reconstruido", true);
    root.statusLabel = "Reconstruido";
    RecoveryNode activeRoot = makeNode("Archivos y carpetas", "/NTFS reconstruido/Archivos y carpetas", true);
    activeRoot.statusLabel = "Activo";
    RecoveryNode deletedRoot = makeNode("Borrados detectados", "/NTFS reconstruido/Borrados detectados", true);
    deletedRoot.deleted = true;
    deletedRoot.statusLabel = "Borrado";

    QHash<std::uint64_t, QVector<std::uint64_t>> activeChildrenByParent;
    QHash<std::uint64_t, QVector<std::uint64_t>> deletedChildrenByParent;

    for (auto it = activeEntries.begin(); it != activeEntries.end(); ++it) {
        const NtfsMftEntry &entry = it.value();
        if (entry.recordNumber == 5 || entry.name.isEmpty()) {
            continue;
        }
        const bool parentExists = activeEntries.contains(entry.parentRef) && entry.parentRef != entry.recordNumber;
        activeChildrenByParent[parentExists ? entry.parentRef : 5].append(entry.recordNumber);
    }

    QHash<std::uint64_t, QString> activePathByRecord;
    ntfsBuildMftTreeBranch(5, activeRoot.path, activeChildrenByParent, activeEntries, activeRoot.children, &activePathByRecord);

    if (includeDeleted) {
        for (auto it = deletedEntries.begin(); it != deletedEntries.end(); ++it) {
            const NtfsMftEntry &entry = it.value();
            if (entry.recordNumber == 5 || entry.name.isEmpty()) {
                continue;
            }
            const bool parentExists = activeEntries.contains(entry.parentRef) && entry.parentRef != entry.recordNumber;
            deletedChildrenByParent[parentExists ? entry.parentRef : 5].append(entry.recordNumber);
        }

        ntfsBuildMftTreeBranch(5, deletedRoot.path, deletedChildrenByParent, deletedEntries, deletedRoot.children);

        for (auto it = deletedChildrenByParent.begin(); it != deletedChildrenByParent.end(); ++it) {
            if (it.key() == 5 || !activePathByRecord.contains(it.key())) {
                continue;
            }
            const QString parentPath = activePathByRecord.value(it.key());
            RecoveryNode *parentNode = findRecoveryNodeByPath(activeRoot, parentPath);
            if (!parentNode) {
                continue;
            }
            ntfsBuildMftTreeBranch(it.key(), parentNode->path, deletedChildrenByParent, deletedEntries, parentNode->children);
        }
    }

    finalizeFolderNode(activeRoot);
    if (includeDeleted) {
        finalizeFolderNode(deletedRoot);
    }
    root.children.append(activeRoot);
    if (includeDeleted && !deletedRoot.children.isEmpty()) {
        root.children.append(deletedRoot);
    }
    finalizeFolderNode(root);

    result.roots.append(root);
    result.summary =
        "Reconstruccion NTFS rapida basada en MFT.\n"
        "Activos detectados: " + QString::number(activeEntries.size()) +
        (includeDeleted ? "\nBorrados detectados: " + QString::number(deletedEntries.size()) : QString()) +
        "\nRegistros MFT procesados: " + QString::number(scannedRecords);
    result.details =
        "Metodo: lectura secuencial de MFT, parecido al flujo rapido de la version Python.\n"
        "Cobertura MFT estimada: " + QString::number(static_cast<qulonglong>(mftStreamSize / 1024ull / 1024ull)) + " MB\n"
        "Registros analizados: " + QString::number(scannedRecords) + "\n"
        "Arbol reconstruido: " + QString(result.success ? "si" : "no") + "\n"
        "Nota: el modo super rapido prioriza sacar estructura util muy pronto; para maxima cobertura usa el escaneo NTFS completo.";
    return result;
}

NtfsDeletedCandidate ntfsParseDeletedCandidate(
    const QByteArray &record,
    std::uint64_t recordNumber,
    std::uint64_t clusterSize
) {
    NtfsDeletedCandidate candidate;
    if (record.size() < 24) {
        return candidate;
    }

    const std::uint16_t flags = readLe16(record, 22);
    if ((flags & 0x0001u) != 0) {
        return candidate;
    }

    const std::uint16_t attrOffset = readLe16(record, 20);
    int offset = attrOffset;
    const NtfsDataDescriptor dataDescriptor = ntfsParseDataDescriptor(record);

    while (offset + 16 <= record.size()) {
        const std::uint32_t type = readLe32(record, offset);
        if (type == 0xFFFFFFFFu) {
            break;
        }
        const std::uint32_t length = readLe32(record, offset + 4);
        if (length == 0 || offset + static_cast<int>(length) > record.size()) {
            break;
        }

        const bool nonResident = readU8(record, offset + 8) != 0;
        if (type == 0x30u && !nonResident) {
            const std::uint32_t valueLength = readLe32(record, offset + 16);
            const std::uint16_t valueOffset = readLe16(record, offset + 20);
            const int valueStart = offset + valueOffset;
            if (valueLength >= 66 && valueStart + static_cast<int>(valueLength) <= record.size()) {
                const QByteArray value = record.mid(valueStart, valueLength);
                const std::uint64_t realSize = readLe64(value, 48);
                const std::uint32_t fileFlags = readLe32(value, 56);
                const bool isDir = (fileFlags & 0x10000000u) != 0;
                const int nameLength = readU8(value, 64);
                const QString name = decodeNtfsUtf16Name(value, 66, nameLength);
                if (!name.isEmpty() && name != "." && name != "..") {
                    QString metadata;
                    metadata += "Registro MFT: " + QString::number(static_cast<qulonglong>(recordNumber)) + "\n";
                    const QString created = formatNtfsFileTime(readLe64(value, 8));
                    const QString modified = formatNtfsFileTime(readLe64(value, 16));
                    const QString changed = formatNtfsFileTime(readLe64(value, 24));
                    const QString accessed = formatNtfsFileTime(readLe64(value, 32));
                    if (!created.isEmpty()) {
                        metadata += "Creado: " + created + "\n";
                    }
                    if (!modified.isEmpty()) {
                        metadata += "Modificado: " + modified + "\n";
                    }
                    if (!changed.isEmpty()) {
                        metadata += "Cambio MFT: " + changed + "\n";
                    }
                    if (!accessed.isEmpty()) {
                        metadata += "Accedido: " + accessed + "\n";
                    }
                    metadata += "Flags NTFS: 0x" + QString::number(fileFlags, 16).toUpper() + "\n";
                    metadata += ntfsBuildDataMetadata(dataDescriptor, clusterSize) + "\n";
                    metadata += "Estado de registro: no activo";

                    candidate.valid = true;
                    candidate.name = name;
                    candidate.isDir = isDir;
                    candidate.parentRef = readLe64(value, 0) & 0x0000FFFFFFFFFFFFull;
                    candidate.realSize = dataDescriptor.valid && !isDir ? dataDescriptor.realSize : realSize;
                    candidate.metadata = metadata;
                    return candidate;
                }
            }
        }

        offset += static_cast<int>(length);
    }
    return candidate;
}

bool recoveryNodeHasChild(const RecoveryNode &parent, const QString &name, bool isDir) {
    for (const RecoveryNode &child : parent.children) {
        if (child.name == name && child.kind == (isDir ? RecoveryNodeKind::Folder : RecoveryNodeKind::File)) {
            return true;
        }
    }
    return false;
}

void parseNtfsDirectoryRecursive(
    QFile &file,
    NtfsContext &context,
    std::uint64_t recordNumber,
    RecoveryNode &parent,
    int depth,
    int maxDepth
);

void parseNtfsIndexEntriesBuffer(
    QFile &file,
    NtfsContext &context,
    const QByteArray &buffer,
    int indexHeaderOffset,
    RecoveryNode &parent,
    int depth,
    int maxDepth
) {
    if (indexHeaderOffset + 16 > buffer.size()) {
        return;
    }
    const std::uint32_t entriesOffset = readLe32(buffer, indexHeaderOffset + 0);
    const std::uint32_t totalSize = readLe32(buffer, indexHeaderOffset + 4);
    int entryOffset = indexHeaderOffset + static_cast<int>(entriesOffset);
    const int entryEnd = indexHeaderOffset + static_cast<int>(totalSize);

    while (entryOffset + 16 <= buffer.size() && entryOffset < entryEnd && context.parsedEntries < context.maxEntries) {
        const std::uint64_t fileReference = readLe64(buffer, entryOffset) & 0x0000FFFFFFFFFFFFull;
        const std::uint16_t entryLength = readLe16(buffer, entryOffset + 8);
        const std::uint16_t streamLength = readLe16(buffer, entryOffset + 10);
        const std::uint8_t flags = readU8(buffer, entryOffset + 12);

        if (entryLength < 16 || entryOffset + entryLength > buffer.size()) {
            break;
        }

        if (streamLength >= 66 && fileReference > 15) {
            const QByteArray stream = buffer.mid(entryOffset + 16, streamLength);
            const QString name = decodeNtfsFileNameFromIndexEntry(stream);
            const std::uint64_t realSize = readLe64(stream, 48);
            const std::uint32_t fileFlags = readLe32(stream, 56);
            const bool isDir = (fileFlags & 0x10000000u) != 0;

            if (!name.isEmpty() && name != "." && name != ".." && !recoveryNodeHasChild(parent, name, isDir)) {
                const QByteArray childRecord = readNtfsRecord(file, context, fileReference);
                const NtfsDataDescriptor dataDescriptor = ntfsParseDataDescriptor(childRecord);
                const std::uint64_t effectiveSize =
                    (!isDir && dataDescriptor.valid) ? dataDescriptor.realSize : realSize;

                RecoveryNode child = makeNode(name, parent.path + "/" + name, isDir, isDir ? 0 : effectiveSize);
                QString metadata;
                metadata += "Registro MFT: " + QString::number(static_cast<qulonglong>(fileReference)) + "\n";
                const QString created = formatNtfsFileTime(readLe64(stream, 8));
                const QString modified = formatNtfsFileTime(readLe64(stream, 16));
                const QString changed = formatNtfsFileTime(readLe64(stream, 24));
                const QString accessed = formatNtfsFileTime(readLe64(stream, 32));
                if (!created.isEmpty()) {
                    metadata += "Creado: " + created + "\n";
                }
                if (!modified.isEmpty()) {
                    metadata += "Modificado: " + modified + "\n";
                }
                if (!changed.isEmpty()) {
                    metadata += "Cambio MFT: " + changed + "\n";
                }
                if (!accessed.isEmpty()) {
                    metadata += "Accedido: " + accessed + "\n";
                }
                metadata += "Flags NTFS: 0x" + QString::number(fileFlags, 16).toUpper() + "\n";
                metadata += ntfsBuildDataMetadata(dataDescriptor, context.clusterSize);
                child.metadataText = metadata;
                child.internalDataJson = ntfsBuildInternalNodeJson(fileReference, isDir, dataDescriptor);
                parent.children.append(child);
                RecoveryNode &childRef = parent.children.last();
                ++context.parsedEntries;

                if (isDir && depth < maxDepth) {
                    parseNtfsDirectoryRecursive(file, context, fileReference, childRef, depth + 1, maxDepth);
                }
            }
        }

        entryOffset += entryLength;
        if ((flags & 0x02u) != 0) {
            break;
        }
    }
}

RecoveryNode ntfsBuildDeletedRoot(QFile &file, NtfsContext &context, std::uint64_t maxRecordsToCheck) {
    RecoveryNode deletedRoot = makeNode("Borrados detectados", "/NTFS raiz/Borrados detectados", true);
    deletedRoot.deleted = true;
    deletedRoot.statusLabel = "Borrado";

    for (std::uint64_t recordNumber = 24;
         recordNumber < maxRecordsToCheck && context.parsedEntries < context.maxEntries;
         ++recordNumber) {
        const QByteArray record = readNtfsRecord(file, context, recordNumber);
        if (record.isEmpty()) {
            continue;
        }
        const NtfsDeletedCandidate candidate = ntfsParseDeletedCandidate(record, recordNumber, context.clusterSize);
        if (!candidate.valid) {
            continue;
        }
        const QString displayName = candidate.name + " [borrado]";
        if (recoveryNodeHasChild(deletedRoot, displayName, candidate.isDir)) {
            continue;
        }

        RecoveryNode child = makeNode(
            displayName,
            deletedRoot.path + "/" + displayName,
            candidate.isDir,
            candidate.isDir ? 0 : candidate.realSize
        );
        child.deleted = true;
        child.statusLabel = "Borrado";
        child.metadataText = candidate.metadata;
        child.internalDataJson = ntfsBuildInternalNodeJson(recordNumber, candidate.isDir, ntfsParseDataDescriptor(record));
        deletedRoot.children.append(child);
        ++context.deletedEntries;
    }

    finalizeFolderNode(deletedRoot);
    return deletedRoot;
}

void parseNtfsDirectoryRecursive(
    QFile &file,
    NtfsContext &context,
    std::uint64_t recordNumber,
    RecoveryNode &parent,
    int depth,
    int maxDepth
) {
    if (depth > maxDepth || context.parsedEntries >= context.maxEntries || context.visitedDirectories.contains(recordNumber)) {
        return;
    }
    context.visitedDirectories.insert(recordNumber);

    QByteArray record = readNtfsRecord(file, context, recordNumber);
    if (record.isEmpty()) {
        return;
    }

    const std::uint16_t attrOffset = readLe16(record, 20);
    int offset = attrOffset;

    while (offset + 16 <= record.size()) {
        const std::uint32_t type = readLe32(record, offset);
        if (type == 0xFFFFFFFFu) {
            break;
        }
        const std::uint32_t length = readLe32(record, offset + 4);
        if (length == 0 || offset + static_cast<int>(length) > record.size()) {
            break;
        }

        const bool nonResident = readU8(record, offset + 8) != 0;
        if (type == 0x90u && !nonResident) {
            const std::uint32_t valueLength = readLe32(record, offset + 16);
            const std::uint16_t valueOffset = readLe16(record, offset + 20);
            const int valueStart = offset + valueOffset;
            if (valueLength >= 32 && valueStart + static_cast<int>(valueLength) <= record.size()) {
                parseNtfsIndexEntriesBuffer(file, context, record, valueStart + 16, parent, depth, maxDepth);
            }
        } else if (type == 0xA0u && nonResident) {
            const std::uint16_t runOffset = readLe16(record, offset + 32);
            const std::uint64_t dataSize = readLe64(record, offset + 48);
            const QByteArray attrRecord = record.mid(offset, length);
            const QVector<NtfsRun> runs = ntfsParseDataRuns(attrRecord, runOffset);
            const QByteArray allocationData = ntfsReadRuns(
                file,
                context,
                runs,
                dataSize > 0 ? dataSize : 2ull * 1024ull * 1024ull
            );

            int blockOffset = 0;
            while (blockOffset + static_cast<int>(context.indexBufferSize) <= allocationData.size()) {
                QByteArray indexBuffer = allocationData.mid(blockOffset, context.indexBufferSize);
                if (indexBuffer.mid(0, 4) == "INDX" && ntfsApplyFixup(indexBuffer)) {
                    parseNtfsIndexEntriesBuffer(file, context, indexBuffer, 24, parent, depth, maxDepth);
                }
                blockOffset += static_cast<int>(context.indexBufferSize);
            }
        }

        offset += static_cast<int>(length);
    }
}

void parseFat32DirectoryRecursive(
    QFile &file,
    Fat32Context &context,
    std::uint32_t startCluster,
    RecoveryNode &parent,
    int depth,
    int maxDepth
) {
    if (startCluster < 2 || depth > maxDepth || context.parsedEntries >= context.maxEntries) {
        return;
    }

    const QByteArray dir = readFat32Chain(file, context, startCluster, 64);
    if (dir.isEmpty()) {
        return;
    }

    QStringList lfnParts;
    for (int offset = 0; offset + 32 <= dir.size() && context.parsedEntries < context.maxEntries; offset += 32) {
        const QByteArray entry = dir.mid(offset, 32);
        const std::uint8_t firstByte = readU8(entry, 0);
        if (firstByte == 0x00) {
            break;
        }
        const std::uint8_t attr = readU8(entry, 11);
        if (attr == 0x0F) {
            lfnParts.prepend(decodeFatLfnPart(entry));
            continue;
        }
        if ((attr & 0x08) != 0) {
            lfnParts.clear();
            continue;
        }

        const bool deleted = firstByte == 0xE5;
        QByteArray normalizedEntry = entry;
        if (deleted) {
            normalizedEntry[0] = '_';
        }
        QString name = lfnParts.isEmpty() ? decodeFatShortName(normalizedEntry) : lfnParts.join("");
        lfnParts.clear();
        if (name.isEmpty() || name == "." || name == "..") {
            continue;
        }
        if (deleted) {
            name += " [borrado]";
        }

        const bool isDir = (attr & 0x10) != 0;
        const std::uint32_t size = readLe32(normalizedEntry, 28);
        const std::uint32_t childCluster = fat32StartCluster(normalizedEntry);
        RecoveryNode child = makeNode(name, parent.path + "/" + name, isDir, isDir ? 0 : size);
        child.deleted = deleted;
        child.statusLabel = deleted ? "Borrado" : "Detectado";
        QString metadata;
        metadata += "Cluster inicial: " + QString::number(childCluster) + "\n";
        const QString modified = formatDosDateTime(readLe16(normalizedEntry, 24), readLe16(normalizedEntry, 22));
        if (!modified.isEmpty()) {
            metadata += "Modificado: " + modified + "\n";
        }
        metadata += "Atributos FAT: 0x" + QString::number(attr, 16).rightJustified(2, '0');
        child.metadataText = metadata;
        child.internalDataJson = buildFat32InternalNodeJson(childCluster, size, isDir, deleted);
        parent.children.append(child);
        RecoveryNode &childRef = parent.children.last();
        ++context.parsedEntries;
        if (deleted) {
            ++context.deletedEntries;
        }

        if (isDir && childCluster >= 2 && depth < maxDepth && !deleted) {
            parseFat32DirectoryRecursive(file, context, childCluster, childRef, depth + 1, maxDepth);
        }
    }
}

VolumeExploreResult exploreFat32(QFile &file, const QString &path, int maxEntries, int maxDepth) {
    file.seek(0);
    const QByteArray boot = file.read(512);
    if (boot.size() < 90) {
        return {false, path, FileSystemKind::FAT32, "No se pudo leer la cabecera FAT32.", "Cabecera insuficiente.", {}};
    }

    const std::uint64_t bytesPerSector = readLe16(boot, 11);
    const std::uint64_t sectorsPerCluster = readU8(boot, 13);
    const std::uint64_t reservedSectors = readLe16(boot, 14);
    const std::uint64_t fatCount = readU8(boot, 16);
    const std::uint64_t sectorsPerFat = readLe32(boot, 36);
    const std::uint32_t rootCluster = readLe32(boot, 44);
    const std::uint64_t firstDataSector = reservedSectors + fatCount * sectorsPerFat;
    const std::uint64_t fatOffset = reservedSectors * bytesPerSector;
    const std::uint64_t fatBytes = sectorsPerFat * bytesPerSector;

    file.seek(static_cast<qint64>(fatOffset));
    const QByteArray fatTable = file.read(static_cast<qint64>(fatBytes));
    if (fatTable.isEmpty()) {
        return {false, path, FileSystemKind::FAT32, "No se pudo leer la FAT.", "Offset FAT: " + QString::number(static_cast<qulonglong>(fatOffset)), {}};
    }

    Fat32Context context;
    context.bytesPerSector = bytesPerSector;
    context.sectorsPerCluster = sectorsPerCluster;
    context.reservedSectors = reservedSectors;
    context.fatCount = fatCount;
    context.sectorsPerFat = sectorsPerFat;
    context.firstDataSector = firstDataSector;
    context.rootCluster = rootCluster;
    context.fatTable = fatTable;
    context.maxEntries = maxEntries;
    context.parsedEntries = 0;
    context.deletedEntries = 0;

    RecoveryNode root = makeNode("FAT32 raiz", "/FAT32 raiz", true);
    parseFat32DirectoryRecursive(file, context, rootCluster, root, 0, maxDepth);
    finalizeFolderNode(root);

    VolumeExploreResult result;
    result.success = !root.children.isEmpty();
    result.sourcePath = path;
    result.fileSystem = FileSystemKind::FAT32;
    result.roots = {root};
    result.summary = "Exploracion FAT32 completada.";
    result.details =
        "Entradas raiz detectadas: " + QString::number(root.children.size()) + "\n"
        "Entradas totales parseadas: " + QString::number(context.parsedEntries) + "\n"
        "Entradas borradas detectadas: " + QString::number(context.deletedEntries) + "\n"
        "Cluster raiz: " + QString::number(rootCluster) + "\n"
        "Bytes/sector: " + QString::number(static_cast<qulonglong>(bytesPerSector)) + "\n"
        "Sectores/cluster: " + QString::number(static_cast<qulonglong>(sectorsPerCluster));
    return result;
}

QString decodeExfatNameChunk(const QByteArray &entry) {
    QString result;
    for (int offset = 2; offset < 32; offset += 2) {
        const std::uint16_t value = readLe16(entry, offset);
        if (value == 0x0000 || value == 0xFFFF) {
            break;
        }
        result.append(QChar(value));
    }
    return result;
}

std::uint32_t exfatClusterValue(const ExfatContext &context, std::uint32_t cluster) {
    const int offset = static_cast<int>(cluster * 4);
    if (offset + 4 > context.fatTable.size()) {
        return 0xFFFFFFFFu;
    }
    return readLe32(context.fatTable, offset);
}

QByteArray readExfatCluster(QFile &file, const ExfatContext &context, std::uint32_t cluster) {
    const std::uint64_t offset = clusterOffset(
        cluster,
        context.clusterHeapOffset,
        context.sectorsPerCluster,
        context.bytesPerSector
    );
    const std::uint64_t bytes = context.bytesPerSector * context.sectorsPerCluster;
    file.seek(static_cast<qint64>(offset));
    return file.read(static_cast<qint64>(bytes));
}

QByteArray readExfatDirectoryData(
    QFile &file,
    const ExfatContext &context,
    std::uint32_t startCluster,
    std::uint64_t dataLength,
    bool noFatChain,
    int maxClusters = 128
) {
    QByteArray data;
    if (startCluster < 2) {
        return data;
    }

    const std::uint64_t clusterSize = context.bytesPerSector * context.sectorsPerCluster;
    int clustersToRead = 1;
    if (dataLength > 0) {
        clustersToRead = static_cast<int>((dataLength + clusterSize - 1) / clusterSize);
        if (clustersToRead < 1) {
            clustersToRead = 1;
        }
    }
    if (clustersToRead > maxClusters) {
        clustersToRead = maxClusters;
    }

    if (noFatChain) {
        for (int i = 0; i < clustersToRead; ++i) {
            data.append(readExfatCluster(file, context, startCluster + static_cast<std::uint32_t>(i)));
        }
    } else {
        QSet<std::uint32_t> seen;
        std::uint32_t cluster = startCluster;
        int count = 0;
        while (cluster >= 2 && cluster < 0xFFFFFFF8u && !seen.contains(cluster) && count < clustersToRead) {
            seen.insert(cluster);
            data.append(readExfatCluster(file, context, cluster));
            cluster = exfatClusterValue(context, cluster);
            ++count;
        }
    }

    if (dataLength > 0 && static_cast<std::uint64_t>(data.size()) > dataLength) {
        data.truncate(static_cast<int>(dataLength));
    }
    return data;
}

void parseExfatDirectoryRecursive(
    QFile &file,
    ExfatContext &context,
    std::uint32_t startCluster,
    std::uint64_t dataLength,
    RecoveryNode &parent,
    int depth,
    int maxDepth
) {
    if (startCluster < 2 || depth > maxDepth || context.parsedEntries >= context.maxEntries) {
        return;
    }

    const QByteArray dir = readExfatDirectoryData(file, context, startCluster, dataLength, false, 128);
    if (dir.isEmpty()) {
        return;
    }

    for (int offset = 0; offset + 32 <= dir.size() && context.parsedEntries < context.maxEntries; ) {
        const QByteArray entry = dir.mid(offset, 32);
        const std::uint8_t type = readU8(entry, 0);
        if (type == 0x00) {
            break;
        }
        const bool deleted = type == 0x05;
        if (!(type == 0x85 || deleted)) {
            offset += 32;
            continue;
        }

        const int secondaryCount = readU8(entry, 1);
        const std::uint16_t attributes = readLe16(entry, 4);
        const std::uint16_t createdTime = readLe16(entry, 8);
        const std::uint16_t createdDate = readLe16(entry, 10);
        const std::uint16_t modifiedTime = readLe16(entry, 12);
        const std::uint16_t modifiedDate = readLe16(entry, 14);
        const std::uint16_t accessedTime = readLe16(entry, 16);
        const std::uint16_t accessedDate = readLe16(entry, 18);
        const bool isDir = (attributes & 0x10) != 0;
        QString name;
        std::uint64_t dataLen = 0;
        std::uint32_t firstCluster = 0;
        bool noFatChain = false;

        for (int i = 1; i <= secondaryCount && offset + (i * 32) + 32 <= dir.size(); ++i) {
            const QByteArray secondary = dir.mid(offset + i * 32, 32);
            const std::uint8_t secondaryType = readU8(secondary, 0);
            if (secondaryType == 0xC0 || secondaryType == 0x40) {
                noFatChain = (readU8(secondary, 1) & 0x02) != 0;
                firstCluster = readLe32(secondary, 20);
                dataLen = readLe64(secondary, 24);
                if (dataLen == 0) {
                    dataLen = readLe64(secondary, 8);
                }
            } else if (secondaryType == 0xC1 || secondaryType == 0x41) {
                name += decodeExfatNameChunk(secondary);
            }
        }

        if (!name.isEmpty() && name != "." && name != "..") {
            if (deleted) {
                name += " [borrado]";
            }
            RecoveryNode child = makeNode(name, parent.path + "/" + name, isDir, isDir ? 0 : dataLen);
            child.deleted = deleted;
            child.statusLabel = deleted ? "Borrado" : "Detectado";
            QString metadata;
            metadata += "Cluster inicial: " + QString::number(firstCluster) + "\n";
            const QString created = formatDosDateTime(createdDate, createdTime);
            const QString modified = formatDosDateTime(modifiedDate, modifiedTime);
            const QString accessed = formatDosDateTime(accessedDate, accessedTime);
            if (!created.isEmpty()) {
                metadata += "Creado: " + created + "\n";
            }
            if (!modified.isEmpty()) {
                metadata += "Modificado: " + modified + "\n";
            }
            if (!accessed.isEmpty()) {
                metadata += "Accedido: " + accessed + "\n";
            }
            metadata += "Atributos exFAT: 0x" + QString::number(attributes, 16).rightJustified(4, '0').toUpper() + "\n";
            metadata += "Cadena FAT: " + QString(noFatChain ? "no usada (contigua)" : "usada");
            child.metadataText = metadata;
            child.internalDataJson = buildExfatInternalNodeJson(firstCluster, dataLen, isDir, deleted, noFatChain);
            parent.children.append(child);
            RecoveryNode &childRef = parent.children.last();
            ++context.parsedEntries;
            if (deleted) {
                ++context.deletedEntries;
            }

            if (isDir && firstCluster >= 2 && depth < maxDepth && !deleted) {
                const QByteArray subdir = readExfatDirectoryData(file, context, firstCluster, dataLen, noFatChain, 128);
                if (!subdir.isEmpty()) {
                    // Reusar el lector recursivo sobre el directorio ya direccionado por cluster.
                    parseExfatDirectoryRecursive(file, context, firstCluster, dataLen, childRef, depth + 1, maxDepth);
                }
            }
        }
        offset += (secondaryCount + 1) * 32;
    }
}

VolumeExploreResult exploreExfat(QFile &file, const QString &path, int maxEntries, int maxDepth) {
    file.seek(0);
    const QByteArray boot = file.read(512);
    if (boot.size() < 120) {
        return {false, path, FileSystemKind::ExFAT, "No se pudo leer la cabecera exFAT.", "Cabecera insuficiente.", {}};
    }

    const std::uint8_t sectorShift = readU8(boot, 108);
    const std::uint8_t clusterShift = readU8(boot, 109);
    const std::uint64_t bytesPerSector = 1ull << sectorShift;
    const std::uint64_t sectorsPerCluster = 1ull << clusterShift;
    const std::uint64_t fatOffsetSectors = readLe32(boot, 80);
    const std::uint64_t fatLengthSectors = readLe32(boot, 84);
    const std::uint64_t clusterHeapOffset = readLe32(boot, 88);
    const std::uint32_t rootCluster = readLe32(boot, 96);
    const std::uint64_t fatOffsetBytes = fatOffsetSectors * bytesPerSector;
    const std::uint64_t fatBytes = fatLengthSectors * bytesPerSector;

    file.seek(static_cast<qint64>(fatOffsetBytes));
    const QByteArray fatTable = file.read(static_cast<qint64>(fatBytes));
    if (fatTable.isEmpty()) {
        return {false, path, FileSystemKind::ExFAT, "No se pudo leer la FAT exFAT.", "Offset FAT: " + QString::number(static_cast<qulonglong>(fatOffsetBytes)), {}};
    }

    ExfatContext context;
    context.bytesPerSector = bytesPerSector;
    context.sectorsPerCluster = sectorsPerCluster;
    context.clusterHeapOffset = clusterHeapOffset;
    context.rootCluster = rootCluster;
    context.fatTable = fatTable;
    context.maxEntries = maxEntries;
    context.parsedEntries = 0;
    context.deletedEntries = 0;

    RecoveryNode root = makeNode("exFAT raiz", "/exFAT raiz", true);
    parseExfatDirectoryRecursive(file, context, rootCluster, bytesPerSector * sectorsPerCluster * 32, root, 0, maxDepth);
    finalizeFolderNode(root);

    VolumeExploreResult result;
    result.success = !root.children.isEmpty();
    result.sourcePath = path;
    result.fileSystem = FileSystemKind::ExFAT;
    result.roots = {root};
    result.summary = "Exploracion exFAT completada.";
    result.details =
        "Entradas raiz detectadas: " + QString::number(root.children.size()) + "\n"
        "Entradas totales parseadas: " + QString::number(context.parsedEntries) + "\n"
        "Entradas borradas detectadas: " + QString::number(context.deletedEntries) + "\n"
        "Cluster raiz: " + QString::number(rootCluster) + "\n"
        "Tamano de sector: " + QString::number(static_cast<qulonglong>(bytesPerSector)) + "\n"
        "Sectores/cluster: " + QString::number(static_cast<qulonglong>(sectorsPerCluster));
    return result;
}

VolumeExploreResult exploreNtfs(QFile &file, const QString &path, int maxEntries, int maxDepth, bool includeDeleted) {
    file.seek(0);
    const QByteArray boot = file.read(512);
    if (boot.size() < 80 || boot.mid(3, 8) != "NTFS    ") {
        return {false, path, FileSystemKind::NTFS, "No se pudo validar la cabecera NTFS.", "Cabecera NTFS no reconocida.", {}};
    }

    NtfsContext context;
    context.bytesPerSector = readLe16(boot, 11);
    context.sectorsPerCluster = readU8(boot, 13);
    context.clusterSize = context.bytesPerSector * context.sectorsPerCluster;
    context.mftLcn = readLe64(boot, 48);
    context.mftOffset = context.mftLcn * context.clusterSize;
    context.recordSize = ntfsRecordSize(boot, context.clusterSize);
    context.indexBufferSize = ntfsSizedStructure(boot, 68, context.clusterSize, 4096);
    context.maxEntries = maxEntries;
    context.parsedEntries = 0;
    context.deletedEntries = 0;

    RecoveryNode root = makeNode("NTFS raiz", "/NTFS raiz", true);
    parseNtfsDirectoryRecursive(file, context, 5, root, 0, maxDepth);
    if (includeDeleted) {
        RecoveryNode deletedRoot = ntfsBuildDeletedRoot(file, context, 4096);
        if (!deletedRoot.children.isEmpty()) {
            root.children.append(deletedRoot);
        }
    }
    finalizeFolderNode(root);

    VolumeExploreResult result;
    result.success = !root.children.isEmpty();
    result.sourcePath = path;
    result.fileSystem = FileSystemKind::NTFS;
    result.roots = {root};
    result.summary = "Exploracion NTFS completada con indices y analisis basico de DATA.";
    result.details =
        "Entradas raiz detectadas: " + QString::number(root.children.size()) + "\n"
        "Entradas totales parseadas: " + QString::number(context.parsedEntries) + "\n"
        "Entradas borradas detectadas: " + QString::number(context.deletedEntries) + "\n"
        "Bytes/sector: " + QString::number(static_cast<qulonglong>(context.bytesPerSector)) + "\n"
        "Sectores/cluster: " + QString::number(static_cast<qulonglong>(context.sectorsPerCluster)) + "\n"
        "Tamano de cluster: " + QString::number(static_cast<qulonglong>(context.clusterSize)) + "\n"
        "LCN de MFT: " + QString::number(static_cast<qulonglong>(context.mftLcn)) + "\n"
        "Tamano de registro MFT: " + QString::number(context.recordSize) + "\n"
        "Tamano de buffer de indice: " + QString::number(context.indexBufferSize) + "\n"
        "Motor activo: INDEX_ROOT + INDEX_ALLOCATION + atributo DATA\n"
        "Nota: algunos directorios complejos o archivos muy danados aun pueden quedar incompletos.";
    return result;
}
} // namespace

VolumeExploreResult VolumeExplorer::explore(
    const QString &filePath,
    FileSystemKind fileSystem,
    int maxEntries,
    int maxDepth,
    bool includeDeleted
) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {false, filePath, fileSystem, "No se pudo abrir el volumen o imagen.", "Permiso denegado o archivo inaccesible.", {}};
    }

    switch (fileSystem) {
    case FileSystemKind::NTFS:
        return exploreNtfs(file, filePath, maxEntries, maxDepth, includeDeleted);
    case FileSystemKind::FAT32:
        return exploreFat32(file, filePath, maxEntries, maxDepth);
    case FileSystemKind::ExFAT:
        return exploreExfat(file, filePath, maxEntries, maxDepth);
    default:
        return {
            false,
            filePath,
            fileSystem,
            "Este explorador inicial aun no implementa listado real para este sistema de archivos.",
            "Primer soporte activo: FAT32 y exFAT en directorio raiz.",
            {}
        };
    }
}

VolumeExploreResult VolumeExplorer::exploreFastNtfsMft(
    const QString &filePath,
    int maxEntries,
    int maxRecords,
    bool includeDeleted,
    FastScanProgressCallback progressCallback
) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {
            false,
            filePath,
            FileSystemKind::NTFS,
            "No se pudo abrir el volumen o imagen.",
            "Permiso denegado o archivo inaccesible.",
            {}
        };
    }

    const std::uint64_t deviceSize = exactDeviceSizeBytes(filePath);
    file.seek(0);
    const QByteArray boot = file.read(512);
    const NtfsBootScanInfo scan = ntfsParseBootScanInfo(boot, deviceSize);
    if (!scan.valid) {
        return {false, filePath, FileSystemKind::NTFS, scan.error, "Cabecera NTFS no valida para escaneo rapido MFT.", {}};
    }

    file.seek(static_cast<qint64>(scan.mftOffset));
    QByteArray mftRecord = file.read(static_cast<qint64>(scan.recordSize));
    if (mftRecord.size() < static_cast<int>(scan.recordSize) || !ntfsApplyFixup(mftRecord)) {
        return {false, filePath, FileSystemKind::NTFS, "No se pudo leer el registro base de $MFT.", "La lectura directa del registro MFT fallo.", {}};
    }

    bool resident = true;
    const QByteArray mftDataAttr = ntfsFindAttributeRaw(mftRecord, 0x80u, &resident);
    if (mftDataAttr.isEmpty() || resident) {
        return {false, filePath, FileSystemKind::NTFS, "No se pudo localizar el flujo de datos de $MFT.", "El atributo DATA de $MFT no es legible o parece residente.", {}};
    }

    const NtfsDataDescriptor mftDescriptor = ntfsParseDataDescriptor(mftRecord);
    if (!mftDescriptor.valid || mftDescriptor.runs.isEmpty()) {
        return {false, filePath, FileSystemKind::NTFS, "La runlist de $MFT no es legible.", "No se pudo reconstruir el mapa fisico de la MFT.", {}};
    }

    const std::uint64_t mftStreamSize =
        mftDescriptor.realSize > 0
            ? mftDescriptor.realSize
            : mftDescriptor.allocatedSize;
    const int recordCount = std::max<int>(
        1,
        std::min<int>(
            maxRecords,
            static_cast<int>(std::min<std::uint64_t>(mftStreamSize / scan.recordSize, 500000ull))
        )
    );

    if (progressCallback) {
        progressCallback(0, recordCount, 0, 0);
    }

    QHash<std::uint64_t, NtfsMftEntry> activeEntries;
    QHash<std::uint64_t, NtfsMftEntry> deletedEntries;

    for (int recordIndex = 0; recordIndex < recordCount; ++recordIndex) {
        if (static_cast<int>(activeEntries.size()) >= maxEntries &&
            (!includeDeleted || static_cast<int>(deletedEntries.size()) >= std::max(64, maxEntries / 8))) {
            break;
        }

        const QByteArray record = ntfsReadRecordFromRuns(file, scan, mftDescriptor.runs, static_cast<std::uint64_t>(recordIndex));
        if (record.isEmpty()) {
            continue;
        }

        NtfsMftEntry entry;
        if (!ntfsParseMftEntryRecord(record, static_cast<std::uint64_t>(recordIndex), scan.clusterSize, entry)) {
            continue;
        }

        if (entry.active) {
            activeEntries.insert(entry.recordNumber, entry);
        } else if (includeDeleted) {
            deletedEntries.insert(entry.recordNumber, entry);
        }

        if (progressCallback && (recordIndex < 32 || ((recordIndex + 1) % 256) == 0 || recordIndex + 1 == recordCount)) {
            progressCallback(
                recordIndex + 1,
                recordCount,
                activeEntries.size(),
                deletedEntries.size()
            );
        }
    }

    return buildNtfsMftExploreResult(
        filePath,
        activeEntries,
        deletedEntries,
        includeDeleted,
        recordCount,
        mftStreamSize
    );
}
