#pragma once

#include "Lvs/Engine/RenderingV2/RHI/Texture.hpp"
#include "Lvs/Engine/RenderingV2/RHI/Types.hpp"

#include <memory>

namespace Lvs::Engine::RenderingV2::RHI {

struct RenderPassInfo {
    u32 width{0};
    u32 height{0};
    bool clearColor{false};
    float clearColorValue[4]{0.0F, 0.0F, 0.0F, 1.0F};
    bool clearDepth{false};
    float clearDepthValue{1.0F};
};

struct PipelineDesc {
    bool depthTest{true};
    bool depthWrite{true};
    bool blending{false};
};

struct ResourceSet {
    union {
        void* native_handle_ptr;
        int native_handle_i;
    };

    ResourceSet()
        : native_handle_ptr(nullptr) {}
};

class IPipeline {
public:
    virtual ~IPipeline() = default;
};

class ICommandBuffer {
public:
    virtual ~ICommandBuffer() = default;
    virtual void BeginRenderPass(const RenderPassInfo& info) = 0;
    virtual void EndRenderPass() = 0;
    virtual void BindPipeline(const IPipeline& pipeline) = 0;
    virtual void BindResourceSet(u32 slot, const ResourceSet& set) = 0;
    virtual void DrawIndexed(u32 indexCount) = 0;
};

class IContext {
public:
    virtual ~IContext() = default;
    virtual std::unique_ptr<ICommandBuffer> AllocateCommandBuffer() = 0;
    virtual std::unique_ptr<IPipeline> CreatePipeline(const PipelineDesc& desc) = 0;
    virtual void BindTexture(u32 slot, const Texture& texture) = 0;
};

} // namespace Lvs::Engine::RenderingV2::RHI
