#include "Lvs/Engine/Rendering/Vulkan/VulkanFramebuffer.hpp"

namespace Lvs::Engine::Rendering::Vulkan {

VulkanFramebuffer::VulkanFramebuffer(const VkDevice device, const VkFramebuffer framebuffer, const Common::Rect renderArea)
    : device_(device),
      framebuffer_(framebuffer),
      renderArea_(renderArea) {
}

VulkanFramebuffer::~VulkanFramebuffer() {
    if (framebuffer_ != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device_, framebuffer_, nullptr);
    }
}

void* VulkanFramebuffer::GetNativeHandle() const {
    return reinterpret_cast<void*>(framebuffer_);
}

Common::Rect VulkanFramebuffer::GetRenderArea() const {
    return renderArea_;
}

bool VulkanFramebuffer::IsValid() const {
    return framebuffer_ != VK_NULL_HANDLE;
}

VkFramebuffer VulkanFramebuffer::GetHandle() const {
    return framebuffer_;
}

} // namespace Lvs::Engine::Rendering::Vulkan
