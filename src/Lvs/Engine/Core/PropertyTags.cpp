#include "Lvs/Engine/Core/PropertyTags.hpp"

#include <cstring>

namespace Lvs::Engine::Core::PropertyTags {

QString BuildVisibleIfTag(const QString& propertyName, const QString& expectedValue) {
    return QString("%1%2=%3").arg(VISIBLE_IF_TAG_PREFIX, propertyName, expectedValue);
}

std::optional<QPair<QString, QString>> ParseVisibleIfTag(const QString& tag) {
    if (!tag.startsWith(VISIBLE_IF_TAG_PREFIX)) {
        return std::nullopt;
    }

    const QString condition = tag.mid(static_cast<int>(strlen(VISIBLE_IF_TAG_PREFIX)));
    const int separator = condition.indexOf('=');
    if (separator < 0) {
        return std::nullopt;
    }

    const QString propertyName = condition.left(separator).trimmed();
    const QString expectedValue = condition.mid(separator + 1).trimmed();
    if (propertyName.isEmpty()) {
        return std::nullopt;
    }

    return QPair<QString, QString>{propertyName, expectedValue};
}

} // namespace Lvs::Engine::Core::PropertyTags
