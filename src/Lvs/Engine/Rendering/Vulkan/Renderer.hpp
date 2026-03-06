#pragma once

#include "Lvs/Engine/Math/Matrix4.hpp"
#include "Lvs/Engine/Enums/MeshCullMode.hpp"
#include "Lvs/Engine/Rendering/Vulkan/OverlayPrimitive.hpp"
#include "Lvs/Engine/Rendering/Vulkan/MeshCache.hpp"
#include "Lvs/Engine/Rendering/Vulkan/RenderScene.hpp"
#include "Lvs/Engine/Rendering/Vulkan/SkyboxRenderer.hpp"
#include "Lvs/Engine/Rendering/Vulkan/ShadowRenderer.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanBufferUtils.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanTextureUtils.hpp"

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <memory>
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
class Mesh;

class Renderer final {
public:
    Renderer() = default;
    ~Renderer() = default;

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    void Initialize(VulkanContext& context);
    void RecreateSwapchain(VulkanContext& context);
    void DestroySwapchainResources(VulkanContext& context);
    void Shutdown(VulkanContext& context);

    void BindToPlace(const std::shared_ptr<DataModel::Place>& place);
    void Unbind();
    void SetOverlayPrimitives(std::vector<OverlayPrimitive> primitives);
    void RecordShadowCommands(VulkanContext& context, VkCommandBuffer commandBuffer, std::uint32_t frameIndex);
    void RecordDrawCommands(VulkanContext& context, VkCommandBuffer commandBuffer, std::uint32_t frameIndex);

    MeshCache& GetMeshCache();
    void DrawPart(VkCommandBuffer commandBuffer, const RenderPartProxy& proxy, bool transparent = false);
    void DrawOverlayPrimitive(VkCommandBuffer commandBuffer, const OverlayPrimitive& primitive);

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
    VkPipeline CreateGraphicsPipelineVariant(
        VulkanContext& context,
        VkCullModeFlags cullMode,
        bool depthTest,
        bool depthWrite,
        VkCompareOp depthCompare,
        bool enableBlending
    );
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

    std::shared_ptr<DataModel::Place> place_;
    std::shared_ptr<DataModel::Workspace> workspace_;
    RenderScene scene_;
    MeshCache meshCache_;
    SkyboxRenderer skyboxRenderer_;
    ShadowRenderer shadowRenderer_;

    VkDescriptorSetLayout descriptorSetLayout_{VK_NULL_HANDLE};
    VkPipelineLayout pipelineLayout_{VK_NULL_HANDLE};
    VkPipeline graphicsPipelineBackCull_{VK_NULL_HANDLE};
    VkPipeline graphicsPipelineFrontCull_{VK_NULL_HANDLE};
    VkPipeline graphicsPipelineNoCull_{VK_NULL_HANDLE};
    VkPipeline transparentPipelineBackCull_{VK_NULL_HANDLE};
    VkPipeline transparentPipelineFrontCull_{VK_NULL_HANDLE};
    VkPipeline transparentPipelineNoCull_{VK_NULL_HANDLE};
    VkPipeline alwaysOnTopPipelineBackCull_{VK_NULL_HANDLE};
    VkPipeline alwaysOnTopPipelineNoCull_{VK_NULL_HANDLE};
    VkDescriptorPool descriptorPool_{VK_NULL_HANDLE};
    std::vector<BufferUtils::BufferHandle> uniformBuffers_;
    std::vector<VkDescriptorSet> descriptorSets_;
    VkImageView boundSkyImageView_{VK_NULL_HANDLE};
    VkImageView boundSurfaceAtlasView_{VK_NULL_HANDLE};
    VkImageView boundSurfaceNormalAtlasView_{VK_NULL_HANDLE};
    TextureUtils::Texture2DHandle surfaceAtlas_{};
    TextureUtils::Texture2DHandle surfaceNormalAtlas_{};
    bool hasSurfaceNormalAtlas_{false};
    std::vector<OverlayPrimitive> overlayPrimitives_;
    bool initialized_{false};
    VulkanContext* context_{nullptr};
};

} // namespace Lvs::Engine::Rendering::Vulkan
