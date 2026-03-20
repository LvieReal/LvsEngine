#include "Lvs/Engine/Math/Color3.hpp"

#include <iomanip>
#include <sstream>

namespace Lvs::Engine::Math {

Core::String Color3::ToString() const {
    std::ostringstream out;
    out.setf(std::ios::fmtflags(0), std::ios::floatfield);
    out << "Color3(" << std::setprecision(10) << r << ", " << g << ", " << b << ")";
    return out.str();
}

bool Color3::operator==(const Color3& other) const {
    return r == other.r && g == other.g && b == other.b;
}

} // namespace Lvs::Engine::Math
