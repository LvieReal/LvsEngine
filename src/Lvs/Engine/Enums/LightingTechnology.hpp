#pragma once

#include <QMetaType>

namespace Lvs::Engine::Enums {

enum class LightingTechnology {
    Default = 0
};

} // namespace Lvs::Engine::Enums

Q_DECLARE_METATYPE(Lvs::Engine::Enums::LightingTechnology)
