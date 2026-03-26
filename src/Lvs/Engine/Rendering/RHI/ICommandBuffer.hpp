#pragma once

#include "Lvs/Engine/Rendering/RHI/IBuffer.hpp"
#include "Lvs/Engine/Rendering/RHI/IPipeline.hpp"
#include "Lvs/Engine/Rendering/RHI/IResourceSet.hpp"
#include "Lvs/Engine/Rendering/RHI/Types.hpp"

#include <cstdint>

namespace Lvs::Engine::Rendering::RHI {

struct RenderPassInfo {
    u32 width{0};
    u32 height{0};
    u32 colorAttachmentCount{1};
    void* renderPassHandle{nullptr};
    void* framebufferHandle{nullptr};
    bool clearColor{false};
    float clearColorValue[4]{0.0F, 0.0F, 0.0F, 1.0F};
    bool clearDepth{false};
    float clearDepthValue{0.0F};
    bool clearStencil{false};
    u32 clearStencilValue{0U};
};

class ICommandBuffer {
public:
    virtual ~ICommandBuffer() = default;
    virtual void BeginRenderPass(const RenderPassInfo& info) = 0;
    virtual void EndRenderPass() = 0;
    virtual void BindPipeline(const IPipeline& pipeline) = 0;
    virtual void BindVertexBuffer(u32 slot, const IBuffer& buffer, std::size_t offset) = 0;
    virtual void BindIndexBuffer(const IBuffer& buffer, IndexType indexType, std::size_t offset) = 0;
    virtual void BindResourceSet(u32 slot, const IResourceSet& set) = 0;

    enum class PushConstantFieldType : std::uint8_t {
        Float4,
        UInt4,
        Matrix4x4
    };

    struct PushConstantField {
        const char* name{nullptr};
        PushConstantFieldType type{PushConstantFieldType::Float4};
        const void* data{nullptr};
    };

    struct PushConstantsInfo {
        const void* data{nullptr};
        std::size_t size{0};
        const PushConstantField* fields{nullptr};
        std::size_t fieldCount{0};
    };

    virtual void PushConstants(const PushConstantsInfo& info) = 0;

    struct DrawInfo {
        u32 vertexCount{0};
        u32 indexCount{0};
        u32 instanceCount{1};
    };

    virtual void Draw(const DrawInfo& info) = 0;
};

} // namespace Lvs::Engine::Rendering::RHI
