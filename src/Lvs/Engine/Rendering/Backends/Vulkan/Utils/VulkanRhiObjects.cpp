#include "Lvs/Engine/Rendering/Backends/Vulkan/Utils/VulkanRhiObjects.hpp"

namespace Lvs::Engine::Rendering::Backends::Vulkan::Utils {

VulkanResourceSet::VulkanResourceSet(
    const VkDevice device,
    const VkDescriptorPool pool,
    const VkDescriptorSet set,
    const bool ownsSet
)
    : device_(device),
      pool_(pool),
      set_(set),
      ownsSet_(ownsSet) {}

VulkanResourceSet::~VulkanResourceSet() {
    if (ownsSet_ && device_ != VK_NULL_HANDLE && pool_ != VK_NULL_HANDLE && set_ != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(device_, pool_, 1, &set_);
        set_ = VK_NULL_HANDLE;
    }
}

void* VulkanResourceSet::GetNativeHandle() const {
    return reinterpret_cast<void*>(set_);
}

VulkanBuffer::VulkanBuffer(
    const VkDevice device,
    const VkBuffer buffer,
    const VkDeviceMemory memory,
    const std::size_t size
)
    : device_(device),
      buffer_(buffer),
      memory_(memory),
      size_(size) {}

VulkanBuffer::~VulkanBuffer() {
    if (device_ != VK_NULL_HANDLE && buffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, buffer_, nullptr);
    }
    if (device_ != VK_NULL_HANDLE && memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, memory_, nullptr);
    }
}

void* VulkanBuffer::GetNativeHandle() const {
    return reinterpret_cast<void*>(buffer_);
}

std::size_t VulkanBuffer::GetSize() const {
    return size_;
}

VulkanRenderTarget::VulkanRenderTarget(
    const VkDevice device,
    const RHI::u32 width,
    const RHI::u32 height,
    const VkFormat colorFormat,
    const VkRenderPass renderPass,
    const VkFramebuffer framebuffer,
    const std::vector<ColorAttachment>& resolveColors,
    const std::vector<ColorAttachment>& msaaColors,
    const VkImage depthImage,
    const VkDeviceMemory depthMemory,
    const VkImageView depthView,
    const VkSampler depthSampler,
    const RHI::Format depthFormat,
    const RHI::u32 sampleCount
)
    : device_(device),
      width_(width),
      height_(height),
      colorFormat_(colorFormat),
      renderPass_(renderPass),
      framebuffer_(framebuffer),
      colorAttachments_(resolveColors),
      msaaColorAttachments_(msaaColors),
      depthImage_(depthImage),
      depthMemory_(depthMemory),
      depthView_(depthView),
      depthSampler_(depthSampler),
      depthFormat_(depthFormat),
      sampleCount_(sampleCount) {
    colorTextures_.reserve(colorAttachments_.size());
    for (const auto& color : colorAttachments_) {
        RHI::Texture texture{};
        texture.width = width_;
        texture.height = height_;
        switch (colorFormat_) {
            case VK_FORMAT_R16G16B16A16_SFLOAT:
                texture.format = RHI::Format::R16G16B16A16_Float;
                break;
            case VK_FORMAT_B8G8R8A8_UNORM:
                texture.format = RHI::Format::B8G8R8A8_UNorm;
                break;
            default:
                texture.format = RHI::Format::R8G8B8A8_UNorm;
                break;
        }
        texture.type = RHI::TextureType::Texture2D;
        texture.graphic_handle_ptr = color.view;
        texture.sampler_handle_ptr = color.sampler;
        colorTextures_.push_back(texture);
    }

    if (depthView_ != VK_NULL_HANDLE) {
        depthTexture_.width = width_;
        depthTexture_.height = height_;
        depthTexture_.format = depthFormat_;
        depthTexture_.type = RHI::TextureType::Texture2D;
        depthTexture_.graphic_handle_ptr = depthView_;
        depthTexture_.sampler_handle_ptr = depthSampler_;
    }
}

VulkanRenderTarget::~VulkanRenderTarget() {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    if (framebuffer_ != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device_, framebuffer_, nullptr);
    }
    if (renderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, renderPass_, nullptr);
    }
    for (const auto& color : msaaColorAttachments_) {
        if (color.view != VK_NULL_HANDLE) {
            vkDestroyImageView(device_, color.view, nullptr);
        }
        if (color.image != VK_NULL_HANDLE) {
            vkDestroyImage(device_, color.image, nullptr);
        }
        if (color.memory != VK_NULL_HANDLE) {
            vkFreeMemory(device_, color.memory, nullptr);
        }
    }
    for (const auto& color : colorAttachments_) {
        if (color.sampler != VK_NULL_HANDLE) {
            vkDestroySampler(device_, color.sampler, nullptr);
        }
        if (color.view != VK_NULL_HANDLE) {
            vkDestroyImageView(device_, color.view, nullptr);
        }
        if (color.image != VK_NULL_HANDLE) {
            vkDestroyImage(device_, color.image, nullptr);
        }
        if (color.memory != VK_NULL_HANDLE) {
            vkFreeMemory(device_, color.memory, nullptr);
        }
    }
    if (depthView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, depthView_, nullptr);
    }
    if (depthSampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device_, depthSampler_, nullptr);
    }
    if (depthImage_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, depthImage_, nullptr);
    }
    if (depthMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, depthMemory_, nullptr);
    }
}

void* VulkanRenderTarget::GetRenderPassHandle() const {
    return reinterpret_cast<void*>(renderPass_);
}

void* VulkanRenderTarget::GetFramebufferHandle() const {
    return reinterpret_cast<void*>(framebuffer_);
}

RHI::u32 VulkanRenderTarget::GetWidth() const {
    return width_;
}

RHI::u32 VulkanRenderTarget::GetHeight() const {
    return height_;
}

RHI::u32 VulkanRenderTarget::GetColorAttachmentCount() const {
    return static_cast<RHI::u32>(colorTextures_.size());
}

RHI::u32 VulkanRenderTarget::GetSampleCount() const {
    return sampleCount_;
}

RHI::Texture VulkanRenderTarget::GetColorTexture(const RHI::u32 index) const {
    if (index >= colorTextures_.size()) {
        return {};
    }
    return colorTextures_[index];
}

bool VulkanRenderTarget::HasDepth() const {
    return depthView_ != VK_NULL_HANDLE;
}

RHI::Texture VulkanRenderTarget::GetDepthTexture() const {
    return depthView_ != VK_NULL_HANDLE ? depthTexture_ : RHI::Texture{};
}

} // namespace Lvs::Engine::Rendering::Backends::Vulkan::Utils

