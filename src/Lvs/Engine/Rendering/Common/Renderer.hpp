#pragma once

#include "Lvs/Engine/DataModel/Lighting.hpp"
#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Engine/DataModel/Workspace.hpp"
#include "Lvs/Engine/Objects/Camera.hpp"
#include "Lvs/Engine/Objects/DirectionalLight.hpp"
#include "Lvs/Engine/Rendering/Common/MeshCache.hpp"
#include "Lvs/Engine/Rendering/Common/OverlayPrimitive.hpp"
#include "Lvs/Engine/Rendering/Common/SceneRenderer.hpp"
#include "Lvs/Engine/Rendering/RenderingFactory.hpp"

#include <array>
#include <memory>
#include <vector>

namespace Lvs::Engine::Rendering::Common {

class Renderer : public SceneRenderer {
public:
    Renderer(
        std::shared_ptr<::Lvs::Engine::Rendering::RenderingFactory> factory,
        std::unique_ptr<MeshUploader> meshUploader
    );
    ~Renderer() override;

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    void Initialize(GraphicsContext& context, const RenderSurface& surface) override;
    void RecreateSwapchain(GraphicsContext& context, const RenderSurface& surface) override;
    void DestroySwapchainResources(GraphicsContext& context, const RenderSurface& surface) override;
    void Shutdown(GraphicsContext& context) override;

    void BindToPlace(const std::shared_ptr<DataModel::Place>& place) override;
    void Unbind() override;
    void SetOverlayPrimitives(std::vector<OverlayPrimitive> primitives) override;

    [[nodiscard]] MeshCache& GetMeshCache() override;

    void RecordFrameCommands(
        GraphicsContext& context,
        const RenderSurface& surface,
        CommandBuffer& commandBuffer,
        std::uint32_t imageIndex,
        std::uint32_t frameIndex,
        const std::array<float, 4>& clearColor
    ) override;
    void RecordShadowCommands(
        GraphicsContext& context,
        const RenderSurface& surface,
        CommandBuffer& commandBuffer,
        std::uint32_t frameIndex
    ) override;
    void RecordDrawCommands(
        GraphicsContext& context,
        const RenderSurface& surface,
        CommandBuffer& commandBuffer,
        std::uint32_t frameIndex
    ) override;

protected:
    virtual void OnInitialize(GraphicsContext& context, const RenderSurface& surface) = 0;
    virtual void OnRecreateSwapchain(GraphicsContext& context, const RenderSurface& surface) = 0;
    virtual void OnDestroySwapchainResources(GraphicsContext& context, const RenderSurface& surface) = 0;
    virtual void OnShutdown(GraphicsContext& context) = 0;
    virtual void OnBindToPlace(const std::shared_ptr<DataModel::Place>& place) = 0;
    virtual void OnUnbind() = 0;
    virtual void OnOverlayPrimitivesChanged(const std::vector<OverlayPrimitive>& primitives) = 0;
    virtual void OnRecordShadowCommands(
        GraphicsContext& context,
        const RenderSurface& surface,
        CommandBuffer& commandBuffer,
        std::uint32_t frameIndex
    ) = 0;
    virtual void OnRecordDrawCommands(
        GraphicsContext& context,
        const RenderSurface& surface,
        CommandBuffer& commandBuffer,
        std::uint32_t frameIndex
    ) = 0;

    [[nodiscard]] GraphicsContext* GetCurrentContext() const;
    [[nodiscard]] const RenderSurface* GetCurrentSurface() const;
    [[nodiscard]] const std::shared_ptr<DataModel::Place>& GetPlace() const;
    [[nodiscard]] const std::shared_ptr<DataModel::Workspace>& GetWorkspace() const;
    [[nodiscard]] const std::shared_ptr<::Lvs::Engine::Rendering::RenderingFactory>& GetFactory() const;
    [[nodiscard]] const std::vector<OverlayPrimitive>& GetOverlayPrimitives() const;
    [[nodiscard]] std::shared_ptr<Objects::Camera> GetCamera() const;
    [[nodiscard]] std::shared_ptr<DataModel::Lighting> GetLighting() const;
    [[nodiscard]] std::shared_ptr<Objects::DirectionalLight> GetDirectionalLight(
        const std::shared_ptr<DataModel::Lighting>& lighting
    ) const;

private:
    std::shared_ptr<DataModel::Place> place_;
    std::shared_ptr<DataModel::Workspace> workspace_;
    std::shared_ptr<::Lvs::Engine::Rendering::RenderingFactory> factory_;
    GraphicsContext* context_{nullptr};
    const RenderSurface* surface_{nullptr};
    std::unique_ptr<MeshUploader> meshUploader_;
    MeshCache meshCache_;
    std::vector<OverlayPrimitive> overlayPrimitives_;
    bool initialized_{false};
};

} // namespace Lvs::Engine::Rendering::Common
