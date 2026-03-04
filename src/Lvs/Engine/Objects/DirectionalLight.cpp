#include "Lvs/Engine/Objects/DirectionalLight.hpp"

#include "Lvs/Engine/DataModel/ClassRegistry.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"

namespace Lvs::Engine::Objects {

Core::ClassDescriptor& DirectionalLight::Descriptor() {
    static Core::ClassDescriptor descriptor("DirectionalLight", &Light::Descriptor());
    static const bool initialized = []() {
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Math::Vector3>(
            "Direction", {0.5, -1.0, 0.5}, true, "Appearance"
        ));

        Core::ClassDescriptor::RegisterClassDescriptor(&descriptor);
        DataModel::ClassRegistry::RegisterClass<DirectionalLight>("DirectionalLight", "Lights", "Light");
        return true;
    }();
    static_cast<void>(initialized);
    return descriptor;
}

DirectionalLight::DirectionalLight()
    : Light(Descriptor()) {
    SetInsertable(true);
}

int DirectionalLight::GetLightType() const {
    return LIGHT_TYPE_DIRECTIONAL;
}

} // namespace Lvs::Engine::Objects
