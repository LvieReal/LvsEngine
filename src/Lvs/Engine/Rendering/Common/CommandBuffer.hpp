#pragma once

#include "Lvs/Engine/Rendering/Common/GpuResource.hpp"

#include <cstddef>
#include <cstdint>

namespace Lvs::Engine::Rendering::Common {

enum class IndexFormat {
    UInt16,
    UInt32
};

class CommandBuffer {
public:
    virtual ~CommandBuffer() = default;

    virtual void BindVertexBuffer(const BufferResource& buffer, std::uint32_t binding = 0, std::size_t offset = 0) = 0;
    virtual void BindIndexBuffer(const BufferResource& buffer, IndexFormat format, std::size_t offset = 0) = 0;
    virtual void DrawIndexed(std::uint32_t indexCount, std::uint32_t instanceCount, std::uint32_t firstIndex = 0) = 0;
};

} // namespace Lvs::Engine::Rendering::Common
