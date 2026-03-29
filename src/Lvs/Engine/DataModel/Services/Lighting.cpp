#include "Lvs/Engine/DataModel/Services/Lighting.hpp"

#include "Lvs/Engine/Core/ObjectBase.hpp"
#include "Lvs/Engine/DataModel/ClassRegistry.hpp"
#include "Lvs/Engine/DataModel/Services/ServiceRegistry.hpp"
#include "Lvs/Engine/Enums/LightingComputationMode.hpp"
#include "Lvs/Engine/Enums/LightingTechnology.hpp"
#include "Lvs/Engine/Math/Color3.hpp"
#include "Lvs/Engine/DataModel/Objects/DirectionalLight.hpp"
#include "Lvs/Engine/DataModel/Objects/PostEffects.hpp"
#include "Lvs/Engine/DataModel/Objects/Skybox.hpp"

#include <mutex>

namespace Lvs::Engine::DataModel {

Core::ClassDescriptor& Lighting::Descriptor() {
    static Core::ClassDescriptor descriptor("Lighting", &Service::Descriptor());
    static std::once_flag registered;
    std::call_once(registered, [&]() {
        Core::ClassDescriptor::RegisterClassDescriptor(&descriptor);
        ClassRegistry::RegisterClass<Lighting>("Lighting", "Services", "Service");
        ServiceRegistry::RegisterService<Lighting>();
    });
    return descriptor;
}

Lighting::Lighting()
    : Service(Descriptor()) {
    SetInsertable(false);
}

void Lighting::InitializeDefaultObjects() {
    auto sky = std::make_shared<Objects::Skybox>();
    sky->SetParent(shared_from_this());

    auto postEffects = std::make_shared<Objects::PostEffects>();
    postEffects->SetParent(shared_from_this());

    auto sun = std::make_shared<Objects::DirectionalLight>();
    sun->SetProperty("Name", "Sun");
    sun->SetProperty("SpecularStrength", 10.0);
    sun->SetProperty("Intensity", 2.0);
    sun->SetParent(shared_from_this());
}

} // namespace Lvs::Engine::DataModel
