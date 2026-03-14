#pragma once

#include <QMetaType>

namespace Lvs::Engine::Enums {

enum class SurfaceMipmapping {
    Off = 0,
    On = 1
};

} // namespace Lvs::Engine::Enums

Q_DECLARE_METATYPE(Lvs::Engine::Enums::SurfaceMipmapping)
