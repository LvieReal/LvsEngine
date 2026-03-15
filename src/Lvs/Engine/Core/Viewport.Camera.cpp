#include "Lvs/Engine/Core/Viewport.hpp"

#include "Lvs/Engine/Core/CameraController.hpp"
#include "Lvs/Engine/DataModel/Services/Workspace.hpp"
#include "Lvs/Engine/Math/CFrame.hpp"
#include "Lvs/Engine/Objects/BasePart.hpp"
#include "Lvs/Engine/Objects/Camera.hpp"

#include <QVariant>
#include <Qt>

#include <algorithm>
#include <cmath>

namespace Lvs::Engine::Core {

void Viewport::UpdateCamera(const double deltaSeconds) {
    if (cameraController_ == nullptr) {
        return;
    }

    Math::Vector3 direction{};
    if (IsMoveForwardPressed()) {
        direction = direction + cameraController_->GetForward();
    }
    if (IsMoveBackwardPressed()) {
        direction = direction - cameraController_->GetForward();
    }
    if (IsMoveRightPressed()) {
        direction = direction + cameraController_->GetRight();
    }
    if (IsMoveLeftPressed()) {
        direction = direction - cameraController_->GetRight();
    }
    if (IsMoveUpPressed()) {
        direction = direction + cameraController_->GetUp();
    }
    if (IsMoveDownPressed()) {
        direction = direction - cameraController_->GetUp();
    }

    if (direction.Magnitude() > 0.0) {
        cameraController_->Move(direction.Unit(), deltaSeconds, IsSlowMovePressed());
    }
}

bool Viewport::IsMoveForwardPressed() const {
    return HasKey(Qt::Key_W) || HasScanCode(0x11);
}

bool Viewport::IsMoveBackwardPressed() const {
    return HasKey(Qt::Key_S) || HasScanCode(0x1F);
}

bool Viewport::IsMoveLeftPressed() const {
    return HasKey(Qt::Key_A) || HasScanCode(0x1E);
}

bool Viewport::IsMoveRightPressed() const {
    return HasKey(Qt::Key_D) || HasScanCode(0x20);
}

bool Viewport::IsMoveDownPressed() const {
    return HasKey(Qt::Key_Q) || HasScanCode(0x10);
}

bool Viewport::IsMoveUpPressed() const {
    return HasKey(Qt::Key_E) || HasScanCode(0x12);
}

bool Viewport::IsSlowMovePressed() const {
    return HasKey(Qt::Key_Shift) || HasScanCode(0x2A) || HasScanCode(0x36);
}

bool Viewport::HasKey(const int key) const {
    return pressedKeys_.find(key) != pressedKeys_.end();
}

bool Viewport::HasScanCode(const std::uint32_t sc) const {
    return pressedScanCodes_.find(sc) != pressedScanCodes_.end();
}

std::shared_ptr<Objects::Camera> Viewport::GetCurrentCamera() const {
    if (workspace_ == nullptr) {
        return nullptr;
    }
    return workspace_->GetProperty("CurrentCamera").value<std::shared_ptr<Objects::Camera>>();
}

std::optional<Utils::Ray> Viewport::BuildRay(const double x, const double y) const {
    const auto camera = GetCurrentCamera();
    if (camera == nullptr) {
        return std::nullopt;
    }

    return Utils::ScreenPointToRay(x, y, width(), height(), camera);
}

void Viewport::FocusOnPart(const std::shared_ptr<Objects::BasePart>& part) {
    if (part == nullptr || cameraController_ == nullptr || workspace_ == nullptr) {
        return;
    }

    const auto camera = GetCurrentCamera();
    if (camera == nullptr) {
        return;
    }

    const auto aabb = Utils::BuildPartWorldAABB(part);
    FocusOnBounds(aabb);
}

void Viewport::FocusOnBounds(const Math::AABB& bounds) {
    if (cameraController_ == nullptr || workspace_ == nullptr) {
        return;
    }

    const auto camera = GetCurrentCamera();
    if (camera == nullptr) {
        return;
    }

    const Math::Vector3 center = (bounds.Min + bounds.Max) * 0.5;
    const Math::Vector3 extents = bounds.Max - bounds.Min;
    const double maxExtent = std::max({extents.x, extents.y, extents.z});

    const double distance = std::max(0.1, maxExtent * 1.5);

    const auto currentCFrame = camera->GetProperty("CFrame").value<Math::CFrame>();
    const Math::Vector3 direction = (center - currentCFrame.Position).Unit();
    const Math::Vector3 newPosition = center - direction * distance;
    const Math::Vector3 lookDirection = (center - newPosition).Unit();

    camera->SetProperty("CFrame", QVariant::fromValue(Math::CFrame::LookAt(newPosition, center)));

    constexpr double radToDeg = 180.0 / 3.14159265358979323846;
    const double newPitch = std::asin(lookDirection.y) * radToDeg;
    const double newYaw = std::atan2(lookDirection.z, lookDirection.x) * radToDeg;
    cameraController_->SetRotation(newYaw, newPitch);
}

} // namespace Lvs::Engine::Core
