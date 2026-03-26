#include "Lvs/Engine/DataModel/Services/Lighting.hpp"

#include "Lvs/Engine/Core/ObjectBase.hpp"
#include "Lvs/Engine/DataModel/ClassRegistry.hpp"
#include "Lvs/Engine/DataModel/Services/ServiceRegistry.hpp"
#include "Lvs/Engine/Enums/LightingComputationMode.hpp"
#include "Lvs/Engine/Enums/LightingTechnology.hpp"
#include "Lvs/Engine/Enums/RenderCullMode.hpp"
#include "Lvs/Engine/Enums/RenderDepthCompare.hpp"
#include "Lvs/Engine/Enums/ShadowVolumeCapMode.hpp"
#include "Lvs/Engine/Enums/ShadowVolumeStencilMode.hpp"
#include "Lvs/Engine/Math/Color3.hpp"
#include "Lvs/Engine/Objects/DirectionalLight.hpp"
#include "Lvs/Engine/Objects/PostEffects.hpp"
#include "Lvs/Engine/Objects/Skybox.hpp"

namespace Lvs::Engine::DataModel {

Core::ClassDescriptor& Lighting::Descriptor() {
    static Core::ClassDescriptor descriptor("Lighting", &Service::Descriptor());
    static const bool initialized = []() {
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Math::Color3>(
            "Ambient", Math::Color3{1.0, 1.0, 1.0}, true, "Rendering", "Global ambient color."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<double>(
            "AmbientStrength", 0.1, true, "Rendering", "Global ambient strength."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Enums::LightingTechnology>(
            "Technology", Enums::LightingTechnology::Default, true, "Rendering", "Controls the lighting style."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Enums::LightingComputationMode>(
            "Shading",
            Enums::LightingComputationMode::PerPixel,
            true,
            "Rendering",
            "Selects per-fragment or legacy per-vertex lighting."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<double>(
            "FresnelAmount",
            1.0,
            true,
            "Rendering",
            "Scales Fresnel term contribution (0 disables angle-dependent Fresnel, 1 is default)."
        ));

        // Temporary shadow-volume tuning knobs (for debugging Z-fail vs Z-pass behavior).
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Enums::RenderDepthCompare>(
            "ShadowVolumeDepthCompare",
            Enums::RenderDepthCompare::GreaterOrEqual,
            true,
            "ShadowVolumes (Temp)",
            "Depth compare used for the shadow-volume stencil pass (Z-fail uses DepthFail stencil ops)."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Enums::RenderCullMode>(
            "ShadowVolumeCullMode",
            Enums::RenderCullMode::None,
            true,
            "ShadowVolumes (Temp)",
            "Cull mode used for the shadow-volume stencil pass (two-sided stencil typically needs None)."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Enums::RenderDepthCompare>(
            "ShadowVolumeMaskDepthCompare",
            Enums::RenderDepthCompare::NotEqual,
            true,
            "ShadowVolumes (Temp)",
            "Depth compare used for the shadow-volume mask pass (default NotEqual avoids shadowing the sky/background)."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Enums::RenderCullMode>(
            "ShadowVolumeMaskCullMode",
            Enums::RenderCullMode::None,
            true,
            "ShadowVolumes (Temp)",
            "Cull mode used for the shadow-volume mask pass."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Enums::ShadowVolumeCapMode>(
            "ShadowVolumeCapMode",
            Enums::ShadowVolumeCapMode::BackNear_FrontFar,
            true,
            "ShadowVolumes (Temp)",
            "Cap construction mode for shadow volumes (near/far caps vs light-facing classification)."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Enums::ShadowVolumeStencilMode>(
            "ShadowVolumeStencilMode",
            Enums::ShadowVolumeStencilMode::ZFail,
            true,
            "ShadowVolumes (Temp)",
            "Stencil counting mode used by shadow volumes."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<bool>(
            "ShadowVolumeSwapStencilOps",
            false,
            true,
            "ShadowVolumes (Temp)",
            "Swaps front/back stencil increment/decrement ops (useful when face classification differs)."
        ));

        Core::ClassDescriptor::RegisterClassDescriptor(&descriptor);
        ClassRegistry::RegisterClass<Lighting>("Lighting", "Services", "Service");
        ServiceRegistry::RegisterService<Lighting>();
        return true;
    }();
    static_cast<void>(initialized);
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
