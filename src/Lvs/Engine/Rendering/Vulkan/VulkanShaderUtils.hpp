#pragma once

#include <vulkan/vulkan.h>

#include <QString>
#include <vector>

namespace Lvs::Engine::Rendering::Vulkan::ShaderUtils {

std::vector<char> ReadBinaryFile(const QString& path);
VkShaderModule CreateShaderModule(VkDevice device, const std::vector<char>& bytecode);

} // namespace Lvs::Engine::Rendering::Vulkan::ShaderUtils
