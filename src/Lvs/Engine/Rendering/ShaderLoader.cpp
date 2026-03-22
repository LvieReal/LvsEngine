#include "Lvs/Engine/Rendering/ShaderLoader.hpp"

#include "Lvs/Engine/Utils/FileIO.hpp"
#include "Lvs/Engine/Utils/PathUtils.hpp"

#include <array>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace Lvs::Engine::Rendering {

namespace {

using StagePair = std::array<std::string, 2>;

const std::unordered_map<std::string, StagePair> kPipelineShaderTable{
    {"main", {"CompiledShaders/Vulkan/Main.vert.spv", "CompiledShaders/Vulkan/Main.frag.spv"}},
    {"shadow", {"CompiledShaders/Vulkan/Shadow.vert.spv", "CompiledShaders/Vulkan/Shadow.frag.spv"}},
    {"sky", {"CompiledShaders/Vulkan/Sky.vert.spv", "CompiledShaders/Vulkan/Sky.frag.spv"}},
    {"post_composite", {"CompiledShaders/Vulkan/PostProcess.vert.spv", "CompiledShaders/Vulkan/PostProcess.frag.spv"}},
    {"post_hbao", {"CompiledShaders/Vulkan/PostProcess.vert.spv", "CompiledShaders/Vulkan/Hbao.frag.spv"}},
    {"post_blur_down", {"CompiledShaders/Vulkan/PostProcess.vert.spv", "CompiledShaders/Vulkan/DualKawaseDown.frag.spv"}},
    {"post_blur_up", {"CompiledShaders/Vulkan/PostProcess.vert.spv", "CompiledShaders/Vulkan/DualKawaseUp.frag.spv"}}
};

const std::unordered_map<std::string, StagePair> kOpenGLPipelineShaderTable{
    {"main", {"CompiledShaders/OpenGL/Main.vert.glsl", "CompiledShaders/OpenGL/Main.frag.glsl"}},
    {"shadow", {"CompiledShaders/OpenGL/Shadow.vert.glsl", "CompiledShaders/OpenGL/Shadow.frag.glsl"}},
    {"sky", {"CompiledShaders/OpenGL/Sky.vert.glsl", "CompiledShaders/OpenGL/Sky.frag.glsl"}},
    {"post_composite", {"CompiledShaders/OpenGL/PostProcess.vert.glsl", "CompiledShaders/OpenGL/PostProcess.frag.glsl"}},
    {"post_hbao", {"CompiledShaders/OpenGL/PostProcess.vert.glsl", "CompiledShaders/OpenGL/Hbao.frag.glsl"}},
    {"post_blur_down", {"CompiledShaders/OpenGL/PostProcess.vert.glsl", "CompiledShaders/OpenGL/DualKawaseDown.frag.glsl"}},
    {"post_blur_up", {"CompiledShaders/OpenGL/PostProcess.vert.glsl", "CompiledShaders/OpenGL/DualKawaseUp.frag.glsl"}}
};

std::size_t StageIndex(const std::string& stage) {
    if (stage == "vert" || stage == "vertex" || stage == "vs") {
        return 0U;
    }
    if (stage == "frag" || stage == "fragment" || stage == "fs") {
        return 1U;
    }
    throw std::runtime_error("Unsupported shader stage: " + stage);
}

} // namespace

std::string ShaderLoader::GetShaderPath(const std::string& pipelineId, const std::string& stage) {
    const auto fallback = kPipelineShaderTable.at("main");
    const auto it = kPipelineShaderTable.find(pipelineId);
    const auto& pair = it != kPipelineShaderTable.end() ? it->second : fallback;
    const std::size_t index = StageIndex(stage);
    return Utils::PathUtils::GetResourcePath(pair[index]).string();
}

std::string ShaderLoader::GetOpenGLShaderPath(const std::string& pipelineId, const std::string& stage) {
    const auto fallback = kOpenGLPipelineShaderTable.at("main");
    const auto it = kOpenGLPipelineShaderTable.find(pipelineId);
    const auto& pair = it != kOpenGLPipelineShaderTable.end() ? it->second : fallback;
    const std::size_t index = StageIndex(stage);
    return Utils::PathUtils::GetResourcePath(pair[index]).string();
}

std::vector<std::uint32_t> ShaderLoader::LoadSPIRV(const std::string& pipelineId, const std::string& stage) {
    const auto bytes = Utils::FileIO::ReadBinaryFile(GetShaderPath(pipelineId, stage));
    if (bytes.empty()) {
        return {};
    }

    const std::size_t wordCount = (bytes.size() + sizeof(std::uint32_t) - 1) / sizeof(std::uint32_t);
    std::vector<std::uint32_t> spirv(wordCount, 0U);
    std::memcpy(spirv.data(), bytes.data(), bytes.size());
    return spirv;
}

std::string ShaderLoader::LoadGLSL(const std::string& pipelineId, const std::string& stage) {
    return Utils::FileIO::ReadTextFile(GetOpenGLShaderPath(pipelineId, stage));
}

} // namespace Lvs::Engine::Rendering
