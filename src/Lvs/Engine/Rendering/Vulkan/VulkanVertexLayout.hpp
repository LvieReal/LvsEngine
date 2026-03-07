#pragma once

#include "Lvs/Engine/Rendering/Common/Vertex.hpp"

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace Lvs::Engine::Rendering::Vulkan {

struct VulkanVertexLayout {
    static VkVertexInputBindingDescription BindingDescription() {
        return VkVertexInputBindingDescription{
            .binding = 0,
            .stride = sizeof(Common::Vertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        };
    }

    static std::array<VkVertexInputAttributeDescription, 2> AttributeDescriptions() {
        return {
            VkVertexInputAttributeDescription{
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = static_cast<std::uint32_t>(offsetof(Common::Vertex, Position))
            },
            VkVertexInputAttributeDescription{
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = static_cast<std::uint32_t>(offsetof(Common::Vertex, Normal))
            }
        };
    }
};

} // namespace Lvs::Engine::Rendering::Vulkan
