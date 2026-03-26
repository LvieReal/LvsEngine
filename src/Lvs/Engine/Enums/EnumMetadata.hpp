#pragma once

#include "Lvs/Engine/Core/Types.hpp"
#include "Lvs/Engine/Core/Variant.hpp"

namespace Lvs::Engine::Enums::Metadata {

struct EnumEntry {
    const char* Name;
    int Value;
    const char* DisplayName;
    const char* Description;
};

struct EnumOption {
    const char* Name;
    int Value;
};

[[nodiscard]] Core::Vector<EnumEntry> EntriesForEnum(const Core::String& enumType);
[[nodiscard]] Core::Vector<EnumOption> OptionsForEnum(const Core::String& enumType);
[[nodiscard]] Core::Variant VariantFromInt(const Core::String& enumType, int value);
[[nodiscard]] int IntFromVariant(const Core::Variant& value);
[[nodiscard]] Core::String NameFromInt(const Core::String& enumType, int value);
[[nodiscard]] Core::String NameFromVariant(const Core::String& enumType, const Core::Variant& value);
[[nodiscard]] Core::Variant VariantFromName(const Core::String& enumType, const Core::String& nameOrNumber);
[[nodiscard]] Core::Variant CoerceVariant(const Core::String& enumType, const Core::Variant& value);
[[nodiscard]] bool IsRegisteredEnum(const Core::String& enumType);

} // namespace Lvs::Engine::Enums::Metadata
