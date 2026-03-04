#pragma once

#include "Lvs/Engine/Math/Matrix4.hpp"

namespace Lvs::Engine::Math::Projection {

Matrix4 ReversedInfinitePerspective(double fovYDeg, double aspect, double nearPlane);

} // namespace Lvs::Engine::Math::Projection
