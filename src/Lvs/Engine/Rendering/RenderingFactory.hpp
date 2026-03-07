#pragma once

#include "Lvs/Engine/Rendering/Common/PostProcessRenderer.hpp"
#include "Lvs/Engine/Rendering/Common/SceneRenderer.hpp"
#include "Lvs/Engine/Rendering/Common/ShadowRenderer.hpp"
#include "Lvs/Engine/Rendering/Common/SkyboxRenderer.hpp"
#include "Lvs/Engine/Rendering/RenderingApi.hpp"

#include <memory>

namespace Lvs::Engine::Rendering {

class RenderingFactory {
public:
    virtual ~RenderingFactory() = default;

    [[nodiscard]] virtual RenderingApi GetApi() const = 0;
    [[nodiscard]] virtual std::unique_ptr<Common::SceneRenderer> CreateSceneRenderer() = 0;
    [[nodiscard]] virtual std::unique_ptr<Common::ShadowRenderer> CreateShadowRenderer() = 0;
    [[nodiscard]] virtual std::unique_ptr<Common::SkyboxRenderer> CreateSkyboxRenderer() = 0;
    [[nodiscard]] virtual std::unique_ptr<Common::PostProcessRenderer> CreatePostProcessRenderer() = 0;
};

[[nodiscard]] std::shared_ptr<RenderingFactory> CreateRenderingFactory(RenderingApi requestedApi = RenderingApi::Auto);

} // namespace Lvs::Engine::Rendering
