#include "Lvs/Engine/DataModel/Services/QualitySettings.hpp"

#include "Lvs/Engine/Core/ObjectBase.hpp"
#include "Lvs/Engine/DataModel/ClassRegistry.hpp"
#include "Lvs/Engine/DataModel/Services/ServiceRegistry.hpp"
#include "Lvs/Engine/Enums/MSAA.hpp"
#include "Lvs/Engine/Enums/SurfaceMipmapping.hpp"

#include <mutex>

namespace Lvs::Engine::DataModel {

Core::ClassDescriptor& QualitySettings::Descriptor() {
    static Core::ClassDescriptor descriptor("QualitySettings", &Service::Descriptor());
    static std::once_flag registered;
    std::call_once(registered, [&]() {
        Core::ClassDescriptor::RegisterClassDescriptor(&descriptor);
        ClassRegistry::RegisterClass<QualitySettings>("QualitySettings", "Services", "Service");
        ServiceRegistry::RegisterService<QualitySettings>();
    });
    return descriptor;
}

QualitySettings::QualitySettings()
    : Service(Descriptor()) {
    SetServiceFlags(true, true);
    SetInsertable(false);
}

} // namespace Lvs::Engine::DataModel
