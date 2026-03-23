#include "Lvs/Engine/Objects/PostEffects.hpp"

#include "Lvs/Engine/Core/ObjectBase.hpp"
#include "Lvs/Engine/DataModel/ClassRegistry.hpp"
#include "Lvs/Engine/Math/Color3.hpp"

namespace Lvs::Engine::Objects {

Core::ClassDescriptor& PostEffects::Descriptor() {
    static Core::ClassDescriptor descriptor("PostEffects", &Core::Instance::Descriptor());
    static const bool initialized = []() {
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<bool>(
            "GammaCorrection", true, true, "Post-Process", "Apply regular 2.2 gamma to lighting."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<bool>(
            "Dithering", true, true, "Post-Process", "Apply post-process dithering to reduce visible color banding."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<bool>(
            "NeonEnabled", true, true, "Post-Process", "Enable neon glow contribution in post-process composite."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<bool>(
            "InaccurateNeon",
            true,
            true,
            "Post-Process",
            "Allow emissive parts with very dark colors to still emit glow.",
            false,
            {"VisibleIf:NeonEnabled=true"}
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<double>(
            "NeonBlur",
            2.0,
            true,
            "Post-Process",
            "Dual Kawase blur amount for neon glow.",
            false,
            {"VisibleIf:NeonEnabled=true"}
        ));

        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<bool>(
            "HBAOEnabled", true, true, "HBAO", "Enable Horizon-Based Ambient Occlusion post effect."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<double>(
            "HBAOStrength", 1.9, true, "HBAO", "Ambient occlusion strength multiplier."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<double>(
            "HBAORadius", 0.3, true, "HBAO", "AO sampling radius in view-space units."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<double>(
            "HBAOBiasDegrees", 30.0, true, "HBAO", "Bias angle in degrees to reduce self-occlusion."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<int>(
            "HBAOMaxRadiusPixels", 96, true, "HBAO", "Clamp AO kernel footprint in pixels."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<int>(
            "HBAODirections", 6, true, "HBAO", "Number of sampling directions (higher is smoother, slower)."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<int>(
            "HBAOSamples", 4, true, "HBAO", "Number of samples per direction (higher is smoother, slower)."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<double>(
            "HBAOPower", 1.0, true, "HBAO", "Power curve applied to AO factor (higher increases contrast)."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<double>(
            "HBAOBlur", 1.0, true, "HBAO", "Dual Kawase blur amount applied to AO output."
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Math::Color3>(
            "AOTint", Math::Color3{0.0, 0.0, 0.0}, true, "HBAO", "Tint applied to ambient occlusion contribution."
        ));

        Core::ClassDescriptor::RegisterClassDescriptor(&descriptor);
        DataModel::ClassRegistry::RegisterClass<PostEffects>("PostEffects", "Rendering", "Instance");
        return true;
    }();
    static_cast<void>(initialized);
    return descriptor;
}

PostEffects::PostEffects()
    : Core::Instance(Descriptor()) {
    SetInsertable(true);
}

} // namespace Lvs::Engine::Objects
