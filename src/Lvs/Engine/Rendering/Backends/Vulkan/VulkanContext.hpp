#pragma once

#include "Lvs/Engine/Rendering/Backends/Vulkan/VulkanApi.hpp"
#include "Lvs/Engine/Rendering/Renderer.hpp"
#include "Lvs/Engine/Rendering/RHI/IContext.hpp"

#include <stdexcept>
#include <optional>
#include <unordered_map>
#include <vector>

namespace Lvs::Engine::Rendering::Backends::Vulkan {

class VulkanCommandBuffer;

class VulkanInitializationError final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class VulkanContext final : public RHI::IContext {
public:
    explicit VulkanContext(VulkanApi api);
    ~VulkanContext() override;
    std::unique_ptr<RHI::ICommandBuffer> AllocateCommandBuffer() override;
    std::unique_ptr<RHI::IPipeline> CreatePipeline(const RHI::PipelineDesc& desc) override;
    std::unique_ptr<RHI::IRenderTarget> CreateRenderTarget(const RHI::RenderTargetDesc& desc) override;
    std::unique_ptr<RHI::IBuffer> CreateBuffer(const RHI::BufferDesc& desc) override;
    std::unique_ptr<RHI::IResourceSet> CreateResourceSet(const RHI::ResourceSetDesc& desc) override;
    [[nodiscard]] RHI::Texture CreateTexture2D(const RHI::Texture2DDesc& desc) override;
    [[nodiscard]] RHI::Texture CreateTexture3D(const RHI::Texture3DDesc& desc) override;
    [[nodiscard]] RHI::Texture CreateTextureCube(const RHI::CubemapDesc& desc) override;
    void DestroyTexture(RHI::Texture& texture) override;
    void BindTexture(RHI::u32 slot, const RHI::Texture& texture) override;
    [[nodiscard]] void* GetDefaultRenderPassHandle() const override;
    [[nodiscard]] void* GetDefaultFramebufferHandle() const override;

    void Initialize(RHI::u32 width, RHI::u32 height);
    void Resize(RHI::u32 width, RHI::u32 height);
    void Render(const ::Lvs::Engine::Rendering::SceneData& sceneData);
    void WaitIdle();

    void FreeCommandBuffer(VkCommandBuffer commandBuffer);
    void BeginRenderPass(VkCommandBuffer commandBuffer, const RHI::RenderPassInfo& info) const;
    void EndRenderPass(VkCommandBuffer commandBuffer) const;
    void BindPipeline(VkCommandBuffer commandBuffer, const RHI::IPipeline& pipeline) const;
    void BindVertexBuffer(VkCommandBuffer commandBuffer, RHI::u32 slot, const RHI::IBuffer& buffer, std::size_t offset) const;
    void BindIndexBuffer(VkCommandBuffer commandBuffer, const RHI::IBuffer& buffer, RHI::IndexType indexType, std::size_t offset) const;
    void BindResourceSet(VkCommandBuffer commandBuffer, RHI::u32 slot, const RHI::IResourceSet& set) const;
    void PushConstants(VkCommandBuffer commandBuffer, const void* data, std::size_t size) const;
    void Draw(VkCommandBuffer commandBuffer, RHI::u32 vertexCount) const;
    void DrawIndexed(VkCommandBuffer commandBuffer, RHI::u32 indexCount) const;

private:
    [[nodiscard]] VkShaderModule CreateShaderModule(const std::vector<std::uint32_t>& spirv) const;
    void InitializeBackendObjects();
    void EnsureApiBootstrap();
    void CreateSurfaceIfNeeded();
    bool RecreateSurfaceAndSwapchain();
    void RecreateSwapchain();
    void InitializeSwapchainImageLayouts();
    void DestroySwapchain();
    void RecreateColorAttachment();
    void DestroyColorAttachment();
    void RecreateDepthAttachment();
    void DestroyDepthAttachment();
    void RecreateFramebuffer();
    void RecreateRenderFinishedSemaphores();
    [[nodiscard]] std::uint32_t FindMemoryType(std::uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    VulkanApi api_;
    std::unique_ptr<::Lvs::Engine::Rendering::Renderer> renderer_;
    std::unique_ptr<VulkanCommandBuffer> cmdBuffer_;
    RHI::u32 frameIndex_{0};
    std::unordered_map<RHI::u32, RHI::Texture> textureSlots_;
    VkImage ownedColorImage_{VK_NULL_HANDLE};
    VkDeviceMemory ownedColorMemory_{VK_NULL_HANDLE};
    VkImage ownedDepthImage_{VK_NULL_HANDLE};
    VkDeviceMemory ownedDepthMemory_{VK_NULL_HANDLE};
    VkSemaphore imageAvailableSemaphore_{VK_NULL_HANDLE};
    std::vector<VkSemaphore> renderFinishedSemaphores_{};
    VkFence inFlightFence_{VK_NULL_HANDLE};
    std::uint32_t currentSwapchainImage_{0};
    std::optional<std::uint32_t> graphicsQueueFamily_;
    std::optional<std::uint32_t> presentQueueFamily_;
    bool depthClampEnabled_{false};
    bool ownsInstance_{false};
    bool ownsDevice_{false};
    bool ownsCommandPool_{false};
    bool pendingSwapchainRebuild_{false};
    VkDebugUtilsMessengerEXT debugMessenger_{VK_NULL_HANDLE};
    bool validationLayersEnabled_{false};
    struct OwnedCubeTexture {
        VkImage Image{VK_NULL_HANDLE};
        VkDeviceMemory Memory{VK_NULL_HANDLE};
        VkImageView View{VK_NULL_HANDLE};
        VkSampler Sampler{VK_NULL_HANDLE};
        std::uint32_t MipLevels{1};
    };
    using OwnedTexture2D = OwnedCubeTexture;
    std::unordered_map<VkImageView, OwnedCubeTexture> ownedCubeTextures_{};
    std::unordered_map<VkImageView, OwnedTexture2D> owned2DTextures_{};
    std::unordered_map<VkImageView, OwnedCubeTexture> owned3DTextures_{};
};

} // namespace Lvs::Engine::Rendering::Backends::Vulkan
