#include "Lvs/Engine/DataModel/Services/QualitySettings.hpp"

#include "Lvs/Engine/Core/ObjectBase.hpp"
#include "Lvs/Engine/DataModel/ClassRegistry.hpp"
#include "Lvs/Engine/DataModel/Services/ServiceRegistry.hpp"
#include "Lvs/Engine/Enums/MSAA.hpp"
#include "Lvs/Engine/Enums/SurfaceMipmapping.hpp"

namespace Lvs::Engine::DataModel {

Core::ClassDescriptor& QualitySettings::Descriptor() {
    static Core::ClassDescriptor descriptor("QualitySettings", &Service::Descriptor());
    static const bool initialized = []() {
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Enums::MSAA>(
            "MSAA",
            Enums::MSAA::Off,
            true,
            "Quality",
            "Multisample anti-aliasing sample count."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Enums::SurfaceMipmapping>(
            "SurfaceMipmapping",
            Enums::SurfaceMipmapping::On,
            true,
            "Quality",
            "Mipmapped filtering for surface atlas textures."
        ));

        Core::ClassDescriptor::RegisterClassDescriptor(&descriptor);
        ClassRegistry::RegisterClass<QualitySettings>("QualitySettings", "Services", "Service");
        ServiceRegistry::RegisterService<QualitySettings>();
        return true;
    }();
    static_cast<void>(initialized);
    return descriptor;
}

QualitySettings::QualitySettings()
    : Service(Descriptor()) {
    SetServiceFlags(true, true);
    SetInsertable(false);
}

} // namespace Lvs::Engine::DataModel
