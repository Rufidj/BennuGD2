#ifndef PUBLISHER_H
#define PUBLISHER_H

#include <QString>
#include <QObject>
#include "projectmanager.h"

class Publisher : public QObject
{
    Q_OBJECT
public:
    explicit Publisher(QObject *parent = nullptr);

    enum Platform {
        Linux,
        Android,
        Windows,
        MacOS
    };

    struct PublishConfig {
        Platform platform;
        QString outputPath;
        
        // Linux
        bool generateAppImage = false;
        QString appImageToolPath;
        
        // Windows
        bool generateStandalone = false;
        bool generateSfx = false;
        bool generateZip = true;
        
        // Android
        QString packageName;
        QString iconPath;
        bool fullProject = true;    // For Android: Generate full android studio project
        bool generateAPK = false;
        QString ndkPath; // Optional override
    };

    bool publish(const ProjectData &project, const PublishConfig &config);

signals:
    void progress(int percentage, QString message);
    void finished(bool success, QString message);

private:
    bool publishLinux(const ProjectData &project, const PublishConfig &config);
    bool publishAndroid(const ProjectData &project, const PublishConfig &config);
    bool publishWindows(const ProjectData &project, const PublishConfig &config);
    
    // Helper
    bool copyDir(const QString &source, const QString &destination);
};

#endif // PUBLISHER_H
