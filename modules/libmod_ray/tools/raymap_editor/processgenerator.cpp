#include "processgenerator.h"
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QFileInfo>
#include <QSet>

QString ProcessGenerator::generateProcessCode(const QString &processName, 
                                              const QString &assetPath,
                                              const QString &type)
{
    QString code;
    QTextStream out(&code);
    
    out << "// Auto-generated process for " << processName << "\n";
    out << "// Asset: " << assetPath << "\n";
    out << "// Type: " << type << "\n\n";
    
    if (type == "model") {
        // Global cache variables to avoid loading same asset multiple times
        out << "GLOBAL\n";
        out << "    int g_" << processName << "_model = 0;\n";
        out << "    int g_" << processName << "_texture = 0;\n";
        out << "END\n\n";
        
        out << "PROCESS " << processName << "(int spawn_id)\n";
        out << "PRIVATE\n";
        out << "    int sprite_id;\n";
        out << "    float world_x, world_y, world_z;\n";
        out << "    float rotation = 0.0;\n";
        out << "    float scale = 1.0;\n";
        out << "END\n";
        out << "BEGIN\n";
        out << "    \n";
        out << "    // Get spawn position from flag\n";
        out << "    world_x = RAY_GET_FLAG_X(spawn_id);\n";
        out << "    world_y = RAY_GET_FLAG_Y(spawn_id);\n";
        out << "    world_z = RAY_GET_FLAG_Z(spawn_id);\n";
        out << "    \n";
        
        // Clean path logic
        QString cleanPath = assetPath;
        QFileInfo fi(assetPath);
        if (fi.isAbsolute()) {
            cleanPath = "assets/models/" + fi.fileName();
        }
        
        // Create texture path (assume .png for now, matching editor save logic)
        QString texturePath = cleanPath;
        if (texturePath.endsWith(".md3", Qt::CaseInsensitive)) {
            texturePath.replace(".md3", ".png", Qt::CaseInsensitive);
        } else {
            texturePath += ".png";
        }
    
        out << "    // Load Model and Texture (Cached)\n";
        out << "    if (g_" << processName << "_model == 0)\n";
        out << "        g_" << processName << "_model = RAY_LOAD_MD3(\"" << cleanPath << "\");\n";
        out << "        g_" << processName << "_texture = map_load(\"" << texturePath << "\");\n";
        out << "        \n";
        out << "        if (g_" << processName << "_texture == 0)\n";
        out << "            say(\"[" << processName << "] WARNING: Failed to load texture: \" + \"" << texturePath << "\");\n";
        out << "        end\n";
        out << "        if (g_" << processName << "_model == 0)\n";
        out << "            say(\"[" << processName << "] ERROR: Failed to load model: \" + \"" << cleanPath << "\");\n";
        out << "        end\n";
        out << "    end\n";
        out << "    \n";
        
        out << "    if (g_" << processName << "_model == 0)\n";
        out << "        RAY_CLEAR_FLAG();\n";
        out << "        return;\n";
        out << "    end\n";
        out << "    \n";
        
        out << "    // Create sprite with model\n";
        out << "    sprite_id = RAY_ADD_SPRITE(world_x, world_y, world_z, 0, 0, 0);\n";
        out << "    if (sprite_id < 0)\n";
        out << "        say(\"[" << processName << "] ERROR: Failed to create sprite\");\n";
        out << "        RAY_CLEAR_FLAG();\n";
        out << "        return;\n";
        out << "    end\n";
        out << "    \n";
        out << "    RAY_SET_SPRITE_MD2(sprite_id, g_" << processName << "_model, g_" << processName << "_texture);\n";
        out << "    RAY_SET_SPRITE_SCALE(sprite_id, scale);\n";
        out << "    RAY_SET_SPRITE_ANGLE(sprite_id, rotation);\n";
        out << "    \n";
        // out << "    say(\"[" << processName << "] Spawned successfully! sprite_id=\" + sprite_id);\n";
        out << "    \n";
        out << "    LOOP\n";
        out << "        // Entity logic here\n";
        out << "        // Update position if needed:\n";
        out << "        // RAY_UPDATE_SPRITE_POSITION(sprite_id, world_x, world_y, world_z);\n";
        out << "        FRAME;\n";
        out << "    END\n";
        out << "    \n";
        out << "    // Cleanup\n";
        // out << "    say(\"[" << processName << "] Cleaning up\");\n";
        out << "    RAY_CLEAR_FLAG();\n";
        out << "    RAY_REMOVE_SPRITE(sprite_id);\n";
        out << "END\n";
    } else if (type == "sprite") {
        out << "PROCESS " << processName << "(float world_x, float world_y, float world_z)\n";
        out << "PRIVATE\n";
        out << "    int sprite_id;\n";
        out << "    int texture_id = 1;  // TODO: Get from FPG\n";
        out << "END\n";
        out << "BEGIN\n";
        out << "    // Create sprite\n";
        out << "    sprite_id = RAY_ADD_SPRITE(world_x, world_y, world_z, texture_id, 64, 64);\n";
        out << "    \n";
        out << "    LOOP\n";
        out << "        // Sprite logic here\n";
        out << "        // Update position if needed:\n";
        out << "        // RAY_UPDATE_SPRITE_POSITION(sprite_id, world_x, world_y, world_z);\n";
        out << "        FRAME;\n";
        out << "    END\n";
        out << "    \n";
        out << "    // Cleanup\n";
        out << "    RAY_REMOVE_SPRITE(sprite_id);\n";
        out << "END\n";
    }
    
    return code;
}

QString ProcessGenerator::generateIncludesSection(const QVector<EntityInstance> &entities)
{
    QString includes;
    QTextStream out(&includes);
    
    // Get unique process names
    QStringList uniqueProcesses = getUniqueProcessNames(entities);
    
    if (!uniqueProcesses.isEmpty()) {
        out << "// Entity includes\n";
        for (const QString &processName : uniqueProcesses) {
            out << "#include \"includes/" << processName << ".h\"\n";
        }
        out << "\n";
    }
    
    return includes;
}

QString ProcessGenerator::generateSpawnCalls(const QVector<EntityInstance> &entities)
{
    QString spawns;
    QTextStream out(&spawns);
    
    if (!entities.isEmpty()) {
        out << "    // Spawn entities\n";
        for (const EntityInstance &entity : entities) {
            out << "    " << entity.processName << "(" 
                << entity.spawn_id << ");  // Entity at (" 
                << entity.x << ", " 
                << entity.y << ", " 
                << entity.z << ")\n";
        }
        out << "\n";
    }
    
    return spawns;
}

bool ProcessGenerator::saveProcessFile(const QString &projectPath,
                                       const QString &processName,
                                       const QString &code)
{
    // Create includes directory if it doesn't exist
    QString includesPath = projectPath + "/src/includes";
    QDir dir;
    if (!dir.exists(includesPath)) {
        dir.mkpath(includesPath);
    }
    
    // Save file
    QString filePath = includesPath + "/" + processName + ".h";
    QFile file(filePath);
    
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream out(&file);
    out << code;
    file.close();
    
    return true;
}

QStringList ProcessGenerator::getUniqueProcessNames(const QVector<EntityInstance> &entities)
{
    QSet<QString> uniqueNames;
    
    for (const EntityInstance &entity : entities) {
        uniqueNames.insert(entity.processName);
    }
    
    return uniqueNames.values();
}
