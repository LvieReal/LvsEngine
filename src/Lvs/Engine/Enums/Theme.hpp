#pragma once

#include <QMetaType>

namespace Lvs::Engine::Enums {

enum class Theme {
    Light = 0,
    Dark = 1
};

} // namespace Lvs::Engine::Enums

Q_DECLARE_METATYPE(Lvs::Engine::Enums::Theme)

