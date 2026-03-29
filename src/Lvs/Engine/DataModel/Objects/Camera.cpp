#include "Lvs/Engine/DataModel/Objects/Camera.hpp"

#include "Lvs/Engine/DataModel/ClassRegistry.hpp"
#include "Lvs/Engine/Math/CFrame.hpp"
#include "Lvs/Engine/Math/Projection.hpp"

#include <mutex>

namespace Lvs::Engine::DataModel::Objects {

Core::ClassDescriptor& Camera::Descriptor() {
    static Core::ClassDescriptor descriptor("Camera", &Core::Instance::Descriptor());
    static std::once_flag registered;
    std::call_once(registered, [&]() {
        Core::ClassDescriptor::RegisterClassDescriptor(&descriptor);
        ClassRegistry::RegisterClass<Camera>("Camera", "General", "Instance");
    });
    return descriptor;
}

Camera::Camera()
    : Core::Instance(Descriptor()) {
    PropertyChanged.Connect([this](const Core::String&, const Core::Variant&) {
        UpdateProjectionMatrix(lastAspect_.value_or(1.0));
    });
    UpdateProjectionMatrix(1.0);
}

void Camera::Resize(const double aspect) {
    UpdateProjectionMatrix(aspect);
}

Math::Matrix4 Camera::GetViewMatrix() const {
    return GetProperty("CFrame").value<Math::CFrame>().Inverse().ToMatrix4();
}

Math::Matrix4 Camera::GetInverseViewMatrix() const {
    return GetProperty("CFrame").value<Math::CFrame>().ToMatrix4();
}

Math::Matrix4 Camera::GetViewMatrixNoTranslation() const {
    return GetProperty("CFrame").value<Math::CFrame>().Inverse().ToMatrix4NoTranslation();
}

Math::Matrix4 Camera::GetInverseProjectionMatrix() const {
    return projectionMatrix_.Inverse();
}

Math::Matrix4 Camera::GetProjectionMatrix() const {
    return projectionMatrix_;
}

void Camera::UpdateProjectionMatrix(const double aspect) {
    lastAspect_ = aspect;
    double fov = 70.0;
    double nearPlane = 0.1;
    try {
        const auto& props = GetProperties();
        if (const auto it = props.find("FieldOfView"); it != props.end()) {
            fov = it->second.Get().toDouble();
        }
        if (const auto it = props.find("NearPlane"); it != props.end()) {
            nearPlane = it->second.Get().toDouble();
        }
    } catch (...) {
    }
    projectionMatrix_ = Math::Projection::ReversedInfinitePerspective(
        fov,
        aspect,
        nearPlane
    );
}

} // namespace Lvs::Engine::DataModel::Objects
