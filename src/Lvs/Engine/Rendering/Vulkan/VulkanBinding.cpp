#include "Lvs/Engine/Rendering/Vulkan/VulkanBinding.hpp"

#include "Lvs/Engine/Rendering/Vulkan/VulkanGpuResources.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanPipeline.hpp"

#include <stdexcept>

namespace Lvs::Engine::Rendering::Vulkan {

VulkanBindingLayout::VulkanBindingLayout(
    const VkDevice device,
    const VkDescriptorSetLayout descriptorSetLayout,
    const VkDescriptorPool descriptorPool
) : device_(device),
    descriptorSetLayout_(descriptorSetLayout),
    descriptorPool_(descriptorPool) {
}

VulkanBindingLayout::~VulkanBindingLayout() {
    if (descriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
    }
    if (descriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, descriptorSetLayout_, nullptr);
    }
}

VkDescriptorSetLayout VulkanBindingLayout::GetLayoutHandle() const {
    return descriptorSetLayout_;
}

void* VulkanBindingLayout::GetNativeHandle() const {
    return reinterpret_cast<void*>(descriptorSetLayout_);
}

std::unique_ptr<Common::ResourceBinding> VulkanBindingLayout::AllocateBinding() const {
    const VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = descriptorPool_,
        .descriptorSetCount = 1,
        .pSetLayouts = &descriptorSetLayout_
    };
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    if (vkAllocateDescriptorSets(device_, &allocInfo, &descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate resource binding.");
    }
    return std::make_unique<VulkanResourceBinding>(device_, descriptorSet);
}

bool VulkanBindingLayout::IsValid() const {
    return descriptorSetLayout_ != VK_NULL_HANDLE && descriptorPool_ != VK_NULL_HANDLE;
}

std::unique_ptr<VulkanBindingLayout> VulkanBindingLayout::Create(
    const VkDevice device,
    const VkDescriptorSetLayoutCreateInfo& layoutInfo,
    const std::vector<VkDescriptorPoolSize>& poolSizes,
    const std::uint32_t maxSets
) {
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create binding layout.");
    }

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    if (maxSets > 0) {
        const VkDescriptorPoolCreateInfo poolInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .maxSets = maxSets,
            .poolSizeCount = static_cast<std::uint32_t>(poolSizes.size()),
            .pPoolSizes = poolSizes.data()
        };
        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
            throw std::runtime_error("Failed to create descriptor pool for binding layout.");
        }
    }

    return std::make_unique<VulkanBindingLayout>(device, descriptorSetLayout, descriptorPool);
}

VulkanResourceBinding::VulkanResourceBinding(const VkDevice device, const VkDescriptorSet descriptorSet)
    : device_(device),
      descriptorSet_(descriptorSet) {
}

void VulkanResourceBinding::Bind(
    Common::CommandBuffer& commandBuffer,
    const Common::PipelineLayout& layout,
    const std::uint32_t firstSet
) const {
    const auto vkCommandBuffer = reinterpret_cast<VkCommandBuffer>(commandBuffer.GetNativeHandle());
    const auto vkPipelineLayout = reinterpret_cast<VkPipelineLayout>(layout.GetNativeHandle());
    const VkDescriptorSet descriptorSet = descriptorSet_;
    vkCmdBindDescriptorSets(
        vkCommandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        vkPipelineLayout,
        firstSet,
        1,
        &descriptorSet,
        0,
        nullptr
    );
}

VkDescriptorSet VulkanResourceBinding::GetHandle() const {
    return descriptorSet_;
}

void VulkanResourceBinding::UpdateBuffer(
    const std::uint32_t binding,
    const Common::BufferResource& buffer,
    const std::size_t offset,
    const std::size_t range
) {
    const VkDescriptorBufferInfo bufferInfo{
        .buffer = reinterpret_cast<VkBuffer>(buffer.GetNativeHandle()),
        .offset = static_cast<VkDeviceSize>(offset),
        .range = static_cast<VkDeviceSize>(range == 0 ? buffer.GetSize() : range)
    };
    const VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = descriptorSet_,
        .dstBinding = binding,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pImageInfo = nullptr,
        .pBufferInfo = &bufferInfo,
        .pTexelBufferView = nullptr
    };
    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
}

void VulkanResourceBinding::UpdateBuffer(
    const std::uint32_t binding,
    const VkDescriptorBufferInfo& bufferInfo,
    const VkDescriptorType descriptorType,
    const std::uint32_t arrayElement
) const {
    const VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = descriptorSet_,
        .dstBinding = binding,
        .dstArrayElement = arrayElement,
        .descriptorCount = 1,
        .descriptorType = descriptorType,
        .pImageInfo = nullptr,
        .pBufferInfo = &bufferInfo,
        .pTexelBufferView = nullptr
    };
    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
}

void VulkanResourceBinding::UpdateImage(
    const std::uint32_t binding,
    void* sampler,
    void* imageView,
    const std::uint32_t imageLayout
) {
    const void* samplers[] = {sampler};
    const void* imageViews[] = {imageView};
    const std::uint32_t imageLayouts[] = {imageLayout};
    UpdateImages(binding, samplers, imageViews, imageLayouts, 1);
}

void VulkanResourceBinding::UpdateImage(
    const std::uint32_t binding,
    const VkDescriptorImageInfo& imageInfo,
    const VkDescriptorType descriptorType,
    const std::uint32_t arrayElement
) const {
    UpdateImages(binding, &imageInfo, 1, descriptorType, arrayElement);
}

void VulkanResourceBinding::UpdateImages(
    const std::uint32_t binding,
    const void* const* samplers,
    const void* const* imageViews,
    const std::uint32_t* imageLayouts,
    const std::uint32_t count
) {
    std::vector<VkDescriptorImageInfo> imageInfos(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        imageInfos[i] = VkDescriptorImageInfo{
            .sampler = reinterpret_cast<VkSampler>(const_cast<void*>(samplers[i])),
            .imageView = reinterpret_cast<VkImageView>(const_cast<void*>(imageViews[i])),
            .imageLayout = static_cast<VkImageLayout>(imageLayouts[i])
        };
    }
    const VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = descriptorSet_,
        .dstBinding = binding,
        .dstArrayElement = 0,
        .descriptorCount = count,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = imageInfos.data(),
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr
    };
    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
}

void VulkanResourceBinding::UpdateImages(
    const std::uint32_t binding,
    const VkDescriptorImageInfo* imageInfos,
    const std::uint32_t count,
    const VkDescriptorType descriptorType,
    const std::uint32_t arrayElement
) const {
    const VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = descriptorSet_,
        .dstBinding = binding,
        .dstArrayElement = arrayElement,
        .descriptorCount = count,
        .descriptorType = descriptorType,
        .pImageInfo = imageInfos,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr
    };
    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
}

} // namespace Lvs::Engine::Rendering::Vulkan
