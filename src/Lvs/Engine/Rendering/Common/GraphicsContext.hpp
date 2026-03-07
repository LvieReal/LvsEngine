#pragma once

#include "Lvs/Engine/Rendering/Common/GpuResource.hpp"
#include "Lvs/Engine/Rendering/Common/RenderSurface.hpp"

#include <memory>

namespace Lvs::Engine::Rendering::Common {

struct BufferDesc {
    std::size_t Size{0};
    BufferUsage Usage{BufferUsage::None};
    MemoryUsage Memory{MemoryUsage::DeviceLocal};
};

struct ImageDesc {
    std::uint32_t Width{0};
    std::uint32_t Height{0};
    std::uint32_t Depth{1};
    std::uint32_t MipLevels{1};
    std::uint32_t Layers{1};
    PixelFormat Format{PixelFormat::Unknown};
    ImageUsage Usage{ImageUsage::None};
    bool CubeCompatible{false};
};

struct SamplerDesc {
    FilterMode Filter{FilterMode::Linear};
    AddressMode Address{AddressMode::ClampToEdge};
    MipmapMode Mipmap{MipmapMode::Linear};
};

class GraphicsContext {
public:
    virtual ~GraphicsContext() = default;

    [[nodiscard]] virtual std::unique_ptr<BufferResource> CreateBuffer(const BufferDesc& desc) = 0;
    [[nodiscard]] virtual std::unique_ptr<ImageResource> CreateImage(const ImageDesc& desc) = 0;
    [[nodiscard]] virtual std::unique_ptr<SamplerResource> CreateSampler(const SamplerDesc& desc) = 0;
    [[nodiscard]] virtual void* GetNativeDevice() const = 0;
    [[nodiscard]] virtual void* GetNativePhysicalDevice() const = 0;
    [[nodiscard]] virtual void* GetNativeGraphicsQueue() const = 0;
    [[nodiscard]] virtual std::uint32_t GetGraphicsQueueFamily() const = 0;
    [[nodiscard]] virtual Extent2D GetSwapchainExtent() const = 0;
    [[nodiscard]] virtual std::uint32_t GetFramesInFlight() const = 0;
};

} // namespace Lvs::Engine::Rendering::Common
