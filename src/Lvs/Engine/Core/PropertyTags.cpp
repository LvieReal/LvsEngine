#include "Lvs/Engine/Core/PropertyTags.hpp"

#include <cstring>
#include <cctype>

namespace Lvs::Engine::Core::PropertyTags {

namespace {
String TrimCopy(const String& in) {
    size_t start = 0;
    size_t end = in.size();
    while (start < end && std::isspace(static_cast<unsigned char>(in[start])) != 0) {
        ++start;
    }
    while (end > start && std::isspace(static_cast<unsigned char>(in[end - 1])) != 0) {
        --end;
    }
    return in.substr(start, end - start);
}
} // namespace

String BuildVisibleIfTag(const String& propertyName, const String& expectedValue) {
    return String(VISIBLE_IF_TAG_PREFIX) + propertyName + "=" + expectedValue;
}

std::optional<std::pair<String, String>> ParseVisibleIfTag(const String& tag) {
    if (!tag.starts_with(VISIBLE_IF_TAG_PREFIX)) {
        return std::nullopt;
    }

    const String condition = tag.substr(strlen(VISIBLE_IF_TAG_PREFIX));
    const size_t separator = condition.find('=');
    if (separator == String::npos) {
        return std::nullopt;
    }

    const String propertyName = TrimCopy(condition.substr(0, separator));
    const String expectedValue = TrimCopy(condition.substr(separator + 1));
    if (propertyName.empty()) {
        return std::nullopt;
    }

    return std::pair<String, String>{propertyName, expectedValue};
}

} // namespace Lvs::Engine::Core::PropertyTags
