#include "Lvs/Engine/Rendering/Vulkan/VulkanShaderUtils.hpp"

#include <QFile>

#include <stdexcept>

namespace Lvs::Engine::Rendering::Vulkan::ShaderUtils {

std::vector<char> ReadBinaryFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error(QString("Failed to open shader file: %1").arg(path).toStdString());
    }
    const QByteArray data = file.readAll();
    file.close();
    return std::vector<char>(data.begin(), data.end());
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
