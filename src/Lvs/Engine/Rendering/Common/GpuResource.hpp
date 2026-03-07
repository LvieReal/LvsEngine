#pragma once

#include <cstddef>
#include <cstdint>

namespace Lvs::Engine::Rendering::Common {

enum class GpuResourceType {
    Buffer,
    Image,
    Sampler
};

enum class BufferUsage : std::uint32_t {
    None = 0,
    Vertex = 1U << 0,
    Index = 1U << 1,
    Uniform = 1U << 2,
    TransferSource = 1U << 3,
    TransferDestination = 1U << 4
};

constexpr BufferUsage operator|(const BufferUsage lhs, const BufferUsage rhs) {
    return static_cast<BufferUsage>(static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
}

inline bool HasFlag(const BufferUsage value, const BufferUsage flag) {
    return (static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(flag)) != 0U;
}

enum class MemoryUsage {
    CpuVisible,
    DeviceLocal
};

enum class PixelFormat {
    Unknown,
    RGBA8Unorm,
    RGBA8Srgb,
    RGBA16Float,
    RGBA32Float,
    D32Float,
    D32FloatS8,
    D24UnormS8
};

enum class ImageUsage : std::uint32_t {
    None = 0,
    Sampled = 1U << 0,
    ColorAttachment = 1U << 1,
    DepthStencilAttachment = 1U << 2,
    TransferSource = 1U << 3,
    TransferDestination = 1U << 4
};

constexpr ImageUsage operator|(const ImageUsage lhs, const ImageUsage rhs) {
    return static_cast<ImageUsage>(static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
}

inline bool HasFlag(const ImageUsage value, const ImageUsage flag) {
    return (static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(flag)) != 0U;
}

enum class FilterMode {
    Nearest,
    Linear
};

enum class AddressMode {
    ClampToEdge,
    Repeat
};

enum class MipmapMode {
    Nearest,
    Linear
};

class GpuResource {
public:
    virtual ~GpuResource() = default;
    [[nodiscard]] virtual GpuResourceType GetType() const = 0;
    [[nodiscard]] virtual void* GetNativeHandle() const = 0;
};

class BufferResource : public GpuResource {
public:
    [[nodiscard]] GpuResourceType GetType() const final { return GpuResourceType::Buffer; }
    [[nodiscard]] virtual std::size_t GetSize() const = 0;
    virtual void Upload(const void* data, std::size_t size, std::size_t offset = 0) = 0;
};

class ImageResource : public GpuResource {
public:
    [[nodiscard]] GpuResourceType GetType() const final { return GpuResourceType::Image; }
    [[nodiscard]] virtual std::uint32_t GetWidth() const = 0;
    [[nodiscard]] virtual std::uint32_t GetHeight() const = 0;
};

class SamplerResource : public GpuResource {
public:
    [[nodiscard]] GpuResourceType GetType() const final { return GpuResourceType::Sampler; }
};

} // namespace Lvs::Engine::Rendering::Common
