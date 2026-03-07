#include "Lvs/Engine/Rendering/Common/MeshIO.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace Lvs::Engine::Rendering::Common {

namespace {

struct ObjVertexKey {
    int PositionIndex{0};
    int NormalIndex{0};

    bool operator==(const ObjVertexKey& other) const {
        return PositionIndex == other.PositionIndex && NormalIndex == other.NormalIndex;
    }
};

struct ObjVertexKeyHash {
    std::size_t operator()(const ObjVertexKey& key) const noexcept {
        return static_cast<std::size_t>(key.PositionIndex * 73856093) ^
               static_cast<std::size_t>(key.NormalIndex * 19349663);
    }
};

std::vector<std::string> SplitWhitespace(const std::string& line) {
    std::istringstream stream(line);
    std::vector<std::string> tokens;
    std::string token;
    while (stream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

std::vector<std::string> Split(const std::string& text, const char delimiter) {
    std::vector<std::string> parts;
    std::stringstream stream(text);
    std::string part;
    while (std::getline(stream, part, delimiter)) {
        parts.push_back(part);
    }
    if (!text.empty() && text.back() == delimiter) {
        parts.emplace_back();
    }
    return parts;
}

} // namespace

MeshData LoadObjMesh(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open mesh file: " + path.string());
    }

    std::vector<std::array<float, 3>> positions;
    std::vector<std::array<float, 3>> normals;
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::unordered_map<ObjVertexKey, std::uint32_t, ObjVertexKeyHash> vertexMap;

    std::string rawLine;
    while (std::getline(file, rawLine)) {
        if (!rawLine.empty() && rawLine.back() == '\r') {
            rawLine.pop_back();
        }
        const auto tokens = SplitWhitespace(rawLine);
        if (tokens.empty() || tokens[0].starts_with('#')) {
            continue;
        }

        if (tokens[0] == "v" && tokens.size() >= 4) {
            positions.push_back({std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3])});
            continue;
        }
        if (tokens[0] == "vn" && tokens.size() >= 4) {
            normals.push_back({std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3])});
            continue;
        }
        if (tokens[0] == "f" && tokens.size() >= 4) {
            std::vector<std::uint32_t> faceIndices;
            faceIndices.reserve(tokens.size() - 1U);

            for (std::size_t i = 1; i < tokens.size(); ++i) {
                const auto faceToken = Split(tokens[i], '/');
                const int positionIndex = !faceToken.empty() && !faceToken[0].empty() ? std::stoi(faceToken[0]) : 0;
                const int normalIndex = faceToken.size() > 2 && !faceToken[2].empty() ? std::stoi(faceToken[2]) : 0;
                if (positionIndex <= 0 || positionIndex > static_cast<int>(positions.size())) {
                    continue;
                }

                const ObjVertexKey key{positionIndex - 1, normalIndex > 0 ? normalIndex - 1 : -1};
                const auto [it, inserted] = vertexMap.try_emplace(key, static_cast<std::uint32_t>(vertices.size()));
                if (inserted) {
                    const auto& pos = positions[static_cast<std::size_t>(key.PositionIndex)];
                    std::array<float, 3> normal{0.0F, 1.0F, 0.0F};
                    if (key.NormalIndex >= 0 && key.NormalIndex < static_cast<int>(normals.size())) {
                        normal = normals[static_cast<std::size_t>(key.NormalIndex)];
                    }
                    vertices.push_back(Vertex{
                        .Position = {pos[0], pos[1], pos[2]},
                        .Normal = {normal[0], normal[1], normal[2]}
                    });
                }
                faceIndices.push_back(it->second);
            }

            for (std::size_t i = 1; i + 1 < faceIndices.size(); ++i) {
                indices.push_back(faceIndices[0]);
                indices.push_back(faceIndices[i]);
                indices.push_back(faceIndices[i + 1]);
            }
        }
    }

    if (vertices.empty() || indices.empty()) {
        throw std::runtime_error("OBJ mesh contains no valid geometry.");
    }

    return MeshData{
        .Vertices = std::move(vertices),
        .Indices = std::move(indices)
    };
}

} // namespace Lvs::Engine::Rendering::Common
