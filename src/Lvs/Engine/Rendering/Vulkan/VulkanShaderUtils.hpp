#pragma once

#include <vulkan/vulkan.h>

#include <filesystem>
#include <vector>

namespace Lvs::Engine::Rendering::Vulkan::ShaderUtils {

std::vector<char> ReadBinaryFile(const std::filesystem::path& path);
VkShaderModule CreateShaderModule(VkDevice device, const std::vector<char>& bytecode);

} // namespace Lvs::Engine::Rendering::Vulkan::ShaderUtils
