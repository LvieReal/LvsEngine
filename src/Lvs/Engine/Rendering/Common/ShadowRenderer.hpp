#pragma once

#include "Lvs/Engine/Math/Matrix4.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"
#include "Lvs/Engine/Rendering/Common/CommandBuffer.hpp"
#include "Lvs/Engine/Rendering/Common/GraphicsContext.hpp"
#include "Lvs/Engine/Rendering/Common/RenderProxy.hpp"
#include "Lvs/Engine/Rendering/Common/ResourceBinding.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace Lvs::Engine::Objects {
class Camera;
}

namespace Lvs::Engine::Rendering::Common {

class ShadowRenderer {
public:
    struct ShadowSettings {
        bool Enabled{false};
        float BlurAmount{0.0F};
        int TapCount{16};
        int CascadeCount{1};
        float MaxDistance{220.0F};
        std::uint32_t MapResolution{4096};
        float CascadeResolutionScale{0.7F};
        float CascadeSplitLambda{0.75F};
    };

    struct ShadowData {
        bool HasShadowData{false};
        int CascadeCount{1};
        float Split0{220.0F};
        float Split1{220.0F};
        float MaxDistance{220.0F};
        float Bias{0.25F};
        float BlurAmount{0.0F};
        float FadeWidth{0.25F};
        int TapCount{16};
        float JitterScaleX{1.0F / 16.0F};
        float JitterScaleY{1.0F / 16.0F};
        std::array<Math::Matrix4, 3> LightViewProjectionMatrices{
            Math::Matrix4::Identity(),
            Math::Matrix4::Identity(),
            Math::Matrix4::Identity()
        };
    };

    virtual ~ShadowRenderer() = default;

    virtual void Initialize(GraphicsContext& context) = 0;
    virtual void RecreateSwapchain(GraphicsContext& context) = 0;
    virtual void Shutdown(GraphicsContext& context) = 0;

    virtual void Render(
        GraphicsContext& context,
        CommandBuffer& commandBuffer,
        const std::vector<std::shared_ptr<RenderProxy>>& shadowCasters,
        const Objects::Camera& camera,
        const Math::Vector3& directionalLightDirection,
        float cameraAspect,
        const ShadowSettings& settings
    ) = 0;

    virtual void WriteSceneBinding(GraphicsContext& context, ResourceBinding& binding) const = 0;
    [[nodiscard]] virtual const ShadowData& GetShadowData() const = 0;
};

} // namespace Lvs::Engine::Rendering::Common
