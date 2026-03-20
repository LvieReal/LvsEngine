#pragma once

#include "Lvs/Engine/Core/Types.hpp"

#include <array>

namespace Lvs::Engine::Math {

struct Vector3 {
    double x{0.0};
    double y{0.0};
    double z{0.0};

    Vector3 operator-() const;
    Vector3 operator+(const Vector3& other) const;
    Vector3 operator-(const Vector3& other) const;
    Vector3 operator*(double scalar) const;
    friend Vector3 operator*(double scalar, const Vector3& vector);

    [[nodiscard]] std::array<double, 3> ToArray() const;
    [[nodiscard]] double Dot(const Vector3& other) const;
    [[nodiscard]] Vector3 Cross(const Vector3& other) const;
    [[nodiscard]] double MagnitudeSquared() const;
    [[nodiscard]] double Magnitude() const;
    [[nodiscard]] Vector3 Unit() const;

    [[nodiscard]] Core::String ToString() const;
    [[nodiscard]] bool operator==(const Vector3& other) const;
};

} // namespace Lvs::Engine::Math
