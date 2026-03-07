#pragma once

#include "Lvs/Engine/Rendering/Common/CommandBuffer.hpp"
#include "Lvs/Engine/Rendering/Common/GpuResource.hpp"

#include <vulkan/vulkan.h>

namespace Lvs::Engine::Rendering::Vulkan {

class VulkanBufferResource final : public Common::BufferResource {
public:
    VulkanBufferResource(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, std::size_t size);
    ~VulkanBufferResource() override;

    VulkanBufferResource(const VulkanBufferResource&) = delete;
    VulkanBufferResource& operator=(const VulkanBufferResource&) = delete;

    [[nodiscard]] std::size_t GetSize() const override;
    void Upload(const void* data, std::size_t size, std::size_t offset = 0) override;

    [[nodiscard]] VkBuffer GetBuffer() const;

private:
    VkDevice device_{VK_NULL_HANDLE};
    VkBuffer buffer_{VK_NULL_HANDLE};
    VkDeviceMemory memory_{VK_NULL_HANDLE};
    std::size_t size_{0};
};

class VulkanImageResource final : public Common::ImageResource {
public:
    VulkanImageResource(VkDevice device, VkImage image, VkDeviceMemory memory, std::uint32_t width, std::uint32_t height);
    ~VulkanImageResource() override;

    VulkanImageResource(const VulkanImageResource&) = delete;
    VulkanImageResource& operator=(const VulkanImageResource&) = delete;

    [[nodiscard]] std::uint32_t GetWidth() const override;
    [[nodiscard]] std::uint32_t GetHeight() const override;

    [[nodiscard]] VkImage GetImage() const;
    [[nodiscard]] VkDeviceMemory GetMemory() const;

private:
    VkDevice device_{VK_NULL_HANDLE};
    VkImage image_{VK_NULL_HANDLE};
    VkDeviceMemory memory_{VK_NULL_HANDLE};
    std::uint32_t width_{0};
    std::uint32_t height_{0};
};

class VulkanSamplerResource final : public Common::SamplerResource {
public:
    VulkanSamplerResource(VkDevice device, VkSampler sampler);
    ~VulkanSamplerResource() override;

    VulkanSamplerResource(const VulkanSamplerResource&) = delete;
    VulkanSamplerResource& operator=(const VulkanSamplerResource&) = delete;

    [[nodiscard]] VkSampler GetSampler() const;

private:
    VkDevice device_{VK_NULL_HANDLE};
    VkSampler sampler_{VK_NULL_HANDLE};
};

class VulkanRenderCommandBuffer final : public Common::CommandBuffer {
public:
    explicit VulkanRenderCommandBuffer(VkCommandBuffer commandBuffer);

    void BindVertexBuffer(const Common::BufferResource& buffer, std::uint32_t binding = 0, std::size_t offset = 0) override;
    void BindIndexBuffer(const Common::BufferResource& buffer, Common::IndexFormat format, std::size_t offset = 0) override;
    void DrawIndexed(std::uint32_t indexCount, std::uint32_t instanceCount, std::uint32_t firstIndex = 0) override;
    [[nodiscard]] VkCommandBuffer GetHandle() const;

private:
    VkCommandBuffer commandBuffer_{VK_NULL_HANDLE};
};

} // namespace Lvs::Engine::Rendering::Vulkan
