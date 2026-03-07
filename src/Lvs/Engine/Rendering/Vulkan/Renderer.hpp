#pragma once

#include "Lvs/Engine/Math/Matrix4.hpp"
#include "Lvs/Engine/Enums/MeshCullMode.hpp"
#include "Lvs/Engine/Rendering/Common/CommandBuffer.hpp"
#include "Lvs/Engine/Rendering/Common/MeshCache.hpp"
#include "Lvs/Engine/Rendering/Common/OverlayPrimitive.hpp"
#include "Lvs/Engine/Rendering/Common/PipelineVariant.hpp"
#include "Lvs/Engine/Rendering/Common/SceneRenderer.hpp"
#include "Lvs/Engine/Rendering/Vulkan/RenderScene.hpp"
#include "Lvs/Engine/Rendering/Vulkan/SkyboxRenderer.hpp"
#include "Lvs/Engine/Rendering/Vulkan/ShadowRenderer.hpp"
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
class VulkanRenderCommandBuffer;
class Renderer final : public Common::SceneRenderer {
public:
    Renderer();
    ~Renderer() = default;

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
    struct CameraUniform {
        float View[16];
        float Projection[16];
        float CameraPosition[4];
        float LightDirection[4];
        float LightColorIntensity[4];
        float LightSpecular[4];
        float Ambient[4];
        float SkyTint[4];
        float RenderSettings[4];
        float ShadowMatrices[3][16];
        float ShadowCascadeSplits[4];
        float ShadowParams[4];
        float ShadowState[4];
        float CameraForward[4];
    };

    struct PushConstants {
        float Model[16];
        float BaseColor[4];
        float Material[4];
        float SurfaceData0[4];
        float SurfaceData1[4];
    };

    void CreateDescriptorSetLayout(VulkanContext& context);
    void CreatePipelineLayout(VulkanContext& context);
    void CreateGraphicsPipeline(VulkanContext& context);
    VkPipeline CreateGraphicsPipelineVariant(VulkanContext& context, const Common::PipelineVariantKey& key);
    void CreateUniformBuffers(VulkanContext& context);
    void CreateDescriptorPool(VulkanContext& context);
    void CreateDescriptorSets(VulkanContext& context);
    void UpdateCameraUniformAndLighting(VulkanContext& context, std::uint32_t frameIndex);
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
    [[nodiscard]] VkPipeline GetPipeline(const Common::PipelineVariantKey& key) const;

    std::shared_ptr<DataModel::Place> place_;
    std::shared_ptr<DataModel::Workspace> workspace_;
    RenderScene scene_;
    VulkanMeshUploader meshUploader_;
    Common::MeshCache meshCache_;
    SkyboxRenderer skyboxRenderer_;
    ShadowRenderer shadowRenderer_;

    VkDescriptorSetLayout descriptorSetLayout_{VK_NULL_HANDLE};
    VkPipelineLayout pipelineLayout_{VK_NULL_HANDLE};
    std::unordered_map<Common::PipelineVariantKey, VkPipeline, Common::PipelineVariantKeyHash> pipelineVariants_;
    VkDescriptorPool descriptorPool_{VK_NULL_HANDLE};
    std::vector<BufferUtils::BufferHandle> uniformBuffers_;
    std::vector<VkDescriptorSet> descriptorSets_;
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
