#pragma once

#include "Lvs/Engine/Math/Matrix4.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"
#include "Lvs/Engine/Rendering/Common/CommandBuffer.hpp"
#include "Lvs/Engine/Rendering/Common/GraphicsContext.hpp"
#include "Lvs/Engine/Rendering/Common/RenderProxy.hpp"
#include "Lvs/Engine/Rendering/Common/ShadowCascadeUtils.hpp"
#include "Lvs/Engine/Rendering/Common/ShadowJitterUtils.hpp"
#include "Lvs/Engine/Rendering/Common/RenderSettingsSnapshot.hpp"
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
    struct ShadowData {
        bool HasShadowData{false};
        int CascadeCount{1};
        float Split0{kShadowDefaultMaxDistance};
        float Split1{kShadowDefaultMaxDistance};
        float MaxDistance{kShadowDefaultMaxDistance};
        float Bias{kShadowDefaultBias};
        float BlurAmount{kShadowMinBlurAmount};
        float FadeWidth{kShadowDefaultFadeWidth};
        int TapCount{kShadowDefaultTapCount};
        float JitterScaleX{1.0F / static_cast<float>(kShadowDefaultJitterSizeXY)};
        float JitterScaleY{1.0F / static_cast<float>(kShadowDefaultJitterSizeXY)};
        std::array<Math::Matrix4, 3> LightViewProjectionMatrices{
            Math::Matrix4::Identity(),
            Math::Matrix4::Identity(),
            Math::Matrix4::Identity()
        };
    };

    struct ShadowPassInput {
        const std::vector<std::shared_ptr<RenderProxy>>* Casters{nullptr};
        const Objects::Camera* Camera{nullptr};
        Math::Vector3 DirectionalLightDirection{};
        float CameraAspect{1.0F};
        ShadowQualityProfile Quality{};
    };

    struct ShadowPassOutput {
        ShadowData Data{};
    };

    virtual ~ShadowRenderer() = default;

    virtual void Initialize(GraphicsContext& context) = 0;
    virtual void RecreateSwapchain(GraphicsContext& context) = 0;
    virtual void Shutdown(GraphicsContext& context) = 0;

    virtual ShadowPassOutput Render(
        GraphicsContext& context,
        CommandBuffer& commandBuffer,
        const ShadowPassInput& input
    ) = 0;

    virtual void WriteSceneBinding(GraphicsContext& context, ResourceBinding& binding) const = 0;
    [[nodiscard]] virtual const ShadowData& GetShadowData() const = 0;
};

} // namespace Lvs::Engine::Rendering::Common
