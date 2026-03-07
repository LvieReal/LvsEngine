#pragma once

#include "Lvs/Engine/Rendering/Common/Framebuffer.hpp"

#include <vulkan/vulkan.h>

namespace Lvs::Engine::Rendering::Vulkan {

class VulkanFramebuffer final : public Common::Framebuffer {
public:
    VulkanFramebuffer(VkDevice device, VkFramebuffer framebuffer, Common::Rect renderArea);
    ~VulkanFramebuffer() override;

    VulkanFramebuffer(const VulkanFramebuffer&) = delete;
    VulkanFramebuffer& operator=(const VulkanFramebuffer&) = delete;

    [[nodiscard]] void* GetNativeHandle() const override;
    [[nodiscard]] Common::Rect GetRenderArea() const override;
    [[nodiscard]] bool IsValid() const override;
    [[nodiscard]] VkFramebuffer GetHandle() const;

private:
    VkDevice device_{VK_NULL_HANDLE};
    VkFramebuffer framebuffer_{VK_NULL_HANDLE};
    Common::Rect renderArea_{};
};

} // namespace Lvs::Engine::Rendering::Vulkan
