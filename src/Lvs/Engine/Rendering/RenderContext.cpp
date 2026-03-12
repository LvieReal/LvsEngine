#include "Lvs/Engine/Rendering/IRenderContext.hpp"

#include "Lvs/Engine/Rendering/Context/RenderContext.hpp"

#include <cctype>
#include <string>

namespace Lvs::Engine::Rendering {

RenderApi ParseRenderApi(const std::string& value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (const char ch : value) {
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }

    if (normalized == "vulkan") {
        return RenderApi::Vulkan;
    }
    if (normalized == "opengl" || normalized == "gl") {
        return RenderApi::OpenGL;
    }
    return RenderApi::Auto;
}

std::unique_ptr<IRenderContext> CreateRenderContext(const RenderApi preferredApi) {
    return std::make_unique<RenderContext>(preferredApi);
}

} // namespace Lvs::Engine::Rendering

