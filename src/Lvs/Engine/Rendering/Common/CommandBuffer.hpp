#pragma once

#include "Lvs/Engine/Rendering/Common/PipelineLayout.hpp"
#include "Lvs/Engine/Rendering/Common/GpuResource.hpp"

#include <cstddef>
#include <cstdint>

namespace Lvs::Engine::Rendering::Common {

enum class IndexFormat {
    UInt16,
    UInt32
};

enum class ShaderStageFlags : std::uint32_t {
    None = 0,
    Vertex = 1U << 0,
    Fragment = 1U << 1
};

constexpr ShaderStageFlags operator|(const ShaderStageFlags lhs, const ShaderStageFlags rhs) {
    return static_cast<ShaderStageFlags>(static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
}

struct Viewport {
    float X{0.0F};
    float Y{0.0F};
    float Width{0.0F};
    float Height{0.0F};
    float MinDepth{0.0F};
    float MaxDepth{1.0F};
};

struct Rect {
    std::int32_t X{0};
    std::int32_t Y{0};
    std::uint32_t Width{0};
    std::uint32_t Height{0};
};

class DrawPassState {
public:
    virtual ~DrawPassState() = default;
};

class CommandBuffer {
public:
    virtual ~CommandBuffer() = default;
    [[nodiscard]] virtual void* GetNativeHandle() const = 0;

    virtual void BeginDrawPass(const DrawPassState& state) = 0;
    virtual void EndDrawPass() = 0;
    virtual void BindVertexBuffer(const BufferResource& buffer, std::uint32_t binding = 0, std::size_t offset = 0) = 0;
    virtual void BindIndexBuffer(const BufferResource& buffer, IndexFormat format, std::size_t offset = 0) = 0;
    virtual void SetViewport(const Viewport& viewport) = 0;
    virtual void SetScissor(const Rect& scissor) = 0;
    virtual void PushConstants(
        const PipelineLayout& layout,
        ShaderStageFlags stages,
        const void* data,
        std::size_t size,
        std::uint32_t offset = 0
    ) = 0;
    virtual void Draw(std::uint32_t vertexCount, std::uint32_t instanceCount = 1, std::uint32_t firstVertex = 0) = 0;
    virtual void DrawIndexed(std::uint32_t indexCount, std::uint32_t instanceCount, std::uint32_t firstIndex = 0) = 0;
};

} // namespace Lvs::Engine::Rendering::Common
