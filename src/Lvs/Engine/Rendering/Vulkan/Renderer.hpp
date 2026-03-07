#pragma once

#include "Lvs/Engine/Math/Matrix4.hpp"
#include "Lvs/Engine/Enums/MeshCullMode.hpp"
#include "Lvs/Engine/Rendering/Common/CommandBuffer.hpp"
#include "Lvs/Engine/Rendering/Common/BindingLayout.hpp"
#include "Lvs/Engine/Rendering/Common/FrameContext.hpp"
#include "Lvs/Engine/Rendering/Common/MeshCache.hpp"
#include "Lvs/Engine/Rendering/Common/OverlayPrimitive.hpp"
#include "Lvs/Engine/Rendering/Common/PipelineVariant.hpp"
#include "Lvs/Engine/Rendering/Common/ResourceBinding.hpp"
#include "Lvs/Engine/Rendering/Common/SceneUniformData.hpp"
#include "Lvs/Engine/Rendering/Common/SceneRenderer.hpp"
#include "Lvs/Engine/Rendering/Common/ShadowRenderer.hpp"
#include "Lvs/Engine/Rendering/Common/SkyboxRenderer.hpp"
#include "Lvs/Engine/Rendering/RenderingFactory.hpp"
#include "Lvs/Engine/Rendering/Vulkan/RenderScene.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanShadowRenderer.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanSkyboxRenderer.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanBufferUtils.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanMeshUploader.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanTextureUtils.hpp"

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace Lvs::Engine::DataModel {
class Place;
class Workspace;
class Lighting;
}

namespace Lvs::Engine::Objects {
class Camera;
class DirectionalLight;
}

namespace Lvs::Engine::Rendering::Vulkan {

class RenderPartProxy;
class VulkanContext;
class VulkanPipelineLayout;
class VulkanPipelineVariant;
class VulkanRenderCommandBuffer;
class Renderer final : public Common::SceneRenderer {
public:
    explicit Renderer(std::shared_ptr<::Lvs::Engine::Rendering::RenderingFactory> factory = nullptr);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    void Initialize(Common::GraphicsContext& context, const Common::RenderSurface& surface) override;
    void RecreateSwapchain(Common::GraphicsContext& context, const Common::RenderSurface& surface) override;
    void DestroySwapchainResources(Common::GraphicsContext& context, const Common::RenderSurface& surface) override;
    void Shutdown(Common::GraphicsContext& context) override;

    void Initialize(VulkanContext& context);
    void RecreateSwapchain(VulkanContext& context);
    void DestroySwapchainResources(VulkanContext& context);
    void Shutdown(VulkanContext& context);

    void BindToPlace(const std::shared_ptr<DataModel::Place>& place) override;
    void Unbind() override;
    void SetOverlayPrimitives(std::vector<Rendering::Common::OverlayPrimitive> primitives) override;
    void RecordShadowCommands(
        Common::GraphicsContext& context,
        const Common::RenderSurface& surface,
        Common::CommandBuffer& commandBuffer,
        std::uint32_t frameIndex
    ) override;
    void RecordDrawCommands(
        Common::GraphicsContext& context,
        const Common::RenderSurface& surface,
        Common::CommandBuffer& commandBuffer,
        std::uint32_t frameIndex
    ) override;

    Common::MeshCache& GetMeshCache() override;
    void DrawPart(Common::CommandBuffer& commandBuffer, const RenderPartProxy& proxy, bool transparent = false);
    void DrawOverlayPrimitive(Common::CommandBuffer& commandBuffer, const Rendering::Common::OverlayPrimitive& primitive);

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
    std::unique_ptr<VulkanPipelineVariant> CreateGraphicsPipelineVariant(
        VulkanContext& context,
        const Common::PipelineVariantKey& key
    );
    [[nodiscard]] std::vector<BufferUtils::BufferHandle> AllocateUniformBuffers(VulkanContext& context) const;
    [[nodiscard]] std::vector<std::unique_ptr<Common::ResourceBinding>> CreateResourceBindings(VulkanContext& context) const;
    [[nodiscard]] std::vector<Common::PipelineVariantKey> GetPipelineVariants() const;
    void InitializeSubRenderers(VulkanContext& context);
    void UpdateCameraUniformAndLighting(VulkanContext& context, const Common::FrameContext& frameContext);
    void UpdateMainSkyDescriptorSets(VulkanContext& context);
    void UpdateShadowDescriptorSets(VulkanContext& context);
    void CreateSurfaceTextures(VulkanContext& context);
    void DestroySurfaceTextures(VulkanContext& context);
    void UpdateSurfaceDescriptorSets(VulkanContext& context);
    std::shared_ptr<Objects::Camera> GetCamera() const;
    std::shared_ptr<DataModel::Lighting> GetLighting() const;
    std::shared_ptr<Objects::DirectionalLight> GetDirectionalLight(
        const std::shared_ptr<DataModel::Lighting>& lighting
    ) const;
    float GetAspect(const VulkanContext& context) const;
    [[nodiscard]] const VulkanPipelineVariant& GetPipeline(const Common::PipelineVariantKey& key) const;
    [[nodiscard]] const Common::ResourceBinding& GetResourceBinding(std::uint32_t frameIndex) const;

    std::shared_ptr<DataModel::Place> place_;
    std::shared_ptr<DataModel::Workspace> workspace_;
    std::shared_ptr<::Lvs::Engine::Rendering::RenderingFactory> factory_;
    const Common::RenderSurface* surface_{nullptr};
    RenderScene scene_;
    VulkanMeshUploader meshUploader_;
    Common::MeshCache meshCache_;
    std::unique_ptr<Common::SkyboxRenderer> skyboxRenderer_;
    std::unique_ptr<Common::ShadowRenderer> shadowRenderer_;

    std::unique_ptr<Common::BindingLayout> bindingLayout_;
    std::unique_ptr<VulkanPipelineLayout> pipelineLayout_;
    std::unordered_map<Common::PipelineVariantKey, std::unique_ptr<VulkanPipelineVariant>, Common::PipelineVariantKeyHash>
        pipelines_;
    std::vector<BufferUtils::BufferHandle> uniformBuffers_;
    std::vector<std::unique_ptr<Common::ResourceBinding>> bindings_;
    VkImageView boundSkyImageView_{VK_NULL_HANDLE};
    VkImageView boundSurfaceAtlasView_{VK_NULL_HANDLE};
    VkImageView boundSurfaceNormalAtlasView_{VK_NULL_HANDLE};
    TextureUtils::Texture2DHandle surfaceAtlas_{};
    TextureUtils::Texture2DHandle surfaceNormalAtlas_{};
    bool hasSurfaceNormalAtlas_{false};
    std::vector<Rendering::Common::OverlayPrimitive> overlayPrimitives_;
    bool initialized_{false};
    VulkanContext* context_{nullptr};
};

using VulkanRenderer = Renderer;

} // namespace Lvs::Engine::Rendering::Vulkan
