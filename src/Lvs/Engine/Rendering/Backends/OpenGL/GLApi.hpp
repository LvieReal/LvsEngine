#pragma once

namespace Lvs::Engine::Rendering::Backends::OpenGL {

struct GLApi {
    bool GladLoaded{false};
    void* ContextHandle{nullptr};
    void* NativeWindowHandle{nullptr};
    unsigned int DefaultFramebuffer{0U};
};

} // namespace Lvs::Engine::Rendering::Backends::OpenGL
