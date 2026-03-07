#pragma once

#include "Lvs/Engine/Rendering/Common/MeshUploader.hpp"

namespace Lvs::Engine::Rendering::Vulkan {

class VulkanMeshUploader final : public Common::MeshUploader {
public:
    [[nodiscard]] std::shared_ptr<Common::UploadedMesh> Upload(Common::MeshData data) override;
};

} // namespace Lvs::Engine::Rendering::Vulkan
