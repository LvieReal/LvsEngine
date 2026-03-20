#include "Lvs/Engine/Math/Vector3.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace Lvs::Engine::Math {

Vector3 Vector3::operator-() const {
    return {-x, -y, -z};
}

Vector3 Vector3::operator+(const Vector3& other) const {
    return {x + other.x, y + other.y, z + other.z};
}

Vector3 Vector3::operator-(const Vector3& other) const {
    return {x - other.x, y - other.y, z - other.z};
}

Vector3 Vector3::operator*(const double scalar) const {
    return {x * scalar, y * scalar, z * scalar};
}

Vector3 operator*(const double scalar, const Vector3& vector) {
    return vector * scalar;
}

std::array<double, 3> Vector3::ToArray() const {
    return {x, y, z};
}

double Vector3::Dot(const Vector3& other) const {
    return x * other.x + y * other.y + z * other.z;
}

Vector3 Vector3::Cross(const Vector3& other) const {
    return {
        y * other.z - z * other.y,
        z * other.x - x * other.z,
        x * other.y - y * other.x
    };
}

double Vector3::MagnitudeSquared() const {
    return Dot(*this);
}

double Vector3::Magnitude() const {
    return std::sqrt(MagnitudeSquared());
}

Vector3 Vector3::Unit() const {
    const double mag = Magnitude();
    if (mag == 0.0) {
        return {};
    }
    return *this * (1.0 / mag);
}

Core::String Vector3::ToString() const {
    std::ostringstream out;
    out.setf(std::ios::fmtflags(0), std::ios::floatfield);
    out << "Vector3(" << std::setprecision(10) << x << ", " << y << ", " << z << ")";
    return out.str();
}

bool Vector3::operator==(const Vector3& other) const {
    return x == other.x && y == other.y && z == other.z;
}

} // namespace Lvs::Engine::Math
