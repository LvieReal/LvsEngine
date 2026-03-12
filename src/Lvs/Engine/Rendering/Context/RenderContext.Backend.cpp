#include "Lvs/Engine/Rendering/Context/RenderContext.hpp"

#include "Lvs/Engine/Rendering/Backends/OpenGL/GLContext.hpp"
#include "Lvs/Engine/Rendering/Backends/Vulkan/VulkanContext.hpp"
#include "Lvs/Engine/Rendering/Context/RenderContextUtils.hpp"

namespace Lvs::Engine::Rendering {

void RenderContext::WaitForBackendIdle() {
    if (vkBackend_ != nullptr) {
        vkBackend_->WaitIdle();
    }
    if (glBackend_ != nullptr) {
        glBackend_->WaitIdle();
    }
}

void RenderContext::EnsureBackend() {
    if (vkBackend_ != nullptr || glBackend_ != nullptr) {
        return;
    }

    activeApi_ = Context::ResolveApi(preferredApi_);
    if (activeApi_ == RenderApi::Vulkan) {
        vkBackend_ = std::make_unique<Backends::Vulkan::VulkanContext>(vkApi_);
    } else {
        glBackend_ = std::make_unique<Backends::OpenGL::GLContext>(glApi_);
    }
}

RHI::IContext& RenderContext::GetRhiContext() {
    if (vkBackend_ != nullptr) {
        return *vkBackend_;
    }
    return *glBackend_;
}

} // namespace Lvs::Engine::Rendering

