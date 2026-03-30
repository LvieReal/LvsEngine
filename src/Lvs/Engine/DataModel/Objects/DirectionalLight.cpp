#include "Lvs/Engine/DataModel/Objects/DirectionalLight.hpp"

#include "Lvs/Engine/DataModel/ClassRegistry.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"

#include <mutex>

namespace Lvs::Engine::DataModel::Objects {

Core::ClassDescriptor& DirectionalLight::Descriptor() {
    static Core::ClassDescriptor descriptor("DirectionalLight", &Light::Descriptor());
    static std::once_flag registered;
    std::call_once(registered, [&]() {
        Core::ClassDescriptor::RegisterClassDescriptor(&descriptor);
        ClassRegistry::RegisterClass<DirectionalLight>("DirectionalLight", "Lights", "Light");
    });
    return descriptor;
}

DirectionalLight::DirectionalLight()
    : Light(Descriptor()) {
    SetInsertable(true);
}

int DirectionalLight::GetLightType() const {
    return LIGHT_TYPE_DIRECTIONAL;
}

} // namespace Lvs::Engine::DataModel::Objects
