#pragma once

#include <string_view>

namespace Lvs::Engine::Rendering {

enum class RenderingApi {
    Auto,
    Vulkan
};

[[nodiscard]] RenderingApi ParseRenderingApi(std::string_view value);
[[nodiscard]] const char* ToString(RenderingApi api);
[[nodiscard]] RenderingApi ResolveSupportedRenderingApi(RenderingApi requested);

} // namespace Lvs::Engine::Rendering
