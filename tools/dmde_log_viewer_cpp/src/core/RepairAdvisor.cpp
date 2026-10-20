#include "RepairAdvisor.h"

#include <QSysInfo>

namespace {
QString osLabel() {
#if defined(Q_OS_WIN)
    return "Windows";
#elif defined(Q_OS_MACOS)
    return "macOS";
#elif defined(Q_OS_LINUX)
    return "Linux";
#else
    return QSysInfo::prettyProductName();
#endif
}

QString fsLabel(FileSystemKind fileSystem) {
    const QString label = toDisplayString(fileSystem);
    return label == "Auto" ? "desconocido" : label;
}

void addGenericSafety(QStringList &steps) {
    steps << "Trabaja siempre en solo lectura si el disco presenta errores de E/S.";
    steps << "Si hay datos importantes, clona o crea imagen antes de reparar.";
    steps << "No ejecutes reparaciones con el volumen montado en lectura/escritura.";
}

void addLinuxCommands(QStringList &commands, FileSystemKind fileSystem, const QString &sourcePath) {
    switch (fileSystem) {
    case FileSystemKind::NTFS:
        commands << "sudo ntfsfix " + sourcePath;
        break;
    case FileSystemKind::ExFAT:
        commands << "sudo fsck.exfat -n " + sourcePath;
        commands << "sudo fsck.exfat " + sourcePath;
        break;
    case FileSystemKind::FAT32:
        commands << "sudo fsck.vfat -n " + sourcePath;
        commands << "sudo fsck.vfat -a " + sourcePath;
        break;
    case FileSystemKind::EXT4:
        commands << "sudo e2fsck -f -n " + sourcePath;
        commands << "sudo e2fsck -f -y " + sourcePath;
        break;
    default:
        commands << "sudo ddrescue " + sourcePath + " imagen.img mapa.log";
        break;
    }
}

void addWindowsCommands(QStringList &commands, FileSystemKind fileSystem, const QString &sourcePath) {
    Q_UNUSED(fileSystem);
    commands << "chkdsk " + sourcePath + " /scan";
    commands << "chkdsk " + sourcePath + " /f";
}

void addMacCommands(QStringList &commands, FileSystemKind fileSystem, const QString &sourcePath) {
    Q_UNUSED(fileSystem);
    commands << "diskutil verifyVolume " + sourcePath;
    commands << "diskutil repairVolume " + sourcePath;
}
} // namespace

RepairAdvisorResult RepairAdvisor::build(const QString &sourcePath, FileSystemKind fileSystem) {
    RepairAdvisorResult result;
    result.operatingSystemLabel = osLabel();
    result.fileSystem = fileSystem;
    result.sourcePath = sourcePath;

    QStringList safety;
    addGenericSafety(safety);

    QStringList steps;
    steps << "1. Diagnostica primero el disco para saber si el problema es fisico o logico.";
    steps << "2. Si hay errores de lectura, prioriza clonacion o imagen.";

    QStringList commands;
#if defined(Q_OS_LINUX)
    addLinuxCommands(commands, fileSystem, sourcePath);
    result.hasDirectCommandSupport = true;
    steps << "3. Ejecuta primero una verificacion no destructiva.";
    steps << "4. Solo despues pasa a reparacion logica.";
#elif defined(Q_OS_WIN)
    addWindowsCommands(commands, fileSystem, sourcePath);
    result.hasDirectCommandSupport = true;
    steps << "3. Usa primero /scan o verificacion equivalente.";
    steps << "4. Si el sistema lo confirma, continua con /f.";
#elif defined(Q_OS_MACOS)
    addMacCommands(commands, fileSystem, sourcePath);
    result.hasDirectCommandSupport = true;
    steps << "3. Verifica el volumen desde Disk Utility o diskutil.";
    steps << "4. Repara solo despues de guardar copia si el disco falla.";
#else
    result.hasDirectCommandSupport = false;
    steps << "3. En este sistema aun no hay comandos predefinidos.";
    steps << "4. Usa el plan guiado y herramientas nativas del sistema.";
#endif

    steps << "5. Tras reparar, vuelve a explorar el volumen y valida archivos clave.";

    result.summary =
        "Asistente de reparacion para " + fsLabel(fileSystem) + "\n\n"
        "Sistema operativo actual: " + result.operatingSystemLabel + "\n"
        "Origen: " + sourcePath + "\n"
        "Comandos sugeridos disponibles: " + QString(result.hasDirectCommandSupport ? "si" : "no");
    result.safetyNotes = safety.join("\n- ");
    result.guidedSteps = steps;
    result.suggestedCommands = commands;
    return result;
}
