#pragma once

#include "Lvs/Engine/Rendering/RHI/IBuffer.hpp"
#include "Lvs/Engine/Rendering/RHI/Texture.hpp"
#include "Lvs/Engine/Rendering/RHI/Types.hpp"

namespace Lvs::Engine::Rendering::RHI {

enum class ResourceBindingKind {
    Texture2D,
    Texture3D,
    TextureCube,
    UniformBuffer,
    StorageBuffer
};

struct ResourceBinding {
    u32 slot{0};
    // Optional descriptor array element (for bindless/array bindings).
    // For non-array bindings, this must be 0.
    u32 arrayElement{0};
    ResourceBindingKind kind{ResourceBindingKind::Texture2D};
    Texture texture{};
    const IBuffer* buffer{nullptr};
};

struct ResourceSetDesc {
    const ResourceBinding* bindings{nullptr};
    u32 bindingCount{0};
    void* nativeHandleHint{nullptr};
};

class IResourceSet {
public:
    virtual ~IResourceSet() = default;
    [[nodiscard]] virtual void* GetNativeHandle() const = 0;
};

} // namespace Lvs::Engine::Rendering::RHI
