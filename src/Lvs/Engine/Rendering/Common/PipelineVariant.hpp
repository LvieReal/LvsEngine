#pragma once

#include <cstddef>
#include <functional>

namespace Lvs::Engine::Rendering::Common {

enum class PipelineCullMode {
    None,
    Front,
    Back
};

enum class PipelineDepthMode {
    ReadWrite,
    ReadOnly,
    Disabled
};

enum class PipelineBlendMode {
    Opaque,
    AlphaBlend
};

struct PipelineVariantKey {
    PipelineCullMode CullMode{PipelineCullMode::Back};
    PipelineDepthMode DepthMode{PipelineDepthMode::ReadWrite};
    PipelineBlendMode BlendMode{PipelineBlendMode::Opaque};

    [[nodiscard]] bool operator==(const PipelineVariantKey& other) const {
        return CullMode == other.CullMode && DepthMode == other.DepthMode && BlendMode == other.BlendMode;
    }
};

struct PipelineVariantKeyHash {
    [[nodiscard]] std::size_t operator()(const PipelineVariantKey& key) const noexcept {
        const auto cullHash = std::hash<int>{}(static_cast<int>(key.CullMode));
        const auto depthHash = std::hash<int>{}(static_cast<int>(key.DepthMode));
        const auto blendHash = std::hash<int>{}(static_cast<int>(key.BlendMode));
        return cullHash ^ (depthHash << 1U) ^ (blendHash << 2U);
    }
};

} // namespace Lvs::Engine::Rendering::Common
