#include "Lvs/Engine/Objects/BasePart.hpp"

#include "Lvs/Engine/DataModel/ClassRegistry.hpp"
#include "Lvs/Engine/Core/PropertyTags.hpp"
#include "Lvs/Engine/Enums/MeshCullMode.hpp"
#include "Lvs/Engine/Math/Color3.hpp"

namespace Lvs::Engine::Objects {

Core::ClassDescriptor& BasePart::Descriptor() {
    static Core::ClassDescriptor descriptor("BasePart", &Core::Instance::Descriptor());
    static const bool initialized = []() {
        const Core::String alwaysOnTopVisibleTag = Core::PropertyTags::BuildVisibleIfTag("AlwaysOnTop", "true");

        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Math::Color3>(
            "Color", Math::Color3{0.7, 0.7, 0.7}, true, "Appearance"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<double>(
            "Transparency", 0.0, true, "Appearance"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<double>(
            "Roughness", 0.8, true, "Appearance"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<double>(
            "Metalness", 0.0, true, "Appearance"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<double>(
            "Emissive", 0.0, true, "Appearance"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<bool>(
            "Renders", true, true, "Appearance"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<bool>(
            "Locked", false, true, "Behavior", "Locked parts cannot be selected."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<bool>(
            "AlwaysOnTop", false, true, "Appearance"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<int>(
            "ZIndex", 0, true, "Appearance", {}, false, Core::StringList{alwaysOnTopVisibleTag}
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Enums::MeshCullMode>(
            "CullMode", Enums::MeshCullMode::Back, true, "Appearance"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<bool>(
            "Anchored", false, true, "Physics"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<bool>(
            "CanCollide", true, true, "Physics"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Math::Vector3>(
            "Position", {}, true, "Transform"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Math::Vector3>(
            "Rotation", {}, true, "Transform"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Math::CFrame>(
            "CFrame", Math::CFrame::Identity(), true, "Transform", {}, true, {"IsSeparateBox"}
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Math::Vector3>(
            "Size", {4.0, 1.0, 2.0}, true, "Transform"
        ));

        Core::ClassDescriptor::RegisterClassDescriptor(&descriptor);
        DataModel::ClassRegistry::RegisterClass<BasePart>("BasePart", "3D Objects", "Instance");
        return true;
    }();
    static_cast<void>(initialized);
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

} // namespace Lvs::Engine::Objects
