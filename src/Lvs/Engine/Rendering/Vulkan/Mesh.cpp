#include "Lvs/Engine/Rendering/Vulkan/Mesh.hpp"

#include <cstring>
#include <stdexcept>
#include <utility>

namespace Lvs::Engine::Rendering::Vulkan {

Mesh::Mesh(MeshData data)
    : data_(std::move(data)) {
}

void Mesh::EnsureUploaded(const VkPhysicalDevice physicalDevice, const VkDevice device) {
    if (vertexBuffer_.Buffer != VK_NULL_HANDLE && indexBuffer_.Buffer != VK_NULL_HANDLE) {
        return;
    }
    if (data_.Vertices.empty() || data_.Indices.empty()) {
        throw std::runtime_error("Mesh has no vertex or index data.");
    }

    const VkDeviceSize vertexSize = static_cast<VkDeviceSize>(data_.Vertices.size() * sizeof(Vertex));
    const VkDeviceSize indexSize = static_cast<VkDeviceSize>(data_.Indices.size() * sizeof(std::uint32_t));

    vertexBuffer_ = BufferUtils::CreateBuffer(
        physicalDevice,
        device,
        vertexSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    indexBuffer_ = BufferUtils::CreateBuffer(
        physicalDevice,
        device,
        indexSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    void* mapped = nullptr;
    vkMapMemory(device, vertexBuffer_.Memory, 0, vertexSize, 0, &mapped);
    std::memcpy(mapped, data_.Vertices.data(), static_cast<std::size_t>(vertexSize));
    vkUnmapMemory(device, vertexBuffer_.Memory);

    vkMapMemory(device, indexBuffer_.Memory, 0, indexSize, 0, &mapped);
    std::memcpy(mapped, data_.Indices.data(), static_cast<std::size_t>(indexSize));
    vkUnmapMemory(device, indexBuffer_.Memory);
}

void Mesh::Destroy(const VkDevice device) {
    BufferUtils::DestroyBuffer(device, vertexBuffer_);
    BufferUtils::DestroyBuffer(device, indexBuffer_);
}

void Mesh::Draw(const VkCommandBuffer commandBuffer) const {
    const VkBuffer vertexBuffers[] = {vertexBuffer_.Buffer};
    const VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer_.Buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, static_cast<std::uint32_t>(data_.Indices.size()), 1, 0, 0, 0);
}

} // namespace Lvs::Engine::Rendering::Vulkan
