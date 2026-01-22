#include "fpgloader.h"
#include <QFile>
#include <QDataStream>
#include <QDebug>
#include <QMessageBox>
#include <zlib.h>

FPGLoader::FPGLoader()
{
}

bool FPGLoader::loadFPG(const QString &filename, QVector<TextureEntry> &textures,
                        std::function<void(int, int, const QString&)> progressCallback)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "No se pudo abrir el archivo .fpg:" << filename;
        return false;
    }

    // Leer header principal (8 bytes)
    QByteArray headerBytes = file.read(8);
    qDebug() << "Header leído (hex):" << headerBytes.toHex();

    QByteArray uncompressedData;

    // Detectar gzip
    if (headerBytes.startsWith(QByteArray::fromHex("1f8b"))) {
        qDebug() << "Archivo FPG comprimido con gzip detectado";

        // Volver al inicio y leer todo para descompresión
        file.seek(0);
        QByteArray compressedData = file.readAll();

        // Descompresión robusta con buffer dinámico
        z_stream strm;
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;
        strm.avail_in = compressedData.size();
        strm.next_in = (Bytef*)compressedData.data();

        // Inicializar inflate con modo gzip (+16)
        int ret = inflateInit2(&strm, 15 + 16);
        if (ret != Z_OK) {
            qDebug() << "inflateInit2 failed:" << ret;
            qWarning() << "Fallo al inicializar descompresión gzip";
            return false;
        }

        // Buffer dinámico que crece según necesidad
        QByteArray buffer;
        buffer.resize(4096); // Tamaño inicial

        do {
            strm.avail_out = buffer.size();
            strm.next_out = (Bytef*)buffer.data();

            ret = inflate(&strm, Z_NO_FLUSH);

            if (ret == Z_STREAM_ERROR) {
                qDebug() << "inflate error:" << ret;
                inflateEnd(&strm);
                qWarning() << "Error durante descompresión gzip";
                return false;
            }

            // Calcular bytes descomprimidos en esta iteración
            size_t have = buffer.size() - strm.avail_out;
            uncompressedData.append(buffer.data(), have);

            // Si necesitamos más espacio, agrandar buffer
            if (strm.avail_out == 0) {
                buffer.resize(buffer.size() * 2);
            }

        } while (ret != Z_STREAM_END);

        inflateEnd(&strm);
        qDebug() << "Descompresión exitosa - Bytes descomprimidos:" << uncompressedData.size();

    } else {
        // Archivo sin comprimir
        file.seek(0);
        uncompressedData = file.readAll();
        qDebug() << "Archivo FPG sin comprimir detectado";
    }

    file.close();

    // Validar magic number del formato FPG (case-insensitive)
    if (uncompressedData.size() < 8) {
        qWarning() << "Archivo FPG demasiado pequeño";
        return false;
    }

    QString magicOriginal = QString::fromLatin1(uncompressedData.left(7));
    QString magicBase = magicOriginal.left(3).toUpper();

    if (magicBase != "F32") {
        qWarning() << QString("Formato .fpg inválido - se esperaba 'F32*', se encontró '%1'")
                          .arg(magicOriginal);
        return false;
    }

    qDebug() << "Magic number original:" << magicOriginal;
    qDebug() << "Magic number validado correctamente";

    // Configurar QDataStream con endianness correcto
    QDataStream in(uncompressedData);
    in.setByteOrder(QDataStream::LittleEndian);  // ¡CRUCIAL! BennuGD2 usa little-endian
    in.setFloatingPointPrecision(QDataStream::SinglePrecision);

    // Saltar header (8 bytes)
    in.skipRawData(8);

    // Procesar chunks del archivo FPG
    int chunkCount = 0;
    textures.clear();

    qDebug() << "Iniciando lectura de chunks...";

    while (!in.atEnd()) {
        // Check if we have enough bytes left for a chunk header (68 bytes minimum)
        if (in.device()->bytesAvailable() < 68) {
            qDebug() << "No hay suficientes bytes para otro chunk, finalizando lectura";
            break;
        }
        
        // Leer estructura FPG_info usando operadores >> que respetan endianness
        FPG_CHUNK chunk;
        in >> chunk.code >> chunk.regsize;
        in.readRawData(chunk.name, 32);
        in.readRawData(chunk.filename, 12);
        in >> chunk.width >> chunk.height >> chunk.flags;

        if (in.status() != QDataStream::Ok) {
            qDebug() << "Error leyendo chunk header";
            break;
        }

        qDebug() << QString("Chunk %1 : código=%2, tamaño=%3x%4, flags=%5")
                        .arg(chunkCount + 1)
                        .arg(chunk.code)
                        .arg(chunk.width)
                        .arg(chunk.height)
                        .arg(chunk.flags);

        // Validar dimensiones del chunk
        if (chunk.width <= 0 || chunk.height <= 0 ||
            chunk.width > 4096 || chunk.height > 4096) {
            qDebug() << "Chunk con dimensiones inválidas, saltando";
            continue;
        }
        
        // Validar que el código sea razonable (evitar IDs basura como 0xFFFFFFFF)
        if (chunk.code < 0 || chunk.code > 100000) {
            qDebug() << "Chunk con código inválido:" << chunk.code << ", finalizando lectura";
            break;
        }

        // Leer control points si flags > 0
        if (chunk.flags > 0) {
            int numPoints = chunk.flags;
            for (int i = 0; i < numPoints; i++) {
                int16_t x, y;
                in >> x >> y;
            }
        }

        // Calcular tamaño de datos de píxeles según formato
        int pixelDataSize = chunk.width * chunk.height * 4; // 32 bits RGBA

        // Leer datos de píxeles
        char* pixelBuffer = new char[pixelDataSize];
        int bytesRead = in.readRawData(pixelBuffer, pixelDataSize);

        if (bytesRead != pixelDataSize) {
            qDebug() << "Error: no se pudieron leer todos los bytes del chunk";
            delete[] pixelBuffer;
            break;
        }

        // Convertir de BGRA a RGBA manualmente para corregir colores
        for (int i = 0; i < pixelDataSize; i += 4) {
            char temp = pixelBuffer[i];
            pixelBuffer[i] = pixelBuffer[i + 2];   // R = B
            pixelBuffer[i + 2] = temp;             // B = R
            // G y A permanecen igual
        }

        // Crear QImage desde el buffer corregido
        QImage image(reinterpret_cast<const uchar*>(pixelBuffer),
                     chunk.width, chunk.height, QImage::Format_RGBA8888);

        if (!image.isNull()) {
            // Crear QPixmap y añadir a textures
            QPixmap pixmap = QPixmap::fromImage(image);
            TextureEntry tex(filename, chunk.code);
            tex.pixmap = pixmap;
            textures.append(tex);
            qDebug() << "Textura" << chunk.code << "cargada exitosamente";
            
            // Reportar progreso si hay callback
            if (progressCallback) {
                QString texName = QString("Textura %1").arg(chunk.code);
                progressCallback(chunkCount, -1, texName); // -1 = total desconocido aún
            }
        } else {
            qWarning() << "Error al crear QImage para chunk" << chunk.code;
        }

        delete[] pixelBuffer;
        chunkCount++;

        // Límite de seguridad para evitar bucles infinitos
        if (chunkCount > 1000) {
            qDebug() << "Alcanzado límite máximo de chunks";
            break;
        }
    }

    qDebug() << "Procesados" << chunkCount << "chunks";
    qDebug() << "Texturas FPG cargadas:" << textures.size();

    return textures.size() > 0;
}

bool FPGLoader::saveFPG(const QString &filename, const QVector<TextureEntry> &textures, bool compress)
{
    if (textures.isEmpty()) {
        qWarning() << "No textures to save";
        return false;
    }
    
    QByteArray fpgData;
    QDataStream out(&fpgData, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    out.setFloatingPointPrecision(QDataStream::SinglePrecision);
    
    // Write FPG header correctly "f32\x1A\x0D\x0A\x00\x00" (8 bytes)
    // Standard BennuGD header structure for 32-bit FPG
    const char header[8] = {'f', '3', '2', '\x1A', '\x0D', '\x0A', '\x00', '\x00'};
    out.writeRawData(header, 8);
    
    qDebug() << "Writing FPG with" << textures.size() << "textures";
    
    // Write each texture as a chunk
    for (const TextureEntry &tex : textures) {
        if (tex.pixmap.isNull()) continue;
        
        // Convert pixmap to image
        QImage image = tex.pixmap.toImage().convertToFormat(QImage::Format_RGBA8888);
        
        // Calculate data size
        int pixelDataSize = image.width() * image.height() * 4;
        
        // Write chunk header
        FPG_CHUNK chunk;
        chunk.code = tex.id;
        
        // Regsize: size of data + struct remaining fields (starting from name)
        // name(32) + filename(12) + width(4) + height(4) + flags(4) = 56 bytes
        // + control points (flags * 4) (0 in this case)
        // + pixel data
        chunk.regsize = 56 + pixelDataSize;
        
        // Fill name and filename
        memset(chunk.name, 0, 32);
        memset(chunk.filename, 0, 12);
        
        QString baseName = tex.filename;
        if (baseName.length() > 31) baseName = baseName.left(31);
        strncpy(chunk.name, baseName.toLatin1().constData(), 31);
        
        QString shortName = baseName;
        if (shortName.length() > 11) shortName = shortName.left(11);
        strncpy(chunk.filename, shortName.toLatin1().constData(), 11);
        
        chunk.width = image.width();
        chunk.height = image.height();
        chunk.flags = 0; // No control points
        
        qDebug() << "Writing texture" << chunk.code << "size:" << chunk.width << "x" << chunk.height << "regsize:" << chunk.regsize;
        
        // Write chunk header - ensure we write int32 values
        qint32 code32 = chunk.code;
        qint32 regsize32 = chunk.regsize;
        qint32 width32 = chunk.width;
        qint32 height32 = chunk.height;
        qint32 flags32 = chunk.flags;
        
        out << code32 << regsize32;
        out.writeRawData(chunk.name, 32);
        out.writeRawData(chunk.filename, 12);
        out << width32 << height32 << flags32;
        
        // Convert RGBA to BGRA for FPG format
        // pixelDataSize already calculated above
        char* pixelBuffer = new char[pixelDataSize];
        memcpy(pixelBuffer, image.constBits(), pixelDataSize);
        
        // Swap R and B channels
        for (int i = 0; i < pixelDataSize; i += 4) {
            char temp = pixelBuffer[i];
            pixelBuffer[i] = pixelBuffer[i + 2];   // R = B
            pixelBuffer[i + 2] = temp;             // B = R
        }
        
        // Write pixel data
        out.writeRawData(pixelBuffer, pixelDataSize);
        delete[] pixelBuffer;
    }
    
    // Write to file
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open file for writing:" << filename;
        return false;
    }
    
    if (compress) {
        // Compress with gzip
        z_stream strm;
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;
        
        int ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);
        if (ret != Z_OK) {
            qWarning() << "Failed to initialize compression";
            file.close();
            return false;
        }
        
        strm.avail_in = fpgData.size();
        strm.next_in = (Bytef*)fpgData.data();
        
        // Allocate buffer large enough for compressed data
        // deflateBound gives upper bound on compressed size
        uLong compressedBound = deflateBound(&strm, fpgData.size());
        QByteArray compressedData;
        compressedData.resize(compressedBound);
        
        strm.avail_out = compressedData.size();
        strm.next_out = (Bytef*)compressedData.data();
        
        ret = deflate(&strm, Z_FINISH);
        if (ret != Z_STREAM_END) {
            qWarning() << "Compression failed, ret =" << ret;
            deflateEnd(&strm);
            file.close();
            return false;
        }
        
        uLong compressedSize = compressedData.size() - strm.avail_out;
        deflateEnd(&strm);
        
        qDebug() << "Compressed" << fpgData.size() << "bytes to" << compressedSize << "bytes";
        
        file.write(compressedData.data(), compressedSize);
    } else {
        // Write uncompressed
        file.write(fpgData);
    }
    
    file.close();
    qDebug() << "FPG saved successfully:" << filename << "(" << textures.size() << "textures)";
    return true;
}

QMap<int, QPixmap> FPGLoader::getTextureMap(const QVector<TextureEntry> &textures)
{
    QMap<int, QPixmap> textureMap;
    for (const TextureEntry &tex : textures) {
        textureMap[tex.id] = tex.pixmap;
    }
    return textureMap;
}
