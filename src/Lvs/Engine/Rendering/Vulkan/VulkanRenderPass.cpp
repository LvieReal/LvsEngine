#include "Lvs/Engine/Rendering/Vulkan/VulkanRenderPass.hpp"

namespace Lvs::Engine::Rendering::Vulkan {

VulkanRenderPass::VulkanRenderPass(const VkDevice device, const VkRenderPass renderPass)
    : device_(device),
      renderPass_(renderPass) {
}

VulkanRenderPass::~VulkanRenderPass() {
    if (renderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, renderPass_, nullptr);
    }
}

void* VulkanRenderPass::GetNativeHandle() const {
    return reinterpret_cast<void*>(renderPass_);
}

bool VulkanRenderPass::IsValid() const {
    return renderPass_ != VK_NULL_HANDLE;
}

VkRenderPass VulkanRenderPass::GetHandle() const {
    return renderPass_;
}

} // namespace Lvs::Engine::Rendering::Vulkan
