#include "Lvs/Engine/Rendering/Common/MeshCache.hpp"

#include "Lvs/Engine/Rendering/Common/MeshIO.hpp"
#include "Lvs/Engine/Rendering/Common/Primitives.hpp"
#include "Lvs/Engine/Utils/PathUtils.hpp"

#include <stdexcept>
#include <utility>

namespace Lvs::Engine::Rendering::Common {

MeshCache::MeshCache(MeshUploader& uploader)
    : uploader_(&uploader) {
}

void MeshCache::Initialize() {
    if (initialized_) {
        return;
    }
    if (uploader_ == nullptr) {
        throw std::runtime_error("MeshCache requires a mesh uploader.");
    }

    primitiveMeshes_[static_cast<int>(Enums::PartShape::Cube)] = uploader_->Upload(Primitives::GenerateCube());
    primitiveMeshes_[static_cast<int>(Enums::PartShape::Sphere)] = uploader_->Upload(Primitives::GenerateSphere());
    primitiveMeshes_[static_cast<int>(Enums::PartShape::Cylinder)] = uploader_->Upload(Primitives::GenerateCylinder());
    primitiveMeshes_[static_cast<int>(Enums::PartShape::Cone)] = uploader_->Upload(Primitives::GenerateCone());
    initialized_ = true;
}

void MeshCache::Clear() {
    primitiveMeshes_.clear();
    meshPartMeshes_.clear();
    initialized_ = false;
}

std::shared_ptr<UploadedMesh> MeshCache::GetPrimitive(const Enums::PartShape shape) {
    if (!initialized_) {
        Initialize();
    }
    return GetByShape(shape);
}

std::shared_ptr<UploadedMesh> MeshCache::GetMeshPart(const std::string& contentId) {
    if (!initialized_) {
        Initialize();
    }
    if (contentId.empty()) {
        return GetByShape(Enums::PartShape::Cube);
    }

    if (const auto it = meshPartMeshes_.find(contentId); it != meshPartMeshes_.end()) {
        return it->second;
    }

    try {
        auto mesh = uploader_->Upload(LoadObjMesh(Utils::PathUtils::ToOsPath(contentId)));
        meshPartMeshes_[contentId] = mesh;
        return mesh;
    } catch (const std::exception&) {
        meshPartMeshes_[contentId] = GetByShape(Enums::PartShape::Cube);
        return meshPartMeshes_[contentId];
    }
}

std::shared_ptr<UploadedMesh> MeshCache::GetByShape(const Enums::PartShape shape) {
    if (const auto it = primitiveMeshes_.find(static_cast<int>(shape)); it != primitiveMeshes_.end()) {
        return it->second;
    }
    return primitiveMeshes_.at(static_cast<int>(Enums::PartShape::Cube));
}

} // namespace Lvs::Engine::Rendering::Common
