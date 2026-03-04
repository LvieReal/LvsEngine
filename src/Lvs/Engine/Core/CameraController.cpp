#include "Lvs/Engine/Core/CameraController.hpp"

#include "Lvs/Engine/Math/CFrame.hpp"
#include "Lvs/Engine/Objects/Camera.hpp"

#include <cmath>

namespace Lvs::Engine::Core {

CameraController::CameraController(const std::shared_ptr<Objects::Camera>& camera)
    : camera_(camera) {
    if (camera_ == nullptr) {
        return;
    }

    const auto cframe = camera_->GetProperty("CFrame").value<Math::CFrame>();
    const auto forward = cframe.LookVector().Unit();
    pitch_ = std::asin(forward.y) * (180.0 / 3.14159265358979323846);
    yaw_ = std::atan2(forward.z, forward.x) * (180.0 / 3.14159265358979323846);
}

void CameraController::Rotate(const double dx, const double dy) {
    yaw_ += dx * mouseSensitivity_;
    pitch_ -= dy * mouseSensitivity_;
    if (pitch_ > 89.99) {
        pitch_ = 89.99;
    } else if (pitch_ < -89.99) {
        pitch_ = -89.99;
    }
    ApplyRotation();
}

void CameraController::Move(const Math::Vector3& direction, const double dt, const bool slow) const {
    if (camera_ == nullptr) {
        return;
    }

    const double moveSpeed = slow ? slowSpeed_ : speed_;
    const auto cframe = camera_->GetProperty("CFrame").value<Math::CFrame>();
    const auto newPos = cframe.Position + direction * moveSpeed * dt;
    camera_->SetProperty("CFrame", QVariant::fromValue(Math::CFrame::LookAt(newPos, newPos + cframe.LookVector())));
}

Math::Vector3 CameraController::GetForward() const {
    if (camera_ == nullptr) {
        return {};
    }
    return camera_->GetProperty("CFrame").value<Math::CFrame>().LookVector();
}

Math::Vector3 CameraController::GetRight() const {
    if (camera_ == nullptr) {
        return {1.0, 0.0, 0.0};
    }
    return camera_->GetProperty("CFrame").value<Math::CFrame>().Right;
}

Math::Vector3 CameraController::GetUp() const {
    if (camera_ == nullptr) {
        return {0.0, 1.0, 0.0};
    }
    return camera_->GetProperty("CFrame").value<Math::CFrame>().Up;
}

void CameraController::SetSpeed(const double speed) {
    speed_ = speed;
}

void CameraController::SetShiftSpeed(const double speed) {
    slowSpeed_ = speed;
}

void CameraController::ApplyRotation() const {
    if (camera_ == nullptr) {
        return;
    }

    constexpr double degToRad = 3.14159265358979323846 / 180.0;
    const double yaw = yaw_ * degToRad;
    const double pitch = pitch_ * degToRad;

    const auto forward = Math::Vector3{
                             std::cos(yaw) * std::cos(pitch),
                             std::sin(pitch),
                             std::sin(yaw) * std::cos(pitch),
                         }
                             .Unit();

    const auto pos = camera_->GetProperty("CFrame").value<Math::CFrame>().Position;
    camera_->SetProperty("CFrame", QVariant::fromValue(Math::CFrame::LookAt(pos, pos + forward)));
}

} // namespace Lvs::Engine::Core
