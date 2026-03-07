#include "Lvs/Engine/Rendering/RenderingApi.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace Lvs::Engine::Rendering {

RenderingApi ParseRenderingApi(std::string_view value) {
    std::string lowered(value);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](const unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (lowered == "vulkan") {
        return RenderingApi::Vulkan;
    }
    return RenderingApi::Auto;
}

const char* ToString(const RenderingApi api) {
    switch (api) {
        case RenderingApi::Vulkan:
            return "Vulkan";
        case RenderingApi::Auto:
        default:
            return "Auto";
    }
}

RenderingApi ResolveSupportedRenderingApi(const RenderingApi requested) {
    switch (requested) {
        case RenderingApi::Vulkan:
        case RenderingApi::Auto:
        default:
            return RenderingApi::Vulkan;
    }
}

} // namespace Lvs::Engine::Rendering
