#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Lvs::Engine::Rendering {

class ShaderLoader {
public:
    static std::vector<std::uint32_t> LoadSPIRV(const std::string& pipelineId, const std::string& stage);
    static std::string LoadGLSL(const std::string& pipelineId, const std::string& stage);

    static std::string GetShaderPath(const std::string& pipelineId, const std::string& stage);
    static std::string GetOpenGLShaderPath(const std::string& pipelineId, const std::string& stage);
};

} // namespace Lvs::Engine::Rendering
