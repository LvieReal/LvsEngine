#include "Lvs/Engine/Math/CFrame.hpp"

#include <cmath>

namespace Lvs::Engine::Math {

CFrame::CFrame()
    : Position{},
      Right{1.0, 0.0, 0.0},
      Up{0.0, 1.0, 0.0},
      Back{0.0, 0.0, 1.0} {
}

CFrame::CFrame(Vector3 position, Vector3 right, Vector3 up, Vector3 back)
    : Position(std::move(position)),
      Right(std::move(right)),
      Up(std::move(up)),
      Back(std::move(back)) {
}

Vector3 CFrame::GetRotation() const {
    return ToEulerXYZ();
}

CFrame CFrame::RotationOnly() const {
    return CFrame{{}, Right, Up, Back};
}

Vector3 CFrame::RightVector() const {
    return Right;
}

Vector3 CFrame::UpVector() const {
    return Up;
}

Vector3 CFrame::LookVector() const {
    return Back * -1.0;
}

CFrame CFrame::Identity() {
    return {};
}

CFrame CFrame::New(const Vector3& position) {
    return CFrame{position, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};
}

CFrame CFrame::LookAt(const Vector3& eye, const Vector3& target, const Vector3& up) {
    const Vector3 forward = (target - eye).Unit();
    const Vector3 right = forward.Cross(up).Unit();
    const Vector3 realUp = right.Cross(forward).Unit();
    return {eye, right, realUp, -forward};
}

Matrix4 CFrame::ToMatrix4() const {
    return Matrix4({{
        {Right.x, Up.x, Back.x, Position.x},
        {Right.y, Up.y, Back.y, Position.y},
        {Right.z, Up.z, Back.z, Position.z},
        {0.0, 0.0, 0.0, 1.0}
    }});
}

Matrix4 CFrame::ToMatrix4NoTranslation() const {
    return Matrix4({{
        {Right.x, Up.x, Back.x, 0.0},
        {Right.y, Up.y, Back.y, 0.0},
        {Right.z, Up.z, Back.z, 0.0},
        {0.0, 0.0, 0.0, 1.0}
    }});
}

CFrame CFrame::Inverse() const {
    const Vector3 invRight{Right.x, Up.x, Back.x};
    const Vector3 invUp{Right.y, Up.y, Back.y};
    const Vector3 invBack{Right.z, Up.z, Back.z};
    const Vector3 invPos{
        -Position.Dot(Right),
        -Position.Dot(Up),
        -Position.Dot(Back)
    };
    return {invPos, invRight, invUp, invBack};
}

Vector3 CFrame::ToEulerXYZ() const {
    const double r00 = Right.x;
    const double r01 = Up.x;
    const double r02 = Back.x;
    const double r11 = Up.y;
    const double r12 = Back.y;
    const double r21 = Up.z;
    const double r22 = Back.z;

    double rx = 0.0;
    double ry = 0.0;
    double rz = 0.0;

    if (std::abs(r02) < 0.999999) {
        ry = std::asin(r02);
        rx = std::atan2(-r12, r22);
        rz = std::atan2(-r01, r00);
    } else {
        ry = std::asin(r02);
        rx = std::atan2(r21, r11);
        rz = 0.0;
    }

    constexpr double RadToDeg = 180.0 / 3.14159265358979323846;
    return {rx * RadToDeg, ry * RadToDeg, rz * RadToDeg};
}

CFrame CFrame::FromPositionRotation(const Vector3& position, const Vector3& rotationDeg) {
    constexpr double DegToRad = 3.14159265358979323846 / 180.0;

    const double rx = rotationDeg.x * DegToRad;
    const double ry = rotationDeg.y * DegToRad;
    const double rz = rotationDeg.z * DegToRad;

    const double cx = std::cos(rx);
    const double sx = std::sin(rx);
    const double cy = std::cos(ry);
    const double sy = std::sin(ry);
    const double cz = std::cos(rz);
    const double sz = std::sin(rz);

    const double r00 = cy * cz;
    const double r01 = -cy * sz;
    const double r02 = sy;

    const double r10 = sx * sy * cz + cx * sz;
    const double r11 = -sx * sy * sz + cx * cz;
    const double r12 = -sx * cy;

    const double r20 = -cx * sy * cz + sx * sz;
    const double r21 = cx * sy * sz + sx * cz;
    const double r22 = cx * cy;

    const Vector3 right{r00, r10, r20};
    const Vector3 up{r01, r11, r21};
    const Vector3 back{r02, r12, r22};
    return {position, right, up, back};
}

CFrame CFrame::operator*(const CFrame& other) const {
    const Vector3 right{
        Right.x * other.Right.x + Up.x * other.Right.y + Back.x * other.Right.z,
        Right.y * other.Right.x + Up.y * other.Right.y + Back.y * other.Right.z,
        Right.z * other.Right.x + Up.z * other.Right.y + Back.z * other.Right.z
    };

    const Vector3 up{
        Right.x * other.Up.x + Up.x * other.Up.y + Back.x * other.Up.z,
        Right.y * other.Up.x + Up.y * other.Up.y + Back.y * other.Up.z,
        Right.z * other.Up.x + Up.z * other.Up.y + Back.z * other.Up.z
    };

    const Vector3 back{
        Right.x * other.Back.x + Up.x * other.Back.y + Back.x * other.Back.z,
        Right.y * other.Back.x + Up.y * other.Back.y + Back.y * other.Back.z,
        Right.z * other.Back.x + Up.z * other.Back.y + Back.z * other.Back.z
    };

    const Vector3 position{
        Right.x * other.Position.x + Up.x * other.Position.y + Back.x * other.Position.z + Position.x,
        Right.y * other.Position.x + Up.y * other.Position.y + Back.y * other.Position.z + Position.y,
        Right.z * other.Position.x + Up.z * other.Position.y + Back.z * other.Position.z + Position.z
    };

    return {position, right, up, back};
}

CFrame CFrame::FromMatrix(const Matrix4& matrix) {
    const auto& m = matrix.Rows();
    return {
        {m[0][3], m[1][3], m[2][3]},
        {m[0][0], m[1][0], m[2][0]},
        {m[0][1], m[1][1], m[2][1]},
        {m[0][2], m[1][2], m[2][2]}
    };
}

bool CFrame::operator==(const CFrame& other) const {
    return Position == other.Position &&
           Right == other.Right &&
           Up == other.Up &&
           Back == other.Back;
}

} // namespace Lvs::Engine::Math
