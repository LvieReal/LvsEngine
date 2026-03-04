#include "Lvs/Engine/Rendering/Vulkan/MeshCache.hpp"

#include "Lvs/Engine/Math/Vector3.hpp"
#include "Lvs/Engine/Objects/BasePart.hpp"
#include "Lvs/Engine/Objects/MeshPart.hpp"
#include "Lvs/Engine/Objects/Part.hpp"
#include "Lvs/Engine/Rendering/Vulkan/Mesh.hpp"
#include "Lvs/Engine/Rendering/Vulkan/MeshData.hpp"
#include "Lvs/Engine/Rendering/Vulkan/Primitives.hpp"
#include "Lvs/Engine/Utils/SourcePath.hpp"

#include <QFile>
#include <QStringList>

#include <array>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Lvs::Engine::Rendering::Vulkan {

namespace {

std::shared_ptr<Mesh> BuildMeshFromData(MeshData data) {
    return std::make_shared<Mesh>(std::move(data));
}

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

MeshData LoadObjMesh(const QString& inputPath) {
    const QString resolvedPath = Utils::SourcePath::ToOsPath(inputPath);
    QFile file(resolvedPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        throw std::runtime_error(QString("Failed to open mesh file: %1").arg(resolvedPath).toStdString());
    }

    std::vector<std::array<float, 3>> positions;
    std::vector<std::array<float, 3>> normals;
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::unordered_map<ObjVertexKey, std::uint32_t, ObjVertexKeyHash> vertexMap;

    while (!file.atEnd()) {
        const QString rawLine = QString::fromUtf8(file.readLine()).trimmed();
        if (rawLine.isEmpty() || rawLine.startsWith('#')) {
            continue;
        }

        const QStringList tokens = rawLine.split(' ', Qt::SkipEmptyParts);
        if (tokens.isEmpty()) {
            continue;
        }

        if (tokens[0] == "v" && tokens.size() >= 4) {
            positions.push_back({tokens[1].toFloat(), tokens[2].toFloat(), tokens[3].toFloat()});
            continue;
        }
        if (tokens[0] == "vn" && tokens.size() >= 4) {
            normals.push_back({tokens[1].toFloat(), tokens[2].toFloat(), tokens[3].toFloat()});
            continue;
        }
        if (tokens[0] == "f" && tokens.size() >= 4) {
            std::vector<std::uint32_t> faceIndices;
            faceIndices.reserve(static_cast<std::size_t>(tokens.size() - 1));

            for (int i = 1; i < tokens.size(); ++i) {
                const QStringList faceToken = tokens[i].split('/');
                const int pIdx = faceToken.size() > 0 ? faceToken[0].toInt() : 0;
                const int nIdx = faceToken.size() > 2 ? faceToken[2].toInt() : 0;
                if (pIdx <= 0 || pIdx > static_cast<int>(positions.size())) {
                    continue;
                }

                ObjVertexKey key{pIdx - 1, nIdx > 0 ? nIdx - 1 : -1};
                auto it = vertexMap.find(key);
                if (it == vertexMap.end()) {
                    const auto pos = positions[static_cast<std::size_t>(key.PositionIndex)];
                    std::array<float, 3> normal{0.0F, 1.0F, 0.0F};
                    if (key.NormalIndex >= 0 && key.NormalIndex < static_cast<int>(normals.size())) {
                        normal = normals[static_cast<std::size_t>(key.NormalIndex)];
                    }
                    const auto index = static_cast<std::uint32_t>(vertices.size());
                    vertices.push_back(Vertex{
                        .Position = {pos[0], pos[1], pos[2]},
                        .Normal = {normal[0], normal[1], normal[2]}
                    });
                    vertexMap.emplace(key, index);
                    faceIndices.push_back(index);
                } else {
                    faceIndices.push_back(it->second);
                }
            }

            for (std::size_t i = 1; i + 1 < faceIndices.size(); ++i) {
                indices.push_back(faceIndices[0]);
                indices.push_back(faceIndices[i]);
                indices.push_back(faceIndices[i + 1]);
            }
        }
    }

    file.close();

    if (vertices.empty() || indices.empty()) {
        throw std::runtime_error("OBJ mesh contains no valid geometry.");
    }

    return MeshData{
        .Vertices = std::move(vertices),
        .Indices = std::move(indices)
    };
}

} // namespace

void MeshCache::Initialize() {
    if (initialized_) {
        return;
    }

    primitiveMeshes_[static_cast<int>(Enums::PartShape::Cube)] = BuildMeshFromData(Primitives::GenerateCube());
    primitiveMeshes_[static_cast<int>(Enums::PartShape::Sphere)] = BuildMeshFromData(Primitives::GenerateSphere());
    primitiveMeshes_[static_cast<int>(Enums::PartShape::Cylinder)] = BuildMeshFromData(Primitives::GenerateCylinder());
    primitiveMeshes_[static_cast<int>(Enums::PartShape::Cone)] = BuildMeshFromData(Primitives::GenerateCone());

    initialized_ = true;
}

void MeshCache::Destroy(const VkDevice device) {
    for (auto& [_, mesh] : primitiveMeshes_) {
        mesh->Destroy(device);
    }
    for (auto& [_, mesh] : meshPartMeshes_) {
        mesh->Destroy(device);
    }
    primitiveMeshes_.clear();
    meshPartMeshes_.clear();
    initialized_ = false;
}

std::shared_ptr<Mesh> MeshCache::Get(const std::shared_ptr<Objects::BasePart>& part) {
    if (!initialized_) {
        Initialize();
    }
    if (part == nullptr) {
        return nullptr;
    }

    if (const auto meshPart = std::dynamic_pointer_cast<Objects::MeshPart>(part); meshPart != nullptr) {
        return GetMeshPart(meshPart);
    }

    if (const auto concretePart = std::dynamic_pointer_cast<Objects::Part>(part); concretePart != nullptr) {
        return GetByShape(concretePart->GetProperty("Shape").value<Enums::PartShape>());
    }

    return GetByShape(Enums::PartShape::Cube);
}

std::shared_ptr<Mesh> MeshCache::GetPrimitive(const Enums::PartShape shape) {
    if (!initialized_) {
        Initialize();
    }
    return GetByShape(shape);
}

std::shared_ptr<Mesh> MeshCache::GetByShape(const Enums::PartShape shape) {
    const auto it = primitiveMeshes_.find(static_cast<int>(shape));
    if (it != primitiveMeshes_.end()) {
        return it->second;
    }
    return primitiveMeshes_[static_cast<int>(Enums::PartShape::Cube)];
}

std::shared_ptr<Mesh> MeshCache::GetMeshPart(const std::shared_ptr<Objects::MeshPart>& meshPart) {
    const QString contentId = meshPart->GetProperty("ContentId").toString().trimmed();
    if (contentId.isEmpty()) {
        return GetByShape(Enums::PartShape::Cube);
    }

    const std::string key = contentId.toStdString();
    if (auto it = meshPartMeshes_.find(key); it != meshPartMeshes_.end()) {
        return it->second;
    }

    try {
        auto mesh = BuildMeshFromData(LoadObjMesh(contentId));
        meshPartMeshes_[key] = mesh;
        return mesh;
    } catch (const std::exception&) {
        meshPartMeshes_[key] = GetByShape(Enums::PartShape::Cube);
        return meshPartMeshes_[key];
    }
}

} // namespace Lvs::Engine::Rendering::Vulkan
