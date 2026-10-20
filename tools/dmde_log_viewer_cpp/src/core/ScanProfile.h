#pragma once

#include <QString>

enum class ScanMode {
    Normal,
    Turbo,
};

enum class FileSystemKind {
    AutoDetect,
    NTFS,
    ExFAT,
    FAT32,
    EXT4,
    APFS,
    HFSPlus,
    RawGeneric,
};

enum class ScanStrategy {
    Metadata,
    Signatures,
};

struct ScanProfile {
    ScanMode mode = ScanMode::Normal;
    FileSystemKind fileSystem = FileSystemKind::AutoDetect;
    ScanStrategy strategy = ScanStrategy::Metadata;
};

QString toDisplayString(ScanMode mode);
QString toDisplayString(FileSystemKind fileSystem);
QString toDisplayString(ScanStrategy strategy);
