#include "Lvs/Engine/Rendering/Common/MeshLoader.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace Lvs::Engine::Rendering::Common {

std::optional<MeshData> LoadMeshFromFile(const std::filesystem::path& filePath, const bool smoothNormals) {
    Assimp::Importer importer;

    unsigned int flags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_PreTransformVertices |
        aiProcess_ImproveCacheLocality | aiProcess_SortByPType;
    flags |= smoothNormals ? aiProcess_GenSmoothNormals : aiProcess_GenNormals;

    const aiScene* scene = importer.ReadFile(filePath.string(), flags);
    if (scene == nullptr || !scene->HasMeshes()) {
        return std::nullopt;
    }

    MeshData mesh{};
    std::size_t totalVertexCount = 0;
    std::size_t totalIndexCount = 0;
    for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        const aiMesh* part = scene->mMeshes[meshIndex];
        if (part == nullptr) {
            continue;
        }
        totalVertexCount += static_cast<std::size_t>(part->mNumVertices);
        totalIndexCount += static_cast<std::size_t>(part->mNumFaces) * 3U;
    }
    mesh.Vertices.reserve(totalVertexCount);
    mesh.Indices.reserve(totalIndexCount);

    for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        const aiMesh* part = scene->mMeshes[meshIndex];
        if (part == nullptr || part->mNumVertices == 0 || part->mNumFaces == 0) {
            continue;
        }

        const RHI::u32 vertexOffset = static_cast<RHI::u32>(mesh.Vertices.size());
        for (unsigned int vertexIndex = 0; vertexIndex < part->mNumVertices; ++vertexIndex) {
            const aiVector3D& position = part->mVertices[vertexIndex];
            const aiVector3D normal = part->HasNormals() ? part->mNormals[vertexIndex] : aiVector3D{0.0F, 1.0F, 0.0F};
            mesh.Vertices.push_back(VertexP3N3{
                .Position = {position.x, position.y, position.z},
                .Normal = {normal.x, normal.y, normal.z}
            });
        }

        for (unsigned int faceIndex = 0; faceIndex < part->mNumFaces; ++faceIndex) {
            const aiFace& face = part->mFaces[faceIndex];
            if (face.mNumIndices != 3U) {
                continue;
            }
            mesh.Indices.push_back(vertexOffset + static_cast<RHI::u32>(face.mIndices[0]));
            mesh.Indices.push_back(vertexOffset + static_cast<RHI::u32>(face.mIndices[1]));
            mesh.Indices.push_back(vertexOffset + static_cast<RHI::u32>(face.mIndices[2]));
        }
    }

    if (mesh.Vertices.empty() || mesh.Indices.empty()) {
        return std::nullopt;
    }

    return mesh;
}

} // namespace Lvs::Engine::Rendering::Common
