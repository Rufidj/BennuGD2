#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include "RecoveryNode.h"

struct LoadedSession {
    QString format;
    QString sourceRoot;
    QString summaryText;
    QString rawJsonText;
    QVector<RecoveryNode> roots;
    QStringList checkedPaths;
};

class SessionLoader {
public:
    static LoadedSession loadFromFile(const QString &filePath);

private:
    static QJsonValue restoreJsonSafeValue(const QJsonValue &value);
    static RecoveryNode parseNode(const QJsonObject &object);
    static QVector<RecoveryNode> parseRoots(const QJsonObject &object);
    static QVector<RecoveryNode> parsePythonSessionRoots(const QJsonObject &object);
    static QString buildSummaryText(const QJsonObject &rootObject, const LoadedSession &session);
};

