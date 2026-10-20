#include "DiskDoctor.h"

#include <algorithm>
#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>
#include <QVariantMap>

#include "VolumeProbe.h"

namespace {
QString fsDisplay(FileSystemKind kind, const QString &fallbackLabel) {
    const QString label = toDisplayString(kind);
    return label == "Auto" ? fallbackLabel : label;
}

bool looksLikeDevicePath(const QString &path) {
    return path.startsWith("/dev/");
}

QVariantMap parseLsblkPairs(const QString &text) {
    QVariantMap map;
    const QStringList parts = text.trimmed().split(' ', Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        const int eq = part.indexOf('=');
        if (eq <= 0) {
            continue;
        }
        const QString key = part.left(eq);
        QString value = part.mid(eq + 1);
        if (value.startsWith('"') && value.endsWith('"') && value.size() >= 2) {
            value = value.mid(1, value.size() - 2);
        }
        map.insert(key, value);
    }
    return map;
}

QVariantMap probeLinuxBlockInfo(const QString &path) {
    QProcess process;
    process.start("lsblk", {"-P", "-b", "-o", "PATH,SIZE,FSTYPE,LABEL,UUID,MOUNTPOINT,TYPE,MODEL,RO", path});
    if (!process.waitForFinished(2500) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return {};
    }
    return parseLsblkPairs(QString::fromUtf8(process.readAllStandardOutput()));
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

bool sampleRead(QFile &file, std::uint64_t offset, int bytesToRead, QStringList &notes) {
    if (!file.seek(static_cast<qint64>(offset))) {
        notes.append("No se pudo posicionar en offset " + QString::number(static_cast<qulonglong>(offset)));
        return false;
    }
    const QByteArray data = file.read(bytesToRead);
    if (data.size() != bytesToRead) {
        notes.append(
            "Lectura corta o con error en offset " +
            QString::number(static_cast<qulonglong>(offset)) +
            " (" + QString::number(data.size()) + "/" + QString::number(bytesToRead) + " bytes)"
        );
        return false;
    }
    return true;
}

QStringList genericRepairPlan(FileSystemKind kind, bool hasReadErrors, bool seemsMounted) {
    QStringList plan;
    plan << "1. No escribas en el disco hasta terminar el diagnostico.";
    if (hasReadErrors) {
        plan << "2. Prioridad alta: clonar o crear imagen antes de intentar reparar.";
    } else {
        plan << "2. Si no hay errores de lectura claros, puedes intentar una verificacion logica primero.";
    }

    switch (kind) {
    case FileSystemKind::NTFS:
        plan << "3. Revisar MFT y directorios desde Recovery Studio.";
        plan << "4. Si el volumen es importante, validar primero en solo lectura.";
        plan << "5. Reparacion habitual: ntfsfix en Linux o chkdsk en Windows, mejor despues de clonar.";
        break;
    case FileSystemKind::ExFAT:
        plan << "3. Comprobar la FAT y el directorio raiz.";
        plan << "4. Reparacion habitual: fsck.exfat o utilidad equivalente, preferiblemente tras copia.";
        break;
    case FileSystemKind::FAT32:
        plan << "3. Comprobar FAT, cluster raiz y longitud de cadenas.";
        plan << "4. Reparacion habitual: fsck.vfat/dosfsck en modo no destructivo primero.";
        break;
    case FileSystemKind::EXT4:
        plan << "3. Revisar superbloque, grupos y diario.";
        plan << "4. Reparacion habitual: e2fsck en modo solo lectura primero, luego reparacion si procede.";
        break;
    case FileSystemKind::APFS:
    case FileSystemKind::HFSPlus:
        plan << "3. Revisar metadatos del contenedor/volumen y catalogos.";
        plan << "4. En Apple suele convenir validar desde herramientas especificas del sistema o imagen clonica.";
        break;
    default:
        plan << "3. Como no hay formato claro, prioriza recuperacion por firmas o imagen completa.";
        break;
    }

    if (seemsMounted) {
        plan << "6. El origen parece montado: desmontar antes de reparaciones reales.";
    } else {
        plan << "6. El origen no parece montado: buen momento para diagnostico en frio.";
    }
    return plan;
}

bool reportProgress(const DiskDoctor::ProgressCallback &progress, int percent, const QString &phase, const QString &detail) {
    if (!progress) {
        return true;
    }
    return progress(percent, phase, detail);
}
} // namespace

DiskDoctorResult DiskDoctor::diagnose(const QString &sourcePath, ProgressCallback progress) {
    DiskDoctorResult result;
    result.sourcePath = sourcePath;

    if (!reportProgress(progress, 2, "Validando origen", "Comprobando que la ruta exista y sea accesible.")) {
        result.summary = "Diagnostico cancelado por el usuario.";
        result.details = sourcePath;
        return result;
    }

    QFileInfo info(sourcePath);
    if (!info.exists()) {
        result.summary = "La ruta indicada no existe.";
        result.details = sourcePath;
        result.warnings << "Selecciona un disco, particion o imagen valido.";
        return result;
    }

    if (!reportProgress(progress, 10, "Identificando sistema de archivos", "Leyendo la cabecera principal del origen.")) {
        result.summary = "Diagnostico cancelado por el usuario.";
        result.details = sourcePath;
        return result;
    }
    const VolumeProbeResult probe = VolumeProbe::probe(sourcePath);
    result.detectedFileSystem = probe.detectedFileSystem;
    result.detectedLabel = probe.detectedLabel;

    QVariantMap linuxInfo;
    if (looksLikeDevicePath(sourcePath)) {
        if (!reportProgress(progress, 18, "Consultando informacion del bloque", "Leyendo tamaño, montaje, UUID y modelo con lsblk.")) {
            result.summary = "Diagnostico cancelado por el usuario.";
            result.details = sourcePath;
            return result;
        }
        linuxInfo = probeLinuxBlockInfo(sourcePath);
        const QString sizeText = linuxInfo.value("SIZE").toString();
        if (!sizeText.isEmpty()) {
            result.estimatedSizeBytes = sizeText.toULongLong();
        }
    } else {
        result.estimatedSizeBytes = static_cast<std::uint64_t>(info.size());
    }

    if (!reportProgress(progress, 28, "Abriendo origen en solo lectura", "Intentando acceso seguro sin escribir nada en el disco.")) {
        result.summary = "Diagnostico cancelado por el usuario.";
        result.details = sourcePath;
        return result;
    }
    QFile file(sourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        result.summary = "No se pudo abrir el origen en solo lectura.";
        result.details = "Error de apertura: " + file.errorString();
        result.warnings << "Permiso denegado, dispositivo ocupado o error de E/S.";
        result.repairPlan = genericRepairPlan(result.detectedFileSystem, true, false);
        return result;
    }

    QStringList sampleNotes;
    QList<std::uint64_t> offsets;
    offsets << 0;
    if (result.estimatedSizeBytes > 1024ull * 1024ull) {
        offsets << result.estimatedSizeBytes / 100;
        offsets << result.estimatedSizeBytes / 10;
        offsets << result.estimatedSizeBytes / 4;
        offsets << result.estimatedSizeBytes / 2;
        offsets << (result.estimatedSizeBytes * 3) / 4;
        offsets << (result.estimatedSizeBytes * 9) / 10;
        if (result.estimatedSizeBytes > 65536) {
            offsets << result.estimatedSizeBytes - 65536;
        }
    }

    QList<std::uint64_t> uniqueOffsets;
    for (const std::uint64_t offset : offsets) {
        if (!uniqueOffsets.contains(offset)) {
            uniqueOffsets.append(offset);
        }
    }
    offsets = uniqueOffsets;
    const int totalSamples = offsets.size();
    for (int index = 0; index < offsets.size(); ++index) {
        const std::uint64_t offset = offsets.at(index);
        const int percent = 35 + ((index + 1) * 45 / std::max(1, totalSamples));
        if (!reportProgress(
                progress,
                percent,
                "Probando lecturas en puntos estrategicos",
                "Leyendo muestra " + QString::number(index + 1) + "/" + QString::number(totalSamples) +
                    " en offset 0x" + QString::number(static_cast<qulonglong>(offset), 16))) {
            result.summary = "Diagnostico cancelado por el usuario.";
            result.details = sourcePath;
            return result;
        }
        if (sampleRead(file, offset, 4096, sampleNotes)) {
            ++result.readSamplesOk;
        } else {
            ++result.readSamplesFailed;
        }
    }

    if (probe.detectedFileSystem == FileSystemKind::NTFS && result.estimatedSizeBytes > 0) {
        const QByteArray boot = [&file]() {
            file.seek(0);
            return file.read(512);
        }();
        if (boot.size() >= 56) {
            const std::uint64_t bytesPerSector =
                static_cast<std::uint64_t>(static_cast<unsigned char>(boot[11])) |
                (static_cast<std::uint64_t>(static_cast<unsigned char>(boot[12])) << 8);
            const std::uint64_t sectorsPerCluster = static_cast<unsigned char>(boot[13]);
            const std::uint64_t clusterSize = bytesPerSector * sectorsPerCluster;
            const std::uint64_t mftLcn = readLe64(boot, 48);
            const std::uint64_t mftOffset = mftLcn * clusterSize;
            if (!reportProgress(progress, 84, "Comprobando anclas NTFS", "Verificando boot sector y primer registro de la MFT.")) {
                result.summary = "Diagnostico cancelado por el usuario.";
                result.details = sourcePath;
                return result;
            }
            if (mftOffset > 0 && mftOffset < result.estimatedSizeBytes) {
                if (!sampleRead(file, mftOffset, 1024, sampleNotes)) {
                    result.warnings << "El arranque NTFS responde, pero la zona inicial de la MFT da errores o lectura corta.";
                }
            } else {
                result.warnings << "La posicion declarada de la MFT parece incoherente respecto al tamaño estimado del volumen.";
            }
        }
    }

    const bool hasReadErrors = result.readSamplesFailed > 0;
    const bool seemsMounted = !linuxInfo.value("MOUNTPOINT").toString().trimmed().isEmpty();

    if (!probe.success) {
        result.warnings << "No se detecto un sistema de archivos claro en cabecera.";
    }
    if (hasReadErrors) {
        result.warnings << "Hay indicios de errores de lectura fisica o lecturas cortas.";
    }
    if (seemsMounted) {
        result.warnings << "El origen parece montado. Evita reparaciones agresivas mientras este montado.";
    }
    if (looksLikeDevicePath(sourcePath) && linuxInfo.isEmpty()) {
        result.warnings << "No se pudo obtener informacion adicional del dispositivo con lsblk.";
    }

    result.repairPlan = genericRepairPlan(result.detectedFileSystem, hasReadErrors, seemsMounted);

    if (!reportProgress(progress, 94, "Generando recomendaciones", "Preparando alertas, plan sugerido y siguientes pasos.")) {
        result.summary = "Diagnostico cancelado por el usuario.";
        result.details = sourcePath;
        return result;
    }

    result.success = probe.success || result.readSamplesOk > 0;
    result.summary =
        "Diagnostico completado para " + sourcePath + "\n\n"
        "Sistema detectado: " + fsDisplay(result.detectedFileSystem, result.detectedLabel.isEmpty() ? "Desconocido" : result.detectedLabel) + "\n"
        "Lecturas de muestra correctas: " + QString::number(result.readSamplesOk) + "\n"
        "Lecturas de muestra fallidas: " + QString::number(result.readSamplesFailed);

    QString details;
    details += "Origen: " + sourcePath + "\n";
    details += "Ruta tipo dispositivo: " + QString(looksLikeDevicePath(sourcePath) ? "si" : "no") + "\n";
    if (result.estimatedSizeBytes > 0) {
        details += "Tamano estimado: " + QString::number(static_cast<qulonglong>(result.estimatedSizeBytes)) + " bytes\n";
    }
    if (!linuxInfo.isEmpty()) {
        details += "Tipo de bloque: " + linuxInfo.value("TYPE").toString() + "\n";
        details += "FS lsblk: " + linuxInfo.value("FSTYPE").toString() + "\n";
        details += "Etiqueta: " + linuxInfo.value("LABEL").toString() + "\n";
        details += "UUID: " + linuxInfo.value("UUID").toString() + "\n";
        details += "Montaje: " + linuxInfo.value("MOUNTPOINT").toString() + "\n";
        details += "Solo lectura: " + linuxInfo.value("RO").toString() + "\n";
        details += "Modelo: " + linuxInfo.value("MODEL").toString() + "\n";
    }
    details += "\nProbe de cabecera:\n" + probe.summary + "\n\n" + probe.details;
    if (!sampleNotes.isEmpty()) {
        details += "\n\nIncidencias de lectura:\n- " + sampleNotes.join("\n- ");
    }
    if (!result.warnings.isEmpty()) {
        details += "\n\nAlertas:\n- " + result.warnings.join("\n- ");
    }
    details += "\n\nPlan sugerido:\n- " + result.repairPlan.join("\n- ");

    result.details = details.trimmed();
    reportProgress(progress, 100, "Diagnostico completado", "Analisis finalizado. Ya puedes elegir el siguiente paso.");
    return result;
}
