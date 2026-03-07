#pragma once

#include <cstdint>

namespace Lvs::Engine::Rendering::Common {

class CommandBuffer;
class BufferResource;
class PipelineLayout;

class ResourceBinding {
public:
    virtual ~ResourceBinding() = default;

    virtual void Bind(CommandBuffer& commandBuffer, const PipelineLayout& layout, std::uint32_t firstSet = 0) const = 0;
    virtual void UpdateBuffer(std::uint32_t binding, const BufferResource& buffer, std::size_t offset = 0, std::size_t range = 0) = 0;
    virtual void UpdateImage(std::uint32_t binding, void* sampler, void* imageView, std::uint32_t imageLayout) = 0;
    virtual void UpdateImages(
        std::uint32_t binding,
        const void* const* samplers,
        const void* const* imageViews,
        const std::uint32_t* imageLayouts,
        std::uint32_t count
    ) = 0;
};

} // namespace Lvs::Engine::Rendering::Common
