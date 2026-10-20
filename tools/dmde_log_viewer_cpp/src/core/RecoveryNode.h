#pragma once

#include <QString>
#include <QVector>
#include <QMetaType>
#include <cstdint>

enum class RecoveryNodeKind {
    Folder,
    File,
};

struct RecoveryNode {
    QString name;
    QString path;
    QString typeLabel;
    QString statusLabel;
    QString metadataText;
    QString internalDataJson;
    std::uint64_t sizeBytes = 0;
    int lineCount = 0;
    bool deleted = false;
    RecoveryNodeKind kind = RecoveryNodeKind::Folder;
    QVector<RecoveryNode> children;
};

Q_DECLARE_METATYPE(RecoveryNode)

