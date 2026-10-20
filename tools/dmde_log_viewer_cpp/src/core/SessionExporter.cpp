#include "SessionExporter.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

namespace {
QJsonArray buildLineNumbersPlaceholder(int lineCount) {
    QJsonArray array;
    for (int i = 0; i < lineCount; ++i) {
        array.append(i + 1);
    }
    return array;
}

QJsonValue internalDataValue(const QString &jsonText) {
    if (jsonText.trimmed().isEmpty()) {
        return QJsonValue();
    }
    const QJsonDocument document = QJsonDocument::fromJson(jsonText.toUtf8());
    if (document.isObject()) {
        return document.object();
    }
    return jsonText;
}

QJsonObject nodeToJson(const RecoveryNode &node) {
    QJsonObject object;
    object.insert("name", node.name);
    object.insert("path", node.path);
    object.insert("is_file", node.kind == RecoveryNodeKind::File);
    object.insert("size_bytes", QString::number(static_cast<qulonglong>(node.sizeBytes)));
    object.insert("metadata_text", node.metadataText);
    object.insert("type_label", node.typeLabel);
    object.insert("status_label", node.statusLabel);
    const QJsonValue internal = internalDataValue(node.internalDataJson);
    if (!internal.isUndefined() && !internal.isNull()) {
        if (internal.isObject()) {
            object.insert("internal_data", internal.toObject());
        } else {
            object.insert("internal_data_json", internal.toString());
        }
    }
    object.insert("line_numbers", buildLineNumbersPlaceholder(node.lineCount));

    QJsonArray children;
    for (const RecoveryNode &child : node.children) {
        children.append(nodeToJson(child));
    }
    object.insert("children", children);
    return object;
}
} // namespace

bool SessionExporter::saveToFile(
    const QString &filePath,
    const QString &sourceRoot,
    const QVector<RecoveryNode> &roots,
    const QStringList &checkedPaths,
    QString *errorMessage
) {
    QJsonObject root;
    root.insert("format", "recovery_studio_qt_session_v1");
    root.insert("source_root", sourceRoot);

    QJsonArray checked;
    for (const QString &path : checkedPaths) {
        checked.append(path);
    }
    root.insert("checked_paths", checked);

    if (!roots.isEmpty()) {
        if (roots.size() == 1) {
            root.insert("tree", nodeToJson(roots.first()));
        } else {
            QJsonArray multipleRoots;
            for (const RecoveryNode &node : roots) {
                multipleRoots.append(nodeToJson(node));
            }
            root.insert("trees", multipleRoots);
        }
    }

    QJsonObject scanSummary;
    scanSummary.insert("scan_dev", sourceRoot);
    scanSummary.insert("root_count", roots.size());
    scanSummary.insert("checked_count", checkedPaths.size());
    root.insert("scan_summary", scanSummary);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = "No se pudo guardar la sesion en " + filePath;
        }
        return false;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    stream << QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}
