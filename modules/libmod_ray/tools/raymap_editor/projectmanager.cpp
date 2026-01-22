#include "projectmanager.h"
#include "codegenerator.h"
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDebug>

ProjectManager::ProjectManager(QObject *parent) : QObject(parent), m_project(nullptr)
{
}

bool ProjectManager::createProject(const QString &path, const QString &name)
{
    if (m_project) delete m_project;
    m_project = new Project();
    m_project->name = name;
    m_project->path = path;
    
    // Create directory if it doesn't exist
    QDir dir(path);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    // Create src directory
    dir.mkpath("src");
    dir.mkpath("assets");
    
    // Generate default main.prg
    CodeGenerator generator;
    // We can set project info if needed, but for now default template is fine
    // generator.setProject(m_project); 
    
    QString mainCode = generator.generateMainPrg();
    QFile srcFile(path + "/src/main.prg");
    if (srcFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        srcFile.write(mainCode.toUtf8());
        srcFile.close();
    }
    
    // Save project file
    QJsonObject obj;
    obj["name"] = name;
    obj["version"] = "1.0";
    
    QJsonDocument doc(obj);
    QFile file(path + "/" + name + ".bgd2proj");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        
        // Also ensure createProject implies opening it? 
        // Logic usually separates creation from opening, but we can set the current logic to "open" it right here by setting m_project values correctly which we did.
        // m_project->name and m_project->path are set.
        return true;
    }
    return false;
}

bool ProjectManager::openProject(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) return false;
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) return false;
    
    if (m_project) delete m_project;
    m_project = new Project();
    
    QJsonObject obj = doc.object();
    m_project->name = obj["name"].toString();
    m_project->path = QFileInfo(fileName).path();
    
    return true;
}

void ProjectManager::closeProject()
{
    if (m_project) {
        delete m_project;
        m_project = nullptr;
    }
}

bool ProjectManager::hasProject() const
{
    return m_project != nullptr;
}

const Project* ProjectManager::getProject() const
{
    return m_project;
}

QString ProjectManager::getProjectPath() const
{
    if (m_project) return m_project->path;
    return QString();
}

ProjectData ProjectManager::loadProjectData(const QString &projectPath)
{
    ProjectData data;
    
    // Set defaults
    data.path = projectPath;
    data.screenWidth = 800;
    data.screenHeight = 600;
    data.renderWidth = 800;
    data.renderHeight = 600;
    data.fps = 60;
    data.fov = 90;
    data.raycastQuality = 1;
    data.fpgFile = "assets.fpg";
    data.initialMap = "map.raymap";
    data.cameraX = 384.0;
    data.cameraY = 384.0;
    data.cameraZ = 0.0;
    data.cameraRot = 0.0;
    data.cameraPitch = 0.0;
    
    // Try to load from config file
    QString configPath = projectPath + "/project_config.json";
    QFile configFile(configPath);
    
    if (configFile.open(QIODevice::ReadOnly)) {
        QByteArray jsonData = configFile.readAll();
        configFile.close();
        
        QJsonDocument doc = QJsonDocument::fromJson(jsonData);
        if (!doc.isNull() && doc.isObject()) {
            QJsonObject config = doc.object();
            
            // Load all values (with defaults if not present)
            data.name = config.value("name").toString(data.name);
            data.version = config.value("version").toString("1.0");
            data.fpgFile = config.value("fpgFile").toString(data.fpgFile);
            data.initialMap = config.value("initialMap").toString(data.initialMap);
            data.screenWidth = config.value("screenWidth").toInt(data.screenWidth);
            data.screenHeight = config.value("screenHeight").toInt(data.screenHeight);
            data.renderWidth = config.value("renderWidth").toInt(data.renderWidth);
            data.renderHeight = config.value("renderHeight").toInt(data.renderHeight);
            data.fps = config.value("fps").toInt(data.fps);
            data.fov = config.value("fov").toInt(data.fov);
            data.raycastQuality = config.value("raycastQuality").toInt(data.raycastQuality);
            data.fullscreen = config.value("fullscreen").toBool(data.fullscreen);
            data.cameraX = config.value("cameraX").toDouble(data.cameraX);
            data.cameraY = config.value("cameraY").toDouble(data.cameraY);
            data.cameraZ = config.value("cameraZ").toDouble(data.cameraZ);
            data.cameraRot = config.value("cameraRot").toDouble(data.cameraRot);
            data.cameraPitch = config.value("cameraPitch").toDouble(data.cameraPitch);
            
            qDebug() << "Loaded project configuration from" << configPath;
        }
    } else {
        qDebug() << "No project configuration found at" << configPath << "- using defaults";
    }
    
    return data;
}
