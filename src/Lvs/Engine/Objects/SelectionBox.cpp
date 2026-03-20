#include "Lvs/Engine/Objects/SelectionBox.hpp"

#include "Lvs/Engine/DataModel/ClassRegistry.hpp"
#include "Lvs/Engine/Math/Color3.hpp"

namespace Lvs::Engine::Objects {

Core::ClassDescriptor& SelectionBox::Descriptor() {
    static Core::ClassDescriptor descriptor("SelectionBox", &Core::Instance::Descriptor());
    static const bool initialized = []() {
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Core::Variant::InstanceRef>(
            "Adornee",
            {},
            true,
            "Data",
            {},
            false,
            {},
            {},
            true
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Math::Color3>(
            "Color",
            Math::Color3{0.0, 0.5, 1.0},
            true,
            "Appearance"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<double>(
            "Transparency",
            0.0,
            true,
            "Appearance"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<double>(
            "LineThickness",
            0.06,
            true,
            "Appearance"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<double>(
            "Metalness",
            0.0,
            true,
            "Appearance"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<double>(
            "Roughness",
            1.0,
            true,
            "Appearance"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<double>(
            "Emissive",
            1.0,
            true,
            "Appearance"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<bool>(
            "IgnoreLighting",
            true,
            true,
            "Appearance"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<bool>(
            "AlwaysOnTop",
            true,
            true,
            "Appearance"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<bool>(
            "Visible",
            true,
            true,
            "Appearance"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<bool>(
            "ScaleWithDistance",
            false,
            true,
            "Appearance"
        ));

        Core::ClassDescriptor::RegisterClassDescriptor(&descriptor);
        DataModel::ClassRegistry::RegisterClass<SelectionBox>("SelectionBox", "Visuals", "Instance");
        return true;
    }();
    static_cast<void>(initialized);
    return descriptor;
}

SelectionBox::SelectionBox()
    : Core::Instance(Descriptor()) {
    SetInsertable(true);
}

} // namespace Lvs::Engine::Objects
