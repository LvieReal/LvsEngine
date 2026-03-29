#include "Lvs/Engine/Objects/Light.hpp"

#include "Lvs/Engine/DataModel/ClassRegistry.hpp"
#include "Lvs/Engine/Enums/SpecularHighlightType.hpp"
#include "Lvs/Engine/Math/Color3.hpp"

namespace Lvs::Engine::Objects {

Core::ClassDescriptor& Light::Descriptor() {
    static Core::ClassDescriptor descriptor("Light", &Core::Instance::Descriptor());
    static const bool initialized = []() {
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Math::Color3>(
            "Color", Math::Color3{1.0, 1.0, 1.0}, true, "Appearance"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<double>(
            "Intensity", 1.0, true, "Appearance"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<double>(
            "SpecularStrength", 1.0, true, "Appearance"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<double>(
            "Shininess", 32.0, true, "Appearance"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Enums::SpecularHighlightType>(
            "SpecularHighlightType", Enums::SpecularHighlightType::CookTorrance, true, "Appearance"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<bool>(
            "Enabled", true, true, "Appearance"
        ));

        Core::ClassDescriptor::RegisterClassDescriptor(&descriptor);
        DataModel::ClassRegistry::RegisterClass<Light>("Light", "Lights", {});
        return true;
    }();
    static_cast<void>(initialized);
    return descriptor;
}

Light::Light()
    : Light(Descriptor()) {
}

Light::Light(const Core::ClassDescriptor& descriptor)
    : Core::Instance(descriptor) {
    SetInsertable(false);
}

int Light::GetLightType() const {
    return -1;
}

} // namespace Lvs::Engine::Objects
