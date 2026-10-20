#include "SessionLoader.h"

#include <QByteArray>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QTextStream>
#include <algorithm>
#include <stdexcept>

namespace {
QString formatJsonIndented(const QJsonObject &object) {
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Indented));
}

QJsonObject loadJsonObject(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        throw std::runtime_error(QString("No se pudo abrir %1").arg(filePath).toStdString());
    }

    const QByteArray data = file.readAll();
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        throw std::runtime_error(
            QString("JSON invalido en %1: %2").arg(filePath, error.errorString()).toStdString()
        );
    }
    return document.object();
}

QString pathLeaf(const QString &path) {
    const QString normalized = path.trimmed();
    if (normalized.isEmpty()) {
        return QString();
    }
    const int slash = normalized.lastIndexOf('/');
    return slash >= 0 ? normalized.mid(slash + 1) : normalized;
}

QString extensionFromName(const QString &name) {
    const int dot = name.lastIndexOf('.');
    if (dot < 0 || dot == name.size() - 1) {
        return QString();
    }
    return name.mid(dot + 1).toUpper();
}

QString objectToCompactJson(const QJsonObject &object) {
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

QString objectToIndentedJson(const QJsonObject &object) {
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Indented));
}

QStringList splitPathParts(const QString &path) {
    return path.split('/', Qt::SkipEmptyParts);
}

QString buildRecordKey(const QJsonObject &record) {
    const qlonglong offset = record.value("start_offset").toVariant().toLongLong();
    const qlonglong size = record.value("estimated_size_bytes").toVariant().toLongLong();
    return QString::number(offset) + ":" + QString::number(size);
}

std::uint64_t preferredRecordSize(const QJsonObject &record, const QJsonObject &annotation) {
    const std::uint64_t extractedSize =
        annotation.value("content_size").toVariant().toULongLong();
    if (extractedSize > 0) {
        return extractedSize;
    }

    const std::uint64_t estimatedSize =
        record.value("estimated_size_bytes").toVariant().toULongLong();
    const std::uint64_t signatureOffset =
        annotation.value("signature_offset").toVariant().toULongLong();
    if (signatureOffset > 0 && estimatedSize > signatureOffset) {
        return estimatedSize - signatureOffset;
    }
    return estimatedSize;
}

QString bestTypeText(const QString &signatureLabel, const QJsonObject &annotation) {
    if (annotation.contains("file_type")) {
        const QString fileType = annotation.value("file_type").toString().trimmed();
        if (!fileType.isEmpty()) {
            return fileType;
        }
    }

    static const QHash<QString, QString> knownTypes = {
        {"jpeg_probable", "JPEG probable"},
        {"png_probable", "PNG probable"},
        {"gif_probable", "GIF probable"},
        {"bmp_probable", "BMP probable"},
        {"tiff_probable", "TIFF probable"},
        {"webp_probable", "WEBP probable"},
        {"heic_probable", "HEIC probable"},
        {"mp4_probable", "MP4 probable"},
        {"mov_probable", "MOV probable"},
        {"avi_probable", "AVI probable"},
        {"mpeg_probable", "MPEG probable"},
        {"mkv_probable", "MKV probable"},
        {"3gp_probable", "3GP probable"},
        {"mts_probable", "MTS probable"},
        {"mp3_probable", "MP3 probable"},
        {"pdf_probable", "PDF probable"},
        {"xml_probable", "XML probable"},
        {"openpgp_probable", "OpenPGP probable"},
        {"zip_probable", "ZIP probable"},
    };

    const QString mapped = knownTypes.value(signatureLabel);
    if (!mapped.isEmpty()) {
        return mapped;
    }
    if (!signatureLabel.trimmed().isEmpty()) {
        return signatureLabel;
    }
    return "Fragmento";
}

QString categoryForRecord(const QString &signatureLabel, const QString &description) {
    const QString lowerDescription = description.toLower();
    if (QStringList{"mp4_probable", "mov_probable", "avi_probable", "mpeg_probable",
                    "mkv_probable", "3gp_probable", "mts_probable", "mp3_probable"}
            .contains(signatureLabel)) {
        return "Videos";
    }
    if (QStringList{"jpeg_probable", "png_probable", "gif_probable", "bmp_probable",
                    "tiff_probable", "webp_probable", "heic_probable"}
            .contains(signatureLabel)) {
        return "Imagenes";
    }
    if (QStringList{"pdf_probable", "xml_probable", "openpgp_probable"}.contains(signatureLabel)) {
        return "Documentos";
    }
    if (QStringList{"zip_probable"}.contains(signatureLabel)) {
        return "Comprimidos";
    }
    if (lowerDescription.contains("mpeg") || lowerDescription.contains("video") ||
        lowerDescription.contains("quicktime") || lowerDescription.contains("matroska") ||
        lowerDescription.contains("3gpp") || lowerDescription.contains("transport stream") ||
        lowerDescription.contains("audio")) {
        return "Videos";
    }
    if (lowerDescription.contains("jpeg") || lowerDescription.contains("png") ||
        lowerDescription.contains("gif") || lowerDescription.contains("bitmap") ||
        lowerDescription.contains("tiff") || lowerDescription.contains("webp") ||
        lowerDescription.contains("heic") || lowerDescription.contains("heif")) {
        return "Imagenes";
    }
    if (lowerDescription.contains("pdf") || lowerDescription.contains("document") ||
        lowerDescription.contains("xml") || lowerDescription.contains("openpgp") ||
        lowerDescription.contains("key")) {
        return "Documentos";
    }
    if (lowerDescription.contains("zip") || lowerDescription.contains("archive")) {
        return "Comprimidos";
    }
    return "Desconocidos";
}

QString suggestedExtension(const QJsonObject &annotation) {
    const QString extension = annotation.value("suggested_extension").toString();
    if (!extension.trimmed().isEmpty()) {
        return extension;
    }
    return ".bin";
}

QString formatRecoveryItemName(int index, const QJsonObject &record, const QJsonObject &annotation) {
    const QString signatureLabel =
        annotation.value("signature_label").toString(record.value("detected_signature").toString("unknown_signature"));
    const qlonglong offset = record.value("start_offset").toVariant().toLongLong();
    const std::uint64_t sizeBytes = preferredRecordSize(record, annotation);
    const QString extension = suggestedExtension(annotation);
    const QString typeName = bestTypeText(signatureLabel, annotation);
    const QString baseName =
        QString("%1 %2 [0x%3]").arg(typeName).arg(index, 3, 10, QChar('0')).arg(offset, 0, 16);
    if (!extension.isEmpty() && extension != ".bin") {
        return QString("%1%2 (%3 bytes)").arg(baseName, extension).arg(sizeBytes);
    }
    return QString("%1 (%2 bytes)").arg(baseName).arg(sizeBytes);
}

RecoveryNode buildFolderNode(const QString &name, const QString &path, const QString &statusLabel = "Activo") {
    RecoveryNode node;
    node.name = name;
    node.path = path;
    node.kind = RecoveryNodeKind::Folder;
    node.typeLabel = "Carpeta";
    node.statusLabel = statusLabel;
    node.deleted = statusLabel.contains("Borrado", Qt::CaseInsensitive);
    return node;
}

RecoveryNode buildFileNodeFromEntry(
    const QString &displayName,
    const QString &path,
    const QJsonObject &entry,
    bool deleted
) {
    RecoveryNode node;
    node.name = displayName;
    node.path = path;
    node.kind = entry.value("is_dir").toBool(false) ? RecoveryNodeKind::Folder : RecoveryNodeKind::File;
    node.sizeBytes = entry.value("size").toVariant().toULongLong();
    node.lineCount = 1;
    node.deleted = deleted;
    node.statusLabel = node.kind == RecoveryNodeKind::Folder ? (deleted ? "Carpeta borrada" : "Carpeta")
                                                             : (deleted ? "Borrado" : "Activo");
    node.typeLabel = node.kind == RecoveryNodeKind::Folder ? "Carpeta" : extensionFromName(displayName);
    if (node.kind == RecoveryNodeKind::File && node.typeLabel.isEmpty()) {
        node.typeLabel = "Archivo";
    }
    node.metadataText = objectToIndentedJson(entry);

    QJsonObject internal;
    internal.insert("fs", "ntfs");
    internal.insert("record_number", QString::number(entry.value("record_index").toVariant().toULongLong()));
    internal.insert("is_dir", entry.value("is_dir").toBool(false));
    internal.insert("real_size", QString::number(entry.value("size").toVariant().toULongLong()));
    internal.insert("cluster_size", QString::number(entry.value("cluster_size").toVariant().toULongLong()));
    internal.insert("device_path", entry.value("device_path").toString());

    const QJsonValue residentValue = entry.value("resident_data");
    if (residentValue.isObject() && residentValue.toObject().value("__type__").toString() == "bytes_b64") {
        const QString dataB64 = residentValue.toObject().value("data").toString();
        internal.insert("has_data", !dataB64.isEmpty());
        internal.insert("resident", true);
        internal.insert("resident_data_b64", dataB64);
    } else {
        const QJsonArray runlist = entry.value("data_runlist").toArray();
        QJsonArray runsArray;
        for (const QJsonValue &runValue : runlist) {
            const QJsonArray run = runValue.toArray();
            if (run.size() < 2) {
                continue;
            }
            const qlonglong lcn = run.at(0).toVariant().toLongLong();
            const qulonglong clusters = run.at(1).toVariant().toULongLong();
            QJsonObject runObject;
            runObject.insert("lcn", QString::number(static_cast<qulonglong>(lcn < 0 ? 0 : lcn)));
            runObject.insert("clusters", QString::number(clusters));
            runObject.insert("sparse", lcn <= 0);
            runsArray.append(runObject);
        }
        internal.insert("has_data", !runsArray.isEmpty());
        internal.insert("resident", false);
        internal.insert("runs", runsArray);
    }

    node.internalDataJson = objectToCompactJson(internal);
    return node;
}

RecoveryNode *findChildNode(RecoveryNode &parent, const QString &childName) {
    for (RecoveryNode &child : parent.children) {
        if (child.name == childName) {
            return &child;
        }
    }
    return nullptr;
}

RecoveryNode *ensureChildFolder(RecoveryNode &parent, const QString &childName, const QString &childPath, const QString &statusLabel) {
    if (RecoveryNode *existing = findChildNode(parent, childName)) {
        return existing;
    }
    parent.children.append(buildFolderNode(childName, childPath, statusLabel));
    return &parent.children.last();
}

QVector<RecoveryNode> buildRootsFromNtfsRecoveryEntries(const QJsonObject &entriesObject) {
    if (entriesObject.isEmpty()) {
        return {};
    }

    RecoveryNode root = buildFolderNode("NTFS reconstruido", "/NTFS reconstruido");
    root.children.append(buildFolderNode("Archivos y carpetas", "/NTFS reconstruido/Archivos y carpetas"));
    root.children.append(buildFolderNode("Borrados detectados", "/NTFS reconstruido/Borrados detectados", "Borrado"));

    RecoveryNode *activeRoot = &root.children[0];
    RecoveryNode *deletedRoot = &root.children[1];

    QStringList paths = entriesObject.keys();
    std::sort(paths.begin(), paths.end(), [](const QString &left, const QString &right) {
        const int leftDepth = splitPathParts(left).size();
        const int rightDepth = splitPathParts(right).size();
        if (leftDepth != rightDepth) {
            return leftDepth < rightDepth;
        }
        return left < right;
    });

    for (const QString &path : paths) {
        const QJsonObject entry = entriesObject.value(path).toObject();
        if (entry.isEmpty()) {
            continue;
        }

        const bool deleted = !entry.value("active").toBool(true) || path.contains("/Borrados detectados/");
        RecoveryNode *current = deleted ? deletedRoot : activeRoot;
        const QString containerPrefix = deleted ? deletedRoot->path : activeRoot->path;
        QString relativePath = path;
        if (relativePath.startsWith(containerPrefix)) {
            relativePath = relativePath.mid(containerPrefix.size());
        } else if (relativePath.startsWith(root.path)) {
            relativePath = relativePath.mid(root.path.size());
        }
        const QStringList parts = splitPathParts(relativePath);
        if (parts.isEmpty()) {
            continue;
        }

        QString currentPath = current->path;
        for (int index = 0; index < parts.size(); ++index) {
            const QString part = parts.at(index);
            currentPath += "/" + part;
            const bool isLeaf = index == parts.size() - 1;
            if (isLeaf) {
                if (RecoveryNode *existing = findChildNode(*current, part)) {
                    existing->sizeBytes = entry.value("size").toVariant().toULongLong();
                    existing->metadataText = objectToIndentedJson(entry);
                    existing->internalDataJson = objectToCompactJson(entry);
                    existing->deleted = deleted;
                    existing->statusLabel = existing->kind == RecoveryNodeKind::Folder
                                                ? (deleted ? "Carpeta borrada" : "Carpeta")
                                                : (deleted ? "Borrado" : "Activo");
                } else {
                    current->children.append(buildFileNodeFromEntry(part, currentPath, entry, deleted));
                }
            } else {
                current = ensureChildFolder(*current, part, currentPath, deleted ? "Borrado" : "Activo");
            }
        }
    }

    QVector<RecoveryNode> roots;
    roots.append(root);
    return roots;
}

QVector<RecoveryNode> buildRootsFromDirectScanData(
    const QJsonArray &directRecords,
    const QJsonObject &recordAnnotationsObject,
    const QJsonObject &scanSummaryObject
) {
    RecoveryNode libraryRoot = buildFolderNode("Biblioteca de recuperacion", "/Biblioteca de recuperacion");
    QHash<QString, int> categoryIndexes;
    const QStringList orderedCategories = {
        "Videos",
        "Imagenes",
        "Documentos",
        "Comprimidos",
        "Desconocidos",
    };
    for (const QString &categoryName : orderedCategories) {
        const int index = libraryRoot.children.size();
        libraryRoot.children.append(buildFolderNode(categoryName, libraryRoot.path + "/" + categoryName));
        categoryIndexes.insert(categoryName, index);
    }

    auto appendRecord = [&](const QJsonObject &record, const QString &groupName, int index) {
        const QString recordKey = buildRecordKey(record);
        const QJsonObject annotation = recordAnnotationsObject.value(recordKey).toObject();
        const QString signatureLabel =
            annotation.value("signature_label").toString(record.value("detected_signature").toString());
        const QString description = annotation.value("file_description").toString();
        const QString categoryName = categoryForRecord(signatureLabel, description);
        RecoveryNode &categoryNode = libraryRoot.children[categoryIndexes.value(categoryName)];
        const QString groupPath = categoryNode.path + "/" + groupName;

        RecoveryNode *groupNode = findChildNode(categoryNode, groupName);
        if (!groupNode) {
            categoryNode.children.append(buildFolderNode(groupName, groupPath));
            groupNode = &categoryNode.children.last();
        }

        const QString displayName = formatRecoveryItemName(index, record, annotation);
        RecoveryNode fileNode;
        fileNode.name = displayName;
        fileNode.path = groupNode->path + "/" + pathLeaf(displayName);
        fileNode.kind = RecoveryNodeKind::File;
        fileNode.typeLabel = bestTypeText(signatureLabel, annotation);
        fileNode.statusLabel = signatureLabel.isEmpty() ? "Sin identificar" : "Probable";
        fileNode.sizeBytes = preferredRecordSize(record, annotation);
        fileNode.lineCount = 1;
        fileNode.metadataText = objectToIndentedJson(record);
        fileNode.internalDataJson = objectToCompactJson(record);
        groupNode->children.append(fileNode);
    };

    int directIndex = 1;
    for (const QJsonValue &value : directRecords) {
        if (!value.isObject()) {
            continue;
        }
        appendRecord(value.toObject(), "Escaneo directo del disco", directIndex++);
    }

    const QJsonArray blockGroups = scanSummaryObject.value("raw_block_groups").toArray();
    for (const QJsonValue &groupValue : blockGroups) {
        const QJsonObject groupObject = groupValue.toObject();
        const QString groupName = groupObject.value("group").toString("sin_grupo");
        const QJsonArray decodedRecords = groupObject.value("decoded_records").toArray();
        int recordIndex = 1;
        for (const QJsonValue &recordValue : decodedRecords) {
            if (!recordValue.isObject()) {
                continue;
            }
            appendRecord(recordValue.toObject(), groupName, recordIndex++);
        }
    }

    QVector<RecoveryNode> filteredCategories;
    for (RecoveryNode &categoryNode : libraryRoot.children) {
        if (!categoryNode.children.isEmpty()) {
            filteredCategories.append(categoryNode);
        }
    }
    libraryRoot.children = filteredCategories;

    if (libraryRoot.children.isEmpty()) {
        return {};
    }
    return {libraryRoot};
}
} // namespace

LoadedSession SessionLoader::loadFromFile(const QString &filePath) {
    const QJsonObject rootObject = loadJsonObject(filePath);
    const QJsonObject restoredObject = restoreJsonSafeValue(rootObject).toObject();

    LoadedSession session;
    session.format = restoredObject.value("format").toString();
    session.sourceRoot = restoredObject.value("source_root").toString();
    if (session.sourceRoot.trimmed().isEmpty()) {
        session.sourceRoot = restoredObject.value("scan_summary").toObject().value("scan_dev").toString();
    }
    session.rawJsonText = formatJsonIndented(restoredObject);
    session.roots = parseRoots(restoredObject);

    const QJsonArray checked = restoredObject.value("checked_paths").toArray();
    for (const QJsonValue &value : checked) {
        session.checkedPaths.append(value.toString());
    }

    session.summaryText = buildSummaryText(restoredObject, session);
    return session;
}

QJsonValue SessionLoader::restoreJsonSafeValue(const QJsonValue &value) {
    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        if (object.value("__type__").toString() == "bytes_b64") {
            return object;
        }
        QJsonObject restored;
        for (auto it = object.begin(); it != object.end(); ++it) {
            restored.insert(it.key(), restoreJsonSafeValue(it.value()));
        }
        return restored;
    }
    if (value.isArray()) {
        QJsonArray restored;
        const QJsonArray array = value.toArray();
        for (const QJsonValue &item : array) {
            restored.append(restoreJsonSafeValue(item));
        }
        return restored;
    }
    return value;
}

RecoveryNode SessionLoader::parseNode(const QJsonObject &object) {
    RecoveryNode node;
    node.name = object.value("name").toString();
    node.path = object.value("path").toString();
    node.metadataText = object.value("metadata_text").toString();
    if (object.value("internal_data").isObject()) {
        node.internalDataJson = QString::fromUtf8(QJsonDocument(object.value("internal_data").toObject()).toJson(QJsonDocument::Compact));
    } else {
        node.internalDataJson = object.value("internal_data_json").toString();
    }
    if (node.metadataText.trimmed().isEmpty()) {
        const QJsonArray rawLines = object.value("raw_lines").toArray();
        if (!rawLines.isEmpty()) {
            QStringList lines;
            lines.reserve(rawLines.size());
            for (const QJsonValue &lineValue : rawLines) {
                lines.append(lineValue.toString());
            }
            node.metadataText = lines.join('\n');
            if (node.internalDataJson.trimmed().isEmpty() && rawLines.first().isString()) {
                node.internalDataJson = rawLines.first().toString();
            }
        }
    }
    node.kind = object.value("is_file").toBool(false) ? RecoveryNodeKind::File : RecoveryNodeKind::Folder;
    node.sizeBytes = static_cast<std::uint64_t>(object.value("size_bytes").toDouble(0.0));
    node.lineCount = object.value("line_numbers").toArray().size();
    node.deleted = node.name.contains("[borrado]", Qt::CaseInsensitive) || node.path.contains("/Borrados detectados/");
    node.statusLabel = object.value("status_label").toString();
    if (node.statusLabel.trimmed().isEmpty()) {
        node.statusLabel = node.deleted ? "Borrado" : "Activo";
    }
    if (node.kind == RecoveryNodeKind::Folder) {
        node.typeLabel = object.value("type_label").toString("Carpeta");
    } else {
        node.typeLabel = object.value("type_label").toString();
        if (node.typeLabel.trimmed().isEmpty()) {
            node.typeLabel = extensionFromName(node.name);
            if (node.typeLabel.isEmpty()) {
                static const QRegularExpression signatureRegex("^([A-Za-z0-9+._-]+) probable");
                const QRegularExpressionMatch match = signatureRegex.match(node.name);
                node.typeLabel = match.hasMatch() ? match.captured(1).toUpper() : QString("Archivo");
            }
        }
    }

    const QJsonArray children = object.value("children").toArray();
    node.children.reserve(children.size());
    for (const QJsonValue &childValue : children) {
        if (!childValue.isObject()) {
            continue;
        }
        node.children.append(parseNode(childValue.toObject()));
    }
    return node;
}

QVector<RecoveryNode> SessionLoader::parseRoots(const QJsonObject &object) {
    QVector<RecoveryNode> roots;

    const bool looksLikePythonSession =
        object.value("format").toString() == "dmde-scan-session-v1" ||
        object.value("ntfs_recovery_entries").isObject() ||
        object.value("direct_scan_records").isArray();
    if (looksLikePythonSession) {
        roots = parsePythonSessionRoots(object);
        if (!roots.isEmpty()) {
            return roots;
        }
    }

    if (object.contains("name") && object.contains("path") && object.contains("children")) {
        const RecoveryNode parsedRoot = parseNode(object);
        if (!parsedRoot.name.trimmed().isEmpty() || !parsedRoot.children.isEmpty()) {
            roots.append(parsedRoot);
            return roots;
        }
    }

    const QJsonValue sessionTree = object.value("ntfs_tree_root");
    if (sessionTree.isObject()) {
        const RecoveryNode parsedRoot = parseNode(sessionTree.toObject());
        if (!parsedRoot.name.trimmed().isEmpty() || !parsedRoot.children.isEmpty()) {
            roots.append(parsedRoot);
            return roots;
        }
    }

    const QJsonValue manifestTree = object.value("tree");
    if (manifestTree.isObject()) {
        roots.append(parseNode(manifestTree.toObject()));
        return roots;
    }

    const QJsonValue multipleTrees = object.value("trees");
    if (multipleTrees.isArray()) {
        const QJsonArray array = multipleTrees.toArray();
        for (const QJsonValue &value : array) {
            if (value.isObject()) {
                roots.append(parseNode(value.toObject()));
            }
        }
        if (!roots.isEmpty()) {
            return roots;
        }
    }

    return parsePythonSessionRoots(object);
}

QVector<RecoveryNode> SessionLoader::parsePythonSessionRoots(const QJsonObject &object) {
    QVector<RecoveryNode> roots;
    const QJsonObject ntfsEntries = object.value("ntfs_recovery_entries").toObject();
    const QVector<RecoveryNode> ntfsRoots = buildRootsFromNtfsRecoveryEntries(ntfsEntries);
    for (const RecoveryNode &root : ntfsRoots) {
        roots.append(root);
    }

    const QJsonArray directRecords = object.value("direct_scan_records").toArray();
    const QJsonObject recordAnnotations = object.value("record_annotations").toObject();
    const QJsonObject scanSummary = object.value("scan_summary").toObject();
    const QVector<RecoveryNode> libraryRoots =
        buildRootsFromDirectScanData(directRecords, recordAnnotations, scanSummary);
    for (const RecoveryNode &root : libraryRoots) {
        roots.append(root);
    }

    return roots;
}

QString SessionLoader::buildSummaryText(const QJsonObject &rootObject, const LoadedSession &session) {
    QString summary;
    summary += "Formato: " + (session.format.isEmpty() ? QString("desconocido") : session.format) + "\n";
    if (!session.sourceRoot.isEmpty()) {
        summary += "Origen: " + session.sourceRoot + "\n";
    }
    summary += "Raices cargadas: " + QString::number(session.roots.size()) + "\n";
    summary += "Marcados restaurados: " + QString::number(session.checkedPaths.size()) + "\n";

    const QJsonValue scanSummaryValue = rootObject.value("scan_summary");
    if (scanSummaryValue.isObject()) {
        const QJsonObject scanSummary = scanSummaryValue.toObject();
        summary += "\nResumen de escaneo:\n";
        summary += "- scan_dev: " + scanSummary.value("scan_dev").toString() + "\n";
        summary += "- scan_range: " + scanSummary.value("scan_range").toString() + "\n";
        summary += "- firmas habilitadas: " + QString::number(scanSummary.value("signature_types_total").toInt()) + "\n";
        summary += "- grupos raw: " + QString::number(scanSummary.value("raw_block_groups").toArray().size()) + "\n";
    }

    return summary.trimmed();
}

