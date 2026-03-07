#pragma once

#include "Lvs/Engine/Rendering/Common/CommandBuffer.hpp"
#include "Lvs/Engine/Rendering/Common/Pipeline.hpp"
#include "Lvs/Engine/Rendering/Common/PipelineLayout.hpp"
#include "Lvs/Engine/Rendering/Common/ShaderModule.hpp"

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

namespace Lvs::Engine::Rendering::Vulkan {

class VulkanShaderModule final : public Common::ShaderModule {
public:
    VulkanShaderModule(VkDevice device, VkShaderModule shaderModule);
    ~VulkanShaderModule() override;

    VulkanShaderModule(const VulkanShaderModule&) = delete;
    VulkanShaderModule& operator=(const VulkanShaderModule&) = delete;

    [[nodiscard]] bool IsValid() const override;
    [[nodiscard]] VkShaderModule GetHandle() const;

    static std::unique_ptr<VulkanShaderModule> Create(VkDevice device, const std::vector<char>& code);

private:
    VkDevice device_{VK_NULL_HANDLE};
    VkShaderModule shaderModule_{VK_NULL_HANDLE};
};

class VulkanPipelineLayout final : public Common::PipelineLayout {
public:
    VulkanPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout);
    ~VulkanPipelineLayout() override;

    VulkanPipelineLayout(const VulkanPipelineLayout&) = delete;
    VulkanPipelineLayout& operator=(const VulkanPipelineLayout&) = delete;

    [[nodiscard]] void* GetNativeHandle() const override;
    [[nodiscard]] bool IsValid() const override;
    [[nodiscard]] VkPipelineLayout GetHandle() const;

    static std::unique_ptr<VulkanPipelineLayout> Create(VkDevice device, const VkPipelineLayoutCreateInfo& createInfo);

private:
    VkDevice device_{VK_NULL_HANDLE};
    VkPipelineLayout pipelineLayout_{VK_NULL_HANDLE};
};

class VulkanPipelineVariant final : public Common::Pipeline {
public:
    VulkanPipelineVariant(VkDevice device, VkPipeline pipeline, const Common::PipelineLayout& layout);
    ~VulkanPipelineVariant();

    VulkanPipelineVariant(const VulkanPipelineVariant&) = delete;
    VulkanPipelineVariant& operator=(const VulkanPipelineVariant&) = delete;

    void Bind(Common::CommandBuffer& commandBuffer) const override;
    [[nodiscard]] const Common::PipelineLayout& GetLayout() const override;
    [[nodiscard]] bool IsValid() const override;
    [[nodiscard]] VkPipeline GetHandle() const;

    static std::unique_ptr<VulkanPipelineVariant> CreateGraphicsPipeline(
        VkDevice device,
        const VkGraphicsPipelineCreateInfo& createInfo,
        const Common::PipelineLayout& layout
    );

private:
    VkDevice device_{VK_NULL_HANDLE};
    VkPipeline pipeline_{VK_NULL_HANDLE};
    const Common::PipelineLayout* layout_{nullptr};
};

} // namespace Lvs::Engine::Rendering::Vulkan
