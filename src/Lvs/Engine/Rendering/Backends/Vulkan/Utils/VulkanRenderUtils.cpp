#include "Lvs/Engine/Rendering/Backends/Vulkan/Utils/VulkanRenderUtils.hpp"

#include <array>
#include <cstring>
#include <iostream>
#include <vector>

namespace Lvs::Engine::Rendering::Backends::Vulkan::Utils {

VkCullModeFlags ResolveCullMode(const RHI::PipelineDesc& desc) {
    switch (desc.cullMode) {
        case RHI::CullMode::None:
            return VK_CULL_MODE_NONE;
        case RHI::CullMode::Front:
            return VK_CULL_MODE_FRONT_BIT;
        case RHI::CullMode::Back:
        default:
            return VK_CULL_MODE_BACK_BIT;
    }
}

VkSampleCountFlagBits ResolveSampleCount(const VkPhysicalDevice physicalDevice, const RHI::u32 sampleCount) {
    if (sampleCount <= 1U || physicalDevice == VK_NULL_HANDLE) {
        return VK_SAMPLE_COUNT_1_BIT;
    }
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);
    const VkSampleCountFlags counts =
        properties.limits.framebufferColorSampleCounts & properties.limits.framebufferDepthSampleCounts;
    const auto supports = [&](const VkSampleCountFlagBits bit) { return (counts & bit) == bit; };
    if (sampleCount >= 8U && supports(VK_SAMPLE_COUNT_8_BIT)) {
        return VK_SAMPLE_COUNT_8_BIT;
    }
    if (sampleCount >= 4U && supports(VK_SAMPLE_COUNT_4_BIT)) {
        return VK_SAMPLE_COUNT_4_BIT;
    }
    if (sampleCount >= 2U && supports(VK_SAMPLE_COUNT_2_BIT)) {
        return VK_SAMPLE_COUNT_2_BIT;
    }
    return VK_SAMPLE_COUNT_1_BIT;
}

VkCompareOp ResolveDepthCompare(const RHI::DepthCompare compare) {
    switch (compare) {
        case RHI::DepthCompare::Always:
            return VK_COMPARE_OP_ALWAYS;
        case RHI::DepthCompare::Equal:
            return VK_COMPARE_OP_EQUAL;
        case RHI::DepthCompare::LessOrEqual:
            return VK_COMPARE_OP_LESS_OR_EQUAL;
        case RHI::DepthCompare::GreaterOrEqual:
        default:
            return VK_COMPARE_OP_GREATER_OR_EQUAL;
    }
}

bool HasInstanceLayer(const char* layerName) {
    std::uint32_t layerCount = 0;
    if (vkEnumerateInstanceLayerProperties(&layerCount, nullptr) != VK_SUCCESS || layerCount == 0) {
        return false;
    }
    std::vector<VkLayerProperties> layers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, layers.data());
    for (const auto& layer : layers) {
        if (std::strcmp(layer.layerName, layerName) == 0) {
            return true;
        }
    }
    return false;
}

bool HasInstanceExtension(const char* extensionName) {
    std::uint32_t extensionCount = 0;
    if (vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr) != VK_SUCCESS || extensionCount == 0) {
        return false;
    }
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
    for (const auto& extension : extensions) {
        if (std::strcmp(extension.extensionName, extensionName) == 0) {
            return true;
        }
    }
    return false;
}

VkImageAspectFlags DepthAspectMaskForFormat(const VkFormat format) {
    switch (format) {
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        default:
            return VK_IMAGE_ASPECT_DEPTH_BIT;
    }
}

VkResult CreateDebugUtilsMessenger(
    const VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* createInfo,
    VkDebugUtilsMessengerEXT* messenger
) {
    const auto fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT")
    );
    if (fn == nullptr) {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    return fn(instance, createInfo, nullptr, messenger);
}

void DestroyDebugUtilsMessenger(const VkInstance instance, const VkDebugUtilsMessengerEXT messenger) {
    const auto fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT")
    );
    if (fn != nullptr) {
        fn(instance, messenger, nullptr);
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL DebugMessageCallback(
    const VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void*
) {
    const char* prefix = "[Vulkan][Info]";
    if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0U) {
        prefix = "[Vulkan][Error]";
    } else if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0U) {
        prefix = "[Vulkan][Warning]";
    } else if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) != 0U) {
        prefix = "[Vulkan][Verbose]";
    }
    std::cerr << prefix << " " << (callbackData != nullptr ? callbackData->pMessage : "Unknown") << std::endl;
    return VK_FALSE;
}

std::uint32_t SelectVulkanApiVersion() {
    std::uint32_t loaderVersion = VK_API_VERSION_1_0;
    if (vkEnumerateInstanceVersion(&loaderVersion) != VK_SUCCESS) {
        loaderVersion = VK_API_VERSION_1_0;
    }

    const std::array<std::uint32_t, 4> candidates{
        VK_API_VERSION_1_3,
        VK_API_VERSION_1_2,
        VK_API_VERSION_1_1,
        VK_API_VERSION_1_0
    };
    for (const auto candidate : candidates) {
        if (VK_API_VERSION_MAJOR(loaderVersion) > VK_API_VERSION_MAJOR(candidate) ||
            (VK_API_VERSION_MAJOR(loaderVersion) == VK_API_VERSION_MAJOR(candidate) &&
             VK_API_VERSION_MINOR(loaderVersion) >= VK_API_VERSION_MINOR(candidate))) {
            return candidate;
        }
    }
    return VK_API_VERSION_1_0;
}

bool SupportsVulkanRuntime() {
    std::uint32_t loaderVersion = VK_API_VERSION_1_0;
    if (vkEnumerateInstanceVersion(&loaderVersion) != VK_SUCCESS) {
        return false;
    }

    const std::array<std::uint32_t, 4> candidates{
        VK_API_VERSION_1_3,
        VK_API_VERSION_1_2,
        VK_API_VERSION_1_1,
        VK_API_VERSION_1_0
    };
    for (const auto candidate : candidates) {
        if (VK_API_VERSION_MAJOR(loaderVersion) > VK_API_VERSION_MAJOR(candidate) ||
            (VK_API_VERSION_MAJOR(loaderVersion) == VK_API_VERSION_MAJOR(candidate) &&
             VK_API_VERSION_MINOR(loaderVersion) >= VK_API_VERSION_MINOR(candidate))) {
            return true;
        }
    }
    return false;
}

} // namespace Lvs::Engine::Rendering::Backends::Vulkan::Utils

