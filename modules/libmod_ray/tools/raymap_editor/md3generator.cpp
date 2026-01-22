#include "md3generator.h"
#include <QFileInfo>
#include <QDebug>
#include <QtMath>

// Helper to encode normal to 16-bit (lat/lng)
// Detailed impl omitted for brevity, simplified version:
// For now, we might write dummy normals or implement a simple encoder.
// MD3 normal encoding: 
// 8 bits lat (0..255 -> 0..360), 8 bits lng (0..255 -> 0..360)
void encodeNormal(const QVector3D &n, uint8_t *bytes) {
    float x = n.x();
    float y = n.y();
    float z = n.z();
    
    // Simple placeholder: defaults to UP
    bytes[0] = 0; 
    bytes[1] = 0;
    
    if (n.lengthSquared() == 0) return;
    
    // Normalized check
    float length = qSqrt(x*x + y*y + z*z);
    if (length > 0) {
        x /= length; y /= length; z /= length;
    }
    
    // Convert to spherical
    float lat = qAcos(z); // 0..PI
    float lng = qAtan2(y, x); // -PI..PI
    
    bytes[0] = (uint8_t)( (lat / 3.14159f) * 255.0f );
    bytes[1] = (uint8_t)( ((lng + 3.14159f) / (2.0f * 3.14159f)) * 255.0f );
}

bool MD3Generator::generateAndSave(MeshType type, float width, float height, float depth, int segments, 
                                   const QString &texturePath, const QString &outputPath)
{
    MeshData mesh = generateMesh(type, width, height, depth, segments);
    
    // Set texture name from path (just filename)
    QFileInfo fi(texturePath);
    mesh.textureName = fi.fileName();
    mesh.name = "surfaces";
    
    bool success = saveMD3(mesh, outputPath);
    
    // Copy texture file alongside the MD3 if texture path is valid
    if (success && !texturePath.isEmpty() && QFile::exists(texturePath)) {
        QFileInfo mdlInfo(outputPath);
        QString textureExt = fi.suffix(); // Get original extension (jpg, png, etc)
        QString destTexturePath = mdlInfo.absolutePath() + "/" + mdlInfo.baseName() + "." + textureExt;
        
        // Remove existing file if present
        if (QFile::exists(destTexturePath)) {
            QFile::remove(destTexturePath);
        }
        
        // Copy texture
        if (!QFile::copy(texturePath, destTexturePath)) {
            qWarning() << "Failed to copy texture to:" << destTexturePath;
        }
    }
    
    return success;
}

MD3Generator::MeshData MD3Generator::generateMesh(MeshType type, float width, float height, float depth, int segments)
{
    MeshData mesh;
    switch (type) {
        case RAMP: mesh = generateRamp(width, height, depth); break;
        case STAIRS: mesh = generateStairs(width, height, depth, segments); break;
        case BOX: mesh = generateBox(width, height, depth); break;
        case CYLINDER: mesh = generateCylinder(width, height, depth, segments); break;
        case BRIDGE: mesh = generateBridge(width, height, depth, true, false); break; // Railings yes, arch no by default
        case HOUSE: mesh = generateHouse(width, height, depth, 2); break; // Gabled roof by default
        case ARCH: mesh = generateArch(width, height, depth, qMax(segments, 8)); break;
    }
    return mesh;
}

// Overload with all options
MD3Generator::MeshData MD3Generator::generateMesh(MeshType type, float width, float height, float depth, int segments, bool hasRailings, int roofType)
{
    // Note: This overload doesn't have hasArch parameter, so we'll add it
    return generateMesh(type, width, height, depth, segments, hasRailings, false, roofType);
}

// Full overload with all options including hasArch
MD3Generator::MeshData MD3Generator::generateMesh(MeshType type, float width, float height, float depth, int segments, bool hasRailings, bool hasArch, int roofType)
{
    MeshData mesh;
    switch (type) {
        case RAMP: mesh = generateRamp(width, height, depth); break;
        case STAIRS: mesh = generateStairs(width, height, depth, segments); break;
        case BOX: mesh = generateBox(width, height, depth); break;
        case CYLINDER: mesh = generateCylinder(width, height, depth, segments); break;
        case BRIDGE: mesh = generateBridge(width, height, depth, hasRailings, hasArch); break;
        case HOUSE: mesh = generateHouse(width, height, depth, roofType); break;
        case ARCH: mesh = generateArch(width, height, depth, qMax(segments, 8)); break;
    }
    return mesh;
}

MD3Generator::MeshData MD3Generator::generateRamp(float width, float height, float depth)
{
    MeshData mesh;
    float w2 = width / 2.0f;
    
    // Ramp is a wedge.
    // Base (y=0) is at depth 0? No, let's say depth is Y length.
    // 6 vertices? Or 8 for full UV control? 
    // Vertices needed for sharp edges -> split normals -> unique vertices per face.
    // Faces: Bottom, Back, Left, Right, Sloped Top.
    // 5 Faces. 
    // Bottom: 2 Tris
    // Back: 2 Tris
    // Sides: 1 Tri each (wedge)
    // Top: 2 Tris
    
    // Total 8 triangles, 24 indices.
    // Vertices: Unshared for UV/Normals? 
    
    // Coordinates: X = Width, Y = Depth, Z = Height
    
    QVector3D v_bl( -w2, 0, 0 ); // Bottom Left Front
    QVector3D v_br(  w2, 0, 0 ); // Bottom Right Front
    QVector3D v_tl( -w2, depth, height ); // Top Left Back
    QVector3D v_tr(  w2, depth, height ); // Top Right Back
    QVector3D v_bl_back( -w2, depth, 0 ); // Bottom Left Back
    QVector3D v_br_back(  w2, depth, 0 ); // Bottom Right Back
    
    // Front edge is at height 0.
    
    // --- TOP FACE (Sloped) ---
    // v_bl -> v_br -> v_tr -> v_tl
    // UVs: 0,0 to 1,1
    // Normal: Calculated from slope
    QVector3D slopeN = QVector3D::crossProduct(v_br - v_bl, v_tl - v_bl).normalized();
    
    mesh.vertices.append({ v_bl, slopeN, QVector2D(0, 0) });
    mesh.vertices.append({ v_br, slopeN, QVector2D(1, 0) });
    mesh.vertices.append({ v_tr, slopeN, QVector2D(1, 1) });
    mesh.vertices.append({ v_tl, slopeN, QVector2D(0, 1) });
    
    // Indices for Top
    int base = 0;
    mesh.indices << base+0 << base+1 << base+2;
    mesh.indices << base+0 << base+2 << base+3;
    
    // --- BACK FACE ---
    // v_bl_back, v_br_back, v_tr, v_tl
    QVector3D backN(0, 1, 0); // Outwards? Or inwards? MD3 Y is usually forward.
    // Let's assume standard right hand.
    
    // ... Simplified box-like generation for now.
    // Actually, let's just make the Ramp face for now as that's the most visible part.
    // But we need a closed mesh for shadows etc.
    
    // Left Side
    base = mesh.vertices.size();
    mesh.vertices.append({ v_bl, QVector3D(-1,0,0), QVector2D(0,0) });
    mesh.vertices.append({ v_tl, QVector3D(-1,0,0), QVector2D(1,1) });
    mesh.vertices.append({ v_bl_back, QVector3D(-1,0,0), QVector2D(1,0) });
    mesh.indices << base+0 << base+2 << base+1;
    
    // Right side
    base = mesh.vertices.size();
    mesh.vertices.append({ v_br, QVector3D(1,0,0), QVector2D(0,0) });
    mesh.vertices.append({ v_br_back, QVector3D(1,0,0), QVector2D(1,0) });
    mesh.vertices.append({ v_tr, QVector3D(1,0,0), QVector2D(1,1) });
    mesh.indices << base+0 << base+1 << base+2;
    
    // Back Face
    base = mesh.vertices.size();
    mesh.vertices.append({ v_bl_back, QVector3D(0,1,0), QVector2D(0,0) });
    mesh.vertices.append({ v_tl, QVector3D(0,1,0), QVector2D(0,1) });
    mesh.vertices.append({ v_tr, QVector3D(0,1,0), QVector2D(1,1) });
    mesh.vertices.append({ v_br_back, QVector3D(0,1,0), QVector2D(1,0) });
    mesh.indices << base+0 << base+2 << base+1;
    mesh.indices << base+0 << base+3 << base+2;
    
    // Bottom Face
    base = mesh.vertices.size();
    mesh.vertices.append({ v_bl, QVector3D(0,0,-1), QVector2D(0,0) });
    mesh.vertices.append({ v_bl_back, QVector3D(0,0,-1), QVector2D(0,1) });
    mesh.vertices.append({ v_br_back, QVector3D(0,0,-1), QVector2D(1,1) });
    mesh.vertices.append({ v_br, QVector3D(0,0,-1), QVector2D(1,0) });
    mesh.indices << base+0 << base+2 << base+1;
    mesh.indices << base+0 << base+3 << base+2;
    
    return mesh;
}

MD3Generator::MeshData MD3Generator::generateBox(float width, float height, float depth) {
    MeshData mesh;
    float w2 = width / 2.0f;
    float h = height;
    float d = depth; // Y axis depth
    
    // 8 vertices for a box? UVs need duplicated vertices per face.
    // 6 Faces * 4 Vertices/Face = 24 Vertices.
    
    // Helper lambda to add face
    auto addFace = [&](QVector3D v1, QVector3D v2, QVector3D v3, QVector3D v4, QVector3D normal) {
        int base = mesh.vertices.size();
        mesh.vertices.append({v1, normal, QVector2D(0,0)});
        mesh.vertices.append({v2, normal, QVector2D(1,0)});
        mesh.vertices.append({v3, normal, QVector2D(1,1)});
        mesh.vertices.append({v4, normal, QVector2D(0,1)});
        
        mesh.indices << base << base+1 << base+2;
        mesh.indices << base << base+2 << base+3;
    };
    
    // Coordinates: X=Width (Left/Right), Y=Depth (Back), Z=Height (Up)
    QVector3D p1(-w2, 0, 0); // Front Left Bottom
    QVector3D p2( w2, 0, 0); // Front Right Bottom
    QVector3D p3( w2, d, 0); // Back Right Bottom
    QVector3D p4(-w2, d, 0); // Back Left Bottom
    
    QVector3D p5(-w2, 0, h); // Front Left Top
    QVector3D p6( w2, 0, h); // Front Right Top
    QVector3D p7( w2, d, h); // Back Right Top
    QVector3D p8(-w2, d, h); // Back Left Top
    
    // Front Face (p1, p2, p6, p5) - Normal -Y? No, Front is Y=0 usually? 
    // Wait, coordinate system:
    // User sees Grid. X is horizontal, Y is vertical (Top down).
    // In 3D: X is Right, Y is Forward (check logic).
    // Let's assume Front is Y=0, Back is Y=Depth.
    
    addFace(p5, p6, p2, p1, QVector3D(0,-1,0)); // Front
    addFace(p7, p8, p4, p3, QVector3D(0,1,0));  // Back
    addFace(p8, p5, p1, p4, QVector3D(-1,0,0)); // Left
    addFace(p6, p7, p3, p2, QVector3D(1,0,0));  // Right
    addFace(p8, p7, p6, p5, QVector3D(0,0,1));  // Top
    addFace(p1, p2, p3, p4, QVector3D(0,0,-1)); // Bottom
    
    return mesh; 
}

MD3Generator::MeshData MD3Generator::generateStairs(float width, float height, float depth, int steps) {
    MeshData mesh;
    if (steps < 1) steps = 1;
    
    float w2 = width / 2.0f;
    float stepDepth = depth / steps;
    float stepHeight = height / steps;
    
    // Helper lambda to add face (copied from box, duplicate code acceptable for now or move to helper)
    auto addFace = [&](QVector3D v1, QVector3D v2, QVector3D v3, QVector3D v4, QVector3D normal, float uvYStart, float uvYEnd, float uvZStart = 0, float uvZEnd = 0) {
        int base = mesh.vertices.size();
        mesh.vertices.append({v1, normal, QVector2D(0,uvYStart)});
        mesh.vertices.append({v2, normal, QVector2D(1,uvYStart)});
        mesh.vertices.append({v3, normal, QVector2D(1,uvYEnd)});
        mesh.vertices.append({v4, normal, QVector2D(0,uvYEnd)});
        
        mesh.indices << base << base+1 << base+2;
        mesh.indices << base << base+2 << base+3;
    };

    // Generate each step as a box-like structure (but optimized? No, just draw visible faces)
    // Actually, simple way: Just draw blocks.
    for (int i = 0; i < steps; i++) {
        float yFront = i * stepDepth;
        float yBack = (i + 1) * stepDepth;
        float zBottom = 0; // Usually solid stairs go to ground? Or floating? Solid is better.
        float zTop = (i + 1) * stepHeight;
        
        // Vertices for this step
        QVector3D p1(-w2, yFront, zBottom);
        QVector3D p2( w2, yFront, zBottom);
        QVector3D p3( w2, yBack, zBottom);
        QVector3D p4(-w2, yBack, zBottom);
        
        QVector3D p5(-w2, yFront, zTop);
        QVector3D p6( w2, yFront, zTop);
        QVector3D p7( w2, yBack, zTop);
        QVector3D p8(-w2, yBack, zTop);
        
        // Front Face (Riser)
        addFace(p5, p6, p2, p1, QVector3D(0,-1,0), 0, 1);
        
        // Top Face (Tread)
        addFace(p8, p7, p6, p5, QVector3D(0,0,1), 0, 1);
        
        // Side Faces (Left/Right) - Triangle or Quad?
        // Left
        addFace(p8, p5, p1, p4, QVector3D(-1,0,0), 0, 1);
        // Right
        addFace(p6, p7, p3, p2, QVector3D(1,0,0), 0, 1);
        
        // Back Face only for last step?
        if (i == steps - 1) {
            addFace(p7, p8, p4, p3, QVector3D(0,1,0), 0, 1);
        }
        
        // Bottom Face only? 
        // addFace(p1, p2, p3, p4, QVector3D(0,0,-1), 0, 1);
    }
    
    // Bottom Face (Full length)
    int base = mesh.vertices.size();
    mesh.vertices.append({QVector3D(-w2, 0, 0), QVector3D(0,0,-1), QVector2D(0,0)});
    mesh.vertices.append({QVector3D( w2, 0, 0), QVector3D(0,0,-1), QVector2D(1,0)});
    mesh.vertices.append({QVector3D( w2, depth, 0), QVector3D(0,0,-1), QVector2D(1,1)});
    mesh.vertices.append({QVector3D(-w2, depth, 0), QVector3D(0,0,-1), QVector2D(0,1)});
    mesh.indices << base << base+2 << base+1;
    mesh.indices << base << base+3 << base+2;
    
    return mesh;
}

MD3Generator::MeshData MD3Generator::generateCylinder(float width, float height, float depth, int segments) {
    MeshData mesh;
    if (segments < 3) segments = 3;
    
    float radiusX = width / 2.0f;
    float radiusY = depth / 2.0f; // Elliptical cylinder support? Assume circle for now, or scaled.
    // If Depth is Y length, maybe it's a horizontal cylinder? 
    // Usually "Cylinder" implies vertical column.
    // So Width/Depth are diameter X/Y? 
    
    float h = height;
    
    // Top and Bottom vertices
    QVector<QVector3D> topVerts;
    QVector<QVector3D> botVerts;
    
    for (int i = 0; i <= segments; i++) {
        float angle = 2.0f * 3.14159f * i / segments;
        float x = cos(angle) * radiusX;
        float y = sin(angle) * radiusY;
        
        topVerts.append(QVector3D(x, y, h));
        botVerts.append(QVector3D(x, y, 0));
    }
    
    // Side Faces
    for (int i = 0; i < segments; i++) {
        QVector3D t1 = topVerts[i];
        QVector3D t2 = topVerts[i+1];
        QVector3D b1 = botVerts[i];
        QVector3D b2 = botVerts[i+1];
        
        QVector3D normal = (t1 - QVector3D(0,0,h)).normalized(); // Radial normal
        
        int base = mesh.vertices.size();
        float u1 = (float)i / segments;
        float u2 = (float)(i+1) / segments;
        
        mesh.vertices.append({t1, normal, QVector2D(u1, 0)}); // Top Left
        mesh.vertices.append({b1, normal, QVector2D(u1, 1)}); // Bot Left
        mesh.vertices.append({b2, normal, QVector2D(u2, 1)}); // Bot Right
        mesh.vertices.append({t2, normal, QVector2D(u2, 0)}); // Top Right
        
        mesh.indices << base << base+1 << base+2;
        mesh.indices << base << base+2 << base+3;
    }
    
    // Caps (Fan)
    // Top Cap
    QVector3D centerTop(0,0,h);
    int centerIdx = mesh.vertices.size();
    mesh.vertices.append({centerTop, QVector3D(0,0,1), QVector2D(0.5, 0.5)});
    
    for (int i = 0; i < segments; i++) {
        int base = mesh.vertices.size();
        // Simple planar UV mapping
        QVector2D uv1((topVerts[i].x()/width)+0.5, (topVerts[i].y()/depth)+0.5);
        QVector2D uv2((topVerts[i+1].x()/width)+0.5, (topVerts[i+1].y()/depth)+0.5);
        
        mesh.vertices.append({topVerts[i], QVector3D(0,0,1), uv1});
        mesh.vertices.append({topVerts[i+1], QVector3D(0,0,1), uv2});
        
        mesh.indices << centerIdx << base << base+1; 
    }
    
    // Bottom Cap
    // Not strictly needed for floor columns, but good for completeness.
    // ... Omitted for brevity unless needed.
    
    return mesh; 
}

// NEW GEOMETRY TYPES

MD3Generator::MeshData MD3Generator::generateBridge(float width, float height, float depth, bool hasRailings, bool hasArch)
{
    MeshData mesh;
    float w2 = width / 2.0f;
    float h = height; // Platform height
    float railHeight = 32.0f; // Railing height
    
    // Helper lambda with texture region support
    // textureIndex: 0 = surface (left half of atlas), 1 = walls/railings (right half)
    auto addFace = [&](QVector3D v1, QVector3D v2, QVector3D v3, QVector3D v4, QVector3D normal, int textureIndex = 0) {
        // Calculate UV offset based on texture index (for 2-texture horizontal atlas)
        float uOffset = (textureIndex == 0) ? 0.0f : 0.5f;
        float uScale = 0.5f; // Each texture occupies half the atlas width
        
        int base = mesh.vertices.size();
        mesh.vertices.append({v1, normal, QVector2D(uOffset + 0.0f * uScale, 0)});
        mesh.vertices.append({v2, normal, QVector2D(uOffset + 1.0f * uScale, 0)});
        mesh.vertices.append({v3, normal, QVector2D(uOffset + 1.0f * uScale, 1)});
        mesh.vertices.append({v4, normal, QVector2D(uOffset + 0.0f * uScale, 1)});
        mesh.indices << base << base+1 << base+2;
        mesh.indices << base << base+2 << base+3;
    };
    
    // Bridge platform (box)
    QVector3D p1(-w2, 0, 0);
    QVector3D p2( w2, 0, 0);
    QVector3D p3( w2, depth, 0);
    QVector3D p4(-w2, depth, 0);
    QVector3D p5(-w2, 0, h);
    QVector3D p6( w2, 0, h);
    QVector3D p7( w2, depth, h);
    QVector3D p8(-w2, depth, h);
    
    // Platform faces
    addFace(p8, p7, p6, p5, QVector3D(0,0,1), 0);  // Top (walkable surface)
    
    if (!hasArch) {
        // Solid bridge - add bottom and sides
        addFace(p1, p2, p3, p4, QVector3D(0,0,-1)); // Bottom
    }
    
    addFace(p5, p6, p2, p1, QVector3D(0,-1,0), 1); // Front
    addFace(p7, p8, p4, p3, QVector3D(0,1,0), 1);  // Back
    
    if (!hasArch) {
        // Solid bridge - full side walls (use texture 1 for walls)
        addFace(p8, p5, p1, p4, QVector3D(-1,0,0), 1); // Left wall
        addFace(p6, p7, p3, p2, QVector3D(1,0,0), 1);  // Right wall
    } else {
        // Bridge with arch tunnel underneath
        // The arch runs ALONG THE SIDES (X axis, from left to right)
        // Visible when looking from FRONT or BACK (Y direction)
        int archSegments = 16;
        float archRadius = (height * 0.65f);  // Arch height (65% of bridge height)
        float archWidth = width * 0.9f;       // Arch is 90% of bridge width
        float archHalfWidth = archWidth / 2.0f;
        
        // Generate the arch profile (semicircle in YZ plane)
        QVector<QVector3D> archProfile;
        for (int i = 0; i <= archSegments; i++) {
            float angle = 3.14159f * i / archSegments;
            float y = cos(angle) * (depth / 2.0f);  // Y from -depth/2 to +depth/2
            float z = sin(angle) * archRadius;       // Z from 0 to archRadius
            archProfile.append(QVector3D(0, y, z));
        }
        
        // Extrude arch along X axis to create HOLLOW tunnel from left to right
        for (int i = 0; i < archSegments; i++) {
            QVector3D p1 = archProfile[i];
            QVector3D p2 = archProfile[i+1];
            
            // Left side of tunnel (X = -archHalfWidth)
            QVector3D p1Left(-archHalfWidth, p1.y() + depth/2, p1.z());
            QVector3D p2Left(-archHalfWidth, p2.y() + depth/2, p2.z());
            
            // Right side of tunnel (X = +archHalfWidth)
            QVector3D p1Right(archHalfWidth, p1.y() + depth/2, p1.z());
            QVector3D p2Right(archHalfWidth, p2.y() + depth/2, p2.z());
            
            // Inner ceiling of arch (connects left and right)
            QVector3D edge1 = p2Left - p1Left;
            QVector3D edge2 = p1Right - p1Left;
            QVector3D normal = QVector3D::crossProduct(edge1, edge2).normalized();
            
            addFace(p1Left, p2Left, p2Right, p1Right, -normal, 1);
        }
        
        // Side walls ABOVE the arch (from arch top to platform top)
        // Left wall (X = -w2 to X = -archHalfWidth)
        if (archHalfWidth < w2) {
            float wallThickness = w2 - archHalfWidth;
            
            // Left outer wall (X = -w2)
            addFace(QVector3D(-w2, 0, archRadius), QVector3D(-w2, depth, archRadius),
                    QVector3D(-w2, depth, h), QVector3D(-w2, 0, h), QVector3D(-1,0,0), 1);
            
            // Left inner wall (X = -archHalfWidth) - connects to arch
            addFace(QVector3D(-archHalfWidth, 0, h), QVector3D(-archHalfWidth, depth, h),
                    QVector3D(-archHalfWidth, depth, archRadius), QVector3D(-archHalfWidth, 0, archRadius), QVector3D(1,0,0), 1);
            
            // Left wall top (connects outer to inner)
            addFace(QVector3D(-w2, 0, h), QVector3D(-w2, depth, h),
                    QVector3D(-archHalfWidth, depth, h), QVector3D(-archHalfWidth, 0, h), QVector3D(0,0,1), 0);
            
            // Left wall BOTTOM (fills gap between bridge edge and arch)
            addFace(QVector3D(-archHalfWidth, 0, 0), QVector3D(-archHalfWidth, depth, 0),
                    QVector3D(-w2, depth, 0), QVector3D(-w2, 0, 0), QVector3D(0,0,-1), 1);
            
            // Left wall FRONT face (Y=0, connects arch to wall)
            addFace(QVector3D(-w2, 0, 0), QVector3D(-archHalfWidth, 0, 0),
                    QVector3D(-archHalfWidth, 0, archRadius), QVector3D(-w2, 0, archRadius), QVector3D(0,-1,0), 1);
            
            // Left wall BACK face (Y=depth, connects arch to wall)
            addFace(QVector3D(-archHalfWidth, depth, 0), QVector3D(-w2, depth, 0),
                    QVector3D(-w2, depth, archRadius), QVector3D(-archHalfWidth, depth, archRadius), QVector3D(0,1,0), 1);
            
            // Right outer wall (X = w2)
            addFace(QVector3D(w2, 0, h), QVector3D(w2, depth, h),
                    QVector3D(w2, depth, archRadius), QVector3D(w2, 0, archRadius), QVector3D(1,0,0), 1);
            
            // Right inner wall (X = archHalfWidth) - connects to arch
            addFace(QVector3D(archHalfWidth, 0, archRadius), QVector3D(archHalfWidth, depth, archRadius),
                    QVector3D(archHalfWidth, depth, h), QVector3D(archHalfWidth, 0, h), QVector3D(-1,0,0), 1);
            
            // Right wall top (connects outer to inner)
            addFace(QVector3D(archHalfWidth, 0, h), QVector3D(archHalfWidth, depth, h),
                    QVector3D(w2, depth, h), QVector3D(w2, 0, h), QVector3D(0,0,1), 0);
            
            // Right wall BOTTOM (fills gap between bridge edge and arch)
            addFace(QVector3D(w2, 0, 0), QVector3D(w2, depth, 0),
                    QVector3D(archHalfWidth, depth, 0), QVector3D(archHalfWidth, 0, 0), QVector3D(0,0,-1), 1);
            
            // Right wall FRONT face (Y=0, connects arch to wall)
            addFace(QVector3D(archHalfWidth, 0, 0), QVector3D(w2, 0, 0),
                    QVector3D(w2, 0, archRadius), QVector3D(archHalfWidth, 0, archRadius), QVector3D(0,-1,0), 1);
            
            // Right wall BACK face (Y=depth, connects arch to wall)
            addFace(QVector3D(w2, depth, 0), QVector3D(archHalfWidth, depth, 0),
                    QVector3D(archHalfWidth, depth, archRadius), QVector3D(w2, depth, archRadius), QVector3D(0,1,0), 1);
        }
        
        // Front and back walls (full height)
        addFace(QVector3D(-w2, 0, 0), QVector3D(w2, 0, 0),
                QVector3D(w2, 0, h), QVector3D(-w2, 0, h), QVector3D(0,-1,0), 1);
        
        addFace(QVector3D(w2, depth, 0), QVector3D(-w2, depth, 0),
                QVector3D(-w2, depth, h), QVector3D(w2, depth, h), QVector3D(0,1,0), 1);
    }
    
    // Railings (if enabled) - use texture 1 (railings/walls)
    if (hasRailings) {
        float postWidth = 4.0f;
        int numPosts = 5;
        
        for (int side = 0; side < 2; side++) {
            float x = (side == 0) ? -w2 : w2;
            
            for (int i = 0; i < numPosts; i++) {
                float y = (depth / (numPosts - 1)) * i;
                
                QVector3D pb1(x - postWidth/2, y - postWidth/2, h);
                QVector3D pb2(x + postWidth/2, y - postWidth/2, h);
                QVector3D pb3(x + postWidth/2, y + postWidth/2, h);
                QVector3D pb4(x - postWidth/2, y + postWidth/2, h);
                QVector3D pt1(x - postWidth/2, y - postWidth/2, h + railHeight);
                QVector3D pt2(x + postWidth/2, y - postWidth/2, h + railHeight);
                QVector3D pt3(x + postWidth/2, y + postWidth/2, h + railHeight);
                QVector3D pt4(x - postWidth/2, y + postWidth/2, h + railHeight);
                
                // All railing faces use texture 1
                addFace(pt1, pt2, pb2, pb1, QVector3D(0,-1,0), 1);
                addFace(pt2, pt3, pb3, pb2, QVector3D(1,0,0), 1);
                addFace(pt3, pt4, pb4, pb3, QVector3D(0,1,0), 1);
                addFace(pt4, pt1, pb1, pb4, QVector3D(-1,0,0), 1);
            }
        }
    }
    
    return mesh;
}

MD3Generator::MeshData MD3Generator::generateHouse(float width, float height, float depth, int roofType)
{
    MeshData mesh;
    float w2 = width / 2.0f;
    float wallHeight = height;
    float roofHeight = height * 0.4f; // Roof adds 40% more height
    
    auto addFace = [&](QVector3D v1, QVector3D v2, QVector3D v3, QVector3D v4, QVector3D normal) {
        int base = mesh.vertices.size();
        mesh.vertices.append({v1, normal, QVector2D(0,0)});
        mesh.vertices.append({v2, normal, QVector2D(1,0)});
        mesh.vertices.append({v3, normal, QVector2D(1,1)});
        mesh.vertices.append({v4, normal, QVector2D(0,1)});
        mesh.indices << base << base+1 << base+2;
        mesh.indices << base << base+2 << base+3;
    };
    
    // Walls (box)
    QVector3D w1(-w2, 0, 0);
    QVector3D w2v( w2, 0, 0);
    QVector3D w3( w2, depth, 0);
    QVector3D w4(-w2, depth, 0);
    QVector3D w5(-w2, 0, wallHeight);
    QVector3D w6( w2, 0, wallHeight);
    QVector3D w7( w2, depth, wallHeight);
    QVector3D w8(-w2, depth, wallHeight);
    
    // Wall faces
    addFace(w5, w6, w2v, w1, QVector3D(0,-1,0)); // Front wall
    addFace(w7, w8, w4, w3, QVector3D(0,1,0));   // Back wall
    addFace(w8, w5, w1, w4, QVector3D(-1,0,0));  // Left wall
    addFace(w6, w7, w3, w2v, QVector3D(1,0,0));  // Right wall
    
    // Floor
    addFace(w1, w2v, w3, w4, QVector3D(0,0,-1));
    
    // Roof
    if (roofType == 0) { // FLAT
        addFace(w8, w7, w6, w5, QVector3D(0,0,1));
    } else if (roofType == 1) { // SLOPED (one direction)
        QVector3D r1(-w2, 0, wallHeight);
        QVector3D r2( w2, 0, wallHeight);
        QVector3D r3( w2, depth, wallHeight + roofHeight);
        QVector3D r4(-w2, depth, wallHeight + roofHeight);
        
        // Sloped top
        QVector3D slopeN = QVector3D::crossProduct(r2 - r1, r4 - r1).normalized();
        addFace(r4, r3, r2, r1, slopeN);
        
        // Triangular ends
        int base = mesh.vertices.size();
        mesh.vertices.append({w5, QVector3D(0,-1,0), QVector2D(0,0)});
        mesh.vertices.append({w6, QVector3D(0,-1,0), QVector2D(1,0)});
        mesh.vertices.append({r1, QVector3D(0,-1,0), QVector2D(0.5,1)});
        mesh.indices << base << base+1 << base+2;
        
        base = mesh.vertices.size();
        mesh.vertices.append({w7, QVector3D(0,1,0), QVector2D(1,0)});
        mesh.vertices.append({w8, QVector3D(0,1,0), QVector2D(0,0)});
        mesh.vertices.append({r4, QVector3D(0,1,0), QVector2D(0.5,1)});
        mesh.indices << base << base+1 << base+2;
        
    } else { // GABLED (two slopes meeting at peak)
        QVector3D peak1(0, 0, wallHeight + roofHeight);
        QVector3D peak2(0, depth, wallHeight + roofHeight);
        
        // Left slope
        QVector3D ln = QVector3D::crossProduct(peak1 - w5, peak2 - w5).normalized();
        addFace(peak2, peak1, w5, w8, ln);
        
        // Right slope
        QVector3D rn = QVector3D::crossProduct(w6 - peak1, peak2 - peak1).normalized();
        addFace(peak2, w7, w6, peak1, rn);
        
        // Triangular gable ends
        int base = mesh.vertices.size();
        mesh.vertices.append({w5, QVector3D(0,-1,0), QVector2D(0,0)});
        mesh.vertices.append({w6, QVector3D(0,-1,0), QVector2D(1,0)});
        mesh.vertices.append({peak1, QVector3D(0,-1,0), QVector2D(0.5,1)});
        mesh.indices << base << base+1 << base+2;
        
        base = mesh.vertices.size();
        mesh.vertices.append({w7, QVector3D(0,1,0), QVector2D(1,0)});
        mesh.vertices.append({w8, QVector3D(0,1,0), QVector2D(0,0)});
        mesh.vertices.append({peak2, QVector3D(0,1,0), QVector2D(0.5,1)});
        mesh.indices << base << base+1 << base+2;
    }
    
    return mesh;
}

MD3Generator::MeshData MD3Generator::generateArch(float width, float height, float depth, int segments)
{
    MeshData mesh;
    if (segments < 4) segments = 4;
    
    float w2 = width / 2.0f;
    float archRadius = width / 2.0f;
    float pillarWidth = width * 0.15f;
    float archThickness = depth;
    
    auto addFace = [&](QVector3D v1, QVector3D v2, QVector3D v3, QVector3D v4, QVector3D normal) {
        int base = mesh.vertices.size();
        mesh.vertices.append({v1, normal, QVector2D(0,0)});
        mesh.vertices.append({v2, normal, QVector2D(1,0)});
        mesh.vertices.append({v3, normal, QVector2D(1,1)});
        mesh.vertices.append({v4, normal, QVector2D(0,1)});
        mesh.indices << base << base+1 << base+2;
        mesh.indices << base << base+2 << base+3;
    };
    
    // Left pillar
    addFace(QVector3D(-w2, 0, height), QVector3D(-w2 + pillarWidth, 0, height),
            QVector3D(-w2 + pillarWidth, 0, 0), QVector3D(-w2, 0, 0), QVector3D(0,-1,0));
    addFace(QVector3D(-w2, archThickness, 0), QVector3D(-w2 + pillarWidth, archThickness, 0),
            QVector3D(-w2 + pillarWidth, archThickness, height), QVector3D(-w2, archThickness, height), QVector3D(0,1,0));
    
    // Right pillar
    addFace(QVector3D(w2 - pillarWidth, 0, height), QVector3D(w2, 0, height),
            QVector3D(w2, 0, 0), QVector3D(w2 - pillarWidth, 0, 0), QVector3D(0,-1,0));
    addFace(QVector3D(w2 - pillarWidth, archThickness, 0), QVector3D(w2, archThickness, 0),
            QVector3D(w2, archThickness, height), QVector3D(w2 - pillarWidth, archThickness, height), QVector3D(0,1,0));
    
    // Arch curve (semicircle)
    QVector<QVector3D> innerCurve, outerCurve;
    for (int i = 0; i <= segments; i++) {
        float angle = 3.14159f * i / segments; // 0 to PI
        float x = cos(angle) * archRadius;
        float z = sin(angle) * archRadius + height;
        
        innerCurve.append(QVector3D(x, 0, z));
        outerCurve.append(QVector3D(x, archThickness, z));
    }
    
    // Arch faces
    for (int i = 0; i < segments; i++) {
        // Front face
        addFace(innerCurve[i+1], innerCurve[i],
                QVector3D(innerCurve[i].x(), 0, height),
                QVector3D(innerCurve[i+1].x(), 0, height), QVector3D(0,-1,0));
        
        // Back face
        addFace(outerCurve[i], outerCurve[i+1],
                QVector3D(outerCurve[i+1].x(), archThickness, height),
                QVector3D(outerCurve[i].x(), archThickness, height), QVector3D(0,1,0));
    }
    
    return mesh;
}


bool MD3Generator::saveMD3(const MeshData &mesh, const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        qCritical() << "Cannot write to " << filename;
        return false;
    }
    
    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);
    
    // Layout:
    // Header
    // Frames (1)
    // Tags (0)
    // Surface (1)
    
    int num_frames = 1;
    int num_surfaces = 1;
    int num_tags = 0;
    int num_skins = 0; // Defines shader in surface
    
    int ofs_frames = sizeof(MD3::Header);
    int ofs_tags = ofs_frames + num_frames * sizeof(MD3::Frame);
    int ofs_surfaces = ofs_tags; // num_tags is 0, so no offset increase
    // Actually MD3 tag struct size is 64+12+9*4 = 112 bytes?
    // Let's assume 0 tags.
    
    // Header
    MD3::Header header;
    memcpy(header.ident, "IDP3", 4);
    header.version = 15;
    strncpy(header.name, mesh.name.toLatin1().constData(), 63);
    header.flags = 0;
    header.num_frames = num_frames;
    header.num_tags = num_tags;
    header.num_surfaces = num_surfaces;
    header.num_skins = num_skins; // Global skins?
    header.ofs_frames = ofs_frames;
    header.ofs_tags = ofs_tags; // 0 tags -> same offset? No, typically offset points to start of tags.
    header.ofs_surfaces = ofs_tags; // No tags
    header.ofs_eof = 0; // Fill later
    
    out.writeRawData(reinterpret_cast<const char*>(&header), sizeof(MD3::Header));
    
    // Frame (1)
    MD3::Frame frame;
    frame.min_bounds = QVector3D(-100,-100,-100); // Bounds
    frame.max_bounds = QVector3D(100,100,100);
    frame.local_origin = QVector3D(0,0,0);
    frame.radius = 100.0f;
    memset(frame.name, 0, 16);
    strcpy(frame.name, "default");
    
    out.writeRawData(reinterpret_cast<const char*>(&frame), sizeof(MD3::Frame));
    
    // Tags (0)
    
    // Surface
    long surfaceStart = file.pos();
    
    int num_verts = mesh.vertices.size();
    int num_tris = mesh.indices.size() / 3;
    int num_shaders = 1;

    // Calculate Surface Offsets (relative to surface start)
    int surfHeaderSize = sizeof(MD3::Surface); // 
    
    MD3::Surface surface;
    memcpy(surface.ident, "IDP3", 4);
    memset(surface.name, 0, 64);
    strcpy(surface.name, "mesh");
    surface.flags = 0;
    surface.num_frames = num_frames;
    surface.num_shaders = num_shaders;
    surface.num_verts = num_verts;
    surface.num_triangles = num_tris;
    
    surface.ofs_shaders = surfHeaderSize;
    surface.ofs_triangles = surface.ofs_shaders + num_shaders * sizeof(MD3::Shader);
    surface.ofs_st = surface.ofs_triangles + num_tris * sizeof(MD3::Triangle);
    surface.ofs_xyznormal = surface.ofs_st + num_verts * sizeof(MD3::TexCoord);
    surface.ofs_end = surface.ofs_xyznormal + num_verts * sizeof(MD3::Vertex); // For 1 frame
    
    out.writeRawData(reinterpret_cast<const char*>(&surface), sizeof(MD3::Surface));
    
    // Shaders
    MD3::Shader shader;
    memset(shader.name, 0, 64);
    strncpy(shader.name, mesh.textureName.toLatin1().constData(), 63);
    shader.shader_index = 0;
    out.writeRawData(reinterpret_cast<const char*>(&shader), sizeof(MD3::Shader));
    
    // Triangles
    for (int i = 0; i < num_tris; i++) {
        MD3::Triangle tri;
        tri.indexes[0] = mesh.indices[i*3+0];
        tri.indexes[1] = mesh.indices[i*3+1]; // MD3 winding might be different (CW vs CCW). 
        // OpenGL is CCW. MD3 is usually CW? 
        // Build Engine usually likes things a specific way. Let's try default.
        tri.indexes[2] = mesh.indices[i*3+2];
        out.writeRawData(reinterpret_cast<const char*>(&tri), sizeof(MD3::Triangle));
    }
    
    // TexCoords (ST)
    for (int i = 0; i < num_verts; i++) {
        MD3::TexCoord st;
        st.st[0] = mesh.vertices[i].uv.x();
        st.st[1] = mesh.vertices[i].uv.y(); // MD3 V is usually flipped? or 1-V? 
        // 1-V often needed.
        out.writeRawData(reinterpret_cast<const char*>(&st), sizeof(MD3::TexCoord));
    }
    
    // Vertices (XYZ + Normal)
    for (int i = 0; i < num_verts; i++) {
        MD3::Vertex v;
        // XYZ is scaled by 64
        v.coord[0] = (int16_t)(mesh.vertices[i].pos.x() * 64.0f);
        v.coord[1] = (int16_t)(mesh.vertices[i].pos.y() * 64.0f);
        v.coord[2] = (int16_t)(mesh.vertices[i].pos.z() * 64.0f);
        
        encodeNormal(mesh.vertices[i].normal, v.normal);
        
        out.writeRawData(reinterpret_cast<const char*>(&v), sizeof(MD3::Vertex));
    }
    
    // Update Header EOF
    long endPos = file.pos();
    file.seek(offsetof(MD3::Header, ofs_eof));
    int32_t eof = (int32_t)endPos;
    out.writeRawData(reinterpret_cast<const char*>(&eof), sizeof(int32_t));
    
    file.close();
    return true;
}
