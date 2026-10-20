#include "ScanEngine.h"

#include "ScanProfile.h"

namespace {
QString normalizedFsName(FileSystemKind fileSystem) {
    return toDisplayString(fileSystem);
}

QVector<ScanPhase> metadataPhases(FileSystemKind fileSystem, ScanMode mode) {
    const bool turbo = mode == ScanMode::Turbo;
    switch (fileSystem) {
    case FileSystemKind::NTFS:
        return {
            {"Lectura de cabecera", "Valida sector de arranque, tamanos base y geometria del volumen.", 8, false},
            {"Exploracion MFT", turbo ? "Recorre MFT de forma secuencial con validacion reducida." : "Recorre MFT con validacion completa de registros y atributos.", turbo ? 40 : 28, false},
            {"Directorios y nombres", "Reconstruye jerarquia, nombres largos y enlaces a registros.", 18, false},
            {"Borrados y runlists", turbo ? "Intenta enlazar runlists y borrados en modo agresivo." : "Analiza borrados, runlists y consistencia de clusters.", turbo ? 24 : 34, false},
            {"Clasificacion final", "Clasifica por tipo y consolidacion de resultados recuperables.", 10, false},
        };
    case FileSystemKind::ExFAT:
        return {
            {"Cabecera exFAT", "Valida boot region y parametros principales.", 10, false},
            {"Bitmap y FAT", turbo ? "Lee bitmap y FAT minimizando comprobaciones redundantes." : "Lee bitmap y FAT para estimar asignacion y coherencia.", turbo ? 30 : 24, false},
            {"Directorios", "Reconstruye entradas primarias, stream extension y nombres largos.", 30, false},
            {"Recuperacion de borrados", "Evalua entradas borradas y clusters reutilizados.", turbo ? 20 : 26, false},
            {"Consolidacion", "Agrupa resultados por carpetas, fechas y tipos.", 10, false},
        };
    case FileSystemKind::FAT32:
        return {
            {"BPB y FAT", "Valida BPB y localiza FAT activa.", 12, false},
            {"Cadenas de clusters", turbo ? "Reconstruye cadenas con menos relecturas." : "Reconstruye cadenas y detecta cortes o ciclos.", turbo ? 32 : 26, false},
            {"Directorios", "Lee directorios cortos y nombres largos.", 26, false},
            {"Archivos borrados", "Marca primeras letras borradas y coherencia de cadenas.", turbo ? 18 : 26, false},
            {"Revision final", "Limpia duplicados y agrupa hallazgos.", 10, false},
        };
    case FileSystemKind::EXT4:
        return {
            {"Superbloques", "Valida superbloques y grupos.", 12, false},
            {"Inodos y extents", turbo ? "Lee inodos y extents con cache agresiva." : "Lee inodos y extents con validacion de arboles.", turbo ? 38 : 30, false},
            {"Directorios", "Reconstruye entradas de directorio y rutas.", 22, false},
            {"Recuperacion de borrados", "Evalua inodos huérfanos y bloques reutilizados.", turbo ? 16 : 24, true},
            {"Consolidacion", "Agrupa resultados y etiquetas.", 12, false},
        };
    case FileSystemKind::APFS:
        return {
            {"Container APFS", "Detecta container, volumes y checkpoints.", 16, false},
            {"Arboles B-Tree", turbo ? "Lee nodos clave con estrategia reducida." : "Lee object map y arboles principales.", turbo ? 34 : 28, false},
            {"Namespace", "Reconstruye nombres, rutas y snapshots utiles.", 24, false},
            {"Extents", "Relaciona extents y datos accesibles.", 18, false},
            {"Resultados", "Consolida archivos visibles y recuperables.", 8, false},
        };
    case FileSystemKind::HFSPlus:
        return {
            {"Cabecera HFS+", "Valida volumen y parametros.", 14, false},
            {"Catalog file", turbo ? "Lee catalog file en modo rapido." : "Lee catalog file y extents overflow.", turbo ? 32 : 26, false},
            {"Jerarquia", "Reconstruye carpetas, nombres y relaciones.", 24, false},
            {"Archivos utiles", "Extrae candidatos activos y parcialmente borrados.", 20, false},
            {"Consolidacion", "Normaliza resultados.", 10, false},
        };
    case FileSystemKind::RawGeneric:
        return {
            {"Preparacion raw", "Define ventanas y alineaciones de lectura.", 12, false},
            {"Firmas base", "Busca imagenes, videos y documentos frecuentes.", 56, false},
            {"Clasificacion", "Descarta ruido y prioriza candidatos fuertes.", 20, false},
            {"Agrupacion", "Agrupa hallazgos por tipo y tamano.", 12, false},
        };
    case FileSystemKind::AutoDetect:
        return {
            {"Autodeteccion", "Prueba firmas de volumen y decide el mejor motor.", 20, false},
            {"Motor elegido", "Deriva a un analizador especifico del sistema de archivos.", 60, false},
            {"Consolidacion", "Fusiona resultados del motor seleccionado.", 20, false},
        };
    }
    return {};
}

QVector<ScanPhase> signaturePhases(FileSystemKind fileSystem, ScanMode mode) {
    const bool turbo = mode == ScanMode::Turbo;
    Q_UNUSED(fileSystem);
    return {
        {"Muestreo inicial", turbo ? "Calcula offsets y bloques saltando comprobaciones lentas." : "Calcula offsets y alinea bloques para lectura segura.", turbo ? 12 : 18, false},
        {"Barrido de firmas", turbo ? "Barrido secuencial con firmas principales y cache agresiva." : "Barrido secuencial con firmas y validaciones de cabeceras/pies.", turbo ? 58 : 42, false},
        {"Verificacion", turbo ? "Verifica candidatos mas prometedores." : "Verifica candidatos y estima tamanos coherentes.", turbo ? 16 : 24, false},
        {"Clasificacion final", "Agrupa por imagen, video, documento y archivo comprimido.", 14, false},
    };
}
} // namespace

ScanEngineDescriptor ScanEngineRegistry::describe(const ScanProfile &profile) {
    ScanEngineDescriptor descriptor;
    descriptor.targetFileSystem = normalizedFsName(profile.fileSystem);
    descriptor.strategyName = toDisplayString(profile.strategy);
    descriptor.modeName = toDisplayString(profile.mode);

    if (profile.strategy == ScanStrategy::Metadata) {
        descriptor.engineId = "metadata." + descriptor.targetFileSystem.toLower();
        descriptor.displayName = "Motor de metadatos " + descriptor.targetFileSystem;
        descriptor.summary = "Reconstruye estructura logica, carpetas y nombres usando metadatos del volumen.";
        descriptor.strengths = "Maximiza la recuperacion con estructura original cuando el sistema de archivos aun conserva referencias utiles.";
        descriptor.risks = profile.mode == ScanMode::Turbo
            ? "Turbo puede aceptar mas inconsistencias y requerira revision posterior."
            : "Normal sera mas lento, pero suele reducir falsos positivos y enlaces incorrectos.";
        descriptor.available = profile.fileSystem != FileSystemKind::RawGeneric;
        descriptor.phases = metadataPhases(profile.fileSystem, profile.mode);
        descriptor.outputs = QStringList{
            "Arbol de carpetas",
            "Archivos activos",
            "Borrados detectados",
            "Metadatos recuperables",
        };
        return descriptor;
    }

    descriptor.engineId = "signature.raw";
    descriptor.displayName = "Motor de firmas";
    descriptor.summary = "Busca contenido recuperable directamente en los datos aunque la estructura logica este dañada.";
    descriptor.strengths = "Ideal para fotos y videos cuando el volumen no monta o los metadatos estan rotos.";
    descriptor.risks = profile.mode == ScanMode::Turbo
        ? "Turbo puede devolver mas fragmentos, estimaciones amplias y mas ruido."
        : "Normal mejora validacion, pero puede tardar bastante en discos grandes.";
    descriptor.available = true;
    descriptor.phases = signaturePhases(profile.fileSystem, profile.mode);
    descriptor.outputs = QStringList{
        "Imagenes detectadas",
        "Videos detectados",
        "Documentos detectados",
        "Fragmentos pendientes de revisar",
    };
    return descriptor;
}
