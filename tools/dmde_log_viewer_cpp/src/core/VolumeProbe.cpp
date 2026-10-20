#include "VolumeProbe.h"

#include <QByteArray>
#include <QFile>
#include <QStringList>

namespace {
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

QString formatHex(std::uint64_t value) {
    return "0x" + QString::number(static_cast<qulonglong>(value), 16);
}

VolumeProbeResult makeUnknownResult(const QString &path, const QString &reason) {
    VolumeProbeResult result;
    result.success = false;
    result.sourcePath = path;
    result.detectedLabel = "Desconocido";
    result.summary = "No se pudo identificar un sistema de archivos compatible en la cabecera leida.";
    result.details = reason;
    return result;
}

VolumeProbeResult probeNtfs(const QString &path, const QByteArray &boot) {
    VolumeProbeResult result;
    result.success = true;
    result.sourcePath = path;
    result.detectedFileSystem = FileSystemKind::NTFS;
    result.detectedLabel = "NTFS";
    result.blockOrSectorSize = readLe16(boot, 11);
    const std::uint8_t sectorsPerCluster = readU8(boot, 13);
    result.clusterSize = result.blockOrSectorSize * sectorsPerCluster;
    result.totalUnits = readLe64(boot, 40);
    const std::uint64_t mftLcn = readLe64(boot, 48);
    result.summary = "Cabecera NTFS valida detectada. El motor de metadatos podra apoyarse en MFT y directorios.";
    result.details =
        "OEM: NTFS\n"
        "Bytes/sector: " + QString::number(result.blockOrSectorSize) + "\n"
        "Sectores/cluster: " + QString::number(sectorsPerCluster) + "\n"
        "Tamano de cluster: " + QString::number(static_cast<qulonglong>(result.clusterSize)) + "\n"
        "Total de sectores: " + QString::number(static_cast<qulonglong>(result.totalUnits)) + "\n"
        "LCN de MFT: " + QString::number(static_cast<qulonglong>(mftLcn));
    return result;
}

VolumeProbeResult probeExfat(const QString &path, const QByteArray &boot) {
    VolumeProbeResult result;
    result.success = true;
    result.sourcePath = path;
    result.detectedFileSystem = FileSystemKind::ExFAT;
    result.detectedLabel = "exFAT";
    const std::uint8_t sectorShift = readU8(boot, 108);
    const std::uint8_t clusterShift = readU8(boot, 109);
    result.blockOrSectorSize = 1ull << sectorShift;
    result.clusterSize = result.blockOrSectorSize * (1ull << clusterShift);
    result.totalUnits = readLe64(boot, 72);
    const std::uint32_t clusterCount = readLe32(boot, 92);
    const std::uint32_t rootCluster = readLe32(boot, 96);
    result.summary = "Cabecera exFAT valida detectada. Buen candidato para recuperar fotos y videos con estructura reciente.";
    result.details =
        "OEM: EXFAT\n"
        "Shift de sector: " + QString::number(sectorShift) + "\n"
        "Shift de cluster: " + QString::number(clusterShift) + "\n"
        "Tamano de sector: " + QString::number(static_cast<qulonglong>(result.blockOrSectorSize)) + "\n"
        "Tamano de cluster: " + QString::number(static_cast<qulonglong>(result.clusterSize)) + "\n"
        "Longitud del volumen: " + QString::number(static_cast<qulonglong>(result.totalUnits)) + "\n"
        "Numero de clusters: " + QString::number(clusterCount) + "\n"
        "Cluster raiz: " + QString::number(rootCluster);
    return result;
}

VolumeProbeResult probeFat32(const QString &path, const QByteArray &boot) {
    VolumeProbeResult result;
    result.success = true;
    result.sourcePath = path;
    result.detectedFileSystem = FileSystemKind::FAT32;
    result.detectedLabel = "FAT32";
    result.blockOrSectorSize = readLe16(boot, 11);
    const std::uint8_t sectorsPerCluster = readU8(boot, 13);
    result.clusterSize = result.blockOrSectorSize * sectorsPerCluster;
    const std::uint32_t totalSectors = readLe32(boot, 32);
    const std::uint32_t sectorsPerFat = readLe32(boot, 36);
    const std::uint32_t rootCluster = readLe32(boot, 44);
    result.totalUnits = totalSectors;
    result.summary = "Cabecera FAT32 valida detectada. Adecuado para tarjetas, camaras y memorias USB clasicas.";
    result.details =
        "Etiqueta FS: FAT32\n"
        "Bytes/sector: " + QString::number(result.blockOrSectorSize) + "\n"
        "Sectores/cluster: " + QString::number(sectorsPerCluster) + "\n"
        "Tamano de cluster: " + QString::number(static_cast<qulonglong>(result.clusterSize)) + "\n"
        "Sectores totales: " + QString::number(totalSectors) + "\n"
        "Sectores por FAT: " + QString::number(sectorsPerFat) + "\n"
        "Cluster raiz: " + QString::number(rootCluster);
    return result;
}

VolumeProbeResult probeExt4(const QString &path, const QByteArray &data) {
    VolumeProbeResult result;
    result.success = true;
    result.sourcePath = path;
    result.detectedFileSystem = FileSystemKind::EXT4;
    result.detectedLabel = "ext4";
    const std::uint32_t logBlockSize = readLe32(data, 1024 + 24);
    result.blockOrSectorSize = 1024ull << logBlockSize;
    const std::uint32_t blocksCountLo = readLe32(data, 1024 + 4);
    const std::uint32_t inodesCount = readLe32(data, 1024 + 0);
    result.totalUnits = blocksCountLo;
    result.summary = "Superbloque ext4 detectado. El motor podra trabajar con inodos, grupos y extents.";
    result.details =
        "Firma: ext4\n"
        "Tamano de bloque: " + QString::number(static_cast<qulonglong>(result.blockOrSectorSize)) + "\n"
        "Bloques declarados: " + QString::number(blocksCountLo) + "\n"
        "Inodos declarados: " + QString::number(inodesCount) + "\n"
        "Magic: 0xEF53";
    return result;
}

VolumeProbeResult probeApfs(const QString &path, const QByteArray &data) {
    VolumeProbeResult result;
    result.success = true;
    result.sourcePath = path;
    result.detectedFileSystem = FileSystemKind::APFS;
    result.detectedLabel = "APFS";
    result.blockOrSectorSize = readLe32(data, 36);
    const std::uint64_t blockCount = readLe64(data, 40);
    result.totalUnits = blockCount;
    result.summary = "Contenedor APFS detectado. El soporte inicial podra centrarse en container, volumes y arboles base.";
    result.details =
        "Firma: NXSB\n"
        "Tamano de bloque: " + QString::number(static_cast<qulonglong>(result.blockOrSectorSize)) + "\n"
        "Bloques del contenedor: " + QString::number(static_cast<qulonglong>(blockCount)) + "\n"
        "Offset magic: " + formatHex(32);
    return result;
}

VolumeProbeResult probeHfs(const QString &path, const QByteArray &data, const QString &label) {
    VolumeProbeResult result;
    result.success = true;
    result.sourcePath = path;
    result.detectedFileSystem = FileSystemKind::HFSPlus;
    result.detectedLabel = label;
    result.blockOrSectorSize = readLe32(data, 1024 + 40);
    const std::uint32_t totalBlocks = readLe32(data, 1024 + 44);
    result.totalUnits = totalBlocks;
    result.summary = "Volumen " + label + " detectado. El soporte podra usar catalog file y extents overflow.";
    result.details =
        "Firma: " + label + "\n"
        "Tamano de bloque: " + QString::number(static_cast<qulonglong>(result.blockOrSectorSize)) + "\n"
        "Bloques totales: " + QString::number(totalBlocks);
    return result;
}
} // namespace

VolumeProbeResult VolumeProbe::probe(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return makeUnknownResult(filePath, "No se pudo abrir la fuente en solo lectura.");
    }

    const QByteArray head = file.read(8192);
    if (head.size() < 2048) {
        return makeUnknownResult(filePath, "No hay suficientes bytes para identificar la cabecera.");
    }

    const QByteArray oem = head.mid(3, 8);
    const QByteArray fat32Label = head.mid(82, 8);
    const std::uint16_t extMagic = readLe16(head, 1024 + 56);
    const QByteArray apfsMagic = head.mid(32, 4);
    const QByteArray hfsMagic = head.mid(1024, 2);

    if (oem == "NTFS    ") {
        return probeNtfs(filePath, head);
    }
    if (oem == "EXFAT   ") {
        return probeExfat(filePath, head);
    }
    if (fat32Label == "FAT32   ") {
        return probeFat32(filePath, head);
    }
    if (extMagic == 0xEF53) {
        return probeExt4(filePath, head);
    }
    if (apfsMagic == "NXSB") {
        return probeApfs(filePath, head);
    }
    if (hfsMagic == "H+") {
        return probeHfs(filePath, head, "HFS+");
    }
    if (hfsMagic == "HX") {
        return probeHfs(filePath, head, "HFSX");
    }

    QStringList guesses;
    if (head.size() >= 512) {
        guesses << "Cabecera leida: " + QString::number(head.size()) + " bytes";
        guesses << "OEM offset 0x03: " + QString::fromLatin1(oem);
        guesses << "FS label FAT32 offset 0x52: " + QString::fromLatin1(fat32Label);
        guesses << "APFS magic offset 0x20: " + QString::fromLatin1(apfsMagic);
        guesses << "ext magic offset 0x438: " + formatHex(extMagic);
    }
    return makeUnknownResult(filePath, guesses.join("\n"));
}
