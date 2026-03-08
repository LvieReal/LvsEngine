#include "Lvs/Engine/Rendering/Common/Renderer.hpp"

#include <stdexcept>
#include <utility>

namespace Lvs::Engine::Rendering::Common {

Renderer::Renderer(
    std::shared_ptr<::Lvs::Engine::Rendering::RenderingFactory> factory,
    std::unique_ptr<MeshUploader> meshUploader
)
    : place_(nullptr),
      workspace_(nullptr),
      factory_(std::move(factory)),
      meshUploader_(std::move(meshUploader)),
      meshCache_(*meshUploader_) {
    if (meshUploader_ == nullptr) {
        throw std::invalid_argument("Renderer requires a mesh uploader implementation.");
    }
}

Renderer::~Renderer() = default;

void Renderer::Initialize(GraphicsContext& context, const RenderSurface& surface) {
    context_ = &context;
    surface_ = &surface;
    if (initialized_) {
        return;
    }

    meshCache_.Initialize();
    OnInitialize(context, surface);
    initialized_ = true;
}

void Renderer::RecreateSwapchain(GraphicsContext& context, const RenderSurface& surface) {
    context_ = &context;
    surface_ = &surface;
    if (!initialized_) {
        Initialize(context, surface);
        return;
    }
    OnRecreateSwapchain(context, surface);
}

void Renderer::DestroySwapchainResources(GraphicsContext& context, const RenderSurface& surface) {
    context_ = &context;
    surface_ = &surface;
    if (!initialized_) {
        return;
    }
    OnDestroySwapchainResources(context, surface);
}

void Renderer::Shutdown(GraphicsContext& context) {
    context_ = &context;
    if (!initialized_) {
        return;
    }

    OnShutdown(context);
    meshCache_.Clear();
    initialized_ = false;
}

void Renderer::BindToPlace(const std::shared_ptr<DataModel::Place>& place) {
    place_ = place;
    workspace_ = place_ != nullptr ? std::dynamic_pointer_cast<DataModel::Workspace>(place_->FindService("Workspace")) : nullptr;
    OnBindToPlace(place_);
}

void Renderer::Unbind() {
    place_.reset();
    workspace_.reset();
    overlayPrimitives_.clear();
    OnUnbind();
}

void Renderer::SetOverlayPrimitives(std::vector<OverlayPrimitive> primitives) {
    overlayPrimitives_ = std::move(primitives);
    OnOverlayPrimitivesChanged(overlayPrimitives_);
}

MeshCache& Renderer::GetMeshCache() {
    return meshCache_;
}

void Renderer::RecordFrameCommands(
    GraphicsContext& context,
    const RenderSurface& surface,
    CommandBuffer& commandBuffer,
    const std::uint32_t imageIndex,
    const std::uint32_t frameIndex,
    const std::array<float, 4>& clearColor
) {
    static_cast<void>(imageIndex);
    static_cast<void>(clearColor);
    RecordShadowCommands(context, surface, commandBuffer, frameIndex);
    RecordDrawCommands(context, surface, commandBuffer, frameIndex);
}

void Renderer::RecordShadowCommands(
    GraphicsContext& context,
    const RenderSurface& surface,
    CommandBuffer& commandBuffer,
    const std::uint32_t frameIndex
) {
    context_ = &context;
    surface_ = &surface;
    if (!initialized_ || place_ == nullptr || workspace_ == nullptr) {
        return;
    }
    OnRecordShadowCommands(context, surface, commandBuffer, frameIndex);
}

void Renderer::RecordDrawCommands(
    GraphicsContext& context,
    const RenderSurface& surface,
    CommandBuffer& commandBuffer,
    const std::uint32_t frameIndex
) {
    context_ = &context;
    surface_ = &surface;
    if (!initialized_ || place_ == nullptr || workspace_ == nullptr) {
        return;
    }
    OnRecordDrawCommands(context, surface, commandBuffer, frameIndex);
}

GraphicsContext* Renderer::GetCurrentContext() const {
    return context_;
}

const RenderSurface* Renderer::GetCurrentSurface() const {
    return surface_;
}

const std::shared_ptr<DataModel::Place>& Renderer::GetPlace() const {
    return place_;
}

const std::shared_ptr<DataModel::Workspace>& Renderer::GetWorkspace() const {
    return workspace_;
}

const std::shared_ptr<::Lvs::Engine::Rendering::RenderingFactory>& Renderer::GetFactory() const {
    return factory_;
}

const std::vector<OverlayPrimitive>& Renderer::GetOverlayPrimitives() const {
    return overlayPrimitives_;
}

std::shared_ptr<Objects::Camera> Renderer::GetCamera() const {
    if (workspace_ == nullptr) {
        return nullptr;
    }
    return workspace_->GetProperty("CurrentCamera").value<std::shared_ptr<Objects::Camera>>();
}

std::shared_ptr<DataModel::Lighting> Renderer::GetLighting() const {
    if (place_ == nullptr) {
        return nullptr;
    }
    return std::dynamic_pointer_cast<DataModel::Lighting>(place_->FindService("Lighting"));
}

std::shared_ptr<Objects::DirectionalLight> Renderer::GetDirectionalLight(
    const std::shared_ptr<DataModel::Lighting>& lighting
) const {
    if (lighting == nullptr) {
        return nullptr;
    }
    for (const auto& child : lighting->GetChildren()) {
        if (const auto light = std::dynamic_pointer_cast<Objects::DirectionalLight>(child); light != nullptr) {
            return light;
        }
    }
    return nullptr;
}

} // namespace Lvs::Engine::Rendering::Common
