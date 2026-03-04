#include "Lvs/Engine/Math/Color3.hpp"

namespace Lvs::Engine::Math {

QString Color3::ToString() const {
    return QString("Color3(%1, %2, %3)").arg(r).arg(g).arg(b);
}

bool Color3::operator==(const Color3& other) const {
    return r == other.r && g == other.g && b == other.b;
}

} // namespace Lvs::Engine::Math
