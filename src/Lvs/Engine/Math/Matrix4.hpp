#pragma once

#include "Lvs/Engine/Math/Vector3.hpp"

#include <QMetaType>

#include <array>

namespace Lvs::Engine::Math {

class Matrix4 {
public:
    Matrix4();
    explicit Matrix4(const std::array<std::array<double, 4>, 4>& rows);

    Matrix4 operator-() const;
    Matrix4 operator*(const Matrix4& other) const;

    static Matrix4 Identity();
    Matrix4 Inverse() const;
    static Matrix4 Translation(const Vector3& vector);
    static Matrix4 Scale(const Vector3& vector);

    [[nodiscard]] const std::array<std::array<double, 4>, 4>& Rows() const;
    [[nodiscard]] std::array<double, 16> FlattenRowMajor() const;
    [[nodiscard]] std::array<double, 16> FlattenColumnMajor() const;

private:
    std::array<std::array<double, 4>, 4> m_{};
};

} // namespace Lvs::Engine::Math

Q_DECLARE_METATYPE(Lvs::Engine::Math::Matrix4)
