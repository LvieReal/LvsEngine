#include "Lvs/Engine/Rendering/Common/RenderPolicyResolver.hpp"

#include <algorithm>

namespace Lvs::Engine::Rendering::Common {

FrameRenderData RenderPolicyResolver::Resolve(
    const std::vector<std::shared_ptr<RenderProxy>>& proxies,
    const Math::Vector3& cameraPosition
) const {
    FrameRenderData frameData{};
    frameData.CameraPosition = cameraPosition;
    frameData.OpaqueDraws.reserve(proxies.size());
    frameData.TransparentDraws.reserve(proxies.size());
    frameData.ShadowCasters.reserve(proxies.size());

    for (const auto& proxy : proxies) {
        if (proxy == nullptr) {
            continue;
        }

        const auto policy = proxy->GetPolicy();
        if (!policy.Visible) {
            continue;
        }

        if (policy.Transparent) {
            frameData.TransparentDraws.push_back(proxy);
        } else {
            frameData.OpaqueDraws.push_back(proxy);
        }
        if (policy.CastsShadow) {
            frameData.ShadowCasters.push_back(proxy);
        }
    }

    std::sort(
        frameData.TransparentDraws.begin(),
        frameData.TransparentDraws.end(),
        [&cameraPosition](const std::shared_ptr<RenderProxy>& lhs, const std::shared_ptr<RenderProxy>& rhs) {
            const double lhsDistance = (lhs->GetWorldPosition() - cameraPosition).MagnitudeSquared();
            const double rhsDistance = (rhs->GetWorldPosition() - cameraPosition).MagnitudeSquared();
            return lhsDistance > rhsDistance;
        }
    );

    return frameData;
}

} // namespace Lvs::Engine::Rendering::Common

