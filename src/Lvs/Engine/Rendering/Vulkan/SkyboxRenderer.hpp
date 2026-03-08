#pragma once

#include "Lvs/Engine/Math/Color3.hpp"
#include "Lvs/Engine/Core/Instance.hpp"
#include "Lvs/Engine/Rendering/Common/BindingLayout.hpp"
#include "Lvs/Engine/Rendering/Common/CommandBuffer.hpp"
#include "Lvs/Engine/Rendering/Common/GraphicsContext.hpp"
#include "Lvs/Engine/Rendering/Common/Mesh.hpp"
#include "Lvs/Engine/Rendering/Common/PipelineManifestProvider.hpp"
#include "Lvs/Engine/Rendering/Common/PipelineVariant.hpp"
#include "Lvs/Engine/Rendering/Common/ResourceBinding.hpp"
#include "Lvs/Engine/Rendering/Common/SkyboxSettingsResolver.hpp"
#include "Lvs/Engine/Rendering/Common/SkyboxRenderer.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanCubemapUtils.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace Lvs::Engine::DataModel {
class Place;
class Lighting;
}

namespace Lvs::Engine::Objects {
class Camera;
class Skybox;
}

namespace Lvs::Engine::Rendering::Vulkan {

class VulkanContext;
class VulkanPipelineLayout;
class VulkanPipelineVariant;

class SkyboxRenderer final : public Common::SkyboxRenderer {
public:
    SkyboxRenderer() = default;
    ~SkyboxRenderer() override;

    SkyboxRenderer(const SkyboxRenderer&) = delete;
    SkyboxRenderer& operator=(const SkyboxRenderer&) = delete;
    SkyboxRenderer(SkyboxRenderer&&) = delete;
    SkyboxRenderer& operator=(SkyboxRenderer&&) = delete;

    void Initialize(Common::GraphicsContext& context) override;
    void RecreateSwapchain(Common::GraphicsContext& context) override;
    void Shutdown(Common::GraphicsContext& context) override;

    void BindToPlace(const std::shared_ptr<DataModel::Place>& place) override;
    void Unbind() override;

    void UpdateResources(Common::GraphicsContext& context) override;
    void WriteSceneBinding(Common::GraphicsContext& context, Common::ResourceBinding& binding) const override;
    void Draw(
        Common::GraphicsContext& context,
        Common::CommandBuffer& commandBuffer,
        std::uint32_t frameIndex,
        const Objects::Camera& camera
    ) override;

    [[nodiscard]] const CubemapUtils::CubemapHandle& GetCubemap() const;
    [[nodiscard]] Math::Color3 GetSkyTint() const override;

private:
    [[nodiscard]] std::unique_ptr<Common::BindingLayout> CreateBindingLayout(VulkanContext& context) const;
    void CreatePipelineLayout(VulkanContext& context);
    void CreateGraphicsPipelines(VulkanContext& context);
    std::unique_ptr<VulkanPipelineVariant> CreateGraphicsPipelineVariant(
        VulkanContext& context,
        const Common::PipelineVariantKey& key
    );
    [[nodiscard]] std::vector<std::unique_ptr<Common::ResourceBinding>> CreateBindings(VulkanContext& context) const;
    [[nodiscard]] std::vector<Common::PipelineVariantKey> GetPipelineVariants() const;
    void UpdateDescriptorSets(VulkanContext& context);
    void DestroySwapchainResources(VulkanContext& context);
    void UpdateResourcesInternal(VulkanContext& context);
    void DrawInternal(VulkanContext& context, Common::CommandBuffer& commandBuffer, std::uint32_t frameIndex, const Objects::Camera& camera);
    void UpdateSkyFromPlace();
    [[nodiscard]] const VulkanPipelineVariant& GetPipeline(const Common::PipelineVariantKey& key) const;

    std::shared_ptr<DataModel::Place> place_;
    std::shared_ptr<Common::Mesh> skyboxMesh_;

    std::unique_ptr<Common::BindingLayout> bindingLayout_;
    std::unique_ptr<VulkanPipelineLayout> pipelineLayout_;
    std::unordered_map<Common::PipelineVariantKey, std::unique_ptr<VulkanPipelineVariant>, Common::PipelineVariantKeyHash>
        pipelines_;
    std::vector<std::unique_ptr<Common::ResourceBinding>> bindings_;
    std::shared_ptr<Common::PipelineManifestProvider> pipelineManifest_;
    Common::SkyboxSettingsResolver settingsResolver_{};

    CubemapUtils::CubemapHandle cubemap_;
    std::shared_ptr<Objects::Skybox> activeSkybox_;
    Math::Color3 skyTint_{1.0, 1.0, 1.0};
    std::optional<Engine::Core::Instance::PropertyInvalidatedConnection> skyboxPropertyConnection_;
    bool skyboxDirty_{true};
    bool initialized_{false};
};

} // namespace Lvs::Engine::Rendering::Vulkan
