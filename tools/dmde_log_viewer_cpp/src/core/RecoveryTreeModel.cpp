#include "RecoveryTreeModel.h"

#include <QBrush>
#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QStandardItem>
#include <QStringList>

namespace {
constexpr int kIconCanvas = 20;

QString detectCategory(const RecoveryNode &node) {
    const QString lower = node.name.toLower();
    if (lower.endsWith(".jpg") || lower.endsWith(".jpeg") || lower.endsWith(".png") ||
        lower.endsWith(".gif") || lower.endsWith(".bmp") || lower.endsWith(".webp") ||
        lower.endsWith(".tif") || lower.endsWith(".tiff") || lower.endsWith(".heic") ||
        lower.endsWith(".raw") || lower.endsWith(".cr2") || lower.endsWith(".nef")) {
        return "image";
    }
    if (lower.endsWith(".mp4") || lower.endsWith(".mov") || lower.endsWith(".avi") ||
        lower.endsWith(".mkv") || lower.endsWith(".mts") || lower.endsWith(".mpeg") ||
        lower.endsWith(".mpg") || lower.endsWith(".m4v") || lower.endsWith(".3gp")) {
        return "video";
    }
    if (lower.endsWith(".mp3") || lower.endsWith(".wav") || lower.endsWith(".flac") ||
        lower.endsWith(".ogg") || lower.endsWith(".aac") || lower.endsWith(".m4a") ||
        lower.endsWith(".wma")) {
        return "audio";
    }
    if (lower.endsWith(".pdf") || lower.endsWith(".doc") || lower.endsWith(".docx") ||
        lower.endsWith(".txt") || lower.endsWith(".xml") || lower.endsWith(".json") ||
        lower.endsWith(".csv") || lower.endsWith(".xls") || lower.endsWith(".xlsx") ||
        lower.endsWith(".ppt") || lower.endsWith(".pptx") || lower.endsWith(".odt") ||
        lower.endsWith(".ods") || lower.endsWith(".rtf") || lower.endsWith(".md")) {
        return "document";
    }
    if (lower.endsWith(".zip") || lower.endsWith(".rar") || lower.endsWith(".7z") ||
        lower.endsWith(".gz") || lower.endsWith(".tar") || lower.endsWith(".bz2") ||
        lower.endsWith(".xz")) {
        return "archive";
    }
    if (lower.endsWith(".iso") || lower.endsWith(".img") || lower.endsWith(".dmg") ||
        lower.endsWith(".vhd") || lower.endsWith(".vmdk")) {
        return "disk_image";
    }
    if (lower.endsWith(".exe") || lower.endsWith(".msi") || lower.endsWith(".app") ||
        lower.endsWith(".sh") || lower.endsWith(".bat") || lower.endsWith(".cmd") ||
        lower.endsWith(".bin") || lower.endsWith(".elf")) {
        return "executable";
    }
    return "file";
}

QIcon makeDocumentLikeIcon(const QColor &paperColor, const QColor &accentColor, const QString &kind) {
    QPixmap pixmap(kIconCanvas, kIconCanvas);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath page;
    page.moveTo(3.5, 2.0);
    page.lineTo(12.8, 2.0);
    page.lineTo(16.0, 5.2);
    page.lineTo(16.0, 17.5);
    page.lineTo(3.5, 17.5);
    page.closeSubpath();

    painter.setPen(QPen(QColor("#101214"), 0.9));
    painter.setBrush(paperColor);
    painter.drawPath(page);

    QPolygonF fold;
    fold << QPointF(12.8, 2.0) << QPointF(12.8, 5.2) << QPointF(16.0, 5.2);
    painter.setBrush(paperColor.lighter(125));
    painter.drawPolygon(fold);

    if (kind == "image") {
        painter.setPen(Qt::NoPen);
        painter.setBrush(accentColor);
        painter.drawRoundedRect(QRectF(5.0, 7.2, 9.4, 6.6), 1.2, 1.2);
        painter.setBrush(QColor("#d9f3ff"));
        painter.drawEllipse(QPointF(12.1, 8.8), 1.0, 1.0);
        QPainterPath mountain;
        mountain.moveTo(5.6, 12.8);
        mountain.lineTo(8.3, 10.0);
        mountain.lineTo(10.0, 11.3);
        mountain.lineTo(11.8, 9.2);
        mountain.lineTo(13.8, 12.8);
        mountain.closeSubpath();
        painter.setBrush(QColor("#2f8f57"));
        painter.drawPath(mountain);
    } else if (kind == "video") {
        painter.setPen(Qt::NoPen);
        painter.setBrush(accentColor);
        painter.drawRoundedRect(QRectF(5.0, 7.0, 9.6, 6.8), 1.2, 1.2);
        QPolygonF play;
        play << QPointF(8.0, 8.4) << QPointF(12.0, 10.4) << QPointF(8.0, 12.4);
        painter.setBrush(QColor("#ffffff"));
        painter.drawPolygon(play);
    } else if (kind == "audio") {
        painter.setPen(QPen(accentColor, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(11.6, 5.8), QPointF(11.6, 12.4));
        painter.drawLine(QPointF(11.6, 5.8), QPointF(14.0, 6.7));
        painter.setBrush(accentColor);
        painter.drawEllipse(QPointF(8.4, 13.0), 1.7, 1.5);
        painter.drawEllipse(QPointF(11.8, 12.0), 1.7, 1.5);
    } else if (kind == "archive") {
        painter.setPen(Qt::NoPen);
        painter.setBrush(accentColor);
        painter.drawRoundedRect(QRectF(6.0, 5.6, 6.8, 8.7), 0.9, 0.9);
        painter.setBrush(QColor("#f3d48b"));
        for (int y = 6; y <= 13; y += 2) {
            painter.drawRect(QRectF(8.6, y, 1.6, 1.2));
        }
    } else if (kind == "executable") {
        painter.setPen(QPen(accentColor, 1.6));
        painter.drawLine(QPointF(5.8, 7.0), QPointF(8.1, 9.4));
        painter.drawLine(QPointF(5.8, 11.8), QPointF(8.1, 9.4));
        painter.drawLine(QPointF(9.8, 12.2), QPointF(13.4, 12.2));
    } else if (kind == "disk_image") {
        painter.setPen(Qt::NoPen);
        painter.setBrush(accentColor);
        painter.drawEllipse(QRectF(4.8, 6.0, 9.6, 9.6));
        painter.setBrush(QColor("#e6eefb"));
        painter.drawEllipse(QRectF(8.6, 9.8, 2.0, 2.0));
    } else {
        painter.setPen(QPen(accentColor, 1.1));
        painter.drawLine(QPointF(5.8, 8.2), QPointF(13.6, 8.2));
        painter.drawLine(QPointF(5.8, 10.6), QPointF(13.6, 10.6));
        painter.drawLine(QPointF(5.8, 13.0), QPointF(11.0, 13.0));
    }

    return QIcon(pixmap);
}

QIcon makeFolderIcon() {
    QPixmap pixmap(kIconCanvas, kIconCanvas);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor("#8e641f"), 0.9));
    painter.setBrush(QColor("#d9a441"));
    painter.drawRoundedRect(QRectF(2.0, 5.0, 16.0, 11.5), 1.6, 1.6);
    painter.setBrush(QColor("#e3b85f"));
    painter.drawRoundedRect(QRectF(3.0, 2.5, 6.2, 3.8), 1.0, 1.0);
    painter.setBrush(QColor("#f2cb71"));
    painter.drawRoundedRect(QRectF(2.4, 6.0, 15.0, 9.2), 1.2, 1.2);
    return QIcon(pixmap);
}
} // namespace

RecoveryTreeModel::RecoveryTreeModel(QObject *parent)
    : QStandardItemModel(parent),
      folderIcon_(makeFolderIcon()),
      imageIcon_(makeDocumentLikeIcon(QColor("#eef7fb"), QColor("#4a8cab"), "image")),
      videoIcon_(makeDocumentLikeIcon(QColor("#fff5df"), QColor("#a78f42"), "video")),
      audioIcon_(makeDocumentLikeIcon(QColor("#fff0e4"), QColor("#c97c2f"), "audio")),
      documentIcon_(makeDocumentLikeIcon(QColor("#f5f0fb"), QColor("#7d6b9c"), "document")),
      archiveIcon_(makeDocumentLikeIcon(QColor("#fff2e8"), QColor("#a56d39"), "archive")),
      executableIcon_(makeDocumentLikeIcon(QColor("#fff0f0"), QColor("#d14f4f"), "executable")),
      diskImageIcon_(makeDocumentLikeIcon(QColor("#eef3ff"), QColor("#4f7bd1"), "disk_image")),
      fileIcon_(makeDocumentLikeIcon(QColor("#f0f2f4"), QColor("#8d959d"), "file")) {
    setColumnCount(ColumnCount);
    setHorizontalHeaderLabels(QStringList() << "Ruta" << "Tipo" << "Estado" << "Tamano" << "Lineas");
}

void RecoveryTreeModel::setNodes(const QVector<RecoveryNode> &nodes) {
    removeRows(0, rowCount());
    for (const RecoveryNode &node : nodes) {
        appendNode(invisibleRootItem(), node);
    }
}

RecoveryNode RecoveryTreeModel::nodeFromIndex(const QModelIndex &index) const {
    if (!index.isValid()) {
        return {};
    }
    const QVariant payload = itemFromIndex(index.sibling(index.row(), PathColumn))->data(Qt::UserRole + 1);
    return payload.value<RecoveryNode>();
}

void RecoveryTreeModel::appendNode(QStandardItem *parentItem, const RecoveryNode &node) {
    QList<QStandardItem *> row = makeRow(node);
    parentItem->appendRow(row);
    QStandardItem *pathItem = row.at(PathColumn);
    for (const RecoveryNode &child : node.children) {
        appendNode(pathItem, child);
    }
}

QList<QStandardItem *> RecoveryTreeModel::makeRow(const RecoveryNode &node) const {
    auto *pathItem = new QStandardItem(iconForNode(node), node.name);
    auto *typeItem = new QStandardItem(node.typeLabel);
    const QString statusText =
        node.statusLabel.trimmed().isEmpty() ? (node.deleted ? "Borrado" : "Activo") : node.statusLabel;
    auto *statusItem = new QStandardItem(statusText);
    auto *sizeItem = new QStandardItem(formatSize(node.sizeBytes));
    auto *linesItem = new QStandardItem(QString::number(node.lineCount));

    pathItem->setCheckable(true);
    pathItem->setEditable(false);
    typeItem->setEditable(false);
    statusItem->setEditable(false);
    sizeItem->setEditable(false);
    linesItem->setEditable(false);

    pathItem->setData(QVariant::fromValue(node), Qt::UserRole + 1);
    typeItem->setData(QVariant::fromValue(node), Qt::UserRole + 1);
    statusItem->setData(QVariant::fromValue(node), Qt::UserRole + 1);
    sizeItem->setData(QVariant::fromValue(node), Qt::UserRole + 1);
    linesItem->setData(QVariant::fromValue(node), Qt::UserRole + 1);

    const QColor muted(node.deleted ? "#b58a8a" : "#a9b2ac");
    typeItem->setForeground(QBrush(muted));
    QColor statusColor("#7ee0a1");
    if (node.deleted) {
        statusColor = QColor("#d98b8b");
    } else if (statusText.compare("Recuperado", Qt::CaseInsensitive) == 0) {
        statusColor = QColor("#7ec8ff");
    } else if (statusText.compare("Recuperado parcial", Qt::CaseInsensitive) == 0) {
        statusColor = QColor("#f0c36d");
    }
    statusItem->setForeground(QBrush(statusColor));
    sizeItem->setForeground(QBrush(QColor("#d7d7d7")));
    linesItem->setForeground(QBrush(QColor("#9da3a8")));

    return {pathItem, typeItem, statusItem, sizeItem, linesItem};
}

QString RecoveryTreeModel::formatSize(std::uint64_t sizeBytes) {
    static const char *units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    double value = static_cast<double>(sizeBytes);
    int unitIndex = 0;
    while (value >= 1024.0 && unitIndex < 5) {
        value /= 1024.0;
        ++unitIndex;
    }
    if (unitIndex == 0) {
        return QString::number(static_cast<qulonglong>(value)) + " " + units[unitIndex];
    }
    return QString::number(value, 'f', 1) + " " + units[unitIndex];
}

QIcon RecoveryTreeModel::makeColorIcon(const QColor &color) {
    QPixmap pixmap(14, 14);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawRoundedRect(1, 1, 12, 12, 3, 3);
    return QIcon(pixmap);
}

QIcon RecoveryTreeModel::iconForNode(const RecoveryNode &node) const {
    if (node.kind == RecoveryNodeKind::Folder) {
        return folderIcon_;
    }

    const QString category = detectCategory(node);
    if (category == "image") {
        return imageIcon_;
    }
    if (category == "video") {
        return videoIcon_;
    }
    if (category == "audio") {
        return audioIcon_;
    }
    if (category == "document") {
        return documentIcon_;
    }
    if (category == "archive") {
        return archiveIcon_;
    }
    if (category == "executable") {
        return executableIcon_;
    }
    if (category == "disk_image") {
        return diskImageIcon_;
    }
    return fileIcon_;
}

