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

struct PipelineShaderPaths {
    std::string Vert{};
    std::string Frag{};
    std::string Geom{}; // optional
};

const std::unordered_map<std::string, PipelineShaderPaths> kPipelineShaderTable{
    {"main", {.Vert = "CompiledShaders/Vulkan/Main.vert.spv", .Frag = "CompiledShaders/Vulkan/Main.frag.spv"}},
    {"shadow", {.Vert = "CompiledShaders/Vulkan/Shadow.vert.spv", .Frag = "CompiledShaders/Vulkan/Shadow.frag.spv"}},
    {"shadow_volume",
     {.Vert = "CompiledShaders/Vulkan/ShadowVolume.vert.spv",
      .Frag = "CompiledShaders/Vulkan/ShadowVolume.frag.spv",
      .Geom = "CompiledShaders/Vulkan/ShadowVolume.geom.spv"}},
    {"shadow_volume_mask",
     {.Vert = "CompiledShaders/Vulkan/PostProcess.vert.spv", .Frag = "CompiledShaders/Vulkan/ShadowVolumeMask.frag.spv"}},
    {"shadow_volume_mask_clear",
     {.Vert = "CompiledShaders/Vulkan/PostProcess.vert.spv", .Frag = "CompiledShaders/Vulkan/ShadowVolumeMaskClear.frag.spv"}},
    {"shadow_volume_apply",
     {.Vert = "CompiledShaders/Vulkan/PostProcess.vert.spv", .Frag = "CompiledShaders/Vulkan/ShadowVolumeApply.frag.spv"}},
    {"sky", {.Vert = "CompiledShaders/Vulkan/Sky.vert.spv", .Frag = "CompiledShaders/Vulkan/Sky.frag.spv"}},
    {"post_composite", {.Vert = "CompiledShaders/Vulkan/PostProcess.vert.spv", .Frag = "CompiledShaders/Vulkan/PostProcess.frag.spv"}},
    {"post_hbao", {.Vert = "CompiledShaders/Vulkan/PostProcess.vert.spv", .Frag = "CompiledShaders/Vulkan/Hbao.frag.spv"}},
    {"post_blur_down", {.Vert = "CompiledShaders/Vulkan/PostProcess.vert.spv", .Frag = "CompiledShaders/Vulkan/DualKawaseDown.frag.spv"}},
    {"post_blur_up", {.Vert = "CompiledShaders/Vulkan/PostProcess.vert.spv", .Frag = "CompiledShaders/Vulkan/DualKawaseUp.frag.spv"}}
};

const std::unordered_map<std::string, PipelineShaderPaths> kOpenGLPipelineShaderTable{
    {"main", {.Vert = "CompiledShaders/OpenGL/Main.vert.glsl", .Frag = "CompiledShaders/OpenGL/Main.frag.glsl"}},
    {"shadow", {.Vert = "CompiledShaders/OpenGL/Shadow.vert.glsl", .Frag = "CompiledShaders/OpenGL/Shadow.frag.glsl"}},
    {"shadow_volume",
     {.Vert = "CompiledShaders/OpenGL/ShadowVolume.vert.glsl",
      .Frag = "CompiledShaders/OpenGL/ShadowVolume.frag.glsl",
      .Geom = "CompiledShaders/OpenGL/ShadowVolume.geom.glsl"}},
    {"shadow_volume_mask",
     {.Vert = "CompiledShaders/OpenGL/PostProcess.vert.glsl", .Frag = "CompiledShaders/OpenGL/ShadowVolumeMask.frag.glsl"}},
    {"shadow_volume_mask_clear",
     {.Vert = "CompiledShaders/OpenGL/PostProcess.vert.glsl", .Frag = "CompiledShaders/OpenGL/ShadowVolumeMaskClear.frag.glsl"}},
    {"shadow_volume_apply",
     {.Vert = "CompiledShaders/OpenGL/PostProcess.vert.glsl", .Frag = "CompiledShaders/OpenGL/ShadowVolumeApply.frag.glsl"}},
    {"sky", {.Vert = "CompiledShaders/OpenGL/Sky.vert.glsl", .Frag = "CompiledShaders/OpenGL/Sky.frag.glsl"}},
    {"post_composite", {.Vert = "CompiledShaders/OpenGL/PostProcess.vert.glsl", .Frag = "CompiledShaders/OpenGL/PostProcess.frag.glsl"}},
    {"post_hbao", {.Vert = "CompiledShaders/OpenGL/PostProcess.vert.glsl", .Frag = "CompiledShaders/OpenGL/Hbao.frag.glsl"}},
    {"post_blur_down", {.Vert = "CompiledShaders/OpenGL/PostProcess.vert.glsl", .Frag = "CompiledShaders/OpenGL/DualKawaseDown.frag.glsl"}},
    {"post_blur_up", {.Vert = "CompiledShaders/OpenGL/PostProcess.vert.glsl", .Frag = "CompiledShaders/OpenGL/DualKawaseUp.frag.glsl"}}
};

std::string NormalizeStage(const std::string& stage) {
    if (stage == "vert" || stage == "vertex" || stage == "vs") {
        return "vert";
    }
    if (stage == "frag" || stage == "fragment" || stage == "fs") {
        return "frag";
    }
    if (stage == "geom" || stage == "geometry" || stage == "gs") {
        return "geom";
    }
    throw std::runtime_error("Unsupported shader stage: " + stage);
}

const PipelineShaderPaths& ResolvePipeline(const std::unordered_map<std::string, PipelineShaderPaths>& table, const std::string& id) {
    const auto it = table.find(id);
    return it != table.end() ? it->second : table.at("main");
}

std::string ResolveStagePath(const PipelineShaderPaths& paths, const std::string& stage) {
    const std::string s = NormalizeStage(stage);
    if (s == "vert") {
        return paths.Vert;
    }
    if (s == "frag") {
        return paths.Frag;
    }
    if (s == "geom") {
        return paths.Geom;
    }
    return {};
}

} // namespace

std::string ShaderLoader::GetShaderPath(const std::string& pipelineId, const std::string& stage) {
    const auto& entry = ResolvePipeline(kPipelineShaderTable, pipelineId);
    const std::string rel = ResolveStagePath(entry, stage);
    if (rel.empty()) {
        return {};
    }
    return Utils::PathUtils::GetResourcePath(rel).string();
}

std::string ShaderLoader::GetOpenGLShaderPath(const std::string& pipelineId, const std::string& stage) {
    const auto& entry = ResolvePipeline(kOpenGLPipelineShaderTable, pipelineId);
    const std::string rel = ResolveStagePath(entry, stage);
    if (rel.empty()) {
        return {};
    }
    return Utils::PathUtils::GetResourcePath(rel).string();
}

std::vector<std::uint32_t> ShaderLoader::LoadSPIRV(const std::string& pipelineId, const std::string& stage) {
    const std::string path = GetShaderPath(pipelineId, stage);
    if (path.empty()) {
        return {};
    }
    const auto bytes = Utils::FileIO::ReadBinaryFile(path);
    if (bytes.empty()) {
        return {};
    }

    const std::size_t wordCount = (bytes.size() + sizeof(std::uint32_t) - 1) / sizeof(std::uint32_t);
    std::vector<std::uint32_t> spirv(wordCount, 0U);
    std::memcpy(spirv.data(), bytes.data(), bytes.size());
    return spirv;
}

std::string ShaderLoader::LoadGLSL(const std::string& pipelineId, const std::string& stage) {
    const std::string path = GetOpenGLShaderPath(pipelineId, stage);
    if (path.empty()) {
        return {};
    }
    return Utils::FileIO::ReadTextFile(path);
}

} // namespace Lvs::Engine::Rendering
