#pragma once

#include <string>

namespace Lvs::Engine::Rendering::Common {

enum class ShaderStage {
    Vertex,
    Fragment
};

class PipelineManifestProvider {
public:
    virtual ~PipelineManifestProvider() = default;
    [[nodiscard]] virtual std::string GetShaderPath(const std::string& pipelineId, ShaderStage stage) const = 0;
};

} // namespace Lvs::Engine::Rendering::Common

