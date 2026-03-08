#pragma once

#include <vulkan/vulkan.h>

#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Engine/Rendering/Common/GraphicsContext.hpp"
#include "Lvs/Engine/Rendering/Common/OverlayPrimitive.hpp"
#include "Lvs/Engine/Rendering/Common/SceneRenderer.hpp"
#include "Lvs/Engine/Rendering/RenderingApi.hpp"
#include "Lvs/Engine/Rendering/RenderingFactory.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace Lvs::Engine::Core {
class Window;
}

namespace Lvs::Engine::Rendering::Vulkan {

class SkyboxRenderer;
class VulkanInitializationError final : public std::runtime_error {
public:
    enum class Reason {
        UnsupportedApi,
        NoPhysicalDevices,
        NoSuitableDevice
    };

    VulkanInitializationError(Reason reason, std::string message);

    [[nodiscard]] Reason GetReason() const noexcept;

private:
    Reason reason_;
};

class VulkanRenderer;
class VulkanFrameManager;

class VulkanContext final : public Rendering::Common::GraphicsContext {
public:
    VulkanContext();
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;
    VulkanContext(VulkanContext&&) = delete;
    VulkanContext& operator=(VulkanContext&&) = delete;

    void Initialize();
    void AttachToNativeWindow(void* nativeWindowHandle, std::uint32_t width, std::uint32_t height);
    void Resize(std::uint32_t width, std::uint32_t height);
    void Render();
    void SetClearColor(float r, float g, float b, float a);
    void SetOverlayPrimitives(std::vector<Rendering::Common::OverlayPrimitive> primitives);
    void BindToPlace(const std::shared_ptr<DataModel::Place>& place);
    void Unbind();
    void Shutdown();

    [[nodiscard]] VkInstance GetInstance() const;
    [[nodiscard]] VkPhysicalDevice GetPhysicalDevice() const;
    [[nodiscard]] VkDevice GetDevice() const;
    [[nodiscard]] VkQueue GetGraphicsQueue() const;
    [[nodiscard]] void* GetNativeDevice() const override;
    [[nodiscard]] void* GetNativePhysicalDevice() const override;
    [[nodiscard]] void* GetNativeGraphicsQueue() const override;
    [[nodiscard]] std::uint32_t GetGraphicsQueueFamily() const override;
    [[nodiscard]] VkFormat GetSwapchainImageFormat() const;
    [[nodiscard]] VkFormat GetOffscreenImageFormat() const;
    [[nodiscard]] VkExtent2D GetSwapchainExtentVk() const;
    [[nodiscard]] Rendering::Common::Extent2D GetSwapchainExtent() const override;
    [[nodiscard]] std::uint32_t GetFramesInFlight() const override;

    [[nodiscard]] std::unique_ptr<Rendering::Common::BufferResource> CreateBuffer(
        const Rendering::Common::BufferDesc& desc
    ) override;
    [[nodiscard]] std::unique_ptr<Rendering::Common::ImageResource> CreateImage(
        const Rendering::Common::ImageDesc& desc
    ) override;
    [[nodiscard]] std::unique_ptr<Rendering::Common::SamplerResource> CreateSampler(
        const Rendering::Common::SamplerDesc& desc
    ) override;

private:
    friend class VulkanRenderer;
    friend class SkyboxRenderer;

    [[nodiscard]] VkRenderPass GetSceneRenderPassHandle() const;
    [[nodiscard]] VkRenderPass GetPostProcessRenderPassHandle() const;
    void EnsureRenderingFactory();
    void CreateInstance();
    void CreateSurface(void* nativeWindowHandle);
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void RecordCommandBuffer(VkCommandBuffer commandBuffer, std::uint32_t imageIndex, std::uint32_t frameIndex);
    void RecreateSwapchain(std::uint32_t width, std::uint32_t height);
    void CleanupDeviceAndSurface();
    bool IsPhysicalDeviceSuitable(VkPhysicalDevice device) const;

    std::uint32_t FindGraphicsPresentQueueFamily(VkPhysicalDevice device) const;

    VkInstance instance_{VK_NULL_HANDLE};
    VkSurfaceKHR surface_{VK_NULL_HANDLE};
    VkPhysicalDevice physicalDevice_{VK_NULL_HANDLE};
    VkDevice device_{VK_NULL_HANDLE};
    VkQueue graphicsQueue_{VK_NULL_HANDLE};
    std::uint32_t graphicsQueueFamily_{0};

    void* nativeWindowHandle_{nullptr};
    std::uint32_t lastWidth_{0};
    std::uint32_t lastHeight_{0};
    std::uint32_t instanceApiVersion_{VK_API_VERSION_1_0};
    VkClearColorValue clearColor_{{1.0F, 1.0F, 1.0F, 1.0F}};
    std::vector<Rendering::Common::OverlayPrimitive> overlayPrimitives_;
    Rendering::RenderingApi requestedApi_{Rendering::RenderingApi::Auto};
    Rendering::RenderingApi activeApi_{Rendering::RenderingApi::Auto};
    std::shared_ptr<Rendering::RenderingFactory> renderingFactory_;
    std::unique_ptr<VulkanFrameManager> frameManager_;
    std::unique_ptr<Rendering::Common::SceneRenderer> renderer_;
    std::shared_ptr<DataModel::Place> currentPlace_;
};

} // namespace Lvs::Engine::Rendering::Vulkan
