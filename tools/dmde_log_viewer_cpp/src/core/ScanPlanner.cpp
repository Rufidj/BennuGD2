#include "ScanPlanner.h"

namespace {
FileSystemCapabilities makeNtfsCapabilities() {
    FileSystemCapabilities caps;
    caps.summary = "NTFS permite reconstruccion avanzada a partir de MFT, directorios, atributos y runlists.";
    caps.strengths = "Muy bueno para mantener carpetas, nombres originales y distinguir activos/borrados.";
    caps.limitations = "Si la MFT esta muy dañada, habra que caer a firmas o a reconstruccion parcial.";
    caps.recommendedOrder = "1. Metadatos NTFS  2. Borrados NTFS  3. Firmas como rescate final";
    caps.supportsMetadataScan = true;
    caps.supportsSignatureScan = true;
    caps.keepsFolderStructureWell = true;
    caps.deletedFileRecovery = true;
    caps.runlistOrExtentRecovery = true;
    return caps;
}

FileSystemCapabilities makeExFatCapabilities() {
    FileSystemCapabilities caps;
    caps.summary = "exFAT se apoya en directorios, bitmap de clusters y entradas extendidas.";
    caps.strengths = "Muy adecuado para discos USB, SD y camaras con fotos y videos recientes.";
    caps.limitations = "La recuperacion de borrados y fragmentacion compleja puede ser menos fiable que en NTFS.";
    caps.recommendedOrder = "1. Metadatos exFAT  2. Firmas de imagen/video  3. Raw si el volumen esta roto";
    caps.supportsMetadataScan = true;
    caps.supportsSignatureScan = true;
    caps.keepsFolderStructureWell = true;
    caps.deletedFileRecovery = true;
    caps.runlistOrExtentRecovery = false;
    return caps;
}

FileSystemCapabilities makeFat32Capabilities() {
    FileSystemCapabilities caps;
    caps.summary = "FAT32 se reconstruye siguiendo FAT, directorios y cadenas de clusters.";
    caps.strengths = "Ideal para tarjetas antiguas, USB sencillos y muchos dispositivos multimedia.";
    caps.limitations = "Los nombres largos dañados y la fragmentacion fuerte empeoran bastante el resultado.";
    caps.recommendedOrder = "1. Metadatos FAT32  2. Firmas  3. Raw generico";
    caps.supportsMetadataScan = true;
    caps.supportsSignatureScan = true;
    caps.keepsFolderStructureWell = true;
    caps.deletedFileRecovery = true;
    caps.runlistOrExtentRecovery = false;
    return caps;
}

FileSystemCapabilities makeExt4Capabilities() {
    FileSystemCapabilities caps;
    caps.summary = "ext4 permite analisis por superbloques, grupos, inodos y extent trees.";
    caps.strengths = "Muy bueno para estructura jerarquica y para archivos aun referenciados.";
    caps.limitations = "La recuperacion de borrados puede complicarse mucho si ya hubo actividad posterior.";
    caps.recommendedOrder = "1. Metadatos ext4  2. Extents recuperables  3. Firmas si falla la estructura";
    caps.supportsMetadataScan = true;
    caps.supportsSignatureScan = true;
    caps.keepsFolderStructureWell = true;
    caps.deletedFileRecovery = true;
    caps.runlistOrExtentRecovery = true;
    return caps;
}

FileSystemCapabilities makeApfsCapabilities() {
    FileSystemCapabilities caps;
    caps.summary = "APFS requiere lectura de containers, volumes y arboles copy-on-write.";
    caps.strengths = "Buena base para recuperar estructura reciente cuando las estructuras principales son legibles.";
    caps.limitations = "Es un formato complejo; el soporte inicial suele ser mas parcial que NTFS o exFAT.";
    caps.recommendedOrder = "1. Metadatos APFS  2. Archivos referenciados  3. Firmas";
    caps.supportsMetadataScan = true;
    caps.supportsSignatureScan = true;
    caps.keepsFolderStructureWell = true;
    caps.deletedFileRecovery = false;
    caps.runlistOrExtentRecovery = true;
    return caps;
}

FileSystemCapabilities makeHfsCapabilities() {
    FileSystemCapabilities caps;
    caps.summary = "HFS+ se basa en catalog file, extents overflow y, si ayuda, en journal.";
    caps.strengths = "Puede mantener bastante bien la estructura en discos Apple clasicos.";
    caps.limitations = "El borrado y la consistencia de metadatos dependen mucho del estado real del catalogo.";
    caps.recommendedOrder = "1. Metadatos HFS+  2. Extents  3. Firmas";
    caps.supportsMetadataScan = true;
    caps.supportsSignatureScan = true;
    caps.keepsFolderStructureWell = true;
    caps.deletedFileRecovery = false;
    caps.runlistOrExtentRecovery = true;
    return caps;
}

FileSystemCapabilities makeRawCapabilities() {
    FileSystemCapabilities caps;
    caps.summary = "Raw generico ignora el sistema de archivos y busca cabeceras reconocibles.";
    caps.strengths = "Muy util cuando la particion esta rota, no monta o el volumen es desconocido.";
    caps.limitations = "Suele perder nombres originales, fechas y carpetas, y puede generar fragmentos o falsos positivos.";
    caps.recommendedOrder = "1. Firmas  2. Clasificacion posterior  3. Revision manual";
    caps.supportsMetadataScan = false;
    caps.supportsSignatureScan = true;
    caps.keepsFolderStructureWell = false;
    caps.deletedFileRecovery = false;
    caps.runlistOrExtentRecovery = false;
    return caps;
}
} // namespace

FileSystemCapabilities ScanPlanner::capabilitiesFor(FileSystemKind fileSystem) {
    switch (fileSystem) {
    case FileSystemKind::AutoDetect: {
        FileSystemCapabilities caps;
        caps.summary = "Auto detectara el volumen y elegira el mejor analizador disponible.";
        caps.strengths = "Ideal para no obligar al usuario a saber si el disco es NTFS, exFAT, FAT32 o ext4.";
        caps.limitations = "Hasta que no lea estructuras base no puede prometer el camino exacto.";
        caps.recommendedOrder = "1. Detectar volumen  2. Metadatos del FS encontrado  3. Firmas si hace falta";
        caps.supportsMetadataScan = true;
        caps.supportsSignatureScan = true;
        caps.keepsFolderStructureWell = true;
        caps.deletedFileRecovery = true;
        caps.runlistOrExtentRecovery = true;
        return caps;
    }
    case FileSystemKind::NTFS:
        return makeNtfsCapabilities();
    case FileSystemKind::ExFAT:
        return makeExFatCapabilities();
    case FileSystemKind::FAT32:
        return makeFat32Capabilities();
    case FileSystemKind::EXT4:
        return makeExt4Capabilities();
    case FileSystemKind::APFS:
        return makeApfsCapabilities();
    case FileSystemKind::HFSPlus:
        return makeHfsCapabilities();
    case FileSystemKind::RawGeneric:
        return makeRawCapabilities();
    }
    return makeRawCapabilities();
}

QString ScanPlanner::modeDescription(ScanMode mode) {
    switch (mode) {
    case ScanMode::Normal:
        return "Normal prioriza equilibrio y estabilidad: mas validaciones, mejor clasificacion y menos riesgo de falsos positivos.";
    case ScanMode::Turbo:
        return "Turbo prioriza velocidad: menos comprobaciones, mas lectura secuencial y reconstruccion agresiva para discos grandes.";
    }
    return {};
}

QString ScanPlanner::profileSummary(const ScanProfile &profile) {
    const FileSystemCapabilities caps = capabilitiesFor(profile.fileSystem);
    QString summary;
    summary += "Perfil activo:\n";
    summary += "- Modo: " + toDisplayString(profile.mode) + "\n";
    summary += "- Sistema de archivos: " + toDisplayString(profile.fileSystem) + "\n";
    summary += "- Estrategia: " + toDisplayString(profile.strategy) + "\n\n";
    summary += modeDescription(profile.mode) + "\n";
    summary += caps.summary + "\n";
    summary += "Fortalezas: " + caps.strengths + "\n";
    summary += "Limites: " + caps.limitations + "\n";
    summary += "Orden recomendado: " + caps.recommendedOrder + "\n";
    return summary.trimmed();
}

QString ScanPlanner::strategySummary(const ScanProfile &profile) {
    const FileSystemCapabilities caps = capabilitiesFor(profile.fileSystem);
    if (profile.strategy == ScanStrategy::Metadata) {
        if (!caps.supportsMetadataScan) {
            return "El sistema de archivos elegido no tiene aun un escaneo por metadatos fiable; conviene usar firmas.";
        }
        return "Escaneo por metadatos: prioriza estructura de carpetas, nombres y consistencia del sistema de archivos.";
    }
    return "Escaneo por firmas: prioriza encontrar contenido recuperable incluso cuando la estructura logica esta dañada.";
}
