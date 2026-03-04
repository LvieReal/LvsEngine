#pragma once

#include "Lvs/Engine/Math/Color3.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanCubemapUtils.hpp"
#include "Lvs/Engine/Utils/Signal.hpp"

#include <vulkan/vulkan.h>

#include <QString>
#include <QVariant>

#include <cstdint>
#include <memory>
#include <optional>
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

class Mesh;
class VulkanContext;

class SkyboxRenderer final {
public:
    SkyboxRenderer() = default;
    ~SkyboxRenderer() = default;

    SkyboxRenderer(const SkyboxRenderer&) = delete;
    SkyboxRenderer& operator=(const SkyboxRenderer&) = delete;
    SkyboxRenderer(SkyboxRenderer&&) = delete;
    SkyboxRenderer& operator=(SkyboxRenderer&&) = delete;

    void Initialize(VulkanContext& context);
    void RecreateSwapchain(VulkanContext& context);
    void Shutdown(VulkanContext& context);

    void BindToPlace(const std::shared_ptr<DataModel::Place>& place);
    void Unbind();

    void UpdateResources(VulkanContext& context);
    void Draw(VulkanContext& context, VkCommandBuffer commandBuffer, std::uint32_t frameIndex, const Objects::Camera& camera);

    [[nodiscard]] const CubemapUtils::CubemapHandle& GetCubemap() const;
    [[nodiscard]] Math::Color3 GetSkyTint() const;

private:
    void CreateDescriptorSetLayout(VulkanContext& context);
    void CreatePipelineLayout(VulkanContext& context);
    void CreateGraphicsPipeline(VulkanContext& context);
    void CreateDescriptorPool(VulkanContext& context);
    void CreateDescriptorSets(VulkanContext& context);
    void UpdateDescriptorSets(VulkanContext& context);
    void DestroySwapchainResources(VulkanContext& context);
    void UpdateSkyFromPlace();

    std::shared_ptr<DataModel::Lighting> GetLighting() const;
    std::shared_ptr<Objects::Skybox> GetSkybox(const std::shared_ptr<DataModel::Lighting>& lighting) const;

    std::shared_ptr<DataModel::Place> place_;
    std::shared_ptr<Mesh> skyboxMesh_;

    VkDescriptorSetLayout descriptorSetLayout_{VK_NULL_HANDLE};
    VkPipelineLayout pipelineLayout_{VK_NULL_HANDLE};
    VkPipeline pipeline_{VK_NULL_HANDLE};
    VkDescriptorPool descriptorPool_{VK_NULL_HANDLE};
    std::vector<VkDescriptorSet> descriptorSets_;

    CubemapUtils::CubemapHandle cubemap_;
    std::shared_ptr<Objects::Skybox> activeSkybox_;
    Math::Color3 skyTint_{1.0, 1.0, 1.0};
    std::optional<Engine::Utils::Signal<const QString&, const QVariant&>::Connection> skyboxPropertyConnection_;
    bool skyboxDirty_{true};
    bool initialized_{false};
};

} // namespace Lvs::Engine::Rendering::Vulkan
