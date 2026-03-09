#pragma once

#include "Lvs/Engine/Rendering/RHI/IBuffer.hpp"
#include "Lvs/Engine/Rendering/RHI/ICommandBuffer.hpp"
#include "Lvs/Engine/Rendering/RHI/IPipeline.hpp"
#include "Lvs/Engine/Rendering/RHI/IResourceSet.hpp"
#include "Lvs/Engine/Rendering/RHI/Texture.hpp"
#include "Lvs/Engine/Rendering/RHI/Types.hpp"

#include <memory>

namespace Lvs::Engine::Rendering::RHI {

class IContext {
public:
    virtual ~IContext() = default;
    virtual std::unique_ptr<ICommandBuffer> AllocateCommandBuffer() = 0;
    virtual std::unique_ptr<IPipeline> CreatePipeline(const PipelineDesc& desc) = 0;
    virtual std::unique_ptr<IBuffer> CreateBuffer(const BufferDesc& desc) = 0;
    virtual std::unique_ptr<IResourceSet> CreateResourceSet(const ResourceSetDesc& desc) = 0;
    [[nodiscard]] virtual Texture CreateTextureCube(const CubemapDesc& desc) = 0;
    virtual void DestroyTexture(Texture& texture) = 0;
    virtual void BindTexture(u32 slot, const Texture& texture) = 0;
    [[nodiscard]] virtual void* GetDefaultRenderPassHandle() const = 0;
    [[nodiscard]] virtual void* GetDefaultFramebufferHandle() const = 0;
};

} // namespace Lvs::Engine::Rendering::RHI
