#include "ScanProfile.h"

QString toDisplayString(ScanMode mode) {
    switch (mode) {
    case ScanMode::Normal:
        return "Normal";
    case ScanMode::Turbo:
        return "Turbo";
    }
    return "Normal";
}

QString toDisplayString(FileSystemKind fileSystem) {
    switch (fileSystem) {
    case FileSystemKind::AutoDetect:
        return "Auto";
    case FileSystemKind::NTFS:
        return "NTFS";
    case FileSystemKind::ExFAT:
        return "exFAT";
    case FileSystemKind::FAT32:
        return "FAT32";
    case FileSystemKind::EXT4:
        return "ext4";
    case FileSystemKind::APFS:
        return "APFS";
    case FileSystemKind::HFSPlus:
        return "HFS+";
    case FileSystemKind::RawGeneric:
        return "Raw genérico";
    }
    return "Auto";
}

QString toDisplayString(ScanStrategy strategy) {
    switch (strategy) {
    case ScanStrategy::Metadata:
        return "Metadatos";
    case ScanStrategy::Signatures:
        return "Firmas";
    }
    return "Metadatos";
}
