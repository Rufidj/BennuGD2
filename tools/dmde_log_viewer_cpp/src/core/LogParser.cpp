#include "LogParser.h"

#include <QFile>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>
#include <algorithm>
#include <stdexcept>

namespace {
struct LogEntry {
    QString path;
    int lineNumber = 0;
    QString rawLine;
    bool isFile = false;
};

const QRegularExpression kWindowsPathRe(
    R"(([A-Za-z]:\\[^:*?"<>|\r\n]+(?:\\[^:*?"<>|\r\n]+)*))"
);
const QRegularExpression kPosixPathRe(
    R"(((?:/|\\\$Root[\\/])[^:*?"<>|\r\n]+(?:[\\/][^:*?"<>|\r\n]+)*))"
);
const QRegularExpression kQuotedPathRe(
    R"dmde("([^"\n]+(?:\\|/)[^"\n]+)")dmde"
);
const QRegularExpression kFileExtensionRe(
    R"(\.(jpg|jpeg|png|gif|bmp|tif|tiff|heic|webp|mp4|mov|avi|mkv|mts|3gp|mpg|mpeg|pdf|zip|rar|7z|doc|docx|xls|xlsx|txt|xml)$)",
    QRegularExpression::CaseInsensitiveOption
);
const QRegularExpression kDrivePathRe(R"(^[A-Za-z]:/)");
const QRegularExpression kRepeatedSlashRe(R"(/+)");

QString normalizePath(const QString &path) {
    QString cleaned = path.trimmed();
    cleaned.remove('"');
    cleaned.remove('\'');
    cleaned.replace('\\', '/');
    cleaned.replace(kRepeatedSlashRe, "/");

    if (kDrivePathRe.match(cleaned).hasMatch()) {
        return cleaned;
    }
    if (cleaned.startsWith("/$Root/") || cleaned.startsWith("/Root/") || cleaned.startsWith('/')) {
        return cleaned;
    }
    if (cleaned.startsWith("$Root/")) {
        return "/" + cleaned;
    }
    return {};
}

bool looksLikeFile(const QString &path) {
    const int slash = path.lastIndexOf('/');
    const QString name = slash >= 0 ? path.mid(slash + 1) : path;
    return kFileExtensionRe.match(name).hasMatch();
}

QString extractDevicePath(const QString &line) {
    if (!line.startsWith("scan_dev=")) {
        return {};
    }
    const int devIndex = line.indexOf("/dev/");
    return devIndex >= 0 ? line.mid(devIndex).trimmed() : QString();
}

QStringList extractBreadcrumbs(const QString &line) {
    QString cleaned = line;
    cleaned.replace('\\', '/');
    const QStringList tokens = cleaned.split(QRegularExpression(R"(\s+)"), Qt::SkipEmptyParts);
    QStringList result;
    for (const QString &token : tokens) {
        if (!token.contains('/')) {
            continue;
        }
        QString normalized = token;
        normalized.remove('[');
        normalized.remove(']');
        normalized.remove('(');
        normalized.remove(')');
        normalized.remove('{');
        normalized.remove('}');
        normalized.remove('<');
        normalized.remove('>');
        normalized.remove(',');
        normalized.remove(';');
        result.append(normalized);
    }
    return result;
}

void parsePathCandidates(const QString &line, int lineNumber, QVector<LogEntry> &entries) {
    QStringList candidates;

    auto captureAll = [&](const QRegularExpression &expression, int captureGroup) {
        QRegularExpressionMatchIterator it = expression.globalMatch(line);
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();
            candidates.append(match.captured(captureGroup));
        }
    };

    captureAll(kWindowsPathRe, 1);
    captureAll(kPosixPathRe, 1);
    captureAll(kQuotedPathRe, 1);

    if (candidates.isEmpty() &&
        (line.contains("/$Root/") || line.contains("\\$Root\\") || line.contains("/Root/") || line.contains("\\Root\\"))) {
        candidates.append(extractBreadcrumbs(line));
    }

    for (const QString &candidate : candidates) {
        const QString normalized = normalizePath(candidate);
        if (normalized.isEmpty() || normalized == "/" || normalized == "\\") {
            continue;
        }
        entries.append(LogEntry{normalized, lineNumber, line, looksLikeFile(normalized)});
    }
}

RecoveryNode *findChild(RecoveryNode &parent, const QString &name) {
    for (RecoveryNode &child : parent.children) {
        if (child.name == name) {
            return &child;
        }
    }
    return nullptr;
}

void sortChildrenRecursive(RecoveryNode &node) {
    std::sort(node.children.begin(), node.children.end(), [](const RecoveryNode &left, const RecoveryNode &right) {
        if (left.kind != right.kind) {
            return left.kind == RecoveryNodeKind::Folder;
        }
        return left.name.toLower() < right.name.toLower();
    });

    for (RecoveryNode &child : node.children) {
        sortChildrenRecursive(child);
    }
}

void finalizeNode(RecoveryNode &node) {
    if (node.kind == RecoveryNodeKind::Folder) {
        std::uint64_t aggregatedSize = 0;
        int aggregatedLines = node.lineCount;
        for (RecoveryNode &child : node.children) {
            finalizeNode(child);
            aggregatedSize += child.sizeBytes;
            aggregatedLines += child.lineCount;
        }
        node.sizeBytes = aggregatedSize;
        node.lineCount = aggregatedLines;
        node.typeLabel = "Carpeta";
        if (node.statusLabel.isEmpty()) {
            node.statusLabel = "Detectado";
        }
    } else {
        const int dot = node.name.lastIndexOf('.');
        node.typeLabel = dot >= 0 ? node.name.mid(dot + 1).toUpper() : "Archivo";
        if (node.statusLabel.isEmpty()) {
            node.statusLabel = "Detectado";
        }
    }
}

QVector<RecoveryNode> buildTree(const QVector<LogEntry> &entries) {
    RecoveryNode root;
    root.name = "/";
    root.path = "/";
    root.kind = RecoveryNodeKind::Folder;
    root.typeLabel = "Carpeta";
    root.statusLabel = "Detectado";

    for (const LogEntry &entry : entries) {
        const QStringList parts = entry.path.split('/', Qt::SkipEmptyParts);
        if (parts.isEmpty()) {
            continue;
        }

        RecoveryNode *current = &root;
        QString currentPath;
        for (int index = 0; index < parts.size(); ++index) {
            const QString &part = parts.at(index);
            currentPath += "/" + part;
            const bool isLeaf = index == parts.size() - 1;

            RecoveryNode *child = findChild(*current, part);
            if (!child) {
                RecoveryNode created;
                created.name = part;
                created.path = currentPath;
                created.kind = (isLeaf && entry.isFile) ? RecoveryNodeKind::File : RecoveryNodeKind::Folder;
                created.statusLabel = "Detectado";
                current->children.append(created);
                child = &current->children.last();
            }

            if (isLeaf) {
                child->kind = entry.isFile ? RecoveryNodeKind::File : RecoveryNodeKind::Folder;
                child->lineCount += 1;
            }
            current = child;
        }
    }

    for (RecoveryNode &child : root.children) {
        finalizeNode(child);
        sortChildrenRecursive(child);
    }
    return root.children;
}

QString buildSummary(
    const QString &filePath,
    const QString &device,
    int pathCount,
    int rootCount,
    bool scanStateOnly
) {
    QString summary;
    summary += "Archivo: " + filePath + "\n";
    summary += "Dispositivo detectado: " + (device.isEmpty() ? QString("no encontrado en el log") : device) + "\n";
    summary += "Rutas detectadas: " + QString::number(pathCount) + "\n";
    summary += "Raices construidas: " + QString::number(rootCount) + "\n";

    if (scanStateOnly) {
        summary += "\nEste log heredado parece contener sobre todo estado interno de escaneo.\n";
        summary += "No expone rutas claras de archivos, asi que el arbol recuperable desde el propio log sera limitado.\n";
        summary += "Para un arbol real completo necesitaremos sesion JSON, escaneo NTFS o lectura directa del disco.";
    } else {
        summary += "\nSe han reconstruido las rutas encontradas directamente en el log heredado.";
    }
    return summary;
}
} // namespace

ParsedLog LogParser::loadFromFile(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        throw std::runtime_error(QString("No se pudo abrir %1").arg(filePath).toStdString());
    }

    QVector<LogEntry> entries;
    QSet<QString> seen;
    QString detectedDevice;
    QTextStream stream(&file);
    int lineNumber = 0;

    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        ++lineNumber;

        if (line.startsWith("scan_dev=")) {
            detectedDevice = extractDevicePath(line);
            continue;
        }

        QVector<LogEntry> lineEntries;
        parsePathCandidates(line, lineNumber, lineEntries);
        for (const LogEntry &entry : lineEntries) {
            const QString key = entry.path + "|" + QString::number(entry.lineNumber);
            if (seen.contains(key)) {
                continue;
            }
            seen.insert(key);
            entries.append(entry);
        }
    }

    ParsedLog parsed;
    parsed.sourceFile = filePath;
    parsed.detectedDevice = detectedDevice;
    parsed.pathEntries = entries.size();
    parsed.scanStateOnly = entries.isEmpty();
    parsed.roots = buildTree(entries);
    parsed.summaryText = buildSummary(filePath, detectedDevice, parsed.pathEntries, parsed.roots.size(), parsed.scanStateOnly);
    return parsed;
}
