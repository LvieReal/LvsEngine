#include "Lvs/Engine/Objects/DirectionalLight.hpp"

#include "Lvs/Engine/DataModel/ClassRegistry.hpp"
#include "Lvs/Engine/Core/PropertyTags.hpp"
#include "Lvs/Engine/Enums/ShadowType.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"

namespace Lvs::Engine::Objects {

Core::ClassDescriptor& DirectionalLight::Descriptor() {
    static Core::ClassDescriptor descriptor("DirectionalLight", &Light::Descriptor());
    static const bool initialized = []() {
        const Core::StringList cascadedOnlyTags{
            Core::PropertyTags::BuildVisibleIfTag("ShadowType", "Cascaded")
        };
        const Core::StringList volumesOnlyTags{
            Core::PropertyTags::BuildVisibleIfTag("ShadowType", "Volumes")
        };

        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Math::Vector3>(
            "Direction", {0.5, -1.0, 0.5}, true, "Appearance"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<bool>(
            "ShadowEnabled", true, true, "Shadow", "Enable directional shadows for this light."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Enums::ShadowType>(
            "ShadowType",
            Enums::ShadowType::Cascaded,
            true,
            "Shadow",
            "Directional shadow technique (Cascaded shadow maps or Shadow Volumes)."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<double>(
            "ShadowBlur",
            4.0,
            true,
            "Shadow",
            "Shadow blur radius for PCF (in shadow texels).",
            false,
            cascadedOnlyTags
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<int>(
            "ShadowTapCount",
            64,
            true,
            "Shadow",
            "Number of Poisson PCF taps.",
            false,
            cascadedOnlyTags
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<double>(
            "ShadowDepthBias",
            0.0,
            true,
            "Shadow",
            "Directional shadow depth bias (in shadow texels). Higher reduces acne but may cause peter panning. Prefer normal offset.",
            false,
            cascadedOnlyTags
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<double>(
            "ShadowNormalOffset",
            4.0,
            true,
            "Shadow",
            "Directional shadow normal offset (in shadow texels). Virtually pushes receivers along surface normal toward the light.",
            false,
            cascadedOnlyTags
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<int>(
            "ShadowCascadeCount",
            3,
            true,
            "Shadow",
            "Directional shadow cascade count (1-3).",
            false,
            cascadedOnlyTags
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<double>(
            "ShadowMaxDistance", 1000.0, true, "Shadow", "Maximum distance for directional shadows."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<int>(
            "ShadowMapResolution",
            8192,
            true,
            "Shadow",
            "Directional shadow map resolution.",
            false,
            cascadedOnlyTags
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<double>(
            "ShadowCascadeResolutionScale",
            0.9,
            true,
            "Shadow",
            "Per-cascade resolution scale (0-1). Higher keeps farther cascades sharper.",
            false,
            cascadedOnlyTags
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<double>(
            "ShadowCascadeSplitLambda",
            0.75,
            true,
            "Shadow",
            "Cascade split weighting (0 uniform, 1 logarithmic). Lower preserves near/mid detail.",
            false,
            cascadedOnlyTags
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<double>(
            "ShadowVolumeBias",
            0.01,
            true,
            "Shadow",
            "Shadow volume bias (world units). Offsets the near cap/volume along the light direction to reduce acne/self-shadowing.",
            false,
            volumesOnlyTags
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
