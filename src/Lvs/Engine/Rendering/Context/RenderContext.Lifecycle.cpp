#include "Lvs/Engine/Rendering/Context/RenderContext.hpp"

#include "Lvs/Engine/Rendering/Backends/OpenGL/GLContext.hpp"
#include "Lvs/Engine/Rendering/Backends/Vulkan/VulkanContext.hpp"
#include "Lvs/Engine/Rendering/Context/RenderContextUtils.hpp"

namespace Lvs::Engine::Rendering {

RenderContext::RenderContext(const RenderApi preferredApi)
    : preferredApi_(preferredApi),
      activeApi_(Context::ResolveApi(preferredApi_)) {}

RenderContext::~RenderContext() {
    WaitForBackendIdle();
    ReleaseGpuResources();
    vkBackend_.reset();
    glBackend_.reset();
}

void RenderContext::Initialize(const RHI::u32 width, const RHI::u32 height) {
    surfaceWidth_ = width;
    surfaceHeight_ = height;
    if (nativeWindowHandle_ == nullptr) {
        return;
    }
    EnsureBackend();
    if (vkBackend_ != nullptr) {
        vkBackend_->Initialize(width, height);
    } else if (glBackend_ != nullptr) {
        glBackend_->Initialize(width, height);
    } else {
        throw RenderingInitializationError("No render backend available");
    }
    InitializeGeometryBuffers();
}

void RenderContext::AttachToNativeWindow(void* nativeWindowHandle, const RHI::u32 width, const RHI::u32 height) {
    WaitForBackendIdle();
    ReleaseGpuResources();
    nativeWindowHandle_ = nativeWindowHandle;
    vkApi_.NativeWindowHandle = nativeWindowHandle_;
    glApi_.NativeWindowHandle = nativeWindowHandle_;
    glApi_.ContextHandle = nullptr;
    vkBackend_.reset();
    glBackend_.reset();
    Initialize(width, height);
}

void RenderContext::Resize(const RHI::u32 width, const RHI::u32 height) {
    surfaceWidth_ = width;
    surfaceHeight_ = height;
    if (nativeWindowHandle_ == nullptr) {
        return;
    }
    EnsureBackend();
    if (vkBackend_ != nullptr) {
        vkBackend_->Resize(width, height);
    } else if (glBackend_ != nullptr) {
        glBackend_->Resize(width, height);
    }
}

void RenderContext::SetClearColor(const float r, const float g, const float b, const float a) {
    clearColor_[0] = r;
    clearColor_[1] = g;
    clearColor_[2] = b;
    clearColor_[3] = a;
}

void RenderContext::BindToPlace(const std::shared_ptr<DataModel::Place>& place) {
    place_ = place;
}

void RenderContext::Unbind() {
    place_.reset();
    overlayPrimitives_.clear();
}

void RenderContext::SetOverlayPrimitives(std::vector<Common::OverlayPrimitive> primitives) {
    overlayPrimitives_ = std::move(primitives);
}

} // namespace Lvs::Engine::Rendering
