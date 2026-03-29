#include "Lvs/Engine/DataModel/Objects/BasePart.hpp"

#include "Lvs/Engine/DataModel/ClassRegistry.hpp"
#include "Lvs/Engine/Enums/MeshCullMode.hpp"
#include "Lvs/Engine/Math/Color3.hpp"

#include <mutex>

namespace Lvs::Engine::DataModel::Objects {

Core::ClassDescriptor& BasePart::Descriptor() {
    static Core::ClassDescriptor descriptor("BasePart", &Core::Instance::Descriptor());
    static std::once_flag registered;
    std::call_once(registered, [&]() {
        Core::ClassDescriptor::RegisterClassDescriptor(&descriptor);
        ClassRegistry::RegisterClass<BasePart>("BasePart", "3D Objects", "Instance");
    });
    return descriptor;
}

BasePart::BasePart()
    : BasePart(Descriptor()) {
}

BasePart::BasePart(const Core::ClassDescriptor& descriptor)
    : Core::Instance(descriptor) {
    AncestryChanged.Connect([this](const std::shared_ptr<Core::Instance>&) {
        MarkTransformDirty();
    });
}

Math::CFrame BasePart::GetWorldCFrame() {
    if (transformDirty_) {
        RecomputeWorldCFrame();
    }
    return worldCFrame_;
}

Math::Vector3 BasePart::GetWorldPosition() {
    return GetWorldCFrame().Position;
}

Math::Vector3 BasePart::GetWorldRotation() {
    return GetWorldCFrame().ToEulerXYZ();
}

void BasePart::SetProperty(const Core::String& name, const Core::Variant& value) {
    if (name == "CFrame") {
        SetCFrame(value.value<Math::CFrame>());
        return;
    }
    if (name == "Position") {
        SetPosition(value.value<Math::Vector3>());
        return;
    }
    if (name == "Rotation") {
        SetRotation(value.value<Math::Vector3>());
        return;
    }

    Core::Instance::SetProperty(name, value);
}

void BasePart::MarkTransformDirty() {
    transformDirty_ = true;
    for (const auto& child : GetChildren()) {
        if (const auto part = std::dynamic_pointer_cast<BasePart>(child); part != nullptr) {
            part->MarkTransformDirty();
        }
    }
}

void BasePart::RecomputeWorldCFrame() {
    const auto parent = GetParent();
    const auto local = GetProperty("CFrame").value<Math::CFrame>();

    if (const auto partParent = std::dynamic_pointer_cast<BasePart>(parent); partParent != nullptr) {
        worldCFrame_ = partParent->GetWorldCFrame() * local;
    } else {
        worldCFrame_ = local;
    }

    transformDirty_ = false;
}

void BasePart::SetCFrame(const Math::CFrame& cframe) {
    Core::Instance::SetProperty("CFrame", Core::Variant::From(cframe));
    Core::Instance::SetProperty("Position", Core::Variant::From(cframe.Position));
    Core::Instance::SetProperty("Rotation", Core::Variant::From(cframe.ToEulerXYZ()));
    MarkTransformDirty();
}

void BasePart::SetPosition(const Math::Vector3& position) {
    const auto cframe = GetProperty("CFrame").value<Math::CFrame>();
    const auto next = Math::CFrame::FromPositionRotation(position, cframe.ToEulerXYZ());
    SetCFrame(next);
}

void BasePart::SetRotation(const Math::Vector3& rotation) {
    const auto cframe = GetProperty("CFrame").value<Math::CFrame>();
    const auto next = Math::CFrame::FromPositionRotation(cframe.Position, rotation);
    SetCFrame(next);
}

} // namespace Lvs::Engine::DataModel::Objects
