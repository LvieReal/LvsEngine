#pragma once

#include <QPair>
#include <QString>

#include <optional>

namespace Lvs::Engine::Core::PropertyTags {

inline constexpr auto VISIBLE_IF_TAG_PREFIX = "VisibleIf:";

QString BuildVisibleIfTag(const QString& propertyName, const QString& expectedValue);
std::optional<QPair<QString, QString>> ParseVisibleIfTag(const QString& tag);

} // namespace Lvs::Engine::Core::PropertyTags
