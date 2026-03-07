#include "Lvs/Engine/Rendering/Vulkan/VulkanMeshUploader.hpp"

#include "Lvs/Engine/Rendering/Common/Mesh.hpp"

#include <memory>
#include <utility>

namespace Lvs::Engine::Rendering::Vulkan {

std::shared_ptr<Common::UploadedMesh> VulkanMeshUploader::Upload(Common::MeshData data) {
    return std::make_shared<Common::Mesh>(std::move(data));
}

} // namespace Lvs::Engine::Rendering::Vulkan
