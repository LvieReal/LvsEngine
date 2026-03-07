#pragma once

#include "Lvs/Engine/Rendering/Common/RenderPass.hpp"

#include <vulkan/vulkan.h>

namespace Lvs::Engine::Rendering::Vulkan {

class VulkanRenderPass final : public Common::RenderPass {
public:
    VulkanRenderPass(VkDevice device, VkRenderPass renderPass);
    ~VulkanRenderPass() override;

    VulkanRenderPass(const VulkanRenderPass&) = delete;
    VulkanRenderPass& operator=(const VulkanRenderPass&) = delete;

    [[nodiscard]] void* GetNativeHandle() const override;
    [[nodiscard]] bool IsValid() const override;
    [[nodiscard]] VkRenderPass GetHandle() const;

private:
    VkDevice device_{VK_NULL_HANDLE};
    VkRenderPass renderPass_{VK_NULL_HANDLE};
};

} // namespace Lvs::Engine::Rendering::Vulkan
