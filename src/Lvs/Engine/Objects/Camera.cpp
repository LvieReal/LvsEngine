#include "Lvs/Engine/Objects/Camera.hpp"

#include "Lvs/Engine/DataModel/ClassRegistry.hpp"
#include "Lvs/Engine/Math/CFrame.hpp"
#include "Lvs/Engine/Math/Projection.hpp"

namespace Lvs::Engine::Objects {

Core::ClassDescriptor& Camera::Descriptor() {
    static Core::ClassDescriptor descriptor("Camera", &Core::Instance::Descriptor());
    static const bool initialized = []() {
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Math::CFrame>(
            "CFrame", Math::CFrame::Identity(), true, "Transform", {}, false, {"IsSeparateBox"}
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<double>(
            "FieldOfView", 70.0, true, "Camera"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<double>(
            "NearPlane", 0.1, true, "Camera"
        ));

        Core::ClassDescriptor::RegisterClassDescriptor(&descriptor);
        DataModel::ClassRegistry::RegisterClass<Camera>("Camera", "General", "Instance");
        return true;
    }();
    static_cast<void>(initialized);
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
    projectionMatrix_ = Math::Projection::ReversedInfinitePerspective(
        GetProperty("FieldOfView").toDouble(),
        aspect,
        GetProperty("NearPlane").toDouble()
    );
}

} // namespace Lvs::Engine::Objects
