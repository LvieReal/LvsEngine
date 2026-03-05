#include "Lvs/Engine/DataModel/Lighting.hpp"

#include "Lvs/Engine/Core/ObjectBase.hpp"
#include "Lvs/Engine/DataModel/ClassRegistry.hpp"
#include "Lvs/Engine/DataModel/ServiceRegistry.hpp"
#include "Lvs/Engine/Enums/LightingComputationMode.hpp"
#include "Lvs/Engine/Enums/LightingTechnology.hpp"
#include "Lvs/Engine/Math/Color3.hpp"
#include "Lvs/Engine/Objects/DirectionalLight.hpp"
#include "Lvs/Engine/Objects/Skybox.hpp"

namespace Lvs::Engine::DataModel {

Core::ClassDescriptor& Lighting::Descriptor() {
    static Core::ClassDescriptor descriptor("Lighting", &Service::Descriptor());
    static const bool initialized = []() {
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<bool>(
            "GammaCorrection", true, true, "Rendering", "Apply regular 2.2 gamma to lighting."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<bool>(
            "Dithering", true, true, "Rendering", "Apply post-process dithering to reduce visible color banding."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<bool>(
            "InaccurateNeon", true, true, "Rendering", "Allow emissive parts with very dark colors to still emit glow."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<double>(
            "NeonBlur", 2.0, true, "Rendering", "Dual Kawase blur amount for neon glow."
        ));
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
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<bool>(
            "ShadowsEnabled",
            true,
            true,
            "Rendering",
            "Enables directional shadows."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<double>(
            "ShadowBlur",
            4.0,
            true,
            "Rendering",
            "Shadow blur radius for shadow PCF."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<int>(
            "DefaultShadowTapCount",
            16,
            true,
            "Rendering",
            "Number of Poisson PCF taps."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<int>(
            "DefaultShadowCascadeCount",
            3,
            true,
            "Rendering",
            "Directional shadow cascade count (1-3)."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<double>(
            "DefaultShadowMaxDistance",
            1000.0,
            true,
            "Rendering",
            "Maximum distance for directional shadows."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<int>(
            "DefaultShadowMapResolution",
            8192,
            true,
            "Rendering",
            "Directional shadow map resolution."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<double>(
            "DefaultShadowCascadeResolutionScale",
            0.9,
            true,
            "Rendering",
            "Per-cascade resolution scale (0-1). Higher keeps farther cascades sharper."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<double>(
            "DefaultShadowCascadeSplitLambda",
            0.75,
            true,
            "Rendering",
            "Cascade split weighting (0 uniform, 1 logarithmic). Lower preserves near/mid detail."
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

    auto sun = std::make_shared<Objects::DirectionalLight>();
    sun->SetProperty("Name", "Sun");
    sun->SetProperty("Intensity", 3.0);
    sun->SetParent(shared_from_this());
}

} // namespace Lvs::Engine::DataModel
