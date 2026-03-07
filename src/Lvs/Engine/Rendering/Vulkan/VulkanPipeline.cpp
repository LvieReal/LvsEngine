#include "Lvs/Engine/Rendering/Vulkan/VulkanPipeline.hpp"

#include "Lvs/Engine/Rendering/Vulkan/VulkanGpuResources.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanShaderUtils.hpp"

#include <stdexcept>

namespace Lvs::Engine::Rendering::Vulkan {

VulkanShaderModule::VulkanShaderModule(const VkDevice device, const VkShaderModule shaderModule)
    : device_(device),
      shaderModule_(shaderModule) {
}

VulkanShaderModule::~VulkanShaderModule() {
    if (shaderModule_ != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device_, shaderModule_, nullptr);
    }
}

bool VulkanShaderModule::IsValid() const {
    return shaderModule_ != VK_NULL_HANDLE;
}

VkShaderModule VulkanShaderModule::GetHandle() const {
    return shaderModule_;
}

std::unique_ptr<VulkanShaderModule> VulkanShaderModule::Create(const VkDevice device, const std::vector<char>& code) {
    return std::make_unique<VulkanShaderModule>(device, ShaderUtils::CreateShaderModule(device, code));
}

VulkanPipelineLayout::VulkanPipelineLayout(const VkDevice device, const VkPipelineLayout pipelineLayout)
    : device_(device),
      pipelineLayout_(pipelineLayout) {
}

VulkanPipelineLayout::~VulkanPipelineLayout() {
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
    }
}

void* VulkanPipelineLayout::GetNativeHandle() const {
    return reinterpret_cast<void*>(pipelineLayout_);
}

bool VulkanPipelineLayout::IsValid() const {
    return pipelineLayout_ != VK_NULL_HANDLE;
}

VkPipelineLayout VulkanPipelineLayout::GetHandle() const {
    return pipelineLayout_;
}

std::unique_ptr<VulkanPipelineLayout> VulkanPipelineLayout::Create(
    const VkDevice device,
    const VkPipelineLayoutCreateInfo& createInfo
) {
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(device, &createInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout.");
    }
    return std::make_unique<VulkanPipelineLayout>(device, pipelineLayout);
}

VulkanPipelineVariant::VulkanPipelineVariant(const VkDevice device, const VkPipeline pipeline, const Common::PipelineLayout& layout)
    : device_(device),
      pipeline_(pipeline),
      layout_(&layout) {
}

VulkanPipelineVariant::~VulkanPipelineVariant() {
    if (pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, pipeline_, nullptr);
    }
}

void VulkanPipelineVariant::Bind(Common::CommandBuffer& commandBuffer) const {
    const auto vkCommandBuffer = reinterpret_cast<VkCommandBuffer>(commandBuffer.GetNativeHandle());
    vkCmdBindPipeline(vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
}

const Common::PipelineLayout& VulkanPipelineVariant::GetLayout() const {
    return *layout_;
}

bool VulkanPipelineVariant::IsValid() const {
    return pipeline_ != VK_NULL_HANDLE && layout_ != nullptr;
}

VkPipeline VulkanPipelineVariant::GetHandle() const {
    return pipeline_;
}

std::unique_ptr<VulkanPipelineVariant> VulkanPipelineVariant::CreateGraphicsPipeline(
    const VkDevice device,
    const VkGraphicsPipelineCreateInfo& createInfo,
    const Common::PipelineLayout& layout
) {
    VkPipeline pipeline = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline variant.");
    }
    return std::make_unique<VulkanPipelineVariant>(device, pipeline, layout);
}

} // namespace Lvs::Engine::Rendering::Vulkan
