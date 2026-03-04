#pragma once

#include "Lvs/Engine/Math/Matrix4.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace Lvs::Engine::Objects {
class Camera;
}

namespace Lvs::Engine::Rendering::Vulkan {

class RenderPartProxy;
class VulkanContext;

class ShadowRenderer final {
public:
    struct ShadowSettings {
        bool Enabled{false};
        float BlurAmount{0.0F};
        int TapCount{16};
        int CascadeCount{1};
        float MaxDistance{220.0F};
        std::uint32_t MapResolution{4096};
    };

    struct ShadowData {
        bool HasShadowData{false};
        int CascadeCount{1};
        float Split0{220.0F};
        float Split1{220.0F};
        float MaxDistance{220.0F};
        float Bias{0.25F};
        float BlurAmount{0.0F};
        float FadeWidth{0.25F};
        int TapCount{16};
        float JitterScaleX{1.0F / 16.0F};
        float JitterScaleY{1.0F / 16.0F};
        std::array<Math::Matrix4, 3> LightViewProjectionMatrices{
            Math::Matrix4::Identity(),
            Math::Matrix4::Identity(),
            Math::Matrix4::Identity()
        };
    };

    ShadowRenderer() = default;
    ~ShadowRenderer() = default;

    ShadowRenderer(const ShadowRenderer&) = delete;
    ShadowRenderer& operator=(const ShadowRenderer&) = delete;
    ShadowRenderer(ShadowRenderer&&) = delete;
    ShadowRenderer& operator=(ShadowRenderer&&) = delete;

    void Initialize(VulkanContext& context);
    void RecreateSwapchain(VulkanContext& context);
    void Shutdown(VulkanContext& context);

    void Render(
        VulkanContext& context,
        VkCommandBuffer commandBuffer,
        const std::vector<std::shared_ptr<RenderPartProxy>>& shadowCasters,
        const Objects::Camera& camera,
        const Math::Vector3& directionalLightDirection,
        float cameraAspect,
        const ShadowSettings& settings
    );

    [[nodiscard]] const ShadowData& GetShadowData() const;
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
    void CreatePipeline(VulkanContext& context);
    void EnsureDepthResources(VulkanContext& context, std::uint32_t resolution);
    void EnsureJitterTexture(VulkanContext& context);
    void DestroyDepthResources(VulkanContext& context);
    void DestroyJitterTexture(VulkanContext& context);
    void DestroySwapchainResources(VulkanContext& context);

    bool ComputeCascades(
        const Objects::Camera& camera,
        const Math::Vector3& directionalLightDirection,
        float cameraAspect,
        int cascadeCount,
        float maxDistance,
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

    std::array<double, MAX_CASCADES> ComputeCascadeSplits(double nearPlane, double farPlane, int cascadeCount) const;
    Math::Matrix4 BuildOrthographicZeroToOne(double left, double right, double bottom, double top, double nearPlane, double farPlane) const;
    Math::Matrix4 StabilizeProjection(const Math::Matrix4& projection, const Math::Matrix4& lightView, double resolution) const;

    VkFormat depthFormat_{VK_FORMAT_UNDEFINED};
    VkRenderPass renderPass_{VK_NULL_HANDLE};
    VkPipelineLayout pipelineLayout_{VK_NULL_HANDLE};
    VkPipeline pipeline_{VK_NULL_HANDLE};
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
