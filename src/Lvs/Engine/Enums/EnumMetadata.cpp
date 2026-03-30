#include "Lvs/Engine/Enums/EnumMetadata.hpp"

#include "Lvs/Engine/Enums/EnumMetadataTraits.hpp"

#include "Lvs/Engine/Enums/LightingComputationMode.hpp"
#include "Lvs/Engine/Enums/LightingTechnology.hpp"
#include "Lvs/Engine/Enums/MSAA.hpp"
#include "Lvs/Engine/Enums/MeshCullMode.hpp"
#include "Lvs/Engine/Enums/PartShape.hpp"
#include "Lvs/Engine/Enums/PartSurface.hpp"
#include "Lvs/Engine/Enums/PartSurfaceType.hpp"
#include "Lvs/Engine/Enums/SkyboxTextureLayout.hpp"
#include "Lvs/Engine/Enums/RenderCullMode.hpp"
#include "Lvs/Engine/Enums/RenderDepthCompare.hpp"
#include "Lvs/Engine/Enums/SpecularHighlightType.hpp"
#include "Lvs/Engine/Enums/SurfaceMipmapping.hpp"
#include "Lvs/Engine/Enums/Tonemapper.hpp"
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
    Core::Vector<EnumEntry> Entries{};
};

[[nodiscard]] std::string LowerAscii(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

[[nodiscard]] std::string LowerAlnum(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc)) {
            out.push_back(static_cast<char>(std::tolower(uc)));
        }
    }
    return out;
}

[[nodiscard]] bool IsAllowedValue(const EnumInfo& info, const int value) {
    for (const auto& entry : info.Entries) {
        if (entry.Value == value) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::optional<int> TryParseOptionName(const EnumInfo& info, const Core::String& nameOrNumber) {
    const std::string needle = LowerAscii(nameOrNumber);
    const std::string needleFlat = LowerAlnum(nameOrNumber);
    if (needle.empty()) {
        return std::nullopt;
    }

    for (const auto& entry : info.Entries) {
        if (entry.Name != nullptr && (LowerAscii(entry.Name) == needle || LowerAlnum(entry.Name) == needleFlat)) {
            return entry.Value;
        }
        if (entry.DisplayName != nullptr
            && (LowerAscii(entry.DisplayName) == needle || LowerAlnum(entry.DisplayName) == needleFlat)) {
            return entry.Value;
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

template <typename E>
void AddEnum(Core::HashMap<Core::String, EnumInfo>& out) {
    constexpr std::string_view enumName = Core::EnumTraits<E>::Name;
    if (enumName.empty()) {
        return;
    }

    EnumInfo info;
    info.Entries.reserve(EnumInfoTraits<E>::ValueCount);
    for (std::size_t i = 0; i < EnumInfoTraits<E>::ValueCount; ++i) {
        const auto& meta = EnumInfoTraits<E>::Values[i];
        info.Entries.push_back(EnumEntry{
            .Name = meta.Name,
            .Value = meta.Id,
            .DisplayName = meta.DisplayName,
            .Description = meta.Description
        });
    }

    out.insert_or_assign(Core::String(enumName), std::move(info));
}

[[nodiscard]] const Core::HashMap<Core::String, EnumInfo>& Registry() {
    using namespace Lvs::Engine::Enums;
    using Lvs::Engine::Rendering::RenderApi;

    static const Core::HashMap<Core::String, EnumInfo> registry = [] {
        Core::HashMap<Core::String, EnumInfo> out;
        AddEnum<LightingTechnology>(out);
        AddEnum<LightingComputationMode>(out);
        AddEnum<TextureFiltering>(out);
        AddEnum<SpecularHighlightType>(out);
        AddEnum<MSAA>(out);
        AddEnum<SurfaceMipmapping>(out);
        AddEnum<SkyboxTextureLayout>(out);
        AddEnum<MeshCullMode>(out);
        AddEnum<PartShape>(out);
        AddEnum<PartSurface>(out);
        AddEnum<PartSurfaceType>(out);
        AddEnum<Tonemapper>(out);
        AddEnum<Theme>(out);
        AddEnum<RenderCullMode>(out);
        AddEnum<RenderDepthCompare>(out);
        AddEnum<RenderApi>(out);

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

Core::Vector<EnumEntry> EntriesForEnum(const Core::String& enumType) {
    if (const EnumInfo* info = Find(enumType); info != nullptr) {
        return info->Entries;
    }
    return {};
}

Core::Vector<EnumOption> OptionsForEnum(const Core::String& enumType) {
    if (const EnumInfo* info = Find(enumType); info != nullptr) {
        Core::Vector<EnumOption> out;
        out.reserve(info->Entries.size());
        for (const auto& entry : info->Entries) {
            out.push_back(EnumOption{
                .Name = (entry.DisplayName != nullptr && entry.DisplayName[0] != '\0') ? entry.DisplayName : entry.Name,
                .Value = entry.Value
            });
        }
        return out;
    }
    return {};
}

Core::Variant VariantFromInt(const Core::String& enumType, const int value) {
    if (const EnumInfo* info = Find(enumType); info != nullptr) {
        if (IsAllowedValue(*info, value)) {
            return Core::Variant::From(value);
        }
        if (!info->Entries.empty()) {
            return Core::Variant::From(info->Entries.front().Value);
        }
    }
    return Core::Variant::From(value);
}

int IntFromVariant(const Core::Variant& value) {
    return value.toInt();
}

Core::String NameFromInt(const Core::String& enumType, const int value) {
    if (const EnumInfo* info = Find(enumType); info != nullptr) {
        for (const auto& entry : info->Entries) {
            if (entry.Value == value) {
                return Core::String(entry.Name);
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
