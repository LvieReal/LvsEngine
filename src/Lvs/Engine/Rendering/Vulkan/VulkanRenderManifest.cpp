#include "Lvs/Engine/Rendering/Vulkan/VulkanRenderManifest.hpp"

#include "Lvs/Engine/Utils/PathUtils.hpp"

#include <array>
#include <unordered_map>

namespace Lvs::Engine::Rendering::Vulkan {

namespace {

using StagePair = std::array<std::string, 2>;

const std::unordered_map<std::string, StagePair> kPipelineShaderTable{
    {"main", {"Shaders/Vulkan/Main.vert.spv", "Shaders/Vulkan/Main.frag.spv"}},
    {"shadow", {"Shaders/Vulkan/Shadow.vert.spv", "Shaders/Vulkan/Shadow.frag.spv"}},
    {"sky", {"Shaders/Vulkan/Sky.vert.spv", "Shaders/Vulkan/Sky.frag.spv"}},
    {"post_composite", {"Shaders/Vulkan/PostProcess.vert.spv", "Shaders/Vulkan/PostProcess.frag.spv"}},
    {"post_blur_down", {"Shaders/Vulkan/PostProcess.vert.spv", "Shaders/Vulkan/DualKawaseDown.frag.spv"}},
    {"post_blur_up", {"Shaders/Vulkan/PostProcess.vert.spv", "Shaders/Vulkan/DualKawaseUp.frag.spv"}}
};

const std::unordered_map<std::string, std::string> kTextureTable{
    {"surface_atlas", "Surfaces/Surfaces2.png"},
    {"surface_normal_atlas", "Surfaces/Surfaces2_normals.png"}
};

} // namespace

std::string VulkanPipelineManifestProvider::GetShaderPath(const std::string& pipelineId, const Common::ShaderStage stage) const {
    const auto fallback = kPipelineShaderTable.at("main");
    const auto it = kPipelineShaderTable.find(pipelineId);
    const auto& pair = it != kPipelineShaderTable.end() ? it->second : fallback;
    const std::size_t index = stage == Common::ShaderStage::Vertex ? 0U : 1U;
    return Utils::PathUtils::GetResourcePath(pair[index]).string();
}

std::string VulkanRenderResourceRegistry::GetTexturePath(const std::string& resourceId) const {
    const auto it = kTextureTable.find(resourceId);
    if (it != kTextureTable.end()) {
        return Utils::PathUtils::GetResourcePath(it->second).string();
    }
    return Utils::PathUtils::GetResourcePath(kTextureTable.at("surface_atlas")).string();
}

} // namespace Lvs::Engine::Rendering::Vulkan

