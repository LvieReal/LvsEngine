#pragma once

#include "Lvs/Engine/Rendering/Common/PipelineManifestProvider.hpp"
#include "Lvs/Engine/Rendering/Common/RenderResourceRegistry.hpp"

#include <string>

namespace Lvs::Engine::Rendering::Vulkan {

class VulkanPipelineManifestProvider final : public Common::PipelineManifestProvider {
public:
    [[nodiscard]] std::string GetShaderPath(const std::string& pipelineId, Common::ShaderStage stage) const override;
};

class VulkanRenderResourceRegistry final : public Common::RenderResourceRegistry {
public:
    [[nodiscard]] std::string GetTexturePath(const std::string& resourceId) const override;
};

} // namespace Lvs::Engine::Rendering::Vulkan

