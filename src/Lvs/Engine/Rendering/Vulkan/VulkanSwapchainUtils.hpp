#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace Lvs::Engine::Rendering::Vulkan::SwapchainUtils {

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR Capabilities{};
    std::vector<VkSurfaceFormatKHR> Formats;
    std::vector<VkPresentModeKHR> PresentModes;
};

SwapchainSupportDetails QuerySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, std::uint32_t width, std::uint32_t height);

} // namespace Lvs::Engine::Rendering::Vulkan::SwapchainUtils
