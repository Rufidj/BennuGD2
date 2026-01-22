#ifndef PROJECTMANAGER_H
#define PROJECTMANAGER_H

#include <QObject>
#include <QString>
#include <QVector>

struct Project {
    QString name;
    QString path;
    // Add other project fields as needed
};

struct ProjectData {
    QString name;
    QString version;
    QString path;
    QString fpgFile;
    QString initialMap;
    QString mainScript = "src/main.prg";  // Main script to compile
    
    // Display settings
    int screenWidth = 800;
    int screenHeight = 600;
    int renderWidth = 800;   // Internal render resolution
    int renderHeight = 600;  // Internal render resolution
    int fps = 60;
    int fov = 90;
    int raycastQuality = 1;
    bool fullscreen = false;  // Fullscreen mode
    
    // Camera
    double cameraX = 0.0;
    double cameraY = 0.0;
    double cameraZ = 0.0;
    double cameraRot = 0.0;
    double cameraPitch = 0.0;
    
    // Publishing
    QString packageName;  // For Android publishing
    QString iconPath;     // For publishing
};

class ProjectManager : public QObject
{
    Q_OBJECT
public:
    explicit ProjectManager(QObject *parent = nullptr);
    
    bool createProject(const QString &path, const QString &name);
    bool openProject(const QString &fileName);
    void closeProject();
    bool hasProject() const;
    const Project* getProject() const;
    QString getProjectPath() const;
    
    // Load project configuration (resolution, etc.)
    static ProjectData loadProjectData(const QString &projectPath);

private:
    Project *m_project;
};

#endif // PROJECTMANAGER_H
