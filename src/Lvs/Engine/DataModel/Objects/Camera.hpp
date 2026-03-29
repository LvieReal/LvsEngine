#pragma once

#include "Lvs/Engine/Core/Instance.hpp"
#include "Lvs/Engine/Math/Matrix4.hpp"

#include <optional>

namespace Lvs::Engine::DataModel::Objects {

class Camera : public Core::Instance {
public:
    Camera();
    ~Camera() override = default;

    static Core::ClassDescriptor& Descriptor();

    void Resize(double aspect);

    [[nodiscard]] Math::Matrix4 GetViewMatrix() const;
    [[nodiscard]] Math::Matrix4 GetInverseViewMatrix() const;
    [[nodiscard]] Math::Matrix4 GetViewMatrixNoTranslation() const;
    [[nodiscard]] Math::Matrix4 GetInverseProjectionMatrix() const;
    [[nodiscard]] Math::Matrix4 GetProjectionMatrix() const;

private:
    void UpdateProjectionMatrix(double aspect);

    std::optional<double> lastAspect_;
    Math::Matrix4 projectionMatrix_{Math::Matrix4::Identity()};
};

} // namespace Lvs::Engine::DataModel::Objects
