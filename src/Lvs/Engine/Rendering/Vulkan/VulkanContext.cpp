#include "Lvs/Engine/Rendering/Vulkan/VulkanContext.hpp"

#include "Lvs/Engine/Rendering/Vulkan/PostProcessRenderer.hpp"
#include "Lvs/Engine/Rendering/Vulkan/Renderer.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanBufferUtils.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanSwapchainUtils.hpp"

#include <array>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Lvs::Engine::Rendering::Vulkan {

namespace {

[[nodiscard]] bool IsSuccess(const VkResult result) {
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

VulkanContext::~VulkanContext() {
    Shutdown();
}

VulkanContext::VulkanContext() = default;

void VulkanContext::Initialize() {
    CreateInstance();
}

void VulkanContext::AttachToNativeWindow(
    void* nativeWindowHandle,
    const std::uint32_t width,
    const std::uint32_t height
) {
    if (instance_ == VK_NULL_HANDLE) {
        throw std::runtime_error("Vulkan instance is not initialized.");
    }
    if (nativeWindowHandle == nullptr) {
        throw std::runtime_error("Native window handle is null.");
    }
    if (width == 0 || height == 0) {
        return;
    }

    if (nativeWindowHandle_ == nativeWindowHandle && swapchain_ != VK_NULL_HANDLE) {
        Resize(width, height);
        return;
    }

    nativeWindowHandle_ = nativeWindowHandle;
    lastWidth_ = width;
    lastHeight_ = height;

    CleanupDeviceAndSurface();
    CreateSurface(nativeWindowHandle_);
    PickPhysicalDevice();
    CreateLogicalDevice();
    RecreateSwapchain(width, height);
}

void VulkanContext::Resize(const std::uint32_t width, const std::uint32_t height) {
    if (width == 0 || height == 0) {
        return;
    }
    lastWidth_ = width;
    lastHeight_ = height;
}

void VulkanContext::Render() {
    if (device_ == VK_NULL_HANDLE || swapchain_ == VK_NULL_HANDLE) {
        return;
    }

    vkWaitForFences(device_, 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);

    std::uint32_t imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(
        device_,
        swapchain_,
        UINT64_MAX,
        imageAvailableSemaphores_[currentFrame_],
        VK_NULL_HANDLE,
        &imageIndex
    );
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        RecreateSwapchain(lastWidth_, lastHeight_);
        return;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swapchain image.");
    }

    vkResetFences(device_, 1, &inFlightFences_[currentFrame_]);
    vkResetCommandBuffer(commandBuffers_[currentFrame_], 0);
    RecordCommandBuffer(commandBuffers_[currentFrame_], imageIndex);

    constexpr VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    const VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &imageAvailableSemaphores_[currentFrame_],
        .pWaitDstStageMask = waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffers_[currentFrame_],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &renderFinishedSemaphores_[currentFrame_]
    };

    if (!IsSuccess(vkQueueSubmit(graphicsQueue_, 1, &submitInfo, inFlightFences_[currentFrame_]))) {
        throw std::runtime_error("Failed to submit draw command buffer.");
    }

    const VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderFinishedSemaphores_[currentFrame_],
        .swapchainCount = 1,
        .pSwapchains = &swapchain_,
        .pImageIndices = &imageIndex,
        .pResults = nullptr
    };

    result = vkQueuePresentKHR(graphicsQueue_, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        RecreateSwapchain(lastWidth_, lastHeight_);
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swapchain image.");
    }

    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanContext::SetClearColor(const float r, const float g, const float b, const float a) {
    clearColor_ = {{r, g, b, a}};
}

void VulkanContext::SetOverlayPrimitives(std::vector<OverlayPrimitive> primitives) {
    overlayPrimitives_ = std::move(primitives);
    if (renderer_ != nullptr) {
        renderer_->SetOverlayPrimitives(overlayPrimitives_);
    }
}

void VulkanContext::BindToPlace(const std::shared_ptr<DataModel::Place>& place) {
    currentPlace_ = place;
    if (renderer_ != nullptr) {
        renderer_->BindToPlace(place);
    }
    if (postProcessRenderer_ != nullptr) {
        postProcessRenderer_->BindToPlace(place);
    }
}

void VulkanContext::Unbind() {
    currentPlace_.reset();
    if (renderer_ != nullptr) {
        renderer_->Unbind();
    }
    if (postProcessRenderer_ != nullptr) {
        postProcessRenderer_->Unbind();
    }
}

void VulkanContext::Shutdown() {
    CleanupDeviceAndSurface();

    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
}

VkInstance VulkanContext::GetInstance() const {
    return instance_;
}

VkPhysicalDevice VulkanContext::GetPhysicalDevice() const {
    return physicalDevice_;
}

VkDevice VulkanContext::GetDevice() const {
    return device_;
}

VkQueue VulkanContext::GetGraphicsQueue() const {
    return graphicsQueue_;
}

std::uint32_t VulkanContext::GetGraphicsQueueFamily() const {
    return graphicsQueueFamily_;
}

VkRenderPass VulkanContext::GetRenderPass() const {
    return sceneRenderPass_;
}

VkRenderPass VulkanContext::GetPostProcessRenderPass() const {
    return postProcessRenderPass_;
}

VkFormat VulkanContext::GetSwapchainImageFormat() const {
    return swapchainImageFormat_;
}

VkExtent2D VulkanContext::GetSwapchainExtent() const {
    return swapchainExtent_;
}

std::uint32_t VulkanContext::GetFramesInFlight() const {
    return MAX_FRAMES_IN_FLIGHT;
}

void VulkanContext::CreateInstance() {
    const VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = "Lvs Engine",
        .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
        .pEngineName = "Lvs Engine",
        .engineVersion = VK_MAKE_VERSION(0, 1, 0),
        .apiVersion = VK_API_VERSION_1_3
    };

    std::vector<const char*> extensions{VK_KHR_SURFACE_EXTENSION_NAME};
#ifdef _WIN32
    extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif

    const VkInstanceCreateInfo instanceInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data()
    };

    if (!IsSuccess(vkCreateInstance(&instanceInfo, nullptr, &instance_))) {
        throw std::runtime_error("Failed to create Vulkan instance.");
    }
}

void VulkanContext::CreateSurface(void* nativeWindowHandle) {
#ifdef _WIN32
    const auto hwnd = reinterpret_cast<HWND>(nativeWindowHandle);
    if (hwnd == nullptr) {
        throw std::runtime_error("Failed to retrieve Win32 window handle.");
    }

    const VkWin32SurfaceCreateInfoKHR surfaceInfo{
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .hinstance = GetModuleHandleW(nullptr),
        .hwnd = hwnd
    };

    if (!IsSuccess(vkCreateWin32SurfaceKHR(instance_, &surfaceInfo, nullptr, &surface_))) {
        throw std::runtime_error("Failed to create Vulkan Win32 surface.");
    }
#else
    static_cast<void>(nativeWindowHandle);
    throw std::runtime_error("Vulkan surface creation is currently implemented only for Windows.");
#endif
}

void VulkanContext::PickPhysicalDevice() {
    std::uint32_t deviceCount = 0;
    if (!IsSuccess(vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr)) || deviceCount == 0) {
        throw std::runtime_error("No Vulkan physical devices available.");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    if (!IsSuccess(vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data()))) {
        throw std::runtime_error("Failed to enumerate Vulkan physical devices.");
    }

    for (const auto& device : devices) {
        if (IsPhysicalDeviceSuitable(device)) {
            physicalDevice_ = device;
            graphicsQueueFamily_ = FindGraphicsPresentQueueFamily(device);
            return;
        }
    }

    throw std::runtime_error("No suitable Vulkan physical device found.");
}

void VulkanContext::CreateLogicalDevice() {
    constexpr float queuePriority = 1.0F;
    const VkDeviceQueueCreateInfo queueInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueFamilyIndex = graphicsQueueFamily_,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority
    };

    const std::vector<const char*> deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    const VkPhysicalDeviceFeatures features{};
    const VkDeviceCreateInfo deviceInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = static_cast<std::uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
        .pEnabledFeatures = &features
    };

    if (!IsSuccess(vkCreateDevice(physicalDevice_, &deviceInfo, nullptr, &device_))) {
        throw std::runtime_error("Failed to create Vulkan logical device.");
    }

    vkGetDeviceQueue(device_, graphicsQueueFamily_, 0, &graphicsQueue_);
}

void VulkanContext::CreateSwapchain(const std::uint32_t width, const std::uint32_t height) {
    const auto support = SwapchainUtils::QuerySwapchainSupport(physicalDevice_, surface_);
    if (support.Formats.empty() || support.PresentModes.empty()) {
        throw std::runtime_error("Swapchain support details are incomplete.");
    }

    const VkSurfaceFormatKHR surfaceFormat = SwapchainUtils::ChooseSurfaceFormat(support.Formats);
    const VkPresentModeKHR presentMode = SwapchainUtils::ChoosePresentMode(support.PresentModes);
    const VkExtent2D extent = SwapchainUtils::ChooseExtent(support.Capabilities, width, height);

    std::uint32_t imageCount = support.Capabilities.minImageCount + 1;
    if (support.Capabilities.maxImageCount > 0 && imageCount > support.Capabilities.maxImageCount) {
        imageCount = support.Capabilities.maxImageCount;
    }

    const VkSwapchainCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .surface = surface_,
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

    if (!IsSuccess(vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapchain_))) {
        throw std::runtime_error("Failed to create swapchain.");
    }

    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr);
    swapchainImages_.resize(imageCount);
    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, swapchainImages_.data());

    swapchainImageFormat_ = surfaceFormat.format;
    swapchainExtent_ = extent;
}

void VulkanContext::CreateImageViews() {
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

        if (!IsSuccess(vkCreateImageView(device_, &createInfo, nullptr, &swapchainImageViews_[i]))) {
            throw std::runtime_error("Failed to create swapchain image view.");
        }
    }
}

void VulkanContext::CreateSceneRenderPass() {
    const VkAttachmentDescription sceneColorAttachment{
        .flags = 0,
        .format = swapchainImageFormat_,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    const VkAttachmentDescription glowColorAttachment{
        .flags = 0,
        .format = swapchainImageFormat_,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    const VkAttachmentDescription depthAttachment{
        .flags = 0,
        .format = depthFormat_,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
    };

    const VkAttachmentReference sceneColorAttachmentRef{
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };
    const VkAttachmentReference glowColorAttachmentRef{
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };
    const std::array colorAttachmentRefs{sceneColorAttachmentRef, glowColorAttachmentRef};
    const VkAttachmentReference depthAttachmentRef{
        .attachment = 2,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };

    const VkSubpassDescription subpass{
        .flags = 0,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = 0,
        .pInputAttachments = nullptr,
        .colorAttachmentCount = static_cast<std::uint32_t>(colorAttachmentRefs.size()),
        .pColorAttachments = colorAttachmentRefs.data(),
        .pResolveAttachments = nullptr,
        .pDepthStencilAttachment = &depthAttachmentRef,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = nullptr
    };

    const VkSubpassDependency dependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = 0
    };
    const std::array attachments{sceneColorAttachment, glowColorAttachment, depthAttachment};

    const VkRenderPassCreateInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .attachmentCount = static_cast<std::uint32_t>(attachments.size()),
        .pAttachments = attachments.data(),
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency
    };

    if (!IsSuccess(vkCreateRenderPass(device_, &renderPassInfo, nullptr, &sceneRenderPass_))) {
        throw std::runtime_error("Failed to create Vulkan scene render pass.");
    }
}

void VulkanContext::CreatePostProcessRenderPass() {
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
    const VkAttachmentReference colorAttachmentRef{
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };
    const VkSubpassDescription subpass{
        .flags = 0,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = 0,
        .pInputAttachments = nullptr,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentRef,
        .pResolveAttachments = nullptr,
        .pDepthStencilAttachment = nullptr,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = nullptr
    };
    const VkSubpassDependency dependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = 0
    };

    const VkRenderPassCreateInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .attachmentCount = 1,
        .pAttachments = &colorAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency
    };
    if (!IsSuccess(vkCreateRenderPass(device_, &renderPassInfo, nullptr, &postProcessRenderPass_))) {
        throw std::runtime_error("Failed to create Vulkan post-process render pass.");
    }
}

void VulkanContext::CreateFramebuffers() {
    sceneFramebuffers_.resize(swapchainImageViews_.size());
    swapchainFramebuffers_.resize(swapchainImageViews_.size());

    for (std::size_t i = 0; i < swapchainImageViews_.size(); ++i) {
        const std::array attachments{
            offscreenColorImageViews_[i],
            offscreenGlowImageViews_[i],
            depthImageViews_[i],
        };
        const VkFramebufferCreateInfo framebufferInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderPass = sceneRenderPass_,
            .attachmentCount = static_cast<std::uint32_t>(attachments.size()),
            .pAttachments = attachments.data(),
            .width = swapchainExtent_.width,
            .height = swapchainExtent_.height,
            .layers = 1
        };

        if (!IsSuccess(vkCreateFramebuffer(device_, &framebufferInfo, nullptr, &sceneFramebuffers_[i]))) {
            throw std::runtime_error("Failed to create Vulkan scene framebuffer.");
        }

        const VkFramebufferCreateInfo postFramebufferInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderPass = postProcessRenderPass_,
            .attachmentCount = 1,
            .pAttachments = &swapchainImageViews_[i],
            .width = swapchainExtent_.width,
            .height = swapchainExtent_.height,
            .layers = 1
        };
        if (!IsSuccess(vkCreateFramebuffer(device_, &postFramebufferInfo, nullptr, &swapchainFramebuffers_[i]))) {
            throw std::runtime_error("Failed to create Vulkan post-process framebuffer.");
        }
    }
}

void VulkanContext::CreateOffscreenResources() {
    offscreenColorImages_.resize(swapchainImages_.size(), VK_NULL_HANDLE);
    offscreenGlowImages_.resize(swapchainImages_.size(), VK_NULL_HANDLE);
    offscreenColorImageViews_.resize(swapchainImages_.size(), VK_NULL_HANDLE);
    offscreenGlowImageViews_.resize(swapchainImages_.size(), VK_NULL_HANDLE);
    offscreenColorMemories_.resize(swapchainImages_.size(), VK_NULL_HANDLE);
    offscreenGlowMemories_.resize(swapchainImages_.size(), VK_NULL_HANDLE);

    for (std::size_t i = 0; i < swapchainImages_.size(); ++i) {
        const VkImageCreateInfo imageInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = swapchainImageFormat_,
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
        if (vkCreateImage(device_, &imageInfo, nullptr, &offscreenColorImages_[i]) != VK_SUCCESS ||
            vkCreateImage(device_, &imageInfo, nullptr, &offscreenGlowImages_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create offscreen color image.");
        }

        VkMemoryRequirements colorMemReq{};
        vkGetImageMemoryRequirements(device_, offscreenColorImages_[i], &colorMemReq);
        const VkMemoryAllocateInfo colorAllocInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .allocationSize = colorMemReq.size,
            .memoryTypeIndex = BufferUtils::FindMemoryType(
                physicalDevice_,
                colorMemReq.memoryTypeBits,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
            )
        };
        if (vkAllocateMemory(device_, &colorAllocInfo, nullptr, &offscreenColorMemories_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate offscreen color memory.");
        }
        vkBindImageMemory(device_, offscreenColorImages_[i], offscreenColorMemories_[i], 0);
        VkMemoryRequirements glowMemReq{};
        vkGetImageMemoryRequirements(device_, offscreenGlowImages_[i], &glowMemReq);
        const VkMemoryAllocateInfo glowAllocInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .allocationSize = glowMemReq.size,
            .memoryTypeIndex = BufferUtils::FindMemoryType(
                physicalDevice_,
                glowMemReq.memoryTypeBits,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
            )
        };
        if (vkAllocateMemory(device_, &glowAllocInfo, nullptr, &offscreenGlowMemories_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate offscreen glow memory.");
        }
        vkBindImageMemory(device_, offscreenGlowImages_[i], offscreenGlowMemories_[i], 0);

        const VkImageViewCreateInfo colorViewInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = offscreenColorImages_[i],
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
        if (vkCreateImageView(device_, &colorViewInfo, nullptr, &offscreenColorImageViews_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create offscreen color image view.");
        }

        const VkImageViewCreateInfo glowViewInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = offscreenGlowImages_[i],
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
        if (vkCreateImageView(device_, &glowViewInfo, nullptr, &offscreenGlowImageViews_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create offscreen glow image view.");
        }
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
    if (vkCreateSampler(device_, &samplerInfo, nullptr, &offscreenColorSampler_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create offscreen color sampler.");
    }
}

void VulkanContext::CreateDepthResources() {
    depthFormat_ = FindSupportedFormat(
        physicalDevice_,
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
        if (vkCreateImage(device_, &imageInfo, nullptr, &depthImages_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create depth image.");
        }

        VkMemoryRequirements memReq{};
        vkGetImageMemoryRequirements(device_, depthImages_[i], &memReq);
        const VkMemoryAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .allocationSize = memReq.size,
            .memoryTypeIndex = BufferUtils::FindMemoryType(
                physicalDevice_,
                memReq.memoryTypeBits,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
            )
        };
        if (vkAllocateMemory(device_, &allocInfo, nullptr, &depthMemories_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate depth image memory.");
        }
        vkBindImageMemory(device_, depthImages_[i], depthMemories_[i], 0);

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
        if (vkCreateImageView(device_, &viewInfo, nullptr, &depthImageViews_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create depth image view.");
        }
    }

}

void VulkanContext::CreateCommandPool() {
    const VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = graphicsQueueFamily_
    };
    if (!IsSuccess(vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_))) {
        throw std::runtime_error("Failed to create Vulkan command pool.");
    }
}

void VulkanContext::CreateCommandBuffers() {
    commandBuffers_.resize(MAX_FRAMES_IN_FLIGHT);
    const VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = commandPool_,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<std::uint32_t>(commandBuffers_.size())
    };

    if (!IsSuccess(vkAllocateCommandBuffers(device_, &allocInfo, commandBuffers_.data()))) {
        throw std::runtime_error("Failed to allocate Vulkan command buffers.");
    }
}

void VulkanContext::CreateSyncObjects() {
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
        if (!IsSuccess(vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &imageAvailableSemaphores_[i])) ||
            !IsSuccess(vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &renderFinishedSemaphores_[i])) ||
            !IsSuccess(vkCreateFence(device_, &fenceInfo, nullptr, &inFlightFences_[i]))) {
            throw std::runtime_error("Failed to create Vulkan synchronization objects.");
        }
    }
    syncObjectsCreated_ = true;
    currentFrame_ = 0;
}

void VulkanContext::RecordCommandBuffer(const VkCommandBuffer commandBuffer, const std::uint32_t imageIndex) {
    const VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pInheritanceInfo = nullptr
    };
    if (!IsSuccess(vkBeginCommandBuffer(commandBuffer, &beginInfo))) {
        throw std::runtime_error("Failed to begin recording Vulkan command buffer.");
    }

    if (renderer_ != nullptr) {
        renderer_->RecordShadowCommands(*this, commandBuffer, currentFrame_);
    }

    std::array<VkClearValue, 3> clearValues{};
    clearValues[0].color = clearColor_;
    clearValues[1].color = {{0.0F, 0.0F, 0.0F, 0.0F}};
    clearValues[2].depthStencil = {.depth = 0.0F, .stencil = 0};
    const VkRenderPassBeginInfo sceneRenderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = sceneRenderPass_,
        .framebuffer = sceneFramebuffers_[imageIndex],
        .renderArea = {.offset = {0, 0}, .extent = swapchainExtent_},
        .clearValueCount = static_cast<std::uint32_t>(clearValues.size()),
        .pClearValues = clearValues.data()
    };

    vkCmdBeginRenderPass(commandBuffer, &sceneRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    if (renderer_ != nullptr) {
        renderer_->RecordDrawCommands(*this, commandBuffer, currentFrame_);
    }
    vkCmdEndRenderPass(commandBuffer);

    const VkImageMemoryBarrier sceneToSampleBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = offscreenColorImages_[imageIndex],
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}
    };
    const VkImageMemoryBarrier glowToSampleBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = offscreenGlowImages_[imageIndex],
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}
    };
    const std::array barriers{sceneToSampleBarrier, glowToSampleBarrier};
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        static_cast<std::uint32_t>(barriers.size()),
        barriers.data()
    );

    if (postProcessRenderer_ != nullptr) {
        postProcessRenderer_->RecordBlurCommands(*this, commandBuffer, imageIndex);
    }

    const VkClearValue postClearValue{.color = clearColor_};
    const VkRenderPassBeginInfo postRenderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = postProcessRenderPass_,
        .framebuffer = swapchainFramebuffers_[imageIndex],
        .renderArea = {.offset = {0, 0}, .extent = swapchainExtent_},
        .clearValueCount = 1,
        .pClearValues = &postClearValue
    };
    vkCmdBeginRenderPass(commandBuffer, &postRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    if (postProcessRenderer_ != nullptr) {
        const VkViewport viewport{
            .x = 0.0F,
            .y = 0.0F,
            .width = static_cast<float>(swapchainExtent_.width),
            .height = static_cast<float>(swapchainExtent_.height),
            .minDepth = 0.0F,
            .maxDepth = 1.0F
        };
        const VkRect2D scissor{.offset = {0, 0}, .extent = swapchainExtent_};
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        postProcessRenderer_->DrawComposite(*this, commandBuffer, imageIndex, currentFrame_);
    }
    vkCmdEndRenderPass(commandBuffer);

    if (!IsSuccess(vkEndCommandBuffer(commandBuffer))) {
        throw std::runtime_error("Failed to record Vulkan command buffer.");
    }
}

void VulkanContext::RecreateSwapchain(const std::uint32_t width, const std::uint32_t height) {
    if (device_ == VK_NULL_HANDLE || surface_ == VK_NULL_HANDLE || width == 0 || height == 0) {
        return;
    }
    vkDeviceWaitIdle(device_);

    CleanupSwapchain();

    CreateSwapchain(width, height);
    CreateImageViews();
    CreateOffscreenResources();
    CreateDepthResources();
    CreateSceneRenderPass();
    CreatePostProcessRenderPass();
    CreateFramebuffers();
    CreateCommandPool();
    CreateCommandBuffers();

    if (!syncObjectsCreated_) {
        CreateSyncObjects();
    }

    if (renderer_ == nullptr) {
        renderer_ = std::make_unique<Renderer>();
        renderer_->Initialize(*this);
    } else {
        renderer_->RecreateSwapchain(*this);
    }
    if (postProcessRenderer_ == nullptr) {
        postProcessRenderer_ = std::make_unique<PostProcessRenderer>();
        postProcessRenderer_->Initialize(*this, offscreenColorImageViews_, offscreenGlowImageViews_, offscreenColorSampler_);
    } else {
        postProcessRenderer_->RecreateSwapchain(*this, offscreenColorImageViews_, offscreenGlowImageViews_, offscreenColorSampler_);
    }
    if (currentPlace_ != nullptr) {
        renderer_->BindToPlace(currentPlace_);
        postProcessRenderer_->BindToPlace(currentPlace_);
    }
    renderer_->SetOverlayPrimitives(overlayPrimitives_);
}

void VulkanContext::CleanupSwapchain() {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }

    if (renderer_ != nullptr) {
        renderer_->DestroySwapchainResources(*this);
    }
    if (postProcessRenderer_ != nullptr) {
        postProcessRenderer_->DestroySwapchainResources(*this);
    }

    if (commandPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_, commandPool_, nullptr);
        commandPool_ = VK_NULL_HANDLE;
        commandBuffers_.clear();
    }

    for (const auto framebuffer : sceneFramebuffers_) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device_, framebuffer, nullptr);
        }
    }
    sceneFramebuffers_.clear();

    for (const auto framebuffer : swapchainFramebuffers_) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device_, framebuffer, nullptr);
        }
    }
    swapchainFramebuffers_.clear();

    if (sceneRenderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, sceneRenderPass_, nullptr);
        sceneRenderPass_ = VK_NULL_HANDLE;
    }
    if (postProcessRenderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, postProcessRenderPass_, nullptr);
        postProcessRenderPass_ = VK_NULL_HANDLE;
    }

    for (const auto depthImageView : depthImageViews_) {
        if (depthImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(device_, depthImageView, nullptr);
        }
    }
    depthImageViews_.clear();
    for (const auto depthImage : depthImages_) {
        if (depthImage != VK_NULL_HANDLE) {
            vkDestroyImage(device_, depthImage, nullptr);
        }
    }
    depthImages_.clear();
    for (const auto depthMemory : depthMemories_) {
        if (depthMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device_, depthMemory, nullptr);
        }
    }
    depthMemories_.clear();
    depthFormat_ = VK_FORMAT_UNDEFINED;

    if (offscreenColorSampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device_, offscreenColorSampler_, nullptr);
        offscreenColorSampler_ = VK_NULL_HANDLE;
    }
    for (const auto offscreenView : offscreenColorImageViews_) {
        if (offscreenView != VK_NULL_HANDLE) {
            vkDestroyImageView(device_, offscreenView, nullptr);
        }
    }
    offscreenColorImageViews_.clear();
    for (const auto offscreenView : offscreenGlowImageViews_) {
        if (offscreenView != VK_NULL_HANDLE) {
            vkDestroyImageView(device_, offscreenView, nullptr);
        }
    }
    offscreenGlowImageViews_.clear();
    for (const auto offscreenImage : offscreenColorImages_) {
        if (offscreenImage != VK_NULL_HANDLE) {
            vkDestroyImage(device_, offscreenImage, nullptr);
        }
    }
    offscreenColorImages_.clear();
    for (const auto offscreenImage : offscreenGlowImages_) {
        if (offscreenImage != VK_NULL_HANDLE) {
            vkDestroyImage(device_, offscreenImage, nullptr);
        }
    }
    offscreenGlowImages_.clear();
    for (const auto offscreenMemory : offscreenColorMemories_) {
        if (offscreenMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device_, offscreenMemory, nullptr);
        }
    }
    offscreenColorMemories_.clear();
    for (const auto offscreenMemory : offscreenGlowMemories_) {
        if (offscreenMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device_, offscreenMemory, nullptr);
        }
    }
    offscreenGlowMemories_.clear();

    for (const auto imageView : swapchainImageViews_) {
        if (imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(device_, imageView, nullptr);
        }
    }
    swapchainImageViews_.clear();
    swapchainImages_.clear();

    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}

void VulkanContext::CleanupDeviceAndSurface() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
    }

    if (renderer_ != nullptr && device_ != VK_NULL_HANDLE) {
        renderer_->Shutdown(*this);
        renderer_.reset();
    }
    if (postProcessRenderer_ != nullptr && device_ != VK_NULL_HANDLE) {
        postProcessRenderer_->Shutdown(*this);
        postProcessRenderer_.reset();
    }

    CleanupSyncObjects();
    CleanupSwapchain();

    if (device_ != VK_NULL_HANDLE) {
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }
    graphicsQueue_ = VK_NULL_HANDLE;
    graphicsQueueFamily_ = 0;
    physicalDevice_ = VK_NULL_HANDLE;

    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }
}

void VulkanContext::CleanupSyncObjects() {
    if (!syncObjectsCreated_ || device_ == VK_NULL_HANDLE) {
        return;
    }

    for (std::uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (imageAvailableSemaphores_[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(device_, imageAvailableSemaphores_[i], nullptr);
            imageAvailableSemaphores_[i] = VK_NULL_HANDLE;
        }
        if (renderFinishedSemaphores_[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(device_, renderFinishedSemaphores_[i], nullptr);
            renderFinishedSemaphores_[i] = VK_NULL_HANDLE;
        }
        if (inFlightFences_[i] != VK_NULL_HANDLE) {
            vkDestroyFence(device_, inFlightFences_[i], nullptr);
            inFlightFences_[i] = VK_NULL_HANDLE;
        }
    }

    syncObjectsCreated_ = false;
}

bool VulkanContext::IsPhysicalDeviceSuitable(const VkPhysicalDevice device) const {
    if (surface_ == VK_NULL_HANDLE) {
        return false;
    }

    std::uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    bool hasSwapchain = false;
    for (const auto& ext : availableExtensions) {
        if (std::strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
            hasSwapchain = true;
            break;
        }
    }
    if (!hasSwapchain) {
        return false;
    }

    try {
        static_cast<void>(FindGraphicsPresentQueueFamily(device));
    } catch (const std::exception&) {
        return false;
    }

    const auto support = SwapchainUtils::QuerySwapchainSupport(device, surface_);
    return !support.Formats.empty() && !support.PresentModes.empty();
}

std::uint32_t VulkanContext::FindGraphicsPresentQueueFamily(const VkPhysicalDevice device) const {
    std::uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    if (queueFamilyCount == 0) {
        throw std::runtime_error("Physical device has no queue families.");
    }

    std::vector<VkQueueFamilyProperties> families(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, families.data());

    for (std::uint32_t i = 0; i < queueFamilyCount; ++i) {
        if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0U) {
            continue;
        }
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &presentSupport);
        if (presentSupport == VK_TRUE) {
            return i;
        }
    }

    throw std::runtime_error("Physical device has no graphics+present queue family.");
}

} // namespace Lvs::Engine::Rendering::Vulkan
