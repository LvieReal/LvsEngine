#pragma once

#include "Lvs/Engine/Enums/PartShape.hpp"
#include "Lvs/Engine/Rendering/Common/MeshUploader.hpp"

#include <memory>
#include <string>
#include <unordered_map>

namespace Lvs::Engine::Rendering::Common {

class MeshCache {
public:
    explicit MeshCache(MeshUploader& uploader);

    void Initialize();
    void Clear();

    [[nodiscard]] std::shared_ptr<UploadedMesh> GetPrimitive(Enums::PartShape shape);
    [[nodiscard]] std::shared_ptr<UploadedMesh> GetMeshPart(const std::string& contentId);

private:
    [[nodiscard]] std::shared_ptr<UploadedMesh> GetByShape(Enums::PartShape shape);

    MeshUploader* uploader_{nullptr};
    bool initialized_{false};
    std::unordered_map<int, std::shared_ptr<UploadedMesh>> primitiveMeshes_;
    std::unordered_map<std::string, std::shared_ptr<UploadedMesh>> meshPartMeshes_;
};

} // namespace Lvs::Engine::Rendering::Common
