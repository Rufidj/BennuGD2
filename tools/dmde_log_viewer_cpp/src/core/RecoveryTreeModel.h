#pragma once

#include <QIcon>
#include <QStandardItemModel>
#include <QVector>

#include "RecoveryNode.h"

class RecoveryTreeModel : public QStandardItemModel {
    Q_OBJECT

public:
    enum Column {
        PathColumn = 0,
        TypeColumn,
        StatusColumn,
        SizeColumn,
        LinesColumn,
        ColumnCount
    };

    explicit RecoveryTreeModel(QObject *parent = nullptr);

    void setNodes(const QVector<RecoveryNode> &nodes);
    RecoveryNode nodeFromIndex(const QModelIndex &index) const;

private:
    QIcon folderIcon_;
    QIcon imageIcon_;
    QIcon videoIcon_;
    QIcon audioIcon_;
    QIcon documentIcon_;
    QIcon archiveIcon_;
    QIcon executableIcon_;
    QIcon diskImageIcon_;
    QIcon fileIcon_;

    void appendNode(QStandardItem *parentItem, const RecoveryNode &node);
    QList<QStandardItem *> makeRow(const RecoveryNode &node) const;
    static QString formatSize(std::uint64_t sizeBytes);
    static QIcon makeColorIcon(const QColor &color);
    QIcon iconForNode(const RecoveryNode &node) const;
};

