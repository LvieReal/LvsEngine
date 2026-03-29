#include "Lvs/Engine/DataModel/Objects/Light.hpp"

#include "Lvs/Engine/DataModel/ClassRegistry.hpp"
#include "Lvs/Engine/Enums/SpecularHighlightType.hpp"
#include "Lvs/Engine/Math/Color3.hpp"

#include <mutex>

namespace Lvs::Engine::DataModel::Objects {

Core::ClassDescriptor& Light::Descriptor() {
    static Core::ClassDescriptor descriptor("Light", &Core::Instance::Descriptor());
    static std::once_flag registered;
    std::call_once(registered, [&]() {
        Core::ClassDescriptor::RegisterClassDescriptor(&descriptor);
        ClassRegistry::RegisterClass<Light>("Light", "Lights", {});
    });
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

} // namespace Lvs::Engine::DataModel::Objects
