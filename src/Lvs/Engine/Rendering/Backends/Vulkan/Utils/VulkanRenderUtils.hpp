#pragma once

#include "Lvs/Engine/Rendering/RHI/IPipeline.hpp"
#include "Lvs/Engine/Rendering/RHI/Types.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>

namespace Lvs::Engine::Rendering::Backends::Vulkan::Utils {

VkCullModeFlags ResolveCullMode(const RHI::PipelineDesc& desc);
VkSampleCountFlagBits ResolveSampleCount(VkPhysicalDevice physicalDevice, RHI::u32 sampleCount);
VkCompareOp ResolveDepthCompare(RHI::DepthCompare compare);

#if !defined(NDEBUG)
constexpr const char* kValidationLayerName = "VK_LAYER_KHRONOS_validation";
#endif

bool HasInstanceLayer(const char* layerName);
bool HasInstanceExtension(const char* extensionName);
VkImageAspectFlags DepthAspectMaskForFormat(VkFormat format);

VkResult CreateDebugUtilsMessenger(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* createInfo,
    VkDebugUtilsMessengerEXT* messenger
);
void DestroyDebugUtilsMessenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger);

VKAPI_ATTR VkBool32 VKAPI_CALL DebugMessageCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void* userData
);

std::uint32_t SelectVulkanApiVersion();

} // namespace Lvs::Engine::Rendering::Backends::Vulkan::Utils

