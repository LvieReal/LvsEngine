#pragma once

#include "Lvs/Engine/Rendering/Common/CommandBuffer.hpp"
#include "Lvs/Engine/Rendering/Common/GraphicsContext.hpp"
#include "Lvs/Engine/Rendering/Common/PipelineVariant.hpp"
#include "Lvs/Engine/Rendering/Common/RenderProxy.hpp"
#include "Lvs/Engine/Rendering/Common/ResourceBinding.hpp"
#include "Lvs/Engine/Rendering/Common/ShadowRenderer.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanPipeline.hpp"
#include "Lvs/Engine/Math/Matrix4.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace Lvs::Engine::Objects {
class Camera;
}

namespace Lvs::Engine::Rendering::Vulkan {

class RenderPartProxy;
class VulkanContext;
class ShadowRenderer final : public Common::ShadowRenderer {
public:
    using ShadowSettings = Common::ShadowRenderer::ShadowSettings;
    using ShadowData = Common::ShadowRenderer::ShadowData;

    ShadowRenderer() = default;
    ~ShadowRenderer() override;

    ShadowRenderer(const ShadowRenderer&) = delete;
    ShadowRenderer& operator=(const ShadowRenderer&) = delete;
    ShadowRenderer(ShadowRenderer&&) = delete;
    ShadowRenderer& operator=(ShadowRenderer&&) = delete;

    void Initialize(Common::GraphicsContext& context) override;
    void RecreateSwapchain(Common::GraphicsContext& context) override;
    void Shutdown(Common::GraphicsContext& context) override;

    void Initialize(VulkanContext& context);
    void RecreateSwapchain(VulkanContext& context);
    void Shutdown(VulkanContext& context);

    void Render(
        Common::GraphicsContext& context,
        Common::CommandBuffer& commandBuffer,
        const std::vector<std::shared_ptr<Common::RenderProxy>>& shadowCasters,
        const Objects::Camera& camera,
        const Math::Vector3& directionalLightDirection,
        float cameraAspect,
        const ShadowSettings& settings
    ) override;

    void Render(
        VulkanContext& context,
        Common::CommandBuffer& commandBuffer,
        const std::vector<std::shared_ptr<RenderPartProxy>>& shadowCasters,
        const Objects::Camera& camera,
        const Math::Vector3& directionalLightDirection,
        float cameraAspect,
        const ShadowSettings& settings
    );

    void WriteSceneBinding(Common::GraphicsContext& context, Common::ResourceBinding& binding) const override;
    [[nodiscard]] const ShadowData& GetShadowData() const override;
    [[nodiscard]] VkSampler GetShadowSampler() const;
    [[nodiscard]] const std::array<VkImageView, 3>& GetShadowImageViews() const;
    [[nodiscard]] VkSampler GetJitterSampler() const;
    [[nodiscard]] VkImageView GetJitterImageView() const;

private:
    static constexpr int MAX_CASCADES = 3;

    struct ImageResource {
        VkImage Image{VK_NULL_HANDLE};
        VkDeviceMemory Memory{VK_NULL_HANDLE};
        VkImageView View{VK_NULL_HANDLE};
        VkFramebuffer Framebuffer{VK_NULL_HANDLE};
        std::uint32_t Resolution{0};
    };

    struct CascadeComputation {
        std::array<Math::Matrix4, MAX_CASCADES> Matrices{
            Math::Matrix4::Identity(),
            Math::Matrix4::Identity(),
            Math::Matrix4::Identity()
        };
        float Split0{220.0F};
        float Split1{220.0F};
        float MaxDistance{220.0F};
    };

    struct PushConstants {
        float LightViewProjection[16];
        float Model[16];
    };

    void CreateRenderPass(VulkanContext& context);
    void CreatePipelineLayout(VulkanContext& context);
    void CreatePipelines(VulkanContext& context);
    std::unique_ptr<VulkanPipelineVariant> CreatePipelineVariant(VulkanContext& context, const Common::PipelineVariantKey& key);
    [[nodiscard]] std::vector<Common::PipelineVariantKey> GetPipelineVariants() const;
    void EnsureDepthResources(VulkanContext& context, std::uint32_t resolution, float cascadeResolutionScale);
    void EnsureJitterTexture(VulkanContext& context);
    void DestroyDepthResources(VulkanContext& context);
    void DestroyJitterTexture(VulkanContext& context);
    void DestroySwapchainResources(VulkanContext& context);
    [[nodiscard]] const VulkanPipelineVariant& GetPipeline(const Common::PipelineVariantKey& key) const;

    bool ComputeCascades(
        const Objects::Camera& camera,
        const Math::Vector3& directionalLightDirection,
        float cameraAspect,
        int cascadeCount,
        float maxDistance,
        float cascadeSplitLambda,
        CascadeComputation& out
    ) const;

    Math::Matrix4 ComputeCascadeLightViewProjection(
        const Objects::Camera& camera,
        const Math::Vector3& direction,
        float cameraAspect,
        double rangeNear,
        double rangeFar,
        std::uint32_t cascadeResolution,
        bool& success
    ) const;

    std::array<double, MAX_CASCADES> ComputeCascadeSplits(double nearPlane, double farPlane, int cascadeCount, double lambda) const;
    Math::Matrix4 BuildOrthographicZeroToOne(double left, double right, double bottom, double top, double nearPlane, double farPlane) const;
    Math::Matrix4 StabilizeProjection(const Math::Matrix4& projection, const Math::Matrix4& lightView, double resolution) const;

    VkFormat depthFormat_{VK_FORMAT_UNDEFINED};
    VkRenderPass renderPass_{VK_NULL_HANDLE};
    std::unique_ptr<VulkanPipelineLayout> pipelineLayout_;
    std::unordered_map<Common::PipelineVariantKey, std::unique_ptr<VulkanPipelineVariant>, Common::PipelineVariantKeyHash>
        pipelines_;
    VkSampler shadowSampler_{VK_NULL_HANDLE};
    std::array<ImageResource, MAX_CASCADES> cascadeImages_{};
    std::array<VkImageView, MAX_CASCADES> cascadeImageViews_{VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkImage jitterImage_{VK_NULL_HANDLE};
    VkDeviceMemory jitterMemory_{VK_NULL_HANDLE};
    VkImageView jitterImageView_{VK_NULL_HANDLE};
    VkSampler jitterSampler_{VK_NULL_HANDLE};

    std::uint32_t mapResolution_{0};
    std::array<std::uint32_t, MAX_CASCADES> cascadeResolutions_{128, 128, 128};
    std::uint32_t jitterSizeXY_{16};
    std::uint32_t jitterDepth_{32};
    ShadowData shadowData_{};
    bool initialized_{false};
};

} // namespace Lvs::Engine::Rendering::Vulkan
