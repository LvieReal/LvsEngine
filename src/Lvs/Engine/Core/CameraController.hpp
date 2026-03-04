#pragma once

#include "Lvs/Engine/Math/Vector3.hpp"

#include <memory>

namespace Lvs::Engine::Objects {
class Camera;
}

namespace Lvs::Engine::Core {

class CameraController final {
public:
    explicit CameraController(const std::shared_ptr<Objects::Camera>& camera);

    void Rotate(double dx, double dy);
    void Move(const Math::Vector3& direction, double dt, bool slow = false) const;

    [[nodiscard]] Math::Vector3 GetForward() const;
    [[nodiscard]] Math::Vector3 GetRight() const;
    [[nodiscard]] Math::Vector3 GetUp() const;

    void SetSpeed(double speed);
    void SetShiftSpeed(double speed);

private:
    void ApplyRotation() const;

    std::shared_ptr<Objects::Camera> camera_;
    double yaw_{-90.0};
    double pitch_{0.0};
    double mouseSensitivity_{0.15};
    double speed_{1.0};
    double slowSpeed_{0.5};
};

} // namespace Lvs::Engine::Core
