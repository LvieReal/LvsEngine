#include "Lvs/Engine/Rendering/Backends/Vulkan/VulkanContext.hpp"

#include "Lvs/Engine/Rendering/Backends/Vulkan/VulkanCommandBuffer.hpp"
#include "Lvs/Engine/Rendering/Backends/Vulkan/VulkanPipeline.hpp"
#include "Lvs/Engine/Rendering/Backends/Vulkan/Utils/VulkanRhiObjects.hpp"
#include "Lvs/Engine/Rendering/Backends/Vulkan/Utils/VulkanRenderUtils.hpp"
#include "Lvs/Engine/Rendering/Common/SceneUniformData.hpp"
#include "Lvs/Engine/Rendering/ShaderLoader.hpp"

#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

#ifdef _WIN32
#include <vulkan/vulkan_win32.h>
#include <windows.h>
#endif

namespace Lvs::Engine::Rendering::Backends::Vulkan {

// Utility helpers live in Backends/Vulkan/Utils.

VulkanContext::VulkanContext(VulkanApi api)
    : api_(api) {}

VulkanContext::~VulkanContext() {
    if (api_.Device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(api_.Device);
    }

    // Pipelines/command buffers must be released while device/pool are still valid.
    renderer_.reset();
    cmdBuffer_.reset();

    if (api_.Device != VK_NULL_HANDLE) {
        for (auto& [view, texture] : owned2DTextures_) {
            static_cast<void>(view);
            if (texture.Sampler != VK_NULL_HANDLE) {
                vkDestroySampler(api_.Device, texture.Sampler, nullptr);
            }
            if (texture.View != VK_NULL_HANDLE) {
                vkDestroyImageView(api_.Device, texture.View, nullptr);
            }
            if (texture.Image != VK_NULL_HANDLE) {
                vkDestroyImage(api_.Device, texture.Image, nullptr);
            }
            if (texture.Memory != VK_NULL_HANDLE) {
                vkFreeMemory(api_.Device, texture.Memory, nullptr);
            }
        }
        owned2DTextures_.clear();

        for (auto& [view, texture] : owned3DTextures_) {
            static_cast<void>(view);
            if (texture.Sampler != VK_NULL_HANDLE) {
                vkDestroySampler(api_.Device, texture.Sampler, nullptr);
            }
            if (texture.View != VK_NULL_HANDLE) {
                vkDestroyImageView(api_.Device, texture.View, nullptr);
            }
            if (texture.Image != VK_NULL_HANDLE) {
                vkDestroyImage(api_.Device, texture.Image, nullptr);
            }
            if (texture.Memory != VK_NULL_HANDLE) {
                vkFreeMemory(api_.Device, texture.Memory, nullptr);
            }
        }
        owned3DTextures_.clear();

        for (auto& [view, texture] : ownedCubeTextures_) {
            static_cast<void>(view);
            if (texture.Sampler != VK_NULL_HANDLE) {
                vkDestroySampler(api_.Device, texture.Sampler, nullptr);
            }
            if (texture.View != VK_NULL_HANDLE) {
                vkDestroyImageView(api_.Device, texture.View, nullptr);
            }
            if (texture.Image != VK_NULL_HANDLE) {
                vkDestroyImage(api_.Device, texture.Image, nullptr);
            }
            if (texture.Memory != VK_NULL_HANDLE) {
                vkFreeMemory(api_.Device, texture.Memory, nullptr);
            }
        }
        ownedCubeTextures_.clear();

        if (imageAvailableSemaphore_ != VK_NULL_HANDLE) {
            vkDestroySemaphore(api_.Device, imageAvailableSemaphore_, nullptr);
            imageAvailableSemaphore_ = VK_NULL_HANDLE;
        }
        for (const auto semaphore : renderFinishedSemaphores_) {
            if (semaphore != VK_NULL_HANDLE) {
                vkDestroySemaphore(api_.Device, semaphore, nullptr);
            }
        }
        renderFinishedSemaphores_.clear();
        if (inFlightFence_ != VK_NULL_HANDLE) {
            vkDestroyFence(api_.Device, inFlightFence_, nullptr);
            inFlightFence_ = VK_NULL_HANDLE;
        }

        DestroySwapchain();
        DestroyDepthAttachment();
        if (api_.RenderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(api_.Device, api_.RenderPass, nullptr);
            api_.RenderPass = VK_NULL_HANDLE;
        }
        if (api_.PipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(api_.Device, api_.PipelineLayout, nullptr);
            api_.PipelineLayout = VK_NULL_HANDLE;
        }
        if (api_.DescriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(api_.Device, api_.DescriptorPool, nullptr);
            api_.DescriptorPool = VK_NULL_HANDLE;
        }
        if (api_.DescriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(api_.Device, api_.DescriptorSetLayout, nullptr);
            api_.DescriptorSetLayout = VK_NULL_HANDLE;
        }
        if (ownsCommandPool_ && api_.CommandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(api_.Device, api_.CommandPool, nullptr);
            api_.CommandPool = VK_NULL_HANDLE;
        }
        if (ownsDevice_) {
            vkDestroyDevice(api_.Device, nullptr);
            api_.Device = VK_NULL_HANDLE;
            api_.GraphicsQueue = VK_NULL_HANDLE;
            api_.PresentQueue = VK_NULL_HANDLE;
        }
    }
    if (ownsInstance_ && api_.Surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(api_.Instance, api_.Surface, nullptr);
        api_.Surface = VK_NULL_HANDLE;
    }
    if (ownsInstance_ && debugMessenger_ != VK_NULL_HANDLE) {
        Utils::DestroyDebugUtilsMessenger(api_.Instance, debugMessenger_);
        debugMessenger_ = VK_NULL_HANDLE;
    }
    if (ownsInstance_ && api_.Instance != VK_NULL_HANDLE) {
        vkDestroyInstance(api_.Instance, nullptr);
        api_.Instance = VK_NULL_HANDLE;
    }
}

void VulkanContext::WaitIdle() {
    if (api_.Device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(api_.Device);
    }
}

void VulkanContext::EnsureApiBootstrap() {
    if (api_.Instance == VK_NULL_HANDLE) {
#ifdef _WIN32
        std::vector<const char*> requiredExtensions = {
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME
        };
#else
        std::vector<const char*> requiredExtensions = {VK_KHR_SURFACE_EXTENSION_NAME};
#endif

#if !defined(NDEBUG)
        std::vector<const char*> requiredLayers;
        if (Utils::HasInstanceLayer(Utils::kValidationLayerName)) {
            requiredLayers.push_back(Utils::kValidationLayerName);
            validationLayersEnabled_ = true;
            if (Utils::HasInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
                requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            }
        } else {
            validationLayersEnabled_ = false;
        }
#endif
        api_.NegotiatedApiVersion = Utils::SelectVulkanApiVersion();
        const VkApplicationInfo appInfo{
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext = nullptr,
            .pApplicationName = "Lvs",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName = "LvsEngine",
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = api_.NegotiatedApiVersion
        };
        const VkInstanceCreateInfo instanceInfo{
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .pApplicationInfo = &appInfo,
#if !defined(NDEBUG)
            .enabledLayerCount = static_cast<std::uint32_t>(requiredLayers.size()),
            .ppEnabledLayerNames = requiredLayers.empty() ? nullptr : requiredLayers.data(),
#else
            .enabledLayerCount = 0,
            .ppEnabledLayerNames = nullptr,
#endif
            .enabledExtensionCount = static_cast<std::uint32_t>(requiredExtensions.size()),
            .ppEnabledExtensionNames = requiredExtensions.data()
        };
        if (vkCreateInstance(&instanceInfo, nullptr, &api_.Instance) == VK_SUCCESS) {
            ownsInstance_ = true;
        }

#if !defined(NDEBUG)
        if (ownsInstance_ && validationLayersEnabled_ && debugMessenger_ == VK_NULL_HANDLE) {
            const VkDebugUtilsMessengerCreateInfoEXT messengerInfo{
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                .pNext = nullptr,
                .flags = 0,
                .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                .pfnUserCallback = Utils::DebugMessageCallback,
                .pUserData = nullptr
            };
            if (Utils::CreateDebugUtilsMessenger(api_.Instance, &messengerInfo, &debugMessenger_) != VK_SUCCESS) {
                debugMessenger_ = VK_NULL_HANDLE;
            }
        }
#endif
    }

    if (api_.Instance == VK_NULL_HANDLE) {
        throw VulkanInitializationError("Unable to initialize Vulkan instance");
    }

    CreateSurfaceIfNeeded();
    if (api_.NativeWindowHandle != nullptr && api_.Surface == VK_NULL_HANDLE) {
        throw VulkanInitializationError("Unable to create Vulkan surface for native window handle");
    }

    if (api_.PhysicalDevice == VK_NULL_HANDLE) {
        std::uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(api_.Instance, &deviceCount, nullptr);
        if (deviceCount > 0) {
            std::vector<VkPhysicalDevice> devices(deviceCount);
            vkEnumeratePhysicalDevices(api_.Instance, &deviceCount, devices.data());
            for (const auto device : devices) {
                std::uint32_t extensionCount = 0;
                vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
                std::vector<VkExtensionProperties> extensions(extensionCount);
                if (extensionCount > 0) {
                    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());
                }
                bool hasSwapchainExtension = false;
                for (const auto& extension : extensions) {
                    if (std::strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                        hasSwapchainExtension = true;
                        break;
                    }
                }
                if (!hasSwapchainExtension) {
                    continue;
                }

                std::uint32_t queueFamilyCount = 0;
                vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
                std::vector<VkQueueFamilyProperties> queues(queueFamilyCount);
                vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queues.data());

                std::optional<std::uint32_t> candidateGraphicsFamily;
                std::optional<std::uint32_t> candidatePresentFamily;
                for (std::uint32_t i = 0; i < queueFamilyCount; ++i) {
                    const bool supportsGraphics = (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U;
                    VkBool32 supportsPresent = VK_TRUE;
                    if (api_.Surface != VK_NULL_HANDLE) {
                        supportsPresent = VK_FALSE;
                        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, api_.Surface, &supportsPresent);
                    }
                    if (supportsGraphics && !candidateGraphicsFamily.has_value()) {
                        candidateGraphicsFamily = i;
                    }
                    if (supportsPresent && !candidatePresentFamily.has_value()) {
                        candidatePresentFamily = i;
                    }
                    if (supportsGraphics && supportsPresent) {
                        candidateGraphicsFamily = i;
                        candidatePresentFamily = i;
                        break;
                    }
                }

                if (candidateGraphicsFamily.has_value() && candidatePresentFamily.has_value()) {
                    api_.PhysicalDevice = device;
                    graphicsQueueFamily_ = candidateGraphicsFamily;
                    presentQueueFamily_ = candidatePresentFamily;
                    break;
                }
            }
        }
    }
    if (api_.PhysicalDevice == VK_NULL_HANDLE) {
        throw VulkanInitializationError("No Vulkan physical device found");
    }

    if (!graphicsQueueFamily_.has_value()) {
        std::uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(api_.PhysicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queues(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(api_.PhysicalDevice, &queueFamilyCount, queues.data());
        for (std::uint32_t i = 0; i < queueFamilyCount; ++i) {
            const bool supportsGraphics = (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U;
            VkBool32 supportsPresent = VK_FALSE;
            if (api_.Surface != VK_NULL_HANDLE) {
                vkGetPhysicalDeviceSurfaceSupportKHR(api_.PhysicalDevice, i, api_.Surface, &supportsPresent);
            }
            if (supportsGraphics && !graphicsQueueFamily_.has_value()) {
                graphicsQueueFamily_ = i;
            }
            if (supportsPresent && !presentQueueFamily_.has_value()) {
                presentQueueFamily_ = i;
            }
            if (supportsGraphics && supportsPresent) {
                graphicsQueueFamily_ = i;
                presentQueueFamily_ = i;
                break;
            }
        }
    }
    if (!graphicsQueueFamily_.has_value()) {
        throw VulkanInitializationError("No Vulkan graphics queue family found");
    }
    if (!presentQueueFamily_.has_value()) {
        presentQueueFamily_ = graphicsQueueFamily_;
    }

    if (api_.Device == VK_NULL_HANDLE) {
        VkPhysicalDeviceFeatures supportedFeatures{};
        vkGetPhysicalDeviceFeatures(api_.PhysicalDevice, &supportedFeatures);
        VkPhysicalDeviceFeatures enabledFeatures{};
        enabledFeatures.depthClamp = supportedFeatures.depthClamp;
        depthClampEnabled_ = enabledFeatures.depthClamp == VK_TRUE;

        const float queuePriority = 1.0F;
        std::vector<VkDeviceQueueCreateInfo> queueInfos;
        queueInfos.push_back(VkDeviceQueueCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = graphicsQueueFamily_.value(),
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        });
        if (presentQueueFamily_.value() != graphicsQueueFamily_.value()) {
            queueInfos.push_back(VkDeviceQueueCreateInfo{
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .queueFamilyIndex = presentQueueFamily_.value(),
                .queueCount = 1,
                .pQueuePriorities = &queuePriority
            });
        }
        const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        const VkDeviceCreateInfo deviceInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueCreateInfoCount = static_cast<std::uint32_t>(queueInfos.size()),
            .pQueueCreateInfos = queueInfos.data(),
            .enabledLayerCount = 0,
            .ppEnabledLayerNames = nullptr,
            .enabledExtensionCount = static_cast<std::uint32_t>(std::size(deviceExtensions)),
            .ppEnabledExtensionNames = deviceExtensions,
            .pEnabledFeatures = &enabledFeatures
        };
        if (vkCreateDevice(api_.PhysicalDevice, &deviceInfo, nullptr, &api_.Device) == VK_SUCCESS) {
            ownsDevice_ = true;
        }
    }
    if (api_.Device == VK_NULL_HANDLE) {
        throw VulkanInitializationError("Unable to initialize Vulkan logical device");
    }

    if (api_.GraphicsQueue == VK_NULL_HANDLE) {
        vkGetDeviceQueue(api_.Device, graphicsQueueFamily_.value(), 0, &api_.GraphicsQueue);
    }
    if (api_.PresentQueue == VK_NULL_HANDLE) {
        vkGetDeviceQueue(api_.Device, presentQueueFamily_.value(), 0, &api_.PresentQueue);
    }

    if (api_.CommandPool == VK_NULL_HANDLE) {
        const VkCommandPoolCreateInfo poolInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = graphicsQueueFamily_.value()
        };
        if (vkCreateCommandPool(api_.Device, &poolInfo, nullptr, &api_.CommandPool) == VK_SUCCESS) {
            ownsCommandPool_ = true;
        }
    }

    if (api_.CommandPool == VK_NULL_HANDLE) {
        throw VulkanInitializationError("Unable to initialize Vulkan command pool");
    }
}

void VulkanContext::CreateSurfaceIfNeeded() {
    if (api_.Surface != VK_NULL_HANDLE || api_.Instance == VK_NULL_HANDLE || api_.NativeWindowHandle == nullptr) {
        return;
    }
#ifdef _WIN32
    const VkWin32SurfaceCreateInfoKHR surfaceInfo{
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .hinstance = GetModuleHandleW(nullptr),
        .hwnd = reinterpret_cast<HWND>(api_.NativeWindowHandle)
    };
    if (vkCreateWin32SurfaceKHR(api_.Instance, &surfaceInfo, nullptr, &api_.Surface) != VK_SUCCESS) {
        api_.Surface = VK_NULL_HANDLE;
    }
#endif
}

void VulkanContext::DestroySwapchain() {
    if (api_.Device == VK_NULL_HANDLE) {
        return;
    }
    vkDeviceWaitIdle(api_.Device);
    for (const auto framebuffer : api_.SwapchainFramebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(api_.Device, framebuffer, nullptr);
        }
    }
    api_.SwapchainFramebuffers.clear();

    for (const auto imageView : api_.SwapchainImageViews) {
        if (imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(api_.Device, imageView, nullptr);
        }
    }
    api_.SwapchainImageViews.clear();
    api_.SwapchainImages.clear();

    if (api_.Swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(api_.Device, api_.Swapchain, nullptr);
        api_.Swapchain = VK_NULL_HANDLE;
    }
}

bool VulkanContext::RecreateSurfaceAndSwapchain() {
    if (api_.Device == VK_NULL_HANDLE || api_.Instance == VK_NULL_HANDLE) {
        return false;
    }

    vkDeviceWaitIdle(api_.Device);
    DestroySwapchain();
    DestroyDepthAttachment();

    if (api_.Surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(api_.Instance, api_.Surface, nullptr);
        api_.Surface = VK_NULL_HANDLE;
    }

    CreateSurfaceIfNeeded();
    if (api_.Surface == VK_NULL_HANDLE) {
        return false;
    }

    if (api_.Swapchain == VK_NULL_HANDLE || api_.SwapchainImages.empty()) {
        RecreateSwapchain();
    }
    RecreateDepthAttachment();
    RecreateFramebuffer();
    RecreateRenderFinishedSemaphores();
    if (api_.Swapchain == VK_NULL_HANDLE) {
        return false;
    }
    if (api_.SwapchainFramebuffers.empty()) {
        return false;
    }
    return true;
}

void VulkanContext::RecreateSwapchain() {
    if (api_.Device == VK_NULL_HANDLE || api_.PhysicalDevice == VK_NULL_HANDLE || api_.Surface == VK_NULL_HANDLE) {
        return;
    }

    VkSurfaceCapabilitiesKHR capabilities{};
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(api_.PhysicalDevice, api_.Surface, &capabilities) != VK_SUCCESS) {
        return;
    }

    std::uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(api_.PhysicalDevice, api_.Surface, &formatCount, nullptr);
    if (formatCount == 0) {
        return;
    }
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(api_.PhysicalDevice, api_.Surface, &formatCount, formats.data());
    auto chosenFormat = formats.front();
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosenFormat = format;
            break;
        }
    }
    api_.ColorFormat = chosenFormat.format;

    std::uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(api_.PhysicalDevice, api_.Surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    if (presentModeCount > 0) {
        vkGetPhysicalDeviceSurfacePresentModesKHR(api_.PhysicalDevice, api_.Surface, &presentModeCount, presentModes.data());
    }
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (const auto mode : presentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
        // if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            presentMode = mode;
            break;
        }
    }

    VkExtent2D extent = api_.SurfaceExtent;
    if (capabilities.currentExtent.width != UINT32_MAX) {
        extent = capabilities.currentExtent;
    } else {
        if (extent.width == 0U || extent.height == 0U) {
            // Native viewport can transiently report 0x0 during API switch/window recreation.
            extent = capabilities.minImageExtent;
        }
        extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }
    if (extent.width == 0U || extent.height == 0U) {
        return;
    }
    api_.SurfaceExtent = extent;

    std::uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    DestroySwapchain();

    const std::uint32_t queueFamilyIndices[] = {graphicsQueueFamily_.value(), presentQueueFamily_.value()};
    const bool separateQueues = graphicsQueueFamily_.value() != presentQueueFamily_.value();
    const VkSwapchainCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .surface = api_.Surface,
        .minImageCount = imageCount,
        .imageFormat = chosenFormat.format,
        .imageColorSpace = chosenFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = separateQueues ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = separateQueues ? 2U : 0U,
        .pQueueFamilyIndices = separateQueues ? queueFamilyIndices : nullptr,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE
    };
    const VkResult createSwapchainResult = vkCreateSwapchainKHR(api_.Device, &createInfo, nullptr, &api_.Swapchain);
    if (createSwapchainResult != VK_SUCCESS) {
        api_.Swapchain = VK_NULL_HANDLE;
        return;
    }

    std::uint32_t swapchainImageCount = 0;
    vkGetSwapchainImagesKHR(api_.Device, api_.Swapchain, &swapchainImageCount, nullptr);
    api_.SwapchainImages.resize(swapchainImageCount);
    vkGetSwapchainImagesKHR(api_.Device, api_.Swapchain, &swapchainImageCount, api_.SwapchainImages.data());

    api_.SwapchainImageViews.reserve(api_.SwapchainImages.size());
    for (const auto image : api_.SwapchainImages) {
        const VkImageViewCreateInfo viewInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = api_.ColorFormat,
            .components = {
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY},
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        };
        VkImageView imageView = VK_NULL_HANDLE;
        if (vkCreateImageView(api_.Device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            DestroySwapchain();
            return;
        }
        api_.SwapchainImageViews.push_back(imageView);
    }

    InitializeSwapchainImageLayouts();
}

void VulkanContext::InitializeSwapchainImageLayouts() {
    if (api_.Device == VK_NULL_HANDLE || api_.CommandPool == VK_NULL_HANDLE || api_.GraphicsQueue == VK_NULL_HANDLE ||
        api_.SwapchainImages.empty()) {
        return;
    }

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    const VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = api_.CommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    if (vkAllocateCommandBuffers(api_.Device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        return;
    }

    const VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        vkFreeCommandBuffers(api_.Device, api_.CommandPool, 1, &commandBuffer);
        return;
    }

    std::vector<VkImageMemoryBarrier> barriers;
    barriers.reserve(api_.SwapchainImages.size());
    for (const auto image : api_.SwapchainImages) {
        barriers.push_back(VkImageMemoryBarrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = 0,
            .dstAccessMask = 0,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange = {
                VK_IMAGE_ASPECT_COLOR_BIT,
                0,
                1,
                0,
                1
            }
        });
    }
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        static_cast<std::uint32_t>(barriers.size()),
        barriers.data()
    );

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        vkFreeCommandBuffers(api_.Device, api_.CommandPool, 1, &commandBuffer);
        return;
    }

    const VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = nullptr
    };
    if (vkQueueSubmit(api_.GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) == VK_SUCCESS) {
        vkQueueWaitIdle(api_.GraphicsQueue);
    }
    vkFreeCommandBuffers(api_.Device, api_.CommandPool, 1, &commandBuffer);
}

std::uint32_t VulkanContext::FindMemoryType(const std::uint32_t typeFilter, const VkMemoryPropertyFlags properties) const {
    if (api_.PhysicalDevice == VK_NULL_HANDLE) {
        return UINT32_MAX;
    }
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(api_.PhysicalDevice, &memoryProperties);
    for (std::uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1U << i)) != 0U && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return UINT32_MAX;
}

void VulkanContext::InitializeBackendObjects() {
    if (api_.Device == VK_NULL_HANDLE) {
        return;
    }

    if (api_.DescriptorSetLayout == VK_NULL_HANDLE) {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.reserve(9);
        bindings.push_back(VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr
        });
        for (std::uint32_t i = 1; i <= 8; ++i) {
            bindings.push_back(VkDescriptorSetLayoutBinding{
                .binding = i,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = nullptr
            });
        }
        const VkDescriptorSetLayoutCreateInfo layoutInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .bindingCount = static_cast<std::uint32_t>(bindings.size()),
            .pBindings = bindings.data()
        };
        vkCreateDescriptorSetLayout(api_.Device, &layoutInfo, nullptr, &api_.DescriptorSetLayout);
    }

    if (api_.DescriptorPool == VK_NULL_HANDLE) {
        const std::array poolSizes{
            VkDescriptorPoolSize{
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 256
            },
            VkDescriptorPoolSize{
                .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 128
            }
        };
        const VkDescriptorPoolCreateInfo poolInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
            .maxSets = 128,
            .poolSizeCount = static_cast<std::uint32_t>(poolSizes.size()),
            .pPoolSizes = poolSizes.data()
        };
        vkCreateDescriptorPool(api_.Device, &poolInfo, nullptr, &api_.DescriptorPool);
    }

    if (api_.PipelineLayout == VK_NULL_HANDLE && api_.DescriptorSetLayout != VK_NULL_HANDLE) {
        const VkPushConstantRange pushConstantRange{
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = static_cast<std::uint32_t>(sizeof(Common::DrawPushConstants))
        };
        const VkPipelineLayoutCreateInfo layoutInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .setLayoutCount = 1,
            .pSetLayouts = &api_.DescriptorSetLayout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &pushConstantRange
        };
        vkCreatePipelineLayout(api_.Device, &layoutInfo, nullptr, &api_.PipelineLayout);
    }

    RecreateSwapchain();

    if (api_.RenderPass == VK_NULL_HANDLE) {
        const VkAttachmentDescription colorAttachment{
            .flags = 0,
            .format = api_.ColorFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        };
        const VkAttachmentDescription depthAttachment{
            .flags = 0,
            .format = api_.DepthFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        };
        const VkAttachmentReference colorRef{
            .attachment = 0,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        };
        const VkAttachmentReference depthRef{
            .attachment = 1,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        };
        const std::array attachments{colorAttachment, depthAttachment};
        const VkSubpassDependency dependency{
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = 0
        };
        const VkSubpassDescription subpass{
            .flags = 0,
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount = 0,
            .pInputAttachments = nullptr,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorRef,
            .pResolveAttachments = nullptr,
            .pDepthStencilAttachment = &depthRef,
            .preserveAttachmentCount = 0,
            .pPreserveAttachments = nullptr
        };
        const VkRenderPassCreateInfo passInfo{
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
        vkCreateRenderPass(api_.Device, &passInfo, nullptr, &api_.RenderPass);
    }

    RecreateDepthAttachment();
    RecreateFramebuffer();

    if (imageAvailableSemaphore_ == VK_NULL_HANDLE) {
        const VkSemaphoreCreateInfo semaphoreInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0
        };
        vkCreateSemaphore(api_.Device, &semaphoreInfo, nullptr, &imageAvailableSemaphore_);
    }
    RecreateRenderFinishedSemaphores();
    if (inFlightFence_ == VK_NULL_HANDLE) {
        const VkFenceCreateInfo fenceInfo{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };
        vkCreateFence(api_.Device, &fenceInfo, nullptr, &inFlightFence_);
    }
}

void VulkanContext::RecreateRenderFinishedSemaphores() {
    if (api_.Device == VK_NULL_HANDLE) {
        return;
    }
    const std::size_t desiredCount = api_.SwapchainImages.size();
    if (desiredCount == renderFinishedSemaphores_.size()) {
        return;
    }
    for (const auto semaphore : renderFinishedSemaphores_) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(api_.Device, semaphore, nullptr);
        }
    }
    renderFinishedSemaphores_.clear();
    if (desiredCount == 0) {
        return;
    }
    renderFinishedSemaphores_.reserve(desiredCount);
    const VkSemaphoreCreateInfo semaphoreInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0
    };
    for (std::size_t i = 0; i < desiredCount; ++i) {
        VkSemaphore semaphore = VK_NULL_HANDLE;
        if (vkCreateSemaphore(api_.Device, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS) {
            semaphore = VK_NULL_HANDLE;
        }
        renderFinishedSemaphores_.push_back(semaphore);
    }
}

void VulkanContext::DestroyColorAttachment() {
    if (api_.Device == VK_NULL_HANDLE) {
        return;
    }
    if (api_.ColorAttachmentView != VK_NULL_HANDLE && ownedColorImage_ != VK_NULL_HANDLE) {
        vkDestroyImageView(api_.Device, api_.ColorAttachmentView, nullptr);
        api_.ColorAttachmentView = VK_NULL_HANDLE;
    }
    if (ownedColorImage_ != VK_NULL_HANDLE) {
        vkDestroyImage(api_.Device, ownedColorImage_, nullptr);
        ownedColorImage_ = VK_NULL_HANDLE;
    }
    if (ownedColorMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(api_.Device, ownedColorMemory_, nullptr);
        ownedColorMemory_ = VK_NULL_HANDLE;
    }
}

void VulkanContext::RecreateColorAttachment() {
    if (api_.Device == VK_NULL_HANDLE || api_.PhysicalDevice == VK_NULL_HANDLE || api_.SurfaceExtent.width == 0U ||
        api_.SurfaceExtent.height == 0U) {
        return;
    }

    if (api_.ColorAttachmentView != VK_NULL_HANDLE && ownedColorImage_ == VK_NULL_HANDLE) {
        return;
    }

    DestroyColorAttachment();

    const VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = api_.ColorFormat,
        .extent = {api_.SurfaceExtent.width, api_.SurfaceExtent.height, 1},
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
    if (vkCreateImage(api_.Device, &imageInfo, nullptr, &ownedColorImage_) != VK_SUCCESS) {
        ownedColorImage_ = VK_NULL_HANDLE;
        return;
    }

    VkMemoryRequirements memRequirements{};
    vkGetImageMemoryRequirements(api_.Device, ownedColorImage_, &memRequirements);
    const std::uint32_t memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memoryTypeIndex == UINT32_MAX) {
        DestroyColorAttachment();
        return;
    }

    const VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = memoryTypeIndex
    };
    if (vkAllocateMemory(api_.Device, &allocInfo, nullptr, &ownedColorMemory_) != VK_SUCCESS) {
        DestroyColorAttachment();
        return;
    }

    if (vkBindImageMemory(api_.Device, ownedColorImage_, ownedColorMemory_, 0) != VK_SUCCESS) {
        DestroyColorAttachment();
        return;
    }

    const VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = ownedColorImage_,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = api_.ColorFormat,
        .components = {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    if (vkCreateImageView(api_.Device, &viewInfo, nullptr, &api_.ColorAttachmentView) != VK_SUCCESS) {
        DestroyColorAttachment();
    }
}

void VulkanContext::DestroyDepthAttachment() {
    if (api_.Device == VK_NULL_HANDLE) {
        return;
    }
    if (api_.DepthAttachmentView != VK_NULL_HANDLE && ownedDepthImage_ != VK_NULL_HANDLE) {
        vkDestroyImageView(api_.Device, api_.DepthAttachmentView, nullptr);
        api_.DepthAttachmentView = VK_NULL_HANDLE;
    }
    if (ownedDepthImage_ != VK_NULL_HANDLE) {
        vkDestroyImage(api_.Device, ownedDepthImage_, nullptr);
        ownedDepthImage_ = VK_NULL_HANDLE;
    }
    if (ownedDepthMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(api_.Device, ownedDepthMemory_, nullptr);
        ownedDepthMemory_ = VK_NULL_HANDLE;
    }
}

void VulkanContext::RecreateDepthAttachment() {
    if (api_.Device == VK_NULL_HANDLE || api_.PhysicalDevice == VK_NULL_HANDLE || api_.SurfaceExtent.width == 0U ||
        api_.SurfaceExtent.height == 0U) {
        return;
    }

    if (api_.DepthAttachmentView != VK_NULL_HANDLE && ownedDepthImage_ == VK_NULL_HANDLE) {
        return;
    }

    DestroyDepthAttachment();

    const VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = api_.DepthFormat,
        .extent = {api_.SurfaceExtent.width, api_.SurfaceExtent.height, 1},
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
    if (vkCreateImage(api_.Device, &imageInfo, nullptr, &ownedDepthImage_) != VK_SUCCESS) {
        ownedDepthImage_ = VK_NULL_HANDLE;
        return;
    }

    VkMemoryRequirements memRequirements{};
    vkGetImageMemoryRequirements(api_.Device, ownedDepthImage_, &memRequirements);
    const std::uint32_t memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memoryTypeIndex == UINT32_MAX) {
        DestroyDepthAttachment();
        return;
    }

    const VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = memoryTypeIndex
    };
    if (vkAllocateMemory(api_.Device, &allocInfo, nullptr, &ownedDepthMemory_) != VK_SUCCESS) {
        DestroyDepthAttachment();
        return;
    }

    if (vkBindImageMemory(api_.Device, ownedDepthImage_, ownedDepthMemory_, 0) != VK_SUCCESS) {
        DestroyDepthAttachment();
        return;
    }

    const VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = ownedDepthImage_,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = api_.DepthFormat,
        .components = {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange = {Utils::DepthAspectMaskForFormat(api_.DepthFormat), 0, 1, 0, 1}
    };
    if (vkCreateImageView(api_.Device, &viewInfo, nullptr, &api_.DepthAttachmentView) != VK_SUCCESS) {
        DestroyDepthAttachment();
        return;
    }

    if (api_.CommandPool == VK_NULL_HANDLE || api_.GraphicsQueue == VK_NULL_HANDLE) {
        return;
    }

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    const VkCommandBufferAllocateInfo commandAllocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = api_.CommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    if (vkAllocateCommandBuffers(api_.Device, &commandAllocInfo, &commandBuffer) != VK_SUCCESS) {
        return;
    }
    const VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        vkFreeCommandBuffers(api_.Device, api_.CommandPool, 1, &commandBuffer);
        return;
    }
    const VkImageMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = ownedDepthImage_,
        .subresourceRange = {Utils::DepthAspectMaskForFormat(api_.DepthFormat), 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier
    );
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        vkFreeCommandBuffers(api_.Device, api_.CommandPool, 1, &commandBuffer);
        return;
    }
    const VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = nullptr
    };
    if (vkQueueSubmit(api_.GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) == VK_SUCCESS) {
        vkQueueWaitIdle(api_.GraphicsQueue);
    }
    vkFreeCommandBuffers(api_.Device, api_.CommandPool, 1, &commandBuffer);
}

void VulkanContext::RecreateFramebuffer() {
    if (api_.Device == VK_NULL_HANDLE) {
        return;
    }
    for (const auto framebuffer : api_.SwapchainFramebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(api_.Device, framebuffer, nullptr);
        }
    }
    api_.SwapchainFramebuffers.clear();
    api_.Framebuffer = VK_NULL_HANDLE;

    if (api_.RenderPass == VK_NULL_HANDLE || api_.SwapchainImageViews.empty() ||
        api_.DepthAttachmentView == VK_NULL_HANDLE || api_.SurfaceExtent.width == 0U ||
        api_.SurfaceExtent.height == 0U) {
        return;
    }

    api_.SwapchainFramebuffers.reserve(api_.SwapchainImageViews.size());
    for (const auto imageView : api_.SwapchainImageViews) {
        const VkImageView attachments[] = {imageView, api_.DepthAttachmentView};
        const VkFramebufferCreateInfo framebufferInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderPass = api_.RenderPass,
            .attachmentCount = 2,
            .pAttachments = attachments,
            .width = api_.SurfaceExtent.width,
            .height = api_.SurfaceExtent.height,
            .layers = 1
        };
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        if (vkCreateFramebuffer(api_.Device, &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS) {
            framebuffer = VK_NULL_HANDLE;
        }
        api_.SwapchainFramebuffers.push_back(framebuffer);
    }
}

std::unique_ptr<RHI::ICommandBuffer> VulkanContext::AllocateCommandBuffer() {
    if (api_.Device == VK_NULL_HANDLE || api_.CommandPool == VK_NULL_HANDLE) {
        return std::make_unique<VulkanCommandBuffer>(*this, VK_NULL_HANDLE);
    }

    const VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = api_.CommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    VkCommandBuffer handle = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(api_.Device, &allocInfo, &handle) != VK_SUCCESS) {
        handle = VK_NULL_HANDLE;
    }
    if (handle != VK_NULL_HANDLE) {
        const VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr
        };
        if (vkBeginCommandBuffer(handle, &beginInfo) != VK_SUCCESS) {
            vkFreeCommandBuffers(api_.Device, api_.CommandPool, 1, &handle);
            handle = VK_NULL_HANDLE;
        }
    }

    return std::make_unique<VulkanCommandBuffer>(*this, handle);
}

VkShaderModule VulkanContext::CreateShaderModule(const std::vector<std::uint32_t>& spirv) const {
    if (api_.Device == VK_NULL_HANDLE || spirv.empty()) {
        return VK_NULL_HANDLE;
    }
    const VkShaderModuleCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .codeSize = spirv.size() * sizeof(std::uint32_t),
        .pCode = spirv.data()
    };
    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(api_.Device, &createInfo, nullptr, &module) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return module;
}

std::unique_ptr<RHI::IPipeline> VulkanContext::CreatePipeline(const RHI::PipelineDesc& desc) {
    const VkRenderPass renderPass =
        desc.renderPassHandle != nullptr ? reinterpret_cast<VkRenderPass>(desc.renderPassHandle) : api_.RenderPass;
    if (api_.Device == VK_NULL_HANDLE || renderPass == VK_NULL_HANDLE || api_.PipelineLayout == VK_NULL_HANDLE) {
        return std::make_unique<VulkanPipeline>(desc, nullptr);
    }

    const auto vertSpirv = ShaderLoader::LoadSPIRV(desc.pipelineId, "vert");
    const auto fragSpirv = ShaderLoader::LoadSPIRV(desc.pipelineId, "frag");
    const VkShaderModule vertModule = CreateShaderModule(vertSpirv);
    const VkShaderModule fragModule = CreateShaderModule(fragSpirv);
    if (vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE) {
        if (vertModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(api_.Device, vertModule, nullptr);
        }
        if (fragModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(api_.Device, fragModule, nullptr);
        }
        return std::make_unique<VulkanPipeline>(desc, nullptr);
    }

    const VkPipelineShaderStageCreateInfo vertStage{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertModule,
        .pName = "main",
        .pSpecializationInfo = nullptr
    };
    const VkPipelineShaderStageCreateInfo fragStage{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fragModule,
        .pName = "main",
        .pSpecializationInfo = nullptr
    };
    const std::array stages{vertStage, fragStage};

    std::array<VkVertexInputBindingDescription, 1> bindingDescriptions{};
    std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
    std::uint32_t bindingDescriptionCount = 0;
    std::uint32_t attributeDescriptionCount = 0;
    if (desc.vertexLayout == RHI::VertexLayout::P3 || desc.vertexLayout == RHI::VertexLayout::P3N3) {
        bindingDescriptions[0] = VkVertexInputBindingDescription{
            .binding = 0,
            .stride = static_cast<std::uint32_t>(sizeof(float) * 6),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        };
        attributeDescriptions[0] = VkVertexInputAttributeDescription{
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = 0
        };
        bindingDescriptionCount = 1;
        if (desc.vertexLayout == RHI::VertexLayout::P3N3) {
            attributeDescriptions[1] = VkVertexInputAttributeDescription{
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = static_cast<std::uint32_t>(sizeof(float) * 3)
            };
            attributeDescriptionCount = 2;
        } else {
            attributeDescriptionCount = 1;
        }
    }
    const VkPipelineVertexInputStateCreateInfo vertexInput{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .vertexBindingDescriptionCount = bindingDescriptionCount,
        .pVertexBindingDescriptions = bindingDescriptionCount > 0 ? bindingDescriptions.data() : nullptr,
        .vertexAttributeDescriptionCount = attributeDescriptionCount,
        .pVertexAttributeDescriptions = attributeDescriptionCount > 0 ? attributeDescriptions.data() : nullptr
    };
    const VkPipelineInputAssemblyStateCreateInfo inputAssembly{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };
    const VkPipelineViewportStateCreateInfo viewportState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .viewportCount = 1,
        .pViewports = nullptr,
        .scissorCount = 1,
        .pScissors = nullptr
    };
    const VkPipelineRasterizationStateCreateInfo rasterizer{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthClampEnable = (desc.pipelineId == "shadow" && depthClampEnabled_) ? VK_TRUE : VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = Utils::ResolveCullMode(desc),
        // Main/sky use a CPU-side projection flip (projection[1][1] *= -1) to match OpenGL-style screen-space winding.
        // Shadow uses an unflipped light projection (0..1 depth), so its winding is opposite.
        .frontFace = (desc.pipelineId == "shadow") ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .depthBiasConstantFactor = 0.0F,
        .depthBiasClamp = 0.0F,
        .depthBiasSlopeFactor = 0.0F,
        .lineWidth = 1.0F
    };
    const VkSampleCountFlagBits sampleCount = Utils::ResolveSampleCount(api_.PhysicalDevice, desc.sampleCount);
    const VkPipelineMultisampleStateCreateInfo multisample{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .rasterizationSamples = sampleCount,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 1.0F,
        .pSampleMask = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE
    };
    const VkPipelineDepthStencilStateCreateInfo depthStencil{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthTestEnable = desc.depthTest ? VK_TRUE : VK_FALSE,
        .depthWriteEnable = desc.depthWrite ? VK_TRUE : VK_FALSE,
        .depthCompareOp = desc.depthTest ? Utils::ResolveDepthCompare(desc.depthCompare) : VK_COMPARE_OP_ALWAYS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .front = {},
        .back = {},
        .minDepthBounds = 0.0F,
        .maxDepthBounds = 1.0F
    };
    const std::uint32_t colorAttachmentCount = desc.colorAttachmentCount;
    std::vector<VkPipelineColorBlendAttachmentState> blendAttachments{};
    if (colorAttachmentCount > 0U) {
        blendAttachments.reserve(colorAttachmentCount);
        for (std::uint32_t i = 0; i < colorAttachmentCount; ++i) {
            blendAttachments.push_back(VkPipelineColorBlendAttachmentState{
                .blendEnable = desc.blending ? VK_TRUE : VK_FALSE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp = VK_BLEND_OP_ADD,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                                  VK_COLOR_COMPONENT_A_BIT
            });
        }
    }
    const VkPipelineColorBlendStateCreateInfo colorBlend{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = static_cast<std::uint32_t>(blendAttachments.size()),
        .pAttachments = blendAttachments.empty() ? nullptr : blendAttachments.data(),
        .blendConstants = {0.0F, 0.0F, 0.0F, 0.0F}
    };
    const std::array dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    const VkPipelineDynamicStateCreateInfo dynamicState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()
    };
    const VkGraphicsPipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stageCount = static_cast<std::uint32_t>(stages.size()),
        .pStages = stages.data(),
        .pVertexInputState = &vertexInput,
        .pInputAssemblyState = &inputAssembly,
        .pTessellationState = nullptr,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisample,
        .pDepthStencilState = &depthStencil,
        .pColorBlendState = &colorBlend,
        .pDynamicState = &dynamicState,
        .layout = api_.PipelineLayout,
        .renderPass = renderPass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };

    VkPipeline vkPipeline = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(api_.Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &vkPipeline) != VK_SUCCESS) {
        vkPipeline = VK_NULL_HANDLE;
    }

    vkDestroyShaderModule(api_.Device, vertModule, nullptr);
    vkDestroyShaderModule(api_.Device, fragModule, nullptr);

    return std::make_unique<VulkanPipeline>(desc, reinterpret_cast<void*>(vkPipeline), [device = api_.Device](void* handle) {
        if (device != VK_NULL_HANDLE && handle != nullptr) {
            vkDestroyPipeline(device, reinterpret_cast<VkPipeline>(handle), nullptr);
        }
    });
}

std::unique_ptr<RHI::IRenderTarget> VulkanContext::CreateRenderTarget(const RHI::RenderTargetDesc& desc) {
    if (api_.Device == VK_NULL_HANDLE || desc.width == 0U || desc.height == 0U ||
        (desc.colorAttachmentCount == 0U && !desc.hasDepth)) {
        return nullptr;
    }

    VkFormat offscreenColorFormat = api_.ColorFormat;
    if (desc.colorAttachmentCount > 0U) {
        offscreenColorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
        VkFormatProperties formatProperties{};
        vkGetPhysicalDeviceFormatProperties(api_.PhysicalDevice, offscreenColorFormat, &formatProperties);
        const VkFormatFeatureFlags requiredFeatures =
            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
        if ((formatProperties.optimalTilingFeatures & requiredFeatures) != requiredFeatures) {
            offscreenColorFormat = api_.ColorFormat;
        }
    }

    VkSampleCountFlagBits sampleCount = Utils::ResolveSampleCount(api_.PhysicalDevice, desc.sampleCount);
    if (desc.depthTexture && sampleCount != VK_SAMPLE_COUNT_1_BIT) {
        sampleCount = VK_SAMPLE_COUNT_1_BIT;
    }
    const bool useMsaa = sampleCount != VK_SAMPLE_COUNT_1_BIT;

    std::vector<Utils::VulkanRenderTarget::ColorAttachment> colors(desc.colorAttachmentCount);
    std::vector<Utils::VulkanRenderTarget::ColorAttachment> msaaColors(useMsaa ? desc.colorAttachmentCount : 0U);
    std::vector<VkAttachmentDescription> attachments{};
    const std::uint32_t colorAttachmentSlots =
        useMsaa ? (desc.colorAttachmentCount * 2U) : desc.colorAttachmentCount;
    attachments.reserve(colorAttachmentSlots + (desc.hasDepth ? 1U : 0U));
    std::vector<VkAttachmentReference> colorRefs{};
    colorRefs.reserve(desc.colorAttachmentCount);
    std::vector<VkAttachmentReference> resolveRefs{};
    resolveRefs.reserve(desc.colorAttachmentCount);
    std::vector<VkImageView> framebufferAttachments{};
    framebufferAttachments.reserve(colorAttachmentSlots + (desc.hasDepth ? 1U : 0U));

    const auto createColorAttachment = [&](Utils::VulkanRenderTarget::ColorAttachment& outColor,
                                           const VkSampleCountFlagBits samples,
                                           const bool sampled) -> bool {
        const VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                        (sampled ? VkImageUsageFlags{VK_IMAGE_USAGE_SAMPLED_BIT} : VkImageUsageFlags{0});
        const VkImageCreateInfo imageInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = offscreenColorFormat,
            .extent = {desc.width, desc.height, 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = samples,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };
        if (vkCreateImage(api_.Device, &imageInfo, nullptr, &outColor.image) != VK_SUCCESS) {
            return false;
        }
        VkMemoryRequirements memoryRequirements{};
        vkGetImageMemoryRequirements(api_.Device, outColor.image, &memoryRequirements);
        const VkMemoryAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .allocationSize = memoryRequirements.size,
            .memoryTypeIndex = FindMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        };
        if (vkAllocateMemory(api_.Device, &allocInfo, nullptr, &outColor.memory) != VK_SUCCESS) {
            return false;
        }
        if (vkBindImageMemory(api_.Device, outColor.image, outColor.memory, 0) != VK_SUCCESS) {
            return false;
        }
        const VkImageViewCreateInfo viewInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = outColor.image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = offscreenColorFormat,
            .components = {
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY
            },
            .subresourceRange = {
                VK_IMAGE_ASPECT_COLOR_BIT,
                0,
                1,
                0,
                1
            }
          };
          if (vkCreateImageView(api_.Device, &viewInfo, nullptr, &outColor.view) != VK_SUCCESS) {
              return false;
          }
          if (!sampled) {
              outColor.sampler = VK_NULL_HANDLE;
              return true;
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
              .compareOp = VK_COMPARE_OP_ALWAYS,
              .minLod = 0.0F,
              .maxLod = 1.0F,
              .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
              .unnormalizedCoordinates = VK_FALSE
          };
          return vkCreateSampler(api_.Device, &samplerInfo, nullptr, &outColor.sampler) == VK_SUCCESS;
      };

    if (desc.colorAttachmentCount > 0U) {
        for (RHI::u32 i = 0; i < desc.colorAttachmentCount; ++i) {
            if (useMsaa) {
                if (!createColorAttachment(msaaColors[i], sampleCount, false)) {
                    return nullptr;
                }
                if (!createColorAttachment(colors[i], VK_SAMPLE_COUNT_1_BIT, true)) {
                    return nullptr;
                }
                attachments.push_back(VkAttachmentDescription{
                    .flags = 0,
                    .format = offscreenColorFormat,
                    .samples = sampleCount,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                });
                attachments.push_back(VkAttachmentDescription{
                    .flags = 0,
                    .format = offscreenColorFormat,
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                });
                colorRefs.push_back(VkAttachmentReference{
                    .attachment = i * 2U,
                    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                });
                resolveRefs.push_back(VkAttachmentReference{
                    .attachment = (i * 2U) + 1U,
                    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                });
                framebufferAttachments.push_back(msaaColors[i].view);
                framebufferAttachments.push_back(colors[i].view);
            } else {
                if (!createColorAttachment(colors[i], VK_SAMPLE_COUNT_1_BIT, true)) {
                    return nullptr;
                }
                attachments.push_back(VkAttachmentDescription{
                    .flags = 0,
                    .format = offscreenColorFormat,
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                });
                colorRefs.push_back(VkAttachmentReference{
                    .attachment = i,
                    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                });
                framebufferAttachments.push_back(colors[i].view);
            }
        }
    }

    VkImage depthImage = VK_NULL_HANDLE;
    VkDeviceMemory depthMemory = VK_NULL_HANDLE;
    VkImageView depthView = VK_NULL_HANDLE;
    VkSampler depthSampler = VK_NULL_HANDLE;
    VkAttachmentReference depthRef{};
    if (desc.hasDepth) {
        const VkImageUsageFlags depthUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                             (desc.depthTexture ? VkImageUsageFlags{VK_IMAGE_USAGE_SAMPLED_BIT} : VkImageUsageFlags{0});
        const VkImageLayout depthSamplingLayout = desc.depthTexture
                                                      ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                                      : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        const VkImageCreateInfo imageInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = api_.DepthFormat,
            .extent = {desc.width, desc.height, 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = sampleCount,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = depthUsage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };
        if (vkCreateImage(api_.Device, &imageInfo, nullptr, &depthImage) != VK_SUCCESS) {
            return nullptr;
        }
        VkMemoryRequirements memoryRequirements{};
        vkGetImageMemoryRequirements(api_.Device, depthImage, &memoryRequirements);
        const VkMemoryAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .allocationSize = memoryRequirements.size,
            .memoryTypeIndex = FindMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        };
        if (vkAllocateMemory(api_.Device, &allocInfo, nullptr, &depthMemory) != VK_SUCCESS) {
            return nullptr;
        }
        if (vkBindImageMemory(api_.Device, depthImage, depthMemory, 0) != VK_SUCCESS) {
            return nullptr;
        }
        const VkImageViewCreateInfo viewInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = depthImage,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = api_.DepthFormat,
            .components = {
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY
            },
            .subresourceRange = {
                Utils::DepthAspectMaskForFormat(api_.DepthFormat),
                0,
                1,
                0,
                1
            }
        };
        if (vkCreateImageView(api_.Device, &viewInfo, nullptr, &depthView) != VK_SUCCESS) {
            return nullptr;
        }

        attachments.push_back(VkAttachmentDescription{
            .flags = 0,
            .format = api_.DepthFormat,
            .samples = sampleCount,
            .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = depthSamplingLayout,
            .finalLayout = depthSamplingLayout
        });
        const std::uint32_t depthAttachmentIndex =
            useMsaa ? static_cast<std::uint32_t>(desc.colorAttachmentCount * 2U)
                    : static_cast<std::uint32_t>(desc.colorAttachmentCount);
        depthRef = VkAttachmentReference{
            .attachment = depthAttachmentIndex,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        };
        framebufferAttachments.push_back(depthView);

        if (desc.depthTexture) {
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
                .compareEnable = VK_TRUE,
                .compareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
                .minLod = 0.0F,
                .maxLod = 1.0F,
                .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
                .unnormalizedCoordinates = VK_FALSE
            };
            if (vkCreateSampler(api_.Device, &samplerInfo, nullptr, &depthSampler) != VK_SUCCESS) {
                return nullptr;
            }
        }
    }

    const VkSubpassDescription subpass{
        .flags = 0,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = 0,
        .pInputAttachments = nullptr,
        .colorAttachmentCount = static_cast<std::uint32_t>(colorRefs.size()),
        .pColorAttachments = colorRefs.empty() ? nullptr : colorRefs.data(),
        .pResolveAttachments = resolveRefs.empty() ? nullptr : resolveRefs.data(),
        .pDepthStencilAttachment = desc.hasDepth ? &depthRef : nullptr,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = nullptr
    };
    const std::array<VkSubpassDependency, 2> dependencies{
        VkSubpassDependency{
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = 0,
        },
        VkSubpassDependency{
            .srcSubpass = 0,
            .dstSubpass = VK_SUBPASS_EXTERNAL,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
            .dependencyFlags = 0,
        },
    };
    const VkRenderPassCreateInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .attachmentCount = static_cast<std::uint32_t>(attachments.size()),
        .pAttachments = attachments.data(),
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = static_cast<std::uint32_t>(dependencies.size()),
        .pDependencies = dependencies.data()
    };
    VkRenderPass renderPass = VK_NULL_HANDLE;
    if (vkCreateRenderPass(api_.Device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        return nullptr;
    }

    const VkFramebufferCreateInfo framebufferInfo{
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .renderPass = renderPass,
        .attachmentCount = static_cast<std::uint32_t>(framebufferAttachments.size()),
        .pAttachments = framebufferAttachments.data(),
        .width = desc.width,
        .height = desc.height,
        .layers = 1
    };
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    if (vkCreateFramebuffer(api_.Device, &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS) {
        vkDestroyRenderPass(api_.Device, renderPass, nullptr);
        return nullptr;
    }

    // Prime attachment layouts so first render pass load transitions are valid.
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    const VkCommandBufferAllocateInfo allocateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = api_.CommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
      if (vkAllocateCommandBuffers(api_.Device, &allocateInfo, &commandBuffer) == VK_SUCCESS) {
          const VkCommandBufferBeginInfo beginInfo{
              .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
              .pNext = nullptr,
              .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
              .pInheritanceInfo = nullptr
          };
          vkBeginCommandBuffer(commandBuffer, &beginInfo);

          if (useMsaa) {
              for (const auto& color : msaaColors) {
                  const VkImageMemoryBarrier toColorAttachment{
                      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                      .pNext = nullptr,
                      .srcAccessMask = 0,
                      .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                      .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                      .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                      .image = color.image,
                      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
                  };
                  vkCmdPipelineBarrier(
                      commandBuffer,
                      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                      0,
                      0,
                      nullptr,
                      0,
                      nullptr,
                      1,
                      &toColorAttachment
                  );
              }
          }

          for (const auto& color : colors) {
              const VkImageMemoryBarrier toShaderRead{
                  .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                  .pNext = nullptr,
                  .srcAccessMask = 0,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = color.image,
                .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
            };
            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &toShaderRead
            );
        }
        if (depthImage != VK_NULL_HANDLE) {
            const VkImageLayout depthPrimeLayout = desc.depthTexture
                                                       ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                                       : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            const VkImageMemoryBarrier toDepthAttachment{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = nullptr,
                .srcAccessMask = 0,
                .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = depthPrimeLayout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = depthImage,
                .subresourceRange = {Utils::DepthAspectMaskForFormat(api_.DepthFormat), 0, 1, 0, 1}
            };
            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &toDepthAttachment
            );
        }

        vkEndCommandBuffer(commandBuffer);
        const VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,
            .pWaitDstStageMask = nullptr,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffer,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = nullptr
        };
        vkQueueSubmit(api_.GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(api_.GraphicsQueue);
        vkFreeCommandBuffers(api_.Device, api_.CommandPool, 1, &commandBuffer);
    }

    const RHI::Format rhiDepthFormat = api_.DepthFormat == VK_FORMAT_D24_UNORM_S8_UINT ? RHI::Format::D24S8
                                                                                       : RHI::Format::D32_Float;
    return std::make_unique<Utils::VulkanRenderTarget>(
        api_.Device,
        desc.width,
        desc.height,
        offscreenColorFormat,
        renderPass,
        framebuffer,
        colors,
        msaaColors,
        depthImage,
        depthMemory,
        depthView,
        depthSampler,
        rhiDepthFormat,
        static_cast<RHI::u32>(sampleCount)
    );
}

std::unique_ptr<RHI::IBuffer> VulkanContext::CreateBuffer(const RHI::BufferDesc& desc) {
    if (api_.Device == VK_NULL_HANDLE || desc.size == 0) {
        return std::make_unique<Utils::VulkanBuffer>(api_.Device, VK_NULL_HANDLE, VK_NULL_HANDLE, desc.size);
    }

    VkBufferUsageFlags usageFlags = 0;
    switch (desc.type) {
        case RHI::BufferType::Vertex:
            usageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            break;
        case RHI::BufferType::Index:
            usageFlags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            break;
        case RHI::BufferType::Uniform:
            usageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            break;
        case RHI::BufferType::Staging:
            usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            break;
    }

    const VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = static_cast<VkDeviceSize>(desc.size),
        .usage = usageFlags,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr
    };

    VkBuffer buffer = VK_NULL_HANDLE;
    if (vkCreateBuffer(api_.Device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        return std::make_unique<Utils::VulkanBuffer>(api_.Device, VK_NULL_HANDLE, VK_NULL_HANDLE, desc.size);
    }

    VkMemoryRequirements memRequirements{};
    vkGetBufferMemoryRequirements(api_.Device, buffer, &memRequirements);
    const std::uint32_t memoryTypeIndex =
        FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (memoryTypeIndex == UINT32_MAX) {
        vkDestroyBuffer(api_.Device, buffer, nullptr);
        return std::make_unique<Utils::VulkanBuffer>(api_.Device, VK_NULL_HANDLE, VK_NULL_HANDLE, desc.size);
    }

    const VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = memoryTypeIndex
    };
    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (vkAllocateMemory(api_.Device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyBuffer(api_.Device, buffer, nullptr);
        return std::make_unique<Utils::VulkanBuffer>(api_.Device, VK_NULL_HANDLE, VK_NULL_HANDLE, desc.size);
    }
    vkBindBufferMemory(api_.Device, buffer, memory, 0);

    if (desc.initialData != nullptr) {
        void* mapped = nullptr;
        if (vkMapMemory(api_.Device, memory, 0, static_cast<VkDeviceSize>(desc.size), 0, &mapped) == VK_SUCCESS) {
            std::memcpy(mapped, desc.initialData, desc.size);
            vkUnmapMemory(api_.Device, memory);
        }
    }

    return std::make_unique<Utils::VulkanBuffer>(api_.Device, buffer, memory, desc.size);
}

std::unique_ptr<RHI::IResourceSet> VulkanContext::CreateResourceSet(const RHI::ResourceSetDesc& desc) {
    if (desc.nativeHandleHint != nullptr) {
        return std::make_unique<Utils::VulkanResourceSet>(
            api_.Device,
            api_.DescriptorPool,
            reinterpret_cast<VkDescriptorSet>(desc.nativeHandleHint),
            false
        );
    }
    if (api_.Device == VK_NULL_HANDLE || api_.DescriptorPool == VK_NULL_HANDLE || api_.DescriptorSetLayout == VK_NULL_HANDLE) {
        return std::make_unique<Utils::VulkanResourceSet>(api_.Device, api_.DescriptorPool, VK_NULL_HANDLE, false);
    }

    const VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = api_.DescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &api_.DescriptorSetLayout
    };
    VkDescriptorSet set = VK_NULL_HANDLE;
    if (vkAllocateDescriptorSets(api_.Device, &allocInfo, &set) != VK_SUCCESS) {
        return std::make_unique<Utils::VulkanResourceSet>(api_.Device, api_.DescriptorPool, VK_NULL_HANDLE, false);
    }

    std::vector<VkDescriptorImageInfo> imageInfos;
    std::vector<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkWriteDescriptorSet> writes;
    imageInfos.reserve(desc.bindingCount);
    bufferInfos.reserve(desc.bindingCount);
    writes.reserve(desc.bindingCount);
    for (RHI::u32 i = 0; i < desc.bindingCount; ++i) {
        const auto& binding = desc.bindings[i];
        if (binding.kind == RHI::ResourceBindingKind::UniformBuffer) {
            bufferInfos.push_back(VkDescriptorBufferInfo{
                .buffer = binding.buffer != nullptr ? reinterpret_cast<VkBuffer>(binding.buffer->GetNativeHandle()) : VK_NULL_HANDLE,
                .offset = 0,
                .range = VK_WHOLE_SIZE
            });
            writes.push_back(VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = set,
                .dstBinding = binding.slot,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pImageInfo = nullptr,
                .pBufferInfo = &bufferInfos.back(),
                .pTexelBufferView = nullptr
            });
        } else {
            const bool isDepthFormat = binding.texture.format == RHI::Format::D32_Float || binding.texture.format == RHI::Format::D24S8;
            imageInfos.push_back(VkDescriptorImageInfo{
                .sampler = binding.texture.sampler_handle_ptr != nullptr
                               ? reinterpret_cast<VkSampler>(binding.texture.sampler_handle_ptr)
                               : api_.DefaultSampler,
                .imageView = reinterpret_cast<VkImageView>(binding.texture.graphic_handle_ptr),
                .imageLayout = isDepthFormat ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                             : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            });
            writes.push_back(VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = set,
                .dstBinding = binding.slot,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &imageInfos.back(),
                .pBufferInfo = nullptr,
                .pTexelBufferView = nullptr
            });
        }
    }
    if (!writes.empty()) {
        vkUpdateDescriptorSets(api_.Device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

    return std::make_unique<Utils::VulkanResourceSet>(api_.Device, api_.DescriptorPool, set, true);
}

RHI::Texture VulkanContext::CreateTexture2D(const RHI::Texture2DDesc& desc) {
    if (api_.Device == VK_NULL_HANDLE || api_.PhysicalDevice == VK_NULL_HANDLE || api_.GraphicsQueue == VK_NULL_HANDLE ||
        api_.CommandPool == VK_NULL_HANDLE || desc.width == 0U || desc.height == 0U || desc.pixels.empty()) {
        return {};
    }
    if (desc.format != RHI::Format::R8G8B8A8_UNorm) {
        return {};
    }

    std::uint32_t mipLevels = 1U;
    if (desc.generateMipmaps) {
        std::uint32_t w = desc.width;
        std::uint32_t h = desc.height;
        while (w > 1U || h > 1U) {
            w = std::max(1U, w / 2U);
            h = std::max(1U, h / 2U);
            ++mipLevels;
        }
    }
    if (mipLevels > 1U) {
        VkFormatProperties formatProps{};
        vkGetPhysicalDeviceFormatProperties(api_.PhysicalDevice, VK_FORMAT_R8G8B8A8_UNORM, &formatProps);
        const bool supportsBlit =
            (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) != 0 &&
            (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) != 0;
        if (!supportsBlit) {
            mipLevels = 1U;
        }
    }

    const VkDeviceSize totalSize = static_cast<VkDeviceSize>(desc.width) * static_cast<VkDeviceSize>(desc.height) * 4U;
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    {
        const VkBufferCreateInfo bufferInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = totalSize,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr
        };
        if (vkCreateBuffer(api_.Device, &bufferInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
            return {};
        }
        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(api_.Device, stagingBuffer, &requirements);
        const std::uint32_t memoryType = FindMemoryType(
            requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        if (memoryType == UINT32_MAX) {
            vkDestroyBuffer(api_.Device, stagingBuffer, nullptr);
            return {};
        }
        const VkMemoryAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .allocationSize = requirements.size,
            .memoryTypeIndex = memoryType
        };
        if (vkAllocateMemory(api_.Device, &allocInfo, nullptr, &stagingMemory) != VK_SUCCESS) {
            vkDestroyBuffer(api_.Device, stagingBuffer, nullptr);
            return {};
        }
        vkBindBufferMemory(api_.Device, stagingBuffer, stagingMemory, 0);
        void* mapped = nullptr;
        if (vkMapMemory(api_.Device, stagingMemory, 0, totalSize, 0, &mapped) != VK_SUCCESS) {
            vkDestroyBuffer(api_.Device, stagingBuffer, nullptr);
            vkFreeMemory(api_.Device, stagingMemory, nullptr);
            return {};
        }
        std::memcpy(mapped, desc.pixels.data(), static_cast<std::size_t>(totalSize));
        vkUnmapMemory(api_.Device, stagingMemory);
    }

    OwnedTexture2D owned{};
    owned.MipLevels = mipLevels;
    {
        VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        if (mipLevels > 1U) {
            usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        }
        const VkImageCreateInfo imageInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .extent = {desc.width, desc.height, 1},
            .mipLevels = mipLevels,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };
        if (vkCreateImage(api_.Device, &imageInfo, nullptr, &owned.Image) != VK_SUCCESS) {
            vkDestroyBuffer(api_.Device, stagingBuffer, nullptr);
            vkFreeMemory(api_.Device, stagingMemory, nullptr);
            return {};
        }
        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(api_.Device, owned.Image, &requirements);
        const std::uint32_t memoryType = FindMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (memoryType == UINT32_MAX) {
            vkDestroyImage(api_.Device, owned.Image, nullptr);
            vkDestroyBuffer(api_.Device, stagingBuffer, nullptr);
            vkFreeMemory(api_.Device, stagingMemory, nullptr);
            return {};
        }
        const VkMemoryAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .allocationSize = requirements.size,
            .memoryTypeIndex = memoryType
        };
        if (vkAllocateMemory(api_.Device, &allocInfo, nullptr, &owned.Memory) != VK_SUCCESS) {
            vkDestroyImage(api_.Device, owned.Image, nullptr);
            vkDestroyBuffer(api_.Device, stagingBuffer, nullptr);
            vkFreeMemory(api_.Device, stagingMemory, nullptr);
            return {};
        }
        vkBindImageMemory(api_.Device, owned.Image, owned.Memory, 0);
    }

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    {
        const VkCommandBufferAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = api_.CommandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };
        if (vkAllocateCommandBuffers(api_.Device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
            vkDestroyBuffer(api_.Device, stagingBuffer, nullptr);
            vkFreeMemory(api_.Device, stagingMemory, nullptr);
            vkDestroyImage(api_.Device, owned.Image, nullptr);
            vkFreeMemory(api_.Device, owned.Memory, nullptr);
            return {};
        }
        const VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr
        };
        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        const VkImageMemoryBarrier toTransfer{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = owned.Image,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1}
        };
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &toTransfer
        );

        const VkBufferImageCopy region{
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
            .imageOffset = {0, 0, 0},
            .imageExtent = {desc.width, desc.height, 1}
        };
        vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, owned.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        if (mipLevels > 1U) {
            VkFormatProperties formatProps{};
            vkGetPhysicalDeviceFormatProperties(api_.PhysicalDevice, VK_FORMAT_R8G8B8A8_UNORM, &formatProps);
            const bool supportsLinearBlit =
                (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0;
            const VkFilter blitFilter = (desc.linearFiltering && supportsLinearBlit) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;

            std::int32_t mipWidth = static_cast<std::int32_t>(desc.width);
            std::int32_t mipHeight = static_cast<std::int32_t>(desc.height);
            for (std::uint32_t level = 1; level < mipLevels; ++level) {
                const VkImageMemoryBarrier barrierToSrc{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    .pNext = nullptr,
                    .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                    .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                    .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image = owned.Image,
                    .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, level - 1, 1, 0, 1}
                };
                vkCmdPipelineBarrier(
                    commandBuffer,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    0,
                    0,
                    nullptr,
                    0,
                    nullptr,
                    1,
                    &barrierToSrc
                );

                const VkImageBlit blit{
                    .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level - 1, 0, 1},
                    .srcOffsets = {{0, 0, 0}, {mipWidth, mipHeight, 1}},
                    .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level, 0, 1},
                    .dstOffsets = {
                        {0, 0, 0},
                        {std::max(1, mipWidth / 2), std::max(1, mipHeight / 2), 1}}
                };
                vkCmdBlitImage(
                    commandBuffer,
                    owned.Image,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    owned.Image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1,
                    &blit,
                    blitFilter
                );

                const VkImageMemoryBarrier barrierToShader{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    .pNext = nullptr,
                    .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                    .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                    .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image = owned.Image,
                    .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, level - 1, 1, 0, 1}
                };
                vkCmdPipelineBarrier(
                    commandBuffer,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    0,
                    0,
                    nullptr,
                    0,
                    nullptr,
                    1,
                    &barrierToShader
                );

                mipWidth = std::max(1, mipWidth / 2);
                mipHeight = std::max(1, mipHeight / 2);
            }

            const VkImageMemoryBarrier lastLevelToShader{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = nullptr,
                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = owned.Image,
                .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, mipLevels - 1, 1, 0, 1}
            };
            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &lastLevelToShader
            );
        } else {
            const VkImageMemoryBarrier toShader{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = nullptr,
                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = owned.Image,
                .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
            };
            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &toShader
            );
        }

        vkEndCommandBuffer(commandBuffer);
        const VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,
            .pWaitDstStageMask = nullptr,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffer,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = nullptr
        };
        vkQueueSubmit(api_.GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(api_.GraphicsQueue);
        vkFreeCommandBuffers(api_.Device, api_.CommandPool, 1, &commandBuffer);
    }

    vkDestroyBuffer(api_.Device, stagingBuffer, nullptr);
    vkFreeMemory(api_.Device, stagingMemory, nullptr);

    const VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = owned.Image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .components = {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1}
    };
    if (vkCreateImageView(api_.Device, &viewInfo, nullptr, &owned.View) != VK_SUCCESS) {
        vkDestroyImage(api_.Device, owned.Image, nullptr);
        vkFreeMemory(api_.Device, owned.Memory, nullptr);
        return {};
    }

    const VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .magFilter = desc.linearFiltering ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
        .minFilter = desc.linearFiltering ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
        .mipmapMode = desc.linearFiltering ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .mipLodBias = 0.0F,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0F,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0F,
        .maxLod = mipLevels > 1U ? static_cast<float>(mipLevels - 1U) : 0.0F,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
        .unnormalizedCoordinates = VK_FALSE
    };
    if (vkCreateSampler(api_.Device, &samplerInfo, nullptr, &owned.Sampler) != VK_SUCCESS) {
        vkDestroyImageView(api_.Device, owned.View, nullptr);
        vkDestroyImage(api_.Device, owned.Image, nullptr);
        vkFreeMemory(api_.Device, owned.Memory, nullptr);
        return {};
    }

    owned2DTextures_[owned.View] = owned;

    RHI::Texture texture{};
    texture.width = desc.width;
    texture.height = desc.height;
    texture.format = desc.format;
    texture.type = RHI::TextureType::Texture2D;
    texture.graphic_handle_ptr = reinterpret_cast<void*>(owned.View);
    texture.sampler_handle_ptr = reinterpret_cast<void*>(owned.Sampler);
    return texture;
}

RHI::Texture VulkanContext::CreateTexture3D(const RHI::Texture3DDesc& desc) {
    if (api_.Device == VK_NULL_HANDLE || api_.PhysicalDevice == VK_NULL_HANDLE || api_.GraphicsQueue == VK_NULL_HANDLE ||
        api_.CommandPool == VK_NULL_HANDLE || desc.width == 0U || desc.height == 0U || desc.depth == 0U ||
        desc.pixels.empty()) {
        return {};
    }
    if (desc.format != RHI::Format::R8G8B8A8_UNorm) {
        return {};
    }

    const VkDeviceSize totalSize =
        static_cast<VkDeviceSize>(desc.width) * static_cast<VkDeviceSize>(desc.height) * static_cast<VkDeviceSize>(desc.depth) * 4U;

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    {
        const VkBufferCreateInfo bufferInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = totalSize,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr
        };
        if (vkCreateBuffer(api_.Device, &bufferInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
            return {};
        }
        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(api_.Device, stagingBuffer, &requirements);
        const std::uint32_t memoryType = FindMemoryType(
            requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        if (memoryType == UINT32_MAX) {
            vkDestroyBuffer(api_.Device, stagingBuffer, nullptr);
            return {};
        }
        const VkMemoryAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .allocationSize = requirements.size,
            .memoryTypeIndex = memoryType
        };
        if (vkAllocateMemory(api_.Device, &allocInfo, nullptr, &stagingMemory) != VK_SUCCESS) {
            vkDestroyBuffer(api_.Device, stagingBuffer, nullptr);
            return {};
        }
        vkBindBufferMemory(api_.Device, stagingBuffer, stagingMemory, 0);
        void* mapped = nullptr;
        if (vkMapMemory(api_.Device, stagingMemory, 0, totalSize, 0, &mapped) != VK_SUCCESS) {
            vkDestroyBuffer(api_.Device, stagingBuffer, nullptr);
            vkFreeMemory(api_.Device, stagingMemory, nullptr);
            return {};
        }
        std::memcpy(mapped, desc.pixels.data(), static_cast<std::size_t>(totalSize));
        vkUnmapMemory(api_.Device, stagingMemory);
    }

    OwnedCubeTexture owned{};
    {
        const VkImageCreateInfo imageInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .imageType = VK_IMAGE_TYPE_3D,
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .extent = {desc.width, desc.height, desc.depth},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };
        if (vkCreateImage(api_.Device, &imageInfo, nullptr, &owned.Image) != VK_SUCCESS) {
            vkDestroyBuffer(api_.Device, stagingBuffer, nullptr);
            vkFreeMemory(api_.Device, stagingMemory, nullptr);
            return {};
        }
        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(api_.Device, owned.Image, &requirements);
        const std::uint32_t memoryType = FindMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (memoryType == UINT32_MAX) {
            vkDestroyImage(api_.Device, owned.Image, nullptr);
            vkDestroyBuffer(api_.Device, stagingBuffer, nullptr);
            vkFreeMemory(api_.Device, stagingMemory, nullptr);
            return {};
        }
        const VkMemoryAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .allocationSize = requirements.size,
            .memoryTypeIndex = memoryType
        };
        if (vkAllocateMemory(api_.Device, &allocInfo, nullptr, &owned.Memory) != VK_SUCCESS) {
            vkDestroyImage(api_.Device, owned.Image, nullptr);
            vkDestroyBuffer(api_.Device, stagingBuffer, nullptr);
            vkFreeMemory(api_.Device, stagingMemory, nullptr);
            return {};
        }
        vkBindImageMemory(api_.Device, owned.Image, owned.Memory, 0);
    }

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    {
        const VkCommandBufferAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = api_.CommandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };
        if (vkAllocateCommandBuffers(api_.Device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
            vkDestroyBuffer(api_.Device, stagingBuffer, nullptr);
            vkFreeMemory(api_.Device, stagingMemory, nullptr);
            vkDestroyImage(api_.Device, owned.Image, nullptr);
            vkFreeMemory(api_.Device, owned.Memory, nullptr);
            return {};
        }
        const VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr
        };
        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        const VkImageMemoryBarrier toTransfer{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = owned.Image,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        };
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &toTransfer
        );

        const VkBufferImageCopy region{
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
            .imageOffset = {0, 0, 0},
            .imageExtent = {desc.width, desc.height, desc.depth}
        };
        vkCmdCopyBufferToImage(
            commandBuffer,
            stagingBuffer,
            owned.Image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region
        );

        const VkImageMemoryBarrier toShaderRead{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = owned.Image,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        };
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &toShaderRead
        );

        vkEndCommandBuffer(commandBuffer);
        const VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,
            .pWaitDstStageMask = nullptr,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffer,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = nullptr
        };
        if (vkQueueSubmit(api_.GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) == VK_SUCCESS) {
            vkQueueWaitIdle(api_.GraphicsQueue);
        }
        vkFreeCommandBuffers(api_.Device, api_.CommandPool, 1, &commandBuffer);
    }

    vkDestroyBuffer(api_.Device, stagingBuffer, nullptr);
    vkFreeMemory(api_.Device, stagingMemory, nullptr);

    const VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = owned.Image,
        .viewType = VK_IMAGE_VIEW_TYPE_3D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .components = {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY
        },
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    if (vkCreateImageView(api_.Device, &viewInfo, nullptr, &owned.View) != VK_SUCCESS) {
        vkDestroyImage(api_.Device, owned.Image, nullptr);
        vkFreeMemory(api_.Device, owned.Memory, nullptr);
        return {};
    }

    const VkSamplerAddressMode addr = desc.repeat ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    const VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .magFilter = desc.linearFiltering ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
        .minFilter = desc.linearFiltering ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = addr,
        .addressModeV = addr,
        .addressModeW = addr,
        .mipLodBias = 0.0F,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0F,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0F,
        .maxLod = 0.0F,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
        .unnormalizedCoordinates = VK_FALSE
    };
    if (vkCreateSampler(api_.Device, &samplerInfo, nullptr, &owned.Sampler) != VK_SUCCESS) {
        vkDestroyImageView(api_.Device, owned.View, nullptr);
        vkDestroyImage(api_.Device, owned.Image, nullptr);
        vkFreeMemory(api_.Device, owned.Memory, nullptr);
        return {};
    }

    owned3DTextures_[owned.View] = owned;

    RHI::Texture texture{};
    texture.width = desc.width;
    texture.height = desc.height;
    texture.depth = desc.depth;
    texture.format = desc.format;
    texture.type = RHI::TextureType::Texture3D;
    texture.graphic_handle_ptr = reinterpret_cast<void*>(owned.View);
    texture.sampler_handle_ptr = reinterpret_cast<void*>(owned.Sampler);
    return texture;
}

RHI::Texture VulkanContext::CreateTextureCube(const RHI::CubemapDesc& desc) {
    if (api_.Device == VK_NULL_HANDLE || api_.PhysicalDevice == VK_NULL_HANDLE || api_.GraphicsQueue == VK_NULL_HANDLE ||
        api_.CommandPool == VK_NULL_HANDLE || desc.width == 0U || desc.height == 0U) {
        return {};
    }
    for (const auto& face : desc.faces) {
        if (face.empty()) {
            return {};
        }
    }

    const VkDeviceSize faceSize = static_cast<VkDeviceSize>(desc.width) * static_cast<VkDeviceSize>(desc.height) * 4U;
    const VkDeviceSize totalSize = faceSize * 6U;

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    {
        const VkBufferCreateInfo bufferInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = totalSize,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr
        };
        if (vkCreateBuffer(api_.Device, &bufferInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
            return {};
        }
        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(api_.Device, stagingBuffer, &requirements);
        const std::uint32_t memoryType = FindMemoryType(
            requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        if (memoryType == UINT32_MAX) {
            vkDestroyBuffer(api_.Device, stagingBuffer, nullptr);
            return {};
        }
        const VkMemoryAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .allocationSize = requirements.size,
            .memoryTypeIndex = memoryType
        };
        if (vkAllocateMemory(api_.Device, &allocInfo, nullptr, &stagingMemory) != VK_SUCCESS) {
            vkDestroyBuffer(api_.Device, stagingBuffer, nullptr);
            return {};
        }
        vkBindBufferMemory(api_.Device, stagingBuffer, stagingMemory, 0);
        void* mapped = nullptr;
        if (vkMapMemory(api_.Device, stagingMemory, 0, totalSize, 0, &mapped) != VK_SUCCESS) {
            vkDestroyBuffer(api_.Device, stagingBuffer, nullptr);
            vkFreeMemory(api_.Device, stagingMemory, nullptr);
            return {};
        }
        auto* dst = static_cast<std::uint8_t*>(mapped);
        for (std::uint32_t face = 0; face < 6U; ++face) {
            std::memcpy(dst + (faceSize * face), desc.faces[face].data(), static_cast<std::size_t>(faceSize));
        }
        vkUnmapMemory(api_.Device, stagingMemory);
    }

    OwnedCubeTexture owned{};
    {
        const VkImageCreateInfo imageInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .extent = {desc.width, desc.height, 1},
            .mipLevels = 1,
            .arrayLayers = 6,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };
        if (vkCreateImage(api_.Device, &imageInfo, nullptr, &owned.Image) != VK_SUCCESS) {
            vkDestroyBuffer(api_.Device, stagingBuffer, nullptr);
            vkFreeMemory(api_.Device, stagingMemory, nullptr);
            return {};
        }
        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(api_.Device, owned.Image, &requirements);
        const std::uint32_t memoryType = FindMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (memoryType == UINT32_MAX) {
            vkDestroyImage(api_.Device, owned.Image, nullptr);
            vkDestroyBuffer(api_.Device, stagingBuffer, nullptr);
            vkFreeMemory(api_.Device, stagingMemory, nullptr);
            return {};
        }
        const VkMemoryAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .allocationSize = requirements.size,
            .memoryTypeIndex = memoryType
        };
        if (vkAllocateMemory(api_.Device, &allocInfo, nullptr, &owned.Memory) != VK_SUCCESS) {
            vkDestroyImage(api_.Device, owned.Image, nullptr);
            vkDestroyBuffer(api_.Device, stagingBuffer, nullptr);
            vkFreeMemory(api_.Device, stagingMemory, nullptr);
            return {};
        }
        vkBindImageMemory(api_.Device, owned.Image, owned.Memory, 0);
    }

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    {
        const VkCommandBufferAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = api_.CommandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };
        if (vkAllocateCommandBuffers(api_.Device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
            vkDestroyBuffer(api_.Device, stagingBuffer, nullptr);
            vkFreeMemory(api_.Device, stagingMemory, nullptr);
            vkDestroyImage(api_.Device, owned.Image, nullptr);
            vkFreeMemory(api_.Device, owned.Memory, nullptr);
            return {};
        }
        const VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr
        };
        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        const VkImageMemoryBarrier toTransfer{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = owned.Image,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6}
        };
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &toTransfer
        );

        std::array<VkBufferImageCopy, 6> regions{};
        for (std::uint32_t face = 0; face < 6U; ++face) {
            regions[face] = VkBufferImageCopy{
                .bufferOffset = faceSize * face,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, face, 1},
                .imageOffset = {0, 0, 0},
                .imageExtent = {desc.width, desc.height, 1}
            };
        }
        vkCmdCopyBufferToImage(
            commandBuffer,
            stagingBuffer,
            owned.Image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<std::uint32_t>(regions.size()),
            regions.data()
        );

        const VkImageMemoryBarrier toShader{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = owned.Image,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6}
        };
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &toShader
        );

        vkEndCommandBuffer(commandBuffer);
        const VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,
            .pWaitDstStageMask = nullptr,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffer,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = nullptr
        };
        vkQueueSubmit(api_.GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(api_.GraphicsQueue);
        vkFreeCommandBuffers(api_.Device, api_.CommandPool, 1, &commandBuffer);
    }

    vkDestroyBuffer(api_.Device, stagingBuffer, nullptr);
    vkFreeMemory(api_.Device, stagingMemory, nullptr);

    const VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = owned.Image,
        .viewType = VK_IMAGE_VIEW_TYPE_CUBE,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .components = {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6}
    };
    if (vkCreateImageView(api_.Device, &viewInfo, nullptr, &owned.View) != VK_SUCCESS) {
        vkDestroyImage(api_.Device, owned.Image, nullptr);
        vkFreeMemory(api_.Device, owned.Memory, nullptr);
        return {};
    }

    const VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .magFilter = desc.linearFiltering ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
        .minFilter = desc.linearFiltering ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias = 0.0F,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0F,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0F,
        .maxLod = 0.0F,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
        .unnormalizedCoordinates = VK_FALSE
    };
    if (vkCreateSampler(api_.Device, &samplerInfo, nullptr, &owned.Sampler) != VK_SUCCESS) {
        vkDestroyImageView(api_.Device, owned.View, nullptr);
        vkDestroyImage(api_.Device, owned.Image, nullptr);
        vkFreeMemory(api_.Device, owned.Memory, nullptr);
        return {};
    }

    ownedCubeTextures_[owned.View] = owned;

    RHI::Texture texture{};
    texture.width = desc.width;
    texture.height = desc.height;
    texture.format = desc.format;
    texture.type = RHI::TextureType::TextureCube;
    texture.graphic_handle_ptr = reinterpret_cast<void*>(owned.View);
    texture.sampler_handle_ptr = reinterpret_cast<void*>(owned.Sampler);
    return texture;
}

void VulkanContext::DestroyTexture(RHI::Texture& texture) {
    if (api_.Device == VK_NULL_HANDLE || texture.graphic_handle_ptr == nullptr) {
        texture = {};
        return;
    }

    // Textures/samplers can be referenced by in-flight descriptor sets/command buffers.
    // Waiting here keeps higher-level resource teardown simple and avoids use-after-free validation errors.
    vkDeviceWaitIdle(api_.Device);

    const auto view = reinterpret_cast<VkImageView>(texture.graphic_handle_ptr);
    if (const auto it2D = owned2DTextures_.find(view); it2D != owned2DTextures_.end()) {
        if (it2D->second.Sampler != VK_NULL_HANDLE) {
            vkDestroySampler(api_.Device, it2D->second.Sampler, nullptr);
        }
        if (it2D->second.View != VK_NULL_HANDLE) {
            vkDestroyImageView(api_.Device, it2D->second.View, nullptr);
        }
        if (it2D->second.Image != VK_NULL_HANDLE) {
            vkDestroyImage(api_.Device, it2D->second.Image, nullptr);
        }
        if (it2D->second.Memory != VK_NULL_HANDLE) {
            vkFreeMemory(api_.Device, it2D->second.Memory, nullptr);
        }
        owned2DTextures_.erase(it2D);
    } else if (const auto it3D = owned3DTextures_.find(view); it3D != owned3DTextures_.end()) {
        if (it3D->second.Sampler != VK_NULL_HANDLE) {
            vkDestroySampler(api_.Device, it3D->second.Sampler, nullptr);
        }
        if (it3D->second.View != VK_NULL_HANDLE) {
            vkDestroyImageView(api_.Device, it3D->second.View, nullptr);
        }
        if (it3D->second.Image != VK_NULL_HANDLE) {
            vkDestroyImage(api_.Device, it3D->second.Image, nullptr);
        }
        if (it3D->second.Memory != VK_NULL_HANDLE) {
            vkFreeMemory(api_.Device, it3D->second.Memory, nullptr);
        }
        owned3DTextures_.erase(it3D);
    } else if (const auto it = ownedCubeTextures_.find(view); it != ownedCubeTextures_.end()) {
        if (it->second.Sampler != VK_NULL_HANDLE) {
            vkDestroySampler(api_.Device, it->second.Sampler, nullptr);
        }
        if (it->second.View != VK_NULL_HANDLE) {
            vkDestroyImageView(api_.Device, it->second.View, nullptr);
        }
        if (it->second.Image != VK_NULL_HANDLE) {
            vkDestroyImage(api_.Device, it->second.Image, nullptr);
        }
        if (it->second.Memory != VK_NULL_HANDLE) {
            vkFreeMemory(api_.Device, it->second.Memory, nullptr);
        }
        ownedCubeTextures_.erase(it);
    }
    texture = {};
}

void VulkanContext::BindTexture(const RHI::u32 slot, const RHI::Texture& texture) {
    textureSlots_[slot] = texture;
}

void* VulkanContext::GetDefaultRenderPassHandle() const {
    return reinterpret_cast<void*>(api_.RenderPass);
}

void* VulkanContext::GetDefaultFramebufferHandle() const {
    return reinterpret_cast<void*>(api_.Framebuffer);
}

void VulkanContext::Initialize(const RHI::u32 width, const RHI::u32 height) {
    EnsureApiBootstrap();
    const bool sizeChanged = api_.SurfaceExtent.width != width || api_.SurfaceExtent.height != height;
    api_.SurfaceExtent = {width, height};
    InitializeBackendObjects();
    if (api_.Swapchain == VK_NULL_HANDLE || api_.SwapchainFramebuffers.empty()) {
        static_cast<void>(RecreateSurfaceAndSwapchain());
    }
    if (sizeChanged) {
        pendingSwapchainRebuild_ = true;
    }
    if (renderer_ == nullptr) {
        renderer_ = std::make_unique<::Lvs::Engine::Rendering::Renderer>();
    }
    renderer_->Initialize(*this, ::Lvs::Engine::Rendering::RenderSurface{width, height});
    frameIndex_ = 0;
}

void VulkanContext::Render(const ::Lvs::Engine::Rendering::SceneData& sceneData) {
    if (renderer_ == nullptr) {
        return;
    }

    if (api_.Instance == VK_NULL_HANDLE || api_.Device == VK_NULL_HANDLE || api_.CommandPool == VK_NULL_HANDLE) {
        try {
            EnsureApiBootstrap();
            InitializeBackendObjects();
        } catch (const std::exception&) {
            return;
        }
    }

    if (api_.Swapchain == VK_NULL_HANDLE || api_.SwapchainFramebuffers.empty()) {
        if (!RecreateSurfaceAndSwapchain()) {
            return;
        }
    }
    if (pendingSwapchainRebuild_) {
        if (api_.SurfaceExtent.width == 0U || api_.SurfaceExtent.height == 0U) {
            return;
        }
        vkDeviceWaitIdle(api_.Device);
        RecreateSwapchain();
        RecreateDepthAttachment();
        RecreateFramebuffer();
        RecreateRenderFinishedSemaphores();
        pendingSwapchainRebuild_ = false;
        if (api_.Swapchain == VK_NULL_HANDLE || api_.SwapchainFramebuffers.empty()) {
            static_cast<void>(RecreateSurfaceAndSwapchain());
            if (api_.Swapchain == VK_NULL_HANDLE || api_.SwapchainFramebuffers.empty()) {
                return;
            }
        }
    }

    if (api_.Device == VK_NULL_HANDLE || imageAvailableSemaphore_ == VK_NULL_HANDLE || renderFinishedSemaphores_.empty() ||
        inFlightFence_ == VK_NULL_HANDLE) {
        return;
    }

    const auto recreateInFlightFenceSignaled = [this]() {
        if (api_.Device == VK_NULL_HANDLE) {
            return;
        }
        if (inFlightFence_ != VK_NULL_HANDLE) {
            vkDestroyFence(api_.Device, inFlightFence_, nullptr);
            inFlightFence_ = VK_NULL_HANDLE;
        }
        const VkFenceCreateInfo fenceInfo{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };
        vkCreateFence(api_.Device, &fenceInfo, nullptr, &inFlightFence_);
    };

    if (inFlightFence_ != VK_NULL_HANDLE) {
        const VkResult waitResult = vkWaitForFences(api_.Device, 1, &inFlightFence_, VK_TRUE, 100'000'000ULL);
        if (waitResult == VK_TIMEOUT) {
            // Recover from a potentially stuck unsignaled fence to avoid
            // permanent frozen output after backend/API switches.
            recreateInFlightFenceSignaled();
            return;
        }
        if (waitResult != VK_SUCCESS) {
            recreateInFlightFenceSignaled();
            return;
        }
    }

    auto cmd = AllocateCommandBuffer();
    auto* vkCmd = dynamic_cast<VulkanCommandBuffer*>(cmd.release());
    if (vkCmd == nullptr) {
        return;
    }
    cmdBuffer_.reset(vkCmd);
    const VkCommandBuffer handle = cmdBuffer_->GetHandle();
    if (handle == VK_NULL_HANDLE) {
        return;
    }

    std::uint32_t imageIndex = 0;
    const VkResult acquireResult = vkAcquireNextImageKHR(
        api_.Device,
        api_.Swapchain,
        1'000'000'000ULL,
        imageAvailableSemaphore_,
        VK_NULL_HANDLE,
        &imageIndex
    );
    if (acquireResult == VK_TIMEOUT || acquireResult == VK_NOT_READY) {
        return;
    }
    if (acquireResult == VK_ERROR_SURFACE_LOST_KHR) {
        static_cast<void>(RecreateSurfaceAndSwapchain());
        return;
    }
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR || acquireResult == VK_SUBOPTIMAL_KHR) {
        RecreateSwapchain();
        RecreateDepthAttachment();
        RecreateFramebuffer();
        RecreateRenderFinishedSemaphores();
        if (api_.Swapchain == VK_NULL_HANDLE || api_.SwapchainFramebuffers.empty()) {
            static_cast<void>(RecreateSurfaceAndSwapchain());
        }
        return;
    }
    if (acquireResult != VK_SUCCESS || imageIndex >= api_.SwapchainFramebuffers.size()) {
        return;
    }
    currentSwapchainImage_ = imageIndex;
    api_.Framebuffer = api_.SwapchainFramebuffers[currentSwapchainImage_];
    if (currentSwapchainImage_ >= renderFinishedSemaphores_.size()) {
        RecreateRenderFinishedSemaphores();
        if (currentSwapchainImage_ >= renderFinishedSemaphores_.size()) {
            return;
        }
    }
    const VkSemaphore renderFinishedSemaphore = renderFinishedSemaphores_[currentSwapchainImage_];
    if (renderFinishedSemaphore == VK_NULL_HANDLE) {
        return;
    }

    renderer_->RecordFrameCommands(*this, *cmdBuffer_, sceneData, frameIndex_++);

    const VkResult endResult = vkEndCommandBuffer(handle);
    if (endResult != VK_SUCCESS || api_.GraphicsQueue == VK_NULL_HANDLE || api_.PresentQueue == VK_NULL_HANDLE) {
        static_cast<void>(RecreateSurfaceAndSwapchain());
        if (endResult != VK_SUCCESS) {
        } else {
        }
        return;
    }

    {
        const VkResult resetFenceResult = vkResetFences(api_.Device, 1, &inFlightFence_);
        if (resetFenceResult != VK_SUCCESS) {
            recreateInFlightFenceSignaled();
            return;
        }
        const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        const VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &imageAvailableSemaphore_,
            .pWaitDstStageMask = &waitStage,
            .commandBufferCount = 1,
            .pCommandBuffers = &handle,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &renderFinishedSemaphore
        };
        const VkResult submitResult = vkQueueSubmit(api_.GraphicsQueue, 1, &submitInfo, inFlightFence_);
        if (submitResult == VK_SUCCESS) {
            const VkPresentInfoKHR presentInfo{
                .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                .pNext = nullptr,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &renderFinishedSemaphore,
                .swapchainCount = 1,
                .pSwapchains = &api_.Swapchain,
                .pImageIndices = &currentSwapchainImage_,
                .pResults = nullptr
            };
            const VkResult presentResult = vkQueuePresentKHR(api_.PresentQueue, &presentInfo);
            if (presentResult == VK_ERROR_SURFACE_LOST_KHR) {
                static_cast<void>(RecreateSurfaceAndSwapchain());
            } else if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
                vkDeviceWaitIdle(api_.Device);
                RecreateSwapchain();
                RecreateDepthAttachment();
                RecreateFramebuffer();
                RecreateRenderFinishedSemaphores();
                if (api_.Swapchain == VK_NULL_HANDLE || api_.SwapchainFramebuffers.empty()) {
                    static_cast<void>(RecreateSurfaceAndSwapchain());
                }
            } else if (presentResult != VK_SUCCESS) {
            } else {
            }
        } else {
            recreateInFlightFenceSignaled();
            static_cast<void>(RecreateSurfaceAndSwapchain());
        }
    }
}

void VulkanContext::FreeCommandBuffer(const VkCommandBuffer commandBuffer) {
    if (api_.Device == VK_NULL_HANDLE || api_.CommandPool == VK_NULL_HANDLE || commandBuffer == VK_NULL_HANDLE) {
        return;
    }
    vkFreeCommandBuffers(api_.Device, api_.CommandPool, 1, &commandBuffer);
}

void VulkanContext::BeginRenderPass(const VkCommandBuffer commandBuffer, const RHI::RenderPassInfo& info) const {
    if (commandBuffer == VK_NULL_HANDLE) {
        return;
    }
    const VkRenderPass renderPass =
        info.renderPassHandle != nullptr ? reinterpret_cast<VkRenderPass>(info.renderPassHandle) : api_.RenderPass;
    const VkFramebuffer framebuffer =
        info.framebufferHandle != nullptr ? reinterpret_cast<VkFramebuffer>(info.framebufferHandle) : api_.Framebuffer;
    if (renderPass == VK_NULL_HANDLE || framebuffer == VK_NULL_HANDLE) {
        return;
    }
    const VkExtent2D extent = (info.width > 0U && info.height > 0U)
                                  ? VkExtent2D{info.width, info.height}
                                  : api_.SurfaceExtent;

    const VkRenderPassBeginInfo passInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = renderPass,
        .framebuffer = framebuffer,
        .renderArea = {.offset = {0, 0}, .extent = extent},
        .clearValueCount = 0,
        .pClearValues = nullptr
    };
    vkCmdBeginRenderPass(commandBuffer, &passInfo, VK_SUBPASS_CONTENTS_INLINE);

    const VkViewport viewport{
        .x = 0.0F,
        .y = 0.0F,
        .width = static_cast<float>(extent.width),
        .height = static_cast<float>(extent.height),
        .minDepth = 0.0F,
        .maxDepth = 1.0F
    };
    const VkRect2D scissor{
        .offset = {0, 0},
        .extent = extent
    };
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    if (info.clearColor || info.clearDepth) {
        std::vector<VkClearAttachment> clearAttachments{};
        clearAttachments.reserve(info.colorAttachmentCount + (info.clearDepth ? 1U : 0U));
        if (info.clearColor && info.colorAttachmentCount > 0U) {
            for (std::uint32_t colorIndex = 0; colorIndex < info.colorAttachmentCount; ++colorIndex) {
                const bool isPrimaryColor = colorIndex == 0U;
                clearAttachments.push_back(VkClearAttachment{
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .colorAttachment = colorIndex,
                    .clearValue = {.color = {
                        {
                            isPrimaryColor ? info.clearColorValue[0] : 0.0F,
                            isPrimaryColor ? info.clearColorValue[1] : 0.0F,
                            isPrimaryColor ? info.clearColorValue[2] : 0.0F,
                            isPrimaryColor ? info.clearColorValue[3] : 0.0F
                        }
                    }}
                });
            }
        }
        if (info.clearDepth) {
            clearAttachments.push_back(VkClearAttachment{
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .colorAttachment = 0,
                .clearValue = {.depthStencil = {.depth = info.clearDepthValue, .stencil = 0}}
            });
        }
        if (clearAttachments.empty()) {
            return;
        }
        const VkClearRect clearRect{
            .rect = {.offset = {0, 0}, .extent = extent},
            .baseArrayLayer = 0,
            .layerCount = 1
        };
        vkCmdClearAttachments(
            commandBuffer,
            static_cast<std::uint32_t>(clearAttachments.size()),
            clearAttachments.data(),
            1,
            &clearRect
        );
    }
}

void VulkanContext::EndRenderPass(const VkCommandBuffer commandBuffer) const {
    if (commandBuffer != VK_NULL_HANDLE) {
        vkCmdEndRenderPass(commandBuffer);
    }
}

void VulkanContext::BindPipeline(const VkCommandBuffer commandBuffer, const RHI::IPipeline& pipeline) const {
    if (commandBuffer == VK_NULL_HANDLE || pipeline.GetNativeHandle() == nullptr) {
        return;
    }
    vkCmdBindPipeline(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        reinterpret_cast<VkPipeline>(pipeline.GetNativeHandle())
    );
}

void VulkanContext::BindVertexBuffer(
    const VkCommandBuffer commandBuffer,
    const RHI::u32 slot,
    const RHI::IBuffer& buffer,
    const std::size_t offset
) const {
    if (commandBuffer == VK_NULL_HANDLE || buffer.GetNativeHandle() == nullptr) {
        return;
    }
    const VkBuffer vkBuffer = reinterpret_cast<VkBuffer>(buffer.GetNativeHandle());
    const VkDeviceSize vkOffset = static_cast<VkDeviceSize>(offset);
    vkCmdBindVertexBuffers(commandBuffer, slot, 1, &vkBuffer, &vkOffset);
}

void VulkanContext::BindIndexBuffer(
    const VkCommandBuffer commandBuffer,
    const RHI::IBuffer& buffer,
    const RHI::IndexType indexType,
    const std::size_t offset
) const {
    if (commandBuffer == VK_NULL_HANDLE || buffer.GetNativeHandle() == nullptr) {
        return;
    }
    const VkBuffer vkBuffer = reinterpret_cast<VkBuffer>(buffer.GetNativeHandle());
    const VkIndexType vkIndexType = indexType == RHI::IndexType::UInt16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
    vkCmdBindIndexBuffer(commandBuffer, vkBuffer, static_cast<VkDeviceSize>(offset), vkIndexType);
}

void VulkanContext::BindResourceSet(
    const VkCommandBuffer commandBuffer,
    const RHI::u32 slot,
    const RHI::IResourceSet& set
) const {
    if (commandBuffer == VK_NULL_HANDLE || api_.PipelineLayout == VK_NULL_HANDLE || set.GetNativeHandle() == nullptr) {
        return;
    }
    const VkDescriptorSet descriptorSet = reinterpret_cast<VkDescriptorSet>(set.GetNativeHandle());
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        api_.PipelineLayout,
        slot,
        1,
        &descriptorSet,
        0,
        nullptr
    );
}

void VulkanContext::PushConstants(const VkCommandBuffer commandBuffer, const void* data, const std::size_t size) const {
    if (commandBuffer == VK_NULL_HANDLE || api_.PipelineLayout == VK_NULL_HANDLE || data == nullptr || size == 0) {
        return;
    }
    const std::uint32_t clampedSize = static_cast<std::uint32_t>(
        std::min<std::size_t>(size, sizeof(Common::DrawPushConstants))
    );
    vkCmdPushConstants(
        commandBuffer,
        api_.PipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        clampedSize,
        data
    );
}

void VulkanContext::Draw(const VkCommandBuffer commandBuffer, const RHI::u32 vertexCount) const {
    if (commandBuffer == VK_NULL_HANDLE || vertexCount == 0) {
        return;
    }
    vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
}

void VulkanContext::DrawIndexed(const VkCommandBuffer commandBuffer, const RHI::u32 indexCount) const {
    if (commandBuffer == VK_NULL_HANDLE || indexCount == 0) {
        return;
    }
    vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
}

} // namespace Lvs::Engine::Rendering::Backends::Vulkan
