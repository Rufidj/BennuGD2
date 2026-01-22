#ifndef PROCESSGENERATOR_H
#define PROCESSGENERATOR_H

#include <QString>
#include <QMap>
#include "mapdata.h"

class ProcessGenerator
{
public:
    // Generate process code for an entity type
    static QString generateProcessCode(const QString &processName, 
                                      const QString &assetPath,
                                      const QString &type);
    
    // Generate includes section for main.prg
    static QString generateIncludesSection(const QVector<EntityInstance> &entities);
    
    // Generate spawn calls for main.prg
    static QString generateSpawnCalls(const QVector<EntityInstance> &entities);
    
    // Save process file to src/includes/
    static bool saveProcessFile(const QString &projectPath,
                               const QString &processName,
                               const QString &code);
    
    // Get unique process names from entities
    static QStringList getUniqueProcessNames(const QVector<EntityInstance> &entities);
};

#endif // PROCESSGENERATOR_H
