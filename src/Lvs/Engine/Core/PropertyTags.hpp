#pragma once

#include "Lvs/Engine/Core/Types.hpp"

#include <optional>
#include <utility>

namespace Lvs::Engine::Core::PropertyTags {

inline constexpr auto VISIBLE_IF_TAG_PREFIX = "VisibleIf:";

String BuildVisibleIfTag(const String& propertyName, const String& expectedValue);
std::optional<std::pair<String, String>> ParseVisibleIfTag(const String& tag);

} // namespace Lvs::Engine::Core::PropertyTags
