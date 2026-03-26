#include "Lvs/Engine/Enums/EnumMetadata.hpp"

#include "Lvs/Engine/Enums/LightingComputationMode.hpp"
#include "Lvs/Engine/Enums/LightingTechnology.hpp"
#include "Lvs/Engine/Enums/MSAA.hpp"
#include "Lvs/Engine/Enums/MeshCullMode.hpp"
#include "Lvs/Engine/Enums/PartShape.hpp"
#include "Lvs/Engine/Enums/PartSurface.hpp"
#include "Lvs/Engine/Enums/PartSurfaceType.hpp"
#include "Lvs/Engine/Enums/SkyboxTextureLayout.hpp"
#include "Lvs/Engine/Enums/ShadowType.hpp"
#include "Lvs/Engine/Enums/RenderCullMode.hpp"
#include "Lvs/Engine/Enums/RenderDepthCompare.hpp"
#include "Lvs/Engine/Enums/ShadowVolumeCapMode.hpp"
#include "Lvs/Engine/Enums/ShadowVolumeStencilMode.hpp"
#include "Lvs/Engine/Enums/SpecularHighlightType.hpp"
#include "Lvs/Engine/Enums/SurfaceMipmapping.hpp"
#include "Lvs/Engine/Enums/TextureFiltering.hpp"
#include "Lvs/Engine/Enums/Theme.hpp"
#include "Lvs/Engine/Rendering/IRenderContext.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <optional>

namespace Lvs::Engine::Enums::Metadata {

namespace {

struct EnumInfo {
    Core::Vector<EnumOption> Options{};
};

[[nodiscard]] std::string LowerAscii(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

[[nodiscard]] bool IsAllowedValue(const EnumInfo& info, const int value) {
    for (const auto& opt : info.Options) {
        if (opt.Value == value) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::optional<int> TryParseOptionName(const EnumInfo& info, const Core::String& nameOrNumber) {
    const std::string needle = LowerAscii(nameOrNumber);
    if (needle.empty()) {
        return std::nullopt;
    }

    for (const auto& opt : info.Options) {
        if (LowerAscii(opt.Name) == needle) {
            return opt.Value;
        }
    }

    int parsed = 0;
    const auto* begin = nameOrNumber.data();
    const auto* end = begin + nameOrNumber.size();
    const auto [ptr, ec] = std::from_chars(begin, end, parsed);
    if (ec == std::errc{} && ptr == end) {
        return parsed;
    }

    return std::nullopt;
}

[[nodiscard]] const Core::HashMap<Core::String, EnumInfo>& Registry() {
    using namespace Lvs::Engine::Enums;
    using Lvs::Engine::Rendering::RenderApi;

    static const Core::HashMap<Core::String, EnumInfo> registry = [] {
        Core::HashMap<Core::String, EnumInfo> out;
        auto add = [&out](const Core::String& name, std::initializer_list<EnumOption> options) {
            EnumInfo info;
            info.Options.reserve(options.size());
            for (const auto& opt : options) {
                info.Options.push_back(opt);
            }
            out.insert_or_assign(name, std::move(info));
        };

        add(Core::String(Core::EnumTraits<LightingTechnology>::Name), {
            {"Default", static_cast<int>(LightingTechnology::Default)},
        });
        add(Core::String(Core::EnumTraits<LightingComputationMode>::Name), {
            {"Per-Pixel", static_cast<int>(LightingComputationMode::PerPixel)},
            {"Per-Vertex", static_cast<int>(LightingComputationMode::PerVertex)},
        });
        add(Core::String(Core::EnumTraits<TextureFiltering>::Name), {
            {"Linear", static_cast<int>(TextureFiltering::Linear)},
            {"Nearest", static_cast<int>(TextureFiltering::Nearest)},
        });
        add(Core::String(Core::EnumTraits<SpecularHighlightType>::Name), {
            {"Phong", static_cast<int>(SpecularHighlightType::Phong)},
            {"Blinn-Phong", static_cast<int>(SpecularHighlightType::BlinnPhong)},
            {"Cook-Torrance", static_cast<int>(SpecularHighlightType::CookTorrance)},
        });
        add(Core::String(Core::EnumTraits<MSAA>::Name), {
            {"Off", static_cast<int>(MSAA::Off)},
            {"2x", static_cast<int>(MSAA::X2)},
            {"4x", static_cast<int>(MSAA::X4)},
            {"8x", static_cast<int>(MSAA::X8)},
        });
        add(Core::String(Core::EnumTraits<SurfaceMipmapping>::Name), {
            {"Off", static_cast<int>(SurfaceMipmapping::Off)},
            {"On", static_cast<int>(SurfaceMipmapping::On)},
        });
        add(Core::String(Core::EnumTraits<SkyboxTextureLayout>::Name), {
            {"Individual", static_cast<int>(SkyboxTextureLayout::Individual)},
            {"Cross", static_cast<int>(SkyboxTextureLayout::Cross)},
        });
        add(Core::String(Core::EnumTraits<MeshCullMode>::Name), {
            {"NoCull", static_cast<int>(MeshCullMode::NoCull)},
            {"Back", static_cast<int>(MeshCullMode::Back)},
            {"Front", static_cast<int>(MeshCullMode::Front)},
        });
        add(Core::String(Core::EnumTraits<PartShape>::Name), {
            {"Cube", static_cast<int>(PartShape::Cube)},
            {"Sphere", static_cast<int>(PartShape::Sphere)},
            {"Cylinder", static_cast<int>(PartShape::Cylinder)},
            {"Cone", static_cast<int>(PartShape::Cone)},
        });
        add(Core::String(Core::EnumTraits<PartSurface>::Name), {
            {"RightSurface", static_cast<int>(PartSurface::RightSurface)},
            {"LeftSurface", static_cast<int>(PartSurface::LeftSurface)},
            {"TopSurface", static_cast<int>(PartSurface::TopSurface)},
            {"BottomSurface", static_cast<int>(PartSurface::BottomSurface)},
            {"FrontSurface", static_cast<int>(PartSurface::FrontSurface)},
            {"BackSurface", static_cast<int>(PartSurface::BackSurface)},
        });
        add(Core::String(Core::EnumTraits<PartSurfaceType>::Name), {
            {"Smooth", static_cast<int>(PartSurfaceType::Smooth)},
            {"Studs", static_cast<int>(PartSurfaceType::Studs)},
            {"Inlets", static_cast<int>(PartSurfaceType::Inlets)},
        });
        add(Core::String(Core::EnumTraits<Theme>::Name), {
            {"Light", static_cast<int>(Theme::Light)},
            {"Dark", static_cast<int>(Theme::Dark)},
            {"Auto", static_cast<int>(Theme::Auto)},
        });
        add(Core::String(Core::EnumTraits<ShadowType>::Name), {
            {"Volumes", static_cast<int>(ShadowType::Volumes)},
            {"Cascaded", static_cast<int>(ShadowType::Cascaded)},
        });
        add(Core::String(Core::EnumTraits<RenderCullMode>::Name), {
            {"None", static_cast<int>(RenderCullMode::None)},
            {"Front", static_cast<int>(RenderCullMode::Front)},
            {"Back", static_cast<int>(RenderCullMode::Back)},
        });
        add(Core::String(Core::EnumTraits<RenderDepthCompare>::Name), {
            {"Always", static_cast<int>(RenderDepthCompare::Always)},
            {"Equal", static_cast<int>(RenderDepthCompare::Equal)},
            {"NotEqual", static_cast<int>(RenderDepthCompare::NotEqual)},
            {"Less", static_cast<int>(RenderDepthCompare::Less)},
            {"LessOrEqual", static_cast<int>(RenderDepthCompare::LessOrEqual)},
            {"Greater", static_cast<int>(RenderDepthCompare::Greater)},
            {"GreaterOrEqual", static_cast<int>(RenderDepthCompare::GreaterOrEqual)},
        });
        add(Core::String(Core::EnumTraits<ShadowVolumeCapMode>::Name), {
            {"FrontNear_BackFar", static_cast<int>(ShadowVolumeCapMode::FrontNear_BackFar)},
            {"BackNear_FrontFar", static_cast<int>(ShadowVolumeCapMode::BackNear_FrontFar)},
            {"None", static_cast<int>(ShadowVolumeCapMode::None)},
        });
        add(Core::String(Core::EnumTraits<ShadowVolumeStencilMode>::Name), {
            {"ZFail", static_cast<int>(ShadowVolumeStencilMode::ZFail)},
            {"ZPass", static_cast<int>(ShadowVolumeStencilMode::ZPass)},
        });

        // Non-Engine-Enums enum types.
        add(Core::String(Core::EnumTraits<RenderApi>::Name), {
            {"Auto", static_cast<int>(RenderApi::Auto)},
            {"Vulkan", static_cast<int>(RenderApi::Vulkan)},
            {"OpenGL", static_cast<int>(RenderApi::OpenGL)},
        });

        return out;
    }();

    return registry;
}

[[nodiscard]] const EnumInfo* Find(const Core::String& enumType) {
    const auto& registry = Registry();
    const auto it = registry.find(enumType);
    return it == registry.end() ? nullptr : &it->second;
}

} // namespace

Core::Vector<EnumOption> OptionsForEnum(const Core::String& enumType) {
    if (const EnumInfo* info = Find(enumType); info != nullptr) {
        return info->Options;
    }
    return {};
}

Core::Variant VariantFromInt(const Core::String& enumType, const int value) {
    if (const EnumInfo* info = Find(enumType); info != nullptr) {
        if (IsAllowedValue(*info, value)) {
            return Core::Variant::From(value);
        }
        if (!info->Options.empty()) {
            return Core::Variant::From(info->Options.front().Value);
        }
    }
    return Core::Variant::From(value);
}

int IntFromVariant(const Core::Variant& value) {
    return value.toInt();
}

Core::String NameFromInt(const Core::String& enumType, const int value) {
    if (const EnumInfo* info = Find(enumType); info != nullptr) {
        for (const auto& opt : info->Options) {
            if (opt.Value == value) {
                return Core::String(opt.Name);
            }
        }
    }
    return {};
}

Core::String NameFromVariant(const Core::String& enumType, const Core::Variant& value) {
    if (!IsRegisteredEnum(enumType)) {
        return {};
    }
    return NameFromInt(enumType, IntFromVariant(value));
}

Core::Variant VariantFromName(const Core::String& enumType, const Core::String& nameOrNumber) {
    if (const EnumInfo* info = Find(enumType); info != nullptr) {
        if (const auto parsed = TryParseOptionName(*info, nameOrNumber); parsed.has_value()) {
            return VariantFromInt(enumType, *parsed);
        }
    }
    return {};
}

Core::Variant CoerceVariant(const Core::String& enumType, const Core::Variant& value) {
    if (!IsRegisteredEnum(enumType) || !value.IsValid() || value.IsNull()) {
        return {};
    }

    if (value.Is<Core::String>()) {
        return VariantFromName(enumType, value.Get<Core::String>());
    }

    if (value.Is<int>() || value.Is<int64_t>() || value.Is<double>() || value.Is<bool>()) {
        return VariantFromInt(enumType, value.toInt());
    }

    return {};
}

bool IsRegisteredEnum(const Core::String& enumType) {
    return Find(enumType) != nullptr;
}

} // namespace Lvs::Engine::Enums::Metadata
