#pragma once

#include "Lvs/Engine/Enums/MeshCullMode.hpp"
#include "Lvs/Engine/Math/Matrix4.hpp"
#include "Lvs/Engine/Rendering/Common/BindingLayout.hpp"
#include "Lvs/Engine/Rendering/Common/FrameContext.hpp"
#include "Lvs/Engine/Rendering/Common/Pipeline.hpp"
#include "Lvs/Engine/Rendering/Common/PipelineVariant.hpp"
#include "Lvs/Engine/Rendering/Common/PostProcessRenderer.hpp"
#include "Lvs/Engine/Rendering/Common/RenderPolicyResolver.hpp"
#include "Lvs/Engine/Rendering/Common/RenderSettingsResolver.hpp"
#include "Lvs/Engine/Rendering/Common/Renderer.hpp"
#include "Lvs/Engine/Rendering/Common/ResourceBinding.hpp"
#include "Lvs/Engine/Rendering/Common/RenderResourceRegistry.hpp"
#include "Lvs/Engine/Rendering/Common/PipelineManifestProvider.hpp"
#include "Lvs/Engine/Rendering/Common/RenderPartProxy.hpp"
#include "Lvs/Engine/Rendering/Common/SceneUniformData.hpp"
#include "Lvs/Engine/Rendering/Common/ShadowRenderer.hpp"
#include "Lvs/Engine/Rendering/Common/SkyboxRenderer.hpp"
#include "Lvs/Engine/Rendering/Common/RenderScene.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanTextureUtils.hpp"

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace Lvs::Engine::Objects {
class Camera;
class DirectionalLight;
}

namespace Lvs::Engine::DataModel {
class Lighting;
}

namespace Lvs::Engine::Rendering::Vulkan {

class VulkanContext;

class VulkanRenderer final : public Common::Renderer {
public:
    explicit VulkanRenderer(std::shared_ptr<::Lvs::Engine::Rendering::RenderingFactory> factory = nullptr);
    ~VulkanRenderer() override;

    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;
    VulkanRenderer(VulkanRenderer&&) = delete;
    VulkanRenderer& operator=(VulkanRenderer&&) = delete;

    void DrawRenderProxy(Common::CommandBuffer& commandBuffer, const Common::RenderProxy& proxy, bool transparent = false) override;
    void RecordFrameCommands(
        Common::GraphicsContext& context,
        const Common::RenderSurface& surface,
        Common::CommandBuffer& commandBuffer,
        std::uint32_t imageIndex,
        std::uint32_t frameIndex,
        const std::array<float, 4>& clearColor
    ) override;
    void DrawPart(Common::CommandBuffer& commandBuffer, const Common::RenderPartProxy& proxy, bool transparent = false);
    void DrawOverlayPrimitive(Common::CommandBuffer& commandBuffer, const Rendering::Common::OverlayPrimitive& primitive);

protected:
    void OnInitialize(Common::GraphicsContext& context, const Common::RenderSurface& surface) override;
    void OnRecreateSwapchain(Common::GraphicsContext& context, const Common::RenderSurface& surface) override;
    void OnDestroySwapchainResources(Common::GraphicsContext& context, const Common::RenderSurface& surface) override;
    void OnShutdown(Common::GraphicsContext& context) override;
    void OnBindToPlace(const std::shared_ptr<DataModel::Place>& place) override;
    void OnUnbind() override;
    void OnOverlayPrimitivesChanged(const std::vector<Common::OverlayPrimitive>& primitives) override;
    void OnRecordShadowCommands(
        Common::GraphicsContext& context,
        const Common::RenderSurface& surface,
        Common::CommandBuffer& commandBuffer,
        std::uint32_t frameIndex
    ) override;
    void OnRecordDrawCommands(
        Common::GraphicsContext& context,
        const Common::RenderSurface& surface,
        Common::CommandBuffer& commandBuffer,
        std::uint32_t frameIndex
    ) override;

private:
    using CameraUniform = Common::SceneUniformData;

    struct PushConstants {
        float Model[16];
        float BaseColor[4];
        float Material[4];
        float SurfaceData0[4];
        float SurfaceData1[4];
    };

    [[nodiscard]] std::unique_ptr<Common::BindingLayout> CreateBindingLayout(VulkanContext& context) const;
    void CreatePipelineLayout(VulkanContext& context);
    void CreateGraphicsPipelines(VulkanContext& context);
    std::unique_ptr<Common::Pipeline> CreateGraphicsPipelineVariant(
        VulkanContext& context,
        const Common::PipelineVariantKey& key
    );
    [[nodiscard]] std::vector<std::unique_ptr<Common::BufferResource>> AllocateUniformBuffers(VulkanContext& context) const;
    [[nodiscard]] std::vector<std::unique_ptr<Common::ResourceBinding>> CreateResourceBindings(VulkanContext& context) const;
    [[nodiscard]] std::vector<Common::PipelineVariantKey> GetPipelineVariants() const;
    void InitializeSubRenderers(VulkanContext& context);
    void UpdateCameraUniformAndLighting(VulkanContext& context, const Common::FrameContext& frameContext);
    void UpdateMainSkyDescriptorSets(VulkanContext& context);
    void UpdateShadowDescriptorSets(VulkanContext& context);
    void CreateSurfaceTextures(VulkanContext& context);
    void DestroySurfaceTextures(VulkanContext& context);
    void UpdateSurfaceDescriptorSets(VulkanContext& context);
    float GetAspect(const VulkanContext& context) const;
    [[nodiscard]] const Common::Pipeline& GetPipeline(const Common::PipelineVariantKey& key) const;
    [[nodiscard]] const Common::ResourceBinding& GetResourceBinding(std::uint32_t frameIndex) const;

    Common::RenderScene scene_;
    std::unique_ptr<Common::SkyboxRenderer> skyboxRenderer_;
    std::unique_ptr<Common::ShadowRenderer> shadowRenderer_;
    std::unique_ptr<Common::PostProcessRenderer> postProcessRenderer_;
    std::shared_ptr<Common::PipelineManifestProvider> pipelineManifest_;
    std::shared_ptr<Common::RenderResourceRegistry> resourceRegistry_;
    Common::RenderPolicyResolver policyResolver_{};
    Common::RenderSettingsResolver settingsResolver_{};
    Common::FrameRenderData frameRenderData_{};
    Common::RenderSettingsSnapshot settingsSnapshot_{};

    std::unique_ptr<Common::BindingLayout> bindingLayout_;
    std::unique_ptr<Common::PipelineLayout> pipelineLayout_;
    std::unordered_map<Common::PipelineVariantKey, std::unique_ptr<Common::Pipeline>, Common::PipelineVariantKeyHash>
        pipelines_;
    std::vector<std::unique_ptr<Common::BufferResource>> uniformBuffers_;
    std::vector<std::unique_ptr<Common::ResourceBinding>> bindings_;
    VkImageView boundSkyImageView_{VK_NULL_HANDLE};
    VkImageView boundSurfaceAtlasView_{VK_NULL_HANDLE};
    VkImageView boundSurfaceNormalAtlasView_{VK_NULL_HANDLE};
    TextureUtils::Texture2DHandle surfaceAtlas_{};
    TextureUtils::Texture2DHandle surfaceNormalAtlas_{};
    bool hasSurfaceNormalAtlas_{false};
};

} // namespace Lvs::Engine::Rendering::Vulkan
