#pragma once

#include "Lvs/Engine/Rendering/Common/OverlayPrimitive.hpp"
#include "Lvs/Engine/Rendering/RHI/Types.hpp"

#include "Lvs/Engine/Core/EnumTraits.hpp"

#include <memory>
#include <string>
#include <stdexcept>
#include <vector>

namespace Lvs::Engine::DataModel {
class Place;
}

namespace Lvs::Engine::Rendering {

enum class RenderApi {
    Auto,
    Vulkan,
    OpenGL
};

class RenderingInitializationError final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class IRenderContext {
public:
    virtual ~IRenderContext() = default;

    virtual void Initialize(RHI::u32 width, RHI::u32 height) = 0;
    virtual void AttachToNativeWindow(void* nativeWindowHandle, RHI::u32 width, RHI::u32 height) = 0;
    virtual void Resize(RHI::u32 width, RHI::u32 height) = 0;
    virtual void SetClearColor(float r, float g, float b, float a) = 0;
    virtual void BindToPlace(const std::shared_ptr<DataModel::Place>& place) = 0;
    virtual void Unbind() = 0;
    virtual void SetOverlayPrimitives(std::vector<Common::OverlayPrimitive> primitives) = 0;
    virtual void RefreshShaders() = 0;
    virtual void Render() = 0;
};

[[nodiscard]] RenderApi ParseRenderApi(const std::string& value);
[[nodiscard]] std::unique_ptr<IRenderContext> CreateRenderContext(RenderApi preferredApi = RenderApi::Auto);

} // namespace Lvs::Engine::Rendering

template <>
struct Lvs::Engine::Core::EnumTraits<Lvs::Engine::Rendering::RenderApi> {
    static constexpr std::string_view Name = "RenderApi";
};
