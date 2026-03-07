#include "Lvs/Engine/Rendering/Vulkan/VulkanContext.hpp"

#include "Lvs/Engine/Rendering/Vulkan/VulkanGpuResources.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanFrameManager.hpp"
#include "Lvs/Engine/Rendering/Vulkan/PostProcessRenderer.hpp"
#include "Lvs/Engine/Rendering/Vulkan/Renderer.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanBufferUtils.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanSwapchainUtils.hpp"

#include <array>
#include <algorithm>
#include <cstring>
#include <string>
#include <stdexcept>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Lvs::Engine::Rendering::Vulkan {

namespace {

[[nodiscard]] std::string ApiVersionToString(const std::uint32_t version) {
    return std::to_string(VK_API_VERSION_MAJOR(version)) + "." +
        std::to_string(VK_API_VERSION_MINOR(version)) + "." +
        std::to_string(VK_API_VERSION_PATCH(version));
}

[[nodiscard]] const char* VkResultToString(const VkResult result) {
    switch (result) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        default: return "VK_RESULT_UNKNOWN";
    }
}

[[nodiscard]] std::uint32_t QueryLoaderApiVersion() {
    const auto enumerateInstanceVersion =
        reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
            vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion")
        );

    if (enumerateInstanceVersion == nullptr) {
        return VK_API_VERSION_1_0;
    }

    std::uint32_t loaderVersion = VK_API_VERSION_1_0;
    if (enumerateInstanceVersion(&loaderVersion) != VK_SUCCESS) {
        return VK_API_VERSION_1_0;
    }

    return loaderVersion;
}

[[nodiscard]] bool HasRequiredInstanceExtensions(const std::vector<const char*>& requiredExtensions) {
    std::uint32_t extensionCount = 0;
    if (vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr) != VK_SUCCESS) {
        return false;
    }

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    if (extensionCount > 0 &&
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data()) != VK_SUCCESS) {
        return false;
    }

    for (const char* required : requiredExtensions) {
        const auto it = std::find_if(
            availableExtensions.begin(),
            availableExtensions.end(),
            [required](const VkExtensionProperties& extension) {
                return std::strcmp(extension.extensionName, required) == 0;
            }
        );
        if (it == availableExtensions.end()) {
            return false;
        }
    }

    return true;
}

[[nodiscard]] bool IsSuccess(const VkResult result) {
    return result == VK_SUCCESS;
}

VkBufferUsageFlags ToVkBufferUsage(const Rendering::Common::BufferUsage usage) {
    VkBufferUsageFlags flags = 0;
    if (Rendering::Common::HasFlag(usage, Rendering::Common::BufferUsage::Vertex)) {
        flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }
    if (Rendering::Common::HasFlag(usage, Rendering::Common::BufferUsage::Index)) {
        flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    }
    if (Rendering::Common::HasFlag(usage, Rendering::Common::BufferUsage::Uniform)) {
        flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }
    if (Rendering::Common::HasFlag(usage, Rendering::Common::BufferUsage::TransferSource)) {
        flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    }
    if (Rendering::Common::HasFlag(usage, Rendering::Common::BufferUsage::TransferDestination)) {
        flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }
    return flags;
}

VkMemoryPropertyFlags ToVkMemoryProperties(const Rendering::Common::MemoryUsage usage) {
    switch (usage) {
        case Rendering::Common::MemoryUsage::CpuVisible:
            return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        case Rendering::Common::MemoryUsage::DeviceLocal:
        default:
            return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }
}

VkFormat ToVkFormat(const Rendering::Common::PixelFormat format) {
    switch (format) {
        case Rendering::Common::PixelFormat::RGBA8Unorm: return VK_FORMAT_R8G8B8A8_UNORM;
        case Rendering::Common::PixelFormat::RGBA8Srgb: return VK_FORMAT_R8G8B8A8_SRGB;
        case Rendering::Common::PixelFormat::RGBA16Float: return VK_FORMAT_R16G16B16A16_SFLOAT;
        case Rendering::Common::PixelFormat::RGBA32Float: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case Rendering::Common::PixelFormat::D32Float: return VK_FORMAT_D32_SFLOAT;
        case Rendering::Common::PixelFormat::D32FloatS8: return VK_FORMAT_D32_SFLOAT_S8_UINT;
        case Rendering::Common::PixelFormat::D24UnormS8: return VK_FORMAT_D24_UNORM_S8_UINT;
        case Rendering::Common::PixelFormat::Unknown:
        default:
            return VK_FORMAT_UNDEFINED;
    }
}

VkImageUsageFlags ToVkImageUsage(const Rendering::Common::ImageUsage usage) {
    VkImageUsageFlags flags = 0;
    if (Rendering::Common::HasFlag(usage, Rendering::Common::ImageUsage::Sampled)) {
        flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }
    if (Rendering::Common::HasFlag(usage, Rendering::Common::ImageUsage::ColorAttachment)) {
        flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }
    if (Rendering::Common::HasFlag(usage, Rendering::Common::ImageUsage::DepthStencilAttachment)) {
        flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
    if (Rendering::Common::HasFlag(usage, Rendering::Common::ImageUsage::TransferSource)) {
        flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }
    if (Rendering::Common::HasFlag(usage, Rendering::Common::ImageUsage::TransferDestination)) {
        flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    return flags;
}

VkFilter ToVkFilter(const Rendering::Common::FilterMode filter) {
    return filter == Rendering::Common::FilterMode::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
}

VkSamplerAddressMode ToVkAddressMode(const Rendering::Common::AddressMode mode) {
    return mode == Rendering::Common::AddressMode::Repeat ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
}

VkSamplerMipmapMode ToVkMipmapMode(const Rendering::Common::MipmapMode mode) {
    return mode == Rendering::Common::MipmapMode::Nearest ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;
}

} // namespace

VulkanInitializationError::VulkanInitializationError(const Reason reason, std::string message)
    : std::runtime_error(std::move(message)),
      reason_(reason) {
}

VulkanInitializationError::Reason VulkanInitializationError::GetReason() const noexcept {
    return reason_;
}

VulkanContext::~VulkanContext() {
    Shutdown();
}

VulkanContext::VulkanContext()
    : frameManager_(std::make_unique<VulkanFrameManager>()) {
}

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

    if (nativeWindowHandle_ == nativeWindowHandle && frameManager_ != nullptr && frameManager_->HasSwapchain()) {
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
    if (device_ == VK_NULL_HANDLE || frameManager_ == nullptr || !frameManager_->HasSwapchain()) {
        return;
    }

    bool needsRecreate = false;
    const auto frameState = frameManager_->BeginFrame(*this, needsRecreate);
    if (needsRecreate) {
        RecreateSwapchain(lastWidth_, lastHeight_);
        return;
    }
    if (frameState.CommandBuffer == VK_NULL_HANDLE) {
        return;
    }

    RecordCommandBuffer(frameState.CommandBuffer, frameState.ImageIndex, frameState.FrameIndex);
    if (!frameManager_->EndFrame(*this, frameState)) {
        RecreateSwapchain(lastWidth_, lastHeight_);
    }
}

void VulkanContext::SetClearColor(const float r, const float g, const float b, const float a) {
    clearColor_ = {{r, g, b, a}};
}

void VulkanContext::SetOverlayPrimitives(std::vector<Rendering::Common::OverlayPrimitive> primitives) {
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
    return frameManager_ != nullptr ? frameManager_->GetSceneRenderPass() : VK_NULL_HANDLE;
}

VkRenderPass VulkanContext::GetPostProcessRenderPass() const {
    return frameManager_ != nullptr ? frameManager_->GetPostProcessRenderPass() : VK_NULL_HANDLE;
}

VkFormat VulkanContext::GetSwapchainImageFormat() const {
    return frameManager_ != nullptr ? frameManager_->GetSwapchainImageFormat() : VK_FORMAT_UNDEFINED;
}

VkFormat VulkanContext::GetOffscreenImageFormat() const {
    return frameManager_ != nullptr ? frameManager_->GetOffscreenImageFormat() : VK_FORMAT_UNDEFINED;
}

VkExtent2D VulkanContext::GetSwapchainExtent() const {
    return frameManager_ != nullptr ? frameManager_->GetSwapchainExtentVk() : VkExtent2D{};
}

std::uint32_t VulkanContext::GetFramesInFlight() const {
    return frameManager_ != nullptr ? frameManager_->GetFramesInFlight() : 0;
}

std::unique_ptr<Rendering::Common::BufferResource> VulkanContext::CreateBuffer(const Rendering::Common::BufferDesc& desc) {
    const auto handle = BufferUtils::CreateBuffer(
        physicalDevice_,
        device_,
        static_cast<VkDeviceSize>(desc.Size),
        ToVkBufferUsage(desc.Usage),
        ToVkMemoryProperties(desc.Memory)
    );
    return std::make_unique<VulkanBufferResource>(device_, handle.Buffer, handle.Memory, desc.Size);
}

std::unique_ptr<Rendering::Common::ImageResource> VulkanContext::CreateImage(const Rendering::Common::ImageDesc& desc) {
    const VkFormat format = ToVkFormat(desc.Format);
    const VkImageCreateFlags flags = desc.CubeCompatible ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
    const VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = flags,
        .imageType = desc.Depth > 1 ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {.width = desc.Width, .height = desc.Height, .depth = desc.Depth},
        .mipLevels = desc.MipLevels,
        .arrayLayers = desc.Layers,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = ToVkImageUsage(desc.Usage),
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VkImage image = VK_NULL_HANDLE;
    if (vkCreateImage(device_, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan image.");
    }

    VkMemoryRequirements memRequirements{};
    vkGetImageMemoryRequirements(device_, image, &memRequirements);
    const VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = BufferUtils::FindMemoryType(
            physicalDevice_,
            memRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        )
    };

    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (vkAllocateMemory(device_, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyImage(device_, image, nullptr);
        throw std::runtime_error("Failed to allocate Vulkan image memory.");
    }
    vkBindImageMemory(device_, image, memory, 0);
    return std::make_unique<VulkanImageResource>(device_, image, memory, desc.Width, desc.Height);
}

std::unique_ptr<Rendering::Common::SamplerResource> VulkanContext::CreateSampler(
    const Rendering::Common::SamplerDesc& desc
) {
    const VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .magFilter = ToVkFilter(desc.Filter),
        .minFilter = ToVkFilter(desc.Filter),
        .mipmapMode = ToVkMipmapMode(desc.Mipmap),
        .addressModeU = ToVkAddressMode(desc.Address),
        .addressModeV = ToVkAddressMode(desc.Address),
        .addressModeW = ToVkAddressMode(desc.Address),
        .mipLodBias = 0.0F,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0F,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0F,
        .maxLod = 0.0F,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE
    };

    VkSampler sampler = VK_NULL_HANDLE;
    if (vkCreateSampler(device_, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan sampler.");
    }
    return std::make_unique<VulkanSamplerResource>(device_, sampler);
}

void VulkanContext::CreateInstance() {
    std::vector<const char*> extensions{VK_KHR_SURFACE_EXTENSION_NAME};
#ifdef _WIN32
    extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif

    if (!HasRequiredInstanceExtensions(extensions)) {
        throw VulkanInitializationError(
            VulkanInitializationError::Reason::UnsupportedApi,
            "Vulkan runtime is missing required instance extensions for window surfaces."
        );
    }

    const std::uint32_t loaderVersion = QueryLoaderApiVersion();
    const std::array<std::uint32_t, 4> preferredApiVersions{
        VK_API_VERSION_1_3,
        VK_API_VERSION_1_2,
        VK_API_VERSION_1_1,
        VK_API_VERSION_1_0
    };

    VkResult lastError = VK_SUCCESS;
    bool attempted = false;
    for (const std::uint32_t apiVersion : preferredApiVersions) {
        if (apiVersion > loaderVersion) {
            continue;
        }

        attempted = true;
        const VkApplicationInfo appInfo{
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext = nullptr,
            .pApplicationName = "Lvs Engine",
            .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
            .pEngineName = "Lvs Engine",
            .engineVersion = VK_MAKE_VERSION(0, 1, 0),
            .apiVersion = apiVersion
        };

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

        lastError = vkCreateInstance(&instanceInfo, nullptr, &instance_);
        if (lastError == VK_SUCCESS) {
            instanceApiVersion_ = apiVersion;
            return;
        }
    }

    if (!attempted) {
        throw VulkanInitializationError(
            VulkanInitializationError::Reason::UnsupportedApi,
            "No supported Vulkan API version was reported by the Vulkan loader."
        );
    }

    throw VulkanInitializationError(
        VulkanInitializationError::Reason::UnsupportedApi,
        "Failed to create Vulkan instance with fallback API versions (1.3 -> 1.0). "
        "Loader max version: " + ApiVersionToString(loaderVersion) +
        ". Last error: " + std::string(VkResultToString(lastError)) + "."
    );
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
        throw VulkanInitializationError(
            VulkanInitializationError::Reason::NoPhysicalDevices,
            "No Vulkan physical devices were found on this system."
        );
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

    throw VulkanInitializationError(
        VulkanInitializationError::Reason::NoSuitableDevice,
        "No suitable Vulkan physical device found. A graphics+present queue and swapchain support are required."
    );
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

void VulkanContext::RecordCommandBuffer(
    const VkCommandBuffer commandBuffer,
    const std::uint32_t imageIndex,
    const std::uint32_t frameIndex
) {
    const VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pInheritanceInfo = nullptr
    };
    if (!IsSuccess(vkBeginCommandBuffer(commandBuffer, &beginInfo))) {
        throw std::runtime_error("Failed to begin recording Vulkan command buffer.");
    }

    VulkanRenderCommandBuffer renderCommandBuffer(commandBuffer);
    if (renderer_ != nullptr) {
        renderer_->RecordShadowCommands(*this, *frameManager_, renderCommandBuffer, frameIndex);
    }

    std::array<VkClearValue, 3> clearValues{};
    clearValues[0].color = clearColor_;
    clearValues[1].color = {{0.0F, 0.0F, 0.0F, 0.0F}};
    clearValues[2].depthStencil = {.depth = 0.0F, .stencil = 0};
    const VkRenderPassBeginInfo sceneRenderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = frameManager_->GetSceneRenderPass(),
        .framebuffer = frameManager_->GetSceneFramebuffer(imageIndex),
        .renderArea = {.offset = {0, 0}, .extent = frameManager_->GetSwapchainExtentVk()},
        .clearValueCount = static_cast<std::uint32_t>(clearValues.size()),
        .pClearValues = clearValues.data()
    };

    vkCmdBeginRenderPass(commandBuffer, &sceneRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    if (renderer_ != nullptr) {
        renderer_->RecordDrawCommands(*this, *frameManager_, renderCommandBuffer, frameIndex);
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
        .image = frameManager_->GetOffscreenColorImage(imageIndex),
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
        .image = frameManager_->GetOffscreenGlowImage(imageIndex),
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
        .renderPass = frameManager_->GetPostProcessRenderPass(),
        .framebuffer = frameManager_->GetSwapchainFramebuffer(imageIndex),
        .renderArea = {.offset = {0, 0}, .extent = frameManager_->GetSwapchainExtentVk()},
        .clearValueCount = 1,
        .pClearValues = &postClearValue
    };
    vkCmdBeginRenderPass(commandBuffer, &postRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    if (postProcessRenderer_ != nullptr) {
        const VkExtent2D extent = frameManager_->GetSwapchainExtentVk();
        const VkViewport viewport{
            .x = 0.0F,
            .y = 0.0F,
            .width = static_cast<float>(extent.width),
            .height = static_cast<float>(extent.height),
            .minDepth = 0.0F,
            .maxDepth = 1.0F
        };
        const VkRect2D scissor{.offset = {0, 0}, .extent = extent};
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        postProcessRenderer_->DrawComposite(*this, commandBuffer, imageIndex, frameIndex);
    }
    vkCmdEndRenderPass(commandBuffer);

    if (!IsSuccess(vkEndCommandBuffer(commandBuffer))) {
        throw std::runtime_error("Failed to record Vulkan command buffer.");
    }
}

void VulkanContext::RecreateSwapchain(const std::uint32_t width, const std::uint32_t height) {
    if (device_ == VK_NULL_HANDLE || surface_ == VK_NULL_HANDLE || width == 0 || height == 0 || frameManager_ == nullptr) {
        return;
    }

    if (renderer_ != nullptr) {
        renderer_->DestroySwapchainResources(*this, *frameManager_);
    }
    if (postProcessRenderer_ != nullptr) {
        postProcessRenderer_->DestroySwapchainResources(*this);
    }

    frameManager_->Recreate(*this, surface_, width, height);

    if (renderer_ == nullptr) {
        renderer_ = std::make_unique<Renderer>();
        renderer_->Initialize(*this, *frameManager_);
    } else {
        renderer_->RecreateSwapchain(*this, *frameManager_);
    }
    if (postProcessRenderer_ == nullptr) {
        postProcessRenderer_ = std::make_unique<PostProcessRenderer>();
        postProcessRenderer_->Initialize(*this, frameManager_->GetOffscreenColorImageViews(), frameManager_->GetOffscreenGlowImageViews(), frameManager_->GetOffscreenColorSampler());
    } else {
        postProcessRenderer_->RecreateSwapchain(*this, frameManager_->GetOffscreenColorImageViews(), frameManager_->GetOffscreenGlowImageViews(), frameManager_->GetOffscreenColorSampler());
    }
    if (currentPlace_ != nullptr) {
        renderer_->BindToPlace(currentPlace_);
        postProcessRenderer_->BindToPlace(currentPlace_);
    }
    renderer_->SetOverlayPrimitives(overlayPrimitives_);
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
    if (frameManager_ != nullptr && device_ != VK_NULL_HANDLE) {
        frameManager_->Cleanup(*this);
    }

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

