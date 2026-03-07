#include "Lvs/Engine/Rendering/Vulkan/VulkanFrameManager.hpp"

#include "Lvs/Engine/Rendering/Vulkan/VulkanBufferUtils.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanContext.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanSwapchainUtils.hpp"

#include <array>
#include <stdexcept>
#include <vector>

namespace Lvs::Engine::Rendering::Vulkan {

namespace {

bool IsSuccess(const VkResult result) {
    return result == VK_SUCCESS;
}

VkFormat FindSupportedFormat(
    const VkPhysicalDevice physicalDevice,
    const std::vector<VkFormat>& candidates,
    const VkImageTiling tiling,
    const VkFormatFeatureFlags features
) {
    for (const auto format : candidates) {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        }
        if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }
    throw std::runtime_error("Failed to find supported Vulkan format.");
}

} // namespace

void VulkanFrameManager::Recreate(
    VulkanContext& context,
    const VkSurfaceKHR surface,
    const std::uint32_t width,
    const std::uint32_t height
) {
    if (surface == VK_NULL_HANDLE || width == 0 || height == 0) {
        return;
    }

    vkDeviceWaitIdle(context.GetDevice());
    CleanupSwapchain(context);

    CreateSwapchain(context, surface, width, height);
    CreateImageViews(context);
    SelectOffscreenImageFormat(context);
    CreateDepthResources(context);
    CreateSceneRenderPass(context);
    CreateOffscreenResources(context);
    CreatePostProcessRenderPass(context);
    CreateFramebuffers(context);
    CreateCommandPool(context);
    CreateCommandBuffers(context);

    if (!syncObjectsCreated_) {
        CreateSyncObjects(context);
    }
}

void VulkanFrameManager::CleanupSwapchain(VulkanContext& context) {
    const VkDevice device = context.GetDevice();
    if (device == VK_NULL_HANDLE) {
        return;
    }

    if (commandPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, commandPool_, nullptr);
        commandPool_ = VK_NULL_HANDLE;
        commandBuffers_.clear();
    }

    sceneFramebuffers_.clear();

    swapchainFramebuffers_.clear();

    sceneRenderPass_.reset();
    postProcessRenderPass_.reset();

    for (const auto imageView : depthImageViews_) {
        if (imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(device, imageView, nullptr);
        }
    }
    depthImageViews_.clear();
    for (const auto image : depthImages_) {
        if (image != VK_NULL_HANDLE) {
            vkDestroyImage(device, image, nullptr);
        }
    }
    depthImages_.clear();
    for (const auto memory : depthMemories_) {
        if (memory != VK_NULL_HANDLE) {
            vkFreeMemory(device, memory, nullptr);
        }
    }
    depthMemories_.clear();
    depthFormat_ = VK_FORMAT_UNDEFINED;

    if (offscreenColorSampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device, offscreenColorSampler_, nullptr);
        offscreenColorSampler_ = VK_NULL_HANDLE;
    }
    for (const auto view : offscreenColorImageViews_) {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(device, view, nullptr);
        }
    }
    offscreenColorImageViews_.clear();
    for (const auto view : offscreenGlowImageViews_) {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(device, view, nullptr);
        }
    }
    offscreenGlowImageViews_.clear();
    for (const auto image : offscreenColorImages_) {
        if (image != VK_NULL_HANDLE) {
            vkDestroyImage(device, image, nullptr);
        }
    }
    offscreenColorImages_.clear();
    for (const auto image : offscreenGlowImages_) {
        if (image != VK_NULL_HANDLE) {
            vkDestroyImage(device, image, nullptr);
        }
    }
    offscreenGlowImages_.clear();
    for (const auto memory : offscreenColorMemories_) {
        if (memory != VK_NULL_HANDLE) {
            vkFreeMemory(device, memory, nullptr);
        }
    }
    offscreenColorMemories_.clear();
    for (const auto memory : offscreenGlowMemories_) {
        if (memory != VK_NULL_HANDLE) {
            vkFreeMemory(device, memory, nullptr);
        }
    }
    offscreenGlowMemories_.clear();
    offscreenImageFormat_ = VK_FORMAT_UNDEFINED;

    for (const auto imageView : swapchainImageViews_) {
        if (imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(device, imageView, nullptr);
        }
    }
    swapchainImageViews_.clear();
    swapchainImages_.clear();

    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}

void VulkanFrameManager::CleanupSyncObjects(VulkanContext& context) {
    const VkDevice device = context.GetDevice();
    if (!syncObjectsCreated_ || device == VK_NULL_HANDLE) {
        return;
    }

    for (std::uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (imageAvailableSemaphores_[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, imageAvailableSemaphores_[i], nullptr);
            imageAvailableSemaphores_[i] = VK_NULL_HANDLE;
        }
        if (renderFinishedSemaphores_[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, renderFinishedSemaphores_[i], nullptr);
            renderFinishedSemaphores_[i] = VK_NULL_HANDLE;
        }
        if (inFlightFences_[i] != VK_NULL_HANDLE) {
            vkDestroyFence(device, inFlightFences_[i], nullptr);
            inFlightFences_[i] = VK_NULL_HANDLE;
        }
    }

    syncObjectsCreated_ = false;
}

void VulkanFrameManager::Cleanup(VulkanContext& context) {
    CleanupSyncObjects(context);
    CleanupSwapchain(context);
}

bool VulkanFrameManager::HasSwapchain() const {
    return swapchain_ != VK_NULL_HANDLE;
}

VulkanFrameManager::FrameState VulkanFrameManager::BeginFrame(VulkanContext& context, bool& needsRecreate) {
    needsRecreate = false;
    FrameState frameState{};

    if (context.GetDevice() == VK_NULL_HANDLE || swapchain_ == VK_NULL_HANDLE) {
        return frameState;
    }

    vkWaitForFences(context.GetDevice(), 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);

    std::uint32_t imageIndex = 0;
    const VkResult result = vkAcquireNextImageKHR(
        context.GetDevice(),
        swapchain_,
        UINT64_MAX,
        imageAvailableSemaphores_[currentFrame_],
        VK_NULL_HANDLE,
        &imageIndex
    );
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        needsRecreate = true;
        return frameState;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swapchain image.");
    }

    vkResetFences(context.GetDevice(), 1, &inFlightFences_[currentFrame_]);
    vkResetCommandBuffer(commandBuffers_[currentFrame_], 0);

    frameState.CommandBuffer = commandBuffers_[currentFrame_];
    frameState.ImageIndex = imageIndex;
    frameState.FrameIndex = currentFrame_;
    return frameState;
}

bool VulkanFrameManager::EndFrame(VulkanContext& context, const FrameState& frameState) {
    if (frameState.CommandBuffer == VK_NULL_HANDLE) {
        return false;
    }

    constexpr VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    const VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &imageAvailableSemaphores_[frameState.FrameIndex],
        .pWaitDstStageMask = waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &frameState.CommandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &renderFinishedSemaphores_[frameState.FrameIndex]
    };

    if (!IsSuccess(vkQueueSubmit(context.GetGraphicsQueue(), 1, &submitInfo, inFlightFences_[frameState.FrameIndex]))) {
        throw std::runtime_error("Failed to submit draw command buffer.");
    }

    const VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderFinishedSemaphores_[frameState.FrameIndex],
        .swapchainCount = 1,
        .pSwapchains = &swapchain_,
        .pImageIndices = &frameState.ImageIndex,
        .pResults = nullptr
    };

    const VkResult result = vkQueuePresentKHR(context.GetGraphicsQueue(), &presentInfo);
    const bool valid = result == VK_SUCCESS;
    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
    return valid;
}

VkFormat VulkanFrameManager::GetSwapchainImageFormat() const {
    return swapchainImageFormat_;
}

VkFormat VulkanFrameManager::GetOffscreenImageFormat() const {
    return offscreenImageFormat_;
}

VkExtent2D VulkanFrameManager::GetSwapchainExtentVk() const {
    return swapchainExtent_;
}

Common::Extent2D VulkanFrameManager::GetExtent() const {
    return Common::Extent2D{.Width = swapchainExtent_.width, .Height = swapchainExtent_.height};
}

std::uint32_t VulkanFrameManager::GetImageCount() const {
    return static_cast<std::uint32_t>(swapchainImages_.size());
}

std::uint32_t VulkanFrameManager::GetFramesInFlight() const {
    return MAX_FRAMES_IN_FLIGHT;
}

const Common::RenderPass& VulkanFrameManager::GetSceneRenderPass() const {
    return *sceneRenderPass_;
}

const Common::RenderPass& VulkanFrameManager::GetPostProcessRenderPass() const {
    return *postProcessRenderPass_;
}

const Common::Framebuffer& VulkanFrameManager::GetSceneFramebuffer(const std::uint32_t imageIndex) const {
    return *sceneFramebuffers_.at(imageIndex);
}

const Common::Framebuffer& VulkanFrameManager::GetSwapchainFramebuffer(const std::uint32_t imageIndex) const {
    return *swapchainFramebuffers_.at(imageIndex);
}

Common::NativeSampledImage VulkanFrameManager::GetOffscreenColorImage(const std::uint32_t imageIndex) const {
    if (imageIndex >= offscreenColorImages_.size()) {
        return {};
    }
    return Common::NativeSampledImage{
        .Image = reinterpret_cast<void*>(offscreenColorImages_[imageIndex]),
        .View = reinterpret_cast<void*>(offscreenColorImageViews_[imageIndex]),
        .Sampler = reinterpret_cast<void*>(offscreenColorSampler_)
    };
}

Common::NativeSampledImage VulkanFrameManager::GetOffscreenGlowImage(const std::uint32_t imageIndex) const {
    if (imageIndex >= offscreenGlowImages_.size()) {
        return {};
    }
    return Common::NativeSampledImage{
        .Image = reinterpret_cast<void*>(offscreenGlowImages_[imageIndex]),
        .View = reinterpret_cast<void*>(offscreenGlowImageViews_[imageIndex]),
        .Sampler = reinterpret_cast<void*>(offscreenColorSampler_)
    };
}

void VulkanFrameManager::CreateSwapchain(
    VulkanContext& context,
    const VkSurfaceKHR surface,
    const std::uint32_t width,
    const std::uint32_t height
) {
    const auto support = SwapchainUtils::QuerySwapchainSupport(context.GetPhysicalDevice(), surface);
    const auto surfaceFormat = SwapchainUtils::ChooseSurfaceFormat(support.Formats);
    const auto presentMode = SwapchainUtils::ChoosePresentMode(support.PresentModes);
    const auto extent = SwapchainUtils::ChooseExtent(support.Capabilities, width, height);

    std::uint32_t imageCount = support.Capabilities.minImageCount + 1;
    if (support.Capabilities.maxImageCount > 0 && imageCount > support.Capabilities.maxImageCount) {
        imageCount = support.Capabilities.maxImageCount;
    }

    const VkSwapchainCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .surface = surface,
        .minImageCount = imageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .preTransform = support.Capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE
    };
    if (!IsSuccess(vkCreateSwapchainKHR(context.GetDevice(), &createInfo, nullptr, &swapchain_))) {
        throw std::runtime_error("Failed to create Vulkan swapchain.");
    }

    vkGetSwapchainImagesKHR(context.GetDevice(), swapchain_, &imageCount, nullptr);
    swapchainImages_.resize(imageCount);
    vkGetSwapchainImagesKHR(context.GetDevice(), swapchain_, &imageCount, swapchainImages_.data());
    swapchainImageFormat_ = surfaceFormat.format;
    swapchainExtent_ = extent;
}

void VulkanFrameManager::CreateImageViews(VulkanContext& context) {
    swapchainImageViews_.resize(swapchainImages_.size());
    for (std::size_t i = 0; i < swapchainImages_.size(); ++i) {
        const VkImageViewCreateInfo createInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = swapchainImages_[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = swapchainImageFormat_,
            .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                           .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                           .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                           .a = VK_COMPONENT_SWIZZLE_IDENTITY},
            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                 .baseMipLevel = 0,
                                 .levelCount = 1,
                                 .baseArrayLayer = 0,
                                 .layerCount = 1}
        };
        if (!IsSuccess(vkCreateImageView(context.GetDevice(), &createInfo, nullptr, &swapchainImageViews_[i]))) {
            throw std::runtime_error("Failed to create swapchain image view.");
        }
    }
}

void VulkanFrameManager::SelectOffscreenImageFormat(VulkanContext& context) {
    offscreenImageFormat_ = FindSupportedFormat(
        context.GetPhysicalDevice(),
        {VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
    );
}

void VulkanFrameManager::CreateSceneRenderPass(VulkanContext& context) {
    const std::array attachments{
        VkAttachmentDescription{
            .flags = 0,
            .format = offscreenImageFormat_,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        },
        VkAttachmentDescription{
            .flags = 0,
            .format = offscreenImageFormat_,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        },
        VkAttachmentDescription{
            .flags = 0,
            .format = depthFormat_,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        }
    };
    const std::array colorRefs{
        VkAttachmentReference{.attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        VkAttachmentReference{.attachment = 1, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}
    };
    const VkAttachmentReference depthRef{.attachment = 2, .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    const VkSubpassDescription subpass{
        .flags = 0,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = 0,
        .pInputAttachments = nullptr,
        .colorAttachmentCount = static_cast<std::uint32_t>(colorRefs.size()),
        .pColorAttachments = colorRefs.data(),
        .pResolveAttachments = nullptr,
        .pDepthStencilAttachment = &depthRef,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = nullptr
    };
    const VkRenderPassCreateInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .attachmentCount = static_cast<std::uint32_t>(attachments.size()),
        .pAttachments = attachments.data(),
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 0,
        .pDependencies = nullptr
    };
    VkRenderPass renderPass = VK_NULL_HANDLE;
    if (!IsSuccess(vkCreateRenderPass(context.GetDevice(), &renderPassInfo, nullptr, &renderPass))) {
        throw std::runtime_error("Failed to create scene render pass.");
    }
    sceneRenderPass_ = std::make_unique<VulkanRenderPass>(context.GetDevice(), renderPass);
}

void VulkanFrameManager::CreatePostProcessRenderPass(VulkanContext& context) {
    const VkAttachmentDescription colorAttachment{
        .flags = 0,
        .format = swapchainImageFormat_,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };
    const VkAttachmentReference colorRef{.attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    const VkSubpassDescription subpass{
        .flags = 0,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = 0,
        .pInputAttachments = nullptr,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorRef,
        .pResolveAttachments = nullptr,
        .pDepthStencilAttachment = nullptr,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = nullptr
    };
    const VkRenderPassCreateInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .attachmentCount = 1,
        .pAttachments = &colorAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 0,
        .pDependencies = nullptr
    };
    VkRenderPass renderPass = VK_NULL_HANDLE;
    if (!IsSuccess(vkCreateRenderPass(context.GetDevice(), &renderPassInfo, nullptr, &renderPass))) {
        throw std::runtime_error("Failed to create post-process render pass.");
    }
    postProcessRenderPass_ = std::make_unique<VulkanRenderPass>(context.GetDevice(), renderPass);
}

void VulkanFrameManager::CreateFramebuffers(VulkanContext& context) {
    sceneFramebuffers_.resize(swapchainImages_.size());
    swapchainFramebuffers_.resize(swapchainImages_.size());

    for (std::size_t i = 0; i < swapchainImages_.size(); ++i) {
        const std::array sceneAttachments{offscreenColorImageViews_[i], offscreenGlowImageViews_[i], depthImageViews_[i]};
        const auto sceneRenderPass = reinterpret_cast<VkRenderPass>(sceneRenderPass_->GetNativeHandle());
        const VkFramebufferCreateInfo sceneInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderPass = sceneRenderPass,
            .attachmentCount = static_cast<std::uint32_t>(sceneAttachments.size()),
            .pAttachments = sceneAttachments.data(),
            .width = swapchainExtent_.width,
            .height = swapchainExtent_.height,
            .layers = 1
        };
        VkFramebuffer sceneFramebuffer = VK_NULL_HANDLE;
        if (!IsSuccess(vkCreateFramebuffer(context.GetDevice(), &sceneInfo, nullptr, &sceneFramebuffer))) {
            throw std::runtime_error("Failed to create scene framebuffer.");
        }
        sceneFramebuffers_[i] = std::make_unique<VulkanFramebuffer>(
            context.GetDevice(),
            sceneFramebuffer,
            Common::Rect{.X = 0, .Y = 0, .Width = swapchainExtent_.width, .Height = swapchainExtent_.height}
        );

        const auto postProcessRenderPass = reinterpret_cast<VkRenderPass>(postProcessRenderPass_->GetNativeHandle());
        const VkFramebufferCreateInfo postInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderPass = postProcessRenderPass,
            .attachmentCount = 1,
            .pAttachments = &swapchainImageViews_[i],
            .width = swapchainExtent_.width,
            .height = swapchainExtent_.height,
            .layers = 1
        };
        VkFramebuffer swapchainFramebuffer = VK_NULL_HANDLE;
        if (!IsSuccess(vkCreateFramebuffer(context.GetDevice(), &postInfo, nullptr, &swapchainFramebuffer))) {
            throw std::runtime_error("Failed to create swapchain framebuffer.");
        }
        swapchainFramebuffers_[i] = std::make_unique<VulkanFramebuffer>(
            context.GetDevice(),
            swapchainFramebuffer,
            Common::Rect{.X = 0, .Y = 0, .Width = swapchainExtent_.width, .Height = swapchainExtent_.height}
        );
    }
}

void VulkanFrameManager::CreateOffscreenResources(VulkanContext& context) {
    offscreenColorImages_.resize(swapchainImages_.size(), VK_NULL_HANDLE);
    offscreenGlowImages_.resize(swapchainImages_.size(), VK_NULL_HANDLE);
    offscreenColorImageViews_.resize(swapchainImages_.size(), VK_NULL_HANDLE);
    offscreenGlowImageViews_.resize(swapchainImages_.size(), VK_NULL_HANDLE);
    offscreenColorMemories_.resize(swapchainImages_.size(), VK_NULL_HANDLE);
    offscreenGlowMemories_.resize(swapchainImages_.size(), VK_NULL_HANDLE);

    const auto createImageSet = [&](VkImage& image, VkDeviceMemory& memory, VkImageView& view) {
        const VkImageCreateInfo imageInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = offscreenImageFormat_,
            .extent = {.width = swapchainExtent_.width, .height = swapchainExtent_.height, .depth = 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };
        if (!IsSuccess(vkCreateImage(context.GetDevice(), &imageInfo, nullptr, &image))) {
            throw std::runtime_error("Failed to create offscreen image.");
        }

        VkMemoryRequirements memReq{};
        vkGetImageMemoryRequirements(context.GetDevice(), image, &memReq);
        const VkMemoryAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .allocationSize = memReq.size,
            .memoryTypeIndex = BufferUtils::FindMemoryType(
                context.GetPhysicalDevice(),
                memReq.memoryTypeBits,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
            )
        };
        if (!IsSuccess(vkAllocateMemory(context.GetDevice(), &allocInfo, nullptr, &memory))) {
            throw std::runtime_error("Failed to allocate offscreen image memory.");
        }
        vkBindImageMemory(context.GetDevice(), image, memory, 0);

        const VkImageViewCreateInfo viewInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = offscreenImageFormat_,
            .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                           .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                           .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                           .a = VK_COMPONENT_SWIZZLE_IDENTITY},
            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                 .baseMipLevel = 0,
                                 .levelCount = 1,
                                 .baseArrayLayer = 0,
                                 .layerCount = 1}
        };
        if (!IsSuccess(vkCreateImageView(context.GetDevice(), &viewInfo, nullptr, &view))) {
            throw std::runtime_error("Failed to create offscreen image view.");
        }
    };

    for (std::size_t i = 0; i < swapchainImages_.size(); ++i) {
        createImageSet(offscreenColorImages_[i], offscreenColorMemories_[i], offscreenColorImageViews_[i]);
        createImageSet(offscreenGlowImages_[i], offscreenGlowMemories_[i], offscreenGlowImageViews_[i]);
    }

    const VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias = 0.0F,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0F,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_NEVER,
        .minLod = 0.0F,
        .maxLod = 0.0F,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE
    };
    if (!IsSuccess(vkCreateSampler(context.GetDevice(), &samplerInfo, nullptr, &offscreenColorSampler_))) {
        throw std::runtime_error("Failed to create offscreen color sampler.");
    }
}

void VulkanFrameManager::CreateDepthResources(VulkanContext& context) {
    depthFormat_ = FindSupportedFormat(
        context.GetPhysicalDevice(),
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );

    depthImages_.resize(swapchainImages_.size(), VK_NULL_HANDLE);
    depthImageViews_.resize(swapchainImages_.size(), VK_NULL_HANDLE);
    depthMemories_.resize(swapchainImages_.size(), VK_NULL_HANDLE);

    for (std::size_t i = 0; i < swapchainImages_.size(); ++i) {
        const VkImageCreateInfo imageInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = depthFormat_,
            .extent = {.width = swapchainExtent_.width, .height = swapchainExtent_.height, .depth = 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };
        if (!IsSuccess(vkCreateImage(context.GetDevice(), &imageInfo, nullptr, &depthImages_[i]))) {
            throw std::runtime_error("Failed to create depth image.");
        }

        VkMemoryRequirements memReq{};
        vkGetImageMemoryRequirements(context.GetDevice(), depthImages_[i], &memReq);
        const VkMemoryAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .allocationSize = memReq.size,
            .memoryTypeIndex = BufferUtils::FindMemoryType(
                context.GetPhysicalDevice(),
                memReq.memoryTypeBits,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
            )
        };
        if (!IsSuccess(vkAllocateMemory(context.GetDevice(), &allocInfo, nullptr, &depthMemories_[i]))) {
            throw std::runtime_error("Failed to allocate depth image memory.");
        }
        vkBindImageMemory(context.GetDevice(), depthImages_[i], depthMemories_[i], 0);

        const VkImageViewCreateInfo viewInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = depthImages_[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = depthFormat_,
            .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                           .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                           .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                           .a = VK_COMPONENT_SWIZZLE_IDENTITY},
            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                                 .baseMipLevel = 0,
                                 .levelCount = 1,
                                 .baseArrayLayer = 0,
                                 .layerCount = 1}
        };
        if (!IsSuccess(vkCreateImageView(context.GetDevice(), &viewInfo, nullptr, &depthImageViews_[i]))) {
            throw std::runtime_error("Failed to create depth image view.");
        }
    }
}

void VulkanFrameManager::CreateCommandPool(VulkanContext& context) {
    const VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = context.GetGraphicsQueueFamily()
    };
    if (!IsSuccess(vkCreateCommandPool(context.GetDevice(), &poolInfo, nullptr, &commandPool_))) {
        throw std::runtime_error("Failed to create Vulkan command pool.");
    }
}

void VulkanFrameManager::CreateCommandBuffers(VulkanContext& context) {
    commandBuffers_.resize(MAX_FRAMES_IN_FLIGHT);
    const VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = commandPool_,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<std::uint32_t>(commandBuffers_.size())
    };
    if (!IsSuccess(vkAllocateCommandBuffers(context.GetDevice(), &allocInfo, commandBuffers_.data()))) {
        throw std::runtime_error("Failed to allocate Vulkan command buffers.");
    }
}

void VulkanFrameManager::CreateSyncObjects(VulkanContext& context) {
    const VkSemaphoreCreateInfo semaphoreInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0
    };
    const VkFenceCreateInfo fenceInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    for (std::uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (!IsSuccess(vkCreateSemaphore(context.GetDevice(), &semaphoreInfo, nullptr, &imageAvailableSemaphores_[i])) ||
            !IsSuccess(vkCreateSemaphore(context.GetDevice(), &semaphoreInfo, nullptr, &renderFinishedSemaphores_[i])) ||
            !IsSuccess(vkCreateFence(context.GetDevice(), &fenceInfo, nullptr, &inFlightFences_[i]))) {
            throw std::runtime_error("Failed to create Vulkan synchronization objects.");
        }
    }
    syncObjectsCreated_ = true;
    currentFrame_ = 0;
}

} // namespace Lvs::Engine::Rendering::Vulkan
