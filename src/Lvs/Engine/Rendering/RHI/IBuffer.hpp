#pragma once

#include "Lvs/Engine/Rendering/RHI/Types.hpp"

#include <cstddef>

namespace Lvs::Engine::Rendering::RHI {

enum class BufferType {
    Vertex,
    Index,
    Uniform,
    Staging
};

enum class BufferUsage {
    Static,
    Dynamic
};

enum class IndexType {
    UInt16,
    UInt32
};

struct BufferDesc {
    BufferType type{BufferType::Vertex};
    BufferUsage usage{BufferUsage::Static};
    std::size_t size{0};
    const void* initialData{nullptr};
};

class IBuffer {
public:
    virtual ~IBuffer() = default;
    [[nodiscard]] virtual void* GetNativeHandle() const = 0;
    [[nodiscard]] virtual std::size_t GetSize() const = 0;
};

} // namespace Lvs::Engine::Rendering::RHI
