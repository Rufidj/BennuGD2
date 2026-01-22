#ifndef MD3GENERATOR_H
#define MD3GENERATOR_H

#include <QString>
#include <QVector>
#include <QVector3D>
#include <QVector2D>
#include <QFile>
#include <QDataStream>

// MD3 Structures (simplified for export)
namespace MD3 {
    struct Header {
        char ident[4]; // "IDP3"
        int32_t version; // 15
        char name[64];
        int32_t flags;
        int32_t num_frames;
        int32_t num_tags;
        int32_t num_surfaces;
        int32_t num_skins;
        int32_t ofs_frames;
        int32_t ofs_tags;
        int32_t ofs_surfaces;
        int32_t ofs_eof;
    };

    struct Surface {
        char ident[4]; // "IDP3"
        char name[64];
        int32_t flags;
        int32_t num_frames;
        int32_t num_shaders;
        int32_t num_verts;
        int32_t num_triangles;
        int32_t ofs_triangles;
        int32_t ofs_shaders;
        int32_t ofs_st;
        int32_t ofs_xyznormal;
        int32_t ofs_end;
    };
    
    struct Shader {
        char name[64];
        int32_t shader_index;
    };
    
    struct Triangle {
        int32_t indexes[3];
    };
    
    struct TexCoord {
        float st[2];
    };
    
    struct Vertex {
        int16_t coord[3]; // x,y,z * 64
        uint8_t normal[2]; // encoded
    };
    
    struct Frame {
        QVector3D min_bounds;
        QVector3D max_bounds;
        QVector3D local_origin;
        float radius;
        char name[16];
    };
}

class MD3Generator
{
public:
    enum MeshType { RAMP, STAIRS, CYLINDER, BOX, BRIDGE, HOUSE, ARCH };
    
    struct MeshData {
        struct VertexData {
            QVector3D pos;
            QVector3D normal;
            QVector2D uv;
        };
        
        QVector<VertexData> vertices;
        QVector<int> indices;
        QString textureName;
        QString name;
    };

    static bool generateAndSave(MeshType type, float width, float height, float depth, int segments, 
                                const QString &texturePath, const QString &outputPath);
    
    // New: Multi-texture version
    static bool generateAndSaveMultiTex(MeshType type, float width, float height, float depth, int segments,
                                        const QStringList &texturePaths, const QString &outputPath,
                                        bool hasRailings = false, int roofType = 0);
                                
    // New: Generate mesh data without saving (for preview)
    static MeshData generateMesh(MeshType type, float width, float height, float depth, int segments);
    static MeshData generateMesh(MeshType type, float width, float height, float depth, int segments, bool hasRailings, int roofType);
    static MeshData generateMesh(MeshType type, float width, float height, float depth, int segments, bool hasRailings, bool hasArch, int roofType);

    // Save MD3 file
    static bool saveMD3(const MeshData &mesh, const QString &filename);

private:
    static MeshData generateRamp(float width, float height, float depth);
    static MeshData generateStairs(float width, float height, float depth, int steps);
    static MeshData generateBox(float width, float height, float depth);
    static MeshData generateCylinder(float width, float height, float depth, int segments);
    
    // New geometry types
    static MeshData generateBridge(float width, float height, float depth, bool hasRailings, bool hasArch);
    static MeshData generateHouse(float width, float height, float depth, int roofType);
    static MeshData generateArch(float width, float height, float depth, int segments);
    
    // Helper to calculate normals
    static void calculateNormals(MeshData &mesh);
};

#endif // MD3GENERATOR_H
