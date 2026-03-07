#include "Lvs/Engine/Rendering/Vulkan/VulkanShaderUtils.hpp"

#include "Lvs/Engine/Utils/FileIO.hpp"

#include <stdexcept>

namespace Lvs::Engine::Rendering::Vulkan::ShaderUtils {

std::vector<char> ReadBinaryFile(const std::filesystem::path& path) {
    return Utils::FileIO::ReadBinaryFile(path);
}

VkShaderModule CreateShaderModule(const VkDevice device, const std::vector<char>& bytecode) {
    const VkShaderModuleCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .codeSize = bytecode.size(),
        .pCode = reinterpret_cast<const std::uint32_t*>(bytecode.data())
    };

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan shader module.");
    }
    return shaderModule;
}

} // namespace Lvs::Engine::Rendering::Vulkan::ShaderUtils
