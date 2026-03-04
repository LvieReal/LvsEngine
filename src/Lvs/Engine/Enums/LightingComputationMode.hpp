#pragma once

#include <QMetaType>

namespace Lvs::Engine::Enums {

enum class LightingComputationMode {
    PerPixel = 0,
    PerVertex = 1
};

} // namespace Lvs::Engine::Enums

Q_DECLARE_METATYPE(Lvs::Engine::Enums::LightingComputationMode)
