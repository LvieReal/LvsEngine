#pragma once

#include "Lvs/Engine/Math/Matrix4.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"


namespace Lvs::Engine::Math {

class CFrame {
public:
    CFrame();
    CFrame(Vector3 position, Vector3 right, Vector3 up, Vector3 back);

    [[nodiscard]] Vector3 GetRotation() const;
    [[nodiscard]] CFrame RotationOnly() const;
    [[nodiscard]] Vector3 RightVector() const;
    [[nodiscard]] Vector3 UpVector() const;
    [[nodiscard]] Vector3 LookVector() const;

    static CFrame Identity();
    static CFrame New(const Vector3& position);
    static CFrame LookAt(const Vector3& eye, const Vector3& target, const Vector3& up = {0.0, 1.0, 0.0});
    static CFrame FromPositionRotation(const Vector3& position, const Vector3& rotationDeg);
    static CFrame FromMatrix(const Matrix4& matrix);

    [[nodiscard]] Matrix4 ToMatrix4() const;
    [[nodiscard]] Matrix4 ToMatrix4NoTranslation() const;
    [[nodiscard]] CFrame Inverse() const;
    [[nodiscard]] Vector3 ToEulerXYZ() const;

    CFrame operator*(const CFrame& other) const;
    [[nodiscard]] bool operator==(const CFrame& other) const;

    Vector3 Position;
    Vector3 Right;
    Vector3 Up;
    Vector3 Back;
};

} // namespace Lvs::Engine::Math
