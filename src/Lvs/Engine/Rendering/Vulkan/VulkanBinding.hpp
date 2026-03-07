#pragma once

#include "Lvs/Engine/Rendering/Common/BindingLayout.hpp"
#include "Lvs/Engine/Rendering/Common/ResourceBinding.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace Lvs::Engine::Rendering::Vulkan {

class VulkanResourceBinding;

class VulkanBindingLayout final : public Common::BindingLayout {
public:
    VulkanBindingLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, VkDescriptorPool descriptorPool);
    ~VulkanBindingLayout() override;

    VulkanBindingLayout(const VulkanBindingLayout&) = delete;
    VulkanBindingLayout& operator=(const VulkanBindingLayout&) = delete;

    [[nodiscard]] VkDescriptorSetLayout GetLayoutHandle() const;
    [[nodiscard]] std::unique_ptr<Common::ResourceBinding> AllocateBinding() const override;
    [[nodiscard]] void* GetNativeHandle() const override;
    [[nodiscard]] bool IsValid() const override;

    static std::unique_ptr<VulkanBindingLayout> Create(
        VkDevice device,
        const VkDescriptorSetLayoutCreateInfo& layoutInfo,
        const std::vector<VkDescriptorPoolSize>& poolSizes,
        std::uint32_t maxSets
    );

private:
    VkDevice device_{VK_NULL_HANDLE};
    VkDescriptorSetLayout descriptorSetLayout_{VK_NULL_HANDLE};
    VkDescriptorPool descriptorPool_{VK_NULL_HANDLE};
};

class VulkanResourceBinding final : public Common::ResourceBinding {
public:
    VulkanResourceBinding(VkDevice device, VkDescriptorSet descriptorSet);
    ~VulkanResourceBinding() override = default;

    VulkanResourceBinding(const VulkanResourceBinding&) = delete;
    VulkanResourceBinding& operator=(const VulkanResourceBinding&) = delete;

    void Bind(Common::CommandBuffer& commandBuffer, const Common::PipelineLayout& layout, std::uint32_t firstSet = 0) const override;
    void UpdateBuffer(std::uint32_t binding, const Common::BufferResource& buffer, std::size_t offset = 0, std::size_t range = 0) override;
    void UpdateImage(std::uint32_t binding, void* sampler, void* imageView, std::uint32_t imageLayout) override;
    void UpdateImages(
        std::uint32_t binding,
        const void* const* samplers,
        const void* const* imageViews,
        const std::uint32_t* imageLayouts,
        std::uint32_t count
    ) override;
    [[nodiscard]] VkDescriptorSet GetHandle() const;
    void UpdateBuffer(
        std::uint32_t binding,
        const VkDescriptorBufferInfo& bufferInfo,
        VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        std::uint32_t arrayElement = 0
    ) const;
    void UpdateImage(
        std::uint32_t binding,
        const VkDescriptorImageInfo& imageInfo,
        VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        std::uint32_t arrayElement = 0
    ) const;
    void UpdateImages(
        std::uint32_t binding,
        const VkDescriptorImageInfo* imageInfos,
        std::uint32_t count,
        VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        std::uint32_t arrayElement = 0
    ) const;

private:
    VkDevice device_{VK_NULL_HANDLE};
    VkDescriptorSet descriptorSet_{VK_NULL_HANDLE};
};

} // namespace Lvs::Engine::Rendering::Vulkan
