#pragma once

#include "Lvs/Engine/Rendering/Vulkan/MeshData.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanBufferUtils.hpp"

#include <vulkan/vulkan.h>

namespace Lvs::Engine::Rendering::Vulkan {

class Mesh final {
public:
    explicit Mesh(MeshData data);
    ~Mesh() = default;

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&&) = delete;
    Mesh& operator=(Mesh&&) = delete;

    void EnsureUploaded(VkPhysicalDevice physicalDevice, VkDevice device);
    void Destroy(VkDevice device);
    void Draw(VkCommandBuffer commandBuffer) const;

private:
    MeshData data_;
    BufferUtils::BufferHandle vertexBuffer_;
    BufferUtils::BufferHandle indexBuffer_;
};

} // namespace Lvs::Engine::Rendering::Vulkan
