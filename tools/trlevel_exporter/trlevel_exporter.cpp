#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <QApplication>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QOpenGLWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace fs = std::filesystem;

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Material {
    std::string name;
    std::array<float, 3> diffuse{1.0f, 1.0f, 1.0f};
    float alpha = 1.0f;
    fs::path texture_path;
    bool has_texture = false;
    std::vector<std::uint8_t> embedded_texture_bytes;
    std::string embedded_texture_mime_type;
    std::string embedded_texture_name;
};

struct ObjVertexRef {
    int v = -1;
    int vt = -1;
    int vn = -1;
};

struct Triangle {
    std::array<ObjVertexRef, 3> refs;
    std::string material_name;
};

struct ObjScene {
    std::vector<Vec3> positions;
    std::vector<Vec2> texcoords;
    std::vector<Vec3> normals;
    std::vector<Triangle> triangles;
    std::map<std::string, Material> materials;
};

struct BufferViewDef {
    uint32_t byte_offset = 0;
    uint32_t byte_length = 0;
    std::optional<uint32_t> target;
};

struct AccessorDef {
    uint32_t buffer_view = 0;
    uint32_t component_type = 0;
    uint32_t count = 0;
    std::string type;
    std::optional<std::array<float, 3>> min_vec3;
    std::optional<std::array<float, 3>> max_vec3;
};

struct ImageDef {
    std::optional<uint32_t> buffer_view;
    std::string mime_type;
    std::string name;
    std::optional<std::string> uri;
};

struct TextureDef {
    uint32_t source = 0;
};

struct PrimitiveDef {
    uint32_t position_accessor = 0;
    uint32_t normal_accessor = 0;
    uint32_t uv_accessor = 0;
    uint32_t index_accessor = 0;
    uint32_t material_index = 0;
};

struct MaterialDef {
    std::string name;
    std::array<float, 4> base_color_factor{1.0f, 1.0f, 1.0f, 1.0f};
    std::optional<uint32_t> base_color_texture;
    bool alpha_blend = false;
};

struct CliOptions {
    std::string command;
    fs::path input_obj;
    fs::path output;
    fs::path output_dir;
    double sector_size = 0.0;
    double export_scale = 0.001;
    std::string manifest_name = "manifest.json";
};

static constexpr bool kTraceTrParser = false;

static std::string trim(const std::string& value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

static std::vector<std::string> split_ws(const std::string& line) {
    std::istringstream input(line);
    std::vector<std::string> parts;
    std::string token;
    while (input >> token) {
        parts.push_back(token);
    }
    return parts;
}

static int normalize_index(int index, int count) {
    if (index > 0) {
        return index - 1;
    }
    if (index < 0) {
        return count + index;
    }
    throw std::runtime_error("Indice OBJ 0 invalido");
}

static ObjVertexRef parse_vertex_ref(const std::string& token, const ObjScene& scene) {
    ObjVertexRef ref;
    std::vector<std::string> parts;
    std::string current;
    for (char ch : token) {
        if (ch == '/') {
            parts.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    parts.push_back(current);

    if (parts.empty() || parts[0].empty()) {
        throw std::runtime_error("Referencia de vertice OBJ invalida");
    }

    ref.v = normalize_index(std::stoi(parts[0]), static_cast<int>(scene.positions.size()));
    if (parts.size() > 1 && !parts[1].empty()) {
        ref.vt = normalize_index(std::stoi(parts[1]), static_cast<int>(scene.texcoords.size()));
    }
    if (parts.size() > 2 && !parts[2].empty()) {
        ref.vn = normalize_index(std::stoi(parts[2]), static_cast<int>(scene.normals.size()));
    }
    return ref;
}

static std::string join_tail(const std::vector<std::string>& parts, std::size_t begin) {
    std::ostringstream output;
    for (std::size_t i = begin; i < parts.size(); ++i) {
        if (i > begin) {
            output << ' ';
        }
        output << parts[i];
    }
    return output.str();
}

static void parse_mtl(const fs::path& path, std::map<std::string, Material>& materials) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("No se pudo abrir el MTL: " + path.string());
    }

    Material* current = nullptr;
    std::string raw_line;
    while (std::getline(input, raw_line)) {
        const std::string line = trim(raw_line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        const auto parts = split_ws(line);
        if (parts.empty()) {
            continue;
        }

        if (parts[0] == "newmtl" && parts.size() >= 2) {
            Material material;
            material.name = join_tail(parts, 1);
            current = &materials[material.name];
            *current = material;
        } else if (!current) {
            continue;
        } else if (parts[0] == "Kd" && parts.size() >= 4) {
            current->diffuse = {std::stof(parts[1]), std::stof(parts[2]), std::stof(parts[3])};
        } else if ((parts[0] == "d" || parts[0] == "Tr") && parts.size() >= 2) {
            const float value = std::stof(parts[1]);
            current->alpha = parts[0] == "Tr" ? (1.0f - value) : value;
        } else if (parts[0] == "map_Kd" && parts.size() >= 2) {
            current->texture_path = fs::absolute(path.parent_path() / join_tail(parts, 1));
            current->has_texture = true;
        }
    }
}

static ObjScene parse_obj(const fs::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("No se pudo abrir el OBJ: " + path.string());
    }

    ObjScene scene;
    std::string current_material;
    std::vector<fs::path> mtllibs;
    std::string raw_line;

    while (std::getline(input, raw_line)) {
        const std::string line = trim(raw_line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        const auto parts = split_ws(line);
        if (parts.empty()) {
            continue;
        }

        if (parts[0] == "v" && parts.size() >= 4) {
            scene.positions.push_back({std::stof(parts[1]), std::stof(parts[2]), std::stof(parts[3])});
        } else if (parts[0] == "vt" && parts.size() >= 3) {
            scene.texcoords.push_back({std::stof(parts[1]), 1.0f - std::stof(parts[2])});
        } else if (parts[0] == "vn" && parts.size() >= 4) {
            scene.normals.push_back({std::stof(parts[1]), std::stof(parts[2]), std::stof(parts[3])});
        } else if (parts[0] == "usemtl" && parts.size() >= 2) {
            current_material = join_tail(parts, 1);
        } else if (parts[0] == "mtllib" && parts.size() >= 2) {
            mtllibs.push_back(fs::absolute(path.parent_path() / join_tail(parts, 1)));
        } else if (parts[0] == "f" && parts.size() >= 4) {
            std::vector<ObjVertexRef> refs;
            refs.reserve(parts.size() - 1);
            for (std::size_t i = 1; i < parts.size(); ++i) {
                refs.push_back(parse_vertex_ref(parts[i], scene));
            }
            for (std::size_t i = 1; i + 1 < refs.size(); ++i) {
                scene.triangles.push_back({{refs[0], refs[i], refs[i + 1]}, current_material});
            }
        }
    }

    for (const auto& mtllib : mtllibs) {
        if (fs::exists(mtllib)) {
            parse_mtl(mtllib, scene.materials);
        }
    }

    if (scene.positions.empty() || scene.triangles.empty()) {
        throw std::runtime_error("El OBJ no contiene geometria util");
    }

    return scene;
}

static Vec3 subtract(const Vec3& a, const Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

static Vec3 cross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

static Vec3 normalize(const Vec3& v) {
    const double length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (length <= 1e-8) {
        return {0.0f, 1.0f, 0.0f};
    }
    return {static_cast<float>(v.x / length), static_cast<float>(v.y / length), static_cast<float>(v.z / length)};
}

static Vec3 triangle_centroid(const std::array<Vec3, 3>& positions) {
    return {
        (positions[0].x + positions[1].x + positions[2].x) / 3.0f,
        (positions[0].y + positions[1].y + positions[2].y) / 3.0f,
        (positions[0].z + positions[1].z + positions[2].z) / 3.0f,
    };
}

static float distance_squared(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

static float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static bool point_in_polygon_2d(const std::vector<std::pair<float, float>>& polygon, const std::pair<float, float>& point) {
    bool inside = false;
    for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const auto& a = polygon[i];
        const auto& b = polygon[j];
        const bool intersects = ((a.second > point.second) != (b.second > point.second)) &&
            (point.first < (b.first - a.first) * (point.second - a.second) / ((b.second - a.second) + 1e-12f) + a.first);
        if (intersects) {
            inside = !inside;
        }
    }
    return inside;
}

static bool point_inside_quad_projected(const std::array<Vec3, 4>& quad, const Vec3& point, const Vec3& plane_normal) {
    const Vec3 abs_normal{std::abs(plane_normal.x), std::abs(plane_normal.y), std::abs(plane_normal.z)};
    std::vector<std::pair<float, float>> polygon;
    polygon.reserve(4);
    std::pair<float, float> projected_point{};

    if (abs_normal.x >= abs_normal.y && abs_normal.x >= abs_normal.z) {
        for (const auto& v : quad) {
            polygon.emplace_back(v.y, v.z);
        }
        projected_point = {point.y, point.z};
    } else if (abs_normal.y >= abs_normal.z) {
        for (const auto& v : quad) {
            polygon.emplace_back(v.x, v.z);
        }
        projected_point = {point.x, point.z};
    } else {
        for (const auto& v : quad) {
            polygon.emplace_back(v.x, v.y);
        }
        projected_point = {point.x, point.y};
    }

    return point_in_polygon_2d(polygon, projected_point);
}

struct TrPortal;
static bool face_hidden_by_portal(const std::vector<TrPortal>& portals, const std::vector<Vec3>& face_vertices);

static std::array<int, 4> ordered_quad_indices(const std::array<Vec3, 4>& positions) {
    Vec3 center{};
    for (const auto& p : positions) {
        center.x += p.x;
        center.y += p.y;
        center.z += p.z;
    }
    center.x *= 0.25f;
    center.y *= 0.25f;
    center.z *= 0.25f;

    Vec3 normal{};
    for (int i = 0; i < 4; ++i) {
        const auto& a = positions[i];
        const auto& b = positions[(i + 1) % 4];
        normal.x += (a.y - b.y) * (a.z + b.z);
        normal.y += (a.z - b.z) * (a.x + b.x);
        normal.z += (a.x - b.x) * (a.y + b.y);
    }
    normal = normalize(normal);

    Vec3 axis_u = cross(std::abs(normal.y) < 0.9f ? Vec3{0.0f, 1.0f, 0.0f} : Vec3{1.0f, 0.0f, 0.0f}, normal);
    axis_u = normalize(axis_u);
    Vec3 axis_v = normalize(cross(normal, axis_u));

    struct QuadOrderEntry {
        int index = 0;
        float angle = 0.0f;
    };

    std::array<QuadOrderEntry, 4> entries{};
    for (int i = 0; i < 4; ++i) {
        const Vec3 offset = subtract(positions[i], center);
        const float u = offset.x * axis_u.x + offset.y * axis_u.y + offset.z * axis_u.z;
        const float v = offset.x * axis_v.x + offset.y * axis_v.y + offset.z * axis_v.z;
        entries[i] = {i, std::atan2(v, u)};
    }

    std::sort(entries.begin(), entries.end(), [](const QuadOrderEntry& a, const QuadOrderEntry& b) {
        return a.angle < b.angle;
    });

    std::array<int, 4> order{};
    for (int i = 0; i < 4; ++i) {
        order[i] = entries[i].index;
    }
    return order;
}

struct DynamicBuffer {
    std::vector<std::uint8_t> data;

    std::pair<uint32_t, uint32_t> append(const std::vector<std::uint8_t>& bytes, uint32_t alignment = 4) {
        while (data.size() % alignment != 0) {
            data.push_back(0);
        }
        const auto offset = static_cast<uint32_t>(data.size());
        data.insert(data.end(), bytes.begin(), bytes.end());
        return {offset, static_cast<uint32_t>(bytes.size())};
    }
};

static std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

static std::string shell_single_quote(const std::string& value) {
    std::string escaped = "'";
    for (char ch : value) {
        if (ch == '\'') {
            escaped += "'\\''";
        } else {
            escaped.push_back(ch);
        }
    }
    escaped.push_back('\'');
    return escaped;
}

struct LoadedTexture {
    std::vector<std::uint8_t> bytes;
    std::string mime_type;
};

static std::vector<std::uint8_t> read_file_bytes(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("No se pudo leer el archivo: " + path.string());
    }
    input.seekg(0, std::ios::end);
    const std::streamsize size = input.tellg();
    input.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    if (size > 0) {
        input.read(reinterpret_cast<char*>(bytes.data()), size);
    }
    return bytes;
}

static LoadedTexture load_texture_bytes(const fs::path& path) {
    const auto ext = lower_copy(path.extension().string());
    if (ext == ".png") {
        return {read_file_bytes(path), "image/png"};
    }
    if (ext == ".jpg" || ext == ".jpeg") {
        return {read_file_bytes(path), "image/jpeg"};
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
    if (!pixels) {
        throw std::runtime_error("No se pudo cargar la textura: " + path.string());
    }

    int out_len = 0;
    unsigned char* png_bytes = stbi_write_png_to_mem(pixels, width * 4, width, height, 4, &out_len);
    stbi_image_free(pixels);
    if (!png_bytes || out_len <= 0) {
        throw std::runtime_error("No se pudo convertir la textura a PNG: " + path.string());
    }

    std::vector<std::uint8_t> result(png_bytes, png_bytes + out_len);
    STBIW_FREE(png_bytes);
    return {std::move(result), "image/png"};
}

static std::vector<std::uint8_t> encode_png_rgba(const std::vector<std::uint8_t>& rgba, int width, int height) {
    int out_len = 0;
    unsigned char* png_bytes = stbi_write_png_to_mem(const_cast<unsigned char*>(rgba.data()), width * 4, width, height, 4, &out_len);
    if (!png_bytes || out_len <= 0) {
        throw std::runtime_error("No se pudo codificar PNG en memoria");
    }
    std::vector<std::uint8_t> result(png_bytes, png_bytes + out_len);
    STBIW_FREE(png_bytes);
    return result;
}

static void write_file_bytes(const fs::path& path, const std::vector<std::uint8_t>& bytes) {
    if (!path.parent_path().empty()) {
        fs::create_directories(path.parent_path());
    }
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("No se pudo escribir el archivo: " + path.string());
    }
    if (!bytes.empty()) {
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
}

template <typename T>
static void append_bytes(std::vector<std::uint8_t>& out, const T& value) {
    const auto* raw = reinterpret_cast<const std::uint8_t*>(&value);
    out.insert(out.end(), raw, raw + sizeof(T));
}

static std::vector<std::uint8_t> pack_floats(const std::vector<float>& values) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(values.size() * sizeof(float));
    for (float value : values) {
        append_bytes(bytes, value);
    }
    return bytes;
}

static std::vector<std::uint8_t> pack_u32(const std::vector<std::uint32_t>& values) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(values.size() * sizeof(std::uint32_t));
    for (std::uint32_t value : values) {
        append_bytes(bytes, value);
    }
    return bytes;
}

struct VertexKey {
    int v = -1;
    int vt = -1;
    std::array<int, 3> normal_quantized{0, 0, 0};

    bool operator<(const VertexKey& other) const {
        return std::tie(v, vt, normal_quantized) < std::tie(other.v, other.vt, other.normal_quantized);
    }
};

static std::array<int, 3> quantize_normal(const Vec3& normal) {
    return {
        static_cast<int>(std::lround(normal.x * 1000000.0f)),
        static_cast<int>(std::lround(normal.y * 1000000.0f)),
        static_cast<int>(std::lround(normal.z * 1000000.0f)),
    };
}

static std::string escape_json(const std::string& input) {
    std::ostringstream out;
    for (unsigned char ch : input) {
        switch (ch) {
            case '\\': out << "\\\\"; break;
            case '"': out << "\\\""; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (ch < 0x20) {
                    out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(ch) << std::dec;
                } else {
                    out << static_cast<char>(ch);
                }
                break;
        }
    }
    return out.str();
}

static std::map<std::pair<int, int>, std::vector<const Triangle*>> sectorize(const ObjScene& scene, double sector_size) {
    float min_x = std::numeric_limits<float>::max();
    float min_z = std::numeric_limits<float>::max();
    for (const auto& position : scene.positions) {
        min_x = std::min(min_x, position.x);
        min_z = std::min(min_z, position.z);
    }

    std::map<std::pair<int, int>, std::vector<const Triangle*>> sectors;
    for (const auto& triangle : scene.triangles) {
        std::array<Vec3, 3> points = {
            scene.positions[triangle.refs[0].v],
            scene.positions[triangle.refs[1].v],
            scene.positions[triangle.refs[2].v],
        };
        const Vec3 centroid = triangle_centroid(points);
        const int sx = static_cast<int>(std::floor((centroid.x - min_x) / sector_size));
        const int sz = static_cast<int>(std::floor((centroid.z - min_z) / sector_size));
        sectors[{sx, sz}].push_back(&triangle);
    }
    return sectors;
}

template <typename T>
static T read_le(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    if (offset + sizeof(T) > bytes.size()) {
        std::ostringstream message;
        message << "Lectura fuera de rango en parser TR (offset=" << offset
                << ", size=" << sizeof(T)
                << ", file_size=" << bytes.size() << ")";
        throw std::runtime_error(message.str());
    }
    T value{};
    std::memcpy(&value, bytes.data() + offset, sizeof(T));
    return value;
}

struct TrObjectTextureVert {
    std::uint8_t x_coordinate = 0;
    std::uint8_t x_pixel = 0;
    std::uint8_t y_coordinate = 0;
    std::uint8_t y_pixel = 0;
};

struct TrObjectTexture {
    std::uint16_t attribute = 0;
    std::uint16_t tile_and_flag = 0;
    std::array<TrObjectTextureVert, 4> vertices;
};

struct TrPortal {
    std::int16_t adjoining_room = -1;
    std::array<Vec3, 4> vertices{};
};

struct TrRoomSector {
    std::uint16_t floor_data_index = 0;
    std::uint16_t box_index = 0;
    std::uint8_t room_below = 255;
    std::int8_t floor = -127;
    std::uint8_t room_above = 255;
    std::int8_t ceiling = -127;
};

struct TrRoomInfo {
    std::int32_t x = 0;
    std::int32_t z = 0;
    std::int32_t y = 0;
    std::size_t data_offset = 0;
    std::uint16_t num_vertices = 0;
    std::uint16_t num_rectangles = 0;
    std::uint16_t num_triangles = 0;
    std::int16_t alternate_room = -1;
    std::vector<TrPortal> portals;
    std::uint16_t z_sectors = 0;
    std::uint16_t x_sectors = 0;
    std::vector<TrRoomSector> sectors;
};

static bool face_hidden_by_portal(const std::vector<TrPortal>& portals, const std::vector<Vec3>& face_vertices) {
    if (face_vertices.size() < 3) {
        return false;
    }

    Vec3 centroid{};
    for (const auto& v : face_vertices) {
        centroid.x += v.x;
        centroid.y += v.y;
        centroid.z += v.z;
    }
    const float inv_count = 1.0f / static_cast<float>(face_vertices.size());
    centroid.x *= inv_count;
    centroid.y *= inv_count;
    centroid.z *= inv_count;

    const Vec3 face_normal = normalize(cross(subtract(face_vertices[1], face_vertices[0]), subtract(face_vertices[2], face_vertices[0])));

    for (const auto& portal : portals) {
        const Vec3 portal_normal = normalize(cross(
            subtract(portal.vertices[1], portal.vertices[0]),
            subtract(portal.vertices[2], portal.vertices[0])));
        if (std::abs(dot(face_normal, portal_normal)) < 0.95f) {
            continue;
        }
        const float plane_distance = std::abs(dot(subtract(centroid, portal.vertices[0]), portal_normal));
        if (plane_distance > 1.0f) {
            continue;
        }
        if (point_inside_quad_projected(portal.vertices, centroid, portal_normal)) {
            return true;
        }
    }

    return false;
}

static const TrRoomSector* sector_at_point(const TrRoomInfo& room, const Vec3& point) {
    if (room.x_sectors == 0 || room.z_sectors == 0 || room.sectors.empty()) {
        return nullptr;
    }
    const int sx = static_cast<int>(std::floor((point.x - static_cast<float>(room.x)) / 1024.0f));
    const int sz = static_cast<int>(std::floor((point.z - static_cast<float>(room.z)) / 1024.0f));
    if (sx < 0 || sz < 0 || sx >= room.x_sectors || sz >= room.z_sectors) {
        return nullptr;
    }
    const std::size_t index = static_cast<std::size_t>(sz) * room.x_sectors + sx;
    if (index >= room.sectors.size()) {
        return nullptr;
    }
    return &room.sectors[index];
}

static bool face_hidden_by_sector_opening(const TrRoomInfo& room, const std::vector<Vec3>& face_vertices) {
    if (face_vertices.size() < 3) {
        return false;
    }

    Vec3 centroid{};
    for (const auto& v : face_vertices) {
        centroid.x += v.x;
        centroid.y += v.y;
        centroid.z += v.z;
    }
    const float inv_count = 1.0f / static_cast<float>(face_vertices.size());
    centroid.x *= inv_count;
    centroid.y *= inv_count;
    centroid.z *= inv_count;

    const TrRoomSector* sector = sector_at_point(room, centroid);
    if (!sector) {
        return false;
    }

    const Vec3 face_normal = normalize(cross(subtract(face_vertices[1], face_vertices[0]), subtract(face_vertices[2], face_vertices[0])));
    if (face_normal.y > 0.35f && sector->room_below != 255) {
        return true;
    }
    if (face_normal.y < -0.35f && sector->room_above != 255) {
        return true;
    }

    return false;
}

static Material make_embedded_material(
    const std::string& name,
    const std::vector<std::uint8_t>& rgba,
    int width,
    int height,
    bool transparent,
    const std::string& image_name) {
    Material material;
    material.name = name;
    material.alpha = 1.0f;
    material.embedded_texture_bytes = encode_png_rgba(rgba, width, height);
    material.embedded_texture_mime_type = "image/png";
    material.embedded_texture_name = image_name;
    material.has_texture = true;
    if (transparent) {
        material.alpha = 0.999f;
    }
    return material;
}

static void debug_skip_block(
    const std::vector<std::uint8_t>& bytes,
    std::size_t& cur,
    const char* format_name,
    const char* label,
    std::size_t element_size) {
    const std::uint32_t count = read_le<std::uint32_t>(bytes, cur);
    if (kTraceTrParser) {
        std::cerr << "[" << format_name << "] " << label << " count=" << count << " offset=" << cur << " elem=" << element_size << std::endl;
    }
    cur += 4 + static_cast<std::size_t>(count) * element_size;
}

static void debug_skip_block_u16(
    const std::vector<std::uint8_t>& bytes,
    std::size_t& cur,
    const char* format_name,
    const char* label,
    std::size_t element_size) {
    const std::uint16_t count = read_le<std::uint16_t>(bytes, cur);
    if (kTraceTrParser) {
        std::cerr << "[" << format_name << "] " << label << " count=" << count << " offset=" << cur << std::endl;
    }
    cur += 2 + static_cast<std::size_t>(count) * element_size;
}

static std::vector<std::uint8_t> decode_tr1_tile_rgba(
    const std::vector<std::uint8_t>& bytes,
    std::size_t tile_offset,
    const std::vector<std::uint8_t>& palette) {
    std::vector<std::uint8_t> rgba(256 * 256 * 4, 0);
    for (int i = 0; i < 256 * 256; ++i) {
        const std::uint8_t key = bytes[tile_offset + static_cast<std::size_t>(i)];
        const std::size_t out = static_cast<std::size_t>(i) * 4;
        if (key == 0) {
            rgba[out + 0] = 0;
            rgba[out + 1] = 0;
            rgba[out + 2] = 0;
            rgba[out + 3] = 0;
        } else {
            rgba[out + 0] = palette[key * 3 + 0];
            rgba[out + 1] = palette[key * 3 + 1];
            rgba[out + 2] = palette[key * 3 + 2];
            rgba[out + 3] = 255;
        }
    }
    return rgba;
}

static std::vector<std::uint8_t> decode_tr2_tile_rgba(
    const std::vector<std::uint8_t>& bytes,
    std::size_t tile_offset,
    const std::vector<std::uint8_t>& palette) {
    std::vector<std::uint8_t> rgba(256 * 256 * 4, 0);
    for (int i = 0; i < 256 * 256; ++i) {
        const std::uint8_t key = bytes[tile_offset + static_cast<std::size_t>(i)];
        const std::size_t out = static_cast<std::size_t>(i) * 4;
        if (key == 0) {
            rgba[out + 0] = 255;
            rgba[out + 1] = 0;
            rgba[out + 2] = 255;
            rgba[out + 3] = 0;
        } else {
            rgba[out + 0] = palette[key * 3 + 0];
            rgba[out + 1] = palette[key * 3 + 1];
            rgba[out + 2] = palette[key * 3 + 2];
            rgba[out + 3] = 255;
        }
    }
    return rgba;
}

static std::vector<std::uint8_t> decode_tr2_tile16_rgba(
    const std::vector<std::uint8_t>& bytes,
    std::size_t tile_offset) {
    std::vector<std::uint8_t> rgba(256 * 256 * 4, 0);
    for (int i = 0; i < 256 * 256; ++i) {
        const std::uint16_t pixel = read_le<std::uint16_t>(bytes, tile_offset + static_cast<std::size_t>(i) * 2);
        const std::size_t out = static_cast<std::size_t>(i) * 4;
        rgba[out + 0] = static_cast<std::uint8_t>(((pixel >> 10) & 0x1Fu) * 255 / 31);
        rgba[out + 1] = static_cast<std::uint8_t>(((pixel >> 5) & 0x1Fu) * 255 / 31);
        rgba[out + 2] = static_cast<std::uint8_t>((pixel & 0x1Fu) * 255 / 31);
        rgba[out + 3] = (pixel & 0x8000u) ? 255 : 0;
    }
    return rgba;
}

static void append_face_triangle(
    ObjScene& scene,
    int a,
    int b,
    int c,
    const std::array<Vec3, 4>& positions,
    const TrObjectTexture& texture,
    const std::string& material_name,
    const std::array<int, 4>& uv_map_indices) {

    const std::array<int, 3> corner_indices{a, b, c};
    Triangle triangle;
    triangle.material_name = material_name;

    for (int i = 0; i < 3; ++i) {
        const int face_corner = corner_indices[i];
        const auto& p = positions[face_corner];
        scene.positions.push_back(p);

        const auto& uv_src = texture.vertices[uv_map_indices[face_corner]];
        const float u = (static_cast<float>(uv_src.x_pixel) + 0.5f) / 256.0f;
        const float v = (static_cast<float>(uv_src.y_pixel) + 0.5f) / 256.0f;
        scene.texcoords.push_back({u, v});

        ObjVertexRef ref;
        ref.v = static_cast<int>(scene.positions.size()) - 1;
        ref.vt = static_cast<int>(scene.texcoords.size()) - 1;
        ref.vn = -1;
        triangle.refs[i] = ref;
    }

    scene.triangles.push_back(triangle);
}

static void append_face_quad(
    ObjScene& scene,
    const std::array<Vec3, 4>& positions,
    const TrObjectTexture& texture,
    const std::string& material_name) {

    const auto order = ordered_quad_indices(positions);
    std::array<Vec3, 4> ordered_positions{
        positions[order[0]],
        positions[order[1]],
        positions[order[2]],
        positions[order[3]],
    };
    std::array<int, 4> ordered_uv_map{order[0], order[1], order[2], order[3]};

    const float diagonal_02 = distance_squared(ordered_positions[0], ordered_positions[2]);
    const float diagonal_13 = distance_squared(ordered_positions[1], ordered_positions[3]);
    if (diagonal_13 < diagonal_02) {
        append_face_triangle(scene, 0, 1, 3, ordered_positions, texture, material_name, ordered_uv_map);
        append_face_triangle(scene, 1, 2, 3, ordered_positions, texture, material_name, ordered_uv_map);
        return;
    }

    append_face_triangle(scene, 0, 1, 2, ordered_positions, texture, material_name, ordered_uv_map);
    append_face_triangle(scene, 0, 2, 3, ordered_positions, texture, material_name, ordered_uv_map);
}

static bool room_face_indices_valid(
    std::size_t vertex_count,
    std::uint16_t v0,
    std::uint16_t v1,
    std::uint16_t v2,
    std::uint16_t v3 = 0,
    bool check_v3 = false) {
    if (v0 >= vertex_count || v1 >= vertex_count || v2 >= vertex_count) {
        return false;
    }
    if (check_v3 && v3 >= vertex_count) {
        return false;
    }
    return true;
}

static ObjScene parse_tr1_level(const fs::path& path) {
    const auto bytes = read_file_bytes(path);
    if (read_le<std::uint32_t>(bytes, 0) != 32u) {
        throw std::runtime_error("El archivo no parece ser TR1 PC");
    }

    const bool is_tub = lower_copy(path.extension().string()) == ".tub";
    std::size_t cur = 4;
    const std::uint32_t num_textiles = read_le<std::uint32_t>(bytes, cur);
    cur += 4;
    std::vector<std::size_t> tile_offsets(num_textiles);
    for (std::uint32_t i = 0; i < num_textiles; ++i) {
        tile_offsets[i] = cur;
        cur += 65536;
    }
    cur += 4;
    if (kTraceTrParser) {
        std::cerr << "[TR1] num_textiles=" << num_textiles << " after_textiles=" << cur << " file_size=" << bytes.size() << std::endl;
    }

    const std::uint16_t num_rooms = read_le<std::uint16_t>(bytes, cur);
    cur += 2;
    std::vector<TrRoomInfo> rooms(num_rooms);
    for (std::uint16_t room = 0; room < num_rooms; ++room) {
        rooms[room].x = read_le<std::int32_t>(bytes, cur + 0);
        rooms[room].z = read_le<std::int32_t>(bytes, cur + 4);
        rooms[room].y = read_le<std::int32_t>(bytes, cur + 8);
        rooms[room].data_offset = cur + 20;
        rooms[room].num_vertices = read_le<std::uint16_t>(bytes, rooms[room].data_offset);
        cur += 20;
        cur += 2 + static_cast<std::size_t>(rooms[room].num_vertices) * 8;
        rooms[room].num_rectangles = read_le<std::uint16_t>(bytes, cur);
        cur += 2 + static_cast<std::size_t>(rooms[room].num_rectangles) * 10;
        rooms[room].num_triangles = read_le<std::uint16_t>(bytes, cur);
        cur += 2 + static_cast<std::size_t>(rooms[room].num_triangles) * 8;
        const std::uint16_t num_sprites = read_le<std::uint16_t>(bytes, cur);
        cur += 2 + static_cast<std::size_t>(num_sprites) * 4;
        const std::uint16_t num_portals = read_le<std::uint16_t>(bytes, cur);
        cur += 2 + static_cast<std::size_t>(num_portals) * 32;
        const std::uint16_t z_sectors = read_le<std::uint16_t>(bytes, cur);
        const std::uint16_t x_sectors = read_le<std::uint16_t>(bytes, cur + 2);
        cur += 6 + static_cast<std::size_t>(z_sectors) * x_sectors * 8;
        const std::uint16_t num_lights = read_le<std::uint16_t>(bytes, cur);
        cur += 2 + static_cast<std::size_t>(num_lights) * 18;
        const std::uint16_t num_statics = read_le<std::uint16_t>(bytes, cur);
        cur += 6 + static_cast<std::size_t>(num_statics) * 18;
    }
    if (kTraceTrParser) {
        std::cerr << "[TR1] num_rooms=" << num_rooms << " after_rooms=" << cur << std::endl;
    }

    debug_skip_block(bytes, cur, "TR1", "NumFloorData", 2);
    debug_skip_block(bytes, cur, "TR1", "NumMeshData", 2);
    debug_skip_block(bytes, cur, "TR1", "NumMeshPointers", 4);
    debug_skip_block(bytes, cur, "TR1", "NumAnimations", 32);
    debug_skip_block(bytes, cur, "TR1", "NumStateChanges", 6);
    debug_skip_block(bytes, cur, "TR1", "NumAnimDispatches", 8);
    debug_skip_block(bytes, cur, "TR1", "NumAnimCommands", 2);
    debug_skip_block(bytes, cur, "TR1", "NumMeshTrees", 4);
    debug_skip_block(bytes, cur, "TR1", "NumFrames", 2);
    debug_skip_block(bytes, cur, "TR1", "NumMoveables", 18);
    debug_skip_block(bytes, cur, "TR1", "NumStaticMeshes", 32);
    if (kTraceTrParser) {
        std::cerr << "[TR1] before_object_textures=" << cur << std::endl;
    }

    const std::size_t object_textures_offset = cur;
    const std::uint32_t num_object_textures = read_le<std::uint32_t>(bytes, cur);
    cur += 4;
    if (kTraceTrParser) {
        std::cerr << "[TR1] num_object_textures=" << num_object_textures << " offset=" << object_textures_offset << std::endl;
    }
    std::vector<TrObjectTexture> object_textures(num_object_textures);
    for (std::uint32_t i = 0; i < num_object_textures; ++i) {
        TrObjectTexture tex;
        tex.attribute = read_le<std::uint16_t>(bytes, cur + 0);
        tex.tile_and_flag = read_le<std::uint16_t>(bytes, cur + 2);
        for (int v = 0; v < 4; ++v) {
            tex.vertices[v].x_coordinate = bytes[cur + 4 + v * 4 + 0];
            tex.vertices[v].x_pixel = bytes[cur + 4 + v * 4 + 1];
            tex.vertices[v].y_coordinate = bytes[cur + 4 + v * 4 + 2];
            tex.vertices[v].y_pixel = bytes[cur + 4 + v * 4 + 3];
        }
        object_textures[i] = tex;
        cur += 20;
    }
    (void)object_textures_offset;
    if (kTraceTrParser) {
        std::cerr << "[TR1] after_object_textures=" << cur << std::endl;
    }

    debug_skip_block(bytes, cur, "TR1", "NumSpriteTextures", 16);
    debug_skip_block(bytes, cur, "TR1", "NumSpriteSequences", 8);

    std::vector<std::uint8_t> palette(256 * 3, 0);

    debug_skip_block(bytes, cur, "TR1", "NumCameras", 16);
    debug_skip_block(bytes, cur, "TR1", "NumSoundSources", 16);
    const std::uint32_t num_boxes = read_le<std::uint32_t>(bytes, cur);
    if (kTraceTrParser) {
        std::cerr << "[TR1] NumBoxes count=" << num_boxes << " offset=" << cur << std::endl;
    }
    cur += 4 + static_cast<std::size_t>(num_boxes) * 20;
    debug_skip_block(bytes, cur, "TR1", "NumOverlaps", 2);
    cur += static_cast<std::size_t>(num_boxes) * 12;
    debug_skip_block(bytes, cur, "TR1", "NumAnimatedTextures", 2);
    const std::uint32_t num_items = read_le<std::uint32_t>(bytes, cur);
    if (kTraceTrParser) {
        std::cerr << "[TR1] NumItems count=" << num_items << " offset=" << cur << std::endl;
    }
    cur += 4 + static_cast<std::size_t>(num_items) * 22;
    cur += 8192;

    if (is_tub) {
        for (int i = 0; i < 256; ++i) {
            palette[i * 3 + 0] = bytes[cur + i * 3 + 2] * 4;
            palette[i * 3 + 1] = bytes[cur + i * 3 + 1] * 4;
            palette[i * 3 + 2] = bytes[cur + i * 3 + 0] * 4;
        }
    }

    if (!is_tub) {
        for (int i = 0; i < 256; ++i) {
            palette[i * 3 + 0] = bytes[cur + i * 3 + 2] * 4;
            palette[i * 3 + 1] = bytes[cur + i * 3 + 1] * 4;
            palette[i * 3 + 2] = bytes[cur + i * 3 + 0] * 4;
        }
        cur += 256 * 3;
    }

    debug_skip_block_u16(bytes, cur, "TR1", "NumCinematicFrames", 16);
    debug_skip_block_u16(bytes, cur, "TR1", "NumDemoData", 1);
    cur += 512;
    debug_skip_block(bytes, cur, "TR1", "NumSoundDetails", 8);
    debug_skip_block(bytes, cur, "TR1", "NumSamples", 1);
    debug_skip_block(bytes, cur, "TR1", "NumSampleIndices", 4);

    ObjScene scene;
    std::set<std::uint16_t> used_textures;
    for (const auto& room : rooms) {
        std::size_t room_cur = room.data_offset;
        const std::uint16_t num_vertices = read_le<std::uint16_t>(bytes, room_cur);
        room_cur += 2;
        std::vector<Vec3> room_vertices(num_vertices);
        for (std::uint16_t i = 0; i < num_vertices; ++i) {
            const std::int16_t vx = read_le<std::int16_t>(bytes, room_cur + i * 8 + 0);
            const std::int16_t vy = read_le<std::int16_t>(bytes, room_cur + i * 8 + 2);
            const std::int16_t vz = read_le<std::int16_t>(bytes, room_cur + i * 8 + 4);
            room_vertices[i] = {
                static_cast<float>(room.x + vx),
                static_cast<float>(room.y - vy),
                static_cast<float>(room.z + vz)
            };
        }
        room_cur += static_cast<std::size_t>(num_vertices) * 8;

        const std::uint16_t num_rectangles = read_le<std::uint16_t>(bytes, room_cur);
        room_cur += 2;
        for (std::uint16_t i = 0; i < num_rectangles; ++i) {
            const std::size_t face = room_cur + i * 10;
            const std::uint16_t v0 = read_le<std::uint16_t>(bytes, face + 0);
            const std::uint16_t v1 = read_le<std::uint16_t>(bytes, face + 2);
            const std::uint16_t v2 = read_le<std::uint16_t>(bytes, face + 4);
            const std::uint16_t v3 = read_le<std::uint16_t>(bytes, face + 6);
            const std::uint16_t texture = read_le<std::uint16_t>(bytes, face + 8);
            const std::uint16_t texture_index = texture & 0x7fff;
            if (texture_index >= object_textures.size()) {
                continue;
            }
            if (!room_face_indices_valid(room_vertices.size(), v0, v1, v2, v3, true)) {
                continue;
            }
            used_textures.insert(texture_index);
            const std::string material_name = "tr1_tex_" + std::to_string(texture_index);
            const std::array<Vec3, 4> positions{room_vertices[v0], room_vertices[v1], room_vertices[v2], room_vertices[v3]};
            append_face_quad(scene, positions, object_textures[texture_index], material_name);
        }
        room_cur += static_cast<std::size_t>(num_rectangles) * 10;

        const std::uint16_t num_triangles = read_le<std::uint16_t>(bytes, room_cur);
        room_cur += 2;
        for (std::uint16_t i = 0; i < num_triangles; ++i) {
            const std::size_t face = room_cur + i * 8;
            const std::uint16_t v0 = read_le<std::uint16_t>(bytes, face + 0);
            const std::uint16_t v1 = read_le<std::uint16_t>(bytes, face + 2);
            const std::uint16_t v2 = read_le<std::uint16_t>(bytes, face + 4);
            const std::uint16_t texture = read_le<std::uint16_t>(bytes, face + 6);
            const std::uint16_t texture_index = texture & 0x7fff;
            if (texture_index >= object_textures.size()) {
                continue;
            }
            if (!room_face_indices_valid(room_vertices.size(), v0, v1, v2)) {
                continue;
            }
            used_textures.insert(texture_index);
            const std::string material_name = "tr1_tex_" + std::to_string(texture_index);
            const std::array<Vec3, 4> positions{room_vertices[v0], room_vertices[v1], room_vertices[v2], room_vertices[v2]};
            append_face_triangle(scene, 0, 1, 2, positions, object_textures[texture_index], material_name, {0, 1, 2, 2});
        }
    }

    for (const std::uint16_t texture_index : used_textures) {
        const auto& tex = object_textures[texture_index];
        const std::uint16_t tile = tex.tile_and_flag & 0x7fff;
        if (tile >= tile_offsets.size()) {
            continue;
        }
        const auto rgba = decode_tr1_tile_rgba(bytes, tile_offsets[tile], palette);
        scene.materials["tr1_tex_" + std::to_string(texture_index)] =
            make_embedded_material("tr1_tex_" + std::to_string(texture_index), rgba, 256, 256, tex.attribute != 0, "tr1_tile_" + std::to_string(tile) + ".png");
    }

    return scene;
}

static ObjScene parse_tr2_level(const fs::path& path) {
    const auto bytes = read_file_bytes(path);
    if (read_le<std::uint32_t>(bytes, 0) != 45u) {
        throw std::runtime_error("El archivo no parece ser TR2 PC");
    }

    std::size_t cur = 4;
    std::vector<std::uint8_t> palette8(256 * 3, 0);
    for (int i = 0; i < 256; ++i) {
        palette8[i * 3 + 0] = bytes[cur + i * 3 + 2] * 4;
        palette8[i * 3 + 1] = bytes[cur + i * 3 + 1] * 4;
        palette8[i * 3 + 2] = bytes[cur + i * 3 + 0] * 4;
        cur += 3;
    }
    cur += 256 * 4;

    const std::uint32_t num_textiles = read_le<std::uint32_t>(bytes, cur);
    cur += 4;
    std::vector<std::size_t> tile_offsets8(num_textiles);
    for (std::uint32_t i = 0; i < num_textiles; ++i) {
        tile_offsets8[i] = cur;
        cur += 65536;
    }
    std::vector<std::size_t> tile_offsets16(num_textiles);
    for (std::uint32_t i = 0; i < num_textiles; ++i) {
        tile_offsets16[i] = cur;
        cur += 131072;
    }

    const std::uint32_t num_textiles_check = read_le<std::uint32_t>(bytes, 1796);
    cur = 1804 + static_cast<std::size_t>(num_textiles_check) * 196608;
    if (kTraceTrParser) {
        std::cerr << "[TR2] num_textiles=" << num_textiles << " num_textiles_check=" << num_textiles_check << " after_textiles=" << cur << " file_size=" << bytes.size() << std::endl;
    }
    const std::uint16_t num_rooms = read_le<std::uint16_t>(bytes, cur);
    cur += 2;
    std::vector<TrRoomInfo> rooms(num_rooms);
    for (std::uint16_t room = 0; room < num_rooms; ++room) {
        rooms[room].x = read_le<std::int32_t>(bytes, cur + 0);
        rooms[room].z = read_le<std::int32_t>(bytes, cur + 4);
        rooms[room].y = read_le<std::int32_t>(bytes, cur + 8);
        rooms[room].data_offset = cur + 20;
        rooms[room].num_vertices = read_le<std::uint16_t>(bytes, rooms[room].data_offset);
        cur += 20;
        cur += 2 + static_cast<std::size_t>(rooms[room].num_vertices) * 12;
        rooms[room].num_rectangles = read_le<std::uint16_t>(bytes, cur);
        cur += 2 + static_cast<std::size_t>(rooms[room].num_rectangles) * 10;
        rooms[room].num_triangles = read_le<std::uint16_t>(bytes, cur);
        cur += 2 + static_cast<std::size_t>(rooms[room].num_triangles) * 8;
        const std::uint16_t num_sprites = read_le<std::uint16_t>(bytes, cur);
        cur += 2 + static_cast<std::size_t>(num_sprites) * 4;
        const std::uint16_t num_portals = read_le<std::uint16_t>(bytes, cur);
        cur += 2 + static_cast<std::size_t>(num_portals) * 32;
        const std::uint16_t z_sectors = read_le<std::uint16_t>(bytes, cur);
        const std::uint16_t x_sectors = read_le<std::uint16_t>(bytes, cur + 2);
        rooms[room].z_sectors = z_sectors;
        rooms[room].x_sectors = x_sectors;
        cur += 10 + static_cast<std::size_t>(z_sectors) * x_sectors * 8;
        const std::uint16_t num_lights = read_le<std::uint16_t>(bytes, cur);
        cur += 2 + static_cast<std::size_t>(num_lights) * 24;
        const std::uint16_t num_statics = read_le<std::uint16_t>(bytes, cur);
        cur += 6 + static_cast<std::size_t>(num_statics) * 20;
    }
    if (kTraceTrParser) {
        std::cerr << "[TR2] num_rooms=" << num_rooms << " after_rooms=" << cur << std::endl;
    }

    debug_skip_block(bytes, cur, "TR2", "NumFloorData", 2);
    debug_skip_block(bytes, cur, "TR2", "NumMeshData", 2);
    debug_skip_block(bytes, cur, "TR2", "NumMeshPointers", 4);
    debug_skip_block(bytes, cur, "TR2", "NumAnimations", 32);
    debug_skip_block(bytes, cur, "TR2", "NumStateChanges", 6);
    debug_skip_block(bytes, cur, "TR2", "NumAnimDispatches", 8);
    debug_skip_block(bytes, cur, "TR2", "NumAnimCommands", 2);
    debug_skip_block(bytes, cur, "TR2", "NumMeshTrees", 4);
    debug_skip_block(bytes, cur, "TR2", "NumFrames", 2);
    debug_skip_block(bytes, cur, "TR2", "NumMoveables", 18);
    debug_skip_block(bytes, cur, "TR2", "NumStaticMeshes", 32);
    if (kTraceTrParser) {
        std::cerr << "[TR2] before_object_textures=" << cur << std::endl;
    }

    const std::uint32_t num_object_textures = read_le<std::uint32_t>(bytes, cur);
    cur += 4;
    if (kTraceTrParser) {
        std::cerr << "[TR2] num_object_textures=" << num_object_textures << " offset=" << (cur - 4) << std::endl;
    }
    std::vector<TrObjectTexture> object_textures(num_object_textures);
    for (std::uint32_t i = 0; i < num_object_textures; ++i) {
        TrObjectTexture tex;
        tex.attribute = read_le<std::uint16_t>(bytes, cur + 0);
        tex.tile_and_flag = read_le<std::uint16_t>(bytes, cur + 2);
        for (int v = 0; v < 4; ++v) {
            tex.vertices[v].x_coordinate = bytes[cur + 4 + v * 4 + 0];
            tex.vertices[v].x_pixel = bytes[cur + 4 + v * 4 + 1];
            tex.vertices[v].y_coordinate = bytes[cur + 4 + v * 4 + 2];
            tex.vertices[v].y_pixel = bytes[cur + 4 + v * 4 + 3];
        }
        object_textures[i] = tex;
        cur += 20;
    }

    for (auto& room : rooms) {
        std::size_t room_cur = room.data_offset;
        const std::uint16_t num_vertices = read_le<std::uint16_t>(bytes, room_cur);
        room_cur += 2 + static_cast<std::size_t>(num_vertices) * 12;
        const std::uint16_t num_rectangles = read_le<std::uint16_t>(bytes, room_cur);
        room_cur += 2 + static_cast<std::size_t>(num_rectangles) * 10;
        const std::uint16_t num_triangles = read_le<std::uint16_t>(bytes, room_cur);
        room_cur += 2 + static_cast<std::size_t>(num_triangles) * 8;
        const std::uint16_t num_sprites = read_le<std::uint16_t>(bytes, room_cur);
        room_cur += 2 + static_cast<std::size_t>(num_sprites) * 4;
        const std::uint16_t num_portals = read_le<std::uint16_t>(bytes, room_cur);
        room_cur += 2;

        room.portals.clear();
        room.portals.reserve(num_portals);
        for (std::uint16_t i = 0; i < num_portals; ++i) {
            const std::size_t portal_offset = room_cur + static_cast<std::size_t>(i) * 32;
            TrPortal portal;
            portal.adjoining_room = read_le<std::int16_t>(bytes, portal_offset + 0);
            for (int v = 0; v < 4; ++v) {
                const std::int16_t px = read_le<std::int16_t>(bytes, portal_offset + 8 + v * 6 + 0);
                const std::int16_t py = read_le<std::int16_t>(bytes, portal_offset + 8 + v * 6 + 2);
                const std::int16_t pz = read_le<std::int16_t>(bytes, portal_offset + 8 + v * 6 + 4);
                portal.vertices[v] = {
                    static_cast<float>(room.x + px),
                    static_cast<float>(room.y - py),
                    static_cast<float>(room.z + pz)
                };
            }
            room.portals.push_back(portal);
        }

        room_cur += static_cast<std::size_t>(num_portals) * 32;
        room.z_sectors = read_le<std::uint16_t>(bytes, room_cur);
        room.x_sectors = read_le<std::uint16_t>(bytes, room_cur + 2);
        room.sectors.clear();
        room.sectors.reserve(static_cast<std::size_t>(room.z_sectors) * room.x_sectors);
        room_cur += 4;
        for (std::size_t i = 0; i < static_cast<std::size_t>(room.z_sectors) * room.x_sectors; ++i) {
            TrRoomSector sector;
            sector.floor_data_index = read_le<std::uint16_t>(bytes, room_cur + i * 8 + 0);
            sector.box_index = read_le<std::uint16_t>(bytes, room_cur + i * 8 + 2);
            sector.room_below = bytes[room_cur + i * 8 + 4];
            sector.floor = static_cast<std::int8_t>(bytes[room_cur + i * 8 + 5]);
            sector.room_above = bytes[room_cur + i * 8 + 6];
            sector.ceiling = static_cast<std::int8_t>(bytes[room_cur + i * 8 + 7]);
            room.sectors.push_back(sector);
        }
    }

    ObjScene scene;
    std::set<std::uint16_t> used_textures;
    for (const auto& room : rooms) {
        std::size_t room_cur = room.data_offset;
        const std::uint16_t num_vertices = read_le<std::uint16_t>(bytes, room_cur);
        room_cur += 2;
        std::vector<Vec3> room_vertices(num_vertices);
        for (std::uint16_t i = 0; i < num_vertices; ++i) {
            const std::int16_t vx = read_le<std::int16_t>(bytes, room_cur + i * 12 + 0);
            const std::int16_t vy = read_le<std::int16_t>(bytes, room_cur + i * 12 + 2);
            const std::int16_t vz = read_le<std::int16_t>(bytes, room_cur + i * 12 + 4);
            room_vertices[i] = {
                static_cast<float>(room.x + vx),
                static_cast<float>(room.y - vy),
                static_cast<float>(room.z + vz)
            };
        }
        room_cur += static_cast<std::size_t>(num_vertices) * 12;

        const std::uint16_t num_rectangles = read_le<std::uint16_t>(bytes, room_cur);
        room_cur += 2;
        for (std::uint16_t i = 0; i < num_rectangles; ++i) {
            const std::size_t face = room_cur + i * 10;
            const std::uint16_t v0 = read_le<std::uint16_t>(bytes, face + 0);
            const std::uint16_t v1 = read_le<std::uint16_t>(bytes, face + 2);
            const std::uint16_t v2 = read_le<std::uint16_t>(bytes, face + 4);
            const std::uint16_t v3 = read_le<std::uint16_t>(bytes, face + 6);
            const std::uint16_t texture = read_le<std::uint16_t>(bytes, face + 8);
            const std::uint16_t texture_index = texture & 0x7fff;
            if (texture_index >= object_textures.size()) {
                continue;
            }
            if (!room_face_indices_valid(room_vertices.size(), v0, v1, v2, v3, true)) {
                continue;
            }
            used_textures.insert(texture_index);
            const std::string material_name = "tr2_tex_" + std::to_string(texture_index);
            const std::array<Vec3, 4> positions{room_vertices[v0], room_vertices[v1], room_vertices[v2], room_vertices[v3]};
            if (face_hidden_by_portal(room.portals, {positions[0], positions[1], positions[2], positions[3]})) {
                continue;
            }
            append_face_quad(scene, positions, object_textures[texture_index], material_name);
        }
        room_cur += static_cast<std::size_t>(num_rectangles) * 10;

        const std::uint16_t num_triangles = read_le<std::uint16_t>(bytes, room_cur);
        room_cur += 2;
        for (std::uint16_t i = 0; i < num_triangles; ++i) {
            const std::size_t face = room_cur + i * 8;
            const std::uint16_t v0 = read_le<std::uint16_t>(bytes, face + 0);
            const std::uint16_t v1 = read_le<std::uint16_t>(bytes, face + 2);
            const std::uint16_t v2 = read_le<std::uint16_t>(bytes, face + 4);
            const std::uint16_t texture = read_le<std::uint16_t>(bytes, face + 6);
            const std::uint16_t texture_index = texture & 0x7fff;
            if (texture_index >= object_textures.size()) {
                continue;
            }
            if (!room_face_indices_valid(room_vertices.size(), v0, v1, v2)) {
                continue;
            }
            used_textures.insert(texture_index);
            const std::string material_name = "tr2_tex_" + std::to_string(texture_index);
            const std::array<Vec3, 4> positions{room_vertices[v0], room_vertices[v1], room_vertices[v2], room_vertices[v2]};
            if (face_hidden_by_portal(room.portals, {positions[0], positions[1], positions[2]})) {
                continue;
            }
            append_face_triangle(scene, 0, 1, 2, positions, object_textures[texture_index], material_name, {0, 1, 2, 2});
        }
    }

    for (const std::uint16_t texture_index : used_textures) {
        const auto& tex = object_textures[texture_index];
        const std::uint16_t tile = tex.tile_and_flag & 0x7fff;
        if (tile >= tile_offsets16.size()) {
            continue;
        }
        const auto rgba = decode_tr2_tile16_rgba(bytes, tile_offsets16[tile]);
        scene.materials["tr2_tex_" + std::to_string(texture_index)] =
            make_embedded_material("tr2_tex_" + std::to_string(texture_index), rgba, 256, 256, tex.attribute != 0, "tr2_tile_" + std::to_string(tile) + ".png");
    }

    return scene;
}

static ObjScene parse_native_level(const fs::path& path) {
    const auto bytes = read_file_bytes(path);
    if (bytes.size() < 4) {
        throw std::runtime_error("Archivo demasiado pequeno");
    }
    const std::uint32_t version = read_le<std::uint32_t>(bytes, 0);
    if (version == 32u) {
        return parse_tr1_level(path);
    }
    if (version == 45u) {
        return parse_tr2_level(path);
    }
    throw std::runtime_error("Formato nativo aun no soportado por este importador directo");
}

struct ExportResult {
    std::string file_name;
    std::size_t triangle_count = 0;
};

static ExportResult write_glb(
    const ObjScene& scene,
    const std::vector<const Triangle*>& triangles,
    const fs::path& output_path,
    const std::string& node_name,
    std::optional<std::array<float, 3>> node_translation = std::nullopt) {

    DynamicBuffer buffer;
    std::vector<BufferViewDef> buffer_views;
    std::vector<AccessorDef> accessors;
    std::vector<ImageDef> images;
    std::vector<TextureDef> textures;
    std::vector<MaterialDef> materials;
    std::vector<PrimitiveDef> primitives;
    std::map<std::string, uint32_t> material_indices;
    std::map<fs::path, uint32_t> texture_indices;

    auto get_material_index = [&](const std::string& name) -> uint32_t {
        const auto found = material_indices.find(name);
        if (found != material_indices.end()) {
            return found->second;
        }

        MaterialDef def;
        def.name = name.empty() ? "default" : name;

        const auto material_it = scene.materials.find(name);
        if (material_it != scene.materials.end()) {
            const auto& src = material_it->second;
            def.base_color_factor = {src.diffuse[0], src.diffuse[1], src.diffuse[2], src.alpha};
            def.alpha_blend = src.alpha < 0.999f;

            if (!src.embedded_texture_bytes.empty()) {
                const fs::path synthetic_key = fs::path("__embedded__") / src.embedded_texture_name;
                auto tex_it = texture_indices.find(synthetic_key);
                if (tex_it == texture_indices.end()) {
                    const auto [offset, length] = buffer.append(src.embedded_texture_bytes, 4);
                    buffer_views.push_back({offset, length, std::nullopt});
                    images.push_back({static_cast<uint32_t>(buffer_views.size() - 1), src.embedded_texture_mime_type, src.embedded_texture_name, std::nullopt});
                    textures.push_back({static_cast<uint32_t>(images.size() - 1)});
                    const uint32_t texture_index = static_cast<uint32_t>(textures.size() - 1);
                    texture_indices[synthetic_key] = texture_index;
                    tex_it = texture_indices.find(synthetic_key);
                }
                def.base_color_texture = tex_it->second;
            } else if (src.has_texture && fs::exists(src.texture_path)) {
                auto tex_it = texture_indices.find(src.texture_path);
                if (tex_it == texture_indices.end()) {
                    const auto loaded = load_texture_bytes(src.texture_path);
                    const auto [offset, length] = buffer.append(loaded.bytes, 4);
                    buffer_views.push_back({offset, length, std::nullopt});
                    images.push_back({static_cast<uint32_t>(buffer_views.size() - 1), loaded.mime_type, src.texture_path.filename().string(), std::nullopt});
                    textures.push_back({static_cast<uint32_t>(images.size() - 1)});
                    const uint32_t texture_index = static_cast<uint32_t>(textures.size() - 1);
                    texture_indices[src.texture_path] = texture_index;
                    tex_it = texture_indices.find(src.texture_path);
                }
                def.base_color_texture = tex_it->second;
            }
        }

        const uint32_t index = static_cast<uint32_t>(materials.size());
        materials.push_back(def);
        material_indices[name] = index;
        return index;
    };

    std::map<std::string, std::vector<const Triangle*>> by_material;
    for (const auto* triangle : triangles) {
        by_material[triangle->material_name].push_back(triangle);
    }

    const std::array<float, 3> translation = node_translation.value_or(std::array<float, 3>{0.0f, 0.0f, 0.0f});

    for (const auto& [material_name, tris] : by_material) {
        std::vector<float> positions_flat;
        std::vector<float> normals_flat;
        std::vector<float> uvs_flat;
        std::vector<std::uint32_t> indices;
        std::map<VertexKey, std::uint32_t> vertex_map;

        std::array<float, 3> min_pos{
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max()
        };
        std::array<float, 3> max_pos{
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest()
        };

        for (const auto* triangle : tris) {
            std::array<Vec3, 3> tri_positions = {
                scene.positions[triangle->refs[0].v],
                scene.positions[triangle->refs[1].v],
                scene.positions[triangle->refs[2].v],
            };
            const Vec3 face_normal = normalize(cross(
                subtract(tri_positions[1], tri_positions[0]),
                subtract(tri_positions[2], tri_positions[0])));

            for (int i = 0; i < 3; ++i) {
                const auto& ref = triangle->refs[i];
                const Vec3 position = {
                    scene.positions[ref.v].x + translation[0],
                    scene.positions[ref.v].y + translation[1],
                    scene.positions[ref.v].z + translation[2]
                };
                const Vec2 uv = (ref.vt >= 0 && ref.vt < static_cast<int>(scene.texcoords.size()))
                    ? scene.texcoords[ref.vt]
                    : Vec2{};
                const Vec3 normal = (ref.vn >= 0 && ref.vn < static_cast<int>(scene.normals.size()))
                    ? normalize(scene.normals[ref.vn])
                    : face_normal;

                VertexKey key;
                key.v = ref.v;
                key.vt = ref.vt;
                key.normal_quantized = quantize_normal(normal);

                auto it = vertex_map.find(key);
                if (it == vertex_map.end()) {
                    const std::uint32_t index = static_cast<std::uint32_t>(positions_flat.size() / 3);
                    vertex_map[key] = index;
                    positions_flat.push_back(position.x);
                    positions_flat.push_back(position.y);
                    positions_flat.push_back(position.z);
                    normals_flat.push_back(normal.x);
                    normals_flat.push_back(normal.y);
                    normals_flat.push_back(normal.z);
                    uvs_flat.push_back(uv.x);
                    uvs_flat.push_back(uv.y);
                    min_pos[0] = std::min(min_pos[0], position.x);
                    min_pos[1] = std::min(min_pos[1], position.y);
                    min_pos[2] = std::min(min_pos[2], position.z);
                    max_pos[0] = std::max(max_pos[0], position.x);
                    max_pos[1] = std::max(max_pos[1], position.y);
                    max_pos[2] = std::max(max_pos[2], position.z);
                    indices.push_back(index);
                } else {
                    indices.push_back(it->second);
                }
            }
        }

        const auto [pos_offset, pos_length] = buffer.append(pack_floats(positions_flat), 4);
        buffer_views.push_back({pos_offset, pos_length, 34962u});
        const uint32_t pos_bv = static_cast<uint32_t>(buffer_views.size() - 1);

        const auto [normal_offset, normal_length] = buffer.append(pack_floats(normals_flat), 4);
        buffer_views.push_back({normal_offset, normal_length, 34962u});
        const uint32_t normal_bv = static_cast<uint32_t>(buffer_views.size() - 1);

        const auto [uv_offset, uv_length] = buffer.append(pack_floats(uvs_flat), 4);
        buffer_views.push_back({uv_offset, uv_length, 34962u});
        const uint32_t uv_bv = static_cast<uint32_t>(buffer_views.size() - 1);

        const auto [idx_offset, idx_length] = buffer.append(pack_u32(indices), 4);
        buffer_views.push_back({idx_offset, idx_length, 34963u});
        const uint32_t idx_bv = static_cast<uint32_t>(buffer_views.size() - 1);

        accessors.push_back({pos_bv, 5126u, static_cast<uint32_t>(positions_flat.size() / 3), "VEC3", min_pos, max_pos});
        const uint32_t pos_acc = static_cast<uint32_t>(accessors.size() - 1);
        accessors.push_back({normal_bv, 5126u, static_cast<uint32_t>(normals_flat.size() / 3), "VEC3", std::nullopt, std::nullopt});
        const uint32_t normal_acc = static_cast<uint32_t>(accessors.size() - 1);
        accessors.push_back({uv_bv, 5126u, static_cast<uint32_t>(uvs_flat.size() / 2), "VEC2", std::nullopt, std::nullopt});
        const uint32_t uv_acc = static_cast<uint32_t>(accessors.size() - 1);
        accessors.push_back({idx_bv, 5125u, static_cast<uint32_t>(indices.size()), "SCALAR", std::nullopt, std::nullopt});
        const uint32_t idx_acc = static_cast<uint32_t>(accessors.size() - 1);

        primitives.push_back({pos_acc, normal_acc, uv_acc, idx_acc, get_material_index(material_name)});
    }

    std::ostringstream json;
    json << "{\"asset\":{\"version\":\"2.0\",\"generator\":\"trlevel_exporter\"},";
    json << "\"scene\":0,";
    json << "\"scenes\":[{\"nodes\":[0]}],";
    json << "\"nodes\":[{\"mesh\":0,\"name\":\"" << escape_json(node_name) << "\"";
    if (node_translation.has_value()) {
        const auto& t = *node_translation;
        json << ",\"translation\":[" << t[0] << ',' << t[1] << ',' << t[2] << "]";
    }
    json << "}],";
    json << "\"meshes\":[{\"name\":\"" << escape_json(node_name) << "\",\"primitives\":[";
    for (std::size_t i = 0; i < primitives.size(); ++i) {
        const auto& primitive = primitives[i];
        if (i > 0) {
            json << ',';
        }
        json << "{\"attributes\":{\"POSITION\":" << primitive.position_accessor
             << ",\"NORMAL\":" << primitive.normal_accessor
             << ",\"TEXCOORD_0\":" << primitive.uv_accessor
             << "},\"indices\":" << primitive.index_accessor
             << ",\"material\":" << primitive.material_index
             << "}";
    }
    json << "]}],";

    json << "\"buffers\":[{\"byteLength\":" << buffer.data.size() << "}],";

    json << "\"bufferViews\":[";
    for (std::size_t i = 0; i < buffer_views.size(); ++i) {
        const auto& view = buffer_views[i];
        if (i > 0) {
            json << ',';
        }
        json << "{\"buffer\":0,\"byteOffset\":" << view.byte_offset << ",\"byteLength\":" << view.byte_length;
        if (view.target.has_value()) {
            json << ",\"target\":" << *view.target;
        }
        json << "}";
    }
    json << "],";

    json << "\"accessors\":[";
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        const auto& accessor = accessors[i];
        if (i > 0) {
            json << ',';
        }
        json << "{\"bufferView\":" << accessor.buffer_view
             << ",\"componentType\":" << accessor.component_type
             << ",\"count\":" << accessor.count
             << ",\"type\":\"" << accessor.type << "\"";
        if (accessor.min_vec3.has_value()) {
            const auto& minv = *accessor.min_vec3;
            json << ",\"min\":[" << minv[0] << ',' << minv[1] << ',' << minv[2] << "]";
        }
        if (accessor.max_vec3.has_value()) {
            const auto& maxv = *accessor.max_vec3;
            json << ",\"max\":[" << maxv[0] << ',' << maxv[1] << ',' << maxv[2] << "]";
        }
        json << "}";
    }
    json << "],";

    json << "\"samplers\":[{\"magFilter\":9729,\"minFilter\":9987,\"wrapS\":10497,\"wrapT\":10497}],";

    json << "\"images\":[";
    for (std::size_t i = 0; i < images.size(); ++i) {
        const auto& image = images[i];
        if (i > 0) {
            json << ',';
        }
        json << "{";
        bool needs_comma = false;
        if (image.buffer_view.has_value()) {
            json << "\"bufferView\":" << *image.buffer_view;
            needs_comma = true;
        }
        if (!image.mime_type.empty()) {
            if (needs_comma) {
                json << ',';
            }
            json << "\"mimeType\":\"" << image.mime_type << "\"";
            needs_comma = true;
        }
        if (image.uri.has_value()) {
            if (needs_comma) {
                json << ',';
            }
            json << "\"uri\":\"" << escape_json(*image.uri) << "\"";
            needs_comma = true;
        }
        if (needs_comma) {
            json << ',';
        }
        json << "\"name\":\"" << escape_json(image.name) << "\"}";
    }
    json << "],";

    json << "\"textures\":[";
    for (std::size_t i = 0; i < textures.size(); ++i) {
        const auto& texture = textures[i];
        if (i > 0) {
            json << ',';
        }
        json << "{\"sampler\":0,\"source\":" << texture.source << "}";
    }
    json << "],";

    json << "\"materials\":[";
    for (std::size_t i = 0; i < materials.size(); ++i) {
        const auto& material = materials[i];
        if (i > 0) {
            json << ',';
        }
        json << "{\"name\":\"" << escape_json(material.name) << "\",\"pbrMetallicRoughness\":{\"baseColorFactor\":["
             << material.base_color_factor[0] << ',' << material.base_color_factor[1] << ','
             << material.base_color_factor[2] << ',' << material.base_color_factor[3]
             << "],\"metallicFactor\":0.0,\"roughnessFactor\":1.0";
        if (material.base_color_texture.has_value()) {
            json << ",\"baseColorTexture\":{\"index\":" << *material.base_color_texture << "}";
        }
        json << "}";
        if (material.alpha_blend) {
            json << ",\"alphaMode\":\"BLEND\"";
        }
        json << "}";
    }
    json << "]}";

    std::string json_str = json.str();
    while (json_str.size() % 4 != 0) {
        json_str.push_back(' ');
    }
    while (buffer.data.size() % 4 != 0) {
        buffer.data.push_back(0);
    }

    const uint32_t total_length = 12u + 8u + static_cast<uint32_t>(json_str.size()) + 8u + static_cast<uint32_t>(buffer.data.size());
    if (!output_path.parent_path().empty()) {
        fs::create_directories(output_path.parent_path());
    }

    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("No se pudo escribir el GLB: " + output_path.string());
    }

    out.write("glTF", 4);
    const uint32_t version = 2;
    out.write(reinterpret_cast<const char*>(&version), sizeof(version));
    out.write(reinterpret_cast<const char*>(&total_length), sizeof(total_length));

    const uint32_t json_length = static_cast<uint32_t>(json_str.size());
    out.write(reinterpret_cast<const char*>(&json_length), sizeof(json_length));
    out.write("JSON", 4);
    out.write(json_str.data(), static_cast<std::streamsize>(json_str.size()));

    const uint32_t bin_length = static_cast<uint32_t>(buffer.data.size());
    out.write(reinterpret_cast<const char*>(&bin_length), sizeof(bin_length));
    out.write("BIN\0", 4);
    out.write(reinterpret_cast<const char*>(buffer.data.data()), static_cast<std::streamsize>(buffer.data.size()));

    return {output_path.filename().string(), triangles.size()};
}

static ExportResult write_gltf(
    const ObjScene& scene,
    const std::vector<const Triangle*>& triangles,
    const fs::path& output_path,
    const std::string& node_name,
    const fs::path& shared_texture_dir,
    std::optional<std::array<float, 3>> node_translation = std::nullopt) {

    DynamicBuffer buffer;
    std::vector<BufferViewDef> buffer_views;
    std::vector<AccessorDef> accessors;
    std::vector<ImageDef> images;
    std::vector<TextureDef> textures;
    std::vector<MaterialDef> materials;
    std::vector<PrimitiveDef> primitives;
    std::map<std::string, uint32_t> material_indices;
    std::map<fs::path, uint32_t> texture_indices;

    const fs::path output_dir = output_path.parent_path();
    const fs::path bin_path = output_path.parent_path() / (output_path.stem().string() + ".bin");

    auto get_material_index = [&](const std::string& name) -> uint32_t {
        const auto found = material_indices.find(name);
        if (found != material_indices.end()) {
            return found->second;
        }

        MaterialDef def;
        def.name = name.empty() ? "default" : name;

        const auto material_it = scene.materials.find(name);
        if (material_it != scene.materials.end()) {
            const auto& src = material_it->second;
            def.base_color_factor = {src.diffuse[0], src.diffuse[1], src.diffuse[2], src.alpha};
            def.alpha_blend = src.alpha < 0.999f;

            if (!src.embedded_texture_bytes.empty()) {
                const fs::path synthetic_key = fs::path("__embedded__") / src.embedded_texture_name;
                auto tex_it = texture_indices.find(synthetic_key);
                if (tex_it == texture_indices.end()) {
                    const fs::path texture_path = shared_texture_dir / src.embedded_texture_name;
                    write_file_bytes(texture_path, src.embedded_texture_bytes);
                    const fs::path relative_uri = fs::relative(texture_path, output_dir);
                    images.push_back({std::nullopt, src.embedded_texture_mime_type, src.embedded_texture_name, relative_uri.generic_string()});
                    textures.push_back({static_cast<uint32_t>(images.size() - 1)});
                    const uint32_t texture_index = static_cast<uint32_t>(textures.size() - 1);
                    texture_indices[synthetic_key] = texture_index;
                    tex_it = texture_indices.find(synthetic_key);
                }
                def.base_color_texture = tex_it->second;
            } else if (src.has_texture && fs::exists(src.texture_path)) {
                auto tex_it = texture_indices.find(src.texture_path);
                if (tex_it == texture_indices.end()) {
                    const fs::path texture_path = shared_texture_dir / src.texture_path.filename();
                    const auto loaded = load_texture_bytes(src.texture_path);
                    write_file_bytes(texture_path, loaded.bytes);
                    const fs::path relative_uri = fs::relative(texture_path, output_dir);
                    images.push_back({std::nullopt, loaded.mime_type, src.texture_path.filename().string(), relative_uri.generic_string()});
                    textures.push_back({static_cast<uint32_t>(images.size() - 1)});
                    const uint32_t texture_index = static_cast<uint32_t>(textures.size() - 1);
                    texture_indices[src.texture_path] = texture_index;
                    tex_it = texture_indices.find(src.texture_path);
                }
                def.base_color_texture = tex_it->second;
            }
        }

        const uint32_t index = static_cast<uint32_t>(materials.size());
        materials.push_back(def);
        material_indices[name] = index;
        return index;
    };

    std::map<std::string, std::vector<const Triangle*>> by_material;
    for (const auto* triangle : triangles) {
        by_material[triangle->material_name].push_back(triangle);
    }

    const std::array<float, 3> translation = node_translation.value_or(std::array<float, 3>{0.0f, 0.0f, 0.0f});

    for (const auto& [material_name, tris] : by_material) {
        std::vector<float> positions_flat;
        std::vector<float> normals_flat;
        std::vector<float> uvs_flat;
        std::vector<std::uint32_t> indices;
        std::map<VertexKey, std::uint32_t> vertex_map;

        std::array<float, 3> min_pos{
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max()
        };
        std::array<float, 3> max_pos{
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest()
        };

        for (const auto* triangle : tris) {
            std::array<Vec3, 3> tri_positions = {
                scene.positions[triangle->refs[0].v],
                scene.positions[triangle->refs[1].v],
                scene.positions[triangle->refs[2].v],
            };
            const Vec3 face_normal = normalize(cross(
                subtract(tri_positions[1], tri_positions[0]),
                subtract(tri_positions[2], tri_positions[0])));

            for (int i = 0; i < 3; ++i) {
                const auto& ref = triangle->refs[i];
                const Vec3 position = {
                    scene.positions[ref.v].x + translation[0],
                    scene.positions[ref.v].y + translation[1],
                    scene.positions[ref.v].z + translation[2]
                };
                const Vec2 uv = (ref.vt >= 0 && ref.vt < static_cast<int>(scene.texcoords.size()))
                    ? scene.texcoords[ref.vt]
                    : Vec2{};
                const Vec3 normal = (ref.vn >= 0 && ref.vn < static_cast<int>(scene.normals.size()))
                    ? normalize(scene.normals[ref.vn])
                    : face_normal;

                VertexKey key;
                key.v = ref.v;
                key.vt = ref.vt;
                key.normal_quantized = quantize_normal(normal);

                auto it = vertex_map.find(key);
                if (it == vertex_map.end()) {
                    const std::uint32_t index = static_cast<std::uint32_t>(positions_flat.size() / 3);
                    vertex_map[key] = index;
                    positions_flat.push_back(position.x);
                    positions_flat.push_back(position.y);
                    positions_flat.push_back(position.z);
                    normals_flat.push_back(normal.x);
                    normals_flat.push_back(normal.y);
                    normals_flat.push_back(normal.z);
                    uvs_flat.push_back(uv.x);
                    uvs_flat.push_back(uv.y);
                    min_pos[0] = std::min(min_pos[0], position.x);
                    min_pos[1] = std::min(min_pos[1], position.y);
                    min_pos[2] = std::min(min_pos[2], position.z);
                    max_pos[0] = std::max(max_pos[0], position.x);
                    max_pos[1] = std::max(max_pos[1], position.y);
                    max_pos[2] = std::max(max_pos[2], position.z);
                    indices.push_back(index);
                } else {
                    indices.push_back(it->second);
                }
            }
        }

        const auto [pos_offset, pos_length] = buffer.append(pack_floats(positions_flat), 4);
        buffer_views.push_back({pos_offset, pos_length, 34962u});
        const uint32_t pos_bv = static_cast<uint32_t>(buffer_views.size() - 1);

        const auto [normal_offset, normal_length] = buffer.append(pack_floats(normals_flat), 4);
        buffer_views.push_back({normal_offset, normal_length, 34962u});
        const uint32_t normal_bv = static_cast<uint32_t>(buffer_views.size() - 1);

        const auto [uv_offset, uv_length] = buffer.append(pack_floats(uvs_flat), 4);
        buffer_views.push_back({uv_offset, uv_length, 34962u});
        const uint32_t uv_bv = static_cast<uint32_t>(buffer_views.size() - 1);

        const auto [idx_offset, idx_length] = buffer.append(pack_u32(indices), 4);
        buffer_views.push_back({idx_offset, idx_length, 34963u});
        const uint32_t idx_bv = static_cast<uint32_t>(buffer_views.size() - 1);

        accessors.push_back({pos_bv, 5126u, static_cast<uint32_t>(positions_flat.size() / 3), "VEC3", min_pos, max_pos});
        const uint32_t pos_acc = static_cast<uint32_t>(accessors.size() - 1);
        accessors.push_back({normal_bv, 5126u, static_cast<uint32_t>(normals_flat.size() / 3), "VEC3", std::nullopt, std::nullopt});
        const uint32_t normal_acc = static_cast<uint32_t>(accessors.size() - 1);
        accessors.push_back({uv_bv, 5126u, static_cast<uint32_t>(uvs_flat.size() / 2), "VEC2", std::nullopt, std::nullopt});
        const uint32_t uv_acc = static_cast<uint32_t>(accessors.size() - 1);
        accessors.push_back({idx_bv, 5125u, static_cast<uint32_t>(indices.size()), "SCALAR", std::nullopt, std::nullopt});
        const uint32_t idx_acc = static_cast<uint32_t>(accessors.size() - 1);

        primitives.push_back({pos_acc, normal_acc, uv_acc, idx_acc, get_material_index(material_name)});
    }

    while (buffer.data.size() % 4 != 0) {
        buffer.data.push_back(0);
    }

    std::ostringstream json;
    json << "{\"asset\":{\"version\":\"2.0\",\"generator\":\"trlevel_exporter\"},";
    json << "\"scene\":0,";
    json << "\"scenes\":[{\"nodes\":[0]}],";
    json << "\"nodes\":[{\"mesh\":0,\"name\":\"" << escape_json(node_name) << "\"";
    if (node_translation.has_value()) {
        const auto& t = *node_translation;
        json << ",\"translation\":[" << t[0] << ',' << t[1] << ',' << t[2] << "]";
    }
    json << "}],";
    json << "\"meshes\":[{\"name\":\"" << escape_json(node_name) << "\",\"primitives\":[";
    for (std::size_t i = 0; i < primitives.size(); ++i) {
        const auto& primitive = primitives[i];
        if (i > 0) {
            json << ',';
        }
        json << "{\"attributes\":{\"POSITION\":" << primitive.position_accessor
             << ",\"NORMAL\":" << primitive.normal_accessor
             << ",\"TEXCOORD_0\":" << primitive.uv_accessor
             << "},\"indices\":" << primitive.index_accessor
             << ",\"material\":" << primitive.material_index
             << "}";
    }
    json << "]}],";

    json << "\"buffers\":[{\"byteLength\":" << buffer.data.size()
         << ",\"uri\":\"" << escape_json(bin_path.filename().generic_string()) << "\"}],";

    json << "\"bufferViews\":[";
    for (std::size_t i = 0; i < buffer_views.size(); ++i) {
        const auto& view = buffer_views[i];
        if (i > 0) {
            json << ',';
        }
        json << "{\"buffer\":0,\"byteOffset\":" << view.byte_offset << ",\"byteLength\":" << view.byte_length;
        if (view.target.has_value()) {
            json << ",\"target\":" << *view.target;
        }
        json << "}";
    }
    json << "],";

    json << "\"accessors\":[";
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        const auto& accessor = accessors[i];
        if (i > 0) {
            json << ',';
        }
        json << "{\"bufferView\":" << accessor.buffer_view
             << ",\"componentType\":" << accessor.component_type
             << ",\"count\":" << accessor.count
             << ",\"type\":\"" << accessor.type << "\"";
        if (accessor.min_vec3.has_value()) {
            const auto& minv = *accessor.min_vec3;
            json << ",\"min\":[" << minv[0] << ',' << minv[1] << ',' << minv[2] << "]";
        }
        if (accessor.max_vec3.has_value()) {
            const auto& maxv = *accessor.max_vec3;
            json << ",\"max\":[" << maxv[0] << ',' << maxv[1] << ',' << maxv[2] << "]";
        }
        json << "}";
    }
    json << "],";

    json << "\"samplers\":[{\"magFilter\":9729,\"minFilter\":9987,\"wrapS\":10497,\"wrapT\":10497}],";

    json << "\"images\":[";
    for (std::size_t i = 0; i < images.size(); ++i) {
        const auto& image = images[i];
        if (i > 0) {
            json << ',';
        }
        json << "{";
        bool needs_comma = false;
        if (image.buffer_view.has_value()) {
            json << "\"bufferView\":" << *image.buffer_view;
            needs_comma = true;
        }
        if (!image.mime_type.empty()) {
            if (needs_comma) {
                json << ',';
            }
            json << "\"mimeType\":\"" << image.mime_type << "\"";
            needs_comma = true;
        }
        if (image.uri.has_value()) {
            if (needs_comma) {
                json << ',';
            }
            json << "\"uri\":\"" << escape_json(*image.uri) << "\"";
            needs_comma = true;
        }
        if (needs_comma) {
            json << ',';
        }
        json << "\"name\":\"" << escape_json(image.name) << "\"}";
    }
    json << "],";

    json << "\"textures\":[";
    for (std::size_t i = 0; i < textures.size(); ++i) {
        const auto& texture = textures[i];
        if (i > 0) {
            json << ',';
        }
        json << "{\"sampler\":0,\"source\":" << texture.source << "}";
    }
    json << "],";

    json << "\"materials\":[";
    for (std::size_t i = 0; i < materials.size(); ++i) {
        const auto& material = materials[i];
        if (i > 0) {
            json << ',';
        }
        json << "{\"name\":\"" << escape_json(material.name) << "\",\"pbrMetallicRoughness\":{\"baseColorFactor\":["
             << material.base_color_factor[0] << ',' << material.base_color_factor[1] << ','
             << material.base_color_factor[2] << ',' << material.base_color_factor[3]
             << "],\"metallicFactor\":0.0,\"roughnessFactor\":1.0";
        if (material.base_color_texture.has_value()) {
            json << ",\"baseColorTexture\":{\"index\":" << *material.base_color_texture << "}";
        }
        json << "}";
        if (material.alpha_blend) {
            json << ",\"alphaMode\":\"BLEND\"";
        }
        json << "}";
    }
    json << "]}";

    if (!output_dir.empty()) {
        fs::create_directories(output_dir);
    }
    write_file_bytes(bin_path, buffer.data);
    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("No se pudo escribir el glTF: " + output_path.string());
    }
    out << json.str();

    return {output_path.filename().string(), triangles.size()};
}

static std::array<float, 3> min_vec() {
    return {
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()
    };
}

static std::array<float, 3> max_vec() {
    return {
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest()
    };
}

static std::pair<std::array<float, 3>, std::array<float, 3>> bounds_for_triangles(const ObjScene& scene, const std::vector<const Triangle*>& triangles) {
    auto minv = min_vec();
    auto maxv = max_vec();
    for (const auto* triangle : triangles) {
        for (const auto& ref : triangle->refs) {
            const auto& p = scene.positions[ref.v];
            minv[0] = std::min(minv[0], p.x);
            minv[1] = std::min(minv[1], p.y);
            minv[2] = std::min(minv[2], p.z);
            maxv[0] = std::max(maxv[0], p.x);
            maxv[1] = std::max(maxv[1], p.y);
            maxv[2] = std::max(maxv[2], p.z);
        }
    }
    return {minv, maxv};
}

static void write_manifest(
    const ObjScene& scene,
    const std::map<std::pair<int, int>, std::vector<const Triangle*>>& sectors,
    const fs::path& output_dir,
    double sector_size,
    const std::string& manifest_name,
    const std::string& sector_extension = ".glb") {

    std::ofstream out(output_dir / manifest_name);
    if (!out) {
        throw std::runtime_error("No se pudo escribir el manifest");
    }

    out << "{\n";
    out << "  \"source\": \"OBJ\",\n";
    out << "  \"sector_size\": " << sector_size << ",\n";
    out << "  \"sector_count\": " << sectors.size() << ",\n";
    out << "  \"sectors\": [\n";

    std::size_t index = 0;
    for (const auto& [sector, triangles] : sectors) {
        const auto [minv, maxv] = bounds_for_triangles(scene, triangles);
        const std::string file_name = "sector_" + std::to_string(sector.first) + "_" + std::to_string(sector.second) + sector_extension;

        out << "    {\n";
        out << "      \"sector\": [" << sector.first << ", " << sector.second << "],\n";
        out << "      \"file\": \"" << file_name << "\",\n";
        out << "      \"triangles\": " << triangles.size() << ",\n";
        out << "      \"min\": [" << minv[0] << ", " << minv[1] << ", " << minv[2] << "],\n";
        out << "      \"max\": [" << maxv[0] << ", " << maxv[1] << ", " << maxv[2] << "]\n";
        out << "    }";
        if (++index < sectors.size()) {
            out << ",";
        }
        out << "\n";
    }

    out << "  ]\n";
    out << "}\n";
}

struct ViewerVertex {
    float x;
    float y;
    float z;
    float u;
    float v;
};

struct ViewerBatch {
    GLuint texture_id = 0;
    std::vector<ViewerVertex> vertices;
};

struct LoadedViewerScene {
    ObjScene scene;
    fs::path source_path;
    std::map<std::string, ViewerBatch> batches;
    Vec3 center{};
    float distance = 1024.0f;
    bool valid = false;
};

static std::vector<std::uint8_t> rgba_from_material(const Material& material, int& width, int& height) {
    int channels = 0;
    unsigned char* pixels = nullptr;
    if (!material.embedded_texture_bytes.empty()) {
        pixels = stbi_load_from_memory(
            material.embedded_texture_bytes.data(),
            static_cast<int>(material.embedded_texture_bytes.size()),
            &width,
            &height,
            &channels,
            4);
    } else if (material.has_texture && fs::exists(material.texture_path)) {
        pixels = stbi_load(material.texture_path.string().c_str(), &width, &height, &channels, 4);
    }

    if (!pixels) {
        width = 1;
        height = 1;
        return {255, 255, 255, 255};
    }

    std::vector<std::uint8_t> rgba(pixels, pixels + width * height * 4);
    stbi_image_free(pixels);
    return rgba;
}

static LoadedViewerScene build_viewer_scene(const ObjScene& scene, const fs::path& source_path) {
    LoadedViewerScene loaded;
    loaded.scene = scene;
    loaded.source_path = source_path;

    for (const auto& triangle : loaded.scene.triangles) {
        auto& batch = loaded.batches[triangle.material_name];
        for (const auto& ref : triangle.refs) {
            const auto& pos = loaded.scene.positions[ref.v];
            const Vec2 uv = (ref.vt >= 0 && ref.vt < static_cast<int>(loaded.scene.texcoords.size()))
                ? loaded.scene.texcoords[ref.vt]
                : Vec2{};
            batch.vertices.push_back({pos.x, pos.y, pos.z, uv.x, uv.y});
        }
    }

    Vec3 minp{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    Vec3 maxp{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
    for (const auto& p : loaded.scene.positions) {
        minp.x = std::min(minp.x, p.x);
        minp.y = std::min(minp.y, p.y);
        minp.z = std::min(minp.z, p.z);
        maxp.x = std::max(maxp.x, p.x);
        maxp.y = std::max(maxp.y, p.y);
        maxp.z = std::max(maxp.z, p.z);
    }
    loaded.center = {(minp.x + maxp.x) * 0.5f, (minp.y + maxp.y) * 0.5f, (minp.z + maxp.z) * 0.5f};
    const float dx = maxp.x - minp.x;
    const float dy = maxp.y - minp.y;
    const float dz = maxp.z - minp.z;
    loaded.distance = std::max({dx, dy, dz}) * 1.5f;
    if (loaded.distance < 1.0f) {
        loaded.distance = 1024.0f;
    }
    loaded.valid = true;
    return loaded;
}

static void destroy_viewer_scene_gl(LoadedViewerScene& loaded) {
    for (auto& [_, batch] : loaded.batches) {
        if (batch.texture_id != 0) {
            glDeleteTextures(1, &batch.texture_id);
            batch.texture_id = 0;
        }
    }
}

static void upload_viewer_scene_gl(LoadedViewerScene& loaded) {
    for (auto& [material_name, batch] : loaded.batches) {
        glGenTextures(1, &batch.texture_id);
        glBindTexture(GL_TEXTURE_2D, batch.texture_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

        int width = 1;
        int height = 1;
        std::vector<std::uint8_t> rgba{255, 255, 255, 255};
        const auto it = loaded.scene.materials.find(material_name);
        if (it != loaded.scene.materials.end()) {
            rgba = rgba_from_material(it->second, width, height);
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    }
}

static ObjScene load_scene_from_path(const fs::path& input) {
    const bool is_obj_input = lower_copy(input.extension().string()) == ".obj";
    return is_obj_input ? parse_obj(input) : parse_native_level(input);
}

static ObjScene scene_with_transform(const ObjScene& scene, const std::array<float, 3>& translation, float scale) {
    ObjScene transformed = scene;
    for (auto& position : transformed.positions) {
        position.x = (position.x + translation[0]) * scale;
        position.y = (position.y + translation[1]) * scale;
        position.z = (position.z + translation[2]) * scale;
    }
    return transformed;
}

static std::optional<fs::path> pick_level_file_with_zenity(const fs::path& suggested_path) {
    std::string filename_arg;
    if (!suggested_path.empty()) {
        if (fs::is_directory(suggested_path)) {
            filename_arg = suggested_path.string();
        } else if (suggested_path.has_parent_path()) {
            filename_arg = suggested_path.parent_path().string();
        } else {
            filename_arg = suggested_path.string();
        }
    } else {
        filename_arg = fs::current_path().string();
    }
    if (!filename_arg.empty() && filename_arg.back() != '/') {
        filename_arg.push_back('/');
    }

    const std::string command =
        "zenity --file-selection "
        "--title=" + shell_single_quote("Examinar nivel Tomb Raider") + " "
        "--filename=" + shell_single_quote(filename_arg) + " "
        "--file-filter=" + shell_single_quote("Niveles | *.phd *.PHD *.tub *.TUB *.tr2 *.TR2 *.obj *.OBJ") + " "
        "--file-filter=" + shell_single_quote("Todos | *");

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return std::nullopt;
    }

    std::string output;
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    const int exit_code = pclose(pipe);
    if (exit_code != 0) {
        return std::nullopt;
    }

    while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
        output.pop_back();
    }
    if (output.empty()) {
        return std::nullopt;
    }
    return fs::path(output);
}

static void set_perspective(double fov_y_deg, double aspect, double z_near, double z_far) {
    const double top = std::tan(fov_y_deg * 0.5 * 3.14159265358979323846 / 180.0) * z_near;
    const double bottom = -top;
    const double right = top * aspect;
    const double left = -right;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(left, right, bottom, top, z_near, z_far);
}

class QtLevelView final : public QOpenGLWidget {
public:
    explicit QtLevelView(QWidget* parent = nullptr) : QOpenGLWidget(parent) {
        setMinimumSize(640, 480);
    }

    ~QtLevelView() override {
        if (context()) {
            makeCurrent();
            destroy_viewer_scene_gl(scene_);
            doneCurrent();
        }
    }

    void setLoadedScene(const LoadedViewerScene& scene) {
        if (context()) {
            makeCurrent();
            destroy_viewer_scene_gl(scene_);
            doneCurrent();
        }
        scene_ = scene;
        yaw_ = 0.8f;
        pitch_ = 0.6f;
        distance_ = scene_.valid ? scene_.distance : 1024.0f;
        if (context() && scene_.valid) {
            makeCurrent();
            upload_viewer_scene_gl(scene_);
            doneCurrent();
        }
        update();
    }

    void resetCamera() {
        yaw_ = 0.8f;
        pitch_ = 0.6f;
        distance_ = scene_.valid ? scene_.distance : 1024.0f;
        update();
    }

    bool hasScene() const {
        return scene_.valid;
    }

private:
    void initializeGL() override {
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_TEXTURE_2D);
        if (scene_.valid) {
            upload_viewer_scene_gl(scene_);
        }
    }

    void paintGL() override {
        glViewport(0, 0, width(), height());
        glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (!scene_.valid) {
            return;
        }

        set_perspective(60.0, static_cast<double>(std::max(width(), 1)) / static_cast<double>(std::max(height(), 1)), 1.0, std::max(100000.0, static_cast<double>(distance_) * 10.0));
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef(0.0f, 0.0f, -distance_);
        glRotatef(pitch_ * 57.29578f, 1.0f, 0.0f, 0.0f);
        glRotatef(yaw_ * 57.29578f, 0.0f, 1.0f, 0.0f);
        glTranslatef(-scene_.center.x, -scene_.center.y, -scene_.center.z);

        glColor3f(1.0f, 1.0f, 1.0f);
        for (const auto& [_, batch] : scene_.batches) {
            glBindTexture(GL_TEXTURE_2D, batch.texture_id);
            glBegin(GL_TRIANGLES);
            for (const auto& vertex : batch.vertices) {
                glTexCoord2f(vertex.u, vertex.v);
                glVertex3f(vertex.x, vertex.y, vertex.z);
            }
            glEnd();
        }
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            rotating_ = true;
            last_pos_ = event->pos();
        }
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            rotating_ = false;
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (!rotating_ || !scene_.valid) {
            return;
        }
        const QPoint delta = event->pos() - last_pos_;
        yaw_ += static_cast<float>(delta.x()) * 0.01f;
        pitch_ += static_cast<float>(delta.y()) * 0.01f;
        pitch_ = std::clamp(pitch_, -1.4f, 1.4f);
        last_pos_ = event->pos();
        update();
    }

    void wheelEvent(QWheelEvent* event) override {
        if (!scene_.valid) {
            return;
        }
        distance_ *= event->angleDelta().y() > 0 ? 0.9f : 1.1f;
        distance_ = std::max(distance_, 32.0f);
        update();
    }

    LoadedViewerScene scene_;
    float yaw_ = 0.8f;
    float pitch_ = 0.6f;
    float distance_ = 1024.0f;
    bool rotating_ = false;
    QPoint last_pos_;
};

static int run_qt_gui_app(
    const std::optional<fs::path>& initial_path,
    double initial_sector_size,
    double initial_export_scale) {

    int argc = 1;
    char app_name[] = "trlevel_exporter";
    char* argv[] = {app_name, nullptr};
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle("TR Native Converter");
    window.resize(1600, 900);

    auto* root = new QHBoxLayout(&window);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    auto* left_panel = new QWidget(&window);
    left_panel->setFixedWidth(380);
    auto* left_layout = new QVBoxLayout(left_panel);
    left_layout->setContentsMargins(0, 0, 0, 0);
    left_layout->setSpacing(8);

    auto* title = new QLabel("Tomb Raider GLB", left_panel);
    auto* help = new QLabel("GUI Qt nativa: carga, visualiza y exporta niveles TR1/TR2.", left_panel);
    help->setWordWrap(true);
    auto* path_edit = new QLineEdit(left_panel);
    auto* button_row = new QHBoxLayout();
    auto* browse_button = new QPushButton("Examinar...", left_panel);
    auto* load_button = new QPushButton("Cargar", left_panel);
    auto* reset_button = new QPushButton("Recentrar", left_panel);
    button_row->addWidget(browse_button);
    button_row->addWidget(load_button);
    button_row->addWidget(reset_button);

    auto* sector_label = new QLabel("Tam sector", left_panel);
    auto* sector_spin = new QDoubleSpinBox(left_panel);
    sector_spin->setDecimals(0);
    sector_spin->setRange(1.0, 1000000.0);
    sector_spin->setSingleStep(128.0);
    sector_spin->setValue(initial_sector_size > 0.0 ? initial_sector_size : 2048.0);

    auto* scale_label = new QLabel("Escala export", left_panel);
    auto* scale_spin = new QDoubleSpinBox(left_panel);
    scale_spin->setDecimals(4);
    scale_spin->setRange(0.0001, 1000.0);
    scale_spin->setSingleStep(0.001);
    scale_spin->setValue(initial_export_scale > 0.0 ? initial_export_scale : 0.001);

    auto* export_glb_button = new QPushButton("Exportar GLB", left_panel);
    auto* export_sectors_button = new QPushButton("Exportar Sectores", left_panel);
    auto* status_box = new QTextEdit(left_panel);
    status_box->setReadOnly(true);
    status_box->setMinimumHeight(180);

    left_layout->addWidget(title);
    left_layout->addWidget(help);
    left_layout->addWidget(path_edit);
    left_layout->addLayout(button_row);
    left_layout->addWidget(sector_label);
    left_layout->addWidget(sector_spin);
    left_layout->addWidget(scale_label);
    left_layout->addWidget(scale_spin);
    left_layout->addWidget(export_glb_button);
    left_layout->addWidget(export_sectors_button);
    left_layout->addWidget(status_box, 1);

    auto* viewer = new QtLevelView(&window);
    root->addWidget(left_panel);
    root->addWidget(viewer, 1);

    LoadedViewerScene loaded;
    auto append_status = [&](const std::string& text) {
        status_box->append(QString::fromStdString(text));
    };
    append_status("Listo. Usa Examinar... para cargar un nivel.");

    auto load_path = [&](const fs::path& path) {
        try {
            loaded = build_viewer_scene(load_scene_from_path(path), path);
            viewer->setLoadedScene(loaded);
            path_edit->setText(QString::fromStdString(path.string()));
            append_status("Cargado: " + path.filename().string());
            append_status("Triangulos: " + std::to_string(loaded.scene.triangles.size()));
        } catch (const std::exception& ex) {
            append_status(std::string("Error al cargar: ") + ex.what());
        }
    };

    QObject::connect(browse_button, &QPushButton::clicked, [&]() {
        const QString path = QFileDialog::getOpenFileName(
            &window,
            "Examinar nivel Tomb Raider",
            path_edit->text(),
            "Niveles (*.phd *.PHD *.tub *.TUB *.tr2 *.TR2 *.obj *.OBJ);;Todos (*)");
        if (!path.isEmpty()) {
            load_path(fs::path(path.toStdString()));
        }
    });

    QObject::connect(load_button, &QPushButton::clicked, [&]() {
        if (!path_edit->text().isEmpty()) {
            load_path(fs::path(path_edit->text().toStdString()));
        }
    });

    QObject::connect(reset_button, &QPushButton::clicked, [&]() {
        viewer->resetCamera();
    });

    QObject::connect(export_glb_button, &QPushButton::clicked, [&]() {
        if (!loaded.valid) {
            append_status("No hay escena cargada.");
            return;
        }
        const QString default_path = QString::fromStdString((loaded.source_path.parent_path() / (loaded.source_path.stem().string() + ".glb")).string());
        const QString out = QFileDialog::getSaveFileName(&window, "Guardar GLB", default_path, "GLB (*.glb)");
        if (out.isEmpty()) {
            return;
        }
        std::vector<const Triangle*> triangles;
        triangles.reserve(loaded.scene.triangles.size());
        for (const auto& triangle : loaded.scene.triangles) {
            triangles.push_back(&triangle);
        }
        const auto [minv, maxv] = bounds_for_triangles(loaded.scene, triangles);
        const std::array<float, 3> translation{
            -((minv[0] + maxv[0]) * 0.5f),
            -((minv[1] + maxv[1]) * 0.5f),
            -((minv[2] + maxv[2]) * 0.5f)
        };
        const ObjScene transformed = scene_with_transform(loaded.scene, translation, static_cast<float>(scale_spin->value()));
        write_glb(transformed, triangles, fs::path(out.toStdString()), loaded.source_path.stem().string());
        append_status("Exportado GLB: " + out.toStdString());
    });

    QObject::connect(export_sectors_button, &QPushButton::clicked, [&]() {
        if (!loaded.valid) {
            append_status("No hay escena cargada.");
            return;
        }
        const QString out_dir = QFileDialog::getExistingDirectory(&window, "Elegir carpeta de sectores", QString::fromStdString(loaded.source_path.parent_path().string()));
        if (out_dir.isEmpty()) {
            return;
        }
        const auto sectors = sectorize(loaded.scene, sector_spin->value());
        const ObjScene transformed = scene_with_transform(loaded.scene, {0.0f, 0.0f, 0.0f}, static_cast<float>(scale_spin->value()));
        const fs::path output_root = fs::path(out_dir.toStdString());
        const fs::path texture_root = output_root / "textures";
        for (const auto& [sector, triangles] : sectors) {
            const fs::path out = output_root / ("sector_" + std::to_string(sector.first) + "_" + std::to_string(sector.second) + ".gltf");
            write_gltf(transformed, triangles, out, out.stem().string(), texture_root);
        }
        write_manifest(transformed, sectors, output_root, sector_spin->value() * scale_spin->value(), "manifest.json", ".gltf");
        append_status("Exportados sectores glTF con texturas compartidas: " + out_dir.toStdString());
    });

    if (initial_path.has_value()) {
        load_path(fs::absolute(*initial_path));
    }

    window.show();
    return app.exec();
}

static int run_viewer(const ObjScene& scene, const fs::path& source_path, double sector_size) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        throw std::runtime_error(std::string("No se pudo iniciar SDL2: ") + SDL_GetError());
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* window = SDL_CreateWindow(
        "TR Level Viewer",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1280,
        720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Quit();
        throw std::runtime_error(std::string("No se pudo crear la ventana: ") + SDL_GetError());
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        throw std::runtime_error(std::string("No se pudo crear contexto OpenGL: ") + SDL_GetError());
    }

    SDL_GL_SetSwapInterval(1);

    std::map<std::string, ViewerBatch> batches;
    for (const auto& triangle : scene.triangles) {
        auto& batch = batches[triangle.material_name];
        for (const auto& ref : triangle.refs) {
            const auto& pos = scene.positions[ref.v];
            const Vec2 uv = (ref.vt >= 0 && ref.vt < static_cast<int>(scene.texcoords.size()))
                ? scene.texcoords[ref.vt]
                : Vec2{};
            batch.vertices.push_back({pos.x, pos.y, pos.z, uv.x, uv.y});
        }
    }

    for (auto& [material_name, batch] : batches) {
        glGenTextures(1, &batch.texture_id);
        glBindTexture(GL_TEXTURE_2D, batch.texture_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

        int width = 1;
        int height = 1;
        std::vector<std::uint8_t> rgba{255, 255, 255, 255};
        const auto it = scene.materials.find(material_name);
        if (it != scene.materials.end()) {
            rgba = rgba_from_material(it->second, width, height);
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    }

    Vec3 minp{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    Vec3 maxp{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
    for (const auto& p : scene.positions) {
        minp.x = std::min(minp.x, p.x);
        minp.y = std::min(minp.y, p.y);
        minp.z = std::min(minp.z, p.z);
        maxp.x = std::max(maxp.x, p.x);
        maxp.y = std::max(maxp.y, p.y);
        maxp.z = std::max(maxp.z, p.z);
    }
    const Vec3 center{(minp.x + maxp.x) * 0.5f, (minp.y + maxp.y) * 0.5f, (minp.z + maxp.z) * 0.5f};
    const float dx = maxp.x - minp.x;
    const float dy = maxp.y - minp.y;
    const float dz = maxp.z - minp.z;
    float distance = std::max({dx, dy, dz}) * 1.5f;
    if (distance < 1.0f) {
        distance = 1024.0f;
    }

    bool running = true;
    bool rotating = false;
    float yaw = 0.8f;
    float pitch = 0.6f;
    int last_x = 0;
    int last_y = 0;
    const fs::path export_root = fs::absolute(fs::path("/home/ruben/BennuGD2/build/trlevel_exports"));
    fs::create_directories(export_root);

    SDL_SetWindowTitle(window, "TR Viewer | raton: orbita | rueda: zoom | G: GLB | T: sectores | Esc: salir");

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                } else if (event.key.keysym.sym == SDLK_g) {
                    std::vector<const Triangle*> triangles;
                    triangles.reserve(scene.triangles.size());
                    for (const auto& triangle : scene.triangles) {
                        triangles.push_back(&triangle);
                    }
                    const fs::path out = export_root / (source_path.stem().string() + ".glb");
                    const auto [minv, maxv] = bounds_for_triangles(scene, triangles);
                    const std::array<float, 3> translation{
                        -((minv[0] + maxv[0]) * 0.5f),
                        -((minv[1] + maxv[1]) * 0.5f),
                        -((minv[2] + maxv[2]) * 0.5f)
                    };
                    const ObjScene centered_scene = scene_with_transform(scene, translation, 0.001f);
                    write_glb(centered_scene, triangles, out, source_path.stem().string());
                    std::cout << "Exportado desde visor: " << out << "\n";
                } else if (event.key.keysym.sym == SDLK_t) {
                    const fs::path out_dir = export_root / (source_path.stem().string() + "_sectors");
                    fs::create_directories(out_dir);
                    const auto sectors = sectorize(scene, sector_size > 0.0 ? sector_size : 2048.0);
                    const fs::path texture_root = out_dir / "textures";
                    for (const auto& [sector, triangles] : sectors) {
                        const fs::path out = out_dir / ("sector_" + std::to_string(sector.first) + "_" + std::to_string(sector.second) + ".gltf");
                        write_gltf(scene, triangles, out, out.stem().string(), texture_root);
                    }
                    write_manifest(scene, sectors, out_dir, sector_size > 0.0 ? sector_size : 2048.0, "manifest.json", ".gltf");
                    std::cout << "Sectores exportados desde visor: " << out_dir << "\n";
                }
            } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                rotating = true;
                last_x = event.button.x;
                last_y = event.button.y;
            } else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
                rotating = false;
            } else if (event.type == SDL_MOUSEMOTION && rotating) {
                const int dx_mouse = event.motion.x - last_x;
                const int dy_mouse = event.motion.y - last_y;
                yaw += static_cast<float>(dx_mouse) * 0.01f;
                pitch += static_cast<float>(dy_mouse) * 0.01f;
                pitch = std::clamp(pitch, -1.4f, 1.4f);
                last_x = event.motion.x;
                last_y = event.motion.y;
            } else if (event.type == SDL_MOUSEWHEEL) {
                distance *= event.wheel.y > 0 ? 0.9f : 1.1f;
                distance = std::max(distance, 32.0f);
            }
        }

        int width = 1280;
        int height = 720;
        SDL_GetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_TEXTURE_2D);
        glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        set_perspective(60.0, static_cast<double>(width) / static_cast<double>(std::max(height, 1)), 1.0, std::max(100000.0, static_cast<double>(distance) * 10.0));
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef(0.0f, 0.0f, -distance);
        glRotatef(pitch * 57.29578f, 1.0f, 0.0f, 0.0f);
        glRotatef(yaw * 57.29578f, 0.0f, 1.0f, 0.0f);
        glTranslatef(-center.x, -center.y, -center.z);

        glColor3f(1.0f, 1.0f, 1.0f);
        for (const auto& [_, batch] : batches) {
            glBindTexture(GL_TEXTURE_2D, batch.texture_id);
            glBegin(GL_TRIANGLES);
            for (const auto& vertex : batch.vertices) {
                glTexCoord2f(vertex.u, vertex.v);
                glVertex3f(vertex.x, vertex.y, vertex.z);
            }
            glEnd();
        }

        SDL_GL_SwapWindow(window);
    }

    for (auto& [_, batch] : batches) {
        if (batch.texture_id != 0) {
            glDeleteTextures(1, &batch.texture_id);
        }
    }
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

static int run_gui_app(const std::optional<fs::path>& initial_path) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        throw std::runtime_error(std::string("No se pudo iniciar SDL2: ") + SDL_GetError());
    }

    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* window = SDL_CreateWindow(
        "TR Native Converter",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1400,
        900,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Quit();
        throw std::runtime_error(std::string("No se pudo crear la ventana: ") + SDL_GetError());
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        throw std::runtime_error(std::string("No se pudo crear contexto OpenGL: ") + SDL_GetError());
    }

    SDL_GL_SetSwapInterval(1);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 120");

    LoadedViewerScene loaded;
    char path_buffer[4096]{0};
    double sector_size = 2048.0;
    double export_scale = 0.001;
    std::string status = "Arrastra un nivel o escribe una ruta y pulsa Cargar.";
    float yaw = 0.8f;
    float pitch = 0.6f;
    bool rotating = false;
    int last_x = 0;
    int last_y = 0;
    bool running = true;
    const fs::path export_root = fs::absolute(fs::path("/home/ruben/BennuGD2/build/trlevel_exports"));
    fs::create_directories(export_root);

    auto load_path = [&](const fs::path& path) {
        try {
            destroy_viewer_scene_gl(loaded);
            loaded = build_viewer_scene(load_scene_from_path(path), path);
            upload_viewer_scene_gl(loaded);
            std::snprintf(path_buffer, sizeof(path_buffer), "%s", path.string().c_str());
            status = "Cargado: " + path.filename().string();
            yaw = 0.8f;
            pitch = 0.6f;
        } catch (const std::exception& ex) {
            status = std::string("Error al cargar: ") + ex.what();
        }
    };

    if (initial_path.has_value()) {
        load_path(fs::absolute(*initial_path));
    }

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_DROPFILE) {
                load_path(fs::path(event.drop.file));
                SDL_free(event.drop.file);
            } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT && !io.WantCaptureMouse) {
                rotating = true;
                last_x = event.button.x;
                last_y = event.button.y;
            } else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
                rotating = false;
            } else if (event.type == SDL_MOUSEMOTION && rotating && !io.WantCaptureMouse) {
                yaw += static_cast<float>(event.motion.x - last_x) * 0.01f;
                pitch += static_cast<float>(event.motion.y - last_y) * 0.01f;
                pitch = std::clamp(pitch, -1.4f, 1.4f);
                last_x = event.motion.x;
                last_y = event.motion.y;
            } else if (event.type == SDL_MOUSEWHEEL && !io.WantCaptureMouse && loaded.valid) {
                loaded.distance *= event.wheel.y > 0 ? 0.9f : 1.1f;
                loaded.distance = std::max(loaded.distance, 32.0f);
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(500, 250), ImGuiCond_Once);
        ImGui::Begin("Tomb Raider GLB");
        ImGui::TextWrapped("GUI nativa: carga, visualiza y exporta niveles TR1/TR2 a GLB.");
        ImGui::TextWrapped("El visor 3D se muestra en la parte derecha de la ventana.");
        ImGui::InputText("Ruta", path_buffer, sizeof(path_buffer));
        if (ImGui::Button("Examinar...")) {
            const fs::path suggested = path_buffer[0] != '\0' ? fs::path(path_buffer) : fs::current_path();
            const auto picked = pick_level_file_with_zenity(suggested);
            if (picked.has_value()) {
                load_path(fs::absolute(*picked));
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cargar")) {
            load_path(fs::absolute(fs::path(path_buffer)));
        }
        ImGui::SameLine();
        if (ImGui::Button("Recentrar") && loaded.valid) {
            yaw = 0.8f;
            pitch = 0.6f;
        }
        ImGui::InputDouble("Tam sector", &sector_size, 128.0, 512.0, "%.0f");
        ImGui::InputDouble("Escala GLB", &export_scale, 0.0005, 0.005, "%.4f");
        if (!loaded.valid) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Exportar GLB")) {
            std::vector<const Triangle*> triangles;
            triangles.reserve(loaded.scene.triangles.size());
            for (const auto& triangle : loaded.scene.triangles) {
                triangles.push_back(&triangle);
            }
            const fs::path out = export_root / (loaded.source_path.stem().string() + ".glb");
            const auto [minv, maxv] = bounds_for_triangles(loaded.scene, triangles);
            const std::array<float, 3> translation{
                -((minv[0] + maxv[0]) * 0.5f),
                -((minv[1] + maxv[1]) * 0.5f),
                -((minv[2] + maxv[2]) * 0.5f)
            };
            const ObjScene centered_scene = scene_with_transform(loaded.scene, translation, static_cast<float>(export_scale));
            write_glb(centered_scene, triangles, out, loaded.source_path.stem().string());
            status = "Exportado GLB: " + out.string();
        }
        ImGui::SameLine();
        if (ImGui::Button("Exportar Sectores")) {
            const fs::path out_dir = export_root / (loaded.source_path.stem().string() + "_sectors");
            fs::create_directories(out_dir);
            const auto sectors = sectorize(loaded.scene, sector_size);
            const fs::path texture_root = out_dir / "textures";
            const ObjScene scaled_scene = scene_with_transform(loaded.scene, {0.0f, 0.0f, 0.0f}, static_cast<float>(export_scale));
            for (const auto& [sector, triangles] : sectors) {
                const fs::path out = out_dir / ("sector_" + std::to_string(sector.first) + "_" + std::to_string(sector.second) + ".gltf");
                write_gltf(scaled_scene, triangles, out, out.stem().string(), texture_root);
            }
            write_manifest(scaled_scene, sectors, out_dir, sector_size * export_scale, "manifest.json", ".gltf");
            status = "Exportados sectores glTF con texturas compartidas: " + out_dir.string();
        }
        if (!loaded.valid) {
            ImGui::EndDisabled();
        }
        ImGui::Separator();
        ImGui::TextWrapped("%s", status.c_str());
        if (loaded.valid) {
            ImGui::Text("Archivo: %s", loaded.source_path.filename().string().c_str());
            ImGui::Text("Triangulos: %zu", loaded.scene.triangles.size());
        }
        ImGui::End();

        int width = 1280;
        int height = 720;
        SDL_GetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_TEXTURE_2D);
        glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (loaded.valid) {
            const int panel_width = 540;
            const int viewport_x = std::min(panel_width, std::max(0, width - 64));
            const int viewport_w = std::max(64, width - viewport_x);
            glViewport(viewport_x, 0, viewport_w, height);
            set_perspective(60.0, static_cast<double>(viewport_w) / static_cast<double>(std::max(height, 1)), 1.0, std::max(100000.0, static_cast<double>(loaded.distance) * 10.0));
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            glTranslatef(0.0f, 0.0f, -loaded.distance);
            glRotatef(pitch * 57.29578f, 1.0f, 0.0f, 0.0f);
            glRotatef(yaw * 57.29578f, 0.0f, 1.0f, 0.0f);
            glTranslatef(-loaded.center.x, -loaded.center.y, -loaded.center.z);

            glColor3f(1.0f, 1.0f, 1.0f);
            for (const auto& [_, batch] : loaded.batches) {
                glBindTexture(GL_TEXTURE_2D, batch.texture_id);
                glBegin(GL_TRIANGLES);
                for (const auto& vertex : batch.vertices) {
                    glTexCoord2f(vertex.u, vertex.v);
                    glVertex3f(vertex.x, vertex.y, vertex.z);
                }
                glEnd();
            }
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    destroy_viewer_scene_gl(loaded);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

static void print_usage() {
    std::cerr << "Uso:\n";
    std::cerr << "  trlevel_exporter\n";
    std::cerr << "  trlevel_exporter obj2glb <input.obj> --output <nivel.glb> [--scale <factor>]\n";
    std::cerr << "  trlevel_exporter obj2glb <input.obj> --output-dir <dir> --sector-size <tam> [--scale <factor>]  # sectores en glTF + BIN + texturas compartidas\n";
    std::cerr << "  trlevel_exporter level2glb <input.phd|input.tub|input.tr2> --output <nivel.glb> [--scale <factor>]\n";
    std::cerr << "  trlevel_exporter level2glb <input.phd|input.tub|input.tr2> --output-dir <dir> --sector-size <tam> [--scale <factor>]  # sectores en glTF + BIN + texturas compartidas\n";
    std::cerr << "  trlevel_exporter view <input.obj|input.phd|input.tub|input.tr2> [--sector-size <tam>] [--scale <factor>]\n";
    std::cerr << "  trlevel_exporter gui [input.phd|input.tub|input.tr2|input.obj]\n";
}

static CliOptions parse_cli(int argc, char** argv) {
    if (argc == 1) {
        CliOptions options;
        options.command = "gui";
        return options;
    }
    const std::string command = argv[1];
    if (command == "--help" || command == "-h") {
        print_usage();
        throw std::runtime_error("Ayuda solicitada");
    }
    if (argc < 3 && command != "gui") {
        print_usage();
        throw std::runtime_error("Argumentos insuficientes");
    }
    if (command != "obj2glb" && command != "level2glb" && command != "view" && command != "gui") {
        print_usage();
        throw std::runtime_error("Comando no soportado");
    }

    CliOptions options;
    options.command = command;
    if (argc >= 3 && argv[2][0] != '-') {
        options.input_obj = argv[2];
    }

    const int arg_start = options.input_obj.empty() ? 2 : 3;
    for (int i = arg_start; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--output" && i + 1 < argc) {
            options.output = argv[++i];
        } else if (arg == "--output-dir" && i + 1 < argc) {
            options.output_dir = argv[++i];
        } else if (arg == "--sector-size" && i + 1 < argc) {
            options.sector_size = std::stod(argv[++i]);
        } else if (arg == "--scale" && i + 1 < argc) {
            options.export_scale = std::stod(argv[++i]);
        } else if (arg == "--manifest-name" && i + 1 < argc) {
            options.manifest_name = argv[++i];
        } else {
            print_usage();
            throw std::runtime_error("Argumento no reconocido: " + arg);
        }
    }

    if (command == "view" || command == "gui") {
        return options;
    }

    if (options.sector_size > 0.0 && options.output_dir.empty()) {
        throw std::runtime_error("--output-dir es obligatorio con --sector-size");
    }
    if (options.sector_size <= 0.0 && options.output.empty()) {
        throw std::runtime_error("--output es obligatorio si no usas --sector-size");
    }
    return options;
}

int main(int argc, char** argv) {
    try {
        const CliOptions options = parse_cli(argc, argv);
        if (options.command == "gui") {
            const std::optional<fs::path> initial_path = options.input_obj.empty()
                ? std::nullopt
                : std::optional<fs::path>(fs::absolute(options.input_obj));
            return run_qt_gui_app(initial_path, options.sector_size, options.export_scale);
        }
        const fs::path input = fs::absolute(options.input_obj);
        const bool is_obj_input = lower_copy(input.extension().string()) == ".obj";
        const ObjScene scene = is_obj_input ? parse_obj(input) : parse_native_level(input);

        if (options.command == "view") {
            return run_qt_gui_app(input, options.sector_size, options.export_scale);
        }

        if (options.sector_size > 0.0) {
            fs::create_directories(options.output_dir);
            const auto sectors = sectorize(scene, options.sector_size);
            const ObjScene transformed = scene_with_transform(scene, {0.0f, 0.0f, 0.0f}, static_cast<float>(options.export_scale));
            const fs::path texture_root = options.output_dir / "textures";
            for (const auto& [sector, triangles] : sectors) {
                const fs::path output = options.output_dir / ("sector_" + std::to_string(sector.first) + "_" + std::to_string(sector.second) + ".gltf");
                write_gltf(transformed, triangles, output, output.stem().string(), texture_root);
                std::cout << "Escrito " << output << " con " << triangles.size() << " triangulos\n";
            }
            write_manifest(transformed, sectors, options.output_dir, options.sector_size * options.export_scale, options.manifest_name, ".gltf");
            std::cout << "Escrito " << (options.output_dir / options.manifest_name) << "\n";
        } else {
            std::vector<const Triangle*> triangles;
            triangles.reserve(scene.triangles.size());
            for (const auto& triangle : scene.triangles) {
                triangles.push_back(&triangle);
            }
            const auto [minv, maxv] = bounds_for_triangles(scene, triangles);
            const std::array<float, 3> translation{
                -((minv[0] + maxv[0]) * 0.5f),
                -((minv[1] + maxv[1]) * 0.5f),
                -((minv[2] + maxv[2]) * 0.5f)
            };
            const ObjScene centered_scene = scene_with_transform(scene, translation, static_cast<float>(options.export_scale));
            const auto result = write_glb(centered_scene, triangles, options.output, options.output.stem().string());
            std::cout << "Escrito " << options.output << " con " << result.triangle_count << " triangulos\n";
        }
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
