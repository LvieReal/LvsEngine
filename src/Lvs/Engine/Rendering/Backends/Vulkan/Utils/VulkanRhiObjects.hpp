#pragma once

#include "Lvs/Engine/Rendering/RHI/IBuffer.hpp"
#include "Lvs/Engine/Rendering/RHI/IRenderTarget.hpp"
#include "Lvs/Engine/Rendering/RHI/IResourceSet.hpp"

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Lvs::Engine::Rendering::Backends::Vulkan::Utils {

class VulkanResourceSet final : public RHI::IResourceSet {
public:
    VulkanResourceSet(VkDevice device, VkDescriptorPool pool, VkDescriptorSet set, bool ownsSet);
    ~VulkanResourceSet() override;

    [[nodiscard]] void* GetNativeHandle() const override;

private:
    VkDevice device_{VK_NULL_HANDLE};
    VkDescriptorPool pool_{VK_NULL_HANDLE};
    VkDescriptorSet set_{VK_NULL_HANDLE};
    bool ownsSet_{false};
};

class VulkanBuffer final : public RHI::IBuffer {
public:
    VulkanBuffer(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, std::size_t size);
    ~VulkanBuffer() override;

    [[nodiscard]] void* GetNativeHandle() const override;
    [[nodiscard]] std::size_t GetSize() const override;

private:
    VkDevice device_{VK_NULL_HANDLE};
    VkBuffer buffer_{VK_NULL_HANDLE};
    VkDeviceMemory memory_{VK_NULL_HANDLE};
    std::size_t size_{0};
};

class VulkanRenderTarget final : public RHI::IRenderTarget {
public:
    struct ColorAttachment {
        VkImage image{VK_NULL_HANDLE};
        VkDeviceMemory memory{VK_NULL_HANDLE};
        VkImageView view{VK_NULL_HANDLE};
        VkSampler sampler{VK_NULL_HANDLE};
    };

    VulkanRenderTarget(
        VkDevice device,
        RHI::u32 width,
        RHI::u32 height,
        VkFormat colorFormat,
        VkRenderPass renderPass,
        VkFramebuffer framebuffer,
        const std::vector<ColorAttachment>& resolveColors,
        const std::vector<ColorAttachment>& msaaColors,
        VkImage depthImage,
        VkDeviceMemory depthMemory,
        VkImageView depthView,
        VkSampler depthSampler,
        RHI::Format depthFormat,
        RHI::u32 sampleCount
    );
    ~VulkanRenderTarget() override;

    [[nodiscard]] void* GetRenderPassHandle() const override;
    [[nodiscard]] void* GetFramebufferHandle() const override;
    [[nodiscard]] RHI::u32 GetWidth() const override;
    [[nodiscard]] RHI::u32 GetHeight() const override;
    [[nodiscard]] RHI::u32 GetColorAttachmentCount() const override;
    [[nodiscard]] RHI::u32 GetSampleCount() const override;
    [[nodiscard]] RHI::Texture GetColorTexture(RHI::u32 index) const override;
    [[nodiscard]] bool HasDepth() const override;
    [[nodiscard]] RHI::Texture GetDepthTexture() const override;

private:
    VkDevice device_{VK_NULL_HANDLE};
    RHI::u32 width_{0U};
    RHI::u32 height_{0U};
    VkFormat colorFormat_{VK_FORMAT_R8G8B8A8_UNORM};
    VkRenderPass renderPass_{VK_NULL_HANDLE};
    VkFramebuffer framebuffer_{VK_NULL_HANDLE};
    std::vector<ColorAttachment> colorAttachments_{};
    std::vector<ColorAttachment> msaaColorAttachments_{};
    VkImage depthImage_{VK_NULL_HANDLE};
    VkDeviceMemory depthMemory_{VK_NULL_HANDLE};
    VkImageView depthView_{VK_NULL_HANDLE};
    VkSampler depthSampler_{VK_NULL_HANDLE};
    RHI::Format depthFormat_{RHI::Format::D32_Float};
    std::vector<RHI::Texture> colorTextures_{};
    RHI::Texture depthTexture_{};
    RHI::u32 sampleCount_{1U};
};

} // namespace Lvs::Engine::Rendering::Backends::Vulkan::Utils

